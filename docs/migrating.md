# Migrating from QCustomPlot

NeoQCP started from QCustomPlot 2.1.1 and keeps its API. Most code that builds with QCustomPlot 2.1.1 on Qt 6 builds with NeoQCP unchanged. This page lists what does change: the build, the rendering backend, a few defaults, the APIs that are gone, and the new classes you may want to move to.

## Qt 6.7 or newer only

Qt 4 and Qt 5 support is gone, and so are the version checks that went with it. NeoQCP needs Qt 6.7 or newer, because `QCustomPlot` now derives from `QRhiWidget`, which appeared in Qt 6.7. It uses the Core, Gui, Widgets, Svg and PrintSupport modules.

## Build with Meson

There is no single `qcustomplot.h` / `qcustomplot.cpp` pair to drop into your project, and no qmake support. The sources are split into one header and source file per class or topic under `src/`, and Meson builds them into a static library.

- Add NeoQCP as a Meson subproject and ask for the `NeoQCP` dependency. {doc}`getting-started` shows the wrap file and the `meson.build` lines.
- Keep including one header: `#include <qcustomplot.h>`. It pulls in everything.
- The C++ standard is C++20.

A CMake or qmake project can build NeoQCP with Meson first, then link `libNeoQCP.a`. Its include paths are `src/` and the build directory, which holds the generated `neoqcp_config.h`.

## The widget renders through QRhi

Upstream QCustomPlot paints into `QPixmap` buffers, or into OpenGL framebuffers when `setOpenGl(true)` is called. NeoQCP replaces both with QRhi, Qt's portable GPU layer (Metal, Vulkan, Direct3D or OpenGL).

What that means for your code:

- `QCustomPlot` is still a `QWidget`, through `QRhiWidget`. Layouts, Qt Designer promotion, signals and slots work as before.
- `setOpenGl()` and `openGl()` are removed. GPU rendering is always on. Remove the calls, and any `QCUSTOMPLOT_USE_OPENGL` define.
- The OpenGL paint buffers `QCPPaintBufferGlPbuffer` and `QCPPaintBufferGlFbo` are removed.
- `QCustomPlot` no longer overrides `paintEvent()`. `QRhiWidget` draws through `initialize()` and `render()`. A subclass that reimplemented `paintEvent()` to draw on top of the plot must use an item, a layer or the overlay instead.
- `setBufferDevicePixelRatio()` still exists. The ratio also follows the screen on its own when the window moves.
- The `NEOQCP_RHI_BACKEND` environment variable forces a backend: `opengl`, `vulkan`, `metal`, `d3d11` or `d3d12`.

Exports (`savePdf()`, `savePng()`, `saveJpg()`, `saveBmp()`, `saveRastered()`, `toPixmap()`, `toPainter()`) keep their signatures and still draw with `QPainter`.

{doc}`performance` explains what is drawn on the GPU, and {doc}`architecture/qrhi-rendering-pipeline` how.

## Replot priorities

Under `QRhiWidget` the widget repaint is always queued; there is no synchronous repaint.

| `RefreshPriority` | Upstream | NeoQCP |
|---|---|---|
| `rpImmediateRefresh` | replot, then repaint at once | replot now, repaint on the next frame |
| `rpQueuedRefresh` | replot, then queue the repaint | deprecated alias of `rpImmediateRefresh` |
| `rpRefreshHint` | depends on `phImmediateRefresh` | deprecated alias of `rpImmediateRefresh` |
| `rpQueuedReplot` | replot on the next event-loop pass | unchanged |

The default argument of `replot()` is now `rpImmediateRefresh` (it was `rpRefreshHint`). The `QCP::phImmediateRefresh` plotting hint is still accepted but has no effect any more.

Code using `rpQueuedRefresh` or `rpRefreshHint` compiles, with a deprecation warning.

## Changed defaults

| Setting | Upstream 2.1.1 | NeoQCP |
|---|---|---|
| `antialiasedElements()` | none | axes, sub-grid, legend, legend items, plottables, items |
| `noAntialiasingOnDrag()` | `false` | `true` |
| `plottingHints()` | `phCacheLabels \| phImmediateRefresh` | `phImmediateRefresh` (label caching off) |
| `"main"` layer mode | `lmLogical` | `lmBuffered` |
| Multisampling | — | 4 samples, or 1 on a HiDPI screen |

Set the old value back if your pictures depend on it, for example `customPlot->setAntialiasedElements(QCP::aeNone)`. GPU-drawn lines are antialiased by multisampling whatever the flags say; change that with `setSampleCount()`.

## `[[nodiscard]]` results

Many functions that return a status or a value are now `[[nodiscard]]`. Among them: `removePlottable()`, `removeGraph()`, `removeItem()`, `addLayer()`, `removeLayer()`, `moveLayer()`, the `save...()` functions, `toPixmap()`, and most getters.

Code that ignores the result still compiles, with a warning:

```cpp
customPlot->removeGraph(0);                  // warning: result ignored
if (!customPlot->savePng("plot.png"))        // fine
    qWarning() << "export failed";
```

## Behaviour that changed

Most changes are confined to the new classes. A few touch existing code.

- **Item positions.** Items far outside the visible range no longer abort under Qt 6.11's checked integer rectangles. Pixel geometry of items uses floating point.
- **Axis labels.** An axis label longer than its axis is elided with "…" instead of being cut at both ends.
- **Log axes.** On a log axis spanning less than 1.6 decades, upstream falls back to linear ticks. NeoQCP puts the ticks on round mantissas (2, 5, ...) instead.
- **Keyboard.** `QCustomPlot` now handles key presses. Each plottable sees the key first, through the new virtual `QCPAbstractPlottable::keyPress()`. Delete and Backspace make every selected item emit `QCPAbstractItem::deleteRequested()`; nothing is deleted for you.
- **Fixed upstream bugs.** DPI scaling on macOS and Windows; a crash of the date-time ticker on Qt 6.10 and newer; a use-after-delete in `removeLayer()`.

