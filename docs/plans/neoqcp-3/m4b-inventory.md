# m4b inventory — the line family onto the drawing API

Read-only survey, branch `neoqcp-3.0`. Every claim carries `file:line`; guesses are marked **[guess]**. Nothing was
built or run. Paths are relative to `/home/jeandet/Documents/prog/NeoQCP-3.0`.

Baseline: only `QCPBars` is ported (`plottables/plottable-bars.h:173`). `QCPMultiGraph` (`plottable-multigraph.h:30`),
`QCPGraph2` (`plottable-graph2.h:17`), `QCPCurve` (`plottable-curve.h:77`), `QCPGraph` (`plottable-graph.h:75`) and
`QCPWaterfallGraph` (`plottable-waterfall.h:44`) still derive the old bases and draw into `QCPPainter*`.

## 1. Line building in `src/datasource/`

`QCPAbstractMultiDataSource` (`abstract-multi-datasource.h`) is the multigraph draw's only entry:

| call | signature | out space | axes? |
| --- | --- | --- | --- |
| `getLines` | `(int column, int begin, int end, QCPAxis*, QCPAxis*) -> QVector<QPointF>` (:27-29) | pixels | yes |
| `getOptimizedLineData` | `(…, int pixelWidth, QCPAxis*, QCPAxis*)` (:23-25) | pixels | yes |
| `getLinesAll` | `(begin, end, keyAxis, valueAxis, QVector<QPointF>* results, int numColumns)` (:40-47, non-pure, loops) | pixels | yes |
| `getOptimizedLineDataAll` | same + `pixelWidth` (:31-38, non-pure, loops) | pixels | yes |
| `getLinesIndexed` | `(…, QVector<int>& sourceIndices)` (:52-54, pure; generic impl `abstract-multi-datasource.cpp:31-41`) | pixels | yes |
| `getOptimizedLineDataIndexed` | (:55-58; generic impl `.cpp:43-54`) | pixels | yes |
| `gapsBefore` | `(begin, end) -> qcp::algo::GapVector` (:62-68) | data (mask) | no |
| `findBegin` / `findEnd` | `(double sortKey, bool expandedRange=true)` (:15-16) | indices | no |

- Gap threshold `kDefaultGapThreshold = 1.5` (`abstract-datasource.h:77`); per-source `setGapThreshold`/`gapThreshold`
  (`abstract-multi-datasource.h:75-79`); the multigraph pushes its own every draw (`plottable-multigraph.cpp:1275`).
  Rule `qcp::algo::detectKeyGaps(keys,begin,end,threshold)` (`algorithms.h:23-49`): a gap when `step > threshold *
  min(nonzero neighbour step)`, needs `count >= 3`, repeated keys are no reference; ratios reusable through `GapCache`
  (`algorithms.h:82-109`).
- `findBegin`/`findEnd` are `lower_bound`/`upper_bound` with a ±1 expansion (`algorithms.h:111-141`), pure index math,
  no axes.
- **Pixel output is structural**: `qcp::algo::AffineTransform::fromAxis(const QCPAxis*)` (`algorithms.h:244-294`)
  reads `range()`, `axisRect()->left()/width()/bottom()/height()`, `rangeReversed`; `linesToPixelsImpl` maps through
  it (`algorithms.h:298-360`). No axes → no output.
- Adaptive sampling keys on **key pixel width, one bucket per key pixel** — not on `pixelWidth`:
  `optimizedLineDataImpl(..., int /*pixelWidth*/, ...)` ignores it (`algorithms.h:389`); the decision is `keyPixelSpan
  = |keyAxis->coordToPixel(first) - coordToPixel(last)|`, `maxCount = 2*keyPixelSpan + 2`, raw when `dataCount <
  maxCount` (`algorithms.h:401-409`); else a min/max envelope with up to 4 points per key pixel at
  epsilon·0.2/0.25/0.75/0.8 (`algorithms.h:446-470`), broken by a NaN run wider than a pixel and by a detected gap
  (`algorithms.h:558-587`). Multi-column twin: `optimizedLineDataMulti` (`algorithms.h:633-875`, per-column `ColState`
  :716-724).
