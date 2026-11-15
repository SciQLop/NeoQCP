# NeoQCP 3.0 milestone 6c — drawing context Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** a plot type or annotation draws itself with short, unit-explicit calls (`line`, `circle`, `polygon`,
`rect`, `roundedRect`, boxed `text`, `5_px`, `data + PixelOffset`), and what it records later is drawn over what it
recorded earlier, text included, on screen and in an export.

**Architecture:** the helpers are thin recordings over the existing command set where one exists (`line` is a
two-point `polyline`, `rect(QRectF)` is `rect(Data, Data)`), and new commands only where the replay must know the
shape (pixel-radius circle, rounded rect, boxed text). Call order: today a layer composites its QPainter buffer, then
its GPU plottable geometry, then its markers, so text, paths and `painter()` callbacks land under every GPU command of
the layer. 6c makes the GPU replay draw a painter-only command as an image quad at its place in the recording, the
mechanism raster fills already use (`addRasterCommand`), with its own budget, and makes the export replay follow call
order.

**Tech Stack:** C++20, Qt 6.12, meson + `cmake/sources.cmake`, QtTest.

**Spec:** `docs/specs/2026-11-14-neoqcp-3-milestone-6-design.md`. Input: `docs/plans/neoqcp-3/m6-gaps.md` theme 3
rows "A pixel length type", "New drawing-context helpers", "A data point plus a pixel offset", "Call order is screen
order"; decision 12. Exact signatures: `docs/plans/neoqcp-3/m6-vocabulary.md` rows 21-33 (copy them from there).

## Calls

- **C-1** `QCP::PixelLength`, the `_px` literals in `namespace QCP::Literals`, `operator-(Pixels, Pixels)`,
  `QCP::DataOffset` with its two `operator+`, as the vocabulary rows say. `DataOffset` is a new `QCP::Position`
  alternative: every `std::visit` over a position handles it (resolve = the data point's pixel plus the offset; it
  moves with the data on a pan; the item index, hit tests, `fallbackPixelBounds`, `QCPItemPosition` all handle it).
- **C-2** Helpers on `QCPDrawContext`, signatures from the vocabulary: `line`, `circle(Position, double radius, ...)`
  (data radius per axis, an ellipse in pixels), `circle(Position, PixelLength, ...)`, `polygon(const QPolygonF&,
  ...)` (data vertices), `polygon(std::initializer_list<Position>, ...)`, `rect(const QRectF&, ...)`,
  `roundedRect(Position, Position, PixelLength, ...)`, `text(Position a, Position b, const QString&, const
  QCPTextStyle&)` (laid out inside the box, words wrapped, `style.padding` inside the box). Each draws the same picture
  through the GPU replay and the QPainter replay.
- **C-3** A pixel-radius circle is not a `markers` command: markers live on the scatter layer, which draws after every
  plottable of the layer, and would break C-4. It is GPU geometry (a fan) or a raster image on the plottable layer.
- **C-4 (decision 12)** Inside one recording, a command recorded later draws over one recorded earlier, whatever its
  kind (GPU geometry, raster image, text, path, image, `painter()`). Across recordings on one layer, drawables draw in
  the layer's child order (add order). On screen, a painter-only command (text, path, `painter()`) becomes an image
  quad on the plottable layer at its place in the recording, as `addRasterCommand` does for raster fills; it gets a
  budget of its own (not `kMaxImages`, which raster fills keep), large enough for the targets (a HUD, a few callouts),
  and past the budget the existing fallback applies (the whole recording painted under the layer, one warning).
  Consecutive painter-only commands of one recording may share one image. An export replays each drawable in call
  order (`QCPReplayKinds::All`), so it stacks like the screen.
- **C-5** Non-drawable layerables (a plain `QCPLayerable` subclass painting through `draw(QCPPainter*)`) still paint
  into the layer's buffer, under its GPU content. That is the documented limit; it goes in `docs/limits.md`.
- **C-6** `QCPItemText` honours `QCPTextStyle::mathText` (carried from 6b R-8): an item text with `mathText` on is
  measured and drawn through `QCPLabelRenderer::defaultRenderer`, as the drawing context's `text()` already does, and
  `ax->text()` passes it on.
- **C-7** No forwarders, no deprecated aliases. Builds and suites on the Coder workspace `neoqcp`; reference pictures
  locally (fonts); one build per tree at a time.

## Global Constraints

- Each break or behaviour change goes into `docs/migrating.md`, `changelog.txt` (the milestone 6 entry, before 5f) and
  one line each in `docs/plans/neoqcp-3/sciqlopplots-migration.md`. Never rewrite released changelog sections.
- `cmake/sources.cmake` and `meson.build` stay in sync (`tests/cmake/check-sources.py`) if a file is added.
- The `examples/user-code/` targets are not edited and not built in 6c (they need 6d).
- Failing test first for every new behaviour (QtTest under `tests/auto/`). GPU-path behaviour is tested by reading
  back a frame (as `tests/auto/test-repaint` and the `*-rhi` tests do) on Xvfb.
- Performance: a recording with no painter-only command must replay exactly as today (no new allocation, no new pass).
  Run the `benchmark` rows that replay recordings before and after Task 3 on Coder; a row slower by more than 5 % is a
  finding.
- Commit with explicit pathspecs; never `subprojects/.wraplock`, `subprojects/jkqtmathtext`, `qt.ini`, `.superpowers`.
  Push only when asked.

## Review Focus

1. A pan or zoom of a recording that mixes GPU geometry and text: the text image moves with its data (or stays, for
   pixel positions) and stays above the geometry recorded before it; no stale image after a re-record.
