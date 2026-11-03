# NeoQCP 3.0 milestone 2: creation, handles, automatic repaint — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Objects are created with `plot.add<T>(…)` and a fluent builder that is itself a handle; handles carry the common operations as no-op-when-dead members; ownership moves through `take`/`add(unique_ptr)`; every change repaints automatically, once per event-loop turn.

**Architecture:** Repaint: every layer dirtying schedules the existing queued, deduplicated replot (`replot(rpQueuedReplot)`), and every appearance setter dirties its layer. Creation: `QCustomPlot::add<T>(args…)` builds the object at once with its defaults and returns `typename T::Builder`, which derives from `QCPHandle<T>` and adds fluent setters (CRTP so a subclass's builder extends its base's). The old `new`/`addGraph` API stays until milestone 5.

**Tech Stack:** C++20, Qt 6 (QRhiWidget), Meson (+ CMake list, WebAssembly gallery), Qt Test.

**Spec:** `docs/specs/2026-11-02-neoqcp-3-object-model-design.md` (decisions 1, 2, 3, 4, 7; "Calls made while writing this up"). Milestone 1 rulings: `docs/plans/neoqcp-3/milestone-1-rulings.md`. Code inventory used for this plan: `docs/plans/neoqcp-3/milestone-2-inventory.md` (copied in Task 0).

## Global Constraints

