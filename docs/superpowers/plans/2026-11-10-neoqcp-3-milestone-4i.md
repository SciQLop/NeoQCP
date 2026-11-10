# NeoQCP 3.0 milestone 4i — items on coordinate systems Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** step 10 of the coordinate-systems design: an item position binds to a coordinate system and a
ratio position to an area, so items draw, hit and clip on polar areas (and, in 4j, on user systems) as they
do on axis rects; the item index groups items by system.

**Architecture:** a position holds `QPointer<QCPCoordinateSystem>` (plot coordinates) and `QPointer<QCPArea>`
(ratio and area-absolute coordinates). Its pixels come from the system's `transform()` and the area's new
ratio transform. The 2.x setters (`setAxes`, `setAxisRect`, `setClipAxisRect`) are the cartesian projection,
as `setKeyAxis`/`setValueAxis` are for plottables since 4g. The item's recording frame is built from its
system like a plottable's (the frame memo), with the area's ratio transform beside it.

**Tech Stack:** C++20, Qt 6.12, meson + `cmake/sources.cmake`, QtTest, `tests/benchmark`.

**Spec:** `docs/specs/2026-11-09-neoqcp-3-coordinate-systems-design.md` (2.3, 2.7, 2.9, 4 step 10, D5, D10).
Facts: `docs/plans/neoqcp-3/m4i-inventory.md` (read it first: it has every `file:line` this plan relies on),
`milestone-4g-rulings.md`, `milestone-4h-rulings.md`. The maintainer said "go ahead on 4i" (2026-11-10) with
plan calls delegated as for 4g/4h.

## Calls made in this plan (I-1..I-8; the maintainer can overrule any)

- **I-1 A position has a system.** `QCPItemPosition::setSystem(QCPCoordinateSystem*)`, `system()`. Its plot-
  coordinate slots resolve through `system()->transform()` (map and inverse). `setAxes(key, value)` becomes
  `setSystem(key->axisRect()->system(key, value))`; `keyAxis()`/`valueAxis()` return the system's axes as
  `QCPAxis*` (null on a non-cartesian system). The one-axis idiom `setAxes(nullptr, axis)` (and the mirror)
  resolves the missing axis to the rect's first axis of the other orientation, so the position still has a
  system; `keyAxis()` then returns that axis (changelog: 2.x returned null). Misuse (two axes of two rects,
  same orientation) reports through `QCP_CHECK_OR_RETURN` and keeps the old system.
- **I-2 A position has an area.** `setArea(QCPArea*)`, `area()`; `setAxisRect(rect)` is `setArea(rect)` and
  `axisRect()` the `qobject_cast`. `ptAxisRectRatio` keeps its name and means "a fraction of the area":
  on an axis rect today's (0,0 top left, 1,1 bottom right); on a polar area (fraction of the angular range,
  fraction of the radius) per spec 2.9. `ptAxisRectAbsolute` is pixels from the area rect's top left on any
  area. Setting a system with no explicit area sets the area to the system's area.
- **I-3 The ratio transform belongs to the area.** `virtual QCPTransform QCPArea::ratioTransform() const`
  (axis rect: the affine map of its rect; polar area: the polar mapping of unit ranges). `QCPDrawFrame::ratio`
  becomes a `QCPTransform`; `QCPRatioTransform`, `QCPCoordinateSystem::ratioTransform()` and
  `frameRatioTransform()` are deleted (a user system then implements one pure virtual less). The 4g misuse
  "a polar area has no axis ratio" goes.
- **I-4 The item frame is its system's.** `qcpDrawnViewOf(item)` takes the system of the item's first plot-
  coordinate position (else of its first position) and the area of its first position; the frame memo builds
  it as for plottables (`QCPDrawnView::system` no longer null for items). The `simplify:` note on mixed pairs
  stays, reworded for systems. `qcpMakeFrame(QCPAxis*, QCPAxis*, QCPAxisRect*, clip)` goes if nothing else
  uses it.
