# Milestone 6 gaps by theme

The library changes the eleven targets in `examples/user-code/` need, grouped by theme. Each change names the rows of
`m6-vocabulary.md` it covers. This is stage 2's input.

Size, by what the change touches in `src/` and the tests:

- **S**: a few new functions or one file, no existing call sites move.
- **M**: several files, or a mechanical change over many call sites.
- **L**: a change in how a core path works (construction, event dispatch, replay), or a reshape that moves hundreds of
  call sites by hand.

Counts are `grep -o` hits on the 2026-11-14 tree (`src/`, `tests/`, `examples/` without `user-code/`).

## 1. Multi-panel plumbing

| Change | Vocabulary rows | Targets needing it | Size | Breaks today's API? (what) |
|---|---|---|---|---|
| A grid of axis rects in one call, with shared axes, hidden inner tick labels, aligned margins and stretch factors | `setAxisRectGrid` / `axisRect(row, column)`, `QCPAxisRectGridOptions` | peak-fit, shared-x, symlog, spectrogram, annotations | M | No. Additive. `axisRect(int)` keeps its meaning (cell 0, 0 for index 0). The 166 `plotLayout()->addElement` calls in tests keep working |
| Rescale on linked axes fits the union of every linked rect's data | `rescaleAxes` on shared axes | shared-x | S | Behaviour: today the last range set wins across a link (7 `linkTo` hits in `src/`) |
| The rect's own first bottom and left axes | `QCPAxisRect::xAxis()`, `yAxis()` | lollipop, asteroids, brick-breaker, peak-fit, six matplotlib targets | S | No |
| An inset rect placed in the parent's axis-ratio space | `QCPAxisRect::addInset` | inset-zoom | S | No. Wraps `insetLayout()` (11 hits in `src/`) |
| A title above one rect, in its top margin | `QCPAxisRect::setTitle`, `title()` | fill-between | M | No. Needs the top margin to reserve the title's height |

## 2. Scales

| Change | Vocabulary rows | Targets needing it | Size | Breaks today's API? (what) |
|---|---|---|---|---|
| A scale value type (linear, log, symlog) replacing the scale enum on every axis, colour scale and plottable with a data scale | `QCPScale`, `QCPAxis::setScale` / `scale()` | symlog, spectrogram | L | Yes: `QCPAxisBase::ScaleType`, `setScaleType`, `setDataScaleType` go. `ScaleType`: 208 hits in 28 `src/` files, 222 in 41 test files; `stLogarithmic`: 77 + 183; `setDataScaleType`: 31 + 27 |
| `setScale` installs the scale's default ticker | `QCPAxis::setScale` | symlog, spectrogram | S | Behaviour: a log axis no longer needs a separate `setTicker` |
| The symlog mapping and its ticker (ticks on both sides of the linear band) | `QCPScale::symlog` | symlog | M | No. New mapping in the axis coordinate path and a new ticker |

## 3. Drawing API in derived classes

