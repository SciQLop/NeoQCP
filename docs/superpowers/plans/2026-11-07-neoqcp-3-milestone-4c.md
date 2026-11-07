# NeoQCP 3.0 milestone 4c — colour maps, histograms, the colour-scale relation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `QCPColorMap`, `QCPColorMap2`, `QCPHistogram2D` and `QCPHistogram` draw through the drawing
API; one colour-scale relation, set up once in a base class, serves them and the line family; a colour
scale's rescale sees every user; deleting a scale is noticed; each axis rect owns and places its
colour scale; the two histograms share one API. Every step is the final design: nothing interim.

**Architecture:** First the image command becomes a GPU command: a textured quad with data-space
corners, kept across a pan and re-uploaded only when its image changes, on the plottable GPU layer that
already holds raster images. Then a base `QCPAbstractColorMapped` (between `QCPAbstractPlottable` and the
colour-mapping classes) owns gradient, data range, scale type and the link to a `QCPColorScale`, which
keeps a list of its users. Each axis rect owns one scale, made on first use, hidden until used and
placed in the rect's margin; colour-mapped plottables use it unless told otherwise. The colour maps then
record an image command (and contours as polylines and text); the separate colour-map GPU layer is
deleted. The histograms get one base API and the 1D one records rect batches. Last, the line family's
colour-by-value moves onto the relation, under the base's names.

**Tech Stack:** C++20, Qt 6.12 (QRhi), meson, QtTest, the in-tree benchmark (`tests/benchmark`).

**Spec:** `docs/specs/2026-11-06-neoqcp-3-milestone-4-design.md` (order item 3 "M4c"; answers 7, 13,
14), with `docs/specs/2026-11-02-neoqcp-3-object-model-design.md` (decision 14; answer "the
one-interface rule covers every duplicated family") and `docs/specs/2026-11-04-neoqcp-3-drawing-api-design.md`
(section 3: images move to the GPU in milestone 4). Maintainer, 2026-11-07: no interim designs; the
rect-owned scale moves here from M4d; the histograms share one API where it is common and cleaner;
the line colour setters take the base's names with no deprecated forwarders. Facts: `docs/plans/neoqcp-3/m4c-inventory.md`
(file:line for everything this plan names).

## Global Constraints

- One build or test at a time, in the foreground, `ninja -C build-qt612 -j 4`. Never two at once.
- Env: `PATH=/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin:/home/jeandet/Qt/6.12.0/gcc_64/bin:$PATH`.
- Offscreen suite: `QT_QPA_PLATFORM=offscreen QT_LOGGING_RULES="*.debug=true" meson test -C build-qt612 --num-processes 4`;
  totals = sum of the `Totals:` lines of `build-qt612/meson-logs/testlog.txt`. GPU tests: the whole
  `auto-tests` binary under `xvfb-run -a -s "-screen 0 1600x1200x24" env QT_QPA_PLATFORM=xcb`.
- Bench gate (answer 14): baseline = end of milestone 3 in `baseline.md`; rows that did not exist then
  are measured on the old code by Task 1. `QCPGpu_*`, `QCPDrawn_*`, `QCPItems_*`, `QCPBars_*` and the
  rows Task 1 adds: 5 %; the rest 15 %. `-median 5`, three runs; a row over its gate gets six more runs
  and their median decides.
- No interim design: a task builds the final shape of what it touches. No forwarder, alias or
  `\deprecated` method is added for a NeoQCP-only API this milestone replaces; its tests move to the new
  names. 2.x QCustomPlot API (`QCPColorMap`, `QCPColorScale`) keeps working until milestone 5.
- The colour signals keep their 2.x arguments (`dataRangeChanged(const QCPRange&)`,
  `gradientChanged(const QCPColorGradient&)`, `dataScaleTypeChanged(QCPAxis::ScaleType)`); they are
  emitted through `qcpEmit` (`src/core.h`).
- Errors: refuse misuse through `QCP_CHECK_OR_RETURN` (`src/misuse.h`), never an exception.
- An export looks as it does today: the painter replay of an image matches today's `drawImage`.
- A test fails before the change that makes it pass; a test that already passes is reported as a guard.
- Never bend library behaviour to please a test. Keep code size down: delete what a port replaces.
- Commit with explicit pathspecs; never `subprojects/.wraplock`, `subprojects/jkqtmathtext`, `qt.ini`.

## Review Focus

1. A pan of a colour map, colour map 2 or 2D histogram uploads no texture and colourises nothing; the
   quad moves (Task 2 test `aPannedImageIsNotUploadedAgain`, Task 6 test `aPanDoesNotRecolour`).
2. Reversed axes and a vertical key axis put the first cell where 2.x put it (Task 2 test
   `imageFollowsReversedAxes`, Task 5 test `verticalKeyAxisMapMatchesTheExport`).
3. An image larger than the GPU's texture limit, and a vectorised export, still draw it at today's
   quality (Task 2 tests `anImageTooBigForATextureIsPainted`, `vectorExportKeepsSharpCells`).
4. Deleting a colour scale that maps use: no crash, the maps keep the last gradient and range, and
   they say so with `colorScaleChanged()` (Task 3 test `deletingTheScaleIsNoticed`).
5. Five colour maps on one layer all draw on the GPU, under that layer's lines (Task 2 test
   `fiveImagesOnOneLayerStayOnTheGpu`).
