# NeoQCP 3.0 milestone 6b — construction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** plot types and annotations are built without a plot (`T(args...)`), `add<T>(args...)` places them, built-in
plot types take their data in the constructor, and the short calls `ax->line`, `ax->text`, `ax->ellipse` exist.

**Architecture:** the library already detaches and re-adopts objects (`QCustomPlot::take` / `add(std::unique_ptr<T>)`,
`adopt()`): a plot-free constructor makes an object that is born detached, and every `add<T>(args...)` becomes "build
`T(args...)`, then adopt it into this rect (or plot)". Binding to axes moves from the constructor to adoption.

**Tech Stack:** C++20, Qt 6.12, meson + `cmake/sources.cmake`, QtTest.

**Spec:** `docs/specs/2026-11-14-neoqcp-3-milestone-6-design.md`. Input: `docs/plans/neoqcp-3/m6-gaps.md` (theme 3 rows
"built without a plot", "Range-of-numbers concept"; theme 1 row `xAxis()`; theme 5 rows `line`, `ellipse` / `text`;
decisions 4 and 5) and `docs/plans/neoqcp-3/m6-vocabulary.md` (exact signatures: copy them from there).

## Calls

- **B-1 (decision 4)** `QCPItem` and every built-in item lose their `QCustomPlot*` constructor argument. Positions and
  anchors made in a constructor or a member initialiser (`createPosition`, `createAnchor`) work on a detached item.
