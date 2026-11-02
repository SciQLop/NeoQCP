# NeoQCP ownership inventory

## Which tree this is

The brief named `/home/jeandet/Documents/prog/NeoQCP` at HEAD 442f8eda.
That checkout is stale. It is on `fix/l2-raw-density` (2026-09-30), 106 commits behind `upstream/main`.
It has no `QCPPlottableGroup`, no `QCPIntervals`, no `QCPLaneLayout`, no `QCPHistogram`, and few of the fuzzing guards.

So this audit reads the tree SciQLopPlots actually builds and fuzzes:
`/home/jeandet/Documents/prog/SciQLopPlots/subprojects/NeoQCP/src` at e9daa0c (2026-10-31, the `NeoQCP.wrap` pin).
Every `file:line` below is relative to that `src/`. Read-only; nothing was built or run.

---

## 1. One-page summary

### The rules as they really are

1. **Everything you see is a `QCPLayerable`.** Its QObject parent starts as the plot (`layer.cpp:539-555`). A layout reparents its elements to itself when it adopts them (`layout.cpp:878-891`), and gives them back to the plot when it releases them (`layout.cpp:903-914`).
2. **Plottables and items register themselves.** Their base constructor adds them to the plot's lists (`plottable.cpp:454`, `item.cpp:1080`). There is no "add" call. They cannot move to another plot and cannot be detached.
3. **Most other things are created, then added.** Layout elements go in with `addElement`. Legend items go in with `addItem`. Axes are made by `QCPAxisRect::addAxis`. Groups and layers are made by plot factories (`addPlottableGroup`, `addLayer`, `addGraph`).
4. **Free-standing helpers are user-owned QObjects.** Margin groups, bars groups, lane layouts and external themes. You pick the parent. A lane layout defaults to no parent (`lane-layout.h:26`).
5. **Almost every `remove*` deletes.** The exceptions only detach: `take`/`takeAt`, `QCPBarsGroup::remove`, `QCPPlottableGroup::removeMember`. `removePlottableGroup` deletes the group but keeps its members. `setTheme` never deletes anything.
6. **Whether a plain `delete` is safe depends on the class.**
   - Safe: layout elements, legends, legend items, plottable groups, bars, bars groups, margin groups, selection rect, colour scales, item anchors. These unregister themselves or are held through `QPointer`.
   - Unsafe: plottables, items, axes, grids, layers, the plot layout, the overlay, the plot's own theme, polar graphs. The owner keeps a raw pointer and never hears about it.
7. **Data is shared, never borrowed safely.**
   - Old plottables share a mutable `QSharedPointer<QCPDataContainer>`.
   - New plottables take a `std::shared_ptr` or `std::unique_ptr` data source.
   - The `viewData` helpers wrap the caller's memory with no lifetime guard, and worker threads then read it.

### Top 10 problems, most likely to bite first

1. **`delete plottable` leaves three dangling pointers.** The plottable destructor only deletes its decorator (`plottable.cpp:461-467`). It stays in `mPlottables` and `mGraphs` (`core.h:384-388`). Its legend row keeps a raw `mPlottable` (`layoutelement-legend.h:128`) and draws through it (`layoutelement-legend.cpp:323-357`). Only `removePlottable` is safe (`core.cpp:1342-1369`). Python GC or `deleteLater` hits this directly.
2. **`delete item` has the same problem.** `~QCPAbstractItem` deletes its anchors but never leaves `mItems` (`item.cpp:1090-1094`). Later `itemAt`, `selectedItems`, `applyTheme` and `keyPressEvent` walk the dangling entry.
3. **`replot()` has no guard around its three signals.** It emits `beforeReplot` (`core.cpp:2338`), then `afterLayout` from inside `updateLayout` (`core.cpp:3787`), then `afterReplot`, and finally writes `mReplotting = false` (`core.cpp:2440-2441`). A slot that deletes the plot is a use-after-free. The mouse and selection handlers got `QPointer` guards; replot did not.
4. **`viewData` hands borrowed memory to worker threads.** `QCPGraph2`, `QCPMultiGraph`, `QCPColorMap2` and `QCPHistogram2D` wrap raw spans with no guard (`plottable-graph2.h:47-60`, `plottable-multigraph.h:72`, `plottable-colormap2.h:42-52`, `plottable-histogram2d.h:36-43`). The source already accepts a `dataGuard` (`soa-datasource.h:15`), but `viewData` never passes one. Free the buffer while a job runs, and the worker reads freed memory.
5. **Removing an axis rect leaves plottables behind with null axes.** Plottables track their axes only through a `QPointer` (`plottable.h:256`). Then `QCPAxisRect::plottables()` and `graphs()` call `keyAxis()->axisRect()` with no null check (`layoutelement-axisrect.cpp:617-645`). `processRectSelection` calls that, so a rubber-band selection crashes. `items()` does the same through `position->keyAxis()` (`layoutelement-axisrect.cpp:660-690`).
6. **`delete axis` leaves two dangling pointers.** `~QCPAxis` neither leaves its rect's `mAxes` nor clears `xAxis`/`yAxis` (`axis.cpp:486-494`, `layoutelement-axisrect.h:165`, `core.h:351`). Only `QCPAxisRect::removeAxis` does both (`layoutelement-axisrect.cpp:432-466`).
7. **`removeItem` deletes before it unlists.** It runs `delete item; mItems.removeOne(item)` (`core.cpp:1673-1686`). `removePlottable` does the opposite (`core.cpp:1358-1362`). A `destroyed()` slot that calls `removeItem` or `clearItems` therefore deletes the item twice. `QCPPolarAxisAngular::removeGraph` has the same order (`layoutelement-angularaxis.cpp:645-646`).
8. **`rescaleAxes()` walks raw pointers while emitting.** It loops over a raw axis snapshot (`core.cpp:2463-2470`). Each `rescale()` ends in `setRange` and so in `rangeChanged` (`axis.cpp:1494-1540`). A slot that deletes another axis, a rect or the plot leaves the next iteration on freed memory.
9. **Layers are fragile.**
   - `removeLayer` moves children over a raw snapshot, and each move emits `layerChanged` (`core.cpp:1943-1947`, `layer.cpp:753-775`).
   - `QCPIntervals` answers `layerChanged` by calling `addLayer`, in the middle of that removal (`plottable-intervals.cpp:86-89`, `46-63`).
   - A plain `delete layer` leaves the `mLayers` entry and the GPU-layer map key dangling, and `replot` dereferences that key (`core.cpp:2350`, `2370`).
