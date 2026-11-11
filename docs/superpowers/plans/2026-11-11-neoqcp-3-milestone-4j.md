# NeoQCP 3.0 milestone 4j — the extension point (a user's own coordinate system) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** step 11 of the coordinate-systems design, with D9: a user writes a coordinate system of their own,
and `QCPGraph`, `QCPCurve` and items draw, hit, select, rescale and survive lifetime events in it; plottables
that need a rectangle are refused with a misuse report.

**Architecture:** a new capability, `capStraightSegments`, says "a segment between two data points may be
drawn as a straight pixel line". `QCPGraph` and `QCPCurve` require only it. A user system lives on a
`QCPAxisRect` subclass that overrides `createSystem`; its `transform()` is `QCPTransform::custom(...)`. The
2.x axis projections (`keyAxis()`/`valueAxis()` as `QCPAxis*`) are set only on cartesian systems (spec 2.5),
so 2.x pixel code cannot draw a user system as a plain rectangle by accident. `QCPGraph` keeps its cartesian
painter path untouched and gains one non-cartesian path: build lines, steps, impulses, scatters and fills in
data space, then map them through the frame once.

**Tech Stack:** C++20, Qt 6.12, meson + `cmake/sources.cmake`, QtTest, `tests/benchmark`.

**Spec:** `docs/specs/2026-11-09-neoqcp-3-coordinate-systems-design.md` (2.3-2.7, 2.9, 4 step 11, 7 D9/D10).
Facts: `docs/plans/neoqcp-3/m4j-inventory.md` (read it first: every `file:line` this plan relies on). Rulings:
`milestone-4i-rulings.md`, `milestone-4h-rulings.md`, `milestone-4g-rulings.md`. The maintainer said "push and
continue with 4j" (2026-11-11), with plan calls delegated as for 4g-4i.

## Calls made in this plan (J-1..J-8; the maintainer can overrule any)

- **J-1 The capability.** `QCP::capStraightSegments = 0x10`, doc "A segment between two data points may be
  drawn as a straight pixel line". `QCPCartesianSystem` offers it (with the other four). `QCPPolarSystem`
  offers nothing (unchanged). `QCPGraph::requiredCapabilities()` and `QCPCurve::requiredCapabilities()` return
  `capStraightSegments` only. The base default stays `capRectangular | capStraightKey`, so `QCPBars`, colour
  maps, lanes, error bars, `QCPGraph2`, `QCPMultiGraph` and every other plottable keep needing a rectangle.
- **J-2 Projections are cartesian only (spec 2.5).** `QCPAbstractPlottable::keyAxis()/valueAxis()` and
  `QCPItemPosition::keyAxis()/valueAxis()` are non-null only when `system->kind() == Kind::Cartesian`. Today
  they are a bare `qobject_cast` (`plottable.cpp:830-831`, `item.cpp:407-408`), so a user system on `QCPAxis`
  axes would hand 2.x pixel code real axes and draw as a rectangle. Every library site that asks "is this
  axis mine" through a projection asks the system instead (`axis.cpp:1365`, `axis.cpp:1606`,
  `layoutelement-axisrect.cpp:550`, `item.cpp:1874`; grep for more). Rescale of a plottable on a user system
  goes through the system's axes.
