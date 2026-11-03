# Inventory for NeoQCP 3.0 milestone 2

Repository: `/home/jeandet/Documents/prog/NeoQCP-3.0`, branch `neoqcp-3.0`, read-only pass.
Every entry is a citation of the current tree; no opinions, no proposals.

---

## 1. Creation entry points

Everything a plot can own is created today either through a `QCustomPlot::add*`/`QCPAxisRect::add*`
method, or with `new` in user code followed by an explicit insert into a layout.

| Entry point | Signature | Parent/registration it performs | Defaults it picks |
|---|---|---|---|
| `QCustomPlot::addGraph` — `src/core.cpp:1610` | `QCPGraph* addGraph(QCPAxis* keyAxis = nullptr, QCPAxis* valueAxis = nullptr)` | creates `new QCPGraph(keyAxis, valueAxis)` (`:1633`), which self-registers into `mPlottables` (`src/plottables/plottable.cpp:466`) and `mGraphs` (`src/plottables/plottable-graph.cpp:190-192`) | null axes → `xAxis`/`yAxis` (`:1613-1614`); name `"Graph N"` (`:1634`); layer = `currentLayer()` (`src/layer.cpp:547-548`) |
| `QCustomPlot::addPlottableGroup` — `src/core.cpp:1476` | `QCPPlottableGroup* addPlottableGroup(const QString& name)` | `new QCPPlottableGroup(this, name)`; appends to `mPlottableGroups` (`:1481`) | QObject child of the plot; connects `legendDoubleClick` (`src/plottables/plottable-group.cpp:46-51`) |
| `QCPPlottableGroup::QCPPlottableGroup` — `src/plottables/plottable-group.cpp:43` | `QCPPlottableGroup(QCustomPlot* parentPlot, const QString& name)` | `QObject(parentPlot)`; **no list registration** — only `addPlottableGroup` appends to `mPlottableGroups` | no legend row until members are added (`plottable-group.h:33-36`) |
| `QCustomPlot::addLayer` — `src/core.cpp:1980` | `bool addLayer(const QString& name, QCPLayer* otherLayer = nullptr, LayerInsertMode insertMode = limAbove)` | inserts a new `QCPLayer` into `mLayers`; returns `bool`, the layer is fetched back with `layer(name)` | created layer gets a paint buffer at the next `setupPaintBuffers()` (`src/core.cpp:4057-4088`); default other layer = top |
| `QCPAxisRect::addAxis` — `src/layoutelements/layoutelement-axisrect.cpp:357` | `QCPAxis* addAxis(QCPAxis::AxisType type, QCPAxis* axis = nullptr)` | `new QCPAxis(this, type)` (`:363`), appends to `mAxes[type]` (`:391`), sets the plot's `xAxis`/`yAxis`/… convenience pointers when this is `axisRect(0)` (`:393-412`) | `QCPAxis` ctor creates its `QCPGrid` (`src/axis/axis.cpp:443`), grid hidden (`:454`), layer = `currentLayer()` (`:456`) |
| `QCPAxisRect::QCPAxisRect` — `src/layoutelements/layoutelement-axisrect.cpp:176` | `QCPAxisRect(QCustomPlot* parentPlot, bool setupDefaultAxes = true)` | `QCPLayoutElement(parentPlot)`; creates `mInsetLayout` (`:181`); **does not insert itself into any layout** | with `setupDefaultAxes` creates bottom+left axes (`:209-215`) |
| `QCPAxisRect::setupFullAxesBox` — `src/layoutelements/layoutelement-axisrect.cpp:609` (decl `layoutelement-axisrect.h:110`) | `void setupFullAxesBox(bool connectRanges = false)` | adds the four axes to the rect | visible axes on all four sides |
| `QCPLayoutGrid::addElement` — `src/layout.h:318-319` | `bool addElement(int row, int column, QCPLayoutElement* element)` / `bool addElement(QCPLayoutElement* element)` | puts the element into the grid; `initializeParentPlot` sets the plot pointer (`src/layout.cpp:780-800`) | free cell / next row |
| `QCPLayoutInset::addElement` — `src/layout.h:388-389` | `void addElement(QCPLayoutElement* element, Qt::Alignment alignment)` / `void addElement(QCPLayoutElement* element, const QRectF& rect)` | puts the element into the inset (the default legend goes here, `src/core.cpp:493`) | alignment/rect as given |
| `QCPLegend` created with `new` — `src/layoutelements/layoutelement-legend.h:183`, ctor `layoutelement-legend.cpp:478` | `QCPLegend()` | **no plot argument at all**; becomes plot-owned only when inserted (`src/core.cpp:491-493` inserts the default legend into `axisRect(0)->insetLayout()`) | `QCPLayoutGrid`, fill order rows, icon size 32×18, black border, white brush (`:480-500`) |
| `QCPColorScale` — `src/layoutelements/layoutelement-colorscale.h:81`, ctor `layoutelement-colorscale.cpp:126` | `QCPColorScale(QCustomPlot* parentPlot)` | `QCPLayoutElement(parentPlot)`, creates a private `QCPColorScaleAxisRectPrivate` (`:133`); **not inserted into a layout** | type `atRight`, `stLinear`, `gpCold`, bar width 20, data range 0…6 (`:143-148`) |
| `QCPTextElement` — `src/layoutelements/layoutelement-textelement.h:50-55`, ctors `layoutelement-textelement.cpp:78,105,132,163,186` | `(QCustomPlot*)`, `(QCustomPlot*, const QString&)`, `(…, double pointSize)`, `(…, QString family, int pointSize, bool bold)`, `(…, const QFont&)` | `QCPLayerable` child of the plot; **not inserted** | layer = `currentLayer()` |
| `QCPBarsGroup` — `src/plottables/plottable-bars.h:63`, ctor `plottable-bars.cpp:104` | `explicit QCPBarsGroup(QCustomPlot* parentPlot)` | `QObject(parentPlot)` only; **no list** | spacing type `stAbsolute`, spacing 4 (`:105`) |
| `QCPLaneLayout` — `src/plottables/lane-layout.h:26` | `explicit QCPLaneLayout(QObject* parent = nullptr)` | plain `QObject`; **not plot-owned** | empty lane list |
| `QCPMarginGroup` — `src/layout.h:41` | `explicit QCPMarginGroup(QCustomPlot* parentPlot)` | `QObject(parentPlot)` | — |
| Item creation, all items — e.g. `src/items/item-text.h:55`, ctor `src/items/item.cpp:1075` | `explicit QCPItemText(QCustomPlot* parentPlot)` | base ctor calls `parentPlot->registerItem(this)` (`:1078`) → `mItems` (`src/core.cpp:4493-4513`); **no null check on `parentPlot` before the call** | layer = `currentLayer()`; clip-to-axis-rect on with the first axis rect (`item.cpp:1082-1087`) |
| Item creation by drag — `QCustomPlot::setCreationModeEnabled` — `src/core.h:183`, `src/core.cpp:1156` | `void setCreationModeEnabled(bool enabled)` | drives `QCPItemCreationState` (`src/item-creation-state.cpp:65,84,134,150`) which `new`s the selected item type | item type and pointer from internal state |
| Plottables created with `new` (all 14 concrete types, §2) | `<T>(QCPAxis* keyAxis, QCPAxis* valueAxis)` | base ctor `QCPAbstractPlottable::QCPAbstractPlottable` (`src/plottables/plottable.cpp:437`) calls `mParentPlot->registerPlottable(this)` (`:466`) → `mPlottables`, plus `addToLegend()` when `mAutoAddPlottableToLegend` (`src/core.cpp:4437-4440`) | layer = `currentLayer()` (`src/layer.cpp:547`); null axes → inert, not registered (`plottable.cpp:461-462`) |
| `QCPErrorBars` — `src/plottables/plottable-errorbar.h:94` | `explicit QCPErrorBars(QCPAxis*, QCPAxis*)` | same base registration; attaches to a data plottable only via `setDataPlottable` (`:112`) | no data plottable |
| `QCPIntervals` — `src/plottables/plottable-intervals.h:22` | `QCPIntervals(QCPAxis* keyAxis, QCPAxis* valueAxis, QCPLaneLayout* layout)` | base registration; the layout is **not** owned (`lane-layout.h`) | no lane layout → flat rendering |
| `QCPMultiGraph` — `src/plottables/plottable-multigraph.h:43` | `explicit QCPMultiGraph(QCPAxis*, QCPAxis*)` | base registration | — |
| Polar axis — `src/polar/layoutelement-angularaxis.h:79`, ctor `layoutelement-angularaxis.cpp:138` | `explicit QCPPolarAxisAngular(QCustomPlot* parentPlot)` | `QCPLayoutElement(parentPlot)`; **does not insert itself into any layout** (`:204-205` are commented-out inset wiring); owns `mGraphs` | layer = `currentLayer()` |
| Polar radial axis — `src/polar/radialaxis.h:100` | `explicit QCPPolarAxisRadial(QCPPolarAxisAngular* parent)` | child of the angular axis | — |
| `QCPPolarAxisAngular::addRadialAxis` — `src/polar/layoutelement-angularaxis.h:234` | `QCPPolarAxisRadial* addRadialAxis(QCPPolarAxisRadial* axis = 0)` | appends to the angular axis | — |
| Polar graph — `src/polar/polargraph.h:91`, ctor `polargraph.cpp:149` | `QCPPolarGraph(QCPPolarAxisAngular* keyAxis, QCPPolarAxisRadial* valueAxis)` | `mKeyAxis->registerPolarGraph(this)` in the ctor body (`polargraph.cpp:165`) | layer = `currentLayer()`; pen blue, `lsLine` (`:169-171`) |
| Polar grid — `src/polar/polargrid.h:59` | `explicit QCPPolarGrid(QCPPolarAxisAngular* parentAxis)` | owned by the angular axis | — |
| `QCPOverlay` — `src/core.h:249` (`overlay()`), impl `src/core.cpp` | `QCPOverlay* overlay()` | lazily creates and owns the overlay | layer `"overlay"` |
| `QCPSelectionRect` — `src/core.h:211` (`setSelectionRect`) | `void setSelectionRect(QCPSelectionRect*)` | plot takes the pointer, `mSelectionRect` is a `QPointer` (`src/core.h:414`) | created by the plot on demand |

