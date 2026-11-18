# NeoQCP 3.0 milestone 6d (identity and hooks) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The lollipop, callout, brick-breaker and Asteroids targets (and peak-fit, which also needed 6e) compile
unchanged, run, and have reference pictures: per-datum and per-part identity from the recording, mouse handlers on
items, anchors placed by a function, `QCPItem::changed()`, `dataRect()`, key signals on the plot, and legend icons
drawn in the icon's own frame.

**Architecture:** The recording carries a tag per command (data index, part) set by `QCPDrawContext::setDataIndex` /
`setPart`; the hit test reads the tag of the topmost command within tolerance and hands it on in the hit details.
Items get Qt-style virtual handlers taking a value event, `QCPMouseEvent`, built by `QCPItem` from the plot's
existing grab logic (the topmost layerable that accepts the press gets the drag, so an accepted press already stops
the pan). Anchors become functions returning a `QCP::Position`, resolved through the item's frame like any recorded
position. Keys leave the plottable virtual for two plot signals.

**Tech Stack:** C++23 (GCC 14 floor), Qt 6.12, meson + `cmake/sources.cmake`, QtTest (`tests/auto`, one binary).

**Spec:** `docs/specs/2026-11-14-neoqcp-3-milestone-6-design.md`; the change list is
`docs/plans/neoqcp-3/m6-gaps.md` themes 3 and 4 (rows named below) with maintainer decisions 6, 7, 8, 10, 11, 17;
the targets are `examples/user-code/{lollipop,callout,brick-breaker,asteroids,peak-fit}/main.cpp`.

## Global Constraints

- The targets compile **unchanged** from their committed text, except the one edit P-1 asks the maintainer for.
- No interim designs: no deprecated forwarders, no old name kept beside the new one (`createAnchor(name, int)`,
  `anchorPixelPosition`, `QCPPlottable::keyPress`, public `dataExtent()` all go).
- Every break goes into `docs/migrating.md` (the 3.0 section) and one line into the SciQLopPlots migration notes.
- Every rule from earlier milestones stands: signals through `qcpEmit` (held while an emission scope is open, 4a/5e),
  a slot may delete the object or the plot (5e), handles for plot-owned objects (5b).
- Failing test first for every change. Whole suite green locally under Xvfb and on Coder (`build`, `build-asan`,
  `build-fco`); reference pictures drawn and checked locally (fonts). clang `-fsyntax-only` clean on changed files.
- `-j 4` locally, one build per build directory at a time, never `meson setup` in the local worktree, no build trees
  in /tmp. Explicit pathspecs; never commit `subprojects/.wraplock`, `subprojects/jkqtmathtext`, `.superpowers`.
  Never push.

## Pre-flight decisions (for the maintainer)

- **P-1 (target edit, brick-breaker line 165):** `QList::removeIf` hands its predicate a `const` reference, so
  `[&](QCPHandle<Brick>& brick)` cannot compile whatever the library does. Proposed: the target reads
  `const QCPHandle<Brick>& brick`, and `QCPHandle::remove()` becomes `const` (it deletes the object; the handle's
  `QPointer` clears itself, so nothing in the handle changes).
- **P-2 (which part a press hits):** the topmost (latest recorded) command within the selection tolerance names the
  data index and the part; the distance the object reports is still the nearest command's. So a peak's edge line,
  recorded after its body, is grabbed anywhere within 5 px of it, also from inside the body. Alternative: the nearest
  command wins, ties to the later one (edges then need a hit right on the line).
- **P-3 (`changed()`):** one emission per `update()` call, through `qcpEmit` (held while a scope is open), none while
  the item is being destroyed. Not coalesced: a slot sees each change.
- **P-4 (keys):** the plot ignores the event, then emits `keyPress(QKeyEvent*)`; a slot that calls `accept()` stops
  the built-in handling after it (Delete on selected items) and the propagation to the parent widget. `QCPIntervals`
  connects to its plot's signal (rebound when it moves to another plot). Focus policy `Qt::StrongFocus`.
- **P-5 (`dataExtent`):** public `dataRect()` (a plain `QRectF` in data coordinates, from the recording, made on
  demand). The spatial index's cheap positions-based answer for built-in items stays, renamed to a protected,
  `\internal` virtual `indexExtent()`; the public virtual `dataExtent()` goes.