The classic plottables (`QCPGraph`, `QCPCurve`, `QCPBars`, `QCPColorMap`, ...) keep their data model and behaviour. Their solid lines and `QCPGraph` fills are drawn by the GPU now; dashed lines, channel fills and scatters still use `QPainter`.

The behaviours below belong to the new plottables only. Code that keeps using the classic ones is not affected.

- **Gaps.** `QCPGraph2`, `QCPMultiGraph` and `QCPColorMap2` detect missing stretches of data and leave them blank instead of joining them. The classic `QCPGraph` only breaks its line at NaN values, as before. See {doc}`architecture/gap-detection`.
- **Asynchronous resampling.** `QCPGraph2`, `QCPMultiGraph`, `QCPColorMap2` and `QCPHistogram2D` do their heavy work on worker threads. The first picture of large data can come a moment after `replot()`.
- **Deferred data swap.** New data on a `QCPGraph2` or `QCPMultiGraph` that is already on screen is shown after a short window (200 ms by default), not at once. Until then the graph reports the old data.
- **Busy state.** These plottables report `busy()` and emit `busyChanged()`. A plottable busy for long is drawn faded, with a ⟳ in its legend entry.

{doc}`performance` explains the three last points and what they mean for code that reads results right after `setData()`.

## New classes

These do not exist upstream. You do not need them to migrate, but they are where most of the speed and new features are.

Plottables:

- <a href="api/classQCPGraph2.html">QCPGraph2</a>: a line or scatter graph that plots from your own containers, in their own numeric type, without copying. Resamples large data on a worker thread. See {doc}`graphs`.
- <a href="api/classQCPMultiGraph.html">QCPMultiGraph</a>: many value columns sharing one key column, from one data source. Each component has its own pen, name and selection. Can colour lines by a scalar.
- <a href="api/classQCPWaterfallGraph.html">QCPWaterfallGraph</a>: a `QCPMultiGraph` that stacks its traces with offsets, optionally normalised.
- <a href="api/classQCPColorMap2.html">QCPColorMap2</a>: a colour map over zero-copy data, resampled on a worker thread, with gaps, log axes and GPU contour lines. See {doc}`colormaps`.
- <a href="api/classQCPHistogram2D.html">QCPHistogram2D</a>: bins a point cloud into a colour map, on a worker thread.
- <a href="api/classQCPIntervals.html">QCPIntervals</a>: a timeline of intervals on lanes, editable with the mouse and the keyboard. See {doc}`plottables`.

Data sources, for the zero-copy plottables: `QCPSoADataSource`, `QCPSoAMultiDataSource`, `QCPRowMajorMultiDataSource`, `QCPSoADataSource2D`, and their abstract bases.

Items and overlays:

- <a href="api/classQCPItemVSpan.html">QCPItemVSpan</a>, `QCPItemHSpan`, `QCPItemRSpan`: shaded key, value or rectangular ranges, drawn on the GPU, draggable. See {doc}`items`.
- <a href="api/classQCPItemRichText.html">QCPItemRichText</a>: an item showing HTML text in a rounded box.
- <a href="api/classQCPOverlay.html">QCPOverlay</a>: a message banner over the plot, from `QCustomPlot::overlay()`.
- Interactive item creation: `setItemCreator()`, `setItemPositioner()`, `setCreationModeEnabled()` on `QCustomPlot`, with the `itemCreated()` and `itemCanceled()` signals. See {doc}`interactions`.

Look and text:

- <a href="api/classQCPTheme.html">QCPTheme</a>: named colours for background, foreground, grid, selection, legend and more, with `light()` and `dark()` presets. `QCustomPlot::setTheme()` applies one and follows its changes, and the colours are Qt properties you can set from a style sheet. See {doc}`styling`.
- LaTeX in labels: axis labels, legend entries, titles and text items typeset any `$...$` span. `QCPAxis::setLabelRenderer()` and `QCPLabelRenderer::setDefaultRenderer()` swap the typesetter. Build with `-Dwith_latex=false` to leave it out.
- New colour gradients: `gpViridis`, `gpMagma`, `gpInferno`, `gpPlasma`, `gpCividis`, `gpTurbo` and `gpCoolwarm`.

Plot-wide settings:

- `setDataSwapDebounceMs()`, `setDataSwapMaxWaitMs()`: timing of the deferred data swap.
- `setMaxPipelineThreads()`: worker threads per plot.
- `setSkipReplotsWhenHidden()`: skip painting while the widget is hidden.
- `QCPLayerable::setMouseTransparent()`: let clicks pass through a layerable.

## Removed APIs

| Removed | Use instead |
|---|---|
| `QCustomPlot::setOpenGl()`, `openGl()` | nothing: the GPU is always used |
| `QCPPaintBufferGlPbuffer`, `QCPPaintBufferGlFbo` | nothing: `QCPPaintBufferRhi` is used internally |
| `QCustomPlot::paintEvent()` override | items, layers or `overlay()` |
| qmake project files, the amalgamated source | the Meson build |
| Qt 4 / Qt 5 compatibility code | Qt 6.7 or newer |

Everything else in the public API of QCustomPlot 2.1.1 is still there.
