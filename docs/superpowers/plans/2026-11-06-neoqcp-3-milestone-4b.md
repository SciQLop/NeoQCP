# NeoQCP 3.0 milestone 4b — the line family Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `QCPMultiGraph` (with the waterfall) and `QCPCurve` draw through the drawing API on one shared
line base, a pan of a big graph stays as cheap as today, and new data replaces old data at once unless
the client opts into the deferred swap.

**Architecture:** First the command set is frozen (R3, then per-point colours). Then the drawing core
learns one new kind of recording: one valid for a key window at a key scale, which a pan inside the
window shifts and a zoom remakes. The data sources get data-space line building. The shared base
`QCPAbstractLine` takes the components, line style, scatter and colour API; the multigraph, then the
curve, record commands from it. `QCPGraph` and `QCPGraph2` are left untouched and deprecated.

**Tech Stack:** C++20, Qt 6.12 (QRhi), meson, QtTest, the in-tree benchmark (`tests/benchmark`).

**Spec:** `docs/specs/2026-11-06-neoqcp-3-milestone-4-design.md` (M4b; maintainer decisions "two
classes" and "new data replaces old data at once"; calls 3, 4; answers 8, 12, 14), with
`docs/specs/2026-11-02-neoqcp-3-object-model-design.md` (decision 11) and
`docs/specs/2026-11-04-neoqcp-3-drawing-api-design.md` (sections 2-4, 7). Facts:
`docs/plans/neoqcp-3/m4b-inventory.md` (file:line for everything this plan names).

## Global Constraints

- One build or test at a time, in the foreground, `ninja -C build-qt612 -j 4`. Never two at once.
- Env: `PATH=/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin:/home/jeandet/Qt/6.12.0/gcc_64/bin:$PATH`.
- Offscreen suite: `QT_QPA_PLATFORM=offscreen QT_LOGGING_RULES="*.debug=true" meson test -C build-qt612 --num-processes 4`;
  totals = sum of the `Totals:` lines of `build-qt612/meson-logs/testlog.txt`. GPU tests: the whole
  `auto-tests` binary under `xvfb-run -a -s "-screen 0 1600x1200x24" env QT_QPA_PLATFORM=xcb`.
- Bench gate (answer 14): baseline = end of milestone 3 in `baseline.md` (rows that did not exist then:
  the measurement Task 1 makes on the old code). `QCPGpu_*`, `QCPDrawn_*`, `QCPItems_*`, `QCPBars_*`
  and the rows Task 1 adds: 5 %; the rest 15 %. `-median 5`, three runs; a row over its gate gets six
  more runs and their median decides.
- Below the resample threshold (`kResampleThreshold`, 100 000 values) nothing is resampled, as today.
- `QCPGraph` and `QCPGraph2` are not modified (answer 8), except their doc comment and the changelog.
- Signals go through `qcpEmit` / `qcpEmitCurrent` / `qcpEmitIf` (`src/core.h`, milestone 4a).
- Errors: refuse misuse through `QCP_CHECK_OR_RETURN` (`src/misuse.h`), never an exception.
- A test fails before the change that makes it pass; a test that already passes is reported as a guard.
- Never bend library behaviour to please a test. Keep code size down: delete what a port replaces.
- Commit with explicit pathspecs; never `subprojects/.wraplock`, `subprojects/jkqtmathtext`, `qt.ini`.

## Review Focus

1. A pan of a 1M-point graph inside its key window replays the kept recording and never calls
   `draw()` (Task 3 test `aPanInsideTheKeyWindowKeepsTheRecording`, Task 6 test `panningABigGraphDoesNotRecord`).
2. A pan past the window, a zoom, or the 150 ms settle after a pan re-records with fresh lines: nothing
   stale and no blank frame (Task 6 tests `panPastTheWindowRecordsFreshLines`,
   `theSettleAfterAPanRecordsAgain`).
3. A log key axis (no L2, non-linear scale) still draws the right lines (Task 3 test
   `aLogKeyAxisRecordsEveryTransform`, Task 6 test `logKeyAxisLinesMatchTheData`).
4. An export right after `setData` of 1M points, before the first async resample, contains the lines
   (Task 6 test `exportBeforeTheFirstResampleHasTheLines`).
5. Every setter of the shared base changes the drawing, for both classes (Task 5 test
   `noSetterIsIgnored`, extended in Task 9).

---

### Task 1: Bench rows for the classes M4b ports, measured on the old code

**Files:**
- Modify: `tests/benchmark/benchmark.cpp`
- Modify: `docs/plans/neoqcp-3/baseline.md` (append "M4b start: rows added for the line family")

**Interfaces:**
- Produces: rows `QCPGpu_MultiGraphPan1M`, `QCPGpu_MultiGraphZoom1M`, `QCPGpu_MultiGraphColored100kPan`,
  `QCPMultiGraph_SetData1M`, `QCPGpu_CurvePan100k` (GPU rows skip offscreen like `QCPGpu_Pan100k`).

- [ ] **Step 1:** Add the rows, built like `QCPGpu_Pan100k` and `QCPGpu_Graph2Render1M`: a 3-column
  multigraph of 1 000 000 keys (sine values), panned by 1 % of the range per iteration; the same zoomed
  in place by 1 %; a 1-column multigraph of 100 000 keys with `setColorValues` panned; `setData` of
  1 000 000 keys x 3 columns timed alone; a `QCPCurve` of 100 000 points (a Lissajous) panned.
- [ ] **Step 2:** Run each three times (`-median 5`, the GPU ones under Xvfb). Expected: numbers, no skip
  under Xvfb.
- [ ] **Step 3:** Append the table to `baseline.md` (method and load as in "End of milestone 4a").
  These numbers are the gate for these rows.
- [ ] **Step 4:** Commit: "bench: line-family rows, measured before milestone 4b".

### Task 2: R3 — rect and ellipse batches (decided by measurement)

**Files:**
- Modify: `src/painting/draw-commands.h`, `src/painting/draw-context.{h,cpp}`,
  `src/painting/command-replay-rhi.cpp`, `src/painting/command-replay-painter.cpp`, `src/drawable.cpp`
  (hit test), `src/plottables/plottable-bars.cpp` only if its hit test reads `RectCmd`
- Test: `tests/auto/test-draw-context/` (or the class that already tests `rect()` recording; reuse it)

**Interfaces:**
- Produces, in `qcp`: `struct RectsCmd { std::vector<double> aKey, aValue, bKey, bValue; std::vector<QRgb>
  fills; QPen pen; QBrush brush; QCPDataBox bounds; bool sortedByKey; };` and `EllipsesCmd` with the same
  fields. `fills` is empty or one colour per rect (a solid brush of that colour replaces `brush` for that
  rect). Helpers unchanged: `rect()` and `ellipse()` append to the previous command when it is the same
  batch type with an equal pen and a brush of equal style, and both corners are `QCP::Data`; otherwise
  they record `RectCmd` / `EllipseCmd` as today. Once adopted, every later recorder of many rects or
  ellipses (intervals, histogram columns, error-bar caps, in M4c and M4e) uses the batch; no per-class
  variant.
- Produces: a hit test on a batch returns the rect's index in `details` (`QVariant(int)`).

- [ ] **Step 1: Write the failing tests:** `tenThousandDataRectsAreOneCommand` (10 000 `rect()` calls with
  one pen and brush → `drawList().commands.size() == 1`); `aPixelCornerStaysASingleRect` (a
  `QCP::Pixels` corner → a `RectCmd`); `perRectColoursStayOneBatch` (same pen, solid brushes of two
  colours → one command, `fills.size() == n`); `batchHitTestNamesTheRect` (index 7 of 10 hit →
  details == 7); `batchExportsOneRectPerBar` (painter replay into a `QPicture`: 10 `drawRect`-shaped
  records, or compare pixels with the `RectCmd` replay at 0 tolerance on the offscreen raster).
- [ ] **Step 2:** Run them. Expected: FAIL (commands.size() == 10 000, no `RectsCmd`).
- [ ] **Step 3:** Implement. GPU replay builds the same vertices `RectCmd` builds today, one batch at a
  time; painter replay draws each rect. Data bounds are kept as the recorder appends.
- [ ] **Step 4:** Run the tests, the offscreen suite and the Xvfb binary. Expected: PASS, suite green.
- [ ] **Step 5: Decide (answer 12).** Bench `QCPGpu_Bars10k`, `QCPDrawn_Zoom10k`, `QCPItems_Zoom10k`,
  `QCPBars_Pan10k`, `QCPGpu_BarsPan10k` (three runs). Measure the recording's bytes for 10 000 bars
  before and after (sum of `sizeof` and vector capacities; a debug-only helper in the test is fine).
  Adopted if every row is within 5 % of the end of milestone 3 and the recording is smaller. If not:
  revert Step 3, keep the tests that still hold, and record the numbers. Either way, append the table
  and the verdict to `docs/plans/neoqcp-3/m3-command-optimisation-survey.md` under "R3 decision".
- [ ] **Step 6:** Commit: "feat(draw): R3, rect and ellipse batches" (or "docs: R3 measured and
  declined").

### Task 3: Recordings valid for a key window

**Files:**
- Modify: `src/painting/draw-context.{h,cpp}`, `src/painting/draw-commands.h` (`QCPDrawList`),
  `src/drawable.cpp` (`recordingIsStale`, `stallOffset`), `src/painting/command-replay-rhi.cpp`
  (`qcpRecordingMovesWithTheData` and the recording cache's reuse)
- Test: `tests/auto/test-drawable/` (the class that tests recording reuse; reuse it)

**Interfaces:**
- Produces: `struct QCPKeyWindow { QCPRange keys; double pixelsPerKey; };` and
  `[[nodiscard]] QCPKeyWindow QCPDrawContext::keyWindow() const;`
  - `keys` is the view's key range widened by one view width on each side (as
    `qcp::algo::lineCacheKeyRange` does today); `pixelsPerKey` is the key axis's pixels per data unit.
  - Reading it records `QCPDrawList::keyWindow` (an `std::optional<QCPKeyWindow>`). It does **not** mark
    the list transform-dependent.
  - A recording with a key window is stale when the key scale changed (relative difference above 1e-9),
    or the view's key range is no longer inside `keys`. Otherwise a translation reuses it with one delta
    (R2), and a value-axis change re-resolves it from its commands.
  - On a logarithmic key axis, `keyWindow()` marks the list transform-dependent instead (pixels per key
    are not constant), with a `simplify:` comment naming the ceiling (a log-key pan re-records every
    frame) and the upgrade (a window in log units).

- [ ] **Step 1: Write the failing tests** (a test drawable that calls `keyWindow()` and records one
  data polyline over `keys`; count its `draw()` calls):
  `aPanInsideTheKeyWindowKeepsTheRecording` (pan by a quarter view width, replot → 0 new draws);
  `aPanPastTheKeyWindowRecords` (pan by 1.5 view widths → 1 draw); `aZoomRecords` (scale range by 0.5
  → 1 draw); `aValueZoomKeepsTheRecording` (value range doubled → 0 draws, pixels follow the value axis);
  `aLogKeyAxisRecordsEveryTransform` (log key axis, pan → 1 draw per frame);
  `keyWindowCoversOneViewWidthEachSide` (view 10..20 → keys exactly 0..30).
- [ ] **Step 2:** Run. Expected: compile failure (`keyWindow` missing). Record it.
- [ ] **Step 3:** Implement.
- [ ] **Step 4:** Run the tests, the offscreen suite, the Xvfb binary. Expected: PASS, green.
- [ ] **Step 5:** Commit: "feat(draw): a recording can be valid for a key window".

### Task 4: Per-point colours on polylines and markers

**Files:**
- Modify: `src/painting/draw-commands.h`, `src/painting/draw-context.{h,cpp}`,
  `src/painting/command-replay-rhi.cpp`, `src/painting/command-replay-painter.cpp`
- Test: the draw-context test class

**Interfaces:**
- Produces: `PolylineDataCmd` and `MarkersCmd` gain `std::shared_ptr<const std::vector<QRgb>> colours`
  (null = the pen's / style's colour). Helpers:
  `void polyline(std::shared_ptr<const std::vector<QPointF>> data, std::shared_ptr<const std::vector<QRgb>> colours, const QPen& pen);`
  `void markers(std::shared_ptr<const std::vector<QPointF>> data, std::shared_ptr<const std::vector<QRgb>> colours, const QCPScatterStyle& style);`
  A colour vector whose size differs from the data's is refused through the misuse tier (nothing
  recorded). A segment takes the colour of its first vertex, as `qcp::ColorRun` does today
  (`plottable-draw-utils.h:139-148`).
- The GPU replay reuses today's coloured paths (`drawColoredPolylineCached`'s run building,
  `addScatterColored`); the painter replay draws one polyline per colour run.

