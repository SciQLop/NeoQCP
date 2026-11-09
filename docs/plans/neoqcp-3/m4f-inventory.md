# M4f inventory — polar on the object model

Repository `/home/jeandet/Documents/prog/NeoQCP-3.0`, branch `neoqcp-3.0`, HEAD `c0c5b59`.
Read-only inventory: no source was edited, nothing was built or run. Every fact below is a
`file:line` read at that HEAD. Line numbers are the `.cpp`/`.h` quoted.

Scope. Milestone 4 moved plottables and items onto the drawing API (recorded commands in
data / pixel / axis-rect-ratio positions, replayed by `QCPTransform`; see
`docs/specs/2026-11-04-neoqcp-3-drawing-api-design.md`, `src/drawable.h`,
`src/painting/draw-context.h`, `src/painting/draw-commands.h`, `src/coords.h`). The polar
classes in `src/polar/` are the last subsystem outside it. Spec commitments:

- `docs/specs/2026-11-02-neoqcp-3-object-model-design.md:324` — answer 7: "Polar plots are
  kept and ported to the new base classes, polar coordinates and transforms included".
- `docs/specs/2026-11-06-neoqcp-3-milestone-4-design.md:61` — M4f is the polar port.
- `docs/specs/2026-11-06-neoqcp-3-milestone-4-design.md:105` — answer 10: the polar graph is a
  plottable, the polar axes live on the axis and transform model, selection is unified as in M4a.
- `docs/specs/2026-11-06-neoqcp-3-milestone-4-design.md:68-74` — selection call 2: `selectable()`,
  `selected()`, one argument-less `selectionChanged()` per object.

Source sizes (`wc -l`): angular axis 1897 cpp / 567 h; polar graph 1410 cpp / 300 h; polar grid
255 cpp / 179 h; radial axis 1631 cpp / 478 h; `src/coords.h` 239.

## 1. Classes, ownership, creation

| Class | Header | Base | LOC (cpp/h) | Owns |
|---|---|---|---|---|
| `QCPPolarAxisAngular` | `src/polar/layoutelement-angularaxis.h:46` | `QCPLayoutElement` | 1897 / 567 | inset layout, grid, radial axes, graph list |
| `QCPPolarAxisRadial` | `src/polar/radialaxis.h:44` | `QCPLayerable` | 1631 / 478 | nothing (pens/fonts/ticker only) |
| `QCPPolarGrid` | `src/polar/polargrid.h:38` | `QCPLayerable` | 255 / 179 | nothing |
| `QCPPolarGraph` | `src/polar/polargraph.h:73` | `QCPLayerable` | 1410 / 300 | shared data container, legend item? no |
| `QCPPolarLegendItem` | `src/polar/polargraph.h:46` | `QCPAbstractLegendItem` | in graph cpp | `QPointer<QCPPolarGraph>` |

`QCPPolarAxisAngular` (`layoutelement-angularaxis.h:46`) is constructed with
`explicit QCPPolarAxisAngular(QCustomPlot*)` at `layoutelement-angularaxis.cpp:137`, destructor at
`:230`. Ctor defaults: `mInsetLayout(new QCPLayoutInset)` at `:142` (never parented — TODO at
`:202-205`), `mAngle(-90)` at `:148`, `mRange(0, 360)` at `:191`,
`mGrid(new QCPPolarGrid(this))` at `:197`, a `QCPAxisTickerFixed` with a 30 degree tick step at
`:198-207`, `setLayer(currentLayer())` at `:212`, `minimumSize(50,50)`/`setMargins(30)` at
`:222-223`, then `addRadialAxis()` at `:226` and `mGrid->setRadialAxis(radialAxis())` at `:227`.
The class has no `Q_PROPERTY` (empty `\cond` block). Members are declared at
`layoutelement-angularaxis.h:340-400`: `mInsetLayout` `:342`, `mCenter` `:377`, `mRadius` `:378`,
`QList<QCPPolarAxisRadial*> mRadialAxes` `:379`, `QCPPolarGrid* mGrid` `:380`,
`QList<QCPPolarGraph*> mGraphs` `:381`, plus range/angle/reversed/tick vectors/label painter/ticker
and drag state. The destructor (`:230-253`) restores antialiasing after a mid-drag delete, deletes
the grid (`:238`), nulls each graph's `mKeyAxis` (`:243-245`), deletes the inset layout (`:249`),
and removes every radial axis (`:251-252`). It owns exactly: grid, inset layout, radial axes, and
the registration list of graphs (graphs are not deleted).