---

## 2. Constructor signatures of concrete plottables and items

All plottables are `QCPLayerable` subclasses; the base ctor (`src/plottables/plottable.cpp:437-467`)
does the registration described in §1. All items are `QCPLayerable` subclasses; the base ctor
(`src/items/item.cpp:1075-1087`) registers in `mItems`.

### Plottables (`src/plottables`)

| Class | Constructor declaration | Registers in ctor |
|---|---|---|
| `QCPGraph` | `explicit QCPGraph(QCPAxis* keyAxis, QCPAxis* valueAxis)` — `plottable-graph.h:110` | yes: `mPlottables` + `mGraphs` (`plottable-graph.cpp:190-192`) |
| `QCPGraph2` | `explicit QCPGraph2(QCPAxis*, QCPAxis*)` — `plottable-graph2.h:27` | yes (base); ctor replots once (`plottable-graph2.cpp:40`) |
| `QCPCurve` | `explicit QCPCurve(QCPAxis*, QCPAxis*)` — `plottable-curve.h:97` | yes (base) |
| `QCPBars` | `explicit QCPBars(QCPAxis*, QCPAxis*)` — `plottable-bars.h:183` | yes (base); joins a `QCPBarsGroup` only via `QCPBarsGroup::append` |
| `QCPStatisticalBox` | `explicit QCPStatisticalBox(QCPAxis*, QCPAxis*)` — `plottable-statisticalbox.h:100` | yes (base) |
| `QCPColorMap` | `explicit QCPColorMap(QCPAxis*, QCPAxis*)` — `plottable-colormap.h:120` | yes (base) |
| `QCPColorMap2` | `explicit QCPColorMap2(QCPAxis*, QCPAxis*)` — `plottable-colormap2.h:22` | yes (base); ctor schedules a queued replot (`plottable-colormap2.cpp:36`) |
| `QCPFinancial` | `explicit QCPFinancial(QCPAxis*, QCPAxis*)` — `plottable-financial.h:123` | yes (base) |
| `QCPErrorBars` | `explicit QCPErrorBars(QCPAxis*, QCPAxis*)` — `plottable-errorbar.h:94` | yes (base); data plottable set separately (`:112`) |
| `QCPHistogram` | `explicit QCPHistogram(QCPAxis*, QCPAxis*)` — `plottable-histogram.h:26` | yes (base) |
| `QCPHistogram2D` | `explicit QCPHistogram2D(QCPAxis*, QCPAxis*)` — `plottable-histogram2d.h:23` | yes (base); ctor schedules a queued replot (`plottable-histogram2d.cpp:23`) |
| `QCPMultiGraph` | `explicit QCPMultiGraph(QCPAxis*, QCPAxis*)` — `plottable-multigraph.h:43` | yes (base); ctor schedules a queued replot (`plottable-multigraph.cpp:55`) |
| `QCPWaterfallGraph` | `explicit QCPWaterfallGraph(QCPAxis*, QCPAxis*)` — `plottable-waterfall.h:47` | yes (base) |
| `QCPIntervals` | `QCPIntervals(QCPAxis*, QCPAxis*, QCPLaneLayout*)` — `plottable-intervals.h:22` | yes (base); the layout is borrowed |
| `QCPAbstractPlottable1D<T>` (base) | `QCPAbstractPlottable1D(QCPAxis*, QCPAxis*)` — `plottable1d.h:63` | no own registration, delegates to `QCPAbstractPlottable` |
| `QCPAbstractPlottable` (base) | `QCPAbstractPlottable(QCPAxis*, QCPAxis*)` — `plottable.h:121` | yes (`plottable.cpp:466`); null axes → inert (`:461-462`) |

