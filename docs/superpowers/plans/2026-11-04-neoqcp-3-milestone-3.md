# NeoQCP 3.0 milestone 3 — drawing and extension API — implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** a new plot type is one small class whose `draw()` records typed commands; it stacks, exports, hit-tests and draws as fast as the built-ins.

**Architecture:** typed positions and transforms (`src/coords.h`) feed a private command list recorded by `draw(QCPDrawContext&) const`. A recording is cached on (object generation, view epoch); replay resolves it to pixels, sends GPU kinds to the layer's existing RHI layers and paints QPainter kinds into the layer's buffer, and keeps the pan translation fast path. During milestones 3 and 4 a ported class keeps its old base and gets the new contract through the adapter `QCPDrawn<Base>`; milestone 4 folds it into the bases.

**Tech Stack:** C++20, Qt 6.12 (QRhi, QPainter), Meson, QTest.

**Spec:** `docs/specs/2026-11-04-neoqcp-3-drawing-api-design.md` (refines decisions 8–10 of `docs/specs/2026-11-02-neoqcp-3-object-model-design.md`). Evidence: `docs/plans/neoqcp-3/m3-drawing-inventory.md`, `m3-prior-art-survey.md`, `m3-design-review.md`. Maintainer 2026-11-04: "keep the best drawing performances you can".

## Global Constraints

- As in `docs/superpowers/plans/2026-11-04-neoqcp-3-item-scaling.md`: env (`PATH=/home/jeandet/Qt/6.12.0/gcc_64/bin:/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin:$PATH`, `PKG_CONFIG_PATH=/home/jeandet/Qt/6.12.0/gcc_64/lib/pkgconfig`, `LLVM_INSTALL_DIR=/usr`), one build or test at a time in the foreground with `-j 4`, `QT_QPA_PLATFORM=offscreen QT_LOGGING_RULES="*.debug=true"`, both builds (`build-qt612`, `build-asan` with `LSAN_OPTIONS=suppressions=$PWD/tests/lsan.supp`), explicit-path commits ending `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`, no push. New sources go in `meson.build`, `cmake/sources.cmake`, `tests/auto/meson.build` and `tests/auto/autotest.cpp`.
- No Qt Canvas Painter, no new dependency.
- **Performance is a gate, not a report.** Every task that touches a draw path runs the offscreen benchmark rows it names and the GPU benchmark (Task 1) before and after; no existing row may regress beyond 15 % (three runs, median). A miss is fixed in the task, or the task is not done.
- A command's backend depends on its kind only, never on its style: GPU for `polyline`, `polygon`, `fillBetween`, `rect`, `ellipse`, `errorBars`, `markers`; QPainter for `text`, `path`, `image`. (Images are QPainter in milestone 3: the colour-map texture layer is not generic yet; ruling recorded in the design.)
- The command list and its structs are private (`src/painting/`, not installed); users see only `QCPDrawContext`.
- The setter rule stays: equality early return, then `contentChanged()` (which marks), then store.
- GPU work is measured under Xvfb with llvmpipe: it measures the CPU side (extrusion, uploads, draw calls), not a real GPU. Say so in every number reported.

## Review Focus