- [ ] **Step 1: Write the failing tests:** `colouredPolylineIsOneCommand`; `segmentTakesItsFirstVertexColour`
  (raster replay of a 3-point line red/green/blue: the first half-segment pixel is red, the second green);
  `colouredMarkersKeepTheirColours`; `mismatchedColourCountIsRefused` (misuse recorded, 0 commands).
- [ ] **Step 2:** Run. Expected: compile failure. Record it.
- [ ] **Step 3:** Implement.
- [ ] **Step 4:** Tests, offscreen suite, Xvfb binary. Expected: PASS, green.
- [ ] **Step 5:** Commit: "feat(draw): per-point colours on polylines and markers".

### Task 5: Data-space line building and the shared line base

**Files:**
- Modify: `src/datasource/abstract-multi-datasource.{h,cpp}`, `src/datasource/algorithms.h`,
  `src/datasource/soa-multi-datasource.h`, `src/datasource/row-major-multi-datasource.h`,
  `src/datasource/resampled-multi-datasource.h`, `src/plottables/plottable-waterfall.{h,cpp}` (its adapter)
- Create: `src/plottables/plottable-line.{h,cpp}` (register in `meson.build`)
- Modify: `src/plottables/plottable.{h,cpp}` (`setPen`/`setBrush` virtual),
  `src/plottables/plottable1d.h` (second template parameter), `src/plottables/plottable-multigraph.{h,cpp}`
  (moves its component, style and colour API to the base; drawing unchanged in this task)