`QCPPolarAxisRadial` (`radialaxis.h:44`) is a bare `QCPLayerable`, constructed with
`explicit QCPPolarAxisRadial(QCPPolarAxisAngular* parent)` at `radialaxis.cpp:118` as
`QCPLayerable(parent->parentPlot(), "", parent)` plus `setParent(parent)`; its destructor
(`:189`) is empty. Defaults: `mAngle(45)` `:126`, `mAngleReference(arAngularAxis)` `:127`,
`mRange(0, 5)` `:169`, `mScaleType(stLinear)` `:171`, `mTicker(new QCPAxisTicker)` `:176`,
`mRangeDrag`/`mRangeZoom` true at `:120-121`. Geometry (`mCenter`, `mRadius`) is stored at
`radialaxis.h:309-310` but pushed in by the angular axis, not computed. No `Q_PROPERTY`. It is
owned by the angular axis: `addRadialAxis` appends (`layoutelement-angularaxis.cpp:350`),
`removeRadialAxis` does `removeOne` then `delete` (`:382-396`), and the angular destructor removes
all remaining axes (`:251-252`).

`QCPPolarGrid` (`polargrid.h:38`) is a `QCPLayerable` created only by the angular constructor
(`layoutelement-angularaxis.cpp:197`), constructed at `polargrid.cpp:48` with a raw
`QCPPolarAxisAngular* mParentAxis` (`:54`) and a `QPointer<QCPPolarAxisRadial> mRadialAxis`. Ctor
sets `setType(gtAll)` `:59`, `setSubGridType(gtNone)` `:60`, default pens `:62-67`,
`setAntialiased(true)` `:69`. No signals, no selection API, no public factory.

`QCPPolarGraph` (`polargraph.h:73`) is a `QCPLayerable`, constructed
`QCPPolarGraph(QCPPolarAxisAngular* keyAxis, QCPPolarAxisRadial* valueAxis)` at
`polargraph.cpp:150`; it registers itself with `mKeyAxis->registerPolarGraph(this)` at `:167`.
Defaults: `mPeriodic(true)` `:158`, blue pen `:171`, `lsLine`, `stWhole`. Members at
`polargraph.h:187-196`: `QSharedPointer<QCPGraphDataContainer> mDataContainer` `:187`,
`QPointer<QCPPolarAxisAngular> mKeyAxis` `:195`, `QPointer<QCPPolarAxisRadial> mValueAxis` `:196`.
It is explicitly *not* a `QCPAbstractPlottable` and is absent from `QCustomPlot::mPlottables`; the
plot knows it only through `detach`/`adopt` (`src/core.h:501,504`, `src/core.cpp:1488-1518`, where
`adopt` always refuses with "a polar graph is added through its angular axis") and through
`removeObject(QCPPolarGraph*)` (`src/core.cpp:5171`). `core.h:790-812` static_asserts allow taking
a `QCPPolarGraph`. Its destructor (`polargraph.cpp:176-179`) removes itself from
`mKeyAxis->mGraphs`. `interface1D()` returns 0 with a TODO (`polargraph.h:170-174`), and the whole
selection decorator is commented out (`polargraph.h:133,160`; `polargraph.cpp:483-499`).

`QCPPolarLegendItem` (`polargraph.h:46`) is a `QCPAbstractLegendItem` with
`QPointer<QCPPolarGraph> mPolarGraph` (`:57`), constructed at `polargraph.cpp:42`, drawn at `:54`,
sized at `:87`, and styled at `:104-115`. It reuses the base class's selectable/selected API
(`src/layoutelement-legend.h:67-85`).

Handles. Every polar class has a `QCP...Ops` handle deriving from `QCPLayerableOps`:
`QCPPolarGraphOps` (`polargraph.h:253`), `QCPPolarAxisAngularOps` (`layoutelement-angularaxis.h:~430`),
`QCPPolarAxisRadialOps` (`radialaxis.h:~355`), `QCPPolarGridOps` (`polargrid.h:~120`). They cover
the setters listed in the API below and, for angular, `add<QCPPolarGraph>`.