6. Two colour maps on one rect share their rect's scale; a 2.x program that puts its own scale in the
   plot layout still works (Task 4 tests `mapsOnOneRectShareItsScale`, `aUserMadeScaleStillWorks`).

---

### Task 1: Bench rows for the classes M4c ports, measured on the old code

**Files:**
- Modify: `tests/benchmark/benchmark.cpp`, `docs/plans/neoqcp-3/baseline.md`

**Interfaces:**
- Produces rows (GPU rows QSKIP without a QRhi, as `QCPGpu_Pan100k` does):
  `QCPGpu_ColorMapPan` (a 1000×1000 classic map, panned by 1 % of the view per iteration),
  `QCPGpu_ColorMap2Pan` and `QCPGpu_ColorMap2Zoom` (a 4000×1000 source, panned / zoomed by 1 %, with the
  pipeline's first result waited for before `QBENCHMARK`), `QCPGpu_Histogram2DPan` (1 000 000 points,
  200×200 bins), `QCPGpu_HistogramPan` (1 000 000 values, 100 bins),
  `QCPColorScale_Rescale8` (8 maps on one scale, `rescaleDataRange(true)`).

- [ ] **Step 1:** Add the rows, mirroring `QCPGpu_MultiGraphPan1M` (`benchmark.cpp:1790`) for warm-up.
- [ ] **Step 2:** Run each three times on this commit (`-median 5`). Expected: every row runs (no QSKIP
  under Xvfb), numbers stable to ±10 % across runs.
- [ ] **Step 3:** Append "M4c start: rows added for the colour maps" to `baseline.md` with the three
  runs and the gate (the lower median), what each row times, and the load averages.
- [ ] **Step 4:** Commit: "bench: rows for the colour maps and histograms, measured on the old code".

### Task 2: The image command on the GPU

**Files:**
- Modify: `src/painting/draw-commands.h` (`ImageCmd`, its kind and extent),
  `src/painting/draw-context.{h,cpp}` (`image()`), `src/painting/command-replay-rhi.cpp`,
  `src/painting/command-replay-painter.cpp`, `src/painting/plottable-rhi-layer.{h,cpp}`
- Test: `tests/auto/test-draw-context/` (or the class that tests `image()` today; reuse it)

**Interfaces:**
- Produces: `struct ImageCmd { QCP::Position a, b; QImage image; QRectF source; bool smooth; }`.
  `a` is where the image's top-left texel lands and `b` its bottom-right: the replay never normalises,
  so a reversed axis mirrors the image. `source` is in texels (default: the whole image); `smooth`
  picks a linear sampler, else nearest.
- Produces: `void QCPDrawContext::image(QCP::Position a, QCP::Position b, const QImage& image,
  const QRectF& source = {}, bool smooth = false)`.
