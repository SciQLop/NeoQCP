# NeoQCP 3.0 milestone 4h — the polar graph is a plottable Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** step 9 of the coordinate-systems design: `QCPPolarGraph` becomes an ordinary recording plottable
(`QCPAbstractPlottable1D<QCPGraphData>`) drawn in a `QCPPolarSystem`, so polar plots get the plot's plottable
list, legend, selection, rescale, ownership, `take`/`adopt`, the recording and the GPU path like every other
plottable; the polar-only plumbing that duplicated them goes.

**Architecture:** the polar graph records its line, fill and scatter in data space (angle, radius); the polar
mapping (milestone 4f) turns them into pixels at replay with its radius clamp, so a rotation re-resolves a
periodic graph without re-recording. What the old painter did in pixels that a point map cannot do (the rim
crossing, hiding a run beyond the range along the rim) is done at record time, reading the radial range. The polar area stops
keeping its own graph list and legend row type: the plot's lists and `QCPPlottableLegendItem` serve.

**Tech Stack:** C++20, Qt 6.12, meson + `cmake/sources.cmake`, QtTest, `tests/benchmark`.

**Spec:** `docs/specs/2026-11-09-neoqcp-3-coordinate-systems-design.md` (sections 2.6, 2.8, 4 step 9, 5).
Facts: `docs/plans/neoqcp-3/m4f-inventory.md` (sections 2, 5, 6 on the polar graph),
`docs/plans/neoqcp-3/milestone-4f-rulings.md`, `docs/plans/neoqcp-3/milestone-4g-rulings.md`. The maintainer
approved going ahead on 2026-11-10 ("yeah go"), with plan calls delegated as for 4g.

## Calls made in this plan (H-1..H-6; the maintainer can overrule any)

Revised after the plan review (`.superpowers/m4h-plan-review.md`, 15 findings). The 2.1 behaviour each call
keeps is read from `src/polar/polargraph.cpp` at the milestone's base (`draw` ~741, `drawFill` ~907,
`getVisibleDataBounds` ~1131, `getOptimizedLineData` ~1245).

- **H-1 `QCPPolarGraph : QCPAbstractPlottable1D<QCPGraphData>`**, `requiredCapabilities() == {}`. Its 2.1
  constructor `QCPPolarGraph(QCPPolarAxisAngular* key, QCPPolarAxisRadial* value)` stays and resolves
  `key->area()->system(key, value)`; a system constructor joins it. Axis changes go through `setSystem`
  (4g G-2); the graph's own axis `QPointer`s and its re-registration in `setKeyAxis`/`setValueAxis` go. It
  keeps its API (`setLineStyle` lsNone/lsLine, `setScatterStyle`, `setPeriodic`, `setBrush`, `rescale*`, data
  setters) and gains the plottable one. Moved with it: `QCPPolarGraphOps` derives from the plottable ops
  family (as `plottable-graph.h`); `core.h`'s two `static_assert` lists drop `QCPPolarGraph` (it is a
  plottable); `QCustomPlot::detach/adopt(QCPPolarGraph*)` (`core.cpp` ~1503, ~1525) go, the plottable
  overloads serve; `removeObject(QCPPolarGraph&)` (`core.cpp` ~5211, `handle.h:27`) goes, the plottable one
  routes through `QCustomPlot::remove`.