Creation paths. Angular: `new QCPPolarAxisAngular(plot)` then `plotLayout()->addElement(row, col)`
(`docs/snippets/polar.cpp:10-12`) or `plot.add<QCPPolarAxisAngular>(row, col)` via
`Construct<LayoutPart>` (`src/construct.h:154-170`; tested at `tests/auto/test-builder/test-builder.cpp:1119`).
Radial: `new QCPPolarAxisRadial(angular)` passed to `addRadialAxis`, or the convenience
`angular->addRadialAxis()` (`layoutelement-angularaxis.cpp:226,350`). Grid: automatic in the angular
constructor, no public path. Graph: `new QCPPolarGraph(angular, angular->radialAxis())`
(`docs/snippets/polar.cpp:29,57`) or `angular->add<QCPPolarGraph>(args)`
(`layoutelement-angularaxis.h:259-283`), which `QCP_CHECK_OR_RETURN`s `plotTakesNewParts`
(`:270`, defined `layoutelement-angularaxis.cpp:299`) and a non-null `radialAxis()`, makes a handle,
marks dirty, and calls `setData` inside a `try`/`catch` that removes the graph and rethrows
(`:274-281`). Legend row: `addToLegend` (`polargraph.cpp:632`) or automatic via
`autoAddPlottableToLegend` in `registerPolarGraph` (`layoutelement-angularaxis.cpp:1869-1890`).

## 2. Drawing

`QCPPolarAxisAngular::draw(QCPPainter*)` (`layoutelement-angularaxis.cpp:666-711`) draws the
background (`drawBackground`, `:1554`) and then the baseline circle in the base pen, subtick radial
lines from `mSubTickVectorCosSin`, ticks, and `QCPLabelPainterPrivate` labels; the last label is
skipped when it lands within a few degrees of the first (`:700-710`). It reads `mRange`, `mAngle`,
`mSelectedParts`, the pen/font members, `mCenter`/`mRadius` (set by `update`) and the tick vectors.
Antialiasing hint at `:660` (`aeAxes`); `selectionCategory()` is `iSelectAxes` (`:713`).

`QCPPolarAxisRadial::draw` (`radialaxis.cpp:1472-1537`) draws a baseline spoke from `mCenter` at
`mAngle` (plus the angular axis angle when `mAngleReference == arAngularAxis`), subticks/ticks via
`coordToRadius`, and labels anchored `artTangent`; a label landing on the angular axis is skipped
(`:1522-1536`). Tick vectors are built in `setupTickVectors` (`:1539`), geometry arrives through
`updateGeometry(center, radius)` (`:1438`). `selectionCategory()` is `iSelectAxes` (`:1622`).

`QCPPolarGrid::draw` (`polargrid.cpp:187-214`) draws angular spokes at the parent's
`mTickVectorCosSin`/`mSubTickVectorCosSin`, radial circles at `mRadialAxis`'s
`tickVector`/`subTickVector`, and the zero-radius ellipse; helpers at `:216` (radial) and `:246`
(angular). Antialiasing hint `:177` (`aeGrid`).

`QCPPolarGraph::draw` (`polargraph.cpp:734-813`) returns early if either axis is null, the angular
range is empty, the data is empty, or both line and scatter are off (`:737-745`), clips with
`painter->setClipRegion(mKeyAxis->exactClipRegion())` (`:747`), splits the data into selected and
unselected `QCPDataRange`s, builds lines, and per segment draws fill (`drawFill`, `:907`), line
(`drawLinePlot`, `:881`) and scatters (`drawScatterPlot`, `:921`). No selection decorator is drawn
(the hooks at `:786-812` are commented). Helpers: `drawPolyline` `:1071`, `getVisibleDataBounds`
`:1123`, `getLines` `:1179`, `getScatters` `:1206`, `getOptimizedLineData` `:1237`,
`getOptimizedScatterData` `:1359`, `dataToLines` `:1394`. It reads the data container, both axis
ranges, the reversed flags, `mPeriodic`, the `QCPDataSelection`, pen/brush/scatter style, and the
plot's `selectionTolerance`.

Legend icon: `QCPPolarGraph::drawLegendIcon` (`polargraph.cpp:934`) fills a stripe, strokes the
line and draws a scatter — the same shape as `QCPGraph`. The legend row draws itself at
`polargraph.cpp:54`.

GPU and export. There is no GPU path: no `QCPDrawContext`, no command recording, no
`plottableRhiLayer`, no `rhiLayer` anywhere in `src/polar/`. `QCPLayer::draw` just calls
`child->draw(painter)` for every layerable (`src/layer.cpp:211-243`), and the export path
`QCustomPlot::draw` walks layers the same way (`src/core.cpp:4276-4292`, `toPainter` at `:5127+`).
Polar therefore paints immediately on screen, in export and in any `toPixmap`/`toPainter` call, and
never enters the deferred GPU pass. Selection plumbing for it is the generic layer path
(`src/layer.cpp:745-1080`).

