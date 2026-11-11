# NeoQCP 3.0 milestone 5a — the graph grows up Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** before the old graph classes are deleted (5c), `QCPMultiGraph` (the future `QCPGraph`) gains everything
users of the old graph need: drawing on any straight-segment coordinate system, `addData`, one-column `setData`, a
fill to another graph, and a tracer that follows it. Its cartesian behaviour and speed do not move.

**Architecture:** the multigraph keeps its cartesian path (resampler, thinning, GPU) byte for byte and gains a
non-cartesian branch that works on the displayed source in data space, as 4j did for the old graph. `addData`
appends into a source the graph owns. A fill partner is another graph's component, cropped and interpolated in data
space. The tracer reads keys through the 1D interface and values through `componentValueAt`.

**Tech Stack:** C++20, Qt 6.12, meson + `cmake/sources.cmake`, QtTest, `tests/benchmark`.

**Spec:** `docs/specs/2026-11-11-neoqcp-3-milestone-5-design.md` (M5-1, M5-2, C2, C4, C5 and amendments A1-A5, A7,
A8, A9, A12, A13 — the amendments override the calls). Facts: `docs/plans/neoqcp-3/m5-inventory.md` (§3 is the
line family, with file:line), `docs/plans/neoqcp-3/milestone-4j-rulings.md` (the old graph's data-space path this
plan mirrors: `plottable-graph.cpp` `dataSpaceLines`, `dataSpaceFillPolygon`, `dataSpaceChannelFillPolygon`,
`mapToPixels`), the 4j fixture `tests/auto/test-user-system/skew-system.h`. The maintainer answered M5-1..M5-4 and
said "you can start the implementation once you are done with the plan" (2026-11-11); calls are delegated.

## Calls made in this plan (P-1..P-8; the maintainer can overrule any)

- **P-1** `QCPGraphData` and `QCPGraphDataContainer` move to `src/plottables/graph-data.h` (public, in `qcp.h`, meson,
  cmake); `plottable-graph.h`, `polar/polargraph.h` and `items/item-tracer.h` include it. No behaviour change.
- **P-2** `QCPDrawContext::keyWindow()` asks the context's system when it has one (`QCPCoordinateSystem::keyWindow()`);
  only a cartesian frame keeps today's axis computation. Every caller treats an empty window as unbounded (the
  multigraph today treats it as [0, 0], `plottable-multigraph.cpp:1119-1124`). This also fixes `QCPCurve` on user
  systems if it reads the window.
- **P-3** `QCPMultiGraph`'s mask becomes `capStraightSegments` only. `QCPWaterfallGraph` keeps the rectangle mask
  (`capRectangular | capStraightKey`) — its offsets are pixel stacking; a waterfall on a user system is refused with
  the capability misuse (documented limit).
- **P-4** Non-cartesian branch (A1, A9): decided once per draw by `drawsInDataSpace()` = system present and no
  projection. It uses the displayed source with `pixelsPerKey = 0` (no thinning, no L1/L2), the system key window,
  fills with data-space baselines (value 0, or the log range bound nearer zero, read through `valueAxisBase()`),
  error-bar caps converted through the transform at the bar's own point, and colour scales joined through
  `system()->area()` when that area is a `QCPAxisRect` (`followKeyAxisRectScale` rewritten to read the area).
  `selectTest`, `selectTestRect`, `canProduceContent`, `hitTest` and `dataPixelPosition` gate on the system and map
  through the transform (one batch per call). A `simplify:` comment names the ceiling (every point mapped per
  frame and per click) and the upgrade (the J-6 box query, data-space decimation by `keyWindow()`).