- Produces: an `ImageCmd` with both corners in `QCP::Data` is a GPU command that moves with the data: the
  RHI replay adds it with `QCPPlottableRhiLayer::addImage` (texture keyed by `QImage::cacheKey()`, as
  today), so a pan shifts the quad and uploads nothing.
- Decision: images that come from commands get their own budget, separate from the 4 raster-fallback
  slots (`kMaxImages`); the cap for command images is 16 per layer, past which the painter draws them
  (warned once, as the raster cap is).
- Decision: an image wider or taller than `QRhi::TextureSizeMax` is drawn by the painter replay.
- Decision: the painter replay of a non-smooth image on a vectorised painter upscales it 3× first, as
  `QCPColorMap::draw` does today (`plottable-colormap.cpp:1246-1262`), so PDF viewers do not blur cells.

- [ ] **Step 1: Write the failing tests:** `anImageIsAGpuCommand` (a data-cornered image recorded →
  replayed under Xvfb, the layer holds one image entry and no raster fallback);
  `aPannedImageIsNotUploadedAgain` (pan by 10 px → the texture upload count is unchanged, the quad
  moved by 10 px); `imageFollowsReversedAxes` (a 2×1 image red|blue, x axis reversed → blue on the
  left, on the GPU and in a `toPixmap` export); `sourceRectCropsTheImage`; `smoothImageIsInterpolated`
  (a 2×1 red|blue image stretched over 100 px: the middle pixel is a mix when smooth, pure when not);
  `anImageTooBigForATextureIsPainted`; `vectorExportKeepsSharpCells`; `fiveImagesOnOneLayerStayOnTheGpu`.
- [ ] **Step 2:** Run them. Expected: FAIL (no `source`/`smooth` members; the images are rasterised).
- [ ] **Step 3:** Implement. The upload count comes from a test hook on the layer if none exists
  (`imageUploadCount()`).
- [ ] **Step 4:** Run the tests, the offscreen suite and the Xvfb binary. Expected: PASS, suite green.
- [ ] **Step 5:** Commit: "feat(draw): the image command is a GPU texture that a pan moves".

### Task 3: The colour-scale relation in one base

**Files:**
- Create: `src/plottables/plottable-colormapped.{h,cpp}`
- Modify: `src/plottables/plottable-colormap.{h,cpp}`, `src/plottables/plottable-colormap2.{h,cpp}`,
  `src/plottables/plottable-histogram2d.{h,cpp}`, `src/painting/colormap-renderer.{h,cpp}` (the scale
  pointer goes), `src/layoutelements/layoutelement-colorscale.{h,cpp}`, `meson.build` (new files)
- Test: `tests/auto/test-colormap/`, `tests/auto/test-datasource2d/`

**Interfaces:**
- Produces: `class QCPAbstractColorMapped : public QCPAbstractPlottable` (Q_OBJECT) with
  `gradient()`, `dataRange()`, `dataScaleType()`, `colorScale()` (a `QCPColorScale*`, null when none),
  `setGradient(const QCPColorGradient&)`, `setDataRange(const QCPRange&)`,
  `setDataScaleType(QCPAxis::ScaleType)`, `setColorScale(QCPColorScale*)`, `rescaleDataRange(bool
  recalculate = false)`; pure virtual `QCPRange colorDataBounds(bool& found) const`; protected virtual
  `void colorMappingChanged()` (default `markDirty()`); signals `dataRangeChanged(const QCPRange&)`,
  `gradientChanged(const QCPColorGradient&)`, `dataScaleTypeChanged(QCPAxis::ScaleType)`,
  `colorScaleChanged()`.
- Rule (answer 13): with a scale attached the scale's gradient, range and scale type are the ones used;
  a setter on the plottable sets the scale's value, which every user of the scale then sees. Attaching
  takes the scale's values (2.x order). `setColorScale(nullptr)` keeps the current values.
- Produces on `QCPColorScale`: `QList<QCPAbstractColorMapped*> users() const`; the scale keeps the list
  itself (no Qt connections between scale and users). `rescaleDataRange` unions `colorDataBounds` of
  every user (visible ones only when asked). `colorMaps()` keeps its 2.x meaning.
