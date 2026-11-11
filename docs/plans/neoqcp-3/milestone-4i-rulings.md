# NeoQCP 3.0 — milestone 4i rulings and deferred items

Milestone 4i: step 10 of the coordinate-systems design. An item position binds to a coordinate system (plot
coordinates) and an area (ratio and area-absolute coordinates); items draw, hit and clip on polar areas; the item
index groups items by system. Branch `neoqcp-3.0`, `5d326e0..` the commit that adds this file (local, not pushed).
Plan: `docs/superpowers/plans/2026-11-10-neoqcp-3-milestone-4i.md` (calls I-1..I-8, amendments A1-A11 after an
opencode plan review, `.superpowers/m4i-plan-review.md`). Inventory: `m4i-inventory.md`. The maintainer said "go ahead
on 4i" (2026-11-10) and answered D9 (user systems host graph and curve through a new capability, milestone 4j);
everything below was decided by the controller.

## Results

- Offscreen suite 2345 passed, 0 failed, 135 skipped; Xvfb whole binary 2480/0/0; whole ASan suite clean after the
  last change; clang clean on every changed `.cpp`.
- Bench: `baseline.md`, "End of milestone 4i". Item creation 24-34 % faster than the milestone base (d88fe37);
  everything else level.

## Rulings made while planning (from the plan review)

- R-1 (A1): the ratio space keeps its own type, `QCPRatioTransform`, with an inverse and a polar shape (angle
  fraction, radius fraction); it belongs to the area (`QCPArea::ratioTransform()`), not the system.
- R-2 (A6): no invented axis. A position given one axis keeps it, has no system and keeps the 2.x per-slot
  resolution; it stays indexed through its axis pair.
- R-3 (A4): only cartesian systems are indexed; items on other systems are always candidates.
- R-4 (A3/A2): hit and paint agree on the clip, and both follow the item's clip flag on every area.

## Rulings made during execution (what, why, cost if wrong)

- R-5 (Task 1): items keep the painter clip and an unclipped recording on axis rects (`clippedToFrame` for
  plottables and path-clipping areas only): the 2.x recording shape, and the first-frame bench.
- R-6 (Task 2): `setSystem()` always takes the system's area; `setArea()` after it measures ratios elsewhere. Cost:
  a caller relying on set-area-then-system order loses its area.
- R-7 (final): binding a position to another system moves the item's clip area and parent layerable when the clip
  area was the position's previous area; an explicit other clip area stays. Cost: an item deliberately clipped to
  its old area follows the new one.
- R-8 (final): one predicate decides how a position resolves (plot slots by the system, ratio slots by the area's
  separability), shared by `pixelPosition`, `setPixelPosition` and `setType`; an unrepresentable request is one
  misuse report and leaves the position unchanged (never raw pixels as coordinates).

## Found and fixed in review

- Task 1: a 2.x bug in `setPixelPosition` with a vertical key axis (read an overwritten slot), with the matching
  anchored branch of `pixelPosition`.
- Task 2: an unclipped polar item was painted clipped to the disc but hit unclipped.
- Final review (fresh Opus reviewer and fresh opencode reviewer; they agreed on the important findings; one batch,
  each fix with a test that failed first): `setType` on a polar position moved the item; `ptAxisRectAbsolute` on a
  polar area answered the viewport origin; a ratio measured in a polar area under a cartesian system stored pixels
  as ratios; rebinding setters told the layer about the old frames (a retargeted item missed polar repaints — the
  cartesian `setAxes` case predates 4i); `setAxes` after a polar system kept the disc as area; binding a system left
  the clip area behind; `take` left a one-axis position's axis; `setAxes(nullptr, nullptr)` did not unbind a polar
  system; a repeated `setSystem` did not retake the area; two copies of the "system of two axes" helper; two index
  tests that passed without the change; the index built a whole transform to read its kind; stale comments.
- End checks: `QCPItems_FirstFrame10k` +5.1 % against the base; the profile showed `axisRect()` building the whole
  rect list per created position (a pre-4i cost), fixed in d88fe37.

## Deferred (also in backlog.md, for milestone 4j)

- A user system's area ratio; the cartesian-only box gate in `QCPDrawnCore::hitTest`; culling items on non-cartesian
  systems (`dataBoxOf(pixelRect)`); a tracer on a polar or user system; `area->add<Item>()`; areas moving between
  plots.
- The polar item snippet in `docs/snippets/polar.cpp` was not added: `docs/polar.md` shows the one call inline.