### Items (`src/items`, `src/polar`)

Every item takes `QCustomPlot* parentPlot` and registers in the base ctor
(`item.cpp:1078`), then clips to the first axis rect (`:1082-1087`).

| Class | Declaration |
|---|---|
| `QCPItemBracket` | `explicit QCPItemBracket(QCustomPlot*)` — `item-bracket.h:64` |
| `QCPItemCurve` | `explicit QCPItemCurve(QCustomPlot*)` — `item-curve.h:47` |
| `QCPItemEllipse` | `explicit QCPItemEllipse(QCustomPlot*)` — `item-ellipse.h:46` |
| `QCPItemHSpan` | `explicit QCPItemHSpan(QCustomPlot*)` — `item-hspan.h:21` |
| `QCPItemLine` | `explicit QCPItemLine(QCustomPlot*)` — `item-line.h:47` |
| `QCPItemPixmap` | `explicit QCPItemPixmap(QCustomPlot*)` — `item-pixmap.h:48` |
| `QCPItemRect` | `explicit QCPItemRect(QCustomPlot*)` — `item-rect.h:46` |
| `QCPItemRichText` | `explicit QCPItemRichText(QCustomPlot*)` — `item-richtext.h:15` |
| `QCPItemRSpan` | `explicit QCPItemRSpan(QCustomPlot*)` — `item-rspan.h:21` |
| `QCPItemStraightLine` | `explicit QCPItemStraightLine(QCustomPlot*)` — `item-straightline.h:44` |
| `QCPItemText` | `explicit QCPItemText(QCustomPlot*)` — `item-text.h:55` |
| `QCPItemTracer` | `explicit QCPItemTracer(QCustomPlot*)` — `item-tracer.h:72` |
| `QCPItemVSpan` | `explicit QCPItemVSpan(QCustomPlot*)` — `item-vspan.h:21` |
| `QCPAbstractItem` (base) | `explicit QCPAbstractItem(QCustomPlot*)` — `item.h:191` |
| `QCPPolarGraph` | `QCPPolarGraph(QCPPolarAxisAngular*, QCPPolarAxisRadial*)` — `polargraph.h:91` |
| `QCPPolarGrid` | `explicit QCPPolarGrid(QCPPolarAxisAngular*)` — `polargrid.h:59` |