- Rule (decision 14): deleting a scale calls each user, which keeps the scale's last values, forgets
  the scale and emits `colorScaleChanged()`. Deleting a user removes it from the scale's list.
- `QCPColorMap`, `QCPColorMap2`, `QCPHistogram2D` derive the base; their own copies of the members,
  setters, signals and the 6-connection wiring are deleted. A `QCPColorMap*` still connects to
  `&QCPColorMap::gradientChanged` (it names the inherited signal).

- [ ] **Step 1: Write the failing tests:** `rescaleSeesEveryUser` (a colour map, a colour map 2 and a
  2D histogram on one scale, data maxima 1, 5, 9 → the scale's range upper is 9);
  `aSetterOnOneUserReachesTheOthers` (`map2->setGradient(g)` → `map->gradient() == g`);
  `attachingTakesTheScaleValues`; `deletingTheScaleIsNoticed` (delete the scale → no crash,
  `colorScaleChanged` emitted once per user, gradient and range unchanged, `colorScale() == nullptr`);
  `deletingAUserLeavesTheScaleList`. Update the three `*_forgetsADeletedColorScale` tests
  (`test-colormap.h:36-41`) to the new rule: they now also expect the signal.
- [ ] **Step 2:** Run them. Expected: FAIL (rescale sees one class; no signal; no `users()`).
- [ ] **Step 3:** Implement. `colorDataBounds` per class: the classic map's `data()->dataBounds()`, the
  colour map 2's `colorDataBounds(found, true)`, the histogram's `colorDataBounds(found)` (inventory §5).
- [ ] **Step 4:** Run the tests and the offscreen suite. Expected: PASS, suite green.
- [ ] **Step 5:** Commit: "feat(colour): one colour-scale relation in a base; rescale sees every user".

### Task 4: Each axis rect owns and places its colour scale

**Files:**
- Modify: `src/layoutelements/layoutelement-axisrect.{h,cpp}`,
  `src/layoutelements/layoutelement-colorscale.{h,cpp}`, `src/plottables/plottable-colormapped.{h,cpp}`
- Test: `tests/auto/test-colormap/` (or a new `test-colorscale/` if the slots exceed 15)

**Interfaces:**
- Consumes: Task 3's base and the scale's user list.
- Produces: `QCPAxisRect::colorScale()` returns a handle to the rect's own scale, made on first call and
  owned by the rect (deleted with it). It is visible exactly while it has at least one user.
- Produces: the rect places its own scale in its margin on the side the scale's `type()` names (right
  by default), outside that side's axes; the rect's auto margin on that side grows by the scale's
  width; the scale is not in the plot's layout grid. Changing the scale's type moves it.
- Rule (answer 13): a `QCPAbstractColorMapped` uses its key axis rect's scale from construction unless
  `setColorScale` gives it another scale or none; the rect places only its own. A line counts as a
  user only while it has colour values.
- Rule: deleting the rect's own scale (through `remove`) is noticed as Task 3 says; the next
  `colorScale()` call makes a new one.