- **H-2 What is recorded is 2.1's optimized line, in data space.** Every data point of the window (H-3) is
  recorded as it is. In the radial fraction of the current frame (`QCPTransform::Polar::fraction`, so log
  and reversed radial axes follow their own scale — this fixes 2.1's `TODO: fix for log axes`):
  (a) a segment that crosses the rim (fraction 1) gets the exact crossing point inserted (line/circle
  intersection in the unit-disc plane, mapped back to (angle, value)), so the visible part of the segment
  is 2.1's pixel chord to the real outside point; (b) between two consecutive points both at or beyond the
  rim (crossing points count), points are inserted at the clamp radius every 15 degrees of the circle
  (`angularSpan * 15 / 360` in key units; a chord between them stays outside the rim since
  15 < 2 * acos(1 / 1.05) ≈ 35.5), so a run outside the range is hidden along the rim as in 2.1, never a chord
  through the disc; (c) below the radial lower bound the mapping's clamp at fraction 0 is already 2.1's
  centre point; nothing is inserted. Nothing else is densified: two in-range points 90 degrees apart
  record two points and draw 2.1's chord. Non-finite values split the recording: one polyline per finite
  run (2.1 broke the stroke at a NaN pixel). The recording therefore depends on the radial range and the
  angular span (not the angular offset): `dependencyRevision()` hashes radial lower, upper, scale type,
  reversed and the angular range size.
- **H-3 The window is 2.1's.** `periodic()` (the default) records the whole container: a rotation re-records
  nothing. Non-periodic records the keys inside the angular range (2.1 `getVisibleDataBounds`): the
  angular lower and upper join `dependencyRevision()` for such a graph only. 2.1 had no seam closure;
  none is added.
- **H-4 The fill is 2.1's polygon**: the recorded line points of each finite run in order, closed last to
  first, `brush()`, no pen, no centre point. `lsNone` records markers only: no line and no fill (2.1's `getLines`
  returned nothing for it, so its fill polygon was empty).
- **H-5 The polar legend row and graph list go**: `QCPPolarLegendItem` and the graph's
  `addToLegend`/`legendItemIn`/`removeFromLegend` (`polargraph.cpp` ~640-700) go, the plottable ones and
  `QCPPlottableLegendItem` serve. In the area: `mGraphs`, `registerPolarGraph`, `removeGraph` go (no
  delete alias survives: `plot.remove(graph)`, polar 2.1 API break, D6); `graphs()` scans the plot's
  plottables whose `system()->area() == this` (G-5); `rescale` and `markAffectedLayersDirty` select the
  same way (4g A18); `~QCPPolarArea` stops deleting graphs (the plot owns them; R-9's inert rule applies
  when the area goes first). This lifts 4g ruling A2: `area->add<T>()` on `QCPPolarArea` is the generic
  template in `src/construct.h` beside the `QCPAxisRect` one, on the angular axis and the first radial axis;
  a plottable whose `requiredCapabilities()` the polar system lacks (`capabilities() == {}`) is refused
  through `QCP_CHECK_OR_RETURN` with an empty handle.
- **H-6 Hit testing through the system**: `selectTest` maps the data points through `system()->transform()`,
  measures segment distance as 2.1 did, gates on `system()->contains(pos)` (the disc, replacing the
  axis-rect gate), and fills `details` with the nearest point's `QCPDataSelection`; `interface1D()` returns
  `this` so rubber-band and stSingleData selection work. The selection decorator is the standard one (2.1
  left it commented out: a selected polar graph now looks selected); a reference picture that moves only
  because it selects a graph is re-baselined with a ruling.

## Global Constraints

- One build or test at a time, in the foreground, `ninja -C build-qt612 -j 4`. Env PATH/PKG_CONFIG_PATH in the
  same command (`/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin:/home/jeandet/Qt/6.12.0/gcc_64/bin`,
  `/home/jeandet/Qt/6.12.0/gcc_64/lib/pkgconfig`). Never `meson setup` in the worktree.
- Every added or removed source/header in `meson.build`, `cmake/sources.cmake`, `src/qcp.h`.
- Offscreen suite; Xvfb whole binary (`pgrep -x Xvfb -a | grep 1600` empty after); ASan on touched classes
  per task, whole ASan suite at the end; clang check on every changed `.cpp` before commits adding types.
- The polar reference pictures (six, plus two Task 1 adds) (`tests/auto/test-reference-m4e`, `polar-*`, taken on the 2.1-era code)
  must keep matching (20 pixels over 32 at most); a picture that moves is a code bug unless the plan says the
  old drawing was wrong — then the task proves it and the ruling says so.
- Bench gate: `QCPPolar_*` and every drawing row within 5 % of the task's base (A/B, five pairs,
  `-tickcounter -iterations 50 -median 5`); `QCPPolar_Rotate10k` is expected to get faster (no re-record) —
  report by how much; `QCPPolar_RadialZoom10k` re-records (H-2) and stays within the gate. At the end, A/B against the milestone's base.
- No shims for the polar 2.1 API (D6); misuse via `QCP_CHECK_OR_RETURN`; signals via the M4a helpers;
  `contentChanged()` on every input the recording reads; tests fail before the change; never bend library
  behaviour to please a test; delete what a move replaces; no whole-file clang-format.
