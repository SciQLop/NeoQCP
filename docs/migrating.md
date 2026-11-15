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
- **Keyboard.** `QCustomPlot` now handles key presses. Each plottable sees the key first, through the new virtual `QCPPlottable::keyPress()`. Delete and Backspace make every selected item emit `QCPItem::deleteRequested()`; nothing is deleted for you.
- **Fixed upstream bugs.** DPI scaling on macOS and Windows; a crash of the date-time ticker on Qt 6.10 and newer; a use-after-delete in `removeLayer()`.
- **Slots may delete their sender.** A slot connected to a NeoQCP signal can delete the object that emitted it: a plottable, an item, an axis rect, the legend, a colour scale, even the plot. Upstream this often crashed. To make it safe, a few signals moved or went away:
  - Deleting a selection rect no longer emits its `canceled` signal. If your code resets its own drag state on `canceled`, also watch the rect's `destroyed` signal.
  - `setSelectionRect()` with the rect already in use does nothing. It used to delete and reinstall it.
  - Deleting a `QCPColorScale` detaches the colour maps that use it, without a signal. Their `colorScale()` then returns `nullptr`.
  - A signal argument that points to an object arrives as `nullptr` when that object was deleted before the signal was delivered.
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
- **`requiredCapabilities()` is a stored mask.** It is no longer virtual: the last argument of the `QCPPlottable` constructors is the `QCP::Capabilities` mask, and every intermediate constructor forwards it. An out-of-tree plottable that overrode `requiredCapabilities()` must pass the mask to its base constructor instead; an old override hides the accessor and is ignored.
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

- **Default pen.** Column *i* of a new graph takes colour *i* of the palette, with a cosmetic pen (width 0). A graph with one column is drawn in the palette's first colour, a blue close to 2.x's. Call `setPen()` for another look.
- **Shape misuses.** Keys and values of different lengths, and a NaN key, are misuses: the call reports one and changes nothing. 2.x truncated unequal data and let NaN keys through. `removeData()`, `removeDataBefore()` and `removeDataAfter()` at a NaN key are misuses too; under the default handler a debug build aborts.
- **Sorting.** `setData()` with `double` keys and `double` columns sorts unsorted keys, unless `alreadySorted = true`. Other types must come sorted.
- **Selection.** A selected graph draws its selected segments with each column's selected pen (the column's pen, 1.5 pixels wider). When you style the selection decorator or set one of your own, its pen, brush and marker style draw them instead. Markers of a selected segment take the selected pen as their outline. The selection type works as in 2.x: with the default `stWhole`, a click selects the whole graph and a click with the multi-select modifier deselects it.
- **Hit details.** `selectTest()` hands a graph's hit back as a `QVariantMap` with `componentIndex` and `dataIndex`, not as a `QCPDataSelection`. `details.value<QCPDataSelection>()` gives an empty selection. Read the row with `qcp::hitDataIndex(details)`, which takes either form.
- **Legend.** The row of a graph with one column shows the graph's name, unless you named the column yourself. A graph without data shows its pen next to its name.
- **Logarithmic value axis.** A fill to a value the axis cannot place, and the baseline of impulses, go to the range bound nearer zero.
- **Brushes.** `setBrush()` fills every column without a fill down to zero. A column that already has a fill keeps where it fills to (a constant, another column, another graph) and takes the new brush. `setBrush(Qt::NoBrush)` removes every fill, the ones to another graph too, as 2.x drew no channel without a brush. A later `setBrush()` fills to zero: call `setComponentFillToGraph()` again to get the band back.
- **Fill partners.** A fill to another graph goes when that graph leaves the plot or its frame (taken, or moved to another axis rect), as it goes when that graph is deleted.
- **Removing points.** `removeData(from, to)` removes nothing when `from >= to`, as 2.x.
- **Equal values.** `setPen()` with the pen the graph already has does not repaint.
- **New virtual.** `QCPPlottable::assignSelection(const QCPDataSelection&)` is a new protected virtual, called when the selection is set. A plottable of your own that has a method of that name must rename it.

## Properties, change signals and bindings

Every property you can set has a change signal, `xChanged(T)`, carrying the new value.
So every such property can be bound from C++, with Qt's property bindings.

