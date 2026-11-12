# Milestone 5b inventory — handles

State of branch `neoqcp-3.0` at `e83708c`. Facts only, `file:line`.
Read-only except this file. No build, no test.

Sources read: `docs/specs/2026-11-02-neoqcp-3-object-model-design.md` (handle rules around `:331-335`),
`docs/specs/2026-11-11-neoqcp-3-milestone-5-design.md` (C8, A5, A6, A7),
`docs/plans/neoqcp-3/milestone-1-rulings.md`, `milestone-2-rulings.md`,
`docs/plans/neoqcp-3/m5-inventory.md` (§1 rows 5–6, §5.3), the source tree, the tests, examples,
snippets, docs and `tests/benchmark`, and a read-only grep of
`/home/jeandet/Documents/prog/SciQLopPlots/{src,include}` (SciQLop and TSCat untouched).

Counts below are literal textual counts of qualified call sites, `[.>]getter(`, in
`*.cpp/*.h/*.hpp/*.cc/*.cxx/*.md/*.dox/*.txt/*.py`; comments and documentation are included, and
unqualified calls inside the defining class are not. They say how much text would move, not how many
semantic users exist.

---

## 1. The handle machinery today

### 1.1 `QCPHandle<T>`

| Fact | Where |
|---|---|
| `QCPHandle<T>` is the only generic handle; it stores one `QPointer<T>` and nothing else. | `src/handle.h:44-45,103` |
| It requires `T` derived from `QObject` (`static_assert(std::derived_from<T, QObject>)`). | `src/handle.h:91-94` |
| Default constructor makes a null handle. | `src/handle.h:48` |
| Construction from `T*` is `explicit`; `QCPHandle<T> h = ptr;` does not compile. | `src/handle.h:49` |
| There is no conversion to `T*` at all, implicit or explicit (the comment names the dangling-pointer reason). | `src/handle.h:51` |
| `explicit operator bool()` reports liveness (null `QPointer`). | `src/handle.h:52` |
| `get()` returns `T*` and checks the `QObject` requirement; `operator->` aborts with `qFatal` on a dead handle. | `src/handle.h:53-64` |
| `as<U>()` returns `QCPHandle<U>` through `qobject_cast<U*>`. | `src/handle.h:66-70` |
| `remove()` exists only when `T` satisfies `qcp::Removable`; it calls the owner-specific `qcp::detail::removeObject`. | `src/handle.h:36-40,79-84` |
| `remove()` on a dead handle is a no-op. | `src/handle.h:82-83` |
| Only `operator==` is defined (compares `get()`); C++20 rewrites `!=` from it. No `<`, no ordering. | `src/handle.h:86` |
| No `qHash`, `std::hash`, `QDebug operator<<`, `Q_DECLARE_METATYPE(QCPHandle<...>)`, or `QVariant` support exists in the tree. | grep over `src/`, `tests/`, `docs/`, `examples/`, `documentation/` |
| `qcpConnect(handle, signal, ...)` is provided; a dead handle returns an empty `QMetaObject::Connection`. | `src/handle.h:106-112` |
| Copy/move are implicit (the only member is `QPointer`, which is copyable/movable); no user-declared destructor or special member. | `src/handle.h:44-104`; `src/handle.h:103` |
| Cost is one weak reference: Qt 6 `QPointer` stores `QWeakPointer<QObjectType> wp`; no strong reference is taken. | `/usr/include/qt6/QtCore/qpointer.h:53-59`; `src/handle.h:103` |

The owner-specific removals are declared in `src/handle.h:24-30` and defined in `core.cpp`:
plottable, item, axis, layer, layout element, bars group, plottable group. A widget (`QCustomPlot`)
is deliberately not removable (`src/handle.h:36-37`).

### 1.2 Ops handles

| Fact | Where |
|---|---|
| `QCPObjectOps<T, Self>` derives from `QCPHandle<T>`; adds `with(change)` and `apply(change)`. `apply` runs the change only while the object lives and, for a `QCPLayerable`, marks it dirty after the change; it returns the most-derived handle by value. | `src/handle-ops.h:74-104` |
| `QCPLayerableOps<T, Self>` derives from `QCPObjectOps`; adds `setVisible` and `setLayer`. | `src/handle-ops.h:108-124` |
| `OpsRet<Self, Own>` makes a setter return `Self` when the ops class is used through a concrete handle, otherwise the ops class itself (`Self = void`). | `src/handle-ops.h:21-22` |
| `HandleFor<T>` is `T::Handle` when `T` declares its own `Handle` deriving from `QCPHandle<T>`, else `T::OpsFor<T>`. | `src/handle-ops.h:34-50` |
| `qcp::makeHandle<H>(T*)` default-constructs `H` and assigns its `QCPHandle<T>` base, so a user `Handle` needs no constructor. | `src/handle-ops.h:52-61` |
| `QCPLayerable` supplies the fallback `OpsFor<T> = QCPLayerableOps<T>`, so every layerable that declares neither `Handle` nor its own `OpsFor` still has a handle type. | `src/layer.h:247-249` |

Classes that declare their own ops handle (`using OpsFor = ...`), at this commit:

| Family | Classes | Ops class and base | Where |
|---|---|---|---|
| Axis | `QCPAxis` | `QCPAxisOps : QCPLayerableOps` | `src/axis/axis.h:161,546` |
| Plottables | `QCPAbstractPlottable` | `QCPPlottableOps : QCPLayerableOps` | `src/plottables/plottable.h:131,446` |
| Plottables | `QCPGraph`, `QCPGraph2`, `QCPCurve`, `QCPBars`, `QCPFinancial`, `QCPStatisticalBox`, `QCPColorMap`, `QCPColorMap2`, `QCPHistogram`, `QCPHistogram2D`, `QCPIntervals`, `QCPErrorBars`, `QCPMultiGraph`, `QCPWaterfallGraph` | each on `QCPPlottableOps` (or on `QCPMultiGraphOps`) | `plottable-graph.h:79,198`; `plottable-graph2.h:22,227`; `plottable-curve.h:98,162`; `plottable-bars.h:188,295`; `plottable-financial.h:98,221`; `plottable-statisticalbox.h:105,186`; `plottable-colormap.h:125,184`; `plottable-colormap2.h:30,152`; `plottable-histogram.h:25,125`; `plottable-histogram2d.h:30,189`; `plottable-intervals.h:26,289`; `plottable-errorbar.h:83,206`; `plottable-multigraph.h:43,415`; `plottable-waterfall.h:61,118` |
| Items | `QCPAbstractItem` | `QCPItemOps : QCPLayerableOps` | `src/items/item.h:250,417` |
| Items | `QCPItemLine`, `QCPItemCurve`, `QCPItemRect`, `QCPItemEllipse`, `QCPItemText`, `QCPItemRichText`, `QCPItemPixmap`, `QCPItemTracer`, `QCPItemBracket`, `QCPItemSpan`, `QCPItemRSpan`, `QCPItemStraightLine` | each on `QCPItemOps` (text/rich text on `QCPItemTextOps`) | `item-line.h:50,90`; `item-curve.h:51,93`; `item-rect.h:50,110`; `item-ellipse.h:50,116`; `item-text.h:61,169`; `item-richtext.h:19,51`; `item-pixmap.h:53,127`; `item-tracer.h:58,177`; `item-bracket.h:50,119`; `item-span.h:33,82`; `item-rspan.h:25,72`; `item-straightline.h:48,80` |
| Layout | `QCPAxisRect` | `QCPAxisRectOps : QCPLayerableOps` | `src/layoutelements/layoutelement-axisrect.h:58,242` |
| Layout | `QCPColorScale` | `QCPColorScaleOps : QCPLayerableOps` | `src/layoutelements/layoutelement-colorscale.h:88,175` |
| Layout | `QCPLegend` | `QCPLegendOps : QCPLayerableOps` | `src/layoutelements/layoutelement-legend.h:171,305` |
| Layout | `QCPTextElement` | `QCPTextElementOps : QCPLayerableOps` | `src/layoutelements/layoutelement-textelement.h:55,136` |
| Groups | `QCPPlottableGroup` | `QCPPlottableGroupOps : QCPObjectOps` | `src/plottables/plottable-group.h:31,96` |
| Groups | `QCPBarsGroup` | `QCPBarsGroupOps : QCPObjectOps` | `src/plottables/plottable-bars.h:52,342` |
| Polar | `QCPPolarArea` | `QCPPolarAreaOps : QCPLayerableOps` | `src/polar/polar-area.h:58,170` |
| Polar | `QCPPolarAxisAngular` | `QCPPolarAxisAngularOps : QCPLayerableOps` | `src/polar/angularaxis.h:46,235` |
| Polar | `QCPPolarAxisRadial` | `QCPPolarAxisRadialOps : QCPLayerableOps` | `src/polar/radialaxis.h:79,257` |
| Polar | `QCPPolarGrid` | `QCPPolarGridOps : QCPLayerableOps` | `src/polar/polargrid.h:65,136` |
| Polar | `QCPPolarGraph` | `QCPPolarGraphOps : QCPPlottableOps` | `src/polar/polargraph.h:84,154` |