- **No data-space decimation for lines anywhere.** The only data-space binning is the L1/L2 resampler
  (`graph-resampler.h`): `binMinMax` :162-206, `binMinMaxMulti` :359-363/443-491, `kLevel1TargetBins = 100'000` :599,
  `kResampleThreshold = 100'000` :600, `kLevel2PixelMultiplier = 4` :601, `l2BinCount = plotWidthPx*4` (min 3200)
  :644-648. `resample.h/.cpp` is 2D colourmap only (`qcp::algo2d::resample`, `resample.h:45-54`).
- Variants: `QCPSoAMultiDataSource` (`soa-multi-datasource.h:73-128`, multi path uses `optimizedLineDataMulti`);
  `QCPRowMajorMultiDataSource` (`row-major-multi-datasource.h:142-205`); `QCPResampledMultiDataSource`
  (`resampled-multi-datasource.h:63-202`), where `getOptimizedLineDataAll` **delegates to `getLinesAll`** (:112-117)
  and `getOptimizedLineDataIndexed` **delegates to `getLinesIndexed`** (:190-195) — an L2 line is never thinned twice.

## 2. L1/L2 pipeline of `QCPMultiGraph`

- Members, all `plottable-multigraph.h`: `mPipeline` :245, `mL1Cache`
  (`shared_ptr<qcp::algo::MultiGraphResamplerCache>`) :246, `mL2Result` (`shared_ptr<QCPAbstractMultiDataSource>`)
  :247, `mL2Dirty` :248, `mNeedsResampling` :249, `mRenderedRange` :250, `mHasRenderedRange` :251, `mCachedLines`
  :253, `mLineCacheDirty` :255, `mCachedPlotSize` :256, `mExtrusionCaches` :258, `mViewportDebounce` :260,
  `mCachedIndices` :266.
- **Both levels hold data space.** L1 = `MultiGraphResamplerCache::level1` = `MultiColumnBinResult` {keys, values,
  origin, gapRatio} (`graph-resampler.h:333-347`), min/max pairs per bin, bins = `min(kLevel1TargetBins, srcSize/10)`
  (`graph-resampler.h:679-709`, multi :765-800). L2 is a `QCPResampledMultiDataSource` over the same struct
  (`resampled-multi-datasource.h:10-23`). Pixels appear only later, in `getLines`/`getOptimizedLineData`.
- L1 is built off-thread by `ensureL1TransformMulti` (`plottable-multigraph.cpp:90-111`,
  `TransformKind::ViewportIndependent`) and extracted in `onL1Ready` via `qcp::extractL1Cache`
  (`plottable-l1-cache.h:10-22`).
- **What triggers an L2 rebuild: the draw, not the range change.** `onViewportChanged`
  (`plottable-multigraph.cpp:295-307`) only sets `mL2Dirty = true` and, for a pure pan (key range size ratio within
  1e-4), starts `mViewportDebounce` (150 ms single-shot, ctor :50-52) whose timeout calls `markLinesDirty()` (:77-82).
  The rebuild is in `draw()`: `if (needFreshLines && mL2Dirty && mL1Cache) { rebuildL2(viewport); mL2Dirty = false; …
  }` (:1263-1272); `rebuildL2` = :288-293 (`resampleL2Multi` + `mL2HasOrigin`).