| Change | Vocabulary rows | Targets needing it | Size | Breaks today's API? (what) |
|---|---|---|---|---|
| Drop `Abstract` from the two base families | `QCPItem`, `QCPPlottable` / `QCPPlottable1D<D>` | all five extension targets | M | Yes: class names. `QCPAbstractItem`: 217 hits in 40 `src/` files, 87 in tests. `QCPAbstractPlottable(1D)`: 494 in 60 files, 201 in tests. Mechanical |
| Items and plottables built without a plot or axes; `add<T>(args...)` builds `T(args...)` then binds it to the rect's axes (and, for items, clips and layers it); built-in plottables gain data constructors | `QCPItem` (constructor), `QCPAxisRect::add<T>` | lollipop, asteroids, brick-breaker, callout, peak-fit | L | Yes: every item constructor takes `QCustomPlot*` today (31 built-in ones), every plottable takes `(keyAxis, valueAxis)`. 883 `new QCP<plottable>(` sites and 420 item creation sites move. `add<T>(args)` stops forwarding args to `setData` |
| Range-of-numbers concept and the 1D data constructor | `QCPNumberRange`, `QCPPlottable1D<D>(keys, values)` | lollipop, callout, peak-fit, six matplotlib targets | S | No |
| Indexed read and selection test by index | `QCPPlottable1D<D>::at`, `QCPDataSelection::contains(int)` | lollipop | S | No |
| A pixel length type and its literal | `QCP::PixelLength`, `_px` | lollipop, callout, annotations | S | No |
| New drawing-context helpers: `line`, `circle` (data and pixel radius), `polygon(QPolygonF)`, `polygon({...})`, `rect(QRectF)`, `roundedRect`, boxed `text(a, b, ...)` | the `QCPDrawContext::*` rows | lollipop, asteroids, brick-breaker, callout, peak-fit | M | No. `draw-context.h` plus both replay backends (`command-replay-rhi.cpp` 869 lines, `command-replay-painter.cpp` 381) for circle, rounded rect and wrapped text |
| A data point plus a pixel offset as a position, recorded without a snapshot | `QCP::DataOffset`, `operator-(Pixels, Pixels)` | callout | M | No, but every `std::visit` over `QCP::Position` gains an alternative |
| Call order is screen order inside one recording; items on one layer draw in add order, text included | Call order inside one recording, Item order on a layer | asteroids, callout | L | Behaviour: today text, paths and `painter()` replay in the QPainter pass, under the layer's GPU content. Needs the two passes interleaved per command run |
| Per-datum identity from the recording: hit test, rubber band and `stSingleData` answer per data point | `QCPDrawContext::setDataIndex` | lollipop | L | No new break; replaces hand-written `hitTest` overrides. Selection must be derived from recorded commands |
| `update()` means "record me again"; the layout pass is renamed | `QCPItem::update()` / `QCPPlottable::update()`, `QCPLayoutElement::updateLayout` | asteroids, brick-breaker, callout, peak-fit | M | Yes: `markDirty` (356 hits in 58 `src/` files, 36 in tests), `contentChanged` (10 + 5), and `update(UpdatePhase)` (6 overrides, 16 calls) |
| A legend icon draws in the icon's own frame (`AxisRatio` rebound to the icon rect) | `drawLegendIcon` | lollipop | M | Behaviour: every built-in icon (40 hits in `src/`) stops relying on the fit-to-data-box replay; `QCPBars`' invisible rect goes |
| The recording's data box as a plain rect | `QCPItem::dataRect()` | brick-breaker | S | Yes if `dataExtent()` (24 + 8 hits) is removed, as no-interim-designs asks |

## 4. Annotations and hooks

| Change | Vocabulary rows | Targets needing it | Size | Breaks today's API? (what) |
|---|---|---|---|---|
| Parts of an item from the recording, the topmost hit wins | `QCPDrawContext::setPart` | callout, peak-fit | M | No |
| Mouse handlers on items with a value event (data and pixel positions, part, accept / ignore, grab on accepted press) | `QCPItem` mouse handlers, `QCPMouseEvent` | brick-breaker, callout, peak-fit | L | Yes: items stop overriding `QCPLayerable::mousePressEvent(QMouseEvent*, const QVariant&)` and friends (54 / 33 hits in `src/`). Event dispatch in `core.cpp` changes: an accepted press suppresses the pan |
| An anchor placed by a function returning a position | `QCPItem::createAnchor` | callout | M | Yes: `createAnchor(name, int)` and the `anchorPixelPosition` switches go (42 + 24 hits in 9 `src/` files, every built-in item) |
| A deferred `changed()` signal on every item | `QCPItem::changed()` | peak-fit | S | No |
| Keys as plot signals; the plottable key virtual goes | `QCustomPlot::keyPress`, `keyRelease` | asteroids, brick-breaker, fill-between | M | Yes: `QCPAbstractPlottable::keyPress` is deleted. `QCPIntervals` uses it today (11 hits over 7 files) and must move to the signal or to its own handling |
| The plot takes keys without a click | `QCustomPlot` focus policy | asteroids, brick-breaker | S | Behaviour: `Qt::ClickFocus` (one line, `core.cpp:619`) becomes `Qt::StrongFocus`; the plot joins the Tab chain |
| Zoom inset indicator: a box over the inset's ranges and two connectors to its nearest corners, read at draw time | `QCPAxisRect::zoomIndicator`, `QCPItemZoomIndicator`, `QCPShapeOptions` | inset-zoom | M | No |
| One annotation item: text plus a connector to a target, with head, shrink and five connector shapes | `QCPAxisRect::annotation`, `QCPItemAnnotation`, `QCPAnnotationOptions`, `QCPConnector` | annotations | L | No. Five path builders (arc, curve, elbow, arms, bracket), rounded corners, shrink at both ends, a box-edge start |