1. A dashed, dotted or custom-dash pen on a graph or a ported class: same stacking as a solid pen, same picture on screen and in a PDF (dash phase continuous across segments and across a pan).
2. A pan over a ported class with 10 000 bars: no re-record, no re-extrusion, only the offset uniform changes (count calls).
3. A ported item whose axis is removed, or anchored to an item that is deleted: hidden with a debug message, never drawn at pixel 0.
4. A press on a non-selectable label lying over a selectable graph: the label gets the press and the click signal, the graph gets the selection (the documented 2.x split).
5. An export at another size of a plot with ported classes: the PDF stacks like the screen; afterwards the screen is unchanged (no stale resolve cache from the export's transform).

---

### Task 1: GPU benchmark harness and baseline

**Files:** Modify `tests/benchmark/benchmark.cpp`, `tests/benchmark/meson.build` if needed; Create `docs/plans/neoqcp-3/m3-gpu-baseline.md`.

- [ ] Add `QCPGpu_*` rows that run only when the plot has a QRhi (`QSKIP` otherwise): the plot is shown, `QTest::qWaitForWindowExposed`, then `QTRY_VERIFY(plot->rhi())`. Rows: `QCPGpu_LineSolid100k` (one QCPGraph, 100 000 points, solid pen, zoom step + `replot(rpImmediateRefresh)`), `QCPGpu_LineDashed100k` (same, `Qt::DashLine`), `QCPGpu_Pan100k` (range shift by 1 %), `QCPGpu_Graph2Render1M`, `QCPGpu_Bars10k` (10 000 QCPBars, zoom step), `QCPGpu_Text1k` (1 000 QCPItemText, zoom step), `QCPGpu_MixedLayer` (100 graphs and 100 text items alternating on `main`).
- [ ] Offscreen rows: `QCPBars_Zoom10k`, `QCPBars_Pan10k`, `QCPItemText_Zoom1k`.
- [ ] Run: `xvfb-run -a -s "-screen 0 1600x1200x24" env QT_QPA_PLATFORM=xcb build-qt612/tests/benchmark/qcp_benchmark -median 5 'QCPGpu_*'` three times, and the offscreen rows three times. Write `m3-gpu-baseline.md`: table (row, 3 runs, median), the llvmpipe caveat, the machine load. Check no Xvfb is left behind (`pgrep -a Xvfb`; only other sessions' 800x600x16 ones may remain).
- [ ] Commit "bench: GPU rows and the milestone 3 baseline".

### Task 2: Strong position types and transforms

**Files:** Create `src/coords.h`, `src/coords.cpp`; Modify `src/items/item.h`, `src/items/item.cpp` (QCPItemPosition adapter); Test `tests/auto/test-coords/`.

**Produces:**
```cpp
namespace QCP {
struct Data { double x, y; };        struct Pixels { double x, y; };
struct AxisRatio { double x, y; };   struct PixelOffset { double dx, dy; };
struct PixelLength { double value; };
struct AnchorOffset { QCPItemAnchor* anchor; PixelOffset offset; };
struct MixedX { /* x as Data, y as AxisRatio */ double dataX, ratioY; };
using Position = std::variant<Data, Pixels, AxisRatio, AnchorOffset, MixedX>;
AnchorOffset operator+(QCPItemAnchor* anchor, PixelOffset offset);
Pixels operator+(Pixels p, PixelOffset o);   // Data + Pixels: no operator, must not compile
}
class QCPTransform {                         // one axis pair, value type, cheap to copy
public:
    QCPTransform(const QCPAxis* keyAxis, const QCPAxis* valueAxis);  // snapshot of ranges + rect
    [[nodiscard]] QCP::Pixels map(QCP::Data d) const;
    [[nodiscard]] QCP::Data inverse(QCP::Pixels p) const;
    void map(std::span<const double> keys, std::span<const double> values,
             std::span<QPointF> out) const;  // batch, no per-point branch on scale type
    [[nodiscard]] bool isTranslationOf(const QCPTransform& other, QPointF* delta) const;
    [[nodiscard]] bool valid() const;          // false when an axis is gone
};
class QCPRatioTransform { /* axis rect fraction <-> pixels, same shape */ };
// QCPItemPosition: void set(const QCP::Position&); [[nodiscard]] QCP::Position value() const;
```
- [ ] Tests first (`TestCoords`): `mapMatchesCoordToPixel` (linear, log, reversed, log+reversed, vertical key axis; 1 000 random values each; equal within 1e-9 px to `QCPAxis::coordToPixel`); `inverseRoundTrips`; `batchMatchesScalar`; `translationDetected` (range shifted → true with the pixel delta; zoomed or log → false); `positionSetMatchesSetters` (each `QCP::Position` alternative through `QCPItemPosition::set` gives the same `pixelPosition()` as the 2.x setter sequence, including `MixedX` = `setTypeX(ptPlotCoords)` + `setTypeY(ptAxisRectRatio)` and anchor + offset); `mixingSpacesDoesNotCompile` (a `static_assert` on `!requires { Data{} + Pixels{}; }`).
- [ ] Inventory (read-only, can go to opencode): `docs/plans/neoqcp-3/strong-type-candidates.md` lists every public signature outside drawing where a raw `double`, `int`, `QPointF` or `QString` invites a mix-up (key vs value, pixels vs data, layer names, axis types), with the misuse each type would stop. Milestone 3 implements the drawing ones; the list feeds milestone 4.
- [ ] Implement. The batch `map` precomputes the linear coefficients (or log coefficients) once and runs one tight loop per scale type.
- [ ] Bench row `QCPTransform_Map1M` (1M points) against `QCPCoords_CoordToPixel1M` (a loop over `coordToPixel`): the batch must be at least 2x faster.
- [ ] Commit "feat: typed positions and transforms".

### Task 3: The command list, the draw context and the QPainter replay

**Files:** Create `src/painting/draw-commands.h` (private), `src/painting/draw-context.h`, `src/painting/draw-context.cpp`, `src/painting/command-replay-painter.cpp`; Test `tests/auto/test-draw-context/`.

**Consumes:** Task 2's types. **Produces:**
```cpp
class QCPDrawContext {                       // public
public:
    [[nodiscard]] const QCPTransform& transform() const;   // the object's axis pair
    [[nodiscard]] QRectF clip() const;                      // default: the axis rect
    [[nodiscard]] const QCPTheme& theme() const;
    void polyline(std::shared_ptr<const std::vector<QPointF>> data, const QPen& pen); // data space
    void polyline(std::span<const QCP::Position> points, const QPen& pen);
    void polygon(std::span<const QCP::Position> points, const QPen& pen, const QBrush& brush);
    void fillBetween(std::shared_ptr<const std::vector<QPointF>> upper,
                     std::shared_ptr<const std::vector<QPointF>> lower, const QBrush& brush);
    void rect(QCP::Position a, QCP::Position b, const QPen& pen, const QBrush& brush);
    void ellipse(QCP::Position a, QCP::Position b, const QPen& pen, const QBrush& brush);
    void markers(std::shared_ptr<const std::vector<QPointF>> data, const QCPScatterStyle& style);
    void errorBars(std::shared_ptr<const std::vector<QPointF>> centres,
                   std::shared_ptr<const std::vector<QPointF>> spans, const QPen& pen);
    void text(QCP::Position at, const QString& text, const QCPTextStyle& style);
    void path(QCP::Position origin, const QPainterPath& path, const QPen& pen, const QBrush& brush);
    void image(QCP::Position a, QCP::Position b, const QImage& image);
    void painter(std::function<void(QCPPainter&, const QCPTransform&)> callback); // escape hatch
};
// private: struct QCPDrawList { std::vector<QCPDrawCommand> commands; QCPDataBox bounds; };
// using QCPDrawCommand = std::variant<PolylineCmd, PolygonCmd, ...>;  one struct per helper
void qcpReplayWithPainter(const QCPDrawList&, QCPPainter&, const QCPTransform&); // export + QPainter kinds
```
`QCPTextStyle` is new and small (font, colour, alignment, rotation, padding); `QCPItemText` maps onto it in Task 9.
- [ ] Tests first: per helper, `<helper>MatchesHandPainting` — record one command, replay it with `qcpReplayWithPainter` into a 200x150 image, paint the same shape directly with QPainter at the same pixels into another image, `QCOMPARE` the images (same painter, so byte-identical). `boundsCoverEveryCommand` (the recording's data box contains every data-space point; pixel and ratio commands leave it unbounded on that side, the `dataExtent` rules). `sharedDataIsNotCopied` (`use_count` of the shared vector grows, its pointer is unchanged). `drawListIsCompact` (`sizeof` of every command struct ≤ 96 bytes; a `static_assert`).
- [ ] Implement. Commands keep positions in their own space; replay resolves through the transform.
- [ ] Commit "feat: the draw context records typed commands".

### Task 4: GPU replay and GPU dashes

**Files:** Create `src/painting/command-replay-rhi.cpp`, `src/painting/shaders/plottable_dashed.vert`, `plottable_dashed.frag`; Modify `src/painting/line-extruder.{h,cpp}` (an arc-length variant), `src/painting/plottable-rhi-layer.{h,cpp}` (a dashed pipeline next to the solid one), `src/painting/shaders/embed_shaders.py`, `src/plottables/plottable1d.h`, `src/plottables/plottable-draw-utils.cpp`, `src/plottables/plottable-graph.cpp`; Test `tests/auto/test-draw-context/` (GPU cases skip without QRhi), `tests/auto/test-repaint/`.

**Produces:** `void qcpReplayWithRhi(const QCPDrawList&, QCPPlottableRhiLayer&, QCPScatterRhiLayer&, const QCPTransform&, QCPResolveCache&)` for the GPU kinds; `QCPLineExtruder::appendPolylineWithDistance(points, penWidth, color, out)` emitting 7 floats per vertex (x, y, r, g, b, a, arc length in pixels); `QCPPlottableRhiLayer::addDashedPlottable(strokeVerts, dashPattern, dashOffset, clip, dpr, height, offsetX, offsetY, alpha)`.
- [ ] Tests first: `dashedLineStacksLikeSolid` (a QCPGraph with a dashed pen and a QCPItemRect created after it on `main`, both overlapping: the pixel colour at the overlap is the same as with a solid pen — fails today); `dashedLineMatchesExportDashes` (GPU frame vs `toPixmap` export: the on/off pattern along a horizontal line agrees within one dash length — run under Xvfb, skip offscreen); `dashPhaseSurvivesPan` (pan by 7 px, the dash pattern moves with the line); `solidLinesKeepTheirVertexFormat` (the solid pipeline still consumes 6 floats per vertex: no cost added to solid lines).
- [ ] Implement: the dashed pipeline is a separate `QRhiGraphicsPipeline` with a 7-float layout; the fragment shader discards by `mod(arcLength + dashOffset, patternLength)` against up to 8 pattern entries in a uniform. Arc length restarts at each NaN gap. The pen-style gate in `plottable1d.h:569`, `plottable-draw-utils.cpp` (three sites) and `plottable-graph.cpp:1019` accepts every `Qt::PenStyle` except `Qt::NoPen`.
- [ ] Bench: `QCPGpu_LineSolid100k` unchanged (±15 %); `QCPGpu_LineDashed100k` within 1.3x of the solid row (today it is a QPainter path: record before/after).
- [ ] Commit "feat(rhi): dashed lines on the GPU; a pen style never changes stacking".

### Task 5: Unsupported styles rasterised into a GPU image

**Files:** Create `src/painting/raster-fallback.{h,cpp}`; Modify `src/painting/command-replay-rhi.cpp`, `src/painting/colormap-rhi-layer.{h,cpp}` (accept a plain image quad); Test `tests/auto/test-draw-context/`.

- [ ] Tests first: `textureBrushStacksLikeSolid` (a rect command with a `Qt::TexturePattern` brush under a later GPU polyline: same order as with a solid brush); `rasterFallbackIsCapped` (a fallback command larger than the viewport is rasterised at viewport size; at most 4 fallback images per layer, the 5th falls back to the layer's QPainter buffer with one `qWarning` per plot).
- [ ] Implement: GPU kinds whose brush or pen the GPU cannot draw (texture, gradient other than the solid fill path already supported, conical) are painted with QPainter into a `QImage` covering the command's pixel bounds, uploaded once per (generation, view epoch), and drawn as a textured quad at the command's place in the GPU order.
- [ ] Commit "feat(rhi): styles the GPU cannot draw become a GPU image".

### Task 6: `QCPDrawn<Base>`: the contract, the recording cache and the resolve cache

**Files:** Create `src/drawable.h`, `src/drawable.cpp`; Modify `src/layer.{h,cpp}` (`markDirty` vs `contentChanged`), `src/core.cpp` (view epoch); Test `tests/auto/test-drawable/`.

**Consumes:** Tasks 2–5. **Produces:**
```cpp
template <class Base>  // Base = QCPAbstractPlottable, QCPAbstractPlottable1D<T> or QCPAbstractItem
class QCPDrawn : public Base {
protected:
    virtual void draw(QCPDrawContext& context) const = 0;
    virtual std::optional<QCPDataBox> dataRange() const;                    // default: recording bounds
    virtual std::optional<double> hitTest(QCP::Pixels at, QVariant* details) const; // default: recording
    virtual void drawLegendIcon(QCPDrawContext& icon) const;                 // default: recording fitted
    void contentChanged();             // ++generation, then Base::markDirty()
private:  // final overrides of the old virtuals: draw(QCPPainter*), selectTest, drawLegendIcon(QCPPainter*, QRectF),
          // getKeyRange/getValueRange (plottables, with signDomain), dataExtent (items)
};
quint64 QCustomPlot::viewEpoch() const;  // bumped by viewport size, device pixel ratio, theme changes
```
- [ ] Tests first (a test class `TestShape : QCPDrawn<QCPAbstractItem>` drawing one polygon and one text, counting `draw()` calls): `panDoesNotReRecord` and `zoomDoesNotReRecord` (draw count unchanged); `setterReRecordsOnce` (one setter → one more draw, at the next frame); `unrelatedMarkDoesNotReRecord` (another item's setter on the same layer: count unchanged); `viewEpochReRecordsEscapeHatch` (a `painter()` command re-records on resize, a plain polygon does not); `panReusesResolvedGeometry` (pan over 10 000 TestShape-like bars: `QCPLineExtruder` call count unchanged, counted through a test hook); `exportDoesNotPoisonTheResolveCache` (export at 800x600, then a screen frame resolves with the screen transform); `defaultHitTestMatchesGeometry` (distance to the polygon edge minus half the pen, 0 inside a visible fill, missed beyond tolerance); `defaultDataRangeFollowsTheExtentRules` (mixed and anchored positions unbounded, log axis non-positive unbounded, same as `dataExtent`); `defaultLegendIconIsTheRecordingFitted`; `missingAxisHidesWithAMessage` (remove the axis: nothing drawn, one debug message).
- [ ] `contentChangedInsideDrawIsAMisuse`: a `draw()` that calls a setter of its own object triggers the misuse report (milestone 1 tier) and does not loop; `draw()` runs inside the replot's emission scope, so signals it causes are delivered after the frame.
- [ ] Implement. The recording cache key is (generation, view epoch). The resolve cache key adds the transform; `QCPTransform::isTranslationOf` reuses resolved geometry with the offset uniform (the `gpuOffset` path the line caches use today), anything else re-resolves.
- [ ] Bench: `QCPItems_Zoom10k`, `Pan10k`, `Click10k` unchanged (the index still filters first).
- [ ] Commit "feat: QCPDrawn records once, resolves once per view".

### Task 7: One hit-test rule

**Files:** Modify `src/core.cpp` (`layerableListAt`, `mousePressEvent`, `processPointSelection`, `wheelEvent`), `src/plottables/plottable-graph2.cpp`, `src/plottables/plottable-multigraph.cpp`; Test `tests/auto/test-hit-rule/`.

- [ ] Tests first: `wheelNeedsDistanceToo` (wheel far from a graph2 curve but inside its axis rect: the graph2 is not the wheel target — fails today); `pressAndSelectionAgreeWhenSelectable`; `nonSelectableLabelTakesPressGraphTakesSelection` (Review Focus 4); `axisRectWheelZoomStillWins` (wheel over the axis rect zooms as before); `topmostWithinToleranceWins` (two overlapping objects: the later child on the higher layer wins for press, selection and wheel).
- [ ] Implement: remove the `!details` fast paths (graph2 and multigraph keep their binary-search distance, which is O(log n)); one helper `QCustomPlot::topmostAt(pos, QCPHitAction action)` with `QCPHitAction { Press, Select, Wheel }` used by the three handlers; precedence mouse grabber → axis rect interactions → drawables.
- [ ] Bench: `QCPInput_MouseMoveStorm` unchanged; a new `QCPInput_WheelOverGraph2_1M` row ≤ 0.1 ms.
- [ ] Commit "fix: one hit-test rule for press, selection and wheel".

### Task 8: The `annotations` layer

**Files:** Modify `src/core.cpp` (default layers), `src/items/item.cpp` (default layer), the 2.x docs that list the default layers; Test `tests/auto/test-repaint/`.

- [ ] Tests first: `itemsDefaultToAnnotations` (a new QCPItemText, QCPItemLine and span land on `annotations`, which sits between `main` and `axes`); `textAboveGraphLine` (a solid-pen graph created after a text item: the text stays visible over the line — fails today); `setLayerStillWins`.
- [ ] Implement: `annotations` (buffered) between `main` and `axes`; `QCPAbstractItem` uses it unless a layer is given; `setCurrentLayer` still applies to plottables.
- [ ] Changelog line (behaviour change). Bench: the default-layer rows (`QCPGraph_Standard`, `QCPItems_*`) unchanged.
- [ ] Commit "feat: items default to an annotations layer above the data".

### Task 9: Port `QCPBars` and `QCPItemText`

**Files:** Modify `src/plottables/plottable-bars.{h,cpp}`, `src/items/item-text.{h,cpp}`; Create `examples/custom-drawable/` (one class written only against public headers, built in CI); Test the existing `test-bars*` / `test-item-text*` classes plus `tests/auto/test-drawable/`.

- [ ] `QCPBars : public QCPDrawn<QCPAbstractPlottable1D<QCPBarsData>>`: `draw()` records one `polygon` per visible bar (or one batched polygon command per style run), overrides `hitTest` (bar rect via `getBarRect`, groups and stacking included) and `dataRange` (widened by half a bar). `QCPItemText : public QCPDrawn<QCPAbstractItem>`: `draw()` records one `text`; `setPosition(QCP::Position)` added, `position` kept.
- [ ] Every existing bars and text test passes unchanged (pixel tests included). New: `barsStackWithTextAbove`, `barsPanDoesNotReRecord`, `textRotatedHitTest`, `textAnchorParentDeletedKeepsPlace` (the text follows an item's anchor; deleting that item leaves the text where it was, never at pixel 0).
- [ ] The example draws a "range band" (fill between two lines plus a label) in under 60 lines of user code, with a pixel test.
- [ ] Bench: `QCPBars_Zoom10k`, `QCPBars_Pan10k`, `QCPItemText_Zoom1k`, `QCPGpu_Bars10k`, `QCPGpu_Text1k` no slower than Task 1's baseline; target faster (bars go to the GPU).
- [ ] Commit "feat: QCPBars and QCPItemText on the drawing API".

### Task 10: Close

- [ ] Benchmark section "End of milestone 3" in `docs/plans/neoqcp-3/baseline.md` (whole binary, three runs) plus the GPU table against `m3-gpu-baseline.md`.
- [ ] Changelog section; `docs/plans/neoqcp-3/milestone-3-rulings.md` (rulings, false findings, deferred); user guide page "Writing a plot type" built from the example.
- [ ] Final review (opencode), each finding verified; fixes test-first; GPU suite under Xvfb.
- [ ] Memory; no push.
