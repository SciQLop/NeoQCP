# NeoQCP 3.0 milestone 6e (multi-panel) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A grid of axis rects in one call (`setAxisRectGrid`, `axisRect(row, column)`), shared x/y axes that link,
hide inner tick labels and fit the union of their data on rescale, a title per rect, and insets placed in a rect's
axis-ratio space — so `examples/user-code/{shared-x,symlog,spectrogram}` compile, build and draw.

**Architecture:** `setAxisRectGrid` owns the cells `[0, rows) x [0, columns)` of the main `QCPLayoutGrid`: it keeps
the rects already there, creates the missing ones, deletes the ones outside, sets stretch factors, aligns every
column's left/right and every row's top/bottom margins with `QCPAlignmentGroup`s, and, when asked, shares axes with
identity `linkTo` links (one source per column / row). Linking stays a star (`linkTo` refuses chains). Rescale fits a
link's union through its source. Titles live in the rect's top margin; insets are rects in the parent's
`insetLayout()`.

**Tech Stack:** C++23, Qt 6.12, meson; QtTest auto-tests; reference pictures under Xvfb.

**Revision:** rev. 2 (pre-flight C1-C9, M1-M10 folded in, see the binding rulings below).

**Spec:** `docs/specs/2026-11-14-neoqcp-3-milestone-6-design.md`; vocabulary `docs/plans/neoqcp-3/m6-vocabulary.md`
rows 45, 46, 52, 57, 61; gaps `docs/plans/neoqcp-3/m6-gaps.md` theme 1 and decisions 13, 14. Layout facts:
`docs/layout.md` ("Several axis rects", "Align edges with an alignment group", "Stacked axis rects").

## Global Constraints

- Branch `neoqcp-3.0`, worktree `~/Documents/prog/NeoQCP-3.0`. Builds and suites on Coder `neoqcp`; reference
  pictures and clang `-fsyntax-only` locally (`build-qt612`, `-j 4`). Never `meson setup` in the local worktree; one
  build or test per build directory at a time; no scratch build trees or binaries in `/tmp` (RAM).
- Failing test first for every change; whole suite green before each commit (Coder `build`, `build-asan`,
  `build-fco`; local Xvfb `auto-tests`).
- No interim designs (decision 14): `setAxisRectGrid` is the one way to lay out several rects; no `addAxisRect`.
  `axisRect(int)` keeps its meaning (index 0 is cell (0, 0)). The 166 `plotLayout()->addElement` calls in tests keep
  working.
- Names exactly as the vocabulary: `void setAxisRectGrid(int rows, int columns, const QCPAxisRectGridOptions&
  options = { })`, `QCPHandle<QCPAxisRect> axisRect(int row, int column) const`, `struct QCPAxisRectGridOptions {
  bool shareX = false; bool shareY = false; QList<double> rowStretch; QList<double> columnStretch; }`,
  `void QCPAxisRect::setTitle(const QString&)`, `QString title() const`, `QCPHandle<QCPAxisRect>
  QCPAxisRect::addInset(const QRectF& axisRatio)`.
- Misuse (bad row/column, non-positive counts, a cell the grid cannot own) goes through the misuse tier
  (`QCP_CHECK_OR_RETURN`): one report, nothing changed, an empty handle where a handle is returned.
- Every behaviour change in `docs/migrating.md`, `changelog.txt` (unreleased section), one line in
  `docs/plans/neoqcp-3/sciqlopplots-migration.md`. Commit with explicit pathspecs; never commit
  `subprojects/.wraplock`, `subprojects/jkqtmathtext`, `qt.ini`, `.superpowers`. Never push.
- State signals with `qcpEmitCurrent`. GCC UBSan lacks float-cast-overflow: `build-fco` must be clean.

## Rulings made in this plan (the pre-flight review may challenge them)

