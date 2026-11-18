# SciQLopPlots migration to NeoQCP 3.0 — what to port

The port happens once NeoQCP 3.0 is finished, not milestone by milestone. This file collects what
each milestone changed that SciQLopPlots will meet, so nothing has to be rediscovered then. Each
milestone adds its items here when it ends.

The full list of changes is `changelog.txt`, "NeoQCP 3.0 (in progress)", one section per milestone
(the "Removed" and "Behaviour changes for 2.x users" parts). Below are the ones known to touch
SciQLopPlots, with the places found on 2026-11-09 (SciQLopPlots branch feat/api-fuzzing, 8b456e70;
line numbers will drift).

## Known compile breaks

| Change (milestone) | Use instead | Where in SciQLopPlots |
|---|---|---|
| `QCPItemHSpan` / `QCPItemVSpan` deleted (4e) | `plot.add<QCPItemSpan>(Qt::Horizontal / Qt::Vertical)`; same `range()`, `setRange()`, `lowerEdge`, `upperEdge`, `rangeChanged` | `src/SciQLopMultiPlotPanel.cpp` ~823-908 (preview spans, `qobject_cast<QCPItemVSpan*>`), `include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp` ~45, 70, `src/SpanTargets.cpp` ~52-54 (`SpanItem<QCPItemVSpan>`, `SpanItem<QCPItemHSpan>`): 16 references in all |
| Line colour API renamed (4c): `setColorGradient`, `setColorRange`, `setColorScaleType` | `setGradient`, `setDataRange`, `setDataScaleType` | `src/SciQLopLineGraph.cpp` ~229-237 |
| A scale is a value (6f): `setScaleType(bool ? QCPAxis::stLogarithmic : QCPAxis::stLinear)`, `scaleTypeChanged`, `dataScaleType()` and `setDataScaleType()` | `setScale(log ? QCPScale::log() : QCPScale::linear())`, `scaleChanged`, `dataScale()`, `setDataScale(...)` | `src/SciQLopPlotAxis.cpp` ~138, ~676-737, `src/SciQLopTimeColoredCurve.cpp` ~102-123, `src/SciQLopLineGraph.cpp` ~205-231, `src/SciQLopHistogram.cpp` ~409, `src/SciQLopHistogram2D.cpp` |
| `QCPHistogram2D::Normalization` (`nNone`, `nColumn`) removed (4c) | `setColumnNormalized(bool)` / `columnNormalized()` | `src/SciQLopHistogram2D.cpp` ~229-238 |
| `QCPHistogram::edgesChanged()` removed (4c) | `histogramReady()` | `src/SciQLopHistogram.cpp` ~124 |
| `setSelectable(bool)` on items, legend rows, text elements (4a) | `setSelectable(QCP::stWhole / QCP::stNone)` | `src/SciQLopCrosshair.cpp` ~99-115, `src/SpanTargets.cpp` ~43 |
| Selection signals lost their arguments (4a) | connect to `selectionChanged()` and read `selected()` / `selection()` | any connection to a NeoQCP `selectionChanged(bool)` or `selectionChanged(const QCPDataSelection&)`; SciQLopPlots' own `SciQLopPlotPanelInterface::selectionChanged(bool)` is its own signal, check what feeds it |
| `rangeChanged` has one signature `(newRange, oldRange)` (4a) | connect to that one | any single-argument connection |
| `QCPAbstractItem::draw(QCPPainter*)` is final (4e) | override `draw(QCPDrawContext&)` | none found today; check every `SciQLopPlotItem<...>` subclass again |
| Item constructors lost the plot argument (6b) | `plot.add<QCPItemText>()`, `plot.add<QCPItemSpan>(Qt::Vertical)`; a user item's constructor takes its own arguments, `createPosition()` binds nothing | every `new QCPItem*` (`src/SciQLopCrosshair.cpp` ~94-124, `src/SciQLopNDProjectionPlot.cpp` ~477, `SpanTargets.cpp`), and every `SciQLopPlotItem<...>` subclass that forwards the plot |

## Known behaviour changes to check

- Symlog draws and interacts (6f): `QCPScale::symlog()` maps, pans, zooms, exports and survives a
  polar radial axis like the linear and log scales (`docs/axes.md`, "The scale is a value"). Nothing
  to change in SciQLopPlots: a symlog colour mapping follows it too.
- Each scale brings its ticker (6f): `setScale()` installs a linear, log or symlog ticker, so a
  `setTicker()` made before it is replaced. SciQLopPlots sets its own ticker right after
  `setScaleType()` / `setDataScaleType()` (`src/SciQLopPlotAxis.cpp` ~138, ~737), so it still
  wins; nothing to change, but do not reorder those two calls. The same holds for a colour scale:
  `QCPColorScale::setDataScale()` gives the bar's axis its scale (and so its ticker), so a custom
  bar ticker goes after it, and setting the same data scale again now leaves the ticker alone.
- The data pipeline follows the scale (6f): thinning, L2 bins, the line-cache window, colour-map
  rows, histogram bins and automatic contour levels are equal spans of scale space. Nothing to
  change in SciQLopPlots, which reads the results; only code that calls the `qcp::algo` helpers
  directly moves from bools to `QCPScale` (see `docs/migrating.md`).
- `QCPMarginGroup` was deprecated in 4d and is removed in 5d: see "From milestone 5d" below
  (`src/SciQLopPlot.cpp` ~141). The panel's own `VPlotsAlign` may be replaceable by alignment groups.
- Linked axes and drag/zoom of every axis by default (4d): the time and axis synchronizers
  (`AxisSynchronizer`, `XAxisSynchronizer`, `TimeAxisSynchronizer`) may become `QCPAxis::linkTo`;
  `setupFullAxesBox(true)` now links the top/right axes.
- `QCPGraph` and `QCPGraph2` are deprecated (4b) and deleted in milestone 5; the line family is
  `QCPMultiGraph` (one column = a line) and `QCPCurve`. `SciQLopGraphComponent.hpp` still visits
  `QCPGraph*` in many places.
- New data on a multigraph is shown at once (4b); SciQLopPlots wants the deferred swap:
  `setDeferredDataSwap(true)`.
- `SciQLopTimeColoredCurve` overrides `draw(QCPPainter*)` on a curve
  (`include/SciQLopPlots/Plotables/SciQLopTimeColoredCurve.hpp` ~87). `QCPCurve` records since 4b,
  so this override bypasses the recording (GPU path, hit test, legend icon from the recording): port
  it to `draw(QCPDrawContext&)` or drop it if the curve's per-point colours cover it.
- Items default to the `annotations` layer (3); an item that must sit under or with plottables calls
  `setLayer`.
- `QCPItemTracer` (used for the projection plot's time markers, `src/SciQLopNDProjectionPlot.cpp`
  ~477): a tracer whose graph is deleted is hidden (1); a graph-following tracer now records once and
  follows the graph's data (4e).