## 3. Coordinates

Angle mapping is in the angular axis: `coordToAngleRad` (`layoutelement-angularaxis.h:295-300`)
computes `mAngleRad + (coord - lower) / size * (±2π)` with the sign from `mRangeReversed` and no
wrap; `angleRadToCoord` (`:302-307`) is the inverse. The angular axis's `pixelToCoord`
(`layoutelement-angularaxis.cpp:501`) and `coordToPixel` (`:513`) simply delegate to
`mRadialAxes.first()`.

Radius mapping is in the radial axis. `pixelToCoord` (`radialaxis.cpp:1173`) takes
`QCPVector2D(pixel - mCenter)`, converts the length through `radiusToCoord` and the angle through
the angular axis's `angleRadToCoord`. `coordToPixel` (`:1184`) is
`mCenter + (cos, sin) * coordToRadius(coord)`. `coordToRadius` (`:1192`) is linear, or
`ln(coord/lower) / ln(upper/lower) * radius` for `stLog`, honours reversal, and returns a point
±200 px outside the range when the coordinate cannot be mapped. `radiusToCoord` (`:1220`) is the
inverse.

Centre and radius are computed in `QCPPolarAxisAngular::update(UpdatePhase::Layout)`
(`layoutelement-angularaxis.cpp:597-630`): `mCenter = mRect.center()`,
`mRadius = 0.5 * min(|width|, |height|)` clamped to at least 1, then every radial axis gets
`updateGeometry(center, radius)` (`:628-629`). The clip region is an ellipse `QRegion` built from
the centre and radius (`exactClipRegion`, `:398`).

Callers. Every call to these conversion functions is inside `src/polar/`: the graph
(`draw`, `dataToLines`, `getScatters`, `coordsToPixels`, `pixelsToCoords`, `selectTest`,
`pointDistance`), the grid (`drawRadialGrid`), the angular/radial drag and wheel handlers, and
`rescale`. Tests use `angular->center()`/`radius()` only. Nothing outside `src/polar/` calls a
polar conversion (grep over `src/`, `tests/`, `examples/`).

Comparison with `QCPTransform` (`src/coords.h:112-190`). The transform snapshots two `QCPAxis`
objects (vertical flag, logarithmic/reversed/usable flags, origin, length, lower/upper, rect,
slope/intercept, log slope/intercept, positive flag), keeps `QPointer`s and a shared link, and
offers `map(QCP::Data)`, batch `map(span keys, span values)` / `map(span QPointF)`,
`fitted(QCPDataBox, QRectF)` for legend icons, `isTranslationOf` for pan detection, `keyIsVertical`,
`valid()` and `operator==`. `QCPRatioTransform` (`src/coords.h:~200-237`) snapshots a
`QCPAxisRect` and maps axis-rect ratios. A polar transform needs:

- a snapshot of centre and radius (from the angular `QRect`), the angular range/lower/upper,
  `mAngleRad` and reversal (angle is always linear in pixels along the circle), and the radial
  range, scale type and reversal;
- forward `map(angle, radius) -> QPointF` and an inverse used by hit testing, plus a batch form;
- a clip that is a circle rather than a rect (`exactClipRegion` at `:398` is the existing recipe);
- an equivalence test that is rotation + radial scaling about the centre, not pixel translation, so
  `isTranslationOf` as written does not generalize; a polar recording would need either an
  always-remake policy on range change or its own equivalence predicate;
- a position type. `QCP::Data` (`src/coords.h:66` region) is a key/value pair, which is exactly
  (angle, radius); reusing it avoids a new variant, a dedicated `PolarData` keeps the meaning.

## 4. Axes model

`QCPAxis` (`src/axis/axis.h:118`) is a `QCPLayerable` with `Q_PROPERTY`s (`:123-171`), a
`QCPAxisPainterPrivate` that owns all tick/label geometry (`:489`), an `axisRect()` (`:448`),
`linkTo`/`moveTarget`/`link` (`:416-421`), `scaleRatio` (`:410`),
`pixelToCoord`/`coordToPixel` (`:412-413`), `rangeChanged`/`scaleTypeChanged` (`:438-440`), and the
usual selection surface.