- Resample threshold `needsResamplingMulti = size()*columnCount() >= kResampleThreshold` (100'000) (:113-117), same
  product in `ensureL1TransformMulti` (:93-94); density `kLevel2PixelMultiplier = 4` (`graph-resampler.h:601`);
  `resampleL2Multi` (`resampled-multi-datasource.h:412-428`) returns nullptr for a log key, empty L1, `columns == 0`,
  or `fewEnoughToDrawRaw` (`graph-resampler.h:652-657`).
- Deferred swap, names and lines: `mPendingSource` :291, `mPendingL1` :292, `mPendingColorValues` :295,
  `mPendingReady` :296, `mPendingGeneration` :297, `mAcceptedGeneration` :302; `stagePendingSource()` :162-184,
  `markPendingReady()` :186-191 (calls `mParentPlot->requestDataSwap()`), `commitPendingData()` :193-231, overrides
  `hasPendingData()` h:63 / `commitPendingData()` h:64. `setDataSource(shared_ptr)` :119-128 **defers** iff `source &&
  mDataSource && mHasRenderedRange && mParentPlot`, else `applySourceNow` :130-160.
- **How it is toggled today: it is not.** The deferral is unconditional on a live rendered graph; no public setter or
  flag disables it (public header h:49-204). Pacing lives in `QCustomPlot::requestDataSwap`. **[guess]** there is no
  opt-out anywhere; not checked past that call.
- `QCPGraph2` has the same shape, single-column (`plottable-graph2.cpp:122-171`).

## 3. `QCPMultiGraph::draw` (1208-1425) and helpers

1. bail :1211-1214 (no axes / source / empty / empty key range).
2. export :1216-1223: `!mL1Cache && mNeedsResampling && pmNoCaching` → `mPipeline.runSynchronously(vp)` + `onL1Ready`.
3. source :1225-1236 (L2 > raw; raw only if `!mNeedsResampling || mL1Cache || pmNoCaching || log key`);
  `findBegin`/`findEnd` on the visible range :1241-1245; `pixelWidth` = axis-rect height for a vertical key, width
  otherwise :1247-1250.
4. line-cache decision :1253-1259 `qcp::evaluateLineCache(...)` (`painting/viewport-offset.h:54-103`) →
  `{needFreshLines, gpuOffset}`; export is always fresh with a null offset (:81-82); a pan keeps `gpuOffset` unless it
  exceeds one axis dimension (:98-99).
5. L2 rebuild and gap threshold :1263-1275; `coloured` = mapper has values and the source carries origin :1277.
6. fresh frame :1285-1338: per column `getOptimizedLineDataIndexed` (coloured ∧ adaptive ∧ no L2) / `getLinesIndexed`
  (coloured) / `getLinesAll` (uncoloured, all visible, >1 column, not adaptive) / per-column `getOptimizedLineData` /
  `getLines`; then sets `mRenderedRange`, `mHasRenderedRange`, `mLineCacheDirty = false`, `mCachedPlotSize`, and drops
  `mExtrusionCaches`, fill verts, error-bar bars/band and `gpuOffset`.
7. **fills, then error bars, before the lines** :1338-1342.
8. per component :1344-1424: invisible skip; `lsNone` + no scatter skip; `activePen` = selected pen when the component
  has a selection; `needStyledLines = dataIdx || qcp::needsReextrusion(...)`; step/impulse via
  `qcp::toStepLeft/Right/Center/ImpulseLines(...)` :1366-1374; style dispatch :1383-1411; scatters at the original
  data positions :1416-1423.

GPU path per step:

| step | call | path |
| --- | --- | --- |
| plain lines | `qcp::drawPolylineCached` :1406-1408 | extrude + `QCPPlottableRhiLayer` |
| plain lines, export | `qcp::drawPolylineWithGpuFallback` :1410-1411 | GPU, else QPainter |
| coloured lines | `qcp::drawColoredPolylineCached` :1395-1398 | colour runs + extrusion cache |
| coloured, export | `qcp::drawColoredPolylineRuns` :1400-1402 | QPainter, one polyline per run |
| impulses | `drawColoredImpulses` :1383 / `painter->drawLines` :1385-1391 | QPainter only |
| fills | `drawFillOnGpu` :1524-1537 → `qcp::gpuLayer(...)->addPlottable` | plottable RHI layer |
| fill fallback | `paintFill` :1544-1554 (`simplify:` note :1541-1543) | QPainter |
| error bars | `drawBarsAsPolylines` :1656-1674 | cached / fallback polyline |
| dense band | `drawBarsAsBand` :1676-1688 (alpha 64) | `drawFillOnGpu` else `paintFill` |
| scatters | `drawPlainScatters`/`drawColoredScatters` :1416-1423 | `scatterRhiLayer(mLayer)->addScatter(Colored)` |
| legend | `drawComponentLegendIcon` :1721-1746 | QPainter |

Caches and windows:

- `mCachedLines` holds the **line cache window**, not the view: `lineCacheWindow` :1435-1440 =
  `qcp::algo::lineCacheKeyRange(axis range, log)` (`graph-resampler.h:634-642`, one view width each side:
  `[lower-size, upper+size]`) narrowed by `findBegin`/`findEnd`. A pan shorter than a view width therefore reuses the
  pixels with `gpuOffset`.
- `mCachedIndices` (only when coloured) :266; `mRenderedRange`/`mHasRenderedRange` = the ranges the pixels were mapped
  at :250-251; invalidated by `invalidateLines` :641-646 and `markLinesDirty` :77-82.
- `mExtrusionCaches` per component :258; `needsReextrusion` (`plottable-draw-utils.h:112-113`); type
  `qcp::ExtrusionCache` (`plottable-draw-utils.h:76-88`).
- Helpers: `resizePerColumn` :1427-1433, `fillDrawable` :1442-1451, `ownerStep` :1453-1461, `fillRows` :1463-1482,
  `shapedFillRows` :1485-1490, `binnedWhenDense` :1492-1500 (bins above 4 rows/pixel), `errorBarRows` :1575-1591,
  `errorBarsFitInView` :1593-1605 (3·capWidthPx), `errorBandRows` :1607-1622, `drawErrorBars` :1626-1654 (mode fixed
  on a fresh frame, `entry.asBand` on pan frames).
- No busy fade in `draw()`: busy is a base layerable (`updateEffectiveBusy`, connected at :47-48, :159, :183, :229);
  `draw()` only forwards `painter->opacity()` to GPU submissions (:710, :769, :1536).

## 4. The drawing API side

Structs (`src/painting/draw-commands.h`): `PolylineDataCmd {shared_ptr<const vector<QPointF>> points; QPen pen;}`
:37-41; `FillBetweenCmd {upper, lower, QBrush brush;}` :59-64; `MarkersCmd {points; QCPScatterStyle style;}` :85-89;
`ErrorBarsCmd {centres; spans; QPen pen;}` :92-97 (spans are half extents, data units). All four are `{gpu, dataOnly}`
(`commandKind` :172-195).

Replay:

- GPU (`command-replay-rhi.cpp`): `PolylineDataCmd` → `cache.pixels()` (:548-554) then `addPolyline` (:75-98) →
  `qcp::extrudeStroke` + `addGpuStroke`, solid or dashed pipeline; `FillBetweenCmd` → `addRibbonFill` when
  `gpuCanFill(brush) && upper.size() == lower.size()` (:569-582), else an image via `addRasterCommand` (:155-204);
  `MarkersCmd` → `scatter.addScatter` (:254-268, one instance per point, **one style for all points**); `ErrorBarsCmd`
  → `qcpErrorBarPixels` then a stroke (:614-623).
- Painter (`command-replay-painter.cpp`): polyline mapped then `drawPolylineSplitNaN` :139-146; fill-between rebuilt
  as upper + reversed lower polygon :161-175; markers `style.applyTo` + `drawShape` per point :194-202; error bars as
  one NaN-separated polyline :203-209.
- **Per-vertex colour: no.** `PolylineDataCmd` carries one `QPen`; the only per-vertex payload in the command set is
  `PolylinePositionsCmd`'s `QCP::Position` (space, not colour) (`draw-commands.h:44-48`), and `QCPDrawContext` has no
  colour-run overload (`draw-context.h:136-152`, no colour API in the header). Colour-by-scalar stays outside the API,
  through `qcp::ColorRun`/`ColorScalarMapper` (`plottable-draw-utils.h:139-148`).
- **Pan (R2) is implemented at two levels.** Recording level: `QCPRecordingCache` (`command-replay-rhi.h:79-173`)
  keeps the whole recording's vertices; `reuse()` succeeds on the same list id, dpr, output height, clip and a
  **translating** transform, returning `delta` (`command-replay-rhi.cpp:369-378`); `replayKept` (:282) re-submits the
  kept runs with that delta, dashes keeping their phase, raster runs keeping their image; runs built by
  `keepSolid`/`keepDashed`/`keepRaster` (:398-463), committed at :631-632. Markers are rebuilt every pan frame (:506,
  :254). Plottable level — what the line family uses today: `ExtrusionCache` keeps **untranslated** vertices
  (`plottable-draw-utils.h:76-88`) and `drawPolylineCached` translates them by `gpuOffset` when `freshLines == false`
  (:126-131).
- `transformDependent` = a command read the frame's transform and baked a pixel out of it (`draw-commands.h:316`;
  reading `transform()`/`clip()` marks the list, `draw-context.h:106-129`). Effects: `recordingIsStale` compares the
  stored transform (`drawable.cpp:383-384`), `stallOffset` refuses to shift (`drawable.cpp:389`), the recording is
  excluded from geometry reuse (`command-replay-rhi.cpp:71`, `qcpRecordingMovesWithTheData` :640-655).