- **P-5** `addData` (A2): the graph owns a growable SoA source when its data came from `setData`/`addData` (a flag on
  the graph, set by those calls and cleared by `setDataSource`/`viewData`/`viewRowMajorData`). Appends keep keys
  sorted the way `setData` requires (an append with keys below the last key inserts in order, as 2.x did; the 2.x
  "already sorted" fast path is kept). On a non-owned source: misuse, no change. With a deferred swap pending: the
  append goes to the staged source if owned, else misuse. Signatures (class and ops): `addData(const QVector<double>&
  keys, const QVector<double>& values)`, `addData(double key, double value)` (one-column graphs; misuse otherwise),
  `addData(keys, std::vector<QVector<double>> columns)`, `addData(double key, std::span<const double> values)`.
  One-column `setData(keys, values)` overloads for `QVector<double>` and `std::vector<double>` (class and ops).
- **P-6** Fill to another graph (A3): `setComponentFillToGraph(int component, QCPMultiGraph* other, int otherComponent,
  const QBrush& brush = {})` with a matching `Fill` alternative holding `QPointer<QCPMultiGraph>` + component. The
  band: both series cropped to the common key range, the partner linearly interpolated onto this graph's keys inside
  it (equal lengths, GPU ribbon kept), recorded with the existing `fillBetween`. Different systems: misuse at the call
  and the band is not drawn if they diverge later. The fill drops when the partner dies or loses the component. This
  graph's selection splits the band like its line. The partner's data change marks this graph dirty (dependency).
- **P-7** Tracer (A8): `QCPItemTracer::setGraph(QCPMultiGraph* graph, int component = 0)`, `setComponent(int)`,
  `component()`; the ops handle gets the same; it reads keys through `interface1D()` and the value through
  `componentValueAt(component, index)`; the position takes the graph's system (as 4j). The old
  `setGraph(QCPGraph*)` and `graph()` stay until 5c; the tracer holds one target (setting one clears the other).
- **P-8** Bench twins (A12): `QCPMultiGraph_*` rows mirroring every `QCPGraph_*` workload except `RemoveData*`
  (`Standard`, `ManyPoints`, `ManyLines`, `ManyOffScreenLines`, the six `AddData*Sorted/Unsorted`, the three
  `AddDataSingle*`, using `addData`), and mirrors of the `QCPGraph2_*`, `QCPInput_WheelOverGraph2_1M` and
  `QCPGpu_Graph2Render1M` workloads on the multigraph. Their end-of-5a medians are the 5c gate.

## Global Constraints

- One build or test at a time, in the foreground, `ninja -C build-qt612 -j 4`, timeout 600000, long output to a log
  file and only its tail printed. Env in the same command:
  `PATH=/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin:/home/jeandet/Qt/6.12.0/gcc_64/bin:$PATH`,
  `PKG_CONFIG_PATH=/home/jeandet/Qt/6.12.0/gcc_64/lib/pkgconfig`. Never `meson setup` in the worktree.
- Every added or removed source/header in `meson.build`, `cmake/sources.cmake`, `src/qcp.h`; new test directories
  registered like the others.
- Offscreen suite; Xvfb whole binary (no leaked Xvfb of ours); ASan on touched classes per task, whole ASan suite
  after the last code change; clang check on every changed `.cpp`.
- Every existing reference picture keeps matching. A new picture is looked at by the controller before it is
  accepted.
- Bench gate: every `QCPMultiGraph_*`, `QCPGpu_MultiGraph*`, `QCPDrawn_*` and drawing row within 5 % of the task's base
  (A/B, five pairs, `-tickcounter -iterations 50 -median 5`, ten for a row over); others 15 %. The cartesian
  multigraph must not move.
- 2.x and existing multigraph behaviour on cartesian systems unchanged; the old graph classes untouched except the
  include of `graph-data.h` and the tracer's second target.
- No shims; misuse via `QCP_CHECK_OR_RETURN`; signals via the 4a helpers; `contentChanged()` on every input the
  recording reads; tests fail before the change; never bend library behaviour to please a test; no whole-file
  clang-format.
- Commit with explicit pathspecs; never `subprojects/.wraplock`, `subprojects/jkqtmathtext`, `qt.ini`. `/tmp` is RAM:
  remove worktrees and perf data. Never push.

## Review Focus

