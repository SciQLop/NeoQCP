# NeoQCP 3.0 milestone 4d — axes, linked axes, alignment groups Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** axes are made and styled through a handle like everything else; dragging and zooming follow
every axis of a rect unless the axis opts out; an axis can be linked to another through a pair of
functions (matplotlib's `secondary_xaxis`); alignment groups align the edges of axis rects across plot
widgets and replace margin groups.

**Architecture:** The axis gets its own handle ops and a plot-level `add<QCPAxis>`. A linked axis holds
a shared link object (source axis, forward, inverse, direction). Its range is always `forward` of the
source's range; every change of its range, from any mutator, is sent to the source through `inverse`.
Its pixel of `v` is the source's fraction of `inverse(v)` along its own length, so ticks, grid, data
and items on it need no special case; `QCPTransform` snapshots the link the same way. Drag and zoom
resolve every axis to its source and move each source once. An alignment group is a parentless
`QObject` made by a factory with its first members; in the margin phase each member asks it for its
margin, computed from every member's own requirement in window coordinates; when a shared edge moves,
the group marks the other members' plots dirty, so they follow on the next repaint.

**Tech Stack:** C++20, Qt 6.12, meson, QtTest, the in-tree benchmark (`tests/benchmark`).

**Spec:** `docs/specs/2026-11-06-neoqcp-3-milestone-4-design.md` (order item 4 "M4d"; answers 11, 14,
15), with `docs/specs/2026-11-02-neoqcp-3-object-model-design.md` (decisions 7, 8, 12, 13; call
"alignment groups are shared by their members"). Maintainer, 2026-11-07: no interim designs, no
deprecated forwarders for NeoQCP-only APIs. Answer 11: alignment groups are built, no re-measure.
Plan review by opencode: `.superpowers/m4d-plan-review.md` (its findings are folded in below).

## Calls made in this plan (the maintainer can overrule any)

- **P-1 Interactions stay opt-in.** "Drag and zoom on by default" is read as decision 12 says it:
  every axis follows a drag or zoom by default. `setInteractions` still starts at `iNone`; turning
  wheel zoom on for every 2.x program would steal the wheel from scroll areas.
- **P-2 Default or explicit, per rect.** A rect starts with default drag and zoom sets. Any 2.x
  setter (`setRangeDragAxes`, `setRangeZoomAxes`) makes that set explicit and used whole, nulls
  meaning "none in this orientation" as in 2.x; the opt-out flags filter only the default sets.
  `resetRangeDragAxes()` / `resetRangeZoomAxes()` go back to the default. Default order: bottom, top,
  left, right, each side in its stacking order, so `rangeDragAxis(Qt::Horizontal)` is still `xAxis`.
  A plain plot with drag on now also pans its hidden top and right axes (changelog).
- **P-3 Data may sit on a linked axis.** A plottable or item on a linked axis maps through the link (a
  spectrum in wavelength on an energy plot). The transform takes a per-point path for a linked axis
  only; that path is outside the bench gate (no row draws through it).
- **P-4 `setupFullAxesBox(true)` links the top and right axes** to the bottom and left ones (identity)
  instead of a one-way `rangeChanged` connection. Two-way now: setting the top axis's range moves the
  bottom one (changelog). Tests that pin the one-way behaviour move to the new one
  (`test-signal-delivery.cpp:882` `linkedAxisLastRangeSignalIsItsRange`).
- **P-5 Links are one level, monotonic, and the source is authoritative.** A linked axis cannot be a
  source (no chains, so no cycles). Only monotonic pairs are supported (documented, not checked); the
  direction is read once at `linkTo` from `forward` at the source's range ends. After a `setRange`
  on the linked axis, its range is `forward(source)`, which may differ from the asked range by
  rounding.
- **P-6 A linked axis's reversal is the link's.** `rangeReversed()` of a linked axis is the source's,
  flipped for a decreasing link, so `pixelOrientation()` (read by bars, error bars, financial and the
  downsampler) stays right; `setRangeReversed` on a linked axis is a misuse. Its `scaleType()` stays
  free and chooses its ticks (a log wavelength axis over a log energy axis).
- **P-7 Alignment-group members are layout elements**, as margin-group members are, so a user-made
  colour scale in its own cell aligns with its rect. One group per element; joining another leaves the
  first. A side whose auto margin is off is neither aligned nor counted.
- **P-8 Edges are compared in the top-level window's coordinates**, as integers. Only relative
  positions matter. The group hears a member plot's resize and move; a move of an ancestor inside the
  window is not heard (`simplify:` in the code; upgrade path: an event filter on the ancestors).
  Members in different windows are documented as meaningless together, not checked.
- **P-9 A member is left out** of the shared edge while its plot is hidden (`!isVisible()`) or its
  outer rect is empty (never laid out). So the first show of an aligned stack takes two repaints:
  the first lays every plot out, the second aligns them.
- **P-10 `QCPMarginGroup` stays as 2.x API** until milestone 5, marked `\deprecated` in its docs. On a
  side in both kinds of group, the alignment group wins.
- **P-11 Ops names follow the code's convention** (`setLabel`, `setColor`, …, as every ops class
  does), not the spelling of decision 12's example (`.label(…).color(…)`).

