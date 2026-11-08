# NeoQCP 3.0 — milestone 4d rulings and deferred items

Milestone 4d: axes, linked axes, alignment groups. Axes are made and styled through a handle; drag
and zoom follow every axis of a rect unless the axis opts out; an axis can be linked to another
through a pair of functions; alignment groups align the edges of axis rects across plot widgets and
replace margin groups. Branch `neoqcp-3.0`, commits `58181b5..3f0e2ba` plus this file (local, not pushed). Plan:
`docs/superpowers/plans/2026-11-08-neoqcp-3-milestone-4d.md` (its calls P-1..P-11 were approved by
the maintainer on 2026-11-08). Spec: `docs/specs/2026-11-06-neoqcp-3-milestone-4-design.md` (M4d,
answers 11, 14, 15) and decisions 12-13 of the object-model spec. Benchmarks: `baseline.md`, "End of
milestone 4d".

## Results

- At `3f0e2ba` on Qt 6.12: offscreen suite 2068 passed, 0 failed, 135 skipped; ASan the same, no
  sanitizer report (Qt's printing-library leaks suppressed); Xvfb whole binary 2203/0/0; clang
  `-fsyntax-only` clean on all 19 `.cpp` changed in the milestone.
- src/: 22 files, +1250 / -198 (the alignment group is one 160-line file).
- Bench: the machine was loaded all evening (load 4 to 16), so rows in touched code were settled by an
  interleaved A/B against a build of the end of M4c. Every row is level with it. The one real cost
  found, `QCPAxis::coordToPixel` +16 %, was fixed (0.99 after). An aligned stack of eight plots
  repaints as fast as an unaligned one. See baseline.md, "End of milestone 4d".

## Rulings made during execution (what, why, cost if wrong)

- R-1 (Task 2): a linked axis with its own log scale keeps a positive display range for its ticks,
  which can differ from forward(source) when the source range crosses 0. No drag, zoom or write-back
  reads it (final review): moves are computed on the source. Cost: such an axis shows ticks only on
  its positive part.
- R-2 (Task 3): the changelog does not claim that hand-connected axes zoom twice. Range signals are
  delivered after the event since milestone 4a, so both axes zoom from the same range; the colour
  bar's explicit zoom list only removes a wasted zoom. Cost: none.
- R-3 (final): a linked axis stays off the GPU grid and span path; its grid and spans draw on the
  painter. A shader cannot evaluate an arbitrary link function. Cost: painter grid and spans for
  linked axes only.
- R-4 (final): a drag, pan or zoom of a linked axis moves its source by the equivalent source pixels
  (the length ratio of the two axes), from the source's own range. Cost: none known.
- R-5 (final): a non-finite inverse during a range change is refused quietly (qDebug), not as a
  misuse: a wheel near 0 on a 1/x link reaches it, and a misuse is fatal in debug builds. Cost: a
  wrong user inverse is reported only by qDebug.
- R-6 (final): no change for "an empty explicit list restores the default" (the reset functions are
  the API, plan P-2) nor for non-monotonic links (documented as unsupported, plan P-5).
- R-7 (final): the bench table was taken under load and is kept as measured; rows were judged by
  interleaved A/B against the end of M4c instead of re-running the whole table. Cost: the table's
  medians overstate this milestone's cost; the A/B lines next to it are the verdict.

## Found and fixed in review

- Task 2: a source could drive only one link; a deleted linked axis blocked its source from being
  linked again; `setScaleType` on a linked axis wrote back into the source.
- Task 3: the colour bar's zoom list was left to the new default (a wasted second zoom per wheel step).
- Task 4: moving or resizing one aligned plot repainted every aligned plot; hiding a member did not
  realign the others; one test's check was always true.
- Final review (fresh reviewer, 1 Critical and 4 Important; opencode, 2 High; one fix batch):
  - a `rangeChanged` slot deleting an axis during a direct `setRange` on a linked pair was a
    use-after-free;
  - an aligned peer stayed misaligned after a member was resized inside its plot;
  - opting a source out of drag or zoom was undone by an axis linked to it;
  - `setupFullAxesBox()` after `setupFullAxesBox(true)` kept the link and reported misuses;
  - the GPU grid and spans placed a linked axis linearly;
  - a drag on a non-linear linked axis zoomed its source, and could write a clamped range back;
  - a null axis in `setRangeDragAxes(QList)` crashed (2.x bug);
  - a group whose last member left during its plot's teardown was never deleted.
- End checks: the alignment tests leaked their plots (ASan); `coords.cpp` did not compile with clang
  (a `std::optional` of a nested struct); an unlinked `coordToPixel` was 16 % slower (A/B).

Each fix has a test that failed first, except the colour-bar zoom list (a guard: the double zoom was
not visible) and comments.

## Deferred minors (also in backlog.md)

- A log linked axis is only re-clamped on the next source change after `setScaleType`, not at once
  (final review M6): until then it shows no ticks over a range crossing 0.
- The colour scale asks its rect for the drag and zoom axis twice per event (two short lists built).
- `QCPAlignmentGroup` hears a plot's own move, not a move of one of its ancestors inside the window
  (`simplify:` in the code).