Other layerables (`QCPArea`, `QCPLayout`, `QCPLayoutGrid`, `QCPLayoutInset`, `QCPGrid`,
`QCPOverlay`, `QCPSelectionRect`, `QCPItemCreationState`) inherit the `QCPLayerable::OpsFor`
fallback. `QCPLayer`, `QCPTheme`, `QCPCoordinateSystem`, `QCPPipelineScheduler`, `QCPLaneLayout`,
`QCPMarginGroup`, `QCPAlignmentGroup`, `QCPSelectionDecorator`, `QCPItemPosition` and
`QCPItemAnchor` declare no ops handle; the last two are not `QObject`s (see §3).

`QCPAxisRect::colorScale()` already returns a handle, `QCPColorScaleOps<QCPColorScale>`
(`src/layoutelements/layoutelement-axisrect.h:131-133`). `QCPAxisRect::add<QCPAxis>` is the handle
form of `addAxis` (`:97-104`).

### 1.3 How `add<T>()` returns handles

| Fact | Where |
|---|---|
| `QCustomPlot::add<T>(args...)` is declared to return `qcp::HandleFor<T>`. | `src/core.h:372-385` |
| `QCustomPlot::add<T>(std::unique_ptr<T>)` also returns `qcp::HandleFor<T>`. | `src/core.h:387-395` |
| The plottable specialization builds the handle with `makeHandle<Handle>` and returns it. | `src/construct.h:101-124` |
| The item specialization returns `makeHandle<Handle>(new T(...))`. | `src/construct.h:128-142` |
| The axis specialization returns `makeHandle<Handle>(axis)`. | `src/construct.h:146-160` |
| The layout-part specialization returns `makeHandle<Handle>(element.release())`. | `src/construct.h:165-181` |
| `QCPBarsGroup` and `QCPPlottableGroup` specializations return their handles. | `src/construct.h:185-194`; `src/construct.h:198-207` |
| `QCPAxisRect::add<T>(...)` returns `qcp::HandleFor<T>`. | `src/construct.h:215-245` |
| `QCPPolarArea::add<T>(...)` returns `qcp::HandleFor<T>`. | `src/construct.h:251-294` |
| `QCustomPlot::addGraph()` still returns a raw `QCPGraph*`; its documentation says it returns `nullptr` on refusal. | `src/core.h:383,412` |
| `QCPAxisRect::addAxis()` and `QCPPolarArea::addRadialAxis()` still return raw pointers. | `src/layoutelements/layoutelement-axisrect.h:117`; `src/polar/polar-area.h:86` |

The handle is returned by value; every chained setter returns a handle by value, never a reference
(`src/handle-ops.h:81-85,89-102`).

---

## 2. Public getters that return plot-owned pointers

Column "property READ" names the `Q_PROPERTY` whose READ is this getter; "—" means not a property
READ. "QObject?" is about the returned type, hence handle-ability by `QCPHandle<T>`.

### 2.1 `QCustomPlot`

| Getter (file:line) | Return | Owner | QObject? | property READ |
|---|---|---|---|---|
| `plotLayout()` `core.h:215` | `QCPLayoutGrid*` | plot (`new` in `core.cpp:594`, parent set `:596-597`) | yes | `plotLayout` `core.h:113` |
| `theme()` `core.h:272` | `QCPTheme*` | plot owns its own theme (`new QCPTheme(this)` `core.cpp:623-626`); `setTheme` stores an external pointer without reparenting (`core.cpp:634-648`) | yes | — |
| `overlay()` `core.h:306` | `QCPOverlay*` | plot (`new QCPOverlay(this)` `core.cpp:787`) | yes | — |
| `gridRhiLayer()` `core.h:308` | `QCPGridRhiLayer*` | plot (`new` `core.cpp:796`) | no (plain class `painting/grid-rhi-layer.h:15`) | — |
| `gridRhiLayerIfExists()` `core.h:311` | `QCPGridRhiLayer*` | plot | no | — |
| `pipelineScheduler()` `core.h:349` | `QCPPipelineScheduler*` | plot (`new ... this` `core.cpp:546-548`) | yes (`datasource/pipeline-scheduler.h:10`) | — |
| `plottable(int)` `core.h:354`; `plottable()` `:355` | `QCPAbstractPlottable*` | plot (`mPlottables` `core.h:533`; deleted `core.cpp:575-576`) | yes | — |
| `plottableGroups()` `core.h:363` | `QList<QCPPlottableGroup*>` | plot (`mPlottableGroups` `core.h:534`; deleted `core.cpp:570-573`) | yes | — |
| `selectedPlottables()` `core.h:364` | `QList<QCPAbstractPlottable*>` | plot | yes | — |
| `plottableAt(...)` template `core.h:366` and non-template `:368` | `PlottableType*` / `QCPAbstractPlottable*` | plot | yes | — |
| `graph(int)` `core.h:410`; `graph()` `:411` | `QCPGraph*` | plot (`mGraphs` `core.h:535-536`) | yes | — |
| `addGraph(...)` `core.h:412` | `QCPGraph*` (creator) | plot | yes | — |
| `selectedGraphs()` `core.h:417` | `QList<QCPGraph*>` | plot | yes | — |
| `item(int)` `core.h:420`; `item()` `:421` | `QCPAbstractItem*` | plot (`mItems` `core.h:537`; deleted `core.cpp:577`) | yes | — |
| `selectedItems()` `core.h:426` | `QList<QCPAbstractItem*>` | plot | yes | — |
| `itemAt(...)` template `core.h:428`, non-template `:429` | `ItemType*` / `QCPAbstractItem*` | plot | yes | — |
| `layer(const QString&)` `core.h:433`; `layer(int)` `:434`; `currentLayer()` `:435` | `QCPLayer*` | plot (`mLayers` `core.h:538`; `qDeleteAll` `core.cpp:582-583`) | yes | — |
| `axisRect(int)` `core.h:446` | `QCPAxisRect*` | parent layout (QObject parent set in `layout.cpp:927-936`) | yes | — |
| `axisRects()` `core.h:447` | `QList<QCPAxisRect*>` | parent layout | yes | — |
| `areas()` `core.h:449` | `QList<QCPArea*>` | parent layout | yes | — |
| `layoutElementAt(...)` `core.h:450` | `QCPLayoutElement*` | parent layout | yes | — |
| `areaAt(...)` `core.h:452` | `QCPArea*` | parent layout | yes | — |
| `axisRectAt(...)` `core.h:453` | `QCPAxisRect*` | parent layout | yes | — |
| `selectedAxes()` `core.h:456` | `QList<QCPAxis*>` | owning `QCPAxisRect` (`mAxes` `layoutelement-axisrect.h:185`) | yes | — |
| `selectedLegends()` `core.h:457` | `QList<QCPLegend*>` | owning layout (legend is a layout element) | yes | — |
| `xAxis`, `yAxis`, `xAxis2`, `yAxis2` members `core.h:480` | `QCPAxis*` | owning axis rect | yes | — (public member, not a property) |
| `legend` member `core.h:481` | `QCPLegend*` | default axis rect's inset layout (`core.cpp:520-522`) | yes | — (public member) |
| `selectionRect()` `core.h:235` | `QCPSelectionRect*` | plot (`new QCPSelectionRect(this)` `core.cpp:537`) | yes (`selectionrect.h:35`) | — |
| `plottableRhiLayer(...)` `core.h:203`; `scatterRhiLayer(...)` `core.h:204` | `QCPPlottableRhiLayer*` / `QCPScatterRhiLayer*` | plot (maps `core.h:605-606`; `qDeleteAll` `core.cpp:566-567`) | no (plain classes `painting/plottable-rhi-layer.h:13`, `painting/scatter-rhi-layer.h:14`) | — |

### 2.2 `QCPAxisRect` / `QCPArea`

