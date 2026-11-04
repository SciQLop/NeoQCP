# NeoQCP 3.0 — drawing and extension API (milestone 3 design)

Status: revised 2026-11-04 after an adversarial review (`docs/plans/neoqcp-3/m3-design-review.md`).
It refines decisions 8, 9 and 10 of `docs/specs/2026-11-02-neoqcp-3-object-model-design.md`.

Evidence:
- How every built-in class draws, picks and splits GPU and QPainter today:
  `docs/plans/neoqcp-3/m3-drawing-inventory.md`.
- Prior art and bibliography (Qt, Qwt, matplotlib, Makie, ImPlot, VisPy, Skia, Chromium, Flutter,
  egui_plot, PRISM): `docs/plans/neoqcp-3/m3-prior-art-survey.md`.

## Maintainer decisions (2026-11-04)

- No Qt Canvas Painter for now (GPLv3/commercial, an add-on module). The command list stays
  backend-neutral so it can be added later behind a build option.
- A pen style never changes stacking: the GPU line draws dashes itself.
- Strong types as soon as possible, everywhere they make the API safer — not only positions.

## What we want

A user writes a new plot type the way they would in matplotlib. They derive from one small class,
write `draw()`, and call a handful of helpers with positions that say what they are. Clipping, hit
testing, the legend icon and the data range have defaults. The result stacks the same on screen and in
an export, whatever the pen style.

## What is true today (from the inventory)

1. Paint buffers are shared: consecutive logical layers share one texture, a buffered layer gets its
   own (`setupPaintBuffers`). The render pass composites a buffer's texture, then draws the GPU geometry
   of its layers on top. So inside a buffer, GPU always lands above QPainter.
2. New objects go on the current layer, `main`. Text and annotations share it with the data.
3. A graph goes to the GPU only with a solid pen and outside an export. A dashed pen, a gradient fill or
   an export sends it to QPainter, under items created after it. That is the "dashed pen changes
   stacking" bug. The GPU-or-QPainter test is repeated in about ten places.
