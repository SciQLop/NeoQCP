# NeoQCP 3.0 n11 — data adapters and C++23 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** any data structure plots without a wrapper class, STL-style: a user free function `qcp_numbers(x)` (or
`qcp_columns(m)` for 2D) found by argument-dependent lookup adapts it, a plain range of numbers needs nothing, and
the library builds as C++23.

**Architecture:** two customization point objects, `qcp::numbers` and `qcp::columns`, modelled on
`std::ranges::begin`. `QCPNumberRange` is redefined as "`qcp::numbers(r)` is valid", so every data call that takes
it today reads through the hook with no change at its call site. A contiguous range of `double` comes back as
`std::span<const double>` and is read in place.

**Tech Stack:** C++23, Qt 6.12, meson (+ `CMakeLists.txt` / `cmake/sources.cmake`), QtTest. Test-only: Eigen,
xtensor (+ xtl), a reference `mdspan` (Kokkos), from meson wraps.

**Spec:** `docs/plans/neoqcp-3/backlog.md` row n11 (maintainer direction 2026-11-15) under
`docs/specs/2026-11-14-neoqcp-3-milestone-6-design.md` (constraints, "any range of numbers goes in").

## Global Constraints

- Branch `neoqcp-3.0`, worktree `~/Documents/prog/NeoQCP-3.0`. Builds and suites on the Coder workspace `neoqcp`
  (`. ~/.config/lpp-coder/env`); reference pictures and clang `-fsyntax-only` locally (`build-qt612`, `-j 4`).
- Never run `meson setup` in the local worktree. One build or test run per build directory at a time, foreground.
- Failing test first for every behaviour change; whole suite green before each commit.
- No traits specialisation, no `tag_invoke`, no wrapper class the user has to define. The library ships no hook
  for any third-party type.
- Breaking changes go into `docs/migrating.md`, `changelog.txt` (unreleased section only) and one line in
  `docs/plans/neoqcp-3/sciqlopplots-migration.md`.
- Commit with explicit pathspecs; never commit `subprojects/.wraplock`, `subprojects/jkqtmathtext`, `qt.ini`,
  `.superpowers`. Never push.

## Review Focus

1. A 2D container that is also a flat range (xtensor 2D, a `std::vector<std::vector<double>>` given where numbers are
   expected): with a deleted `qcp_numbers` it is refused at compile time, never silently flattened.
2. A user hook that returns a temporary view over the user object (Eigen `.array()`, an `mdspan` row): the data call
   reads it before the full expression ends; no dangling read.
3. Integer and `float` element types, and a `std::span<const double>` with an extent: same points as a
   `std::vector<double>` of the same values.
4. An input-only range (a generator, `std::views::istream`): measured once or read once, never read twice.
5. A hook found for a base class (user type derived from an adapted type): ADL finds it, no ambiguity with the
   fallback.

## Pre-flight rulings (2026-11-16, review in the plan workspace `preflight.md`; they override the task text)

- P-1 (B1): the detail-namespace fallback is a C variadic, `NoHook qcp_numbers(...)` (same for `qcp_columns`), used
  only in unevaluated operands. An ellipsis is the worst match, so any user hook (base-class, CRTP, forwarding
  template) wins; a deleted best match still makes the call ill-formed (the opt-out).
- P-2 (B2, C6): `cpp_std=c++23,c++latest`, `meson_version >= 1.3.0`; GCC 14 on every Linux CI job.
- P-3 (B3): Task 3 tests pass keys as `std::vector<double>{0, 1, 2}`.
- P-4 (C1): the concepts check the const object (`requires(const R& r) { qcp::numbers(r); }`), matching the
  `const K&` consumers. Ranges that are not const-iterable (generators, `views::istream`, `filter_view`) are out,
  said in the docs; Review Focus 4 becomes "a non-common range (`take(iota(0.0), n)`) is read once".
- P-5 (C2): every `QCPNumberRange` consumer reads through `qcp::numbers` / `qcp::toDoubles`, including
  `QCPPlottable1D(keys, values)` (no `std::ranges::distance` pre-pass on a non-sized range). `toDoubles` takes
  non-common ranges.
