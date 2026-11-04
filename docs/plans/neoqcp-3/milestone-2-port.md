# Milestone 2 — the guide and the examples use `add<T>()` and automatic repaint

Every change below was made mechanically (466 changed lines in 29 files): the same
snippet semantics, a new API. Nothing was redesigned.

Rules applied: a creation call became `add<T>(...)`; a setter the handle has was folded into the
chain, a setter it lacks stayed a `->` call on the handle; a handle used where a raw pointer is
expected was `.get()`-ed; `replot()` calls that only showed a change were removed (the setters
schedule one repaint), and the ones a snippet still needs are listed at the end with their reason.

## `docs/axes.md`

| before | after |
| --- | --- |
| `addGraph()` and the other factories use `xAxis` and `yAxis` unless told otherwise. | `add<T>()` uses `xAxis` and `yAxis` unless told otherwise. |
| - Pass the axes to `addGraph(keyAxis, valueAxis)`, or to the constructor of any other plottable. | - Pass the axes to the handle: `plot->add<QCPGraph>().setAxes(keyAxis, valueAxis)`. |

## `docs/concepts.md`

| before | after |
| --- | --- |
| `addGraph()` with no arguments uses `xAxis` and `yAxis`. | `add<T>()` uses `xAxis` and `yAxis`. |
| Pass other axes to plot against them, for example `addGraph(customPlot->xAxis, customPlot->yAxis2)` for a second y scale on the right. | Pass other axes to plot against them, for example `customPlot->add<QCPGraph>().setAxes(customPlot->xAxis, customPlot->yAxis2)` for a second y scale on the right. |

## `docs/getting-started.md`

| before | after |
| --- | --- |
| `addGraph()` creates a graph on the default axes. `setData()` copies the points into it. | `add<QCPGraph>()` creates a graph on the default axes. `setData()` copies the points into it. |
| Nothing is drawn until `replot()`: change as much as you like, then replot once. | Setters schedule one repaint, so the graph appears without a call to `replot()`: change as much as you like, the plot draws once the event loop runs. |

## `docs/graphs.md`

| before | after |
| --- | --- |
| `addGraph()` creates a `QCPGraph` on the default axes. `setData()` takes two `QVector<double>`, keys and values. | `add<QCPGraph>()` creates a `QCPGraph` on the default axes. `setData()` takes two `QVector<double>`, keys and values. |
| You create it with `new QCPGraph2(keyAxis, valueAxis)`; the plot takes ownership, like for any plottable. | You create it with `plot->add<QCPGraph2>().setAxes(keyAxis, valueAxis)`; the plot owns it, like any plottable. |

## `docs/layout.md`

| before | after |
| --- | --- |
| - Pass those axes to `addGraph()` or to a plottable constructor to draw in that rect. | - Pass those axes to the handle (`plot->add<QCPGraph>().setAxes(rect->axis(QCPAxis::atLeft), rect->axis(QCPAxis::atBottom))`) to draw in that rect. |

## `docs/performance.md`

| before | after |
| --- | --- |
| `replot()` repaints every layer that changed. Change everything first, then call it once. | Setters schedule one repaint, so a burst of changes costs one redraw. `replot()` forces that redraw now, when a synchronous frame is needed. |

## `docs/polar.md`

| before | after |
| --- | --- |
| - `new QCPPolarGraph(angularAxis, radialAxis)` creates a graph and registers it with the axes. The plot owns it. | - `angular->add<QCPPolarGraph>()` creates a graph on the angular axis's first radial axis and registers it. The plot owns it. |

## `docs/snippets/axes.cpp`

| before | after |
| --- | --- |
| QCPGraph* profile = customPlot->addGraph(); | auto profile = customPlot->add<QCPGraph>() |
| profile->setData(temperature, depth); | .setData(temperature, depth) |
| profile->setPen(QPen(QColor(31, 119, 180), 2)); | .setPen(QPen(QColor(31, 119, 180), 2)); |
| customPlot->replot(); |  |
| QCPGraph* spectrum = customPlot->addGraph(); | auto spectrum = customPlot->add<QCPGraph>() |
| spectrum->setData(frequency, power); | .setData(frequency, power) |
| spectrum->setPen(QPen(QColor(214, 39, 40), 2)); | .setPen(QPen(QColor(214, 39, 40), 2)); |
| customPlot->replot(); |  |
| QCPGraph* graph = customPlot->addGraph(customPlot->xAxis, axis); | auto graph = customPlot->add<QCPGraph>().setAxes(customPlot->xAxis, axis) |
| graph->setData(hour, values); | .setData(hour, values) |
| graph->setPen(QPen(color, 2)); | .setPen(QPen(color, 2)); |
| customPlot->replot(); |  |
| customPlot->replot(); |  |
| customPlot->replot(); |  |
| customPlot->replot(); |  |
| customPlot->replot(); |  |

## `docs/snippets/colormaps.cpp`

