# NeoQCP 3.0 milestone 5b — handles Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** every public getter that hands out a plot-owned `QObject` returns a handle, the default axes and the legend
are handle-typed members, and creators return handles; 2.x code keeps compiling because a handle converts
implicitly to `T*` (maintainer answers M5-3, M5-5, M5-6).

**Architecture:** `QCPHandle<T>` gains an implicit `operator T*()` (null when dead), a converting constructor from
a handle of a derived class, `qHash` and `QDebug`. Getters change their return type to the class's handle type
(`qcp::HandleFor<T>`, else `QCPHandle<T>`); lists become `QList` of handles. Object-typed `Q_PROPERTY`s keep their
pointer type: moc assigns the getter's result to a `T*`, which the conversion covers. Inside the library, hot paths
read members, never the public handle getters.

**Tech Stack:** C++20, Qt 6.12, meson + `cmake/sources.cmake`, QtTest, `tests/benchmark`.

**Spec:** `docs/specs/2026-11-11-neoqcp-3-milestone-5-design.md` (M5-3, M5-5, M5-6, C8, A5, A6, A7) and the
object-model spec's decision 4 and "Handles in the public API, objects in signals and drawing"
(`docs/specs/2026-11-02-neoqcp-3-object-model-design.md:150-164,331-335`). Facts:
`docs/plans/neoqcp-3/m5b-inventory.md` (every getter with file:line, the exceptions, the properties, the users,
the hot paths). The maintainer said "go ahead on 5b" (2026-11-12) and answered M5-5/M5-6; calls are delegated.

## Calls made in this plan (H-1..H-10; the maintainer can overrule any)

- **H-1 The handle type.** `QCPHandle<T>` gains: `operator T*() const` (implicit; `mObject.data()`, so null when
  dead); a converting constructor `template <typename U> requires std::derived_from<U, T> QCPHandle(const
  QCPHandle<U>&)` (implicit); `qHash(const QCPHandle<T>&, size_t seed)` and `std::hash`; `QDebug operator<<`.
  `explicit operator bool`, `get()`, `as<U>()`, `remove()`, the dead-handle `qFatal` on `operator->` stay.
  `qcpConnect(handle, ...)` is deleted (plain `QObject::connect(h, ...)` now works; NeoQCP-only API, no shim) and
  its users ported. The `explicit` constructor from `T*` stays (making a handle is a deliberate act).
