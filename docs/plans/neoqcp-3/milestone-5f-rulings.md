# NeoQCP 3.0 — milestone 5f rulings and deferred items

Milestone 5f: the triage. Every backlog item is now fixed, scheduled to project 3 or 4, or written down as a 3.0
limit. The limits are on one page, `docs/limits.md`. `backlog.md` is the disposition table. The guide and API
pictures are regenerated. There is no release (maintainer, 2026-11-14): no version, tag or release notes. Branch
`neoqcp-3.0`, `42956b6..` the commit that adds this file (not pushed). Plan:
`docs/superpowers/plans/2026-11-14-neoqcp-3-milestone-5f.md` (calls F-0..F-7). Design:
`docs/specs/2026-11-11-neoqcp-3-milestone-5-design.md`, 5f and A13.

Every task was implemented by a Claude subagent and reviewed by a fresh one. A fresh Opus reviewer read the whole
milestone at the end, and one fix batch followed. Builds, suites and ASan+UBSan ran on the Coder workspace `neoqcp`;
reference pictures were checked locally (they depend on fonts).

## Results

- Offscreen 4117 passed, 0 failed, 135 skipped, on Coder at 780cb25. 5e ended at 4081.
- The whole ASan + UBSan suite 4117/0/135 with no sanitizer report. Leak checks were off: LeakSanitizer does not
  run under the workspace's ptrace.
- Local `meson test` (Xvfb): 5/5 OK, auto-tests 4252/0, reference pictures and guide snippets green.
- clang `-fsyntax-only` clean on the 41 `.cpp` files changed in the milestone. Every example builds.
- Triage: 181 items. `backlog.md` counts 17 fixed, 8 project 4, 51 limits, 73 dev notes, 41 stale.
- CI: not run. The milestone is not pushed.

## Rulings (what, why, cost if wrong)

- R-1 (maintainer, 2026-11-14): SciQLopPlots is rebuilt on 3.0 later, not ported. The migration task is light: one
  compile probe, its three unlisted breaks written down, no further probing. Cost: more breaks found at rebuild time.
- R-2 (F-3, overrules 5c R-2): `setBrush` on a graph keeps each fill's target and changes only its brush.
  `milestone-5c-rulings.md` is annotated.
- R-3 (F-4, extended in Task 2): `draw(QCPDrawContext&)` is pure on plottables and on items. A subclass that drew
  nothing by accident now fails to compile. Cost: none intended.
- R-4 (Task 2 probe): two `unitBoxPixels` helpers clashed in the unity build; one internal helper now. CI has no
  unity build, so such a clash can return unseen (deferred below).
- R-5 (final I1): a colour-mapped plottable records its scale choice in three states: the rect's scale (default),
  one the user picked, or none. Only `setColorScale()` changes it. Before, a null scale meant both "never chose"
  and "chose none", so four paths re-joined the rect's bar after an opt-out. Cost: a plottable taken from one plot
  after the user picked a scale arrives in the new plot with no scale; the user picks again.
- R-6 (final I2): the internals the review found are private (`showsColorScale`, the plot's ancestor watch). No
  new public surface.
- R-7 (final): `plottables-histogram.png` stays as committed: the fixed step render is byte-identical to it.
- R-8 (final minor 6): the joint-density gallery map opts out of the rect's colour bar, as in 2.x: the bar
  squeezed the marginal layout. Cost: that gallery picture shows no density scale.
- R-9 (process): a false positive of mine in the Task 3 review (layout elements moving between plots; only
  plottables and items move) was dropped, not fixed.

## Found and fixed

- Drawing: the step histogram drew one box per bin (a regression from 4027293); the bracket decorator ignored a
  user system; a click on an error-bar whisker missed; a polar tick label's click box was off at 0/180 degrees; a
  NaN row in a filled curve joined the fill across the gap.
- State and signals: a linked axis switched to log kept a non-positive range; a curve given data of another size
  kept stale colour values; a colour map moved to another rect kept the old rect's scale (and kept a rect scale
  when moved off any rect); a legend re-wrap sent a burst of stretch signals; a legend row kept a stale legend
  pointer; `removeData` at a NaN key silently did nothing; a rect showed its bar for a curve it could not colour.
- Alignment groups did not hear a move of a plot's container; the fix watches the ancestors and filters only them.
- Docs: a rect's shared colour scale and how to opt out; `linkTo` for a second axis in other units; the
  architecture pages walk through `linesInData()`; the limits page; the backlog table.

## Deferred

- Error-bar hit test builds its backbone and whisker list per bar per click.
- A curve fill records one polygon per NaN run (one raster per run on the GPU).
- A rotated radial label's click box is its upright bounding box.
- The bracket user-system test checks strokes near the skewed ends only, not their absence elsewhere.
- No unity build in CI.
- `joinsOnArrival()` is still a public virtual on the colour-mapped base; only `QCustomPlot` calls it.
- The ancestor filter fix (final minor 9) has no test: the work it removes has no visible effect.
- Design question for the maintainer: the automatic rect colour bar sits flush against the data, with no gap.
