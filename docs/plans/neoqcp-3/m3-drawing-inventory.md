# NeoQCP 3.0 milestone 3 — drawing inventory

Repo /home/jeandet/Documents/prog/NeoQCP-3.0, branch neoqcp-3.0, HEAD **fa43577**. Read-only: no build,
no run, no edit. Every statement below was read from the source at that commit; file:line given.
"Lines" are line counts of the function body as written (comments included).

---

## 1. Per class: what draw() does

### 1a. Plottables

`key/value range` = the getKeyRange/getValueRange implementations; every concrete plottable overrides both
(pure in `src/plottables/plottable.h:198,200`). `clip` = the clipRect override: every plottable except
QCPPolarGraph inherits `QCPAbstractPlottable::clipRect()` (`src/plottables/plottable.cpp:1395` =
intersection of the key and value axis rects).

| class | file (draw / selectTest lines) | painter calls in draw() | coordinate conversions | GPU path | selectTest | legend icon | key/value range | clip |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| QCPGraph | plottable-graph.cpp (79/22) | 1D `drawPolyline` (line 4 of drawLinePlot), `drawFill`→`drawPolygon` fallback, `drawImpulsePlot`→`painter->drawLines`, `drawScatterPlot`→`QCPScatterStyle::drawShape`, `drawDecoration` | via `getLines()` (coordsToPixels on both axes) | yes: plottable RHI layer for lines (`plottable1d.h:569`) and for solid-brush fills (`plottable-graph.cpp:1019-1040`); scatters QPainter | `pointDistance(pos, closestDataPoint)` over the 1D container; `-1` unless the click is inside `keyAxis->axisRect()->rect()` or `iSelectPlottablesBeyondAxisRect` | pen line + fill sample + scatter shape (43 l.) | container `keyRange`/`valueRange` | base |
| QCPCurve | plottable-curve.cpp (74/22) | `drawCurveLine`→1D `drawPolyline`, `drawPolygon` (fill), `drawScatterPlot`→`drawShape`, `drawDecoration` | `getCurveLines()` (coordsToPixels) | lines: plottable RHI layer via the 1D helper; fill: QPainter `drawPolygon` | `pointDistance` (same shape as graph) | pen line + fill + scatter (43 l.) | container ranges | base |
| QCPBars | plottable-bars.cpp (54/29) | `painter->drawPolygon` per bar (`getBarRect`), pen+brush, `drawDecoration` | `getBarRect()` uses axis rect ratio for width (pixels) + coordsToPixels | no | `getBarRect(key,value).contains(pos)` per visible bar; returns `tolerance*0.99` (rect hit) | `drawRect` (10 l.) | computed with bar width / group spacing, not the container | base |
| QCPFinancial | plottable-financial.cpp (35/36) | `drawOhlcPlot` (drawLine per bar) / `drawCandlestickPlot` (drawRect/drawLine), `drawDecoration` | `getVisibleDataBounds` + coordsToPixels inside the helpers | no | per style: `ohlcSelectTest` / `candlestickSelectTest` (distance to the drawn lines, per bar) | both styles drawn side by side (102 l.) | key range widened by half the bar width; value = container | base |
| QCPStatisticalBox | plottable-statisticalbox.cpp (64/54) | `drawStatisticalBox` (drawRect for the quartile box, drawLine whiskers/median, `drawShape` outliers), `drawDecoration` | `getQuartileBox`, `getWhiskerBackboneLines` | no | quartile box `contains(pos)` → `tolerance*0.99`; else min distance to whisker backbones | `drawRect` (10 l.) | key widened by half width; value = container | base |
| QCPErrorBars | plottable-errorbar.cpp (68/25) | `painter->drawLines` (error bar segments from `getErrorBarLines`), `drawDecoration` | data plottable's `coordsToPixels` through its interface1D | no | `pointDistance` of the error-bar data container | `drawLine` (23 l.) | own container (lower/upper ends), independent of the data plottable | base |
| QCPColorMap | plottable-colormap.cpp (80/24) | `drawImage`/`drawPixmap` of `mMapImage` (rebuilt by `updateMapImage`), clip region from the axis rects | `coordsToPixels` for the image rect | no (pure QPainter image) | `pixelsToCoords` then `keyRange().contains && valueRange().contains` → rect hit | gradient bar (`drawRect`+`drawPixmap`, 19 l.) | `mMapData->keyRange()/valueRange()` | base |
| QCPColorMap2 | plottable-colormap2.cpp (93/26) | none directly: `mRenderer.draw` (QPainter image or GPU quad) + `drawLabels` (contours) | renderer maps data range→pixels via the axes | yes: `QCPColormapRhiLayer` when `!pmNoCaching` and the image fits a texture (`painting/colormap-renderer.cpp:26-60`) | data source `xRange/yRange` contains the clicked coords → rect hit | gradient bar (6 l.) | data source ranges | base |
| QCPHistogram | plottable-histogram.cpp (22/4) | per column: `drawColumnOnGpu` (GPU) or `paintColumn` (QPainter) | `columnBars()` builds pixel bars via the axes | yes: plottable RHI layer, fill quads + extruded outline (`drawColumnOnGpu`, re-extruded per frame) | **always `-1`** (not hit-testable) | one `drawRect` per column (13 l.) | from `mResult->edges` / heights | base |
| QCPHistogram2D | plottable-histogram2d.cpp (33/16) | colormap renderer (image/GPU quad) + contour lines/labels | `pixelToCoord`/`coordToPixel` inside the renderer and contour extractor | yes, through `QCPColormapRenderer` (same policy as ColorMap2) | `pixelToCoord` + `cachedFiniteBounds` → distance `0` (rect hit) | gradient bar (6 l.) | `cachedFiniteBounds` (scattered true min/max, not sorted ends) | base |
| QCPIntervals | plottable-intervals.cpp (8/15) | `drawBarsOnGpu` (GPU) or `drawBarsWithPainter` (`drawPolygon` per shape) + `drawSelection`, `drawPreview`, `drawLabels` | `toPixelBar` with the key axis + lane bands | yes: RHI (bars) — see §3 | `hitTest(pos)` → row/lane bands; `0` on a bar, `tolerance*0.99` on empty lane space (still a press/hover target) | one `fillRect` (4 l.) | key = min start/max stop of the rows; **value range always `foundRange=false`** (rows are pixel lanes) | base |
| QCPMultiGraph | plottable-multigraph.cpp (218/78) | `drawFills` (fill polygons), `drawErrorBars`, `drawPolylineCached`/`drawColoredPolylineCached`/`drawPolylineWithGpuFallback`, `drawColoredImpulses`→`drawLines`, `drawColoredScatters`/`drawPlainScatters`→`drawShape` | `getOptimizedLineData*`/`getLines*` → coordToPixel per component | yes: plottable RHI layer (cached extrusion), scatter RHI layer, plus fills | binary search of the nearest key by `findEnd`, then per-component point distance + segment distance; **`if (!details) return tolerance*0.99` for any point in the axis rect** | component pen/spread (15 l.) | data source ranges (value = union of visible components) | base |
| QCPGraph2 | plottable-graph2.cpp (326/59) | `drawPolylineCached`, `drawPolylineWithGpuFallback`, `drawColoredPolylineCached`, `drawLines` (steps/impulses), `drawShape`/`drawPoint` (scatters) | `getOptimizedLineData`/`getLinesIndexed` (L1 cache) → coordToPixel | yes: plottable RHI layer + scatter RHI layer; extrusion cache reused across pans | nearest key (binary search) + neighbour/segment distance; same `!details` fast path | pen line + scatter (22 l.) | data source ranges | base |
| QCPWaterfallGraph | plottable-waterfall.cpp (**no own draw** / no selectTest) | inherits `QCPMultiGraph::draw`, `selectTest`, `drawLegendIcon` | inherited | inherited (GPU) | inherited | inherited | value range from `mOriginalSource`; key range inherited | base |
| QCPPolarGraph | polar/polargraph.cpp (79/21) | `drawFill`, `drawLinePlot`→1D `drawPolyline` (QPainter: no `rhi` use in the file), `drawScatterPlot`→`drawShape`, `drawDecoration`, `setClipRegion` | `getLines()` with the angular/radial axes | no | `pointDistance` when the click is inside `mKeyAxis->rect()` | pen line + scatter (43 l.) | container ranges | **overrides clipRect → `keyAxis->rect()`** (`polargraph.cpp:160`) |
| QCPPolarGrid | polar/polargrid.cpp (28/–) | `drawAngularGrid`/`drawRadialGrid` → setPen/brush, `drawLine` | tick/subtick vectors + angular/radial mapping | no | none (not selectable) | – | – | none (layerable) |
| QCPPolarAxisAngular | polar/layoutelement-angularaxis.cpp (45/30) | `drawBackground` (setPen + `drawEllipse`), `drawLine` (grid/tick lines), `drawTickLabel` (text) | angular mapping, `setAnchorReference` | no | whole `mOuterRect` (returns `tolerance*0.99`) **unless `onlySelectable` → `-1`** | – | – | – |
| QCPPolarAxisRadial | polar/radialaxis.cpp (58/13) | `drawLine` (axis line, ticks, subticks), `drawTickLabel` | `coordToRadius`, `angle()`, vector math | no | `getPartAt(pos)` → part + `tolerance*0.99` | – | – | – |
| QCPPlottableGroupLegendItem | plottable-group.cpp (45/–) | `drawRect` icon border, `drawLegendIcon` (member's), text via `QCPLabelRenderer` | font metrics, icon rect | no | legend item (not a plottable) | – | – | – |
| QCPPolarLegendItem | polar/polargraph.cpp (32/–) | `drawText` + `drawRect` (icon) | font metrics | no | legend item | – | – | – |

Note: `drawLegendIcon` is pure virtual on `QCPAbstractPlottable` (`plottable.h:284`, documented at
`plottable.cpp:347`) — there is no base implementation; every concrete plottable writes one.

### 1b. Items

All items share: coordinate conversion through `QCPItemPosition::pixelPosition()`
(`src/items/item.cpp`), `clipRect()` = `mClipAxisRect->rect()` when `mClipToAxisRect` (set in the ctor to
the first axis rect) else `mParentPlot->viewport()` (`item.cpp:1230`), `applyDefaultAntialiasingHint`,
and the helper `QCPAbstractItem::rectDistance(rect, pos, filledRect)` (`item.cpp:1317`). No item has a
`drawLegendIcon`, and none has a key/value range (they are not plottables).

| class | file (draw / selectTest lines) | painter calls | selectTest | GPU |
| --- | --- | --- | --- | --- |
| QCPItemLine | item-line.cpp (23/9) | `drawLine` after `getRectClippedLine`, `setPen`/`setBrush` at the ends (`QCPLineEnding::draw`) | distance to the segment (start→end) | no |
| QCPItemStraightLine | item-straightline.cpp (15/9) | `drawLine` of the clipped infinite line | `distanceToStraightLine` | no |
| QCPItemRect | item-rect.cpp (17/10) | `drawRect` (pen + brush) | `rectDistance(rect, pos, filled)` — inside counts only when the brush is visible | no |
| QCPItemEllipse | item-ellipse.cpp (29/26) | `drawEllipse` (pen + brush) | analytic distance to the ellipse border; inside counts when filled | no |
| QCPItemCurve | item-curve.cpp (31/28) | `drawPath` of a cubic `QPainterPath` | min distance over `toSubpathPolygons()` segments | no |
| QCPItemBracket | item-bracket.cpp (84/44) | `drawLine` arms, `drawPath` (curly/calligraphic) | per style: 3 segment distances (square/round) or curve samples | no |
| QCPItemText | item-text.cpp (35/23) | `QCPLabelRenderer::defaultRenderer()->drawWith` (drawRect/drawText), rotated with `painter->setTransform` | `rectDistance` of the measured text box in the rotation-corrected frame | no |
| QCPItemRichText | item-richtext.cpp (46/19) | `QTextDocument::drawContents` (drawRect/drawRoundedRect border), rotation transform | `rectDistance` of `computeDrawRect` | no |
| QCPItemPixmap | item-pixmap.cpp (21/8) | `drawPixmap` + optional `drawRect` border | `rectDistance(getFinalRect(), pos, true)` | no |
| QCPItemTracer | item-tracer.cpp (46/63) | `updatePosition` then `drawLine`/`drawRect`/`drawEllipse` per style | per style: segment/ellipse distance for plus/circle/square/crosshair, gated by `clipRect().intersects(...)` | no |
| QCPItemHSpan | item-hspan.cpp (26/43) | `tryRhiDraw` (GPU) else `drawRect` + border `drawLine`s | distance to the two edges, limited to the axis rect in x | **yes** (`QCPSpanRhiLayer`) |
| QCPItemVSpan | item-vspan.cpp (26/43) | same, vertical | same | **yes** |
| QCPItemRSpan | item-rspan.cpp (38/71) | `tryRhiDraw` else `drawRect` + 4 border lines | distance to each of the four edges, band-limited | **yes** |
| QCPAbstractItem (base) | item.cpp (draw is pure, `item.h:293`) | — | pure (`item.h:244`) | — |

---

## 2. Shared helpers that already exist

| helper | signature | where | used by |
| --- | --- | --- | --- |
| `QCPAbstractPlottable1D<T>::drawPolyline` | `void drawPolyline(QCPPainter*, const QVector<QPointF>&) const` | `src/plottables/plottable1d.h:566` | QCPGraph, QCPCurve, QCPPolarGraph (via their drawLinePlot/drawCurveLine); GPU branch for solid pens, else NaN-splitting `painter->drawPolyline` |
| `QCPAbstractPlottable1D<T>::getDataSegments` | `void getDataSegments(QList<QCPDataRange>&, ...) const` | `plottable1d.h:533` | selected/unselected segment split for every 1D plottable |
| `qcp::drawPolylineSplitNaN` | `void(QCPPainter*, const QVector<QPointF>&)` | `plottables/plottable-draw-utils.h:25` | software branch of `drawPolylineWithGpuFallback` |
| `qcp::drawPolylineWithGpuFallback` | `(painter, plot, layer, pts, pen, gpuOffset, clipRect)` | `plottable-draw-utils.h:46`, impl `plottable-draw-utils.cpp:63` | QCPGraph2, QCPMultiGraph (steps, impulses) |
| `qcp::drawPolylineCached` | `(…, bool freshLines, ExtrusionCache&)` | `plottable-draw-utils.h:66` | QCPGraph2, QCPMultiGraph (line cache, pan translation) |
| `qcp::drawColoredPolylineCached` / `drawColoredPolylineRuns` / `extrudeColorRuns` | see header | `plottable-draw-utils.h:78,92,70` | QCPMultiGraph, QCPGraph2 coloured lines |
| `qcp::gpuLayer` | `QCPPlottableRhiLayer*(QCustomPlot*, QCPLayer*, bool exporting)` | `plottable-draw-utils.cpp:48` | QCPHistogram (`draw`), the single "may I use the GPU" predicate for plottables |
| `qcp::extrusionPenWidth`, `needsReextrusion`, `needsColoredReextrusion` | widths/cache validity | `plottable-draw-utils.h:58,62,77` | every GPU polyline user |
| `QCPLineEnding::draw` | `draw(QCPPainter*, const QCPVector2D& pos, dir)`, `draw(..., double angle)` | `src/lineending.cpp:96,152` | QCPItemLine, QCPItemCurve, QCPItemBracket, QCPGraph/Curve (legend icons) |
| `QCPScatterStyle::drawShape` | `drawShape(QCPPainter*, const QPointF&)`, `drawShape(..., double x, double y)` | `src/scatterstyle.cpp:250,266` | every scatter drawing (graphs, curves, statistical box outliers, graph2/multigraph) |
| `QCPAbstractPlottable::applyDefaultAntialiasingHint / applyFillAntialiasingHint / applyScattersAntialiasingHint` | `void(QCPPainter*) const` | `plottable.cpp:1195,1210,1224` | all plottables |
| `QCPAbstractPlottable::drawDecoration` (virtual) | `void(QCPPainter*, QCPDataSelection)` | `plottable.cpp` (QCPSelectionDecorator) | every plottable calls it last, draws the selection |
| `QCPAbstractItem::rectDistance` | `double(rect, pos, filledRect) const` | `item.cpp:1317` | rect/ellipse/text/pixmap/richtext items |
| `QCPLabelRenderer` (`defaultRenderer()`, `measureWith`, `drawWith`) | static + virtual | `src/axis/labelrenderer.{h,cpp}`, latex subclass | QCPItemText, QCPItemRichText, legend rows, axis tick labels |
| `QCPColormapRenderer::draw` | `(painter, keyAxis, valueAxis, keyRange, valueRange)` | `painting/colormap-renderer.cpp:10` | QCPColorMap2, QCPHistogram2D (image or GPU quad + contour fallback) |
| histogram geometry: `qcp::histogram::appendBarQuads`, `outline`, `BarRect` | `plottables/histogram-geometry.h` | | QCPHistogram (GPU fill/outline and legend icons) |
| interval geometry: `qcp::intervals::toPixelBar`, `rowBand`, `laneBand` | `plottables/intervals-algo.h`, `src/plottables/lane-layout.h` | | QCPIntervals, drawPreview/drawLabels |
| `QCPPolarGrid::drawAngularGrid/drawRadialGrid` | private members | `polar/polargrid.cpp:70,110` | the polar axis classes call them |

---

## 3. The passes

**Widget.** `QCustomPlot : public QRhiWidget` (`src/core.h:83`). One replot = QPainter drawing into
per-layer paint buffers, then one QRhi render pass that composites those textures and draws the GPU
geometry between them.

1. **Layer list** — `core.cpp:486-497`: `background`, `grid`, `main`, `axes`, `legend`, `overlay`
   (bottom→top). `setCurrentLayer("main")`, then `overlay` and `main` are set to `QCPLayer::lmBuffered`;
   all others stay `lmLogical`. New layerables are added to their layer's child list by
   `QCPLayerable::setLayer` (`layer.cpp`), appended at the end (draw order).
2. **Paint buffer per layer** — `QCustomPlot::setupPaintBuffers()` (`core.cpp:~4440`, doc at 4423):
   consecutive `lmLogical` layers share one buffer; an `lmBuffered` layer gets its own, and a logical
   layer *after* it starts another buffer. So `background`+`grid` share buffer 0, `main` has its own,
   `axes`+`legend` share the next, `overlay` its own (four buffers for the default plot).
3. **Drawing into the buffers** — `QCPLayer::drawToPaintBuffer()` (`layer.cpp:249`) → `QCPLayer::draw()`
   (`layer.cpp:209`): per child, skip if `!realVisibility()`, skip plottables that fail
   `drawableAxes()`, skip indexed items whose candidate bit is 0, then save/clip to
   `child->clipRect()`/antialias/`child->draw(painter)`/restore. Called from `replot` at
   `core.cpp:2907`.
4. **Upload** — `QCustomPlot::uploadLayerTextures` (`core.cpp:3453`) uploads every dirty
   `QCPPaintBufferRhi` texture plus the RHI-layer vertex/uniform buffers.
5. **Render pass** — `QCustomPlot::executeRenderPass` (`core.cpp:3533`), iterating **layers**, not
   buffers, and per layer, in this order:
   1. composite the layer's paint buffer texture (`core.cpp:3550`; one quad, translated by
      `pixelOffset()` for the pan fast path, scissored to the plottables' clip rect),
   2. colormap texture quads of that layer (`QCPColormapRhiLayer`, `core.cpp:3624`),
   3. GPU plottable geometry of that layer (`QCPPlottableRhiLayer`, `core.cpp:3649`),
   4. GPU scatter geometry (`QCPScatterRhiLayer`, `core.cpp:3656`),
   5. GPU spans of that layer (`QCPSpanRhiLayer`, `core.cpp:3663`),
   6. grid lines, only on the layer named `grid` (`QCPGridRhiLayer::renderGridLines`, `core.cpp:3667`),
   7. tick marks, only on the layer named `axes` (`renderTickMarks`, `core.cpp:3672`).

   Consequence: inside one layer, GPU geometry always composites **above** that layer's QPainter buffer.
6. **GPU or QPainter?** The decision is per draw call and always the same shape: `parentPlot->rhi()`
   non-null **and** `!painter->isExporting()` **and** a style gate, then push vertices into the layer's
   RHI layer; otherwise paint with QPainter. `painter->isExporting()` = `pmVectorized || pmNoCaching`
   (`src/painting/painter.h:66-68`). Sites:
   `plottable1d.h:569` (solid pen lines), `plottable-graph.cpp:1019` (solid brush fills),
   `plottable-draw-utils.cpp:72-73,114-115,192-193` (cached/coloured polylines),
   `plottable-histogram.cpp` (`qcp::gpuLayer` + `drawColumnOnGpu`), `painting/colormap-renderer.cpp:26`
   (`!pmNoCaching` + `fitsInTexture`), `item-spanbase.cpp:73-88` (`tryRhiDraw`: solid/no brush only),
   `plottable-intervals.cpp` (`drawBarsOnGpu`), `polar/*` and `items/item-{line,rect,ellipse,curve,text,
   pixmap,tracer,straightline}.cpp` never use the GPU.
7. **Exports** — `toPixmap`/`savePdf`/`savePng`/`toPainter` build a `QCPPainter` with `pmVectorized` +
   `pmNoCaching` (+ `pmNonCosmetic` when asked) (`core.cpp:3068-3070, 5095, 5159`), and paint through
   `QCustomPlot::draw(painter)` (`core.cpp:~4320`), which lays out and calls `layer->draw(painter)` for
   every layer (`core.cpp:4327`) — never the paint buffers, never the RHI. Every GPU path has a QPainter
   fallback in the same function: `plottable1d.h:640-650` (NaN-split `drawPolyline`),
   `drawPolylineWithGpuFallback`'s software branch (`plottable-draw-utils.cpp:87-97`),
   `QCPColormapRenderer::draw`'s `painter->drawImage` + `drawContourFallback`
   (`colormap-renderer.cpp:88-90`), `QCPHistogram::paintColumn`, `QCPIntervals::drawBarsWithPainter`,
   `QCPAbstractSpanItem::tryRhiDraw` returning false, `QCPGrid`/axis tick QPainter drawing (the RHI grid
   is only for the screen pass).

---

## 4. Stacking anomalies

### 4a. "A text item added after a graph can end up under the graph line"

Both are added to the same layer (`main` by default) and the paint buffer draws them in child order, so
insertion order is what decides inside the buffer. The graph's line is not in that buffer, though.

1. `QCPAbstractItem`'s constructor puts the item on the current layer (`QCPLayerable(parentPlot)` →
   `setLayer(mParentPlot->currentLayer())`, `layer.cpp:589-590`; `item.cpp:1104-1105`).