Support classes that items allocate themselves and expose as members (not registered anywhere):
`QCPItemPosition` (`item.h:122`), `QCPItemAnchor` (`item.h:45`) — no registration, owned by the item.

---

## 3. Getters that return a raw pointer to a plot-owned object

### `QCustomPlot` (`src/core.h`)

| Getter | Line | Returns |
|---|---|---|
| `QCPAbstractPlottable* plottable(int)` / `plottable()` | `277-278` | `mPlottables` entry |
| `QCPPlottableGroup* addPlottableGroup` / `QList<QCPPlottableGroup*> plottableGroups()` | `284`, `286` | group list (copy of raw pointers) |
| `QList<QCPAbstractPlottable*> selectedPlottables()` | `287` | computed scan |
| `QCPGraph* graph(int)` / `graph()` | `296-297` | `mGraphs` entry |
| `QList<QCPGraph*> selectedGraphs()` | `303` | computed |
| `QCPAbstractItem* item(int)` / `item()` | `306-307` | `mItems` entry |
| `QList<QCPAbstractItem*> selectedItems()` | `312` | computed |
| `QCPLayer* layer(const QString&)` / `layer(int)` / `currentLayer()` | `319-321` | `mLayers` entry |
| `QCPAxisRect* axisRect(int)` / `QList<QCPAxisRect*> axisRects()` | `332-333` | layout scan |
| `QCPLayoutGrid* plotLayout()` | `156` | `mPlotLayout` (`QPointer` behind the getter) |
| `QCPOverlay* overlay()` | `237` | lazily created |
| `QCPSpanRhiLayer* spanRhiLayer()` / `gridRhiLayer()` | `239`, `243` | lazily created GPU layers |
| `QCPTheme* theme()` | `213` | borrowed/stored |
| `QCPPipelineScheduler* pipelineScheduler()` | `272` | owned |
| public members `xAxis, yAxis, xAxis2, yAxis2` | `362` | convenience pointers, cleared by `~QCPAxis` |
| public member `legend` | `363` | cleared by `~QCPLegend` (`core.cpp:4245-4246`) |

There is no `QCustomPlot::axis()` accessor: the convenience members are the only plot-level axis
handles.

### `QCPAxisRect` (`src/layoutelements/layoutelement-axisrect.h`)

| Getter | Line |
|---|---|
| `QCPAxis* axis(QCPAxis::AxisType, int index = 0)` | `95` |
| `QList<QCPAxis*> axes(QCPAxis::AxisTypes)` / `axes()` | `96-97` |
| `QCPLayoutInset* insetLayout()` | `102` |

### `QCPAxis` (`src/axis/axis.h`)

| Getter | Line |
|---|---|
| `QCPAxisRect* axisRect()` | `232` |
| `QCPGrid* grid()` (allocating, `axis.cpp:499-509`) / `gridIfExists()` | `311`, `314` |

### `QCPLegend` (`src/layoutelements/layoutelement-legend.h`)

| Getter | Line |
|---|---|
| `QCPAbstractLegendItem* item(int index)` | `237` |
| `QCPPlottableLegendItem* itemWithPlottable(const QCPAbstractPlottable*)` | `238` |
| `QCPAbstractLegendItem::parentLegend()` (on the item) | `56` |

### `QCPColorScale` (`src/layoutelements/layoutelement-colorscale.h`)

| Getter | Line |
|---|---|
| `QCPAxis* axis()` | `85` |

### `QCPAbstractPlottable` (`src/plottables/plottable.h`)

| Getter | Line | Backing |
|---|---|---|
| `QCPAxis* keyAxis()` | `135` | `QPointer` (`mKeyAxis.data()`) |
| `QCPAxis* valueAxis()` | `137` | `QPointer` |
| `QCPSelectionDecorator* selectionDecorator()` | `145` | raw, owned (`plottable.cpp:461-467` deletes it) |
| `QCPPlottableGroup* group()` | `plottable.h:206` | raw, member `mGroup` (`:289`), set and cleared by the group |

Class-specific: `QCPErrorBars::dataPlottable()` — `plottable-errorbar.h:100` (`QPointer`, `:145`).

### `QCPAbstractItem` (`src/items/item.h`)

| Getter | Line |
|---|---|
| `QCPAxisRect* clipAxisRect()` | `197` |
| `QCPItemPosition* position(const QString& name)` | `219` |
| `QCPItemAnchor* anchor(const QString& name)` | `220` |