Bind with `QBindable<T>(object, "name")`.
There are no `bindableX()` accessors.
Inside a binding, read a property with `value()`: that is what makes it a dependency.

```{literalinclude} snippets/migrating.cpp
:start-after: // [migrating_bindings]
:end-before: // [/migrating_bindings]
:language: cpp
:dedent:
```

A binding writes through the setter, like a call.
A setter that refuses a value refuses it from a binding too.
A plain write does not break a binding: `setLabel("x")` on a bound label sets it, and the next change of a
dependency writes over it.

Change signals are deferred, like every NeoQCP signal.
They arrive once, after the operation that caused them, not in the middle of it.
A write of an equal value sends nothing.

A binding must not keep a `QBindable` of another object.
That `QBindable` dies with its object.
A binding that captured one crashes on its next evaluation once the object is deleted.
Hold the source by `QPointer` instead, and build a fresh `QBindable` inside the binding:

```{literalinclude} snippets/migrating.cpp
:start-after: // [migrating_binding_guarded]
:end-before: // [/migrating_binding_guarded]
:language: cpp
:dedent:
```

A few more things to know:

- **A pointer that goes null on its own has no signal.** A plottable's axes, an error bar's data plottable and a tracer's graph are held weakly. When the target is deleted, the property reads null, but its change signal does not fire. The target's `destroyed()` is the notification.
- **Don't delete the plot from a slot that runs during teardown, or from inside a destructor.** Signals sent while an object is being destroyed are delivered at once, and NeoQCP cannot protect a slot that deletes the plot there. Everywhere else a slot may delete its sender, or the plot.
- **A late connect can miss one change.** NeoQCP queues a change signal only if something listens at that moment. A slot connected after the write, but before delivery, does not get that change. Read the property once when you connect.

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
| Overriding `QCPItem::draw(QCPPainter*)` (it is final) | override `draw(QCPDrawContext&)` and record what the item draws |
| Overriding `QCPPlottable::draw(QCPPainter*)` or `drawLegendIcon(QCPPainter*, const QRectF&)` (both final) | override `draw(QCPDrawContext&)`, and `drawLegendIcon(QCPDrawContext&)` for an icon of its own; `context.painter()` keeps painter code no command expresses |
| `QCPPlottable1D::drawPolyline()`, `QCPPlottable::applyFillAntialiasingHint()`/`applyScattersAntialiasingHint()`, `QCPSelectionDecorator::applyPen()`/`applyBrush()` | record through `draw(QCPDrawContext&)`; inside a `context.painter()` callback, set the pen, brush and antialiasing on the painter directly |
| Overriding `QCPStatisticalBox::drawStatisticalBox()` | override `draw(QCPDrawContext&)` |
| `QCPPlottableLegendItem::getIconBorderPen()` | `legendIconBorderPen()`, on every legend row |
| `QCPPolarAxisAngular` as a layout element (`addElement`, its background, inset layout, drag and wheel, `addRadialAxis`, `add<QCPPolarGraph>`) | `QCPPolarArea`, which owns the angular axis, the radial axes and the grid |
| `QCPPolarAxisRadial::ScaleType`, the polar axes' `SelectablePart(s)`, `rangeDrag`/`rangeZoom` | `QCPAxisBase::ScaleType`, `QCPAxisBase::SelectablePart(s)`, `rangeDraggable`/`rangeZoomable` |
| Overriding `QCPPlottable::axesChanged()` (NeoQCP 3.0 preview) | override `systemDidChange()`: a plottable now moves between coordinate systems |
| Overriding `QCPPlottable::systemChanged()` (NeoQCP 3.0 preview) | override `systemDidChange()`: `systemChanged(QCPCoordinateSystem*)` is now the `system` property's signal |
| `QCPGraph2`, `QCPMultiGraph`, `QCPGraph::data()` (NeoQCP 3.0 preview) | see "One graph" above |
| `QCPPolarArea::removeGraph(graph)` | `plot.remove(graph)`: the plot owns polar graphs; the area no longer deletes them |
| `QCPPolarLegendItem`, `QCPPolarGraph::addToLegend`/`removeFromLegend` (polar overloads) | the plottable ones: a polar graph has a `QCPPlottableLegendItem` row |
| `QCPCoordinateSystem::ratioTransform()`, `frameRatioTransform()` (NeoQCP 3.0 preview) | `QCPArea::ratioTransform()`: the ratio space belongs to the area |
| `setRangeZoomFactor` on each radial axis | `QCPPolarArea::setRangeZoomFactor` (one factor for the area) |
| String-based connections naming `QCPAxis::ScaleType` or `QCPAxis::SelectableParts`, such as `SIGNAL(scaleTypeChanged(QCPAxis::ScaleType))` | pointer-to-member `connect` (`&QCPAxis::scaleTypeChanged`), or spell the type `QCPAxisBase::ScaleType`: the enums now live in `QCPAxisBase` |
| `QCPPlottable::mKeyAxis`, `mValueAxis` (protected members, now private) | in a subclass, the protected `keyAxisRaw()`/`valueAxisRaw()` (raw pointers, for per-point code) or `system()`; from outside, `keyAxis()`/`valueAxis()` (handles). All are null on a non-cartesian system |
| `QCPItemPosition::mKeyAxis`, `mValueAxis`, `mAxisRect` (protected members, now private) | the position's `keyAxis()`/`valueAxis()`/`axisRect()` (handles) or `system()`/`area()`. A one-axis position (`setAxes(nullptr, axis)`) keeps its axis and has no system; on a non-cartesian system the axes are null, and `axisRect()` is null on an area that is not an axis rect |
| `QCPMarginGroup`, `QCPLayoutElement::setMarginGroup()`/`marginGroup()`/`marginGroups()` | `QCPAlignmentGroup::create({ a, b }, sides)`. Membership is per side, so one element can be in a left/right group and in a top/bottom group: `a->setMarginGroup(QCP::msLeft, g); b->setMarginGroup(QCP::msLeft, g);` becomes `QCPAlignmentGroup::create({ a, b }, QCP::msLeft);`. A group lines up inner edges instead of making margins equal; for elements in one column or one row this is the same. Stacked rows grouped on their top and bottom sides too (`msAll`) have no port: group their left and right sides only, and set the top and bottom margins by hand if they must match |
| `QCPLayoutElement::alignmentGroup()` (3.0 preview, one group per element) | `alignmentGroup(side)`: an element holds one group per side |
| `QCPItemSpan`/`QCPItemRSpan` `setPen()`, `setSelectedPen()`, `pen()`, `selectedPen()` and the `pen`/`selectedPen` properties | nothing: a span never drew its pen. It has no pen, so `handle.setPen(...)` on a span handle no longer compiles. Use `setBorderPen()`/`setSelectedBorderPen()` for the edge lines and `setBrush()` for the band |