- **I-5 Clipping by area.** `setClipArea(QCPArea*)`/`clipArea()`; `setClipAxisRect`/`clipAxisRect()` are the
  cartesian forms; `setClipToAxisRect`/`clipToAxisRect()` keep their names. An item clipped to the area of
  its own system clips to the system's `clipPath()` (the disc on a polar area), as plottables do since 4g;
  otherwise to the clip area's rect.
- **I-6 The item index keys on systems.** `QCPItemExtent { QCPCoordinateSystem* system; box; pixelPad }`;
  `ItemIndexGroup` keys on `QPointer<QCPCoordinateSystem>`; `itemsUseAxis(axis)` asks
  `system->usesAxis(axis)`. Only `capRectangular` systems are indexed: an item on another system is always a
  candidate (`simplify:` comment: ceiling = no culling on polar/user systems; upgrade = the system maps a
  pixel rect to a data box).
- **I-7 Lifetime.** Deleting an axis deletes its systems (4g); a position whose system or area is gone is
  inert: the item draws nothing for it and reports once (the plottable rule R-9 of 4g); `pixelPosition()` of
  an inert plot-coordinate slot answers 0 as 2.x did, quietly (the 4g `report` flag). `take` clears every
  position's system and area; `adopt`/`add(unique_ptr)` binds the plot's default `(xAxis, yAxis)` system and
  `axisRect()` (2.x). `QCPPolarArea::add<Item>()` and `QCPAxisRect::add<Item>()` are not added (an item is
  built, then `setSystem`): deferred to 4j with the area-move question.