The polar axes duplicate: `QCPRange`, a shared `QCPAxisTicker`, tick/subtick vectors, pens, fonts,
a label painter, `setRange`/`moveRange`/`scaleRange`/`rescale`, a `rangeChanged(const QCPRange&,
const QCPRange&)` signal, `selectedParts`/`selectableParts`, `selectionCategory() == iSelectAxes`,
the `selectTest` shape, and the drag/zoom arithmetic. They lack: any `QCPAxis` base, `Q_PROPERTY`s,
`axisRect()`, orientation, `linkTo`/`moveTarget`/`scaleRatio`, `QCPAxisPainterPrivate`, axis
endings, and automatic margin calculation. The angular axis gets only what `QCPLayoutElement` gives
it (rect and margins, `layoutelement-angularaxis.h:311-328`); the radial axis is a bare layerable
whose labels are drawn where the geometry says and can overlap anything.

Drag and zoom. `QCPPolarAxisAngular::mousePress` (`layoutelement-angularaxis.cpp:1710-1723`)
snapshots the angular range and every radial range into `mDragRadialStart`; `mouseMove`
(`:1741-1798`) converts the start and current pixels to (angle, radius), applies the angular linear
difference to the angular range and a linear difference (linear scale) or ratio (log scale) to each
radial range, with `QPointer` liveness guards. `wheelEvent` (`:1824-1867`) zooms the angular range
and every radial range about the cursor by `qPow(rangeZoomFactor, wheelSteps)` and queues a replot.
The radial axis's own drag and zoom are disabled: `mousePress` only sets `mDragging`
(`radialaxis.cpp:1340-1350`), `mouseMove` is commented out (`:1360-1377`), `wheelEvent` is
commented out (`:1428-1435`). Both handlers read `parentPlot()->interactions()` for
`QCP::iRangeDrag`/`QCP::iRangeZoom`; there is no `QCPAxisRect` pointer anywhere in polar and no
`rangeDragAxes`.

Relationship with `QCPAxisRect`: none. The angular axis takes the axis rect's place: `docs/polar.md:19-24`
says to clear the plot layout and add the angular axis, and the tests do exactly that
(`tests/auto/test-qcustomplot/test-qcustomplot.cpp:649-650`,
`tests/auto/test-repaint/test-repaint.cpp:1402`). The inset layout member is an unwired stub
(`layoutelement-angularaxis.cpp:202-205`).

## 5. Selection and signals

The M4a surface is already present in shape. `QCPPolarGraph` has
`selectable()`/`selected()`/`selection()`/`setSelectable`/`setSelection`/`emitSelectionChanged`
(`polargraph.cpp:323-374`) and `selectionChanged`/`selectableChanged` signals
(`polargraph.h:179-181`). The angular and radial axes have the part-based variant
`selectedParts()`/`selectableParts()`/`selected()`/`selectable()`/`setSelectedParts`/
`setSelectableParts` (angular `layoutelement-angularaxis.cpp:860-905`, radial
`radialaxis.cpp:331-376`) with `selectionChanged`/`selectableChanged` (`layoutelement-angularaxis.h:333-337`,
`radialaxis.h:390-394`). `QCPPolarLegendItem` inherits the base item's
`selectable`/`selected`/`selectionChanged` (`src/layoutelement-legend.h:67-85`). There is no
`isSelected()` anywhere, matching ruling R-B (`docs/plans/neoqcp-3/milestone-4a-rulings.md:27-31`).

Every emit uses the M4a helpers, no bare `emit` in `src/polar/`: `qcpEmit` for
`QCPPolarGraph::selectableChanged` (`polargraph.cpp:332`) and `selectionChanged` (`:374`);
`qcpEmit` for the angular axis (`layoutelement-angularaxis.cpp:865,893`) and the radial axis
(`radialaxis.cpp:336,364`); `qcpEmitCurrent` for `QCPPolarAxisRadial::scaleTypeChanged`
(`radialaxis.cpp:288`, inside a `QCPEmissionScope` because `setScaleType` sanitizes the range);
`qcpEmitIf` for the deferred `rangeChanged` of the angular axis (`layoutelement-angularaxis.cpp:1895`)
and the radial axis (`radialaxis.cpp:1629`). `rangeChanged` uses one `(newRange, oldRange)`
signature. This matches rulings 7 and 8 (`docs/plans/neoqcp-3/milestone-4a-rulings.md:29-31`).

