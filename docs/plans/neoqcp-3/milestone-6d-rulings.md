# NeoQCP 3.0 — milestone 6d rulings and deferred items

Milestone 6d: identity and hooks. A recorded command can name its datum (`context.setDataIndex(row)`) and its piece
(`context.setPart(n)`); items get mouse handlers (`QCPMouseEvent`: an accepted press grabs the drag and stops the pan),
anchors made from a function (`createAnchor(name, fn)`), a `changed()` signal and `dataRect()`; the plot emits
`keyPress` / `keyRelease` (the plottable key virtual is gone, `QCPIntervals` listens to the signal) and takes
`Qt::StrongFocus`; legend icons draw in their own frame (`QCP::AxisRatio` = the icon rect). Added at the maintainer's
request: an autoscale margin, `QCPAxisBase::setRescaleMargin` (default 5 % of the span, in the scale's own space) and
`rescale(bool, double)` / `QCustomPlot::rescaleAxes(bool, double)`. The targets `lollipop`, `callout`, `peak-fit`,
`brick-breaker` and `asteroids` compile, are built with the examples and have reference pictures.
Branch `neoqcp-3.0`, `e0db509..` the commit that adds this file (not pushed). Plan:
`docs/superpowers/plans/2026-11-18-neoqcp-3-milestone-6d.md` (maintainer approved P-1..P-7 on 2026-11-18; Task 7 added
on 2026-11-18). Implementers: opencode (DeepSeek V4.1 Flash, auto mode) for Tasks 1-4, 6, 7, fix rounds 1-3 of Task 5
and the final fix pass; a fresh Claude (opus) implementer for Task 5 rounds 4-5; task, re-review and final reviews by
Claude subagents.

## Results

- `setDataIndex`, `setPart`, `qcpHitPart`, `QCPMouseEvent` and the four item handlers, `createAnchor(name, fn)`,
  `QCPItem::changed()`, `dataRect()`, the plot's `keyPress` / `keyRelease`, the legend icon frame and the rescale margin
  are in, with docs (`docs/items.md` "Drag an item from the mouse", `docs/interactions.md`, `docs/axes.md` "The fit's
  margin", `docs/migrating.md`, `docs/plans/neoqcp-3/sciqlopplots-migration.md`, `m6-gaps.md`).
- The targets compile with only the allowed edits: P-1, the "Does not compile yet" comment removals, and
  brick-breaker's `ball->setCentre(game.ball);` (R-6). Coder's `build` log shows all eight user-code targets compiled
  and linked.
- Reference pictures: `brick-breaker-target`, `asteroids-target`, `asteroids-over-target` new; every picture whose
  scene rescales redrawn for the margin and eyeballed (the spectrograms and the impulse stems caught two bugs, R-8).
- Final whole-branch review: 0 Critical, 1 Important (I-1: a click on a plottable that names only a part deselected
  it), 7 Minor. I-1 and five minors (M-1, M-2, M-3, M-5, M-7) fixed in one pass, each behaviour fix with a test seen
  failing first; M-4 and M-6 deferred.
- Verification at the commit that adds this file: local Xvfb 77 classes 4654 passed / 0 failed; Coder offscreen
  4489 passed / 0 failed / 165 skipped (GPU), doc snippets green; ASan + UBSan and float-cast-overflow 4489 / 0 / 165,
  0 reports; clang++ -fsyntax-only clean on the changed files.

## Rulings

- R-1 (P-1): brick-breaker's `removeIf` predicate takes `const QCPHandle<Brick>&`; `QCPHandle::remove()` is `const`.
- R-2 (P-2): the topmost (latest recorded) command within the selection tolerance names the data index and the part;
  the distance reported is still the nearest command's.
- R-3 (P-3, Task 5): one `changed()` per `update()` through `qcpEmit`, not coalesced; every `changed()` of one update
  is held in one emission scope (root cause of a use-after-free in the follower walk). A plain `delete` of an item
  delivers its followers' `changed()` inside `~QCPItem`, after its anchors are released (documented); the library's
  removal paths deliver after the delete.
- R-4 (P-4): the plot ignores a key event, then emits `keyPress` / `keyRelease` through one helper; a slot that
  accepts stops the built-in handling and the propagation to the parent. `QCPIntervals` connects to its plot's signal
  and rebinds when it moves; a taken intervals plottable stops reading keys.
- R-5 (P-5, P-6, Task 2, Task 4): public `dataRect()` from the recording; the cheap index answer is the protected
  `indexExtent()`. Legend icons draw `AxisRatio` and pixels in the icon rect; graph, curve and polar-graph icons keep
  their invisible data-space rect (their commands exist only in data space; pixels identical). An item without
  positions has no axes for its anchors (fixed in Task 4, since the anchors are that task's deliverable).
- R-6 (Task 6): brick-breaker gains `ball->setCentre(game.ball)` after `add<Ball>()`: a bug in the user program (the
  ball sat at (0, 0) until the first tick), not an API workaround.
- R-7 (P-7, final M-2): `QCPMouseEvent` arrives accepted, `QCPItem`'s defaults ignore it; move and release go to the
  item that accepted the press. An object detached from the plot loses the grab, and a press handler that takes its
  item away records no grab.
- R-8 (Task 7, maintainer request): the margin is a fraction of the fitted span in the scale's own space (decades on
  log, transformed units on symlog), default 0.05 as matplotlib, a `Q_PROPERTY`; a negative or non-finite margin is
  misuse. Sticky edges as matplotlib: colour maps and 2D histograms clamp to their data edges, bars to `baseValue`,
  histograms and linear/symlog impulse graphs to 0, a radial axis to 0 when the data reaches the centre; clamped after
  the widening and before the `onlyEnlarge` union. A log impulse graph has no fixed baseline and fits its data. The
  colour-scale rescale and the polar area rescale are unchanged. The callout target keeps its extra value-range line
  (its box is a pixel offset above the tip; a data margin does not move it).
- R-9 (final M-3): an identity follower's margin belongs to its source, as its scale does: `setRescaleMargin` on a
  follower goes to the source, which hands it to each follower; `linkTo` takes the source's margin.
- R-10 (final I-1): a part-only hit carries the whole-plottable selection beside its part (`"selection"` in the details
  map); `qcp::hitSelection` is the one reader of a hit's selection (plottable, intervals, data locator, graph).

## Deferred

- Task 1: `selectTest` drops the part when a hit names both a data index and a part (no target needs it).
- Task 1 / final M-6: `QCPPlottable1D::at()` clamps an out-of-range index silently (documented, matches the container).
- Task 2: an icon with data on only one side is still skipped whole, ratio and pixel parts included.
- Task 2: data-free icons skip a user system's `iconTransform`; needs a doc line.
- Task 2: stale "unit square" comments in three tests and the histogram icon; four mis-indented test messages.
- Task 4: anchor helpers copied five times; rect anchors compute the box twice; wording; coverage gaps.
- Final M-4: internal helpers visible in public headers (`receivingMargin`, `validRescaleMargin`,
  `QCPMouseEvent::details`, test friends, `qcpHitPart` naming per plan).