| before | after |
| --- | --- |
| auto* map = new QCPColorMap(customPlot->xAxis, customPlot->yAxis); | auto map = customPlot->add<QCPColorMap>(); |
| customPlot->replot(); |  |
| auto* map = new QCPColorMap2(customPlot->xAxis, customPlot->yAxis); | auto map = customPlot->add<QCPColorMap2>().setData(std::move(time), std::move(frequency), std::move(power)); |
| map->setData(std::move(time), std::move(frequency), std::move(power)); |  |
| customPlot->replot(); |  |
| auto* map = new QCPColorMap2(customPlot->xAxis, customPlot->yAxis); | auto map = customPlot->add<QCPColorMap2>(); |
| customPlot->replot(); |  |
| auto* map = new QCPColorMap2(customPlot->xAxis, customPlot->yAxis); | auto map = customPlot->add<QCPColorMap2>(); |
| customPlot->replot(); |  |
| auto* histogram = new QCPHistogram2D(customPlot->xAxis, customPlot->yAxis); | auto histogram = customPlot->add<QCPHistogram2D>(); |
| QObject::connect(&histogram->pipeline(), &QCPHistogramPipeline::finished, histogram, | QObject::connect(&histogram->pipeline(), &QCPHistogramPipeline::finished, histogram.get(), |
| customPlot->replot(); |  |
| auto* histogram = new QCPHistogram2D(customPlot->xAxis, customPlot->yAxis); | auto histogram = customPlot->add<QCPHistogram2D>(); |
| QObject::connect(&histogram->pipeline(), &QCPHistogramPipeline::finished, histogram, | QObject::connect(&histogram->pipeline(), &QCPHistogramPipeline::finished, histogram.get(), |
| customPlot->replot(); |  |
| auto* stripe = new QCPColorMap(customPlot->xAxis, customPlot->yAxis); | auto stripe = customPlot->add<QCPColorMap>(); |
| customPlot->replot(); |  |
| customPlot->replot(); |  |
| auto* map = new QCPColorMap(customPlot->xAxis, customPlot->yAxis); | auto map = customPlot->add<QCPColorMap>(); |
| customPlot->replot(); |  |
| auto* map = new QCPColorMap(customPlot->xAxis, customPlot->yAxis); | auto map = customPlot->add<QCPColorMap>(); |
| auto* map = new QCPColorMap(customPlot->xAxis, customPlot->yAxis); | auto map = customPlot->add<QCPColorMap>(); |

## `docs/snippets/concepts.cpp`

| before | after |
| --- | --- |
| QCPGraph* graph = customPlot->addGraph(); | auto graph = customPlot->add<QCPGraph>() |
| graph->setData(x, y); | .setData(x, y) |
| graph->setName("plottable: QCPGraph"); | .setName("plottable: QCPGraph") |
| graph->setPen(QPen(QColor(31, 119, 180), 2)); | .setPen(QPen(QColor(31, 119, 180), 2)); |
| auto* note = new QCPItemText(customPlot); | auto note = customPlot->add<QCPItemText>(); |
| auto* arrow = new QCPItemLine(customPlot); | auto arrow = customPlot->add<QCPItemLine>(); |
| auto* rectLabel = new QCPItemText(customPlot); | auto rectLabel = customPlot->add<QCPItemText>(); |
| customPlot->replot(); |  |
| QCPGraph* graph = customPlot->addGraph(); | auto graph = customPlot->add<QCPGraph>() |
| graph->setData(x, y); | .setData(x, y) |
| graph->setPen(QPen(QColor(31, 119, 180), 2)); | .setPen(QPen(QColor(31, 119, 180), 2)) |
| graph->setBrush(QColor(174, 199, 232)); | .setBrush(QColor(174, 199, 232)); |
| auto* band = new QCPItemRect(customPlot); | auto band = customPlot->add<QCPItemRect>(); |
| customPlot->replot(); |  |
| auto* marker = new QCPItemTracer(customPlot); | auto marker = customPlot->add<QCPItemTracer>(); |
| auto* label = new QCPItemText(customPlot); | auto label = customPlot->add<QCPItemText>(); |
| customPlot->replot(); |  |
| QCPGraph* graph = customPlot->addGraph(); | auto graph = customPlot->add<QCPGraph>(); |

## `docs/snippets/export.cpp`

| before | after |
| --- | --- |
| QCPGraph* graph = customPlot->addGraph(); | auto graph = customPlot->add<QCPGraph>() |
| graph->setData(x, y); | .setData(x, y) |
| graph->setPen(QPen(QColor(31, 119, 180), 2)); | .setPen(QPen(QColor(31, 119, 180), 2)); |
| QCPGraph* graph = customPlot->addGraph(); | auto graph = customPlot->add<QCPGraph>() |
| graph->setData(x, y); | .setData(x, y) |
| graph->setLineStyle(QCPGraph::lsStepCenter); | .setLineStyle(QCPGraph::lsStepCenter) |
| graph->setBrush(QColor(31, 119, 180, 50)); | .setBrush(QColor(31, 119, 180, 50)); |

## `docs/snippets/gallery.cpp`

| before | after |
| --- | --- |
| auto* field = new QCPMultiGraph(customPlot->xAxis, customPlot->yAxis); | auto field = customPlot->add<QCPMultiGraph>().setData(std::move(time), std::move(b)); |
| field->setData(std::move(time), std::move(b)); |  |
| auto* span = new QCPItemVSpan(customPlot); | auto span = customPlot->add<QCPItemVSpan>(); |
| customPlot->replot(); |  |
| auto* overview = new QCPGraph2(customPlot->xAxis, customPlot->yAxis); | auto overview = customPlot->add<QCPGraph2>(); |
| auto* note = new QCPItemText(customPlot); | auto note = customPlot->add<QCPItemText>(); |
| customPlot->replot(); |  |
| auto* segment = new QCPCurve(customPlot->xAxis, customPlot->yAxis); | auto segment = customPlot->add<QCPCurve>().setData({ index[i], index[i + 1] }, { x[i], x[i + 1] }, { y[i], y[i + 1] }); |
| segment->setData({ index[i], index[i + 1] }, { x[i], x[i + 1] }, { y[i], y[i + 1] }); | segments.append(segment.get()); |
| segments.append(segment); |  |
| auto* hours = new QCPCurve(customPlot->xAxis, customPlot->yAxis); | auto hours = customPlot->add<QCPCurve>() |
| hours->setData(hourIndex, hourX, hourY); | .setData(hourIndex, hourX, hourY) |
| hours->setLineStyle(QCPCurve::lsNone); | .setLineStyle(QCPCurve::lsNone) |
| hours->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, Qt::black, 6)); | .setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, Qt::black, 6)); |
| auto* earth = new QCPItemEllipse(customPlot); | auto earth = customPlot->add<QCPItemEllipse>(); |
| customPlot->replot(); |  |
| auto* density = new QCPHistogram2D(customPlot->xAxis, customPlot->yAxis); | auto density = customPlot->add<QCPHistogram2D>(); |
| QObject::connect(&density->pipeline(), &QCPHistogramPipeline::finished, density, | QObject::connect(&density->pipeline(), &QCPHistogramPipeline::finished, density.get(), |
| customPlot->replot(); |  |
| auto* map = new QCPColorMap2(customPlot->xAxis, customPlot->yAxis); | auto map = customPlot->add<QCPColorMap2>().setData(std::move(x), std::move(y), std::move(potential)); |
| map->setData(std::move(x), std::move(y), std::move(potential)); |  |
| customPlot->replot(); |  |
| auto* candles = new QCPFinancial(customPlot->xAxis, customPlot->yAxis); | auto candles = customPlot->add<QCPFinancial>(); |
| QCPGraph* trend = customPlot->addGraph(); | auto trend = customPlot->add<QCPGraph>() |
| trend->setData(days, average); | .setData(days, average) |
| trend->setPen(QPen(QColor(255, 152, 0), 2)); | .setPen(QPen(QColor(255, 152, 0), 2)) |
| trend->setName("20-day average"); | .setName("20-day average"); |
| customPlot->replot(); |  |
| customPlot->replot(); |  |
| QCPGraph* curve = customPlot->addGraph(); | auto curve = customPlot->add<QCPGraph>() |
| curve->setData(fineT, fineX); | .setData(fineT, fineX) |
| curve->setPen(QPen(QColor(214, 39, 40), 2)); | .setPen(QPen(QColor(214, 39, 40), 2)) |
| curve->setName("model"); | .setName("model"); |
| QCPGraph* points = customPlot->addGraph(); | auto points = customPlot->add<QCPGraph>() |
| points->setData(t, x); | .setData(t, x) |
| points->setLineStyle(QCPGraph::lsNone); | .setLineStyle(QCPGraph::lsNone) |
| points->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, QColor(31, 119, 180), 6)); | .setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, QColor(31, 119, 180), 6)) |
| points->setName("measurements"); | .setName("measurements"); |
| auto* errors = new QCPErrorBars(customPlot->xAxis, customPlot->yAxis); | auto errors = customPlot->add<QCPErrorBars>(); |
| errors->setDataPlottable(points); | errors->setDataPlottable(points.get()); |
| QCPGraph* residuals = customPlot->addGraph(residualT, residualAxis); | auto residuals = customPlot->add<QCPGraph>().setAxes(residualT, residualAxis) |
| residuals->setData(t, pull); | .setData(t, pull) |
| residuals->setLineStyle(QCPGraph::lsImpulse); | .setLineStyle(QCPGraph::lsImpulse) |
| residuals->setPen(QPen(QColor(31, 119, 180))); | .setPen(QPen(QColor(31, 119, 180))) |
| residuals->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, QColor(31, 119, 180), 5)); | .setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, QColor(31, 119, 180), 5)); |
| customPlot->replot(); |  |