- **P-6 (legend icon frame):** `QCP::AxisRatio` and pixel positions in `drawLegendIcon` are in the icon rect; an icon
  recorded without any data position is drawn (today it is skipped). Data positions still fit their own box (polar
  and colour-map icons keep working). Built-in icons that record an invisible rect only to give the fit a box move to
  `AxisRatio` and drop it; every legend reference picture stays pixel-identical.
- **P-7 (`QCPMouseEvent`):** arrives accepted, and `QCPItem`'s default handlers call `ignore()` (Qt's rule). Move and
  release go to the item that accepted the press, with the press's part. `lastPixels()` / `lastData()` are the
  previous event of the drag (the press for the first move). `data()` maps through the item's own frame (its rect's
  axes, log and reversed axes included). Double click is offered; hover is not (no target needs it).

## Review Focus

1. A slot or handler that deletes the item, or the plot, from inside `mouseMoveEvent`, `changed()` or `keyPress`:
   no crash, no event to a dead item (Task 3, Task 5, Task 6 tests).
2. A drag that leaves the item and the axis rect keeps going to the grabbing item with its press part, and the
   release outside the plot ends it (Task 3 test).
3. `event->data()` on a log or reversed axis and on an item in the second rect of a grid matches that rect's own
   axes (Task 3 test).
4. A recording whose tags change between two recordings (a lollipop re-recorded with fewer points; a command after
   `setDataIndex(-1)`) reports no stale index (Task 1 test).
5. `dataRect()` asked before the first replot answers the rect the item will draw (Task 5 test).

---

### Task 1: Per-datum and per-part identity in the recording

**Files:**
- Modify: `src/painting/draw-context.h`, `src/painting/draw-context.cpp`, `src/painting/draw-commands.h`
  (`QCPDrawList`), `src/drawable.cpp` (`QCPDrawnCore::hitTest`), `src/plottables/plottable.cpp`
  (`QCPPlottable::selectTest`), `src/plottables/plottable1d.h`, `src/selection.h`, `src/selection.cpp`
- Test: `tests/auto/test-drawable/test-drawable.{h,cpp}`, `tests/auto/test-selection-model/…`

**Interfaces:**
- Produces: `void QCPDrawContext::setDataIndex(int index)` (-1 = none, the default at the start of a recording);
  `void QCPDrawContext::setPart(int part)` (-1 = none); both tag every command recorded after them.
  Hit details for a tagged hit: `QVariantMap { {"dataIndex", int}, {"part", int} }` (keys present only when ≥ 0),
  read by the existing `hitDataIndex(const QVariant&)` and by the new `int qcpHitPart(const QVariant& details)`
  (in `src/drawable.h`, -1 when none). `const DataType& QCPPlottable1D<D>::at(int index) const`.
  `bool QCPDataSelection::contains(int dataIndex) const`.

- [ ] **Step 1: Write the failing tests** in `TestDrawable`:
  `aTaggedCommandNamesItsDataIndex` (a plottable recording three discs at keys 1, 2, 3 with `setDataIndex(i)`; a
  click on the second gives `hitDataIndex(details) == 1`, and `selectTest` details convert to
  `QCPDataSelection(QCPDataRange(1, 2))`), `theTopmostTaggedCommandWins` (a filled rect with part 0 then a line on
  its edge with part 1: a click 3 px inside the rect from the line gives part 1; a click in the middle gives part 0),
  `aRecordingWithFewerPointsForgetsTheOldTags` (re-record with one point; a click where the third was misses),
  `untaggedCommandsKeepTheBatchDetails` (bars still report their batch index as today). In `TestSelectionModel`:
  `containsOneIndex` (`QCPDataSelection(QCPDataRange(2, 5)).contains(4)` true, `contains(5)` false).
  In `TestDrawable`: `aUserPlottableIsNotRestyledWhenSelected` (decision 11: a user type that records a fixed pen
  replays that pen with its selection set).
- [ ] **Step 2: Run them, expect compile failure** (`setDataIndex`, `contains(int)`, `at` missing):
  `PATH=/home/jeandet/Qt/6.12.0/gcc_64/bin:$PATH ninja -C build-qt612 -j 4 tests/auto/auto-tests`
- [ ] **Step 3: Implement.** Tags live in `QCPDrawList` as runs (`{firstCommand, dataIndex, part}` appended only when
  the tag changes), so an untagged recording costs nothing. `QCPDrawnCore::hitTest` keeps the nearest distance as
  its return value and takes the details of the **last** command within tolerance (P-2); untagged commands keep
  today's batch-index details. `QCPPlottable::selectTest` turns a `dataIndex` into a one-point selection.