- Signals are delivered at the end of the outermost NeoQCP call or event (4a): slots that relied on
  running in the middle of a NeoQCP call (for example during `setRange` or a mouse press) see the
  final state instead.
- Misuse is reported through `QCP_CHECK_OR_RETURN` and is fatal in debug builds (1, 2): a plottable on
  axes of two plots, removing the last layer, a plot being destroyed adding things.
- Colour maps on one axis rect share the rect's scale unless given another (4c): SciQLopPlots' own
  colour-scale handling may simplify.

## How to use this file

When the port starts: grep SciQLopPlots for every name in the first table, fix those, build, then walk
the second list against the SciQLopPlots test suite (`tests/integration`, `unit`, `fuzzing`,
`resampler`, `tracing`). Add to this file whenever a NeoQCP milestone removes or changes something
SciQLopPlots uses.

## From milestone 4i (items on coordinate systems)

- Nothing SciQLopPlots calls changes: it uses `ptAxisRectAbsolute` through `setType`, which keeps its meaning on an
  axis rect. `QCPRatioTransform` and `QCPCoordinateSystem::ratioTransform()` are not used there.
- An item position whose axis or axis rect is gone no longer prints the 2.x qDebug lines on `pixelPosition()`;
  `setPixelPosition()` reports one misuse instead. A SciQLopPlots test that counted those messages would change.

## From milestone 4j (coordinate systems of the user's own)

Grepped on 2026-11-11 (read-only) in `src/` and `include/`: `requiredCapabilities` (none), `addElement(`
(two calls), and `keyAxis()`/`valueAxis()`/`system()` on plottables and item positions.

- **`requiredCapabilities` overrides: none.** No SciQLopPlots plottable overrides the accessor, so the
  stored, non-virtual mask changes nothing there. A future out-of-tree plottable must pass the mask to its
  base constructor.
- **`addElement` across plots: none.** Both calls add a local element to the plot's own layout:
  `SciQLopPlot.cpp` ~687 adds the plot's colour scale, ~722 adds its title. Neither belongs to another
  plot, so the stricter refusal changes nothing.
- **`keyAxis()`/`valueAxis()` on plottables and item positions: no change needed.** They are used only on
  ordinary cartesian plots, and the null cases are already guarded:
  - `SciQLopCrosshair.cpp` ~290-291 loops the plot's plottables and checks both axes before converting.
  - `SpanTargets.cpp` ~131 and `SciQLopStraightLines.cpp` ~56, ~115 return early when the axis is null.
  - `SciQLopPlotAxis.cpp` ~538-571 walks `m_axis->plottables()`; an axis lists only plottables whose 2.x
        projection is that axis, so a plottable on a user system is not in the loop.
  SciQLopPlots builds no custom `QCPCoordinateSystem` and no polar area, so no plottable or item of its
  own sits on a non-cartesian system today. If it later adds one, these sites are the ones to revisit.
- **Dispatch of item creation.** `QCPItemPosition::setAxes()` now applies the cartesian projection gate.
  SciQLopPlots' item positions use `setType(ptAxisRectAbsolute)`/pixel positions and explicit axes on
  cartesian rects, so their `keyAxis()` stays non-null.

## From milestone 5a (the graph grows up)

Grepped on 2026-11-12 (read-only) in `src/` and `include/`: `setData` and `addData` on a
`QCPMultiGraph`, `setGraph`/`setLineGraph`/`setComponent` on a `QCPItemTracer`, and
`requiredCapabilities`.

- **Multigraph data goes in through `setDataSource`, not `setData`.** `SciQLopMultiGraphBase.cpp`
  ~78, ~88 and ~99 build a `QCPSoAMultiDataSource` or a `QCPRowMajorMultiDataSource` from spans and
  call `QCPMultiGraph::setDataSource()`. The 5a sorting and the length/NaN misuses belong to the
  double `setData()`/`addData()` path only, so nothing changes. `addData` is not used.
- **Tracers set no target in code.** The only `QCPItemTracer` creation is
  `SciQLopNDProjectionPlot.cpp` ~477; its time markers are moved with `setPixelPosition`, and
  `setGraph` is never called (the five `setGraph` hits are signals such as `setGraphData`). The new
  `setLineGraph()`/`setComponent()` need no port.
- **`requiredCapabilities`: none.** No SciQLopPlots plottable overrides the accessor. The multigraph
  keeps its cartesian rects and the waterfall is still built on one.
- **The multigraph mask change changes nothing here.** `QCPMultiGraph` requiring `capStraightSegments`
  only, and `QCPWaterfallGraph` keeping the rectangle, matter only on a system of the user's own;
  SciQLopPlots builds none today (see the 4j section).

## From milestone 5b (handles)

Grepped on 2026-11-12 (read-only) in `src/` and `include/`. Getters on plot-owned objects now return
`QCPHandle<T>`; it converts implicitly to `T*`, so only the sites below stop compiling. Line numbers will
drift. Calls on SciQLopPlots' own wrappers (`plottables()`, `legend()`, `axis()` of a `SciQLopPlot...`
class) are not affected.

### Stops compiling: `auto*` from a handle

Fix each by writing `auto` (a handle) or `.get()`.

- `src/SciQLopCrosshair.cpp` ~153, ~206: `auto* axisRect = m_plot->axisRect();`. ~279: `auto* plottable =
  m_plot->plottable(i);`. ~290-291: `auto* keyAxis = plottable->keyAxis();` and `valueAxis()`.
- `src/SciQLopPlot.cpp` ~454: `auto* ar = this->axisRect();`, then ~460 `auto* ax = ar->axis(axisType);`.
  ~490: `auto* colorAxis = m_color_scale->axis();`.
- `src/SciQLopNDProjectionPlot.cpp` ~359: `auto* rect = plot->qcp_plot()->axisRect();`. ~472:
  `auto* layer = qcp->layer("markers");`.
- `src/SciQLopGraphInterface.cpp` ~80-81: `if (auto* plot = p->parentPlot(); ...)` and `auto* item =
  plot->legend->itemWithPlottable(p)`.
- `src/SciQLopPlotGroup.cpp` ~173: `if (auto* canvas = p->parentPlot())`.
- `src/SpanTargets.cpp` ~106: `const auto* rect = edges.lower->axisRect();`. ~131: `auto* axis = cond ?
  edges.lower->keyAxis() : edges.lower->valueAxis();`.
- `src/SciQLopStraightLines.cpp` ~50, ~108 (`axisRect()`) and ~56, ~115 (`keyAxis()` / `valueAxis()` in a
  ternary).
- `src/SciQLopPlotLegend.cpp` ~184: `auto* item = cond ? m_legend->item(row) : nullptr;` (the ternary is a
  handle).
