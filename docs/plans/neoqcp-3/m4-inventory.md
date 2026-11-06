# NeoQCP 3.0 milestone 4: code inventory (read-only, factual; guesses are flagged)

Sources read: object-model spec decisions 11-15, drawing-API spec sections 2, 3, 7, 8, rulings "Deferred" and "Format decisions for milestone 4", roadmap bullet 4. Paths are relative to src/. Draw-path line counts are from the function start lines. Where I give a range, the end is an estimate (flagged as such).

## 1. Drawing-API status

### 1a. Already on the drawing API (`QCPDrawn<Base>`)
| Class | File | Notes |
|---|---|---|
| QCPBars | plottables/plottable-bars.h:173, .cpp:539 (1354 lines) | overrides hitTest and dataRange, and `dependencyRevision` (.cpp:1084) |
| QCPItemText | items/item-text.h:40 (406 lines in .cpp) | |
| QCPItemRichText | items/item-richtext.cpp (153 lines) | subclasses QCPItemText; has its own draw at :47 and :70 (not checked whether it goes through the context) |

The adapter lives in drawable.h/.cpp. `QCPDrawnCore` is non-template, and `QCPDrawnAdapter` is drawable.h:184. drawable.h:5 and the drawing spec section 2 say milestone 4 folds it into the bases.

### 1b. Plottables still on `draw(QCPPainter*)`
Pipeline legend: QP = QPainter only; GPUL = `QCPPlottableRhiLayer` polyline/scatter via `drawPolylineWithGpuFallback` (plottables/plottable-draw-utils.cpp); ASYNC = `QCPAsyncPipeline`-based resampler through `QCPPipelineScheduler`.

| Class | File (total lines) | Draw path (start line, approximate extent) | Pipeline | Spec decision |
|---|---|---|---|---|
| QCPGraph | plottables/plottable-graph.cpp (2126) | draw :447, drawLegendIcon :528, drawFill :1007, drawScatterPlot :1072, drawLinePlot :1090, drawImpulsePlot :1107; about 680 lines through :1130 (guess at the end) | QP, with one GPU use at :1022 for fills (`plottableRhiLayer`) | 11 |
| QCPGraph2 | plottables/plottable-graph2.cpp (841) | draw :477 to legend icon :804 (about 330 lines) | ASYNC (`QCPGraphPipeline`, L1 cache, L2 rebuild) plus GPUL | 11 (named in the drawing spec section 8 as untouched until M4) |
| QCPMultiGraph | plottables/plottable-multigraph.cpp (1786) + multigraph-fill.cpp (222) + multigraph-errorbars.cpp (70) | draw :1208; drawFills :1502; drawFillOnGpu :1524; drawErrorBars :1626; legend icons :1690 and :1721; about 530 lines in the .cpp, plus the two helper files | ASYNC (`QCPMultiGraphPipeline`) plus GPUL (9 pipeline/rhi references) | 11, and 14 for colour-by-scalar (drawing spec section 8) |
| QCPWaterfallGraph | plottables/plottable-waterfall.cpp (261) | subclass of QCPMultiGraph; its own code is a data adapter (`QCPWaterfallDataAdapter`) and no draw | inherits QCPMultiGraph | 11 |
| QCPCurve | plottables/plottable-curve.cpp (2048) | draw :443, drawLegendIcon :519, drawCurveLine :569, drawScatterPlot :585; about 180 lines, but the clipping/line-building code is the bulk of the file (guess) | QP only | 11 |
| QCPFinancial | plottables/plottable-financial.cpp (1161) | draw :588, drawOhlcPlot :734, drawCandlestickPlot :808; about 250 lines | QP | not in 11-15; follows the generic port (flag: unlisted) |
| QCPStatisticalBox | plottables/plottable-statisticalbox.cpp (756) | draw :560, drawStatisticalBox :645; about 130 lines | QP | same as QCPFinancial |
| QCPErrorBars | plottables/plottable-errorbar.cpp (1076) | draw :520, legend icon :590; about 80 lines, but selectTest/selectTestRect are large | QP | same; the multigraph has its own error bars (cf. 2) |
| QCPIntervals | plottables/plottable-intervals.cpp (1222) + lane-layout.cpp (188) + intervals-algo.cpp (273) | draw :959; drawBarsOnGpu :1161; drawLabels :1175; drawLaneNames :1198; drawBarsWithPainter :1214; drawSelection :516; drawPreview :783; about 700 lines including preview/selection | GPUL for bars, QP for labels | same |
| QCPHistogram (1D) | plottables/plottable-histogram.cpp (486) | draw :347, drawColumnOnGpu :399, legend icon :431; about 135 lines | ASYNC (`QCPHistogram1DPipeline`) plus GPUL | 11 or 14 (a 1D histogram is not a line plottable; the spec does not name it) |
| QCPHistogram2D | plottables/plottable-histogram2d.cpp (517) | draw :376 (uses `QCPColormapRenderer` at :406), legend icon :445; about 120 lines | ASYNC (`QCPHistogramPipeline`), colormap texture (`painting/colormap-renderer.cpp` 260 lines, `colormap-rhi-layer.cpp` 419 lines) | 14 (drawing spec section 8) |
| QCPColorMap2 | plottables/plottable-colormap2.cpp (523) | draw :341, legend icon :435; about 120 lines (renderer :426) | ASYNC (`QCPColormapPipeline`) plus the same colormap texture layer; contours overlay (painting/contour-overlay.cpp, 264 lines) | 14 |
| QCPColorMap | plottables/plottable-colormap.cpp (1331) | draw :1231, legend icon :1313; about 100 lines; also the setColorScale wiring at :922 | QP image (colour-scale tied to `QCPColorScale`) | 14 |

