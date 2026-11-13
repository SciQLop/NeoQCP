# NeoQCP 3.0 milestone 5d — the rest of the old API Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** remove what milestone 4 kept only for 2.x compatibility and that milestones 5a-5c did not: the
plottables' painter path (every plottable records, its legend icon included), `QCPMarginGroup` (alignment groups
gain per-side membership to replace it), the protected 2.x axis projections on plottables and item positions, the
span items' never-drawn pen, and caller-error `qDebug` reports in the functions this milestone touches.

**Architecture:** deletions with their users ported. The painter virtuals that milestone 4 answer 7 keeps for
internal layerables stay: axes (`QCPAxisBase` and user axes), grids, legend and its rows (`QCPPlottableGroupLegendItem`
included), layout elements, `QCPArea`'s background, `QCPColorScaleAxisRectPrivate`, the selection rect,
`QCPItemCreationState`, the overlay, the top level, the intervals' label layer. On plottables,
`draw(QCPPainter*)` becomes `final` (as on items, `item.h:375`) and `drawLegendIcon(QCPPainter*, const QRectF&)`
becomes `virtual … final`, both replaying the recording.

**Tech Stack:** C++20, Qt 6.12, meson + `cmake/sources.cmake` (`python3 tests/cmake/check-sources.py` must print "the
file lists match"), QtTest, `tests/benchmark`.

**Spec:** `docs/specs/2026-11-11-neoqcp-3-milestone-5-design.md` (C6, C7, C9; A10, A11). Facts and complete lists:
`docs/plans/neoqcp-3/m5d-plan-review.md` (pre-flight review by a fresh Opus reviewer at `d4d1a8a`; its findings are
cited below as B1-B2, I1-I10, M1-M7 and are part of this plan). Older facts: `m5-inventory.md`. Rulings so far:
`milestone-5a/5b/5c-rulings.md`, `milestone-4d-rulings.md` (alignment groups), `milestone-4i-rulings.md` R-2
(one-axis positions).

## Calls (D-1..D-9; the maintainer answered D-3a, the others are delegated)

- **D-1 Recorded legend icons** (C6, A11, I3): statistical box and intervals record native `rect` commands with an
  invisible bounding rect (the bars precedent, `plottable-bars.cpp:1152-1159`). Financial (clip regions, no
  antialiasing) and error bars (fixed pixel offsets, cached recording without the rect in its key) record the
  bounding rect plus a `painter()` callback, so they stay pixel-identical. Before the port, failing-first reference
  pictures for the branches with none: financial OHLC, financial single colour, error bars with a key error, error bars
  on a horizontal value axis.
- **D-2 Final painter entries** (C6, A11, I1, I2, M2): `draw(QCPPainter*)` is `final`; `drawLegendIcon(QCPPainter*,
  const QRectF&)` is `virtual … final` (a 2.x override fails to compile instead of being hidden). No compile-failure
  test (C++ cannot express it; the item's `final` had none either). The 1D nearest-point fallback, `recordsTheDrawing`
  and the forwarding `QCPAbstractPlottable1D::selectTest` go; `TestDrawable::aPainterOnly1DPlottableKeepsThePointClick`
  and `PainterOnlyDots` go with them. Overriding tests: per I4 (`test-data-swap`, `test-repaint` stubs drop their
  overrides; `FillStub` records `rect(AxisRatio{0,0}, AxisRatio{1,1})` and its translate tests must still test the
  translate path). Stale comments of M1 are fixed.
- **D-3a Per-side alignment membership** (maintainer, 2026-11-13: "Per-side membership"): an element may join one
  alignment group per side; `QCPAlignmentGroup::create(members, sides)` keeps its signature; joining a group for a
  side leaves only that side's previous group. This partly reverses 4d P-7. Failing tests first: the four mixed-side
  layouts of B2 expressed as alignment groups, each asserting the edges a margin group gave (measured on the current
  margin-group build first, so the expected numbers are real).
- **D-3 `QCPMarginGroup` goes** (C7, A10, I7): the class, `setMarginGroup/marginGroup/marginGroups`, the auto-margin
  branch, and every user in the I7 list (library, docs comments and `.dox`, the colour-scale `\snippet` tag, tests,
  manual test, snippets, doc pages, doc image generator incl. `genMarginGroup`, doc-code-snippets, the postprocess
  script, `examples/plots`). Tests that align are ported; `alignmentWinsOverMarginGroup` and the `marginGroup()`
  `static_assert` in `test-handle` are deleted (their subject goes). Each ported picture is checked after a single
  render (M6), and a moved picture is listed and looked at by the controller.
- **D-4 Protected projections become private** (C7, A10, B1, I5, M4, M7): `QCPAbstractPlottable` and
  `QCPItemPosition` keep one private raw axis pair (and the position its axis rect). With a cartesian system it is that
  system's projection; with no system it is the unresolved pair (a one-axis position, 4i R-2; a half-set plottable
  pair). Only `setSystem`/`setAxes`/`resolveAxes` (and the position's equivalents) write it. `keyAxis()`/`valueAxis()`
  return it as a handle; library code reads `keyAxisRaw()`/`valueAxisRaw()`/`axisRectRaw()` (the 5b hot-path rule; no
  `qobject_cast` per call). Out-of-tree subclasses port to the protected raw accessors or `system()` (migration row,
  `docs/writing-a-plot-type.md`). Same-name members of other classes (M4) are out of scope.
- **D-5 The span pen goes** (C7, A10, I8): the `pen`/`selectedPen` `Q_PROPERTY` lines, the accessors, `mainPen()`, the
  members, the constructor calls and every user in the I8 list. The generic fluent `QCPItemOps::setPen` stays; called
  on a span handle it no longer compiles (migration row).
- **D-6 Misuse** (C9, I6): in the functions a task changes, a `qDebug` that reports a caller error (a setter or
  argument check) becomes `QCP_CHECK_OR_RETURN`/`reportMisuse`. "Invalid axis" reports on drawing, range and hit paths
  are states (a plottable without a system is inert on purpose) and are deleted or stay `qDebug`. Each task's report
  lists every line it converted or kept.
- **D-7 Builds and checks** as 5c: Claude subagents implement; local builds `-j 4` one at a time; heavy builds, the
  ASan+UBSan whole suite and the bench on the Coder workspace `neoqcp`; reference pictures checked locally;
  `check-sources.py` green; every commit builds.
- **D-8 CI:** the branch CI (Tests, CMake) is watched after each push; a CI failure is a finding of its milestone.
- **D-9 SciQLopPlots** (I10): `SciQLopTimeColoredCurve` (painter override, reads `mKeyAxis`) and `SciQLopPlot.cpp:141`
  (`QCPMarginGroup`) stop compiling; `sciqlopplots-migration.md` says how to port each. SciQLopPlots itself is not
  touched.

## Global Constraints

- One local build at a time, foreground, `-j 4`, `free -g` first; env
  `PATH=/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin:/home/jeandet/Qt/6.12.0/gcc_64/bin:$PATH`,
  `PKG_CONFIG_PATH=/home/jeandet/Qt/6.12.0/gcc_64/lib/pkgconfig`. Never `meson setup` in the worktree.
- Added or removed sources in `meson.build`, `cmake/sources.cmake` and `src/qcp.h`.
- Offscreen suite, Xvfb whole binary, remote ASan+UBSan whole suite after the last change, clang, guide snippets,
  examples (remote).
- Reference pictures keep matching; moved and new pictures are listed and looked at by the controller.
- Bench: remote A/B against the 5d base for the rows a task reaches (5 % drawing/item/axis/layout, 15 % others).
- No shims; failing tests first where behaviour is new; never bend library behaviour to please a test.
- Commit with explicit pathspecs; never `subprojects/.wraplock`, `subprojects/jkqtmathtext`, `qt.ini`. Push only when
  asked.

## Review Focus

1. Legend icons of the four classes look the same, including the branches that had no picture (OHLC, single colour,
   key error, horizontal value axis), and a plottable group's row shows them.
2. The mixed-side layouts align as before: gallery joint density, `QCPColorScale.png`, `examples/plots` advanced axes,
   the manual test — after one render, not after a settle.
3. Axis state the system does not hold survives: one-axis positions (`axistag.cpp`, `test-item-system`) and half-set
   plottable pairs (`test-errorbar-geometry:312`, `test-qcpbars:547`, `test-qcpaxisrect:237`).
4. Hit testing of bars, curve and polar graph with an empty or pending recording, once the 1D fallback is gone.
5. Bench rows reading axes per point or per item (plottable coordsToPixels, bars, items) — no `qobject_cast` per call.

---

### Task 1: Recorded legend icons and final painter entries (D-1, D-2, D-6)

- [ ] Failing-first pictures for the four uncovered icon branches (D-1); `theLegendIconIsRecorded` per class.
- [ ] Port the icons; make the entries final; delete the fallback and its test; port the I4 tests; fix M1 comments.
- [ ] Suites, pictures, ASan on the touched classes; commit `refactor(plottables): every plottable records, its legend
  icon too`.

### Task 2a: Per-side alignment membership (D-3a)

- [ ] Measure the four B2 layouts with margin groups; write their alignment-group tests (failing).
- [ ] Implement per-side membership; existing alignment-group tests stay green; bench layout rows.
- [ ] Commit `feat(layout): an element joins one alignment group per side`.

### Task 2b: `QCPMarginGroup` goes (D-3, D-6)

- [ ] Port every I7 user to alignment groups (pictures unchanged after one render), then delete the class and branch.
- [ ] Commit `refactor(layout): QCPMarginGroup is gone; alignment groups replace it`.

### Task 3a: Plottable projections private (D-4, D-6)

- [ ] Failing tests: half-set pairs keep their axes; getters answer from the cache; a user-system plottable answers
  null. Port the ~236 reader lines (I5 table) to the raw accessors.
- [ ] Remote bench A/B of plottable rows; commit `refactor(plottables): the 2.x projections are private`.

### Task 3b: Item-position projections private (D-4, D-6)

- [ ] Failing tests: one-axis positions keep their axis (4i R-2). Port the `item.cpp` readers.
- [ ] Remote bench A/B of item rows; commit `refactor(items): the 2.x projections are private`.

### Task 3c: The span pen goes (D-5, D-6)

- [ ] Delete per the I8 list; commit `refactor(items): spans have no pen`.

### Task 4: Review and finish

- [ ] Final review by a fresh Opus reviewer; one fix batch.
- [ ] Whole suites, remote ASan+UBSan, clang, snippets, examples, `check-sources.py`; CI green after the push.
- [ ] `docs/migrating.md`, `docs/writing-a-plot-type.md`, `changelog.txt`, `baseline.md`, `backlog.md`,
  `milestone-5d-rulings.md`, `sciqlopplots-migration.md` (D-9).
- [ ] Commit `docs(neoqcp-3): end of milestone 5d — rulings, baseline, backlog, changelog`.