- `src/SpanBase.cpp` ~81, `include/SciQLopPlots/Items/SciQLopPlotItem.hpp` ~84 and ~95:
  `if (auto* l = ...->layer())`.
- `include/SciQLopPlots/helpers.hpp` ~61: `if (auto* plot = item->parentPlot())`.

### Stops compiling: range-for over a list getter

`for (auto* p : axis->plottables())` on a QCP axis: `src/SciQLopPlotAxis.cpp` ~535 and ~557. The list
holds handles, so write `for (auto p : ...)`. The loop at ~455 (`has_image`, a lambda taking
`QCPAbstractPlottable*` inside `std::any_of`) still compiles: the handle converts in the call.

### Stops compiling: `QList<T*>` from a list getter, `QPointer` copy-initialisation

None found. The only `QList<QCP...*>` values (`qcp_plottables_of`, `qcp_plottables()`, `lines()`,
`m_time_markers`) are filled by hand from SciQLopPlots' own components. No `QPointer<QCP...> p = getter`.
No `qcpConnect`, no `delete` of a getter result on a NeoQCP type, no `QSet` or `QHash` of NeoQCP pointers.

### Keeps compiling through the implicit conversion

- `connect(this->xAxis, ...)` and `disconnect(m_impl->xAxis, ...)` (`SciQLopPlot.cpp` ~123-132, ~1809,
  ~1815): the sender is `T*` after the conversion.
- `QSignalBlocker by(m_impl->yAxis)` (~1826) and `m_impl->yAxis->setScaleRatio(m_impl->xAxis, 1.0)`.
- `new QCPIntervals(plot->xAxis, plot->yAxis, layout)` (`SciQLopTimeline.cpp` ~118): the axes convert to
  `QCPAxis*`.
- `parent->xAxis->range()` (`SciQLopLineGraph.cpp` ~251, ~261 and the curve, colour map and waterfall
  twins), `plot && plot->legend`, `plot->legend->itemWithPlottable(...)`.
- `qobject_cast<T*>(getter())`: `SciQLopPlot.cpp` ~605 (`plottableAt`), `SciQLopPlotLegend.cpp` ~50
  (`legend->layout()`), `SciQLopCrosshair.cpp` ~283 and ~297 once `plottable` is a handle, and
  `qobject_cast<SciQLopPlot*>(m_axis->parentPlot())` (`SciQLopPlotAxis.cpp` ~492). The handle test pins
  `qobject_cast` on a handle.
- `plottable->keyAxis() == m_axis` (`SciQLopPlotAxis.cpp` ~541, ~563) and `p->group() != group`
  (`SciQLopPlotGroup.cpp` ~174): comparison with a raw pointer uses the conversion.
- `auto ar = p->qcp_plot()->axisRect();` (`VPlotsAlign.cpp` ~37, ~57): `auto` is a handle and `ar->...`
  works. `auto plot = _multiGraph->parentPlot();` (`SciQLopMultiGraphBase.cpp` ~45) likewise.
- `cond ? plot->legend->itemWithPlottable(_hist) : nullptr` returned as a raw pointer
  (`SciQLopHistogram.cpp` ~151): the return converts.
- `for (auto rect : axisRects())`, `for (auto a : selectedAxes())`, `for (auto item :
  m_impl->selectedItems())` (`SciQLopPlot.cpp` ~317, ~524, ~1675): `auto` takes a handle.
- `selectedGraphs()` and `graphs()` stay raw `QList<QCPGraph*>`, so `for (auto g : selectedGraphs())`
  (~512) and `addGraph(...)` (`QCPAbstractPlottableWrapper.hpp` ~100) need no change.

## From milestone 5c (one graph)

QCPMultiGraph is now QCPGraph, and the old QCPGraph (the container graph) is deleted. Read-only grep of
`/home/jeandet/Documents/prog/SciQLopPlots/src` and `include/` (SciQLop and TSCat not read; line numbers
are those of 2026-11-13). The port is mostly a rename, plus one type that now means something else.

### Rename QCPMultiGraph to QCPGraph (30 sites)

Plain rename, nothing else changes: `QCPMultiGraph::LineStyle` is `QCPGraph::LineStyle` (same enum), and
`component(i)`, `componentCount()`, `setComponent*()`, `setDataSource()`, `setGapThreshold()`,
`setKeyAxis()` and the rest keep their names.

- `include/SciQLopPlots/Plotables/SciQLopGraphComponent.hpp` 7: `MultiGraphRef::graph` `:36`, `to_variant()`
  `:66`, `:92`, `:133` (constructor), `:160` (`static_cast<QCPMultiGraph::LineStyle>`), `:258`, `:352`.
- `src/SciQLopGraphComponent.cpp` 10: `:48`, `:126`, `:153`, `:172`, `:183`, `:193`, `:231`, `:249`
  (`selected_components`), `:295`, `:326`.
- `include/.../SciQLopMultiGraphBase.hpp` 4 (`_multiGraph` `:37`, `create_multi_graph` `:60`, comments);
  `include/.../SciQLopLineGraph.hpp` 3 (`:57-59` `new QCPMultiGraph(keyAxis, valueAxis)`);
  `include/.../SciQLopWaterfallGraph.hpp` 2 (`:34`, comment); `src/SciQLopCrosshair.cpp:297`
  (`qobject_cast<QCPMultiGraph*>(plottable)`); `src/SciQLopMultiGraphBase.cpp:203` and
  `src/SciQLopLineGraph.cpp:56` (comments).
- Includes: `<plottables/plottable-multigraph.h>` is `<plottables/plottable-graph.h>` in
  `SciQLopGraphComponent.hpp:27`, `src/SciQLopCrosshair.cpp:29`, `src/SciQLopPlot.cpp:44`.
- `QCPGroupLegendItem::multiGraph()` is `graph()` (returns a `QCPHandle<QCPGraph>`, which compares with a
  raw pointer): `src/SciQLopGraphComponent.cpp:262`, `:326`, `SciQLopGraphComponent.hpp:97`.
- `QCPMultiGraph`-named helpers that may appear in SciQLopPlots are gone with the rename:
  `QCPMultiGraphOps` and `QCPMultiGraphPipeline` have no uses here.

### The `QCPGraph*` variant alternative now means the multigraph

`SciQLopGraphComponent::graph_or_curve_or_multi` is `std::variant<QCPGraph*, QCPCurve*, MultiGraphRef,
std::monostate>` (`SciQLopGraphComponent.hpp:42`). Both `QCPGraph*` and `MultiGraphRef::graph` are the same
class now. It still compiles, but several visitor lambdas on the `QCPGraph*` alternative no longer do:

- `to_variant()` `:64-70` tests `m_componentIndex >= 0` first and `qobject_cast<QCPGraph*>` second. Every
  graph passes the cast now, so keep that order, or the multigraph components take the wrong branch.