### 1c. Items still on the old draw
| Class | File (lines) | draw at line | Pipeline |
|---|---|---|---|
| QCPItemLine | items/item-line.cpp (283) | :137 | QP |
| QCPItemStraightLine | items/item-straightline.cpp (221) | :102 | QP |
| QCPItemCurve | items/item-curve.cpp (218) | :169 | QP |
| QCPItemRect | items/item-rect.cpp (205) | :139 | QP |
| QCPItemEllipse | items/item-ellipse.cpp (243) | :159 | QP |
| QCPItemPixmap | items/item-pixmap.cpp (318) | :159 | QP (the drawing spec says images stay on QPainter until the colormap texture layer is generic) |
| QCPItemBracket | items/item-bracket.cpp (306) | :185 | QP |
| QCPItemTracer | items/item-tracer.cpp (444) | :307 | QP |
| QCPItemHSpan / VSpan / RSpan | items/item-hspan.cpp (183), item-vspan.cpp (183), item-rspan.cpp (285), item-spanbase.cpp (101) | :95, :95, :151 | GPU via `span-rhi-layer.cpp` (612 lines) with a QPainter fallback (`tryRhiDraw` in item-spanbase.h:51) |

Item base: items/item.cpp (1698 lines, `QCPAbstractItem` at item.h:218). Items on the drawing API already use the recording context, so each remaining item needs a port to helpers (`rect`, `ellipse`, `path`, `polyline`, `image`, `text`). The spans need a decision (see section 4).

Not plottables or items, but on the old draw: axes (axis/axis.cpp), grid, legend, layout elements, polar classes (section 6), selection rect, overlay. The spec milestone 4 does not name them for the drawing API. Flag: ports of these are optional for milestone 4.

## 2. The line-plottable family today

Decision 11 asks for one base for graph, graph2, multigraph and curve with one API for pen, line style, scatter style, fill, gradient and components, with no silently ignored setters.