10. **Legend items remember their first legend forever.** `mParentLegend` is a raw pointer set once in the constructor (`layoutelement-legend.h:87`, `layoutelement-legend.cpp:76-79`). `addItem` can move the item to another legend (`layoutelement-legend.cpp:834-837`, `layout.cpp:1239-1240`), but nothing updates `mParentLegend`. Delete the first legend and every select or draw through it is a use-after-free (`layoutelement-legend.cpp:187`, `213`, `292`, `331`).

Runners-up are covered in section 4: grid double delete, `QCPIntervals` without a lane layout, deleting the plot's own theme, and a span destructor that creates GPU state.

---

## 2. Per-object tables

Columns: **Created by** / **Owned by** / **Deleted by, when** / **User removal** / **Plain `delete`** / **Raw pointers kept elsewhere**.

### QCustomPlot
| | |
|---|---|
| Created by | User. The constructor builds 6 layers, the root grid, the default axis rect with 4 axes, the legend, the selection rect, an owned theme, the pipeline scheduler and the data-swap timer (`core.cpp:420-536`). |
| Owned by | Its parent `QWidget`. |
| Teardown order | The destructor deletes plottable and scatter RHI layers, then paint buffers, groups, plottables, items, the plot layout, the layers (`core.cpp:540-566`). Theme, scheduler, selection rect, margin groups, bars groups, orphan layout elements and polar graphs die later, as QObject children. |
| Not freed | `mSpanRhiLayer`, `mGridRhiLayer`, the composite pipeline, SRB, sampler and buffers are only freed in `releaseResources()` (`core.cpp:3197-3229`), never in the destructor. **Check:** whether Qt calls the override on normal destruction. Its docs say `releaseResources` is "normally not called" for a widget that stays in its hierarchy. |
| Raw pointers to it | `mParentPlot` in every layerable (`layer.h:186`), anchor (`item.h:52`), margin group, bars group, plottable group. |

### QCPLayer
| | |
|---|---|
| Created by | The plot constructor (`core.cpp:476-481`) and `addLayer` (`core.cpp:1880-1915`). Also implicitly by `overlay()` (`core.cpp:694-709`) and by every `QCPIntervals` constructor (`plottable-intervals.cpp:46-63`, `84`). Those intervals layers are never removed. |
| Owned by | QObject parent is the plot (`layer.cpp:125`). The plot also lists it in `mLayers` (raw). |
| Deleted by | `removeLayer` (`core.cpp:1923-1965`) and the plot destructor (`core.cpp:563-565`). |
| User removal | `removeLayer` deletes. Children move to the layer below. It refuses to remove the last layer. |
| Plain `delete` | Children are detached (`layer.cpp:146-147`). But `mLayers` keeps the pointer, and so do the `mPlottableRhiLayers` and `mScatterRhiLayers` keys (`core.h:445-446`). `replot` dereferences those keys (`core.cpp:2350`, `2370`). If it was the current layer, the code only warns (`layer.cpp:149-152`). |
| Raw pointers kept | `mCurrentLayer` (`core.h:399`); `QCPLayerable::mLayer` (`layer.h:188`, kept up to date); `QCPColormapRhiLayer::mLayer` (`colormap-rhi-layer.h:36-37`); span draw groups (`span-rhi-layer.h:24`, `61`). |
| Side effect | `replot()` silently moves the "notification" layer to the top (`core.cpp:2329-2336`). |

### QCPLayerable (base of everything drawn)
| | |
|---|---|
| Created by | Its constructor joins the plot's current layer, or the named one (`layer.cpp:539-555`). |
| Owned by | QObject parent is the plot, unless a subclass reparents it (layouts, axes, grids). |
| Plain `delete` | Leaves its layer (`layer.cpp:557-564`). It does **not** leave any plot-level list; that is up to each subclass. |
| Parent layerable | Held as a `QPointer` (`layer.h:187`), so it is safe. |

### QCPAxisRect
| | |
|---|---|
| Created by | User `new QCPAxisRect(plot)`, or the plot constructor. Not registered anywhere until it is put in a layout. |
| Owned by | The layout it sits in (QObject parent and `mParentLayout`), otherwise the plot. It owns its axes (`mAxes`, raw, `layoutelement-axisrect.h:165`) and its inset layout (raw, `layoutelement-axisrect.h:152`). |
| Deleted by | Its layout (`remove`/`clear`/destructor), or a plain `delete`. |
| Plain `delete` | Safe for the layout: the destructor calls `take` (`layout.cpp:258-265`). The destructor deletes the inset, then every axis through `removeAxis`, which tells the plot (`layoutelement-axisrect.cpp:221-240`). |
| Left behind | Plottables on those axes stay in the plot with null axes. See problem 5. |
| Discovery | `QCustomPlot::axisRects()` only finds rects inside `mPlotLayout` (`core.cpp:2066-2086`). A rect that was created but never added is invisible to `rescaleAxes` and `applyTheme`, but its axes stay on layers. |

