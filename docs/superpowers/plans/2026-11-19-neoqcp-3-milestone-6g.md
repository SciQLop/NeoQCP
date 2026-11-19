# NeoQCP 3.0 milestone 6g (fill and zoom indicator) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The fill-between and inset-zoom targets compile unchanged, run, and have reference pictures: a
`QCPFillBetween` plottable between two lines (or two components), with one brush where the first line is above and
another where it is below, cut at the exact crossings, broken at gaps and following both lines' data; and a zoom
indicator joining an inset to the region of its parent it shows. The graph's component fills between columns and
between graphs are deleted (gaps decision 1).

**Architecture:** `QCPFillBetween` is a plottable that owns no data: at draw time it reads both lines' sources, builds
the strip rows on the union of their keys (`qcp::fill`, pure functions), splits them at each crossing into runs that
know which side is on top, and records one `QCPDrawContext::fillBetween` per run with that side's brush. It is a
dependent of both lines (`QCPPlottable::addDependent`), so a `setData` on either re-records it. The zoom indicator is
an item of the parent rect that records, at draw time, a box over the inset's axis ranges in the parent's data and
two lines from that box to the inset's data-rect corners, chosen by matplotlib's rule.

**Tech Stack:** C++23 (GCC 14 floor), Qt 6.12, meson + `cmake/sources.cmake`, QtTest (`tests/auto`, one binary).

**Spec:** `docs/specs/2026-11-14-neoqcp-3-milestone-6-design.md` (stage 2 item 7, "6g"); the change list is
`docs/plans/neoqcp-3/m6-gaps.md` rows "A fill-between plottable" and "Zoom inset indicator", maintainer decision 1;
the targets are `examples/user-code/{fill-between,inset-zoom}/main.cpp`.

## Global Constraints

- The targets compile **unchanged** from their committed text, except removing "Does not compile yet." from their
  first comment line.
- No interim designs: no deprecated forwarders, no old name kept beside the new one (`setComponentFillBetween`,
  `setComponentFillToGraph` and `QCPGraph::FillToGraph` go). `setComponentFill(column, value)`,
  `setComponentFillBrush`, `clearComponentFill` and `componentFill` stay.
- Every break goes into `docs/migrating.md` (the 3.0 section) and one line into
  `docs/plans/neoqcp-3/sciqlopplots-migration.md`.
- Every rule from earlier milestones stands: signals through `qcpEmit`, a slot may delete the object or the plot,
  handles for plot-owned objects, a rect's misuse goes through `QCP_CHECK_OR_RETURN` (`src/misuse.h`).
- Reference scenes never use `std::*_distribution` other than the ones already in the tree: noise comes from the
  engine's own output (the 6d CI lesson).
- Failing test first for every change. Whole suite green locally under Xvfb and on Coder (`build`, `build-asan`,
  `build-fco`); reference pictures drawn and checked locally (fonts). clang `-fsyntax-only` clean on changed files.
- `-j 4` locally, one build per build directory at a time, never `meson setup` in the local worktree, no build trees
  in /tmp. Explicit pathspecs; never commit `subprojects/.wraplock`, `subprojects/jkqtmathtext`, `.superpowers`,
  `x/`. Never push.

## Pre-flight decisions (for the maintainer)

- **P-1 (which brush where):** `brush` fills where the first line is above the second, `belowBrush` where it is below
  (matplotlib's two `where=` calls of the target's demo). `belowBrush` unset fills both sides with `brush`;
  `Qt::NoBrush` leaves that side empty. Equal values belong to the run they are in (no cut).
- **P-2 (which keys):** the rows are the union of both lines' keys over the key range they share, each line
  interpolated linearly at the other's keys. Today's graph-to-graph fill samples the partner only at the owner's keys,
  so a partner's peak between two owner keys is cut off; matplotlib asks for shared x and has no such case. A NaN or a
  key gap in either line breaks the fill there. The first line's step style shapes both edges, as today.
- **P-3 (draw order):** the fill draws under the lines it joins: `fillBetween` places it just below the lower of the
  two in their layer, as matplotlib draws collections under lines. A fill made with `add<QCPFillBetween>()` keeps
  the add order.