2. `QCPAbstractPlottable`'s constructor does the same for the graph (`plottable.cpp:443-445` +
   `layer.cpp:589`), appending a second child to `main`.
3. `QCPLayer::draw` paints the item into buffer 2 with QPainter (`layer.cpp:209`), because items have no
   GPU path (except spans).
4. The graph's polyline goes to the plottable RHI layer instead: `plottable1d.h:568-583` requires
   `rhi && !isExporting() && pen.style() == Qt::SolidLine`.
5. `executeRenderPass` composites buffer 2 first and draws the plottable geometry **after** it
   (`core.cpp:3550` then `core.cpp:3649`).

Net: with the default solid pen the graph is drawn above the item no matter what order they were created
in, because it is not in the item's buffer at all. Only when the graph takes the QPainter branch (dashed
pen, export) does insertion order apply again.

### 4b. "A dashed pen changes the stacking"

Same mechanism, seen from the other side.

1. `pen.style() == Qt::SolidLine` is part of the GPU condition (`plottable1d.h:570`,
   `plottable-draw-utils.cpp:73,115,193`).
2. A dashed pen therefore falls into the `else` branch and is painted with QPainter into the layer's
   buffer (`plottable1d.h:640-650`), i.e. in child order, under any item created after it.
3. Switching the same graph back to a solid pen moves it into the RHI pass, i.e. above the item again,
   with no change in the child list.