- P-1: margins are always aligned (every column's left/right, every row's top/bottom), shared or not: a grid of
  panels lines its data edges up, as matplotlib's `subplots` does. A rect's colour scale counts in its margin, so a
  column's data edges align with a bar beside one rect only.
- P-2: a share is one identity link per axis to one source: `shareX` links the bottom axis of every rect of a column
  to the BOTTOM rect's bottom axis (the one that keeps its labels); `shareY` links the left axis of every rect of a row
  to the LEFTMOST rect's left axis. Hidden labels: `setTickLabels(false)` on the followers.
- P-3: an identity link (`linkTo(source)` with no forward/inverse) shares the scale too: the follower takes the
  source's scale at link time and follows `scaleChanged`; `setScale` on an identity follower is written to the source
  (as range writes are). A mapped link (forward/inverse given) keeps its own scale. Behaviour change for `linkTo`
  users: migrating.md.
- P-4: rescale of any axis in a link fits the union of the drawn data of the source and its identity followers and
  is applied to the source once (`QCPAxisBase::rescale` resolves `moveTarget()`); `QCustomPlot::rescaleAxes` rescales
  each link once. Mapped followers do not contribute. Decision 13.
- P-5: a second `setAxisRectGrid` undoes the previous call's shares and groups (unlink, labels back on, groups
  dissolved) and applies the new options to the new grid; rects outside the new grid are deleted (decision 14).
- P-6: `setAxisRectGrid` refuses (misuse, no change) when a main-layout cell inside or outside the new grid holds an
  element that is not a `QCPAxisRect` (a title row from `insertRow`, a legend in the main grid): the grid owns the
  main layout. `setTitle` replaces the title-row pattern.
- P-7: `axisRects()` lists the grid cells row after row first, then every other rect (nested layouts, insets) in
  layout order. `axisRect(row, column)` out of range or on a non-rect cell is misuse with an empty handle.
- P-8: the title is drawn centred above the rect's top axis (labels included), in the top margin, font = the top
  axis' `labelFont()` at 1.2x its point size, colour = its `labelColor()`; an empty title reserves nothing.
- P-9: `addInset(axisRatio)` places the inset's DATA rect (inner rect) at `axisRatio` of the parent's inner rect,
  (0, 0) top left, as matplotlib's `inset_axes`; its tick labels fall outside that rect. The inset is not a cell, is
  deleted with its parent, has its own four axes, takes the plot theme, and gets interactions first (deepest rect).

## Pre-flight rulings (rev. 2, binding; `.superpowers/sdd/2026-11-17-neoqcp-3-milestone-6e/preflight.md`)

- C1 -> P-5: a rect the grid drops takes its plottables, items, colour scale and insets (with theirs) with it, through
  their owners' remove paths, then the rect itself (matplotlib's `Axes.remove()`); held handles become empty and the
  legend loses their rows.
- C2: `QCPAxisLink` gains `bool identity` (set when `linkTo` got no functions); P-3 and P-4 read only that flag.
- C3 -> P-3 reverses the 6f deferral ("`linkTo` does not copy the source's scale"): `test-linked-axes.cpp:360` and
  `test-qcpaxisrect.cpp:1177` are rewritten (say why in the commit), `docs/axes.md` and the `setScale` doc follow. At
  link time `setScale` runs only when the scales differ; propagation happens in `scaleDidChange` to identity users
  only; a custom ticker on a follower survives an unchanged scale.
- C4: for identity links only, `linkTo` flattens: linking to an identity follower links to its source; linking an
  axis that drives identity followers moves them to the new source (still a star). Mapped links keep the refusal.
  Unsharing (P-5) puts moved followers back on their recorded original source. Tests: share then
  `setupFullAxesBox(true)`, and the reverse order.
- C5: `rescale` is one non-virtual base function (resolve the identity source, union source + identity followers,
  sign domain from the source's scale, `rescaleTo` on the source) over a virtual per-axis collector ("this axis' own
  data range under a sign domain"); `QCPAxis` and the angular axis override only the collector (cartesian keeps
  `getKeyRange`/`getValueRange`). `rescaleAxes` skips identity followers only.
