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

- `QCPMarginGroup` is deprecated (4d): use `QCPAlignmentGroup`. `src/SciQLopPlot.cpp` ~141. The
  panel's own `VPlotsAlign` may be replaceable by alignment groups.
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