Everything else in the public API of QCustomPlot 2.1.1 is still there. The polar classes were a 2.1 tech preview; their changes are listed above.

## Base classes drop "Abstract"

The base classes of the 3.0 line lose the word. No alias is kept: rename the name in your code.

| Old | New |
|---|---|
| `QCPAbstractPlottable` | `QCPPlottable` |
| `QCPAbstractPlottable1D` | `QCPPlottable1D` |
| `QCPAbstractItem` | `QCPItem` |
| `QCPAbstractLegendItem` | `QCPLegendItem` |
| `QCPAbstractLine` | `QCPLine` |
| `QCPAbstractColorMapped` | `QCPColorMapped` |
| `QCPAbstractSpanItem` | `QCPSpanItem` |
| `QCPAbstractDataSource` | `QCPDataSource` |
| `QCPAbstractDataSource2D` | `QCPDataSource2D` |
| `QCPAbstractMultiDataSource` | `QCPMultiDataSource` |

`QCPAbstractPaintBuffer` is internal and keeps its name. Source files such as `abstract-datasource.h` keep their names.

## `update()` redraws, `updateLayout()` lays out

`update()` now means one thing: draw this object again. No forwarder is kept.

| Old | New |
|---|---|
| `QCPLayerable::markDirty()` and every override | `QCPLayerable::update()` |
| `contentChanged()` on a plottable or an item | `update()` |
| `QCPLayoutElement::update(UpdatePhase)` and every override | `QCPLayoutElement::updateLayout(UpdatePhase)` |
| `QCPLayout::updateLayout()` (protected hook) | `layoutChildren()` |
| `QCPDrawnCore::contentChanged()` | `newGeneration()` |

