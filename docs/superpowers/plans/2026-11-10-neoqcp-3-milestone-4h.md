# NeoQCP 3.0 milestone 4h — the polar graph is a plottable Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** step 9 of the coordinate-systems design: `QCPPolarGraph` becomes an ordinary recording plottable
(`QCPAbstractPlottable1D<QCPGraphData>`) drawn in a `QCPPolarSystem`, so polar plots get the plot's plottable
list, legend, selection, rescale, ownership, `take`/`adopt`, the recording and the GPU path like every other
plottable; the polar-only plumbing that duplicated them goes.

**Architecture:** the polar graph records its line, fill and scatter in data space (angle, radius); the polar
mapping (milestone 4f) turns them into pixels at replay with its radius clamp, so a rotation or a radial zoom
re-resolves without re-recording. What the old painter did in pixels that a point map cannot do (following
the arc along the clamp, the periodic wrap) is done once at record time in data space. The polar area stops
keeping its own graph list and legend row type: the plot's lists and `QCPPlottableLegendItem` serve.

**Tech Stack:** C++20, Qt 6.12, meson + `cmake/sources.cmake`, QtTest, `tests/benchmark`.

**Spec:** `docs/specs/2026-11-09-neoqcp-3-coordinate-systems-design.md` (sections 2.6, 2.8, 4 step 9, 5).
Facts: `docs/plans/neoqcp-3/m4f-inventory.md` (sections 2, 5, 6 on the polar graph),
`docs/plans/neoqcp-3/milestone-4f-rulings.md`, `docs/plans/neoqcp-3/milestone-4g-rulings.md`. The maintainer
approved going ahead on 2026-11-10 ("yeah go"), with plan calls delegated as for 4g.

## Calls made in this plan (H-1..H-6; the maintainer can overrule any)

- **H-1 `QCPPolarGraph : QCPAbstractPlottable1D<QCPGraphData>`**, `requiredCapabilities() == {}`. Its
  2.1 constructor `QCPPolarGraph(QCPPolarAxisAngular* key, QCPPolarAxisRadial* value)` stays and resolves
  `key->area()->system(key, value)`; plus the system constructor. It keeps its API (`setLineStyle`
  lsNone/lsLine, `setScatterStyle`, `setPeriodic`, `setBrush` fill, `rescale*`, data setters) and gains the
  plottable one (selection decorator, groups, `system()`).
