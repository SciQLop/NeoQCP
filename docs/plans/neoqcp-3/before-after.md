# NeoQCP 3.0 — use cases, today and proposed

Draft for discussion, kept as the record of the conversation; the design document
(`docs/specs/2026-11-02-neoqcp-3-object-model-design.md`) is the reference. Since written: factories are `plot.add<T>(...)` (read `addGraph`, `addSpan`, `addText`
below as `add<QCPGraph>`, `add<QCPSpan>`, `add<QCPText>`), and a call with a dot on a handle (`bx.setData`,
`map.remove()`) is one of the handle's own common operations; type-specific methods go through `h->`. "Today" is real code, quoted from NeoQCP's docs and examples and from SciQLopPlots
(sources in `neoqcp_use_cases.md`). "3.0" is a proposal: names and details are open, the shape is what
to judge. It follows what was agreed so far:

- the plot owns everything it draws; nothing a user writes manages lifetimes;
- objects come from factories taking one options struct (C++20 designated initializers), not `new`;
- a factory returns a **handle**: a cheap value, copyable, that never dangles (`if (h)` says whether the
  object is still there; a call on a dead handle is a no-op, and asserts in a debug build);
- `remove()` means gone, everywhere; `delete` on the underlying object is always safe;
- the plot never emits to user slots while it is being destroyed.

---

## 1. Several graphs with data and a style  *(73 `addGraph` in the docs)*

**Today** (`docs/snippets/layout.cpp:37-42`)
```cpp
customPlot->addGraph()->setData(x, s);
customPlot->graph(0)->setName("sin(x)");
customPlot->graph(0)->setPen(QPen(QColor(31, 119, 180), 2));
customPlot->addGraph()->setData(x, c);
customPlot->graph(1)->setName("cos(x)");
customPlot->graph(1)->setPen(QPen(QColor(255, 127, 14), 2));
```
Graphs are reached by index: removing one shifts the rest. Other plottables use `new T(keyAxis, valueAxis)`
instead, and "never delete them yourself" is a rule in prose.

**3.0**
```cpp
auto sin = plot.addGraph(x, s, {.name = "sin(x)", .pen = QPen(QColor(31, 119, 180), 2)});
auto cos = plot.addGraph(x, c, {.name = "cos(x)", .pen = QPen(QColor(255, 127, 14), 2)});
```
One way to make every plottable (`addGraph`, `addCurve`, `addColorMap`, ...). No index, no `new`, no axes
passed: the default axis rect's axes unless `.axes = {...}` says otherwise.

## 2. Big data without copying  *(every SciQLopPlots graph)*

**Today** (`docs/snippets/graphs.cpp:159-174`)
```cpp
// The graph reads these buffers at every replot: they must outlive it.
static std::vector<double> time(1000);
static std::vector<double> level(1000);
auto* graph = new QCPGraph2(customPlot->xAxis, customPlot->yAxis);
graph->viewData(time.data(), level.data(), static_cast<int>(time.size()));
...
graph->dataChanged();
```
The buffers are `static` only to survive; a worker thread reads them; the rule is in prose. SciQLopPlots
picks one of three source classes by memory layout and dtype, and keeps the buffer alive with one of two
incompatible guard styles (an extra argument, or an aliasing `shared_ptr`). Forgetting it is a
use-after-free on a worker thread.

**3.0**
```cpp
auto level = plot.addGraph(std::move(time), std::move(values));         // owns: nothing to keep alive
auto bx = plot.addGraph(QCP::view(keys, columns, /*keepAlive=*/buffer)); // borrows: the keeper is required
bx.setData(QCP::view(keys2, columns2, buffer2));
```
A view can't be built without saying what keeps the memory alive (a `std::shared_ptr`, a Python buffer
wrapper, ...); the layout (one column, row-major, column per span) and the value type come from the
argument types, not from a class the user picks. The data side is project 3: this only shows that it
plugs into the same handle.

## 3. Colour map with its colour scale  *(5 objects wired by hand today)*

**Today** (`docs/snippets/colormaps.cpp:24-34`)
```cpp
auto* scale = new QCPColorScale(customPlot);
customPlot->plotLayout()->addElement(0, 1, scale);   // right of the axis rect
scale->setLabel("amplitude");
map->setColorScale(scale);
map->setGradient(QCPColorGradient::gpViridis);
map->rescaleDataRange();
auto* margins = new QCPMarginGroup(customPlot);
customPlot->axisRect()->setMarginGroup(QCP::msTop | QCP::msBottom, margins);
scale->setMarginGroup(QCP::msTop | QCP::msBottom, margins);
```
Hiding it later is a row search plus `take()` + `simplify()` (`SciQLopPlot.cpp:682-736`). Deleting the
scale silently unlinks the map. The margin group's default minimum margins leak to every plot in it.