- **P-4 (a line goes):** the fill stays in the plot and draws nothing while either line is deleted or moved to
  another rect, and draws again if `setLines` names live lines. It is not deleted with them.
- **P-5 (clicks and legend):** a fill is not selectable by default (`QCP::stNone`), so a click inside it reaches the
  lines and the pan. It joins the legend under the same rule as `line()` (`options.name`).
- **P-6 (two rects):** lines on different key axes, or in different coordinate systems, are misuse: one report, an
  empty handle. Lines on one key axis and two value axes fill as today's graph-to-graph fill does (the second line's
  values go through its pixel onto the first's value axis).
- **P-7 (zoom indicator):** `ax->zoomIndicator(inset, options)` on the parent of an inset made by `addInset`. Its box
  is the inset's bottom-axis range by its left-axis range, in the parent's bottom and left axes; two connectors from
  box corners to the same corners of the inset's data rect, chosen by matplotlib's visibility rule (Task 3). It is
  recomputed at each draw, so panning the inset or resizing the plot moves it. It draws nothing while the inset is
  hidden or deleted. A rect that is not an inset of this one is misuse. Default look: `QCPShapeOptions {}` (black
  1 px pen, no brush).

## Review Focus

1. A fill whose lines change length on `setData` (fewer or more points than the last draw, or one line emptied): no
   stale rows, no crash (Task 1 test `aFillFollowsALineThatShrinks`).
2. A crossing exactly on a data key, and lines that touch without crossing: no zero-width sliver of the other brush
   (Task 1 pure tests).
3. A log value axis with a line at or below zero: the edge goes to the axis bound on zero's side as the component
   fill does (Task 1 test `aFillOnALogAxisClosesAtTheBound`).
4. The second line deleted from a slot connected to the first line's `dataChanged`: the next replot draws the line
   alone (Task 1 test `aFillWhoseLineIsDeletedDrawsNothing`).
5. The inset deleted while the indicator stays, then a replot; and the indicator removed while the inset stays
   (Task 3 test `anIndicatorOutlivesItsInset`).

---

### Task 1: `QCPFillBetween`; the fill-between target builds

**Files:**
- Create: `src/plottables/plottable-fillbetween.{h,cpp}` (add to `meson.build` sources and moc headers,
  `cmake/sources.cmake`, `src/qcustomplot.h`)
- Modify: `src/plottables/graph-fill.{h,cpp}`, `src/layoutelements/layoutelement-axisrect.h`
  (`QCPFillOptions`, `fillBetween` declaration), `src/layoutelements/axisrect-shortcuts.h` (its definition),
  `examples/user-code/fill-between/main.cpp` (comment line only), `examples/user-code/meson.build`,
  `examples/user-code/README.md`, `docs/graphs.md` (a "Fill between two lines" section with a compiled snippet in
  `docs/snippets/graphs.cpp`)
- Test: `tests/auto/test-fill-geometry/…` (pure functions), a new `tests/auto/test-fill-between/…` class,
  `tests/auto/test-reference-m4e/…` (picture)