| Getter (file:line) | Return | Owner | QObject? | property READ |
|---|---|---|---|---|
| `QCPAxisRect::rangeDragAxis(...)` `layoutelement-axisrect.h:68`; `rangeZoomAxis(...)` `:69` | `QCPAxis*` | this rect (`mAxes` `:185`) | yes | — |
| `rangeDragAxes(...)` `:70`; `rangeZoomAxes(...)` `:71` | `QList<QCPAxis*>` | this rect | yes | — |
| `axis(type, index)` `:91` | `QCPAxis*` | this rect | yes | — |
| `axes(types)` `:92`; `axes()` `:93` | `QList<QCPAxis*>` | this rect | yes | — |
| `allAxes()` `:94` (override of `QCPArea::allAxes()` `area.h:82`) | `QList<QCPAxisBase*>` | this rect | yes | — |
| `addAxis(...)` `:117`; `addAxes(...)` `:118` (creators) | `QCPAxis*` / `QList<QCPAxis*>` | this rect | yes | — |
| `colorScale()` `:133` | `QCPColorScaleOps<QCPColorScale>` (already a handle) | this rect (`mColorScale` `:186`) | yes | — |
| `plottables()` `:128` | `QList<QCPAbstractPlottable*>` | plot | yes | — |
| `graphs()` `:129` | `QList<QCPGraph*>` | plot | yes | — |
| `items()` `:130` | `QList<QCPAbstractItem*>` | plot | yes | — |
| `QCPArea::insetLayout()` `area.h:67` | `QCPLayoutInset*` | this area (`new` `area.cpp:41`; `mInsetLayout` `area.h:119`) | yes | — |
| `QCPArea::system(key, value)` `area.h:91` | `QCPCoordinateSystem*` | this area (`QObject` children `area.h:120-122`; `QObject(area)` `coordinate-system.cpp:38-40`) | yes | — |
| `QCPArea::systems()` `area.h:93` | `QList<QCPCoordinateSystem*>` | this area | yes | — |
| `QCPArea::elements(bool)` `area.h:79` (override; base `layout.h:168`) | `QList<QCPLayoutElement*>` | this layout/area | yes | — |

### 2.3 `QCPAxisBase` / `QCPAxis`

| Getter (file:line) | Return | Owner | QObject? | property READ |
|---|---|---|---|---|
| `QCPAxisBase::plottables()` `axis-base.h:170` | `QList<QCPAbstractPlottable*>` | plot | yes | — |
| `QCPAxisBase::linkSource()` `:176`; `moveTarget()` `:180` | `QCPAxisBase*` | owning rect | yes | — |
| `QCPAxisBase::area()` `:183` | `QCPArea*` | parent layout | yes | — |
| `QCPAxis::axisRect()` `axis.h:203` | `QCPAxisRect*` | owning rect; axis is a child of it (`axis.cpp:422,468`) | yes | `axisRect` `axis.h:121` |
| `QCPAxis::area()` `:205` | `QCPArea*` | parent layout | yes | — |
| `QCPAxis::grid()` `:268` | `QCPGrid*` | this axis (`new QCPGrid(this)` `axis.cpp:458`; deleted `:506`) | yes | `grid` `axis.h:156` |
| `QCPAxis::gridIfExists()` `:272` | `QCPGrid*` | this axis | yes | — |
| `QCPAxis::labelRenderer()` `:243` | `QCPLabelRenderer*` | **not owned**: `setLabelRenderer` says not owned (`axis.h:300-301`), default is shared (`axis.cpp:467`) | no (`axis/labelrenderer.h:33`) | — |
| `QCPAxis::linkSource()` `:329`; `moveTarget()` `:332` | `QCPAxis*` | owning rect | yes | — |
| `QCPAxis::plottables()` `:335` | `QList<QCPAbstractPlottable*>` | plot | yes | — |
| `QCPAxis::graphs()` `:336` | `QList<QCPGraph*>` | plot | yes | — |
| `QCPAxis::items()` `:337` | `QList<QCPAbstractItem*>` | plot | yes | — |

### 2.4 `QCPAbstractPlottable` and plottable subclasses

| Getter (file:line) | Return | Owner | QObject? | property READ |
|---|---|---|---|---|
| `system()` `plottable.h:157` | `QCPCoordinateSystem*` | owning area | yes | — |
| `area()` `:160` | `QCPArea*` | parent layout | yes | — |
| `keyAxis()` `:162`; `valueAxis()` `:164` | `QCPAxis*` | owning rect | yes | `keyAxis`/`valueAxis` `plottable.h:119-120` |
| `keyAxisBase()` `:168`; `valueAxisBase()` `:171` | `QCPAxisBase*` | owning rect | yes | — |
| `selectionDecorator()` `:185` | `QCPSelectionDecorator*` | this plottable (`delete` `plottable.cpp:586-589`; ownership documented `plottable.cpp:61-63`) | **no** (`Q_GADGET` `plottable.h:55-57`) | `selectionDecorator` `plottable.h:124` |
| `interface1D()` `:222` | `QCPPlottableInterface1D*` | alias of this plottable (`return this` in `plottable1d.h:94`, etc.) | no interface type (`plottable1d.h:39`) | — |
| `group()` `:278` | `QCPPlottableGroup*` | plot (`QObject(parentPlot)` `plottable-group.cpp:43-44`) | yes | — |
| `dependentLayers()` `:248` (and `plottable-intervals.h:51`) | `QList<QCPLayer*>` | plot | yes | — |
| `QCPGraph::channelFillGraph()` `plottable-graph.h:93` | `QCPGraph*` | plot | yes | `channelFillGraph` `plottable-graph.h:48` |
| `QCPErrorBars::dataPlottable()` `plottable-errorbar.h:113` | `QCPAbstractPlottable*` | plot | yes | `dataPlottable` `plottable-errorbar.h:75` |
| `QCPBars::barsGroup()` `plottable-bars.h:214` | `QCPBarsGroup*` | plot (`QObject(parentPlot)` `plottable-bars.cpp:109-110`) | yes | `barsGroup` `plottable-bars.h:179` |
| `QCPBars::barBelow()` `:220`; `barAbove()` `:222` | `QCPBars*` | plot | yes | `barBelow`/`barAbove` `plottable-bars.h:182-183` |
| `QCPBarsGroup::bars(int)` `:83`; `bars()` `:85` | `QCPBars*` / `QList<QCPBars*>` | plot | yes | — |
| `QCPAbstractColorMapped::colorScale()` `plottable-colormapped.h:32` | `QCPColorScale*` | owning axis rect (`mColorScale` `layoutelement-axisrect.h:186`) | yes | `colorScale` `plottable-colormapped.h:16` |
| `QCPColorMap::data()` `plottable-colormap.h:131` | `QCPColorMapData*` | this plottable (`mMapData`) | no (`plottable-colormap.h:41`) | — |
| `QCPIntervals::laneLayout()` `plottable-intervals.h:36` | `QCPLaneLayout*` | caller or plot when `add<T>` made it (`new QCPLaneLayout(&plot)` `construct.h:48`; `QObject` parent `lane-layout.h:27`) | yes (`lane-layout.h:19`) | — |
| `QCPItemTracer::graph()` `item-tracer.h:96` | `QCPGraph*` | plot | yes | `graph` `item-tracer.h:51` |
| `QCPItemTracer::lineGraph()` `:99` | `QCPMultiGraph*` | plot | yes | — |
| `QCPPlottableGroup::parentPlot()` `plottable-group.h:35` | `QCustomPlot*` | the widget, not a plot-owned child | yes | — |
| `QCPPlottableGroup::members()` `:39` | `QList<QCPAbstractPlottable*>` | plot (the group holds them without owning them, `plottable-group.h:113`) | yes | — |
| `QCPPlottableGroupLegendItem::group()` `:77` | `QCPPlottableGroup*` | plot (`QObject(parentPlot)` `plottable-group.cpp:43-44`) | yes | — |
| `QCPDataLocator::plottable()` `data-locator.h:26`; `hitPlottable()` `:27` | `QCPAbstractPlottable*` | plot (the locator only borrows them) | yes | — |

### 2.5 Colour scale

| Getter (file:line) | Return | Owner | QObject? | property READ |
|---|---|---|---|---|
| `QCPColorScale::axis()` `layoutelement-colorscale.h:94` | `QCPAxis*` | the scale's private axis rect | yes | — |
| `QCPColorScale::colorMaps()` `:122` | `QList<QCPColorMap*>` | plot | yes | — |
| `QCPColorScale::users()` `:125` | `QList<QCPAbstractColorMapped*>` | plot | yes | — |

### 2.6 `QCPAbstractItem`, positions and anchors