## Global Constraints

- One build or test at a time, in the foreground, `ninja -C build-qt612 -j 4`. Never two at once.
- Env: `PATH=/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin:/home/jeandet/Qt/6.12.0/gcc_64/bin:$PATH`
  and `PKG_CONFIG_PATH=/home/jeandet/Qt/6.12.0/gcc_64/lib/pkgconfig`. Never reconfigure with the
  system meson; `ldd build-qt612/tests/auto/auto-tests | grep Qt6Core` must show Qt 6.12.0.
- Offscreen suite: `QT_QPA_PLATFORM=offscreen QT_LOGGING_RULES="*.debug=true" meson test -C build-qt612 --num-processes 4`;
  totals = sum of the `Totals:` lines of `build-qt612/meson-logs/testlog.txt`. GPU tests: the whole
  `auto-tests` binary under `xvfb-run -a -s "-screen 0 1600x1200x24" env QT_QPA_PLATFORM=xcb`.
- Bench gate (answer 14): baseline = end of milestone 3 in `baseline.md` (later rows: the row's own
  first measurement). `QCPGpu_*`, `QCPDrawn_*`, `QCPItems_*`, `QCPBars_*` and the touched classes' own
  rows (`QCPAxis_*`, `QCPAxisRects_*`, `QCPPlots_*`, `QCPInput_*`): 5 %; the rest 15 %; `-median 5`,
  three runs; a row over its gate gets six more runs and their median decides. The M4c re-gates
  (R-7, R-11, R-18, R-21 in `milestone-4c-rulings.md`) stand.
- No interim design, no forwarder or alias for a NeoQCP-only API this milestone replaces. 2.x
  QCustomPlot API (`setRangeDragAxes`, `setupFullAxesBox`, `QCPMarginGroup`) keeps working.
- Signals go through `qcpEmit` / `deliver` (`src/core.h`); a new signal has no overload.
- Errors: refuse misuse through `QCP_CHECK_OR_RETURN` (`src/misuse.h`), never an exception; tests count
  misuses with `tests/auto/misuse-recorder.h` (as `test-handle.cpp:209-219` does).
- A `rangeChanged` slot may delete an axis or a rect: loops over axes walk `QPointer` snapshots, as
  `forEachAxis` in `layoutelement-axisrect.cpp:545` does.
- A test fails before the change that makes it pass; a test that already passes is reported as a guard.
- Never bend library behaviour to please a test. Keep code size down: delete what a task replaces.
- Commit with explicit pathspecs; never `subprojects/.wraplock`, `subprojects/jkqtmathtext`, `qt.ini`.