- **H-2 Arc following at record time.** Between two consecutive points whose angles differ by more than
  2 degrees of the angular axis' unit circle, the recorded polyline gets intermediate points every 2 degrees,
  radius interpolated linearly (in the radial axis' own scale): a run clamped by the polar mapping then
  follows the arc instead of cutting a chord through the disc, which is what the old pixel-space dummy points
  did. Data-space and range-independent, so a rotation or a radial zoom does not re-record.
- **H-3 Periodic wrap at record time**: with `periodic()` the polyline closes over the seam (last point back
  to the first through the wrap, densified by H-2); without it the recording covers the key window
  (`context.keyWindow()`), as non-periodic data did through `getVisibleDataBounds`.
- **H-4 Fill to the centre** is a recorded polygon: the line's points plus the centre as the data point
  (angle of the last point, radial range lower bound) — read from the radial axis, so a radial range change
  re-records a filled graph only (the recording reads the range: `dependencyRevision` includes the radial
  lower bound). Unfilled graphs never depend on the range.
- **H-5 The polar legend row and graph list go**: `QCPPolarLegendItem` is deleted (the plottable legend row
  draws the recorded icon); `QCPPolarArea::graphs()` returns the area's systems' plottables that are polar
  graphs (G-5 scan); `QCPPolarArea::removeGraph`/`registerPolarGraph` go (use `plot.remove(graph)` — polar 2.1
  API break, D6); `QCPPolarArea::add<T>` becomes the generic area `add<T>` (A2's restriction lifted): any
  plottable whose capabilities the polar system offers, on the angular axis and the first radial axis.
- **H-6 Hit testing keeps the data index**: the graph's `selectTest` still answers the nearest data point
  (today's `pointDistance` idea) through the polar transform's `map`, so a click selects that point's range;
  the axis-rect gate is replaced by `system()->contains` (the disc).

## Global Constraints

- One build or test at a time, in the foreground, `ninja -C build-qt612 -j 4`. Env PATH/PKG_CONFIG_PATH in the
  same command (`/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin:/home/jeandet/Qt/6.12.0/gcc_64/bin`,
  `/home/jeandet/Qt/6.12.0/gcc_64/lib/pkgconfig`). Never `meson setup` in the worktree.
- Every added or removed source/header in `meson.build`, `cmake/sources.cmake`, `src/qcp.h`.
- Offscreen suite; Xvfb whole binary (`pgrep -x Xvfb -a | grep 1600` empty after); ASan on touched classes
  per task, whole ASan suite at the end; clang check on every changed `.cpp` before commits adding types.
- The six polar reference pictures (`tests/auto/test-reference-m4e`, `polar-*`, taken on the 2.1-era code)
  must keep matching (20 pixels over 32 at most); a picture that moves is a code bug unless the plan says the
  old drawing was wrong — then the task proves it and the ruling says so.
- Bench gate: `QCPPolar_*` and every drawing row within 5 % of the task's base (A/B, five pairs,
  `-tickcounter -iterations 50 -median 5`); `QCPPolar_Rotate10k` and `QCPPolar_RadialZoom10k` are expected to
  get faster (no re-record) — report by how much. At the end, A/B against the milestone's base.
- No shims for the polar 2.1 API (D6); misuse via `QCP_CHECK_OR_RETURN`; signals via the M4a helpers;
  `contentChanged()` on every input the recording reads; tests fail before the change; never bend library
  behaviour to please a test; delete what a move replaces; no whole-file clang-format.
- Commit with explicit pathspecs; never `subprojects/.wraplock`, `subprojects/jkqtmathtext`, `qt.ini`. `/tmp` is
  RAM: remove worktrees and perf data. `neoqcp-3.0` is a release branch on origin; never push without the
  maintainer asking.

## Review Focus

1. Deleting the polar area, an axis, or the plot with polar graphs in it (also from a slot during a drag):
   graphs become inert or are deleted with the plot, no crash (Task 1 `deletingTheRadialAxisLeavesTheGraphInert`).
2. The six polar pictures still match; log radial axes, reversed angular axes, rotated start angles, radians
   (Task 1 references).
3. A data range clamped by the radial range (values below the lower bound, above the upper) draws along the
   rim, never a chord through the disc (Task 1 `aClampedRunFollowsTheArc`).
4. Legend, selection (click and rubber band), groups, `take`/`adopt` work for polar graphs as for any
   plottable (Task 2).
5. A rotation and a radial zoom do not re-record an unfilled polar graph (Task 1 `aRotationDoesNotReRecord`).

---

### Task 1: The polar graph records in its polar system

**Files:** `src/polar/polargraph.{h,cpp}`, `src/polar/polar-area.{h,cpp}` (registration through the system),
`src/core.{h,cpp}` (`adopt` stops refusing polar graphs; the polar `removeObject` overload goes if the generic
one covers it), `src/handle.h`; tests in `tests/auto/test-polar/`.

**Interfaces:**
- Produces: `class QCPPolarGraph : public QCPAbstractPlottable1D<QCPGraphData>` (H-1); `void draw(QCPDrawContext&)
  const override` recording line (`polyline`, H-2/H-3), fill (`polygon`, H-4), scatter (`markers`); the old
  `draw(QCPPainter*)`, `drawFill`, `drawLinePlot`, `drawScatterPlot`, `getOptimizedLineData`,
  `getOptimizedScatterData`, `getLines`, `getScatters`, `dataToLines` and the commented selection-decorator
  blocks are deleted; `requiredCapabilities()` returns none.

- [ ] **Step 1: Failing tests** in `TestPolar`: `aPolarGraphIsAPlottable` (`plot.plottableCount()` counts it;
  `graph->system()` is the area's `(angular, radial)` system); `aPolarGraphRecords` (`recordedCommandCount() > 0`
  after a replot); `aRotationDoesNotReRecord` (unfilled graph, settle with one replot, rotate the angular axis
  and zoom the radial axis: `recordingCount` unchanged, pixels moved); `aClampedRunFollowsTheArc` (points at
  radius 2 x the radial upper bound at angles 0..90: no inked pixel inside 0.9 R between those angles);
  `aFilledGraphFillsToTheCentre` (pixel near the centre inside the filled sector has the brush colour);
  `deletingTheRadialAxisLeavesTheGraphInert`. The six `polar-*` references must still match.
  Expected: FAIL (compile, then assertions).
- [ ] **Step 2: Implement.**
- [ ] **Step 3:** `TestPolar`, `TestReferenceM4e`, `TestOwnership`, `TestBuilder`, `TestRepaint`, the suite
  offscreen and under Xvfb, ASan on `TestPolar`, A/B `QCPPolar_*` (report the rotation/zoom gain).
- [ ] **Step 4: Commit** `feat(polar): the polar graph is a recording plottable in its polar system`.

### Task 2: Legend, selection, area add and the polar plumbing that goes

**Files:** `src/polar/polargraph.{h,cpp}` (`QCPPolarLegendItem` deleted; `selectTest` per H-6),
`src/polar/polar-area.{h,cpp}` (H-5: `graphs()` by scan, `removeGraph`/`registerPolarGraph`/`mGraphs`
deleted, the generic `add<T>`), `src/construct.h`, every user (`tests/auto/test-polar`, `test-ownership`,
`test-builder`, `test-selection-model`, `test-qcustomplot`, `test-repaint`, `docs/snippets/polar.cpp`,
`docs/snippets/gallery.cpp`, `examples/plots/mainwindow.cpp`, `tests/manual/mainwindow.cpp`, `docs/polar.md`).

- [ ] **Step 1: Failing tests:** `aPolarGraphHasAPlottableLegendRow` (`legend->itemWithPlottable(graph)` is a
  `QCPPlottableLegendItem`; its icon shows the line colour); `clickingAPolarGraphSelectsThePoint` (stSingleData:
  click on point 7 → selection `[7, 8)`; a click outside the disc misses); `rubberBandSelectsPolarPoints`;
  `aPolarGraphJoinsAGroup` (`QCPPlottableGroup` hides it with the others); `takeAndAdoptAPolarGraph`;
  `areaAddTakesAnyPolarCapablePlottable` (`area->add<QCPPolarGraph>()` and a `QCPBars` refused).
  Expected: FAIL.
- [ ] **Step 2: Implement**; move every user.
- [ ] **Step 3:** the moved tests, `TestReferenceM4e`, the suite offscreen and under Xvfb, ASan on the touched
  classes, A/B `QCPPolar_*`.
- [ ] **Step 4: Commit** `feat(polar): polar graphs use the plot's legend, selection and lists`.

### Task 3: Finish

**Files:** `changelog.txt` (milestone 4h), `docs/migrating.md` (polar: `removeGraph` → `plot.remove`,
`QCPPolarLegendItem` gone, the graph is a plottable), `docs/polar.md`, `docs/plans/neoqcp-3/baseline.md`,
`docs/plans/neoqcp-3/backlog.md`, `docs/plans/neoqcp-3/milestone-4h-rulings.md`.

- [ ] **Step 1:** Final whole-branch review (fresh Opus reviewer and fresh opencode reviewer), one fix batch.
- [ ] **Step 2:** Offscreen suite, Xvfb whole binary, ASan whole suite, clang on every changed `.cpp`.
- [ ] **Step 3:** Whole bench table against end of 4g; A/B every drawing row and every row over its gate against
  a build of the milestone's base.
- [ ] **Step 4:** Changelog, docs, rulings file, backlog.
- [ ] **Step 5: Commit** `docs(neoqcp-3): end of milestone 4h — rulings, baseline, backlog, changelog`.