- C++20, Qt 6; the Meson build, `cmake/sources.cmake` and the WebAssembly gallery keep compiling (clang syntax check on changed files).
- No exceptions. GUI thread only. Misuse: `QCP_CHECK` / `QCP_CHECK_OR_RETURN` / `qcp::reportMisuse` (`src/misuse.h`).
- Additive: the 2.x API (`addGraph`, `new QCPItem…(plot)`, raw getters, public `xAxis`… members, `replot()`) keeps compiling and passing its tests. Getters keep returning raw pointers in this milestone (switching them to handles is part of milestone 5's removal of the old API). One sanctioned behaviour change: setters schedule a repaint.
- `replot()` stays public and synchronous (export, tests). `replot(rpQueuedReplot)` stays the one deduplicated path.
- Build and test one at a time, foreground, `-j 4`. Full suite in `build-qt612` and `build-asan` (`LSAN_OPTIONS=suppressions=tests/lsan.supp`); GPU tests under `xvfb-run -a -s "-screen 0 1600x1200x24" env QT_QPA_PLATFORM=xcb`.
- Every bug fix starts with a failing test. No benchmark regression against `docs/plans/neoqcp-3/baseline.md` ("End of milestone 1" medians).
- Commit after each task with explicit paths, ending `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Do not push.
- Delegation: mechanical sweeps (setter lists, snippet ports) go to opencode with an exact pattern; a Claude reviewer checks them.

## Review Focus

1. A builder kept with `auto` after its object was deleted: setters are no-ops, `if (b)` is false (pinned in Task 3).
2. `add<MyGraph>()` for a user subclass with its own builder setter, chained after base setters (pinned in Task 3).
3. 1000 setter calls in one turn produce exactly one replot; a hidden plot still skips (pinned in Task 1).
4. A setter called from a `beforeReplot`/`afterReplot` slot does not start an endless repaint loop (pinned in Task 1).
5. `take()` then `add(std::move(ptr))` onto another plot: the object is drawn there, the old plot has no entry left (pinned in Task 7).

---

### Task 0: Inventory into the repo

**Files:** Create `docs/plans/neoqcp-3/milestone-2-inventory.md` (copy of the opencode inventory).
- [ ] **Step 1:** Copy, commit — "docs(neoqcp-3): milestone 2 code inventory".

### Task 1: Dirty means a repaint is scheduled

**Files:** Modify `src/layer.{h,cpp}` (`QCPLayer::markDirty`, `invalidatePaintBuffer`; new `QCPLayerable::markDirty()`), `src/core.{h,cpp}` (`requestRepaint()`); Test: `tests/auto/test-repaint/test-repaint.{h,cpp}` (class `TestRepaint`, registered as usual).

**Interfaces:**
- Produces: `void QCustomPlot::requestRepaint();` — `replot(rpQueuedReplot)`; no-op while `isBeingDestroyed()` or while a replot is running (the layer is redrawn by that replot). `void QCPLayerable::markDirty();` — marks its layer dirty (or invalidates when it has no buffered layer) and calls `requestRepaint()`. `QCPLayer::markDirty()` and `invalidatePaintBuffer()` call `requestRepaint()`.

- [ ] **Step 1: Failing tests** in `TestRepaint`, counting `afterReplot`:
  - `markDirtySchedulesOneRepaint`: `graph->markDirty()` ×1000, then `QTRY_COMPARE(count, 1)` and one more `processEvents()` keeps 1.
  - `setterInReplotSlotDoesNotLoop`: an `afterReplot` slot calls `graph->markDirty()`; after 5 `processEvents()` the count is ≤ 2.
  - `hiddenPlotSkips`: hidden plot with `setSkipReplotsWhenHidden(true)`: `markDirty()` → 0 replots.
  - `syncReplotStillWorks`: `replot()` emits `afterReplot` before it returns.
- [ ] **Step 2:** Build, run. Expected: the first fails (0 replots).
- [ ] **Step 3:** Implement. A setter that marks dirty during a replot must still be drawn: if the layer was already drawn in this replot, schedule the next one (one more at most).
- [ ] **Step 4:** `TestRepaint`, full suite, benchmark. Expected: green, no regression.
- [ ] **Step 5: Commit** — "feat(core): marking a layer dirty schedules one repaint".

### Task 2: Every appearance setter marks dirty

**Files:** Modify the setters listed by `docs/plans/neoqcp-3/milestone-2-inventory.md` §7 and every other public setter that changes what is drawn, in `src/axis/`, `src/plottables/`, `src/items/`, `src/layoutelements/`, `src/polar/`, `src/layer.cpp` (`setVisible`), `src/painting/` (theme); replace setter-internal `replot()` calls (§5: span `setRange`, `setExpanded`, colormap2/histogram/intervals request paths) with `markDirty()`/`requestRepaint()`. Test: `tests/auto/test-repaint`.

**Interfaces:**
- Consumes: `QCPLayerable::markDirty()`, `QCustomPlot::requestRepaint()` (Task 1).

- [ ] **Step 1: Failing test** `everyAppearanceSetterSchedulesRepaint`: data-driven, one row per class family with 3–5 representative setters each (axis `setLabel`/`setTickLabelColor`/`setBasePen`, grid `setPen`, graph `setPen`/`setLineStyle`/`setScatterStyle`, bars `setWidth`, colormap `setGradient`, text item `setText`, tracer `setGraphKey`, legend `setFont`, colour scale `setLabel`, polar axis `setLabel`, layerable `setVisible`): after the call, `QTRY_COMPARE(count, 1)`.
- [ ] **Step 2:** Run. Expected: most rows fail.
- [ ] **Step 3 (opencode sweep):** opencode adds `markDirty()` (or the owning element's equivalent) at the end of every public appearance setter, from a list it writes first to `docs/plans/neoqcp-3/milestone-2-setters.md` (class, setter, file:line, added/already-marks/not-appearance). Setters that only store a value used at the next draw get the call; data setters (`setData`, `addData`) and range setters already dirty through their paths — verify, do not double.
- [ ] **Step 4:** `TestRepaint`, full suite, benchmark (`QCPAxis_SetRange10k`, `QCPItems_AddRemove10k` especially). Expected: green, no regression.
- [ ] **Step 5: Commit** — "feat: appearance setters schedule a repaint".

### Task 3: `add<T>` and the builder for graphs

**Files:** Create `src/builder.h`; Modify `src/core.h` (`add<T>`), `src/plottables/plottable.h` (`QCPAbstractPlottable::Builder` CRTP base), `src/plottables/plottable-graph.h` (`QCPGraph::Builder`), `src/qcp.h`, `src/meson.build`, `cmake/sources.cmake`; Test: `tests/auto/test-builder/test-builder.{h,cpp}` (class `TestBuilder`).

**Interfaces:**
- Produces:
  - `template <typename T, typename... Args> typename T::Builder QCustomPlot::add(Args&&... args);` — the contract, written in `QCPAbstractPlottable`'s doc: `T` has a nested `Builder`, keeps the 2.x constructor `T(QCPAxis* key, QCPAxis* value)`, and `add` forwards `args…` to `Builder::data(args…)` (a builder with no `data` overload for the given arguments fails to compile with a `static_assert` naming `T`). Constructors are not changed, so `new T(...)` keeps working (additive).
  - `template <typename T, typename Self> class QCPPlottableBuilder : public QCPHandle<T>` with `Self& name(const QString&)`, `pen(const QPen&)`, `brush(const QBrush&)`, `axes(QCPAxis* key, QCPAxis* value)`, `layer(const QString&)`, `visible(bool)`; every setter is a no-op on a dead handle and checks its value (`QCP_CHECK_OR_RETURN` for a null/foreign axis, an unknown layer).
  - `QCPGraph::Builder` adds `lineStyle`, `scatterStyle`; `plot.add<QCPGraph>(keys, values)` sets the data.
  - Defaults: the plot's default axis rect's bottom/left axes, `currentLayer()`.

- [ ] **Step 1: Failing tests** in `TestBuilder`:
  - `addGraphWithData`: `auto g = plot.add<QCPGraph>(QVector<double>{1,2,3}, QVector<double>{4,5,6}).name("Bx").pen(QPen(Qt::red));` → `g` true, `g->name() == "Bx"`, `g->dataCount() == 3`, `plot.graphCount() == 1`, `g->keyAxis() == plot.xAxis`.
  - `builderConvertsToHandle`: `QCPHandle<QCPGraph> h = plot.add<QCPGraph>();` compiles and is true; `static_assert(std::is_base_of_v<QCPHandle<QCPGraph>, QCPGraph::Builder>)`.
  - `deadBuilderSettersAreNoOps`: `auto g = plot.add<QCPGraph>(); g.remove(); g.name("x").pen(QPen());` no crash, no misuse recorded, `!g`.
  - `builderRejectsForeignAxis`: `.axes(otherPlot.xAxis, otherPlot.yAxis)` records one misuse and keeps the default axes.
  - `addUserSubclass`: a test-local `MyGraph : QCPGraph` with `MyGraph::Builder : QCPGraphBuilderBase<MyGraph, MyGraph::Builder>` adding `.tag(int)`; `plot.add<MyGraph>().name("m").tag(7)` compiles (base setter then subclass setter) and sets both.
- [ ] **Step 2:** Build. Expected: does not compile (`add` missing).
- [ ] **Step 3:** Implement. One chain = one repaint (Task 1 dedup).
- [ ] **Step 4:** `TestBuilder`, `TestHandle`, full suite; clang syntax check of `test-builder.cpp`.
- [ ] **Step 5: Commit** — "feat(core): plot.add<T>() with fluent builders, starting with QCPGraph".

### Task 4: Builders for the other plottables

**Files:** Modify every concrete plottable header in `src/plottables/` (curve, bars, financial, statistical box, error bars, colormap, colormap2, multigraph, histogram, histogram2d, intervals, waterfall if present) — each gets `Builder` with its main appearance setters and `add<T>` data arguments matching its `setData`; Test: `TestBuilder`.
- [ ] **Step 1: Failing test** `addEveryPlottable`: data-driven, one row per class: `add<T>(data…)` with one class-specific setter; assert the type, data count, and one property.
- [ ] **Step 2:** Run. Expected: fails to compile per class.
- [ ] **Step 3 (opencode transcription):** opencode writes each `Builder` following `QCPGraph::Builder` exactly; Claude reviewer checks.
- [ ] **Step 4:** `TestBuilder`, full suite, clang check.
- [ ] **Step 5: Commit** — "feat: builders for every plottable".

### Task 5: Builders for items, axes, axis rects and layout parts; plot-owned factories

**Files:** Modify `src/items/*.h` (each item's `Builder`: positions as setters, e.g. `.from(QPointF).to(QPointF)` in the item's default position type), `src/layoutelements/layoutelement-axisrect.h` (`add<QCPAxis>(QCPAxis::AxisType)` on the axis rect), `src/core.h` (`add<QCPAxisRect>(row, col)`, `add<QCPTextElement>`, `add<QCPColorScale>`, `add<QCPBarsGroup>()`, `add<QCPPlottableGroup>(name)`), `src/plottables/plottable-intervals.h` (lane layout made by the intervals/plot, not the user); Test: `TestBuilder`.
- [ ] **Step 1: Failing tests:** `addItems` (data-driven over every item class: type, one property, `itemCount()`); `addAxisToRect` (`rect.add<QCPAxis>(QCPAxis::atRight)` → `rect->axisCount(atRight) == 2`); `addAxisRectToLayout`; `barsGroupOwnedByPlot` (deleting the plot frees the group — ASan; no `new QCPBarsGroup` in the test).
- [ ] **Step 2:** Run. Expected: compile failures.
- [ ] **Step 3:** Implement (opencode transcribes item builders after the first one is written by the implementer).
- [ ] **Step 4:** `TestBuilder`, `TestOwnership`, full suite, ASan.
- [ ] **Step 5: Commit** — "feat: builders for items, axes and layout parts; plot-owned groups".

### Task 6: Polar builders

**Files:** `src/polar/*.h`; Test: `TestBuilder`.
- [ ] **Step 1: Failing test** `addPolar`: `plot.add<QCPPolarAxisAngular>(row, col)` then `angular.add<QCPPolarGraph>(keys, values).name("p")`; the graph draws (pixel check) and has a legend row.
- [ ] **Step 2–5:** as Task 4. Commit — "feat(polar): builders".

### Task 7: `take` and `add(std::unique_ptr<T>)`

**Files:** `src/core.{h,cpp}`, `src/plottables/plottable.cpp`, `src/items/item.cpp`; Test: `TestBuilder`.

**Interfaces:**
- Produces: `template <typename T> std::unique_ptr<T> QCustomPlot::take(QCPHandle<T> h);` — unlists, unparents, clears axes/layer; dead handle → empty pointer + one misuse. `template <typename T> typename T::Builder QCustomPlot::add(std::unique_ptr<T> object);` — reparents, registers on the default axes/layer unless the builder says otherwise.

- [ ] **Step 1: Failing tests:** `takeAndReAdd` (take a graph from plot A, add it to plot B with `.axes(...)`; A has 0 graphs, B draws it, A's legend has no row); `takeDeadHandle` (one misuse, null); `addUserBuilt` (`plot.add(std::make_unique<MyGraph>(…))`); ASan clean, including deleting the taken pointer without re-adding.
- [ ] **Step 2–4:** as above.
- [ ] **Step 5: Commit** — "feat(core): take(handle) and add(std::unique_ptr)".

### Task 8: Common operations on the handle

**Files:** `src/handle.h`; Test: `TestHandle`.

**Interfaces:**
- Produces, as `QCPHandle<T>` members constrained with `requires` on `T`'s family, each a no-op (default value for getters) when dead: `setVisible(bool)`/`visible()` (layerables), `setName(const QString&)`/`name()` (plottables), `setLayer(const QString&)` (layerables), `remove()` (routes axes through `QCPAxisRect::removeAxis`, layers through `removeLayer`, everything else `delete` — replaces the milestone-1 `simplify:` note).
- [ ] **Step 1: Failing tests:** `handleCommonOpsLive`, `handleCommonOpsDeadAreNoOps` (no misuse, defaults returned), `removeAxisThroughHandle` (`QCPHandle<QCPAxis>(rect->axis(atRight)).remove()` → `axisCount` drops, replot OK), `removeLayerThroughHandle`, `commonOpsOnlyWhereTheyApply` (`static_assert` via a requires-expression that `QCPHandle<QCPLayer>` has no `setName`).
- [ ] **Step 2–4:** as above.
- [ ] **Step 5: Commit** — "feat(core): common operations on handles".

### Task 9: Items scale

**Files:** `src/core.{h,cpp}` (item registry), `src/layer.cpp` (draw culling), `tests/benchmark`; Test: `TestOwnership`, benchmark.
- [ ] **Step 1:** Benchmark rows `QCPItems_AddRemove10k` (baseline 80 ms) and `QCPItems_Replot10k` with items outside the view (new row `QCPItems_Replot10kOffscreen`). Failing test `itemRemovalIsConstantTime` is not testable directly: instead assert behaviour (`removeItem` order-independent, `itemCount`, item(i) still valid after removals) so the registry change is covered.
- [ ] **Step 2:** Replace the item list's linear removal with an index map (stable `item(i)` order kept by swap-and-pop or by a tombstone compaction — choose, write why); skip drawing and hit-testing items whose bounding rect is outside the axis rect (items without a cheap bounds always draw).
- [ ] **Step 3:** Full suite; benchmark. Expected: `AddRemove10k` well below 80 ms, `Replot10kOffscreen` well below `Replot10k`.
- [ ] **Step 4: Commit** — "perf(core): constant-time item removal and view culling".

### Task 10: Port the guide and examples

**Files:** `docs/snippets/*.cpp`, `docs/*.md`, `examples/**`; Test: the `doc-snippets` meson test, `build/docs/snippets/doc-shots docs/images` (images unchanged).
- [ ] **Step 1 (opencode):** replace `addGraph`/`new QCPItem…` with `add<T>` builders and delete `replot()` calls that only existed to show a change (keep the ones before `savePng`/`toPixmap` and in programs that need a synchronous frame); list every change in `docs/plans/neoqcp-3/milestone-2-port.md`.
- [ ] **Step 2:** `doc-snippets` passes; regenerate the guide images and confirm with `git diff --stat docs/images` that nothing changed visually (byte changes from encoders are OK; check 3 images by eye).
- [ ] **Step 3: Commit** — "docs: the guide and examples use add<T>() and automatic repaint".

### Task 11: Close the milestone

- [ ] **Step 1:** Full suite in both builds (+ xvfb ASan), benchmark ×3 appended to `baseline.md` ("End of milestone 2").
- [ ] **Step 2:** opencode honest, contradicting review of `git diff 393d6bc..HEAD -- src tests`; verify each point; fix the real ones test-first.
- [ ] **Step 3:** Changelog under "3.0 (in progress)": `add<T>` and builders (the subclass contract), handle common operations, `take`/`add(unique_ptr)`, automatic repaint (setters schedule one repaint per turn; `replot()` is for export and tests), item scaling.
- [ ] **Step 4: Commit** — "docs: milestone 2 of the 3.0 object model". Report to the maintainer; no push.