- Compile breaks: the generic `auto* any` lambdas call `any->scatterStyle()`, which QCPGraph has not (read
  `component(i).scatterStyle`; QCPCurve keeps its getter). `:171-176` (`set_marker_shape`), `:194-199`
  (`set_marker_pen`), `:218-223` (`set_marker_size`), `:297`, `:316`, `:334` (the three getters).
- Compile break: `src/SciQLopGraphComponent.cpp:239-241`, `any->data()->dataRange()` on a graph. `data()` is
  gone. Use `QCPDataRange(0, graph->dataCount())`, as the `m_componentIndex >= 0` branch above it does.
- Port: delete the `QCPGraph*` alternative and its lambdas (`:147`, `:179`, `:202`, `:226`, `:272`, `:299`,
  `:317`, `:336`). A plain graph is a one-component graph, `MultiGraphRef { graph, 0 }`. Component 0 exists
  only once data is set, so a getter before the first data needs a default, and a setter before it goes
  through `graph->setScatterStyle()` / `setPen()` / `setLineStyle()`, which new components take.
- `QCPGraph::LineStyle` conversions (`include/SciQLopPlots/qcp_enums.hpp:107-135`, `to_qcp`/`from_qcp`):
  no change. `QCPGraph::lsLine` and the others are the same enumerators, now inherited from the shared
  line base.

### Plain-graph creation: `addGraph`, `selectedGraphs`, `setAdaptiveSampling`

- `include/.../QCPAbstractPlottableWrapper.hpp:100-102`: `std::is_same_v<T, QCPGraph>` with
  `plot->addGraph(keyAxis, valueAxis)`. `addGraph` returns the ops handle of `add<QCPGraph>()` and
  `SciQLopGraphComponent(QCPAbstractPlottable*)` takes a pointer: write `.get()` if the conversion does not
  apply. Nothing instantiates `newComponent<QCPGraph>` (only the time-coloured curve does), so this branch
  is dead and can go with the variant alternative.
- `src/SciQLopPlot.cpp:512` `for (auto g : selectedGraphs())`: `auto` takes a handle, `g->setSelection()`
  works. No change. `selectedGraphs()` also lists multigraphs and waterfalls now, and so
  `clearGraphs()` removes them.
- `src/SciQLopGraphComponent.cpp:36-40`: `qobject_cast<QCPGraph*>` then `setAdaptiveSampling(false)`. It now
  runs for any graph handed to the plain constructor. The setter exists on the new class, so it compiles;
  the multigraph constructor `:48` does not call it, and SciQLopMultiGraphBase graphs keep adaptive sampling.
- `include/.../Resamplers/AbstractResampler.hpp:36-41`: `copy_data` builds `QVector<QCPGraphData>` and has
  no caller in src or include. `QCPGraphData` survives in NeoQCP, so it compiles; delete it with the dead
  branch.
- `setChannelFillGraph`, `QCPGraph2`, `QCPGraphComponent`: no use. `src/SciQLopPlot.cpp:457` mentions
  QCPGraph2 in a comment only. `QCPGraphComponent` is the new name of the component struct behind
  `component(i)`: SciQLopPlots only uses it through `auto`.

### Behaviour changes that reach SciQLopPlots