- P-6 (C3): for an rvalue that is not a borrowed range (including a hook returning a container by value), rule 3
  returns `std::views::all(std::move(x))` (an `owning_view`), never a span. Test: a hook returning
  `std::vector<double>` by value, through `QCPGraph`, under ASan.
- P-7 (C4): the numbers forms are constrained `&& !QCPColumns<V>`, so a type with a columns hook is 2D. Same in the
  6f `colorMap` pair.
- P-8 (C5): no `std::views::stride` anywhere (libc++ age); strided columns are `iota | transform`. Floor stays GCC 14,
  Clang 18 / Apple Clang 16, MSVC 17.10.
- P-9 (C7): wraps from wrapdb where they exist, else `[wrap-git]` pinned to a release with `method = cmake` or a
  `packagefiles` overlay; xtensor and xtl pinned as a matching pair; third-party includes `include_type: 'system'`.
- P-10 (C8): drop `anIntVectorConverts` and the deref-count form of `eachElementIsReadOnce`. Instead:
  `aNonCommonRangeIsRead` (`std::views::iota(0.0) | std::views::take(5)` as keys), `aHookedTypeReachesEveryPlotType`
  (a hooked type through graph, curve, bars, error bars, financial, statistical box, polar graph, waterfall,
  `QCPPlottable1D`), and `copiesOnce` checked by storage identity or an allocation count.
- P-11 (C9): CI green on all five platforms after Task 1 is a gate before Task 2 (the push is the maintainer's).
- P-12 (C10): an invalid hook result makes the CPO's constraint false (the concept is false, the error is the usual
  "constraints not satisfied"); `static_assert` with a readable message only inside consumer bodies.
- Minors M1-M7: the implementer reads them in `preflight.md` and applies them where they touch its task.

---

### Task 1: C++23

**Files:**
- Modify: `meson.build:3` (`cpp_std=c++23`), `CMakeLists.txt:10` (`CMAKE_CXX_STANDARD 23`),
  `.github/workflows/tests.yml` and `cmake.yml` (Linux jobs build with GCC 14: `CC=gcc-14 CXX=g++-14`, installing
  `g++-14` if the image lacks it), `README.md` / `docs/` build requirements, `changelog.txt`, `docs/migrating.md`.

- [ ] **Step 1:** switch both build systems to C++23. An existing build directory regenerates on its own at the next
  `ninja`/`meson compile`; do not run `meson setup`.
- [ ] **Step 2:** whole suite on Coder (`meson test -C build --num-processes 16`), whole `auto-tests` under
  `xvfb-run -a -s "-screen 0 1600x1200x24" env QT_QPA_PLATFORM=xcb`, local `build-qt612` pictures, clang
  `-fsyntax-only` on every `src/*.cpp` from `build-qt612/compile_commands.json` with `-std=c++23`.
  Expected: same totals as before the switch, 0 failed. Fix any C++23 break (removed or changed library behaviour)
  at its root.
- [ ] **Step 3:** requirements line: GCC 14, Clang 18 / Apple Clang 16, MSVC 17.10 (Visual Studio 2022). Use only
  language and library features those three ship; `std::mdspan` is not one of them (GCC 14 lacks it) and the
  library does not use it.
- [ ] **Step 4:** commit `build: C++23`. CI on every platform needs a push: the controller asks the maintainer.

### Task 2: `qcp::numbers` and `QCPNumberRange`

**Files:**
- Create: `src/data-adapters.h` (included by `src/global.h`, which keeps the `QCPNumberRange` doc), add to
  `cmake/sources.cmake` and meson header lists.
- Modify: `src/global.h:83-94`; the data calls that copy twice (`QCPGraph(keys, values)`,
  `src/plottables/plottable-graph.h:58-64`, and any other `QCPNumberRange` consumer that builds an intermediate
  vector before its own storage).
- Test: new class `tests/auto/test-data-adapters/` (`TestDataAdapters`), registered like the other auto-test classes.

