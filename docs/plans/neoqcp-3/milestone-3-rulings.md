# NeoQCP 3.0 — milestone 3 rulings and deferred items

Milestone 3: strong position types, the command list, GPU replay, `QCPDrawn<Base>`, one hit-test rule,
the `annotations` layer, `QCPBars` and `QCPItemText` on the drawing API, and the custom-drawable
example. Branch `neoqcp-3.0`, commits `2eac8a2..ba3bb01` (local, not pushed). Plan:
`docs/superpowers/plans/2026-11-04-neoqcp-3-milestone-3.md`. Spec:
`docs/specs/2026-11-04-neoqcp-3-drawing-api-design.md`. Survey:
`m3-command-optimisation-survey.md`. Benchmarks: `baseline.md` ("End of milestone 3") and
`m3-gpu-baseline.md`. Guide page: `docs/writing-a-plot-type.md`.

Every ruling below is a `Ruling:` line of the plan ledger
(`.superpowers/sdd/2026-11-04-neoqcp-3-milestone-3/progress.md`), reduced to one sentence. Costs are
the ledger's.

## Maintainer decisions

- No Qt Canvas Painter for now (GPLv3, an add-on): the command list stays backend-neutral so it can
  be added later behind a build option.
- A pen style never changes stacking: the GPU draws its own dashes.
- Strong types as soon as possible everywhere they make the API safer.
- Cleanup and dead code: keep the code size reasonable, simplify and refactor without hurting
  performance; Task 10 gets a whole-branch dead-code sweep.
- "9b, then close": a late task took the survey's R1/R2/R7 before the milestone closed.

## Rulings (what was decided, why, cost if wrong)

Pre-flight
- `QCPResolveCache` is introduced by Task 4 with the key (generation, transform) and extended by
  Task 6 (view epoch, translation reuse), because the plan names it in Task 4's signature before
  Task 6 defines its policy. Cost if wrong: one refactor in Task 6.

Task 1 and 1b (benchmarks and profiling)
- Task 1b was added to the plan: a `zone_stats` meson option that gives `neoqcp-profiling-hook` a
  per-call-site timer (static site object, atomics, sorted table at exit to `$NEOQCP_ZONE_STATS`),
  because Tracy's headless capture tools are not installed and the GUI cannot run overnight. Cost if
  wrong: ~80 lines of dev-only code; the default build is unchanged.
- The GPU rows time one `replot()`, the CPU side of the frame, not the GPU raster. Cost if wrong:
  the numbers understate what a real driver pays.

Task 2 (positions and transforms)
- The plan's single `MixedX` misses the horizontal span, so there are two types named for what they
  hold: `QCP::DataXRatioY{x, y}` (vertical span) and `QCP::RatioXDataY{x, y}`. Cost if wrong: a
  rename.

Task 4 (GPU replay and dashes)
- A filled free-form polygon is stroked on the GPU and its fill is painted by QPainter until Task 5
  rasterises it into a GPU image: there is no general tessellator in the tree and Task 5 already
  builds the raster path. Cost if wrong: such fills stack under GPU content until Task 5.
- The dash phase is pixel arc length on the extruded vertices, not data units: a pan only translates
  the vertices so the phase survives it, and QPainter dashes are pixel-based too. Cost if wrong:
  dashes re-flow on zoom (as with QPainter).
- The Task 5 fallback image is a third kind in `QCPPlottableRhiLayer`'s ordered draw list, not a
  colour-map-layer quad as the plan says: the colour-map layer renders apart, which would lose the
  image's place in the order, the spec's binding rule. Cost if wrong: one more pipeline in the
  plottable layer.
- GPU verification must run the whole auto-tests binary under Xvfb, not selected classes, because
  that is where Task 4's draw-order regression hid. Cost: ~2 min per check.
- The non-solid brushes and free polygon fills (review items I3/I7) go to Task 5; meanwhile the GPU
  fills only `Qt::SolidPattern` and `PainterOnly` draws the rest, because Task 5 is the raster path
  for exactly these. Cost if wrong: such fills stack under GPU content until Task 5.
- A cosmetic pen is 1 device pixel through `plottable1d`'s `drawPolyline`, matching QPainter's
  cosmetic pen and the other GPU paths. Cost if wrong: thinner 0-width lines on HiDPI than in 2.x.

