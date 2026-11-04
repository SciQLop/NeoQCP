# NeoQCP 3.0 item scaling — implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** thousands of items add, remove and draw without any O(n²) step in NeoQCP's own code.

**Architecture:** measure first (new benchmark rows plus a profile), then remove the redundant list scans
in the item registry and the layer child lists, then cull drawing and hit tests only if the numbers say
items outside the view still cost.

**Tech Stack:** C++20, Qt 6.12, Meson, QTest benchmarks.

**Spec:** `docs/specs/2026-11-02-neoqcp-3-object-model-design.md`, decision 2 ("Items stay `QObject`s and
must scale": constant-time add and remove; drawing and hit testing skip items outside the view) and
"Performance is part of done" (items added and removed per second, frame time with 10 000 items).
Previous milestone: `docs/plans/neoqcp-3/milestone-2-rulings.md`.

## Why the numbers move today

Measured in Task 1 (`docs/plans/neoqcp-3/item-scaling-profile.md`): about half of adding and deleting
10 000 items is NeoQCP's own linear list scans, each run once per item.
1. `QCustomPlot::registerItem` checks `mItems.contains(item)` on every new item.
2. `QCPLayer::addChild` checks `mChildren.contains(layerable)` on every add.
3. `itemDestroyed` and `removeItem` search `mItems` (`removeItem` twice: `contains`, then `removeOne`).
4. `QCPLayer::removeChild` (`removeOne`) searches the layer list; `deleteAllItems` deletes newest first,
   so that search and Qt's own `children.indexOf` walk the whole list each time. (Qt 6 erases a first
   element by moving the begin pointer: deleting oldest first makes both O(1).)

Task 1 also found that a range change never redrew a layer holding only items (fixed in 770b1eb). With
items really drawn, a zoom over 10 000 items with 100 in view costs 4.7 ms against 0.53 ms for 100
items, and a click 1.2 ms against 0.013 ms: drawing and hit testing are linear in all items.

## Global Constraints

- Same as the object-model plan (`docs/superpowers/plans/2026-11-02-neoqcp-3-object-model.md`): C++20, Qt
  6, no exceptions, GUI thread only, the CMake and WebAssembly builds keep compiling.
- One build or test at a time, in the foreground, `-j 4`. Env:
  `PATH=/home/jeandet/Qt/6.12.0/gcc_64/bin:/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin:$PATH`,
  `PKG_CONFIG_PATH=/home/jeandet/Qt/6.12.0/gcc_64/lib/pkgconfig`, `LLVM_INSTALL_DIR=/usr`.
- One test class: `QT_QPA_PLATFORM=offscreen QT_LOGGING_RULES="*.debug=true" QCP_TEST_CLASS=<Class> build-qt612/tests/auto/auto-tests`.
  Full suite: `QT_LOGGING_RULES="*.debug=true" meson test -C build-qt612 --print-errorlogs`, and the same in
  `build-asan` with `LSAN_OPTIONS=suppressions=tests/lsan.supp`.
- Benchmark: `QT_QPA_PLATFORM=offscreen build-qt612/tests/benchmark/qcp_benchmark -median 5 <row>`.
- Item order is API: `item(index)` and `item()` (the last added) keep their 2.x meaning, and a layer's
  child order is the drawing order.