| Getter (file:line) | Return | Owner | QObject? | property READ |
|---|---|---|---|---|
| `clipAxisRect()` `item.h:258` | `QCPAxisRect*` | parent layout | yes | `clipAxisRect` `item.h:242` |
| `clipArea()` `item.h:260` | `QCPArea*` | parent layout | yes | — |
| `firstPosition()` `:307` | `const QCPItemPosition*` | this item | **no** (`Q_GADGET` `item.h:95-97`) | — |
| `positions()` `:332`; `position(name)` `:336` | `QList<QCPItemPosition*>` / `QCPItemPosition*` | this item (`new` `item.cpp:1971`; `qDeleteAll` `:1489-1490`) | **no** | — |
| `anchors()` `:334`; `anchor(name)` `:337` | `QList<QCPItemAnchor*>` / `QCPItemAnchor*` | this item (`mAnchors`, `qDeleteAll` `item.cpp:1489-1490`) | **no** (`Q_GADGET` `item.h:47-49`) | — |
| `QCPItemPosition::parentAnchor()` `:146`; `parentAnchorX()` `:148`; `parentAnchorY()` `:150` | `QCPItemAnchor*` | this item | **no** | — |
| `QCPItemPosition::keyAxis()` `:158`; `valueAxis()` `:160` | `QCPAxis*` | owning rect | yes | — |
| `QCPItemPosition::axisRect()` `:162` | `QCPAxisRect*` | parent layout | yes | — |
| `QCPItemPosition::system()` `:166` | `QCPCoordinateSystem*` | owning area | yes | — |
| `QCPItemPosition::area()` `:169` | `QCPArea*` | parent layout | yes | — |
| `QCPItemPosition::toQCPItemPosition()` `item.h:212`; `QCPItemAnchor::toQCPItemPosition()` `:75` | `QCPItemPosition*` | alias (`this` or null) | **no** | — |
| Item subclass position/anchor members, e.g. `QCPItemLine::start/end` `item-line.h:76-77`; `QCPItemCurve::start/startDir/end/endDir` `item-curve.h:77-80`; `QCPItemRect::topLeft/bottomRight/top/right/bottom/left/bottomLeft/topRight` `item-rect.h:76-83`; `QCPItemEllipse` `item-ellipse.h:76-86`; `QCPItemText` `item-text.h:111-119`; `QCPItemPixmap` `item-pixmap.h:85-92`; `QCPItemBracket` `item-bracket.h:94-96`; `QCPItemSpan` `item-span.h:45-47`; `QCPItemRSpan` `item-rspan.h:37-46`; `QCPItemStraightLine` `item-straightline.h:65-66`; `QCPItemTracer::position` `item-tracer.h:140`; `QCPItemText::position` `item-text.h:111` | `QCPItemPosition* const` / `QCPItemAnchor* const` | this item | **no** | — |

### 2.7 Legend

| Getter (file:line) | Return | Owner | QObject? | property READ |
|---|---|---|---|---|
| `QCPAbstractLegendItem::parentLegend()` `layoutelement-legend.h:57` | `QCPLegend*` | the legend (items are its layout children) | yes | `parentLegend` `:43` |
| `QCPLegend::item(int)` `:251` | `QCPAbstractLegendItem*` | this legend (`QCPLayoutGrid` elements; `adoptElement` sets parent `layout.cpp:927-936`) | yes | — |
| `QCPLegend::itemWithPlottable(...)` `:252` | `QCPPlottableLegendItem*` | this legend | yes | — |
| `QCPLegend::selectedItems()` `:260` | `QList<QCPAbstractLegendItem*>` | this legend | yes | — |
| `QCPPlottableLegendItem::plottable()` `:128` | `QCPAbstractPlottable*` | plot | yes | — |
| `QCPGroupLegendItem::multiGraph()` `layoutelement-legend-group.h:13` | `QCPMultiGraph*` | plot | yes | — |
| `QCPIntervalsLegendItem::intervals()` `layoutelement-legend-intervals.h:14` | `QCPIntervals*` | plot | yes | — |

### 2.8 Layers and layerables

| Getter (file:line) | Return | Owner | QObject? | property READ |
|---|---|---|---|---|
| `QCPLayer::parentPlot()` `layer.h:86` | `QCustomPlot*` | the widget, not a plot-owned child | yes | `parentPlot` `layer.h:56` |
| `QCPLayer::children()` `:92` | `QList<QCPLayerable*>` | the plot/layout, **not** the layer; `~QCPLayer` only detaches children (`layer.cpp:154-157`) | yes | `children` `layer.h:59` |
| `QCPLayerable::parentPlot()` `:258` | `QCustomPlot*` | the widget | yes | `parentPlot` `layer.h:239` |
| `QCPLayerable::parentLayerable()` `:260` | `QCPLayerable*` | its actual QObject parent (plot/layout), possibly null | yes | `parentLayerable` `layer.h:240` |
| `QCPLayerable::layer()` `:262` | `QCPLayer*` | plot | yes | `layer` `layer.h:241` |

### 2.9 Layouts

| Getter (file:line) | Return | Owner | QObject? | property READ |
|---|---|---|---|---|
| `QCPLayoutElement::layout()` `layout.h:122` | `QCPLayout*` | the parent layout; an element is a QObject child of its layout (`layout.cpp:927-936`) | yes | `layout` `layout.h:74` |
| `QCPLayoutElement::marginGroup(side)` `:140` | `QCPMarginGroup*` | plot (`QObject(parentPlot)` `layout.cpp:84-85`) | yes | — |
| `QCPLayoutElement::marginGroups()` `:145` | `QHash<QCP::MarginSide, QCPMarginGroup*>` | plot | yes | — |
| `QCPLayoutElement::alignmentGroup()` `:147` | `QPointer<QCPAlignmentGroup>` | the members own the group; it deletes itself when the last leaves (`alignment-group.h:16-19`) | yes | — |
| `QCPLayoutElement::elements(bool)` `:168` | `QList<QCPLayoutElement*>` | this layout/element | yes | — |
| `QCPLayout::elements(bool)` `:217` | `QList<QCPLayoutElement*>` | this layout | yes | — |
| `QCPLayout::elementAt(int)` `:221` | `QCPLayoutElement*` | this layout | yes | — |
| `QCPLayoutGrid::elementAt(int)` `:322`; `QCPLayoutInset::elementAt(int)` `:392` | `QCPLayoutElement*` | this layout | yes | — |
| `QCPLayoutGrid::element(row, column)` `:331` | `QCPLayoutElement*` | this layout | yes | — |
| `QCPLayout::takeAt(int)` `:222`; `QCPLayoutGrid::takeAt` `:323`; `QCPLayoutInset::takeAt` `:393` | `QCPLayoutElement*` | this layout (ownership leaves on take) | yes | — |
| `QCPAlignmentGroup::elements()` `alignment-group.h:39` | `QList<QCPLayoutElement*>` | the group's members (not owned) | yes | — |
| `QCPMarginGroup::elements(side)` `layout.h:47` | `QList<QCPLayoutElement*>` | members (not owned) | yes | — |

### 2.10 `QCPCoordinateSystem` and polar

| Getter (file:line) | Return | Owner | QObject? | property READ |
|---|---|---|---|---|
| `QCPCoordinateSystem::area()` `coordinate-system.h:62` | `QCPArea*` | parent layout | yes | — |
| `keyAxis()` `:65`; `valueAxis()` `:68` | `QCPAxisBase*` | owning rect/area | yes | — |
| `plottables()` `:108` | `QList<QCPAbstractPlottable*>` | plot | yes | — |
| `QCPPolarArea::angularAxis()` `polar-area.h:64` | `QCPPolarAxisAngular*` | this area (`new` `polar-area.cpp:62`; `delete` `:95`) | yes | — |
| `QCPPolarArea::radialAxis(int)` `:68`; `radialAxes()` `:70` | `QCPPolarAxisRadial*` / `QList<QCPPolarAxisRadial*>` | this area (`mRadialAxes`; deleted `polar-area.cpp:89-91`) | yes | — |
| `QCPPolarArea::grid()` `:72` | `QCPPolarGrid*` | this area (`new` `polar-area.cpp:60`; `delete` `:85`) | yes | — |
| `QCPPolarArea::graphs()` `:83` | `QList<QCPPolarGraph*>` | plot (scan of `mParentPlot->plottable(i)` `polar-area.cpp:225`) | yes | — |
| `QCPPolarArea::allAxes()` `:119` | `QList<QCPAxisBase*>` | this area | yes | — |
| `QCPPolarGraph::angularAxis()` `polargraph.h:94`; `radialAxis()` `:96` | `QCPPolarAxisAngular*` / `QCPPolarAxisRadial*` | the polar area | yes | — |
| `QCPPolarGrid::radialAxis()` `polargrid.h:72` | `QCPPolarAxisRadial*` | the polar area | yes | — |
| `QCPPolarAxisAngular::area()` `angularaxis.h:54`; `polarArea()` `:172` | `QCPPolarArea*` | parent layout | yes | — |
| `QCPPolarAxisRadial::area()` `radialaxis.h:86`; `polarArea()` `:194`; `angularAxis()` `:89` | `QCPPolarArea*` / `QCPPolarAxisAngular*` | the polar area | yes | — |

---

## 3. The exceptions: returns that are not `QObject`s, and parent getters

### 3.1 Non-`QObject` returned types (cannot be `QCPHandle<T>`)