## `docs/snippets/getting-started.cpp`

| before | after |
| --- | --- |
| customPlot->addGraph(); | customPlot->add<QCPGraph>(); |
| customPlot->replot(); |  |
| QCPGraph* graph = customPlot->addGraph(); | auto graph = customPlot->add<QCPGraph>() |
| graph->setData(t, signal); | .setData(t, signal) |
| graph->setName("damped sine"); | .setName("damped sine") |
| graph->setPen(QPen(QColor(31, 119, 180), 2)); | .setPen(QPen(QColor(31, 119, 180), 2)); |
| customPlot->replot(); |  |

## `docs/snippets/graphs.cpp`

| before | after |
| --- | --- |
| QCPGraph* graph = customPlot->addGraph(); | auto graph = customPlot->add<QCPGraph>() |
| graph->setData(x, shifted); | .setData(x, shifted) |
| graph->setLineStyle(styles[s].first); | .setLineStyle(styles[s].first) |
| graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, 5)); | .setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, 5)) |
| graph->setPen(QPen(colors[s], 1.5)); | .setPen(QPen(colors[s], 1.5)) |
| graph->setName(styles[s].second); | .setName(styles[s].second); |
| customPlot->replot(); |  |
| QCPGraph* points = customPlot->addGraph(); | auto points = customPlot->add<QCPGraph>() |
| points->setData(x, measured); | .setData(x, measured) |
| points->setLineStyle(QCPGraph::lsNone); | .setLineStyle(QCPGraph::lsNone); |
| QCPGraph* fit = customPlot->addGraph(); | auto fit = customPlot->add<QCPGraph>() |
| fit->setData(modelX, model); | .setData(modelX, model) |
| fit->setPen(QPen(QColor(214, 39, 40), 2)); | .setPen(QPen(QColor(214, 39, 40), 2)) |
| fit->setName("model"); | .setName("model"); |
| customPlot->replot(); |  |
| QCPGraph* spectrum = customPlot->addGraph(); | auto spectrum = customPlot->add<QCPGraph>() |
| spectrum->setData(f, power); | .setData(f, power) |
| spectrum->setPen(QPen(QColor(31, 119, 180), 1.5)); | .setPen(QPen(QColor(31, 119, 180), 1.5)); |
| customPlot->replot(); |  |
| QCPGraph* low = customPlot->addGraph(); | auto low = customPlot->add<QCPGraph>() |
| low->setData(t, lower); | .setData(t, lower) |
| low->setPen(Qt::NoPen); | .setPen(Qt::NoPen); |
| QCPGraph* high = customPlot->addGraph(); | auto high = customPlot->add<QCPGraph>() |
| high->setData(t, upper); | .setData(t, upper) |
| high->setPen(Qt::NoPen); | .setPen(Qt::NoPen) |
| high->setBrush(QColor(255, 127, 14, 70)); | .setBrush(QColor(255, 127, 14, 70)); |
| high->setChannelFillGraph(low);   // fill between high and low, with high's brush | high->setChannelFillGraph(low.get());   // fill between high and low, with high's brush |
| QCPGraph* centre = customPlot->addGraph(); | auto centre = customPlot->add<QCPGraph>() |
| centre->setData(t, mean); | .setData(t, mean) |
| centre->setPen(QPen(QColor(255, 127, 14), 2)); | .setPen(QPen(QColor(255, 127, 14), 2)) |
| centre->setName("mean"); | .setName("mean"); |
| customPlot->replot(); |  |
| auto* graph = new QCPGraph2(customPlot->xAxis, customPlot->yAxis); | auto graph = customPlot->add<QCPGraph2>(); |
| customPlot->replot(); |  |
| auto* graph = new QCPGraph2(customPlot->xAxis, customPlot->yAxis); | auto graph = customPlot->add<QCPGraph2>(); |
| customPlot->replot(); |  |
| auto* overview = new QCPGraph2(customPlot->xAxis, customPlot->yAxis); | auto overview = customPlot->add<QCPGraph2>(); |
| customPlot->replot(); |  |
| auto* graph = new QCPGraph2(customPlot->xAxis, customPlot->yAxis); | auto graph = customPlot->add<QCPGraph2>() |
| graph->setData(std::move(t), std::move(v)); | .setData(std::move(t), std::move(v)) |
| graph->setLineStyle(QCPGraph2::lsNone); | .setLineStyle(QCPGraph2::lsNone) |
| graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 5)); | .setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 5)); |
| customPlot->replot(); |  |
| auto* b = new QCPMultiGraph(customPlot->xAxis, customPlot->yAxis); | auto b = customPlot->add<QCPMultiGraph>(); |
| customPlot->replot(); |  |
| auto* graph = new QCPMultiGraph(customPlot->xAxis, customPlot->yAxis); | auto graph = customPlot->add<QCPMultiGraph>() |
| graph->setData(std::move(time), std::move(rates)); | .setData(std::move(time), std::move(rates)) |
| graph->setName("count rate"); | .setName("count rate"); |
| customPlot->replot(); |  |
| auto* model = new QCPMultiGraph(customPlot->xAxis, customPlot->yAxis); | auto model = customPlot->add<QCPMultiGraph>().setData(std::move(time), std::move(columns)); |
| model->setData(std::move(time), std::move(columns)); |  |
| customPlot->replot(); |  |
| auto* measured = new QCPMultiGraph(customPlot->xAxis, customPlot->yAxis); | auto measured = customPlot->add<QCPMultiGraph>().setData(std::move(x), std::move(columns)); |
| measured->setData(std::move(x), std::move(columns)); |  |
| customPlot->replot(); |  |
| QCPGraph* classic = customPlot->addGraph(); | auto classic = customPlot->add<QCPGraph>() |
| classic->setData(t, withNan); | .setData(t, withNan) |
| classic->setPen(QPen(QColor(31, 119, 180), 2)); | .setPen(QPen(QColor(31, 119, 180), 2)) |
| classic->setName("QCPGraph, NaN values"); | .setName("QCPGraph, NaN values"); |
| auto* zeroCopy = new QCPGraph2(customPlot->xAxis, customPlot->yAxis); | auto zeroCopy = customPlot->add<QCPGraph2>() |
| zeroCopy->setData(std::move(keys), std::move(values)); | .setData(std::move(keys), std::move(values)) |
| zeroCopy->setPen(QPen(QColor(214, 39, 40), 2)); | .setPen(QPen(QColor(214, 39, 40), 2)) |
| zeroCopy->setName("QCPGraph2, missing keys"); | .setName("QCPGraph2, missing keys"); |
| customPlot->replot(); |  |

