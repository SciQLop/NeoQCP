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

```{literalinclude} snippets/migrating.cpp
:start-after: // [migrating_nodiscard]
:end-before: // [/migrating_nodiscard]
:language: cpp
:dedent:
```

## Behaviour that changed

Most changes are confined to the new classes. A few touch existing code.

- **Item positions.** Items far outside the visible range no longer abort under Qt 6.11's checked integer rectangles. Pixel geometry of items uses floating point.
- **Axis labels.** An axis label longer than its axis is elided with "…" instead of being cut at both ends.
- **Log axes.** On a log axis spanning less than 1.6 decades, upstream falls back to linear ticks. NeoQCP puts the ticks on round mantissas (2, 5, ...) instead.
- **Keyboard.** `QCustomPlot` now handles key presses. Each plottable sees the key first, through the new virtual `QCPAbstractPlottable::keyPress()`. Delete and Backspace make every selected item emit `QCPAbstractItem::deleteRequested()`; nothing is deleted for you.
- **Fixed upstream bugs.** DPI scaling on macOS and Windows; a crash of the date-time ticker on Qt 6.10 and newer; a use-after-delete in `removeLayer()`.
- **Slots may delete their sender.** A slot connected to a NeoQCP signal can delete the object that emitted it: a plottable, an item, an axis rect, the legend, a colour scale, even the plot. Upstream this often crashed. To make it safe, a few signals moved or went away:
  - Deleting a selection rect no longer emits its `canceled` signal. If your code resets its own drag state on `canceled`, also watch the rect's `destroyed` signal.
  - `setSelectionRect()` with the rect already in use does nothing. It used to delete and reinstall it.
  - Deleting a `QCPColorScale` detaches the colour maps that use it, without a signal. Their `colorScale()` then returns `nullptr`.
- **Span dragging.** Dragging a `QCPItemSpan` or `QCPItemRSpan` moves its edges by the mouse movement in pixels, whatever the type of their positions. An edge placed in pixels used to move by the wrong amount. If the axis moves during the drag, the edge now stays under the pointer.
- **Native pinch.** A macOS pinch that the plot does not zoom on is left to the parent widget. It used to be swallowed.
- **New axes are followed.** `QCPGraph`, `QCPColorMap2`, `QCPHistogram2D` and `QCPHistogram` follow the scale and range of an axis given later with `setKeyAxis()` or `setValueAxis()`, not only the axes they were built with.

The classic plottables (`QCPCurve`, `QCPBars`, `QCPColorMap`, ...) keep their data model and behaviour. Their solid lines are drawn by the GPU now; dashed lines and scatters still use `QPainter`. `QCPGraph` is not classic any more: see "One graph" below.

The behaviours below belong to the new plottables only. Code that keeps using the classic ones is not affected.

- **Gaps.** `QCPGraph` and `QCPColorMap2` detect missing stretches of data and leave them blank instead of joining them. 2.x only broke the line at NaN values. See {doc}`architecture/gap-detection`.
- **Asynchronous resampling.** `QCPGraph`, `QCPColorMap2` and `QCPHistogram2D` do their heavy work on worker threads. The first picture of large data can come a moment after `replot()`.
- **Deferred data swap.** New data on a `QCPGraph` that is already on screen is shown after a short window (200 ms by default), not at once. Until then the graph reports the old data.
- **Busy state.** These plottables report `busy()` and emit `busyChanged()`. A plottable busy for long is drawn faded, with a ⟳ in its legend entry.

