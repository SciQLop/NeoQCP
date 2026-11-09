# Coordinate-system inventory — where two QCPAxis are assumed

Repository `/home/jeandet/Documents/prog/NeoQCP-3.0`, branch `neoqcp-3.0`, HEAD `c0c5b59`.
Read-only inventory: no source edited, nothing built or run. Every line number is a read at that
HEAD.

Scope. Maintainer decision 2026-11-09: NeoQCP 3.0 gets a **public coordinate-system abstraction**.
A plottable holds a coordinate system that gives it its drawing frame (data to pixels and back),
its clip, its hit area and its axes, instead of holding two `QCPAxis`. The cartesian coordinate
system wraps today's axis rect plus key/value axis pair; a polar one wraps the angular and radial
axes (`docs/plans/neoqcp-3/m4f-inventory.md`). Users must be able to write their own (ternary,
Smith chart) without touching NeoQCP. The cartesian path must stay as fast as today.

This file maps every place the abstraction touches. Sections 1-8 follow the brief. Guesses are
marked **(guess)**.

## 1. Plottables: every read of the axis pair

### 1.1 Which classes record and which still paint

The drawing API split matters: a recording plottable already talks to `QCPDrawContext` and only
needs the axes through `QCPTransform`; a painting plottable still reaches for `keyAxis()` directly.

| Plottable | Draws through | Notes |
|---|---|---|
| `QCPBars` | recording | width and stacking in axis-rect pixels |
| `QCPColorMap` | recording | cells in axis coordinates |
| `QCPColorMap2` | recording | cells, axis-rect gate |
| `QCPCurve` | recording | painter callback receives `QCPTransform` |
| `QCPErrorBars` | recording | delegates data to `interface1D()` |
| `QCPFinancial` | recording | candle width in axis-rect pixels |
| `QCPHistogram` | recording | bins on the key axis |
| `QCPHistogram2D` | recording | cell grid + contours |
| `QCPIntervals` | recording | lanes indexed by pixel y |
| `QCPMultiGraph` | recording | lane geometry from the rect |
| `QCPStatisticalBox` | recording | box rects |
| `QCPGraph` | `draw(QCPPainter*)` | the 2.x path, still painter-only |
| `QCPGraph2` | `draw(QCPPainter*)` + GPU pipeline | own viewport/offset math |
| `QCPPlottableGroup` | none | no axis use at all |
| polar classes | `draw(QCPPainter*)` | outside `QCPAbstractPlottable` today |

Evidence for the split: `plottable.h:297` states that a class painting through the old virtual
(`QCPGraph`, the polar plottables) overrides it and never records. Each recording class declares
`draw(QCPDrawContext&)` in its own header and defines it in its cpp (`bars.cpp:1016`,
`curve.cpp:500`, `errorbar.cpp:580`, `financial.cpp:624`, `statisticalbox.cpp:598`,
`histogram.cpp`, `histogram2d.cpp`, `colormap.cpp`, `colormap2.cpp`, `intervals.cpp`,
`multigraph.cpp`). `QCPGraph`, `QCPGraph2` and `QCPPlottableGroup` only override
`draw(QCPPainter*)` (`graph.cpp`, `graph2.cpp`, `plottable-group.cpp`).

Axis-read counts give the size of the job per file (pattern `mKeyAxis|mValueAxis|keyAxis()|valueAxis()|axisRect()|coordsToPixels|pixelsToCoords|clipRect`):

```
multigraph.cpp 48   graph2.cpp 46   graph.cpp 44   intervals.cpp 38
plottable.cpp 30    bars.cpp 30     financial.cpp 27  errorbar.cpp 20
histogram2d.cpp 16  colormap2.cpp 15  statisticalbox.cpp 14  draw-utils.cpp 12
histogram.cpp 11    plottable1d.h 11  colormap.cpp 10  plottable.h 8
draw-utils.h 5      curve.cpp 4      colormapped.cpp 1
```

All of them are in `.cpp` bodies or template helpers; the public plottable headers contain no axis
member reads of their own.

### 1.2 The base class

`QCPAbstractPlottable` is `QCPLayerable` + `QCPDrawable` (`src/plottables/plottable.h:118`) with
`Q_PROPERTY` key/value axis (`:122-123`) and `QPointer<QCPAxis> mKeyAxis, mValueAxis`
(`:283`). Getters are `mKeyAxis.data()` / `mValueAxis.data()` (`:146-148`). The constructor takes
`QCPAxis* keyAxis, QCPAxis* valueAxis` (`:136`).

Reads, grouped by what they are for:

| Purpose | Read | Where |
|---|---|---|
| frame construction | both axes, no orientation | `plottable.cpp:514` (`view()`), `drawable.cpp:452` |
| plot and layer parenting | `keyAxis->parentPlot()`, `keyAxis->axisRect()` | `plottable.cpp:455` |
| clip | `mKeyAxis->axisRect()->rect() & mValueAxis->axisRect()->rect()` | `plottable.cpp:1197-1203` |
| hit gate | `axisRect()->rect().contains(pos)` | `plottable.cpp:538` via `qcpDrawnClickInsideAxisRect` |
| data to pixels | `keyAxis->coordToPixel`, `valueAxis->coordToPixel`, orientation swap | `plottable.cpp:870-887`, `:896-907` |
| pixels to data | same pair | `plottable.cpp:921-941`, `:947-950` |
| range queries | neither axis is read: the recorded data box | `plottable.cpp:218-220` (decl), `mCore.dataRange` |
| rescale | one axis pointer + its `scaleType` | `plottable.cpp` `rescaleKeyAxis`/`rescaleValueAxis` |
| selection rect | both axes via `pixelsToCoords` in `QCPAbstractPlottable1D` | `plottable1d.h` `selectTestRect` |
| legend icon | none | `plottable.h:307`, `drawable.cpp:1003` |
| orientation | `keyAxis->orientation()` | `plottable.cpp:873` |
| misuse | both null check | `plottable.cpp:475` |
| cross-plot | both `parentPlot()` | `plottable.cpp:478-484` |
| same orientation | both | `plottable.cpp:485-486` |

The constructor (`plottable.cpp:452-489`) makes the pair load-bearing: a null pair leaves the
object inert (`:471-473`), a mismatch fails a `QCP_CHECK` and moves the plottable to layer null
(`:478-484`), and equal orientations only warn (`:485-486`). `axesChanged()` (`plottable.h:270`)
is called by `setKeyAxis`/`setValueAxis` (`plottable.cpp:743-748`, `:761-766`) but not by the
constructor.

`qcp::ops::setAxes` (raw pair) checks non-null, same plot and perpendicular orientation
(`plottable.cpp:1449-1460`). The typed form resolves both axes on the plottable's current
`keyAxis()->axisRect()` (`:1477-1486`, helpers `:1464-1475`), and `setAxisRect` moves the plottable
to another rect's bottom/left axes (`:1488-1493`). These are the pieces 2.x code uses to move a
plottable between axes without reconstructing it.

### 1.3 Per class

**`QCPGraph`** (painter, `graph.cpp`) is the most axis-entangled. It reads both axes at entry
(`:416`, `:452`), gates the draw on `mKeyAxis->axisRect()->rect().contains` (`:419`), and reorders
key/value according to `rangeReversed` and `keyAxis()->orientation()` in six separate helpers
(`:610-611`, `:675-676`, `:655-682`, `:720-731`, `:764-775`, `:818-829`, `:872-883`, `:938-949`).
`getNonNanSegments` takes `keyAxis()->orientation()` (`:1017`, `:1057`). `getKeyRange`/
`getValueRange` use orientation plus the axis rect's right/left/top/bottom (`:1722-1768`). Channel
fill requires both graphs on the same plot and same orientation (`:1831-1912`). Hit testing maps
through both axes (`:2058-2072`). Genuinely cartesian-only: channel-fill axis sharing, the
vertical-key swap, and using rect edges as fill bounds. Everything else needs "map a point".

**`QCPGraph2`** (`graph2.cpp`) is painter plus its own GPU pipeline. It subscribes to the key
axis's `rangeChanged`/`scaleTypeChanged` (`:47-54`), maps through `coordsToPixels` (`:303-305`),
gates on the axis rect (`:323-328`, `:366-369`, `:392`), computes its GPU offset from
`axisRangeSizeRatio(mKeyAxis, ...)` and `computeViewportOffset` (`:461-470`), builds
`ViewportParams::fromAxes` (`:487`, `:495`, `:551`), reads key log/range/size (`:510-594`),
passes `mKeyAxis, mValueAxis` into `getLinesIndexed(..., :574, :587)`, maps the impulse origin with
`mValueAxis->coordToPixel(0)` (`:650`), and computes `clipRect()` (`:620`, `:624`, `:729`). This
class hard-codes two axes, rect dimensions, log flags and orientation; it will need a real adapter,
not a rename.

**`QCPMultiGraph`** (`multigraph.cpp`) reads the axis rect at `:742` for viewport geometry and at
`:1318`/`:1400` for lane rects. Lanes are laid out in rect pixels, so it is cartesian-only in
layout but only "map a point" per lane.

**`QCPCurve`** (`curve.cpp`) gates `selectTest` on the axis rect (`:424`) and records with a
painter callback that receives a `QCPTransform` (`:500`). It is cartesian only through the gate;
the recording is transform-based.

**`QCPBars`** (`bars.cpp`) computes spacing from `keyAxis()->axisRect()->width()/height()`
(`:369-372`), converts width to axis-rect pixels (`:1255-1261`), reads `context.transform()` in
`draw` with the comment "geometry is in pixels: only a pan keeps this recording good" (`:1064`),
reads the transform again for stacking gaps (`:1072`, `:1078`), branches on orientation
(`:1155-1157`, `:1218`), and uses `mKeyAxis->pixelOrientation()` in `pixelWidth` (`:1245-1249`).
Cartesian-only: axis-aligned bar rectangles, pixel widths, stacking offsets, bars-group spacing.

**`QCPErrorBars`** (`errorbar.cpp`) gates on the axis rect (`:531`), records through a painter
callback (`:580`), and delegates its data to `mDataPlottable->interface1D()` (`:373-433`); it needs
a plottable that exposes `interface1D()` (`:227`). Whiskers are pixels along the value axis, so it
is axis-aligned but only needs "map a point" plus an orientation.