## Review Focus

1. A 2.x program with `setupFullAxesBox(true)` and wheel zoom on: one wheel step zooms the bottom and
   top axes to the same range, once (Task 3 test `fullAxesBoxZoomsOnce`).
2. The source of a linked axis, or its axis rect, is deleted, or a `rangeChanged` slot deletes either
   axis during a drag: no crash, the linked axis keeps its last range and becomes a plain axis
   (Task 2 tests `deletedSourceLeavesAPlainAxis`, `slotDeletingAnAxisDuringDrag`; ASan in Task 5).
3. A decreasing link (`1/x`) with bars on it: bars grow the right way, the linked range is ordered,
   and an inverse giving a non-finite value changes nothing (Task 2 tests `decreasingLinkOrdersTheRange`,
   `barsOnADecreasingLink`, `nonFiniteInverseIsRefused`).
4. A group member hidden, never shown, or whose plot is destroyed (one or two members of it): the
   others realign without it; the last member leaving deletes the group (Task 4 tests
   `hiddenMemberIsLeftOut`, `destroyedPlotLeavesItsGroup`, `lastMemberLeavingDeletesTheGroup`).
5. After an aligned stack has settled, a repaint with no edge change makes no other plot repaint, and
   a change settles within a bounded number of repaints (Task 4 test `settledStackRepaintsNoPeer`).

---

### Task 1: Axes through a handle

**Files:**
- Modify: `src/axis/axis.h`, `src/axis/axis.cpp` (ops class, `setColor`)
- Modify: `src/construct.h`, `src/core.h` (plot-level `add<QCPAxis>`)
- Modify: `src/layoutelements/layoutelement-axisrect.h:109-116` (`add<QCPAxis>` returns the new ops)
- Test: `tests/auto/test-handle/test-handle.{h,cpp}`

**Interfaces:**
- Produces: `template <typename T, typename Self = void> class QCPAxisOps : public QCPLayerableOps<T, …>`
  with `setLabel(QString)`, `setRange(double, double)`, `setScaleType(QCPAxis::ScaleType)`,
  `setColor(QColor)`, `setTicker(QSharedPointer<QCPAxisTicker>)`, `setRangeReversed(bool)`;
  `QCPAxis::OpsFor = QCPAxisOps<T>`. Task 2 adds `linkTo`, Task 3 `setRangeDraggable` /
  `setRangeZoomable`.
- Produces: `void QCPAxis::setColor(const QColor&)`: label colour, tick label colour, and the colour of
  the base, tick and sub-tick pens (each pen keeps its width and style). Setter only.
- Produces: `plot.add<QCPAxis>(QCPAxis::AxisType)`: on the default axis rect; no default rect: one
  misuse and an empty handle.

- [ ] **Step 1: Write the failing tests** in `TestHandle`:
  `axisAddedOnThePlotStacksOnItsSide`: `auto a = plot.add<QCPAxis>(QCPAxis::atLeft).setLabel("B").setColor(Qt::red);`
  then `QCOMPARE(plot.axisRect()->axisCount(QCPAxis::atLeft), 2)`, `QCOMPARE(a->label(), "B")`,
  `QCOMPARE(a->labelColor(), QColor(Qt::red))`, `QCOMPARE(a->basePen().color(), QColor(Qt::red))`,
  `QCOMPARE(a->tickLabelColor(), QColor(Qt::red))`; after `plot.replot()` (the offsets are set by
  the layout) `a->offset()` is larger than the first left axis's.
  `axisAddedWithoutARectIsRefused`: `plot.plotLayout()->clear()`, add → empty handle, misuses == 1.
  `colorKeepsPenWidthAndStyle`: `setBasePen(QPen(Qt::black, 3, Qt::DashLine))`, `setColor(Qt::blue)`
  → width 3, dash line, colour blue.