| Type (file:line) | Getter(s) | Why a handle cannot be formed |
|---|---|---|
| `QCPSelectionDecorator` `plottable.h:55-57` | `QCPAbstractPlottable::selectionDecorator()` `plottable.h:185` | `Q_GADGET`, not `QObject`; `QCPHandle` rejects it at compile time (`handle.h:91-94`). It does have `Q_DECLARE_METATYPE(QCPSelectionDecorator*)` `plottable.h:108`. |
| `QCPItemPosition` `item.h:95-97`; `QCPItemAnchor` `item.h:47-49` | `positions()`, `position()`, `anchors()`, `anchor()`, `parentAnchor*()`, every item's position/anchor member | `Q_GADGET`, not `QObject`; same static_assert. The item owns them and deletes them with itself (`item.cpp:1489-1490`). |
| `QCPPlottableInterface1D` `plottable1d.h:39` | `interface1D()` `plottable.h:222` and overrides | Plain interface, not `QObject`; the returned pointer is normally the plottable itself (`plottable1d.h:94`, `plottable-graph2.h:142`, `plottable-multigraph.h:214`, `plottable-intervals.h:124`, `plottable-errorbar.h:153`). |
| `QCPColorMapData` `plottable-colormap.h:41` | `QCPColorMap::data()` `plottable-colormap.h:131`; `QCPColorMapData::rawData()` `:61,63` | Plain data class, not `QObject`; owned by the map. |
| `QCPLabelRenderer` `labelrenderer.h:33` | `QCPAxis::labelRenderer()` `axis.h:243` | Plain class, not `QObject`; explicitly **not owned** by the axis (`axis.h:300-301`), default is shared (`axis.cpp:467`). |
| `QCPAbstractPaintBuffer` `paintbuffer.h:32` | no public getter (`createPaintBuffer` is protected, `core.h:707`) | Plain class, not `QObject`; plot keeps `QList<QSharedPointer<QCPAbstractPaintBuffer>>` `core.h:557`. |
| `QCPGridRhiLayer` `grid-rhi-layer.h:15`; `QCPPlottableRhiLayer` `plottable-rhi-layer.h:13`; `QCPScatterRhiLayer` `scatter-rhi-layer.h:14` | `gridRhiLayer()`, `gridRhiLayerIfExists()`, `plottableRhiLayer()`, `scatterRhiLayer()` | Plain classes, not `QObject`; plot owns them (`core.h:605-607`; `qDeleteAll` `core.cpp:566-567`; `core.cpp:796`). |

`QCPCoordinateSystem` is a `QObject` (`coordinate-system.h:54`), so a `QCPHandle<QCPCoordinateSystem>`
can be constructed by hand, but it declares no `OpsFor`, so `qcp::HandleFor<QCPCoordinateSystem>` does
not exist and `add<T>` cannot return one (`handle-ops.h:34-50`).

### 3.2 Parent getters

| Getter | Return | Ownership / removal fact | Does a handle fit? |
|---|---|---|---|
| `QCPLayerable::parentPlot()` `layer.h:258`; `QCPLayer::parentPlot()` `:86` | `QCustomPlot*` | The widget is not owned by any plot object and is not in `qcp::Removable` (`handle.h:24-30,36-40`). `QCPHandle<QCustomPlot>` compiles (it is a `QObject`) but has no `remove()`. | A weak handle is possible; it cannot express deletion. |
| `QCPLayerable::parentLayerable()` `layer.h:260` | `QCPLayerable*` or null | The parent is the actual QObject parent, normally the plot or a layout; `QCPLayerable` has no `removeObject` overload. | A handle is possible, with no `remove()`. |
| `QCPLayoutElement::layout()` `layout.h:122` | `QCPLayout*` or null | The layout owns the element (`adoptElement` sets the QObject parent, `layout.cpp:927-936`); `QCPLayout` derives from `QCPLayoutElement`, so `QCPHandle<QCPLayout>::remove()` is enabled through `removeObject(QCPLayoutElement&)` (`handle.h:28`). Calling it from a child would remove/delete its own parent (and itself). | A handle is possible but `remove()` points upward. |
| `QCPAbstractLegendItem::parentLegend()` `layoutelement-legend.h:57` | `QCPLegend*` | The legend owns the item; `QCPLegend` is a `QCPLayoutElement` (`:147`), so `QCPHandle<QCPLegend>::remove()` is enabled and would delete the legend (and the item). | Same upward-removal fact. |
| `QCPAbstractPlottable::group()` `plottable.h:278`; `QCPBars::barsGroup()` `plottable-bars.h:214` | group pointers | Groups are plot-owned; both group types have `removeObject` overloads (`handle.h:29-30`). | A handle can remove the group, not detach the member. |
| `QCPItemPosition::parentAnchor()` `item.h:146`; `parentAnchorX/Y()` `:148-150` | `QCPItemAnchor*` | Same item, `Q_GADGET`, not `QObject`. | No `QCPHandle` at all. |
| `QCPItemPosition::area()/axisRect()/system()` `item.h:162-169` | area/rect/system | These are the containers the position is mapped through, not its QObject parent. | Handles possible for the `QObject` types. |

---

## 4. Object-typed `Q_PROPERTY`s

Every `Q_PROPERTY` whose type is a pointer to a plot object (grep of `src/**/*.h`):

| Class | Property (declaration) | Type | READ | WRITE | NOTIFY |
|---|---|---|---|---|---|
| `QCustomPlot` | `plotLayout` `core.h:113` | `QCPLayoutGrid*` | `plotLayout()` `core.h:215` | — | — |
| `QCPAxis` | `axisRect` `axis.h:121` | `QCPAxisRect*` | `axisRect()` `axis.h:203` | — | — |
| `QCPAxis` | `grid` `axis.h:156` | `QCPGrid*` | `grid()` `axis.h:268` | — | — |
| `QCPLayoutElement` | `layout` `layout.h:74` | `QCPLayout*` | `layout()` `layout.h:122` | — | — |
| `QCPLayer` | `parentPlot` `layer.h:56` | `QCustomPlot*` | `parentPlot()` `layer.h:86` | — | — |
| `QCPLayer` | `children` `layer.h:59` | `QList<QCPLayerable*>` | `children()` `layer.h:92` | — | — |
| `QCPLayerable` | `parentPlot` `layer.h:239` | `QCustomPlot*` | `parentPlot()` `layer.h:258` | — | — |
| `QCPLayerable` | `parentLayerable` `layer.h:240` | `QCPLayerable*` | `parentLayerable()` `layer.h:260` | — | — |
| `QCPLayerable` | `layer` `layer.h:241` | `QCPLayer*` | `layer()` `layer.h:262` | `setLayer` `layer.h:274-275` | `layerChanged` `:297` |
| `QCPAbstractLegendItem` | `parentLegend` `layoutelement-legend.h:43` | `QCPLegend*` | `parentLegend()` `:57` | — | — |
| `QCPItemTracer` | `graph` `item-tracer.h:51` | `QCPGraph*` | `graph()` `:96` | `setGraph` `:115` | — |
| `QCPAbstractItem` | `clipAxisRect` `item.h:242` | `QCPAxisRect*` | `clipAxisRect()` `:258` | `setClipAxisRect` `:272` | — |
| `QCPGraph` | `channelFillGraph` `plottable-graph.h:48` | `QCPGraph*` | `channelFillGraph()` `:93` | `setChannelFillGraph` `:104` | — |
| `QCPAbstractColorMapped` | `colorScale` `plottable-colormapped.h:16` | `QCPColorScale*` | `colorScale()` `:32` | `setColorScale` (overrides, e.g. `plottable-multigraph.h:189`) | — |
| `QCPErrorBars` | `dataPlottable` `plottable-errorbar.h:75` | `QCPAbstractPlottable*` | `dataPlottable()` `:113` | `setDataPlottable` `:125` | — |
| `QCPBars` | `barsGroup` `plottable-bars.h:179` | `QCPBarsGroup*` | `barsGroup()` `:214` | `setBarsGroup` `:232` | — |
| `QCPBars` | `barBelow` `plottable-bars.h:182` | `QCPBars*` | `barBelow()` `:220` | — | — |
| `QCPBars` | `barAbove` `plottable-bars.h:183` | `QCPBars*` | `barAbove()` `:222` | — | — |
| `QCPAbstractPlottable` | `keyAxis` `plottable.h:119` | `QCPAxis*` | `keyAxis()` `:162` | `setKeyAxis` `:193` | — |
| `QCPAbstractPlottable` | `valueAxis` `plottable.h:120` | `QCPAxis*` | `valueAxis()` `:164` | `setValueAxis` `:194` | — |
| `QCPAbstractPlottable` | `selectionDecorator` `plottable.h:124` | `QCPSelectionDecorator*` | `selectionDecorator()` `:185` | `setSelectionDecorator` `:199` | — |

Facts about handles and moc/QVariant:

- The declarations above name pointer types. `QCPHandle<T>` has no conversion to `T*`
  (`handle.h:51`), so a READ that returns a handle cannot satisfy a property declared with the raw
  pointer type; the `Q_PROPERTY` type would have to become the handle type (or the raw accessor
  would have to stay separate).