- Commit with explicit pathspecs; never `subprojects/.wraplock`, `subprojects/jkqtmathtext`, `qt.ini`. `/tmp` is
  RAM: remove worktrees and perf data. `neoqcp-3.0` is a release branch on origin; never push without the
  maintainer asking.

## Review Focus

1. Deleting the polar area, an axis, the graph or the plot with polar graphs in it, also from a
   `rangeChanged` slot during a polar drag: graphs become inert or are deleted with the plot, no crash
   (Task 1 `deletingTheRadialAxisLeavesTheGraphInert`, `deletingTheGraphFromADragSlot`).
2. The six polar pictures still match; log radial axes, reversed axes, rotated start angles, radians
   (Task 1 references).
3. A run beyond the radial range is hidden along the rim, never a chord through the disc, on linear, log and
   reversed radial axes (Task 1 `aClampedRunFollowsTheArc` and its log/reversed rows, pixel based).
4. Legend, selection (click, stSingleData, rubber band), groups, `take`/`adopt` work for polar graphs as for
   any plottable (Task 2).
5. A rotation does not re-record a periodic graph; a radial zoom does (H-2 reads the radial range), and a
   non-periodic graph re-records on rotation (Task 1 `aRotationDoesNotReRecord`, `aNonPeriodicGraphKeepsItsWindow`).

---

### Task 1: The polar graph is a recording plottable in its polar system

Graph class, recording, registration and the polar plumbing that would otherwise list a graph twice, in one
task, so it ends green (review finding 10).

**Files:** `src/polar/polargraph.{h,cpp}` (H-1..H-5; `QCPPolarLegendItem` deleted), `src/polar/polar-area.{h,cpp}`
(H-5 except `add<T>`), `src/core.{h,cpp}`, `src/handle.h`, `meson.build`/`cmake/sources.cmake`/`src/qcp.h` if
a file moves; users of `removeGraph`: `tests/auto/test-polar/test-polar.cpp:131-140`; users of
`QCPPolarLegendItem`/`addToLegend` found by grep; `tests/auto/test-reference-m4e` (two new scenes).

**Interfaces:**
- Produces: `class QCPPolarGraph : public QCPAbstractPlottable1D<QCPGraphData>`; `void draw(QCPDrawContext&)
  const override` recording per finite run a `polygon` (fill, H-4), a `polyline` (line, H-2) and `markers`;
  `quint64 dependencyRevision() const override` (H-2, H-3); `requiredCapabilities()` none;
  `QCPPolarArea::graphs()` by scan. Deleted: `draw(QCPPainter*)`, `drawFill`, `drawLinePlot`,
  `drawScatterPlot`, `getOptimizedLineData`, `getOptimizedScatterData`, `getLines`, `getScatters`,
  `dataToLines`, `getVisibleDataBounds` (folded into the window), the commented decorator blocks,
  `QCPPolarLegendItem`, `addToLegend/legendItemIn/removeFromLegend` overrides, `mGraphs`,
  `registerPolarGraph`, `removeGraph`, `detach/adopt(QCPPolarGraph*)`, `removeObject(QCPPolarGraph&)`.
- `QCPPolarArea::add<QCPPolarGraph>` keeps working (it now just constructs; the plot registers).

- [ ] **Step 1: Failing tests** in `TestPolar`:
  - `aPolarGraphIsAPlottable`: `plot.plottableCount()` counts it once; `graph->system() == area->system(angular, radial)`;
    `area->graphs() == {graph}`; one legend row, a `QCPPlottableLegendItem`.
  - `aPolarGraphRecords`: after one replot the recording holds one polyline of N points for N in-range points
    (and one polygon when the brush is set).
  - `aRotationDoesNotReRecord`: periodic, unfilled; settle; shift the angular range by 10: `recordingCount`
    unchanged, pixels moved. Then zoom the radial range: `recordingCount` + 1.
  - `aNonPeriodicGraphKeepsItsWindow`: keys 0..720 on a 0..360 axis, `setPeriodic(false)`: the polyline holds
    the 0..360 points only; a rotation re-records.
  - `aClampedRunFollowsTheArc` (pixel based): values 2 x the radial upper at angles 0, 90; inked pixels only
    outside 0.97 R between those angles. Rows: linear, log radial, reversed radial.
  - `aSparseInRangePairIsAChord`: two in-range points 90 degrees apart record two points.
  - `aNaNBreaksTheLine`: a NaN in the middle gives two polylines.
  - `aScatterOnlyGraphRecordsNoLine`: `lsNone` + a scatter style: markers only, no polyline, no polygon.
  - `aFilledArcClosesWithAChord`: arc 0..90 at value 0.5, brush set: the centre pixel is not brushed, a pixel
    inside the arc/chord segment is.
  - `deletingTheRadialAxisLeavesTheGraphInert`; `deletingTheGraphFromADragSlot` (a `rangeChanged` slot deletes
    the graph during a polar drag; next replot fine); `deletingTheAreaDoesNotDeleteTheGraph`.
  - `TestReferenceM4e`: two new scenes taken on the milestone's base before any change: `polar-clamped` (a
    sparse series crossing the rim in and out) and `polar-sparse` (in-range points 60 degrees apart).
  Expected: FAIL (compile, then assertions); the two new references are taken from the base and pass there.
