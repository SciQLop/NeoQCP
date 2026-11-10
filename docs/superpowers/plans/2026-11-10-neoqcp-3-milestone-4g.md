# NeoQCP 3.0 milestone 4g — coordinate systems and the plottables on them Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** steps 5-8 of the coordinate-systems design: `QCPCoordinateSystem` (public) with a cartesian and a
polar implementation owned by their areas; every plottable drawn in a system instead of "two `QCPAxis`";
capability and relationship checks; plot-level code (rescale, hit search, selection rect, `add<T>`, dirty
marking) working through areas and systems.

**Architecture:** an area hands out one system per (key axis, value axis) pair it owns, created on first use
and deleted with either axis. A plottable holds a `QPointer` to its system; its view, frame, clip, hit gate
and pixel conversions come from the system. The 2.x axis-pair constructors and accessors remain, as a
projection of the cartesian system. Every existing plottable declares that it needs a cartesian system
(rectangular, straight key axis); the polar system exists and is tested with a test-local recording
plottable, and the polar graph joins it in 4h. Items stay on axis pairs until 4i.

**Tech Stack:** C++20, Qt 6.12, meson + `cmake/sources.cmake`, QtTest, `tests/benchmark`.

**Spec:** `docs/specs/2026-11-09-neoqcp-3-coordinate-systems-design.md` (approved; sections 2.3-2.7, 3, 4
steps 5-8, 5). Facts: `docs/plans/neoqcp-3/coordsys-inventory.md` (sections 1, 2, 4, 6), the design review
`.superpowers/coordsys-design-review.md` (F-x), `docs/plans/neoqcp-3/milestone-4f-rulings.md`.
Approval: the maintainer delegated plan approval for the night of 2026-11-09/10 ("make decisions for me").

## Calls made in this plan (G-1..G-7; the maintainer can overrule any)

- **G-1 One system per (key, value) pair, owned by the area.** `QCPArea::system(QCPAxisBase* key,
  QCPAxisBase* value)` returns the existing system or creates it (a `QObject` child of the area), refusing
  as misuse a pair that is not two axes of this area (and, for a cartesian area, two axes of one
  orientation). An axis being deleted deletes every system that uses it, after detaching their plottables
  (milestone 1 rule: unlist, then delete): those plottables become inert, as a plottable whose axis was
  removed is today.
- **G-2 A plottable holds `QPointer<QCPCoordinateSystem> mSystem`.** The protected 2.x members
  `mKeyAxis`/`mValueAxis` stay as the cartesian projection, kept in sync by the single setter
  `setSystem`; `keyAxis()`/`valueAxis()` return them (null in a non-cartesian system). `setKeyAxis(k)`
  is `setSystem(area->system(k, valueAxis))`.
- **G-3 Capabilities** are `QCP::Capabilities` flags (`capRectangular`, `capStraightKey`,
  `capColourScale`, `capLanes`); a system answers `capabilities()`, a plottable `virtual
  QCP::Capabilities requiredCapabilities() const`, default `capRectangular | capStraightKey` for every
  existing class (all are cartesian until 4h). `setSystem` refuses a system missing one (misuse).
- **G-4 Relationship checks** where the relationship is made (design 2.6): `QCPGraph::setChannelFillGraph`,
  `QCPBars::moveAbove/moveBelow` and `QCPBarsGroup::append`, `QCPErrorBars::setDataPlottable`, the colour
  scale's users — all require the same area (and, for stacking, channel fill, the same system).
- **G-5 Queries scan, lists do not exist yet.** `QCPAxisBase::plottables()`, `QCPAxisRect::plottables()`
  and `QCPCoordinateSystem::plottables()` filter the plot's plottable list by system, as today's code
  filters by axis (same order, same O(n)). `simplify:` note: a maintained per-system list is the upgrade
  when a profile asks for it.
- **G-6 The frame memo** (`drawable.cpp`, `thread_local FrameMemo`) is keyed on `(plot, frame epoch,
  system, system revision, clip)` instead of the two axes and the rect.