Hit testing. `QCPPolarGraph::selectTest` (`polargraph.cpp:690-712`) refuses when
`onlySelectable && style == stNone`, when the data is empty, or when an axis is null; it requires
`mKeyAxis->rect().contains(pos)` (the square cell, not the circle) and calls `pointDistance`
(`:988-1043`), which converts ±`selectionTolerance` into an angular range and takes the nearest
data point or line segment. Details are a single-range `QCPDataSelection`.

`QCPPolarAxisRadial::selectTest` (`radialaxis.cpp:1267-1280`) calls `getPartAt`
(`:1247-1264`), which always returns `spNone`, so it always returns -1: radial click selection is
dead code. `QCPPolarAxisAngular::selectTest` (`layoutelement-angularaxis.cpp:556-595`) returns -1
when `onlySelectable` is set and otherwise returns `selectionTolerance * 0.99` for any point inside
`mOuterRect` with no details; its `getPartAt` (`:535-554`) also always returns `spNone` (TODO).
No axis part can be selected by clicking, only by calling `setSelectedParts` manually.
`QCPPolarGrid` inherits the base `selectTest` (-1) and reports `iNone`. The graph's category is
`iSelectPlottables` (`polargraph.cpp:814`).

## 6. Data

`QCPPolarGraph` stores `QSharedPointer<QCPGraphDataContainer>` (`polargraph.h:187`), i.e.
`QCPGraphData` key/value pairs, and includes `../plottables/plottable-graph.h` at
`polargraph.h:34`. Overloads: `data()` (`:125`), `setData(QSharedPointer<...>)`
(`polargraph.cpp:392`, no null check), `setData(keys, values, alreadySorted)` (`:411`, clears then
`addData`), `addData(keys, values, alreadySorted)` (`:446`), `addData(key, value)` (`:468`). There
is no polar data type of its own and no dependency on the `QCPGraph` class itself beyond its data
classes and shared header.

`mPeriodic` defaults to true (`polargraph.cpp:158`). `getVisibleDataBounds` (`:1123`) uses the whole
container when periodic and `findBegin`/`findEnd` over the angular range otherwise.
`getOptimizedLineData` (`:1237`) clips the radius to the range ±5% plus the pen margin and inserts
dummy angle points to avoid tangents at the clip (TODO for log axes and thick lines at `:1243`, for
an inner radius limit at `:1276`).

## 7. Users

Tests, `tests/auto/`:

- `test-qcustomplot`: nine slots at `test-qcustomplot.h:43-51` (angular/radial range slots deleting
  the axis, radial drag/wheel deleting a radial or the angular axis, angular drag/wheel deleting the
  angular axis, antialiasing restore); bodies at `test-qcustomplot.cpp:617-800` with the
  `polarPlot` helper (`:646-660`: three radial axes [0,10], [0,100], [0,1000], layout cleared then
  angular added), `polarPoint` `:661`, `dragRadially` `:666`, `wheelOnPolar` `:674`.
- `test-ownership`: `test-ownership.h:50` `polarRemoveGraphUnlistsFirst`, `:52`
  `deletePolarGraphInLegend`, `:55` `movePolarGraphKeepsOneLegendRow`; bodies at
  `test-ownership.cpp:753,879,957` (axis teardown at `:931-955`).
- `test-repaint`: `test-repaint.h:38` `polarPlotSettles`, body `test-repaint.cpp:1396-1417` (a
  background pixmap must not dirty inside `drawBackground`); polar rows in the setter/repaint tables
  at `:1115-1143` (`setTickLabelMode`, `setSelectedSubTickPen`, grid `setRadialZeroLinePen`),
  `:1169-1196` (`setAngle`, `setNumberFormat`, `setBackground`), `:1262-1268`
  (`setTickLabelColor`, `setBasePen`, grid `setAngularPen`).
- `test-builder`: slots at `test-builder.h:61-67` (`addPolar`, `takePolarGraph`,
  `addPolarGraphWithoutRadialAxisIsRefused`, `deletedPolarGraphLeavesNoLegendRow`,
  `takePolarGraphRefusals`, `takenPolarGraphOutlivesItsPlot`, `orphanedPolarGraphCanBeTaken`),
  bodies `test-builder.cpp:1119-1290`.

Benchmarks: `tests/benchmark/benchmark.cpp` has no polar row.

Examples and manual tests: `examples/plots/mainwindow.cpp:1386` `setupPolarPlotDemo` (declaration
`mainwindow.h:82`), reachable through case 20, which is commented out at `:81` and dispatched at
`:112`; it builds one angular axis, one radial axis, two graphs, a subgrid and non-default tick label
modes. `tests/manual/mainwindow.cpp:1283` `setupPolarAxisTest` (`mainwindow.h:59`), disabled at
`:55`, and rotates the angular axis on mouse move at `:1668-1671`.