### QCPAxis (and the `xAxis`/`yAxis`/`xAxis2`/`yAxis2` shortcuts)
| | |
|---|---|
| Created by | `QCPAxisRect::addAxis(type)`. It can also adopt an axis whose constructor named this rect (`layoutelement-axisrect.cpp:332-400`). |
| Owned by | QObject parent is the rect (`axis.cpp:458`). Also listed in the rect's `mAxes` (raw). The rect is fixed for life (`axis.h:406`); there is no way to move an axis. |
| Deleted by | `removeAxis` (`layoutelement-axisrect.cpp:432-466`), or the rect's destructor. |
| User removal | `removeAxis` deletes, and calls `QCustomPlot::axisRemoved` to clear the shortcuts (`core.cpp:4024-4035`). `addAxis` refills empty shortcuts on rect 0 (`layoutelement-axisrect.cpp:360-396`). |
| Plain `delete` | Leaves its rect's `mAxes` and the plot shortcuts dangling (`axis.cpp:486-494`). It does unregister from the grid GPU layer. |
| Other holders (all `QPointer`, all safe) | Plottables `mKeyAxis`/`mValueAxis` (`plottable.h:256`); item positions (`item.h:167`); range drag/zoom lists (`layoutelement-axisrect.h:154-155`); colour scale (`layoutelement-colorscale.h:134`). |
| Raw holders | `QCPGridRhiLayer::mAxes` and `mCachedTicks` (`grid-rhi-layer.h:91`, `130`), cleaned by `unregisterAxis`; the `xAxis` shortcuts (`core.h:351`). |
| Owns | `mGrid` (raw), `mAxisPainter` (raw new/delete), `mTicker` (`QSharedPointer`) (`axis.h:441-443`). |

### QCPAxisTicker
Held as a `QSharedPointer` (`axis.h:443`). It can be shared across axes on purpose (`axis.cpp:816-824`). `setTicker(nullptr)` is refused with a log message. The colour scale moves the ticker between its private axes (`layoutelement-colorscale.cpp:204`). Polar axes use the same type (`radialaxis.h:298`, `layoutelement-angularaxis.h:342`). This one is fine.

### QCPGrid
| | |
|---|---|
| Created by | The axis constructor (`axis.cpp:448`). Its QObject parent is the axis (`axis.cpp:69`). |
| Deleted by | `~QCPAxis` with a raw `delete mGrid` (`axis.cpp:492`). |
| Plain `delete` | `mGrid` dangles (`axis.h:441`). Every `axis->grid()` call and `applyTheme` (`core.cpp:623-626`) use it, and `~QCPAxis` then deletes it a **second** time. |

### QCPLegend
| | |
|---|---|
| Created by | The plot constructor puts it in the default rect's inset (`core.cpp:499-501`). Users can create more. |
| Owned by | The layout it sits in. The plot shortcut `legend` is raw (`core.h:352`). |
| Plain `delete` | Safe. The destructor clears its items and calls `legendRemoved` (`layoutelement-legend.cpp:482-490`, `core.cpp:4044-4048`). |
| Raw holders | `QCPAbstractLegendItem::mParentLegend` (problem 10). Groups read `mParentPlot->legend` live (`plottable-group.cpp:154`). |

### Legend items (QCPAbstractLegendItem, QCPPlottableLegendItem, group, intervals, polar)
| | |
|---|---|
| Created by | `addToLegend` (`plottable.cpp:867-890`), group `updateLegendRow` (`plottable-group.cpp:151-163`), or the user. The constructor does **not** add the item (`layoutelement-legend.cpp:72-79`). |
| Owned by | The legend (a `QCPLayoutGrid`); before that, the plot. |
| User removal | `QCPLegend::removeItem` and `clearItems` delete (`layoutelement-legend.cpp:852-899`). |
| Plain `delete` | Safe for the legend (layout `take`). The group's row is a `QPointer` (`plottable-group.h:55`). |
| Raw pointers in items | `mParentLegend` (`layoutelement-legend.h:87`); `QCPPlottableLegendItem::mPlottable` (`:128`); `QCPIntervalsLegendItem::mIntervals` (`layoutelement-legend-intervals.h:21`); `QCPPolarLegendItem::mPolarGraph` (`polargraph.h:57`). Only `QCPPlottableGroupLegendItem::mGroup` is a `QPointer` (`plottable-group.h:85`). |

### QCPLayout / QCPLayoutGrid / QCPLayoutInset / QCPLayoutElement
| | |
|---|---|
| Created by | User `new`; the element constructor does not register it (`layout.cpp:242-256`). The plot makes the root grid (`core.cpp:488-494`). |
| Owned by | The parent layout: QObject parent plus `mParentLayout` (raw, kept in sync, `layout.h:169`). The root grid's QObject parent is the plot (`core.cpp:490`). |
| Removal | `remove`/`removeAt`/`clear` delete (`layout.cpp:787-834`). `take`/`takeAt` only detach and hand the element back to the plot (`layout.cpp:1742-1778`, `2153-2189`). |
| Adding | `addElement` first takes the element out of its old layout (`layout.cpp:1239-1240`, `2231-2232`, `2258-2259`). There is no check that it belongs to the same plot, and no check for cycles (`layout.cpp:878-891`). |
| Plain `delete` | Safe: the destructor unregisters from its margin groups and parent layout (`layout.cpp:258-265`). **Except the root:** `delete plot->plotLayout()` leaves `mPlotLayout` dangling (`core.h:382`), and the next `updateLayout` dereferences it (`core.cpp:3782-3784`). |
| Margin groups | Held raw in a hash (`layout.h:175`), but both sides unregister (`layout.cpp:87-90`, `438-466`). |

