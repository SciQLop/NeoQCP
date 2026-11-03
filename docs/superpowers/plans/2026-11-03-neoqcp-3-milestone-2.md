# NeoQCP 3.0 milestone 2: creation, handles, automatic repaint — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Objects are created with `plot.add<T>(…)`, which returns a fluent, typed handle; the handle carries the common operations as no-op-when-dead members; ownership moves through `take`/`add(unique_ptr)`; every change repaints automatically, once per event-loop turn.

**Architecture:** One type per family does both jobs (revised after the opencode plan review): `QCPGraphOps<T, Self>` derives from `QCPHandle<T>` and holds Qt-named setters (`setName`, `setPen`, …) that return the handle **by value**, so a chain is `plot.add<QCPGraph>(x, y).setName("Bx").setPen(p)` and the same handle is used later (`g.setVisible(false)`). Repaint: a layer dirtied after it was drawn schedules the existing queued, deduplicated replot; every appearance setter dirties what it touches. The old `new`/`addGraph` API and raw getters stay until milestone 5.

**Tech Stack:** C++20, Qt 6 (QRhiWidget), Meson (+ CMake list, WebAssembly gallery), Qt Test.

**Spec:** `docs/specs/2026-11-02-neoqcp-3-object-model-design.md` (decisions 1, 3, 4, 7; "Calls made while writing this up"). Milestone 1 rulings: `docs/plans/neoqcp-3/milestone-1-rulings.md`. Code inventory: `docs/plans/neoqcp-3/milestone-2-inventory.md`. Plan review: opencode, 2026-11-03 (points 1–13 applied below; item scaling moved out).

## Global Constraints

