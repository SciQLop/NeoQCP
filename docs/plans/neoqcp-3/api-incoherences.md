# NeoQCP API incoherences (evidence for NeoQCP 3.0)

Tree read: `/home/jeandet/Documents/prog/SciQLopPlots/subprojects/NeoQCP` (the NeoQCP.wrap pin SciQLopPlots builds).
NeoQCP paths are relative to its `src/`, except `docs/...`.
SciQLopPlots paths start with `SQP/` (= `/home/jeandet/Documents/prog/SciQLopPlots/`).
Read-only survey; nothing built or run. Ownership and lifetime are in `neoqcp_ownership_inventory.md`; use cases are in `neoqcp_use_cases.md`. They are only referenced here.

---

## Summary: the 10 incoherences most worth fixing in 3.0

Ranked by how much they hurt users.

1. **Drawing order inside a layer depends on the backend, not on the add order.** Each layer is painted as one QPainter texture. GPU colour maps, lines, scatters and spans are then drawn on top of it (`core.cpp:3018-3147`). So a text item added after a graph still ends up under the graph line. A solid pen takes the GPU path and a dashed pen does not (`plottables/plottable1d.h:569-590`), so changing the pen style changes the stacking. `docs/concepts.md:83` says objects draw in add order; `docs/items.md:273` says the opposite.
2. **New data is not visible for 200 to 1000 ms, and getters return the old data until then.** A second `setData` on `QCPGraph2` or `QCPMultiGraph` is staged, not applied (`plottables/plottable-graph2.cpp:111-120`). `dataSource()`, `dataCount()` and `getKeyRange()` keep reading the old source, so `setData(); rescaleAxes();` fits the previous data. `QCPColorMap2` swaps at once (`plottables/plottable-colormap2.cpp:149-168`). No signal says "committed", so SciQLopPlots polls `hasPendingData()` on every plottable after each replot (`SQP/src/SciQLopPlot.cpp:882-905`).
3. **Hit-testing has two rules, and the classes disagree on what `selectTest` returns.** If `details` is null, `QCPGraph2` and `QCPMultiGraph` report a hit anywhere in the axis rect (`plottables/plottable-graph2.cpp:378-379`, `plottables/plottable-multigraph.cpp:965-966`). `plottableAt()` passes null whenever no data index is asked for (`core.h:580-582`), so it returns those graphs for clicks far from the line. A span fill answers 0, a filled rect or ellipse answers 0.99×tolerance, and a bar of `QCPIntervals` answers 0. `plottableAt` picks the closest hit; event dispatch picks the topmost layer (`core.cpp:4373-4391`).
4. **There are four line classes, with no common interface between them.** `QCPGraph`, `QCPGraph2`, `QCPMultiGraph` and `QCPCurve` each have their own data model, `LineStyle` enum, gradient setter name, fill support and threading. SciQLopPlots has to `std::variant` over them to set a pen (`SQP/include/SciQLopPlots/Plotables/SciQLopGraphComponent.hpp:42-73`). `QCPMultiGraph` silently ignores the base `setPen`/`setBrush`.
5. **Nobody owns "this needs a repaint".** Base setters such as `setPen` do not mark the layer dirty (`plottables/plottable.cpp:509-526`). Some `QCPGraph2` setters do and others do not (`plottables/plottable-graph2.h:74-84`). Some library setters replot by themselves, either queued or immediately (`items/item-vspan.cpp:45`, `plottables/plottable-graph2.cpp:230`). `replot()` only guesses, when no buffer is dirty (`core.cpp:3997-4017`).
6. **Moving something to other axes is only half supported.** `setKeyAxis` checks nothing. It also keeps the old axis rect as the visibility parent, set once in the constructor (`plottables/plottable.cpp:437, 539-543`). Items have no parent axis rect at all. Each item position carries its own axes, axis rect and clip rect, so one item on a second axis rect needs 2 to 4 setter calls per position (`items/item.cpp:1077-1088, 1339-1351`). When its axes go away, a position silently falls to pixel 0 (`items/item.cpp:657-665`).
7. **Margin groups cannot align several plot widgets.** A group belongs to one plot and only equalises margin sizes (`layout.h:36-66`, `layout.cpp:130-146`). SciQLopPlots rebuilds alignment by hand. It hijacks `minimumMargins`, subtracts the colour-scale width and the column spacing, then replots every plot a second time (`SQP/src/VPlotsAlign.cpp:29-88`).
8. **The link between a colour scale and a colour map is hand-wired in each class.** Each colour-map class connects the same six signals itself, in a slightly different order (`plottables/plottable-colormap.cpp:896-926` vs `plottables/plottable-colormap2.cpp:184-209`). `QCPColorScale::colorMaps()` only knows the classic `QCPColorMap` (`layoutelements/layoutelement-colorscale.cpp:396-406`), while `rescaleDataRange` casts to three types. A `QCPMultiGraph` colour axis cannot link to a colour scale.
9. **Errors and threading rules are silent.** Failures are reported by `qDebug` about 308 times, with a `false` or `nullptr` return. Sizes are `int` and truncated without a check (`datasource/soa-datasource.h:34-37`). "GUI thread only" is a comment, never an assert (`datasource/async-pipeline.h:135`). Each plot owns a thread pool whose threads never expire (`core.cpp:527-531`, `datasource/pipeline-scheduler.cpp:15-20`).
10. **Naming and selection come in many models.** Selection exists in four forms: `QCP::SelectionType` with `QCPDataSelection`, a plain `bool`, `SelectableParts` flags, and per-component selections. Plottables have four legend-item classes. Signals are overloaded (`rangeChanged`, `selectionChanged`). Enum prefixes collide (`QCPIntervals::stBars` against `QCP::stNone`), and `hpNone` is -2 in one class and 0 in another. Theme setters on the plot write into a shared theme, and `applyTheme` overwrites colours the user set.

