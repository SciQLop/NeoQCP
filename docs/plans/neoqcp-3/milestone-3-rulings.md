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

## Final review fix rounds (A1, A2, B)

A1 (review findings C1-C4, I1, I2, `clip()`)
- A transform-dependent recording is remade on every transform change, a pan included (the
  `isTranslationOf` exemption is gone): pixel-width, grouped and stacked bars pay one re-record per
  pan, as 2.x did. A data-plus-pixel-offset position type would keep them; it stays deferred. Cost if
  wrong: pan time on those bars.
- The GPU kept geometry is re-emitted only when every kept command moves alike: all data, or all
  pixels in a recording that never read the transform. A mixed, ratio or anchored recording is
  rebuilt from its positions. Cost if wrong: a rebuild where a shift would do.
- The data container's revision counter is the one hook for "the data changed under the recording":
  no setter marks the layer dirty for it, the layer repaints by the 2.x replot contract. Cost if
  wrong: a container edit repaints only when something else asks.
- Grouped or stacked bars, and any bar whose width is not in plot coordinates, count as pixel
  geometry; a group's revision and the bar below join the dependency key. Cost if wrong: a stack or
  group change that no setter reports.
- The pan delta is measured at the range's lower corner, not at data (0, 0): at a 1.7e9 epoch the
  origin is 40 px away from the range. Cost if wrong: a wrong shift on time axes.
- The R-C3 test shifts the span by 1.7 ulp instead of dragging: at 1.7e9 a drag of an ulp-multiple
  span cannot round differently, and the same 40 px error is reproduced. Cost if wrong: none.
- `anchoredShapeFollowsItsParent` was green before the fix; it is kept as a guard and its cause went
  to A2. Cost if wrong: a test that guards nothing.
- `clip()` marks a recording transform dependent (it reads the transform). No `src/` caller yet, so
  it costs nothing today. Cost if wrong: a clipped recording re-records per pan.

A2 (review findings I3-I7, M1-M5, OC-P3, OC-I5, plus the A1 doubt)
- The per-command raster cap is gone: a recording that needs more images than the layer has left is
  painted whole by the layer's QPainter pass, with one warning per layer, so one object never splits
  between stacking levels. Cost if wrong: an object that would have fit draws on the painter.
- A kept raster is shifted across a pan, its texture untouched, and it is its own run, so it never
  spoils the triangles beside it. The one-frame band at the leading edge is accepted. Cost if wrong:
  a few pixels at the edge for one frame.
- The M4 bracket decoration is ported to commands (+85 `src/` lines, 5 over its budget): the only
  option that keeps the stacking fix without a new buffer or command kind. Cost if wrong: 85 lines
  the size loop may trim.
- `setPosition()`'s refusal is reported with `qDebug()`, not the misuse tier: the tier is `qFatal` in
  debug builds and the refusal was only a debug message before. Cost if wrong: a quiet refusal.
- An `everyFrame()` recording is rebuilt once per frame (`QCustomPlot::frameEpoch()`), not once per
  query. Cost if wrong: a stale frame for one paint.
- A drawn object that asks for its own range, hit test or extent from inside `draw()` reports through
  the misuse tier and gets the last recording, instead of recursing into the stack. Cost if wrong: a
  quiet misuse.
- `QCPItemText` caches its measured size on text, font and math mode, so a hit test and an anchor
  query no longer re-measure. Cost if wrong: a size that a font change does not refresh.
- Item 14 (the A1 doubt) is a mechanism, not luck: the recording is kept and the kept *geometry* is
  rebuilt because an anchored recording is neither all data nor all static. The test pins exactly
  that. Cost if wrong: none.

B (round of performance and docs)
- The kept replay keeps ONE run list in record order, so a build frame and a pan frame stack
  identically; merging neighbouring runs stays. Cost if wrong: a run list with a kind tag per run.
- A drawn object whose recording is one piece in data space answers `stallPixelOffset()` with the pan
  delta, so its layer shifts the texture it has instead of repainting and re-uploading
  (`QCPGpu_BarsPan10k`: 0.45 → 0.125 ms, -72 %). Every frame, a transform-dependent or a stale
  recording still vetoes the shift. Cost if wrong: a shift of pixels that should have been redrawn.
- `QCPResolveCache` is kept and not turned into a hash: it is one entry per data vector (1-2 in the
  recordings that use it), the whole cache is 0.08 % of a rebuild frame's profile and no bench row
  consults it, so neither deleting it nor a hash can be measured. The O(k²) risk is real only for a
  recording with many series (1000 contour polylines ≈ 500k `weak_ptr::lock()` per rebuild), which no
  row covers; it stays noted for the format survey (R3). Cost if wrong: that many-series case keeps
  its quadratic scan.