## `docs/snippets/interactions.cpp`

| before | after |
| --- | --- |
| customPlot->replot(); |  |
| QCPGraph* temp = customPlot->addGraph(customPlot->xAxis, customPlot->yAxis); | auto temp = customPlot->add<QCPGraph>().setAxes(customPlot->xAxis, customPlot->yAxis); |
| QCPGraph* pres = customPlot->addGraph(customPlot->xAxis, customPlot->yAxis2); | auto pres = customPlot->add<QCPGraph>().setAxes(customPlot->xAxis, customPlot->yAxis2); |
| customPlot->replot(); |  |
| QCPGraph* first = customPlot->addGraph(); | auto first = customPlot->add<QCPGraph>() |
| first->setData(x, a); | .setData(x, a) |
| first->setName("sin"); | .setName("sin"); |
| QCPGraph* second = customPlot->addGraph(); | auto second = customPlot->add<QCPGraph>() |
| second->setData(x, b); | .setData(x, b) |
| second->setName("cos / 2"); | .setName("cos / 2") |
| second->setPen(QPen(QColor(255, 127, 14))); | .setPen(QPen(QColor(255, 127, 14))); |
| customPlot->replot(); |  |
| QCPGraph* graph = customPlot->addGraph(); | auto graph = customPlot->add<QCPGraph>() |
| graph->setData(x, y); | .setData(x, y) |
| graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, 5)); | .setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, 5)); |
| customPlot->replot(); |  |
| QCPGraph* graph = customPlot->addGraph(); | auto graph = customPlot->add<QCPGraph>().setData(x, y); |
| graph->setData(x, y); |  |
| auto* meanLine = new QCPItemStraightLine(customPlot); | auto meanLine = customPlot->add<QCPItemStraightLine>(); |
| customPlot->replot(); |  |
| QCPGraph* cloud = customPlot->addGraph(); | auto cloud = customPlot->add<QCPGraph>() |
| cloud->setData(x, y); | .setData(x, y) |
| cloud->setLineStyle(QCPGraph::lsNone); | .setLineStyle(QCPGraph::lsNone) |
| cloud->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, QColor(31, 119, 180), 5)); | .setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, QColor(31, 119, 180), 5)); |
| auto* dragged = new QCPItemRect(customPlot); | auto dragged = customPlot->add<QCPItemRect>(); |
| customPlot->replot(); |  |
| QCPGraph* graph = customPlot->addGraph(); | auto graph = customPlot->add<QCPGraph>().setData(t, v); |
| graph->setData(t, v); |  |
| if (locator.locateAtKey(graph, 3.7)) | if (locator.locateAtKey(graph.get(), 3.7)) |
| auto* tracer = new QCPItemTracer(customPlot); | auto tracer = customPlot->add<QCPItemTracer>(); |
| auto* label = new QCPItemText(customPlot); | auto label = customPlot->add<QCPItemText>(); |
| customPlot->replot(); |  |
| QCPGraph* graph = customPlot->addGraph(); | auto graph = customPlot->add<QCPGraph>().setData(QVector<double> { 0, 1, 2 }, QVector<double> { 0, 1, 0 }); |
| graph->setData(QVector<double> { 0, 1, 2 }, QVector<double> { 0, 1, 0 }); |  |
| customPlot->replot(); |  |
| locator->setPlottable(graph); | locator->setPlottable(graph.get()); |
|  | customPlot->replot();   // lays out the axes, so coordToPixel below is exact |
| QCPGraph* graph = customPlot->addGraph(); | auto graph = customPlot->add<QCPGraph>(); |

## `docs/snippets/items.cpp`

