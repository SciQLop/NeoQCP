# Why stacked axis rects in one plot are slow

Input to spec decision 13 (align stacked plots inside one widget, or across widgets).

## Answer

A stacked plot is not slow per rect. It is slow because it cannot repaint *one* rect.

1. Moving the range of one axis rect dirties the plot's shared layers.
2. Those layers hold the graphs of all eight rects.
3. So the replot redraws all eight graphs, not one.
4. Then the GPU frame redraws the whole widget, because a `QRhiWidget` renders its whole target every frame.

Eight separate plot widgets get "repaint only the one that changed" for free: each widget has its own layers
and its own render target. When all eight change together (a synced time axis pan), the stacked plot is
equal or cheaper.

## Numbers

Scenario: 8 rows, each a 100 000-point `QCPGraph` sine. One row's x range is panned (`setRange(0.001 * step,
0.7 + 0.001 * step)`), then replotted. 200 steps, median per step. Equal geometry in all cases: one
640x360 plot, or eight 640x45 plots stacked at y = 45 * i (the same total pixels).

The Task 1 benchmarks `QCPAxisRects_Stack8_Replot` and `QCPPlots_Stack8_Replot` (named `_Frame` in the
brief) already have equal total pixels. They differ in one thing: the stacked benchmark leaves the seven
unchanged rects at the default range 0..5, so it draws a little less. The "rescaled" rows below fix that.

### CPU side of `replot()` (offscreen, three runs, all within 5 %)

| Scenario | replot | graph `draw()` calls | time in graph `draw()` |
|---|---|---|---|
| Stacked, 8 rects, Task 1 setup | 1.8 ms | 8 | 1.2 ms |
| Stacked, 8 rects, ranges rescaled | 2.0 ms | 8 | 1.4 ms |
| Stacked, rescaled, axes and grids on their usual layers | 2.0 ms | 8 | 1.4 ms |
| **Separate, 8 widgets, replot the changed one** | **0.25 ms** | 1 | 0.15 ms |
| Separate, 8 widgets, replot all eight | 2.2 ms | 8 | 1.5 ms |
| Stacked, 3 buffered layers per rect (grid, main, axes) | 0.31 ms | 1 | 0.14 ms |

Layout of the 8 rects (`updateLayout`) costs 0.05 ms per replot. It is not the problem.

### Full frame (replot + `render()` + window flush)

Run under `xvfb-run ... QT_QPA_PLATFORM=xcb`: software OpenGL (Mesa llvmpipe), MSAA 4x (the default at
device pixel ratio 1). Time from `setRange` until the changed widget emits `frameSubmitted`. "Idle frame"
is `update()` with nothing changed: the cost of just re-rendering and compositing.

The machine was shared with a large compile during these runs (load average 13 to 29). The table gives the
best median of 5 to 8 separate processes. Noisy runs were up to 4x slower, for every scenario alike.

| Scenario | full frame | idle frame | replot inside it |
|---|---|---|---|
| Stacked, 8 rects, ranges rescaled | 7.4 ms | 4.0 ms | 2.8 ms |
| **Separate, 8 widgets, one changed** | **1.8 ms** | 1.3 ms | 0.35 ms |
| Separate, 8 widgets, all eight changed | 11.8 ms | 7.3 ms | 3.0 ms |
| Stacked, 3 buffered layers per rect | 10.9 ms | 10 ms | 0.66 ms |

Software rendering pays per pixel filled, so it exaggerates fill cost against a real GPU. Use these numbers
for ratios between rows, not as absolute frame times.

Headline: a one-row pan costs **2.0 ms vs 0.25 ms** of CPU, and **7.4 ms vs 1.8 ms** per full frame.

## Cause

**Layers and paint buffers belong to the plot, not to an axis rect.** A range change goes through
`QCPAxis::rangeDidChange` (`src/axis/axis.cpp:614-617`) to `QCPAxisRect::markAffectedLayersDirty`
(`src/layoutelements/layoutelement-axisrect.cpp:1455-1489`). That marks the layer of each of the rect's
axes, grids and plottables dirty (`QCPLayer::markDirty`, `src/layer.cpp:300-304`). Dirty is a flag on the
layer's paint buffer, and the "main" buffer holds every graph of every rect. `QCustomPlot::replot` then
calls `drawToPaintBuffer` on every layer of a dirty buffer (`src/core.cpp:2407-2416`), which draws every
child. So the seven unchanged graphs redraw: 8 graph `draw()` calls instead of 1 (measured). The GPU path
repeats the waste. `replot` clears the layer's whole GPU line list (`src/core.cpp:2348-2367`), so all eight
graphs re-extrude their lines. `QCPPlottableRhiLayer::uploadResources` then re-uploads the whole vertex
buffer (`src/painting/plottable-rhi-layer.cpp:227-250`). The paint buffers are viewport-sized
(`src/core.cpp:3893`), so the dirty buffer is cleared (`src/painting/paintbuffer-rhi.cpp:82-91`) and
uploaded (`src/core.cpp:2938-2948`) at full widget size. Last, `render()` cannot do part of the widget.
`executeRenderPass` begins a pass that clears the target (`src/core.cpp:3018`). It then composites every
paint buffer as a full-viewport quad and draws every GPU line, dirty or not (`src/core.cpp:3027-3158`).
That is why the idle frame of the stacked widget (4.0 ms) is three times that of one small widget (1.3 ms).

Two side findings:

- New axis rects land on the current layer, "main" (`src/core.cpp:484`, `src/axis/axis.cpp:461`). In the
  Task 1 stacked benchmark every axis, grid and rect is on "main", not on "axes" and "grid". It makes no
  measurable difference here (2.0 ms either way), but a 3.0 rect should put its parts on the right layers.
- The pan fast path (shift the old texture instead of repainting) is per layer. `QCPLayer::pixelOffset`
  refuses it as soon as one layer holds plottables of two axis rects (`src/layer.cpp:364-382`). The
  benchmark's `QCPGraph` never takes that path, so it is not in the numbers. But for `QCPGraph2` and
  `QCPMultiGraph`, the types SciQLopPlots uses, a stacked plot loses that fast path on every pan.

## The existing workaround does not hold

Giving each rect its own buffered layers (grid, main, axes) fixes the CPU side: 0.31 ms, one graph redrawn.
It makes the frame worse: 10.9 ms. There are now 28 full-viewport buffers. Every frame composites all 28
as full-widget quads, and each dirty one uploads a full-widget texture.

## What would make a one-rect change repaint only that rect

All four, together:

1. **Dirty state per axis rect.** A range change marks (layer, rect) dirty, and replot redraws only that
   rect's layerables on that layer.
2. **Paint buffers sized to the rect, not the viewport.** Each rect owns a texture per layer, the size of
   its outer rect, composited as a quad at its position. Clear and upload cost then scale with the rect.
3. **GPU line and scatter lists per rect.** The clear in `replot` and the upload in `uploadResources` only
   touch the dirty rect's geometry. This also brings back the per-layer pan fast path, since one rect's
   layer holds one rect's plottables.
4. **Partial render-target updates.** Keep the previous frame and redraw only the dirty rect, scissored.
   NeoQCP's pass clears the target, and keeping the old contents would need a render target NeoQCP makes
   itself (`QRhiWidget::setAutoRenderTarget(false)`), with MSAA resolve in the way. In practice this means
   rendering each rect into its own offscreen texture and compositing those.

Points 2 to 4 rebuild, inside one widget, what separate widgets already get from Qt: their own target, and
re-rendering only the widget that called `update()`.

## For decision 13

- If panels mostly change one row at a time (zoom one plot, y autoscale of one plot), separate widgets win
  by about 4x per frame today, and stacking needs all four changes above to catch up.
- If panels mostly change every row together (the synced time axis, SciQLop's main case), stacking is
  already as good or better: 7.4 ms vs 11.8 ms per frame here, 2.0 ms vs 2.2 ms of CPU. One render pass
  beats eight.
- "Per-layer repaint" alone (the spec's hope at answer 5) does not fix the one-row case. Layers are
  plot-wide, so it needs points 1 and 2 at least for the CPU, and 3 and 4 for the GPU.

Recommendation: keep cross-widget alignment groups (decision 13 as written). Per-rect buffers in one widget
is a large rendering redesign, and it would only pay off for the one-row case that separate widgets
already handle.

## Method and confidence

- Measured with a throwaway program linked against `build-qt612/libNeoQCP.a` (debugoptimized, Qt 6.12.0),
  not committed. It times graph `draw()` and `updateLayout()` through subclass overrides, `render()` through
  an override, and counts dirty paint buffers before each replot. No library code was changed.
- No Tracy capture tool is installed on this machine and there is no display, so the brief's Tracy capture
  on a real display was replaced by that program plus Xvfb.
- Sure: the CPU cause. The draw counts (8 vs 1) and the per-rect-layer experiment (2.0 ms to 0.31 ms)
  isolate it, and the code path is direct.
- Fairly sure: the GPU side. Its ranking (whole-widget render every frame, per-buffer full-viewport
  composite) follows from the code and the idle-frame numbers. But software GL on a loaded machine puts
  absolute numbers and the stacked-vs-separate ratio for the synced case only roughly right. A check on a
  real GPU (macOS Metal, Linux Vulkan or GL) should come before decision 13 is closed, mostly for the
  all-rows-change case.
