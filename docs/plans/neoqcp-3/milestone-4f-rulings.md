# NeoQCP 3.0 — milestone 4f rulings and deferred items

Milestone 4f: steps 1-4 of the coordinate-systems design (`docs/specs/2026-11-09-neoqcp-3-coordinate-systems-design.md`).
`QCPArea` (background, inset layout) under `QCPAxisRect`; `QCPPolarArea` takes the layout role the angular axis
had (the polar 2.1 tech preview API breaks, no shims); `QCPAxisBase` under `QCPAxis` and both polar axes; polar
axis click selection and radial drag/zoom work; `QCPTransform` holds a cartesian, polar or custom mapping
(`QCPCustomMapping`, public). Branch `neoqcp-3.0`, `fd60df3..` the commit that adds this file (local, not pushed).
Plan: `docs/superpowers/plans/2026-11-09-neoqcp-3-milestone-4f.md` (calls K-1..K-7, approved 2026-11-09).

## Results

- Offscreen suite 2227 passed, 0 failed, 135 skipped; Xvfb whole binary 2362/0/0; ASan whole suite 2227/0/135
  with no sanitizer report; clang `-fsyntax-only` clean on every changed `.cpp`.
- The six polar reference pictures taken on the old code (Task 1) match unchanged at the end.
- Bench: every drawing row within 5 % of the milestone's base (A/B against a build of fd60df3, steady state):
  `QCPCoords_CoordToPixel1M` +2 %, `QCPItems_Pan10k` -1 %, `QCPPolar_*` -2..+1 %, GPU rows level.
  `QCPAxis_SetRange10k` +5 % (gate 15 %).

## Rulings made during execution (what, why, cost if wrong)

- R-1 (Task 2): `QCPArea::colorScale()` (call K-1) is not added: `QCPAxisRect::colorScale()` returns a
  `QCPColorScaleOps` handle, so a base virtual of that name would collide with it, and the design keeps colour
  scales cartesian-only in 3.0. Milestone 4g's `ColourScale` capability is the extension point. Cost: an
  area-level colour-scale accessor added later under another name.
- R-2 (Task 2): a polar area's background draws under the grid, as an axis rect's does (it drew over the grid
  before). Changelog note. Cost: a polar background pixmap no longer covers the grid.
- R-3 (Task 2): `QCPPolarArea::removeRadialAxis` deletes the axis, as 2.x `QCPAxisRect::removeAxis` does.
- R-4 (Task 3): `QCPPolarAxisAngular::area()`/`QCPPolarAxisRadial` return `QCPPolarArea*` covariantly (the axis
  headers include `polar-area.h`); `linkSource()`/`moveTarget()` are virtual so `QCPAxis` returns `QCPAxis*`.
  One implementation, no forwarders.
- R-5 (Task 3): polar drag/zoom defaults are unchanged (angular off, radial on; the old area drag honoured
  them). Only the setters' names changed (`rangeDraggable`/`rangeZoomable`).
- R-6 (Task 4): `src/qcp.h` names `coords.h` and `coords-custom.h`, so the extension point is reachable from the
  umbrella header, not only through `drawable.h`.
- R-7 (final): the polar angular axis refuses a logarithmic scale (its geometry is linear): misuse.
- R-8 (final): `QCPAxisBase` keeps the area pointer and the orientation next to the range and the flags (one
  36-byte block, one cache line), and `QCPAxis` reads its rect through it: the axis-base move had split
  `coordToPixel`'s inputs over several cache lines (+33 %, the regression 3f0e2ba once fixed).
  `QCPAxis::axisRect()` is now out of line. Cost: a call for `axisRect()` outside the per-point paths.
- R-9 (final): `QCPColorMap_SetCell1M` +15 % in the table was code placement (identical instruction counts,
  same per-call code, level after a rebuild): no change.

## Found and fixed in review

- Task 1: none (the six pictures and the bench rows were right first time).
- Task 2: a polar background filled the rect corners; fixed (the area clips to the disc).
- Task 3: the first implementer stopped half-way; resumed by a fresh one with two corrections (polar drag
  defaults kept, no `linkTo(QCPAxis*)` wrapper). ASan caught the angular axis outliving its area.
- Final review (fresh Opus reviewer and fresh opencode reviewer; one fix batch, each fix with a test that
  failed first): polar graphs were children of the angular axis (hiding it hid them); a polar axis or the grid
  deleted directly left the area dangling (use-after-free, double delete); `QCPAxis::getPartAt` had lost its
  2.x public access; polar look changes dirtied only the axis' own layer; the polar background brush lost its
  antialiased rim; the angular axis accepted a log scale; the area set margins where the ring had set minimum
  margins; the link teardown ran after the derived axis was gone; `QCPTransform::polar` accepted axes of two
  areas; custom transforms that are equal were not a translation of each other. Not reproduced (guards kept):
  an angular-only wheel zoom not repainting.
- End checks: `coordToPixel` +33 % (R-8).

## Deferred minors (also in backlog.md)

- Polar tick-label hit boxes are the upright extent around the anchor: offset at 0° and 180° on the angular
  axis and generous at the corners of rotated radial labels; a label is hittable only after its first draw.
  The real fix is the label painter returning the rect it drew.
- `QCPCustomMapping::inverse` is pure (a one-way mapping cannot be built); polar `inverse` returns the angle
  modulo the angular range; a wrong-sign log radius maps to NaN.
- The axis hot block is 36 bytes but the object is 16-byte aligned, so `mOrientation` can fall in the next
  cache line on some allocations (aligning the class to 64 bytes would cost 16-48 bytes per axis).
- `QCPAxis_SetRange10k` +5 % (gate 15 %).
- `TestDrawable::pixelAnchoredShapeIsHittable` failed once in one full run and never again (watch).
