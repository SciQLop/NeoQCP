# NeoQCP 3.0 — milestone 4h rulings and deferred items

Milestone 4h: step 9 of the coordinate-systems design. `QCPPolarGraph` is a `QCPAbstractPlottable1D<QCPGraphData>`
drawn in its area's `QCPPolarSystem`; it records in data space and uses the plot's plottable list, legend, selection,
groups and take/add. The polar-only graph list, legend row type and `removeGraph` are gone. Branch `neoqcp-3.0`,
`6226ebd..` the commit that adds this file (local, not pushed). Plan:
`docs/superpowers/plans/2026-11-10-neoqcp-3-milestone-4h.md` (calls H-1..H-6, revised after an opencode plan review,
`.superpowers/m4h-plan-review.md`). The maintainer approved the milestone ("yeah go") with plan calls delegated as
for 4g; everything below was decided by the controller.

## Results

- Offscreen suite 2313 passed, 0 failed, 135 skipped; Xvfb whole binary 2448/0/0; ASan whole suite clean (67
  classes, after 9dc5975); clang clean on every changed `.cpp`.
- Bench: see `baseline.md`, "End of milestone 4h". A/B against 6226ebd: polar rows -4..-6 %, RadialZoom10k +3 %,
  everything else level.

## Rulings made while planning

- R-1: a non-periodic graph keeps 2.1's window (the keys inside the angular range) and so re-records on rotation; a
  periodic graph (the default) records the whole container and does not. The review proposed recording everything,
  which would wrap out-of-range keys onto the circle.
- R-2: the recording reads the radial range (rim crossings, hidden runs), so a radial zoom re-records; a rotation of
  a periodic graph does not.
- R-3: no seam closure and no fill to the centre: 2.1 had neither. The fill is 2.1's polygon, closed last to first.

## Rulings made during execution (what, why, cost if wrong)

- R-4 (Task 1): `polar-radians` and `polar-gallery` re-baselined. Every moved pixel is a graph meeting the rim: 2.1
  clipped with a `QRegion` ellipse about a pixel inside the disc, the recording clips to the exact disc like every
  polar plottable since 4g. Cost: rim pixels only.
- R-5 (Task 1, final): `polar-clamped` (new in 4h) re-baselined twice, verified side by side each time. A segment
  leaving the radial range is drawn toward its data point (2.1 aimed it at its fraction-1.1 clip point); the final
  batch removed rim slivers at the crossings. The first re-baseline had hidden a real bug (crossings at the wrong
  angle), caught in review: the picture is the guard, `aRimCrossingIsOnTheChord` the proof.
- R-6 (Task 1): a value a logarithmic radial axis cannot take (zero, wrong sign) is a gap like NaN. 2.1 drew it at the
  centre, an artifact of its clip-value substitution.
- R-7 (Task 2): the capability refusal test uses a test-local subclass requiring `capRectangular` (`QCPBars` cannot be
  built on polar axes).
- R-8 (final): clicking inside a visible fill selects a polar graph, as for cartesian graphs.
- R-9 (final): only visible geometry is hittable: segments whose two ends are inside the disc (H-2 records crossings
  exactly on the rim, so a segment is wholly inside or wholly hidden) and markers of in-range points. The rubber band
  ignores points outside the radial range.
- R-10 (final): `setSystem` with a non-polar system is misuse and keeps the old system (`setSystem` is now virtual).
  The plot-level `adopt` refusal of a polar graph stays: a `setSystem` veto cannot refuse and delete.

## Found and fixed in review

- Task 1 review: rim crossings took the wrong root and used clamped ends (a vertical line up a spoke); log-invalid
  values became NaN inside polylines and the fill.
- Task 2: `QCPDrawnCore::hitTest` culled through a mapped data box, wrong on a polar frame (clicks on polar
  plottables missed); now cartesian-only.
- Final review (fresh Opus reviewer and fresh opencode reviewer, one batch plus one round, each fix with a test that
  failed first): a below-range point mirrored through the centre in the crossing; no clamp arc when leaving the disc
  (a chord through it); thick pens left a sliver along the rim; the crossing branch could wrap the fill around the
  disc; markers drew black and outside the radial range; selected markers kept their style; a selected point coloured
  one of its segments; the legend icon replayed the whole graph; rescale read the recording's synthetic rim points
  (overshoot, non-idempotent, creeping key range); hidden geometry was clickable, then visible lines near the rim
  were not; a polar graph on a cartesian system was silently inert; formatting churn and stale comments.
- End checks: `QCPPolar_RadialZoom10k` +21 % from a per-point map in the record-time drawn check (53fdd70: the
  frame's fraction says the same, +3 %). The whole ASan suite found a 4g regression: 39d7028 made the linked-axis
  write-back's emission scope conditional, so a slot on the source deleting the linked axis was a use-after-free
  (`TestLinkedAxes::aSlotDeletingTheLinkedAxisDuringAWriteBackIsSafe` under ASan; 9dc5975 guards `this`).

## Deferred minors (also in backlog.md)

- `QCPPolarArea::add<T>` lacks the friendly `static_assert` of the axis-rect form and builds the object before a
  capability refusal; it only builds types with a polar-axes constructor.
- `QCPDrawnCore::hitTest` scans every command for non-cartesian frames (no box cull).
