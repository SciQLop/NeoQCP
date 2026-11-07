# NeoQCP 3.0 — milestone 4b rulings and deferred items

Milestone 4b: the line family. QCPMultiGraph, QCPWaterfallGraph and QCPCurve share one line base
(QCPAbstractLine) and draw through the drawing API. Branch `neoqcp-3.0`, commits `65a6efe..e8a3973`
plus this file (local, not pushed). Plan: `docs/superpowers/plans/2026-11-06-neoqcp-3-milestone-4b.md`.
Spec: `docs/specs/2026-11-06-neoqcp-3-milestone-4-design.md` (M4b). Inventory: `m4b-inventory.md`.
Benchmarks: `baseline.md` ("M4b start" and "End of milestone 4b").

## Results

- At `e8a3973`: offscreen suite 1948 passed, 0 failed, 129 skipped; ASan the same, no sanitizer
  report; Xvfb whole binary 2077/0/0; clang `-fsyntax-only` passes on every changed `.cpp` file.
- src/: 34 files, +2714 / −3042. The curve lost about 1 300 lines of clipping code.
- Bench: every row inside its gate except the 1M-point multigraph zoom (accepted, R-6) and two rows
  that are one rounding step over and show no change in an A/B against `65a6efe` (see baseline.md).
- The curve pan went from 0.40 to 0.12 ms: it records itself once and a pan moves it.

## Rulings made during execution (what, why, cost if wrong)

- R-1 (Task 1): gates for the new bench rows are the lower median of two series (0.15 / 0.61 /
  0.13 / 0.40 ms, SetData1M 38 ms); the implementer measured at a high load. Cost: a gate a
  little strict.
- R-2 (Task 2): R3 adopted: the rows stay within 5 % and a recording is three times smaller.
- R-3 (Task 4): a coloured polyline command keeps "segment k takes vertex k's colour"; the
  multigraph builds its colours as `colours[k] = mapper(indices[k+1])`, which keeps today's pixels.
  Cost: coloured lines would shift by one segment (the colour tests catch it).
- R-4 (Task 4): a markers command holds its style through a shared pointer, so the command variant
  stays 96 bytes. Cost: one indirection per markers command.
- R-5 (Task 5): a pen, brush or marker style set through setPen/setBrush/setScatterStyle is
  remembered and given to components that arrive later (2.x code calls setPen before setData).
  Cost: a later, wider source no longer gets palette colours once setPen was called.
- R-6 (Task 5): colour values of the wrong length are refused as misuse; 2.x cleared them silently.
  Cost: a caller that relied on the silent clear now reaches the misuse handler.
- R-7 (Task 5): the "hidden by error bars" flag stays in the shared component struct. Cost: one
  unused flag on the curve.
- R-8 (Task 6): the 1M-point zoom row is accepted at 0.68 ms against a 0.61 gate (+11 %). Each
  zoom step extrudes about 2 150 points per column against 1 907 before; pans are flat. Cost:
  0.07 ms per zoom frame on a 1M-point multigraph.
- R-9 (Task 6): tests for the port were written after it; their red was the compile failure on the
  old class. Cost: a test that never failed on behaviour.
- R-10 (Task 7): error bars re-record on every pan: their caps are a pixel width and the choice
  between bars and a band is a pixel question. They draw on reduced rows or small raw data. Cost: a
  slower pan on a huge raw graph with sparse bars.
- R-11 (Task 7): two geometry helpers left without a caller were deleted with their 8 tests.
- R-12 (Task 8): the changelog got its milestone 4b section in Task 8; Task 10 extended it.
- R-13 (Task 9): a selected part of a filled curve keeps the curve's brush. 2.x used the selection
  decorator's brush, which by default is none, so a selected part lost its fill. The fill is one
  polygon of the whole curve. Cost: split the polygon by selection.
- R-14 (Task 9): the curve's setPen returns early on an equal pen; the shared base never does,
  because components can be repenned behind its back, which a one-component curve cannot be.
- R-15 (Task 10): QCPGraph's deprecation note sits in its class documentation in the .cpp, where
  that documentation lives.
- R-16 (final review): a recording made from the reduced data claims a key window three views wide,
  while the reduced data covers only the view. A pan inside the window shows blank edges until the
  150 ms settle, as before this milestone. Only an empty view is remade on every frame. Reducing
  the whole window on every settle would cost the GUI thread three times as much. Cost: blank
  edges during a long pan of a big graph.
- R-17 (final review): the data sources' pixel line API (`getLinesAll`, `getOptimizedLineDataAll`
  and the indexed forms) stays. It is public interface and only tests use it now; pruning it
  belongs with the data-source work that retires QCPGraph2. Cost: dead weight in the interface.

## Found and fixed in review

- Task 7: opencode doubted error bars on a vertical key axis; a pixel test shows they turn with
  the axes (no change needed).
- Task 8: turning the deferred swap off while large staged data was still being prepared dropped
  the colours set for it.
- Task 9: colour values kept after data of another size coloured the wrong points.
- Final review (fresh reviewer): a click selection did not redraw; turning adaptive sampling off
  did nothing; log-value impulses kept a stale bottom; centre steps (line and fill) sat at the
  arithmetic mean on a log key axis; an empty view was replayed over the data; a zoom recorded
  twice; the curve ignored setComponentLineStyle and colour values set through the base; legend
  markers without a pen were black; dead code was left behind.
- Final review (opencode): a fill whose edges cross was filled past the crossing on the GPU.

Each fix has a test that failed first.

## Deferred minors (also in backlog.md)

- No test checks that colours reach the GPU vertex buffer through the markers path (Task 4).
- On a log key axis, an error bar's cap is symmetric in data, so slightly lopsided on screen.
- A NaN inside a filled curve goes into the fill polygon as is (2.x did the same).
- The curve's hit test converts every point to pixels on each mouse move (2.x did the same).