Public `const` pointer members on the concrete items (user-facing, all owned by the item):
`QCPItemBracket::left/right/center` (`item-bracket.h:86-88`), `QCPItemCurve::start/startDir/endDir/end`
(`item-curve.h:69-72`), `QCPItemEllipse::topLeft/bottomRight/topLeftRim/top/topRightRim/right/bottomRightRim/bottom/bottomLeftRim/left/center`
(`item-ellipse.h:68-78`), `QCPItemHSpan::lowerEdge/upperEdge/center` (`item-hspan.h:29-31`),
`QCPItemLine::start/end` (`item-line.h:69-70`), `QCPItemRect::topLeft/bottomRight`
(`item-rect.h:68-69`), `QCPItemStraightLine::point1/point2` (`item-straightline.h:60-61`),
`QCPItemText::position` (`item-text.h:104`),
`QCPItemVSpan::lowerEdge/upperEdge/center` (`item-vspan.h:29-31`).
`QCPItemTracer::graph()` — `item-tracer.h:88` (`QPointer`, `:46` property is raw-typed but reads through it).

### Layout classes (`src/layout.h`)

| Getter | Line |
|---|---|
| `QCPLayoutElement* elementAt(int)` (pure virtual) | `212` |
| `QCPLayoutGrid::elementAt(int)` | `308` |
| `QCPLayoutInset::elementAt(int)` | `378` |
| `QCPMarginGroup* marginGroup(...)` on elements | `layout.h` |

### `QCPLayerable` (`src/layer.h`)

| Getter | Line |
|---|---|
| `QCustomPlot* parentPlot()` | `151` |
| `QCPLayerable* parentLayerable()` | `153` (`QPointer`) |
| `QCPLayer* layer()` | `155` (raw) |

### Polar (`src/polar`)

| Getter | Line |
|---|---|
| `QCPPolarAxisAngular::radialAxis(int index = 0)` | `layoutelement-angularaxis.h:232` |
| `QList<QCPPolarGraph*> mGraphs` (protected, handed out by iteration) | `layoutelement-angularaxis.h:341` |
| `QCPPolarAxisRadial` reachable from `QCPPolarGraph::valueAxis()` | `polargraph.h` |

---

## 4. Removal entry points

"Deletes" = the callee destroys the object; "hands back" = the caller owns it afterwards.

| API | Line | Behaviour |
|---|---|---|
| `QCustomPlot::removePlottable(QCPAbstractPlottable*)` | `core.cpp:1380` | **deletes** via `deletePlottable` (`:1395`): unlists from group/legend/lists first, then `delete` |
| `QCustomPlot::removePlottable(int)` | `core.cpp:1451` | deletes by index |
| `QCustomPlot::clearPlottables()` | `core.cpp:1498` | deletes all, returns count |
| `QCustomPlot::removeGraph(QCPGraph*)` / `(int)` / `clearGraphs()` | `core.cpp:1646`, `1655`, `1672` | delete (routes through `removePlottable`) |
| `QCustomPlot::removeItem(QCPAbstractItem*)` / `(int)` / `clearItems()` | `core.cpp:1756`, `1778`, `1797` | unlist then **delete** (`:1762-1764`); `clearItems` → `deleteAllItems` (`:1809`), unlists all first |
| `QCustomPlot::removeLayer(QCPLayer*)` | `core.cpp:2026` | unlist (children move to the neighbouring layer) then **delete** |
| `QCustomPlot::removePlottableGroup(QCPPlottableGroup*)` | `core.cpp:1489` | **delete**; members keep their data |
| `QCPAxisRect::removeAxis(QCPAxis*)` | `layoutelement-axisrect.cpp:458` | unlist (`:471`) then **delete** |
| `QCPAxisRect::setupFullAxesBox`-created axes | — | removed only via `removeAxis` |
| `QCPLegend::removeItem(int)` / `removeItem(QCPAbstractLegendItem*)` | `layoutelement-legend.cpp:877`, `:902` | **delete** the row via `remove()` (`QCPLayout::remove`), then re-flow the grid |
| `QCPLegend::clearItems()` | `layoutelement-legend.cpp:913` | deletes all rows with `removeAt` (`:919`) |
| `QCPLayout::removeAt(int)` | `layout.h:218`, impl `layout.cpp:787` | **deletes** the element (`takeAt` + `releaseElement` + `delete el`, `:1743-1752`) |
| `QCPLayout::remove(QCPLayoutElement*)` | `layout.h:219` | deletes via `removeAt` |
| `QCPLayout::clear()` | `layout.h:220`, impl `layout.cpp:1743` | deletes every element |
| `QCPLayout::takeAt(int)` | `layout.h:213` (pure virtual; `QCPLayoutGrid` `:309`, `QCPLayoutInset` `:379`) | **hands back**, caller owns |
| `QCPLayout::take(QCPLayoutElement*)` | `layout.h:214` (`QCPLayoutGrid` `:310`, `QCPLayoutInset` `:380`) | **hands back**; `false` when the element is not in the layout (`layout.cpp:1769-1772`) |
| `QCPBarsGroup::clear()` | `plottable-bars.h:84` | detaches bars, does not delete them |
| `QCPPlottableGroup::removeMember(QCPAbstractPlottable*)` / `detach(..., bool restoreRow)` | `plottable-group.h:36`, `:48` | membership only |
| `QCPPolarAxisAngular::removeGraph(QCPPolarGraph*)` | `layoutelement-angularaxis.cpp:639` | detaches then **deletes** (see ctor `~QCPPolarGraph`, `polargraph.cpp:174-176`) |
| `QCPPolarAxisAngular::removeRadialAxis(QCPPolarAxisRadial*)` | `layoutelement-angularaxis.h:235` | deletes |
| `QCPAbstractPlottable::removeFromLegend(QCPLegend*)` | `plottable.h` | removes the row, keeps the plottable |
| `QCPPlottableGroup` destruction | `plottable-group.cpp:54` | returns members to their own legend rows |