- **No notion of a recording valid for a key window.** Staleness is generation / source / dependency / view epoch /
  items epoch / transform only (`drawable.cpp:370-385`). The key-window cache is a plottable private (`mCachedLines` +
  `lineCacheWindow`), not a recording concept.

## 5. `QCPCurve`

- Container `QCPCurveDataContainer = QCPDataContainer<QCPCurveData>`, sorted by `t`, `key` is the main key
  (`plottable-curve.h:38-72`); `mScatterStyle`/`mScatterSkip`/`mLineStyle` h:141-143.
- `LineStyle` has **two** values, `lsNone`, `lsLine` (h:95-100) — no step, no impulse.
- `draw` :443-516: per selected/unselected segment; fill = `drawPolygon(QPolygonF(lines))` :491-492; line =
  `drawCurveLine` :495-500 → `drawPolyline(painter, lines)` :569-576 (**QPainter only, no GPU path**); scatter =
  `drawScatterPlot` :508-509, per point `style.drawShape` :585-594; selection decorator :514-515.
- Clipping is data-space region traversal, ~1100 lines: `getCurveLines` :624-755, `getRegion` :879,
  `getOptimizedPoint` :926, `getOptimizedCornerPoints` :1083, `mayTraverse` :1542, `getTraverse` :1662,
  `getTraverseCornerPoints` :1810 — i.e. :879-1996 project off-screen segments onto a slightly larger rect. A recorded
  polyline gets the clip for free.