| Feature | QCPGraph | QCPGraph2 | QCPMultiGraph | QCPCurve |
|---|---|---|---|---|
| Base | `QCPAbstractPlottable1D<QCPGraphData>` (plottables/plottable1d.h:54), plottable-graph.h:75 | `QCPAbstractPlottable` + `QCPPlottableInterface1D`, plottable-graph2.h:17 | same as graph2, plottable-multigraph.h:27 | `QCPAbstractPlottable1D<QCPCurveData>`, plottable-curve.h:77 |
| Data | `QCPDataContainer<QCPGraphData>` (own container, sorted) | `QCPAbstractDataSource` (SoA, shared_ptr, views) | `QCPAbstractMultiDataSource` (SoA or row-major, one key column and many value columns) | `QCPCurveDataContainer` (t, key, value) |
| Resampler | adaptive sampling in-class (`getOptimizedLineData` :1133) | async L1/L2 pipeline | async multi-pipeline | none |
| Multi-component | no (channel fill to another graph via `setChannelFillGraph`) | no | yes: `Component` struct (name, pen, selectedPen, scatterStyle, selection, lineStyle, visibility, legend) at .h:35 | no |
| Line style enum | own `LineStyle` (plottable-graph.h:~92) | own (graph2.h:23, 6 values) | own (multigraph.h:34, 6 values) | own (curve.h) |
| Gaps | no (a line with NaNs) | `setGapThreshold` (graph2.h:119) | `setGapThreshold` (multigraph.h:152) | no |
| Colour by scalar | no | scatter only: `setScatterColorValues`, `setScatterColorGradient` (graph2.h:94, 99) | per key, all components: `setColorValues`, `setColorGradient`, `setColorRange`, `setColorScaleType` (multigraph.h:157-169) | no |
| Fill | `setBrush` + channel fill, drawFill :1007 | `setBrush(Qt::NoBrush)` set in the constructor (graph2.cpp:26); brush used only for tinting scatter (graph2.cpp:753) | per component: `setComponentFill`, `setComponentFillBetween` (multigraph.h:117-122) | the plottable brush (flag: not checked in detail) |
| Error bars | separate class QCPErrorBars | no | per component: `setComponentErrorBars` (multigraph.h:136) | no |
| Scatter | `setScatterStyle`, `setScatterSkip` | `setScatterStyle`, `setScatterSkip`, `setScatterMaxPoints` | per component scatter style, `setScatterSkip` | `setScatterStyle`, `setScatterSkip` |
| Selection | `QCPDataSelection` | same via base | per-component selection (multigraph.h:~172) | base |
| 1D interface | via template | `interface1D()` | `interface1D()` | via template |
| Pen | base `setPen` | base `setPen` | ignored: per-component pens only (the spec calls it out) | base |

What a shared interface must cover (my reading, not in the spec):
- One `LineStyle` enum and one scatter-style/skip setter family.
- A data source abstraction that includes "one key, N values" and "x, y, t" shapes. Spec decision 11 leaves the concrete class set to the builder and says it touches project 3's data model, so `QCPCurve`'s parametric data may stay a separate source type.
- Components (a list with pen, selected pen, scatter, line style, visibility, legend flag), with a single-component case for graph and graph2.
- Fill: to a constant, to another component, and brush tinting.
- Gaps (a gap threshold on every class that has a source).
- Colour by scalar: values, gradient, range, scale type, ideally linked to a `QCPColorScale` (decision 14).
- Error bars per component (collapses QCPErrorBars into the family, flag: this is a design choice, the spec does not say).
- The optional resampler: with or without an async pipeline, and the viewport-dependent L1/L2 cache state, which in the drawing API's terms is `dependencyRevision` or view-dependent commands.
- Legend icon, hit test, data range, which the `QCPDrawn` defaults supply (drawing spec section 7); the multigraph overrides the icon.

Existing helpers that can be reused: `plottable-draw-utils.cpp` (321 lines, `drawPolylineWithGpuFallback`), `plottable-linestyle.h` (190 lines, step conversions), `plottable-color-mapper.h` (`ColorScalarMapper`), `plottable-color-runs.h`, `plottable-l1-cache.h`.

## 3. Colour maps, histograms, colour scale

Classes:
- QCPColorMap (plottables/plottable-colormap.cpp 1331 lines, own data `QCPColorMapData`, QPointer `mColorScale` at .h:183, QP image).
- QCPColorMap2 (523 lines; source `QCPAbstractDataSource2D`; `QCPColormapPipeline`; `mRenderer` of type `QCPColormapRenderer` in painting/colormap-renderer.h:18; contour overlay).
- QCPHistogram2D (517 lines; 1D source of (key, value); `QCPHistogramPipeline`; same `mRenderer`; setBins, normalization).
- QCPHistogram (1D, 486 lines; `QCPHistogram1DPipeline`; bars or step; columns; no colour scale).

