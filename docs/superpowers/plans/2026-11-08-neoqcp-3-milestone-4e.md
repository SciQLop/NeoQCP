# NeoQCP 3.0 milestone 4e — the remaining plottables and items, one span class, the adapter folded Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** every plottable and item 3.0 keeps draws through the drawing API; the H and V spans become
one span class; the `QCPDrawn` adapter is folded into `QCPAbstractPlottable` and `QCPAbstractItem`.

**Architecture:** First the adapter moves into the two bases: each base owns the recording core and
answers the old virtuals (`draw(QCPPainter*)`, `selectTest`, legend icon, ranges) from
`draw(QCPDrawContext&)`, so a ported class only overrides `draw(QCPDrawContext&)`. Classes not ported
in milestone 4 (`QCPGraph`, `QCPGraph2`, deprecated; the polar classes, M4f) keep overriding the
painter virtual. Then each remaining class records commands with the context helpers, data-space
geometry where it can (a pan only translates it), pixel geometry where it must (only a pan keeps it),
and `recordEveryFrame()` where its shape depends on the viewport's extent. Spans become recorded
rectangles with mixed data/ratio positions, so the span GPU layer goes.

**Tech Stack:** C++20, Qt 6.12 (QRhi), meson, QtTest, the in-tree benchmark (`tests/benchmark`).

**Spec:** `docs/specs/2026-11-06-neoqcp-3-milestone-4-design.md` (order item 5 "M4e"; calls 1 and 6;
answers 7, 8, 14), with `docs/specs/2026-11-04-neoqcp-3-drawing-api-design.md` (sections 2-7: object
contract, helpers, resolve cache, stacking, hit-test rule, legend icon). Maintainer, 2026-11-07: no
interim designs, no deprecated forwarders for NeoQCP-only APIs. Facts with file:line:
`docs/plans/neoqcp-3/m4e-inventory.md`.

## Calls made in this plan (the maintainer can overrule any)

- **P-1 One span class for H and V.** `QCPItemSpan` with a `Qt::Orientation`: `Qt::Vertical` is today's
  `QCPItemVSpan` (vertical edges, a key range, full height), `Qt::Horizontal` today's `QCPItemHSpan`.
  `QCPItemHSpan` and `QCPItemVSpan` are deleted outright: they are NeoQCP-only (no forwarders).
- **P-2 `QCPItemRSpan` stays its own class** (call 6 decided from its code): it has four edges and draws
  a rectangle, which is not an orientation. It shares the span base with `QCPItemSpan`.
- **P-3 Spans are recorded rectangles; the span GPU layer is deleted.** A span records `rect` (and its
  border lines) with `QCP::DataXRatioY` / `QCP::RatioXDataY` positions, which the GPU rect path draws.
  `src/painting/span-rhi-layer.{h,cpp}` (709 lines) and the span base's `tryRhiDraw` /
  `usesLinkedAxis` go: a recording maps through the transform, links included. A command with a ratio
  side is not shifted by a pan but re-resolved from its commands (`draw-commands.h:228-236`,
  `mayReuseGeometry` in `command-replay-rhi.cpp:70`): the span stays right, at the cost of resolving a
  few rectangles per frame; nothing is re-recorded.
- **P-4 The adapter is folded first** (Task 2), before any port, so a port is one override.
  `QCPAbstractPlottable` and `QCPAbstractItem` keep a non-final `draw(QCPPainter*)` whose default
  replays the recording; `QCPGraph`, `QCPGraph2` and the polar classes keep overriding it until they go
  or move (answers 7-8). `draw(QCPDrawContext&)` defaults to recording nothing; it is not pure. Once
  every item records (end of Task 5), `QCPAbstractItem::draw(QCPPainter*)` becomes `final`.
- **P-5 Viewport-shaped items record every frame.** `QCPItemStraightLine` (an infinite line clipped to
  the rect) and `QCPItemTracer` (its crosshair spans the rect; its position follows a graph) call
  `context.recordEveryFrame()`. They are few per plot; the line items of the 10 000-item rows are
  `QCPItemLine`.
- **P-6 Only what needs pixels reads the transform.** A recording that reads `context.transform()` is
  rebuilt on every view change, pans included (`QCPDrawnCore::recordingIsStale`, `drawable.cpp:466-476`).
  So a line item without endings records a data-space `polyline` (a pan shifts it); a line with endings
  (arrows, bars, discs) adds each ending as a `path` in pixels at its data end, oriented from the
  transform, and is rebuilt on a view change; the curve item and the bracket record pixel `path`s and are
  rebuilt the same way. They are few per plot; the 10 000-item rows use plain lines.