**`QCPFinancial`** (`financial.cpp`) gates on the rect (`:489`), records (`:624`), and computes
candle width in axis-rect pixels (`:900-906`). Cartesian: OHLC rects aligned to key/value.

**`QCPStatisticalBox`** (`statisticalbox.cpp`) gates on the rect (`:502`) and records (`:598`).
Cartesian: axis-aligned boxes.

**`QCPHistogram`** (`histogram.cpp`) reacts to the key axis's `scaleTypeChanged` by resetting the
edges (`:87-88`), reads key log (`:95`), builds `ViewportParams::fromAxes` (`:311`), null-checks
both axes (`:303`, `:342`, `:355`), reads value log (`:373`, `:397`, `:428`), and starts the bars
at `mValueAxis->range().lower` (`:431`). Cartesian value bars; the binner needs key range/log.

**`QCPHistogram2D`** (`histogram2d.cpp`) subscribes to both axes' `scaleTypeChanged` (`:39-42`),
reads both log flags (`:55-56`, `:258-259`), builds `ViewportParams::fromAxes` (`:284`), reads
orientation (`:309`), passes both axes to the contour builder (`:326`), and hit-tests with
`mKeyAxis->pixelToCoord(pos.x())` / `mValueAxis->pixelToCoord(pos.y())` (`:425-429`). Cartesian
cell grid plus axis-aligned contours; genuinely needs a rect.

**`QCPColorMap`** (`colormap.cpp`) hit-tests with `pixelsToCoords` (`:937-940`), branches on key
orientation over the cell ranges (`:1022-1156`), and uses `rangeWithHalfACell(mKeyAxis, ...)`
(`:1156`). Cartesian cells.

**`QCPColorMap2`** (`colormap2.cpp`) reads the axis rect (`:225`) and gates `selectTest` on it
(`:350`). Cartesian cells.

**`QCPColorMapped`** (`colormapped.cpp:155-160`) derives the colour scale from the key axis:
`QCPAxisRect* rect = mKeyAxis ? mKeyAxis->axisRect() : nullptr;` then `rect->colorScale()`. This is
the M4c shared-scale path; it needs the rect, not the pair.

**`QCPIntervals`** (`intervals.cpp`) uses the axis rect as the lane frame at fourteen sites
(`:172`, `:303`, `:334`, `:446`, `:522`, `:653`, `:792`, `:1008`, `:1077`, `:1173`, `:1218`,
`:1242`), builds `QCPTransform now(mKeyAxis, mValueAxis)` (`:1019`), and caches
`mRecordedBarsTransform` (`intervals.h:264`). Cartesian-only: lanes are pixel rows inside the rect.

**`QCPWaterfall`** and **`QCPPlottableGroup`** have zero axis reads.

**`QCPAbstractPlottable1D`** (`plottable1d.h`) is where the 1D data classes share their axis work.
It identities itself to the pipeline with `mDataContainer.get()` (`:104`), returns `this` from
`interface1D()` (`:110`), routes `selectTest` to the recorded path when `mCore.recordsTheDrawing`
(`:473`), otherwise maps `pos ± selectionTolerance` through `pixelsToCoords` and compares against
`keyAxis->range()` / `valueAxis->range()` (`:536-537`), maps each point with `coordsToPixels`
(`:545`), and implements `selectTestRect` by mapping the rect corners to key/value ranges and
using `findBegin`/`findEnd` on the container. Its range queries are data-space; only hit testing
and the selection rect touch the pair.

## 2. Drawable, drawn core, transforms, GPU replay

