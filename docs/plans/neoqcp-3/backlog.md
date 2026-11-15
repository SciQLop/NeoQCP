# NeoQCP 3.0 — backlog dispositions

Every item deferred during the 3.0 milestones, triaged in milestone 5f. One line per item.

Outcomes:

- **fixed** — the commit(s) that fixed it, each with a failing test first.
- **project 3 / project 4** — scheduled for that project.
- **limit** — a known 3.0 limit, written down for users in `docs/limits.md` (the anchor names the section).
- **dev note** — a limit no user meets (bench, test or code-shape note). It stays here only.
- **stale** — already fixed, or no longer true of the code.

`#` is the row of the triage table the item came from (`a`: this file's milestone 4b-5e lists; `b`: the
deferred lists of the milestone 1, 2, item-scaling, 3 and 4a rulings files). Paths are under `src/` unless they
say otherwise.

## Milestones 4b to 5e

| # | From | Item | Outcome | Where |
|---|---|---|---|---|
| a1 | 5a | Data-space hit test maps every row per click/hover | limit → [limits.md#performance](../../limits.md#performance) | performance.md |
| a2 | 5a | No dense error band on a user system | limit → [limits.md#coordinate-systems](../../limits.md#coordinate-systems) | coordinate-systems.md |
| a3 | 5a | Cartesian fill to zero on negative log range uses the far bound (B1) | stale | `plottables/plottable-graph.cpp` |
| a4 | 5a | Tracer cannot follow a curve or a polar graph | limit → [limits.md#items](../../limits.md#items) | items.md |
| a5 | 5a | Qt QThreadPool noise in TSan runs | dev note | - |
| a6 | 4j | No culling/decimation on a user system | limit → [limits.md#performance](../../limits.md#performance) | performance.md |
| a7 | 4j | Bracket decorator draws nothing on a user system | fixed ca11384 | `selectiondecorator-bracket.cpp` |
| a8 | 4j | Tracer on a polar graph | limit → [limits.md#items](../../limits.md#items) | items.md |
| a9 | 4j | `area->add<Item>()` missing | project 4 | additive API, J-8 |
| a10 | 4j | Position whose frame went away is inert without a report | stale | `drawable.cpp` |
| a11 | 4j | Curve colour scale does not follow the rect on a user system | stale | `plottables/plottable-colormapped.cpp` |
| a12 | 4j | `QCPDrawn_*` A/B rows bimodal under Xvfb | dev note | - |
| a13 | 4h | `QCPPolarArea::add<T>` lacks the friendly static_assert, builds before refusing, needs a polar ctor | project 4 | `construct.h` |
| a14 | 4h | `QCPDrawnCore::hitTest` has no box cull for non-cartesian frames | limit → [limits.md#performance](../../limits.md#performance) | performance.md |
| a15 | 4g | `QCPMultiGraph` requires `capColourScale` | stale | `plottables/plottable-graph.h` |
| a16 | 4g | Polar rubber-band selects through the disc; rescaleAxes moves polar radial axes | limit → [limits.md#polar-plots](../../limits.md#polar-plots) | polar.md |
| a17 | 4g | Listener connected+disconnected within one setter takes the direct path | dev note | `axis/axis-base.cpp` |
| a18 | 4g | `defersEmissions()` O(systems of the area) | dev note | `layoutelements/area.cpp` |
| a19 | 4g | Item position resolved outside drawing needs the `report` flag | dev note | `painting/draw-commands.h` |
| a20 | 4g | Colour-scale relationship check (map using another rect's scale) | limit → [limits.md#colormaps](../../limits.md#colormaps) | colormaps.md |
| a21 | 4f | Polar tick-label hit boxes are recomputed, not the drawn rect | fixed 7505159 | `polar/angularaxis.cpp` |
| a22 | 4f | `QCPCustomMapping::inverse` is pure; polar inverse modulo; wrong-sign log radius is NaN | limit → [limits.md#coordinate-systems](../../limits.md#coordinate-systems) | coordinate-systems.md |
| a23 | 4f | Axis hot block may straddle a cache line | dev note | - |
| a24 | 4f | `QCPAxis_SetRange10k` +5 % (gate 15 %) | dev note | - |
| a25 | 4f | `pixelAnchoredShapeIsHittable` failed once | dev note | - |
| a26 | 4e | A recording made again allocates fresh buffers | limit → [limits.md#performance](../../limits.md#performance) | performance.md |
| a27 | 4e | Per-item replay overhead | limit → [limits.md#performance](../../limits.md#performance) | performance.md |
| a28 | 4e | Span edge in pixels / on an anchor re-records every frame | limit → [limits.md#performance](../../limits.md#performance) | performance.md |
| a29 | 4e | `QCPLineEnding::draw` builds a QPainterPath per call | dev note | - |
| a30 | 4e | Unclipped tracer crosshair spans the axis rect, not the widget | limit → [limits.md#items](../../limits.md#items) | items.md |
| a31 | 4e | Rect/ellipse keep a positions-based `dataExtent` | dev note | `items/item-rect.cpp` |
| a32 | 4e | Layer index pads an error-bar command by its pen only | fixed 45dfc64 | `drawable.cpp`, `plottables/plottable-errorbar.cpp` |
| a33 | 4e | Two-colour OHLC strokes grouped per colour | limit → [limits.md#other-plottables](../../limits.md#other-plottables) | plottables.md |
| a34 | 4e | Painter-only plottable records an empty list when hit-tested; a subclass that forgets `draw(QCPDrawContext&)` draws nothing silently | fixed b1d7e69, a8f8b6d | `plottables/plottable.h` |
| a35 | 4d | Log linked axis re-clamped late | fixed 239fd90 | `axis/axis-base.cpp` |
| a36 | 4d | Colour scale asks its rect for drag/zoom axes twice | dev note | - |
| a37 | 4d | Alignment group hears the plot's own move, not an ancestor's | fixed c0e1195, cc92479 | `core.cpp` |
| a38 | 4c | Visible colour bar costs ~0.07 ms per pan frame | limit → [limits.md#performance](../../limits.md#performance) | performance.md |
| a39 | 4c | Contour labels are a raster tile in PDF/SVG | limit → [limits.md#export](../../limits.md#export) | export.md |
| a40 | 4c | Images in a recording never culled by the view | limit → [limits.md#performance](../../limits.md#performance) | performance.md |
| a41 | 4c | A 2.x program shows a colour bar it never asked for | limit → [limits.md#migrating-from-qcustomplot](../../limits.md#migrating-from-qcustomplot) | migrating.md |
| a42 | 4c | Colour-map legend icon stretches to the icon rect | limit → [limits.md#colormaps](../../limits.md#colormaps) | colormaps.md |
| a43 | 4c | Export reference compare skips outer row/column | dev note | - |
| a44 | 4c | Zero-height histogram bin draws no base hairline | limit → [limits.md#other-plottables](../../limits.md#other-plottables) | plottables.md |
| a45 | 4c | Step histogram legend icon is a box, not a line | limit → [limits.md#other-plottables](../../limits.md#other-plottables) | plottables.md |
| a46 | 4c | Curve given data of another size keeps colour values: bar shown, curve uncoloured | fixed d9192c5, 3596de6 | `plottables/plottable-curve.cpp` |
| a47 | 4c | Moving a plottable to another rect's axes keeps the old rect's scale | fixed 61640ed, ce2817b, 7e6edd6 | `plottables/plottable-colormapped.cpp` |
| a48 | 4c | 1000-text zoom 5 % slower on llvmpipe | dev note | - |
| a49 | 5b | Handle hash changes when its object dies | limit → [limits.md#api-reference](../../limits.md#api-reference) | api.md |
| a50 | 5b | `Q_FUNC_INFO` strings show the handle type | dev note | - |
| a51 | 5b | `QCPArea::allAxesRaw()` new protected virtual | limit → [limits.md#coordinate-systems](../../limits.md#coordinate-systems) | coordinate-systems.md |
| a52 | 5b | Graph family still raw | stale | `core.h` |
| a53 | 5b | Margin groups still raw | stale | `alignment-group.h` |
| a54 | 5b | A few render paths build a handle per call | dev note | - |
| a55 | 5b | `QCPLayoutElement::layout()` builds a handle per call | dev note | `layout.cpp` |
| a56 | 5b | `QCPItems_RestyleAll10k` noisy | dev note | - |
| a57 | 5b | `QCPAxisRect::add<T>` ignores an axis as first data argument | dev note | `layoutelements/layoutelement-axisrect.h` |
| a58 | 5b | `QPointer<T> p = handle` does not compile | limit → [limits.md#api-reference](../../limits.md#api-reference) | api.md |
| a59 | 4b | No test that marker colours reach the GPU | dev note | - |
| a60 | 4b | Error-bar caps lopsided on a log key axis | limit → [limits.md#line-and-scatter-graphs](../../limits.md#line-and-scatter-graphs) | graphs.md |
| a61 | 4b | Gap inside a filled curve puts a NaN vertex in the fill polygon | fixed d0a4a10 | `plottables/plottable-curve.cpp` |
| a62 | 4b | Clicking near a curve maps every point | limit → [limits.md#performance](../../limits.md#performance) | performance.md |
| a63 | 4b | 1M multigraph zoom 0.69 ms vs 0.61 limit | limit → [limits.md#performance](../../limits.md#performance) | performance.md |
| a64 | 4b | Big graph blank edges during a pan until the 150 ms settle | limit → [limits.md#performance](../../limits.md#performance) | performance.md |
| a65 | 4b | Data sources still carry the old pixel line API | stale | - |
| a66 | 5e | No `BINDABLE` storage | limit → [limits.md#api-reference](../../limits.md#api-reference) | api.md |
| a67 | 5e | Stretch-factor bursts on legend add/remove | fixed 8b0ce9e | `layout.cpp` |
| a68 | 5e | Layout-sensitive bench rows | dev note | - |
| a69 | 5e | A setter's scope costs something when nothing listens | dev note | - |
| a70 | 5e | No bench row adds/removes plottables or touches legend/theme | dev note | - |
| a71 | 5e | `QCPHitAction` not registered | project 4 | `core.h` |
| a72 | 5d | Financial/error-bar legend icons assume a plain fit transform | limit → [limits.md#export](../../limits.md#export) | export.md |
| a73 | 5d | Mixed line widths on HiDPI without the GPU | limit → [limits.md#performance](../../limits.md#performance) | performance.md |
| a74 | 5d | 19 tests assume device ratio 1 | dev note | - |
| a75 | 5d | `QCPAlignmentGroup.png` rendered with workspace fonts | dev note | - |
| a76 | 5c | Pan on a log key axis re-records the graph | limit → [limits.md#performance](../../limits.md#performance) | performance.md |
| a77 | 5c | "User set a decorator" is a heuristic | limit → [limits.md#styling](../../limits.md#styling) | styling.md |
| a78 | 5c | `QCPGraphComponent` shared with the curve (name) | project 4 | rename |
| a79 | 5c | MSVC has not seen the component struct order | stale | - |
| a80 | 5c | Removing the points of a NaN key removes nothing, unreported | fixed 3d38d44 | `plottables/plottable-graph.cpp` |
| a81 | 5c | Remove-data bench rows time the busy clone | dev note | - |
| a82 | 5c | Default component name comparison heuristic | limit → [limits.md#line-and-scatter-graphs](../../limits.md#line-and-scatter-graphs) | graphs.md |
| a83 | 5c | `setBrush` replaces fills to a constant or between columns | fixed bbd6a3c | `plottables/plottable-graph.cpp` |
| a84 | 5c | No `QCPGraph::scatterStyle()` getter | project 4 | API shape |
| a85 | 5c | Selected error bars use the selected pen for all bars | limit → [limits.md#line-and-scatter-graphs](../../limits.md#line-and-scatter-graphs) | graphs.md |
| a86 | 5c | `setScatterMaxPoints` removed; scatter-only 1M+ unchecked | limit → [limits.md#performance](../../limits.md#performance) | performance.md |
| a87 | 5c | Out-of-date architecture pages | fixed bef9d99 | `docs/architecture/` |
| a88 | 5c | Names kept from the rename (TestMultiGraph, test-multigraph dir, ...) | project 4 | rename |

## Milestones 1 to 4a (rulings files)

| # | From | Item | Outcome | Where |
|---|---|---|---|---|
| b1 | M1 T1 | QCPItems_Replot10k mostly measures culling/clipping (items over [0,100], view [0,1]) | dev note | `tests/benchmark/benchmark.cpp` |
| b2 | M1 T1 | single-shot bench rows are noise-level | dev note | `docs/plans/neoqcp-3/baseline.md` |
| b3 | M1 T1 | baseline.md names "0040618 plus Task 1" not 5ec70fa | dev note | `baseline.md` |
| b4 | M1 T1 | Stack8 benches differ in widget geometry | dev note | `benchmark.cpp` |
| b5 | M1 T1 | sineKeys returns a ramp | dev note | `benchmark.cpp` |
| b6 | M1 T2 | measuring program not kept, doc should say so | stale | `docs/plans/neoqcp-3/multi-axisrect-perf.md` |
| b7 | M1 T2 | "idle frame 3x a small widget" unfair comparison | dev note | `multi-axisrect-perf.md` |
| b8 | M1 T3 | Q_UNLIKELY / cold attribute on misuse branch | dev note | `src/misuse.h` |
| b9 | M1 T3 | misuse.h in moc header list without Q_OBJECT | dev note | `meson.build` |
| b10 | M1 T3 | setMisuseHandler(nullptr) restore path untested | dev note | `tests/auto/test-misuse/test-misuse.cpp` |
| b11 | M1 T4 | QCPGroupLegendItem::mMultiGraph / QCPIntervalsLegendItem::mIntervals raw | stale | `layoutelement-legend.h` |
| b12 | M1 T4 | mParentLegend set only by QCPLegend::addItem / ctor, not raw addElement: null deref | fixed afff2d9, 4f17527 | `layoutelement-legend.cpp` |
| b13 | M1 T4 | mGraphs.removeIf compares QCPGraph* with base pointer in ~QCPAbstractPlottable (UB on paper) | dev note: base at offset 0, no observable effect | `src/core.cpp` |
| b14 | M1 T4 | QCPIntervals::requestRepaint calls replot unguarded | stale | `plottable-intervals.cpp` |
| b15 | M1 T5 | items a destroyed() slot adds during clearItems survive | limit → [limits.md#items](../../limits.md#items) | items.md |
| b16 | M1 T5 | plain anchors reset children's pixel position when their item dies | stale | `items/item.cpp` |
| b17 | M1 T5 | unused `second` in clearItemsWhileASlotDeletesAnother | stale | `tests/auto/test-ownership/test-ownership.cpp` |
| b18 | M1 T5 | destroyed() slots see itemCount()==0 during clearItems | limit → [limits.md#items](../../limits.md#items) | items.md |
| b19 | M1 T6 | deleting the last layer leaves zero layers, untested | stale | `test-ownership.cpp` |
| b20 | M1 T6 | "report once" is once per loss | dev note | `plottable.cpp` |
| b21 | M1 T6 | debug builds now abort at the next draw when a plottable's axis goes | limit → [limits.md#migrating-from-qcustomplot](../../limits.md#migrating-from-qcustomplot) | migrating.md |
| b22 | M1 T6 | deleteLayerLeavesLists cannot see GPU-map entries | dev note | - |
| b23 | M1 T6 | re-created grid starts hidden, document on grid() | stale | `axis/axis.cpp` |
| b24 | M1 T6 | stray double blank line axis.cpp:52 | dev note | `axis.cpp` |
| b25 | M1 T6b | deleting an angular axis with live polar graphs untested | stale | `tests/auto/test-polar/test-polar.cpp` |
| b26 | M1 T6b | intervals inert test asserts gMisuses>=1 | dev note | `test-ownership.cpp` |
| b27 | M1 T6b | mid-drag tests check gestureActive() only after release | dev note | `test-ownership.cpp` |
| b28 | M1 T6b | deleteTracedGraph does not check selectTest == -1 | dev note | `test-ownership.cpp` |
| b29 | M1 T6b | pixelOf returns x=0 / y=-1 without axis or layout, undocumented | limit → [limits.md#other-plottables](../../limits.md#other-plottables) | plottables.md |
| b30 | M1 T6b | misuse messages name the guard (Q_FUNC_INFO), not the public method | dev note: message text names the failing call | `misuse.h` |
| b31 | M1 T7 | colour scale detects teardown via "axis rect has no axes" | dev note | `layoutelement-colorscale.cpp` |
| b32 | M1 T7 | setSelectionRect unguarded during teardown | dev note | `core.cpp` |
| b33 | M1 T7 | GPU-layer getters null-during-teardown untested | dev note | - |
| b34 | M1 T7 | releaseResources frees GPU layers without std::exchange | dev note | `core.cpp` |
| b35 | M1 T8 | no converting ctor QCPHandle<Derived> -> QCPHandle<Base>; misuse shows checkAlive | stale | `handle.h` |
| b36 | M1 T9 | replots/flushes inside Qt handlers; index walks in QCPLegend::clearItems / QCPLayout::clear can skip | stale | `layout.cpp` |
| b37 | M1 T9 | Qt CUPS print plugin LSan leak | stale | `tests/lsan.supp` |
| b38 | M1 T10 | axis-rect dirty marking calls the creating GPU grid-layer accessor | dev note | `layoutelement-axisrect.cpp` |
| b39 | M1 T10 | plan doc lists deliver as public | stale | `core.h` |
| b40 | M2 | polar/graph scatter setters lack equality guard | stale | `scatterstyle.h` |
| b41 | M2 | setColorValues, setComponentFillBrush, moveAbove/Below mark before no-op | dev note: spurious repaint only | `plottable-graph.cpp` |
| b42 | M2 | duplicate include in test-repaint.cpp | stale | - |
| b43 | M2 | QCPAxisRect null-plot guard in markLayoutAppearanceDirty untested | stale | `layoutelement-axisrect.cpp` |
| b44 | M2 | tracer on another layer than its graph shows old position | stale | `item-tracer.cpp` |
| b45 | M2 | item ops repeat a setPen already in QCPItemOps | project 4 | ops surface cleanup |
| b46 | M2 | graph2 scatter-colour handle test checks pixels only | stale | - |
| b47 | M2 | polar add<T> rollback untested | dev note | `polar-area.h` |
| b48 | M2 | three of four remove() routing tests pin the contract only | dev note | - |
| b49 | M2 | with() marks even when the lambda only reads | dev note: one spurious repaint per chain | `handle-ops.h` |
| b50 | M2 | three detach functions repeat the unparenting block | dev note | `core.cpp` |
| b51 | M2 | most repaint tests check a repaint is queued, not the pixels | dev note | - |
| b52 | IS def | markItemsUsingAxisDirty marks once per item | dev note | `axis.cpp` |
| b53 | IS def | QCPItemBracket::dataExtent pads by 1.5 x length | dev note | `items/item-bracket.cpp` |
| b54 | IS idx ruling | packed STR R-tree; dragging hundreds of items rebuilds often | limit → [limits.md#performance](../../limits.md#performance) | performance.md |
| b55 | IS idx ruling | boxes in data coordinates; scale-type change rebuilds every tree | dev note | `spatial-index.cpp` |
| b56 | IS idx ruling | every "may have moved" goes through the repaint mark | stale | - |
| b57 | IS idx ruling | layers under 64 items keep moved items on the side list | dev note | `spatial-index.cpp` |
| b58 | IS idx ruling | clicks: three passes over children; plot-wide selected set is the upgrade | limit → [limits.md#performance](../../limits.md#performance) | performance.md |
| b59 | IS idx ruling | selectionChanged slot selecting a not-yet-visited item during deselect | stale | `core.cpp` |
| b60 | IS idx ruling | scaleTypeChangeRebuildsTheIndex checks a counter, not pixels | dev note | - |
| b61 | IS idx ruling | add() of a taken item resets positions to default axes | limit → [limits.md#items](../../limits.md#items) | items.md |
| b62 | IS idx ruling | QCPItems_RemoveEach10k 25->32 ms code placement | dev note | - |
| b63 | IS idx fixed | box covering only first point when other is pinned to the rect | stale | - |
| b64 | IS idx fixed | click queried whole view | stale | - |
| b65 | IS idx fixed | label anchored to item on another layer kept old pixels | stale | `item.cpp` |
| b66 | IS idx fixed | anchor cycles accepted, stack overflow | stale | - |
| b67 | IS idx fixed | self-following position recursed forever | stale | - |
| b68 | IS idx fixed | parallel resampling returned before tasks finished | stale | - |
| b69 | IS idx false | "remaining zoom cost is the cast" | stale | - |
| b70 | IS idx false | "click costs 0.24 ms because of per-child loop" | stale | - |
| b71 | M3 | setupPaintBuffers ~157 us per GPU replot | dev note | `core.h` |
| b72 | M3 | per-frame allocations on pan cache hits | dev note | - |
| b73 | M3 | per-position frames for an item mixing axis pairs | stale | `items/item.h` |
| b74 | M3 | data+pixel-offset position type | limit → [limits.md#items](../../limits.md#items) | items.md |
| b75 | M3 | raster images cut at viewport: one-frame band during pan | dev note | - |
| b76 | M3 | 5th+ raster style per layer falls back to QPainter | limit → [limits.md#performance](../../limits.md#performance) | performance.md |
| b77 | M3 | free-form polygon with solid brush stays on QPainter | limit → [limits.md#performance](../../limits.md#performance) | performance.md |
| b78 | M3 | src/ size sweep stopped at -214 lines | dev note | - |
| b79 | M3 | QCPAxis_SetRange10k crept 1.2 -> 1.5 ms | dev note | - |
| b80 | M3 | port Graph2/Multigraph/ColorMap2/Histogram2D to drawing API, fold QCPDrawn | stale | `plottable-colormap2.cpp` |
| b81 | M3 | strong-type inventory (raw double/int/QPointF/QString) | project 4 | `strong-type-candidates.md` |
| b82 | M3 | bar stack staleness check O(N^2) in depth | dev note | - |
| b83 | M3 fmt | R1, R7, R2 done | stale | - |
| b84 | M3 fmt | R3 typed batch commands to decide | stale | `milestone-4b-rulings.md` |
| b85 | M3 fmt | R4 instanced marks with GPU data transform | limit → [limits.md#performance](../../limits.md#performance) | performance.md |
| b86 | M3 fmt | R5 cull by data range per command | limit → [limits.md#performance](../../limits.md#performance) | performance.md |
| b87 | M3 fmt | R6 bounded look-back merge across pipelines | dev note | - |
| b88 | M3 next | milestone 4: port plottables, decide R3, fold adapter | stale | - |
| b89 | M4a | plot deleted from a nested delivery drops queued teardown signals silently | limit → [limits.md#interactions](../../limits.md#interactions) | interactions.md |
| b90 | M4a | six "slot deletes the plot" tests cannot fail on counters | dev note | - |
| b91 | M4a | item/legend-row clicks check the object lives but emit on the plot | dev note | `core.cpp` |
| b92 | M4a | no test for partial-selection refusal on legend row / text element, nor the NOTIFY fix | dev note | `layoutelement-legend.cpp` |
| b93 | M4a | examples/ not built by meson | stale | `meson_options.txt` |

## Found and deferred in 5f

| # | From | Item | Outcome | Where |
|---|---|---|---|---|
| f1 | 5f pictures | Step histogram drew one box per bin, not a stepped outline | fixed 79acf66 | `plottables/plottable-histogram.cpp` |
| f2 | 5f pictures | Gradients example: 19 maps on one rect share one scale, all show one gradient | fixed 49a257f (docs) | `docs/snippets/colormaps.cpp` |
| f3 | 5f probe | Unity build: `unitBoxPixels` defined twice in one chunk | fixed 589886e, 06b416f | `plottables/plottable-errorbar.cpp` |
| f4 | 5f | No unity build in CI: a duplicate-helper clash can come back unnoticed | dev note | CI workflow |
| f5 | 5f | Old data containers' `remove*` on a NaN key do nothing, unreported (2.x) | limit → [limits.md#other-plottables](../../limits.md#other-plottables) | `datacontainer.h` |
| f6 | 5f | Rotated radial tick label: the click box is the upright bounding box, empty corners hit | limit → [limits.md#polar-plots](../../limits.md#polar-plots) | `polar/radialaxis.cpp` |
| f7 | 5f | Error-bar hit test builds the backbone and whisker list per bar on every click | dev note | `plottables/plottable-errorbar.cpp` |
| f8 | 5f | Curve fill records one polygon per NaN run (GPU: one raster per run) | dev note | `plottables/plottable-curve.cpp` |
| f9 | 5f | Bracket user-system test checks strokes near the skewed ends only, not their absence elsewhere | dev note | `tests/auto/` |

## New plot types and benchmark ideas (milestone 6 brainstorm, 2026-11-14)

Missing for scientific plotting (`.superpowers/mpl-challenge/selection.md`, pattern 3). Each is to be written with the
public drawing API only, as a user would: every one is also a torture test of that API.

| # | Item | Outcome | Where |
|---|---|---|---|
| n1 | Vector field (quiver) and streamlines | project: new plot types | gallery #15 |
| n2 | Polar spectrogram and polar bars (wedges) | project: new plot types | gallery #22 |
| n3 | Violin and box plots computed from raw data | project: new plot types | gallery #16, #17 |
| n4 | Scatter with size by value, and a size legend | project: new plot types | gallery #23 |
| n5 | Stacked areas | project: new plot types | gallery #21 |
| n6 | Event (raster) plot | project: new plot types | matplotlib eventplot |
| n7 | Hexbin | project: new plot types | gallery #18 |
| n8 | Categorical axis (text keys) | project: new plot types | gallery #6 |
| n9 | Contour labels and per-level styles | project: new plot types | gallery #13 |
| n10 | Peak-fit editor: drag peaks, resize widths with handles, live fit and residual panel | milestone 6 target 11 | annotations + linked panels |
| n11 | Data-taking calls tested with third-party arrays: xtensor (`xt::xtensor`, `xt::view`), Eigen vectors and blocks, `std::mdspan` rows, Armadillo, a strided view. Each passes `QCPNumberRange` (or fails with a readable error), copies once, keeps the order. Idea (brainstorm only): a `qcp::columns` hook in the style of `std::ranges::begin` that calls a user free function found by argument-dependent lookup, for matrices split into columns, types that are not ranges, and contiguous memory read without a copy; decide after these tests and 6f `colorMap` | maintainer, 2026-11-15 | `QCPNumberRange` (6b), data constructors, `ax->line` |