- **G-7 `QCPGraph` and `QCPGraph2` keep their painter paths** and read the cartesian projections: they
  require a cartesian system (G-3), so `keyAxis()` is never null for them. They lose `axesChanged()` for
  `systemChanged()` like the other four users.

## Global Constraints

- One build or test at a time, in the foreground, `ninja -C build-qt612 -j 4`. Never two at once.
- Env in the SAME shell command as every meson/ninja call:
  `PATH=/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin:/home/jeandet/Qt/6.12.0/gcc_64/bin:$PATH`
  and `PKG_CONFIG_PATH=/home/jeandet/Qt/6.12.0/gcc_64/lib/pkgconfig`. Never `meson setup` in the worktree.
- Every added, renamed or removed source or public header is changed in `meson.build`, `cmake/sources.cmake`
  and `src/qcp.h` in the same task.
- Offscreen suite, Xvfb whole binary, `pgrep -x Xvfb -a | grep 1600` empty afterwards; ASan on the touched
  test classes per task, the whole ASan suite at the end.
- Bench gate: 5 % drawing rows (`QCPGpu_*`, `QCPDrawn_*`, `QCPItems_*`, `QCPBars_*`, `QCPPolar_*`,
  `QCPPlottables_*`, `QCPCoords_*`), 15 % others; A/B, five interleaved pairs, `-tickcounter -iterations 50
  -median 5`, against a build of the task's base. At the end, A/B every drawing row against the
  milestone's base too (4e/4f lesson).
- Names without "Abstract" (D7): `QCPCoordinateSystem`, `QCPCartesianSystem`, `QCPPolarSystem`.
- No shims or forwarders for NeoQCP-only API; 2.x cartesian signatures stay (design 5).
- Misuse through `QCP_CHECK_OR_RETURN`; signals through the M4a helpers; `contentChanged()` on every input a
  recording reads (a system change is one).
- A test fails before the change that makes it pass; a guard is reported as a guard. Never bend library
  behaviour to please a test. Delete what a move replaces. No clang-format on whole files.
- Clang check on every changed `.cpp` before each commit that adds a type or a template.
- Commit with explicit pathspecs; never `subprojects/.wraplock`, `subprojects/jkqtmathtext`, `qt.ini`.
  `/tmp` is RAM: remove worktrees and perf data you create.
- SciQLopPlots: anything found goes to `docs/plans/neoqcp-3/sciqlopplots-migration.md` only.

## Review Focus

1. Deleting an axis, an area or the plot while plottables sit in its systems: no crash, plottables inert,
   no dangling `QPointer` use (Task 1 `deletingAnAxisDeletesItsSystems`, Task 2 `aPlottableOfADeletedSystemIsInert`).
2. 2.x code is unchanged in behaviour: `new QCPGraph(xAxis, yAxis)`, `keyAxis()`, `setKeyAxis`, `addGraph`,
   `rescaleAxes`, `axis->plottables()` (same list, same order), selection rect, `plottableAt` (Task 2 and 4
   guards over the existing suites).
3. A plottable moved between systems, areas or plots (`setSystem`, `setKeyAxis`, `take`/`adopt`) re-records,
   rewires its axis signals and is refused across plots (Task 2 `movingBetweenSystemsReRecords`).
4. A cartesian-only plottable put in a polar system is refused, never drawn wrong (Task 3
   `barsAreRefusedInAPolarSystem`), and relationships across areas are refused (Task 3).
5. No drawing row slows down (frame memo keyed on the system; Task 2 and Task 5 A/B).

---

### Task 1: `QCPCoordinateSystem`, `QCPCartesianSystem`, `QCPPolarSystem`, owned by their areas

**Files:**
- Create: `src/coordinate-system.{h,cpp}` (`QCPCoordinateSystem`, `QCPCartesianSystem`, `QCPPolarSystem`)
- Modify: `src/layoutelements/area.{h,cpp}` (`system()`), `src/layoutelements/layoutelement-axisrect.cpp`,
  `src/polar/polar-area.cpp`, `src/axis/axis-base.{h,cpp}` (axis deletion drops its systems),
  `src/global.h` (`QCP::Capability`, `QCP::Capabilities`), build lists, `src/qcp.h`