`QCPDrawnView` (`src/drawable.h:28-43`) is the frame description and is already axis-shaped:
`const QCPAxis* keyAxis`, `const QCPAxis* valueAxis`, `const QCPAxisRect* axisRect` ("the rect the
ratio transform maps"), theme, plot, layer, `QRect clip`, `selectable`, `exporting`.
`QCPItemExtent` (`:48-55`) similarly carries `QCPAxis* keyAxis/valueAxis`, a `QCPDataBox` and a
pixel pad. `QCPDrawable` (`:64-95`) declares the pure `draw(QCPDrawContext&)` plus
`drawnDataRange`, `hitTest`, `drawLegendIcon`, `dataSourceIdentity` (`:80`), `dependencyRevision`
(`:89`) and `resolvesForTheExport` (`:94`).

`qcpDrawnViewOf(plottable)` (`drawable.cpp`; used by `plottable.cpp:514`) simply copies
`plottable.keyAxis()`, `valueAxis`, and `keyAxis ? keyAxis->axisRect() : nullptr`. The item overload
takes the first position's pair and adds the comment "an item mixing axis pairs across its
positions is drawn through the first pair; per-position frames when an item needs them".

`QCPDrawnCore` (`drawable.h:120-`) is the recording cache. `ensureRecording` builds the frame with
`qcpMakeFrame(view.keyAxis, view.valueAxis, view.axisRect, QRectF(clip))` (`drawable.cpp:452`); the
key window comes from `qcpKeyPixelsPerUnit(view.keyAxis)` (`:541`); icons use
`qcpMakeFittedFrame` (`:1003`). `stallOffset` refuses reuse unless both view axes are non-null,
the recording is not stale, and `axisRangeSizeRatio` for key and value each stay within 1e-4
(that is, no zoom since painting); a pure pan goes through
`qcp::computeViewportOffset(key, value, paintedKey, paintedValue)`.

`QCPTransform` (`src/coords.h:112-190`) snapshots exactly two axes. `Snapshot::take`
(`src/coords.cpp:97-118`) reads orientation, logarithmic, reversed, the range, and then
`axis->axisRect()`; with no rect the snapshot is unusable — so a transform today cannot exist
without an axis rect. `isTranslationOf` (`:317-352`) requires both transforms valid, the same key
verticality, no links, no logarithmic axis, the same scale within 1e-3 px across the span, and
measures the delta at `(key.lower, value.lower)`. `QCPRatioTransform` (`coords.h:192-`) snapshots a
`QCPAxisRect` (`left/top/width/height`) and maps `AxisRatio` positions.

`QCPDrawFrame` (`painting/draw-context.h:70-84`) bundles `QCPTransform`, `QCPRatioTransform` and a
`QRectF clip`; `QCPKeyWindow` (`:86-96`) is `QCPRange keys` plus `pixelsPerKey`. `qcpMakeFrame`
(`painting/draw-context.cpp:365-369`) constructs the bundle from (`keyAxis`, `valueAxis`,
`axisRect`, clip). Recordings are transform-independent: reading `transform()` or `clip()` inside
`draw` sets `transformDependent` and forces a remake (documented in `draw-context.h`; `QCPBars` at
`bars.cpp:1064` is the example).

Replay maps per command, not per point: `command-replay-painter.cpp:182`, `:220`, `:257` map
`Position` spans and hand `frame.transform` to painter callbacks (`:333`);
`command-replay-rhi.cpp:451` maps with `transform.map(data, out)`, geometry reuse compares
`transform + delta` (`:608`), `:636` begins the build, image entries at `:666`, `:689`, `:691`.
`draw-commands.h` maps polyline points (`:574`), image quads (`:612`), error-bar centres and ends
(`:640-641`), tangent bases and ahead points (`:661-662`), and computes data boxes with
`qcpDataBox(points, frame.transform)` (`:855`, `:872`, `:877-878`).
`raster-fallback.cpp:131-148` shifts a cached raster only when `isTranslationOf` holds.
`selectiondecorator-bracket.cpp:229-289` goes through `interface1D` only.

The GPU is axis-free by construction: `painting/plottable-rhi-layer.h` takes pixel vertices, a
`QRect clipRect`, device-pixel ratio, output height and offset (`addPlottable :48-59`,
`addImage :61`, image entries `:71-85`). `line-extruder` works in pixels. Anything replacing the
axis pair only has to satisfy the replay stage, not the renderer.

Extents follow the same pattern: `QCPDrawnCore::dataRange`/`dataExtent` use the recorded
`QCPDataBox` (data space) plus a pixel pad, so they never call back into an axis.

## 3. Items and their spatial index

`QCPItemPosition` (`items/item.h:93-206`) stores a key/value axis pair (`:156-158`), an axis rect
(`:160`), a `PositionType` (`:100-125`: `ptAbsolute`, `ptViewportRatio`, `ptAxisRectRatio`,
`ptPlotCoords` "a plot coordinate defined by two axes (see setAxes)"), and exposes
`setAxes(QCPAxis*, QCPAxis*)` (`:173`), `setAxisRect` (`:174`) and `asPosition(bool* snapshot)`
(`:181`).

`item.cpp` requires axes for `ptPlotCoords` and a rect for the ratio types, warning otherwise
(`:449-456`, `:484-491`). `pixelPosition()` (`:729-`) has a fast path that builds
`qcpMakeFrame(mKeyAxis, mValueAxis, mAxisRect, mAxisRect->rect())` and calls
`qcpResolvePosition(frame, asPosition())` (`:737-740`); without that it resolves per side using
`keyVertical = mKeyAxis->orientation() == Qt::Vertical` (`:749`), `plotSlot` (`:798-809`) and
`slotAlongScreenAxis` (`:782`). `asPosition` (`:1066-1094`) turns unanchored plot/ratio positions
into `Data`/`AxisRatio`/`DataXRatioY`/`RatioXDataY` and snapshots anything anchored into
`Pixels`. `setAxes` is a no-op when unchanged (`:827-840`); `setAxisRect` at `:842-850`.
`createPosition` defaults new positions to `plot->xAxis`/`yAxis` and the plot's axis rect
(`:1730`, `:1733`).

Spatial indexing is keyed on the pair. `QCPAbstractItem::positionsExtent` (`:1665-1720`) requires
every position to share the first position's axis pair, builds a `QCPDataBox` from unanchored
`ptPlotCoords` sides (a log axis with non-positive lower leaves the side unbounded) and returns a
`QCPItemExtent{keyAxis, valueAxis, box, pixelPad}`. `dataExtent` (`:1375-1379`) delegates to
`mCore.dataExtent`. In the layer, `collectItemAxes` (`layer.cpp:1159-1186`) walks positions and
parent anchors cycle-safely collecting key/value axes, `rebuildItemAxes` (`:1188-1193`),
`itemsUseAxis` (`:1195-1201`), `noteItemAxes` (`:1203-1208`), `noteItemExtentChanged`
(`:1210-`) and `rebuildItemIndex` (`:1229-1300`) which groups items by `(keyAxis, valueAxis)` pair,
sorts by box and caches `mIndexExtent`. A position type that is neither a pixel nor a cartesian
data point has no pair and therefore no index bucket.

A polar item position would need: a position type or a coordinate-system pointer on the position
(currently `setAxes`/`setAxisRect` only), a data-space extent (angle range plus radius range, with
a wrap rule), a place in the index (probably separate buckets per coordinate system), and a
resolver that maps `(angle, radius)` to pixels through the polar frame instead of
`qcpMakeFrame`.

## 4. Plot-level code that walks plottables by axis

**`QCustomPlot`**. `mPlottables`/`mPlottableGroups`/`mGraphs` are plain lists
(`core.h:519-522`). `add<T>()` goes through `qcp::Construct<T>::make` (`core.h:790-793`); the
`unique_ptr` overload static_asserts plottable, item or `QCPPolarGraph`, `adopt`s and makes a
handle (`:792-802`); `take` is `:805-812`. `addGraph(keyAxis, valueAxis)` defaults to `xAxis()` and
`yAxis()` (`core.h:401`, implementation in `core.cpp` which checks same-plot and constructs
`QCPGraph(keyAxis, valueAxis)`). `axesForAdd` (`core.h:497-498`) picks the first rect's bottom and
left axes and reports misuse when there is none, so `add<T>()` cannot work without an axis rect.
`rescaleAxes` walks `axisRects()`, collects every `axisRect()->axes()`, and calls
`axis->rescale(onlyVisible)` inside a `QCPEmissionScope` (`core.cpp`). `plottableAt<T>`
(`core.h:833-876`) filters by `clipRect().contains(pos)` (`:854-855`) then `selectTest`
(`:859-862`). `detach`/`adopt` (`core.h:499-504`) are the raw ownership hooks; `adopt` refuses
`QCPPolarGraph` (`core.cpp:1488-1518`) and `removeObject` knows `QCPPolarGraph`
(`core.cpp:5171`). The selection rect calls `interface1D()->selectTestRect(rectF, true)`
(`core.cpp:4626-4628`).

**`QCPAxis`** assumes it lives on one `QCPAxisRect` (`axis.h:245`, member `:448`). It exposes its
orientation (`:400`), `pixelOrientation()` (`:404`, "reversed != vertical ? -1 : 1"), the map
(`:412-413`), `rescale` (`:411`), and the reverse lookups `plottables()` (`:425`), `graphs()`
(`:426`), `items()` (`:427`) which scan the plot list for this axis. The destructor unlists from
the rect (`axis.cpp:530-531`); range changes dirty the rect's layers and items (`:701-704`,
`:731-736`); link mapping uses the rect widths (`:926-976`); `scaleRatio` compares this rect with
the other axis's rect (`:1904-1911`); `rescale` (`:1923-`) iterates `plottables()` and chooses
`getKeyRange` or `getValueRange` by comparing the pointer with `plottable->keyAxis()`;
`coordToPixel`/`pixelToCoord` use `mAxisRect` edges (`:1985-2045`). `QCPAxisLink`
(`axis.h:480`) sits next to the range because `coordToPixel` reads it per point.