**3.0**
```cpp
auto map = plot.addColorMap(x, y, z, {.gradient = QCP::Viridis, .scale = {.label = "amplitude"}});
map.colorScale().setVisible(false);   // later
```
Each axis rect has its colour scale, placed and aligned by the rect. A map uses it unless told otherwise.
There is no layout cell or margin group to wire.

## 4. Annotations anchored to data  *(40 `new QCPItem` in the docs)*

**Today** (`docs/snippets/items.cpp:59-71`)
```cpp
auto* tracer = new QCPItemTracer(customPlot);
tracer->setGraph(graph);
tracer->setGraphKey(12.5);
tracer->setInterpolating(true);
tracer->setStyle(QCPItemTracer::tsCircle);
tracer->setPen(QPen(QColor(214, 39, 40), 2));
tracer->setBrush(Qt::white);
tracer->setSize(8);
auto* value = new QCPItemText(customPlot);
value->position->setParentAnchor(tracer->position);
value->position->setCoords(10, -6); // 10 px right, 6 px up
value->setPositionAlignment(Qt::AlignLeft | Qt::AlignBottom);
value->setText("tracer at x = 12.5");
```
A position's meaning (data, ratio, pixels) is set apart from its coordinates, and the order matters:
`setType` after `setCoords` moves the item (`SpanTargets.cpp:169-171`).

**3.0**
```cpp
auto tracer = plot.addTracer(sin, 12.5, {.style = QCP::Circle, .size = 8,
                                         .pen = QPen(QColor(214, 39, 40), 2), .brush = Qt::white});
plot.addText("tracer at x = 12.5", {.at = tracer.anchor() + QCP::Pixels{10, -6},
                                    .align = Qt::AlignLeft | Qt::AlignBottom});
```
A position is one typed value: `QCP::Data{x, y}`, `QCP::Pixels{x, y}`, `QCP::AxisRatio{x, y}`, an anchor
plus an offset. There is no order to get wrong.

## 5. A draggable span and its change signal

**Today** (`docs/snippets/items.cpp:213-234`; `SpanTargets.cpp:184-192`)
```cpp
auto* event = new QCPItemVSpan(customPlot);
event->setRange(QCPRange(20, 35));
event->setBrush(QColor(255, 127, 14, 50));
QObject::connect(event, &QCPItemVSpan::rangeChanged, ...);

// SciQLopPlots, to handle any span:
if (auto* v = qobject_cast<QCPItemVSpan*>(item))       connect(v, &QCPItemVSpan::rangeChanged, ...);
else if (auto* h = qobject_cast<QCPItemHSpan*>(item))  connect(h, &QCPItemHSpan::rangeChanged, ...);
else if (auto* r = qobject_cast<QCPItemRSpan*>(item)) { connect(r, &QCPItemRSpan::keyRangeChanged, ...);
                                                        connect(r, &QCPItemRSpan::valueRangeChanged, ...); }
```

**3.0**
```cpp
auto storm = plot.addSpan(QCP::Key, {20, 35}, {.brush = QColor(255, 127, 14, 50), .movable = true});
QObject::connect(storm, &QCPSpan::rangeChanged, ...);   // the same signal for every span kind
```
One span class: `QCP::Key`, `QCP::Value`, or both (a rectangle). One range API, one signal.

## 6. Let the user draw a span

**Today** (`docs/snippets/items.cpp:245-258`)
```cpp
customPlot->setItemCreator([](QCustomPlot* plot, QCPAxis*, QCPAxis*) -> QCPAbstractItem* {
    auto* span = new QCPItemVSpan(plot);
    span->setBrush(QColor(255, 127, 14, 50));
    return span;
});
customPlot->setItemPositioner([](QCPAbstractItem* item, double anchorKey, double,
                                 double currentKey, double) {
    static_cast<QCPItemVSpan*>(item)->setRange(QCPRange(anchorKey, currentKey));
});
```
An unchecked `static_cast`; SciQLopPlots creates the item, removes it, then re-creates its own.