Objects with **no** removal API (only `delete`/`deleteLater`): `QCPColorScale`, `QCPTextElement`,
`QCPAxisRect`, `QCPLegend` (aside from rows), `QCPItemTracer`-specific links, `QCPLaneLayout`.

---

## 5. Repaint

### Every `replot(` call in `src/` (product code), with the enclosing function

```
src/axis/axis.cpp:1929            QCPAxis::mouseMoveEvent
src/axis/axis.cpp:1990            QCPAxis::wheelEvent
src/core.cpp:698                  QCustomPlot::applyTheme
src/core.cpp:1156                 QCustomPlot::setCreationModeEnabled
src/core.cpp:2407                 QCustomPlot::commitPendingData
src/core.cpp:2417                 QCustomPlot::replot (definition)
src/core.cpp:2428                 QCustomPlot::replot (queued path: QTimer::singleShot)
src/core.cpp:2919                 QCustomPlot::initialize
src/core.cpp:2975                 QCustomPlot::initialize
src/core.cpp:3035                 QCustomPlot::ensureCompositePipeline (comment only, see note)
src/core.cpp:3326                 QCustomPlot::render
src/core.cpp:3393                 QCustomPlot::resizeEvent
src/core.cpp:3408                 QCustomPlot::showEvent
src/core.cpp:3596                 QCustomPlot::mouseReleaseEvent
src/core.cpp:3836                 QCustomPlot::pinchStep
src/core.cpp:3856                 QCustomPlot::nativeGestureEvent
src/core.cpp:4335                 QCustomPlot::processRectSelection
src/core.cpp:4361                 QCustomPlot::processRectZoom
src/core.cpp:4405                 QCustomPlot::announceUserSelection
src/item-creation-state.cpp:65    QCPItemCreationState::handleMousePress
src/item-creation-state.cpp:84    QCPItemCreationState::handleMouseMove
src/item-creation-state.cpp:134   QCPItemCreationState::commitItem
src/item-creation-state.cpp:150   QCPItemCreationState::cancelItem
src/items/item-hspan.cpp:46       QCPItemHSpan::setRange
src/items/item-hspan.cpp:174      QCPItemHSpan::mouseMoveEvent
src/items/item-rspan.cpp:61       QCPItemRSpan::setKeyRange
src/items/item-rspan.cpp:74       QCPItemRSpan::setValueRange
src/items/item-rspan.cpp:264      QCPItemRSpan::mouseMoveEvent
src/items/item-vspan.cpp:46       QCPItemVSpan::setRange
src/items/item-vspan.cpp:174      QCPItemVSpan::mouseMoveEvent
src/layer.cpp:272                 QCPLayer::replot (definition)
src/layer.cpp:296                 QCPLayer::replot (fallback: QCustomPlot::replot)
src/layoutelements/layoutelement-axisrect.cpp:196   QCPAxisRect::QCPAxisRect (wheel-restore timer)
src/layoutelements/layoutelement-axisrect.cpp:1384  QCPAxisRect::mouseMoveEvent
src/layoutelements/layoutelement-axisrect.cpp:1485  QCPAxisRect::wheelEvent
src/layoutelements/layoutelement-axisrect.cpp:1515  QCPAxisRect::wheelEvent
src/layoutelements/layoutelement-legend-group.cpp:45 QCPGroupLegendItem::setExpanded
src/overlay.cpp:74                QCPOverlay::redraw
src/plottables/plottable-colormap2.cpp:36   QCPColorMap2::QCPColorMap2
src/plottables/plottable-colormap2.cpp:254  QCPColorMap2::redrawLater
src/plottables/plottable-colormap2.cpp:496  QCPColorMap2::contoursChanged
src/plottables/plottable-graph2.cpp:40      QCPGraph2::QCPGraph2
src/plottables/plottable-graph2.cpp:147     QCPGraph2::applySourceNow
src/plottables/plottable-graph2.cpp:230     QCPGraph2::dataChanged
src/plottables/plottable-graph2.cpp:260     QCPGraph2::onL1Ready
src/plottables/plottable-histogram.cpp:297  QCPHistogram::requestReplot
src/plottables/plottable-histogram2d.cpp:23  QCPHistogram2D::QCPHistogram2D
src/plottables/plottable-histogram2d.cpp:158 QCPHistogram2D::setNormalization
src/plottables/plottable-histogram2d.cpp:234 QCPHistogram2D::redrawLater
src/plottables/plottable-intervals.cpp:262  QCPIntervals::requestRepaint
src/plottables/plottable-multigraph.cpp:55  QCPMultiGraph::QCPMultiGraph
src/plottables/plottable-multigraph.cpp:161 QCPMultiGraph::applySourceNow
src/plottables/plottable-multigraph.cpp:261 QCPMultiGraph::dataChanged
src/plottables/plottable-multigraph.cpp:290 QCPMultiGraph::onL1Ready
src/plottables/plottable.cpp:1161          QCPAbstractPlottable::onDebounceTimeout
src/polar/layoutelement-angularaxis.cpp:1705 QCPPolarAxisAngular::mouseMoveEvent
src/polar/layoutelement-angularaxis.cpp:1778 QCPPolarAxisAngular::wheelEvent
src/polar/radialaxis.cpp:1310             QCPPolarAxisRadial::mouseMoveEvent
src/polar/radialaxis.cpp:1366             QCPPolarAxisRadial::wheelEvent
src/selectionrect.cpp:132                QCPSelectionRect::~QCPSelectionRect
src/selectionrect.cpp:197                QCPSelectionRect::cancelWith
src/selectionrect.cpp:225                QCPSelectionRect::moveSelection
```

