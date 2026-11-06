# NeoQCP 3.0 milestone 4a — signals and selection — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Every signal a plot-owned object emits is delivered when the outermost NeoQCP call returns,
no signal is overloaded, and every selectable object reports selection the same way.

**Architecture:** One template helper routes an emission through the sender's plot queue
(`QCustomPlot::deliver`, milestone 1); public mutators that emit open a `QCPEmissionScope`, so a call
chain delivers once, at its end. A meson test scans the headers for overloaded signals and the
sources for direct emits outside an allow-list, so the rule cannot rot. Selection becomes
`selected()`, `selectable()` (`QCP::SelectionType`), `selectionChanged()` and `selectableChanged()`
on every selectable class; the typed detail stays in the existing getters.

**Tech Stack:** C++20, Qt 6.12 (QRhiWidget), Meson, Qt Test (one `auto-tests` binary), Python 3 for
the scan test.

**Spec:** `docs/specs/2026-11-06-neoqcp-3-milestone-4-design.md` (M4a, calls 2, 9, 16, rulings
below), with `docs/specs/2026-11-02-neoqcp-3-object-model-design.md` (decisions 5, 15, answer 1).
Inputs: `docs/plans/neoqcp-3/m4a-signal-inventory.md` (every emit site, 120 statements),
`docs/plans/neoqcp-3/m4-inventory.md` §5 (selection state).

## Rulings taken while planning (they amend the M4 design's calls 2 and 16)

- R-A: the 2.x selection signals with arguments are **removed**, not emitted alongside. A
  `selectionChanged()` next to `selectionChanged(bool)` makes every `&Class::selectionChanged`
  ambiguous, which breaks 2.x code anyway and violates decision 5. The break is a compile error,
  listed in the changelog. Cost if wrong: 2.x selection slots need a rewrite now instead of in M5.
- R-B: the getter keeps its 2.x name `selected()` (every class already has it except the axes and the
  legend, which gain it); no `isSelected()` is added. Cost if wrong: a rename later.
- R-C: `rangeChanged` keeps one signature, `(const QCPRange& newRange, const QCPRange& oldRange)`. A
  2.x slot taking only the new range still connects (Qt allows fewer slot arguments). Only
  `QOverload<const QCPRange&>::of(...)` call sites break.

## Global Constraints

- Worktree `/home/jeandet/Documents/prog/NeoQCP-3.0`, branch `neoqcp-3.0`; nothing pushed.
- Build: `ninja -C build-qt612 -j 4`; one build or test at a time, in the foreground.
- Offscreen suite: `QT_QPA_PLATFORM=offscreen QT_LOGGING_RULES="*.debug=true" meson test -C build-qt612 --num-processes 4`;
  sum `^Totals` lines of `build-qt612/meson-logs/testlog.txt`. ASan: same in `build-asan` with
  `LSAN_OPTIONS=suppressions=$PWD/tests/lsan.supp`. GPU: the whole `auto-tests` binary under
  `xvfb-run -a -s "-screen 0 1600x1200x24"` with `QT_QPA_PLATFORM=xcb`.
- Every behaviour change is test-first: the test fails before the change, in the task report.
- Never bend library behaviour to please a test.
- Signals are not overloaded; connecting needs no `QOverload` (decision 5).
- A slot may delete the emitter, its plot or anything else (decision 5).
- Bench gate: rows and method as `docs/plans/neoqcp-3/baseline.md`, baseline = end of milestone 3;
  `QCPAxis_*`, `QCPInput_*`, `QCPItems_*`, `QCPGpu_*`, `QCPDrawn_*` 5 %, the rest 15 %; a row over its
  gate is re-run six times, the median decides.
- Commits: explicit pathspecs, message ends with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Never add `subprojects/.wraplock` or `subprojects/jkqtmathtext`.

## Review Focus

1. A slot that deletes the plot while the queue still holds other emissions: the rest are dropped,
   nothing reads the dead plot (Task 2 test `slotDeletingThePlotDropsTheRestOfTheChain`).
2. An object taken out of its plot (`take()`, no parent plot) still emits, directly (Task 1 test
   `noPlotEmitsAtOnce`).
3. A slot that calls the same mutator again (a `rangeChanged` slot calling `setRange`): both emissions
   are delivered in order, no recursion loop (Task 2 test `slotReenteringTheSetterDeliversInOrder`).