- [ ] **Step 2: Implement.**
- [ ] **Step 3:** `TestPolar`, `TestReferenceM4e` (eight polar scenes), `TestOwnership`, `TestBuilder`,
  `TestRepaint`, the suite offscreen and under Xvfb, ASan on `TestPolar` and `TestOwnership`, clang on changed
  `.cpp`. A/B `QCPPolar_*`: `QCPPolar_Rotate10k` faster (no re-record; report by how much),
  `QCPPolar_Draw10k`/`QCPPolar_RadialZoom10k`/`QCPPolar_Export10k` within 5 % (that data never crosses the rim,
  so H-2's insertions are not measured there).
- [ ] **Step 4: Commit** `feat(polar): the polar graph is a recording plottable in its polar system`.

### Task 2: Selection and the generic area `add<T>`

**Files:** `src/polar/polargraph.{h,cpp}` (H-6), `src/polar/polar-area.h`, `src/construct.h` (H-5 `add<T>`),
`docs/snippets/polar.cpp` (its two `add<QCPPolarGraph>` calls still compile; add none), `docs/polar.md`,
`examples/plots/mainwindow.cpp`, `tests/manual/mainwindow.cpp` (any `removeGraph`/legend item use),
`tests/auto/test-selection-model`, `test-builder`.

- [ ] **Step 1: Failing tests:** `clickingAPolarGraphSelectsThePoint` (stSingleData: a click on point 7 selects
  `[7, 8)`, `details` holds it; a click outside the disc misses); `rubberBandSelectsPolarPoints`
  (`interface1D()` non-null, the band selects the points inside it); `aSelectedPolarGraphLooksSelected` (the
  decorator's pen inks the line); `aPolarGraphJoinsAGroup`; `takeAndAdoptAPolarGraph`;
  `areaAddTakesAnyPolarCapablePlottable` (`area->add<QCPPolarGraph>()` works, `area->add<QCPBars>()` is
  refused with one report and an empty handle). Expected: FAIL.
- [ ] **Step 2: Implement**; move every user.
- [ ] **Step 3:** the moved tests, `TestReferenceM4e`, the suite offscreen and under Xvfb, ASan on the touched
  classes, A/B `QCPPolar_*`.
- [ ] **Step 4: Commit** `feat(polar): polar graphs select through their system; generic area add`.

### Task 3: Finish

**Files:** `changelog.txt` (milestone 4h), `docs/migrating.md` (polar: `removeGraph` → `plot.remove`,
`QCPPolarLegendItem` gone, the graph is a plottable, a selected graph now looks selected, the polar area no
longer deletes its graphs), `docs/polar.md`, `docs/plans/neoqcp-3/baseline.md`, `docs/plans/neoqcp-3/backlog.md`,
`docs/plans/neoqcp-3/milestone-4h-rulings.md`, `docs/plans/neoqcp-3/sciqlopplots-migration.md` (if SciQLopPlots
uses any removed polar API).

- [ ] **Step 1:** Final whole-branch review (fresh Opus reviewer and fresh opencode reviewer), one fix batch.
- [ ] **Step 2:** Offscreen suite, Xvfb whole binary, ASan whole suite, clang on every changed `.cpp`.
- [ ] **Step 3:** Whole bench table against end of 4g; A/B every drawing row and every row over its gate against
  a build of the milestone's base.
- [ ] **Step 4:** Changelog, docs, rulings file, backlog.
- [ ] **Step 5: Commit** `docs(neoqcp-3): end of milestone 4h — rulings, baseline, backlog, changelog`.