Note: `src/core.cpp:3035` is a comment ("replot() sets these offsets too"), not a call.
Of these calls, 49 use `rpQueuedReplot` and 4 use `rpImmediateRefresh`; the rest are the default
argument, which is `rpImmediateRefresh` (`src/core.h:358`).

### Existing lazy / queued mechanisms

| Mechanism | Where |
|---|---|
| `QCustomPlot::replot(rpQueuedReplot)` → `mReplotQueued` + `QTimer::singleShot(0, …)` | `src/core.cpp:2417-2434`; flag declared `src/core.h:427`, reset at `:2442`, `:2447`; `rpQueuedReplot` enum `src/core.h:127` |
| `QCPLayer::replot()` (single buffered layer) | `src/layer.cpp:272-297`; requires `lmBuffered` (`src/layer.h:88-96`) |
| `QCPLayer::markDirty()` → `paintBuffer->setContentDirty(true)` | `src/layer.cpp:299-303` |
| `QCPLayer::invalidatePaintBuffer()` → `paintBuffer->setInvalidated()` | `src/layer.cpp:396-400`; callers `layer.cpp:582` (`QCPLayerable::setVisible`), `overlay.cpp:72`, `plottable.cpp:1159`, `plottable-multigraph.cpp:583` |
| `QCPAxisRect::markAffectedLayersDirty()` / `markLayersUsingAxisDirty(const QCPAxis*)` | `layoutelement-axisrect.cpp:1520`, `:494` |
| `QCPIntervals::markLayersDirty()` / `requestRepaint()` | `plottable-intervals.cpp:251-263` |
| `QCPColorMap2::redrawLater()` / `QCPHistogram2D::redrawLater()` | `plottable-colormap2.cpp:249-255`, `plottable-histogram2d.cpp:230-235` |
| `QCPHistogram::requestReplot()` | `plottable-histogram.cpp:293-298` |
| Widget-level `update()` | `src/layer.cpp:290`, `src/core.cpp:2551` |
| Deferred data swap (`requestDataSwap`, `mDataSwapTimer`) | `src/core.h:236-243`, `core.cpp:2407` (`commitPendingData`) |
| Hidden-widget skip (`setSkipReplotsWhenHidden`) | `src/core.h:224-229`, `core.cpp:2435-2444`, `layer.cpp:274-276` |

### How layer dirtiness works today

Each layer holds a weak reference to a paint buffer (`QCPLayer::mPaintBuffer`, `src/layer.h:110`);
`QCustomPlot::setupPaintBuffers()` (`src/core.cpp:4057-4088`) assigns one buffer per run of
`lmLogical` layers, starting a new buffer at every `lmBuffered` layer, and recreates the buffer list
when the layer set changes. A layer's buffer carries two flags from `QCPAbstractPaintBuffer`
(`src/painting/paintbuffer.h:50-52`): `invalidated` (the buffer cannot be reused, e.g. size change or
a layer moved) and `contentDirty` (must be redrawn). Marking is always per layer:
`QCPLayer::markDirty()` sets `contentDirty` (`layer.cpp:299-303`), `QCPLayer::invalidatePaintBuffer()`
sets `invalidated` (`layer.cpp:396-400`), and `QCPAxis::rangeDidChange()` calls
`QCPAxisRect::markAffectedLayersDirty()` (`axis.cpp:629-637`), which walks the rect's axes, grids,
plottables and items and calls `markDirty()` on each distinct layer
(`layoutelement-axisrect.cpp:1518-1560`). `QCustomPlot::replot()` (`core.cpp:2417-2560`) then runs
`updateLayout()`, ensures at least one buffer is dirty, calls `setupPaintBuffers()`, resets the
RHI-layer geometry for buffers that will be skipped for a translation, redraws every layer whose
buffer is `contentDirty` (`layer->drawToPaintBuffer()`), clears both flags on all buffers, and calls
`update()` to schedule the frame. Nothing else runs between `markDirty()` and the next `replot()`:
a dirty layer is only repainted when some caller eventually calls `QCustomPlot::replot()`.

---

## 6. `replot()` counts outside `src/`

Counts are occurrences of the token `replot(` (`grep -rn "replot(" <dir>`); the second column is the
strict no-argument `replot()` call.

| Directory | `replot(` | `replot()` | files with `replot(` |
|---|---|---|---|
| `docs/snippets` (`.cpp`) | 101 | 99 | 15 |
| `docs/*.md` | 18 | 18 | — |
| `docs/plans`, `docs/specs`, `docs/superpowers` (`.md`) | 202 | — | — (plan/spec prose, not snippets) |
| `examples` (`.cpp`) | 14 | 14 | 5 |
| `tests/auto` (`.cpp`) | 687 | 287 | 41 |