- The tree declares no `Q_DECLARE_METATYPE(QCPHandle<...>)` and no `Q_GADGET` for
  `QCPHandle<T>` (grep over `src/`, `tests/`, `docs/`, `examples/`, `documentation/`). The only
  custom-type declaration near this surface is `Q_DECLARE_METATYPE(QCPSelectionDecorator*)`
  `plottable.h:108`.
- Qt requires a `Q_PROPERTY` type to be known to `QMetaType` for moc/QVariant access; a non-pointer
  custom type additionally needs `Q_DECLARE_METATYPE` (and `Q_GADGET` when it needs meta-object
  features/QML exposure). `QCPHandle<T>` is a class template, so each specialization used by a
  property would need its own metatype declaration.
- The WRITE accessors today take raw pointers (`setKeyAxis(QCPAxis*)` `plottable.h:193`,
  `setValueAxis(QCPAxis*)` `:194`, `setSelectionDecorator(QCPSelectionDecorator*)` `:199`,
  `setClipAxisRect(QCPAxisRect*)` `item.h:272`, `setDataPlottable(QCPAbstractPlottable*)`
  `plottable-errorbar.h:125`, `setChannelFillGraph(QCPGraph*)` `plottable-graph.h:104`); a
  handle-returning READ does not change those signatures.
- `QCPLayer::children` `layer.h:59` is the one object-list property; its type is
  `QList<QCPLayerable*>`, not a list of handles.

---

## 5. Signals that carry raw object pointers

They stay raw per C8 (`docs/specs/2026-11-11-...-5-design.md:65-67`).

| Signal (file:line) | Pointer argument(s) |
|---|---|
| `QCustomPlot::plottableClick` `core.h:490`; `plottableDoubleClick` `:491` | `QCPAbstractPlottable*` |
| `QCustomPlot::itemClick` `:492`; `itemDoubleClick` `:493` | `QCPAbstractItem*` |
| `QCustomPlot::axisClick` `:494`; `axisDoubleClick` `:495` | `QCPAxis*` |
| `QCustomPlot::legendClick` `:496`; `legendDoubleClick` `:497` | `QCPLegend*`, `QCPAbstractLegendItem*` |
| `QCustomPlot::itemCreated` `:504` | `QCPAbstractItem*` |
| `QCPLayerable::layerChanged` `layer.h:297` | `QCPLayer*` |
| `QCPItemCreationState::itemCreated` `item-creation-state.h:26` | `QCPAbstractItem*` |

Signals with no pointer argument in this area: `selectionChangedByUser()` `core.h:499`,
`QCPAbstractPlottable::selectionChanged()` `plottable.h:313`, `selectableChanged()` `:314`,
`QCPPlottableGroup::membersChanged()`/`doubleClicked()` `plottable-group.h:48-50`,
`QCPCoordinateSystem::changed()` `coordinate-system.h:112`.

`systemChanged()` is a protected virtual method with no arguments, not a signal
(`plottable.h:320`); `systemWillChange(QCPCoordinateSystem*)` is also a protected virtual and does
carry a raw pointer (`plottable.h:324`). It is not a signal.

---

## 6. Users of the getters

### 6.1 Counts per directory

Textual qualified call sites (`[.>]getter(`); `tests` excludes `tests/benchmark`:
`src` = 241 `.cpp`/`.h` files; `tests` = 85 `.cpp` plus headers; `examples` = 52 files;
`docs/snippets` = 21 files; `documentation` = 94 files (text extensions scanned); `benchmark` = `tests/benchmark`.

| Getter | src | tests | examples | docs/snippets | documentation | benchmark |
|---|---:|---:|---:|---:|---:|---:|
| `xAxis` / `yAxis` / `xAxis2` / `yAxis2` (member access) | 22 | 4550 | 235 | 212 | 228 | 312 |
| `legend` (member access) | 17 | 203 | 33 | 32 | 8 | 0 |
| `graph(` | 1 | 83 | 125 | 5 | 18 | 0 |
| `addGraph(` | 0 | 158 | 4 | 23 | 17 | 33 |
| `plottable(` | 7 | 60 | 2 | 1 | 0 | 0 |
| `plottableGroups(` | 0 | 3 | 0 | 0 | 0 | 0 |
| `item(` | 6 | 38 | 0 | 1 | 0 | 3 |
| `layer(` | 29 | 203 | 2 | 6 | 3 | 0 |
| `currentLayer(` | 2 | 46 | 0 | 0 | 0 | 0 |
| `axisRect(` | 87 | 552 | 32 | 27 | 27 | 9 |
| `axisRects(` | 1 | 6 | 1 | 0 | 1 | 0 |
| `axisRectAt(` | 1 | 1 | 0 | 0 | 0 | 0 |
| `areas(` / `areaAt(` | 0 | 0 | 0 | 0 | 0 | 0 |
| `plotLayout(` | 3 | 240 | 17 | 40 | 36 | 7 |
| `selectedPlottables(` | 0 | 0 | 0 | 0 | 0 | 0 |
| `selectedGraphs(` | 0 | 0 | 3 | 1 | 0 | 0 |
| `selectedItems(` | 1 | 0 | 0 | 0 | 0 | 0 |
| `selectedAxes(` / `selectedLegends(` | 0 | 0 | 0 | 0 | 0 | 0 |
| `plottableAt(` | 0 | 3 | 0 | 0 | 0 | 0 |
| `itemAt(` / `layoutElementAt(` | 0 | 0 | 0 | 0 | 0 | 0 |
| `theme(` | 9 | 10 | 0 | 0 | 0 | 0 |
| `overlay(` | 0 | 36 | 0 | 1 | 0 | 0 |
| `axisRect()` (no-arg) | 85 | 529 | 32 | 25 | 19 | 9 |
| `grid(` | 19 | 80 | 23 | 13 | 20 | 0 |
| `graphs(` | 0 | 3 | 0 | 0 | 0 | 0 |
| `items(` | 0 | 4 | 0 | 0 | 0 | 1 |
| `plottables(` | 2 | 8 | 0 | 0 | 0 | 0 |
| `axis(` | 18 | 329 | 28 | 47 | 43 | 7 |
| `axes(` | 7 | 22 | 1 | 4 | 11 | 0 |
| `allAxes(` | 1 | 0 | 0 | 0 | 0 | 0 |
| `colorScale(` | 1 | 68 | 0 | 0 | 0 | 0 |
| `keyAxis(` | 36 | 44 | 2 | 0 | 1 | 0 |
| `valueAxis(` | 17 | 26 | 4 | 0 | 1 | 0 |
| `keyAxisBase(` | 1 | 1 | 0 | 0 | 0 | 0 |
| `valueAxisBase(` | 0 | 0 | 0 | 0 | 0 | 0 |
| `system(` | 46 | 182 | 0 | 1 | 0 | 0 |
| `area(` | 33 | 23 | 0 | 0 | 0 | 0 |
| `selectionDecorator(` | 0 | 13 | 0 | 1 | 2 | 0 |
| `group(` | 1 | 6 | 2 | 0 | 0 | 4 |
| `positions(` | 8 | 28 | 0 | 0 | 0 | 0 |
| `position(` | 9 | 1 | 0 | 4 | 0 | 0 |
| `anchors(` | 1 | 1 | 0 | 0 | 1 | 0 |
| `anchor(` | 0 | 0 | 0 | 0 | 0 | 0 |
| `parentAnchor*` | 18 | 7 | 0 | 0 | 0 | 0 |
| `clipAxisRect(` | 2 | 4 | 0 | 0 | 0 | 0 |
| `clipArea(` | 3 | 11 | 0 | 0 | 0 | 0 |
| `element(` | 0 | 90 | 0 | 0 | 0 | 0 |
| `elementAt(` | 2 | 11 | 0 | 0 | 0 | 0 |
| `layout()` | 10 | 4 | 0 | 0 | 0 | 0 |
| `children(` | 3 | 13 | 0 | 0 | 0 | 0 |
| `itemWithPlottable(` | 1 | 7 | 1 | 0 | 0 | 0 |
| `bars(` | 1 | 6 | 0 | 0 | 0 | 0 |
| `barsGroup(` | 2 | 11 | 0 | 0 | 0 | 0 |
| `members(` | 2 | 16 | 0 | 0 | 0 | 0 |
| `angularAxis(` | 3 | 159 | 1 | 3 | 0 | 2 |
| `radialAxis(` | 2 | 165 | 6 | 4 | 0 | 3 |
| `radialAxes(` | 0 | 0 | 0 | 0 | 0 | 0 |
| `parentPlot(` | 60 | 19 | 6 | 0 | 3 | 0 |
| `parentLayerable(` | 0 | 2 | 0 | 0 | 0 | 0 |

The families with the largest user surface are the axis shortcuts (`xAxis`/`yAxis` members and
`axisRect()`), `plotLayout()`, `legend`, and the polar axis getters.