**Interfaces:**
- Produces:
  - `struct QCPFillOptions { QBrush brush; std::optional<QBrush> belowBrush; int firstComponent = 0;
    int secondComponent = 0; QString name; };` next to `QCPLineOptions`.
  - `QCPHandle<QCPFillBetween> QCPAxisRect::fillBetween(const QCPHandle<QCPGraph>& first,
    const QCPHandle<QCPGraph>& second, const QCPFillOptions& options = { });` (the same graph twice with two
    components is a fill between columns).
  - `class QCPFillBetween : public QCPPlottable`: `void setLines(QCPGraph* first, int firstComponent,
    QCPGraph* second, int secondComponent)`; `QCPGraph* firstLine() const`, `int firstComponent() const`, and the
    same for second; `void setBelowBrush(std::optional<QBrush>)`, `std::optional<QBrush> belowBrush() const`
    (`brush` is the plottable's own). It reports no key or value range of its own (the lines carry the fit).
  - In `qcp::fill`: `std::vector<StripPoint> unionRows(std::span<const SeriesRow> first,
    std::span<const SeriesRow> second)` (P-2) and `struct SideRun { std::vector<StripPoint> rows; bool firstAbove; };
    std::vector<SideRun> splitAtCrossings(std::span<const StripPoint> rows)`: each crossing inside a segment gets a
    made-up row (`sourceRow = -1`) at the exact linear crossing that ends one run and starts the next; a break
    (`breakBefore`, NaN) ends a run without a made-up row.

- [ ] **Step 1: Write the failing pure tests** in the class of `tests/auto/test-fill-geometry`:
  `unionRowsInterpolatesBothLines` (first keys {0, 2}, values {0, 2}; second keys {1}, value {5}, then {0, 1, 2} /
  {5, 5, 5}: rows at keys 0, 1, 2 with a = 0, 1, 2 and b = 5, 5, 5), `unionRowsCropToTheSharedRange` (first 0..3,
  second 1..2: keys 1..2 only), `unionRowsBreakAtANaN`, `splitAtCrossingsCutsAtTheExactPoint` (the target's data:
  keys {0, 1, 2, 3}, a = {0.8, 0.8, 0.2, 0.2}, b = {0, 0, 1, 1}: two runs, keys {0, 1, 1.5} with `firstAbove` true
  and {1.5, 2, 3} with false, the made-up row at a = b = 0.5), `aCrossingOnAKeyMakesNoSliver` (a = {1, 0, -1},
  b = {0, 0, 0}: two runs meeting at key 1, no run of zero width), `touchingLinesStayOneRun` (a = {1, 0, 1},
  b = {0, 0, 0}: one run, `firstAbove` true).
- [ ] **Step 2: Build, expect compile failure** (`unionRows`, `splitAtCrossings` missing):
  `PATH=/home/jeandet/Qt/6.12.0/gcc_64/bin:$PATH ninja -C build-qt612 -j 4 tests/auto/auto-tests`
- [ ] **Step 3: Implement the two functions** in `graph-fill.cpp`; run `TestFillGeometry`, expect PASS.
- [ ] **Step 4: Write the failing plottable tests** in a new `TestFillBetween` (register it in
  `tests/auto/meson.build` and the runner the way `TestFillGeometry` is): `aFillTakesItsBrushBySide` (the target's
  data on a 400 x 300 plot: the pixel at data (0.5, 0.4) carries the green of `brush`, at (2.5, 0.6) the red of
  `belowBrush`, at (0.5, 0.9) the background), `oneBrushFillsBothSides` (no `belowBrush`), `aNoBrushSideStaysEmpty`,
  `aFillFollowsItsLinesData` (after `second->setData(x, y2 + 0.5)` the pixel at (0.5, 0.25) is the background and
  at (0.5, 0.65) still green), `aFillFollowsALineThatShrinks` (second line set to two points, then empty: no crash,
  nothing drawn for an empty line), `aGapBreaksTheFill` (a NaN at key 2 in the first line: background at (1.5, mid)
  and (2.5, mid)), `aFillBetweenTwoComponents` (one 2-column graph, `firstComponent = 0, secondComponent = 1`),
  `aFillOnALogAxisClosesAtTheBound` (value axis log, the second line 0 everywhere: the fill reaches the axis' lower
  bound), `aFillDrawsUnderItsLines` (P-3: the fill's index in the layer's children is below both lines'),
  `aFillWhoseLineIsDeletedDrawsNothing` (P-4, with the deletion from a slot on the other line's `dataChanged`),
  `linesOfTwoRectsAreRefused` (P-6: empty handle, one misuse report), `aFillIsNotSelectableByDefault` (P-5: a click
  inside it selects nothing and `selectable()` is `QCP::stNone`).
- [ ] **Step 5: Implement `QCPFillBetween` and `fillBetween`.** At draw time: rows from `unionRows` in the first
  line's frame (move the partner-to-axis conversion, `fillValue` / `dataSpaceFillValue` edge rule and the binning
  and step shaping out of `QCPGraph` into `qcp::fill` free functions taking the axes, so the graph's own constant
  fills keep using them); `splitAtCrossings` on the raw rows; then shape (bin, step) each run and record it with its
  side's brush. Register as a dependent of both lines and drop the registration when a line changes; connect to
  each line's `destroyed` for P-4. Keep a misuse check for components out of range at draw time (draw nothing).
- [ ] **Step 6: Run `TestFillBetween`, expect PASS.** Remove "Does not compile yet." from the target, add
  `fill-between` to the `foreach` in `examples/user-code/meson.build`, update the README's list, build.
- [ ] **Step 7: Reference picture** `fill-between-target` in `TestReferenceM4e`, the target's scene copied at
  640 x 400 (title included). Draw it locally under Xvfb (`QCP_WRITE_REFERENCE=1` the first time) and look at it:
  green left of 1.5, red right of it, the dashed lines and discs above the fill.
- [ ] **Step 8: Docs:** the `docs/graphs.md` section (both brushes, components, the key rule P-2, the draw order
  P-3) with a compiled snippet; run the doc snippets. Then the whole suite; commit
  `feat(api): a fill between two lines; the fill-between target builds`.

### Task 2: The graph's fills between columns and between graphs go

**Files:**
- Modify: `src/plottables/plottable-graph.{h,cpp}` (delete `setComponentFillBetween`, `setComponentFillToGraph`,
  `FillToGraph`, the `int` and `FillToGraph` alternatives of `Fill::target` and everything only they use:
  `fillToGraphRows`, `pruneLostPartnerFills`, `partnerDied`, `canFillTo`, `fillsAcrossValueAxes`,
  `releasePartnerDependencies`; the `QCPGraphOps` forwarders that name them), `examples/plots/mainwindow.cpp`,
  `tests/benchmark/benchmark.cpp`, `docs/graphs.md`, `docs/styling.md`, `docs/snippets/{graphs,styling}.cpp`,
  `docs/migrating.md`, `docs/plans/neoqcp-3/sciqlopplots-migration.md`, `docs/plans/neoqcp-3/m6-gaps.md` (mark
  decision 1 done)
- Test: `tests/auto/test-multigraph/…` (60 uses), `tests/auto/test-user-system/…` (10),
  `tests/auto/test-coordinate-system/…` (3), `tests/auto/test-reference-m4e/…` (2), `tests/auto/test-qcpgraph/…` (1)

**Interfaces:**
- Consumes: Task 1's `QCPAxisRect::fillBetween`, `QCPFillBetween`, `QCPFillOptions`.
- Produces: `QCPGraph::Fill { int owner; double target; QBrush brush; }` (only the constant form remains).

- [ ] **Step 1: Port each test that used a deleted call** to `fillBetween` when it tests a fill's look or behaviour
  (same assertions, now on the fill plottable), and delete it when it tested only the old API's own bookkeeping
  (partner pruning, replacing one owner's fill). List each test and its choice in the report. A test that crossed
  value axes or user coordinate systems keeps its case on the new plottable (P-6).
- [ ] **Step 2: Build, expect the ported tests to compile and the old calls to fail** where they still exist; then
  delete the API. Build again.
- [ ] **Step 3: Run the whole suite locally; every reference picture that moved is redrawn only after checking that
  the change is the order rule P-3 or the key rule P-2** (say which in the report, one line per picture), and looked
  at.
- [ ] **Step 4: Docs and examples:** `docs/migrating.md` rows (`setComponentFillBetween(a, b, brush)` →
  `ax->fillBetween(g, g, { .brush = brush, .firstComponent = a, .secondComponent = b })`; `setComponentFillToGraph`
  → `fillBetween(g, other, …)`), the SciQLopPlots line, the guide pages and snippets, `examples/plots`, the
  benchmark. Run the doc snippets.
- [ ] **Step 5: Whole suite; commit** `refactor(api)!: fills between lines are QCPFillBetween; the graph's go`.

### Task 3: The zoom indicator; the inset-zoom target builds

**Files:**
- Create: `src/items/item-zoom-indicator.{h,cpp}` (build lists and `src/qcustomplot.h` as in Task 1)
- Modify: `src/layoutelements/layoutelement-axisrect.h` (declaration), `src/layoutelements/axisrect-shortcuts.h`
  (definition), `examples/user-code/inset-zoom/main.cpp` (comment line only), `examples/user-code/meson.build`,
  `examples/user-code/README.md`, `docs/layout.md` (the inset section: one paragraph and a compiled snippet in
  `docs/snippets/layout.cpp`, or the snippet file that section already uses), `docs/plans/neoqcp-3/m6-gaps.md`
- Test: a new `tests/auto/test-zoom-indicator/…` class, `tests/auto/test-reference-m4e/…` (picture)

**Interfaces:**
- Produces:
  - `QCPHandle<QCPItemZoomIndicator> QCPAxisRect::zoomIndicator(const QCPHandle<QCPAxisRect>& inset,
    const QCPShapeOptions& options = { });`
  - `class QCPItemZoomIndicator : public QCPItem`: `void setInset(QCPAxisRect*)`, `QCPAxisRect* inset() const`,
    `setPen` / `setBrush` (the box's brush; the connectors use the pen).
  - `std::array<bool, 4> qcp::zoomConnectorVisibility(const QRectF& box, const QRectF& inset)`, both in pixels
    (y down); index 0 bottom-left, 1 top-left, 2 bottom-right, 3 top-right; connector i joins corner i of the box to
    corner i of the inset. matplotlib's rule (`inset.py`), written for y down:

    ```cpp
    const bool x0 = box.left() < inset.left(), x1 = box.right() < inset.right();
    const bool y0 = box.bottom() > inset.bottom(), y1 = box.top() > inset.top();
    return { x0 != y0, x0 == y1, x1 == y0, x1 != y1 };
    ```

- [ ] **Step 1: Failing tests** in `TestZoomIndicator`: `connectorsForAnInsetUpRight` (box left 10, right 20, top 80,
  bottom 90; inset left 50, right 90, top 10, bottom 50 → {false, true, true, false}),
  `connectorsForAnInsetDownRight` (box 10..20 x 10..20; inset 50..90 x 50..90 → {true, false, false, true}),
  `theBoxCoversTheInsetRanges` (the target's scene at 560 x 540: ink of the pen on the box edge at parent data
  (-1.5, -2.2) and (-1.2, -1.9), none at (-1.2, -2.2) inside), `theBoxFollowsThePannedInset` (after
  `inset->xAxis()->setRange(0, 0.6)`, the old edge has no ink and the new one has), `connectorsEndAtTheInsetCorners`
  (ink one pixel outside the inset's data-rect top-left and bottom-right corners along each connector, then the same
  after `plot.resize(700, 600)` and a replot), `aHiddenInsetHidesTheIndicator`, `anIndicatorOutlivesItsInset`
  (delete the inset, replot: no ink, no crash; the other order too), `aRectThatIsNotAnInsetIsRefused` (a grid cell, or
  an inset of another rect: empty handle, one misuse report).
- [ ] **Step 2: Build, expect compile failure.**
- [ ] **Step 3: Implement.** The item lives in the parent rect (`add<QCPItemZoomIndicator>()`), records at draw time
  a rect in the parent's data (`context.rect(QRectF, pen, brush)`) and the two visible connectors as lines from a
  data position to an absolute pixel position. Its recording depends on the inset's ranges and pixel rect: call
  `update()` on the inset's bottom/left axes `rangeChanged` and `scaleChanged`, and read the transform in `draw` so
  a layout change re-records it (as `recordFills` does for its log baseline). A `QPointer` to the inset; nothing is
  recorded while it is null or not visible.
- [ ] **Step 4: Run `TestZoomIndicator`, expect PASS.** Remove "Does not compile yet." from the target, add
  `inset-zoom` to the `foreach`, update the README, build.
- [ ] **Step 5: Reference picture** `inset-zoom-target` in `TestReferenceM4e`, the target's scene copied at
  560 x 540. Draw it locally and look at it: the box around the dipole's lower-left lobe, two connectors to the inset's
  top-left and bottom-right corners, the inset above them.
- [ ] **Step 6: Docs** (`docs/layout.md` paragraph and snippet, `m6-gaps.md` rows marked done); run the doc
  snippets; whole suite locally, then Coder `build` (the log must show `user-code-fill-between` and
  `user-code-inset-zoom` compiled and linked), `build-asan`, `build-fco`; commit
  `feat(api): a zoom indicator for an inset; the inset-zoom target builds`.