- [ ] **Step 2: Run** `build-qt612/tests/auto/auto-tests TestHandle` (offscreen). Expected: FAIL to
  compile: the axis handle has no `setLabel` (today `add<QCPAxis>` returns the layerable ops), and
  the plot has no axis family.
- [ ] **Step 3: Implement** the ops class, `setColor`, and the axis family in `construct.h`.
- [ ] **Step 4: Run** the test, then the offscreen suite. Expected: PASS, no other change.
- [ ] **Step 5: Commit** `feat(axis): axes through a handle; setColor colours the whole axis`.

### Task 2: Linked axes

**Files:**
- Modify: `src/axis/axis.h`, `src/axis/axis.cpp` (link, the range funnel, `coordToPixel`,
  `pixelToCoord`, `setRangeReversed`)
- Modify: `src/coords.h`, `src/coords.cpp` (`QCPTransform::Axis` snapshots a link)
- Create: `tests/auto/test-linked-axes/test-linked-axes.{h,cpp}`; register in `tests/auto/meson.build`
  and `tests/auto/autotest.cpp`
- Modify: `tests/auto/test-signal-delivery/test-signal-delivery.cpp:882` only in Task 3 (P-4)

**Interfaces:**
- Consumes: `QCPAxisOps` (Task 1).
- Produces: `using QCPAxisFunction = std::function<double(double)>;`
  `void QCPAxis::linkTo(QCPAxis* source, QCPAxisFunction forward = {}, QCPAxisFunction inverse = {})`
  (both empty = identity; one empty = misuse); `void QCPAxis::unlink()`;
  `QCPAxis* QCPAxis::linkSource() const`; `std::shared_ptr<const QCPAxisLink> QCPAxis::link() const`
  (null when not linked), where `struct QCPAxisLink { QPointer<QCPAxis> source; QCPAxisFunction forward, inverse; bool decreasing; }`
  is made once by `linkTo` and shared by every snapshot. Ops: `linkTo(source, forward, inverse)`.
- Rules (P-5, P-6):
  - Refused with one misuse and no change: a null source, itself, an axis of another plot or
    orientation, a source that is itself linked, linking an axis that is some axis's source.
  - Range: always the ordered `[forward(lower), forward(upper)]` of the source's range, refreshed
    when the source's range, scale type or reversal changes (`setRangeReversed` emits nothing today:
    it refreshes its linked axes directly). Every mutator of a linked axis (`setRange` overloads,
    `setRangeLower/Upper`, `moveRange`, `scaleRange`, `setScaleRatio`) ends in one funnel that sets
    the source to the ordered inverse of the new range and lets the refresh set the linked range. A
    non-finite or invalid inverse: misuse, no change. A guard flag stops the refresh from writing back.
  - Pixels: the pixel of `v` is the source's own fraction of `inverse(v)` (its scale type and
    reversal included), placed along the linked axis's own length; `pixelToCoord` is its inverse.
    The ±200 px sentinel of a log source scales with the length ratio (documented).
  - `rangeReversed()` = source reversed XOR `decreasing`; `setRangeReversed` on a linked axis: misuse.
  - Source deleted (directly or with its rect): the link drops; the axis keeps its range and is plain.
  - `QCPTransform::Axis` copies the link pointer and the source's snapshot; a linked axis maps per
    point (never the `mapRange` fast path), is never `isTranslationOf` another, and two snapshots of
    one link with equal source snapshots compare equal (explicit `operator==`, comparing the link
    pointer), so the recording and resolve caches still hit (`drawable.cpp:470`,
    `painting/command-replay-rhi.cpp:446`). `QCPTransform` reads `QCPAxis::link()`.