### QCPMarginGroup
Made by the user with `new QCPMarginGroup(plot)`. Its QObject parent is the plot (`layout.cpp:78-85`). A plain `delete` is safe: `clear()` detaches every element (`layout.cpp:108-118`). The plot keeps no list of margin groups.

### QCPAbstractPlottable and its subclasses
| | |
|---|---|
| Created by | `new QCPGraph(keyAxis, valueAxis)` and so on. The base constructor dereferences `keyAxis` with no null check, so a null axis crashes (`plottable.cpp:438`). It calls `registerPlottable` (`plottable.cpp:454`), which may `addToLegend()`. That is a virtual call from inside the base constructor, so a subclass override does not run yet (`core.cpp:4238-4239`). `QCPIntervals` works around this (`plottable-intervals.cpp:72-77`). `QCPGraph` also calls `registerGraph` (`plottable-graph.cpp:191`). |
| Owned by | QObject parent is the plot. Listed raw in `mPlottables` (+ `mGraphs`). |
| Deleted by | `removePlottable`/`removeGraph`/`clearPlottables`, or the plot destructor. |
| User removal | `removePlottable` leaves the group, then the legend, then unlists, deletes, and signals the group last (`core.cpp:1342-1369`). There is no detach or take. |
| Plain `delete` | Unsafe (problem 1). |
| Axes | `setKeyAxis`/`setValueAxis` accept any axis, even one from another plot, with no check (`plottable.cpp:539-560`). Mismatched plots in the constructor only log (`plottable.cpp:449-452`). |
| Owns | Selection decorator (raw, replaced with delete, `plottable.cpp:~600-620`); busy timer (by value); its pipeline (by value, in the new plottables). |

Subclass notes:
- **QCPGraph.** `mChannelFillGraph` is a `QPointer` (`plottable-graph.h:154`). Data is a shared `QSharedPointer<QCPGraphDataContainer>` (`plottable-graph.cpp:219-222`).
- **QCPGraph2, QCPMultiGraph, QCPWaterfallGraph, QCPColorMap2, QCPHistogram, QCPHistogram2D.**
  - Each takes a `std::shared_ptr` or `std::unique_ptr` data source (`plottable-graph2.h:31-32`, `plottable-multigraph.h:47`, `plottable-colormap2.h:26`, `plottable-histogram2d.h:45-50`).
  - Each runs an async pipeline (below).
  - Each offers `viewData` (problem 4).
- **QCPCurve, QCPBars, QCPFinancial, QCPStatisticalBox, QCPErrorBars.** Data is a `QSharedPointer` container.
  - `QCPErrorBars::mDataPlottable` is a `QPointer` (`plottable-errorbar.h:145`).
  - The bar stack (`mBarBelow`/`mBarAbove`) uses `QPointer` (`plottable-bars.h:238`), and `~QCPBars` unstacks (`plottable-bars.cpp:535-540`).
- **QCPBarsGroup.** User-made, QObject parent is the plot (`plottable-bars.cpp:104-107`). It holds bars raw (`plottable-bars.h:97`), and each bar holds its group raw (`:235`). Both destructors unregister (`plottable-bars.cpp:109-112`, `161-166`, `535-537`). `remove` only detaches. There is no same-plot check (`:240-244`).
- **QCPColorMap.** `mColorScale` is a `QPointer` (`plottable-colormap.h:174`). `setColorScale` wires six connections, with guards (`plottable-colormap.cpp:896-926`).
- **QCPColorMap2 / QCPHistogram2D.** These go through `QCPColormapRenderer`, which keeps a `QPointer` to the scale and owns the colormap GPU layer raw. The renderer registers that layer with the plot (`colormap-renderer.h:92-93`, `colormap-renderer.cpp:31-34`, `244-259`).
- **QCPIntervals.**
  - Takes a user-owned `QCPLaneLayout` and keeps a `QPointer` to it (`plottable-intervals.h:234`).
  - Owns a hidden `LabelLayer` child layerable (`plottable-intervals.cpp:18-45`, `84`).
  - Creates a plot layer as a side effect (`:46-63`).
- **QCPLaneLayout.** A plain QObject; the default parent is none (`lane-layout.h:26`). Shared by several intervals. It tracks per-owner rows by `const QObject*` (`lane-layout.h:97`). `~QCPIntervals` removes its rows (`plottable-intervals.cpp:119-123`).
- **QCPPlottableGroup.**
  - Made only by `addPlottableGroup` (private constructor, `plottable-group.h:46`, `core.cpp:1399-1404`).
  - Owned by the plot: QObject parent plus `mPlottableGroups`.
  - `removePlottableGroup` deletes it (`core.cpp:1411-1417`). So does a plain `delete`, because the destructor unregisters itself (`plottable-group.cpp:53-60`).
  - Members are `QPointer`s (`plottable-group.h:54`), and each member holds its group as a `QPointer`.
  - Signals come last and are guarded (`plottable-group.cpp:80-117`).
  - This is the one fully consistent design in the codebase.