- Setters call `markDirty()`: `setScatterStyle` :257-261, `setScatterSkip` :274-278, `setLineStyle` :287-294 (one new
  generation once ported).
- Tests: `tests/auto/test-qcpcurve`, `TestQCPCurve`, 4 slots (`init`, `cleanup`, `dataManipulation`, `dataSharing`;
  `.h:7-12`).

## 6. `QCPWaterfallGraph`

- Overrides: `setDataSource(shared_ptr)` h:55 / `plottable-waterfall.cpp:93-99`; `dataChanged()` h:72 /
  `.cpp:196-208`; `getValueRange(...)` h:73-75 / `.cpp:210-261`. Everything else is `QCPMultiGraph`. Members h:78-87;
  `friend class TestWaterfall` h:97.
- It never touches L1/L2/lines directly: every parameter or data change calls `markDirty()` then `rebuildAdapter()`
  (`.cpp:101-142`, :173-194), which builds a fresh **immutable** `QCPWaterfallDataAdapter` and pushes it through
  `QCPMultiGraph::setDataSource` — so it inherits the staged swap, and the comment at `.cpp:196-208` documents the one
  ordering hazard (a `dataChanged` racing the staged source).
- The adapter implements 10 virtuals (`plottable-waterfall.h:17-30`) including its own `getLines` (`.cpp:55-67`) and
  `getOptimizedLineData` (`.cpp:69-84`, copies keys+one column then `detectKeyGaps` + `optimizedLineData`). It does
  **not** override `getLinesAll`/`getOptimizedLineDataAll`/ `getLinesIndexed`, so it falls back to the base per-column
  loops and the generic indexed implementations (`abstract-multi-datasource.h:31-58`).
- `getValueRange` computes the offset envelope itself (`.cpp:233-255`).
- Tests: `tests/auto/test-waterfall`, `TestWaterfall`, 18 slots, incl. `dataChangedWhileRendered…` and
  `clearingSourceEmptiesTheGraph`.

## 7. Shared-interface candidates

| class | values | where |
| --- | --- | --- |
| `QCPGraph` | lsNone, lsLine, lsStepLeft, lsStepRight, lsStepCenter, lsImpulse | `plottable-graph.h:92-110` |
| `QCPGraph2` | same six, one-line enum | `plottable-graph2.h:24-32` |
| `QCPMultiGraph` | same six, one-line enum | `plottable-multigraph.h:33-34` |
| `QCPCurve` | lsNone, lsLine only | `plottable-curve.h:95-101` |