- [ ] **Step 1: Write the failing tests** in `TestLinkedAxes` (`xAxis` 1..10 unless said):
  `linkedRangeFollowsTheSource`: `xAxis2->linkTo(xAxis, x→2x, x→x/2)`; `xAxis->setRange(2, 4)` →
  `xAxis2->range() == QCPRange(4, 8)`.
  `everyMutatorMovesTheSource`: `xAxis2->setRange(10, 20)` → `xAxis == (5, 10)`; then
  `xAxis2->scaleRange(2, 15)` → `xAxis == (2.5, 12.5)`, `xAxis2 == (5, 25)`; same for `moveRange(2)`
  and `setRangeUpper(30)`; after each, `xAxis2->range()` is `forward(xAxis->range())`.
  `writeBackSettles`: forward `x→x*3`, inverse `x→x/3` (inexact in floating point); count
  `rangeChanged` of both over `QCPEmissionScope` and event processing after `xAxis2->setRange(1, 2)`:
  one each.
  `linkedPixelsMatchTheSourceOnOneRect`: for `v` in {1, 3.3, 10}: `xAxis2->coordToPixel(2*v)` equals
  `xAxis->coordToPixel(v)` within 1e-9; again with `xAxis` logarithmic, and with `xAxis` reversed.
  `linkOnAnotherRectUsesItsOwnLength`: a second rect in column 1 of row 0, column stretch 1:3; its
  bottom axis linked (identity) to `xAxis`; after `replot()`, the linked axis's pixel of the source's
  centre value is its own rect's horizontal centre (not the source rect's).
  `decreasingLinkOrdersTheRange`: forward and inverse `x→1/x`; `xAxis` range `(0.1, 1)`;
  `xAxis2->range() == (1, 10)`, `xAxis2->rangeReversed()`, `xAxis2->coordToPixel(10) == xAxis->coordToPixel(0.1)`.
  `barsOnADecreasingLink`: bars with key axis `xAxis2` of that link, export to an image: the bar at
  key 2 sits at `xAxis->coordToPixel(0.5)`; its width points the way `xAxis2->pixelOrientation()` says.
  `nonFiniteInverseIsRefused`: inverse `x→1/x`; `xAxis2->setRange(0, 5)` → misuses == 1, both ranges
  unchanged.
  `reversingTheSourceRefreshesTheLink`: `xAxis->setRangeReversed(true)` → `xAxis2->rangeReversed()`.
  `dataOnALinkedAxisMapsThroughTheLink` (same rect): `QCPTransform(xAxis2, yAxis).map({2*v, y})`
  equals `QCPTransform(xAxis, yAxis).map({v, y})`; two snapshots of the same state compare equal; a
  snapshot after a source pan is not `isTranslationOf` the first.
  `refusedLinks`: to itself, to a vertical axis, to an axis of another plot, to an axis that is
  linked, linking an axis that is a source, one empty function: misuses == 1 each, nothing linked.
  `deletedSourceLeavesAPlainAxis`: link `xAxis2` to an extra bottom axis; delete that axis →
  `linkSource() == nullptr`, range unchanged, `setRange` works alone. Same with the source's rect
  removed from the layout.
  `slotDeletingAnAxisDuringDrag`: two runs, a slot on the source's `rangeChanged` deletes the linked
  axis, then the source; a mouse drag over the rect (`iRangeDrag`) completes without a crash.
- [ ] **Step 2: Run** `auto-tests TestLinkedAxes`. Expected: FAIL to compile (no `linkTo`).
- [ ] **Step 3: Implement** the link in `QCPAxis` and the transform snapshot.
- [ ] **Step 4: Run** the test, the offscreen suite, and `qcp_benchmark -median 5 QCPAxis_SetRange10k`
  plus, under Xvfb, `QCPGpu_MultiGraphPan1M`: an unlinked axis must not slow down. Expected: PASS,
  rows within the gate.
- [ ] **Step 5: Commit** `feat(axis): linked axes, mapped through their source`.

### Task 3: Drag and zoom follow every axis; per-axis opt-out

**Files:**
- Modify: `src/axis/axis.h`, `src/axis/axis.cpp` (two flags; `QCPAxis::mousePressEvent`,
  `mouseMoveEvent`, `wheelEvent` at `axis.cpp:1980-2103`, the per-axis drag and zoom path)