- **Polar (QCPPolarGraph).** This is **not** a plottable: it is a bare layerable, unknown to `mPlottables`.
  - Registered with its angular axis in its constructor (`polargraph.cpp:160`, `layoutelement-angularaxis.cpp:1777-1797`).
  - Listed raw in `mGraphs` (`layoutelement-angularaxis.h:341`).
  - `removeGraph` deletes it, then unlists it (`:634-648`).
  - `~QCPPolarGraph` does not unregister (`polargraph.cpp:169-178`). `~QCPPolarAxisAngular` deletes its radial axes but not its graphs (`:229-248`).
  - Radial axes are listed raw (`:339`) and deleted by `removeRadialAxis` (`:372-386`).

### QCPColorScale
A layout element. It owns a private `QCPColorScaleAxisRectPrivate`, a full axis rect with four axes, held by `QPointer` and deleted in the destructor (`layoutelement-colorscale.cpp:134`, `143-146`). That private rect's QObject parent is the plot, not the scale (`layoutelement-colorscale.cpp:612-616` → `layer.cpp:540`). It is not in the layout tree, so `axisRects()` does not list it. Colour maps find their scale through `QPointer`, and the scale finds its maps by scanning `mPlottables` (`:396-406`).

### QCPAbstractItem, items, QCPItemPosition / QCPItemAnchor
| | |
|---|---|
| Created by | `new QCPItemLine(plot)` and so on. The base constructor calls `registerItem` (`item.cpp:1080`) and clips to the first axis rect. Positions get `plot->xAxis`/`yAxis` at creation, possibly null (`item.cpp:1339-1351`). |
| Owned by | QObject parent is the plot. Listed raw in `mItems`. The item owns its anchors and positions through a raw list plus `qDeleteAll` (`item.cpp:1090-1094`, `item.h:198-199`). |
| User removal | `removeItem`/`clearItems` delete (`core.cpp:1673-1720`). There is no detach. |
| Plain `delete` | Unsafe (problem 2). |
| Parent anchors | Raw pointers in both directions (`item.h:170`, `57`), but both destructors unwind them (`item.cpp:83-99`, `267-293`). So deleting a parent item is safe. There is no same-plot check. |
| Axes and clip rect | `QPointer` (`item.h:167-168`, `231`). There is no same-plot check (`item.cpp:747-761`, `1122-1127`). |
| Span items | Their destructor calls `spanRhiLayer()`, the *creating* accessor (`item-spanbase.cpp:7-11`). `~QCPAxis` uses the non-creating `gridRhiLayerIfExists()` (`axis.cpp:489`). So clearing items, even inside the plot destructor, can create a span GPU layer that nothing frees. |

### QCPSelectionRect
Owned by the plot. The QObject parent is the plot, and `mSelectionRect` is a `QPointer` (`core.h:403`). `setSelectionRect` deletes the previous rect after cancelling it, guarded (`core.cpp:1067-1091`). It does not check that the new rect belongs to this plot. A plain `delete` is safe.

### Data containers and data sources
- **`QCPDataContainer<T>`.** Held as a `QSharedPointer` in `QCPAbstractPlottable1D` (`plottable1d.h:87`). `data()` hands out a mutable shared container, and two plottables may share one by design. Single-threaded.
- **New data sources.** Held as `std::shared_ptr<const ...>`. The pipeline copies the `shared_ptr` into each job (`async-pipeline.h:206-215`), so the source lives as long as any job needs it.
- **Borrowed buffers.** `QCPSoADataSource` can hold a `dataGuard` (`soa-datasource.h:15`, `126`). The `viewData` helpers do not use it.
- **Waterfall.** Wraps the source in a `shared_ptr` adapter (`plottable-waterfall.h:33`, `80-81`).
- **Type mix.** Two smart-pointer families sit side by side: Qt `QSharedPointer` in the old plottables, `std::shared_ptr` in the new ones.

### Colour gradients, scatter styles, selections, line endings
Value types: `Q_GADGET` or plain classes, copied (`colorgradient.h:32-34`, `scatterstyle.h:33`, `selection.h:31`, `82`). No ownership issues.

### QCPTheme
- The plot owns its default theme: `mOwnedTheme`, raw, with the plot as QObject parent (`core.cpp:523-525`).
- The active theme is a `QPointer` (`core.h:405`). `setTheme(nullptr)` falls back to the owned one (`core.cpp:573-583`).
- **Shared theme deleted:** `mTheme` becomes null. The plot does not fall back to its own theme; theming just stops.
- **Owned theme deleted:** `mOwnedTheme` dangles, and the next `setTheme(nullptr)` connects to freed memory (`core.cpp:575`, `585-596`).
- The `light()`/`dark()` factories return raw pointers with a caller-chosen parent (`theme.cpp:95-118`).

### QCPTextElement, QCPOverlay, item-creation state
- `QCPTextElement` is an ordinary layout element.
- `QCPOverlay` is created lazily by `overlay()` (`core.cpp:694-709`). The plot keeps a raw `mOverlay` (`core.h:451`), so a plain `delete` leaves it dangling.
- `QCPItemCreationState` is a QObject child of the plot. It holds the item being drawn through a `QPointer` and removes it with `removeItem` (`item-creation-state.h:36`, `item-creation-state.cpp:141-143`).

### Resamplers, workers and threads
- `QCPPipelineScheduler` is a QObject child of the plot, with its own `QThreadPool` (or the shared browser pool on WASM). Its destructor drops queued jobs and waits for running ones (`pipeline-scheduler.cpp:49-60`).
- Pipelines are members, by value, of each plottable, and keep a raw `mScheduler` (`async-pipeline.h:78`).
- Jobs capture their transforms by value. They deliver through `invokeMethod(self, ..., QueuedConnection)` under a shared `DestroyGuard` (`async-pipeline.h:78-86`, `209-235`, `async-pipeline.cpp:28-34`). That design is sound.
- What can still go wrong is borrowed memory (problem 4).