- Test: new `tests/auto/test-coordinate-system/` (`TestCoordinateSystem`)

**Interfaces:**
- Produces: `class QCPCoordinateSystem : public QObject` with `QCPArea* area() const`, `QCPAxisBase*
  keyAxis() const`, `QCPAxisBase* valueAxis() const`, `bool usesAxis(const QCPAxisBase*) const`,
  `virtual QCPTransform transform() const = 0`, `virtual QRect clipRect() const = 0`, `virtual QPainterPath
  clipPath() const` (default: `clipRect()`), `virtual bool contains(QPointF pixel) const` (true while the
  frame is invalid, F19), `virtual QCPTransform iconTransform(const QCPDataBox&, const QRectF&) const`
  (default: `QCPTransform::fitted`), `virtual QCPKeyWindow keyWindow() const`, `virtual QCP::Capabilities
  capabilities() const = 0`, `quint64 revision() const`, `QList<QCPAbstractPlottable*> plottables() const`
  (G-5); signal `changed()`; protected `void bumpRevision()` (emits `changed()` through `qcpEmit`).
- `QCPCartesianSystem(QCPAxisRect*, QCPAxis* key, QCPAxis* value)`: `transform()` is today's
  `QCPTransform(key, value)`; `clipRect()` is `key->axisRect()->rect() & value->axisRect()->rect()` (today's
  `QCPAbstractPlottable::clipRect`); `contains` is today's `qcpDrawnClickInsideAxisRect` rule;
  `keyWindow()` today's `qcpKeyPixelsPerUnit` rule; capabilities: all. Revision bumps on either axis'
  `rangeChanged`, `scaleTypeChanged`, link change and the rect's geometry change.
- `QCPPolarSystem(QCPPolarArea*, QCPPolarAxisAngular*, QCPPolarAxisRadial*)`: `transform()` =
  `QCPTransform::polar(...)`; `clipRect()` = the disc's bounding rect; `clipPath()` = the disc; `contains`
  = inside the disc; `keyWindow()` = the arc length per angular unit at the outer radius (design 2.4);
  `iconTransform` = cartesian fitted (a polar icon is drawn like a cartesian one); capabilities: none of
  G-3's flags.
- `QCPArea::system(QCPAxisBase* key, QCPAxisBase* value) -> QCPCoordinateSystem*` (G-1), `QList<QCPCoordinateSystem*> systems() const`.

- [ ] **Step 1: Failing tests:** `onePairOneSystem` (`rect->system(x, y)` twice → same object; `(y, x)` is a
  different system with a vertical key); `aPairMustBelongToTheArea` (another rect's axis, a same-orientation
  pair → misuse, null); `cartesianTransformIsTheAxes` (`transform() == QCPTransform(x, y)`);
  `polarSystemMapsLikeTheArea` (`transform() == QCPTransform::polar(angular, radial)`);
  `revisionBumpsOnAnyMappingChange` (range, scale type, rect resize, polar rotation each bump once and emit
  `changed()` once, delivered after the call); `deletingAnAxisDeletesItsSystems` (delete a rect's extra
  axis → its systems are gone from `systems()`, others stay; ASan clean). Expected: FAIL to compile.
- [ ] **Step 2: Implement.**
- [ ] **Step 3:** Run the new tests, `TestPolar`, `TestQCPAxisRect`, `TestAxisBase`, the suite offscreen and
  under Xvfb, ASan on `TestCoordinateSystem`.
- [ ] **Step 4: Commit** `feat(coords): coordinate systems, one per axis pair, owned by their area`.

### Task 2: Plottables hold a system