## 5. Call-site friction

| Change | Vocabulary rows | Targets needing it | Size | Breaks today's API? (what) |
|---|---|---|---|---|
| A line in one call with named options | `QCPAxisRect::line`, `QCPLineOptions` | callout, peak-fit, shared-x, symlog, fill-between, spectrogram, annotations | S | No |
| An ellipse and a text item in one call, each corner in its own space | `QCPAxisRect::ellipse`, `QCPAxisRect::text`, `QCPShapeOptions` | callout, annotations | S | No. Built on `QCPItemPosition::set()` |
| A colour map in one call: gradient, data scale, colour bar on or off, data range fitted on the first data | `QCPAxisRect::colorMap`, `QCPColorMapOptions` | inset-zoom, spectrogram | M | No. Joins the rect's colour scale or detaches it |
| Short named-option calls for every other built-in type (bars, curve, ...) | none: the controller's "whole library" rule | none | M | No |

## 6. Other

| Change | Vocabulary rows | Targets needing it | Size | Breaks today's API? (what) |
|---|---|---|---|---|
| A fill-between plottable: two lines (or components), two brushes by side, cut at exact crossings, broken at gaps, following both lines' data | `QCPAxisRect::fillBetween`, `QCPFillBetween`, `QCPFillOptions` | fill-between | L | Yes: `QCPGraph::setComponentFillBetween` and `setComponentFillToGraph` are deleted (14 hits in `src/`, 103 in tests over 6 files, 4 in `examples/plots`). `setComponentFill(column, value)` stays |

## Decisions for the maintainer

1. **Fill between two lines.** Chosen: `QCPFillBetween` is the only one, and the graph's component fills between lines
   are deleted. Alternative: keep `setComponentFillBetween` beside the new plottable.
2. **Scales.** Chosen: a `QCPScale` value type replaces the `ScaleType` enum everywhere. Alternative: keep the enum and
   add `stSymlog` with a threshold setter.
3. **Scale and ticker.** Chosen: `setScale` also installs the scale's default ticker; a later `setTicker` wins.
   Alternative: the ticker stays separate, as today.
4. **Item construction.** Chosen: items are built without a plot and placed by `add<T>`, so anchors can be made in
   member initialisers. Alternative: keep the plot argument and have `add<T>` pass it.
5. **Plottable construction.** Chosen: built-in plottables gain data constructors and `add<T>(args)` always means
   constructor arguments. Alternative: `add<T>` keeps forwarding its arguments to `setData` for built-in types.
6. **Keys.** Chosen: the plot emits `keyPress` / `keyRelease` signals and the plottable key virtual is deleted.
   Alternative: virtual key handlers on items and plottables, with a focus item.
7. **Focus.** Chosen: `Qt::StrongFocus` by default. Alternative: keep `Qt::ClickFocus` and let the targets call
   `setFocus()`.
8. **Legend icon frame.** Chosen: `QCP::AxisRatio` means a fraction of the icon rect inside `drawLegendIcon`.
   Alternative: a separate `QCP::FrameRatio` type.
9. **"Draw me again".** Chosen: `update()`, Qt's name, and the layout pass becomes `updateLayout`. Alternative: keep
   `markDirty()` and leave the layout pass alone.
10. **Item mouse events.** Chosen: QGraphicsItem's rule, an accepted press grabs the drag and no interaction flag is
    needed. Alternative: a flag such as `iDragItems` gates item drags.