**`QCPAxisRect`** owns the axes (`mAxes`, `axisrect.h:193`), the range drag/zoom axis sets
(`:79-104`, `:181`), and the shared colour scale (`mColorScale`, `:194`, created on first
`colorScale()` call at `axisrect.cpp:798-811`, `usedColorScale` `:819-821`). `plottables()`
(`:132`) scans the plot list for plottables on either of this rect's axes. Drag/zoom target
`axis->moveTarget()` so linked axes zoom their source (`:1710-1740`, wheel `:1743-1795`, pan
`:1690-1705`). `markAffectedLayersDirty` (`:1797-1830`) collects the rect's axes' layers, grids,
and every plottable whose key or value axis belongs to this rect, plus `dependentLayers()`. The
`QCPAxisRect` is therefore both the layout element and the de-facto coordinate system today.

**Legend.** `QCPPlottableLegendItem` (`layoutelement-legend.h:120-`, ctor `:125`) stores the
plottable, draws through `mPlottable->drawLegendIcon(painter, iconRect)`
(`layoutelement-legend.cpp:392`) and knows nothing about axes; the icon is replayed in a fitted
frame (`drawable.cpp:1003`). `itemWithPlottable` is `:252`/`:860`.

**Selection rect.** `QCPAbstractPlottable1D::selectTestRect` maps the rect corners with
`pixelsToCoords`, turns them into key/value ranges and uses the container; `QCPBars` and
`QCPErrorBars` override it. It needs the map both ways, i.e. the frame, not the axes.