- **P-7 The intervals' label layerable stays a painter-drawn internal part** (answer 7: internal parts
  keep `draw(QCPPainter*)`) and owns everything about labels: label rects, label text and lane names,
  computed in its own draw (moved out of today's GPU rebuild). The plottable records bars and lane
  backdrops; its own GPU path (`drawBarsOnGpu`, `plottable-intervals.cpp:1161`) goes. A change that
  re-records the plottable also dirties the label layer (`dependentLayers`).
- **P-8 Error bars depend on another plottable.** Their `dependencyRevision()` combines the attached
  plottable's identity, its own `dependencyRevision()` (data) and its key/value axis pointers, so new
  data, a re-keyed plottable or another attached plottable re-records the bars.
- **P-9 Selection decorators apply at record time**, as `QCPBars` does: the selected pen and brush (or a
  user decorator's) are baked into the recording, and a selection change calls `contentChanged()`.

## Global Constraints

- One build or test at a time, in the foreground, `ninja -C build-qt612 -j 4`. Never two at once.
- Env in the SAME shell command as every meson/ninja call:
  `PATH=/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin:/home/jeandet/Qt/6.12.0/gcc_64/bin:$PATH`
  and `PKG_CONFIG_PATH=/home/jeandet/Qt/6.12.0/gcc_64/lib/pkgconfig`; `which meson` must print the
  `.venv` path. Never reconfigure; `ldd build-qt612/tests/auto/auto-tests | grep Qt6Core` shows 6.12.0.
- Offscreen suite: `QT_QPA_PLATFORM=offscreen QT_LOGGING_RULES="*.debug=true" meson test -C build-qt612 --num-processes 4`;
  totals = sum of the `Totals:` lines of `build-qt612/meson-logs/testlog.txt`. GPU tests: the whole
  `auto-tests` binary under `xvfb-run -a -s "-screen 0 1600x1200x24" env QT_QPA_PLATFORM=xcb`; after it,
  `pgrep -x Xvfb -a | grep 1600` prints nothing.
- Bench gate (answer 14): baseline = end of milestone 3 in `baseline.md`; rows Task 1 adds are gated
  against their own measurement on the old code. `QCPGpu_*`, `QCPDrawn_*`, `QCPItems_*`, `QCPBars_*` and
  the rows Task 1 adds: 5 %; the rest 15 %. Under machine load, settle a row by five interleaved A/B
  pairs against a build of the task's base commit (worktree in the scratchpad), not by whole-table runs.
- No interim design; no forwarder, alias or `\deprecated` method for a NeoQCP-only API this milestone
  replaces (the H/V spans, `QCPDrawn<>`); their tests move to the new names. 2.x QCustomPlot API keeps
  working until milestone 5.
- An export looks as it did: each ported class matches its reference image, taken on the old code in
  Task 1, through `qcp::test::matchesReference` (Task 1).
- A recording goes stale on every input it reads (selection, style setters, data, attached plottable,
  theme, axis scale type, key-axis orientation): each such setter calls `contentChanged()`.
- Signals through `qcpEmit` / `deliver`; misuse through `QCP_CHECK_OR_RETURN`, never an exception.
- A test fails before the change that makes it pass; a test that already passes is reported as a guard.
- Never bend library behaviour to please a test. Keep code size down: delete what a port replaces.
- Run the clang check (`clang++ -fsyntax-only` per changed `.cpp`, from `compile_commands.json`) before
  each commit that adds a type or a template: GCC accepts code clang rejects.
- Commit with explicit pathspecs; never `subprojects/.wraplock`, `subprojects/jkqtmathtext`, `qt.ini`.

## Review Focus

1. A span follows a pan along its data side and keeps full height (or width) on a pan along its ratio
   side (Task 3 test `spanFollowsADataPanAndKeepsItsRatioSide`).
2. Selecting or restyling a ported object, or changing the theme, redraws it at once (Task 2 test
   `aStyleSetterReRecords`, and each port's `selectionReRecords` test).
3. Error bars follow their plottable's new data without a call on the bars (Task 6 test
   `barsFollowTheirPlottablesData`).
4. A vertical key axis and a log value axis draw financial, statistical box, error bars and intervals
   as before (each port's reference images include a vertical-key and a log case).
5. The 10 000-line-item rows (`QCPItems_Pan10k`, `QCPItems_Zoom10k`, `QCPItems_Replot10k`) do not slow
   down after `QCPItemLine` records (Task 4 bench step).

---

### Task 1: Bench rows and reference images, taken on the old code

**Files:**
- Create: `tests/auto/reference-image.h` (header-only helper)
- Modify: `tests/benchmark/benchmark.cpp` (rows below)
- Create: `tests/auto/test-reference-m4e/` (`.h`, `.cpp`, `reference/*.png`); register it

**Interfaces:**
- Produces: `namespace qcp::test { bool matchesReference(const QImage& image, const QString& testDir, const QString& name, QString* why); }`:
  when `QCP_WRITE_REFERENCE` is set, saves `image` to `tests/auto/<testDir>/reference/<name>.png` and
  returns true; else compares with the saved image: same size, and inside the axis rect (its outer
  row and column excluded, as `test-colormap.cpp:1670-1684` does) at most 20 pixels differ by more than
  32 in any channel. The export replays commands with QPainter, so most pixels match exactly; 20 leaves
  room for a few antialiased edge pixels and none for a misplaced arrow or a shifted span. `why` gets
  the first bad pixel and the count. It replaces the ad-hoc compare in `test-colormap`.
- Produces: rows `QCPErrorBars_Pan10k`, `QCPFinancial_Pan10k` (candlesticks), `QCPStatisticalBox_Pan1k`,
  `QCPIntervals_Pan10k`, `QCPItems_MixedKinds_Pan1k` (100 each of line, straight line, curve, bracket,
  rect, ellipse, pixmap, tracer, rspan, and 50 each of H and V spans), `QCPSpans_Pan100`. Each: build,
  `replot()`, then `QBENCHMARK` a pan step with `rpImmediateRefresh`, as `QCPItems_Pan10k`
  (`benchmark.cpp:1649`) does.
- Produces: reference images (400×300 export, `toPixmap`) named `<class>-<case>.png` for each class to
  port: `errorbars-{plain,vertical-key,log}`, `financial-{candle,ohlc,vertical-key,log}`,
  `statbox-{plain,vertical-key}`, `intervals-{plain,lanes}`, `item-{line-arrows,straightline,curve,
  bracket,rect,ellipse,pixmap,pixmap-scaled,tracer}`, `span-{h,v,r,selected}`, plus `*-selected` and
  `*-legend` (the legend shown, so the icon is in the image) for the four plottables. The test file
  builds each scene once in a helper the port tasks reuse.

- [ ] **Step 1:** Write `reference-image.h` and the reference test class; each test builds its scene
  and calls `matchesReference`.
- [ ] **Step 2:** Run with `QCP_WRITE_REFERENCE=1` on the old code to write the images; run again
  without it: every test passes (a guard on itself). Look at each PNG (Read tool) and say in the report
  that each shows the intended scene.
- [ ] **Step 3:** Add the bench rows; run each three times offscreen with `-median 5` (spans and the
  mixed row under Xvfb too) and record the numbers in the report: they are these rows' baseline.
- [ ] **Step 4: Commit** `test(m4e): bench rows and reference images on the old code`.

### Task 2: Fold the adapter into the plottable and item bases

**Files:**
- Modify: `src/drawable.h`, `src/drawable.cpp` (delete `QCPDrawnAdapter`, `QCPDrawnBase`,
  `QCPDrawnPlottable`, `QCPDrawnItem`, `QCPDrawn`; keep `QCPDrawable`, `QCPDrawnCore`)
- Modify: `src/plottables/plottable.{h,cpp}`, `src/items/item.{h,cpp}` (the bases take over)
- Modify: the eight users (`plottable-bars.h:173`, `plottable-curve.h:87`, `plottable-histogram.h:20`,
  `plottable-histogram2d.h:23`, `plottable-colormap.h:115`, `plottable-colormap2.h:22`,
  `plottable-multigraph.h:29`, `item-text.h:40`) to derive from their plain base
- Test: `tests/auto/test-drawable/`

**Interfaces:**
- Produces: `QCPAbstractPlottable` and `QCPAbstractItem` derive from `QCPDrawable` and own
  `mutable QCPDrawnCore mCore`; `virtual void draw(QCPDrawContext&) const` (default: records nothing);
  `void draw(QCPPainter*) override` (non-final; default replays the recording); `selectTest`, the
  legend icon, `getKeyRange`/`getValueRange` (plottables) and `dataExtent` (items) answer from the core
  when the class records, as the adapter did. `contentChanged()` and `markDirty()` behave as before.
- A class that overrides `draw(QCPPainter*)` (QCPGraph, QCPGraph2, polar) is untouched and works.

- [ ] **Step 1: Failing tests** in `TestDrawable`: `aPlottableRecordsWithoutTheAdapter` — a test
  plottable deriving directly from `QCPAbstractPlottable1D<QCPGraphData>` and overriding only
  `draw(QCPDrawContext&)` records one polyline (`mCore.recordedCommandCount() == 1` after `replot`),
  hit-tests on it (`selectTest` < 1 px on the line) and draws its legend icon; same for a test item
  deriving from `QCPAbstractItem`; both assert `recordingCount() == 0` before the first replot (the
  base owns the recording). `aStyleSetterReRecords`: `bars->setPen(red)` then `replot` →
  `recordingCount` +1. Expected: FAIL to compile (the bases do not record).
- [ ] **Step 2: Implement**; the eight users drop `QCPDrawn<>`; `using QCPDrawable::draw;` lines go.
- [ ] **Step 3:** Run `TestDrawable`, the suite offscreen and under Xvfb, and the bench rows
  `QCPBars_*`, `QCPDrawn_*`, `QCPGpu_Bars10k`, `QCPGpu_MultiGraphPan1M`, `QCPItems_Zoom10k`: no change
  beyond the gate (A/B if loaded).
- [ ] **Step 4: Commit** `refactor(drawable): the plottable and item bases record; QCPDrawn goes`.

### Task 3: One span class; spans record; the span GPU layer goes

**Files:**
- Create: `src/items/item-span.{h,cpp}` (`QCPItemSpan`); delete `src/items/item-hspan.*`,
  `src/items/item-vspan.*`, `src/painting/span-rhi-layer.*`; modify `src/items/item-spanbase.*`,
  `src/items/item-rspan.*`, `src/core.{h,cpp}` and `src/layer.cpp` (span layer hooks), `meson.build`,
  `src/qcp.h`
- Tests: merge `tests/auto/test-hspan` and `test-vspan` into `tests/auto/test-span` (both orientations);
  update `test-rspan`, `test-span-rhi` (becomes the recorded-span GPU test), `test-linked-axes`,
  `test-hit-rule`, `test-creation-mode`, and every test or example naming the H/V classes

**Interfaces:**
- Produces: `QCPItemSpan(QCustomPlot*, Qt::Orientation)`; `Qt::Orientation orientation() const`;
  `QCPRange range() const`, `void setRange(const QCPRange&)`; edges `lowerEdge`, `upperEdge`;
  ops `setRange`, `setOrientation` is not offered (orientation is fixed at construction);
  `plot.add<QCPItemSpan>(Qt::Vertical)`. Mouse dragging of the body and of each edge as today.
- Spans record `rect` + border `polyline`s with `DataXRatioY` (vertical) / `RatioXDataY` (horizontal) /
  `Data` (RSpan) positions; hit test from the recording, with `details` naming the edge or body as
  today's `selectTest` does (an override that tests the edges first).

- [ ] **Step 1: Failing tests** in `TestSpan`: `spanFollowsADataPanAndKeepsItsRatioSide` — a vertical
  span on keys 4..6 of 0..10, replot (settle), then pan the key axis by 2: the span's band is now at the
  pixels of keys 4..6 in the new range; pan the value axis by half its range: the band still covers the
  full plot height. The same for a horizontal span with the axes swapped. (Fails today only by not
  compiling; the check that matters is the data-side move.) `bothOrientationsMatchTheirReferences` (`span-h`, `span-v`, `span-selected`,
  `span-r` through `matchesReference`). `aSpanIsRecorded` (`recordedCommandCount() > 0`).
  `dragMovesAnEdge` per orientation (ported from the old tests). Expected: FAIL to compile.
- [ ] **Step 2: Implement**; delete the H/V classes, the span GPU layer and its hooks.
- [ ] **Step 3:** Run the span tests offscreen and under Xvfb (the GPU rect draws the span), the
  suite, and `QCPSpans_Pan100` (gate: its Task 1 number).
- [ ] **Step 4: Commit** `feat(items): one span class with an orientation; spans are recorded`.

### Task 4: Line-like items record (line, straight line, curve, bracket)

**Files:** `src/items/item-line.*`, `item-straightline.*`, `item-curve.*`, `item-bracket.*`,
`src/lineending.*` (a function that builds an ending's `QPainterPath` in pixels, used by the items and,
if it fits, by `QCPLineEnding::draw`); tests in `tests/auto/test-reference-m4e` and the item tests.

**Interfaces:**
- Produces: `QPainterPath QCPLineEnding::path(const QCPVector2D& direction, double penWidth) const`
  (the ending at the origin, pointing along `direction`), replacing the painter-only geometry.
- Rules: a line records a `polyline` of its two positions plus its endings as `path` at each end (P-6);
  a straight line records every frame (P-5); the curve item records its cubic as a `path` in pixels;
  a bracket records its `path` in pixels. Selection colours come from the recording (`contentChanged()`
  on selection change).

- [ ] **Step 1: Failing tests:** each kind's reference (`item-line-arrows`, `item-straightline`,
  `item-curve`, `item-bracket`) through `matchesReference` after asserting
  `recordedCommandCount() > 0` (fails today: the items do not record); `aPannedPlainLineIsNotReRecorded`
  (settle with one replot, pan 10 px → `recordingCount` unchanged); `aPannedArrowIsRightAfterThePan`
  (an arrowed line, pan, export: the arrow tip pixel is at the end's new pixel); `everyStyleSetterReRecords`
  (each setter the old `draw` read: pen, selected pen, head, tail, and selection → `recordingCount` +1).
- [ ] **Step 2: Implement**; delete each class's painter `draw` and any hit test the recording now
  answers (keep an override only where the old one returned more than a distance).
- [ ] **Step 3:** Run the tests, the suite, and `QCPItems_Pan10k`, `QCPItems_Zoom10k`,
  `QCPItems_Replot10k`, `QCPItems_FirstFrame10k`, `QCPItems_MixedKinds_Pan1k` (5 % gates; A/B if loaded).
- [ ] **Step 4: Commit** `feat(items): lines, straight lines, curves and brackets record`.

### Task 5: Shape items record (rect, ellipse, pixmap, tracer)

**Files:** `src/items/item-rect.*`, `item-ellipse.*`, `item-pixmap.*`, `item-tracer.*`; tests as Task 4.

**Interfaces:**
- Rules: rect and ellipse record `rect` / `ellipse` from their two positions (any position type); the
  pixmap records `image` with its scaled rect (its scaling and aspect rules unchanged); the tracer records
  every frame (P-5) its style (none, plus, crosshair, circle, square) as markers or lines.

- [ ] **Step 1: Failing tests:** references `item-rect`, `item-ellipse`, `item-pixmap`, `item-tracer`
  after `recordedCommandCount() > 0`; `aTracerFollowsItsGraph` (graph data changes → tracer's
  recorded marker moves without a call on the tracer); `selectionReRecords`.
- [ ] **Step 2: Implement**; delete the painter draws; make `QCPAbstractItem::draw(QCPPainter*)` final
  (P-4).
- [ ] **Step 3:** Run the tests, the suite, `QCPItems_MixedKinds_Pan1k`, and a 1 000-tracer pan
  (`QCPItems_Tracers_Pan1k`, added here; its gate is its first measurement, recorded in the report).
- [ ] **Step 4: Commit** `feat(items): rects, ellipses, pixmaps and tracers record`.

### Task 6: Error bars record

**Files:** `src/plottables/plottable-errorbar.{h,cpp}`; tests: new `tests/auto/test-errorbars/`.

**Interfaces:**
- Rules: records with `context.errorBars(...)` (whiskers and symbol gap as today); its dependency
  revision includes the attached plottable's identity and data revision (P-8); a vertical key axis and
  log axes as today; the legend icon as today.

- [ ] **Step 1: Failing tests:** references `errorbars-{plain,vertical-key,log,selected}` after
  `recordedCommandCount() > 0`; `barsFollowTheirPlottablesData` (change the attached graph's keys →
  after `replot` the recorded centres moved); `attachDetachReattach` (attach to A, detach → nothing
  recorded, attach to B → B's keys); `everyStyleSetterReRecords` (error type, whisker width, symbol gap,
  pen, selection).
- [ ] **Step 2: Implement**; delete the painter draw.
- [ ] **Step 3:** Run the tests, the suite, `QCPErrorBars_Pan10k`.
- [ ] **Step 4: Commit** `feat(errorbars): error bars record`.

### Task 7: Financial and statistical box record

**Files:** `src/plottables/plottable-financial.*`, `plottable-statisticalbox.*`; tests:
`tests/auto/test-qcpfinancial/`, new `tests/auto/test-statisticalbox/`.

**Interfaces:**
- Rules: candlesticks record a body `rect` plus a wick `polyline` per entry; OHLC records `polyline`s.
  Width type in plot coordinates → data-space geometry (a pan translates); absolute or axis-rect-ratio
  widths → pixel geometry (`(void)context.transform()`, as `QCPBars::recordBar` does). The two-colour
  mode and selection come from the recording. The statistical box records its box, median, whiskers
  and outlier `markers` the same way. Their legend icons as today.

- [ ] **Step 1: Failing tests:** references `financial-{candle,ohlc,vertical-key,log,selected}` and
  `statbox-{plain,vertical-key,selected}` after `recordedCommandCount() > 0`;
  `aPannedCandlestickIsNotReRecorded` (plot-coordinate width; settle with one replot before counting);
  `everyStyleSetterReRecords` for each class (financial: representation, width, width type, two
  colours, brushes and pens, selection; statistical box: width, whisker width, pens, median pen,
  outlier style, selection).
- [ ] **Step 2: Implement**; delete the painter draws.
- [ ] **Step 3:** Run the tests, the suite, `QCPFinancial_Pan10k`, `QCPStatisticalBox_Pan1k`.
- [ ] **Step 4: Commit** `feat(plottables): financial and statistical box record`.

### Task 8: Intervals record

**Files:** `src/plottables/plottable-intervals.{h,cpp}`; tests: `tests/auto/test-intervals/`.

**Interfaces:**
- Rules: bars, lane backdrops and label rects are recorded `rect`s (one per interval, in data space for
  the key and in pixels or ratio for the lane height, as today's geometry needs); `drawBarsOnGpu` and the
  rebuild-on-GPU helpers go; the label layerable stays painter-drawn (P-7) and keeps its layer above.
  Lanes (`QCPLaneLayout`) unchanged.

- [ ] **Step 1: Failing tests:** references `intervals-{plain,lanes,selected}` after
  `recordedCommandCount() > 0`; `aPannedIntervalIsNotReRecorded` (settle first); `labelsStayAboveTheBars`
  (export: a label pixel over a bar has the label colour); `aDataChangeRedrawsTheLabels` (new data →
  the label layer repaints with the new labels).
- [ ] **Step 2: Implement**; delete the GPU bar path.
- [ ] **Step 3:** Run the tests offscreen and under Xvfb, the suite, `QCPIntervals_Pan10k`.
- [ ] **Step 4: Commit** `feat(intervals): intervals record`.

### Task 9: Finish

**Files:** `changelog.txt`, `docs/plans/neoqcp-3/baseline.md`, `docs/plans/neoqcp-3/backlog.md`,
`docs/plans/neoqcp-3/milestone-4e-rulings.md` (new), the docs and examples that name the H/V spans.

- [ ] **Step 1:** Changelog, milestone 4e section: the classes that now record; `QCPItemSpan` replaces
  `QCPItemHSpan`/`QCPItemVSpan` (no forwarders); `QCPDrawn<>` is gone (a user class derives from
  `QCPAbstractPlottable`/`QCPAbstractItem` and overrides `draw(QCPDrawContext&)`); items on the
  annotations layer as before.
- [ ] **Step 2:** Offscreen suite, Xvfb whole binary, ASan suite, clang check on every changed `.cpp`.
- [ ] **Step 3:** Bench: every row, three runs offscreen and three under Xvfb, table "End of milestone
  4e" against end of milestone 4d; settle rows over gate by A/B.
- [ ] **Step 4:** Rulings file in the shape of `milestone-4d-rulings.md`; deferred minors also in
  `backlog.md`.
- [ ] **Step 5: Commit** `docs(neoqcp-3): end of milestone 4e — rulings, baseline, backlog, changelog`.