4. The same style gate exists for fills (`plottable-graph.cpp:1019`: `brush().style() ==
   Qt::SolidPattern`) and spans (`item-spanbase.cpp:78`: gradient/pattern brushes fall back), so a
   gradient fill or a textured brush also drops that object below the items drawn in the buffer.

No other code path was found that reorders children by pen style; the anomaly is entirely the buffer
versus RHI-pass split.

---

## 5. Hit testing

- **Picking order** — `QCustomPlot::layerableListAt(pos, onlySelectable, details)` (`core.cpp:~4990`):
  layers from top to bottom, children from last to first (topmost first), skipping
  `!realVisibility()`, `mouseTransparent()`, and — for items — a spatial-index candidate miss
  (`layer->itemCandidates(QRectF(pos,0,0).adjusted(-tol,-tol,tol,tol))`). A child is added when
  `selectTest(...) >= 0 && < selectionTolerance()` (`int`, default `8`, `core.cpp:446,1018`).
  The result is ordered by **stacking**, not by distance.
- `layerableAt(pos, onlySelectable, details)` (`core.cpp:~5060`) returns `candidates.first()`.
- **Mouse press dispatch** — `mousePressEvent` (`core.cpp:~3900`) asks
  `layerableListAt(mMousePressPos, false, &details)`, remembers `candidates.first()` as
  `mMouseSignalLayerable` (the click-signal target) and forwards the event down the list until one
  accepts it.