1. Cartesian multigraph unchanged: every `test-multigraph`, `test-waterfall`, `test-pipeline`, `test-data-swap`,
   `test-color-by-scalar`, `test-busy-indicator` slot green unchanged; `QCPMultiGraph_*`/`QCPGpu_MultiGraph*` level.
2. A multigraph on a user system with large data, a log value axis, NaN rows, N columns, per-component fills and
   error bars, colour by scalar; clicks and rubber band through the mapping; an axis deleted mid-drag.
3. `addData` on every source kind (owned, view, shared, waterfall adapter, pending swap), unsorted appends, N columns,
   the async pipeline seeing the new revision, a mid-append replot.
4. Fill to another graph: different key grids and lengths, no overlap, partner deleted or its component removed,
   partner moved to another system, selection of a middle range.
5. The tracer on a multigraph component with gaps and outside the data range, its graph deleted, retargeted from an
   old graph.

---

### Task 1: `graph-data.h` and the system's key window

**Files:** new `src/plottables/graph-data.h` (P-1), `src/plottables/plottable-graph.h`, `src/polar/polargraph.h`,
`src/items/item-tracer.h`, build lists; `src/painting/draw-context.{h,cpp}` (P-2) and every `keyWindow()` caller
(grep); tests in `tests/auto/test-draw-context` and `tests/auto/test-user-system`.

- [ ] **Step 1: Failing tests:** `theKeyWindowComesFromTheSystem` (a user system whose `keyWindow()` returns a range:
  the context answers it; a default user system: an empty, unbounded window), `aCurveOnAUserSystemIsNotCulledByAnEmptyWindow`
  (if the curve reads it; else say so). Expected: FAIL.
- [ ] **Step 2: Implement** P-1 (pure move) and P-2.
- [ ] **Step 3:** the touched test classes, the whole suite offscreen and under Xvfb, ASan on them, clang; A/B the
  `QCPDrawn_*` and `QCPMultiGraph_*` rows.
- [ ] **Step 4: Commit** `refactor(graph): QCPGraphData in its own header; the key window comes from the system`.

### Task 2: The multigraph on any straight-segment system

