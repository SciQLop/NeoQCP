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
| `QCPItemHSpan` / `QCPItemVSpan` deleted (4e) | `QCPItemSpan(plot, Qt::Horizontal / Qt::Vertical)`; same `range()`, `setRange()`, `lowerEdge`, `upperEdge`, `rangeChanged` | `src/SciQLopMultiPlotPanel.cpp` ~823-908 (preview spans, `qobject_cast<QCPItemVSpan*>`), `include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp` ~45, 70, `src/SpanTargets.cpp` ~52-54 (`SpanItem<QCPItemVSpan>`, `SpanItem<QCPItemHSpan>`): 16 references in all |
| Line colour API renamed (4c): `setColorGradient`, `setColorRange`, `setColorScaleType` | `setGradient`, `setDataRange`, `setDataScaleType` | `src/SciQLopLineGraph.cpp` ~229-237 |
| `QCPHistogram2D::Normalization` (`nNone`, `nColumn`) removed (4c) | `setColumnNormalized(bool)` / `columnNormalized()` | `src/SciQLopHistogram2D.cpp` ~229-238 |
| `QCPHistogram::edgesChanged()` removed (4c) | `histogramReady()` | `src/SciQLopHistogram.cpp` ~124 |
| `setSelectable(bool)` on items, legend rows, text elements (4a) | `setSelectable(QCP::stWhole / QCP::stNone)` | `src/SciQLopCrosshair.cpp` ~99-115, `src/SpanTargets.cpp` ~43 |
| Selection signals lost their arguments (4a) | connect to `selectionChanged()` and read `selected()` / `selection()` | any connection to a NeoQCP `selectionChanged(bool)` or `selectionChanged(const QCPDataSelection&)`; SciQLopPlots' own `SciQLopPlotPanelInterface::selectionChanged(bool)` is its own signal, check what feeds it |
| `rangeChanged` has one signature `(newRange, oldRange)` (4a) | connect to that one | any single-argument connection |
| `QCPAbstractItem::draw(QCPPainter*)` is final (4e) | override `draw(QCPDrawContext&)` | none found today; check every `SciQLopPlotItem<...>` subclass again |

## Known behaviour changes to check

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