4. A call chain (`setScaleType` → `setRange`): the slot sees the finished outer call
   (Task 2 test `rangeSlotSeesTheNewScaleType`).
5. Plot teardown emits nothing new: the scopes the new mutators open are inert on a plot being
   destroyed (Task 2 test `teardownOpensNoDelivery`).

---

### Task 1: The delivery helper and the scan test

**Files:**
- Modify: `src/core.h` (after `class QCustomPlot`), `src/core.cpp`
- Create: `tools/check-signals.py`
- Modify: `tests/meson.build` (register the scan as a meson test `check-signals`)
- Test: `tests/auto/test-signal-delivery/test-signal-delivery.{h,cpp}`

**Interfaces:**
- Produces:
  `template <typename Sender, typename... SignalArgs, typename... Args> void qcpEmit(Sender* sender, QCustomPlot* plot, void (Sender::*signal)(SignalArgs...), Args&&... args);`
  — copies the arguments, then `plot->deliver(sender, …)` when `plot` is non-null, else calls the
  signal at once. Defined in `core.h` (needs the complete `QCustomPlot`).
- Produces: `tools/check-signals.py <src dir>`: exit 1 and one line per finding when (a) a class in a
  header declares two signals with one name, or (b) an `emit`/`Q_EMIT` statement in a `.cpp` is not
  inside a `qcpEmit`/`deliver` call and its file:function is not in the allow-list at the top of the
  script. The allow-list is data, each entry with its reason (call 9): `QCPTheme` (no plot, 16),
  `QCPLaneLayout` (no plot, 1), `QCPAsyncPipelineBase` (other thread, 5), plus the 17 sites already
  inside `deliver` lambdas need no entry.

- [ ] **Step 1: Write the failing tests** in `TestSignalDelivery`:
  - `qcpEmitHoldsUntilTheScopeCloses`: a `QCPAxis` signal emitted through `qcpEmit` inside an open
    `QCPEmissionScope` on its plot is not received before the scope closes and is received once after.
  - `noPlotEmitsAtOnce`: `qcpEmit(sender, nullptr, …)` is received before it returns.
  - `deadSenderIsNotDelivered`: the sender deleted inside the scope: nothing received.
- [ ] **Step 2:** Build. Expected: does not compile (`qcpEmit` undeclared). Record the error.
- [ ] **Step 3:** Implement `qcpEmit`.
- [ ] **Step 4:** Build, run `auto-tests` offscreen; the three tests pass, `TestSignalDelivery` all green.
- [ ] **Step 5:** Write `tools/check-signals.py`, register it in meson. Run it: expected FAIL listing
  the 5 overloaded signal names (`QCPAxis::rangeChanged`, `QCPAbstractPlottable::selectionChanged`,
  `QCPPolarAxisAngular::rangeChanged`, `QCPPolarGraph::selectionChanged`,
  `QCPPolarAxisRadial::rangeChanged`) and the ~103 direct emits. Mark the meson test
  `should_fail: true` with a comment "until Task 4"; Task 4 removes the flag.
- [ ] **Step 6:** Offscreen suite green. Commit `tests/auto/test-signal-delivery/*`, `src/core.h`,
  `src/core.cpp`, `tools/check-signals.py`, `tests/meson.build`:
  "feat(signals): qcpEmit delivers through the sender's plot; a scan guards the signal rules".

### Task 2: Axes, axis rects and layout elements deliver their signals

**Files:**
- Modify: `src/axis/axis.{h,cpp}`, `src/layoutelements/*.cpp`, `src/layout.cpp`, `src/layer.cpp`,
  `src/core.cpp` (the sites of these classes in `m4a-signal-inventory.md`)
- Test: `tests/auto/test-signal-delivery/test-signal-delivery.{h,cpp}`

**Interfaces:**
- Consumes: `qcpEmit` (Task 1).
- Produces: `QCPAxis::rangeChanged(const QCPRange& newRange, const QCPRange& oldRange)` as the only
  `rangeChanged` (R-C), the same on `QCPPolarAxisAngular` / `QCPPolarAxisRadial`.