- Modify: `src/layoutelements/layoutelement-axisrect.{h,cpp}`: constructor `:219-220`,
  `setupFullAxesBox` `:626`, `zoom(QRectF, …)` `:594-607`, `mousePressEvent` `:1480-1507`,
  `mouseMoveEvent` `:1513-1540`, `panByPixels` `:1586-1593`, `zoomAxesAt` `:1600-1612`, the drag/zoom
  getters and setters `:1038-1330`
- Modify: `tests/auto/test-signal-delivery/test-signal-delivery.cpp:882` (P-4), and check
  `tests/auto/test-ownership/test-ownership.cpp:1172` still holds
- Test: `tests/auto/test-qcpaxisrect/test-qcpaxisrect.{h,cpp}`

**Interfaces:**
- Consumes: `QCPAxis::linkSource()`, `linkTo` (Task 2).
- Produces: `void QCPAxis::setRangeDraggable(bool)`, `bool rangeDraggable() const`,
  `void setRangeZoomable(bool)`, `bool rangeZoomable() const`, default true; ops of the same names.
  `void QCPAxisRect::resetRangeDragAxes()`, `void resetRangeZoomAxes()`.
- Rules (P-2): a default set is every axis of the rect of that orientation, ordered bottom, top, left,
  right and by stacking index on a side, whose flag is on. `rangeDragAxes(o)` / `rangeZoomAxes(o)` and
  `rangeDragAxis(o)` / `rangeZoomAxis(o)` return the effective set. A drag, a wheel zoom, a gesture
  pan and `zoom(QRectF)` move each source once: one helper turns a list into its moves, keeping per
  source the source itself when listed, else the first listed axis linked to it, which then moves its
  source through the funnel of Task 2 by its own pixel motion. The constructor no longer sets explicit
  lists. `setupFullAxesBox(true)` links `xAxis2` to `xAxis` and `yAxis2` to `yAxis` (P-4).

- [ ] **Step 1: Write the failing tests** in `TestQCPAxisRect`:
  `dragMovesEveryAxisByDefault`: add a second left axis; `iRangeDrag`; drag 50 px up → both left
  axes moved, each by its own 50 px.
  `defaultSetKeepsThePrimaryAxisFirst`: `rangeDragAxis(Qt::Horizontal) == xAxis`,
  `rangeDragAxes(Qt::Horizontal) == {xAxis, xAxis2}`, same for zoom and vertical.
  `optedOutAxisStaysPut`: `axis->setRangeDraggable(false)` → that axis unchanged after the drag; same
  for `setRangeZoomable(false)` and one wheel step.
  `explicitListWinsWhole`: `xAxis->setRangeDraggable(false)`, `setRangeDragAxes(xAxis, nullptr)` →
  a drag moves `xAxis` and no vertical axis.
  `resetRestoresTheDefault`: then `resetRangeDragAxes()` → a drag moves `yAxis` and not `xAxis`.
  `linkedAxisMovesItsSourceOnce`: `xAxis2->linkTo(xAxis)`; one wheel step over the rect → `xAxis`
  equals a one-step zoom of its old range (computed with `scaleRange` on a copy plot without the link).
  `draggingOnlyTheLinkedAxisMovesTheSource`: `setRangeDragAxes(xAxis2, nullptr)` with an `x→2x` link
  on the same rect; drag 40 px → `xAxis` equals what a 40 px drag of `xAxis` alone gives.
  `fullAxesBoxZoomsOnce`: `setupFullAxesBox(true)`, `iRangeZoom`, one wheel step through
  `QCPAxisRect::wheelEvent` → `xAxis2->range() == xAxis->range()`, and `xAxis->range()` is one zoom
  step of the original.