---

## 1. Drawing API and subclass contracts

### What a subclass must override

`QCPLayerable` has two pure virtuals: `applyDefaultAntialiasingHint` and `draw` (`layer.h:196-197`). It also has non-pure ones: `selectTest`, `selectEvent`, `deselectEvent`, `clipRect`, `selectionCategory`, and five mouse and wheel handlers (`layer.h:173, 193-207`).

A plottable author must also implement the pure `selectTest`, `getKeyRange`, `getValueRange` and the protected `drawLegendIcon` (`plottables/plottable.h:160-190, 272`). There is also a set of optional virtuals with an unwritten contract:
- `interface1D`
- `stallPixelOffset`, `canProduceContent` and `releaseGpuResources`, read by the layer and the plot (`layer.cpp:371-373`, `core.cpp:3208, 3930`)
- `dependentLayers` (`layoutelements/layoutelement-axisrect.cpp:1486`)
- `keyPress`
- `hasPendingData` and `commitPendingData`
- `pipelineBusy` and `axesChanged`

All of these sit at `plottables/plottable.h:165-184, 221-222, 249, 275`. The comment for `keyPress` is split in two by `dependentLayers` (`plottables/plottable.h:170-175`).

None of this is in the user docs. `docs/plottables.md` and `docs/concepts.md` have no section on subclassing a plottable or an item.

### Signatures that do not match

The mouse virtuals take different second arguments. `mousePressEvent` and `mouseDoubleClickEvent` get the hit `details`. `mouseMoveEvent` and `mouseReleaseEvent` get a `startPos` (`layer.h:203-206`).

`draw` is documented as `const = 0` but declared non-const (`layer.cpp:495` vs `layer.h:197`). Meanwhile the const helper `drawPolyline` has a side effect: it pushes geometry into the GPU layer (`plottables/plottable1d.h:92, 566-590`).

### QCPPainter

`QCPPainter` hides non-virtual `QPainter` methods: `setPen`, `drawLine`, `save`, `restore` and `begin` (`painting/painter.h:75-85`). Its workarounds are lost if code calls them through a `QPainter*`.