- [ ] **Step 4: Run the class, expect PASS:**
  `QCP_TEST_CLASS=TestDrawable xvfb-run -a -s "-screen 0 1600x1200x24" ./build-qt612/tests/auto/auto-tests`
  (same for `TestSelectionModel`), then the whole suite on Coder.
- [ ] **Step 5: Commit** `feat(api): a recording names the datum and the part each command draws`.

### Task 2: Legend icons in the icon's frame; lollipop builds

**Files:**
- Modify: `src/drawable.cpp` (`QCPDrawnCore::paintIcon`), every `drawLegendIcon(QCPDrawContext&)` that records an
  invisible rect for the fit (`plottable-bars.cpp:1203`, `plottable-graph.cpp:2926`, the others found by
  `grep -n "NoBrush" ` inside those functions), `examples/user-code/meson.build`, `examples/user-code/README.md`
- Test: `tests/auto/test-qcplegend/…`, `tests/auto/test-reference-m4e/…` (lollipop picture)

**Interfaces:**
- Consumes: Task 1's `setDataIndex`, `at`, `contains(int)`.
- Produces: icon rule P-6.

- [ ] **Step 1: Failing tests** in `TestQCPLegend`: `aRatioOnlyIconIsDrawn` (a user plottable whose icon is one
  `AxisRatio` line from (0.5, 1) to (0.5, 0): the legend's icon rect has the pen colour in its middle column and not
  at its left edge), `aPixelIconSitsInTheIconRect` (a `3_px` circle at `AxisRatio {0.5, 0.5}` is centred on the
  icon rect). The existing legend reference pictures are the guard that built-in icons do not move.
- [ ] **Step 2: Run, expect the two tests to FAIL** (nothing drawn).
- [ ] **Step 3: Implement P-6.** The ratio transform is already `QCPRatioTransform::fitted(rect)`; the change is the
  early return and the built-in icons. Add `lollipop` to the `foreach` in `examples/user-code/meson.build`.