- **I-8 The tracer follows its graph's system.** `QCPItemTracer::setGraph(g)` sets `position->setSystem(
  g->system())`.

## Global Constraints

- One build or test at a time, in the foreground, `ninja -C build-qt612 -j 4`, timeout 600000. Env in the same
  command: `PATH=/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin:/home/jeandet/Qt/6.12.0/gcc_64/bin:$PATH`,
  `PKG_CONFIG_PATH=/home/jeandet/Qt/6.12.0/gcc_64/lib/pkgconfig`. Never `meson setup` in the worktree.
- Every added or removed source/header in `meson.build`, `cmake/sources.cmake`, `src/qcp.h`.
- Offscreen suite; Xvfb whole binary (`pgrep -x Xvfb -a` shows none of ours after); ASan on touched classes
  per task, **whole ASan suite after the last code change** (4h lesson); clang check on every changed `.cpp`.
- Every existing reference picture keeps matching (20 pixels over 32 at most). A new or moved picture is
  looked at by the controller before it is accepted (4h lesson: a re-baseline hid a bug).
- Bench gate: every `QCPItems_*`, `QCPItemText_*`, `QCPAxis_SetTickLength10kItems` and drawing row within 5 %
  of the task's base (A/B, five pairs, `-tickcounter -iterations 50 -median 5`); others 15 %.
- 2.x cartesian item code compiles and behaves the same (spec 5) except the one-axis `keyAxis()` answer (I-1).
- No shims for NeoQCP-only API; misuse via `QCP_CHECK_OR_RETURN`; signals via the M4a helpers;
  `contentChanged()` on every input the recording reads; tests fail before the change; never bend library
  behaviour to please a test; delete what a move replaces; no whole-file clang-format.
- Commit with explicit pathspecs; never `subprojects/.wraplock`, `subprojects/jkqtmathtext`, `qt.ini`. `/tmp`
  is RAM: remove worktrees and perf data. Never push.

## Review Focus

1. 2.x item code on axis rects: every position type, one-axis positions, anchors, `setPixelPosition`
   round trips, log axes, a second axis rect, `ptAxisRectAbsolute` on resize (Task 1 keeps
   `test-item-position`, `test-item-recording`, `test-coords` green unchanged).
2. Deleting an axis, an axis rect, a polar area, or the plot with items bound to them, also from a slot
   during a drag; `take`/`adopt` of an item bound to a polar system (Task 1 and Task 2 tests).
3. An item on a polar area: data positions follow rotation and radial zoom without re-recording; ratio
   positions mean (angle fraction, radius fraction); clipping to the disc (Task 2).
4. The item index: items on two systems of one rect, an axis deleted (group dead), a polar item never culled
   wrongly (Task 3).
5. Bench: no item row slower (the frame memo now serves items) (every task).

---

### Task 1: Positions on systems and areas; the item frame from its system (cartesian unchanged)

**Files:** `src/items/item.{h,cpp}` (I-1, I-2, I-4, I-5, I-7), `src/layoutelements/area.{h,cpp}` and
`layoutelement-axisrect.{h,cpp}` (`ratioTransform`, I-3), `src/coords.{h,cpp}` (`QCPRatioTransform` gone),
`src/coordinate-system{,-polar}.{h,cpp}` (system `ratioTransform` gone), `src/painting/draw-context.{h,cpp}`,
`draw-commands.h`, `command-replay-*.cpp` (frame ratio is a `QCPTransform`), `src/drawable.{h,cpp}` (item view
with a system), `src/items/item-tracer.cpp` (I-8), `src/core.cpp` (take/adopt of items), every user of
`QCPRatioTransform` (grep, tests included).

**Interfaces — produces:** `void QCPItemPosition::setSystem(QCPCoordinateSystem*)`, `QCPCoordinateSystem*
system() const`, `void setArea(QCPArea*)`, `QCPArea* area() const`; `void QCPAbstractItem::setClipArea(QCPArea*)`,
`QCPArea* clipArea() const`; `virtual QCPTransform QCPArea::ratioTransform() const`; `QCPDrawFrame::ratio` is
`QCPTransform`.

- [ ] **Step 1: Failing tests** (new `TestItemSystem` class, `tests/auto/test-item-system/`, registered like the
  others): `aPositionHasTheSystemOfItsAxes` (`setAxes(x, y)` → `system() == rect->system(x, y)`, `area() == rect`);
  `aOneAxisPositionGetsASystem` (`setAxes(nullptr, yAxis)` with typeX ratio: system has `xAxis` as key, pixels
  equal 2.x's); `aSystemMisuseKeepsTheOldSystem`; `aDeletedAxisLeavesThePositionInert` (one report, draws
  nothing, `pixelPosition()` quiet); `takeClearsAndAdoptRebindsItemSystems`; `theTracerFollowsItsGraphSystem`;
  `itemsShareTheFrameMemo` (two items of one system in one paint resolve one frame — use the memo's counter if
  one exists, else assert pixels only and say so). Expected: FAIL (compile).
- [ ] **Step 2: Implement.**
- [ ] **Step 3:** `TestItemSystem`, `test-item-position`, `test-item-recording`, `test-coords`, `test-drawable`,
  `test-item-index`, `test-qcpaxisrect`, `test-ownership`, `test-reference-m4e`, the suite offscreen and under
  Xvfb, ASan on the item tests, clang. A/B every item row.
- [ ] **Step 4: Commit** `feat(items): positions bind to a coordinate system and an area`.

### Task 2: Items on polar areas

**Files:** `src/polar/polar-area.{h,cpp}` (`ratioTransform`), `src/items/item.cpp` (clip path, I-5),
`tests/auto/test-item-system/`, `tests/auto/test-reference-m4e` (one new scene, `polar-items`, taken on this
task's code and looked at by the controller: a text at a data point, a line between two data points, an
ellipse in area ratio, a span-like rect clipped to the disc).

- [ ] **Step 1: Failing tests:** `aTextFollowsAPolarDataPoint` (rotate and zoom radially: pixel follows,
  `recordingCount` unchanged on rotation); `aPolarRatioIsAngleAndRadiusFractions` (ratio (0.25, 0.5) on a
  0..360 area at offset -90 lands at 90 degrees, half the radius); `aPolarItemClipsToTheDisc` (an ellipse
  larger than the disc inks nothing outside it); `aPolarItemIsHittable` (click selects it, a click outside the
  disc misses when clipped); `deletingThePolarAreaLeavesItsItemsInert`; `takeAndAdoptAPolarItem` (adopt
  rebinds to the default axes per I-7, documented); `polar-items` reference. Expected: FAIL.
- [ ] **Step 2: Implement.**
- [ ] **Step 3:** as Task 1 step 3 plus `test-polar`.
- [ ] **Step 4: Commit** `feat(items): items draw, hit and clip on polar areas`.

### Task 3: The item index by system

**Files:** `src/drawable.h` (`QCPItemExtent`), `src/items/item.cpp` (`positionsExtent`), `src/layer.{h,cpp}`
(groups, `collectItemAxes` → systems, `itemsUseAxis` via `usesAxis`), `src/axis/axis.cpp` and
`layoutelement-axisrect.cpp` (callers), `tests/auto/test-item-index/`.

- [ ] **Step 1: Failing tests** in `TestItemIndex`: `itemsOfTwoSystemsOfOneRectAreIndexedApart` (x/y and x/y2:
  culling correct for each after a y2 zoom); `anItemOnAPolarSystemIsAlwaysACandidate`;
  `aDeletedAxisDropsItsGroup` (no stale culling, rebuild count); keep `placeRandomly` working (it may take a
  system). Expected: FAIL.
- [ ] **Step 2: Implement.**
- [ ] **Step 3:** `TestItemIndex`, the item tests, the suite offscreen and under Xvfb, ASan, clang. A/B every item
  row (the index rows especially: `QCPItems_Zoom10k`, `Click10k`, `Pan10k`, `FirstFrame10k`).
- [ ] **Step 4: Commit** `feat(items): the item index groups items by coordinate system`.

### Task 4: Users, docs and finish

**Files:** `docs/items.md` (system and area, polar items, one-axis positions), `docs/polar.md` (items),
`docs/snippets/items.cpp` and `polar.cpp` (one polar item snippet, compiled by the docs build),
`docs/migrating.md`, `changelog.txt` (milestone 4i), `docs/plans/neoqcp-3/baseline.md`, `backlog.md`,
`milestone-4i-rulings.md`, `sciqlopplots-migration.md` (SciQLopPlots item code: grep its `src/` for
`setAxes`, `setAxisRect`, `setClipAxisRect`, `QCPRatioTransform` and list what changes).

- [ ] **Step 1:** Final whole-branch review (fresh Opus reviewer and fresh opencode reviewer), one fix batch.
- [ ] **Step 2:** Offscreen suite, Xvfb whole binary, whole ASan suite (after the last code change), clang on
  every changed `.cpp`.
- [ ] **Step 3:** Whole bench table against end of 4h; A/B every item and drawing row against a build of the
  milestone's base.
- [ ] **Step 4:** Docs, changelog, rulings, backlog, migration notes.
- [ ] **Step 5: Commit** `docs(neoqcp-3): end of milestone 4i — rulings, baseline, backlog, changelog`.

---

## Amendments after review (binding; they override the call and task text above)

Plan review: `.superpowers/m4i-plan-review.md` (opencode, 4 blockers, 4 majors, 2 minor batches). All accepted.

- **A1 (B1) The ratio map stays its own type.** Keep `QCPRatioTransform`, give it `QCP::AxisRatio inverse(QPointF)`
  and a second, polar shape (centre, radius, start angle, angular span, direction): map `angle = start + x*span`,
  `r = y*radius`; inverse wrapped. It is built by `virtual QCPRatioTransform QCPArea::ratioTransform() const`
  (axis rect: today's affine rect; polar area: the polar shape, NOT `QCPTransform::polar`, so a log radial axis
  does not bend it). `bool separable() const` (true for the rect shape). `QCPDrawFrame::ratio` stays a
  `QCPRatioTransform`. `QCPCoordinateSystem::ratioTransform()`/`frameRatioTransform()` are still deleted (the frame
  takes the ratio from the area). The mixed kinds `DataXRatioY`/`RatioXDataY` on a non-separable ratio are misuse
  (one report, not drawn) — this covers the tracer's ticks off a cartesian system.
- **A2 (B2) The area has a clip shape.** `virtual QPainterPath QCPArea::clipPath() const` (rect by default; the disc
  on `QCPPolarArea`) and `virtual bool QCPArea::clipsToPath() const`. `QCPDrawnView` gains `area` (an item's clip
  area, else its first position's area; a plottable's system area). `QCPSystemClip`/`qcpSystemClipShape` take the
  shape from the view's area. The `polar-items` scene calls `setClipArea(polarArea)`. Files: `drawable.{h,cpp}`,
  `area.{h,cpp}`, `polar-area.cpp` join Task 2.
- **A3 (B3) Hit and paint agree on the clip.** `QCPAbstractItem::selectTest` returns -1 when the item clips to its
  area and the area's `clipPath()` (or rect) does not contain `pos`. Test: same geometry, a click inside the disc
  hits, one outside misses.
- **A4 (B4) Index predicate.** Only systems whose `transform().kind() == QCPTransform::Kind::Cartesian` are indexed;
  every other system, and a group whose `QPointer` system went null, is "always a candidate". The `simplify:`
  comment names 4j's upgrade (a system-side `dataBoxOf(pixelRect)`).
- **A5 (M5) A moving polar area repaints its items.** `QCPPolarArea::markAffectedLayersDirty` marks the layer of every
  item with a position whose system or area is this area (a `QCPLayer` hook replacing or extending
  `collectItemAxes` with systems/areas). Task 2's rotation test asserts on pixels after a replot (item alone on its
  layer), not on `pixelPosition()`.
- **A6 (M6) No invented axis.** A position built from both axes of one rect gets that system. `setAxes(nullptr, axis)`
  (and the mirror) keeps the axis it was given, has no system, and keeps the 2.x per-slot resolution (pixels
  identical, both directions tested: `aOneAxisPositionKeepsTwoXPixels` with two rows). `setAxes(nullptr, nullptr)`
  = no system, no area implied. A null `axisRect()` is guarded. Drops I-1's "keyAxis() returns the substituted axis"
  behaviour change.
- **A7 (M7) Named call sites.** Task 1 covers `src/item-creation-state.cpp:164-178` (`rebindPositions`, with a
  creation-mode test that the rebound item has the new rect's system), `src/core.cpp` `detach` (clear system, area,
  clip area) and `adopt` (default system, `axisRect()`, clip area). I-8 stands for cartesian graphs only:
  `setGraph` takes `QCPGraph*`, a polar tracer is 4j at the earliest (backlog).
- **A8 (M8) Perf.** `QCPArea::system()` scans `mSystems` before `allAxes()`; one shared helper for "the system of
  these two axes" with the plottables' `systemOfAxes` (`plottable.cpp:455-470`), not a second copy. A cartesian
  position keeps the direct `coordToPixel` path in `pixelPosition()`; the transform is used for other kinds only.
  Task 1 A/Bs `QCPItems_Add10k`, `QCPItems_AddRemove10k`, `QCPItems_Replot10k` explicitly.
- **A9 (minor 9) Tests that can fail.** Drop `itemsShareTheFrameMemo`. `deletingThePolarAreaLeavesItsItemsInert` and
  `takeAndAdoptAPolarItem` first assert the item painted / its `system()`/`area()`, then the after state.
  `aDeletedAxisDropsItsGroup` asserts the group's system identity and the rebuild count. "pixelPosition() quiet"
  becomes: the 2.x qDebug messages for a missing axis are replaced by one misuse report (changelog).
- **A10 (minor 10) 2.x details.** The protected `mKeyAxis`, `mValueAxis`, `mAxisRect` stay as synchronised
  projections. `setTypeX/Y`'s keep-pixel predicate becomes "the target type can resolve" (system or axis for plot
  coordinates, area for ratio/absolute). The pre-existing `setPixelPosition` vertical-key Y-branch bug
  (`item.cpp:979-989`) is fixed with its own failing test (`setPixelPositionWithAVerticalKeyRoundTrips`). The parent
  layerable follows `setClipArea` as it follows `setClipAxisRect`. `setAxes` with axes of two rects becomes misuse
  (changelog, `docs/migrating.md`, `docs/items.md`).
- **A11 (4j gate) Backlog now.** Task 4 writes into `backlog.md` under "For milestone 4j": a user system's area ratio
  (an axis rect's affine, or the system's own?), the cartesian-only box gate in `QCPDrawnCore::hitTest`, a polar /
  user-system tracer, `area->add<Item>()`, areas moving between plots.