2. A layer with more painter-only commands than the budget: one warning, everything still drawn, no object split
   between stacking levels.
3. `DataOffset` on a log axis, a reversed axis, a polar area, and a position whose data point is NaN: no crash,
   no point at infinity drawn (a NaN point draws nothing, as a NaN `Data` does today).
4. Boxed text whose box is smaller than the padding or the text: no negative rect, text clipped or overflowing the
   same way in both replays; an empty string draws only the box.
5. An export (`savePng`/`savePdf`) of a layer where text is recorded before and after geometry, compared with the
   screen frame: same stacking.

---

### Task 1: Units and positions (C-1)

**Files:** `src/coords.h`, every `std::visit` over `QCP::Position` in `src/` (`grep -rn "std::visit" src`, plus the
`overloaded`-style visitors), a test under `tests/auto/test-coords/`.

- [ ] **Step 1:** Tests: `static_assert(std::is_same_v<decltype(5_px), QCP::PixelLength>)`, `2.5_px` gives
  `px == 2.5`; `QCP::Pixels{10,20} - QCP::Pixels{4,5}` is `PixelOffset{6,15}`; `QCP::Data{1,2} + QCP::PixelOffset{3,4}`
  is a `DataOffset` and `+ PixelOffset{1,1}` again adds up; a recording with a `DataOffset` rect resolves to the data
  pixel plus the offset, and after a pan the rect moved with the data and kept its offset; an item position set to a
  `DataOffset` reports the same pixel; Review Focus 3.
- [ ] **Step 2:** Implement. Build on Coder, whole suite. Migration note (one row: new alternative; user visitors over
  `QCP::Position` must handle it). Commit `feat(api): QCP::PixelLength, _px, DataOffset`.

### Task 2: The helpers (C-2, C-3)

**Files:** `src/painting/draw-context.{h,cpp}`, `src/painting/draw-commands.h` (only for new commands),
`src/painting/command-replay-painter.cpp`, `src/painting/command-replay-rhi.cpp`, `tests/auto/test-draw-context/`,
a reference-picture test (local).

- [ ] **Step 1:** Tests: each helper records what its row says (command kind and positions); `line` and `rect(QRectF)`
  record the existing commands; a data-radius circle is an ellipse in pixels when the axes scale differently; a
  pixel-radius circle stays 5 px on zoom and is not a `MarkersCmd`; `polygon(QPolygonF)` after a `QTransform` rotation;
  `roundedRect` corners; boxed text wraps inside the box minus the padding; Review Focus 4. One reference picture with
  every helper, painted by both replays (GPU on Xvfb and an export), checked locally.
- [ ] **Step 2:** Implement. GPU and painter replays draw the same picture (pixel tolerance as the existing replay
  comparison tests). Build on Coder, whole suite; local pictures. Commit `feat(api): drawing-context helpers`.

### Task 3: Call order (C-4, C-5)

**Files:** `src/painting/command-replay-rhi.{h,cpp}`, `src/painting/raster-fallback.{h,cpp}`,
`src/painting/plottable-rhi-layer.{h,cpp}`, `src/painting/command-replay-painter.cpp`, `src/drawable.cpp`,
`docs/limits.md`, tests under `tests/auto/test-repaint/` (or a new `test-call-order`).

- [ ] **Step 1:** Tests (frame read back on Xvfb): one recording `rect` (opaque red, GPU) then `text` over it: the text's
  pixels are over red; the reverse order: red covers the text. Two items on one layer, the second a text, the first a
  filled rect under it: text over the rect; swap the add order: rect over text. A `painter()` callback recorded after
  a polyline draws over it. The same three scenes exported with `savePng`: same stacking. A recording without
  painter-only commands takes no new image slot (layer image count unchanged). Review Focus 1, 2, 5.
- [ ] **Step 2:** Implement: painter-only commands as ordered image quads with their own budget (C-4); the export
  replays in call order; `docs/limits.md` row for C-5. Benchmarks before/after on Coder (Global Constraints). Build,
  whole suite on Coder, local pictures (expect moved reference pictures only where text stacked wrongly before; each
  moved picture is named in the report with why). Migration note + changelog (behaviour change). Commit
  `feat(painting): call order is screen order`.

### Task 4: Math text on text items (C-6)

**Files:** `src/items/item-text.{h,cpp}`, `src/layoutelements/axisrect-shortcuts.h`, `tests/auto/test-richtext/` or
`test-qcpaxisrect`.

- [ ] **Step 1:** Tests: `QCPItemText::setMathText(true)` with `"$\\alpha^2$"` measures differently from plain text
  (the plain string's width differs); `ax->text(at, "$x$", {.mathText = true})` gives an item with `mathText()` true.
- [ ] **Step 2:** Implement (`mathText()` / `setMathText()` property with NOTIFY like the item's other properties).
  Build, whole suite. Commit `feat(api): math text on text items`.

### Task 5: Review and finish

- [ ] Final review by a fresh reviewer over the milestone range; one fix batch.
- [ ] Whole offscreen suite and whole ASan+UBSan suite (Coder, `ASAN_OPTIONS=detect_leaks=0`), local `meson test`,
  clang `-fsyntax-only` on changed `.cpp` files, examples build, `check-sources.py`, guide snippets.
- [ ] `docs/writing-a-plot-type.md` shows the new helpers (and no longer names `QCPDrawn<Base>`).
- [ ] `docs/plans/neoqcp-3/milestone-6c-rulings.md`; memory.