- **J-3 Where a user system lives.** The documented and tested route: subclass `QCPAxisRect`, override
  `createSystem(key, value)`, return your system (systems are built lazily by `QCPArea::system()`, never in
  the rect's constructor). It inherits the layout, the `QCPAxis` axes with drag and zoom, the ratio space and
  the rect clip. The system calls the protected base constructor, implements `transform()` (returning
  `QCPTransform::custom(mapping, revision())`), `clipRect()` and `capabilities()`, and overrides `kind()`
  only if it is cartesian. The area already bumps every system's revision on a rect move and on its axes'
  mapping changes. For the system's own parameters, a new protected `void QCPCoordinateSystem::
  parametersChanged()` bumps the revision and marks the area's affected layers dirty (milestone 4g R-6:
  `changed()` alone marks nothing, so a shear change would otherwise not repaint). The other route,
  subclassing `QCPArea` as `QCPPolarArea` does, is documented, not tested.
- **J-4 `QCPGraph` on a non-cartesian frame.** The cartesian path stays byte for byte. It is the one taken
  when both projections exist. A graph whose system is not cartesian builds its points in data space (key as
  x, value as y). That covers lines, the three step styles, impulses (to value 0), scatters, the fill polygon
  and the channel fill polygon. It then maps each finished array once through `mSystem->transform()` (one
  batch call per array, never per point; C-6). The existing polygon builders are reused with a
  horizontal-key, non-reversed convention where that only needs the orientation as a parameter.
  - The fill base is value 0 on a linear value axis and the value range's lower bound on a log one. This
    mirrors 2.x, which uses the rect edge for log.
  - No pixel-space decimation and no visible-range cut on these frames. A `simplify:` comment names the
    ceiling (every point mapped) and the upgrade (decimate in data space by `keyWindow()`).
  - `selectTest` gates on `mSystem->contains(pos)` for every system. The cartesian `contains` is the same
    rect test plus `iSelectPlottablesBeyondAxisRect` it replaces.
  - Channel fill keeps the 2.x rule on cartesian frames (one plot, same key orientation). On a non-cartesian
    frame both graphs must share the system (spec 2.6), otherwise it is a misuse.
  - The legend icon is unchanged: a rect-shaped line and symbol, which suits any frame.
- **J-5 `QCPCurve` and rect selection.** `QCPCurve` already records in data space. Its draw gate and
  `selectTest` gate move from "both projections" to "has a system" plus `system->contains`.
  `QCPAbstractPlottable1D::selectTestRect` gains a non-cartesian branch: map the candidate points through the
  frame (one batch) and keep those inside the pixel rect.
- **J-6 Culling stays off on user systems.** The `QCPDrawnCore::hitTest` box gate (`drawable.cpp:902`) and
  the item index (`item.cpp:1860`) keep culling cartesian systems only. A user system is scanned in full:
  correct, O(n). The contract page says so. The upgrade would be a system virtual mapping a pixel rect to a
  data box, and it is additive. 4i backlog items 2 and 3 close as documented limits.
- **J-7 Areas do not move between plots.** `QCPLayout::adoptElement` of an element whose parent plot is
  another plot is a misuse (`QCP_CHECK_OR_RETURN`), and `addElement` returns false. Today the move is
  accepted silently and leaves the area, its axes and its systems in the old plot (inventory 4.6), and every
  later `setSystem` from the new plot is refused. Moving an area means building a new one. This is a bug fix
  with a failing test first.
- **J-8 No `area->add<Item>()`.** Items are built by the plot and bound with `setSystem`. A factory on the
  area would save one call, and it can be added later without breaking anything. The tracer follows any
  `QCPGraph`, on a user system too (its position takes the graph's system, inventory 4.4). A tracer on a
  polar graph stays in the backlog.

## Global Constraints

- One build or test at a time, in the foreground, `ninja -C build-qt612 -j 4`, timeout 600000. Env in the same
  command: `PATH=/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin:/home/jeandet/Qt/6.12.0/gcc_64/bin:$PATH`,
  `PKG_CONFIG_PATH=/home/jeandet/Qt/6.12.0/gcc_64/lib/pkgconfig`. Never `meson setup` in the worktree.
- Every added or removed source/header in `meson.build`, `cmake/sources.cmake`, `src/qcp.h`; every new test
  directory registered like the others.
- Offscreen suite; Xvfb whole binary (`pgrep -x Xvfb -a` shows none of ours after); ASan on touched classes
  per task, **whole ASan suite after the last code change**; clang check on every changed `.cpp` (absolute
  paths).
- Every existing reference picture keeps matching (20 pixels over 32 at most). A new or moved picture is
  looked at by the controller before it is accepted.
- Bench gate: every `QCPGraph_*`, `QCPDrawn_*`, `QCPItems_*` and drawing row within 5 % of the task's base (A/B,
  five pairs, `-tickcounter -iterations 50 -median 5`); others 15 %.
- 2.x cartesian code compiles and behaves the same (spec 5).
- No shims for NeoQCP-only API.
- Misuse goes through `QCP_CHECK_OR_RETURN`. Signals go through the M4a helpers. `contentChanged()` is called
  on every input the recording reads.
- Tests fail before the change. Never bend library behaviour to please a test. Delete what a move replaces.
  No whole-file clang-format.
- Commit with explicit pathspecs; never `subprojects/.wraplock`, `subprojects/jkqtmathtext`, `qt.ini`. `/tmp`
  is RAM: remove worktrees and perf data. Never push.

## Review Focus

1. **Null projections reach 2.x code.** A `QCPGraph`/`QCPCurve` on a user system now has
   `keyAxis() == nullptr`. Any library path that dereferences a plottable's or a position's `keyAxis()`/
   `valueAxis()` (selection decorator, error bars, tracer, legend, `core.cpp` interactions, rescale, axis
   removal) must not crash. Task 1 step 1 greps every `keyAxis()->`, `valueAxis()->`, `mKeyAxis->`,
   `mValueAxis->` reachable from a graph, curve or item, and the report lists each site with its verdict.
2. **Cartesian graphs are unchanged.** Same pixels (every reference picture), same speed (`QCPGraph_*` rows),
   with a vertical key axis, reversed ranges, log axes, step styles and channel fill between `y` and `y2`.
3. **Log value axis and NaN on a user system.** Gaps split the line and the fill. The fill base is the range's
   lower bound. An impulse to 0 on a log axis is dropped and never drawn to infinity (Task 2 tests).
4. **Lifetime on a user system.** The axis is deleted from a slot during a drag. The rect is deleted with
   graphs and items on it. `take`/`adopt` happen with a graph and an item bound to it (Task 3 tests).
5. **A system parameter change** (the shear) repaints, re-records the curve and moves items, without a manual
   `replot` trick (Task 1 test `aShearChangeRepaints`, Task 3 item row).

---

### Task 1: The capability, cartesian-only projections, the toy system, `QCPCurve` on it

**Files:** `src/global.h` (J-1), `src/coordinate-system.{h,cpp}` (`parametersChanged`, J-3; cartesian
`capabilities`), `src/plottables/plottable.cpp` (J-2), `src/items/item.cpp` (J-2), `src/axis/axis.cpp`,
`src/layoutelements/layoutelement-axisrect.cpp` (system-based side tests), `src/plottables/plottable-graph.h`
and `plottable-curve.{h,cpp}` (`requiredCapabilities`, curve gates, J-5), `src/plottables/plottable1d.h`
(`selectTestRect`, J-5); new `tests/auto/test-user-system/` holding the toy: `SkewMapping : QCPCustomMapping`
(it wraps a cartesian `QCPTransform` of the rect's two axes and shears x by `shear * (rect.bottom - y)`, with
the exact inverse), `SkewSystem : QCPCoordinateSystem` (`kind()` Custom, `capabilities()` =
`capStraightSegments`, `setShear(double)` calls `parametersChanged()`), `SkewRect : QCPAxisRect`
(`createSystem` returns a `SkewSystem`); existing mask tests in `test-coordinate-system`.

**Interfaces — produces:** `QCP::capStraightSegments`; `protected: void QCPCoordinateSystem::
parametersChanged()`; the `SkewRect`/`SkewSystem` fixture (header in the test directory, reused by Tasks 2-3).

- [ ] **Step 1: Grep** the null-projection sites (Review Focus 1). Write the list into the report before
  coding.
- [ ] **Step 2: Failing tests** (`TestUserSystem`):
  - `theCapabilityMasks`: cartesian has all five, polar none, graph and curve need `capStraightSegments`
    only, bars still `capRectangular | capStraightKey`.
  - `aGraphOnASkewRectHasNoProjection`: `graph->system()` is the `SkewSystem`, `keyAxis() == nullptr`.
  - `barsAreRefusedOnASkewRect`: one misuse, the bars keep no system.
  - `aCurvePointLandsOnTheSkewMapping`: the recorded point's pixel equals `SkewMapping::map` of its data.
  - `aCurveIsHitThroughTheMapping`: a click on the sheared point hits; the unsheared pixel misses.
  - `aCurveIsRectSelectedThroughTheMapping`.
  - `aShearChangeRepaints`: `setShear` then `replot` moves the pixel, `recordingCount` +1.
  - `rescaleAxesFitsASkewCurve`: the axes take the data range.
  - `anItemPositionOnASkewSystemHasNoProjection`.

  Expected: FAIL.
- [ ] **Step 3: Implement** J-1, J-2, J-3 (`parametersChanged`), J-5.
- [ ] **Step 4:** `TestUserSystem`, `test-coordinate-system`, `test-item-system`, `test-item-index`,
  `test-qcpcurve`, `test-qcpgraph`, `test-qcpbars`, `test-polar`, the suite offscreen and under Xvfb, ASan on
  these, clang. A/B the `QCPGraph_*`, `QCPDrawn_*` and item rows.
- [ ] **Step 5: Commit** `feat(systems): a straight-segment capability; projections only on cartesian systems`.

### Task 2: `QCPGraph` on a non-cartesian frame

**Files:** `src/plottables/plottable-graph.{h,cpp}` (J-4), `tests/auto/test-user-system/`,
`tests/auto/test-reference-m4e` (one new scene, `user-system-skew`. It is taken on this task's code and looked
at by the controller. Shear 0.5, holding: a line graph with a fill, a step-left graph, an impulse graph, a
scatter-only graph, a channel fill between two graphs, and a curve).

- [ ] **Step 1: Failing tests:**
  - `aGraphLineFollowsTheSkewMapping`: each vertex's pixel equals the mapping; a straight segment between
    them.
  - `stepStylesAreBuiltInDataSpace`: the step-left vertices are the mapped `(k[i+1], v[i])` points.
  - `anImpulseRunsToValueZero`.
  - `aLogValueFillStopsAtTheRangeBound`: no NaN or infinite vertex.
  - `aNaNSplitsLineAndFill`.
  - `aScatterLandsOnTheMapping`.
  - `channelFillNeedsOneSystem`: misuse on a skew graph paired with a cartesian graph; filled when both share
    the system.
  - `aGraphIsHitAndRectSelectedThroughTheMapping`.
  - `aCartesianChannelFillAcrossValueAxesStillWorks`: the 2.x `y`/`y2` case.
  - The `user-system-skew` reference.

  Expected: FAIL.
- [ ] **Step 2: Implement** J-4.
- [ ] **Step 3:** as Task 1 step 4, plus every `test-reference-*`. A/B every `QCPGraph_*` row: the cartesian
  path must not move.
- [ ] **Step 4: Commit** `feat(graph): a graph draws in any straight-segment coordinate system`.

### Task 3: The step-11 test matrix and the area move

**Files:** `src/layout/layout.cpp` (J-7; check the exact file in the inventory, `layout.cpp:923-959`),
`tests/auto/test-user-system/`, `tests/auto/test-qcplayout` (or the existing layout test directory).

- [ ] **Step 1: Failing tests** (the ones about the area move fail today; the others are the matrix and may
  pass on Tasks 1-2's code; say which in the report):
  - `anAreaIsNotAdoptedByAnotherPlot` (J-7): one misuse, `addElement` false, the area still works in its own
    plot.
  - `itemsFollowTheSkewMapping`: a text at a data point, a line between two data points, a ratio position from
    the inherited rect ratio; all move on `setShear`.
  - `theTracerFollowsASkewGraph`.
  - `takeAndAdoptAGraphAndAnItemOnASkewRect` (adopt rebinds to the plot's default axes, 2.x).
  - `anAxisDeletedMidDragLeavesThemInert` (mirror `test-coordinate-system` `aChangedSlotMayDeleteTheAxis`;
    the graph, curve and item draw nothing and report once).
  - `deletingTheSkewRectLeavesThemInert`.
  - `switchingBetweenCartesianAndSkewReRecords`.
  - `theLegendIconOfASkewGraphDraws`.
  - `aSkewItemIsAlwaysAnIndexCandidate` (J-6, pins the documented limit).
- [ ] **Step 2: Implement** J-7 and whatever the matrix exposes. A bug found here gets its own failing test
  first.
- [ ] **Step 3:** as Task 2 step 3, plus `test-ownership`.
- [ ] **Step 4: Commit** `feat(systems): the user-system test matrix; areas stay in their plot`.

### Task 4: The contract, docs and finish

**Files:**
- New `docs/coordinate-systems.md`: the contract. It covers what to implement, the J-3 route, revision and
  `parametersChanged`, the capability table, what each in-tree plottable requires, and the J-6 limits. It is
  linked from the docs index next to `axes.md` and `polar.md`.
- New `docs/snippets/systems.cpp`: the shear system, compiled by the docs build like the other snippets.
- `docs/axes.md` (a pointer), `docs/migrating.md` (J-1, J-2, J-7), `changelog.txt` (milestone 4j).
- `docs/plans/neoqcp-3/baseline.md`, `backlog.md`, `milestone-4j-rulings.md`, `sciqlopplots-migration.md`.
  For the migration notes, grep SciQLopPlots `src/` for `keyAxis()` on plottables that could sit on a
  non-cartesian system, and for `adoptElement`/`addElement` across plots.

- [ ] **Step 1:** Final whole-branch review (fresh Opus reviewer and fresh opencode reviewer), one fix batch.
- [ ] **Step 2:** Offscreen suite, Xvfb whole binary, whole ASan suite (after the last code change), clang on
  every changed `.cpp`.
- [ ] **Step 3:** Whole bench table against the end of 4i; A/B every graph, drawing and item row against a
  build of the milestone's base.
- [ ] **Step 4:** Docs, changelog, rulings, backlog, migration notes.
- [ ] **Step 5: Commit** `docs(neoqcp-3): end of milestone 4j — rulings, baseline, backlog, changelog`.

---

## Amendments after review (binding; they override the call and task text above)

Review: `.superpowers/m4j-plan-review.md` (opencode). The controller checked C1 and C3 against the code.
Rulings for each finding follow.

- **A1 (C1, C2, I4) The graph branches in `getLines` and `getScatters`.** These two functions feed `draw`,
  `pointDistance` and the other side of a channel fill. Each takes the cartesian path, unchanged, when both
  projections exist. Otherwise it takes the data-space path. That path is new code, small, written once:
  - line and step vertices in data space;
  - impulse pairs to value 0 (dropped where the frame maps 0 to a non-finite pixel);
  - one batch map.

  `getFillBasePoint`'s data-space twin is new code. `getFillPolygon` and `getNonNanSegments` are reused, the
  latter with `Qt::Horizontal` on that path. The four `keyAxis()->orientation()` call sites in
  `drawFill`/channel fill read a local orientation instead (`Qt::Horizontal` on a non-cartesian frame).
  The channel fill polygon on a non-cartesian frame works like this:
  1. Crop both graphs' data-space lines to the common key range, interpolating in data space.
  2. Close the polygon.
  3. Map it once.

  `pointDistance` on a non-cartesian frame skips the key window (`findBegin/findEnd` around the click). It
  maps every candidate in one batch and measures. The same goes for `selectTestRect`'s non-cartesian branch.
  A `simplify:` comment names the ceiling (O(n) hit) and the upgrade (the J-6 box query). Task 2's test list
  adds `hoveringASkewGraphDoesNotCrash` (`plottableAt` and `selectTest` with a line style set) and
  `aShearedPointOutsideTheKeyWindowIsHit`.
- **A2 (C3, M2) `parametersChanged()`** calls `bumpRevision()` (the system revision the frame memo keys on,
  never a mapping-only one). It then calls `mArea->markAffectedLayersDirty(true)` and marks every layer of the
  plot for which `layer->itemsUseArea(mArea)` holds, as `QCPPolarArea` does (`polar-area.cpp:338-340`).
  `QCPAxisRect::markAffectedLayersDirty` is not changed: it is a hot path, and range changes already reach
  items through `markItemsUsingAxisDirty`. Task 1's `aShearChangeRepaints` covers a curve and an item.
- **A3 (C4) `keyAxisBase()`/`valueAxisBase()`** (spec 2.5) are added on `QCPAbstractPlottable` in Task 1:
  `QCPAxisBase*`, the system's axes in any system, null without one. Library rewrites use them or
  `system()->usesAxis()`. Item positions use `system()->keyAxis()`. No accessor is added there: the spec names
  none.
- **A4 (I1, I2) Side tests by system.** Each site gets the same fix:
  - `QCPAxis::rescale`: the side comes from `plottable->keyAxisBase() == this`; the rest is unchanged.
  - `QCPAxis::items()`: `position->system() && position->system()->usesAxis(this)`, keeping the one-axis
    positions (4i R-2) through their projections.
  - `markLayersUsingAxisDirty`: `p->system() && p->system()->usesAxis(axis)`.

  New tests: `rescaleAxesFitsASkewGraph`, `anAxisListsASkewItem`, `anAxisRangeChangeRepaintsASkewGraph`.
  A/B the `QCPItems_*` and `QCPAxis_SetTickLength10kItems` rows: one virtual call is added per position.
- **A5 (I3) Channel fill** gets its check in `QCPGraph::setChannelFillGraph`. On a non-cartesian frame
  (either graph), both graphs must share the system, else a misuse and the old target is kept. Cartesian
  keeps the 2.x rule. `channelFillNeedsOneSystem` asserts the misuse.
- **A6 (I5) The area-move refusal** sits before the element is taken from its old layout. It covers
  `QCPLayoutGrid::addElement(row, col, e)` and `addElement(e)` (false returned) and both
  `QCPLayoutInset::addElement` forms (void: one misuse, nothing changes). The test checks the element is still
  in its old layout and plot. The existing "already an element" message path stays.
- **A7 (I6) Error bars on a user system are refused.** They keep needing a rectangle, so an error-bars object
  on a skew graph is refused with the capability misuse. The contract page says so. This is not deferred work:
  it is the D9 rule.
- **A8 (I7) Two test sets in Task 1.** These fail first:
  - `theCapabilityMasks`
  - `aGraphOnASkewRectHasNoProjection`
  - `anItemPositionOnASkewSystemHasNoProjection`
  - `aCurveIsRectSelectedThroughTheMapping`
  - `aShearChangeRepaints`
  - the A4 tests

  These are matrix pins that may pass before the change, and the report says which:
  - `aCurvePointLandsOnTheSkewMapping`
  - `rescaleAxesFitsASkewCurve`
  - `barsAreRefusedOnASkewRect`
  - `aCurveIsHitThroughTheMapping`

  The fixture uses shear 1.0, so the unsheared pixel of a high point lies outside the sheared one's hit
  distance and inside the rect.
- **A9 (I8) Task order.** `QCPGraph::requiredCapabilities()` changes in Task 2, with the graph's frame path.
  Until then a graph on a skew rect is refused: `aGraphOnASkewRectHasNoProjection` moves to Task 2. Task 1
  changes the curve's mask only.
- **A10 (M1, M3)** The fixture's `SkewSystem::contains` is the rect test. The legend sentence in J-4 is about
  `QCPGraph`. `QCPCurve`'s recorded icon goes through `iconTransform` (default `fitted`, fine).