**`QCPPlottableGroup`** is axis-free: visibility, dimming and a legend row, pointers only
(`plottable-group.cpp:81-133`).

**`construct.h`.** `newPlottable` (`:39-52`) constructs `T(keyAxis, valueAxis)` (or the
lane-layout variant). `Construct<Plottable>::make` (`:120-142`) resolves `axesForAdd`, makes the
handle around `new T(...)`, marks dirty and sets data with undo. `Construct<Item>` (`:145-159`)
needs the plot's axis rect; `Construct<Axis>` (`:162-173`) calls
`plot.axisRect()->addAxis(type)`; `Construct<LayoutPart>` is `:177-196`. So both `add<T>()`
families bake in "the plot has an axis rect" and "a plottable is constructed from an axis pair".

**Colour scale.** `QCPAxisRect::colorScale`/`usedColorScale` (`axisrect.h:137`, `:221`;
`axisrect.cpp:798-821`); `QCPAbstractColorMapped::followKeyAxisRectScale` reads
`mKeyAxis->axisRect()->colorScale()` (`plottable-colormapped.cpp:153-176`); users live in
`layoutelement-colorscale.h:125`; `QCPColorScale` owns a private
`QCPColorScaleAxisRectPrivate` (`colorscale.h:41-60`) with `axis()` `:94`, `type`/`dataScaleType`
`:96-100`, and pointers `mAxisRect/mColorAxis/mOwnerRect` `:146-150`. The scale hangs off the rect,
not off the plottable, so every coordinate system that wants a colour scale needs an equivalent
owner (or a deliberate "cartesian only" declaration).

**Alignment groups.** `src/alignment-group.h` has zero mentions of `QCPAxis`, `axisRect` or
plottables: groups operate on layout parts only.

**Linked axes (M4d).** There is no separate file: `QCPAxisLink` lives in `axis.h`/`axis.cpp` and is
snapshotted by `QCPTransform` (`coords.h:157-169`); `isTranslationOf` refuses linked transforms
(`coords.cpp:325-327`); `QCPAxis::moveTarget`/`pixelOnMoveTarget` are used by rect zoom
(`axisrect.cpp:1725-1735`). Any new frame must keep a way to express "this coordinate is linked to
that axis" or declare links cartesian-only.

## 5. Public API surface that names the axes

Constructors taking `(QCPAxis* keyAxis, QCPAxis* valueAxis)` are declared in: `bars.h`,
`colormap.h`, `colormap2.h`, `colormapped.h`, `curve.h`, `errorbar.h`, `financial.h`, `graph.h`,
`graph2.h`, `histogram.h`, `histogram2d.h`, `intervals.h`, `line.h`, `multigraph.h`, `plottable.h`,
`plottable1d.h`, `statisticalbox.h`, `waterfall.h`, the polar headers, plus `item.h`/`item.cpp`
(`QCPItemPosition::setAxes`), the datasource headers and `algorithms.h`, `layer.cpp`,
`core.{h,cpp}` and `construct.h`.

Handles: `QCPPlottableOps` (`plottable.h:366-410`) exposes `setAxes(AxisType, AxisType)` by role
(`:390-394`), `setAxisRect(QCPAxisRect*)` (`:396-399`) and the raw pair as an escape hatch that
"goes away with raw axis getters" (`:401-405`); the design comment is at `:383-388`.
`QCPLayerableOps` is in `handle-ops.h`.

Signals that move axes: `QCPAxis::rangeChanged`/`scaleTypeChanged` (`axis.h:438-440`); the
plottable's `selectionChanged`/`selectableChanged`/`busyChanged`/`visuallyBusyChanged`
(`plottable.h:267-270`) are axis-free.

Text surface: `setKeyAxis`/`setValueAxis` appears 73 times — 18 in docs, 55 in tests, 0 in
examples. Docs: `docs/migrating.md`, `docs/plans/neoqcp-3/api-incoherences.md`, `backlog.md`,
`coordsys-inventory-brief.md`, `m4f-inventory.md`, `milestone-2-setters.md`,
`ownership-inventory.md`. Tests: `test-errorbar-geometry`, `test-handle`, `test-histogram1d`,
`test-multigraph`, `test-ownership`, `test-pipeline`, `test-qcpaxisrect` and others. Constructor
calls appear in `docs/snippets/*.cpp` and `examples/plots`; `add<T>` and `addGraph` are the common
creation paths (`core.h:401`, `construct.h`).

## 6. Hot paths: where a per-point virtual would hurt

The mapping is already batched in two places, and both must keep a non-virtual fast path.

1. **Replay.** `QCPTransform::map(span keys, span values, out)` and `map(span QPointF)`
   (`coords.h:130-137`, implementations `coords.cpp`) run plain arithmetic over a span. Replay
   calls them once per command (`command-replay-painter.cpp:182-257`;
   `command-replay-rhi.cpp:451`, `:608`, `:636`, `:666-691`; `draw-commands.h:574`, `:612`,
   `:640-641`, `:661-662`). A virtual call per point here would sit in the innermost loop of every
   paint; a virtual per command would not. The comment in `coords.h` states the batched form is
   several times faster than `coordToPixel` twice per point.