A zero means no qualified call site was found; unqualified calls inside the defining class (for
example `plottableAt(...)` inside `core.cpp`) are not counted by this method.

### 6.2 Patterns that break if the return type becomes a handle

| Pattern | Examples (`file:line`) |
|---|---|
| Assigning to `T*` (or `auto*`) | `tests/auto/test-busy-indicator/test-busy-indicator.cpp:276` (`QCPLayer* mainLayer = mPlot->layer("main")`); `tests/auto/test-qcpaxisrect/test-qcpaxisrect.cpp:97` (`QCPAxisRect* ar = mPlot->axisRect()`); `tests/auto/test-item-position/test-item-position.cpp:206` (`QCPAxisRect* axisRect = mPlot->axisRect()`); `auto*` at `tests/auto/test-grid-rhi/test-grid-rhi.cpp:156` (`QCPLayer* layer = mPlot->layer("main")`) |
| Comparing with `T*` / `nullptr` | `tests/auto/test-ownership/test-ownership.cpp:429` (`mPlot->item(2) == nullptr`); `:591` (`mPlot->layer("extra") == nullptr`); `:622` (`graph->layer() == nullptr`); `src/layoutelements/layoutelement-axisrect.cpp:398` (`newAxis->axisRect() != this`); `:420` (`mParentPlot->axisRect(0) == this`) |
| Passing to a function taking `T*` | `tests/auto/test-intervals/test-intervals.cpp:1280` (`removePlottable(mPlot->plottable(0))`); `tests/auto/test-layer-removal/test-layer-removal.cpp:26` (`removeLayer(mPlot->layer("extra"))`); `docs/snippets/concepts.cpp:66` (`moveLayer(customPlot->layer("grid"), ...)`) |
| `qobject_cast` on the result | `tests/auto/test-intervals/test-intervals.cpp:1515` (`qobject_cast<QCPIntervalsLegendItem*>(mPlot->legend->item(0))`); `tests/auto/test-user-system/test-user-system.cpp:575` (`qobject_cast<QCPItemLine*>(plot.item(0))`); `tests/benchmark/benchmark.cpp:1651` (`qobject_cast<QCPItemLine*>(mPlot->item(5000))`) |
| Range-for (or index loop) over returned lists | `docs/snippets/interactions.cpp:266` (`for (QCPGraph* graph : customPlot->selectedGraphs())`); `tests/auto/test-ownership/test-ownership.cpp:505` (`for (QCPAxis* axis : second->axes())`); `tests/auto/test-reference-m4e/test-reference-m4e.cpp:551`; `src/core.cpp:2573-2575`; `src/coordinate-system.cpp:110-114` |
| Storing the result in a container / comparing containers | `tests/auto/test-qcplayout/test-qcplayout.cpp:329` (`QList<QCPAxis*> axes = mPlot->axisRect(i)->axes()`); `tests/auto/test-qcpaxisrect/test-qcpaxisrect.cpp:567-570` (`QCOMPARE(ar->rangeDragAxes(...), (QList<QCPAxis*> { mPlot->xAxis, ... }))`); `tests/auto/test-plottable-group/test-plottable-group.cpp:48` (`QCOMPARE(mPlot->plottableGroups(), QList<QCPPlottableGroup*> { group })`); `:109` (`QCOMPARE(group->members(), (QList<QCPAbstractPlottable*> { b, c }))`) |
| `connect(getter(), ...)` | `tests/auto/test-qcplegend/test-qcplegend.cpp:133` (`connect(mPlot->legend->item(0), &QCPAbstractLegendItem::selectionChanged, ...)`); `examples/plots/mainwindow.cpp:1365` (`connect(volumeAxisRect->axis(QCPAxis::atBottom), ..., customPlot->xAxis, ...)`); `tests/auto/test-signal-delivery/test-signal-delivery.cpp:254` (`connect(rect->axis(QCPAxis::atBottom), ...)`) |
| Passing a member/getter as a constructor argument | `examples/plots/mainwindow.cpp` uses `customPlot->xAxis` as a `connect` target (`:1365`); `/home/jeandet/Documents/prog/SciQLopPlots/src/SciQLopTimeline.cpp:118` passes `plot->xAxis`, `plot->yAxis` into `QCPIntervals` |
| `Q_PROPERTY` READ returning the pointer | All rows in §4; e.g. moc's read of `keyAxis` expects `QCPAxis*`, while `keyAxis()` would return `QCPHandle<QCPAxis>` |
| Taking the address / binding to a `T*&` or `T**` | No binding of a getter result to `T*&`/`T**` was found in the searched trees |

---

## 7. SciQLopPlots (read-only grep)

Root: `/home/jeandet/Documents/prog/SciQLopPlots`, directories `src/` and `include/` (98
`.cpp`/`.h` files). No SciQLop or TSCat was read.

Counts of qualified call sites:

| Getter | Count | Main sites |
|---|---:|---|
| `axisRect()` | 16 | `src/SciQLopPlot.cpp:454,1173-1174,1227-1229`; `src/VPlotsAlign.cpp:37,57`; `src/SciQLopCrosshair.cpp:153,206`; `src/SciQLopNDProjectionPlot.cpp:344,359` |
| `keyAxis()` | 9 | `src/SciQLopPlotAxis.cpp:538-571`; `src/SciQLopCrosshair.cpp:290`; `src/SpanTargets.cpp:131`; `src/SciQLopStraightLines.cpp:56,115`; `src/SciQLopTimeColoredCurve.cpp:222` |
| `valueAxis()` | 4 | `src/SciQLopCrosshair.cpp:291`; `src/SpanTargets.cpp:131`; `src/SciQLopStraightLines.cpp:56,115` |
| `xAxis` member | 29 | `src/SciQLopPlot.cpp:123-132,151,395,411,1809,1815,1827`; `src/SciQLopCrosshair.cpp:150-209,265`; `src/SciQLopLineGraph.cpp:251,261`; `src/SciQLopCurve.cpp:513,522`; `src/SciQLopColorMap.cpp:314,324`; `src/SciQLopWaterfallGraph.cpp:180,191` |
| `yAxis` member | 11 | `src/SciQLopPlot.cpp:129-132,152,1212-1217,1826-1827`; `src/SciQLopCrosshair.cpp:161,167`; `src/SciQLopTimeline.cpp:118` |
| `legend` / `legend()` | 8 / 4 | `src/SciQLopHistogram.cpp:151` (`plot->legend->itemWithPlottable`); `src/SciQLopGraphInterface.cpp:80-81`; `src/SciQLopMultiPlotPanel.cpp:68,934`; `src/SciQLopNDProjectionPlot.cpp:595`; `src/SciQLopPlot.cpp:1020`; `src/SciQLopPlotLegend.cpp:50` |
| `plottables()` | 12 | `src/SciQLopPlotAxisDelegate.cpp:53`; `src/SciQLopPlotInterface.cpp:141`; `src/Registrations.cpp:114,195,222,246,259,290`; `src/SciQLopNDProjectionPlot.cpp:451,459,515,560` |
| `axis()` | 19 | `src/SciQLopPlotAxisDelegate.cpp:99`; `src/AxisSynchronizer.cpp:43`; `src/SciQLopPlot.cpp:460-490`; `src/SciQLopPlotAxis.cpp:655-864` |
| `parentPlot()` | 43 | `src/SciQLopGraphInterface.cpp` and the object base classes; e.g. `src/SciQLopGraphInterface.cpp:80` |
| `plotLayout()` | 3 | `src/VPlotsAlign.cpp:68`; `src/SciQLopPlot.cpp:1213-1214` |
| `selectionDecorator()` | 1 | `src/SciQLopGraphComponent.cpp:141` |

Patterns found:

- Raw-pointer assignment: `src/VPlotsAlign.cpp:37,57` (`auto ar = p->qcp_plot()->axisRect();`);
  `src/SciQLopCrosshair.cpp:153,206` (`auto* axisRect = m_plot->axisRect();`);
  `src/SciQLopPlot.cpp:454` (`auto* ar = this->axisRect();`).
- Comparison with a raw axis: `src/SciQLopPlotAxis.cpp:538-571`
  (`plottable->keyAxis() == m_axis`, twice, and `plottable->keyAxis()->range()`).
- Passing axes to functions/constructors: `src/SciQLopTimeline.cpp:118`
  (`new QCPIntervals(plot->xAxis, plot->yAxis, layout)`); `src/SciQLopPlot.cpp:1827`
  (`setScaleRatio(m_impl->xAxis, 1.0)`); `src/SciQLopGraphInterface.cpp` passes plot pointers.
- `connect(getter/member, ...)`: `src/SciQLopPlot.cpp:123-132` (`connect(this->xAxis, ...)`, four
  axes), `:1809,1815` (`connect`/`disconnect(m_impl->xAxis, ...)`), `:1826` (`QSignalBlocker
  by(m_impl->yAxis)`).
