# NeoQCP 3.0 — milestone 5c rulings and deferred items

Milestone 5c: one graph. `QCPGraph2` and the old container/painter `QCPGraph` are deleted; the former `QCPMultiGraph`
is `QCPGraph`; every user is ported once; the graph family's getters return handles. Branch `neoqcp-3.0`,
`e58307f..` the commit that adds this file (local, not pushed). Plan:
`docs/superpowers/plans/2026-11-13-neoqcp-3-milestone-5c.md` (calls G-1..G-10; amendments R1-R9 after a review by a
fresh Opus reviewer; R9 replaced the first two tasks with five steps). Design: `docs/specs/2026-11-11-neoqcp-3-milestone-5-design.md`.
On 2026-11-12 the maintainer asked for more Claude and less opencode, and for builds on the Coder builder: every step
was implemented by a Claude subagent, and the heavy builds, the ASan/UBSan suite and the bench ran on the Coder
workspace `neoqcp`.

## Results

- Offscreen suite 2518 passed, 0 failed, 132 skipped; Xvfb whole binary 2650/0/0; whole ASan + UBSan suite on the
  workspace clean (its only failures are the 71 reference-picture comparisons that differ by fonts there); clang clean;
  guide snippets green; examples build; every commit of the final batch builds on its own.
- Bench: `baseline.md`, "End of milestone 5c" (Coder workspace). One slowdown, 100 small graphs (+50 %), fixed in f4532d5 to +6 % (floor in backlog). The zone-stats profiling hook merged sites across files; fixed in e54c98c.
- Before any push: the Windows CI must be green (the component struct is defined after the class holding a list of it;
  GCC and clang accept it, MSVC is unverified).

## Rulings (what, why, cost if wrong)

- R-1 (R1-R4, plan): before deleting the old graph, the multigraph regained 2.x behaviour the review found missing:
  selection drawn as selected segments with the decorator; the group legend row is a plottable row; `setBrush` does not
  replace a fill to another graph; the negative-log baseline is the bound nearer zero. Cost: none.
- R-2 (S2): `setBrush` still replaces a fill to a constant or between columns (2.x had no such fills). Cost: one flag
  on a fill.
- R-3 (S2): "the user styled the decorator" is a decorator subclass, or a default decorator whose pen, brush or marker
  differ from a fresh one (`simplify:` note). Cost: a decorator restyled back to the exact default looks untouched.
- R-4 (S2): selected markers outline with the selected pen (2.x); a selected component's error bars all draw in the
  selected pen; `assignSelection` is a new protected virtual on `QCPAbstractPlottable`.
- R-5 (S3): a fill to another graph across two value axes (same key axis and area) is a 2.x capability and stays: the
  partner's values map through pixels onto this graph's value axis.
- R-6 (S3): a one-component graph's legend row shows the graph's name unless the component was named.
- R-7 (S4): `setData` takes braced lists (initializer-list overloads), the 2.x call shape.
- R-8 (S4): removing points is a 2.x use case (rolling windows): `removeDataBefore`, `removeDataAfter`,
  `removeData(from, to)` on the graph's own source, with `addData`'s ownership rules; `from >= to` removes nothing.
- R-9 (S4): an empty graph's legend row draws its pen and name; a fill whose partner died or left is removed.
- R-10 (S5): `QCPGraphComponent` keeps its name although `QCPCurve` shares the struct. Cost: a later rename.
- R-11 (final, corrects S3): a one-column graph's default pen is palette colour 0 at cosmetic width (close to 2.x's
  blue at width 0); the docs follow the code.
- R-12 (final): a click with `stWhole` keeps the clicked row in `selection()` and draws the whole graph selected, as
  2.x; `setBrush(Qt::NoBrush)` removes a fill to another graph (no brush, no channel, as 2.x); at teardown the plot
  deletes its loose layout elements while it is whole (not every leftover child: the selection rect's teardown signal
  test relies on its order).
- R-13 (final): the bracket of a multi-column graph sits on component 0's values; accepted as a minor.
- R-14 (docs): of the regenerated guide pictures, only the ten that one graph changes were kept; the others drift from
  earlier milestones and the toolchain, and regenerating and reviewing every guide picture is a release-triage item.

## Found and fixed

- Plan review (fresh Opus reviewer): selection drew nothing, the legend row changed type (`itemWithPlottable` returned
  null and an example crashed), a remembered brush overwrote a fill to another graph, the negative-log baseline differed,
  the graph list lost adopted graphs, deleted tests would have lost coverage, the step order could not keep commits
  building. Folded in as R1-R9.
- During the steps, each with a failing test first: all-NaN keys crashed the draw; scatter markers ignored the busy
  fade; `plottableAt`/`plottableClick` lost the data index; the hit test picked the wrong segment end; the fluent handle
  lacked `setScatterStyle`; braced `setData` was ambiguous; setting the same pen repainted; a plottable group of empty
  graphs read as hidden; a cross-value-axis fill was refused; the legend row read "Component 0"; points could not be
  removed; an empty graph had no legend icon; a dead fill partner stayed reported.
- The 5b bench rerun on the workspace found four slowdowns (`SetRange10k` +27 % and others); fixed in this milestone's
  branch (6ba867b).
- Final review (fresh Opus reviewer, one batch): a click ignored the selection type; `setSelectable` skipped the
  selection hook; no-brush kept a band; a cross-axis fill crashed without a partner system; the bracket read past the
  data after a removal; selection drawing was O(vertices x ranges) (a selected redraw 28 ms -> 0.85 ms); the 2.x
  getters and properties (`scatterStyle`, `lineStyle`, `scatterSkip`, `adaptiveSampling`) were missing;
  `iSelectPlottablesBeyondAxisRect` was ignored; handles per row on walks; and two pre-existing teardown UBs found by
  UBSan (a loose colour scale and an extra polar grid outliving their owner's destructor).

## Deferred (also in backlog.md)

- Regenerate and review every guide picture (release triage).
- The Windows CI check for the component struct order (before any push).
- A pan on a logarithmic key axis re-records instead of translating.
- `setScatterMaxPoints` (a `QCPGraph2`-only feature) has no equivalent.