**Interfaces:**
- Produces:
  - `namespace qcp { inline constexpr /*unspecified*/ numbers; }`: `qcp::numbers(x)` returns
    1. `qcp_numbers(x)` found by ADL when that call is valid; its result must itself pass rule 3
       (`static_assert` with a message naming `qcp_numbers` otherwise);
    2. ill-formed when the best `qcp_numbers` overload is deleted (the user's opt-out);
    3. otherwise, for an input range whose elements convert to `double`: `std::span<const double>` when the range is
       contiguous and its value type is `double` (any extent), else `std::views::all(x)`.
  - `template <typename R> concept QCPNumberRange = requires(R& r) { qcp::numbers(r); };`
  - `template <QCPNumberRange R> auto qcp::toDoubles(const R& r) -> std::vector<double>`: one pass, one allocation
    when the range is sized.
- Algorithm (rule 1 vs 2 vs 3 is not determined by a plain `requires`): in the CPO's detail namespace declare a
  fallback `template <typename T> NoHook qcp_numbers(const T&);` visible to the unqualified call. The call then
  always resolves: a user non-template or more specialised overload wins and is used (or, if deleted, makes the call
  ill-formed: rule 2); with no user overload it picks the fallback, whose `NoHook` return selects rule 3.

- [ ] **Step 1: failing tests** in `TestDataAdapters`:
  - `aVectorIsReadInPlace`: `qcp::numbers(std::vector<double>{1, 2, 3})` on an lvalue is a `std::span<const double>`
    whose `data()` is the vector's.
  - `anIntVectorConverts`: a `std::vector<int>{1, 2, 3}` graph has keys 1, 2, 3.
  - `aHookAdaptsAUserType`: a test-local `struct Samples { double v[4]; }` with a free
    `std::span<const double> qcp_numbers(const Samples&)`; `QCPGraph(Samples, Samples)` has 4 points.
  - `aDeletedHookRefusesARange`: a test-local `struct Grid : std::vector<double> {}` with
    `void qcp_numbers(const Grid&) = delete;`: `static_assert(!QCPNumberRange<Grid>)`, and
    `static_assert(QCPNumberRange<std::vector<double>>)`, `static_assert(!QCPNumberRange<QStringList>)`.
  - `aHookOnABaseIsFound`: a type derived from `Samples` uses its hook (Review Focus 5).
  - `eachElementIsReadOnce`: a test range whose iterator counts dereferences; `QCPGraph(keys, values)` from two
    such ranges of 1000 elements reads each element exactly once (Review Focus 4, and the copy-twice minor).
- [ ] **Step 2:** run `QCP_TEST_CLASS=TestDataAdapters` on Coder. Expected: does not compile (no `qcp::numbers`).
- [ ] **Step 3:** implement `src/data-adapters.h` as in Interfaces; move every `QCPNumberRange` consumer that
  copied into an intermediate container onto `qcp::numbers` / `qcp::toDoubles` so it copies once into its own
  storage.
- [ ] **Step 4:** the class passes; whole suite on Coder green.
- [ ] **Step 5:** commit `feat(api): qcp::numbers adapts any data`.

### Task 3: `qcp::columns`

**Files:**
- Modify: `src/data-adapters.h`; `src/plottables/plottable-graph.h` (multi-column constructor and `setData`).
- Test: `TestDataAdapters`.

**Interfaces:**
- Consumes: `qcp::numbers`, `QCPNumberRange` (Task 2).
- Produces:
  - `qcp::columns(m)`: `qcp_columns(m)` by ADL when valid (a range whose elements pass `QCPNumberRange`); ill-formed
    when the best overload is deleted; otherwise `m` itself when it is a range of `QCPNumberRange` elements. Same
    fallback mechanism as Task 2.
  - `template <typename M> concept QCPColumns = requires(M& m) { qcp::columns(m); };`
  - `QCPGraph(const K& keys, const C& columns, bool alreadySorted = false)` and the matching `setData`, for
    `QCPNumberRange K, QCPColumns C`: one component per column, each copied once. The existing
    `QCPGraph(KC&&, std::vector<VC>&&)` that moves its storage into the data source stays and wins for an rvalue
    `std::vector<VC>` (it is more specialised); `IndexableNumericRange` stays as the concept of storage a data
    source takes over (ruling: storage, not reading, so not the same idea as `QCPNumberRange`).

- [ ] **Step 1: failing tests:**
  - `aVectorOfVectorsGivesComponents`: `QCPGraph({0, 1, 2}, std::vector<std::vector<double>>{{1, 2, 3}, {4, 5, 6}})`
    from an lvalue has 2 components; component 1, point 2 has value 6.
  - `aColumnsHookAdaptsAMatrix`: a test-local row-major `struct Matrix { int rows, cols; std::vector<double> v; }`
    with a `qcp_columns` returning a range of strided column views (`std::views::stride` over `v | drop(c)`); the
    graph's components equal the columns.
  - `aNumbersHookDoesNotMakeColumns`: `static_assert(!QCPColumns<std::vector<double>>)`.
- [ ] **Step 2:** run; Expected: does not compile.
- [ ] **Step 3:** implement.
- [ ] **Step 4:** class passes; whole suite green.
- [ ] **Step 5:** commit `feat(api): qcp::columns adapts 2D data`.

### Task 4: third-party arrays

**Files:**
- Create: `subprojects/eigen.wrap`, `subprojects/xtensor.wrap`, `subprojects/xtl.wrap`, and a reference `mdspan`
  wrap (`meson wrap install <name>` from wrapdb where it exists; otherwise a `[wrap-git]` on
  `kokkos/mdspan` pinned to a release tag); `tests/third-party/test-third-party-arrays.cpp` and its own
  executable in `tests/meson.build`, built only when all three dependencies are found (`required: false`, wrap
  fallback allowed). Not part of `auto-tests`, so a missing dependency never blocks the main suite.
- Modify: `docs/` data guide page that documents `setData` / data constructors (find it): a section "Your own data
  types" with the two hooks, the opt-out, and an Eigen example as a compiled doc snippet if the snippet test can
  see Eigen, prose otherwise.

**Interfaces:**
- Consumes: Tasks 2-3.

- [ ] **Step 1: tests** (each through `ax->line(...)` or `QCPGraph` and compared with the same values given as
  `std::vector<double>`):
  - Eigen: `Eigen::VectorXd`, `Eigen::VectorXf`, a `segment()` and a column `block()` of a `MatrixXd`, all as plain
    ranges with no hook; a `MatrixXd` given to `QCPColumns` through a test-local `qcp_columns` hook in namespace
    `Eigen`.
  - xtensor: `xt::xtensor<double, 1>`, an `xt::view` with a step; a 2D `xt::xtensor<double, 2>` refused as numbers
    by a test-local deleted `qcp_numbers` (Review Focus 1) and accepted as columns by a `qcp_columns` hook.
  - mdspan: rows of a 2D `mdspan` (`std::mdspan` when `__cpp_lib_mdspan`, else `Kokkos::mdspan`) through a
    `qcp_columns` hook; the hook returns views built on the spot (Review Focus 2: run under ASan).
  - a `std::views::stride` view and a `std::views::transform` view of `float` (Review Focus 3).
  - Armadillo: out (needs BLAS/LAPACK in the image); one line in the report.
- [ ] **Step 2:** build and run the executable on Coder, then once under the ASan tree. Expected: all pass, no
  sanitizer report.
- [ ] **Step 3:** docs; whole suite and `doc-snippets` green.
- [ ] **Step 4:** commit `test: third-party arrays through the data hooks`.

### Task 5: Review and finish

- [ ] Final review by a fresh reviewer over the plan's range; one fix batch.
- [ ] Whole offscreen suite, whole ASan+UBSan suite (`ASAN_OPTIONS=detect_leaks=0`), Xvfb `auto-tests`, local
  pictures, clang `-fsyntax-only`, examples, `check-sources.py`, `doc-snippets`.
- [ ] `docs/plans/neoqcp-3/n11-rulings.md`; backlog row n11 marked done; memory.