- C++20, Qt 6; Meson, `cmake/sources.cmake` and the WebAssembly gallery keep compiling (`clang++ -fsyntax-only` with flags from `build-qt612/compile_commands.json` on changed files).
- No exceptions. GUI thread only. Misuse: `QCP_CHECK` / `QCP_CHECK_OR_RETURN` / `qcp::reportMisuse` (`src/misuse.h`).
- Additive: the 2.x API (`addGraph`, `new QCPItem…(plot)`, raw getters, public `xAxis`… members, `replot()`) keeps compiling and passing. Getters keep returning raw pointers (they switch to handles with milestone 5's removal of the old API; the new API avoids raw pointers so it survives that switch). Sanctioned behaviour change: setters schedule a repaint.
- `replot()` stays public and synchronous (export, tests). `replot(rpQueuedReplot)` stays the one deduplicated path.
- Build and test one at a time, foreground, `-j 4`. Full suite in `build-qt612` and `build-asan` (`LSAN_OPTIONS=suppressions=$(pwd)/tests/lsan.supp`); GPU tests under `xvfb-run -a -s "-screen 0 1600x1200x24" env QT_QPA_PLATFORM=xcb`.
- Benchmark: `QT_QPA_PLATFORM=offscreen build-qt612/tests/benchmark/qcp_benchmark -median 5`, three runs; medians within ±15 % of `docs/plans/neoqcp-3/baseline.md` ("End of milestone 1").
- Every bug fix starts with a failing test. Commit after each task with explicit paths, ending `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Do not push.
- Delegation: opencode builds tables (setter lists, item position names) and transcribes code once a pattern exists and is reviewed; judgement calls (which layer a setter dirties, which `replot()` a snippet still needs) stay with the implementer.

## Review Focus

1. A handle kept after its object was deleted, including a slot that deletes it in the middle of a chain: later setters are no-ops, `if (h)` is false (pinned in Task 2).
2. `add<MyGraph>()` and `add<Deeper>()` (a subclass of a subclass) chain base setters then their own (pinned in Task 2).
3. 1000 setter calls in one turn produce exactly one replot; a hidden plot queues nothing visible (pinned in Task 1).
4. A slot that dirties something on every `afterReplot` repaints once per turn, never inside the same turn, and other events still run (pinned in Task 1).
5. `take()` then `add(std::move(ptr))` onto another plot: drawn there, no entry left in the old plot's lists, legend, groups or colour-scale link (pinned in Task 6).

---

### Task 0: Inventory (done)

`docs/plans/neoqcp-3/milestone-2-inventory.md` committed with this plan (3d8a8ad).

### Task 1: A dirtied layer schedules one repaint

**Files:** Modify `src/layer.{h,cpp}`, `src/core.{h,cpp}`; Test: `tests/auto/test-repaint/test-repaint.{h,cpp}` (class `TestRepaint`, registered in `tests/auto/meson.build` and `autotest.cpp`).

**Interfaces:**
- Produces:
  - `void QCustomPlot::requestRepaint();` — schedules `replot(rpQueuedReplot)` once; no-op while `isBeingDestroyed()`. Fix the ordering in `replot()` (`src/core.cpp:2422-2431`): the queued branch must not schedule while a replot is running; the end-of-replot check below decides instead.
  - `bool QCustomPlot::isRepaintQueued() const;`
  - `QCPLayer`: `quint64 mDirtyGeneration, mDrawnGeneration`. `markDirty()` and `invalidatePaintBuffer()` bump the plot-wide generation into `mDirtyGeneration` and call `requestRepaint()`; `drawToPaintBuffer()` records `mDrawnGeneration`. At the end of `replot()`, if any layer has `mDirtyGeneration > mDrawnGeneration` (dirtied during this replot after it was drawn — an `afterReplot` slot or a draw-path setter), schedule one queued replot. That is one per turn: a slot that dirties on every `afterReplot` repaints continuously like an animation, never re-entrantly.
  - `void QCPLayerable::markDirty();` — no-op when `parentPlot()` is null or the layerable has no layer; else marks its layer (or invalidates it when the layer is unbuffered) — which schedules.

- [ ] **Step 1: Failing tests** in `TestRepaint` (count `afterReplot`):
  - `markDirtySchedulesOneRepaint`: `graph->markDirty()` ×1000 → `isRepaintQueued()`; `QTRY_COMPARE(count, 1)`; one more `processEvents()` keeps 1.
  - `dirtyAfterDrawRepaintsNextTurn`: an `afterReplot` slot dirties the graph's layer once (first call only): count reaches exactly 2, and the second replot did not run inside the first (`isReplotting()` false at slot entry both times).
  - `afterReplotSlotDirtyingEveryTimeDoesNotStarve`: the slot dirties every time; a `QTimer::singleShot(0)` posted before still fires; after 5 `processEvents()` the count is ≤ 6 and grows by at most one per call.
  - `hiddenPlotQueuesNoVisibleWork`: hidden plot, `setSkipReplotsWhenHidden(true)`: after `markDirty()` and `processEvents()`, `afterReplot` count 0 and `isRepaintQueued()` false; showing the plot draws the change.
  - `markDirtyWithoutPlotIsNoOp`: a layerable with no parent plot/layer: no crash, no misuse.
  - `syncReplotStillWorks`: `replot()` emits `afterReplot` before it returns.
- [ ] **Step 2:** Build, run. Expected: the first three fail.
- [ ] **Step 3:** Implement.
- [ ] **Step 4:** `TestRepaint`, `TestSignalDelivery`, full suite (both builds), benchmark.
- [ ] **Step 5: Commit** — "feat(core): a dirtied layer schedules one repaint per turn".

### Task 2: `add<T>` and the fluent handle, for graphs

**Files:** Create `src/handle-ops.h` (family ops templates); Modify `src/core.h` (`add<T>`), `src/plottables/plottable.h` (contract doc, `QCPPlottableOps`), `src/plottables/plottable-graph.h` (`QCPGraphOps`), `src/qcp.h`, `src/meson.build`, `cmake/sources.cmake`; Test: `tests/auto/test-builder/test-builder.{h,cpp}` (class `TestBuilder`).

**Interfaces:**
- Produces:
  - `template <typename T, typename Self = void> class QCPPlottableOps : public QCPHandle<T>` — `using Ret = std::conditional_t<std::is_void_v<Self>, QCPPlottableOps, Self>`; setters return `Ret` **by value** (a `QPointer` copy): `setName(const QString&)`, `setPen(const QPen&)`, `setBrush(const QBrush&)`, `setVisible(bool)`, `setLayer(const QString&)`, `setAxes(QCPAxis::AxisType key, QCPAxis::AxisType value)` (resolved on the plottable's axis rect — role, not pointer, so it survives milestone 5), `setAxisRect(QCPAxisRect*)`, and the escape hatch `setAxes(QCPAxis*, QCPAxis*)` (doc: goes away with raw getters). Every setter: no-op on a dead handle; checks its value (`QCP_CHECK_OR_RETURN`: foreign axis or rect, unknown layer, an axis role the rect does not have) and keeps the old value.
  - `template <typename T, typename Self = void> class QCPGraphOps : public QCPPlottableOps<T, Ret…>` adds `setLineStyle`, `setScatterStyle`, `setData(keys, values)`.
  - `QCPGraph` declares `template <typename T> using OpsFor = QCPGraphOps<T>;` (inherited by subclasses). The returned type of `add<T>` is `T::Handle` if `T` declares it, else `typename T::template OpsFor<T>` — so `add<MyGraph>()` with no declaration returns a handle typed `QCPHandle<MyGraph>` with all graph setters; a subclass adding setters declares `class Handle : public QCPGraphOps<MyGraph, Handle> { … }`.
  - `template <typename T, typename... Args> auto QCustomPlot::add(Args&&... args);` — constructs `new T(keyAxis, valueAxis)` on the default axis rect's bottom/left axes and `currentLayer()`, then `if constexpr (sizeof...(Args) > 0) h.setData(std::forward<Args>(args)...)` (`static_assert` with T's name when no such `setData`). Refused (empty handle + one misuse) while `isBeingDestroyed()` or when there is no axis rect.
  - Contract (written in `QCPAbstractPlottable`'s doc): keep the `T(QCPAxis*, QCPAxis*)` constructor; declare `Handle` only to add setters.

- [ ] **Step 1: Failing tests** in `TestBuilder`:
  - `addGraphWithData`: `auto g = plot.add<QCPGraph>(QVector<double>{1,2,3}, QVector<double>{4,5,6}).setName("Bx").setPen(QPen(Qt::red));` → `g` true, `g->name() == "Bx"`, `g->dataCount() == 3`, `plot.graphCount() == 1`, `g->keyAxis() == plot.xAxis`.
  - `addWithoutData`: `auto g = plot.add<QCPGraph>();` → true, `dataCount() == 0`.
  - `handleIsTheHandle`: `static_assert(std::is_base_of_v<QCPHandle<QCPGraph>, decltype(plot.add<QCPGraph>())>)`; `static_assert` that binding `auto&` to a setter result does not compile is not expressible — instead `static_assert(!std::is_reference_v<decltype(g.setName(""))>)`.
  - `oneChainOneRepaint`: a five-setter chain → exactly one `afterReplot` after `processEvents()`.
  - `deadHandleSettersAreNoOps`: `g.remove(); g.setName("x").setPen(QPen());` no crash, no misuse, `!g`.
  - `slotDeletesMidChain`: a `dataChanged` slot calls `remove()` on the graph; `plot.add<QCPGraph>(x, y).setName("x").setPen(QPen())` → no crash, no misuse, plot still replots.
  - `setAxesByRole`: `.setAxes(QCPAxis::atTop, QCPAxis::atRight)` → `keyAxis() == plot.xAxis2`; a role the rect lacks → one misuse, axes unchanged; `.setAxes(other.xAxis, other.yAxis)` (foreign) → one misuse.
  - `addUserSubclass`: test-local `MyGraph : QCPGraph` with `Handle : QCPGraphOps<MyGraph, Handle>` adding `setTag(int)`; `plot.add<MyGraph>().setName("m").setTag(7)` compiles and sets both; `Deeper : MyGraph` with its own `Handle` adding `setDepth(int)`: `plot.add<Deeper>().setName("d").setTag(1).setDepth(2)`; `PlainSub : QCPGraph` with no declaration: `plot.add<PlainSub>()` is a `QCPHandle<PlainSub>` with `setLineStyle`.
  - `addRefusedDuringTeardown`: a child's `destroyed()` slot calls `plot->add<QCPGraph>()` while the plot dies → empty handle, one misuse.
- [ ] **Step 2:** Build. Expected: does not compile (`add` missing).
- [ ] **Step 3:** Implement.
- [ ] **Step 4:** `TestBuilder`, `TestHandle`, `TestRepaint`, full suite; clang check of `test-builder.cpp`.
- [ ] **Step 5: Commit** — "feat(core): plot.add<T>() returning a fluent handle, starting with QCPGraph".

### Task 3: Appearance setters schedule a repaint — axes, grids, axis rects

**Files:** `src/axis/*.cpp`, `src/layoutelements/layoutelement-axisrect.cpp`; Test: `TestRepaint`.

**What "dirty" means here (not layerables):** `QCPAxis` setters → `axisRect()->markLayersUsingAxisDirty(this)` plus the axis's own layer; `QCPGrid` → its layer; `QCPAxisRect` (background, margins, insets) → its own layer and the layers of its axes; range setters already dirty through `rangeDidChange` (verify, do not double).

- [ ] **Step 1 (opencode):** write `docs/plans/neoqcp-3/milestone-2-setters.md`, section "axes": every public setter in these files with file:line and a column "changes the drawing? (yes / data / no)". The implementer reviews the table.
- [ ] **Step 2: Failing test** `axisSettersScheduleRepaint` (data-driven over every "yes" row: call the setter on a shown-or-hidden-skip-off plot → `isRepaintQueued()`).
- [ ] **Step 3:** Implement by hand from the table. Replace setter-internal `replot()` in these files with `requestRepaint()`; input-handler and export `replot()` calls stay.
- [ ] **Step 4:** `TestRepaint`, full suite, benchmark (`QCPAxis_SetRange10k`).
- [ ] **Step 5: Commit** — "feat(axis): appearance setters schedule a repaint".

### Task 4: …plottables

Same shape as Task 3 for `src/plottables/*.cpp` (dirty = `QCPLayerable::markDirty()`; replace the span-free request paths: colormap2 `redrawLater`, histogram `requestReplot`, histogram2d, intervals `requestRepaint`, multigraph/graph2 constructor and source replots that only show a change). Section "plottables" of the setters table; test `plottableSettersScheduleRepaint`. Commit — "feat(plottables): appearance setters schedule a repaint".

### Task 5: …items, legend, colour scale, text, layout, polar, theme

Same shape for `src/items/*.cpp` (span `setRange`/`setKeyRange`/`setValueRange` lose their `replot`), `src/layoutelements/*.cpp` (legend, legend items incl. `setExpanded`, colour scale → its own layer and its axis's, text element), `src/polar/*.cpp`, `src/layer.cpp` (`setVisible`), theme application. Sections "items", "layout", "polar" of the table; test `remainingSettersScheduleRepaint`. Commit — "feat: remaining appearance setters schedule a repaint".

### Task 6: Handles for the other families, `take` and `add(std::unique_ptr)`

**Files:** `src/handle-ops.h`, every concrete plottable and item header, `src/layoutelements/*.h`, `src/core.{h,cpp}`; Test: `TestBuilder`.

**Interfaces:**
- Produces:
  - Ops per family: `QCPPlottableOps` subclasses for every plottable with its main appearance setters and `setData` overloads matching its 2.x `setData` (curve, bars, financial, statistical box, error bars, colormap, colormap2, multigraph, histogram, histogram2d, intervals); `QCPItemOps<T, Self>` (base: `setVisible`, `setLayer`, `setClipToAxisRect`, `setPen`) and per item its positions in the item's default position type (table first: opencode writes the per-item position-name table into the setters doc, e.g. line `setStart/setEnd`, bracket `setLeft/setRight`, ellipse `setTopLeft/setBottomRight`); `QCPLayerableOps<T, Self>` (visible, layer) for axis rects, text elements, colour scales, legends; `QCPObjectOps<T, Self>` (no layer) for `QCPBarsGroup`, `QCPPlottableGroup`.
  - Construction per family (one trait, `qcp::construct<T>(QCustomPlot&, …)`, specialised per family): plottables `(keyAxis, valueAxis)`; items `(plot)`; axis rect / text element / colour scale `(plot)` plus insertion into the layout (`add<QCPAxisRect>(row, col)`, `add<QCPTextElement>(row, col, text)`, `add<QCPColorScale>(row, col)`); groups parented to the plot (`add<QCPBarsGroup>()`, `add<QCPPlottableGroup>(name)` — no user `new`); `rect.add<QCPAxis>(QCPAxis::AxisType)`.
  - `template <typename T> std::unique_ptr<T> QCustomPlot::take(QCPHandle<T> h);` — removes the object from: the plottable/item lists, its legend row (explicitly, the object is not destroyed), plottable and bars groups, a colour map's colour-scale link, its layer, the axes (set to null); unparents. Dead handle, a foreign object, or during teardown → empty pointer + one misuse.
  - `template <typename T> auto QCustomPlot::add(std::unique_ptr<T> object);` — reparents, attaches to the default axes and current layer (re-target with the returned handle's setters), auto-adds the legend row as `addGraph` does.
- [ ] **Step 1: Failing tests:** `addEveryPlottable` and `addEveryItem` (data-driven: type, one family setter, count); `addAxisToRect` (`rect.add<QCPAxis>(QCPAxis::atRight)` → `axisCount(atRight) == 2`); `addLayoutParts`; `groupsOwnedByPlot` (ASan: deleting the plot frees them); `takeAndReAdd` (graph in a legend and a plottable group: take from A, add to B; A has no graph, no legend row, group no member; B draws it — pixel check); `takeColorMapLeavesScale`; `takeDeadHandle` and `takeDuringTeardown` (one misuse, null); `addUserBuilt` (`plot.add(std::make_unique<MyGraph>(nullptr, nullptr))` attaches to the default axes); deleting a taken pointer without re-adding is ASan clean.
- [ ] **Step 2:** Build. Expected: compile failures.
- [ ] **Step 3:** Implement the plottable ops and the construction trait by hand for two plottables and one item; then opencode transcribes the rest from the table; the implementer reviews each and writes the `setData` overloads with non-trivial arguments (financial, histogram2d, intervals, colormap) by hand.
- [ ] **Step 4:** `TestBuilder`, `TestOwnership`, full suite (both builds), clang check.
- [ ] **Step 5: Commit(s)** — "feat: handles for every family", "feat(core): take(handle) and add(std::unique_ptr)".

### Task 7: Polar

**Files:** `src/polar/*.h`; Test: `TestBuilder`.
- [ ] **Step 1: Failing test** `addPolar`: `auto a = plot.add<QCPPolarAxisAngular>(row, col); a.add<QCPPolarGraph>(keys, values).setName("p")` (the polar graph's owner is the angular axis: construction trait `(angularAxis, radialAxis)`) — draws (pixel check), has a legend row; `take` of a polar graph removes it from the angular axis's list.
- [ ] **Step 2–4:** as Task 6. **Step 5: Commit** — "feat(polar): add<T> and handles".

### Task 8: `remove()` routed through the owner

**Files:** `src/handle.h`, `src/handle-ops.h`; Test: `TestHandle`.

**Interfaces:**
- Produces: `remove()` constrained (`requires`) to plot-owned families; plottables, items → `delete`; axes → `QCPAxisRect::removeAxis`; layers → `QCustomPlot::removeLayer` (refuses the last layer, as 2.x); axis rects and layout elements → take from the layout, then delete; `QCPHandle<QCustomPlot>` and other widgets have no `remove()` (doc: close the window or `deleteLater()`). Replaces the milestone-1 `simplify:` note.
- [ ] **Step 1: Failing tests:** `removeAxisThroughHandle` (`axisCount` drops, replot OK), `removeLayerThroughHandle`, `removeAxisRectThroughHandle` (layout cell empty, replot OK), `noRemoveOnWidgets` (`static_assert` with a requires-expression that `QCPHandle<QCustomPlot>` has no `remove()`).
- [ ] **Step 2–4:** as above. **Step 5: Commit** — "feat(core): remove() through a handle asks the owner".

### Task 9: Port the guide and examples

**Files:** `docs/snippets/*.cpp`, `docs/*.md`, `examples/**`; Test: the `doc-snippets` meson test; `build-qt612/docs/snippets/doc-shots docs/images`.
- [ ] **Step 1 (opencode, mechanical):** `addGraph(...)` + setter lines → `add<T>(…)` chains; `new QCPItem…(plot)` → `plot->add<…>()`; list each change in `docs/plans/neoqcp-3/milestone-2-port.md`.
- [ ] **Step 2 (implementer, by file):** delete `replot()` calls that only showed a change; keep those needed for a deterministic image or a synchronous frame (`savePng`/`toPixmap` lay out themselves — verify per snippet).
- [ ] **Step 3:** `doc-snippets` passes; regenerate the images; `git diff --stat docs/images` — any changed image is checked by eye and explained in the port doc.
- [ ] **Step 4: Commit** — "docs: the guide and examples use add<T>() and automatic repaint".

### Task 10: Close the milestone

- [ ] **Step 1:** Full suite in both builds (+ xvfb ASan); benchmark ×3 appended to `baseline.md` ("End of milestone 2").
- [ ] **Step 2:** opencode honest, contradicting review of `git diff 393d6bc..HEAD -- src tests`; verify each point; fix the real ones test-first.
- [ ] **Step 3:** Changelog under "3.0 (in progress)": `add<T>` and the fluent handles (the subclass contract), `take`/`add(unique_ptr)`, `remove()` through the owner, automatic repaint (one per turn, dirty-after-draw rule, `replot()` for export and tests).
- [ ] **Step 4: Commit** — "docs: milestone 2 of the 3.0 object model". Report to the maintainer; no push.

### Moved out of this milestone

Item scaling (constant-time removal, view culling; spec decision 2) becomes its own small milestone: its target must be set from the end-of-milestone-1 number (61 ms, `baseline.md`), after explaining why that row got 24 % faster.