2. **Resampling.** `AffineTransform::fromAxis` (`datasource/algorithms.h:245-290`) precomputes
   scale/offset and inverse from `scaleType`, `range`, `orientation` and the axis rect's
   width/height/left/bottom, with a linear fast path. `PixelPointMap` (`:360-400`) caches two of
   them plus the axis pointers, `isVertical` and `bothLinear`, and its `operator()` uses the inline
   transform when both axes are linear and falls back to `coordToPixel` otherwise.
   `PixelBucketGrid::of(keyAxis)` (`:410-430`) and `linesToPixelsImpl(..., keyAxis, valueAxis)`
   (`:395-410`) build on it. All 1D data sources take the pair: `abstract-datasource.h:145-156`,
   `soa-datasource.h:103-118`, the multi variants (`abstract-multi-datasource.h:25-68`) and
   `algorithms.h:639-790`. `Graph2` calls `getLinesIndexed(..., mKeyAxis, mValueAxis, ...)`
   (`graph2.cpp:574`, `:587`).
3. **Pipelines.** `ViewportParams::fromAxes(keyAxis, valueAxis)` (`async-pipeline.h:27`, cpp
   `:9-22`) copies both ranges, the axis rect's width/height (800x600 fallback) and the log flags;
   the graph resampler and the histogram binner consume it.
4. **GPU uploads** take already-mapped pixel vertices (`plottable-rhi-layer.h:48-85`), and the
   line extruder works in pixels, so nothing per-point crosses into the backend.
5. **Extents** are computed from the recorded data box, not from axes
   (`draw-commands.h:855-878`).

The safe interface shape is therefore: a batch `map` used by replay and resamplers, plus a
rect/rect-equivalent for pixel-width geometry, with no per-point virtual anywhere.

## 7. Polar today, mapped onto the abstraction

From `docs/plans/neoqcp-3/m4f-inventory.md`: `QCPPolarAxisAngular` is a `QCPLayoutElement`
(`layoutelement-angularaxis.h:46`, ctor `layoutelement-angularaxis.cpp:137`) holding
`mCenter`/`mRadius` (`:377-378`) and owning the radial axes, grid, inset layout and graph list;
`QCPPolarAxisRadial` is a bare `QCPLayerable` (`radialaxis.h:44`, ctor `radialaxis.cpp:118`) whose
geometry is pushed by the angular axis; `QCPPolarGrid` is a `QCPLayerable` (`polargrid.h:38`);
`QCPPolarGraph` is a `QCPLayerable` (`polargraph.h:73`), **not** a `QCPAbstractPlottable`, with
`interface1D()` returning 0 (`:170`), painting through `draw(QCPPainter*)` with
`mKeyAxis->exactClipRegion()` (`polargraph.cpp:747`) and storing a `QCPGraphDataContainer`.
Conversions are `coordToAngleRad`/`angleRadToCoord` (angular `h:295-307`) and
`coordToRadius`/`radiusToCoord`/`pixelToCoord`/`coordToPixel` (radial `cpp:1173-1230`), with
centre/radius computed in `QCPPolarAxisAngular::update` (`cpp:597-630`).

Mapped onto the abstraction, the polar pair is already the coordinate system: angular range and
angle are the key side, radial range and scale type the value side, `mCenter`/`mRadius` the frame,
and the ellipse `exactClipRegion()` (`layoutelement-angularaxis.cpp:398`) the clip. What is missing
is everything the abstraction would formalize: no `Position` for `(angle, radius)`, no transform
type, no recording, no plottable registration (`core.cpp:1490-1518` refuses `adopt`), no plottable
list membership, and dead axis selection (`getPartAt` returns `spNone`). Once polar exposes a
frame with `map`/dispatch, `QCPPolarGraph` can become a normal recording plottable and the two
polar axes become the polar coordinate system's axes.

## 8. Assessment

### 8.1 Candidate minimal interface (a proposal, not a decision)

The callers in sections 1-6 need only a handful of capabilities. Names are placeholders.

```
class QCPCoordinateSystem {
public:
  virtual ~QCPCoordinateSystem();
  // frame: the only per-point entry points, batched
  virtual QCPTransform transform() const;            // key/value -> pixels, for replay
  virtual double dataToPixel(QCP::Data) const;       // single point, hit tests/items
  virtual QCP::Data pixelToData(QPointF) const;      // inverse, items/selection rect
  // area and clipping
  virtual QRect clipRect() const;                    // pixels
  virtual bool contains(QPointF) const;              // hit area, may be non-rectangular
  // range queries and rescale
  virtual QCPRange keyRange() const;
  virtual QCPRange valueRange() const;
  virtual void rescale(bool onlyVisible);
  // axes (what QCPAxis::plottables/rescale/legend/listings need)
  virtual QList<QCPAxis*> axes() const;
  // identity for caching/dirty
  virtual quint64 revision() const;                  // or generation
  virtual bool isEquivalentTo(const QCPCoordinateSystem&, QPointF* delta) const; // pan reuse
  virtual bool isCartesian() const;
};
```

Who needs what:

- `QCPAbstractPlottable`: `transform()`/`clip()` for the frame, `dataToPixel`/`pixelToData` for
  the legacy `coordsToPixels`/`pixelsToCoords`, `keyRange`/`valueRange` for ranges and the
  selection rect, `axes()` for `plottables()`/`rescale`/legend, `isEquivalentTo` for
  `stallOffset`/`isTranslationOf`.
- `QCPGraph`, `QCPGraph2`, `QCPBars`, `QCPIntervals`, `QCPMultiGraph`: a rect-equivalent for
  pixel widths, stacking and lane layout, plus orientation for the painter paths. These should
  declare a **cartesian capability** rather than call `axisRect()`.
- `QCPItemPosition`: `dataToPixel`, plus a data-space extent (box in its own coordinates) and a
  bucket key in the item index.
- `QCPAxisRect` range drag/zoom and colour scale: the rect, or a coordinate-system-owned
  equivalent; the rect remains the layout element for cartesian.

### 8.2 What stays cartesian-only, and how a plottable says so

Cartesian-only, per section 1.3: axis-aligned bar/candle/box rectangles (`QCPBars`,
`QCPFinancial`, `QCPStatisticalBox`), cell grids (`QCPColorMap`, `QCPColorMap2`,
`QCPHistogram2D`), pixel-row lanes (`QCPIntervals`, `QCPMultiGraph`), pixel widths and stacking
offsets (bars), channel fill sharing (`QCPGraph`), the vertical-key swap (`QCPGraph`,
`QCPGraph2`), and axis-rect colour scales.

A capability declaration is the honest way to encode this: e.g.
`CoordinateCapabilities { MapPoint, Rect, AxisAligned, TwoAxes, ColourScale }`, defaulting to the
cartesian set for existing classes, with the coordinate system checked at construction and a
`QCP_CHECK`/misuse report otherwise. That keeps a custom coordinate system from silently
receiving a `QCPBars` it cannot place.

### 8.3 Keeping 2.x code working

`QCPGraph(keyAxis, valueAxis)` must keep compressing to "make a cartesian coordinate system from
this pair (and its rect)". `keyAxis()`/`valueAxis()` still return the underlying `QCPAxis*` for
cartesian plottables; for a custom or polar system they return null plus a documented deprecation
(warning on use) — 2.x code that never creates a ternary plot is unaffected. `addGraph()` and
`add<T>()` keep their defaults by constructing the implicit cartesian system from
`axesForAdd()`/`axisRect()`. `setKeyAxis`/`setValueAxis` rebuild that implicit system; a plottable
on an explicit coordinate system reports misuse when asked to change one axis.

### 8.4 Risks

1. **Per-point virtual** in replay/resampling. Mitigation: batch-only `map`, and keep
   `QCPTransform`/`AffineTransform` arithmetic for cartesian.
2. **Rect leakage.** `QCPBars`, `QCPIntervals`, `QCPMultiGraph`, `QCPColorMap*` and
   `QCPHistogram2D` genuinely need a rectangle; the abstraction must expose one (or a
   rect-providing capability) without pretending every system has a rect.
3. **Frame equality and reuse.** `stallOffset`, `isTranslationOf`, `raster-fallback` and
   `frameTransformMatches` compare transforms; a virtual/custom system needs a sound equivalence
   predicate or must always remake (slower but correct).
4. **Selection rect.** `selectTestRect` assumes key/value ranges; a custom system needs an
   equivalent query or must return "no data selection".
5. **Item index.** Buckets are keyed on `(keyAxis, valueAxis)`; a custom system has no pair.
6. **Colour scale ownership** and **linked axes** are rect/axis concepts today.
7. **Polar specifics**: angle wrap, a circular hit area and a circular clip, and the periodic
   data rule.
8. **API surface**: 19 constructors, 3 handle setters and 73 text sites move; keeping the old
   signatures as overloads is what keeps the diff honest.

### 8.5 Suggested ordering (guess on effort, not on facts)

1. Introduce the interface plus a cartesian implementation that wraps `QCPAxisRect` and the pair,
   with `map` delegating to today's `QCPTransform`. No plottable changes; prove that a plottable
   can obtain its frame from it (a parallel `view()` path) and that output is identical.
2. Move `QCPAbstractPlottable`'s frame/clip/map/range accessors onto the coordinate system while
   `keyAxis()`/`valueAxis()` keep returning the cartesian axes. Convert the recording plottables
   one by one (they already go through `QCPDrawContext`; most need no change at all).
3. Convert `QCPGraph` and `QCPGraph2` (the painter paths), keeping the axis-pair constructor as a
   forwarding overload.
4. Convert items (`QCPItemPosition` grows a coordinate-system handle and a data extent) and the
   item index.
5. Convert plot-level walking (`rescaleAxes`, `plottableAt`, selection rect, `construct.h`) and the
   rect's colour scale/range drag to the cartesian implementation.
6. Port polar as the second implementation: a polar `Position`, a polar transform with rotation +
   scale equivalence, a circular clip/hit area, then make `QCPPolarGraph` a recording plottable.
7. Only then document the extension point and add a test that a toy coordinate system (no rect, no
   pair) supports a simple plottable, a hit test and a legend icon.
