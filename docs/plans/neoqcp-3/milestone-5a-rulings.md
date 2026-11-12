# NeoQCP 3.0 — milestone 5a rulings and deferred items

Milestone 5a: the first step of milestone 5 (delete the old API). Before the old graph classes go, `QCPMultiGraph`
(the future `QCPGraph`) gains what users of the old graph need. Branch `neoqcp-3.0`, `3e95d2e..` the commit that adds
this file (local, not pushed). Design: `docs/specs/2026-11-11-neoqcp-3-milestone-5-design.md` (maintainer answers
M5-1..M5-4, calls C1-C11, amendments A1-A14). Plan: `docs/superpowers/plans/2026-11-11-neoqcp-3-milestone-5a.md`
(calls P-1..P-8, amendments B1-B10 after an opencode plan review). Inventory: `m5-inventory.md`. The maintainer
answered M5-1..M5-4 on 2026-11-11 and said "you can start the implementation once you are done with the plan";
everything below was decided by the controller.

## Results

- Offscreen suite 2481 passed, 0 failed, 136 skipped; Xvfb whole binary 2617/0/0; whole ASan suite clean after the
  last code change; ThreadSanitizer clean on the source-cache scenarios; clang clean on every changed `.cpp`.
- Bench: `baseline.md`, "End of milestone 5a". Nothing moved; the new twin rows are the 5c gate.

## What 5a built

- `QCPGraphData` in its own header; the draw context's key window comes from the system (empty = unbounded).
- The multigraph needs only `capStraightSegments` and draws on user coordinate systems: lines, steps, impulses,
  scatters, fills, error bars (pixel caps per bar), colour by scalar, hit and rubber-band selection. No resampling
  there. The waterfall keeps a rectangle.
- `addData` (owned growable source only) and one-column `setData(keys, values)`.
- A component fill to another graph (cropped and interpolated, same system, partner lifetime, selection split).
- The tracer follows a multigraph component (`setLineGraph`).
- Bench twins of every old graph workload.

## Rulings (what, why, cost if wrong)

- R-1 (plan): Task 2 split into 2a (lines, points, hit, pipeline bypass) and 2b (fills, error bars, colour), with
  interim refusals in between. Cost: none.
- R-2 (Task 3): a keys/values length mismatch in `setData`/`addData` is a misuse; 2.x truncated. Cost: 2.x code
  passing unequal vectors draws nothing (migration page).
- R-3 (Task 3): the N-column `double` `setData` sorts by default after an `is_sorted` check, as 2.x `setData` did.
  Cost: one O(n) pass on sorted data.
- R-4 (Task 3): an append while a resample job reads the source clones it first (an in-place append raced the job;
  the bench found the crash). Cost: an append right after a large `setData` is O(n).
- R-5 (Task 4): the dependency walk is cycle-safe (mutual fills crashed it); it does no set work for a plottable
  without dependents. Cost: none measured.
- R-6 (final): NaN keys in `setData`/`addData` are refused with one misuse; there is no order to sort or bin them by,
  and 2.x sorted them with undefined behaviour. Cost: data with NaN keys draws nothing (migration page).
- R-7 (final): on user systems, gaps are computed per call (no cache) so the draw never writes a source a resample
  job reads; the threshold is set on install. Cost: one O(n) pass per frame on user systems only.
- R-8 (final): the data-space hit-test cost and dense error bands on user systems are documented limits.

## Found and fixed in review

- Plan review (opencode): the fill and error-bar code read the cartesian axes in functions the plan had not named;
  large graphs started the resampler on user systems; `addData` during a pending swap would have been overwritten;
  no appendable source type existed; two tracer overloads made `setGraph(nullptr)` ambiguous; a fill partner's change
  had no repaint route. All folded in as B1-B10.
- Task 4: mutual fills recursed in the dependency walk (crash), and cleared fills kept their partner edges.
- Final review (fresh Opus reviewer and fresh opencode reviewer, one batch, a failing test first for each):
  - the growable source sorted before checking lengths (out-of-bounds read); NaN keys reached `std::sort`;
  - a pending swap was lost or given the wrong bins across a move to or from a user system; a busy append skipped
    the deferred swap; a clone lost the gap threshold;
  - the user-system draw and the fill partner wrote a source's gap cache while a resample job read it (TSan);
  - fill selection coloured shaped strip rows instead of data rows;
  - error-bar caps used the first bar's direction for every bar;
  - step-centre and fill shaping ignored a logarithmic key axis on user systems;
  - a coloured graph moved to another rect kept the old rect's colour scale; the base `setColorScale` bypassed the
    guard;
  - the first `addData` on a fresh waterfall was accepted;
  - a tracer kept drawing after its component disappeared; a fill came back when its partner's component returned;
  - three tests passed without exercising their behaviour.

## Deferred (also in backlog.md)

- Data-space hit-test and draw cost (no decimation, no culling, no dense error band on user systems).
- Cartesian `fillValue` on a negative log range closes on the far bound (unchanged on purpose: cartesian must not
  move in 5a).
- A tracer on a curve or a polar graph.
- Qt `QThreadPool` ThreadSanitizer warnings (outside NeoQCP).