**3.0**
```cpp
plot.drawToCreate<QCPSpan>({.kind = QCP::Key, .brush = QColor(255, 127, 14, 50)},
                           [](QCPSpanHandle span) { emit eventDrawn(span.range()); });
```
The plot creates the span from the options and positions it while the user drags. The callback gets a typed
handle once the span is drawn. No cast, no creator/positioner pair.

## 7. Remove something  *(7+ wrapper classes repeat the same five steps)*

**Today** (`SciQLopColorMap.cpp:60-69`, `helpers.hpp:56-67`)
```cpp
SciQLopColorMap::~SciQLopColorMap()
{
    if (_cmap)
    {
        auto* plot = _plot();
        auto* cmap = _cmap.data();
        _cmap = nullptr;
        if (plot && plot->hasPlottable(cmap))
            (void)plot->removePlottable(cmap);
    }
}
// + a destroyed() relay, a QPointer member, and a queued replot so it leaves the screen
```

**3.0**
```cpp
map.remove();      // gone; every copy of the handle now tests false; the plot repaints on its own
```
`delete` on the object behind a handle does the same: the plot notices.

## 8. Tear down a plot

**Today** (`SciQLopPlot.cpp:1032-1054`)
```cpp
SciQLopPlot::~SciQLopPlot()
{
    if (m_theme) { disconnect(m_theme, nullptr, this, nullptr); ... m_theme->detach(this); }
    if (m_lane_layout) disconnect(m_lane_layout, nullptr, this, nullptr);
    disconnect(m_impl, &QCustomPlot::afterLayout, this, &SciQLopPlot::_update_timeline_geometry);
    m_curve_scale->quiesce();
    while (plottables().size() > 0)
        delete plottable(0);
}
```
NeoQCP emits while being destroyed, so slots run on half-destroyed objects; clients disconnect first,
keep "dying" flags, and delete children in a careful order.

**3.0**
```cpp
// nothing: destroying the plot emits nothing but destroyed(), and deletes its children itself.
```

## 9. A long-lived reference to a plot object  *(62 `qptr_apply`, 85 "still alive?" checks in SciQLopPlots)*

**Today** (`SciQLopTextItem.hpp:89-120`)
```cpp
QPointer<impl::TextItem> m_item;
...
virtual ~SciQLopTextItem() { qcp_item_remove(m_item); }
[[nodiscard]] QString tool_tip() const noexcept override
{ return qptr_apply_or(m_item, [](auto&& item) { return item->tooltip(); }); }
void set_tool_tip(const QString& tooltip) noexcept override
{
    SCIQLOP_ON_OWNER_THREAD(set_tool_tip(tooltip));
    qptr_apply(m_item, [tooltip](auto&& item) { item->setToolTip(tooltip); });
}
```

**3.0**
```cpp
QCPTextHandle m_item;                         // the handle is the safe reference
QString tool_tip() const { return m_item.toolTip(); }
void set_tool_tip(const QString& t) { m_item.setToolTip(t); }
```
The handle does what `QPointer` + `qptr_apply` did. And because NeoQCP's own setters stop emitting into
user code mid-call (signals go out last), the 85 "am I still alive?" checks go away. (The GUI-thread hop
stays a SciQLopPlots concern: Python may call from any thread; NeoQCP asserts in debug when called off
its thread.)

---

## Decided since (2026-11-02)

- **Factories are `plot.add<T>(...)`**, so derived classes work. Configuration is a fluent builder, not an
  options struct (maintainer, later the same day): `plot.add<QCPGraph>(x, y).name("Bx").pen(p)`; read the
  `{.name = …}` structs in this file as builder chains. The examples above that say `addGraph` / `addSpan` read as `add<QCPGraph>`,
  `add<QCPSpan>`.
- **Handles are typed: `Handle<T>`.** `h->method()` reaches `T`'s own methods, including a subclass's,
  checked for being alive. So there is no separate "escape hatch" to design.
- **Repaint is automatic, asynchronous and deduplicated.** Any change marks the plot dirty. However many
  changes happen in one event-loop turn, they give one repaint. Clients stop calling `replot()`.

## Still open

- **Global API incoherences** (drawing API, core objects and their relations, duplicated concepts, naming):
  survey in progress (`neoqcp_api_incoherences.md`).
- **Margin groups** are too limited for SciQLopPlots: they align elements inside one widget only, and
  SciQLopPlots stacks plots as separate widgets. The survey checks the real limits.