- C6 -> P-8: the title is a `QCPTextElement` owned by the rect outside the layout tree, on the "axes" layer, placed in
  `upLayout` like `placeColorScale`, its height in `calculateAutoMargin(msTop)` above the top axes, themed in the
  `applyTheme` rect loop. The margin test measures the title box against the top axis' labels, not a raw margin
  delta (the 15 px minimum margin hides small titles).
- C7 -> P-9: `QCPLayoutInset` gains a placement (`ipFreeInner`) that maps the ratio onto the parent area's `rect()`
  (not the inset layout's own 12 px-inset rect) and sets `outerRect = target + element margins`.
- C8 -> P-9: an inset draws above its parent: its background, grid and plottables come after every parent layer
  (its own layers above "annotations", created by `addInset`); `inset->add<...>` and `inset->line(...)` land there.
  Test `anInsetDrawsAboveItsParent`: a parent graph added after `addInset` does not ink over the inset's data rect.
- C9: one function builds a full rect (four axes on "axes", grids on "grid", rect on "background", 12 px inset-layout
  margins, theme); the constructor and the grid both call it. The grid state lives in a small `QCustomPlot` member
  struct: `QPointer`s to the groups and the (follower, original source, labels-were-on) records. The options struct
  is in a header `core.h` includes.
- M1: `setAxisRectGrid` sets `foColumnsFirst`; nested rects are listed breadth-first (walk with a queue), pinned in
  `aGridKeepsTheFirstRect` with an inset.
- M2: unsharing restores only the labels the grid hid (recorded), never a user's own `setTickLabels(false)`.
- M3: `dragAndZoomMoveTheColumn` is a guard (it cannot fail once links exist); `anIdentityLinkSharesTheScale` pins P-3.
- M5: `title` is a Q_PROPERTY with NOTIFY `titleChanged`; the meta-surface E-4b writer table gains three rows (a
  source's `setScale` moves an identity follower's `scale`; `linkTo` moves `scale`; `setAxisRectGrid` moves the
  followers' `tickLabels`).
- M6: P-1 widens vocabulary row 46 (alignment no longer tied to sharing): update the row.
- M7: the grid's groups take the rects' sides from any user alignment group (a cross-plot group, SciQLop's
  `VPlotsAlign` pattern): `docs/migrating.md`.
- M8: P-6 also refuses a `QCPPolarArea` or a nested `QCPLayoutGrid` in the main layout; the SciQLopPlots migration
  note says `setTitle` and the rect's own colour scale replace its title row and colour-scale column
  (`SciQLopPlot.cpp:687, 717`).
- M9: on a second call, missing stretch entries reset kept rows and columns to 1.

## Review Focus

1. A second `setAxisRectGrid` that drops rows while handles to the dropped rects, their plottables and items are held
   (and a drag is in progress on one): the handles become empty, nothing dangles, no crash.
2. `shareX` with a colour scale on one rect of the column (the spectrogram target): the column's data edges line up
   on screen and in an export, and the bar sits beside its own rect only.
3. `rescaleAxes` on a shared axis where one panel has no data, constant data, or a log scale with data <= 0: the
   union is sanitised by the shared scale and never empty or NaN.
4. Drag and wheel zoom on any panel of a shared column move every panel, on linear, log and symlog shared axes.
5. An export (`toPixmap`, `savePdf`) of a never-shown plot with a grid, titles and an inset lays out exactly as the
   screen does, the inset above its parent's data (C8).

---

### Task 1: the grid of rects

**Files:**
- Modify: `src/core.h` (~500), `src/core.cpp` (`axisRect`, `axisRects`, new `setAxisRectGrid`), new
  `src/layoutelements/axisrect-grid.{h,cpp}` (`QCPAxisRectGridOptions`, the grid logic as free functions on the main
  `QCPLayoutGrid`, so `core.cpp` keeps one call), `meson.build`, `cmake/sources.cmake`, `src/qcp.h`.
- Test: new `tests/auto/test-axisrect-grid/` (registered in `tests/auto/meson.build`, `autotest.cpp`).

**Interfaces:**
- Produces: `QCustomPlot::setAxisRectGrid(int, int, const QCPAxisRectGridOptions& = {})`,
  `QCustomPlot::axisRect(int row, int column) const`, `QCPAxisRectGridOptions` (all four fields; Task 2 implements
  the two share flags, this task stores and ignores them), the per-column and per-row `QCPAlignmentGroup`s
  (`P-1`), stretch factors.

- [ ] **Step 1: failing tests** (`TestAxisRectGrid`):
  - `aGridKeepsTheFirstRect`: a graph on `axisRect()` before `setAxisRectGrid(2, 3)`; after, `axisRect(0, 0)` is the
    same object and still holds the graph; `axisRectCount() == 6`; `axisRects()` is row after row (`axisRects()[4]
    == axisRect(1, 1)`); every new rect has four axes like the default rect, the plot theme, and `xAxis()`/`yAxis()`.
  - `stretchFactorsApply`: `{ .rowStretch = { 1, 0.35 } }` on 2 x 1 gives row heights in ratio 1 : 0.35 (±2 px)
    after a replot at 400 x 600; a missing entry is 1; a list longer than the grid is misuse (no change).
  - `aSecondCallGrowsAndShrinks` (Review Focus 1): 3 x 1 with a graph and an item on `axisRect(2, 0)`, a held handle
    to that rect, the graph and the item; `setAxisRectGrid(2, 1)`: the three handles are empty, `axisRectCount() ==
    2`, a replot and an export work; then `setAxisRectGrid(2, 2)` adds two rects.
  - `columnsAndRowsAlign` (P-1): 2 x 2, a long y tick label on (0, 0) only: the left data edges of (0, 0) and (1, 0)
    are equal; the top edges of (0, 0) and (0, 1) are equal.
  - `misuseIsRefused` (P-6, P-7): `setAxisRectGrid(0, 1)`, `(1, -1)`, a `QCPTextElement` in the main layout, and
    `axisRect(5, 0)` each give exactly one misuse report and change nothing / return an empty handle.
- [ ] **Step 2:** run; Expected: does not compile.
- [ ] **Step 3:** implement. A new rect is built as the constructor builds the default one (`new QCPAxisRect(this,
  true)`, layers, theme); deleting outside rects goes through the same path as `QCPHandle::remove()`; groups are
  created with `QCPAlignmentGroup::create(members, sides)` and dissolved on the next call (P-5).
- [ ] **Step 4:** suite (Coder `build`, `build-asan`), local Xvfb.
- [ ] **Step 5:** commit `feat(api): a grid of axis rects in one call`.

### Task 2: shared axes

**Files:**
- Modify: `src/layoutelements/axisrect-grid.cpp`, `src/axis/axis-base.{h,cpp}` (identity link follows the scale,
  P-3).
- Test: `TestAxisRectGrid`, `tests/auto/test-linked-axes/`.

**Interfaces:**
- Consumes: Task 1's grid, groups and stored options.
- Produces: shares per P-2; P-3 for every `linkTo` caller.

- [ ] **Step 1: failing tests:**
  - `shareXLinksEachColumn`: 2 x 2 `{ .shareX = true }`: each column's top bottom-axis `linkSource()` is the bottom
    rect's bottom axis; tick labels hidden on row 0 only; `setRange` on either moves both; columns are independent.
  - `shareYLinksEachRow`: the mirror on the left axes, the leftmost keeps its labels.
  - `anIdentityLinkSharesTheScale` (P-3, test-linked-axes): `follower->linkTo(source)` then `source->setScale(log())`
    gives the follower log; `follower->setScale(symlog())` gives the source symlog; a mapped link
    (`linkTo(source, f, g)`) keeps its own scale.
  - `dragAndZoomMoveTheColumn` (Review Focus 4): a mouse drag and a wheel zoom on row 0 of a shared 2 x 1 move row 1's
    x range identically, for a linear, a log and a symlog shared x axis.
  - `aSecondCallReshares` (P-5, M2): `{ .shareX = true }` then `setAxisRectGrid(2, 1)` (no share): links gone,
    labels the grid hid are back, a label the user hid stays hidden, ranges independent.
  - `sharingAndAFullAxesBox` (C4): share then `setupFullAxesBox(true)` on a cell, and the reverse order: every axis
    follows the column's source, and unsharing restores `xAxis2 -> xAxis`.
- [ ] **Step 2:** run; Expected: fail.
- [ ] **Step 3:** implement.
- [ ] **Step 4:** suite, Xvfb.
- [ ] **Step 5:** commit `feat(api): shared axes in a grid share range, scale and labels`.

### Task 3: rescale fits the union of a link

**Files:**
- Modify: `src/axis/axis-base.cpp` (`rescale`), `src/axis/axis.cpp` (`QCPAxis::rescale`), `src/core.cpp`
  (`rescaleAxes`).
- Test: `tests/auto/test-linked-axes/`, `TestAxisRectGrid`.

**Interfaces:**
- Consumes: identity links (Task 2).
- Produces: P-4.

- [ ] **Step 1: failing tests:**
  - `rescaleFitsTheUnion` (the shared-x target's data): row 0 keys 0..8, row 1 keys 2..10, shared x:
    `rescaleAxes()` gives both x ranges [0, 10] (today always [2, 10]: the source, row 1, rescales last); the same through `ax->xAxis()->
    rescale()` on either panel.
  - `aUnionUnderTheSharedScale` (Review Focus 3): shared log x, one panel with keys {-1, 0, 1, 10}, the other with
    {100, 1000}: the union is positive, [1, 1000]; a panel with no data and one with constant data still give a valid
    range; never NaN.
  - `aMappedFollowerDoesNotWiden`: a follower with forward/inverse functions keeps today's behaviour.
- [ ] **Step 2:** run; Expected: fail.
- [ ] **Step 3:** implement: one union per link, applied to the source once; `rescaleAxes` skips followers.
- [ ] **Step 4:** suite, Xvfb.
- [ ] **Step 5:** commit `feat: a shared axis fits the union of its panels`.

### Task 4: a title per rect

**Files:**
- Modify: `src/layoutelements/layoutelement-axisrect.{h,cpp}` (`setTitle`, `title`, top margin, draw).
- Test: `tests/auto/test-qcpaxisrect/`, one reference picture.

**Interfaces:**
- Produces: `QCPAxisRect::setTitle(const QString&)`, `QString title() const`, a `titleChanged(const QString&)` state
  signal (`qcpEmitCurrent`), P-8.

- [ ] **Step 1: failing tests:**
  - `aTitleTakesTheTopMargin`: after `setTitle("interpolation=True")` and a replot, the rect's top margin grew by the
    title's text height plus its gap; `setTitle({})` gives the old margin back; the text is drawn centred above the
    top axis (painter export: ink in that band, none before).
  - `titlesInARowAlign`: 1 x 2, a title on (0, 1) only: the data tops of both rects are equal (P-1).
  - reference picture `grid-titles`: 2 x 2 with a title on each rect.
- [ ] **Step 2:** run; Expected: does not compile.
- [ ] **Step 3:** implement: the title height is added in `calculateAutoMargin(QCP::msTop)`; drawn on the "axes" layer
  through the draw context; the alignment group reads it with the rest of the margin.
- [ ] **Step 4:** suite, Xvfb, pictures (only the new one).
- [ ] **Step 5:** commit `feat(api): a title per axis rect`.

### Task 5: insets

**Files:**
- Modify: `src/layoutelements/layoutelement-axisrect.{h,cpp}` (`addInset`), `src/layout.{h,cpp}` if the inset layout
  needs a placement that targets the inner rect (P-9).
- Test: `tests/auto/test-qcpaxisrect/`.

**Interfaces:**
- Produces: `QCPHandle<QCPAxisRect> QCPAxisRect::addInset(const QRectF& axisRatio)`, P-9.

- [ ] **Step 1: failing tests:**
  - `anInsetSitsInTheParentsAxisRatio` (C7, on the DEFAULT rect, whose inset layout has 12 px margins):
    `addInset(QRectF(0.5, 0.03, 0.47, 0.47))` on a 400 x 300 plot: the inset's
    inner rect is that fraction of the parent's inner rect (±1 px), measured from the top left.
  - `anInsetFollowsItsParent`: an inset on (0, 0): resizing the plot moves it; `axisRects()` lists it after the
    cells; `axisRect(0, 0)` is still the parent.
  - `anInsetDiesWithItsParent`: an inset on (1, 0); `setAxisRectGrid(1, 1)` empties the inset's handle and its
    plottables' handles (C1).
  - `anInsetDrawsAboveItsParent` (C8).
  - `anInsetTakesTheMouseFirst`: a drag inside the inset moves the inset's ranges, not the parent's.
  - `anInsetExportsLikeTheScreen` (Review Focus 5): a never-shown plot, grid 2 x 1, titles, an inset: `toPixmap`
    places the inset where the shown plot does (compare its inner rect).
- [ ] **Step 2:** run; Expected: does not compile.
- [ ] **Step 3:** implement.
- [ ] **Step 4:** suite, Xvfb.
- [ ] **Step 5:** commit `feat(api): insets in a rect's axis-ratio space`.

### Task 6: the targets build and draw

**Files:**
- Modify: `examples/meson.build` (build `examples/user-code/{shared-x,symlog,spectrogram}` under `with_examples`;
  each as it is, no edit unless a target is wrong, and then say why), `examples/user-code/README.md` (which targets
  build), `docs/layout.md` (a "Grid of rects" section first, with shared axes, stretch, titles, insets; the manual
  "Stacked axis rects" recipe points to it), `docs/snippets/layout.cpp` (compiled snippets), `docs/migrating.md`,
  `changelog.txt`, `docs/plans/neoqcp-3/sciqlopplots-migration.md`.
- Test: reference pictures `shared-x-target`, `symlog-target`, `spectrogram-target`, drawn by the targets' own scene
  code copied into a test (as 6f's `spectrogram-single-rect`), Review Focus 2 asserted on the spectrogram picture
  (column data edges equal, bar beside the top rect only).

- [ ] **Step 1:** the three pictures' tests and the meson targets; run; Expected: the tests fail (no reference yet),
  the three targets build.
- [ ] **Step 2:** write the pictures locally (Xvfb, `build-qt612`), look at each, describe it in the report.
- [ ] **Step 3:** docs and snippets (`doc-snippets` green).
- [ ] **Step 4:** whole suite, Xvfb, pictures (only the new ones), examples build on Coder.
- [ ] **Step 5:** commit `feat: the shared-x, symlog and spectrogram targets build`.

### Task 7: review and finish

- [ ] Final review by a fresh reviewer over the plan's range; one fix batch.
- [ ] Whole offscreen suite, ASan+UBSan, `build-fco`, Xvfb `auto-tests`, local pictures, clang `-fsyntax-only` on
  changed `.cpp`, examples (meson and the three user-code targets), `check-sources.py`, `doc-snippets`.
- [ ] `docs/plans/neoqcp-3/milestone-6e-rulings.md`; `m6-gaps.md` order marked done; memory.