- Test: `tests/auto/test-datasource/`, `tests/auto/test-multigraph/`

**Interfaces:**
- Produces: `virtual std::vector<QPointF> QCPAbstractMultiDataSource::linesInData(int column, int begin,
  int end, double pixelsPerKey, std::vector<int>* sourceIndices = nullptr) const;` data-space points,
  NaN-separated at gaps exactly where `getLines` breaks them; `pixelsPerKey <= 0` means no thinning;
  otherwise the same min/max envelope as `optimizedLineDataImpl` (one bucket per key pixel, up to 4
  points per bucket), with keys back in data units. An L2 source never thins again (as today).
  `sourceIndices`, when given, receives each point's source row (the coloured path).
- Produces: `class QCP_LIB_DECL QCPAbstractLine : public QCPAbstractPlottable` (Q_OBJECT) holding what
  decision 11 shares: `enum LineStyle { lsNone, lsLine, lsStepLeft, lsStepRight, lsStepCenter, lsImpulse }`;
  `struct Component` (today's `QCPMultiGraph::Component`, which becomes `using Component =
  QCPAbstractLine::Component`); `componentCount()`, `component(int)`, `setComponentVisible`,
  `setComponentInLegend`, `setComponentNames`, `setComponentColors`, `setComponentPens`,
  `setLineStyle` / `lineStyle` / `setComponentLineStyle`, `setScatterStyle` (all components) /
  `setComponentScatterStyle`, `setScatterSkip` / `scatterSkip`, the colour-by-scalar setters
  (`setColorValues`, `clearColorValues`, `hasColorValues`, `setColorGradient`, `setColorRange`,
  `setColorScaleType` and getters), signal `componentVisibilityChanged()`. Every setter that changes
  the drawing calls the generation bump; a value it cannot draw is refused through the misuse tier.
- Produces: `QCPAbstractPlottable::setPen` and `setBrush` are `virtual`. `QCPAbstractLine` overrides
  them: `setPen` sets every component's pen; `setBrush(Qt::NoBrush)` clears every component's fill,
  any other brush fills every component to the value-axis zero baseline (as 2.x `QCPGraph`).
- Produces: `template <class DataType, class Base = QCPAbstractPlottable> class QCPAbstractPlottable1D`
  (curve uses `Base = QCPAbstractLine` in Task 9). Existing uses compile unchanged.

- [ ] **Step 1: Write the failing tests:** `linesInDataMapToGetLines` (soa, row-major, resampled, waterfall
  adapter sources; unthinned: `QCPTransform.map(linesInData(...))` equals `getLines(...)` to 1e-6 px,
  NaN positions identical); `thinnedLinesInDataMatchTheOptimizedPixels` (1M points, the same point count
  and the same pixels to 1e-6 px as `getOptimizedLineData`); `indicesFollowTheThinnedPoints`;
  `noSetterIsIgnored` (data-driven over `setPen`, `setBrush`, `setLineStyle`, `setScatterStyle`,
  `setScatterSkip`, `setColorValues` on a multigraph: each bumps the generation and changes the raster of
  one replot; today `setPen` changes nothing it draws).
- [ ] **Step 2:** Run. Expected: compile failure for the first three; `noSetterIsIgnored` FAIL on `setPen`
  and `setBrush`.
- [ ] **Step 3:** Implement. The base takes the members as they are in the multigraph today; the
  multigraph keeps every public name (inventory §9) through the base or a `using`.
- [ ] **Step 4:** Tests, offscreen suite. Expected: PASS, green.
- [ ] **Step 5:** Commit: "feat(lines): data-space line building and the shared line base".

### Task 6: The multigraph records its lines, markers and colours

**Files:**
- Modify: `src/plottables/plottable-multigraph.{h,cpp}` (derives `QCPDrawn<QCPAbstractLine>` and
  `QCPPlottableInterface1D`)
- Test: `tests/auto/test-multigraph/`, `tests/auto/test-color-by-scalar/`, `tests/auto/test-pipeline/`

**Interfaces:**
- Consumes: `keyWindow()` (Task 3), coloured `polyline`/`markers` (Task 4), `linesInData` and
  `QCPAbstractLine` (Task 5).
- Produces: `void QCPMultiGraph::draw(QCPDrawContext&) const` records, per visible component: its
  line over `keyWindow().keys` (step and impulse shapes through `plottable-linestyle.h`, in data space),
  with the selected pen when the component has a selection, coloured when colour values apply; its
  markers at the original data positions with `scatterSkip`. Fills and error bars stay on the old path
  until Task 7, through a `painter(...)` command recorded only when a component has a fill or error
  bars, so nothing disappears in between (that command is re-recorded on every view change until Task 7).
- The L2 rebuild stays where the fresh lines are made, now `draw()` (mutable caches, drawing-spec §2).
  A finished L1, an L2 rebuild and the 150 ms settle after a pan bump a mutable
  `dependencyRevision()` counter instead of `markLinesDirty()`. With no L1 yet and resampling needed,
  `draw()` records nothing, except for an export, which resamples synchronously as today (the mechanism
  is the builder's; the test pins it).
- `hitTest` is overridden from today's `selectTest` (data-point selection per component), not from the
  recording.
- Deleted: `mCachedLines`, `mCachedIndices`, `mRenderedRange`, `mHasRenderedRange`, `mLineCacheDirty`,
  `mCachedPlotSize`, `mExtrusionCaches`, `lineCacheWindow`, and the calls to `evaluateLineCache`.
  `hasRenderedRange()` stays public, answered from whether a recording exists.

- [ ] **Step 1: Write the failing tests** (in `TestMultiGraph`): `panningABigGraphDoesNotRecord`
  (1M points, no fill and no error bars, after the first resample: ten quarter-width pans → `draw()`
  not called; count it with a test-only counter on the drawn core, read through the existing friend);
  `panPastTheWindowRecordsFreshLines`; `theSettleAfterAPanRecordsAgain` (wait 200 ms → one new
  recording); `logKeyAxisLinesMatchTheData` (log key axis, 1 000 points: rasterised line pixels equal
  the 2.x-style pixel lines' to 1 px); `exportBeforeTheFirstResampleHasTheLines` (`setData` 1M points,
  `toPixmap` at once: pixels of the component's colour present); `colouredLineIsOneCommandPerComponent`;
  `waterfallDrawsItsOffsets` (3 rows with offsets: each row's pixels sit at its offset).
- [ ] **Step 2:** Run. Expected: FAIL (no drawing-API recording yet).
- [ ] **Step 3:** Implement. Rewrite the tests that read the deleted caches (inventory §8 lists them) to
  assert behaviour (pixels, draw counts, recording counts); delete none without a replacement that
  checks the same fact; list each rewrite in the report.
- [ ] **Step 4:** Offscreen suite and Xvfb binary green. Bench `QCPGpu_MultiGraphPan1M`,
  `QCPGpu_MultiGraphZoom1M`, `QCPGpu_MultiGraphColored100kPan`, `QCPGpu_Pan100k`,
  `QCPInput_MouseMoveStorm` against their gates. A row over its gate: report it with a profile
  (`PROFILE_HERE_N` zones, `tests/benchmark` with the tracer) before any change; the controller rules.
- [ ] **Step 5:** Commit: "feat(multigraph): lines and markers through the drawing API".

### Task 7: The multigraph's fills, error bars and legend icons

**Files:**
- Modify: `src/plottables/plottable-multigraph.cpp`, `src/plottables/multigraph-fill.{h,cpp}`,
  `src/plottables/multigraph-errorbars.{h,cpp}`
- Test: `tests/auto/test-multigraph/`

**Interfaces:**
- Fills become `fillBetween` commands with equal-length upper and lower series built in data space by
  today's `fillRows` / `shapedFillRows` (so the GPU ribbon path takes them, `command-replay-rhi.cpp:569-582`);
  `binnedWhenDense` keeps its rule. Error bars become `errorBars` commands; the dense band a
  `fillBetween` at alpha 64, as today. Order kept: fills, error bars, then lines and markers.
- `drawLegendIcon(QCPDrawContext&)` records today's per-component icon; `drawComponentLegendIcon` keeps
  its public signature.
- Deleted: the `painter(...)` bridge of Task 6, `drawFillOnGpu`, `paintFill` and the error-bar helpers
  the commands replace.

- [ ] **Step 1: Write the failing tests:** `fillIsOneRibbonCommand`, `fillBetweenComponentsIsOneRibbon`,
  `errorBarsAreOneCommand`, `denseErrorBarsAreABand`, `fillsDrawUnderErrorBarsUnderLines` (pixel order on
  a raster replay), `legendIconShowsEachComponent`.
- [ ] **Step 2:** Run. Expected: FAIL (they are `painter` commands).
- [ ] **Step 3:** Implement.
- [ ] **Step 4:** Suites green; bench the Task 6 rows again.
- [ ] **Step 5:** Commit: "feat(multigraph): fills, error bars and icons through the drawing API".

### Task 8: New data is seen at once; the deferred swap is opt-in

**Files:**
- Modify: `src/plottables/plottable-multigraph.{h,cpp}`
- Test: `tests/auto/test-data-swap/`, `tests/auto/test-multigraph/`, `tests/auto/test-waterfall/`

**Interfaces:**
- Produces: `void QCPMultiGraph::setDeferredDataSwap(bool)` and `bool deferredDataSwap() const`, default
  `false`. Off: `setDataSource` applies at once (`applySourceNow`), so `dataCount()`, `getKeyRange()`,
  `rescaleAxes()`, hit tests and an export see the new data when it returns. On: today's staged swap
  (`stagePendingSource` … `commitPendingData`), unchanged.

- [ ] **Step 1: Write the failing tests:** `newDataIsSeenAtOnceByDefault` (rendered graph, `setData` of
  other keys → `getKeyRange()` and `dataCount()` are the new ones before any event loop);
  `rescaleAxesSeesNewDataAtOnce`; `deferredSwapKeepsTheOldDataUntilCommit` (option on: today's
  behaviour).
- [ ] **Step 2:** Run. Expected: the first two FAIL; the third passes (guard).
- [ ] **Step 3:** Implement. `test-data-swap`, and the swap-timing tests the inventory lists in
  `test-color-by-scalar` and `test-pipeline`, turn the option on; the waterfall's racing-source comment
  (`plottable-waterfall.cpp:196-208`) is updated for the default.
- [ ] **Step 4:** Suites green. Changelog: "Behaviour changes for 2.x users" — new data on a multigraph is
  seen at once; `setDeferredDataSwap(true)` keeps the old data drawn until the new is ready.
- [ ] **Step 5:** Commit: "feat(multigraph): new data is seen at once; the deferred swap is opt-in".

### Task 9: `QCPCurve` on the shared base and the drawing API

**Files:**
- Modify: `src/plottables/plottable-curve.{h,cpp}`
- Test: `tests/auto/test-qcpcurve/`, `tests/auto/test-multigraph/` (`noSetterIsIgnored`)

**Interfaces:**
- Produces: `class QCPCurve : public QCPDrawn<QCPAbstractPlottable1D<QCPCurveData, QCPAbstractLine>>`.
  One component. `draw()` records one data polyline of the whole curve (the clip is the recording's,
  so the data-space clipping code `getCurveLines` … `getTraverseCornerPoints`, ~1100 lines, is deleted),
  markers with `scatterSkip`, the fill as a polygon of data positions, and colours by value through
  Task 4's coloured polyline. The selected segments draw with the selected pen, as today.
- A line style other than `lsNone` / `lsLine` is refused through the misuse tier.
- `QCPCurve::LineStyle` stays as `using LineStyle = QCPAbstractLine::LineStyle` so 2.x code compiles.

- [ ] **Step 1: Write the failing tests:** `curveIsOnePolylineCommand`, `curveRefusesAStepStyle`,
  `curveColoursByValue`, `curveFillIsAPolygon`, `selectedSegmentUsesTheSelectedPen`, and the curve rows of
  `noSetterIsIgnored`. Keep `dataManipulation` and `dataSharing`.
- [ ] **Step 2:** Run. Expected: FAIL (compile for the coloured and base parts).
- [ ] **Step 3:** Implement.
- [ ] **Step 4:** Suites green; bench `QCPGpu_CurvePan100k` against its Task 1 number.
- [ ] **Step 5:** Commit: "feat(curve): the shared line base and the drawing API".

### Task 10: Deprecate the old graphs and finish

**Files:**
- Modify: `src/plottables/plottable-graph.h`, `src/plottables/plottable-graph2.h` (doc `\deprecated` only),
  `changelog.txt`, `docs/plans/neoqcp-3/baseline.md`
- Create: `docs/plans/neoqcp-3/milestone-4b-rulings.md`

- [ ] **Step 1:** `\deprecated` on `QCPGraph` and `QCPGraph2`: "use QCPMultiGraph with one column; this
  class goes in 3.0". No `[[deprecated]]` attribute (it would warn in the 58 files milestone 5 moves).
- [ ] **Step 2:** Changelog, milestone 4b section: the shared line base, `setPen`/`setBrush` virtual and
  honoured by the multigraph, curve colour by value, the immediate swap, the deprecations.
- [ ] **Step 3:** Offscreen suite, ASan suite, Xvfb binary, clang `-fsyntax-only` on every changed
  `.cpp`, full bench table appended as "End of milestone 4b".
- [ ] **Step 4:** Rulings and deferred minors to `milestone-4b-rulings.md`. Commit.