- [ ] **Step 1: Write the failing tests:** `aRectMakesItsScaleOnFirstUse`; `scaleIsHiddenUntilUsed`;
  `mapsOnOneRectShareItsScale` (two maps, one rescale → both use the union range);
  `scaleSitsOutsideTheRightAxis` (scale's outer rect left ≥ the right axis's outer edge, and inside the
  plot); `rectShrinksForItsScale` (the rect's inner width drops by about the scale's width when a map
  arrives, and comes back when it goes); `typeTopPutsTheScaleAbove`; `optingOutHidesTheScale`
  (`setColorScale(nullptr)` on the only user → hidden); `aUserMadeScaleStillWorks` (a 2.x program:
  `add<QCPColorScale>` in the layout, `map->setColorScale(s)`; the rect's scale stays hidden).
- [ ] **Step 2:** Run them. Expected: FAIL (no `QCPAxisRect::colorScale`).
- [ ] **Step 3:** Implement. Test `QCPColorScale`'s internal axis rect is not given a scale of its own.
- [ ] **Step 4:** Run the tests, the offscreen suite and the Xvfb binary. Expected: PASS, suite green;
  the 2.x colour-scale tests that build their own scale pass unchanged.
- [ ] **Step 5:** Commit: "feat(axisrect): each rect owns and places its colour scale".

### Task 5: `QCPColorMap` on the drawing API

**Files:**
- Modify: `src/plottables/plottable-colormap.{h,cpp}`
- Test: `tests/auto/test-colormap/`

**Interfaces:**
- Consumes: Task 2's `image()`, Task 3's base.
- Produces: `class QCPColorMap : public QCPDrawn<QCPAbstractColorMapped>`; `draw(QCPDrawContext&)`
  records one image command with data corners grown by half a cell (as `draw` grows its rect today,
  `.cpp:1270-1286`), `smooth = interpolate()`; with `tightBoundary()` the corners are the data range and
  `source` crops half a cell on each side. `updateMapImage()` stays and runs from `draw` when the data
  or the image is stale; the recording is remade when it reruns (a new image is a new `cacheKey`).
  The legend icon records an image of the thumbnail.
- Decision: the image's first texel is the cell at (key lower, value upper) of the data range as it is
  laid out today, so `a`/`b` carry the orientation and no `flipped()` copy is made.

- [ ] **Step 1: Write the failing tests:** `colourMapIsOneImageCommand`;
  `verticalKeyAxisMapMatchesTheExport` (key on the y axis: GPU frame vs `toPixmap` at the same size,
  every 10th pixel within 2 per channel); `tightBoundaryCropsHalfACell`; `aPanDoesNotRebuildTheImage`
  (`setCell` once, replot, pan 5 times → `updateMapImage` ran once; count with a test hook);
  `exportMatchesTheOldCode` (pixels of a 200×200 map exported now vs a reference image rendered from the old
  code at Step 2, committed under `tests/auto/test-colormap/reference/`).
- [ ] **Step 2:** Run them. Expected: FAIL. Render the reference image on this old code first.
- [ ] **Step 3:** Implement. Delete the painter `draw`, the `flipped()` copy and anything left unused.
- [ ] **Step 4:** Run the tests, the offscreen suite and the Xvfb binary; bench `QCPGpu_ColorMapPan`,
  `QCPColorMap_Standard`, `QCPColorMap_ColorizeMap`. Expected: PASS; rows within their gates.
- [ ] **Step 5:** Commit: "feat(colormap): the classic colour map records an image".

### Task 6: `QCPColorMap2` and `QCPHistogram2D` record their image and contours

**Files:**
- Modify: `src/plottables/plottable-colormap2.{h,cpp}`, `src/plottables/plottable-histogram2d.{h,cpp}`,
  `src/painting/colormap-renderer.{h,cpp}`, `src/painting/contour-overlay.{h,cpp}`, `src/core.cpp`
  (the colour-map layer registration and pan offset, `core.cpp:2890-2905`, `:3454-3463`,
  `:3509-3514`, `:3637-3650`)
- Delete: `src/painting/colormap-rhi-layer.{h,cpp}` and its shaders if nothing else uses them
- Test: `tests/auto/test-colormap/`, `tests/auto/test-pipeline/`, `tests/auto/test-datasource2d/`

**Interfaces:**
- Consumes: Task 2's `image()`, Task 3's base.
- Produces: both classes derive `QCPDrawn<QCPAbstractColorMapped>`. `draw(QCPDrawContext&)` records the
  colourised result grid as one image command with the grid's data corners (nearest sampling), then
  the contour lines as `polyline` commands in data space (NaN-separated, one per level), then labels as
  `text` commands. The renderer keeps only the colourisation (`updateMapImage`, `mapImage`, gradient,
  range, scale type come from the base); its GPU and painter `draw`, the scale pointer and the layer
  plumbing go.
- Rule: the export and the screen read the same recording; an export before the first result still
  resamples or bins synchronously, as `draw` does today (`plottable-colormap2.cpp:373-392`).
- Rule: a pipeline result marks the recording stale; a pan does not (data corners translate).
- Test hooks the tests use today (`rhiLayer()`, `lastContour*`, `fallbackBuildCount()`,
  `stallPixelOffset()`, inventory §8) are replaced by `mCore` recording hooks, as the multigraph's
  tests did in M4b; each rewritten test keeps its assertion's meaning.

- [ ] **Step 1: Write the failing tests:** `colourMap2IsOneImageCommand`; `aPanDoesNotRecolour`
  (colourisation count unchanged over 5 pans, for both classes); `contoursAreDataPolylines`;
  `contoursDrawOverTheImageOnTheGpu` (pixel on a contour line has the contour colour under Xvfb);
  `histogram2DExportBeforeTheFirstBinHasTheImage`; `noColourMapLayerIsCreated`.
- [ ] **Step 2:** Run them. Expected: FAIL.
- [ ] **Step 3:** Implement; port the tests that read the old hooks (list each in the report with its
  old and new assertion).
- [ ] **Step 4:** Run the tests, the offscreen suite and the Xvfb binary; bench `QCPGpu_ColorMap2Pan`,
  `QCPGpu_ColorMap2Zoom`, `QCPGpu_Histogram2DPan`, the `QCPColorMap2_Resample*` rows. Expected: PASS;
  within gates. Report the net src/ lines (the colour-map layer's 419 + 131 lines go).
- [ ] **Step 5:** Commit (one or two commits): "feat(colormap2, histogram2d): record the image and
  contours; the colour-map GPU layer goes".

### Task 7: One histogram API; the 1D histogram on the drawing API

**Files:**
- Create: `src/plottables/plottable-histogram-base.{h,cpp}`
- Modify: `src/plottables/plottable-histogram.{h,cpp}`, `src/plottables/plottable-histogram2d.{h,cpp}`,
  `src/datasource/histogram-binner-1d.h` (the enum moves), the 2D binning code, `meson.build`
- Test: `tests/auto/test-histogram1d/`, `tests/auto/test-pipeline/` (the `histogram2d*` slots)

**Interfaces:**
- Produces: `enum class QCP::HistogramNormalization { Count, Density, Probability }` (numpy's meanings:
  Density divides by the total and the bin area, Probability by the total), replacing
  `qcp::algo::HistogramNormalization` and `QCPHistogram2D::Normalization`.
- Produces: a shared API on both classes, from a mixin or a base as the code allows (the 2D class
  derives the colour base, the 1D one does not): `setBins(int)` / `bins()`,
  `setNormalization(QCP::HistogramNormalization)` / `normalization()`, `refreshBinning()` (slot), and
  the signal `histogramReady()` when a re-bin lands. Bins follow the key (and value) axis scale on both:
  a log axis bins in log10.
- Produces, 2D only: `setBins(int keyBins, int valueBins)`, `keyBins()`, `valueBins()`,
  `setColumnNormalized(bool)` / `columnNormalized()` (today's `nColumn`: each key column sums to 1; it
  replaces the normalization while on). `setKeyBinScale`, `setValueBinScale` and their getters go (they
  only set the axis scale). `binsChanged` becomes `histogramReady`.
- Produces: `class QCPHistogram : public QCPDrawn<...>`; `draw(QCPDrawContext&)` records each column as
  one `RectsCmd` batch (R3, `draw-commands.h`) with the column's outline pen, or, in step style, one
  polyline per column; legend icon recorded. `drawColumnOnGpu`, `paintColumn` and their helpers go.

- [ ] **Step 1: Write the failing tests:** `bothHistogramsShareTheNormalization` (Count, Density,
  Probability on a known 2D sample: totals 100, 100/area, 1); `columnNormalizedSumsEachKeyColumnToOne`;
  `binsFollowALogKeyAxis` (1D and 2D); `histogram2DSaysReady`; `histogramColumnIsOneBatch`;
  `aPanKeepsTheRecording`; rewrite the six `gpu*` tests of `test-histogram-plottable.h` to read the
  recording or the pixels instead of layer geometry, keeping their meaning; move the `histogram2d*`
  tests that call the removed setters to the axis scale or the new names.
- [ ] **Step 2:** Run them. Expected: FAIL (compile for the new names, then behaviour).
- [ ] **Step 3:** Implement.
- [ ] **Step 4:** Run the tests, the offscreen suite and the Xvfb binary; bench `QCPGpu_HistogramPan`,
  `QCPGpu_Histogram2DPan`. Expected: PASS; within the gates.
- [ ] **Step 5:** Commit: "feat(histogram): one histogram API; the 1D histogram records rect batches".

### Task 8: The line family's colour on the relation

**Files:**
- Modify: `src/plottables/plottable-line.{h,cpp}`, `src/plottables/plottable-multigraph.{h,cpp}`,
  `src/plottables/plottable-curve.{h,cpp}`, `src/plottables/plottable-color-mapper.h`
- Test: `tests/auto/test-color-by-scalar/`, `tests/auto/test-multigraph/` (`noSetterIsIgnored`),
  `tests/auto/test-qcpcurve/` (`noSetterIsIgnored`)

**Interfaces:**
- Consumes: Task 3's base, Task 4's rect scale (a coloured line is a user of its rect's scale).
- Produces: `class QCPAbstractLine : public QCPAbstractColorMapped`. The line's colour gradient, range
  and scale type are the base's; `qcp::ColorScalarMapper` keeps the values and the LUT and is fed from
  the base in `colorMappingChanged()`. `colorDataBounds` is the finite range of the colour values (the
  positive part on a log scale). A line can be given a scale (`setColorScale`) and then follows it.
- Decision: `setColorGradient`, `colorGradient`, `setColorRange`, `colorRange`, `setColorScaleType` and
  `colorScaleType` are deleted; a line uses the base's `setGradient`, `setDataRange`,
  `setDataScaleType`. Their tests move to the new names. `setColorValues`, `clearColorValues` and
  `hasColorValues` stay (the values are the line's own).

- [ ] **Step 1: Write the failing tests:** `aColouredLineShowsItsRectScale`; `aLineFollowsItsScale` (a coloured multigraph on a scale;
  `scale->setGradient(g)` → the recording's colours change); `rescaleSeesALine` (a line's colour values
  max 7, a colour map max 3 on one scale → range upper 7); `aCurveFollowsItsScale`; add a
  `setColorScale` row to both `noSetterIsIgnored` tests.
- [ ] **Step 2:** Run them. Expected: FAIL.
- [ ] **Step 3:** Implement.
- [ ] **Step 4:** Run the tests, the offscreen suite and the Xvfb binary; bench
  `QCPGpu_MultiGraphColored100kPan`. Expected: PASS; within the gate.
- [ ] **Step 5:** Commit: "feat(line): colour by value on the colour-scale relation".

### Task 9: Finish

**Files:**
- Modify: `changelog.txt`, `docs/plans/neoqcp-3/baseline.md`, `docs/plans/neoqcp-3/backlog.md`
- Create: `docs/plans/neoqcp-3/milestone-4c-rulings.md`

- [ ] **Step 1:** Changelog, milestone 4c section: the relation and its rules (scale wins, rescale
  unions every user, deletion noticed), images on the GPU, the colour-map layer gone, the deprecated
  line colour setters, and "Behaviour changes for 2.x users" (deleting a scale emits
  `colorScaleChanged()`; `rescaleDataRange` sees colour map 2, 2D histogram and coloured lines;
  colour maps on one rect share its scale unless told otherwise; the line colour setters and the 2D
  histogram's normalization enum and bin-scale setters are renamed or gone).
- [ ] **Step 2:** Offscreen suite, ASan suite, Xvfb binary, clang `-fsyntax-only` on every changed
  `.cpp`, the whole bench table three times appended as "End of milestone 4c".
- [ ] **Step 3:** Rulings and deferred minors to `milestone-4c-rulings.md`; deferred minors also to
  `backlog.md`. Commit.