### Paint buffers and RHI layers
- **Paint buffers.** `QList<QSharedPointer<QCPAbstractPaintBuffer>>` in the plot (`core.h:408`) and a `QWeakPointer` in each layer (`layer.h:108`). This is the cleanest ownership in the codebase.
- **Plottable and scatter RHI layers.** `QMap<QCPLayer*, raw>`, created lazily (`core.cpp:1177-1199`), freed in `removeLayer`, `releaseResources` and the destructor.
- **Colormap RHI layers.** A raw set registered by their renderers (`core.cpp:1201-1209`).
- **Span and grid RHI layers.** Raw, lazy (`core.cpp:711-728`), freed only in `releaseResources`.
- **GPU-side caches.** These key on raw `QCPAxisRect*`/`QCPAxis*`/`QCPLayer*` (`grid-rhi-layer.h:89-131`, `span-rhi-layer.h:24`, `61`, `95`). A freed address can be reused by a new object, so these keys can alias.

---

## 3. The four lists

### 3.1 Inconsistencies

1. **Removal order differs.**
   - `removePlottable` unlists, then deletes (`core.cpp:1358-1362`).
   - `removeItem` deletes, then unlists (`core.cpp:1677-1678`).
   - `QCPPolarAxisAngular::removeGraph` also deletes, then unlists (`layoutelement-angularaxis.cpp:645-646`).
   - `removeRadialAxis` unlists, then deletes (`:376-377`).
2. **Self-unregistration differs.**
   - Destructors that leave their owner: `QCPPlottableGroup` (`plottable-group.cpp:55`), `QCPLayoutElement` (`layout.cpp:258-265`), `QCPLegend` (`layoutelement-legend.cpp:482-490`), `QCPBars` (`plottable-bars.cpp:535-540`), `QCPLayerable` from its layer (`layer.cpp:557-564`), anchors (`item.cpp:83-99`).
   - Destructors that do not: `QCPAbstractPlottable` (`plottable.cpp:461-467`), `QCPAbstractItem` (`item.cpp:1090-1094`), `QCPAxis` (`axis.cpp:486-494`), `QCPLayer` (`layer.cpp:138-153`), `QCPPolarGraph` (`polargraph.cpp:169-178`).
3. **Registration differs.**
   - Constructor self-registration: plottables, items, polar graphs (`plottable.cpp:454`, `item.cpp:1080`, `polargraph.cpp:160`).
   - Plot factory: groups, layers, `addGraph` (`core.cpp:1399`, `1880`, `1530`).
   - Create, then add: layout elements, legend items (`layout.cpp:1235`, `layoutelement-legend.cpp:834`).
   - Owner factory: axes (`layoutelement-axisrect.cpp:332`).
   - Nothing at all: margin groups, bars groups, lane layouts, external themes.
4. **Back-pointer types differ for the same job.** Legend rows: `QPointer` for groups (`plottable-group.h:85`), raw for plottables, intervals and polar graphs (`layoutelement-legend.h:128`, `layoutelement-legend-intervals.h:21`, `polargraph.h:57`). Plot lists of plottables, items and layers are raw. The group's member list is `QPointer` (`plottable-group.h:54`).
5. **Owned children are held raw in some places and by `QPointer` in others.**
   - `QPointer`: colour scale `mAxisRect` (`layoutelement-colorscale.h:133`), `mSelectionRect` (`core.h:403`).
   - Raw: axis `mGrid`/`mAxisPainter` (`axis.h:441-442`), rect `mInsetLayout` (`layoutelement-axisrect.h:152`), `mPlotLayout` (`core.h:382`), `mOverlay` (`core.h:451`), `mOwnedTheme` (`core.h:404`).
6. **QObject parent and logical owner disagree.**
   - The colour scale's private rect: QObject parent is the plot, deleted by the scale (`layoutelement-colorscale.cpp:612-616`, `145`).
   - The grid: QObject parent is the axis, logically drawn by the rect, deleted raw by the axis.
   - Intervals' `LabelLayer`: QObject parent is the intervals, but it sits on a plot layer (`plottable-intervals.cpp:21-27`).
7. **Two smart-pointer families.**
   - Qt `QSharedPointer`/`QWeakPointer`: data containers, tickers, paint buffers.
   - `std::shared_ptr`/`std::unique_ptr`: data sources, pipelines, colour values.
8. **Two "add data" styles.** Old plottables take `setData(QSharedPointer)`, shared and mutable. New ones take `setDataSource(unique_ptr | shared_ptr)` and `viewData` (borrowed).
9. **GPU-layer accessors at teardown.** `~QCPAxis` uses the non-creating `gridRhiLayerIfExists()` (`axis.cpp:489`). `~QCPAbstractSpanItem` uses the creating `spanRhiLayer()` (`item-spanbase.cpp:9-10`).
10. **Cross-plot checks are uneven.**
    - Checked: `addGraph`, `registerPlottable`, `registerItem`, `addToLegend`, `QCPPlottableGroup::addMember`, `moveToLayer` (`core.cpp:1543-1548`, `4229-4234`, `4289-4294`, `plottable.cpp:887-892`, `plottable-group.cpp:80`, `layer.cpp:761-765`).
    - Not checked: `setKeyAxis`/`setValueAxis`, `QCPItemPosition::setAxes`/`setAxisRect`, `setClipAxisRect`, layout `adoptElement`, `setSelectionRect`, `QCPBarsGroup::append`, `setParentAnchor`.