A layout element override that keeps the old `update(UpdatePhase)` name no longer compiles when it says `override`.
`QCPLayer::markDirty()` is a layer-level repaint request and keeps its name.
`QCPLayout::updateLayout()` (the protected hook that positions the children) is `layoutChildren()`, so that the two
`updateLayout` names do not hide each other in a subclass.
`QCPDrawnCore::contentChanged()` (the generation bump behind `update()`) is `newGeneration()`.

## Items are built without a plot

An item's constructor takes the item's own arguments only. It no longer takes the plot: the item is
built detached, with its positions and anchors, and `add<T>()` places it.

| Old | New |
|---|---|
| `new QCPItemText(customPlot)` | `customPlot->add<QCPItemText>()` |
| `new QCPItemSpan(customPlot, Qt::Vertical)` | `customPlot->add<QCPItemSpan>(Qt::Vertical)` |
| `new MyItem(customPlot, ...)` | `customPlot->add<MyItem>(...)`, with the plot dropped from `MyItem`'s constructor |

The arguments of `add<T>()` are constructor arguments, and they are forwarded: a braced initialiser
needs a type where a direct `new` accepted `{ }`, as in `add<MyItem>(QRectF(), true)`.

Where the item lands decides what it is bound to:

- `axisRect->add<T>(...)` clips the item to that rect, puts it on the `annotations` layer, and binds its
  positions that name no system to `axisRect->xAxis()` and `axisRect->yAxis()`, the rect's first bottom
  and left axes. A rect that lost one of them refuses the item: the call returns an empty handle,
  reports once, and owns nothing.
- `customPlot->add<T>(...)` does the same in the plot's axis rect. A plot with no axis rect still takes
  an item, for a polar-only scene or an annotation placed in pixels: it joins unbound and unclipped, and
  the caller binds the positions to a system.
- A position that names a system, or already has an area, before the add keeps it. Only the positions
  that name none take the target's pair.

An item is clipped to the area it lands in unless it was told otherwise: `setClipToAxisRect(false)`
before or after the add, and across a `take()`/`add()` round trip, keeps an item that draws over the
axis margins unclipped.

The theme fills only what nobody set: `QCPItemText` keeps a colour given before the add — in a
`setColor()` call or in a subclass constructor — and the plot's theme colours the rest. The same
holds for a colour map joining its rect's colour scale: a gradient or data range chosen with
`setGradient()` or `setDataRange()` before the add is the one kept, and the scale fills what is
still unset.

An item outside a plot — built and not placed yet, or taken with `QCustomPlot::take()` — answers
`(0, 0)` from its positions' `pixelPosition()`, and its hit test uses no selection tolerance. Nothing
warns.

A user item may create and place its positions in its constructor or in a member initialiser, as before;
`createPosition()` no longer binds them to a plot.

### A text style's colour is unset until someone sets it

`QCPTextStyle::color` now defaults to an invalid `QColor`, meaning "the foreground of whoever draws
this", instead of black. A recording that never named a colour draws in the painter's colour, and
`ax->text()` leaves the item's own colour — the one the plot's theme gives it — in place:

| Old | New |
|---|---|
| `QCPTextStyle style; style.color = Qt::black;` | `QCPTextStyle style;` (unset) or `style.color = plot->theme()->foreground();` |

## One-call annotations: `ax->line`, `ax->text`, `ax->ellipse`

An axis rect makes the three common annotations without an add and a chain of setters:

| Before | Now |
| --- | --- |
| `add<QCPGraph>(keys, values).setPen(p).setName("n")` | `ax->line(keys, values, { .pen = p, .name = "n" })` |
| `add<QCPGraph>(keys, values).setScatterStyle(s)` with `setPen(Qt::NoPen)` | `ax->line(keys, values, { .pen = Qt::NoPen, .scatterStyle = s })` |
| `add<QCPItemText>()` + `position->set(...)` + `setText()` + setters | `ax->text(at, "t", style)` |
| `add<QCPItemEllipse>()` + `topLeft->set(...)` + `bottomRight->set(...)` + `setPen()`/`setBrush()` | `ax->ellipse(a, b, { .pen = p, .brush = b })` |