4. Every GPU path has a hand-written QPainter fallback for exports.
5. Hit testing has four rules (distance; "anywhere in my rectangle"; "anywhere in the axis rect when no
   details are asked", used by the wheel; "never"). The press and the selection may pick different
   objects on purpose: a non-selectable object on top takes the press, the selectable one under it
   gets selected.
6. Every plottable writes its own legend icon. Items have a data box since the item index milestone
   (`QCPAbstractItem::dataExtent()`).
7. A pan does not redraw: the old texture is shifted (`pixelOffset`), and GPU lines reuse their
   extruded vertices with a translation (`ExtrusionCache`).

## What others do (from the survey)

- No Qt API records mixed GPU and QPainter work, so NeoQCP needs its own small command list, like
  PRISM's `DrawList`.
- Skia's `SkDrawable` is the contract: `draw()` records, `bounds()`, a generation counter bumped when
  the object changes, the recording cached against it. egui_plot's `PlotItem` is the same for plots.
- Chromium and Flutter keep the transform and the clip outside the recording.
- Every system fixes the stacking order before any backend sees it; none lets style change it.
- matplotlib's transforms are the model for typed positions; Rust's `euclid` for making a mix of
  spaces a compile error.

## The design

### 1. Strong types

- Positions: `QCP::Data{x, y}`, `QCP::Pixels{x, y}`, `QCP::AxisRatio{x, y}`, an anchor plus a pixel
  offset, and a per-axis mix (`QCP::Mixed{Data x, AxisRatio y}` for a vertical span). No setter order,
  no type flag.
- Lengths and offsets are types too (`QCP::PixelOffset`, `QCP::PixelLength`), so a data value cannot
  be passed where pixels are expected. `Data + Pixels` does not compile; `anchor + PixelOffset` does.
- Transforms are public values: data to pixels for one axis pair, the axis-rect ratio transform, and
  their inverses. Log, reversed and time axes live inside. A batch form maps spans, so nobody writes a
  scalar loop over a million points.
- The same rule applies beyond drawing where it removes a misuse (the plan lists the candidates, e.g.
  key versus value, axis-pair handles).

### 2. The object contract

`QCPDrawable` replaces the old drawing virtuals (`draw(QCPPainter*)`, `drawLegendIcon`, `selectTest`)
for the classes ported to it. In milestones 3 and 4 it is the adapter `QCPDrawn<Base>`: a ported class
keeps its old base (data, groups, legend, selection) and the adapter implements the old virtuals from
the new ones; milestone 4 folds it into the bases. Classes not yet ported stay on the old base until milestone 4; the old
base is removed before 3.0 ships (milestone 5), so a release never has two ways.

```cpp
class QCPDrawable : public QCPLayerable {
protected:
    virtual void draw(QCPDrawContext& context) const = 0;
    virtual std::optional<QCPDataBox> dataRange() const;          // default: from the recording
    virtual std::optional<double> hitTest(QCP::Pixels at, QVariant* details) const; // default: recording
    virtual void drawLegendIcon(QCPDrawContext& icon) const;     // default: the recording, fitted
    void contentChanged();
};
```

- Two counters, never merged. `contentChanged()` bumps the object's generation and marks it dirty (the
  milestone-2 setter rule). A layer mark (a pan, another object's change) never touches a generation.
- The recording is cached on (generation, view epoch). The view epoch changes with the viewport size,
  the device pixel ratio or the theme. A pan or a zoom changes neither key.
- `draw()` is `const`: it may fill mutable caches keyed on those two counters, and must not change
  anything other code observes. Transform, export, pixel ratio and theme facts come only from the
  context.
- `dataRange()` succeeds `dataExtent()` and keeps its two rules: a side counts only when every position
  on that axis is a data position with no anchor; on a log axis a box reaching zero or below is not
  indexed. The item index keeps using it.

### 3. Helpers and the command list

- Helpers: `polyline`, `polygon`, `fillBetween`, `markers`, `errorBars`, `rect`, `ellipse`, `path`,
  `text`, `image`, each taking typed positions and a style.
- Each helper appends one command. Point data is held through `std::shared_ptr<const ...>`, so a
  recording shares a data source's storage instead of copying it (data sources are already shared).
- The command list is private. Users see the helpers only, so the command set can grow without an ABI
  break.
- Each command kind has one screen backend, chosen by kind, never by style: GPU for polylines (dashes
  included), fills, rects, ellipses, error bars and markers; QPainter for text, paths and images
  (images move to the GPU once the colour-map texture layer is generic, milestone 4). A style the GPU cannot draw (a
  texture brush) is rasterised on the CPU into an image covering the command's bounds, then drawn as a
  GPU image command at the same place in the order.
- Each command also paints itself with QPainter. That is the export path, written once per command
  kind instead of once per class.
- Escape hatches: `painter(callback)` for raw QPainter work, and a raw-QRhi command split into
  `prepare()` and `render()`. Both depend on the view, so they are re-recorded when the view changes.

### 4. Resolving to pixels without losing the pan fast path

- Recordings hold positions in their own space. Resolving them to pixels gives the geometry the
  backends draw (extruded GPU vertices, painter paths).
- The resolved geometry is cached with the transform it was built for. A pure pan reuses it with a
  translation, as today's extrusion cache and `pixelOffset` do. A zoom or a log axis re-resolves only
  what is in view (the item index already says what that is).
- Target: pan and zoom of a 1M-point line and of 10 000 items do not regress against
  `docs/plans/neoqcp-3/baseline.md`.

### 5. Stacking: the layer decides, then a fixed order

- Default layers by kind, as decision 9 asks: grid below, data on `main`, annotations (text, labels,
  shapes, spans, tracers) on a new `annotations` layer above `main`. This is a behaviour change for
  2.x programs that stacked items on `main`; `setLayer()` still puts anything anywhere.
- Inside one paint buffer, the order is documented and fixed: QPainter content in child order, then GPU
  geometry in child order. Since the backend of a command depends only on its kind, a pen or brush
  never changes where an object lands.
- No per-run textures: the number of textures and passes stays what it is today.
- Exports follow the same order (QPainter content, then GPU commands' QPainter forms, per buffer), so a
  PDF stacks like the screen. Screen and export are compared on layout, colours and order with a pixel
  tolerance, not byte for byte (MSAA versus QPainter antialiasing).

### 6. One hit-test rule

- `hitTest` returns a pixel distance, or "missed". The default, from the recording: bounds first, then
  distance to segments minus half the pen, 0 inside a visible fill, the rectangle for text and images.
  A million-point object overrides it with its own index.
- The layer's item index is asked first; `hitTest` runs only for candidates.
- Precedence: the mouse grabber, then the axis rect's own interactions (drag, wheel zoom), then
  drawables. Among drawables, the winner is the topmost object within the tolerance **that can take
  the action**: a selection skips non-selectable objects, a press does not. The press and the selection
  therefore agree whenever the topmost object is selectable, and the documented 2.x split stays for
  non-selectable annotations. There is no "no details means anywhere" path any more.

### 7. Legend icon and data range defaults

- The default icon replays the whole recording into the icon rect, through a transform built from the
  recording's data bounds. Classes whose icon is a summary (financial, histogram, multigraph) override.
- The default data range is the recording's data-space bounds.

### 8. Scope of milestone 3

- New: the strong types and transforms, `QCPDrawable`, `QCPDrawContext`, the helpers, the command list,
  the resolve cache, GPU dashes, the CPU-raster fallback, the `annotations` layer, the hit-test rule.
- Ported as proof: `QCPBars` (fills; it overrides `hitTest` and `dataRange` for bar widths and group
  spacing, which shows the override path) and `QCPItemText` (text, rotation, anchor plus offset; a
  missing axis hides it with a debug check, per decision 8). Plus one new example class written only
  against the public API.
- Untouched until milestone 4: graph2, multigraph, colour map 2, histogram 2D and their pipelines
  (milestone 4 wraps their output as shared-storage commands).

## Left to the plan

The command structs, the resolve cache's keys and eviction, the dash shader (phase kept in data units
so a translated buffer keeps it), the CPU-raster size cap, reentrancy during replay (the milestone-1
emission queue), the list of strong-type candidates outside drawing, and the tests: stacking under
every pen and brush style, screen versus export order, hit-test parity between press and selection,
the pan and zoom baselines.