11. **The plot's `axisRemoved`/`legendRemoved` hooks are virtual** (`core.h:498-499`), and they are called from destructors (`layoutelement-axisrect.cpp:449-452`, `layoutelement-legend.cpp:485-489`). Legacy code guards those calls with `qobject_cast<QCustomPlot*>` to dodge a half-destroyed plot.
12. **Copy policy is uneven.** QObject classes and anchors use `Q_DISABLE_COPY`. Non-QObject owners do not: `QCPColormapRenderer` holds a raw `mRhiLayer` it deletes (`colormap-renderer.h:18-93`), and `QCPLayoutElement`'s margin-group hash is raw. Nothing prevents copying a renderer, which would double-delete. `QCPLaneLayout::Batch` deletes its copies properly (`lane-layout.h:34-36`).

### 3.2 Misuse traps (compile, run, then crash, leak or no-op)

1. **`delete plottable`** and **`delete item`** (also `deleteLater`, Python GC): dangling list entries and legend rows (problems 1 and 2).
2. **`delete axis`**: dangling in `mAxes` and the shortcuts (problem 6).
3. **`delete axis->grid()`**: double delete in `~QCPAxis` (`axis.cpp:492`).
4. **`delete layer`** instead of `removeLayer`: dangling `mLayers` entry and GPU map keys (`core.cpp:2350`, `2370`), and possibly a dangling current layer (`layer.cpp:149-152`).
5. **`delete plot->plotLayout()`**: dangling `mPlotLayout`. Crashes on the next replot (`core.cpp:3782`).
6. **`delete plot->overlay()`**: dangling `mOverlay` (`core.cpp:696`, `2329`).
7. **`delete plot->theme()` while the plot still uses its own theme**: dangling `mOwnedTheme` (`core.cpp:575`).
8. **Removing an axis rect (or its axes) while plottables use it.**
   - The plottables become invisible, silently.
   - Any later `QCPAxisRect::plottables()`/`graphs()`/`items()` on *another* rect crashes on the null axis (`layoutelement-axisrect.cpp:624`, `644`, `675`). Rubber-band selection calls it.
   - Items created after `xAxis` was removed get null axes. They hit the same crash.
9. **Passing a null axis to any plottable constructor** crashes (`plottable.cpp:438`). Only `addGraph` checks (`core.cpp:1536-1542`).
10. **Moving a legend item to a second legend** leaves `mParentLegend` on the first (problem 10).
11. **Adding a layout element from another plot.** `adoptElement` keeps the foreign `mParentPlot` (`layout.cpp:884-885`). The element then draws with the other plot's layers.
12. **Adding a layout into itself or into its own child.** There is no cycle check (`layout.cpp:878-891`, `1235-1250`).
13. **`setKeyAxis` with an axis from another plot.** Accepted (`plottable.cpp:539-543`). The plottable stays registered in plot A and maps through plot B.
14. **A `QCPIntervals` with a null lane layout, or one whose layout was deleted.**
    - The constructor connects to null (`plottable-intervals.cpp:78`).
    - `draw` is guarded (`:903`).
    - `regroupIfLanesWereAdded`, `rowBand`, `pixelOf`, `rowsInRect`, `snapCandidatesExcluding`, `laneStepsTo` and `withoutOverlaps` dereference `mLayout` unchecked (`:127`, `167`, `269`, `409`, `566`, `616-619`, `837`).
    - A lane layout created with no parent leaks unless the user parents it (`lane-layout.h:26`).
15. **Every `QCPIntervals` adds a "`<layer>.intervals-labels`" layer** that is never removed (`plottable-intervals.cpp:46-63`). Create and delete them in a loop and layers pile up. This leak was inferred from the code, not observed.
16. **`viewData` over a temporary buffer**, or a numpy array the caller then drops: the worker thread reads freed memory (problem 4).
17. **An axis rect created but never added to a layout.** Its axes still sit on layers. `axisRects()`, `rescaleAxes` and `applyTheme` skip it without a word (`core.cpp:2066-2086`).
18. **Shared mutable data.** `graph->data()` returns a container that can be shared with other graphs (`plottable-graph.cpp:208-222`). Mutating it changes every graph that shares it. This is by design, but nothing in the type says so.
19. **Theme shared between plots, then deleted.** Each plot quietly stops theming instead of falling back to its own theme (`core.cpp:598-601`).

### 3.3 Re-entrancy hazards still open

Guards already exist in mouse, wheel, touch and selection handling, `commitPendingData`, `keyPressEvent`, groups, axis range setters, colour maps, pipelines, the selection rect and legend selection. These spots are not covered:

1. **`QCustomPlot::replot`.**
   - `beforeReplot` (`core.cpp:2338`), then `updateLayout` (which emits `afterLayout`, `:3787`), then layer loops over live `mLayers` and the RHI maps.
   - `afterReplot` (`:2440`), then the write `mReplotting = false`.
   - There is no alive guard, and the lists are not snapshotted after the emits. A slot that deletes the plot is a use-after-free.
