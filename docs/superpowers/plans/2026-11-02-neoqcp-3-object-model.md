# NeoQCP 3.0 Object Model — Milestone 1: Foundations — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make every object a user can reach safe to delete, and give NeoQCP the safety primitives the rest of 3.0 builds on (misuse reporting, typed handles, deferred signal delivery, a safe teardown), with today's speed measured first.

**Architecture:** Additive and compatible: the 2.x API keeps working through this milestone. Registries on the plot, axis rects and legend stop holding pointers that can outlive their targets; destructors unregister. A misuse handler, a `QCPHandle<T>` and an emission scope are new small headers that later milestones use everywhere.

**Tech Stack:** C++20, Qt 6 (QRhiWidget), Meson, Qt Test (one `auto-tests` executable), the existing benchmark target.

**Spec:** `docs/specs/2026-11-02-neoqcp-3-object-model-design.md` (with evidence in `docs/plans/neoqcp-3/`).

## Roadmap: the object model in five milestone plans

The spec is too large for one plan. Each milestone below gets its own plan, written when the previous one
is done, and leaves NeoQCP building and green. Only milestone 1 is planned here.

1. **Foundations (this plan):** benchmark baseline; measure why one plot with many axis rects is slow
   (input to spec decision 13); misuse reporting; delete-safe registries; safe teardown; `QCPHandle<T>`;
   deferred signal delivery at the known hazard sites.
2. **Creation and removal:** `plot.add<T>(…)` returning builders for every existing type; `take()`;
   `handle.remove()`; automatic, deduplicated repaint (every setter marks dirty; no library `replot()`).
3. **Drawing and extension API:** typed positions and transform objects; the recording `draw()` context
   and its helpers; default clip, hit test, legend icon and data range; the stacking rule and default
   layers; the one hit-test rule. One plottable and one item ported as proof.
4. **Port everything:** the line-plottable family, colour maps and histograms behind shared interfaces,
   the colour-scale relation, one span class, axes (`add<QCPAxis>`, linked axes, drag and zoom defaults),
   the selection model, polar; all remaining signals through deferred delivery; the alignment decision.
5. **Finish:** docs, examples and the WebAssembly gallery on the new API; the old API removed; `main`
   becomes 3.0 and `2.x` carries 2.x fixes.

## Global Constraints

- C++20, Qt 6, QRhiWidget rendering; Meson build; the CMake build and the WebAssembly build keep compiling.
- NeoQCP throws no exceptions.
- NeoQCP objects are used on the GUI thread only.
- The 2.x API keeps compiling and passing its tests through this milestone (additive changes only).
- Build and test one at a time, in the foreground, `-j 4`: `meson compile -C build-qt612 -j 4`. Never run two
  builds or test runs against one build directory at once.