- **Legend of a graph with several columns.** The legend row of a `QCPGraph` shows only the components in `legendComponents()`. The bound columns of error bars leave it on their own; `setComponentInLegend()` changes that.
- **Empty data on a graph.** A data source with no rows keeps the components, with their names and styles. It used to reset their number.
- **Lane layouts.** `QCPLaneLayout` setters and `QCPIntervals::setCategoryOrder()` emit nothing, and restack nothing, when the value does not change.
- **First colour map frame.** `QCPColorMap2` resamples one event-loop turn after it is shown or resized, not inside the paint. Exports still resample at once.
- **Contour segments.** `QCPContourExtractor::extract()` returns segments in the image's [0, 1] coordinates, traced on at most 300 by 300 cells by default. It used to return them in data units, on the full grid.
- **Graph keys are sorted.** `QCPGraph::setData()` with `double` keys and `double` columns builds a growable source and sorts unsorted keys, unless `alreadySorted = true`. Other value types keep the source they came in.
- **Graph shape misuses.** A `QCPGraph::setData()` or `addData()` whose columns do not have one value per key, or whose keys are not finite, reports one misuse and changes nothing. 2.x truncated unequal data.
- **The graph needs less from its system.** `QCPGraph` requires only `capStraightSegments`, so a straight-segment coordinate system of your own hosts it. `QCPWaterfallGraph` still requires a rectangle and a straight key and is refused there.
- **`QCPGraph::addData()` and one-column `setData()`.** `addData()` appends to the source a `double` `setData()` built; a view, a shared source, a non-double source and a waterfall refuse it. `setData(keys, values)` takes two `QVector<double>` or two `std::vector<double>` for a one-column graph.
- **A tracer can follow a graph component.** `QCPItemTracer::setGraph(graph, component)` and `setComponent(component)` follow one column; the tracer is inert when the graph or the component goes.

{doc}`performance` explains the asynchronous points and what they mean for code that reads results right after `setData()`.

## Coordinate systems

NeoQCP 3.0 draws every plottable and item through a `QCPCoordinateSystem`. The classic cartesian frame is one, a polar area is another, and your own subclass can be a third (see {doc}`coordinate-systems`). Most 2.x code is unchanged, but these points matter if you write a plottable or reach for axes through it:

- **`QCP::capStraightSegments`.** A graph or curve now needs only this capability, not a rectangle and a straight key. A custom system that keeps straight lines straight can host them. Every other plottable keeps the 2.x default (`capRectangular | capStraightKey`).
- **`keyAxis()` and `valueAxis()` can be null.** On a non-cartesian system, including a user system and a polar area, a plottable's and an item position's `keyAxis()`/`valueAxis()` return `nullptr`; 2.x pixel code must not assume an axis. Use `system()`, `keyAxisBase()` and `valueAxisBase()` to reach the axes on any system.
- **`requiredCapabilities()` is a stored mask.** It is no longer virtual: the last argument of the `QCPAbstractPlottable` constructors is the `QCP::Capabilities` mask, and every intermediate constructor forwards it. An out-of-tree plottable that overrode `requiredCapabilities()` must pass the mask to its base constructor instead; an old override hides the accessor and is ignored.
- **A refused plottable is inert.** A plottable whose system lacks a capability it needs is built but joins no layer and no list, with one misuse report. `QCustomPlot::add<T>()` returns an empty handle for it, and `add(std::unique_ptr)` refuses it before touching the object. A later `setSystem()` with a system that admits it makes it a normal member of the plot.
- **Layout elements stay in their plot.** Adding an axis rect or other layout element that belongs to another plot to this plot's layout is a misuse; the element stays in its old layout and plot, also when it is nested inside a plot-less layout first. Build a new element in the other plot instead.

## Getters return handles

Getters that hand out an object the plot owns now return a `QCPHandle<T>`, not a `T*`.
A handle is a weak pointer: it reads as null when the object is deleted, and calling through a dead one aborts with a message.
The members `xAxis`, `yAxis`, `xAxis2`, `yAxis2` and `legend` are handles too, under the same names.
{doc}`concepts` explains handles, and which getters stay raw.

A handle converts to `T*` by itself, so most 2.x code compiles as it is: `plot->xAxis->setRange(0, 1)`, `connect(plot->xAxis, ...)`, `if (plot->legend)`, `QCPAxis* a = rect->axis(QCPAxis::atBottom)`.
These patterns stop compiling:

| 2.x code | What to write |
|---|---|
| `auto* x = rect->axis(type);` | `auto x = ...` (a handle), or `auto* x = rect->axis(type).get();` |
| `QList<QCPAxis*> l = rect->axes();` | `QList<QCPAxis*> l = qcp::pointers(rect->axes());` |
| `QCOMPARE(rect->axes(), expected)` with a `QList<T*>` | `QCOMPARE(qcp::pointers(rect->axes()), expected)` |
| `list.contains(rect->axis(type))`, `list.removeOne(...)` on a `QList<T*>` | `list.contains(rect->axis(type).get())` |
| `cond ? rect->axis(a) : nullptr` bound to `auto*` | `.get()` inside the expression |
| a template that deduces `T*` from the getter | pass `.get()` |
| `QPointer<T> p = getter();` | `QPointer<T> p(getter());` |
| `qcpConnect(h, ...)` | `QObject::connect(h, ...)` |

```{literalinclude} snippets/migrating.cpp
:start-after: // [migrating_handles]
:end-before: // [/migrating_handles]
:language: cpp
:dedent:
```

`qcpConnect` is gone.
Setters that take a list still take a `QList<T*>`.
`delete handle` compiles and skips `remove()`; delete through the plot instead.
The graph list returns handles too: `graph(i)`, `addGraph()`, `selectedGraphs()`, `graphs()`.
Signals carry raw pointers, as before.

## One graph

NeoQCP 3.0 has one graph class, `QCPGraph`.
It is what the 3.0 previews called `QCPMultiGraph`: one key column, N value columns, an asynchronous resampler, and coordinate systems of your own.
The 2.x `QCPGraph` (a container of `QCPGraphData`) and the preview `QCPGraph2` are gone.
A graph with one column is the 2.x graph: `add<QCPGraph>().setData(keys, values)` still works, and so do `addGraph()`, `graph(i)`, `graphs()` and `selectedGraphs()`.
These patterns need a change:

| Before | What to write |
|---|---|
| `QCPMultiGraph` (a 3.0 preview class) | `QCPGraph`. The helper classes follow: `QCPGraphOps`, `QCPGraphPipeline`, `QCPGraphComponent`. |
| `QCPGraph2` | `QCPGraph`. `setData()`, `viewData()` and `setDataSource()` keep their calls; the source is a `QCPSoAMultiDataSource`. |
| `graph->data()`, the shared `QSharedPointer<QCPGraphDataContainer>` | Nothing holds the container any more. Write with `setData()`, `addData()`, `removeDataBefore()`, `removeDataAfter()` and `removeData(from, to)`. Read with `dataSource()`. Share data between graphs with one source and `setDataSource()`. |
| `graph->setChannelFillGraph(other)` | `graph->setComponentFillToGraph(0, other, 0)`. The graphs must share the key axis; their value axes may differ. |
| `tracer->setGraph(graph)` and `setLineGraph(graph, component)` | `tracer->setGraph(graph, component)`. `lineGraph()` is `graph()`. |
| `QCPGraph::LineStyle` | The same names: `QCPGraph::lsLine` and the others compile as before. |
| A graph that drew with `QCPGraph2::setScatterColorValues()` | `setColorValues()` and `setGradient()`. |
| `QCPGraph2::setScatterMaxPoints()` | Removed with the class. There is no replacement yet. |

Behaviour that changed:

- **Default pen.** A new graph has the next colour of the palette and a pen 2.5 pixels wide. 2.x drew every graph in blue with a cosmetic pen (width 0). Call `setPen()` to get the old look back.
- **Shape misuses.** Keys and values of different lengths, and a NaN key, are misuses: the call reports one and changes nothing. 2.x truncated unequal data and let NaN keys through.
- **Sorting.** `setData()` with `double` keys and `double` columns sorts unsorted keys, unless `alreadySorted = true`. Other types must come sorted.
- **Selection.** A selected graph draws its selected segments with the selection decorator. Markers of a selected segment take the selected pen as their outline.
- **Legend.** The row of a graph with one column shows the graph's name, unless you named the column yourself. A graph without data shows its pen next to its name.
- **Logarithmic value axis.** A fill to a value the axis cannot place, and the baseline of impulses, go to the range bound nearer zero.
- **Brushes.** `setBrush()` fills every column down to zero and replaces fills to a constant or between columns. Fills to another graph stay and take the new brush.
- **Equal values.** `setPen()` with the pen the graph already has does not repaint.
- **New virtual.** `QCPAbstractPlottable::assignSelection(const QCPDataSelection&)` is a new protected virtual, called when the selection is set. A plottable of your own that has a method of that name must rename it.