Task 5 (raster fallback)
- Raster images cut at the viewport show an empty band at the leading edge during a pan offset, the
  same as the paint-buffer pan fast path, healed by the next layer redraw. Cost if wrong: a few px
  band during a pan.
- A layer's 5th and later raster style is painted by QPainter under the layer's GPU content (the
  plan's cap of 4) and the warning says so. Cost if wrong: such styles stack wrongly beyond 4 per
  layer.

Task 6 (`QCPDrawn<Base>`)
- `QCPDrawn<Base>` is a thin template forwarding to a non-template `QCPDrawnCore` that owns the state
  and the logic: one copy of the logic for plottables and items, and less compile time. Cost if
  wrong: one indirection per call.
- An item's frame uses its first position's axes, because the adapter needs one transform pair per
  frame (per-position frames would come later). Cost if wrong: an item mixing axis pairs draws
  through the first pair.
- The pan reuse of extruded vertices lives in the GPU replay as an optional per-recording cache, not
  in each caller: one place. Cost if wrong: none.
- The default legend icon re-records through `drawLegendIcon(ctx)` once per generation instead of
  replaying the screen recording: one door for overrides. Cost if wrong: one extra `draw()` per
  generation.
- The GPU replay resolves positions before looking up kept triangles, because resolving is cheap next
  to extruding. Cost if wrong: some pan time on huge recordings.
- The view epoch bumps on the export temp viewport, so only view-dependent recordings re-record.
  Cost if wrong: one re-record after each export.
- Theme-reading recordings re-record on the epoch through a flag set by `context.theme()`, the spec
  over the brief's rule 5. Cost if wrong: extra re-records on a theme change.
- The per-frame allocations on pan cache hits were deferred to the Task 9 bench. Cost if wrong: pan
  time with many filled items.
- The reformat guard ("check `git diff --cached` against `-w` before every commit") was added to
  `dispatch-context.md`, because the formatter kept reflowing `core.*`. Cost if wrong: none.

Task 7 (hit rule)
- The spec's precedence "axis rect interactions before drawables" is not applied to presses: spans
  and intervals take the press so they can be dragged. Cost if wrong: a custom drawable that accepts
  the wheel beats the axis-rect zoom.
- The hit rule stays an ordered list delivered until one is accepted (`hitsAt`), not a single winner,
  which keeps Qt-style propagation. Cost if wrong: none known.

Task 8 (`annotations` layer)
- Every item is created on `annotations` whatever the current layer (the spec says "by kind"), with a
  fallback to the current layer if `annotations` was removed. Cost if wrong: programs that call
  `setCurrentLayer` before creating items must now call `setLayer`.
- The redirect lives in `registerItem`, not the item constructor, because `adopt()` bypasses subclass
  constructors and must obey the rule too; a subclass constructor's `setLayer` still runs later and
  wins. Cost if wrong: one moved line.

Task 9 (`QCPBars`, `QCPItemText`)
- Pixel-geometry bars mark their recording transform dependent by reading `transform()`: they
  re-record when the frame is not a translation (a zoom, a log change, a resize) and keep it on a
  pan, while data-only bars keep zoom reuse. The position variant has no data+pixel-offset type and
  adding one would touch every resolve. Cost if wrong: pixel-width bars re-record per zoom frame, the
  same as today's QPainter cost.
- Bars record every bar; culling happens only if the perf gate is missed, and then generically in the
  replay, because the recording outlives the visible range. Cost if wrong: replay time on huge bar
  sets.
- `QCPTextStyle` grows `pen`, `brush`, `QMarginsF padding` and `textFlags` so `QCPItemText` is one
  text command. Cost if wrong: a wider near-private struct.
- Net `src/` +290 is accepted for now: the culling and the text-measure cache bought the perf gate,
  and the size sweep is Task 10. Cost if wrong: a larger branch.
- `QCPBars` keeps its own `getKeyRange`/`getValueRange` overrides instead of a new range hook:
  smaller, and it keeps `inKeyRange`. Cost if wrong: none visible.
- The Z1 tests assert a re-record count, not pixels: offscreen has no QRhi screen path and
  `toPixmap` bumps the view epoch, so a pixel test passes before the fix. Cost if wrong: a follow-up
  bug the count misses.

Task 9b (merged draws)
- Stage B is kept although llvmpipe shows no wall-time gain: near-zero cost, and it removes 19 999
  draw calls that a real driver pays for. Cost if wrong: ~60 lines.

Task 10a (size sweep)
- `-214` net `src/` lines is accepted instead of the target `-800`: the clang unused-symbol sweep
  finds nothing dead, the legacy extrusion cluster is the live no-RHI path, and the rest is dense.
  Cost if wrong: dead or duplicate code stays; the final review gets a size lens.

## Review findings ruled false (so nobody re-fixes them)

- Task 4, review M8: "the histogram outline ignores the pen style". The outline pen is built
  internally (`columnLook`, 1 px solid) and is never dashed or `NoPen`; the fix was reverted
  (`d4f58b1`). Source: `progress.md`.
- Task 6, review X3: "a pixel-anchored shape with an unbounded side is not hittable". It does not
  reproduce on Qt 6.12: `QRectF::contains()` on a rect with infinite extents returns true, and the
  test the brief named passed before and after (probe kept at `build-qt612/oc-scratch/probe.cpp`).
  The change was kept as a clarity change, not as a fix. Source: `task-6-fix-report.md`.
- Task 10a: "the painter error bars changed pixels". Drawing 2-point runs through
  `drawPolylineSplitNaN` gives the same pixels and the same dash restart as `drawLine`. Source:
  `progress.md`.
- Task 2, review: "`QCPItemPosition::asPosition()` should report the mixed spaces it accepts". The
  mixed types exist on the way in (`QCP::DataXRatioY`, `QCP::RatioXDataY`): a mixed read-back variant
  is a decision for the drawing API, not a bug in Task 2, and asymmetric anchors are reported as
  `Pixels` on purpose. Source: `task-2-report.md`.

## Deferred (for milestone 4 and the final triage)

- `setupPaintBuffers` costs ~157 us per replot on the GPU row (26 % of the replot): a candidate when
  paint buffers are next touched (Task 1b report, `progress.md`).
- Per-frame allocations on pan cache hits (Task 6 ruling): measured in Task 9, not removed.
- Per-position frames for an item mixing axis pairs (Task 6 ruling).
- A data+pixel-offset position type (Task 9 ruling): would touch every resolve.
- Raster images cut at the viewport show a one-frame band during a pan offset (Task 5 ruling).
- A layer's 5th+ raster style falls back to QPainter under its GPU content (Task 5 ruling).
- A free-form polygon with a solid brush stays on QPainter: the baseline tessellator is the only fill
  path (Task 4 report).
- The `src/` size sweep stopped at -214 lines (Task 10a ruling): the size lens stays for the final
  review.
- Porting `QCPGraph2`, `QCPMultigraph`, `QCPColorMap2` and `QCPHistogram2D` to the drawing API, and
  folding the `QCPDrawn` adapter into the bases (spec section 8, `src/drawable.h` comment).
- The strong-type inventory (`strong-type-candidates.md`, Task 2): every public signature outside
  drawing where a raw `double`, `int`, `QPointF` or `QString` invites a mix-up.

## Format decisions for milestone 4 (survey)

Done in Task 9b:
- R1 (merge adjacent solid entries with equal scissor, offset and alpha) and R7 (dashed runs merge
  only with equal pattern and phase) are in, and they never reorder. R2 (cache the built geometry per
  recording, translate it as one block) is in; the cache key moved from command index to recording.

To decide before milestone 4:
- R3 (typed batch commands: rects and ellipses as SoA arrays, appended to the previous command with
  the same pen and brush kind, per-bar colour as a colour entry, data bounds plus a "sorted by key"
  flag per batch). The command set freezes then; helpers do not change and the list is private, so
  it is ABI-free. R2's cache contract is already fixed by Task 9b; R3 is the one left.

Open, with the survey's conditions:
- R4 (instanced marks with the data transform on the GPU): pays off above a few hundred marks, needs
  R3's data-space SoA, a per-recording double origin for float32 precision, and `QRhi::Instancing`.
- R5 (cull by data range per command and inside sorted batches): matters for 1e5+ marks zoomed in,
  needs R3's per-command data bounds and sorted flag, can come later.
- R6 (bounded look-back merge across pipelines): only if measured, rarely useful in plots.

Not recommended by the survey: a depth-buffer opaque/alpha split, full sort keys, compute tiling.

## Next

- Milestone 4: port the remaining plottables, decide R3, fold the adapter into the bases, and keep the
  rulings above in the format appendix of the new plan.