`isExporting()` is the only way to know whether GPU layers are allowed (`painting/painter.h:66-68`). Every GPU path has to check it by itself (`plottables/plottable1d.h:571`, `plottables/plottable-draw-utils.cpp:26`, `axis/axis.cpp:177-178, 2052-2053`).

### Five GPU paths, each wired differently

| GPU layer | Keyed by | Created by | Source |
|---|---|---|---|
| Plottable lines and fills | `QCPLayer*` | `plottableRhiLayer(layer)` | `core.cpp:1177-1187` |
| Scatters | `QCPLayer*` | `scatterRhiLayer(layer)` | `core.cpp:1189-1199` |
| Colour maps | one per plottable, registered in a set | `registerColormapRhiLayer` | `core.cpp:1201-1209`, `painting/colormap-renderer.cpp:237-257` |
| Spans | one for the whole plot, filtered by layer when drawn | `spanRhiLayer()` | `core.cpp:711-716, 3145-3147` |
| Grid lines and ticks | one for the whole plot, drawn on the layers **named** `"grid"` and `"axes"` | `gridRhiLayer()` | `core.cpp:718-723, 3149-3156` |

All of this plumbing is public API on `QCustomPlot` (`core.h:139-144, 229-236`).

Consequences:
- **The grid ignores its own layer.** With a GPU, `QCPGrid::draw` returns early (`axis/axis.cpp:176-180`). The lines are drawn on whichever layer is named `"grid"`. So `grid->setLayer(...)` changes nothing on screen, but it does change the export.
- **The GPU grid only checks `grid->visible()`** (`painting/grid-rhi-layer.cpp:242`). It never checks `realVisibility()`. The render pass does not check the layer's visibility either (`core.cpp:3150-3152`).
- **Order inside a layer is fixed by the render pass:** QPainter texture, then colour maps, then lines, then scatters, then spans (`core.cpp:3018-3147`). Logical layers share one texture (`core.cpp:3854-3887`). By default `main` is buffered and `axes` and `legend` share the next texture (`core.cpp:477-486`). So painted items on `main` sit under GPU lines on `main`. `docs/migrating.md:93` admits the split: solid lines go to the GPU, dashed lines, channel fills and scatters go through QPainter.
- **The busy fade is applied twice, in two ways.** The layer sets the painter opacity for busy plottables, through a `qobject_cast` special case in the generic layer code (`layer.cpp:212-220`). A GPU path must copy `painter->opacity()` into its vertex data by hand (`plottables/plottable1d.h:586`). A new plottable that forgets this does not fade.

### Layers

`QCPLayer::setVisible` only sets a flag (`layer.cpp:163-166`). `QCPLayerable::setVisible` has to invalidate the buffer, with a long comment explaining that marking it dirty is not enough (`layer.cpp:571-585`).

`QCPLayer::replot()` clears only the line and scatter GPU layers. It quietly becomes a full replot for logical layers, or when any buffer is invalidated (`layer.cpp:273-298`).

`setAntialiased` changes nothing until something else repaints (`layer.cpp:630-633`).

### selectTest semantics

| Class | No `details` | Inside a fill | Beyond the axis rect |
|---|---|---|---|
| `QCPGraph` | real distance | n/a | allowed with `iSelectPlottablesBeyondAxisRect` (`plottables/plottable-graph.cpp:401-402`) |
| `QCPGraph2`, `QCPMultiGraph` | 0.99×tol anywhere in the rect | n/a | never (`plottables/plottable-graph2.cpp:374-379`) |
| `QCPItemVSpan`/`HSpan` | — | 0 (`items/item-vspan.cpp:85-90`) | — |
| filled rect, ellipse | — | 0.99×tol (`items/item.cpp:1298-1301`, `items/item-ellipse.cpp:137-141`) | — |
| `QCPIntervals` | — | 0 on any bar (`plottables/plottable-intervals.cpp:338`) | — |

`details` carries a different type in each case:
- `QCPDataSelection` for plottables
- an `int` hit part for spans (`items/item-vspan.cpp:70-90`)
- `SelectablePart` for axes