`QCPLineOptions` is `{ QPen pen; QString name; QCPScatterStyle scatterStyle; }`, field for field the setters it
feeds, so a designated initialiser list reads as the calls it replaces. `QCPShapeOptions` is `{ QPen pen; QBrush
brush; }`. The positions are the `QCP::Position` spaces (`QCP::Data`, `QCP::Pixels`, `QCP::AxisRatio`, ...), so an
ellipse corner in data coordinates and the other in pixels is two arguments, and a text anchor is wherever it
should sit. A text style field left at its default leaves the item's own (its alignment, and the colour the plot
themes), so `ax->text(at, "t")` is a plain annotation.

## Plottables are built without axes

A plottable's constructor takes the plottable's own arguments only. It no longer takes the key and
value axes: the plottable is built detached and `add<T>()` joins it to a system.

| Old | New |
|---|---|
| `new QCPGraph(customPlot->xAxis, customPlot->yAxis)` | `customPlot->add<QCPGraph>()` |
| `new QCPBars(keyAxis, valueAxis)` | `rect->add<QCPBars>(keyAxis, valueAxis)` (or `rect->add<QCPBars>()` for its first bottom and left axes) |
| `new QCPIntervals(keyAxis, valueAxis, &lanes)` | `rect->add<QCPIntervals>(&lanes)` |
| `new QCPPolarGraph(area->angularAxis(), area->radialAxis())` | `area->add<QCPPolarGraph>()` |
| `MyPlottable : QCPPlottable` with `QCPPlottable(keyAxis, valueAxis)` | `: QCPPlottable()` (or `: QCPPlottable1D<D>()` for a data type), `customPlot->add<MyPlottable>(...)` |
| `MyItem : QCPItem` with `QCPItem(customPlot)` | `: QCPItem()`, `customPlot->add<MyItem>(...)`; `QCPItemAnchor`/`QCPItemPosition` take the parent item, not the plot |

`add<T>(args...)` on the plot or on an axis rect of the plot joins the plottable to that rect's first
bottom and left axes. `add<T>(key, value, args...)` on the same rect binds the pair given, which is how
a plottable on a second axis pair is added: both axes must belong to that rect. A polar area's
`add<T>()` binds its angular axis and its first radial axis; a graph on another radial axis is moved
there with `setSystem()` afterwards.

The pair of the axes a plottable is added with decides whether it is accepted: a system that lacks a
capability the plottable needs (a rectangle for bars, a straight key for a curve) refuses it, and the
add returns an empty handle. The plottable is deleted, one misuse is reported, and the plot, its
layers and its legend are untouched. The same applies to a plot whose axis rect has no bottom or left
axis, and to a rect add whose bottom or left axis was removed.

Every built-in plottable takes its data in the constructor now, in the ranges its `setData()` takes:

```cpp
customPlot->add<QCPGraph>(keys, values);
customPlot->add<QCPFinancial>(keys, open, high, low, close);
customPlot->add<QCPColorMap>(&mapData, true /* copy */);
rect->add<QCPIntervals>(columns, &lanes);
```

A user plottable that inherits from `QCPPlottable1D<D>` takes the key/value constructor with
`using QCPPlottable1D::QCPPlottable1D;`, next to its own constructors; the data is sorted by key. The
extra arguments an `add<T>()` used to forward to `setData()` are constructor arguments now: give the
subclass a constructor that takes them, or call `setData()` on the returned handle.

A plottable that is built and not placed answers its setters, `setData()` and its data accessors, and
draws nothing; it has no parent plot, so `setSystem()` refuses a system until the plottable is added.

A data constructor keeps the source and does not transform it. A plottable outside a plot has no worker
pool, so its pipeline (the L1 resample of a dense graph, the binning of a colour map or a histogram)
waits, and starts on the plot's pool when `add<T>()` joins the plottable: building a graph of a million
points costs a copy, never a resample on the calling thread. A taken plottable resumes the same way when
it is added back.

An intervals plottable added without a lane layout gets one, as before: `rect->add<QCPIntervals>()` and
`rect->add<QCPIntervals>(columns)` draw their rows in the lanes of a layout the plot owns, and a layout
given to the constructor is the one it keeps. The default layout is the plot's, so a plottable taken and
added to another plot gets that plot's layout (the old one stays behind with its plot).

A plottable built outside a plot keeps one pipeline job, the newest: calling `setData` several times before
the add, or resizing a viewport it does not own yet, runs one transform when it joins, not one per call.