- [ ] **Step 2: Run** `auto-tests TestQCPAxisRect`. Expected: FAIL to compile (no `setRangeDraggable`).
- [ ] **Step 3: Implement** the flags, the default sets, the reset functions and the move helper;
  update `linkedAxisLastRangeSignalIsItsRange` to the two-way link (the last signal of `xAxis2` is
  its range, now `(5, 6)`, and `xAxis` follows) and list it in the report.
- [ ] **Step 4: Run** the test, the offscreen suite, and `qcp_benchmark -median 5 QCPInput_MouseMoveStorm`.
  Expected: PASS; the row within its gate.
- [ ] **Step 5: Commit** `feat(axisrect): drag and zoom follow every axis; per-axis opt-out`.

### Task 4: Alignment groups

**Files:**
- Create: `src/alignment-group.h`, `src/alignment-group.cpp`; add to `meson.build` sources and
  `src/qcp.h`
- Modify: `src/layout.h` (friend `QCPAlignmentGroup`, as `QCPMarginGroup` is, for
  `calculateAutoMargin`), `src/layout.cpp` (`QCPLayoutElement::update(upMargins)` at `:505-541`, the
  element destructor `:258-270`, `QCPMarginGroup` docs `\deprecated`)
- Modify: `src/core.h`, `src/core.cpp` (`resizeEvent` at `core.cpp:3697` and a new `moveEvent` tell
  the groups of the plot's elements)
- Modify: `tests/benchmark/benchmark.cpp` (row `QCPPlots_Stack8_Aligned_Replot`)
- Create: `tests/auto/test-alignment-group/test-alignment-group.{h,cpp}`; register it

**Interfaces:**
- Produces: `class QCP_LIB_DECL QCPAlignmentGroup : public QObject` with
  `static QPointer<QCPAlignmentGroup> create(const QList<QCPLayoutElement*>& members, QCP::MarginSides sides = QCP::msLeft | QCP::msRight)`;
  `void add(QCPLayoutElement*)`; `void remove(QCPLayoutElement*)`;
  `QList<QCPLayoutElement*> elements() const`; `QCP::MarginSides sides() const`; signal
  `membersChanged()`. `QPointer<QCPAlignmentGroup> QCPLayoutElement::alignmentGroup() const`.
- Rules:
  - Ownership: parentless; the members own it; the returned pointer is a view. It deletes itself
    (`deleteLater`) when its last member leaves. A member leaves when destroyed (first thing in
    `~QCPLayoutElement`, before its plot can be asked to repaint).
  - Refused with one misuse: `create` with an empty list, a null member or a member without a plot
    (returns null, no group left behind); `add` of a null or plotless element. `add` of a member
    already in this group and `remove` of a non-member do nothing. `add` of a member of another group
    moves it (P-7).
  - Requirement of a member on a side: its outer rect edge in window coordinates (an integer), moved
    inward by `max(calculateAutoMargin(side), minimumMargin)`. Members left out: P-9, and sides whose
    auto margin is off (P-7). The shared edge: the innermost requirement (largest left/top, smallest
    right/bottom). A member's margin on that side puts its inner edge on the shared edge.
  - When a side's shared edge changes (exact integer compare with the last one used), the group marks
    the other members' plots dirty (decision 7: no `replot()`); otherwise it marks nothing. A resize or
    move of a member's plot makes the group recompute on that plot's next layout.
  - A side in an alignment group ignores a margin group on the same side (P-10).

- [ ] **Step 1: Write the failing tests** in `TestAlignmentGroup`. Plots sit in one shown `QWidget`
  (`show()`, `QTest::qWaitForWindowExposed`), with a fixed-width spacer left of plot B so A and B do
  not start at the same x; "settled" means replot, process events, repeat until no plot repaints
  (at most 4 rounds, asserted):
  `leftEdgesAlignAcrossPlots`: A's y range `0..1e9` (wide labels), B's `0..1`; before grouping the
  inner left edges in window coordinates differ (red line); after `create({A.axisRect(), B.axisRect()})`
  and settling, they are equal.
  `rightEdgesAlignWithAColourScale`: B shows its rect's colour scale; settled right inner edges equal.
  `userColourScaleCellAligns`: one plot, a rect and a user-made `QCPColorScale` in the next column,
  grouped with `msTop | msBottom`: the scale's inner top and bottom equal the rect's.
  `stackedRectsInOnePlotAlign`: two rects in one plot's layout, one group: settled lefts equal.
  `firstShowSettlesInTwoRepaints`: a new aligned pair shown: aligned after at most two repaints each.
  `hiddenMemberIsLeftOut`: A (wide) hidden → B's left margin is its own `calculateAutoMargin`; A shown
  again → aligned once settled.
  `destroyedPlotLeavesItsGroup`: three members, delete A → `elements().size() == 2`, B and C settle
  on their own edge; a plot holding two members of one group destroyed → no crash, group size drops 2.
  `lastMemberLeavingDeletesTheGroup`: remove both → after
  `QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete)` the `QPointer` is null.
  `joiningAnotherGroupLeavesTheFirst`; `refusedCreate`: empty list, null member, plotless element:
  misuses == 1 each, null result.
  `settledStackRepaintsNoPeer`: settle A and B; change B's data only; count A's `afterReplot` over
  `QTest::qWait(200)`: 0. Then widen B's labels: A repaints at least once and the stack settles.
  `alignmentWinsOverMarginGroup`: a rect in both kinds of group on `msLeft`: its inner left edge is the
  alignment group's shared edge, not the margin group's common margin.
- [ ] **Step 2: Run** `auto-tests TestAlignmentGroup`. Expected: FAIL to compile.
- [ ] **Step 3: Implement** the group and the hook in `update(upMargins)`. The bench row stacks 8
  plots as `QCPPlots_Stack8_Replot` (`benchmark.cpp:1439`) does, in one group, settled before timing;
  it gates the margin arithmetic only (convergence is the tests' job).
- [ ] **Step 4: Run** the test, the offscreen suite, and `qcp_benchmark -median 5 QCPPlots_Stack8_Replot QCPPlots_Stack8_Aligned_Replot`.
  Expected: PASS; the aligned row within 15 % of the unaligned one, measured in the same run.
- [ ] **Step 5: Commit** `feat(layout): alignment groups align edges across plots`.

### Task 5: Finish

**Files:**
- Modify: `changelog.txt`, `docs/plans/neoqcp-3/baseline.md`, `docs/plans/neoqcp-3/backlog.md`,
  `src/doc-layoutsystem.dox` (alignment groups; margin groups deprecated)
- Create: `docs/plans/neoqcp-3/milestone-4d-rulings.md`

- [ ] **Step 1:** Changelog, milestone 4d section: default drag and zoom sets (several axes on a side,
  and the hidden top and right axes, now move); `setupFullAxesBox(true)` links both ways (P-4);
  `setRangeReversed` refused on a linked axis; linked axes; axis handle and `setColor`; alignment
  groups; margin groups deprecated.
- [ ] **Step 2:** Offscreen suite, Xvfb whole binary, ASan suite (`build-asan`), and the clang check:
  for every `.cpp` changed in the milestone, its command from `build-qt612/compile_commands.json`
  with the `sccache` prefix and output flags removed, run as `clang++ -fsyntax-only`. Expected:
  0 failed, no sanitizer report, clang clean.
- [ ] **Step 3:** Bench: every row, three runs, table "End of milestone 4d" in `baseline.md` against
  end of milestone 3, ratios, gates as in Global Constraints.
- [ ] **Step 4:** Rulings file in the shape of `milestone-4c-rulings.md` (results, rulings with cost,
  found and fixed, deferred minors); deferred minors also go to `backlog.md`.
- [ ] **Step 5: Commit** `docs(neoqcp-3): end of milestone 4d — rulings, baseline, changelog`.