Documentation: `docs/polar.md` in full (pieces, basic snippet, direction/units, radial axis, grid;
states the polar system is a QCustomPlot 2.1 tech preview, one graph type, no selection decoration,
no automatic legend); `docs/snippets/polar.cpp:6-33` `polar_basic` (three graphs, degrees 0-360,
radial 0-1.05) and `:36-61` `polar_radians` (`setAngle(0)`, reversed, 0..2π, `QCPAxisTickerPi`,
six-petal rose with a brush); `docs/snippets/gallery.cpp:469-510` polar gallery scene (two graphs,
dB patterns); `docs/gallery.md:230` (angular axis + graph replace the axis rect);
`docs/index.md:46` links the polar page; `docs/interactions.md:215-218` documents
`selectionChanged`/`selectable`/`selectedParts` for polar axes. `docs/colormaps.md:209` `gpPolar`
is a gradient and unrelated. `examples/wasm-gallery` has no polar scene and no JS binding for it.
NeoQCP docs never state that SciQLopPlots uses the polar classes (SciQLopPlots was not read).

Build registration: `src/qcp.h:103-106` includes all four polar headers; `cmake/sources.cmake`
lists the four polar sources.

## 8. Known bugs, TODOs, and rule gaps

Tech preview: the four cpp doc comments still say it (`layoutelement-angularaxis.cpp:45`,
`polargraph.cpp:39`, `polargrid.cpp:39`, `radialaxis.cpp:40`).

TODOs and dead code:

- inset layout allocated but never parented (`layoutelement-angularaxis.cpp:202-205`);
- `QCPPolarAxisAngular::getPartAt` always `spNone` (`:535-554`);
- `QCPPolarAxisRadial::getPartAt` always `spNone` (`radialaxis.cpp:1247-1264`);
- whole radial `rescale` body commented out (`:1120-1171`), so only graphs rescale through the
  angular axis (`layoutelement-angularaxis.cpp:411-499` iterates `mGraphs`);
- radial drag body commented (`radialaxis.cpp:1360-1377`), radial wheel body commented
  (`:1428-1435`);
- `QCPPolarGraph::interface1D` returns 0 (`polargraph.h:170-174`);
- selection decorator commented throughout (`polargraph.h:133,160`;
  `polargraph.cpp:162,169,180-186,483-499`, draw hooks `:786-812`);
- `getOptimizedLineData` TODOs for log axes and thick lines (`:1243`) and inner radius (`:1276`);
- enum docs marked TODO: `QCPPolarAxisAngular::LabelMode` (`layoutelement-angularaxis.h:73`) and
  `QCPPolarGrid::GridType` (`polargrid.h:47`).

Ownership follows the M1 rules at this HEAD: `removeGraph` unlists before deleting
(`layoutelement-angularaxis.cpp:644-656`), `~QCPPolarGraph` unregisters itself (`polargraph.cpp:176-179`),
the angular destructor nulls graphs' key axis and deletes grid/inset/radials (`:230-253`), and
`removeRadialAxis` removes then deletes (`:382-396`). `docs/plans/neoqcp-3/ownership-inventory.md:181-185,249,253`
is stale on these points.

Misuse handling is inconsistent. `QCP_CHECK_OR_RETURN` appears only at
`layoutelement-angularaxis.h:270,272` (inside `add<T>`) and `polargraph.cpp:283` (`setKeyAxis`).
Everything else `qDebug()`s and returns or silently continues: `addRadialAxis`
(`layoutelement-angularaxis.cpp:355-373`), `removeRadialAxis` (`:382-396`), `removeGraph`
(`:646-650`), `addToLegend`/`removeFromLegend` (`polargraph.cpp:634-648,668-688`),
`registerPolarGraph` (`layoutelement-angularaxis.cpp:1869-1890`), the graph constructor's
cross-plot check (`polargraph.cpp:163-164`), `setValueAxis` (any radial, no validation), and
`setData(QSharedPointer)` (no null check).

`contentChanged()` has zero uses in `src/polar/`: every setter only calls `markDirty()`
(paint-buffer invalidation, `src/layer.cpp:627-632`). Polar classes are not `QCPDrawable`, so there
is no recording to invalidate yet, but every setter will need `contentChanged()` on the port.