Typical patterns:

1. Snippet after every property change: `docs/snippets/axes.cpp:28` — `customPlot->replot();` (same at `:56`, `:93`, `:136`, `:155`).
2. Example after a UI action: `examples/interactions/mainwindow.cpp:218` — `ui->customPlot->replot();` (also `:226`, `:90`, `:107`).
3. Test after building state, per assertion block: `tests/auto/test-qcpgraph/test-qcpgraph.cpp:187` — `mPlot->replot();` (repeated at `:190`, `:193`, `:196`).
4. Queued replot from a slot: `tests/auto/test-pipeline/test-pipeline.cpp` (109 occurrences, the heaviest file).
5. Replot inside a helper that updates data: `tests/auto/test-multigraph/test-multigraph.cpp` (73 occurrences), `tests/auto/test-data-swap/test-data-swap.cpp` (35).

---

## 7. Setters and repaint

### Setters that already replot (queue or immediately)

| Setter | Line | Kind |
|---|---|---|
| `QCPItemHSpan::setRange` | `items/item-hspan.cpp:46` | `rpQueuedReplot` |
| `QCPItemVSpan::setRange` | `items/item-vspan.cpp:46` | `rpQueuedReplot` |
| `QCPItemRSpan::setKeyRange` / `setValueRange` | `items/item-rspan.cpp:61`, `:74` | `rpQueuedReplot` |
| `QCPHistogram2D::setNormalization` | `plottables/plottable-histogram2d.cpp:158` (and via `redrawLater()` `:166,210,222,406`) | `rpQueuedReplot` |
| `QCPColorMap2` setters routed to `redrawLater()` | `plottables/plottable-colormap2.cpp:180,224,241` → `:249-255` | markDirty + `rpQueuedReplot` |
| `QCPIntervals` setters routed to `requestRepaint()` | `plottables/plottable-intervals.cpp:262` | markLayersDirty + `rpQueuedReplot` |
| `QCPHistogram` setters routed to `requestReplot()` | `plottables/plottable-histogram.cpp:293-298` | `rpQueuedReplot` |
| `QCPGroupLegendItem::setExpanded` | `layoutelements/layoutelement-legend-group.cpp:45` | immediate `replot()` |
| `QCPAbstractPlottable::setBusy` | `plottables/plottable.cpp:1127` → timer → `onDebounceTimeout` `:1161` | debounced `rpQueuedReplot` |
| `QCustomPlot::setCreationModeEnabled` | `core.cpp:1156` | `rpQueuedReplot` |
| `QCustomPlot::applyTheme` | `core.cpp:698` | `rpQueuedReplot` |

### Setters that mark the layer dirty but do not replot

| Setter | Line |
|---|---|
| `QCPAxis::setRange` (all overloads → `rangeDidChange`) | `axis/axis.cpp:647,721,753,767,789` → `:629-637` |
| `QCPAbstractLegendItem::setFont` / `setTextColor` / `setSelectedFont` / `setSelectedTextColor` | `layoutelements/layoutelement-legend.cpp:95,109,123,137` |
| `QCPColorScale` (colour changes via `mColorAxis`) | `layoutelements/layoutelement-colorscale.cpp:268` |
| `QCPLayerable::setVisible` (invalidates) | `layer.cpp:575-583` |
| `QCPAxisRect::takeAt`/`removeAt` side effects | `layoutelement-axisrect.cpp:501-512` |

### Setters that change appearance and mark nothing (sample of 15)

Verified by reading the bodies: plain member assignment, no `markDirty`, no `invalidatePaintBuffer`,
no `replot`, no `update`, no signal.

| Setter | Line |
|---|---|
| `QCPAxis::setTickLengthIn` | `axis/axis.cpp:1078` (via `setTickLength` `:1066`) |
| `QCPAxis::setTickLabelFont` | `axis/axis.cpp:894` |
| `QCPAxis::setLabel` | `axis/axis.cpp:1220` |
| `QCPAxis::setPadding` | `axis/axis.cpp:1262` |
| `QCPAxis::setOffset` | `axis/axis.cpp:1279` |
| `QCPGrid::setPen` | `axis/axis.cpp:111` |
| `QCPAbstractPlottable::setPen` | `plottables/plottable.cpp:520` |
| `QCPAbstractPlottable::setBrush` | `plottables/plottable.cpp:534` |
| `QCPAbstractPlottable::setAntialiasedFill` | `plottables/plottable.cpp:496` |
| `QCPAbstractPlottable::setName` | `plottables/plottable.cpp:485` |
| `QCPItemText::setColor` | `items/item-text.cpp:89` |
| `QCPItemText::setText` | `items/item-text.cpp:172` |
| `QCPItemText::setRotation` | `items/item-text.cpp:207` |
| `QCPItemLine::setPen` | `items/item-line.cpp:72` |
| `QCPLegend::setBorderPen` | `layoutelements/layoutelement-legend.cpp:536` |
| `QCPLegend::setIconSize` | `layoutelements/layoutelement-legend.cpp:591` |
| `QCPAxisRect::setBackground` | `layoutelements/layoutelement-axisrect.cpp:827` |
| `QCPColorScale::setGradient` | `layoutelements/layoutelement-colorscale.cpp:315` |
