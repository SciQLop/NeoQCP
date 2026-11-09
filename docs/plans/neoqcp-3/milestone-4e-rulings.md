# NeoQCP 3.0 — milestone 4e rulings and deferred items

Milestone 4e: every plottable and item NeoQCP keeps records through the drawing API; QCPItemSpan with
an orientation replaces QCPItemHSpan and QCPItemVSpan; the QCPDrawn<> adapter is folded into
QCPAbstractPlottable and QCPAbstractItem. Branch `neoqcp-3.0`, commits `a359083..` the commit that adds
this file (local, not pushed). Plan: `docs/superpowers/plans/2026-11-08-neoqcp-3-milestone-4e.md`
(calls P-1..P-9, approved by the maintainer on 2026-11-09; executed while the maintainer was away, so
every call below was made by the controller). Specs: `docs/specs/2026-11-06-neoqcp-3-milestone-4-design.md`
(M4e) and `docs/specs/2026-11-04-neoqcp-3-drawing-api-design.md`. Benchmarks: `baseline.md`, "End of
milestone 4e".

## Results

- Offscreen suite 2172 passed, 0 failed, 135 skipped; Xvfb whole binary 2307/0/0; ASan 2172/0/135 with no
  sanitizer report (after perf batch D);
  clang `-fsyntax-only` clean on all 61 `.cpp` changed in the milestone.
- Reference images: 35 scenes (error bars, financial, statistical box, intervals, every item kind,
  spans) taken on the old code in Task 1; every port matches them through
  `qcp::test::matchesReference` (at most 20 pixels off by more than 32).
- Bench (A/B against a build of the pre-milestone code, CPU ticks, steady state): error bars -35 %,
  financial -55 %, statistical box -29 %, intervals level (labelled +1.4 %), 10k-item pan/zoom/replot
  +1..3 %, item creation, removal, restyle and first frame level, mixed item kinds +4..6 %, a click
  that selects an item +23 % (R-12).

## Rulings made during execution (what, why, cost if wrong)

- R-1 (pre-flight): the mixed-kinds bench row moved to QCPItemSpan with the same counts; each plottable
  port also checks its `-legend` reference; the error-bar tests live in the existing
  test-errorbar-geometry; the compiled docs snippets moved with Task 3, the prose in Task 9. Cost: none.
- R-2 (Task 1): `matchesReference` takes a `Tolerance {pixels, channel}`; the classic colour-map test
  keeps its pixel-exact compare with `{0, 0}`. Cost: none.
- R-3 (Task 2): `QCPAbstractPlottable1D::selectTest` keeps the 2.x nearest-point click for a 1D
  plottable whose recording is empty (a 2.x subclass painting through `draw(QCPPainter*)`); a
  recording plottable is hit-tested through its recording. Cost: about 50 lines kept to milestone 5.
- R-4 (Task 3): the span regression (+21 %) was Qt's general stroker on two-point polylines, not the
  recording bookkeeping; the shared replay draws a two-point run with `QPainter::drawLine`. Cost: none.
- R-5 (Task 3): a recording replayed into a painter clips at `clipRect().translated(0, -1)`, as the
  layer clips every child; a deferred GPU command keeps its object's antialiasing, images none.
  Cost: one pixel row at the clip edge if the layer's rule ever changes.
- R-6 (Task 3): the span hit test stays pixel-based (edges first, then the filled band), since the
  drag needs the part that was hit. Cost: none.
- R-7 (Task 4): a layerable's replay (`QCPReplayState::AlreadySaved`) leaves save/restore and the clip
  to its layer. Only `QCPDrawnCore::paint`, called from the layer's draw, passes it. Cost: a future
  caller passing it outside a layer draw would be unclipped.
- R-8 (Task 5): `QCPAbstractItem::draw(QCPPainter*)` is final (plan P-4), although the constraints keep
  2.x working to milestone 5: P-4 is the approved specific call. A 2.x item that paints itself fails
  to compile and moves to `draw(QCPDrawContext&)`.
- R-9 (Task 5): plan call P-5 is replaced for a tracer on plot coordinates: it records its marker as a
  scatter symbol at the data point and its crosshair as axis-rect-ratio lines, and depends on its
  graph (identity and data revision); every-frame recording only for other position types. Every
  frame cost +31-38 % on 1000 tracers. Cost: an input missing from the dependency would leave a stale
  marker (tests: data, graph key, interpolation, swapped container).
- R-10 (Task 5): a recorded item created after a plottable draws above it on the GPU path too (the
  creation order, as on the painter path). In the changelog.
- R-11 (Task 6): `QCPErrorBars::data()` counts as an edit (a counter its dependency hashes), as a
  `QCPDataContainer` mutable iterator does; the error-bar container is a plain vector. Dependencies
  hash their inputs in order (`qHashMulti`), never sum them: a sum let an axis swap cancel out.