- A graph's legend row is a `QCPGroupLegendItem` and now derives `QCPPlottableLegendItem`:
  `legend->itemWithPlottable(graph)` finds it (it did not for a multigraph). `src/SciQLopGraphInterface.cpp:81`
  relies on that (it connects the row's `selectionChanged`). `src/SciQLopGraphComponent.cpp` ~330, in the
  `m_componentIndex < 0` branch, tests `dynamic_cast<QCPPlottableLegendItem*>(item)`: a group row matches it
  now, so that branch must also check the row's graph is its own plottable.
- A new graph's default pen is a palette colour, 2.5 wide (2.x: blue, 0). SciQLopPlots sets its own pens.
- `assignSelection` is a new protected virtual on `QCPAbstractPlottable`: none of the SciQLopPlots plottable
  subclasses defines that name.

## From milestone 5d (the rest of the old API)

Read-only grep of `/home/jeandet/Documents/prog/SciQLopPlots/src` and `include/` (SciQLop and TSCat not
read; SciQLopPlots at 8b456e70, line numbers of 2026-11-13). Three compile breaks.

### `QCPMarginGroup` is gone

- `src/SciQLopPlot.cpp:141-143`: `new QCPMarginGroup(this)`, then `axisRect()->setMarginGroup(QCP::msTop |
  QCP::msBottom, ...)` and `m_color_scale->setMarginGroup(...)` for the same sides. Port:
  `QCPAlignmentGroup::create({ axisRect(), m_color_scale }, QCP::msTop | QCP::msBottom);`. The rect and its
  scale sit in one row, so lining up their inner top and bottom edges gives what equal margins gave.
  Alignment groups are per side: the plot's left and right sides stay free for a panel-wide left/right
  group (`VPlotsAlign`'s job today) without leaving this one.

### `SciQLopTimeColoredCurve` paints through the removed painter path

- `include/SciQLopPlots/Plotables/SciQLopTimeColoredCurve.hpp:87`: `void draw(QCPPainter*) override;`.
  `QCPAbstractPlottable::draw(QCPPainter*)` is `final` now: this no longer compiles.
- `src/SciQLopTimeColoredCurve.cpp:217`, `:222`, `:232-233`, `:292-293` read `mKeyAxis` / `mValueAxis`: they
  are private now.
- `src/SciQLopTimeColoredCurve.cpp:295` calls `applyScattersAntialiasingHint(painter)`: deleted.
- Port, preferred: drop the override and give the curve per-point colours (`setColorValues()` with the
  times, plus a gradient: colour by scalar, 4c); the GPU path, hit test and legend icon then come from the recording. Otherwise move the code to
  `draw(QCPDrawContext&)`: read `keyAxisRaw()` / `valueAxisRaw()` instead of the members, and put the
  painter code in a `context.painter()` callback that sets its own antialiasing
  (`painter->setAntialiasing(...)`) instead of the deleted hint.
- `:254 drawPolyline` is QPainter's own, not the deleted 1D helper: nothing to port there.

### Span pens are gone

- `src/SciQLopMultiPlotPanel.cpp:824` and `:838`: `vspan->setPen(...)` / `preview->setPen(...)` on the
  preview spans. That pen was never drawn (since 4e). Port: `setBorderPen(...)` if the preview should
  show its edges in that colour, else delete the lines.
- `src/SpanBase.cpp:36-37`: `setPen(QPen(Qt::NoPen))` and `setSelectedPen(QPen(Qt::NoPen))`. Delete both:
  a span has no pen.

### Behaviour changes that reach SciQLopPlots

- On a HiDPI screen, the painted layers' thin lines (axes, ticks, legend borders, colour-scale outline)
  are one logical pixel wide, as the GPU layers already were. Plottables and grids are unchanged.
- An alignment group settles in the replot that needs it (up to three extra layout passes), so a
  freshly shown or exported panel is aligned at once. A layout that never settles stops after one
  follow-up replot and reports a misuse (fatal in debug).

## From milestone 5e (the meta-object surface)

SciQLopPlots reads no QCP property by name today, so nothing here stops compiling. The rows matter only to code
that goes through `QObject::property`, `QMetaProperty` or a `QBindable(obj, "name")`.

### Properties off the surface

A read-only property that moves with no change signal is no longer a `Q_PROPERTY` (E-11). Its accessor stays: call
it instead of `property("name")`.

| class | property | read instead |
|---|---|---|
| `QCPAxisBase` | `tickVector`, `tickVectorLabels` | `tickVector()`, `tickVectorLabels()` |
| `QCPAxis` | `grid` | `grid()` |
| `QCustomPlot` | `plotLayout` | `plotLayout()` |
| `QCPLayer` | `index`, `children` | `index()`, `children()` |
| `QCPLayerable` | `parentPlot`, `parentLayerable` | `parentPlot()`, `parentLayerable()` |
| `QCPLayoutElement` | `layout`, `rect` | `layout()`, `rect()` |
| `QCPLayoutGrid` | `rowCount`, `columnCount` | `rowCount()`, `columnCount()` |
| `QCPAbstractLegendItem` | `parentLegend` | `parentLegend()` |
| `QCPBars` | `barBelow`, `barAbove` | `barBelow()`, `barAbove()` |
| `QCPErrorBars` | `data` | `data()` (a shared container, mutated in place with no signal) |
| `QCPItemPixmap` | `aspectRatioMode`, `transformationMode` | `aspectRatioMode()`, `transformationMode()` |

### Signals

- Every writable property has a change signal now, `xChanged(T)` carrying the new value, delivered after the
  plot's deferred signals like the others (4a). The existing signals keep their signatures.
- `QCPAxis::axisType` and `axisRect` are `CONSTANT`.
- `QCPLayerable::setMouseTransparent` moved out of line.
- `QCPAxis::setTickLabelRotation` compares the clamped angle: a write past ±90° when the axis is already at the
  bound changes nothing and repaints nothing.
- A grid's stretch factor signals also fire when rows or columns are added, inserted or collapsed
  (`expandTo`, `insertRow`, `insertColumn`, `simplify`, `addElement`).
- `QCPColorScale::labelChanged` is the colour axis's: `axis()->setLabel(...)` fires it too.
- A plottable's `keyAxisChanged` / `valueAxisChanged` also fire when `setSystem` moves its axes.
- `QCPAbstractLine::scatterStyleChanged` also fires on `setComponentScatterStyle(0, ...)`: `scatterStyle()` is
  component 0's.
- `QCPErrorBars::dataPlottableChanged(nullptr)` fires when a refused `setDataPlottable` detaches the old one.
- `QCPScatterStyle` has an `operator==`. Equal writes of an outlier style, an error bars' data plottable or an
  axis no longer repaint or re-register. An equal selection decorator is refused, as before, and signals
  nothing.
- The span signals (`brushChanged`, `borderPenChanged`, `movableChanged` and the selected ones) are declared on
  `QCPAbstractSpanItem`, where the setters are; `QCPAbstractSpanItem::setMovable` moved out of line.
- `QCPAbstractItem::clipAxisRectChanged` also fires on `setClipArea` when the clip axis rect moves.
- Writing `QCPItemPixmap::scaled` through the property (a binding, `setProperty`) resets the two modes to their
  defaults, as it did before; `scaledChanged` fires only when `scaled` moves.
- `QCPLayer::parentPlot` and `name` are `CONSTANT`.
- `QCustomPlot::themeChanged()` (new, no value) fires when `setTheme` installs another theme and whenever the theme
  emits `changed()`: the seven `theme*` properties notify through it. The theme keeps its direct `changed()`.
- `viewportChanged` also fires during an export, which sets the export size and restores the screen viewport.
- The plot setters `setBackgroundScaled`, `setBackgroundScaledMode`, `setAutoAddPlottableToLegend`,
  `setSelectionTolerance`, `setNoAntialiasingOnDrag`, `setMultiSelectModifier` and `setBackground(QPixmap)` return
  on an equal value.

### New properties (Task 3)

The one-argument setter/getter pairs that had no `Q_PROPERTY` are properties now, each with an `xChanged(T)`
signal. One rename breaks the source of a subclass (the **Break** row below); two new virtuals change the
vtables of `QCPAbstractPlottable` and `QCPAbstractItem`, so compiled subclasses must be rebuilt (ABI). The other
rows are the behaviour that moved with the new properties.

- Axes and polar classes: `QCPAxisBase::rangeDraggable` / `rangeZoomable`, `QCPAxis::labelRenderer`, every pair of
  `QCPPolarAxisAngular`, `QCPPolarAxisRadial` and `QCPPolarGrid`, `QCPPolarArea::rangeZoomFactor`, and
  `QCPPolarGraph::periodic` / `lineStyle` / `scatterStyle`.
- `QCPAxisBase::setRangeDraggable` / `setRangeZoomable`, `QCPPolarArea::setRangeZoomFactor` and
  `QCPPolarGraph::setScatterStyle` return on an equal value. `QCPPolarArea::setRangeZoomFactor` moved out of line.
- The polar axes' `setTickLabelRotation` compares the clamped angle, like `QCPAxis`'s.
- The polar axes report what they draw: their default number format is `"gbc"` (beautiful powers, cross sign),
  the label painter's default. The radial axis used to report `"gb"` while drawing the cross. The drawing does
  not change; `numberFormat()` of a default radial axis does.
- The plot: `creationModeEnabled`, `creationModifier`, `bufferDevicePixelRatio`, `antialiasedElements`,
  `notAntialiasedElements`, `interactions`, `plottingHints`, `selectionRectMode`, `selectionRect`, `theme`
  (through the existing `themeChanged()`), `skipReplotsWhenHidden`, `itemIndexing`, `dataSwapDebounceMs`,
  `dataSwapMaxWaitMs` and `currentLayer`. `itemCreator` and `itemPositioner` stay off (`std::function`).
- The plot's `setInteractions`, `setPlottingHints`, `setCreationModifier`, `setSkipReplotsWhenHidden`,
  `setDataSwapDebounceMs`, `setDataSwapMaxWaitMs` and `setCurrentLayer` return on an equal value (the clamped one
  for the two data-swap times). `setInteraction` goes through `setInteractions`.
- `setAntialiasedElement(s)` and `setNotAntialiasedElement(s)` can move both sets; each set that moved signals.
- `currentLayerChanged` also fires when `removeLayer` (or deleting the layer) moves the current layer to its
  neighbour. The `QString` overload of `setCurrentLayer` signals through the pointer one.
- `core.h` includes `theme.h`: the `theme` property needs the complete type.
- Items: `QCPItemRichText::cornerRadius`, `QCPItemRSpan::keyRange` / `valueRange` and `QCPItemSpan::range`
  (through their existing signals), `QCPItemTracer::component` and `QCPAbstractItem::clipArea`.
- The span pen, brush and movable properties are declared once, on `QCPAbstractSpanItem`, instead of on each span.
  Name lookups are unchanged.
- `QCPItemRichText::setCornerRadius` moved out of line, returns on an equal value and repaints.
- `QCPItemTracer::componentChanged` also fires when `setGraph(graph, component)` moves the component.
- Layout and overlay: `QCPLayoutElement::autoMargins`, `QCPGroupLegendItem::expanded` / `selectedComponent`,
  `QCPTextElement::textFlags`, `QCPOverlay::collapsible` / `collapsed` / `opacity` / `font` and
  `QCPSelectionRect::pen` / `brush`.
- `QCPGroupLegendItem::setSelectedComponent` moved out of line, returns on an equal value and repaints; a click
  on the row goes through it. `QCPOverlay::setOpacity` / `setFont` and `QCPSelectionRect::setPen` / `setBrush`
  return on an equal value (the clamped one for the opacity).
- `QCPOverlay::setCollapsible(false)` on a collapsed overlay signals `collapsibleChanged` and `collapsedChanged`.
- Plottables: `QCPAbstractPlottable::system` / `wantsLegend`, the colour map 2's gap threshold and contours, the
  graph's `deferredDataSwap` / `gapThreshold`, `QCPPlottableGroup::name` / `visible`, the histograms' binning,
  style and contours, the intervals' stacking, overlap, style, selected rows and editing, and the waterfall's
  offsets, normalisation and gain. The shared data containers (`data` of bars, curve, financial, statistical box,
  polar graph) and `busy` stay off.
- **Break:** the protected virtual hook `QCPAbstractPlottable::systemChanged()` is now `systemDidChange()`; the
  name is the `system` property's signal. A subclass overriding the hook fails to compile (`override`) until
  renamed.
- `systemChanged(nullptr)` also fires when `setKeyAxis` / `setValueAxis` leave the plottable without a system.
  `take` signals `systemChanged(nullptr)`, `keyAxisChanged(nullptr)` and `valueAxisChanged(nullptr)` for what
  it clears.
- `QCPPlottableGroup::visibleChanged` fires whenever `visible()` moves: `setVisible`, a member shown or hidden on
  its own, a member added or removed, and a member leaving through the plot (`take`, `removePlottable`, a plain
  delete).
- `QCPIntervals::selectedRowsChanged` fires with every selection change that moves the rows.
  `emitSelectionChanged()` (protected) is virtual for it (ABI). `StackMode`, `Style` and `EditModes` are registered
  (`Q_ENUM` / `Q_FLAG`). `setForbidOverlap` moved out of line.
- `setContourLevels` and `setAutoContourLevels` of the colour map 2 and the 2D histogram signal `contourLevelsChanged`
  only when the levels move; `setContourPen` returns on an equal pen. `QCPHistogram::setStyle` and
  `setColumnColors`, `QCPPolarGraph::setScatterStyle` return on an equal value.
- Value types (E-7): `QCPScatterStyle`, `QCPLineEnding` and `QCPColorGradient` have gadget properties (no signal;
  `QMetaProperty::readOnGadget` / `writeOnGadget`). The tickers, label painter, selection decorators, item
  position and painter have none.
- The 3.0 classes (section 3.3): `QCPCoordinateSystem::area`, `keyAxis`, `valueAxis` and `capabilities` are
  `CONSTANT`; `revision` is read-only with `changed()` as its NOTIFY (a binding on it connects to `changed()`, so
  the system starts signalling). `QCPArea::insetLayout` and `QCPAlignmentGroup::sides` are `CONSTANT`. The system's
  transform and plottables, the area's systems, the group's elements and an axis's link stay off.
- The span range signals (`keyRangeChanged`, `valueRangeChanged`, `rangeChanged`) also fire when an edge
  position moves: the handle's `setLeftEdge` and friends, and a raw `span->leftEdge->setCoords(...)`. A position
  calls the new protected virtual `QCPAbstractItem::positionMoved(position, before)` after its coordinates move,
  when its item sets the protected flag `mPositionsSignal` (the spans do; ABI). For such an item,
  `QCPItemPosition::setType`, `setParentAnchor` and `set(QCP::Position)` hold an emission scope.
- **Fixed:** the antialiasing setters kept the two sets apart with `|= ~` (2.x). Forcing an element on put every
  other element into the never-antialiased set. Now forcing an element on takes only that element out of the
  other set.

### Macros (Task 4)

- New `Q_ENUM`: `QCPIntervals::HitPart`, `EditMode`, `SnapMode`; `QCPItemSpan::HitPart`; `QCPItemRSpan::HitPart`;
  `QCPPipelineScheduler::Priority`; `QCPItemCreationState::State`. `QCPIntervals::EditModes` was already a `Q_FLAG`
  (Task 3). Registering an enum gives it a `QMetaEnum` and the name `Class::Enum`; nothing is renamed.
- **Out, unchanged:** `QCPHitAction` (global `enum class`, not in a meta-object scope; moving it into `QCP` would break
  `hitsAt` callers, and no property or invokable takes it), `QCPTransform::Kind`, the protected
  `QCPAxisTickerDateTime::DateStrategy`, `QCPRawColumn::Type`.
- **Deleted `Q_DECLARE_METATYPE` lines (32):** the enums moc registers through a `Q_ENUM` in the same class:
  `QCPAxisBase::ScaleType`, `SelectablePart`; `QCPAxis::AxisType`, `LabelSide`; `QCPAxisTicker::TickStepStrategy`;
  `QCPAxisTickerFixed::ScaleStrategy`; `QCPAxisTickerPi::FractionStyle`; `QCPAxisTickerTime::TimeUnit`;
  `QCPLabelPainterPrivate::AnchorMode`, `AnchorSide`; `QCPColorGradient::ColorInterpolation`, `NanHandling`,
  `GradientPreset`; `QCustomPlot::LayerInsertMode`, `RefreshPriority`; `QCPItemBracket::BracketStyle`;
  `QCPItemTracer::TracerStyle`; `QCPItemPosition::PositionType`; `QCPLayer::LayerMode`;
  `QCPLayoutElement::UpdatePhase`; `QCPLayoutGrid::FillOrder`; `QCPLayoutInset::InsetPlacement`;
  `QCPLegend::SelectablePart`; `QCPLineEnding::EndingStyle`; `QCPPainter::PainterMode`; `QCPBarsGroup::SpacingType`;
  `QCPBars::WidthType`; `QCPFinancial::ChartStyle`; `QCPPolarGrid::GridType`; `QCPScatterStyle::ScatterProperty`,
  `ScatterShape`; `QCPSelectionDecoratorBracket::BracketStyle`.
  The registered type name is the same scoped name (a test checks all 32), so `QMetaType::fromName("QCPAxis::AxisType")`
  and queued connections by name keep working, and `qMetaTypeId<QCPAxis::AxisType>()` still compiles: Qt 6 builds the
  id from the moc registration.
- **Kept:** `QCPCurve::LineStyle` (an alias of `QCPAbstractLine::LineStyle`; the line is inert on Qt 6.12, which
  names the registered type from the compiler: the name rows of the test pin the registered names), and the four
  non-enums `QCPDataSelection`, `QSharedPointer<QCPAxisTicker>`,
  `QCPSelectionDecorator*`, `QCPIntervalEdit`.
- `Q_INVOKABLE` on these actions (one overload each; `QMetaObject::invokeMethod` can call them by name, no signature
  changed). `QCPAxisBase`: `moveRange(double)`, `scaleRange(double)`, `rescale(bool)`, `unlink()`. `QCustomPlot`:
  `requestRepaint()`, `settle()`, `requestDataSwap()`, `removePlottable(int)`, `clearPlottables()`,
  `removePlottableGroup(QCPPlottableGroup*)`, `removeGraph(int)`, `clearGraphs()`, `removeItem(int)`, `clearItems()`,
  `addLayer`, `removeLayer`, `moveLayer`, `savePdf`, `savePng`, `saveJpg`, `saveBmp`. `QCPLayout`: `takeAt(int)`,
  `take(el)`, `simplify()`, `removeAt(int)`, `remove(el)`, `clear()`. `QCPLayoutGrid`: `addElement(row, col, el)`,
  `expandTo`, `insertRow`, `insertColumn`. `QCPLayoutInset`: `addElement(el, Qt::Alignment)`. `QCPLegend`:
  `addItem(item)`, `removeItem(int)`, `clearItems()`. Left out: `saveRastered` (takes a `const char*` format, not
  callable from a script), the other overloads, the actions that return handles. `take` and `takeAt` re-parent the
  element to the plot, which keeps it alive.
- `Q_DISABLE_COPY` became `Q_DISABLE_COPY_MOVE` on every `QObject` class (a `QObject` was never copyable, so no
  caller changes). The tickers, `QCPItemAnchor`/`QCPItemPosition` and `QCPSelectionDecorator` are not `QObject`s and
  keep `Q_DISABLE_COPY`.
- The 11 `Q_DECLARE_TYPEINFO` lines were re-checked and stay as they are.

### Final review fixes

- `QCPLegend::selectionChanged` also fires when an item is selected or deselected, or a selected item leaves the
  legend: `selectedParts` reads the items' selection. `QCPLegend` overrides `takeAt` for it (ABI).
- A deferred signal's pointer argument to a `QObject` is held by a `QPointer` until delivery: a slot gets null,
  not a dangling pointer, when the object was deleted before the signal was delivered.
- `removePlottable`, `clearPlottables`, `removeGraph`, `clearGraphs` and a plain delete of a plottable deliver the
  legend's and the group's signals after the plottable is gone. A plain delete of a plottable unparents it from the
  plot first, so a slot that deletes the plot does not delete it twice.
- A plottable whose system goes away (`removeRadialAxis`, deleting an axis or a polar area) delivers its
  `systemChanged(nullptr)` after it is done with itself.
- `QCustomPlot::overlay()` delivers the notification layer's `modeChanged` after the overlay is made; a slot that
  deletes the plot leaves the returned handle null.

### Delivery rules for slots and bindings

The user-facing text is `docs/migrating.md`, "Properties, change signals and bindings". The points that matter
to SciQLopPlots' own slots and to any binding it adds:

- A change signal is queued only when something listens at the write. A slot connected after the write and
  before the delivery misses that one change (ruling R-3): read the property when connecting.
- A pointer property that goes null because its target died sends no change signal: a plottable's axes, an
  error bar's data plottable, a tracer's graph. Watch the target's `destroyed()` (R-10).
- Signals sent from a destructor or during plot teardown arrive at once. A slot must not delete the plot there
  (R-11).
- A binding must not capture a `QBindable` of another object: it crashes once that object is deleted. Hold a
  `QPointer` and build the `QBindable` inside the binding (R-14).
- A plain setter call on a bound property keeps the binding; the next change of a dependency writes over it.

## From milestone 5f (triage)

SciQLopPlots will be rebuilt on NeoQCP 3.0, not ported line by line. These are the breaks a compile probe found
that no section above lists, and the behaviour changes of 5f.

### Compile breaks

- `qcp::algo::HistogramNormalization` is now `QCP::HistogramNormalization` (`global.h`). `qcp_enums.hpp` and
  the `to_qcp`/`from_qcp` overloads in `SciQLopHistogram.cpp` follow it.
- `mScatterStyle` is gone from the line plottables: read `scatterStyle()` (`SciQLopTimeColoredCurve.cpp`).

### Behaviour changes

- `draw(QCPDrawContext&)` is pure on `QCPAbstractPlottable` and on `QCPAbstractItem`. A subclass without it no
  longer compiles; before, it drew nothing.
- `setBrush` on a graph keeps each fill's target (a constant or another column) and changes only its brush.
- A legend row reads its legend from the layout it sits in. `parentLegend()` is null until the row is added to a
  legend.
- A curve given data of another size (`setData`, `addData`, a container edit) drops its colour values and leaves
  its rect's colour scale, as a graph does (#46). A scale picked with `setColorScale()` stays.
- `QCPGraph::removeData()`, `removeDataBefore()` and `removeDataAfter()` at a NaN key report a misuse and change
  nothing (#80). Under the default handler a debug build aborts.
- A group legend row that is not selectable answers a hit test that does not ask for selectable parts, as a plain
  row does.
- The colour scale choice sticks (final review I1). `setColorScale(nullptr)` before colour values keeps the
  line out of its rect's bar; new values, a resize, a move to another rect or a take and add never re-join it. A
  scale picked with `setColorScale()` survives a resize that drops the values.
- `QCPAbstractColorMapped::showsColorScale()` is private, and the protected `mFollowsRectScale` and
  `mJoiningKeyAxisRectScale` are gone. `QCPPlottableLegendItem::getIconBorderPen()` is gone: use
  `legendIconBorderPen()`. `QCustomPlot`'s ancestor watch (`isAligned`, `watchAncestors`, ...) is private.
- Base classes drop "Abstract" (QCPAbstractPlottable -> QCPPlottable, QCPAbstractItem -> QCPItem, QCPAbstractLegendItem -> QCPLegendItem, QCPAbstractLine -> QCPLine, QCPAbstractColorMapped -> QCPColorMapped, QCPAbstractSpanItem -> QCPSpanItem, QCPAbstract{,2D,Multi}DataSource -> QCP{,2D,Multi}DataSource, QCPAbstractPlottable1D -> QCPPlottable1D): rename every use in SciQLopPlots.
- `markDirty()` on a layerable is `update()`, `contentChanged()` is `update()`, `QCPLayoutElement::update(UpdatePhase)` is `updateLayout(UpdatePhase)` (QCPLayer::markDirty stays), `QCPLayout::updateLayout()` (the protected hook) is `layoutChildren()`, `QCPDrawnCore::contentChanged()` is `newGeneration()`: rename every use and override in SciQLopPlots.
- Plottables are built without axes (`new QCPGraph(key, value)` -> `plot.add<QCPGraph>()`, data in the constructor): every `new QCPxxx(keyAxis, valueAxis)` in SciQLopPlots becomes an `add<T>()` on the pair's rect, and a plottable added onto a system that lacks its capabilities is refused (empty handle) instead of joining as an inert object.
- The one-call annotations (`ax->line(keys, values, opts)`, `ax->text(at, "t", style)`, `ax->ellipse(a, b, opts)`) replace `add<T>()` plus setters in SciQLopPlots: `.get()` disappears with them, and the options structs carry the pen, name and scatter style a call site used to chain.
- `QCPTextStyle::color` defaults to an invalid `QColor` (the drawing's foreground) and the theme fills only colours nobody set: a `QCPItemText` colour set before the add survives, and a colour map's chosen gradient and data range are no longer overwritten by its rect's colour scale.
- `QCP::Position` gains `QCP::DataOffset` (a data point with a pixel offset riding on it, `QCP::Data{x, y} + QCP::PixelOffset{dx, dy}`) and coords.h gains `QCP::PixelLength` with the `_px` literal (`QCP::Literals`) and `QCP::Pixels - QCP::Pixels`: every `std::visit` or `std::get_if` chain in SciQLopPlots over `QCP::Position` must handle the new alternative, and an annotation placed with a pixel snapshot that should follow the data becomes a `DataOffset`.
- The drawing context's helpers (`line`, `circle` with a data radius or a `_px` one, `polygon` over a `QPolygonF` or a list of positions, `rect(QRectF)`, `roundedRect`, and a boxed `text`) replace the per-corner calls SciQLopPlots builds by hand: an item's `draw()` shrinks to one call per shape, and a fixed-size disc is no longer a scatter style.
- Call order is screen order (C-4/C-5): a text, a painter callback or a plain layerable recorded after GPU content
  now covers it on screen as in an export, through a painter slice of its layer; a layer holding a slice is
  painted again on a pan instead of being translated, and markers stay over everything else on their layer.
- `QCPItemText::mathText` is on by default and `ax->text()` copies `QCPTextStyle::mathText`, which defaults to
  off: a label SciQLopPlots builds with a default style is plain from now on, so a label that wants `$...$`
  typeset passes `.mathText = true`.

## From milestone 6e (a grid of axis rects)

- `QCustomPlot::setAxisRectGrid(rows, columns, options)` builds the panel grid in one call, and
  `axisRect(row, column)` addresses a cell. The hand-built panel layout (`SciQLopPlot.cpp` ~687 adds
  a colour-scale column, ~717 a title row) becomes the grid plus the rect's own colour scale, which
  counts in that rect's margin, and the rect's `setTitle()`. A rect already in a kept cell survives
  a later call with its plottables, items, colour scale and insets; a rect outside the new grid is
  removed with everything it owns, as `QCPHandle::remove()` would.
- The grid's alignment groups take the rects' left/right and top/bottom sides from any group they
  were in, so a `VPlotsAlign` over the same plots no longer aligns those sides.
- 6e insets (`QCPAxisRect::addInset`) draw in their own layers, named `inset N back/data/axes` (N
  counting the plot's insets): what is added to an inset lands on its data layer, and its background
  follows the plot's until the inset's own `setBackground()` names one.

## From n11 (range data calls, C++23)

### Compile breaks

- `QCPGraph::setData(std::vector<double>, std::vector<double>)` takes its two vectors by value and moves them into
  the growable source. A call with lvalues copies once, as before, and one with rvalues hands its storage over; only
  code that names that overload's type (a member pointer, a subclass hiding it) changes.
- The build is C++23 (GCC 14, Clang 18 or Apple Clang 16, MSVC 17.10). The headers still compile as C++20 today,
  but the CI floor is C++23 and libc++ builds need it: build SciQLopPlots with `cpp_std=c++23`.

### Behaviour changes

- A type with a `qcp_columns` hook is columns only: a number form (a plottable's constructor, `ax->line`, the
  one-column `QCPGraph::setData`) refuses it instead of flattening it row-major. `QCPGraph` and
  `QCPWaterfallGraph` take it through their columns form.
- The range-taking calls are the plottables' data constructors, `ax->line(keys, values)` and
  `QCPGraph::setData(keys, values)` (also through the graph's handle); the other plottables' `setData()` and
  `addData()` keep taking `QVector<double>` and `std::vector<double>`.
- 6e rescale union: `QCPAxisBase::rescale()` is no longer virtual (the hook is `ownDataRange()`), and identity-linked
  axes rescale as one. SciQLopPlots wraps `QCPAxis::rescale()` (`SciQLopPlotAxis::rescale`, `ColorScaleController`)
  without overriding it and makes no identity link: nothing to change.

## From milestone 6d (identity and hooks)

### Compile breaks

- The plottable key virtual is gone: `QCPPlottable::keyPress` and a `keyPress` override on a plottable no longer
  exist. A plottable that took keys connects to its plot's `keyPress`/`keyRelease` signals instead, and calls
  `event->accept()` to take the key.
- An anchor is a function: `QCPItem::createAnchor(name, int anchorId)`, the built-in items' `AnchorIndex` enums and
  every `anchorPixelPosition()` override are deleted, in favour of
  `createAnchor(name, std::function<QCP::Position()>)` with the anchor addressed by name.
- `QCPItem::dataExtent()` is gone: `dataRect()` answers the recording's data box, and a subclass overrides the
  protected `indexExtent()`.
- An item's mouse handlers take a `QCPMouseEvent*`; the `QMouseEvent` overloads on `QCPItem` are `final`, so an
  item that overrode them must move to the value event.

### Behaviour changes

- Keys arrive as the plot's signals before any built-in handling, and the plot ignores what it offers: a slot that
  accepts decides whether `Delete` reaches the selected items and whether the key goes on to the parent widget.
  The default focus policy is `Qt::StrongFocus`, so the plot takes focus on Tab as well as on a click.
- An `item->changed()` that a slot sees may come from a delete: an item anchored to a deleted item emits it before
  that delete returns.
- `QCPHandle<T>::remove()` is `const`, so a predicate that takes `const QCPHandle<T>&` can remove what it is looking
  at (brick-breaker's collision sweep does).