11. **Selected look.** Chosen: a user type reads `selection()` in `draw()` and records its own selected look; the
    decorator does not restyle it. Alternative: the decorator restyles commands tagged with a selected data index.
12. **Call order.** Chosen: screen order inside a recording and add order on a layer, by interleaving the GPU and
    QPainter passes. Alternative: keep the two passes and put text on a higher layer (the spec's anti-goal).
13. **Shared-axis rescale.** Chosen: a linked axis fits the union of the linked rects' data. Alternative: today's
    "last range set wins".
14. **Grid of rects.** Chosen: `setAxisRectGrid` is the one way, with no `addAxisRect`; a second call deletes rects
    outside the new grid. Alternative: an `addAxisRect(row, column)` beside it.
15. **Connector names.** Chosen: `QCPConnector` with Qt-flavoured names (`arc` is matplotlib's `arc3`, `arms` is its
    `arc`). Alternative: matplotlib's names verbatim.
16. **Named-option calls.** Chosen: short calls for every built-in type, per the controller's whole-library rule.
    Alternative: only the ones the targets use (`line`, `colorMap`, `ellipse`, `text`, `fillBetween`, `annotation`).
17. **Data box.** Chosen: `dataRect()` returns a plain `QRectF` and `dataExtent()` goes. Alternative: keep the
    optional-returning `dataExtent()` beside it.

## Proposed order of stage-2 sub-milestones

1. **6a, renames:** `QCPItem`, `QCPPlottable(1D)`, `update()` / `updateLayout`. Mechanical, and every later step
   writes the new names.
2. **6b, construction:** plot-free constructors, `add<T>(args)` unification, data constructors, `QCPNumberRange`,
   `xAxis()` / `yAxis()`, `line` / `text` / `ellipse` shortcuts.
3. **6c, drawing context:** the new helpers, `PixelLength`, `DataOffset`, call order and item order.
4. **6d, identity and hooks:** `setDataIndex`, `setPart`, `QCPMouseEvent` and handlers, function anchors,
   `changed()`, `dataRect()`, key signals and focus, legend icon frame. Lollipop, asteroids, brick-breaker, callout
   compile.
5. **6e, multi-panel:** the rect grid, shared rescale, `setTitle`, `addInset`. Shared-x and peak-fit compile.
   **Done 2026-11-18** (`milestone-6e-rulings.md`); shared-x, symlog and spectrogram compile and have reference
   pictures; peak-fit also needs 6d.
6. **6f, scales:** `QCPScale`, symlog and its ticker, `colorMap` with options. Symlog and spectrogram compile.
   **Done 2026-11-17** (`milestone-6f-rulings.md`); symlog and spectrogram compile once 6e lands (they need
   `setAxisRectGrid`).
7. **6g, fill and zoom indicator:** `QCPFillBetween` (deleting the component fills), `zoomIndicator`. Fill-between
   and inset-zoom compile.
8. **6h, annotations:** `QCPItemAnnotation` and `QCPConnector`. Annotations compiles.

## Gate (2026-11-14)

The maintainer read the eleven targets and the page and answered "let's try like this": the targets stand as
written and the 17 decisions above are taken as chosen. Stage 2 follows the proposed order.

Stage 1 controller rulings:

- Five naming minors of the Task 2 review (two meanings of `add<T>`, two ways to reach axes, `setDatum`, the `keyPress`
  clash, "Abstract" dropped for items only) were fixed at once, since Task 3 reused the vocabulary.
- The shared-x "one call" reading became the single grid call (`setAxisRectGrid` with share options).
- Equal panel widths in the spectrogram target (colour bar beside one panel) are checked on its picture in stage 2.
- `QCPFillBetween` absorbs the graph's component fills (decision 1).
- No separate whole-branch review for stage 1: each task had a fresh review and the maintainer gate is the last one.

Deferred minors: the Asteroids rules (~117 lines) and the peak-fit solver (~60 lines) are long; the legend-icon
frame meaning of `QCP::AxisRatio` (decision 8); `docs/writing-a-plot-type.md` still names the non-existent
`QCPDrawn<Base>` (rewritten in 6c/6d).