- **Click selection** — `mouseReleaseEvent` → `releaseAsClick` → `processPointSelection`
  (`core.cpp`), which asks `layerableAt(event->pos(), true, &details)` — i.e. **only selectable**
  layerables — deselects others, and calls `selectEvent` when
  `mInteractions` has the target's `selectionCategory()`.
- **Where the two winners differ**: the press/dispatch winner allows non-selectable layerables, the
  selection winner does not. An item with `setSelectable(false)` lying on top of a selectable graph
  receives the press (and, because it becomes `mMouseSignalLayerable`, the `itemClick` signal) while
  the selection goes to the graph below. The same split exists for `QCPPolarAxisAngular`, whose
  `selectTest` returns `-1` when `onlySelectable` is true (`layoutelement-angularaxis.cpp:262-265`) — it
  takes the press but can never be the selection.
- **Wheel** — `wheelEvent` (`core.cpp:~4150`) calls `layerableListAt(pos, false)` with **no details
  pointer**. `QCPGraph2` and `QCPMultiGraph` treat `details == nullptr` as "anywhere in the axis rect":
  `if (!details) return tolerance*0.99` (`plottable-graph2.cpp:31`, `plottable-multigraph.cpp:31`), so
  the wheel target is the topmost such plottable whenever the cursor is anywhere over its axis rect,
  regardless of distance to the data.
