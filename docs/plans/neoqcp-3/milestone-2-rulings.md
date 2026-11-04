# NeoQCP 3.0 — milestone 2 rulings and deferred items

Milestone 2: creation with `add<T>()` and fluent handles, `take` / `add(std::unique_ptr)`, `remove()`
through the owner, automatic repaint. Branch `neoqcp-3.0`, commits `393d6bc..c5f449d` (local, not
pushed). Plan: `docs/superpowers/plans/2026-11-03-neoqcp-3-milestone-2.md`. Setter tables:
`milestone-2-setters.md`. Guide port: `milestone-2-port.md`. Benchmark: `baseline.md`, "End of
milestone 2".

## Maintainer decisions

- Getters stay raw pointers until milestone 5.
- `replot()` stays synchronous ("rightness first"); `settle()` is the barrier every export goes through.

## Rulings (what was decided, why, cost if wrong)

Repaint
- `QCPLayerable::markDirty()` (appearance) invalidates the layer's buffer; `QCPLayer::markDirty()` from
  a range change keeps it (pan translation reuse). Cost if wrong: less translation reuse.
- Exports commit ready staged data and never run the screen repaint. Cost: export and screen differ
  for one turn.
- The setter rule: equality early return, mark, then store and emit. A call that changes nothing never
  marks; nothing marks from inside a replot's layout or draw phase. Three bugs came from breaking it
  (layout `setMargins` killed pan translation and the data swap; a graph tracer and a polar background
  repainted forever). Cost if wrong: none found.
- Tickers and renderers are shared values: not dirtied, call `requestRepaint()`; the same for a
  selection decorator's pen. Documented in the guide.
- A colour map's data marks its map, once per draw (a cell-by-cell fill is one mark). Cost: none.
- An axis rect's margin change invalidates every layer of the rect. Cost: extra invalidations.
- 0.15 ms per axis appearance setter with 10k items is accepted (same walk as a pan step).

Handles and creation
- A class meant to be subclassed again writes a template `Ops<T, Self>` plus `OpsFor` (C++20 has no
  deducing this). Cost: one template per extensible user class.
- The construction trait is the class template `qcp::Construct<T>`, not a function.
- A plottable with both axes null is a valid detached object; one null axis, or axes of two plots,
  is a misuse.
- Items cannot be built detached (their constructors need the plot): `add(std::unique_ptr<Item>)` is
  for taken items.
- A layout cell may be at most one row or column past the grid.
- A refused `add(std::unique_ptr)` destroys the object (documented).
- `take` covers plottables, items and polar graphs; axes, layers and layout parts leave only through
  `remove()` (documented).
- `take(item)` cuts anchor links both ways, keeping pixel positions; `take(graph)` clears the tracers
  that follow it; `take(tracer)` clears its graph.
- A detached pipeline plottable runs its jobs on the calling thread (a dropped job would leave it busy
  forever). Cost: a GUI-thread resample of a large hidden plottable.
- `QCustomPlot::removeLayer` refuses the last layer, or another plot's layer, as a misuse.

Process
- opencode implemented the mechanical tasks and wrote the first reviews; Sonnet implemented the
  judgement half of Task 6 and reviewed opencode's work; the final whole-milestone review was
  opencode's, every finding verified by the controller (maintainer asked to save tokens). Cost if
  wrong: a subtle defect a stronger reviewer would catch.

## Review findings ruled false (so nobody re-fixes them)

- A deleted polar graph leaves no legend row: the row deletes itself (`leaveLegendWhenDestroyed`).
- Item anchors do not dangle after a take: anchor and position destructors unlink their children.
- Pipeline jobs cannot run on a freed plottable: they check a destroy guard first.
- The layout-cell guard cannot dereference a null layout: `||` stops at the teardown check.
- Debug builds aborting on misuse is the milestone-1 design, not a milestone-2 defect.

## Deferred minors (for the final triage)

- `QCPPolarGraph::setScatterStyle` and the graph scatter setters have no equality guard
  (`QCPScatterStyle` has no `operator==`).
- `setColorValues`, `setComponentFillBrush`, `moveAbove/Below` mark before their no-op paths (a spurious
  repaint).
- Duplicate include in `tests/auto/test-repaint/test-repaint.cpp`.
- The `QCPAxisRect` null-plot guard in `markLayoutAppearanceDirty` is untested.
- A tracer on a different layer than its graph may show an old position after a data change
  (pre-existing).
- Item ops repeat a `setPen` already in `QCPItemOps`.
- The graph2 scatter-colour handle test checks pixels only (no getters).
- The polar `add<T>` rollback is untested (`QCPPolarGraph::setData` only throws `bad_alloc`).
- Three of the four `remove()` routing tests pin the contract only (the 2.x destructors already do the
  owner's job).
- `with()` marks even when the lambda only reads.
- The three `detach` functions repeat the same unparenting block; one helper would keep them in step.
- Most repaint tests check that a repaint is queued, not what is drawn.

## Next

- The item-scaling milestone (`QCPItems_AddRemove10k`, 72 ms against 61 at the end of milestone 1:
  four O(n^2) list scans, profiled in Task 2).
- Then the next milestone of the object-model plan.