| before | after |
| --- | --- |
| auto* label = new QCPItemText(customPlot); | auto label = customPlot->add<QCPItemText>(); |
| auto* arrow = new QCPItemLine(customPlot); | auto arrow = customPlot->add<QCPItemLine>(); |
| auto* note = new QCPItemText(customPlot); | auto note = customPlot->add<QCPItemText>(); |
| auto* curve = new QCPItemCurve(customPlot); | auto curve = customPlot->add<QCPItemCurve>(); |
| customPlot->replot(); |  |
| QCPGraph* graph = customPlot->addGraph(); | auto graph = customPlot->add<QCPGraph>().setData(x, y); |
| graph->setData(x, y); |  |
| auto* tracer = new QCPItemTracer(customPlot); | auto tracer = customPlot->add<QCPItemTracer>(); |
| tracer->setGraph(graph); | tracer->setGraph(graph.get()); |
| auto* value = new QCPItemText(customPlot); | auto value = customPlot->add<QCPItemText>(); |
| auto* bracket = new QCPItemBracket(customPlot); | auto bracket = customPlot->add<QCPItemBracket>(); |
| auto* bracketLabel = new QCPItemText(customPlot); | auto bracketLabel = customPlot->add<QCPItemText>(); |
| customPlot->replot(); |  |
| QCPGraph* points = customPlot->addGraph(); | auto points = customPlot->add<QCPGraph>() |
| points->setData(x, y); | .setData(x, y) |
| points->setLineStyle(QCPGraph::lsNone); | .setLineStyle(QCPGraph::lsNone) |
| points->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 6)); | .setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 6)); |
| auto* trend = new QCPItemStraightLine(customPlot); | auto trend = customPlot->add<QCPItemStraightLine>(); |
| auto* outlier = new QCPItemEllipse(customPlot); | auto outlier = customPlot->add<QCPItemEllipse>(); |
| auto* region = new QCPItemRect(customPlot); | auto region = customPlot->add<QCPItemRect>(); |
| customPlot->replot(); |  |
| auto* summary = new QCPItemRichText(customPlot); | auto summary = customPlot->add<QCPItemRichText>(); |
| auto* logo = new QCPItemPixmap(customPlot); | auto logo = customPlot->add<QCPItemPixmap>().setPixmap(QPixmap::fromImage(image)); |
| logo->setPixmap(QPixmap::fromImage(image)); |  |
| customPlot->replot(); |  |
| auto* event = new QCPItemStraightLine(customPlot); | auto event = customPlot->add<QCPItemStraightLine>(); |
| auto* eventLabel = new QCPItemText(customPlot); | auto eventLabel = customPlot->add<QCPItemText>(); |
| auto* corner = new QCPItemText(customPlot); | auto corner = customPlot->add<QCPItemText>(); |
| auto* stamp = new QCPItemText(customPlot); | auto stamp = customPlot->add<QCPItemText>(); |
| customPlot->replot(); |  |
| auto* event = new QCPItemVSpan(customPlot); | auto event = customPlot->add<QCPItemVSpan>(); |
| auto* band = new QCPItemHSpan(customPlot); | auto band = customPlot->add<QCPItemHSpan>(); |
| auto* box = new QCPItemRSpan(customPlot); | auto box = customPlot->add<QCPItemRSpan>(); |
| QObject::connect(event, &QCPItemVSpan::rangeChanged, customPlot, | QObject::connect(event.get(), &QCPItemVSpan::rangeChanged, customPlot, |
| customPlot->replot(); |  |
| auto* span = new QCPItemVSpan(plot); | auto span = plot->add<QCPItemVSpan>().setPen(Qt::NoPen); |
| span->setPen(Qt::NoPen); |  |
| return span; | return span.get(); |
| customPlot->replot(); |  |
| customPlot->replot(); |  |

## `docs/snippets/layout.cpp`

| before | after |
| --- | --- |
| QCPGraph* graph = customPlot->addGraph(); | auto graph = customPlot->add<QCPGraph>() |
| graph->setData(x, y); | .setData(x, y) |
| graph->setPen(QPen(QColor(31, 119, 180), 2)); | .setPen(QPen(QColor(31, 119, 180), 2)); |
| customPlot->replot(); |  |
| customPlot->replot(); |  |
| QCPGraph* graph = customPlot->addGraph(); | auto graph = customPlot->add<QCPGraph>() |
| graph->setData(x, y); | .setData(x, y) |
| graph->setName(QString("channel %1").arg(g + 1)); | .setName(QString("channel %1").arg(g + 1)) |
| graph->setPen(QPen(colors[g], 2)); | .setPen(QPen(colors[g], 2)); |
| customPlot->replot(); |  |
| customPlot->replot(); |  |
| customPlot->replot(); |  |
| auto* colorMap = new QCPColorMap(customPlot->xAxis, customPlot->yAxis); | auto colorMap = customPlot->add<QCPColorMap>(); |
| customPlot->replot(); |  |

## `docs/snippets/migrating.cpp`

| before | after |
| --- | --- |
| customPlot->addGraph(); | customPlot->add<QCPGraph>(); |

## `docs/snippets/performance.cpp`

| before | after |
| --- | --- |
| auto* graph = new QCPGraph2(customPlot->xAxis, customPlot->yAxis); | auto graph = customPlot->add<QCPGraph2>(); |
| customPlot->replot(); |  |
| auto* field = new QCPMultiGraph(customPlot->xAxis, customPlot->yAxis); | auto field = customPlot->add<QCPMultiGraph>(); |
| customPlot->replot(); |  |
| auto* overview = new QCPGraph2(customPlot->xAxis, customPlot->yAxis); | auto overview = customPlot->add<QCPGraph2>(); |
| customPlot->replot(); |  |
| auto* cursor = new QCPItemStraightLine(customPlot); | auto cursor = customPlot->add<QCPItemStraightLine>(); |
| QObject::connect(customPlot, &QCustomPlot::mouseMove, cursor, | QObject::connect(customPlot, &QCustomPlot::mouseMove, cursor.get(), |
| customPlot->replot(); |  |
| auto* graph = new QCPGraph2(customPlot->xAxis, customPlot->yAxis); | auto graph = customPlot->add<QCPGraph2>().setData(std::vector<double> { 0, 1 }, std::vector<double> { 0, 1 }); |
| graph->setData(std::vector<double> { 0, 1 }, std::vector<double> { 0, 1 }); |  |
| QObject::connect(graph, &QCPAbstractPlottable::busyChanged, customPlot, | QObject::connect(graph.get(), &QCPAbstractPlottable::busyChanged, customPlot, |
| customPlot->replot(); |  |

## `docs/snippets/plottables.cpp`

| before | after |
| --- | --- |
| auto* lissajous = new QCPCurve(customPlot->xAxis, customPlot->yAxis); | auto lissajous = customPlot->add<QCPCurve>(); |
| customPlot->replot(); |  |
| auto* bars = new QCPBars(customPlot->xAxis, customPlot->yAxis); | auto bars = customPlot->add<QCPBars>().setData(keys, values[s]); |
| bars->setData(keys, values[s]); |  |
| customPlot->replot(); |  |
| auto* fossil = new QCPBars(customPlot->xAxis, customPlot->yAxis); | auto fossil = customPlot->add<QCPBars>() |
| fossil->setData(years, { 62, 58, 57, 54, 50, 46 }); | .setData(years, { 62, 58, 57, 54, 50, 46 }) |
| fossil->setBrush(QColor(127, 127, 127)); | .setBrush(QColor(127, 127, 127)) |
| fossil->setName("fossil"); | .setName("fossil"); |
| auto* nuclear = new QCPBars(customPlot->xAxis, customPlot->yAxis); | auto nuclear = customPlot->add<QCPBars>() |
| nuclear->setData(years, { 18, 19, 18, 17, 18, 18 }); | .setData(years, { 18, 19, 18, 17, 18, 18 }) |
| nuclear->setBrush(QColor(255, 127, 14)); | .setBrush(QColor(255, 127, 14)) |
| nuclear->setName("nuclear"); | .setName("nuclear"); |
| nuclear->moveAbove(fossil); | nuclear->moveAbove(fossil.get()); |
| auto* renewable = new QCPBars(customPlot->xAxis, customPlot->yAxis); | auto renewable = customPlot->add<QCPBars>() |
| renewable->setData(years, { 20, 23, 25, 29, 32, 36 }); | .setData(years, { 20, 23, 25, 29, 32, 36 }) |
| renewable->setBrush(QColor(44, 160, 44)); | .setBrush(QColor(44, 160, 44)) |
| renewable->setName("renewable"); | .setName("renewable"); |
| renewable->moveAbove(nuclear); | renewable->moveAbove(nuclear.get()); |
| for (QCPBars* bars : { fossil, nuclear, renewable }) | for (QCPBars* bars : { fossil.get(), nuclear.get(), renewable.get() }) |
| customPlot->replot(); |  |
| auto* candles = new QCPFinancial(customPlot->xAxis, customPlot->yAxis); | auto candles = customPlot->add<QCPFinancial>(); |
| customPlot->replot(); |  |
| QCPGraph* points = customPlot->addGraph(); | auto points = customPlot->add<QCPGraph>() |
| points->setData(x, y); | .setData(x, y) |
| points->setLineStyle(QCPGraph::lsNone); | .setLineStyle(QCPGraph::lsNone) |
| points->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, QColor(31, 119, 180), 7)); | .setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, QColor(31, 119, 180), 7)); |
| auto* errors = new QCPErrorBars(customPlot->xAxis, customPlot->yAxis); | auto errors = customPlot->add<QCPErrorBars>(); |
| errors->setDataPlottable(points); | errors->setDataPlottable(points.get()); |
| customPlot->replot(); |  |
| auto* boxes = new QCPStatisticalBox(customPlot->xAxis, customPlot->yAxis); | auto boxes = customPlot->add<QCPStatisticalBox>().setBrush(QColor(31, 119, 180, 70)); |
| boxes->setBrush(QColor(31, 119, 180, 70)); |  |
| customPlot->replot(); |  |
| auto* waterfall = new QCPWaterfallGraph(customPlot->xAxis, customPlot->yAxis); | auto waterfall = customPlot->add<QCPWaterfallGraph>().setData(std::move(frequency), std::move(spectra)); |
| waterfall->setData(std::move(frequency), std::move(spectra)); |  |
| customPlot->replot(); |  |
| customPlot->replot(); |  |
| auto* histogram = new QCPHistogram(customPlot->xAxis, customPlot->yAxis); | auto histogram = customPlot->add<QCPHistogram>().setName("quiet / active"); |
| histogram->setName("quiet / active"); |  |
| QObject::connect(histogram, &QCPHistogram::histogramReady, customPlot, [customPlot] { | QObject::connect(histogram.get(), &QCPHistogram::histogramReady, customPlot, [customPlot] { |
| customPlot->replot(); |  |
| auto* histogram = new QCPHistogram(customPlot->xAxis, customPlot->yAxis); | auto histogram = customPlot->add<QCPHistogram>(); |
| QObject::connect(histogram, &QCPHistogram::histogramReady, customPlot, [customPlot] { | QObject::connect(histogram.get(), &QCPHistogram::histogramReady, customPlot, [customPlot] { |
| customPlot->replot(); |  |
| auto* histogram = new QCPHistogram(customPlot->xAxis, customPlot->yAxis); | auto histogram = customPlot->add<QCPHistogram>(); |
| customPlot->replot(); |  |

## `docs/snippets/polar.cpp`

| before | after |
| --- | --- |
| customPlot->replot(); |  |
| customPlot->replot(); |  |

## `docs/snippets/styling.cpp`

| before | after |
| --- | --- |
| QCPGraph* sine = customPlot->addGraph(); | auto sine = customPlot->add<QCPGraph>() |
| sine->setData(t, a); | .setData(t, a) |
| sine->setName("sine"); | .setName("sine") |
| sine->setPen(QPen(QColor(77, 166, 255), 2)); | .setPen(QPen(QColor(77, 166, 255), 2)); |
| QCPGraph* cosine = customPlot->addGraph(); | auto cosine = customPlot->add<QCPGraph>() |
| cosine->setData(t, b); | .setData(t, b) |
| cosine->setName("cosine"); | .setName("cosine") |
| cosine->setPen(QPen(QColor(255, 159, 67), 2)); | .setPen(QPen(QColor(255, 159, 67), 2)); |
| customPlot->replot(); |  |
| QCPGraph* graph = customPlot->addGraph(); | auto graph = customPlot->add<QCPGraph>() |
| graph->setData(x, y); | .setData(x, y) |
| graph->setName("signal"); | .setName("signal") |
| graph->setPen(QPen(QColor("#268bd2"), 2)); | .setPen(QPen(QColor("#268bd2"), 2)); |
| customPlot->replot(); |  |
| QCPGraph* low = customPlot->addGraph(); | auto low = customPlot->add<QCPGraph>() |
| low->setData(x, lower); | .setData(x, lower) |
| low->setPen(Qt::NoPen); | .setPen(Qt::NoPen); |
| QCPGraph* high = customPlot->addGraph(); | auto high = customPlot->add<QCPGraph>() |
| high->setData(x, upper); | .setData(x, upper) |
| high->setPen(Qt::NoPen); | .setPen(Qt::NoPen) |
| high->setBrush(QColor(31, 119, 180, 60)); | .setBrush(QColor(31, 119, 180, 60)); |
| high->setChannelFillGraph(low); // fill between high and low | high->setChannelFillGraph(low.get()); // fill between high and low |
| QCPGraph* centre = customPlot->addGraph(); | auto centre = customPlot->add<QCPGraph>() |
| centre->setData(x, mid); | .setData(x, mid) |
| centre->setPen(QPen(QColor(31, 119, 180), 2)); | .setPen(QPen(QColor(31, 119, 180), 2)); |
| QCPGraph* threshold = customPlot->addGraph(); | auto threshold = customPlot->add<QCPGraph>() |
| threshold->setData(x, limit); | .setData(x, limit) |
| threshold->setPen(QPen(QColor(214, 39, 40), 1.5, Qt::DashLine)); | .setPen(QPen(QColor(214, 39, 40), 1.5, Qt::DashLine)); |
| customPlot->replot(); |  |
| QCPGraph* graph = customPlot->addGraph(); | auto graph = customPlot->add<QCPGraph>() |
| graph->setData({ x }, { y }); | .setData({ x }, { y }) |
| graph->setLineStyle(QCPGraph::lsNone); | .setLineStyle(QCPGraph::lsNone); |
| auto* label = new QCPItemText(customPlot); | auto label = customPlot->add<QCPItemText>(); |
| customPlot->replot(); |  |
| auto* line = new QCPItemLine(customPlot); | auto line = customPlot->add<QCPItemLine>(); |
| auto* label = new QCPItemText(customPlot); | auto label = customPlot->add<QCPItemText>(); |
| customPlot->replot(); |  |
| QCPGraph* graph = customPlot->addGraph(); | auto graph = customPlot->add<QCPGraph>() |
| graph->setData(x, y); | .setData(x, y) |
| graph->setPen(QPen(colors[k], 2)); | .setPen(QPen(colors[k], 2)) |
| graph->setName(QString("phase %1").arg(k)); | .setName(QString("phase %1").arg(k)); |
| customPlot->replot(); |  |
| QCPGraph* graph = customPlot->addGraph(); | auto graph = customPlot->add<QCPGraph>() |
| graph->setData(x, y); | .setData(x, y) |
| graph->setPen(QPen(QColor(31, 119, 180), offset == 0 ? 2.5 : 1.5, style)); | .setPen(QPen(QColor(31, 119, 180), offset == 0 ? 2.5 : 1.5, style)) |
| graph->setName(name); | .setName(name); |
| return graph; | return graph.get(); |
| QCPGraph* zero = customPlot->addGraph(); | auto zero = customPlot->add<QCPGraph>() |
| zero->setData(QVector<double> { 0, 10 }, QVector<double> { 0, 0 }); | .setData(QVector<double> { 0, 10 }, QVector<double> { 0, 0 }) |
| zero->setPen(QPen(QColor(150, 150, 150), 1, Qt::DotLine)); | .setPen(QPen(QColor(150, 150, 150), 1, Qt::DotLine)); |
| customPlot->replot(); |  |
| QCPGraph* graph = customPlot->addGraph(); | auto graph = customPlot->add<QCPGraph>() |
| graph->setData(x, y); | .setData(x, y) |
| graph->setPen(QPen(QColor(20, 60, 110), 2)); | .setPen(QPen(QColor(20, 60, 110), 2)); |
| customPlot->replot(); |  |
| customPlot->replot(); |  |

## `examples/axis-tags/axistag.cpp`

| before | after |
| --- | --- |
| mDummyTracer = new QCPItemTracer(mAxis->parentPlot()); | mDummyTracer = mAxis->parentPlot()->add<QCPItemTracer>().get(); |
| mArrow = new QCPItemLine(mAxis->parentPlot()); | mArrow = mAxis->parentPlot()->add<QCPItemLine>().get(); |
| mLabel = new QCPItemText(mAxis->parentPlot()); | mLabel = mAxis->parentPlot()->add<QCPItemText>().get(); |

## `examples/axis-tags/mainwindow.cpp`

| before | after |
| --- | --- |
| mGraph1 = mPlot->addGraph(mPlot->xAxis, mPlot->axisRect()->axis(QCPAxis::atRight, 0)); | mGraph1 = mPlot->add<QCPGraph>().setAxes(mPlot->xAxis, mPlot->axisRect()->axis(QCPAxis::atRight, 0)).get(); |
| mGraph2 = mPlot->addGraph(mPlot->xAxis, mPlot->axisRect()->axis(QCPAxis::atRight, 1)); | mGraph2 = mPlot->add<QCPGraph>().setAxes(mPlot->xAxis, mPlot->axisRect()->axis(QCPAxis::atRight, 1)).get(); |

## `examples/interactions/mainwindow.cpp`

| before | after |
| --- | --- |
| ui->customPlot->addGraph(); | ui->customPlot->add<QCPGraph>(); |

## `examples/plots/mainwindow.cpp`

| before | after |
| --- | --- |
| customPlot->addGraph(); | customPlot->add<QCPGraph>(); |
| customPlot->addGraph(); | customPlot->add<QCPGraph>(); |
| customPlot->addGraph(); | customPlot->add<QCPGraph>(); |
| customPlot->addGraph(); | customPlot->add<QCPGraph>(); |
| customPlot->addGraph(); | customPlot->add<QCPGraph>(); |
| customPlot->addGraph(); | customPlot->add<QCPGraph>(); |
| customPlot->addGraph(); | customPlot->add<QCPGraph>(); |
| QCPErrorBars *errorBars = new QCPErrorBars(customPlot->xAxis, customPlot->yAxis); | auto errorBars = customPlot->add<QCPErrorBars>(); |
| customPlot->addGraph(); | customPlot->add<QCPGraph>(); |
| customPlot->addGraph(); | customPlot->add<QCPGraph>(); |
| customPlot->addGraph(); | customPlot->add<QCPGraph>(); |
| customPlot->addGraph(); | customPlot->add<QCPGraph>(); |
| customPlot->addGraph(); | customPlot->add<QCPGraph>(); |
| customPlot->addGraph(); | customPlot->add<QCPGraph>(); |
| customPlot->addGraph(); | customPlot->add<QCPGraph>(); |
| QCPErrorBars *errorBars = new QCPErrorBars(customPlot->xAxis, customPlot->yAxis); | auto errorBars = customPlot->add<QCPErrorBars>(); |
| customPlot->addGraph(); | customPlot->add<QCPGraph>(); |
| customPlot->addGraph(); | customPlot->add<QCPGraph>(); |
| customPlot->addGraph(); | customPlot->add<QCPGraph>(); |
| customPlot->addGraph(); | customPlot->add<QCPGraph>(); |
| customPlot->addGraph(); // blue line | customPlot->add<QCPGraph>(); // blue line |
| customPlot->addGraph(); // red line | customPlot->add<QCPGraph>(); // red line |
| QCPCurve *fermatSpiral1 = new QCPCurve(customPlot->xAxis, customPlot->yAxis); | auto fermatSpiral1 = customPlot->add<QCPCurve>(); |
| QCPCurve *fermatSpiral2 = new QCPCurve(customPlot->xAxis, customPlot->yAxis); | auto fermatSpiral2 = customPlot->add<QCPCurve>(); |
| QCPCurve *deltoidRadial = new QCPCurve(customPlot->xAxis, customPlot->yAxis); | auto deltoidRadial = customPlot->add<QCPCurve>(); |
| QCPBars *regen = new QCPBars(customPlot->xAxis, customPlot->yAxis); | auto regen = customPlot->add<QCPBars>(); |
| QCPBars *nuclear = new QCPBars(customPlot->xAxis, customPlot->yAxis); | auto nuclear = customPlot->add<QCPBars>(); |
| QCPBars *fossil = new QCPBars(customPlot->xAxis, customPlot->yAxis); | auto fossil = customPlot->add<QCPBars>(); |
| QCPStatisticalBox *statistical = new QCPStatisticalBox(customPlot->xAxis, customPlot->yAxis); | auto statistical = customPlot->add<QCPStatisticalBox>(); |
| QCPItemText *textLabel = new QCPItemText(customPlot); | auto textLabel = customPlot->add<QCPItemText>(); |
| QCPItemLine *arrow = new QCPItemLine(customPlot); | auto arrow = customPlot->add<QCPItemLine>(); |
| QCPGraph *graph = customPlot->addGraph(); | auto graph = customPlot->add<QCPGraph>(); |
| QCPItemBracket *bracket = new QCPItemBracket(customPlot); | auto bracket = customPlot->add<QCPItemBracket>(); |
| QCPItemText *wavePacketText = new QCPItemText(customPlot); | auto wavePacketText = customPlot->add<QCPItemText>(); |
| QCPItemTracer *phaseTracer = new QCPItemTracer(customPlot); | auto phaseTracer = customPlot->add<QCPItemTracer>(); |
| QCPItemText *phaseTracerText = new QCPItemText(customPlot); | auto phaseTracerText = customPlot->add<QCPItemText>(); |
| QCPItemCurve *phaseTracerArrow = new QCPItemCurve(customPlot); | auto phaseTracerArrow = customPlot->add<QCPItemCurve>(); |
| QCPItemTracer *groupTracer = new QCPItemTracer(customPlot); | auto groupTracer = customPlot->add<QCPItemTracer>(); |
| QCPItemText *groupTracerText = new QCPItemText(customPlot); | auto groupTracerText = customPlot->add<QCPItemText>(); |
| QCPItemCurve *groupTracerArrow = new QCPItemCurve(customPlot); | auto groupTracerArrow = customPlot->add<QCPItemCurve>(); |
| QCPItemCurve *arrow = new QCPItemCurve(customPlot); | auto arrow = customPlot->add<QCPItemCurve>(); |
| QCPItemText *dispersionText = new QCPItemText(customPlot); | auto dispersionText = customPlot->add<QCPItemText>(); |
| QCPGraph *graph1 = customPlot->addGraph(); | auto graph1 = customPlot->add<QCPGraph>(); |
| QCPGraph *graph2 = customPlot->addGraph(); | auto graph2 = customPlot->add<QCPGraph>(); |
| QCPBars *bars1 = new QCPBars(customPlot->xAxis, customPlot->yAxis); | auto bars1 = customPlot->add<QCPBars>(); |
| QCPBars *bars2 = new QCPBars(customPlot->xAxis, customPlot->yAxis); | auto bars2 = customPlot->add<QCPBars>(); |
| QCPGraph *mainGraphCos = customPlot->addGraph(wideAxisRect->axis(QCPAxis::atBottom), wideAxisRect->axis(QCPAxis::atLeft)); | auto mainGraphCos = customPlot->add<QCPGraph>().setAxes(wideAxisRect->axis(QCPAxis::atBottom), wideAxisRect->axis(QCPAxis::atLeft)); |
| QCPGraph *mainGraphGauss = customPlot->addGraph(wideAxisRect->axis(QCPAxis::atBottom), wideAxisRect->axis(QCPAxis::atLeft, 1)); | auto mainGraphGauss = customPlot->add<QCPGraph>().setAxes(wideAxisRect->axis(QCPAxis::atBottom), wideAxisRect->axis(QCPAxis::atLeft, 1)); |
| QCPGraph *subGraphRandom = customPlot->addGraph(subRectLeft->axis(QCPAxis::atBottom), subRectLeft->axis(QCPAxis::atLeft)); | auto subGraphRandom = customPlot->add<QCPGraph>().setAxes(subRectLeft->axis(QCPAxis::atBottom), subRectLeft->axis(QCPAxis::atLeft)); |
| QCPColorMap *colorMap = new QCPColorMap(customPlot->xAxis, customPlot->yAxis); | auto colorMap = customPlot->add<QCPColorMap>(); |
| QCPFinancial *candlesticks = new QCPFinancial(customPlot->xAxis, customPlot->yAxis); | auto candlesticks = customPlot->add<QCPFinancial>(); |
| QCPFinancial *ohlc = new QCPFinancial(customPlot->xAxis, customPlot->yAxis); | auto ohlc = customPlot->add<QCPFinancial>(); |

## `examples/scrollbar-axis-range-control/mainwindow.cpp`

| before | after |
| --- | --- |
| ui->plot->addGraph(); | ui->plot->add<QCPGraph>(); |
| ui->plot->addGraph(); | ui->plot->add<QCPGraph>(); |

## `examples/text-document-integration/mainwindow.cpp`

| before | after |
| --- | --- |
| ui->plot->addGraph(); | ui->plot->add<QCPGraph>(); |
| ui->plot->addGraph(); | ui->plot->add<QCPGraph>(); |

## `examples/theme-demo/main.cpp`

| before | after |
| --- | --- |
| auto* graph1 = plot->addGraph(); | auto graph1 = plot->add<QCPGraph>(); |
| auto* graph2 = plot->addGraph(); | auto graph2 = plot->add<QCPGraph>(); |
| auto* textItem = new QCPItemText(plot); | auto textItem = plot->add<QCPItemText>(); |

## `replot()` calls that stay, and why

91 `replot()` calls were removed (snippets: 89 plain "show the change" calls, plus the two in
`examples`). These 10 stay:

| site | why it stays |
| --- | --- |
| `docs/snippets/concepts.cpp:84` | the snippet reads `coordToPixel` right after, which needs the layout pass |
| `docs/snippets/concepts.cpp:119` | `concepts_replot_batch` — the snippet's subject *is* batching one replot |
| `docs/snippets/concepts.cpp:128` | `concepts_queued_replot` — the snippet's subject *is* `rpQueuedReplot` |
| `docs/snippets/gallery.cpp:233` | `recolour` lambda in a buffered-layer demonstration, queued on purpose |
| `docs/snippets/gallery.cpp:244` | `setScaleRatio` on the next line needs the laid-out rect |
| `docs/snippets/interactions.cpp:287` | `coordToPixel` right after |
| `docs/snippets/performance.cpp:122` | `layer("cursor")->replot()` is the point: only that layer is redrawn |
| `docs/snippets/performance.cpp:138` | the snippet measures a data swap after a settled frame |
| `docs/snippets/main.cpp:56,62` | the documentation harness: it renders a fixed frame, twice, on purpose |

`docs/migrating.md`'s 2.x "before" examples were left untouched, and its "after" examples already
used the new API.

## Images

`doc-shots docs/images` was run after the port and every PNG compared with a copy of the committed
one (`build-qt612/old-images/`, inside the worktree). To separate the port from the environment, the
same run was repeated with the pre-port sources (`git stash push -- docs/snippets`):

- **port vs pre-port renders: 0 of 89 differ.** Every image is byte-identical, so the port paints
  the same thing.
- **pre-port renders vs the committed PNGs: 3 of 89 differ** — `colormaps-contours`,
  `colormaps-histogram2d-contours`, `gallery-fit-residuals`. They differ *without* the port too, so
  they are pre-existing drift (font metrics of the math text and of contour labels in this Qt
  build), not a regression. `docs/images` was restored to the committed files, so this commit
  carries no image change.
- A subset run (`doc-shots <dir> <name>`) renders the two contour snippets differently from a full
  run: they depend on the math-text cache being warm. All comparisons above are full runs.