- Every bug fix starts with a failing test. No other benchmark row may regress beyond noise (±15 %).
- Commit with explicit paths, ending `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. No push.

## Review Focus

1. A slot on an item's `destroyed()` that deletes another item, or calls `clearItems()`, during
   `clearItems()`: no crash, no double delete (milestone 1 rule: all unlisted before the first delete).
2. An item moved between layers (`setLayer`, `moveToLayer`, a removed layer handing its children to the
   neighbour) keeps its drawing order after the faster list code.
3. `clearItems()` with items on several layers, some layers removed meanwhile: every layer's child list
   is empty of them afterwards, and each touched layer repaints.
4. `take(item)` then `add(std::move(ptr))` on another plot still works (it goes through `registerItem`).
5. Deleting a plot with 10k items is not quadratic either (the plot's teardown path).

---

### Task 1: Measure (benchmark rows and profile, no library change)

**Files:** Modify `tests/benchmark/benchmark.cpp`; Create `docs/plans/neoqcp-3/item-scaling-profile.md`.

- [ ] **Step 1: New rows** (each with `addLineItems(mPlot, 10000, 100)` unless said):
  - `QCPItems_Add10k`: `QBENCHMARK_ONCE` around adding 10k items only.
  - `QCPItems_Clear10k`: items added before, `QBENCHMARK_ONCE` around `clearItems()`.
  - `QCPItems_DeleteEach10k`: `QBENCHMARK_ONCE` around `delete` of every item in a fixed shuffled order
    (`std::mt19937{42}`).
  - `QCPItems_RemoveEach10k`: the same order through `mPlot->removeItem(item)`.
  - `QObject_DeleteEachChild10k` (control, the Qt floor): 10k bare `QObject`s parented to a `QObject`,
    deleted in the same shuffled order.
  - `QCPItems_DeletePlot10k`: a plot with 10k items, `QBENCHMARK_ONCE` around `delete plot` (build a
    second plot for it; `mPlot` stays for `cleanup()`).
  - `QCPItems_Zoom10k`: x range 0..1 (100 items in view), each iteration scales the range around its
    centre (a zoom, so no pan translation reuse) and `replot(rpImmediateRefresh)`.
  - `QCPItems_Zoom100`: the same with `addLineItems(mPlot, 100, 1)` (the same 100 in view, nothing
    outside): the frame time culling would reach.
  - `QCPItems_MouseMove10k` and `QCPItems_MouseMove100`: like `QCPInput_MouseMoveStorm`, with the items
    instead of the graph, `setInteractions(QCP::iSelectItems)`.
- [ ] **Step 2:** run each row 3 times (`-median 5`), and `perf record -g` once on `Add10k+Clear10k`,
  `DeleteEach10k`, `Zoom10k` and `MouseMove10k`. Write the profile doc: one table (row, run 1/2/3,
  median), then per profiled row the top five self-time frames with their share.
- [ ] **Step 3:** commit "bench: item scaling rows and profile".

The controller sets the targets of Tasks 2–4 from this doc and writes them into the ledger before
dispatching Task 2.

### Task 2: No quadratic step in add and clear

**Files:** `src/core.cpp`, `src/core.h`, `src/layer.cpp`, `src/layer.h`; Test `tests/auto/test-ownership`
(or the class holding the item registry tests).

- [ ] **Step 1: Failing or pinning tests:** `clearItemsKeepsOtherLayersOrder` (items on two layers with a
  plottable between them; after `clearItems()` each layer holds only its non-items, in the old order, and
  both layers are queued for repaint); `clearItemsSlotDeletesAnother` (Review Focus 1); `itemOrderAfterRemove`
  (`item(i)` after removing a middle item); `layerOrderAfterMoves` (Review Focus 2). They may pass today:
  they pin the behaviour the next step must keep.
- [ ] **Step 2: Implement:**
  - `registerItem`: the duplicate check becomes `item->mRegisteredPlot == this` (O(1); it always holds a
    live, new object). `hasItem`, `removeItem` and `detach` keep their list search: they take a user
    pointer that may dangle, and the search answers that safely without dereferencing it.
  - `QCPLayer::addChild`: drop the `contains` check (not a `Q_ASSERT`: asserts are live in
    `build-qt612`); only `moveToLayer` calls it, right after leaving the old layer, so a duplicate cannot
    happen. Keep `removeChild`'s "not a child" message: `~QCPLayer` must keep working.
  - `deleteAllItems`: first take all items out of their layers in one pass per layer (`removeIf` against
    a `QSet` of the items; one `invalidatePaintBuffer` per touched layer), then delete them oldest first
    so Qt's `indexOf` finds each at the front. Keep "all unlisted before the first delete".
  - The plot destructor path: apply the same if Task 1 shows `DeletePlot10k` quadratic.
- [ ] **Step 3:** add the row `QCPItems_AddRemove20k` (as `AddRemove10k` with 20 000). Run the four tests,
  TestOwnership, the full suite in both builds, and the item rows. Target: the ledger's Task 2 line.
- [ ] **Step 4:** commit "perf(items): adding and clearing items is linear".

### Task 3: Removing one item costs about what Qt costs

**Files:** `src/core.cpp`, `src/layer.cpp`.

Target: `QCPItems_DeleteEach10k` and `RemoveEach10k` within the ledger's factor of
`QObject_DeleteEachChild10k` (Qt's own `indexOf` is the floor while items stay plot children).

- [ ] **Step 1:** `removeItem` does one search, not two (`indexOf` + `removeAt`, the list search stays for
  dangling user pointers). Then, if the profile shows `mItems.removeOne` or the layer's `removeOne` above Qt's share,
  replace the linear search by the cheapest structure that keeps the order rules of the Global
  Constraints (for example `lastIndexOf`, or an index kept on the item). Decide from the profile; a
  `simplify:` comment names the ceiling that remains.
- [ ] **Step 2:** the Task 2 tests, the suite in both builds, the rows. Commit
  "perf(items): removing an item adds no scan of its own".

If the rows already meet the target after Task 2, this task records that in the ledger and changes
nothing.

### Task 4: Items outside the view cost nothing to draw or hit-test

Task 1 shows both linear (`Zoom10k` 4.7 ms vs `Zoom100` 0.53 ms; `Click10k` 1.2 ms vs `Click100`
0.013 ms). The controller writes this task's design into the ledger from the profile (what is cheap to test per item,
where the test lives, how anchors and pixel positions are handled) before dispatching it. Tests first:
an item scrolled into view is drawn (pixel check), an item outside is not hit-tested (its `selectTest`
is not called — count it in a test subclass), an item with pixel-positioned or anchored points is never
culled wrongly.

### Task 5: Close

- [ ] Benchmark section "End of item scaling" in `docs/plans/neoqcp-3/baseline.md` (all rows, 3 runs).
- [ ] Changelog lines; `docs/plans/neoqcp-3/item-scaling-rulings.md` (rulings, false findings, deferred).
- [ ] Final review (opencode), each finding verified by the controller; fixes with failing tests first.
- [ ] Commit "docs: item scaling milestone". Report; no push.