Wiring today (all three colour-mapping classes have a copy):
- `setColorScale` disconnects 6 connections, stores the scale, calls 3 setters, then reconnects 6 connections: plottable-colormap.cpp:922-951; plottable-colormap2.cpp:185-217; plottable-histogram2d.cpp:177-~210.
- Signals both ways: `dataRangeChanged`, `gradientChanged`, `dataScaleTypeChanged` between the plottable and `QCPColorScale` (layoutelements/layoutelement-colorscale.h:101-105).
- QCPColorMap2 and QCPHistogram2D keep the scale pointer in the renderer (`QCPColormapRenderer::setColorScale`, painting/colormap-renderer.cpp:68). Deleting the scale detaches silently (plottable-colormap2.h comment, line ~58), which decision 14 ("nothing unlinks silently") forbids.
- `QCPColorScale::rescaleDataRange` only recognises `QCPColorMap` via `qobject_cast` (layoutelements/layoutelement-colorscale.cpp:469). QCPColorMap2 and QCPHistogram2D are not seen there. This is a gap a shared relation fixes (flag: I read only that one place).
- The multigraph and graph2 colour values and gradients are held in the plottable itself. No `QCPColorScale` is involved.
- The colour scale is created by the user. It is not owned or placed by the axis rect (layoutelements/layoutelement-colorscale.h:68; `QCPColorScaleAxisRectPrivate` at :40).
- GPU: a texture layer exists (painting/colormap-rhi-layer.cpp 419 lines, colormap-renderer.cpp 260 lines) used by colormap2 and histogram2d. QCPColorMap draws a QPainter image. The drawing spec (section 3) says images move to the GPU "once the colour-map texture layer is generic, milestone 4".

Decision 14 asks for:
- One relation "uses a colour scale", set up once in a base class, for all colour-mapping plottables, line plottables with a colour axis included.
- Each axis rect has a colour scale, placed and aligned by the rect (decision 13), hidden until used. A colour map uses it by default.
- A deleted colour scale is noticed (decision 1).

## 4. Axes, axis rects, margin groups, linked axes, drag/zoom, spans

Current API (decision 12):
- `axisRect->add<QCPAxis>(type)` exists as a handle form of `addAxis` (layoutelements/layoutelement-axisrect.h:104-112, core.h:378). `addAxis(type, axis)`, `addAxes`, `removeAxis`, `setupFullAxesBox` also exist (:112-124).
- The rect has a drag/zoom orientation flag (`setRangeDrag`, `setRangeZoom`, h:88-89) and lists of drag/zoom axes (`setRangeDragAxes`, `setRangeZoomAxes`, h:90-95; storage `mRangeDragHorzAxis`, `mRangeDragVertAxis` at h:168). Per-axis opt-out does not exist (grep found no per-axis drag/zoom flag in axis/axis.h). Default behaviour (I did not check the constructor): by 2.x convention only the first axis per side is in the list. Flag: guess.
- No linked-axes feature: axis/axis.h has no secondary axis or range-transform members (grep for "secondary", "linked", "Transform" in axis/axis.h returned nothing).
- A plottable's other axes: constructor takes (keyAxis, valueAxis); `QCPAbstractPlottable` has `axesChanged()` at plottable.h:261. A checked "same plot" call exists in debug checks (spec decision 6); I did not verify it here.
- Sizes: axis/axis.cpp 3301, axis.h 609, layoutelement-axisrect.cpp 1591, layout.cpp 2337 (includes QCPMarginGroup at :30-170).

Margin groups (decision 13):
- `QCPMarginGroup` is in layout.h:36 / layout.cpp:30-170. It is per plot and aligns margin sizes only (`commonMargin`). 42 references in src/. Elements join via `QCPLayoutElement::setMarginGroup`.
- Decision 13 asks for an alignment group that aligns edges, works across several plot widgets, accounts for colour scales and side elements, is made by a factory and owned like other objects.
- Not in the worktree: SciQLopPlots' `VPlotsAlign` (the spec says it repurposes minimum margins). The spec's review answer 5 says alignment across plots is not decided.
- Dependency: the colour scale (decision 14) must be placed by the rect first.

Spans:
- Three concrete classes: `QCPItemHSpan` (183 lines), `QCPItemVSpan` (183), `QCPItemRSpan` (285; radial/polar span), plus the abstract `QCPAbstractSpanItem` (item-spanbase.h, 56 + 101 lines).
- The spec asks for "one span class". Each class has its own draw, hit test, move and emit logic (about 5 emits in rspan, 2 in hspan, 2 in vspan). Flag: whether polar RSpan folds into the same class depends on the polar port.
- Also: `QCPIntervals` plottable (a different thing, intervals with lanes) and legend-intervals.

## 5. Selection state (decision 15)

Decision 15: plottables, items, axis parts and legend rows report selection the same way, with one selection type and one signal per object. `QCPDataSelection` stays a value.