- `setPen`/`setBrush`: inherited from `QCPAbstractPlottable` by all four, none overrides them. graph2 reads `mPen` in
  `draw` (`plottable-graph2.cpp:612`, `:811`, `:822`); curve reads `mPen` (:460, :533, :552, :557, :590).
  **`QCPMultiGraph` never reads `mPen` or `mBrush`** — a word-boundary grep over `plottable-multigraph.cpp`/`.h` finds
  none; its only colours are `Component::pen`/`selectedPen`/ `scatterStyle` (`plottable-multigraph.h:36-47`). So
  `multigraph->setPen(...)` changes nothing it draws (it does bump the generation through the base `markDirty`).
- `setScatterStyle`: graph (base), curve (`.cpp:257-261`, `markDirty`), multigraph per component
  (`Component::scatterStyle`), graph2 inline with **no `markDirty`** (`plottable-graph2.h:89`).
- `setScatterSkip`: curve `.cpp:274-278` (`markDirty`); graph2 `plottable-graph2.h:91` inline, no dirty; multigraph
  `plottable-multigraph.h:154` inline, **no line-cache invalidation** (scatter pass only).
- The three identical six-value enums are the obvious shared-interface candidate; `QCPCurve` is the reduced subset
  (lsNone/lsLine), so it fits only if a shared enum carries all six and curve rejects steps.

## 8. Tests

| suite | class | slots | exercises |
| --- | --- | --- | --- |
| `test-multigraph` | `TestMultiGraph` | 149 | QCPMultiGraph |
| `test-color-by-scalar` | `TestColorByScalar` | 44 | multigraph colour, L1 origin, `mCachedLines` |
| `test-data-swap` | `TestDataSwap` | 21 | multigraph + graph2 deferred swap |
| `test-pipeline` | `TestPipeline` | 188 | graph2, multigraph L1/L2, resampler, colormap2 |
| `test-waterfall` | `TestWaterfall` | 18 | QCPWaterfallGraph |
| `test-qcpcurve` | `TestQCPCurve` | 4 | QCPCurve |
| `test-qcpgraph` | `TestQCPGraph` | 6 | QCPGraph |

- No `TestQCPGraph2`: `QCPGraph2` is exercised from `test-pipeline` (36 mentions), `test-scatter-rhi` (36),
  `test-datasource` (21), `test-busy-indicator` (19), `test-pipeline/test-layer-translation.cpp`,
  `test-gpu-translation.cpp`, `test-data-swap`, `test-color-by-scalar`, `test-builder`, `test-hit-rule`.
- Suites that read multigraph **private caches** (friends at `plottable-multigraph.h:219-222`: `TestMultiGraph`,
  `TestPipeline`, `TestDataSwap`, `TestColorByScalar`). `test-multigraph`: `cacheExtendsBeyondVisibleRange`, the four
  `lineCache*`/`lineCacheWindow*` cases, `theKeyAxisItLeftIsNoLongerListenedTo`,
  `componentShownAfterARebuildIsDrawnAgain`, `renderZoomRepaintsWhenAnotherBufferIsDirty`,
  `renderSetRangeRepaintsWhenAnotherBufferIsDirty`, `panIsRedrawnFreshAfterTheDebounce`,
  `panIsRedrawnFreshWhileAnotherLayerIsDirty` (all `mCachedLines`/`mRenderedRange`),
  `zoomedOutLinesAreThinnedToThePixelWidth`, `resampledLinesFollowANewKeyAxis`, `errorBarsOnResampledDataAreABand`,
  `fillFollowsADownsampledLine` (`mL2Result`). `test-color-by-scalar`: `setLineStyleInvalidatesLineCache`
  (`mLineCacheDirty`), the coloured pan/export group `.cpp:789-849`, L1-origin group `.cpp:467-483` (`mL1Cache`).
- Tests that depend on the **deferred swap timing**: all of `test-data-swap` (`requestsWithinWindow CommitOnce`,
  `requestAfterWindowCommitsAgain`, `swapWaitsForPanToSettle`, `swapCapForcesCommitWhile Panning`, the
  `multiGraph*Pending*`/`multiGraphCommitsAfterWindow` group, `graph2KeepsTranslationWhileDataPending`);
  `test-color-by-scalar`: `deferredSetColorValuesUsesThePendingSourceSize`, `deferredCommitAppliesTheColorSizeRule`,
  `firstColouredCommitSlotDeletingTheGraphIsSafe`; `test-pipeline`:
  `multiGraphPendingSourceCommitsAfterZoomTriggeredRefetch`,
  `multiGraphMultipleRapidLegitimateSupersessionsCommitLast`, `multiGraphRapidSetDataSource`,
  `graph2L2RebuildDeferredToDraw`, `graph2L2CoalescesMultipleViewportChanges`, `graph2L2DirtyAfterL1Ready`.