**Files:** `src/plottables/plottable-multigraph.{h,cpp}`, `multigraph-fill.{h,cpp}`, `multigraph-errorbars.{h,cpp}`,
`plottable-waterfall.h` (P-3), `plottable-colormapped.cpp` (`followKeyAxisRectScale`, P-4), `plottable-line.cpp` if
the colour start reads the axis; `tests/auto/test-user-system/`; `tests/auto/test-reference-m4e` (new scene
`user-system-multigraph`: shear 0.5, a two-column graph with a step style, a fill to a constant, a fill between its
columns, error bars, scatter-only, colour by scalar; taken on this task's code and looked at by the controller).

- [ ] **Step 1: Failing tests** (`TestUserSystem`, on the skew fixture):
  `aMultiGraphJoinsASkewRect` (mask is `capStraightSegments`; `add<QCPMultiGraph>` on the skew rect works);
  `aWaterfallIsRefusedOnASkewRect`; `aMultiGraphLineFollowsTheSkewMapping` (each column);
  `aMultiGraphStepAndImpulseAreBuiltInDataSpace`; `aLargeMultiGraphOnASkewRectDrawsEveryPoint` (100k rows: no blank
  frame, no crash, the last point inked); `aMultiGraphFillOnALogAxisStopsAtTheBoundNearerZero`;
  `aMultiGraphNaNSplitsLineAndFill`; `multiGraphErrorBarCapsFollowTheMapping`;
  `colourByScalarJoinsTheSkewRectsScale`; `aMultiGraphIsHitAndRectSelectedThroughTheMapping`;
  `aMultiGraphDataPixelPositionIsTheMapping`; `anAxisDeletedMidDragLeavesAMultiGraphInert`; the
  `user-system-multigraph` reference. Expected: FAIL.
- [ ] **Step 2: Implement** P-3, P-4.
- [ ] **Step 3:** as Task 1 step 3, plus every multigraph-family test class (Review Focus 1) and every
  `test-reference-*`. A/B every `QCPMultiGraph_*` and `QCPGpu_MultiGraph*` row: the cartesian path must not move.
- [ ] **Step 4: Commit** `feat(graph): the multigraph draws in any straight-segment coordinate system`.

### Task 3: `addData` and one-column `setData`

**Files:** `src/plottables/plottable-multigraph.{h,cpp}` (class and `QCPMultiGraphOps`), `src/datasource/soa-multi-datasource.h`
(a growable owned source, or a new one beside it), `tests/auto/test-multigraph/`.

- [ ] **Step 1: Failing tests:** `addDataAppendsToAnOwnedSource` (keys/values, one column; then N columns);
  `addDataInsertsUnsortedKeysInOrder`; `addDataOnAViewIsAMisuse`, `...OnASharedSource...`, `...OnAWaterfall...`;
  `addDataWithAPendingSwapAppendsToTheStagedSource`; `addDataColumnCountMismatchIsAMisuse`;
  `aPipelineSeesTheAppend` (resampled graph redraws the appended range); `oneColumnSetDataTakesQVectors` (class and
  `plot.add<QCPMultiGraph>(x, y)`); `addDataSinglePointOnAMultiColumnGraphIsAMisuse`. Expected: FAIL.
- [ ] **Step 2: Implement** P-5.
- [ ] **Step 3:** as Task 2 step 3 for the data classes (`test-multigraph`, `test-multi-datasource`, `test-pipeline`,
  `test-data-swap`, `test-handle`, `test-builder`). Add the P-8 `AddData*` twins here and record their medians.
- [ ] **Step 4: Commit** `feat(graph): the multigraph appends data and takes one column as two vectors`.

### Task 4: A fill to another graph

**Files:** `src/plottables/multigraph-fill.{h,cpp}`, `plottable-multigraph.{h,cpp}`, `tests/auto/test-multigraph/`,
`tests/auto/test-user-system/`.

- [ ] **Step 1: Failing tests:** `aFillToAnotherGraphFillsTheBand` (same keys); `aFillToAnotherGraphCropsAndInterpolates`
  (keys 0,0.5,1,1.5 against 0,1: band pixels between the two lines only over [0, 1]); `noOverlapNoBand`;
  `theFillDropsWhenThePartnerDies` and `...WhenThePartnerLosesTheComponent` (no crash, ASan);
  `aFillToAGraphOfAnotherSystemIsAMisuse` and `aPartnerMovedToAnotherSystemDrawsNoBand`;
  `aSelectedRangeSplitsTheBand`; `aPartnerDataChangeRedrawsTheBand`; `theBandStaysOnTheGpuRibbon` (equal series
  lengths reach the ribbon; GPU test under Xvfb); on the skew fixture `aFillToAnotherGraphOnASkewRect`.
  Expected: FAIL.
- [ ] **Step 2: Implement** P-6.
- [ ] **Step 3:** as Task 2 step 3.
- [ ] **Step 4: Commit** `feat(graph): a component fill can reach another graph`.

### Task 5: The tracer follows the multigraph; the bench twins

**Files:** `src/items/item-tracer.{h,cpp}` (P-7), `tests/auto/test-item-system/` or a tracer test class,
`tests/benchmark/benchmark.cpp` (P-8).

- [ ] **Step 1: Failing tests:** `theTracerFollowsAMultiGraphComponent` (component 1 of 3, interpolated and
  stepped styles as the 2.x tracer does); `theTracerOutsideTheDataClampsAs2xDid`; `theTracerOnAGapIsInert` (NaN);
  `deletingTheTracedMultiGraphLeavesTheTracerInert`; `retargetingFromAnOldGraphClearsIt`;
  `theTracerFollowsAMultiGraphOnASkewRect`; the ops handle form. Expected: FAIL.
- [ ] **Step 2: Implement** P-7; add every P-8 twin row not added in Task 3.
- [ ] **Step 3:** as Task 2 step 3, plus `test-item-*` and `test-reference-m4e` (the `item-tracer` picture unchanged).
  Run the new bench rows (three runs) and record their medians in the report.
- [ ] **Step 4: Commit** `feat(items): the tracer follows a multigraph component; bench twins of the graph rows`.

### Task 6: Docs and finish

**Files:** `docs/graphs.md` (the multigraph's `addData`, one-column `setData`, fill to another graph, user
systems), `docs/items.md` (tracer), `docs/coordinate-systems.md` (the multigraph and the waterfall rows of the table),
`docs/migrating.md`, `changelog.txt` (milestone 5a), `docs/plans/neoqcp-3/baseline.md` (end of 5a, including the twin
rows), `backlog.md`, `milestone-5a-rulings.md`, `sciqlopplots-migration.md`.

- [ ] **Step 1:** Final whole-branch review (fresh Opus reviewer and fresh opencode reviewer), one fix batch.
- [ ] **Step 2:** Offscreen suite, Xvfb whole binary, whole ASan suite after the last code change, clang on every
  changed `.cpp`.
- [ ] **Step 3:** Whole bench table against the end of 4j; A/B any row over its gate against a build of the 5a base.
- [ ] **Step 4:** Docs, changelog, rulings, backlog, migration notes.
- [ ] **Step 5: Commit** `docs(neoqcp-3): end of milestone 5a — rulings, baseline, backlog, changelog`.

---

## Amendments after review (binding; they override the calls and the task text above)

Review: `.superpowers/m5a-plan-review.md` (opencode).

- **B1 (C1, I1, I2, I5, M6) Task 2 is split.**
  - **Task 2a** covers lines, steps, impulses, scatters, hit, rect selection, `dataPixelPosition` and the pipeline
    bypass (B2).
  - **Task 2b** covers fills, error bars and colour by scalar.
  - In 2b every 2.x-axis read on the non-cartesian branch goes through one function family:
    - `fillRows`/`fillValue` (`:1286`, `:1300-1305`), `recordFills` (`:1360`), `binnedWhenDense` (`:1316-1343`,
      skipped on that branch: no pixel binning), `errorBarRows` (`:1385`), `errorBarsFitInView` (`:1398-1410`) and
      `recordErrorBars` (`:1467-1469`).
    - The non-cartesian baseline is a new sign-aware function: value 0, or on a log axis the bound nearer zero.
      `fillValue` stays as it is for cartesian (byte for byte). Its negative-log behaviour on cartesian goes to the
      backlog, not changed here.
    - Error bars on that branch use the `ErrorBarsCmd::ends` form (pixel whiskers perpendicular to the mapped bar).
  - A feature that cannot be made right is refused (A9) with a misuse at its setter and at `setSystem`, each with a
    test. The reference scene goes in 2b.
- **B2 (C2) The pipeline on the non-cartesian branch.** While `drawsInDataSpace()` is true:
  - `mNeedsResampling` is false, and no pipeline job is started or consulted (`applySourceNow` `:158-160` asks the
    branch).
  - `draw` uses `mDataSource` directly, never `mL2Result`/`mL1Cache`.
  - `canProduceContent` answers from the source.
  - `setSystem`/`systemChanged` clears L1/L2 and recomputes `mNeedsResampling` both ways (to and from a user system).
  - Tests: `switchingToASkewRectDropsTheBins` and back, with a 100k-row graph.
- **B3 (I3) Hit and rect selection.**
  - `selectTestRect` on the branch maps every candidate row once (one batch) and keeps those inside the pixel rect,
    as the 4j curve does.
  - `hitTest`/`nearestDataPoint` on the branch is a 2D nearest search over the mapped points (no key window).
- **B4 (I4) A system mapping change re-records.** The multigraph's `dependencyRevision()` includes
  `system()->revision()` on the branch (cartesian unchanged). Test: `aShearChangeRedrawsAMultiGraph` with a moved
  pixel.
- **B5 (M2)** `drawsInDataSpace()` is `mSystem && mSystem->kind() != QCPTransform::Kind::Cartesian`, not "no
  projection".
- **B6 (C3, C4, I8) `addData`.**
  - A new `QCPGrowableMultiDataSource` (keys and N value columns in `std::vector<double>`, a revision, `append`)
    lives in `src/datasource/`. It is the only source `addData` writes to. Every `setData` overload (class and ops,
    one-column and N-column) builds it when the inputs are `double`; other value types keep
    `QCPSoAMultiDataSource`, and `addData` on those is the non-owned misuse.
  - The graph keeps a typed `std::shared_ptr<QCPGrowableMultiDataSource> mOwnedSource` next to `mDataSource`, cleared
    by every other source setter.
  - `QCPWaterfallGraph` refuses `addData` (misuse, its source is an adapter).
  - With a deferred swap pending, the append goes to the pending source when it is the owned one. A new
    `appendChanged()` path re-stages that same source (new generation, pipeline re-run) instead of `dataChanged()`,
    which re-stages the displayed one.
  - Signatures add `bool alreadySorted = false` to the vector forms, as 2.x.
- **B7 (C5, I7) The tracer.**
  - It gets one new target member `QPointer<QCPMultiGraph> mLineGraph` + `mComponent`, and `lostGraph()`,
    `dataSourceIdentity()`, `dependencyRevision()` and `updatePosition()` each get the branch.
  - The new setter is named `setLineGraph(QCPMultiGraph*, int component = 0)` (with `lineGraph()`, `setComponent`,
    `component`). No overload of `setGraph`, so `setGraph(nullptr)` and the `Q_PROPERTY` stay unambiguous.
  - The ops handle mirrors it.
  - In 5c the old `setGraph(QCPGraph*)` goes and `setLineGraph` is renamed `setGraph` (one name in 3.0).
  - `setComponent` out of range is a misuse.
- **B8 (C6, I6, M3) Fill to another graph.**
  - The partner relation uses `partner->addDependent(this)`. `QCPMultiGraph::linesChanged()` calls
    `markDependentsDirty()`; check that this does not move the cartesian bench.
  - `dependencyRevision()` hashes the partner's revision.
  - The crop and interpolation helper lives in `multigraph-fill.{h,cpp}` as a pure function (tested on its own:
    unequal grids, no overlap, NaN gaps split runs). It produces equal-length series for `fillBetween`.
  - The selection split: `recordFills` records one `fillBetween` per selected/unselected run of this graph's
    component selection. The selected run takes the selected pen's colour at the fill alpha, as `effectiveFillBrush`
    does for the unselected one.
  - The GPU-ribbon test uses the effective solid brush.
  - Test `aPartnerOnAnotherLayerRedrawsTheBand`.
- **B9 (I9, I10, I11, M1)**
  - The existing mask assertions (`test-coordinate-system.cpp:669-675`, `test-user-system.cpp:385`) change in
    Task 2a.
  - `aCurveOnAUserSystemIsNotCulledByAnEmptyWindow` is replaced by a draw-context spy test that reads
    `keyWindow()`.
  - `aWaterfallIsRefusedOnASkewRect` and `anAxisDeletedMidDragLeavesAMultiGraphInert` are guards (expected to pass
    before).
  - Graph-list registration is 5c (A5 needs the rename).
  - P-2 changes only `QCPMultiGraph`'s handling of an empty window. The other callers' masks keep them off user
    systems; the report proves it.
- **B10 (M4, M5, M7)**
  - The P-8 rows keep their `QCPMultiGraph_*` names until 5c renames them with the class; the 5c gate maps old to
    new names.
  - Add `QCPMultiGraph_UserSystem100k` (draw) and `QCPMultiGraph_FillToGraphPan10k` rows.
  - Task 6 docs state every refusal and the waterfall's rectangle row in the capability table.
