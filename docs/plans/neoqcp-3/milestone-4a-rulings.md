# NeoQCP 3.0 — milestone 4a rulings and deferred items

Milestone 4a: every signal reachable from a plot is delivered when the outermost NeoQCP call returns,
and nine classes share one selection model. Branch `neoqcp-3.0`, commits `8c0e0cf..e6e1b0f`
plus this file (local, not pushed). Plan: `docs/superpowers/plans/2026-11-06-neoqcp-3-milestone-4a.md`.
Spec: `docs/specs/2026-11-06-neoqcp-3-milestone-4-design.md`. Inventory: `m4a-signal-inventory.md`.
Benchmarks: `baseline.md` ("End of milestone 4a").

## Results

- At `e6e1b0f`: offscreen suite 1883 passed, 0 failed, 122 skipped; ASan the same, no sanitizer
  report; Xvfb whole binary 2005/0/0; clang `-fsyntax-only` on all 56 changed `.cpp` files passes;
  `tools/check-signals.py` green (no longer expected to fail).
- Bench: every row inside its gate except `QCPItems_Click100` (+9 %, 0.35 µs on 4.15 µs, see R-9).
- Signals now go through three helpers in `src/core.h`:
  - `qcpEmit` copies the arguments;
  - `qcpEmitCurrent` reads the value at delivery;
  - `qcpEmitIf` drops an emission that is no longer true at delivery.

  All three emit at once when no scope is open, so a plain setter builds nothing.

## Plan rulings

- R-A: the 2.x selection signals with arguments are removed, not emitted alongside: both forms
  would make every `&Class::selectionChanged` ambiguous.
- R-B: the getter keeps its 2.x name `selected()`; no `isSelected()`.
- R-C: `rangeChanged` keeps one signature, `(newRange, oldRange)`; a one-argument slot still connects.

## Rulings made during execution (what, why, cost if wrong)

1. Scope rule: a mutator opens a scope only when one outward call can emit more than once. A scope
   on every range setter cost +21 % on `QCPAxis_SetRange10k`. Cost if wrong: a plain setter whose slot
   deletes the plot is protected only by "emit last".
2. During a flush, a slot's own signals run at once. Queueing them behind the pending ones was tried:
   two-way linked axes then reset each other to old ranges forever (doc-shots hung). Cost if wrong:
   none known; the stale cases it leaves are covered by rulings 3 and 8.
3. A state signal queued behind a nested call (`setScaleType` after `setRange`) reads the state at
   delivery (`qcpEmitCurrent`), on the setters that chain. Two changes in one call then deliver the
   final value twice. Cost if wrong: a listener counting toggles sees (x, x) instead of (y, x).
4. The overlay's `showMessage`/`clearMessage` deliver `visibleChanged` together with `messageChanged`.
   Cost if wrong: a slot comparing `visible()` at `messageChanged` sees the final state.
5. The legend's and legend row's `selectableParts`/`selectedParts` properties notified each other's
   signal; fixed. Cost if wrong: a QML binding that relied on the swap.
6. The Task 3 ruling "nothing is queued ahead of `itemCreated`" was wrong (the final review showed
   `mousePress` is). `itemCreated` is now dropped when the item was deleted first (`qcpEmitIf`).
7. The polar header `add<T>` read `QCustomPlot` while incomplete: clang rejected it, GCC did not.
   The check moved out of line. Older than this milestone, found by its clang pass.
8. A queued `rangeChanged` is dropped when the axis's range moved on before delivery: a one-way link
   had already told the listeners the newer range. One call changing an axis twice therefore
   delivers only the last change. Cost if wrong: a listener that needs every intermediate range.
9. `QCPItems_Click100` +9 % (0.35 µs) is not chased: the 10 000-item click rows are flat (0.98, 1.00)
   and the row is a single 4 µs shot. Cost if wrong: a real per-click cost on small plots.
10. `examples/plots` had not compiled since the handle API (it is not in the meson build); it now
    passes raw pointers with `.get()`. Older than this milestone.

## Deferred minors

- A plot deleted from a nested event's delivery drops the teardown signals still queued, silently.
- Six "slot deletes the plot" tests cannot fail on their counters, because Qt disconnects slots whose
  context object died. They still reproduce crashes under ASan.
- The click delivery of items and legend rows checks the item is alive, but emits on the plot.
  Older than this milestone.
- No test yet for the refusal of a partial selection type on a legend row and a text element (the
  item has one), nor for the NOTIFY fix.
- `examples/` is not built by meson, so it can break again unnoticed. Add a build option when
  milestone 5 moves the examples.