- `qobject_cast` on a getter result: `src/SciQLopPlot.cpp:605`
  (`qobject_cast<QCPIntervals*>(plottableAt(event->pos(), false))`);
  `src/SciQLopPlotLegend.cpp:50` (`qobject_cast<QCPLayoutInset*>(legend->layout())`).
- Range-for over a returned list: `src/SciQLopPlotAxisDelegate.cpp:53` (`for (auto* p :
  plot->plottables())`); `src/SciQLopPlotInterface.cpp:141`;
  `src/SciQLopNDProjectionPlot.cpp:291,451,459,515,560,571`.
- Containers of raw pointers: `src/SciQLopGraphInterface.cpp:43-45`
  (`QList<QCPAbstractPlottable*> plottables;`); `src/SciQLopPlot.cpp` `_pane_axes()` / `plottables()`
  consumers above.
- `xAxis`/`yAxis` as function arguments and member state: `src/SciQLopLineGraph.cpp:251,261`;
  `src/SciQLopCurve.cpp:513,522`; `src/SciQLopColorMap.cpp:314,324`;
  `src/SciQLopWaterfallGraph.cpp:180,191`.

---

## 8. Hot paths in `src`

Places where a handle-returning getter would be evaluated per point or per frame, with the loop
cited:

| Loop (file:line) | Getter(s) evaluated inside | Rate |
|---|---|---|
| `src/polar/polar-area.cpp:222-231` (`graphs()`) | `mParentPlot->plottable(i)` per plottable | once per call; called by `rescale` (`:238-239`) and area users |
| `src/coordinate-system.cpp:102-114` (`QCPCoordinateSystem::plottables()`) | `plot->plottable(i)` per plottable, `plottable->system()` per hit | once per call; the comment calls it one scan per call (`:109`) |
| `src/plottables/plottable-intervals.cpp:376-383` (`siblingHasBarAt`) | `mParentPlot->plottable(i)` per plottable, then `other->laneLayout()` | per hit-test |
| `src/plottables/plottable-intervals.cpp:1233-1237` | `mParentPlot->plottable(i)` per plottable | per row/legend query |
| `src/layoutelements/layoutelement-legend.cpp:541-548` (`selectedParts`) | `item(i)` twice per row | per `selectedParts()` call |
| `src/layoutelements/layoutelement-legend.cpp:592-596, 614-618, 808-812, 828-832` | `item(i)` twice per row | per setter, over every legend row |
| `src/layoutelements/layoutelement-legend.cpp:980-983, 999-1003` | `item(i)` per row | per `selectedItems()`/selection pass |
| `src/core.cpp:2573-2575` (`selectedAxes`) | `axisRects()` then `rect->axes()` per rect | per call, nested loop |
| `src/core.cpp:700-703` (theme application) | `axisRects()` then `rect->axes()` per rect | per theme application |
| `src/core.cpp:3487-3491` (RHI init) | `axisRects()` then `ar->axes()` per rect | once per initialize/render setup |
| `src/core.cpp:99-102` | `layer->children()` | per layer in the loop |
| `src/items/item-creation-state.cpp:44` | `mPlot->axisRects()` per creation-state update | per update |
| `src/layoutelements/layoutelement-axisrect.cpp:944` | `axes()` per preparation phase | per layout pass |
| `src/layoutelements/layoutelement-axisrect.cpp:982` | `axes(side)` per side (`atBottom/atTop/atLeft/atRight`) | per effective-axes query |
| `src/items/item.cpp:1544-1558` (`dependencyRevision`) | `parentAnchorX()`/`parentAnchorY()` per position per item | **every frame, every item** (the comment says every frame asks every item) |
| `src/items/item.cpp:614` (`QCPAbstractItem` anchor walk) | `item->positions()` per anchor chain step | per dependency/anchor query |
| `src/items/item-line.cpp:196`, `item-curve.cpp:204`, `item-ellipse.cpp:220`, `item-rect.cpp:211`, `item-bracket.cpp:260` | `positions()` per `dataExtent`/extent query | per item per frame/index query |
| `src/plottables/plottable-bars.cpp:337-349` | `getPixelSpacing` per bar in the group; `getPixelSpacing` calls `bars->keyAxis()` and `bars->keyAxis()->axisRect()` (`:373-384`) | per bar-width/stack calculation |
| `src/plottables/plottable-graph.cpp:674-690, 975-986, 1019-1092, 1127-1156` | the loops use cached `keyAxis`/`valueAxis` locals; no getter call per point | per point, but the getter is hoisted |
| `src/plottables/plottable-graph2.cpp:469-470, 545, 580-581` | `mKeyAxis->axisRect()` (member, not the getter) | per draw |
| `src/plottables/plottable-intervals.cpp:304, 335, 447, 1009` | `mKeyAxis->axisRect()->rect()` inside lane loops | per lane |
| `src/layoutelements/layoutelement-axisrect.cpp:816-817` | `position->axisRect()`, `position->keyAxis()`, `position->valueAxis()` per position | per rect dirty-mark pass |
| `src/axis/axis.cpp:1610` | `position->keyAxis()`/`valueAxis()` per position | per axis dirty-mark pass |

The per-item-per-frame path is `QCPAbstractItem::dependencyRevision()` (`src/items/item.cpp:1544-1558`):
it walks every position of every item and calls `parentAnchorX()`/`parentAnchorY()`. The per-item
extent path re-creates `positions()` (`item-line.cpp:196` etc.). The per-frame legend paths call
`item(i)` for every row. Those are the places where a handle return would add a `QPointer` copy per
evaluation; the point loops in `QCPGraph` draw already cache raw pointers, so they do not call the
getter per point.

---

## Open questions

Facts that force a design choice:

1. **Non-`QObject` returns.** `QCPSelectionDecorator`, `QCPItemPosition`, `QCPItemAnchor`,
   `QCPPlottableInterface1D`, `QCPColorMapData`, `QCPLabelRenderer` and the paint/RHI buffers cannot
   be `QCPHandle<T>` (`handle.h:91-94`). The getters still return them (`selectionDecorator()`,
   `positions()/anchors()`, `interface1D()`, `data()`, `labelRenderer()`). Do they stay raw, get a
   different handle type, or change return type?
2. **Parent getters.** `parentPlot()` returns the widget, which is not `qcp::Removable`
   (`handle.h:24-30,36-40`); `layout()` and `parentLegend()` return types that *are* removable
   (`handle.h:28`), so a handle's `remove()` would delete the parent. Is `remove()` on a parent
   handle acceptable, removed from the handle, or are these getters left raw?
3. **Properties.** Every object-typed `Q_PROPERTY` is declared with a raw pointer and its READ is
   one of the getters above. `QCPHandle<T>` has no conversion to `T*` (`handle.h:51`) and is not a
   declared metatype anywhere. Do the property types become `QCPHandle<T>` (with metatype
   registration), does a raw accessor stay beside the handle getter, or do the properties change
   shape?
4. **List returns.** `selectedGraphs()`, `selectedPlottables()`, `axisRects()`, `axes()`, `graphs()`,
   `plottables()`, `items()`, `positions()`, `anchors()`, `children()` and `elements()` return
   `QList<T*>` (or `QList<QList<...>>`). Should they return `QList<QCPHandle<T>>`, keep raw lists, or
   return a view? There is no `qHash`/`std::hash` for `QCPHandle` today, so a `QSet`/`QHash` of
   handles has no key function.
5. **`addGraph` and other raw creators.** `addGraph()` still returns `QCPGraph*`
   (`core.h:412`), and `addAxis()`/`addRadialAxis()` still return raw pointers
   (`layoutelement-axisrect.h:117`, `polar-area.h:86`), while `add<T>()` returns
   `HandleFor<T>`. A5 says `addGraph` should return the same handle as `add<QCPGraph>`
   (`docs/specs/2026-11-11-...:147-151`); the tree still returns the raw pointer.
6. **Public members.** `xAxis`, `yAxis`, `xAxis2`, `yAxis2`, `legend` are public raw members
   (`core.h:480-481`), not accessors. Converting them to handle-returning accessors is a source
   break at every use site; keeping them raw leaves the last raw axis/legend pointers in the public
   API (M5 inventory §1 row 6).
7. **`get()`/`as<U>()` escape hatches.** Both return raw pointers (`handle.h:53-57,66-70`), and
   `operator->`/`get()` are the only way to call a method with the same name as a handle member
   (`get()` is used throughout `construct.h`/`handle-ops.h`). The policy for raw escape inside the
   library and for users is not fixed by the handle type itself.
8. **Signals vs handles.** Signals stay raw (`core.h:490-504`, `layer.h:297`,
   `item-creation-state.h:26`). A slot that wants to keep the argument must construct a
   `QCPHandle<T>` explicitly (`handle.h:49`); no helper besides `qcpConnect` exists.