**Files:**
- Modify: `src/plottables/plottable.{h,cpp}` (G-2), `src/plottables/plottable1d.h`, `src/drawable.{h,cpp}`
  (view from the system, G-6 frame memo, hit gate), `src/construct.h` (2.x constructors resolve the
  system), the five `axesChanged` users (`plottable-graph2`, `plottable-histogram`, `plottable-histogram2d`,
  `plottable-colormap2`, `plottable-multigraph`), `QCPGraph`/`QCPGraph2` (G-7), `src/core.cpp`
  (`take`/`adopt` keep a plottable's system or refuse across plots)
- Test: `tests/auto/test-coordinate-system/`, the existing plottable suites as guards

**Interfaces:**
- Produces: `QCPAbstractPlottable(QCPCoordinateSystem*)`; `QCPCoordinateSystem* system() const`;
  `void setSystem(QCPCoordinateSystem*)` (the primitive: capability check G-3, cross-plot refusal,
  re-record via `contentChanged()`, `systemChanged()` for subclasses, sync of `mKeyAxis`/`mValueAxis`);
  2.x `QCPAbstractPlottable(QCPAxis* key, QCPAxis* value)` = `system = key->axisRect()->system(key, value)`
  (null pair: inert, as today); `keyAxis()`/`valueAxis()` unchanged signatures (G-2);
  `protected virtual void systemChanged()` replaces `axesChanged()` (no forwarder: `axesChanged` was
  NeoQCP-only); `virtual QCP::Capabilities requiredCapabilities() const` (G-3).
- `view()`/`qcpDrawnViewOf` take the system: frame = `system->transform()` through the memo (G-6), clip =
  `system->clipRect()` (painter replay uses `clipPath()`), hit gate = `system->contains`;
  `coordsToPixels`/`pixelsToCoords` = the system's transform `map`/`inverse`.

- [ ] **Step 1: Failing tests:** `aPlottableTakesASystem` (`new QCPCurve(rect->system(x, y))`; `keyAxis() == x`);
  `twoXConstructorsShareTheSystem` (`QCPGraph(x, y)` and `QCPCurve(x, y)` → same `system()`);
  `movingBetweenSystemsReRecords` (`setSystem(rect->system(x2, y))`: `recordingCount` +1, `keyAxis() == x2`,
  the old axis' `rangeChanged` no longer re-records it); `aPlottableOfADeletedSystemIsInert` (delete its key
  axis → `system()` null, replot draws nothing, no crash); `crossPlotSystemIsRefused`. Guards: every
  plottable suite and `TestReferenceM4e` unchanged.
- [ ] **Step 2: Implement.**
- [ ] **Step 3:** The new tests, the plottable suites, `TestReferenceM4e`, the suite offscreen and under Xvfb,
  ASan on the new tests, A/B of every drawing row against the task's base (5 %).
- [ ] **Step 4: Commit** `refactor(plottables): a plottable is drawn in a coordinate system`.

### Task 3: Capabilities and relationships

**Files:** `src/plottables/*.{h,cpp}` (each class' `requiredCapabilities`), `plottable-graph.cpp`
(`setChannelFillGraph`), `plottable-bars.cpp` (stacking, `QCPBarsGroup`), `plottable-errorbar.cpp`
(`setDataPlottable`), the colour-scale attachment (`plottable-colormapped.cpp`); tests in
`tests/auto/test-coordinate-system/`.

- [ ] **Step 1: Failing tests:** `barsAreRefusedInAPolarSystem` (`setSystem(polarArea->system(...))` on bars →
  misuse, system unchanged); `aRecordingPlottableWithNoNeedsJoinsPolar` (a test-local `QCPAbstractPlottable`
  subclass recording a polyline, `requiredCapabilities() == {}`, draws in the polar system: a pixel on the
  expected arc); `relationshipsAcrossAreasAreRefused` (channel fill between two rects, stacking bars of two
  rects, error bars on a plottable of another rect, a colour map using another rect's scale → misuse).
- [ ] **Step 2: Implement.**
- [ ] **Step 3:** New tests, plottable suites, suite offscreen/Xvfb.
- [ ] **Step 4: Commit** `feat(plottables): capabilities and relationships are checked`.

### Task 4: Plot level through areas and systems

**Files:** `src/core.{h,cpp}` (`areas()`, `rescaleAxes`, `plottableAt`, selection rect, `axesForAdd`),
`src/construct.h` (`area->add<T>()`, `area->add<T>(key, value)`), `src/layoutelements/layoutelement-axisrect.cpp`
(`plottables()`, `markAffectedLayersDirty` by system), `src/axis/axis-base.cpp` (`plottables()`),
`src/polar/polar-area.cpp`; tests in `tests/auto/test-coordinate-system/`, `test-builder`.

**Interfaces:**
- Produces: `QList<QCPArea*> QCustomPlot::areas() const` (layout order, like `axisRects()`);
  `rescaleAxes` rescales every axis of every area; `area->add<T>(args...)` uses the area's default pair
  (an axis rect: bottom and left; a polar area: angular and first radial) and `area->add<T>(key, value,
  args...)` an explicit one; `plot.add<T>()` keeps today's rule (first axis rect, else misuse).
- A system's `changed()` marks its area's affected layers (replaces the axis-rect scan by axis).

- [ ] **Step 1: Failing tests:** `rescaleAxesVisitsEveryArea` (a polar area with a test-local plottable is
  rescaled by `plot.rescaleAxes()`); `addOnAnArea` (`rect2->add<QCPGraph>()` lands on rect2's bottom/left
  system); `axisPlottablesKeepTheirOrder` (guard: `xAxis->plottables()` equals the 2.x order on a mixed
  scene). Guards: `TestBuilder`, `TestSelectionRect`-like suites, `TestRepaint`.
- [ ] **Step 2: Implement.**
- [ ] **Step 3:** New tests, the suite offscreen/Xvfb, ASan on the new tests, A/B drawing rows.
- [ ] **Step 4: Commit** `feat(plot): rescale, search, selection and add<T> through areas and systems`.

### Task 5: Finish

**Files:** `changelog.txt` (milestone 4g), `docs/migrating.md` (`axesChanged` → `systemChanged`;
2.x unchanged otherwise), a short public-API page section on coordinate systems in `docs/` (the contract,
cartesian and polar; the custom example comes with 4i), `docs/plans/neoqcp-3/baseline.md`,
`docs/plans/neoqcp-3/backlog.md`, `docs/plans/neoqcp-3/milestone-4g-rulings.md`.

- [ ] **Step 1:** Changelog and docs.
- [ ] **Step 2:** Offscreen suite, Xvfb whole binary, ASan whole suite, clang on every changed `.cpp`.
- [ ] **Step 3:** Whole bench table (3 offscreen + 3 Xvfb) against end of 4f; A/B every drawing row and every
  row over its gate against a build of the milestone's base.
- [ ] **Step 4:** Rulings file; deferred minors also in `backlog.md`.
- [ ] **Step 5: Commit** `docs(neoqcp-3): end of milestone 4g — rulings, baseline, backlog, changelog`.

---

## Amendments after review (binding; `.superpowers/m4g-plan-review.md` findings A1-A20)

These override the task text where they differ. Each task's implementer reads the amendments for its task.

**Task 1**
- A6: `QCPAxisBase` gains signal-free notification of mapping changes that are not range/scale changes:
  `linkTo`, `unlink` and `setRangeReversed` call `area()->axisMappingChanged(this)`, and the area bumps the
  revision of every system using the axis. Range and scale-type changes bump through the same call from
  the base's range funnel.
- A11/A19: systems die while their axes still exist: `QCPAxisRect::unlistAxis` (and the polar area's axis
  unlisting) first detaches the plottables of every system using the axis (their `system()` becomes null,
  no signal, they are inert), then deletes those systems; `~QCPCoordinateSystem` touches nothing outside
  itself. An area dying before its axes does the same for all its systems in `~QCPArea`.
- A12: the concrete system constructors are private; `QCPArea`/`QCPAxisRect`/`QCPPolarArea` are friends;
  `area->system(key, value)` is the only way to obtain one.
- A13: `QCPPolarSystem` lives in `src/coordinate-system-polar.{h,cpp}` (build lists, `src/qcp.h`), so
  `coordinate-system.h` does not include the polar headers.
- A5: `QCPCoordinateSystem` gains `virtual QCPRatioTransform ratioTransform() const` (cartesian: today's);
  the polar system refuses recorded axis-ratio positions as misuse in 4g (items and spans stay cartesian
  until 4i), with a test.
- A17: `deletingAnAxisDeletesItsSystems` first asserts two systems exist and that their plottables'
  `system()` became null; `revisionBumpsOnAnyMappingChange` reads `revision()` and counts `changed()` with a
  `QSignalSpy` both outside and inside a `QCPEmissionScope` (one delivery each, after the scope).
- A18: `revision()` returns a stored `quint64`; `bumpRevision()` increments it.

**Task 2**
- A3: the new system constructor is on `QCPAbstractPlottable` (and `QCPAbstractPlottable1D`); concrete
  classes keep their axis-pair constructors in 4g. Tests that need a system constructor use a test-local
  plottable; tests on concrete classes build them from axes or call `setSystem`.
- A4/G-6: the frame memo key is `(plot, epoch, clip)` plus `(system, revision)` when the view has a system,
  else today's axes, ranges and rect (items stay on axis pairs). Test: two `QCPItemLine`s on two axis rects
  in one layer, one replot: both ends at their own rect's pixels.
- A10: `src/painting/draw-context.{h,cpp}` and `src/painting/command-replay-rhi.{h,cpp}` are in scope: the
  context takes the view's system (the axes stay for items).
- A14: `QCPDrawnView` carries `QCPCoordinateSystem*`; a plottable with a null system yields an empty view:
  nothing recorded, nothing drawn, every click misses. Extend the existing deleted-axis guards in
  `TestDrawable`/`TestRepaint` to a deleted system.
- A9: `axesChanged()` is renamed `systemChanged()` with no forwarder (NeoQCP-only API, maintainer rule D6);
  Task 5 amends design section 2.5's "old name kept until milestone 5" line accordingly.
- A20: Step 3 also compiles `examples/wasm-gallery/gallery.cpp` with `-fsyntax-only` against the new headers
  (or states why it is unaffected).

**Task 3**
- A7: the colour-scale relationship check is deferred (design 2.10; recorded in the rulings).
- A8: capability owners: every class requires `capRectangular | capStraightKey` by default;
  `QCPAbstractColorMapped` adds `capColourScale`; `QCPMultiGraph` and `QCPIntervals` add `capLanes`. One
  refusal test row per added flag.
- A15: the 4g test-local polar plottable is not selectable; `QCPSelectionDecoratorBracket` returns early
  when the plottable has no key axis (guard + test).

**Task 4**
- A1: `rescale(bool onlyVisible)` moves to `QCPAxisBase` as a virtual. `QCPAxis` keeps its 2.x
  implementation; the base implementation (used by the radial axis) covers the drawn data ranges
  (`drawnDataRange()`, `qcpDrawnRange`) of the plottables of every system using the axis; the angular axis
  overrides it to keep its range (a full turn). Test: `plot.rescaleAxes()` with a test-local plottable in a
  polar system moves the radial range and leaves the angular range.
- A2: in 4g the generic `area->add<T>()` exists on `QCPAxisRect` only (its bottom/left pair, or
  `add<T>(key, value, args...)`); `QCPPolarArea::add<QCPPolarGraph>` is unchanged until 4h.
- A11/A16: `QCPAxis::plottables()/graphs()` and `QCPAxisRect::plottables()/graphs()` select by
  `system()->usesAxis(this)` / `system()->area() == this`; items keep their axis/rect rule until 4i.
- A18: `markAffectedLayersDirty` stays one pass over the plot's plottables, selecting
  `p->system() && p->system()->area() == this`, with today's `markOnce` de-duplication.

**Task 5**
- A9: amend design section 2.5 (the `axesChanged` line). A20: `docs/axes.md` and `docs/graphs.md` checked
  and updated where they describe what a plottable holds.