- [ ] **Step 4: Add the reference picture** `lollipop-target` to `TestReferenceM4e`, drawn by the target's own class
  and scene (copied, as 6f's `spectrogram-single-rect` copies its scene) at 640 x 400, with the 5th lollipop selected
  by a simulated click (`stSingleData`: one thick stem, one large disc). Draw it locally under Xvfb
  (`QCP_WRITE_REFERENCE=1` the first time) and look at it.
- [ ] **Step 5: Run `TestQCPLegend`, `TestReferenceM4e`, then the whole suite; commit**
  `feat(api): a legend icon draws in its own frame; the lollipop target builds`.

### Task 3: Mouse handlers on items

**Files:**
- Create: `src/items/item-mouse-event.h`, `src/items/item-mouse-event.cpp` (add both to `meson.build`,
  `cmake/sources.cmake`, and `qcustomplot.h`'s include list)
- Modify: `src/items/item.h`, `src/items/item.cpp`, `src/items/item-span.{h,cpp}`, `src/items/item-rspan.{h,cpp}`
- Create test: `tests/auto/test-item-events/test-item-events.{h,cpp}` (register in `tests/auto/meson.build` and
  `QCPTEST(TestItemEvents)` in `autotest.cpp`)

**Interfaces:**
- Consumes: Task 1's `qcpHitPart(details)`.
- Produces:
  ```cpp
  class QCP_LIB_DECL QCPMouseEvent {
  public:
      QCP::Pixels pixels() const;  QCP::Pixels lastPixels() const;
      QCP::Data data() const;      QCP::Data lastData() const;
      int part() const;            // -1 when the press hit no tagged command
      Qt::MouseButton button() const; Qt::MouseButtons buttons() const;
      Qt::KeyboardModifiers modifiers() const;
      void accept(); void ignore(); bool isAccepted() const;
  };
  // QCPItem, protected virtuals (defaults call event->ignore()):
  virtual void mousePressEvent(QCPMouseEvent* event);
  virtual void mouseMoveEvent(QCPMouseEvent* event);
  virtual void mouseReleaseEvent(QCPMouseEvent* event);
  virtual void mouseDoubleClickEvent(QCPMouseEvent* event);
  ```
  `QCPItem` overrides the four `QCPLayerable` `QMouseEvent` handlers as `final`, builds the event (P-7) and copies
  its accepted state back onto the `QMouseEvent`.

- [ ] **Step 1: Failing tests** in `TestItemEvents`: `anAcceptedPressGrabsTheDragAndStopsThePan` (an item accepting
  every press, `iRangeDrag` on: a drag over it moves the item's recorded state and leaves the axis range unchanged),
  `anIgnoredPressPans` (the callout's Pointer rule: ignoring part 0 pans the plot), `theDragKeepsThePressPart`
  (press on part 2, move over part 1: every move reports 2), `lastIsThePreviousEvent` (press at x, moves to x+10,
  x+25: the second move reports `pixels - lastPixels == 15`), `dataFollowsTheItemsOwnAxes` (Review Focus 3: a
  reversed log x axis, and an item in rect (1, 0) of a 2 x 1 grid), `aDragOutsideThePlotStillEndsOnRelease`
  (Review Focus 2), `anItemDeletedInItsMoveHandler` and `aPlotDeletedInAPressHandler` (Review Focus 1, run under
  ASan on Coder), `spansStillDrag` (the span's existing drag tests keep passing through the new handlers).
- [ ] **Step 2: Run `TestItemEvents`, expect compile failure.**
- [ ] **Step 3: Implement.** The plot's dispatch in `core.cpp` (press: topmost accepting candidate becomes
  `mMouseEventLayerable`) stays; only items change. The item keeps the press part and the last pixels between
  events. `data()` maps through the item's frame (`qcpResolvePosition`'s inverse on the item's transform).
  Spans move their drag from the `QMouseEvent` overrides to the new ones.
- [ ] **Step 4: Run `TestItemEvents`, `TestSpan`, `TestRSpan`, then the whole suite (Coder, ASan included).**
- [ ] **Step 5: Commit** `feat(api): items take mouse handlers with a value event; an accepted press grabs the drag`.

### Task 4: Anchors placed by a function

**Files:**
- Modify: `src/items/item.h`, `src/items/item.cpp`, and every built-in item using `createAnchor(name, int)` /
  `anchorPixelPosition` (`item-bracket`, `item-ellipse`, `item-pixmap`, `item-rect`, `item-richtext`, `item-rspan`,
  `item-span`, `item-text`, their headers; 42 + 24 sites)
- Test: `tests/auto/test-item-position/…`

**Interfaces:**
- Produces: `QCPItemAnchor* QCPItem::createAnchor(const QString& name, std::function<QCP::Position()> where)`
  (protected; callable from a derived class's member initialisers, before the item has a plot). The anchor's
  `pixelPosition()` resolves `where()` through the item's frame; `QCPItemAnchor`'s `anchorId` and
  `QCPItem::anchorPixelPosition(int)` are deleted. Built-in items return `QCP::Pixels` from their lambdas.

- [ ] **Step 1: Failing tests** in `TestItemPosition`: `aFunctionAnchorFollowsData` (anchor returning
  `QCP::Data {2, 3}`: its pixel position equals the axes' map of (2, 3), and follows a range change),
  `aFunctionAnchorWithAPixelOffset` (`QCP::Data {2, 3} + QCP::PixelOffset {10, -5}`), `anAnchorMadeBeforeThePlot`
  (an item built bare, anchors in member initialisers, then `ax->add`: positions hanging from it resolve),
  `aPositionHangingFromAFunctionAnchorRepaintsWhenItMoves` (the follower is marked dirty on the source's `update()`).
- [ ] **Step 2: Run, expect compile failure.**
- [ ] **Step 3: Implement; migrate every built-in item** (each `case aiX:` becomes a lambda; the `ai*` enums go).
- [ ] **Step 4: Run `TestItemPosition`, the item test classes, the reference pictures, then the whole suite.**
- [ ] **Step 5: Commit** `feat(api)!: an anchor is a function returning a position`.

### Task 5: `changed()`, `dataRect()`, `remove() const`; callout and peak-fit build

**Files:**
- Modify: `src/items/item.{h,cpp}` (and the overrides of `dataExtent` in `item-rect`, `item-ellipse`, `item-line`,
  `item-bracket`, `item-curve`), `src/drawable.{h,cpp}`, `src/layer.cpp`, `src/core.h`, `src/handle.h`,
  `examples/user-code/brick-breaker/main.cpp` (P-1 only), `examples/user-code/meson.build`
- Test: `tests/auto/test-item-recording/…`, `tests/auto/test-handle/…`, `tests/auto/test-reference-m4e/…`

**Interfaces:**
- Produces: `signal void QCPItem::changed()` (P-3); `QRectF QCPItem::dataRect() const` (P-5: left = key lower,
  top = value lower, positive width and height; an empty `QRectF` when the recording holds no data position);
  protected `\internal virtual std::optional<QCPItemExtent> indexExtent() const` (the former `dataExtent`);
  `void QCPHandle<T>::remove() const`.

- [ ] **Step 1: Failing tests** in `TestItemRecording`: `updateEmitsChanged` (two `update()` calls outside a scope:
  two emissions; inside a `QCPEmissionScope`: delivered after it closes), `aSlotDeletingTheItemOnChanged`
  (Review Focus 1), `dataRectIsTheRecordedBox` (a brick recording `rect(QRectF(10, 36, 8, 3))` answers that rect),
  `dataRectBeforeTheFirstReplot` (Review Focus 5). In `TestHandle`: `removeThroughAConstHandle`.
- [ ] **Step 2: Run, expect compile failure.**
- [ ] **Step 3: Implement**; rename `dataExtent` to `indexExtent` across `src/` and tests; apply P-1 to
  brick-breaker; add `callout` and `peak-fit` to the user-code build.
- [ ] **Step 4: Reference pictures** in `TestReferenceM4e`, each drawn by the target's own classes and scene:
  `callout-target` (720 x 460, after a simulated drag of the box by (+40, +20) and a corner drag by (+30, +10)),
  `peak-fit-target` (800 x 560, after dragging the middle peak's body by +5 keV: the fit and residual follow). Draw
  locally, look at each, describe them in the report.
- [ ] **Step 5: Whole suite; commit** `feat(api): item changed(), dataRect(); the callout and peak-fit targets build`.

### Task 6: Keys as plot signals; brick-breaker and Asteroids build; docs

**Files:**
- Modify: `src/core.h`, `src/core.cpp` (`keyPressEvent`, new `keyReleaseEvent`, focus policy at `core.cpp:644`),
  `src/plottables/plottable.h` (delete `keyPress`), `src/plottables/plottable-intervals.{h,cpp}`,
  `examples/user-code/meson.build`, `examples/user-code/README.md`, `docs/migrating.md`,
  the SciQLopPlots migration notes, `docs/plans/neoqcp-3/m6-gaps.md`
- Test: `tests/auto/test-qcustomplot/…`, `tests/auto/test-intervals/…` (lines 1146, 1270), `tests/auto/test-ownership/…`
  (line 835), `tests/auto/test-reference-m4e/…`

**Interfaces:**
- Produces: `signal void QCustomPlot::keyPress(QKeyEvent* event)`, `signal void QCustomPlot::keyRelease(QKeyEvent* event)`
  (P-4); default focus policy `Qt::StrongFocus`. `QCPIntervals` handles keys from its plot's signal.

- [ ] **Step 1: Failing tests** in `TestQCustomPlot`: `keysReachASlot` (press and release of Left, auto-repeat flag
  passed through), `theFocusPolicyIsStrong`, `anAcceptedKeyStopsTheDeleteRequest`, `aSlotDeletingThePlotOnKeyPress`
  (Review Focus 1). In `TestIntervals`: the two direct `iv->keyPress(&e)` calls become `QTest::keyClick` on the plot
  with the same expectations (Escape cancels a drag; nudges move the selection).
- [ ] **Step 2: Run, expect compile failure.**
- [ ] **Step 3: Implement**; add `brick-breaker` and `asteroids` to the user-code build.
- [ ] **Step 4: Reference pictures** `brick-breaker-target` (800 x 520, the initial wall, paddle and ball) and
  `asteroids-target` (800 x 600, after 30 fixed steps with Up and Left held, one bullet fired; and a second picture
  with `over = true` showing the banner over the sprites, text on top). Draw locally, look at each.
- [ ] **Step 5: Docs.** `docs/migrating.md`: each break of this milestone (anchors, item mouse handlers, plottable
  key virtual, focus, `dataExtent`, icons). `m6-gaps.md`: mark 6d done with the date. `examples/user-code/README.md`:
  which targets build.
- [ ] **Step 6: Whole suite locally (Xvfb) and on Coder (`build`, `build-asan`, `build-fco`), clang check; commit**
  `feat(api)!: keys are plot signals; the brick-breaker and Asteroids targets build`.