Where state and signals live today:
| Place | State | Signal(s) |
|---|---|---|
| QCPAbstractPlottable | `QCPDataSelection mSelection` (plottable.h:270) + `mSelectable` (`QCP::SelectionType`) | `selectionChanged(bool)`, `selectionChanged(const QCPDataSelection&)`, `selectableChanged(QCP::SelectionType)` (plottable.h:253-255) |
| QCPPolarGraph | `QCPDataSelection mSelection` (polar/polargraph.h:199) | same three (polargraph.h:182-184) |
| QCPAbstractItem | `bool mSelected` | `selectionChanged(bool)`, `selectableChanged(bool)` (items/item.h:288-289) |
| QCPAxis | `SelectableParts mSelectableParts, mSelectedParts` (axis/axis.h:413) | `selectionChanged(parts)`, `selectableChanged(parts)` (axis.h:402-403) |
| QCPPolarAxisAngular / Radial | same pair (polar/layoutelement-angularaxis.h:342, radialaxis.h:274) | same pair |
| QCPLegend | `SelectableParts mSelectedParts` (layoutelement-legend.h:266) | parts signals (:255-256) |
| QCPAbstractLegendItem | bool selection | bool signals (legend.h:83-84) |
| QCPTextElement | bool | bool signals (layoutelement-textelement.h:98-99) |
| QCPSelectionDecorator + QCPSelectionDecoratorBracket | decoration pen/brush/scatter for a plottable (plottable.h:52; selectiondecorator-bracket.cpp 393 lines) | none |
| QCPSelectionRect | the rubber band, signals `started`, `changed`, `canceled`, `accepted` (selectionrect.cpp:195-238) | |
| QCustomPlot | aggregated `selectionChangedByUser` (core.h:492), emitted at core.cpp:4799 | |

Shapes: 3 different state types (bool, `SelectableParts` flags, `QCPDataSelection`), 4 signal names for the same event kind, and the plot signal `selectionChangedByUser`. Related code: selection.cpp (628 lines).

## 6. Polar

All under polar/ (6709 lines): `QCPPolarAxisAngular` (layoutelement-angularaxis.cpp 1895 + .h 560, a `QCPLayoutElement`), `QCPPolarAxisRadial` (radialaxis.cpp 1634 + .h 472, a `QCPLayerable`), `QCPPolarGraph` (polargraph.cpp 1413 + .h 301, a `QCPLayerable`, not an `QCPAbstractPlottable`, and with its own selection), `QCPPolarGrid` (255 + 179), `QCPPolarLegendItem` (polargraph.h:46). All draw via `QCPPainter`. The plot owns them through `detach/adopt(QCPPolarGraph*)` in core.h:507-510 and `add<>` special-cases at core.h:730 and :745.

Spec review answer 7: polar is kept and ported to the new base classes, including polar coordinates and transforms. Flag: porting polar to coordinates and the drawing API requires the polar coordinate transform in coords.h (not checked).

## 7. Signals not yet through deferred delivery

Mechanism: `QCustomPlot::deliver(sender, fn)` (core.cpp:2800, core.h:~698) plus `QCPEmissionScope` (signal-delivery.h:63). Scope sites (the scopes that hold things back): core.cpp (12 sites), layer.cpp:146, layoutelement-axisrect.cpp:239, layoutelement-legend.cpp:956, layout.cpp:853.

Signals now deferred (19 `deliver(` calls): `afterReplot` (core.cpp:2947), `mouseDoubleClick`, `mousePress`, `mouseMove`, `mouseRelease`, `mouseWheel`, plottable/axis/item/legend click and double-click (core.cpp:4017-4048), `deleteRequested` (core.cpp:4310), `layerChanged` (layer.cpp:860).

Direct emits outside `deliver` (about 122 `emit` lines in src/ minus about 17 already inside deliver lambdas; counts by file from grep, rough):
- theme.cpp: 16 (`changed()` on every setter)
- core.cpp: about 11 (`group->membersChanged` at 1499, 1667, 1703, `selectionChangedByUser` at 4799, plus others)
- axis/axis.cpp: 5 (`rangeChanged` at :663-665, `scaleTypeChanged` at :639, `selectionChanged` at :755, `selectableChanged` at :727)
- plottables: plottable.cpp 5 (selection/busy), plottable-group.cpp 4 (`membersChanged`), colormap 3, colormap2 5, histogram2d 5, histogram 3, intervals 3, multigraph 2, lane-layout 3
- polar: radialaxis 5, angularaxis 4, polargraph 3
- layout elements: legend 4, legend-group 1, textelement 5, colorscale 3 (`dataRangeChanged`, `dataScaleTypeChanged`, `gradientChanged` at :270-327)
- items: item.cpp 2 (selection), rspan 5, vspan 2, hspan 2, item-creation-state 2
- others: selectionrect 5, overlay 4, layer.cpp 2 (`visibleChanged` :652), async-pipeline 5, command-replay-rhi 1