Exceptions: only the `add<T>` path catches, removes the half-built graph and rethrows
(`layoutelement-angularaxis.h:274-281`). Nothing else throws.

Signal/dirty gaps: `moveRange`/`scaleRange` (angular `layoutelement-angularaxis.cpp:411-457`,
radial `radialaxis.cpp:1044-1118`) mutate `mRange` and emit `rangeChanged` without `markDirty()`;
`setRangeReversed` marks dirty but emits nothing (there is no reversed signal, matching `QCPAxis`).
`QCPPolarGraph::setKeyAxis` can move a graph to another plot's angular axis — `registerPolarGraph`
only checks that the key axis matches, not the parent plot — and `setValueAxis` accepts a radial
axis from any angular axis.

Selection bugs: radial click selection is dead (`getPartAt` returns `spNone`); the angular
`selectTest` claims the whole outer rect when `onlySelectable` is false; the grid has no selection
API; the graph's hit test clips to the square angular rect rather than the drawing circle; the
legend row only has base-class selection.

## 9. Assessment

Smallest set of steps to put polar on the object model:

1. Add a polar position space and a `QCPPolarTransform` next to `QCPTransform`
   (`src/coords.h`). Snapshot centre, radius, angular range/angle/reversal and radial
   range/scale/reversal; provide forward `map(angle, radius)`, inverse, batch map, `valid()`, and an
   equality that is rotation + radial scale about the centre. Reuse `exactClipRegion`'s ellipse
   (`layoutelement-angularaxis.cpp:398`) as the clip value stored in the recording.
2. Turn `QCPPolarGraph` into a `QCPAbstractPlottable1D<QCPGraphData>` that records its polylines in
   that space (line, fill and scatter segments preserved), keeping the periodic, clipping and dummy
   point logic of `getOptimizedLineData` (`polargraph.cpp:1237`). Its axes stay the two polar axes;
   the type is registered with the plot the normal plottable way instead of `adopt` refusing it
   (`src/core.cpp:1488-1518`).
3. Put the angular and radial axes on a shared axis base that carries the range snapshot, the
   transform, part selection and `contentChanged()`; implement real `getPartAt` on both so click
   selection works, and give the angular axis a real hit test rather than the whole cell.
4. Make the grid a drawable that records its spokes and circles (it already knows the tick vectors).
5. Route creation through the existing builders (`plot.add<QCPPolarAxisAngular>`,
   `angular.add<QCPPolarGraph>`, handles in `src/construct.h`), replay the legend icon
   (`polargraph.cpp:934`) from the recording, and check screen, export and GPU paths all agree.

Open design questions:

- Data-space commands with a polar transform versus pixel recording. The former keeps zoom crisp
  (ranges change without remake) but needs a transform whose equivalence is rotation + scale, and
  angle wrap makes "a range moved slightly" ambiguous near 0/360.
- One polar axis-rect-like layout element owning both axes and the centre/radius (closest to
  `QCPAxisRect` + `QCPTransform`) versus today's angular-element + radial-layerable pair. The
  single-element shape is what the transform snapshot and the margins want.
- Whether the polar axes subclass `QCPAxis` (needs orientation, `QCPAxisPainterPrivate`,
  `linkTo`, and would drag in rect semantics) or a new shared base that both `QCPAxis` and the
  polar axes specialize; the axis-model answer in
  `docs/specs/2026-11-06-neoqcp-3-milestone-4-design.md:105` says axes go on the axis and transform
  model, not necessarily on `QCPAxis` itself.
- How `QCPDataBox` and data ranges are defined for a periodic angular axis (wrap-point duplication
  or an explicit guard), and where the circular clip lives (transform value versus per-command).
- Whether `selectionCategory` stays `iSelectAxes`/`iSelectPlottables` or gains a polar part enum.

Baseline on the old code, before the port. Reference images: `docs/snippets/polar.cpp`
`polar_basic` and `polar_radians`, the `docs/snippets/gallery.cpp:469-510` scene, `examples/plots`
case 20 (`mainwindow.cpp:1386`), and `tests/manual` `setupPolarAxisTest`
(`tests/manual/mainwindow.cpp:1283`) including one rotated-axis frame. Benchmarks: there is no
polar row today (`tests/benchmark/benchmark.cpp`), so add a 10k-point polar graph draw, an angular
rotation pan, a radial zoom, a replay-versus-remake comparison, an export (`toPainter`) case, and a
draw with axes only (no graph) to separate axis cost from graph cost.