- [ ] **Step 1: Write the failing tests:**
  - `rangeSlotSeesTheNewScaleType`: range `-5..10`, a `rangeChanged` slot records `axis->scaleType()`
    and `axis->range()`; `axis->setScaleType(QCPAxis::stLogarithmic)` sanitises the range. Assert both
    signals arrive after the call returns, in emission order (`rangeChanged`, then
    `scaleTypeChanged`), and the slot saw `stLogarithmic` and the sanitised range.
  - `slotReenteringTheSetterDeliversInOrder`: a `rangeChanged` slot calls `setRange` once more (guarded
    by a counter); the received ranges are `[r1, r2]`, the slot ran twice, no recursion.
  - `slotDeletingThePlotDropsTheRestOfTheChain`: `setScaleType` with a `rangeChanged` slot that deletes
    the plot: `scaleTypeChanged` is not received, no ASan report.
  - `teardownOpensNoDelivery`: deleting a plot whose axes have `rangeChanged`/`selectionChanged` slots
    connected receives none.
  - `oneArgumentRangeSlotConnects`: `connect(axis, &QCPAxis::rangeChanged, this, [](const QCPRange&){})`
    compiles without `QOverload` and is called.
- [ ] **Step 2:** Build/run. Expected: `oneArgumentRangeSlotConnects` fails to compile (ambiguous
  overload) — record it; then comment it out temporarily, run the others: the order/plot-deleted tests
  FAIL (record output).
- [ ] **Step 3:** Remove the one-argument `rangeChanged` overload on the three axis classes; route every
  emit of the classes in this task through `qcpEmit`; each public mutator that emits opens
  `const QCPEmissionScope scope(mParentPlot);` first. Port every `QOverload<const QCPRange&>` and
  `SIGNAL(rangeChanged(QCPRange))` use in `src/`, `tests/`, `examples/`, `documentation/`.
- [ ] **Step 4:** Restore `oneArgumentRangeSlotConnects`; build; offscreen suite green.
- [ ] **Step 5:** Bench `QCPAxis_SetRange10k`, `QCPAxis_SetTickLength10kItems`, `QCPInput_MouseMoveStorm`,
  `QCPGpu_Pan100k` against the end of milestone 3 (gate above). If `QCPAxis_SetRange10k` is over, the
  scope moves to the entry points that chain (`setScaleType`, `rescale`, the drag/zoom handlers) and
  the plain range setter emits through `qcpEmit` without its own scope; report which.
- [ ] **Step 6:** Commit the touched files: "feat(signals): axes and layout elements deliver at the
  outermost call; rangeChanged has one signature".

### Task 3: Plottables, items and the rest deliver their signals

**Files:**
- Modify: every remaining plot-reachable site of `m4a-signal-inventory.md`: `src/plottables/*`,
  `src/items/*`, `src/selectionrect.cpp`, `src/overlay.cpp`, `src/item-creation-state.cpp`,
  `src/polar/*`, `src/core.cpp` (`selectionChangedByUser`, group `membersChanged`)
- Test: `tests/auto/test-signal-delivery/test-signal-delivery.{h,cpp}`

**Interfaces:**
- Consumes: `qcpEmit` (Task 1); the scope rule of Task 2 (as finally reported in its Step 5).

- [ ] **Step 1: Write the failing tests** (one per emitter family, a slot that deletes the emitter's
  plot inside a call that emits twice, asserting the second emission is dropped and ASan is clean):
  `colorMapGradientSlotDeletingThePlot` (`QCPColorMap::setGradient` with a colour scale attached:
  `gradientChanged` on both), `spanDragSlotDeletingThePlot` (`QCPItemRSpan` move: its 5 emits),
  `selectionRectSlotDeletingThePlot` (`accepted` then the selection signals),
  `groupMembersSlotDeletingThePlot` (`QCPPlottableGroup::membersChanged` during `add`).
- [ ] **Step 2:** Run. Expected: FAIL or ASan report on each (record).
- [ ] **Step 3:** Route the sites through `qcpEmit`, scopes as Task 2's rule. `QCPTheme`,
  `QCPLaneLayout` and `QCPAsyncPipelineBase` stay direct; each such emit gets a one-line comment naming
  the reason (call 9).
- [ ] **Step 4:** Offscreen suite and Xvfb binary green. `tools/check-signals.py` reports only the
  overloaded selection signals (fixed in Task 4).
- [ ] **Step 5:** Bench `QCPItems_Pan10k`, `QCPItems_Click10k`, `QCPBars_Pan10k`, `QCPGpu_BarsPan10k`,
  `QCPItems_AddRemove10k` (gate above).
- [ ] **Step 6:** Commit: "feat(signals): plottables, items and polar deliver at the outermost call".

### Task 4: One selection model