## New classes

These do not exist upstream. You do not need them to migrate, but they are where most of the speed and new features are.

Plottables:

- <a href="api/classQCPGraph.html">QCPGraph</a> is rebuilt: one key column and any number of value columns, from your own containers in their own numeric type, without copying. Resamples large data on a worker thread. Each component has its own pen, name and selection. Can colour lines by a scalar, fill under a component or between two, and draw error bars. See {doc}`graphs` and the next section.
- <a href="api/classQCPWaterfallGraph.html">QCPWaterfallGraph</a>: a `QCPGraph` that stacks its traces with offsets, optionally normalised.
- <a href="api/classQCPColorMap2.html">QCPColorMap2</a>: a colour map over zero-copy data, resampled on a worker thread, with gaps, log axes and GPU contour lines with labels. See {doc}`colormaps`.
- <a href="api/classQCPHistogram2D.html">QCPHistogram2D</a>: bins a point cloud into a colour map, on a worker thread, with contour lines.
- <a href="api/classQCPHistogram.html">QCPHistogram</a>: a 1-D histogram of every column of a data source, on shared bins, binned on a worker thread. See {doc}`plottables`.
- <a href="api/classQCPIntervals.html">QCPIntervals</a>: a timeline of intervals on lanes, editable with the mouse and the keyboard. See {doc}`plottables`.

Data sources, for the zero-copy plottables: `QCPSoADataSource`, `QCPSoAMultiDataSource`, `QCPRowMajorMultiDataSource`, `QCPSoADataSource2D`, and their abstract bases.

Items and overlays:

- <a href="api/classQCPItemSpan.html">QCPItemSpan</a> (vertical or horizontal) and `QCPItemRSpan`: shaded key, value or rectangular ranges, drawn on the GPU, draggable. See {doc}`items`.
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
| Overriding `QCPAbstractItem::draw(QCPPainter*)` (it is final) | override `draw(QCPDrawContext&)` and record what the item draws |
| Overriding `QCPStatisticalBox::drawStatisticalBox()` | override `draw(QCPDrawContext&)` |
| `QCPPolarAxisAngular` as a layout element (`addElement`, its background, inset layout, drag and wheel, `addRadialAxis`, `add<QCPPolarGraph>`) | `QCPPolarArea`, which owns the angular axis, the radial axes and the grid |
| `QCPPolarAxisRadial::ScaleType`, the polar axes' `SelectablePart(s)`, `rangeDrag`/`rangeZoom` | `QCPAxisBase::ScaleType`, `QCPAxisBase::SelectablePart(s)`, `rangeDraggable`/`rangeZoomable` |
| Overriding `QCPAbstractPlottable::axesChanged()` (NeoQCP 3.0 preview) | override `systemChanged()`: a plottable now moves between coordinate systems |
| `QCPGraph2`, `QCPMultiGraph`, `QCPGraph::data()` (NeoQCP 3.0 preview) | see "One graph" above |
| `QCPPolarArea::removeGraph(graph)` | `plot.remove(graph)`: the plot owns polar graphs; the area no longer deletes them |
| `QCPPolarLegendItem`, `QCPPolarGraph::addToLegend`/`removeFromLegend` (polar overloads) | the plottable ones: a polar graph has a `QCPPlottableLegendItem` row |
| `QCPCoordinateSystem::ratioTransform()`, `frameRatioTransform()` (NeoQCP 3.0 preview) | `QCPArea::ratioTransform()`: the ratio space belongs to the area |
| `setRangeZoomFactor` on each radial axis | `QCPPolarArea::setRangeZoomFactor` (one factor for the area) |
| String-based connections naming `QCPAxis::ScaleType` or `QCPAxis::SelectableParts`, such as `SIGNAL(scaleTypeChanged(QCPAxis::ScaleType))` | pointer-to-member `connect` (`&QCPAxis::scaleTypeChanged`), or spell the type `QCPAxisBase::ScaleType`: the enums now live in `QCPAxisBase` |

Everything else in the public API of QCustomPlot 2.1.1 is still there. The polar classes were a 2.1 tech preview; their changes are listed above.