- **H-2 Which getters change.** Every public getter in inventory §2 whose result is a plot-owned `QObject`, including
  parent getters (`parentPlot()`, `parentLayerable()`, `layout()`, `parentLegend()`, `group()`, `barsGroup()`):
  `remove()` on a parent handle removes the parent, which is what it says. Return type: `qcp::HandleFor<T>` when `T`
  has one, else `QCPHandle<T>`. A getter that is virtual keeps one handle type across the hierarchy (the base's).
- **H-3 Exceptions stay raw** (A6, inventory §3.1): the `Q_GADGET`s (`QCPItemPosition`, `QCPItemAnchor`,
  `QCPSelectionDecorator`) — owned by their item/plottable and alive exactly as long; `interface1D()`;
  `QCPColorMapData`; `labelRenderer()` (not owned); the RHI layers and paint buffers (internal). Each gets one line in
  its doc comment saying why it is raw.
- **H-4 Lists.** `QList<T*>` returns become `QList<QCPHandle<T>>` (`plottables()`, `items()`, `graphs()`,
  `axisRects()`, `axes()`, `selected*()`, `children()`, `elements()`, `members()`, `bars()`, `radialAxes()`, the
  axis-rect drag/zoom axis lists, …). Inputs (setters taking lists) keep `QList<T*>`.
- **H-5 Members** `xAxis`, `yAxis`, `xAxis2`, `yAxis2` become `QCPAxis::Handle`/`qcp::HandleFor<QCPAxis>` and `legend`
  the legend's handle; the code that clears them by hand on deletion goes (the handle tests false by itself). They
  stay public members with the same names (M5-6).
- **H-6 Creators** return handles: `addGraph` (the same handle as `add<QCPGraph>(key, value)`, A5), `QCPAxisRect::
  addAxis/addAxes`, `QCPPolarArea::addRadialAxis`, `QCustomPlot::addLayer` if it returns the layer, `QCPLegend::
  addItem`? (keep `bool` returns as they are; only object-returning creators change).
- **H-7 Properties.** Object-typed `Q_PROPERTY`s keep their declared pointer type; READ stays the public getter (moc
  assigns its result to `T*` through H-1's conversion), WRITE stays the `T*` setter. A test reads each one through
  `QObject::property` and checks the pointer. `QCPLayer::children` (`QList<QCPLayerable*>`) gets a private raw READ
  if moc cannot convert the list; say which in the report.
- **H-8 Setters keep `T*` parameters**; a handle converts. No handle overloads (one way).
- **H-9 Inside the library** nothing per frame or per point calls a public handle getter: the inventory §8 hot paths
  read members (`mPlottables`, `mAxes`, `mItems`, `mParentPlot`, …) or a private raw accessor. A/B gates prove it.
- **H-10 Signals stay raw** (object-model spec; C8).

## Global Constraints

- One build or test at a time, in the foreground, `ninja -C build-qt612 -j 4`, timeout 600000, long output to a log
  file and only its tail printed. Env in the same command:
  `PATH=/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin:/home/jeandet/Qt/6.12.0/gcc_64/bin:$PATH`,
  `PKG_CONFIG_PATH=/home/jeandet/Qt/6.12.0/gcc_64/lib/pkgconfig`. Never `meson setup` in the worktree (a /tmp worktree
  is fine, e.g. to build examples with `-Dwith_examples=true` or a base for A/B).
- Offscreen suite; Xvfb whole binary (no leaked Xvfb of ours); ASan on touched classes per task, whole ASan suite
  after the last code change; clang check on every changed `.cpp`; the docs snippets and `doc-snippets` test; the
  examples build (`with_examples`) in a /tmp worktree at the end of each task that touches them.
- Every existing reference picture keeps matching. Bench gate: every drawing, item, axis and multigraph row within 5 %
  of the task's base (A/B, five pairs, ten for a row over); others 15 %.
- Behaviour unchanged: this milestone changes types, not what any call does.
- No shims; misuse via `QCP_CHECK_OR_RETURN`; tests fail before the change where behaviour is new (H-1); type changes
  are pinned by compile-level tests (`static_assert(std::is_same_v<decltype(plot.axisRect()), ...>)`) written first.
- Commit with explicit pathspecs; never `subprojects/.wraplock`, `subprojects/jkqtmathtext`, `qt.ini`. `/tmp` is RAM:
  remove worktrees. Never push.

## Review Focus

1. A handle used where 2.x code had a raw pointer: `auto*` deduction, ternaries mixing handles and pointers,
   template argument deduction, `QList<T*>` comparisons, `qobject_cast` on a getter, `connect`, overloaded functions
   taking `T*` and `bool` (the handle's two conversions).
2. A deleted default axis or legend: `plot->xAxis` tests false, `plot->xAxis->` aborts with the clear message, and
   nothing in the library still dereferences a stale member.
3. Hot paths: no `QPointer` copy per point or per item per frame (QCPItems_*, QCPLegend, axis-rect layout rows).
4. Properties: every object-typed `Q_PROPERTY` still reads/writes through `QObject::property`/`setProperty`.
5. SciQLopPlots: the migration note lists every site of its code that stops compiling (inventory §7).

---

### Task 1: The handle type

**Files:** `src/handle.h` (H-1), every `qcpConnect` user (grep), `tests/auto/test-handle/`.

- [ ] **Step 1: Failing tests** (`TestHandle`): `aHandleConvertsToItsPointer` (alive and dead → null),
  `aDerivedHandleConvertsToABaseHandle`, `handlesHashAndCompare` (`QSet<QCPHandle<QCPAxis>>`, `std::unordered_set`),
  `aHandleConnectsLikeTheObject` (`QObject::connect(h, &QCPAxis::rangeChanged, ...)`), `aHandlePrintsToQDebug`,
  `ifOnAHandleIsNotAmbiguous` (`if (h)` and `bool b{h}` compile and test liveness), and a compile check that
  `QCPHandle<T> h = rawPointer;` still does not compile (a `requires` expression). Expected: FAIL (compile).
- [ ] **Step 2: Implement** H-1; port the `qcpConnect` users to `QObject::connect`.
- [ ] **Step 3:** `TestHandle`, `TestBuilder`, the whole suite offscreen and under Xvfb, ASan on them, clang.
- [ ] **Step 4: Commit** `feat(handle): a handle converts to its pointer, to a base handle, and hashes`.

### Task 2: The plot's getters, members and creators

**Files:** `src/core.{h,cpp}`, `src/construct.h`, and every user that stops compiling (src, tests, benchmark,
docs/snippets, examples, documentation/). Inventory §2.1, §6.

- [ ] **Step 1: Failing compile-level tests** (`TestHandle` or a new `TestHandleGetters`): `static_assert`s that
  `plottable(i)`, `graph(i)`, `item(i)`, `layer(name)`, `layer(i)`, `currentLayer()`, `axisRect(i)`, `axisRects()`,
  `axisRectAt`, `plotLayout()`, `overlay()`, `theme()`, `selectedPlottables/Graphs/Items/Axes/Legends()`,
  `plottableAt`, `itemAt`, `layoutElementAt`, `addGraph`, and the members `xAxis`/`yAxis`/`xAxis2`/`yAxis2`/`legend`
  have the H-2/H-4/H-5/H-6 types; behaviour tests `aDeletedDefaultAxisTestsFalse`, `aDeletedLegendTestsFalse`,
  `addGraphReturnsTheSameHandleAsAdd`. Expected: FAIL.
- [ ] **Step 2: Implement**; port every user; remove the hand-clearing of the members.
- [ ] **Step 3:** whole suite offscreen and under Xvfb, ASan on the touched classes, clang, `doc-snippets`, examples
  build in /tmp. A/B the item, legend and layout rows.
- [ ] **Step 4: Commit** `feat(core): the plot hands out handles`.

### Task 3: Layout, axes and areas

**Files:** `src/layout.{h,cpp}`, `src/layoutelements/*` (axis rect, area, legend, colour scale, text element),
`src/axis/*`, `src/polar/*`, `src/coordinate-system.{h,cpp}`, `src/layer.{h,cpp}`, users. Inventory §2.2, 2.3, 2.5,
2.7-2.10.

- [ ] **Step 1: Failing compile-level tests** for each getter family (axis rect `axis/axes/allAxes/rangeDrag*Axes/
  rangeZoom*Axes/colorScale/insetLayout/plottables/graphs/items`, `addAxis/addAxes`, axis `axisRect/grid`, legend
  `item/itemWithPlottable/parentLegend`, layout `element/elementAt/elements/layout`, layer `children/parentPlot`,
  layerable `parentPlot/parentLayerable/layer`, polar area `angularAxis/radialAxis/radialAxes/grid/addRadialAxis`,
  system `area/keyAxis/valueAxis/plottables`), and `everyObjectPropertyReadsThroughQObjectProperty` (H-7, every
  object-typed property in inventory §4). Expected: FAIL.
- [ ] **Step 2: Implement**; port users; hot paths (inventory §8) read members.
- [ ] **Step 3:** as Task 2 step 3. A/B the item, legend, axis-rect, polar and multigraph rows.
- [ ] **Step 4: Commit** `feat(layout): layouts, axes, areas and layers hand out handles`.

### Task 4: Plottables, items, groups

**Files:** `src/plottables/*`, `src/items/*`, `src/plottables/plottable-group.{h,cpp}`, users. Inventory §2.4, 2.6.

- [ ] **Step 1: Failing compile-level tests**: plottable `keyAxis/valueAxis/keyAxisBase/valueAxisBase/system/group/
  colorScale`, error bars `dataPlottable`, bars `barsGroup/barBelow/barAbove`, bars group `bars`, plottable group
  `members`, tracer `graph/lineGraph`, item `clipAxisRect/clipArea`, position `keyAxis/valueAxis/axisRect/system/
  area` (positions themselves stay raw, H-3), multigraph fill partner getter if any. Expected: FAIL.
- [ ] **Step 2: Implement**; port users; the per-item-per-frame paths (inventory §8, `item.cpp` dependencyRevision,
  extents) read members.
- [ ] **Step 3:** as Task 2 step 3. A/B every item and plottable row.
- [ ] **Step 4: Commit** `feat(plottables): plottables, items and groups hand out handles`.

### Task 5: Docs and finish

**Files:** `docs/concepts.md` (handles: what converts, what stays raw and why, members), `docs/migrating.md`
(5b breaks: `auto*` deductions, list types, `qcpConnect` gone), `changelog.txt` (milestone 5b),
`docs/plans/neoqcp-3/baseline.md`, `backlog.md`, `milestone-5b-rulings.md`, `sciqlopplots-migration.md` (every
SciQLopPlots site from inventory §7 that stops compiling, with the fix).

- [ ] **Step 1:** Final whole-branch review (fresh Opus reviewer and fresh opencode reviewer), one fix batch.
- [ ] **Step 2:** Offscreen suite, Xvfb whole binary, whole ASan suite after the last code change, clang, doc
  snippets, examples build.
- [ ] **Step 3:** Whole bench table against the end of 5a; A/B any row over its gate against a build of the 5b base.
- [ ] **Step 4:** Docs, changelog, rulings, backlog, migration notes.
- [ ] **Step 5: Commit** `docs(neoqcp-3): end of milestone 5b — rulings, baseline, backlog, changelog`.