- Benchmarks (`tests/benchmark/benchmark.cpp`): **no row times QCPMultiGraph, QCPWaterfallGraph or QCPCurve** (grep
  for `multigraph|waterfall|curve` finds none). The line rows are all `QCPGraph` and `QCPGraph2`: `QCPGraph_Standard`
  :148, `QCPGraph_ManyPoints` :175, `QCPGraph_ManyLines` :208, `QCPGraph_ManyOffScreenLines` :241, the remove/add-data
  family :274-585, `QCPGraph2_SetDataDouble` :628, `QCPGraph2_SetDataFloat` :646, `QCPGraph2_RenderDouble` :665,
  `QCPGraph2_RenderFloat` :685, `QCPGraph2_RenderManyLines` :706, `QCPGraph2_ScatterDisc1M` :729,
  `QCPGraph2_ScatterSquare1M` :750, `QCPGraph2_ScatterWithLine1M` :771, `QCPInput_WheelOverGraph2_1M` :1450,
  `QCPGpu_Graph2Render1M` :1705. m4b has **no multigraph regression row** — worth adding before/after.

## 9. Public `QCPMultiGraph` API to keep

From `plottable-multigraph.h` only, so the port keeps the surface:

- Data: `setDataSource(unique_ptr)` :53, `setDataSource(shared_ptr)` virtual :54, `dataSource()` :55, `dataChanged()`
  virtual :56, `pipeline()` :58-59, `hasRenderedRange()` :60, `stallPixelOffset()` :61, `canProduceContent()` :62,
  `hasPendingData()` :63, `commitPendingData()` :64; convenience `setData` :67-74, `viewData` :77-83,
  `viewRowMajorData` :87-93.
- Components: `componentCount()` :96, `component(int)` :97/:106, `setComponentVisible` :101, `setComponentInLegend`
  :104, `legendComponents()` :105, `setComponentNames` :107, `setComponentColors` :108, `setComponentPens` :109,
  struct `Component` :36-47; `componentValueAt` :142; selection `componentSelection` :172 / `setComponentSelection`
  :173.
- Fills: struct `Fill` :111-115, `setComponentFill` :117, `setComponentFillBetween` :119, `setComponentFillBrush`
  :120, `clearComponentFill` :121, `componentFill` :122, `isValidColumn` :125, `effectiveFillBrush` :126.
- Error bars: struct `ErrorBars` :128-132, `setComponentErrorBars` :136-137, `clearComponentErrorBars` :138,
  `componentErrorBars` :139.
- Style: `lineStyle`/`setLineStyle`/`setComponentLineStyle` :145-147, `adaptiveSampling`/ `setAdaptiveSampling`
  :148-149, `gapThreshold`/`setGapThreshold` :151-152, `scatterSkip`/`setScatterSkip` :153-154.
- Colour by scalar: `setColorValues` :160-161, `clearColorValues` :162, `hasColorValues` :163,
  `setColorGradient`/`colorGradient` :164-165, `setColorRange`/`colorRange` :166-167,
  `setColorScaleType`/`colorScaleType` :168-169.
- `QCPPlottableInterface1D`: `dataCount` :176, `dataMainKey` :177, `dataSortKey` :178, `dataMainValue` :179,
  `dataValueRange` :180, `dataPixelPosition` :181, `sortKeyIsMainKey` :182, `selectTestRect` :183,
  `findBegin`/`findEnd` :184-185, `interface1D()` :188.
- `QCPAbstractPlottable`: `selectTest` :189-190, `getKeyRange` :191, `getValueRange` :193-195,
  `addToLegend`/`removeFromLegend` :200-201 (with `using` :198-199), `drawComponentLegendIcon` :204; signal
  `componentVisibilityChanged()` :208. Protected contract to reimplement: `draw(QCPPainter*)` :211, `drawLegendIcon`
  :212, `axesChanged` :213, `selectEvent` :215, `deselectEvent` :217.