**Files:**
- Modify: `src/plottables/plottable.{h,cpp}`, `src/items/item.{h,cpp}`, `src/axis/axis.{h,cpp}`,
  `src/layoutelements/layoutelement-legend.{h,cpp}`, `src/layoutelements/layoutelement-textelement.{h,cpp}`,
  `src/polar/polargraph.{h,cpp}`, `src/polar/layoutelement-angularaxis.{h,cpp}`, `src/polar/radialaxis.{h,cpp}`,
  `src/core.cpp` (selection by click and rect), every connect to the changed signals in `src/`,
  `tests/`, `examples/`, `documentation/` (35 `selectionChanged` and 4 `selectableChanged` refs)
- Test: create `tests/auto/test-selection-model/test-selection-model.{h,cpp}`, register in
  `tests/auto/meson.build` and `autotest.cpp`

**Interfaces:**
- Produces, on `QCPAbstractPlottable`, `QCPAbstractItem`, `QCPAxis`, `QCPLegend`,
  `QCPAbstractLegendItem`, `QCPTextElement`, `QCPPolarGraph`, `QCPPolarAxisAngular`, `QCPPolarAxisRadial`:
  - `bool selected() const` (axis, legend, polar axes: true when any part is selected);
  - `QCP::SelectionType selectable() const` (item, legend item, text element: `stNone` or `stWhole`;
    axis, legend, polar axes: `stWhole` when any part is selectable, else `stNone`);
  - signals `void selectionChanged();` and `void selectableChanged();` — no other signal of those names.
- Produces: `setSelectable(QCP::SelectionType)` on items, legend items and text elements, replacing
  `setSelectable(bool)`; a type other than `stNone`/`stWhole` is refused through the misuse tier
  (`src/misuse.h`). The parts setters (`setSelectableParts`, `setSelectedParts`) and the detail getters
  (`selection()`, `selectedParts()`, `selectableParts()`) are unchanged.

- [ ] **Step 1: Write the failing tests** in `TestSelectionModel`, one data-driven test over the nine
  classes (a factory per row):
  - `everySelectableHasOneShape`: `selected()` false, then the class's own select call makes it true;
    exactly one `selectionChanged()` received (`QSignalSpy` on `SIGNAL(selectionChanged())`), none on
    a repeated call with the same state.
  - `selectableChangeIsOneSignal`: one `selectableChanged()` per real change, `selectable()` returns
    the expected `QCP::SelectionType`.
  - `itemRefusesAPartialSelectionType`: `item->setSelectable(QCP::stDataRange)` leaves `stWhole`
    and records one misuse (recording handler from `test-misuse`).
  - `userSelectionStillAnnounced`: a click that selects a graph emits the graph's `selectionChanged()`
    then the plot's `selectionChangedByUser()`, in that order.
- [ ] **Step 2:** Build. Expected: does not compile (signal without arguments missing, `selected()`
  missing on `QCPAxis`). Record.
- [ ] **Step 3:** Implement: remove the argument forms (R-A), add the members, emit through
  `qcpEmit`. Port every connect listed in Files; a slot that needs the detail reads the getter.
- [ ] **Step 4:** Offscreen suite, ASan, Xvfb binary green; `tools/check-signals.py` passes. Remove
  `should_fail` from its meson registration.
- [ ] **Step 5:** Changelog (milestone 4 section, "Behaviour changes for 2.x users"): the selection
  signals lost their arguments (read `selected()` / `selection()` / `selectedParts()` in the slot);
  `selectable()` is a `QCP::SelectionType` and `setSelectable(bool)` is now
  `setSelectable(QCP::SelectionType)`; `rangeChanged` has one signature; signals are delivered when the
  outermost NeoQCP call returns (list the three direct exceptions). Docs: grep `documentation/` for the
  changed signatures and fix the snippets (the doc-snippets meson test compiles them).
- [ ] **Step 6:** Commit: "feat(selection): one selection shape and one signal per object".

### Task 5: Finish

- [ ] Offscreen suite, ASan suite, whole Xvfb binary (sum of `Totals`), clang `-fsyntax-only` on every
  touched `.cpp` (flags from `build-qt612/compile_commands.json`), `tools/check-signals.py` green.
- [ ] Full bench table of `baseline.md` against the end of milestone 3; append "End of milestone 4a" to
  `baseline.md`.
- [ ] Rulings and deferred minors to `docs/plans/neoqcp-3/milestone-4a-rulings.md` (R-A, R-B, R-C and
  any made during execution). Commit.