- No per-shape-allocation, extra-visit or linear-pass candidate passed the 5 %-of-frame gate: the
  fan-fill scratch variant was measured on `QCPDrawn_Zoom10k` and `QCPGpu_Bars10k` (0.30 → 0.31,
  5.4 → 5.3, inside noise) and reverted, and the replay's two extra visits per command are 0.03 % of a
  rebuild frame. Cost if wrong: those allocations and visits stay.

## Size loop, re-review and fix round C

Size loop (two passes, f9b37b5..75a98a4, `src/` -312 lines, tree net -239)
- Pass 1 kept the `createSrb` overloads merge at -5 lines and moved one candidate to pass 2. Cost if
  wrong: a few lines.
- Pass 2 took the dead code, the `QCPItemText` accessors, the single raster-needs visit and the bars
  visible-bounds rewrite (after its 8-case edge test was committed green on the old code). It skipped
  the per-frame upload path and the hit-test merge (risk above the lines saved). Cost if wrong: about
  20 lines left in.
- The loop stopped after pass 2: the best remaining candidate was under 30 lines. Cost if wrong: a
  few more lines.
- Pass 1's extent refactor broke culling: `QRectF::united` drops a zero-size rect, so a box built
  from points kept only the last one. `qcpPixelBox` tracks the corners by hand, and
  `shapeEndingOutsideTheClipIsDrawn` guards it. Cost if wrong: none (fixed and tested).

Re-review (two reviewers on e2ef986..75a98a4): both found the refactors preserve behaviour kind by
kind; the fix rounds left five Important and eight Minor findings, fixed in round C.

Round C (75a98a4..465cf64)
- M3 (the stack's dependency key is a sum and could collide) is ruled false: the moved bar's own
  generation makes its recording stale whatever the key says. Cost if wrong: a stale stack after a
  restack whose sums happen to match.
- M8 (grid group with a failed SRB) is fixed without a test: no hook makes an SRB creation fail.
  Cost if wrong: an untested skip path.
- A layer's shift is now added to each entry's offset instead of replacing it, so a reused recording
  keeps its own pan delta. Pan rows unchanged. Cost if wrong: a float drift over very long pans
  without a repaint (each repaint clears it).
- Found during the round, in no review: every image on a GPU layer read the first image's uniform
  slot (the offset named binding 0, the texture's). Fixed with its own test. Cost if wrong: none.
- A mixed data/ratio position is a key and a value, like `QCP::Data` *(maintainer, 2026-11-06)*. On
  a vertical key axis the data number goes through the key axis (screen y) and the ratio runs along
  the value axis (screen x). The 2.x `pixelPosition()` sent the ratio through the value axis and
  ignored the data number; the brief said to match it and the first fix (8fde011) did, which
  contradicted the extent the culling uses. `efe0dde` replaces it, and the item path resolves a
  data/ratio pair without anchors through the same function. Changelog: behaviour change. Cost if
  wrong: a 2.x program relying on the broken vertical-key result moves (it could not have meant it).

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
- `QCPAxis_SetRange10k` crept from 1.2 to 1.4-1.5 ms over milestones 1-2 and the item work (flat
  through milestone 3): 0.02 us more per `setRange` call. Profile it when the setters are next
  touched (`baseline.md`, End of milestone 3).
- Porting `QCPGraph2`, `QCPMultigraph`, `QCPColorMap2` and `QCPHistogram2D` to the drawing API, and
  folding the `QCPDrawn` adapter into the bases (spec section 8, `src/drawable.h` comment).
- The strong-type inventory (`strong-type-candidates.md`, Task 2): every public signature outside
  drawing where a raw `double`, `int`, `QPointF` or `QString` invites a mix-up.
- Re-review M6: a bar stack's staleness check recurses down the stack, O(N²) in its depth (a
  `simplify:` note at the recursion, 07293ed). A per-frame cache is unsound: a change deep in the
  stack bumps only that bar's generation. The upgrade is a revision shared by one stack's members.
  Negligible for real stacks.

Round D (2026-11-06) closed the other re-review minors: every plot-coordinate position mix follows
the key/value rule (47e4e02), the stroke scratch gives back a peak above 1 MB (03ec2ff), and the
deferred-painter "guess" was a real crash: an object deleted by another one's draw during an export
left the callback reading freed commands. The callback now holds the state weakly (19a9831).
Verified at 19a9831: offscreen 1843/0/122, Xvfb 1965/0/0, ASan 1843/0/122 (opencode).

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
