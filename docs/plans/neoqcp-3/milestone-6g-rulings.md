# NeoQCP 3.0 — milestone 6g rulings and deferred items

Milestone 6g: fill and zoom indicator. `QCPFillBetween` (two lines or components, two brushes by side, cut at exact
crossings, broken at gaps, following both lines' data) made by `QCPAxisRect::fillBetween` + `QCPFillOptions`
replaces `QCPGraph::setComponentFillBetween` / `setComponentFillToGraph`; `QCPItemZoomIndicator` made by
`QCPAxisRect::zoomIndicator(inset, QCPShapeOptions)` draws the box over an inset's ranges and the two connectors
(matplotlib's rule); deleting a rect deletes the plottables and items drawn in it. Targets `user-code-fill-between`
and `user-code-inset-zoom` build. Branch `neoqcp-3.0`, `9e2d62fc..` the commit that adds this file (not pushed); the
range also carries `00d94f59` (colour bar spans the data area, from the 6d follow-up). Plan:
`docs/superpowers/plans/2026-11-19-neoqcp-3-milestone-6g.md` (P-1..P-7 approved by the maintainer). Spec:
`docs/specs/2026-11-14-neoqcp-3-milestone-6-design.md` stage 2 item 7, `m6-gaps.md` decision 1. Implementer:
opencode (DeepSeek V4.1 Flash, auto mode); task, re-review and final reviews by Claude subagents (opus).

## Results

- Coder (GCC): `build` (with examples), `build-asan` (ASan + UBSan, leak checks off under ptrace) and `build-fco`
  each 4527 passed, 0 failed, 164 skipped (the skips need a QRhi), no sanitizer report.
- Local Qt 6.12 under Xvfb: whole `auto-tests` 4691/0/0. Reference pictures added: `fill-between-target`,
  `inset-zoom-target`; guide pictures redrawn in Task 2 (legend swatch, channel fill), all looked at.
- Benchmark `QCPGraph_FillToGraphPan10k`: flat before and after the final fix (a pan does not remake the band; a
  zoom remakes it from the window, tested by `aZoomedFillRecordsOnlyItsWindow`).
- Reviews: Task 1 one fix round + one re-review (tie-start side); Task 2 two fix rounds; Task 3 three fix rounds
  (reversed parent axes, `setInset` refusing a foreign rect, inset move heard, rect teardown surviving slots that
  delete the plot or the rect, two-inset and axis variants). Final review: 3 Important + 11 Minor; one fix pass
  (6 commits), each behaviour fix with a failing test first.

## Rulings

- R-1 (Task 1): a stepped fill splits at crossings after shaping (bin, step), so colours swap where the drawn step
  lines swap. Cost if wrong: one reorder in `draw()`.
- R-2 (Task 1): binning runs per run after the split (binning erases the side) — accepted from the implementer;
  the final pass then joined each side's runs into one series so a fill records at most one command per brush.
- R-3 (Task 2): `canFillTo` / `fillsAcrossValueAxes` leave `QCPGraph` for the fill side (`qcp::fill`). Cost if
  wrong: one move back.
- R-4 (Task 2): fills on user coordinate systems keep working (the implementer had deleted three tests and two
  pictures' bands); restored byte-identical. A user system whose axes are not `QCPAxis` (polar) cannot take a fill,
  said in `docs/graphs.md`.
- R-5 (Task 3): a rect owns what is drawn in it: deleting a `QCPAxisRect` deletes the plottables and the items
  whose positions name it, its axes or its systems (narrowed in the final pass, R-7). Insets and axes are taken
  down one at a time, checking the plot between each, so a `destroyed()` slot may delete the plot or the rect.
  Cost if wrong: a plottable a user expected to outlive its rect (it could not draw anyway).
- R-6 (final, I-3): a later `setData` that drops a column the fill uses is a data change, not misuse: the band
  draws nothing, silently, like a line's own missing column, and comes back with the column. Creation checks stay.
  Cost if wrong: a band that vanishes without a warning.
- R-7 (final, M-1): pixel-only items that only clip to a rect survive its deletion (`plotLayout()->clear()` keeps
  a watermark, as in 2.x). Cost if wrong: an item clipped to a gone rect stays.
- R-8 (final): minors that break the standing rules were regraded Important and fixed: one teardown mechanism
  (M-2), no forwarders (M-3), handles in and out of the new API (M-4), small functions (M-5), a hidden line hides
  its band (M-6), wrong doc sentences (M-7, M-8, M-9, M-11).

## Deferred

- `graph-fill` is no longer a pure geometry module (it includes the graph, the system and the draw context for the
  fill rules); a small `fill-rules.h` would keep the geometry pure.
- A grid line falls on the data rect's top/right edge when a range ends on a tick (visible on the inset of
  `inset-zoom-target`); matplotlib shows no grid by default. Revisit in the examples migration.
- A fill band is no longer tinted by its line's selection (said in `docs/migrating.md`).
- Test gaps: two leftover insets after a plot-deleting slot; the axis teardown RED was a revert check.