The milestone-1 plan (superpowers plan line 377) says the rest "stay direct until milestone 4".
Notable hazards: `rangeChanged` (slots may delete the axis; axis.cpp:634 already guards with a QPointer), and the colour-scale connect chains, which call a setter that emits in the middle of `setColorScale`.

## 8. Sizes, dependencies, suggested split

Rough sizes (src lines touched, guess from file sizes and draw-path ranges):
| Area | Lines (guess) |
|---|---|
| Signals through deferred delivery | about 120 emit sites across about 30 files; most are 1-line changes. About 300 lines |
| Selection model | plottable.cpp, item.cpp, axis.cpp, legend, textelement, polar, decorators; about 1500 lines |
| Line family | graph 680 + graph2 330 + multigraph 530 + helpers 290 + curve 180 + waterfall; about 2200 lines of draw paths, more with the class unification |
| Other plottables on the context (financial, statbox, errorbars, intervals, histogram) | about 1300 draw lines |
| Colour maps, histograms, colour-scale relation | about 800 draw lines + wiring (3 x 30) + renderer (260) + texture layer (419) |
| Items | about 1900 file lines total, draw paths about 700 |
| Spans merge | about 750 lines |
| Axes, axis rect, linked axes, drag/zoom | about 800 (axis.cpp and axisrect touched, not rewritten) |
| Alignment groups | about 600 (layout.cpp, new class) |
| Polar | up to 6700 lines in scope, port-only about 1500 draw/selection lines |
| Fold the adapter into the bases, R3 | drawable.cpp/.h + painting/draw-commands.h (611) + command-replay-*; about 1500 |

Dependencies:
- Signals first, because selection and every later class touches emit sites. The selection model changes signals, so it follows the deferred-delivery rule.
- The drawing-context port of the line family needs the shared base (decision 11) before the colour-scale relation can be set once in a base (decision 14).
- The colour-scale relation needs the colormap texture layer generic (images on the GPU) only if QCPItemPixmap and QCPColorMap move to the GPU; otherwise independent.
- Alignment groups (13) need the rect-owned colour scale (14) first.
- Linked axes (12) use the transforms of decision 8 (already in coords.h), independent of the others.
- R3 (typed batch commands) should be decided before porting the big line classes, because they produce the commands. The rulings say the command set freezes at R3.
- Folding the adapter into the bases comes last among the plottable work (after all classes are ported), and the old base removal is milestone 5.
- Polar: depends on selection and signals (polar graph has its own copy) and on the one span class.

Suggested split into 5 sub-milestones, each leaving NeoQCP green:
- M4a Signals and selection. Move the remaining about 105 direct emits to `deliver`, and unify selection state and signals (section 5). No drawing changes. Reason: it touches the most files and shallowly, and everything after builds on classes whose signals will not move again.
- M4b Decide R3 (command format) and port the line family: one base with shared setters (decision 11), then QCPGraph, QCPGraph2, QCPMultiGraph, waterfall, QCPCurve onto the context, with the resampler outputs wrapped as shared-storage commands. Reason: the largest cost and the one with the most perf risk (baseline benchmarks must be rerun), and the colour-scale relation depends on its base.
- M4c Colour maps, histograms and the colour-scale relation (decision 14): one `uses colour scale` relation in the base, QCPColorMap, QCPColorMap2, QCPHistogram2D, QCPHistogram onto the context, the colormap texture layer generic, `QCPColorScale::rescaleDataRange` covering all. The line-family colour-by-scalar attaches to this relation. Reason: needs M4b's base.
- M4d Axes and alignment: axis opt-out drag/zoom defaults, linked axes, `add<QCPAxis>` completion, rect-owned colour scale, alignment groups replacing margin groups (decisions 12, 13). Reason: needs the rect to place the colour scale from M4c.
- M4e The rest: financial, statistical box, error bars, intervals, the remaining items, the one span class, polar, fold the adapter into the bases. Reason: the leftover classes are independent of each other and low risk. Polar goes here because it has a separate copy of selection, axes and drawing code and benefits from M4a, M4d.

Flags (guesses): the draw extents in section 1 (ends of ranges), the default drag/zoom axes in section 4, whether QCPItemRichText really goes through the context, and whether QCPFinancial/QCPStatisticalBox/QCPErrorBars are meant to be part of milestone 4 (the spec names only the line family, colour maps and histograms, spans and polar).