2. **`QCustomPlot::draw`, used by export.** It emits `afterLayout` through `updateLayout` (`core.cpp:3750`), then loops `for (QCPLayer* layer : mLayers)` (`:3756`).
3. **`QCustomPlot::rescaleAxes`.** Raw axis snapshot, and each `rescale()` emits `rangeChanged` (`core.cpp:2463-2470`).
4. **`QCustomPlot::removeLayer`.** Raw `children` snapshot, and each `moveToLayer` emits `layerChanged` (`core.cpp:1943-1947`, `layer.cpp:770-774`). The intervals slot calls `addLayer` from inside (`plottable-intervals.cpp:86-89`).
5. **`QCPLayer::~QCPLayer`.** It emits `layerChanged(nullptr)` once per child, during the destructor (`layer.cpp:146-147`).
6. **`removeItem`.** `destroyed()` fires while the item is still in `mItems` (`core.cpp:1677`). Re-entering `removeItem`/`clearItems` from it deletes twice.
7. **`QCPAxisRect::~QCPAxisRect`.** It loops over `axes()` and calls `removeAxis`, which calls the virtual `axisRemoved` and then `delete axis`, which emits `destroyed` (`layoutelement-axisrect.cpp:241-242`, `452-453`). The loop's snapshot is raw. A slot that deletes another axis of this rect is a use-after-free.
8. **`QCustomPlot::applyTheme`.** It loops raw over `axisRects()`, the layout tree and `mItems`, calling setters (`core.cpp:598-666`). Today those setters do not emit, so this is safe only by luck. It runs from a queued timer.
9. **`QCPLegend::clearItems` and `QCPLayout::clear`.** They walk by index while each delete emits `destroyed` (`layoutelement-legend.cpp:888-899`, `layout.cpp:826-834`). A slot that adds or removes elements can shift the indexes.
10. **`QCPPlottableGroup::~QCPPlottableGroup`.** It removes its legend row, then loops `members()` and calls `addToLegend` for each (`plottable-group.cpp:53-60`). That changes the legend's layout during the loop. Low risk, since the snapshot is a fresh list of live pointers.
11. **`QCPAbstractPlottable` constructor.** It registers itself and adds to the legend before the subclass exists (`plottable.cpp:454`). Any slot or virtual call reached from there sees a half-built object.

### 3.4 Where modern Qt / C++ facilities would fit

Each item names a concrete spot.

- **`QObject::destroyed` or a `QPointer` list for plot-level registries.** `mPlottables`, `mGraphs`, `mItems` and `mLayers` (`core.h:384-389`). Unregistering in the destructors would also work. Either removes problems 1, 2 and 9. The precedent is `QCPPlottableGroup::mMembers` (`plottable-group.h:54`).
- **`QPointer` for legend back-pointers.** `QCPPlottableLegendItem::mPlottable`, `QCPIntervalsLegendItem::mIntervals`, `QCPPolarLegendItem::mPolarGraph`, `QCPAbstractLegendItem::mParentLegend` (and update the last one in `addItem`).
- **`std::unique_ptr` for strictly owned children.** `QCPAxis::mAxisPainter` and `mGrid` (`axis.h:441-442`); `mSelectionDecorator` (`plottable.cpp:613-628`); `QCPColormapRenderer::mRhiLayer`; `mSpanRhiLayer`/`mGridRhiLayer`; `mPlottableRhiLayers` values (`core.h:445-449`). This also fixes the destructor leak and the copy hazard. Or keep raw pointers and add a `QObject::destroyed` hookup for the ones the user can reach (`grid()`).
- **"add" taking ownership explicitly.** Plottables and items could stop self-registering. A `plot->add(std::unique_ptr<T>)`, or a factory `plot->addPlottable<T>(axes...)` returning `T*` and owning it, would also fix the virtual-in-constructor `addToLegend` (`core.cpp:4238-4239`). `addPlottableGroup` is already in that shape (`core.cpp:1399`).
- **A `take()` symmetric to `remove()` for plottables and items.** It would return ownership (`std::unique_ptr` or QObject reparent) for detach-and-reuse, matching `QCPLayout::take` (`layout.cpp:1760`).
- **`QPointer` alive guards plus `QScopeGuard`.** For the unguarded emit sites in `replot`/`draw` (`core.cpp:2338`, `2440-2441`, `3787`). A `QScopeGuard` would reset `mReplotting` only `if (alive)`.
- **Guarded snapshots.** `rescaleAxes` (`core.cpp:2465-2470`), `removeLayer` children (`core.cpp:1943`), `~QCPAxisRect` axes (`layoutelement-axisrect.cpp:241`). The file-local `guarded()` helper already exists (`core.cpp:66-80`); lifting it to a shared header would let other files use it.
- **`QSignalBlocker`** instead of manual `blockSignals(true/false)` in `QCPTheme::dark` (`theme.cpp:104-116`).
- **`Qt::QueuedConnection` or `deleteLater` for teardown-style signals.** For `QCPLayer::~QCPLayer`'s per-child `layerChanged` and the intervals layer-creation slot (`plottable-intervals.cpp:86-89`), where a slot changes plot structure.
- **`std::shared_ptr<const void>` lifetime token in `viewData`.** Every `viewData` overload should take a `dataGuard` (or require one) and forward it to `QCPSoADataSource` (`soa-datasource.h:15`). Spans are fine as views only while something keeps the buffer alive.
- **One smart-pointer family.** Data containers and tickers use `QSharedPointer`, data sources use `std::shared_ptr`. Picking one, or `std::span` views plus `std::shared_ptr` owners, would make the "add data" API uniform (`plottable1d.h:87` vs `plottable-graph2.h:31-32`).
- **Value types for config.** Gradients, scatter styles, selections and line endings are already values. Theme colours could be a value struct applied by assignment, without a `QObject`, a `QPointer` and an owned fallback (`core.h:404-405`).
- **Same-plot checks at every setter that takes a sibling object.** `setKeyAxis`, `setAxes`, `setClipAxisRect`, `adoptElement`, `setSelectionRect`, `QCPBarsGroup::append`. A single `Q_ASSERT`-style helper would do.
- **`QBindable` / `Q_OBJECT_BINDABLE_PROPERTY`.** Not needed for ownership. The closest fit is the plot's convenience shortcuts (`xAxis`… `core.h:351-352`): today they are public raw fields patched by `axisRemoved`/`addAxis`. Getters backed by `QPointer` would be simpler than a bindable.
