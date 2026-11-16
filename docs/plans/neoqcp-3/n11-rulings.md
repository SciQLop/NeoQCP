# NeoQCP 3.0 — n11 rulings and deferred items

n11: any data structure reaches the data calls without the user redefining a class. A user free function
`qcp_numbers(x)` (1D) or `qcp_columns(m)` (2D), found by argument-dependent lookup, adapts their type; a plain
range needs nothing; a deleted `qcp_numbers` stops a 2D type from being flattened. NeoQCP builds as C++23.
Branch `neoqcp-3.0`, `985731d..` the commit that adds this file (not pushed). Plan:
`docs/superpowers/plans/2026-11-16-neoqcp-3-n11-data-adapters.md` (pre-flight rulings P-1..P-12). Implementer:
opencode (DeepSeek V4.1 Flash, auto mode); task, re-review and final reviews by Claude subagents. The range also
carries the 6c CI fix (`61f8418`: libc++ variant size, offscreen QRhi waits past meson's timeout).

## Results

- Coder (GCC 16): whole suite 4233 passed, 0 failed, 153 skipped, third-party adapter test included. Whole
  ASan + UBSan suite 4233/0/153, no sanitizer report (leak checks off: LeakSanitizer does not run under ptrace).
- Local Xvfb auto-tests 4374/0/0, no reference picture moved. `check-sources.py`: the file lists match;
  examples build; `doc-snippets` pass.
- The fallback `std::experimental::mdspan` branch (what GCC 14 compiles) was built and run on GCC 16 through
  `QCP_TEST_EXPERIMENTAL_MDSPAN`. MSVC, Apple clang and GCC 14 have not compiled this range yet: the push is the
  first cross-platform run.

## Rulings

- R-1 (P-11 relaxed): Task 2 started before CI ran on the C++23 switch; a CI failure gets its own fix commit.
- R-2: adapted locals are not const, so a hook result that only iterates as non-const (a `filter_view`) is read.
- R-3: the third-party array test (Eigen 5.0.1, xtensor 0.21.5 + xtl 0.7.7, kokkos mdspan 0.6.0) is behind the
  option `with_third_party_tests`, default false, on in the two Linux CI jobs only: Windows and macOS never
  download or compile a 2020 xtensor. The adapter header itself is compiled everywhere by `TestDataAdapters`.
- R-4: xtl comes from a `[wrap-git]` pinned to the 0.7.7 commit with a `packagefiles` overlay: wrapdb's xtl 0.8.0
  dropped `xvariant.hpp`, which wrapdb's own xtensor 0.21.5 includes. The overlay installs nothing.
- R-5: every number form (graph, waterfall, curve, bars, error bars, financial, statistical box, polar graph,
  `QCPPlottable1D`, `ax->line`) is constrained `QCPNumbersOnly` (a number range that is not columns), so a type
  with only a `qcp_columns` hook is refused rather than flattened and cut. Graded up from the final review's
  Minor: the user would have got silently truncated data.
- R-6: an owned const operand (a const rvalue) is copied into a non-const local; a type that cannot be copied
  makes the constraint false (P-12), not a hard error in the body.
- R-7: only an lvalue reference returned by a hook is read in place; a hook returning an rvalue reference is owned
  like a value (copied or moved before it is read).
- R-8: `QCPGraph::setData` (and `QCPGraphOps::setData`) take any number range; the other plottables' `setData`
  keep their `QVector` forms, and the guide lists exactly the calls that take any range.
- R-9: a hook returning a view over its argument dangles if it is reached through an implicit conversion; the
  library cannot see it, the guide says to write such a hook as a template on the exact type or its base.

## Deferred

- `QCPGraphOps::setData` has no by-value twin: moving vectors through a handle copies them.
- `ax->line(keys, columns)` does not compile; `add<QCPGraph>(keys, columns)` does.
- `setData` of the plottables other than `QCPGraph` takes `QVector` only.