- R-12 (final): accepted costs against the pre-milestone code: a click that selects an item +23 %
  (about 3.5 us: the selected look is recorded again, with fresh buffers; the hit test itself is at
  parity), the mixed item kinds row +4..6 % at steady state (the transform-dependent kinds re-record on
  a pan), 1000 tracers +6 %. Everything else in the item rows is inside 3.5 %. Cost: none a user can
  see; the upgrade is a recording that reuses its predecessor's buffers.
- R-13 (Task 7): `QCPStatisticalBox::drawStatisticalBox` is removed (one recording per plottable has
  no per-box painter call); `whiskerAntialiased` no longer sets its own painter state (the plot's
  plottable antialiasing already forced it on). Both in the changelog and the migration page.
- R-14 (Task 8): intervals record in pixels and are re-recorded on every view change, the cadence of
  the old rebuild: bars are merged per pixel, lanes are pixel bands, wave slants are pixels. The plan
  test "a panned interval is not re-recorded" became "a panned interval stays correct". The label
  layer reuses the bars of the frame's recording (keyed on its transform). Cost: a pan rescans the
  visible rows, as before.
- R-15 (Task 6-8): the legend icons of error bars, financial, statistical box and intervals stay
  painter-drawn: their 2.x geometry is in icon pixels. All four `-legend` references match.

## Found and fixed in review

- Task 1: the interval reference scenes and bench row drew nothing (lanes never created); the
  selection images selected everything; the log scenes were squashed; the bench rows placed most
  items outside the view.
- Task 2: a 2.x 1D plottable painting itself lost its click selection; 200 lines of formatting churn.
- Task 3: spans +21 % (R-4). Task 4: item rows +5-6 % (R-7). Task 5: tracers +31-38 % (R-9).
  Task 6: an edit through `data()` was not drawn; the dependency summed its inputs (R-11).
  Task 8: labelled timelines +37 % (the merge scan ran twice per frame).
- Final review (a fresh Opus reviewer and a fresh opencode reviewer; one fix pass in two batches,
  each fix with a test that failed first): error bars ignored their own same-size `setData`; error
  bars on grouped or stacked bars were drawn at the raw key and value; clearing a selection decorator
  while selected could crash the next frame; arrowheads detached from anchored lines; items anchored
  to a tracer kept the old position; a swapped data container with an equal revision was missed;
  error bars on a QCPCurve were culled by the parameter; the intervals wave kept the old ink after a
  theme change; items placed in pixels were not drawn on a plot without axes; an item on another
  layer than its graph was not repainted on new data; the error-bar backbone left a stub inside the
  symbol gap; a decorator's scatter style did not re-record; outliers lost the box pen; a log key
  axis centred candle bodies differently; the export ignored the busy fade on GPU commands.
  Not reproduced (kept as guards): a bar click selecting the whole series; a huge ellipse throwing;
  a ratio pixmap not following the layout.
- End checks: the 10k-item rows had drifted +9-11 % over the milestone although every task passed its
  own gate (perf batch C: a dependency fast path for unanchored items, one frame per view, a cheaper
  cull); the whole table then showed item creation, removal, clear, delete, restyle (+71 %), first
  frame (+27 %) and click (+33 %) over the gate (perf batch D: the recording state is allocated on
  first draw, a restyle that moves no box keeps the item index, a click shares the paint's frame).
  Batch D's index change dropped a newly added item from the index for one frame; the suite caught
  it and it was fixed in the batch.

## Deferred minors (also in backlog.md)

- A recording made again (selection, restyle, a pan of a transform-dependent item) allocates a new
  command list, context and pens (R-12); upgrade: recycle the previous recording's buffers through
  `QCPDrawContext`.
- Per-item replay overhead (view, staleness check, frame) is what remains of the item rows' cost; a
  layer whose items all move with the data could shift its texture on a pan
  (`QCPLayer::canTranslateInsteadOfRepaint`).
- A span edge given in pixels or on an anchor makes the span record every frame.
- `QCPLineEnding::draw` (axes, painter users) builds a `QPainterPath` per call.
- An unclipped tracer's crosshair spans the axis rect, not the widget (in the changelog).
- Rect and ellipse keep a positions-based `dataExtent` (a mixed plot/absolute position resolves to
  pixels).
- The layer index pads an error-bar command by its pen, not the whisker half-width: a click on a
  whisker tip just outside the data box can miss through the index (`selectTest` is exact).
- Two-colour OHLC strokes are grouped per colour, so overlapping entries of different colours no
  longer interleave in entry order.
- Intervals lay out on a horizontal key axis only (as before); no vertical-key reference.
- A painter-only plottable records an empty list when hit-tested, so its `recordingCount()` counts
  hit tests too; a subclass that forgets `draw(QCPDrawContext&)` draws nothing, silently.