`plottableAt` and `itemAt` take the minimum distance, in creation order. They ignore layers and `mouseTransparent` (`core.h:566-640`). `layerableListAt`, used for clicks and the wheel, takes the topmost layer first (`core.cpp:4369-4393`).

SciQLopPlots skips the core wheel dispatch completely and calls `axis->selectTest` itself (`SQP/src/SciQLopPlot.cpp:450-468`).

### Legend icons

`drawLegendIcon` is pure on plottables (`plottables/plottable.h:272`). But the default row is built in the base constructor, before the subclass exists (`core.cpp:4238-4239`, called from `plottables/plottable.cpp:454`). So `QCPMultiGraph` and `QCPIntervals` both remove that row and add their own (`plottables/plottable-multigraph.cpp:36-41`, `plottables/plottable-intervals.cpp:70-75`). Any other subclass with a custom row must copy that trick.

---

## 2. Core objects and their relations

| Relation | Who points to whom | Kept in sync? | What breaks |
|---|---|---|---|
| plot → axis rects | not stored; found by walking the layout tree on every call (`core.cpp:2066-2087`) | n/a | `axisRect(i)` order is not documented |
| axis rect → plottables | not stored; a scan of `mPlottables` (`layoutelements/layoutelement-axisrect.cpp:617-645`) | n/a | a null axis crashes it (ownership doc, top-10 #5) |
| plottable → axes | `QPointer` key and value axis (`plottables/plottable.h:256`) | one-way | `setKeyAxis` checks neither the plot nor the orientation (`plottables/plottable.cpp:539-543`; the constructor only logs, `:449-452`) |
| plottable → visibility parent | the axis rect, fixed in the constructor (`plottables/plottable.cpp:437`) | never updated | after a move, hiding the new axis rect does not hide the plottable, and hiding the old one does |
| plottable → clip | the intersection of the key-axis and value-axis rects (`plottables/plottable.cpp:975-981`) | derived | axes from two different rects give an empty clip: the plottable is invisible, with no warning |
| item → axes | every `QCPItemPosition` has its own axes and axis rect (`items/item.cpp:1339-1351`); the item has a separate clip rect (`items/item.cpp:1077-1088`) | no link between them | several calls per position to move an item; it has no visibility parent, unlike plottables |
| position → removed axis | `QPointer` | null | `pixelPosition` logs and leaves 0 (`items/item.cpp:657-665`) |
| legend item → plottable | raw `mPlottable` (`layoutelements/layoutelement-legend.h:128`) | one-way | see the ownership doc, top-10 #1 |
| legend lookup | `itemWithPlottable` finds only `QCPPlottableLegendItem` (`layoutelements/layoutelement-legend.cpp:769-780`) | — | multigraph group rows need a manual scan (`SQP/include/SciQLopPlots/Plotables/SciQLopGraphComponent.hpp:85-97`) |
| legend selection ↔ plottable selection | none in NeoQCP (`layoutelements/layoutelement-legend.cpp:171-230`) | no | the user wires it (use case 18) |
| colour scale ↔ colour map | six bidirectional connections, made by each map (`plottables/plottable-colormap.cpp:919-924`, `plottables/plottable-colormap2.cpp:208-213`) | two-way, but per class | `colorMaps()` lists only `QCPColorMap` (`layoutelements/layoutelement-colorscale.cpp:396-406`); `rescaleDataRange` casts to three classes (`:442-462`) |
| layer ↔ layerable | `QCPLayer::mChildren` and `mLayer` | two-way (`layer.cpp:753-775`) | the grid's GPU lines ignore it (section 1) |
| plottable ↔ multigraph components | the base `mSelection` is the union of the component selections (`plottables/plottable-multigraph.cpp:509-522`) | only upward | `setSelection` is not virtual (`plottables/plottable.h:156`, `plottables/plottable.cpp:581-589`), so a direct call never reaches the components |

### Selection takes four shapes

- plottables: `QCP::SelectionType` with `QCPDataSelection` (`plottables/plottable.h:139-157`)
- items: `bool` (`items/item.h:206-207`)
- axes and legend: `SelectableParts` flags (`axis/axis.h:357-358`)
- multigraph: one selection per component (`plottables/plottable-multigraph.h:166-167`)

`plottableClick(plottable, dataIndex, event)` cannot say which component was clicked (`core.h:361`). That information only comes from the group legend item's own signal (`layoutelements/layoutelement-legend-group.h:30`).

### Interaction flags

`QCP::Interaction` mixes three different things in one flag set (`global.h:230-262`):
- what is selectable (`iSelect*`)
- how selection works (`iMultiSelect`, `iSelectPlottablesBeyondAxisRect`)
- input styles (`iRangeDrag`, `iGestures`)

Drag and zoom also need `QCPAxisRect::setRangeDrag` and `setRangeZoom` (`layoutelements/layoutelement-axisrect.h:82-91`).

`deselectAll()` ignores the interaction flags (`core.cpp:2199-2204`). `deselectAllExcept` respects them (`core.cpp:2211-2217`).

---

## 3. Duplicated or parallel concepts

### Line plottables

| | `QCPGraph` | `QCPGraph2` | `QCPMultiGraph` | `QCPCurve` |
|---|---|---|---|---|
| data | `QSharedPointer<QCPGraphDataContainer>` (`plottables/plottable-graph.h:114, 127-139`) | `std::shared_ptr` source, raw getter (`plottables/plottable-graph2.h:31-33`) | multi-column source (`plottables/plottable-multigraph.h:47-49`) | container plus `t` (`plottables/plottable-curve.h:101-123`) |
| append (`addData`) | yes | no | no | yes |
| async resample | no | yes | yes | no; SciQLopPlots runs its own `QThread` (`SQP/src/SciQLopCurve.cpp:96-101`) |
| deferred swap | no | yes | yes | no |
| fill | yes, plus channel fill | none (`docs/graphs.md:156`) | per component (`plottables/plottable-multigraph.h:150-157`) | brush |
| pen | base `setPen` | base `setPen` | per component; the base pen is ignored (no `mPen` in `plottables/plottable-multigraph.cpp`) | base |
| gradient setter | — | `setScatterColorGradient` (`plottables/plottable-graph2.h:92`) | `setColorGradient` (`plottables/plottable-multigraph.h:158`) | — |
| gaps | NaN only | `setGapThreshold` | `setGapThreshold` | NaN only |
| `LineStyle` enum | `plottables/plottable-graph.h:90` | `plottables/plottable-graph2.h:17` (same values, separate type) | `plottables/plottable-multigraph.h:27` (again) | its own (`plottables/plottable-curve.h:89`) |
| `addGraph()` / `graph(i)` | yes (`core.h:288-295`) | no; `new` only | no | no |

**The new way, in practice:** SciQLopPlots draws every line as a `QCPMultiGraph`, even a single column. `QCPGraph2` appears only in a comment (`SQP/src/SciQLopPlot.cpp:457`). `docs/graphs.md:4-6` presents all three line classes as equal choices.

### Colour maps

`QCPColorMap` is a cell grid with `setCell`/`setSize`/`setRange`. It takes `setData(QCPColorMapData*, bool copy)` and paints with QPainter (`plottables/plottable-colormap.h:124-149`).

`QCPColorMap2` and `QCPHistogram2D` take data sources. They resample on worker threads, draw with a GPU quad and support contours (`plottables/plottable-colormap2.h:26-80`). SciQLopPlots uses only the new ones (0 files reference `QCPColorMap`).

The two families also differ in when new data shows up: the classic map applies it at once, and `QCPColorMap2` swaps the source at once but resamples asynchronously (section 6).

### Spans

`QCPItemVSpan`, `QCPItemHSpan` and `QCPItemRSpan` share a base class (`items/item-spanbase.h:10-54`). Each copies its own `HitPart` enum (`items/item-vspan.h:33`, `items/item-hspan.h:33`, `items/item-rspan.h:42`). Naming and behaviour also drift:
- "range" means the key range on a V span and the value range on an H span. The R span uses `keyRange` and `valueRange` (`items/item-rspan.h:25-26, 45-46`).
- The base pen is kept but never drawn (`items/item-spanbase.h:26-28`).
- The edge positions are public (`items/item-vspan.h:29-30`). Moving one with `setCoords` skips `rangeChanged`, which only `setRange` emits (`items/item-vspan.cpp:37-48`).

### Data containers vs data sources

The classic plottables expose a mutable, shared, sorted `QCPDataContainer`. The new ones take an `QCPAbstractDataSource` and hand back a raw `const` pointer.

There are six source families under `datasource/`: SoA, multi, row-major, 2D, resampled-multi and histogram. The only bridge between the two worlds is `QCPPlottableInterface1D` (`plottables/plottable-graph2.h:121-131`).

### Ranges, rescale and replot

Rescale functions take a `bool` that means something different on almost every class:
- `QCustomPlot::rescaleAxes(bool onlyVisiblePlottables)` (`core.h:326`)
- `QCPAxis::rescale(bool onlyVisiblePlottables)` (`axis/axis.h:378`)
- `QCPAbstractPlottable::rescaleAxes(bool onlyEnlarge) const` (`plottables/plottable.h:197`)
- `QCPColorScale::rescaleDataRange(bool onlyVisibleMaps)` (`layoutelements/layoutelement-colorscale.h:114`)
- `QCPColorMap::rescaleDataRange(bool recalculateDataBounds)` (`plottables/plottable-colormap.h:149`)

There are many ways to refresh. There is `replot()`, with a priority whose two middle values are deprecated aliases (`core.h:115-130`). There are `QCPLayer::replot()` and `markDirty()` (`layer.cpp:273-304`), and `phImmediateRefresh`, which does nothing (`global.h`, `PlottingHint`). On top of that come about 31 internal `replot` calls from setters in `items/`, `plottables/`, `axis/` and the axis rect. Most are queued, but `QCPGraph2::dataChanged` replots immediately (`plottables/plottable-graph2.cpp:230`).

### Theme vs per-object styling

`QCustomPlot` exposes `themeBackground` and the other theme colours as properties. They write into `mTheme` (`core.h:84-90`, `core.cpp:669-681`), so on a shared theme they change every plot that uses it.

`applyTheme` re-colours the axis, grid and legend pens. That overwrites colours the user set with `setLabelColor` and similar (`core.cpp:603-638`).

---

## 4. Naming and conventions

- **Enum styles mix:**
  - unscoped and prefixed (`lsLine`, `ssDisc`, `rpQueuedReplot`)
  - unscoped and unprefixed (`QCPOverlay::Info/Top`, `overlay.h:13-17`; `QCPPipelineScheduler::Fast/Heavy`, `datasource/pipeline-scheduler.h:15`; `QCPItemCreationState::Idle`, `item-creation-state.h:13`)
  - `enum class` (`datasource/abstract-datasource.h:30`, `datasource/async-pipeline.h:30`, `datasource/histogram-binner-1d.h:19`, `plottables/multigraph-fill.h:40`)
- **Prefixes collide.** `QCPIntervals::stBars` (`plottables/plottable-intervals.h:59`) uses the same `st` prefix as `QCP::stNone` (`global.h:311`). `hpNone` is -2 on spans (`items/item-vspan.h:33`) and 0 on intervals (`plottables/plottable-intervals.h:44`).
- **Signals are overloaded.** `QCPAxis::rangeChanged` (`axis/axis.h:396-397`) and `QCPAbstractPlottable::selectionChanged` (`plottables/plottable.h:241-242`) need `qOverload`. SciQLopPlots writes `QOverload<...>::of` 41 times.
- **Few properties notify.** About 37 `NOTIFY` for about 281 `Q_PROPERTY`. `QCPGraph2`, `QCPMultiGraph`, `QCPColorMap2` and the data sources declare no properties at all.
- **Getters are lopsided.** `busy()` returns the effective state, not the value given to `setBusy()` (`plottables/plottable.cpp:1096-1108`). `type()` and `parentAnchor()` return only the X half (`items/item.h:126, 132`).
- **`const` is wrong in places.** `removeFromLegend() const` changes the legend (`plottables/plottable.h:202-203`). `rescaleAxes() const` changes the axes (`plottables/plottable.h:197-199`). `rangeDragAxis()` is non-const (`layoutelements/layoutelement-axisrect.h:69-73`). `plottable(int)` is non-const while `graph(int)` is const (`core.h:276, 288`).
- **Access is by index almost everywhere:** `graph(i)`, `plottable(i)`, `item(i)`, `layer(i)`, `axisRect(i)`, `axis(type, i)`, `legend->item(i)`, and multigraph `component(i)`. A bad index logs and returns null (`core.cpp:1488-1499`). Component indexes shift when components change, yet SciQLopPlots stores them (`SQP/include/SciQLopPlots/Plotables/SciQLopGraphComponent.hpp:35-38`).
- **Return values differ:** `bool` from `setLayer`, `addLayer` and `addToLegend`; `void` from almost every setter. Some `remove*` are `[[nodiscard]]` while `clearPlottables` is not (`core.h:277-280`). Errors go out through `qDebug() << Q_FUNC_INFO` 308 times, `qWarning` 4 times, and almost never as exceptions.
- **Units are mixed in signatures.** `selectionTolerance` is an `int` of pixels, while distances are `double`. Hit tests turn `QPointF` into `QPoint` by truncation (`core.h:580`). `coordsToPixels` uses out-parameters (`plottables/plottable.h:193-196`).
- **Sizes are `int` throughout the data sources** (`datasource/abstract-datasource.h:86, 134-139`, `viewData(..., int count)` at `plottables/plottable-graph2.h:47`). `size()` is a `static_cast<int>` with no check (`datasource/soa-datasource.h:34-37`). There is no `qsizetype` anywhere in `plottables/` or `datasource/`.
- **Raw pointers carry ownership without saying so:** `setSelectionDecorator(new ...)`, `setSelectionRect`, `QCPColorMap::setData(ptr, copy)`, `addAxis(type, axis)`. Details are in the ownership doc.
- **Flags vs bools:** `setInteraction(flag, bool)` sits next to `setInteractions(flags)`. The same pattern is repeated for antialiasing and plotting hints (`core.h:190-203`).

---

## 5. Margin groups and alignment

### What QCPMarginGroup does

A margin group equalises one or more margin sides of several layout elements. It uses the largest automatic margin in the group (`layout.cpp:130-146`). It acts only during the `upMargins` phase of the layout that owns the element (`layout.cpp:487-512`), and only on sides set to automatic (`layout.cpp:430-436`).

### Where it stops

- **One plot only.** The group is created with a single plot as parent (`layout.cpp:78-85`). `setMarginGroup` checks nothing (`layout.cpp:438-466`), so nothing stops you adding another plot's element. But when that other plot's margin changes, nothing tells this plot to lay out again: a margin is only computed inside the owning plot's `replot`.
- **Sizes, not positions.** It equalises margin sizes, not where the inner rect starts or ends. Plots of different widths, or with a colour scale in an extra layout column, end at different x positions even with equal margins.
- **Colour scales count as regular elements.** Their default 6 px minimum margin spreads to the whole group. SciQLopPlots zeroes it before grouping the scale with the axis rect (`SQP/src/SciQLopPlot.cpp:137-143`).
- **No aspect ratio, no shared outer edge, no "align to the widest".**

### How SciQLopPlots works around it

`VPlotsAlign` is a collection behaviour attached to a panel. It works in four steps:

1. It reads each plot's `calculateAutoMargin(msLeft)` and keeps the largest (`SQP/src/VPlotsAlign.cpp:33-50`).
2. For the right edge, it computes `widget width − colour scale outer width − right auto margin`, and keeps the smallest (`:42-49`).
3. It writes the result into `setMinimumMargins` on each axis rect. It has to add back the colour-scale width or the layout `columnSpacing` by hand (`:61-77`).
4. It replots every plot again (`:80-87`).

It is triggered by y, y2 and z range changes, because tick labels change width, and by resize events. It is debounced by 16 ms (`:90-135`). So every rescale costs each plot a second replot, and the first frame shows the old margins.

The projection plot has its own workaround for equal aspect ratio. It forces `rpImmediateRefresh` to measure the automatic margins, switches margins to manual, writes them, then replots again (`SQP/src/SciQLopNDProjectionPlot.cpp:352-379`).

---

## 6. Threading and async

### Who runs worker threads

- **Each `QCustomPlot` owns a `QCPPipelineScheduler`** (`core.cpp:527-531`). It runs its own pool of `idealThreadCount()/2` threads, and they never expire (`datasource/pipeline-scheduler.cpp:9-20`). A shared-pool constructor exists, but it is used only on wasm (`datasource/pipeline-scheduler.h:19-23`). A panel of N plots can therefore hold N × cores/2 idle threads.
- **The async pipelines** (`datasource/async-pipeline.h:32-115`) serve `QCPGraph2`, `QCPMultiGraph`, `QCPColorMap2`, `QCPHistogram` and `QCPHistogram2D`. Results come back to the GUI thread through queued `invokeMethod` and a destroy guard (`datasource/async-pipeline.cpp:25-35`), then trigger a queued replot (`plottables/plottable-graph2.cpp:243-261`).
- **SciQLopPlots adds its own threads:** a `QThread` per `SciQLopCurve` (`SQP/src/SciQLopCurve.cpp:96-101`) and queued resamplers (`SQP/src/AbstractResampler.cpp:30-58`).

### Rules that are only comments

- "GUI-thread-only" is written on pipeline methods (`datasource/async-pipeline.h:135`), and nowhere is it checked. NeoQCP contains no `currentThread` check.
- SciQLopPlots adds the checks itself. Its `ThreadGuard` posts calls to the owner thread or raises (`SQP/include/SciQLopPlots/ThreadGuard.hpp:63-105`), and it asserts in `SQP/src/SciQLopGraphInterface.cpp:140`.
- The `busyChanged(bool)` argument cannot be trusted: slots must read `isBusy()` again (`datasource/async-pipeline.h:68-70`).

### When data shows up, and how each class follows the view

| Class | Source swap | Getters show new data | Follows the view |
|---|---|---|---|
| `QCPGraph`, `QCPCurve` | immediate, synchronous | at once | n/a |
| `QCPGraph2`, `QCPMultiGraph` | staged; committed after a debounce of 200 ms, at most 1000 ms (`core.h:246-258, 421-425`, `core.cpp:2275-2297`) | **after the commit** (`docs/migrating.md:99`) | key-axis range only, plus a 150 ms debounced redraw (`plottables/plottable-graph2.cpp:35-63`, `plottables/plottable-multigraph.cpp:50-77`) |
| `QCPColorMap2` | immediate (`plottables/plottable-colormap2.cpp:149-168`) | at once; the picture comes later | both axes, re-queued from `draw` (`plottables/plottable-colormap2.cpp:51-56, 296-303`) |

### Busy state

The busy state has three layers:
- `setBusy` stores an external flag.
- `busy()` adds that flag, `pipelineBusy()` and `hasPendingData()` together.
- `visuallyBusy()` is debounced by timers.

All three are in `plottables/plottable.cpp:1096-1137`. The layer reads `visuallyBusy()` to fade the plottable (`layer.cpp:214-220`).

### Export vs screen

Exports run the transform synchronously (`plottables/plottable-graph2.cpp:495`, `plottables/plottable-multigraph.cpp:1180`, `plottables/plottable-colormap2.cpp:355, 372`). So `toPixmap()` right after `setData` shows the new data, while the screen still shows the old data until the commit.

### No "data committed" signal

Nothing announces that staged data was committed. SciQLopPlots keeps a queue of pending fits, re-checks `hasPendingData()` on every plottable on each `afterReplot`, and only then rescales (`SQP/src/SciQLopPlot.cpp:882-905`).