- **"Hits anywhere in the axis rect" classes**: QCPColorMap/QCPColorMap2/QCPHistogram2D (whole data
  rect), QCPBars/QCPFinancial/QCPStatisticalBox (whole bar / quartile box), QCPIntervals (lane bands,
  empty lanes included with the tolerance), QCPGraph2/QCPMultiGraph (the `!details` fast path),
  QCPPolarAxisAngular (whole outer rect for non-selectable queries). QCPHistogram never hits
  (`selectTest` returns `-1`).

---

## 6. Extension today

**A plottable subclass must implement** (pure virtuals in `src/plottables/plottable.h`):
`draw(QCPPainter*)` (`:275`), `drawLegendIcon(QCPPainter*, const QRectF&) const` (`:284`),
`selectTest(const QPointF&, bool onlySelectable, QVariant* details)` (`:169`), `getKeyRange` (`:198`),
`getValueRange` (`:200`). The base already provides `clipRect()` (key∩value rect), the
antialiasing hints, `selectionCategory()` (`iSelectPlottables`), `selectEvent`/`deselectEvent`,
`drawDecoration` and the selection decorator machinery. In practice subclasses also touch
`interface1D()` (for data access by other plottables, e.g. QCPErrorBars), `dependentLayers()`
(GPU layers), `canProduceContent()`, `stallPixelOffset()` (pan translation), `releaseGpuResources()`,
`setPipelineScheduler()` (async data), and `axesChanged()`.

Reference sizes: the smallest concrete implementation is `QCPStatisticalBox`
(756 + 234 lines including documentation, 64-line `draw`, 54-line `selectTest`); `QCPBars` is 1293 + 337
(54/29), `QCPFinancial` 1161 + 276 (35/36), `QCPGraph` 2126 + 246 (79/22).

**An item subclass must implement**: `draw(QCPPainter*)` (pure, `src/items/item.h:293`) and
`selectTest(...)` (pure, `:244`). Everything else is given: `clipRect()` (axis rect when
`mClipToAxisRect`, else viewport), `applyDefaultAntialiasingHint`, `selectionCategory()`
(`iSelectItems`), `selectEvent`/`deselectEvent`, `rectDistance`, position/anchor creation
(`createPosition`), and — since the item index — `dataExtent()` (default `std::nullopt` = never culled)
plus `markDirty()` (must be called by every geometry setter; it also feeds the layer's spatial index and
marks anchored followers).

Reference sizes: `QCPItemStraightLine` is the smallest item at 221 + 113 lines (15-line `draw`,
9-line `selectTest`); `QCPItemRect` 205 + 151 (17/10), `QCPItemLine` 283 + 122 (23/9),
`QCPItemTracer` 444 + 209 (46/63).