- Run one test class: `QT_QPA_PLATFORM=offscreen QT_LOGGING_RULES="*.debug=true" QCP_TEST_CLASS=<Class> build-qt612/tests/auto/auto-tests`.
  Full suite: `QT_LOGGING_RULES="*.debug=true" meson test -C build-qt612 --print-errorlogs`. (`QT_LOGGING_RULES`
  because Fedora's system `qtlogging.ini` turns debug output off, which breaks tests expecting messages.)
- New test classes live in `tests/auto/test-<name>/test-<name>.{h,cpp}`, are added to `test_srcs` and
  `test_headers` in `tests/auto/meson.build`, and get a `QCPTEST(<Class>);` line in `tests/auto/autotest.cpp`.
- Every bug fix starts with a test that fails before the fix.
- Speed: no task may regress the baseline of Task 1 beyond measurement noise (re-run the benchmark at the
  end of each task that touches a registry or an emit site).
- Commit after each task with explicit paths, ending with
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Do not push.

## Review Focus

1. A slot on a child's `destroyed()` that deletes or adds another object while its plot is being destroyed:
   no crash, and the add is refused (pinned in Task 7).
2. A slot that deletes the plot from `beforeReplot`, `afterLayout` or `afterReplot`: no crash, `replot()`
   returns cleanly (pinned in Task 9).
3. `deleteLater()` on a plottable or item, then the event loop runs, then a replot and a legend draw: no
   dangling list entry (pinned in Tasks 4 and 5).
4. Deleting the current layer, or a layer that has GPU layers registered, then replotting (pinned in Task 6).
5. A `QCPHandle` copied into a lambda that runs after the plot is gone: tests false, `get()` is null
   (pinned in Task 8).

---

### Task 0: Branches

**Files:** none.

- [ ] **Step 1:** From the current `design/neoqcp-3.0` branch (based on `origin/main` e9daa0c), create a local
  `2.x` branch at e9daa0c (`git branch 2.x e9daa0c`) and the work branch `neoqcp-3.0` from
  `design/neoqcp-3.0` (`git switch -c neoqcp-3.0`). Expected: `git branch` lists both. Pushing either needs
  the maintainer's consent.

### Task 1: A benchmark that measures, and the baseline

**Files:**
- Modify: `tests/benchmark/meson.build:8` (it runs `auto_tests`, not the benchmark)
- Modify: `tests/benchmark/benchmark.cpp`
- Create: `docs/plans/neoqcp-3/baseline.md`

**Interfaces:**
- Produces: benchmark functions `QCPItems_AddRemove10k`, `QCPItems_Replot10k`, `QCPAxisRects_Stack8_Replot`,
  `QCPPlots_Stack8_Replot`, `QCPAxis_SetRange10k` in class `Benchmark`; the baseline table later tasks compare
  against. These time the CPU side of `replot()` (paint-buffer rasterisation): the GPU upload and composite
  happen later in `render()`, which an offscreen benchmark does not run. Task 2 measures the full frame.

- [ ] **Step 1:** Change the registration to `benchmark('qcp_benchmark', qcp_benchmark)`. Run
  `meson test -C build-qt612 --benchmark --print-errorlogs`. Expected: the output lists `Benchmark::` functions
  (today it lists the unit tests) and contains `RESULT` lines (Qt Test prints them for every `QBENCHMARK`).
- [ ] **Step 2:** Add the four benchmarks, each a `QBENCHMARK` block:
  - `QCPItems_AddRemove10k`: create 10 000 `QCPItemLine`s on `mPlot`, then `clearItems()`.
  - `QCPItems_Replot10k`: with 10 000 `QCPItemLine`s spread over the x range `[0, 100]` and the view at
    `[0, 1]`, time `mPlot->replot(QCustomPlot::rpImmediateRefresh)`.
  - `QCPAxisRects_Stack8_Replot`: one plot, 8 axis rects stacked in the root layout, each with one graph of
    100 000 points; time a replot after changing one rect's x range.
  - `QCPPlots_Stack8_Replot`: the same data as 8 separate `QCustomPlot` widgets; time a replot of the one
    whose range changed.
  - `QCPAxis_SetRange10k`: 10 000 `xAxis->setRange` calls on a plot with one 10 000-point graph (this is
    the path Task 9's deferred delivery touches).
- [ ] **Step 3:** Build, run `build-qt612/tests/benchmark/qcp_benchmark -median 5` three times, and write
  `docs/plans/neoqcp-3/baseline.md`: one table, one row per benchmark, the median of the three runs, the
  machine, the Qt version and the commit. Expected: every existing benchmark and the four new ones have a
  number.
- [ ] **Step 4: Commit** `tests/benchmark/meson.build tests/benchmark/benchmark.cpp docs/plans/neoqcp-3/baseline.md`
  — "test(benchmark): run the benchmark target; items and stacked-axis-rect baselines".

### Task 2: Why is one plot with many axis rects slow? (measurement only)

**Files:**
- Create: `docs/plans/neoqcp-3/multi-axisrect-perf.md`

- [ ] **Step 1:** Using the Task 1 benchmarks `QCPAxisRects_Stack8_Frame` and `QCPPlots_Stack8_Frame`, and a
  Tracy capture of each (`meson setup -Dtracy_enable=true --buildtype=debugoptimized build-tracy` once, then
  the same scenario, on a real display, `QT_QPA_PLATFORM=xcb`, so `render()` runs and the GPU side is in the
  capture), find where the stacked-rect frame spends its time compared with the separate plots:
  which layers repaint on a one-rect range change, whether every rect's plottables redraw, and whether the
  GPU paths re-upload for the whole widget.
- [ ] **Step 2:** Write `multi-axisrect-perf.md`: the two numbers, the cause in one paragraph with
  `file:line`, and what would make a range change in one rect repaint only that rect. Expected: a
  maintainer reading it can decide spec decision 13 (stacked rects in one plot, or cross-widget groups).
  No code change in this task.
- [ ] **Step 3: Commit** the file — "docs(plans): why stacked axis rects in one plot are slow".

### Task 3: Misuse reporting

**Files:**
- Create: `src/misuse.h`, `src/misuse.cpp`
- Modify: `src/meson.build` (sources), `src/qcp.h` (include), the CMake source list
- Test: `tests/auto/test-misuse/test-misuse.{h,cpp}` (class `TestMisuse`)

**Interfaces:**
- Produces, in namespace `qcp`:
  - `using MisuseHandler = void (*)(const char* where, const char* what);`
  - `MisuseHandler setMisuseHandler(MisuseHandler handler);` returns the previous one; `nullptr` restores the default.
  - `void reportMisuse(const char* where, const char* what);`
  - macro `QCP_CHECK(cond, what)`: if `!(cond)`, calls `qcp::reportMisuse(Q_FUNC_INFO, what)`.
  - macro `QCP_CHECK_OR_RETURN(cond, what, ...)`: same, then `return __VA_ARGS__;` (empty for `void`).
- Default handler: in a debug build (`QT_NO_DEBUG` not defined) `qFatal("%s: %s", where, what)`; in a
  release build `qWarning("NeoQCP misuse in %s: %s", where, what)`. (Spec decision 6, answer 4: assert in
  debug, warn and refuse in release; the refusal is the caller's `QCP_CHECK_OR_RETURN`.)

- [ ] **Step 1: Write the failing tests** in `TestMisuse` (each installs a recording handler in `init()` and
  restores the default in `cleanup()`):
  - `checkPassesSilently`: `QCP_CHECK(true, "x")` records nothing.
  - `checkReportsWhereAndWhat`: `QCP_CHECK(false, "bad axis")` records one call whose `what` is `"bad axis"`
    and whose `where` contains the enclosing function's name.
  - `checkOrReturnRefuses`: a helper `int f(bool ok) { QCP_CHECK_OR_RETURN(ok, "no", -1); return 1; }` returns
    `-1` for `false` (and records one call) and `1` for `true`.
  - `setMisuseHandlerReturnsPrevious`: installing handler B after A returns A.
- [ ] **Step 2:** Register the class (Global Constraints), build, run `TestMisuse`. Expected: fails to compile
  (`misuse.h` not found).
- [ ] **Step 3:** Implement `src/misuse.{h,cpp}` with the interfaces above; the handler pointer is a
  file-static `std::atomic<MisuseHandler>`.
- [ ] **Step 4:** Build, run `TestMisuse`. Expected: 4 passed.
- [ ] **Step 5: Commit** — "feat(core): misuse reporting: QCP_CHECK asserts in debug, warns and refuses in release".

### Task 4: Deleting a plottable is safe

**Files:**
- Modify: `src/plottables/plottable.cpp` (destructor; constructor null-axis check), `src/plottables/plottable.h`
- Modify: `src/core.h`, `src/core.cpp` (an unregister entry point; `removePlottable`)
- Modify: `src/layoutelements/layoutelement-legend.h:87,128`, `.cpp` (legend back-pointers)
- Test: `tests/auto/test-ownership/test-ownership.{h,cpp}` (class `TestOwnership`)

**Interfaces:**
- Produces: `void QCustomPlot::plottableDestroyed(QCPAbstractPlottable* plottable)` (private, friend of the
  plottable base): removes it from `mPlottables`, `mGraphs`, its group, and its legend row; safe to call
  twice. `QCPAbstractLegendItem::mParentLegend` and `QCPPlottableLegendItem::mPlottable` become `QPointer`s.

- [ ] **Step 1: Write the failing tests** in `TestOwnership`:
  - `deletePlottableLeavesLists`: add two graphs (one via `addGraph`, one `new QCPCurve`), `delete` the first;
    `plottableCount() == 1`, `graphCount() == 0`, `legend->itemCount() == 1`, and `replot()` runs.
  - `deleteLaterPlottable`: same with `deleteLater()` then `QCoreApplication::sendPostedEvents(nullptr,
    QEvent::DeferredDelete)`.
  - `deletePlottableInGroup`: a plottable in a `QCPPlottableGroup`, deleted directly; the group's members
    no longer include it and its legend row updates.
  - `removePlottableStillDeletes`: `removePlottable(g)` keeps today's behaviour (returns true, deleted).
  - `nullAxisPlottableIsRefused`: `new QCPCurve(nullptr, nullptr)` reports one misuse (recording handler) and
    does not crash; the curve is not registered.
  - `legendItemMovedToSecondLegend`: a plottable legend item added to a second `QCPLegend` reports that
    legend as its parent; deleting the first legend and then drawing the second does not crash.
- [ ] **Step 2:** Register the class, build, run `TestOwnership`. Expected: the delete tests crash or fail on
  the counts (run under `build-asan` to see the use-after-free if the debug build happens to survive).
- [ ] **Step 3:** In `~QCPAbstractPlottable`, call `mParentPlot->plottableDestroyed(this)` if the plot is
  alive (hold the plot as `QPointer` there). Make `removePlottable` unlist before deleting and rely on the
  same unregister. A null axis: the base constructor dereferences `keyAxis` in its member initializer
  list (`plottable.cpp:437-438`), so make that initializer null-safe, then `QCP_CHECK` in the body; a
  plottable with no axes stays inert (never registered, never drawn), which keeps the documented "the
  construction is not aborted". Check that the derived constructors used in the test (`QCPCurve`) do
  not dereference the plot or axes when inert.
  Update `mParentLegend` in `QCPLegend::addItem`. A plottable legend item whose plottable dies removes itself
  from its legend.
- [ ] **Step 4:** Build, run `TestOwnership` (then under `build-asan`). Expected: 6 passed, no ASan report.
- [ ] **Step 5:** Run the full suite and the benchmark. Expected: all green; no regression beyond noise.
- [ ] **Step 6: Commit** — "fix(ownership): deleting a plottable leaves the plot, its group and its legend".

### Task 5: Deleting an item is safe

**Files:**
- Modify: `src/items/item.cpp:1090-1094` (destructor), `src/core.cpp:1673-1720` (`removeItem`, `clearItems`)
- Test: `tests/auto/test-ownership/test-ownership.cpp`

**Interfaces:**
- Produces: `void QCustomPlot::itemDestroyed(QCPAbstractItem* item)` (private), mirror of Task 4.

- [ ] **Step 1: Write the failing tests:**
  - `deleteItemLeavesList`: two items, `delete` one; `itemCount() == 1`; `replot()` runs.
  - `deleteLaterItem`: same with `deleteLater()` then `QCoreApplication::sendPostedEvents(nullptr,
    QEvent::DeferredDelete)`.
  - `removeItemFromDestroyedSlot`: connect an item's `destroyed` to a slot that calls `plot->clearItems()`;
    `removeItem(item)` does not delete anything twice (`build-asan` clean) and `itemCount() == 0` after.
  - `deleteParentAnchorItem`: an item anchored to a tracer; `delete` the tracer; the child item's position
    keeps its pixel location and `replot()` runs.
  - `spanDestructorCreatesNoGpuLayer`: create and delete a `QCPItemVSpan` on a plot that never drew one;
    the plot's span GPU layer still does not exist (use the non-creating accessor).
- [ ] **Step 2:** Build, run. Expected: the first two fail or crash; the last fails (a layer is created).
- [ ] **Step 3:** Unregister in `~QCPAbstractItem`; `removeItem` and `clearItems` unlist before deleting and
  iterate a guarded snapshot; span destructors use the non-creating accessor (`item-spanbase.cpp:7-11`).
- [ ] **Step 4:** Build, run `TestOwnership` (and `build-asan`). Expected: all passed, ASan clean.
- [ ] **Step 5:** Full suite and benchmark. Expected: green, no regression.
- [ ] **Step 6: Commit** — "fix(ownership): deleting an item leaves the plot; removeItem unlists first".

### Task 6: Deleting axes, grids, layers and plot parts is safe

**Files:**
- Modify: `src/axis/axis.{h,cpp}` (destructor; `mGrid` as `QPointer` with on-demand re-creation)
- Modify: `src/layoutelements/layoutelement-axisrect.cpp:221-242, 432-466, 624, 644, 675`
- Modify: `src/layer.cpp:140-152`, `src/core.{h,cpp}` (layer unregister; `mPlotLayout`, `mOverlay`,
  `mOwnedTheme`, `mCurrentLayer`, the `xAxis`…`legend` shortcuts)
- Test: `tests/auto/test-ownership/test-ownership.cpp`

**Interfaces:**
- Produces: `QCPAxis::grid()` always returns a live grid (re-created on demand if deleted, the
  `QMainWindow::statusBar()` pattern). The plot's shortcut members become `QPointer`s; `plotLayout()` and
  `overlay()` re-create an empty root layout or overlay if theirs was deleted, except while the plot is
  being destroyed (Task 7), when they return `nullptr`.

- [ ] **Step 1: Write the failing tests:**
  - `deleteAxisLeavesRectAndShortcuts`: `delete plot->yAxis`; `axisRect()->axes()` no longer contains it,
    `plot->yAxis == nullptr`, and a graph that used it is hidden and reports one misuse at the next replot.
  - `plottablesOfAnotherRectAfterAxisRemoval`: remove an axis rect's axes while a graph uses them, then call
    `plot->axisRect(0)->plottables()` and a rubber-band selection: no crash.
  - `deleteGridTwiceSafe`: `delete axis->grid()`; `axis->grid()` returns a new live grid; deleting the plot
    afterwards is ASan-clean.
  - `deleteLayerLeavesLists`: `delete` a non-current layer that holds a graph with GPU layers; `layerCount()`
    drops by one, the graph moves to the layer below, and `replot()` runs.
  - `deleteCurrentLayer`: `delete plot->currentLayer()`; the current layer becomes the one below; `replot()` runs.
  - `deleteRootLayoutAndOverlay`: `delete plot->plotLayout()` then `replot()`; `delete plot->overlay()` then
    `plot->overlay()` returns a new live overlay.
  - `deleteOwnedTheme`: `delete plot->theme()` while it is the plot's own theme, then `setTheme(nullptr)`
    and `replot()` run.
  - `removeAxisHidesItsPlottables`: `axisRect()->removeAxis(axisRect()->axis(QCPAxis::atLeft))` while a
    graph uses it; the graph is not drawn, one misuse is recorded over two replots.
- [ ] **Step 2:** Build, run. Expected: each fails or crashes before its fix.
- [ ] **Step 3:** `~QCPAxis` tells its rect and the plot (reuse `axisRemoved`); the axis-rect scans skip
  plottables and items with a null axis; a plottable whose key or value axis is gone is skipped when
  drawing and reports one misuse (once per plottable, not once per replot); `QCPAxisRect::removeAxis` gives
  the same result as a `delete`; `~QCPLayer` unregisters from `mLayers` and the GPU-layer maps and fixes
  the current layer; the plot's raw shortcuts and parts become `QPointer`s with the re-creation above.
- [ ] **Step 4:** Build, run `TestOwnership` (and `build-asan`). Expected: all passed, ASan clean.
- [ ] **Step 5:** Full suite and benchmark. Expected: green, no regression.
- [ ] **Step 6: Commit** — "fix(ownership): axes, grids, layers and plot parts are safe to delete".

### Task 6b: The remaining dangling relations

**Files:**
- Modify: `src/items/item-tracer.{h,cpp}` (`mGraph` raw, `item-tracer.h:120`), `src/plottables/plottable-colormap.cpp`,
  `src/plottables/plottable-bars.cpp`, `src/polar/layoutelement-angularaxis.cpp:634-648`,
  `src/plottables/plottable-intervals.cpp` (lane-layout dereferences at `:127, 167, 269, 409, 566, 616-619, 837`)
- Test: `tests/auto/test-ownership/test-ownership.cpp`

- [ ] **Step 1: Write the failing tests:**
  - `deleteTracedGraph`: a tracer on a graph; `delete` the graph; `tracer->graph() == nullptr`, the tracer
    is not drawn, `replot()` runs.
  - `deleteColorScaleOfMap`: `delete` a colour map's scale; `map->colorScale() == nullptr`, `replot()` runs.
  - `deleteBarsInGroup`: bars in a `QCPBarsGroup`; `delete` one bar and then the group; both directions
    ASan-clean.
  - `polarRemoveGraphUnlistsFirst`: a polar graph's `destroyed` slot calls `removeGraph` on another graph
    of the same angular axis; nothing is deleted twice.
  - `intervalsWithoutLaneLayout`: a `QCPIntervals` given no lane layout, and one whose layout is deleted:
    every public method runs (no crash) and reports one misuse.
- [ ] **Step 2:** Build, run. Expected: each fails or crashes before its fix (some may already pass: then
  the test stays as a guard and the step notes it).
- [ ] **Step 3:** `QPointer` for the tracer's graph; polar `removeGraph` unlists before deleting; intervals
  check the lane layout at each listed site; colour map and bars as the tests require.
- [ ] **Step 4:** Build, run `TestOwnership` (and `build-asan`). Expected: all passed, ASan clean.
- [ ] **Step 5:** Full suite and benchmark. Expected: green, no regression.
- [ ] **Step 6: Commit** — "fix(ownership): tracer, colour scale, bars, polar and intervals relations".

### Task 7: Safe teardown

**Files:**
- Modify: `src/core.{h,cpp}:540-566` (destructor), every plot-level `add*`/`remove*`/`register*` entry point
- Test: `tests/auto/test-ownership/test-ownership.cpp`

**Interfaces:**
- Produces: `bool QCustomPlot::isBeingDestroyed() const`. While true, every plot-level mutator
  (`addGraph`, `addLayer`, `removePlottable`, `removeItem`, `clear*`, `addPlottableGroup`, `setTheme`…)
  does nothing and reports one misuse (`QCP_CHECK_OR_RETURN(!isBeingDestroyed(), …)`).

- [ ] **Step 1: Write the failing tests:**
  - `destroyedSlotAddsDuringTeardown`: a graph's `destroyed` slot calls `plot->addGraph()`; deleting the
    plot does not crash, the add is refused (one misuse recorded), ASan clean.
  - `destroyedSlotDeletesSiblingDuringTeardown`: a graph's `destroyed` slot `delete`s another graph; no crash.
  - `childrenDeletedWhilePlotWhole`: a plottable's `destroyed` slot reads `plot->axisRect()->rect()`; it is
    the same rect as before the deletion started (the plot is still whole).
- [ ] **Step 2:** Build, run. Expected: the first two crash or record no misuse.
- [ ] **Step 3:** Set the flag first in the destructor, then delete groups, plottables and items while the
  layout is still in place (the layout outlives them, so `childrenDeletedWhilePlotWhole` holds), then the
  layout, then the rest; guard the mutators.
- [ ] **Step 4:** Build, run `TestOwnership` (and `build-asan`). Expected: all passed.
- [ ] **Step 5:** Full suite and benchmark. Expected: green, no regression.
- [ ] **Step 6: Commit** — "fix(core): a plot tears down whole; mutators refuse while it is destroyed".

### Task 8: `QCPHandle<T>`

**Files:**
- Create: `src/handle.h`
- Modify: `src/qcp.h`, `src/meson.build` (header install list), the CMake list
- Test: `tests/auto/test-handle/test-handle.{h,cpp}` (class `TestHandle`)

**Interfaces:**
- Produces: `template <typename T> class QCPHandle` (requires `std::derived_from<T, QObject>`):
  - `QCPHandle() = default;`, `explicit QCPHandle(T* object);`
  - `explicit operator bool() const;` (alive), `T* get() const;` (null when dead)
  - `T* operator->() const;`: alive → the object; dead → `qcp::reportMisuse(...)` then
    `qFatal("NeoQCP: call through a handle whose object was deleted")` (spec answer 2).
  - no implicit conversion to `T*` (it would bring dangling raw pointers back: `T* p = h;`, `delete h;`).
    Connecting goes through a free helper,
    `template <typename T, typename Signal, typename... Rest> QMetaObject::Connection qcpConnect(const QCPHandle<T>& h, Signal signal, Rest&&... rest)`,
    which returns an invalid connection for a dead handle.
  - `template <typename U> QCPHandle<U> as() const;` (`qobject_cast`, empty on mismatch).
  - `void remove();`: deletes the object if alive (later milestones route this through the owner).
  - copyable, comparable (`==` on the object).
  Built on `QPointer<T>`. The common per-type operations on handles come in milestone 2.

- [ ] **Step 1: Write the failing tests** in `TestHandle`:
  - `handleTestsFalseAfterDelete`: `QCPHandle<QCPGraph> h(plot->addGraph());` is true; after `delete
    h.get()`, `h` and a copy made before the delete are both false and `get()` is null.
  - `handleOutlivesPlot`: a handle copied into a lambda, the plot deleted, then the lambda runs: false.
  - `handleConnects`: `qcpConnect(h, &QCPAbstractPlottable::selectionChanged, …)` fires when the graph's
    selection changes; on a dead handle it returns an invalid connection.
  - `handleIsNotARawPointer`: `static_assert(!std::is_convertible_v<QCPHandle<QCPGraph>, QCPGraph*>);`
  - `handleAsCast`: `QCPHandle<QCPAbstractPlottable>` of a graph `.as<QCPGraph>()` is true and
    `.as<QCPCurve>()` is false.
  - `handleRemove`: `h.remove()` deletes; `plottableCount()` drops; a second `remove()` is a no-op.
  - `deadHandleReportsMisuse`: with a recording handler installed, `checkAlive()` on a dead handle returns
    false and records one misuse; on a live handle it returns true and records nothing. (`checkAlive()` is a
    private member, `TestHandle` is its friend; `operator->` calls it before `qFatal`, which a test cannot
    survive, so the fatal path itself is not tested.)
- [ ] **Step 2:** Register, build, run. Expected: fails to compile (`handle.h` not found).
- [ ] **Step 3:** Implement `src/handle.h`.
- [ ] **Step 4:** Build, run `TestHandle`. Expected: 7 passed.
- [ ] **Step 5: Commit** — "feat(core): QCPHandle<T>, a typed handle that never dangles".

### Task 9: Deferred signal delivery at the hazard sites

**Files:**
- Create: `src/signal-delivery.h`, `src/signal-delivery.cpp`
- Modify: `src/core.cpp` (`replot`, `draw`, `rescaleAxes`, `removeLayer`; mouse, wheel and key handlers),
  `src/layer.cpp:146-147`, `src/layoutelements/layoutelement-axisrect.cpp:241-242`,
  `src/layoutelements/layoutelement-legend.cpp:888-899`, `src/layout.cpp:826-834`; lift `guarded()` from
  `core.cpp:66-80` into a shared header
- Test: `tests/auto/test-signal-delivery/test-signal-delivery.{h,cpp}` (class `TestSignalDelivery`)

**Interfaces:**
- Produces:
  - `class QCPEmissionScope` (RAII, per plot): `explicit QCPEmissionScope(QCustomPlot* plot);`. Scopes nest;
    when the outermost one closes, the queued emissions run in order, each skipped if its sender died.
  - `class QCPInputScope`: same, but the outermost one posts the flush to the event loop
    (`QMetaObject::invokeMethod(plot, …, Qt::QueuedConnection)`), because Qt forbids deleting a widget inside
    its own event handler.
  - `void QCustomPlot::deliver(QObject* sender, std::function<void()> emission);`: runs now if no scope is
    open, else queues (holding `sender` in a `QPointer`).
  - Rule (spec answer 1): method-caused signals are delivered when the outermost NeoQCP call returns;
    input-caused signals on the next event-loop turn. This milestone converts only the sites below; all
    other emits stay direct until milestone 4.

- [ ] **Step 1: Write the failing tests** in `TestSignalDelivery`:
  - `nestedScopeDeliversAtOutermost`: inside two nested scopes, a `deliver` call runs only when the outer one
    closes, after a flag the outer code sets just before closing.
  - `deadSenderSkipped`: a queued emission whose sender is deleted before the flush does not run.
  - `inputScopeDeliversNextTurn`: inside a `QCPInputScope`, the emission has not run when the scope closes,
    and has run after `QCoreApplication::processEvents()`.
  - `slotDeletesPlotDuringReplot`: slots on `beforeReplot`, `afterLayout` and `afterReplot` (one test each,
    data-driven) `delete` the plot; `replot()` returns and ASan is clean.
  - `replotSignalsWaitForReplotToFinish`: a `beforeReplot` slot records `plot->isReplotting()` (add the
    getter if missing); it records `false`, which fails while the emit is direct, so a direct emit left
    inside `replot` is caught.
  - `slotDeletesAxisDuringRescale`: a `rangeChanged` slot deletes another axis during `rescaleAxes()`; no crash.
  - `slotDeletesPlotOnClick`: a `plottableClick` slot deletes the plot after a simulated mouse press and
    release (`QTest::mouseClick` on a graph's pixel); no crash, and the slot ran after the event returned.
  - `removeLayerWhileIntervalsAddsLayer`: removing a layer holding a `QCPIntervals` (whose `layerChanged` slot
    adds a layer) neither crashes nor loses a child.
- [ ] **Step 2:** Register, build, run. Expected: the scope tests fail to compile; the hazard tests crash
  (confirm under `build-asan`).
- [ ] **Step 3:** Implement the scopes and `deliver`; open a `QCPEmissionScope` at the top of `replot`,
  `draw`, `rescaleAxes`, `removeLayer`, `~QCPLayer`, `~QCPAxisRect`, `QCPLegend::clearItems`,
  `QCPLayout::clear`; open a `QCPInputScope` at the top of the mouse, wheel and key handlers; route those
  sites' emits through `deliver`; loops use guarded snapshots.
- [ ] **Step 4:** Build, run `TestSignalDelivery` (and `build-asan`). Expected: all passed, ASan clean.
- [ ] **Step 5:** Full suite and benchmark. Expected: green, no regression (`QCPAxis_SetRange10k` included).
  Existing tests that assumed a synchronous signal from an input handler are updated to process events once;
  list each in the commit. This is the one sanctioned exception to the "additive only" constraint: the
  maintainer chose input-caused signals on the next event-loop turn (spec answer 1).
- [ ] **Step 6: Commit** — "feat(core): deferred signal delivery at the re-entrancy hazard sites".

### Task 9b: The primitives compose

**Files:**
- Test: `tests/auto/test-signal-delivery/test-signal-delivery.cpp`

- [ ] **Step 1: Write the tests** (data-driven; each must be ASan-clean):
  - `deletePlotFromBeforeReplotWithHandle`: a `beforeReplot` slot deletes the plot while a
    `QCPHandle<QCPGraph>` into it is held; after `replot()` returns the handle is false.
  - `deletedChildSkipsQueuedEmission`: inside a `QCPEmissionScope`, queue an emission for a graph, then delete
    the graph from another graph's `destroyed` slot; the emission is skipped at the flush.
  - `removeInsideInputScope`: `handle.remove()` inside a `QCPInputScope`, then `processEvents()`; no crash,
    the queued input signals for that object are skipped.
- [ ] **Step 2:** Build, run. Expected: pass (if one fails, fix the primitive it exposes, test-first, in this task).
- [ ] **Step 3: Commit** — "test(core): handles, teardown and deferred delivery together".

### Task 10: Close the milestone

**Files:**
- Modify: `changelog.txt`, `docs/plans/neoqcp-3/baseline.md` (append the end-of-milestone numbers)

- [ ] **Step 1:** Full suite in `build-qt612` and in `build-asan`. Expected: green, ASan clean.
- [ ] **Step 2:** Benchmark three times; append the medians to `baseline.md`. Expected: every row within noise
  of the baseline.
- [ ] **Step 3:** Ask opencode for an honest, contradicting review of the milestone's diff
  (`git diff e9daa0c..HEAD -- src tests`); verify each point against the code; fix the real ones test-first.
- [ ] **Step 4:** Changelog entry under a new "3.0 (in progress)" heading: what is now safe to delete, the new
  headers, the deferred delivery rule and which sites use it. Add the `2.x` back-port rule to the changelog
  header: fixes land on the 3.0 line and are cherry-picked to `2.x` with `git cherry-pick -x`; CI for `2.x`
  is set up when the branches are first pushed (the maintainer's call).
- [ ] **Step 5: Commit** — "docs: milestone 1 of the 3.0 object model". Report to the maintainer; no push.