- **B-2 (decision 5)** `QCPPlottable`, `QCPPlottable1D` and every built-in plottable lose their `(keyAxis, valueAxis)`
  constructor arguments. A plottable is bound to a system when it is adopted. A system that refuses the plottable's
  capabilities refuses it at adoption: `add` reports once (as today's constructor does) and returns an empty handle,
  and the object is deleted.
- **B-3 (decision 5)** `add<T>(args...)` always means `T(args...)`, on `QCPAxisRect`, `QCPPolarArea` and
  `QCustomPlot`. The forwarding of extra arguments to `setData` (`setDataOrUndo`) goes. `QCPAxisRect::add<T>(key,
  value, args...)` stays for a non-default axis pair, with the same rule.
- **B-4** `QCPAxisRect::add<T>` takes items too: the item is adopted, clipped to the rect, put on the `annotations`
  layer, and its positions that were not given a system take the rect's `xAxis()` / `yAxis()` system.
  `QCustomPlot::add<T>(args...)` for an item keeps today's placement (first rect), with the new construction rule.
  The non-item, non-plottable `Construct<>` specialisations (`QCPBarsGroup`, `QCPPlottableGroup`, ...) are unchanged.
- **B-5 (whole-library rule)** Every built-in plottable gets a data constructor taking what its `setData` takes, with
  `QCPNumberRange` columns where `setData` takes columns of numbers. `QCPPlottable1D<D>(keys, values)` per the
  vocabulary row, inherited by a user type with `using QCPPlottable1D::QCPPlottable1D;`.
- **B-6** `QCPAxisRect::line`, `text`, `ellipse` with `QCPLineOptions`, `QCPShapeOptions` and the existing
  `QCPTextStyle`, signatures from the vocabulary. Positions are today's `QCP::Position` alternatives; `anchor +
  QCP::PixelOffset` is 6c and is not added here. Short calls for the other built-in types (decision 16) are 6f/6g
  (`colorMap`, `fillBetween`) or a later sub-milestone; not here.
- **B-7** No forwarders, no deprecated aliases. Builds and suites on the Coder workspace `neoqcp`; reference pictures
  locally; one build per tree at a time.

## Global Constraints

- Each break goes into `docs/migrating.md`, `changelog.txt` (the milestone 6 entry, before 5f) and one line each in
  `docs/plans/neoqcp-3/sciqlopplots-migration.md`. Never rewrite released changelog sections.
- `cmake/sources.cmake` and `meson.build` stay in sync (`check-sources.py`) if a file is added.
- The `examples/user-code/` targets are not edited and not built in 6b.
- Failing test first for every new behaviour (QtTest under `tests/auto/`).
- Commit with explicit pathspecs; never `subprojects/.wraplock`, `subprojects/jkqtmathtext`, `qt.ini`, `.superpowers`.
  Push only when asked.

## Review Focus

1. An item or plottable used before it is added (setters, positions, anchors, `setData`, `selection()`): no crash, no
   signal into a null plot, no report spam. Test: build each built-in item and plottable detached, call its setters,
   add it, check its state survived.
2. `add<T>` on a rect whose plot is being destroyed, or whose bottom/left axis was removed: empty handle, one report,
   no leak (ASan run).
3. A user type that keeps `QCPPlottable1D`'s data constructor with `using` and also declares its own constructor: both
   reach `add<T>`; the data is sorted by key.
4. A refused plottable (capabilities the system lacks, e.g. a 1D plottable on a polar area that refuses it): exactly
   one report, empty handle, object deleted.
5. An item added to a rect whose positions were given an explicit system on another rect: that system is kept, not
   overwritten by B-4.

---

### Task 1: `QCPNumberRange` and the rect's `xAxis()` / `yAxis()`

**Files:** `src/global.h` (or the header holding other concepts), `src/layoutelements/layoutelement-axisrect.{h,cpp}`,
a test under `tests/auto/`.

- [ ] **Step 1:** Tests: `static_assert(QCPNumberRange<std::vector<double>>)`, `<QVector<int>>`, `<std::array<float,3>>`,
  `<std::views::iota(0, 3)>` type, `!QCPNumberRange<QStringList>`; `rect->xAxis()` / `yAxis()` return the bottom / left
  axis handles, an empty handle once that axis is removed.
- [ ] **Step 2:** Implement per the vocabulary rows. Build + run the test on Coder; whole suite; commit
  `feat(api): QCPNumberRange and QCPAxisRect::xAxis()/yAxis()`.

### Task 2: Items built without a plot (B-1, B-4)

**Files:** `src/items/*`, `src/construct.h`, `src/core.{h,cpp}`, `src/layoutelements/layoutelement-axisrect.h`, every
caller in `src/`, `tests/`, `examples/` (not `user-code/`), `docs/`, `documentation/`.

- [ ] **Step 1:** Tests: a user `QCPItem` subclass with a member-initialised position, built with `T(args)`, added
  with `rect->add<T>(args...)`: clipped to the rect, on `annotations`, position in the rect's system; Review Focus 1
  and 5 for every built-in item; `plot.add<QCPItemText>()` still on the first rect.
- [ ] **Step 2:** Make the constructors plot-free, extend `QCPAxisRect::add<T>` to items, route
  `QCustomPlot::add<T>` for items through `T(args...)` + adoption. Move every call site.
- [ ] **Step 3:** Build, whole offscreen suite on Coder: 0 failed, count ≥ the 6a baseline plus the new tests.
- [ ] **Step 4:** Migration notes and changelog. Commit `refactor(api): items are built without a plot`.

### Task 3: Plottables built without axes, data constructors (B-2, B-3, B-5)

**Files:** `src/plottables/*`, `src/polar/*`, `src/construct.h`, `src/core.{h,cpp}`, the rect header, every caller.

- [ ] **Step 1:** Tests: `rect->add<QCPGraph>(x, y)` with `std::vector`, `QVector`, `std::array` and an iota view;
  the same for each built-in plottable's data constructor; a `QCPPlottable1D<QCPGraphData>` subclass with
  `using QCPPlottable1D::QCPPlottable1D;` gets sorted data; Review Focus 2, 3, 4; `add<T>(key, value, args...)` on a
  second axis pair.
- [ ] **Step 2:** Make the constructors axis-free, bind at adoption, add the data constructors, delete the `setData`
  forwarding. Move every call site (expect ~900).
- [ ] **Step 3:** Build, whole offscreen suite on Coder, then the local `meson test -C build-qt612` (reference
  pictures): 0 failed.
- [ ] **Step 4:** Migration notes and changelog. Commit `refactor(api): plottables are built without axes`.

### Task 4: `line`, `text`, `ellipse` (B-6)

**Files:** a new `src/layoutelements/axisrect-shortcuts.h` (or the rect header if small), `cmake/sources.cmake`,
`meson.build`, a test, `docs/` guide pages that show `add<QCPGraph>` + setters (switch the first example to `line`).

- [ ] **Step 1:** Tests: `line(x, y, {.pen = p, .name = "n"})` gives a graph with that pen and name and the data;
  `.pen = Qt::NoPen` with a scatter style; `text(QCP::AxisRatio{0.05, 0.05}, "t", style)`; `ellipse(a, b, {.brush})`
  with each corner in a different space.
- [ ] **Step 2:** Implement over `add<T>`. Build + whole suite on Coder. Commit `feat(api): ax->line, text, ellipse`.

### Task 5: Review and finish

- [ ] Final review by a fresh reviewer over the milestone range; one fix batch.
- [ ] Whole offscreen suite and whole ASan+UBSan suite (Coder, `ASAN_OPTIONS=detect_leaks=0`), local `meson test`,
  clang `-fsyntax-only` on changed `.cpp` files, examples build, `check-sources.py`, guide snippets.
- [ ] `docs/plans/neoqcp-3/milestone-6b-rulings.md`; memory.
