# NeoQCP 3.0 milestone 6f — scales Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** a scale is a value (`QCPScale::linear()`, `log()`, `symlog(threshold)`) set on any axis, colour scale or
plottable data scale; `setScale` brings its ticker; symlog draws, pans, zooms, resamples, colours and ticks across the
whole library; `ax->colorMap(x, y, z, {...})` makes a colour map in one call.

**Architecture:** `QCPScale` maps a value to *scale space*, where the pixel mapping is linear. Task 2 swaps the enum
for the value type everywhere with log behaviour unchanged (suite and pictures as before). Tasks 3-5 then make every
"linear or log" branch scale-space math, so symlog works in mapping, interaction, the data pipeline and colour.
Task 6 gives each scale its ticker. Rev. 2 after the pre-flight review (`preflight.md` in the plan workspace).

**Tech Stack:** C++23 (after n11), Qt 6.12, meson + `cmake/sources.cmake`, QtTest.

**Spec:** `docs/specs/2026-11-14-neoqcp-3-milestone-6-design.md`; input `docs/plans/neoqcp-3/m6-gaps.md` theme 2 and
decisions 2-3; targets `examples/user-code/symlog/main.cpp`, `examples/user-code/spectrogram/main.cpp`.

## Global Constraints

- Branch `neoqcp-3.0`, worktree `~/Documents/prog/NeoQCP-3.0`. Builds and suites on Coder `neoqcp`; reference
  pictures and clang `-fsyntax-only` locally (`build-qt612`, `-j 4`). Never `meson setup` in the local worktree;
  one build or test per build directory at a time.
- Failing test first for every change; whole suite green before each commit.
- No interim designs: `QCPAxisBase::ScaleType`, `setScaleType`, `scaleType`, `scaleTypeChanged`,
  `setDataScaleType`, `dataScaleType` and their hooks are deleted, not forwarded (decision 2). Every breaking change
  in `docs/migrating.md`, `changelog.txt` (unreleased section), one line in
  `docs/plans/neoqcp-3/sciqlopplots-migration.md`.
- The whole library takes the new scale (spec, controller calls): no plot type, axis kind or pipeline stage is left
  "linear or log". Where symlog has no sensible meaning, it is refused through the misuse tier, and that is a
  ruling in the task.
- Qt naming: `scale()` / `setScale()` / `scaleChanged()`; `dataScale()` / `setDataScale()` / `dataScaleChanged()`.
  State signals are emitted with `qcpEmitCurrent` (5e).
- Commit with explicit pathspecs; never commit `subprojects/.wraplock`, `subprojects/jkqtmathtext`, `qt.ini`,
  `.superpowers`. Never push.
- The two targets also need `setAxisRectGrid` (6e). In 6f they are checked through single-rect reference programs in
  the tests; the targets themselves compile when 6e lands.

## Review Focus

1. Symlog data crossing zero, and a range that straddles the linear band: one continuous line, no NaN gap, no jump
   at ±threshold, on screen (GPU and painter) and in an export.
2. A pan on a symlog or log axis with GPU geometry kept: geometry is reused, and the picture equals a fresh replot at
   the new range.
3. `setScale(log())` on an axis whose range includes zero or negative values, a negative log range ([-1000, -1]), and
   a colour scale whose data range does: the same sanitising and sign rule as `stLogarithmic` today.
4. `setScale` always installs the scale's default ticker, even for an equal scale (no `scaleChanged` then): a
   ticker set before it is replaced, one set after it stays (decision 3).
5. Dense noisy data near zero on a symlog key axis: thinning, L2 resampling and colour-map rows are as dense in the
   linear band as outside it.

---

### Task 1: `QCPScale`

**Files:**
- Create: `src/axis/scale.h`, `src/axis/scale.cpp`; add to `cmake/sources.cmake`, meson lists, `src/qcp.h`.
- Test: new class `tests/auto/test-scale/` (`TestScale`).

**Interfaces:**
- Produces:
  ```cpp
  class QCP_LIB_DECL QCPScale {
      Q_GADGET
  public:
      enum class Kind : quint8 { Linear, Log, Symlog };
      Q_ENUM(Kind)
      static QCPScale linear();
      static QCPScale log(double base = 10.0);
      static QCPScale symlog(double threshold = 2.0, double base = 10.0, double linearScale = 1.0);
      Kind kind() const; bool isLinear() const; bool isLog() const; bool isSymlog() const;
      double base() const; double threshold() const; double linearScale() const;
      double toScale(double value) const;   // value -> scale space
      double fromScale(double s) const;     // inverse; log returns the positive value
      QCPRange sanitized(const QCPRange& range) const; // log: sanitizedForLogScale, else sanitizedForLinScale
      bool operator==(const QCPScale&) const = default;
  };
  size_t qHash(const QCPScale&, size_t seed = 0);
  ```
  No `Q_DECLARE_METATYPE` (a gadget needs none, as `QCPColorGradient`). Default-constructed = linear. Symlog is
  matplotlib's `SymmetricalLogTransform`: `a' = linearScale / (1 - 1/base)`; `|v| <= threshold`: `v * a'`; else
  `sign(v) * threshold * (a' + log_base(|v| / threshold))`. Log is `log_base(|v|)`: the sign rule and the off-range
  rule stay in the axis and `QCPTransform::Snapshot`, as today (a scale cannot know its range). A threshold or a
  linear scale that is not finite and positive, or a base `<= 1` or not finite, is refused through the misuse tier
  and gives the default.

- [ ] **Step 1: failing tests** (`TestScale`):
  - `linearIsIdentity`: `toScale(3.5) == 3.5`; default-constructed `== QCPScale::linear()`.
  - `logIsLogBase`: `QCPScale::log().toScale(1000) == 3` (fuzzy), `QCPScale::log(2).toScale(8) == 3`,
    `QCPScale::log().toScale(-1000) == 3`.
  - `symlogMatchesMatplotlib`: `symlog(2)`: `toScale(1) ≈ 1.11111`, `toScale(20) ≈ 4.22222`,
    `toScale(-20) ≈ -4.22222`, `toScale(100) ≈ 5.62020`, `toScale(0) == 0`, `toScale(2) ≈ 2.22222` from both
    formulas.
  - `roundTrips`: `fromScale(toScale(v)) ≈ v` for v in {-1e6, -20, -2, -0.5, 0, 0.5, 2, 20, 1e6} on symlog, positive
    v on log.
  - `badParametersAreRefused`: `symlog(0)`, `symlog(qQNaN())`, `symlog(2, 10, 0)`, `symlog(2, 1)`, `log(1)` each
    report once through the misuse recorder and equal the defaults.
- [ ] **Step 2:** run `QCP_TEST_CLASS=TestScale`; Expected: does not compile.
- [ ] **Step 3:** implement.
- [ ] **Step 4:** passes; whole suite green.
- [ ] **Step 5:** commit `feat(api): QCPScale`.

### Task 2: the value type everywhere, log unchanged

The whole rename in one task, so the suite is green at its end: every axis (cartesian, polar radial and angular,
colour-scale axes), every colour scale and colour-mapped plottable, `QCPColorGradient`'s `logarithmic` flag, the
pipeline's log flags. Behaviour is today's: each former `stLinear` / `stLogarithmic` branch now tests `isLinear()` /
`isLog()`, and symlog is not reachable yet (Task 3 turns it on).

**Files:**
- Modify: `src/axis/axis-base.{h,cpp}`, `src/axis/axis.{h,cpp}`, `src/coords.{h,cpp}`, `src/core.h`,
  `src/painting/viewport-offset.h`, `src/painting/grid-rhi-layer.{h,cpp}`, `src/polar/*`, `src/drawable.cpp`,
  `src/painting/draw-context.cpp`, `src/coordinate-system.cpp`, `src/plottables/*` (every file the grep finds),
  `src/layoutelements/layoutelement-colorscale.{h,cpp}`, `layoutelement-axisrect.cpp`, `src/colorgradient.{h,cpp}`,
  `src/painting/colormap-renderer.*`, `src/painting/contour-overlay.*`, `src/datasource/async-pipeline.*` and every
  pipeline file that reads its log flags, `src/doc-mainpage.dox`, every test, example and doc snippet that names
  the enum or the flags.
- Test: `TestScale` (axis and colour cases), `tests/auto/test-meta-surface/` migrated.

**Interfaces:**
- Consumes: `QCPScale` (Task 1).
- Produces:
  - `QCPAxisBase::scale() const -> const QCPScale&`, `Q_SLOT void setScale(const QCPScale&)`,
    `signal scaleChanged(const QCPScale&)`, `Q_PROPERTY(QCPScale scale READ scale WRITE setScale NOTIFY scaleChanged)`;
    hooks `scaleWillChange(const QCPScale&)` / `scaleDidChange()`. An equal scale emits nothing.
  - `dataScale() const -> const QCPScale&`, `setDataScale(const QCPScale&)`, `dataScaleChanged(const QCPScale&)` on
    `QCPColorScale` and every colour-mapped plottable, and a "data scale chosen" flag so a data scale set before the
    add wins over the joined colour scale's (6b R-7, which today protects only gradient and range:
    `plottable-colormapped.cpp:107-114`).
  - `QCPColorGradient::color` / `colorize` take a `const QCPScale&` instead of `bool logarithmic`, keeping the
    per-element loop.
  - The pipeline's `ViewportParams` carry a `QCPScale` per axis instead of the log booleans.
  - The hot block of `QCPAxisBase` (`axis-base.h:204-219`) keeps a one-byte kind next to the range; the full scale
    sits outside it. Update the layout comment; the mapping benchmark rows must not move.

- [ ] **Step 1: failing tests:** `anAxisTakesAScale` (`setScale(log())` → `isLog()`, one `scaleChanged`; an equal
  call emits nothing); `aLogAxisMapsAsBefore` (20 values on [0.1, 1e4] and 5 on [-1000, -1]: `coordToPixel`
  equals literals recorded at BASE); `aLogColourScaleMapsAsBefore` (colour indices for 10 z values equal literals
  recorded at BASE); `aDataScaleSetBeforeTheAddWins` (a `QCPColorMap2` with `setDataScale(log())`, then added to a
  rect whose colour scale is linear: the map stays log).
- [ ] **Step 2:** run; Expected: does not compile.
- [ ] **Step 3:** replace the enum and flags everywhere. Meta-surface: a `differentFor<QCPScale>` entry
  (linear ↔ log), the bindable spike case (`test-meta-surface.cpp:262-264`) and the angular-axis exception
  (`:1061-1063`) migrated.
- [ ] **Step 4:** whole suite on Coder, Xvfb `auto-tests`, local reference pictures, the mapping benchmark rows
  (interleaved A/B against BASE, 3 pairs). Expected: totals as at BASE plus the new tests, no picture moved, no row
  more than 5 % slower.
- [ ] **Step 5:** commit `feat(api)!: QCPScale replaces the scale enum`.

### Task 3: symlog mapping and interaction

**Files:**
- Modify: the sites the pre-flight review lists in C1, C3, C4, C5 and the polar part of C14: `axis-base.cpp`
  (`moveRange`, `scaleRange`, flat-range rescale), `axis.cpp` (`coordToPixel`, `pixelToCoord`, drag),
  `layoutelement-axisrect.cpp` (`dragAxis`, `followPointer`), `plottable.cpp` (flat-range expansion),
  `coords.cpp` (`QCPTransform::Axis`, `mapRange`, `isTranslationOf`), `viewport-offset.h`, `draw-context.cpp`,
  `coordinate-system.cpp`, `drawable.cpp`, `plottable-financial.cpp` (key windows), `grid-rhi-layer.cpp` (ticks
  mapped to scale space on the CPU and uploaded linear; the log shader branch stays; the cache keyed on the whole
  `QCPScale`), `polargraph.cpp` (revision hashes the scale), `radialaxis.cpp`, `polar-area.cpp`.
- Test: `TestScale`, `tests/auto/test-repaint/` (pan reuse), `tests/auto/test-grid-rhi/`.

**Interfaces:**
- Consumes: Tasks 1-2.
- Rules:
  - Mapping, drag, wheel zoom, `scaleRange`, `followPointer`, `dragAxis` are scale-space math for every non-linear
    scale; log keeps its current arithmetic (pictures do not move). `moveRange(diff)` is a scale-space offset on
    symlog (documented; on log it stays a factor).
  - Flat-range expansion and `sanitized` on symlog use the linear rule.
  - A key window (`draw-context.cpp:551-563` and the three other sites) is built in scale units; the condition is
    `!isLinear()` where it was "not log".
  - `isTranslationOf` compares slopes and intercepts in scale space. Ruling: log pans start reusing kept geometry
    too (a perf gain; a reused frame must equal a fresh replot).
  - A radial axis takes symlog through the same scale-space mapping (polar is not left out).

- [ ] **Step 1: failing tests:**
  - `symlogMapsBothSides`: `[-100, 100]`, `symlog(2)`: 0 at fraction 0.5, x = 2 at fraction 0.6977 (linear would
    give 0.51), and `coordToPixel(-x)` mirrors `coordToPixel(x)` for x in {1, 2, 10, 100}.
  - `aLineCrossingZeroIsContinuous` (Review Focus 1): `ax->line` of x in [-50, 50] on a symlog x axis; GPU frame
    (Xvfb), painter export (`toPixmap`) and PDF-sized raster all ink every column inside the line's extent, and the
    pixel column of x = 2 is where `coordToPixel(2)` says (fails on a linear fallback).
  - `aPanReusesGeometryAndMatchesAFreshReplot` (Review Focus 2, Xvfb), for symlog and for log: a drag of a GPU line;
    assert the recording cache reused the geometry (no re-record), and the frame equals a fresh plot at the same
    range (the repaint tests' tolerance).
  - `dragAndZoomWorkInScaleSpace`: a drag keeps `toScale(upper) - toScale(lower)`; a wheel zoom about 0 keeps 0 at
    its pixel; a drag on a range straddling the band never gives NaN.
  - `aGpuGridOnSymlogMatchesThePainterGrid`: the GPU grid lines of a symlog axis at the pixels `coordToPixel` gives
    for the ticks (Xvfb); changing `symlog(2)` to `symlog(3)` redraws the grid.
  - `aRadialAxisTakesSymlog`: a polar graph on a symlog radial axis maps r = ±2 symmetric, no NaN.
- [ ] **Step 2:** run; Expected: fail.
- [ ] **Step 3:** implement.
- [ ] **Step 4:** suite, Xvfb, pictures (no picture moved), benchmark pan rows A/B (log pans may get faster; none
  more than 5 % slower).
- [ ] **Step 5:** commit `feat: symlog axes draw and interact in scale space`.

### Task 4: the data pipeline in scale space

**Files:**
- Modify: `src/datasource/async-pipeline.*`, `algorithms.h` (`LogKeyBucketGrid` becomes a scale-space bucket grid),
  `resampled-multi-datasource.h`, `graph-resampler.h`, `resample.{h,cpp}`, `histogram-binner.h`,
  `histogram-binner-1d.h`, `src/plottables/plottable-graph.cpp` (thinning), `plottable-colormap2.cpp`,
  `plottable-histogram.cpp`, `plottable-histogram2d.cpp`, `src/painting/contour-overlay.cpp`.
- Test: `tests/auto/test-pipeline/`, `test-datasource2d/`, `test-histogram1d/`.

**Interfaces:**
- Consumes: Tasks 1-3.
- Rules: thinning buckets, L2 bins and the line-cache window are equal spans of *scale space*; colour-map rows are
  built evenly in scale space and placed by the scale; a histogram on a symlog axis bins in scale space (bin edges
  `fromScale` of equal scale-space steps); automatic contour levels on a symlog data scale are evenly spaced in scale
  space. Log keeps its current results.

- [ ] **Step 1: failing tests:** `symlogThinningIsEvenInScaleSpace` (Review Focus 5: 100 000 noisy points on
  [-50, 50], symlog(2): the bucket count per 100 px inside the band equals the count outside, ±1); `l2OnSymlog`
  (L2 used on a symlog key, bins equal in scale space); `colorMapRowsOnASymlogAxis` (a `QCPColorMap2` with 4 rows
  on a symlog y axis: each row's pixel band is where `coordToPixel` of its edges says); `histogramBinsInScaleSpace`;
  `contourLevelsOnSymlog`. Each also asserts the log case equals literals recorded at BASE.
- [ ] **Step 2:** run; Expected: fail.
- [ ] **Step 3:** implement.
- [ ] **Step 4:** suite, Xvfb, pictures (none moved), pipeline benchmark rows A/B.
- [ ] **Step 5:** commit `feat: the data pipeline resamples in scale space`.

### Task 5: symlog colour

**Files:**
- Modify: `src/colorgradient.cpp`, `src/painting/colormap-renderer.cpp`, `src/plottables/plottable-colormap.cpp`
  (lookup and cell-centre rule, `:767-771`, `:1115-1138`), `plottable-color-mapper.h` (`logPosition`),
  `layoutelement-colorscale.cpp` (the bar, flat data-range rescale `:606-615`), `plottable-line.cpp` (colour data
  scale).
- Test: `TestScale`, colour-map and colour-scale tests.

- [ ] **Step 1: failing tests:** `aSymlogColourScaleIsSymmetric` (a `QCPColorMap2` over z in [-1e4, 1e4] with
  `setDataScale(QCPScale::symlog(1))`, diverging gradient: z and -z give mirror entries, z = 0 the middle one, and
  z = 1 sits at gradient fraction 0.6087, where linear would give 0.50005); `theColourBarFollowsTheDataScale` (the
  bar's axis has the symlog scale and its pixel for z = 1 matches the lookup); `aColouredLineTakesSymlog`.
- [ ] **Step 2:** run; Expected: fail.
- [ ] **Step 3:** implement through `toScale` in the per-element loop.
- [ ] **Step 4:** suite, Xvfb, pictures (none moved).
- [ ] **Step 5:** commit `feat: symlog colour scales`.

### Task 6: each scale's ticker

**Files:**
- Create: `src/axis/axistickersymlog.{h,cpp}` (`QCPAxisTickerSymlog`).
- Modify: `src/axis/axis-base.cpp` (`setScale` installs the default ticker), `src/axis/axis.cpp:2339` (decade
  labels abbreviated on symlog as on log), `tests/auto/test-reference-m4e.cpp` scenes at `:85`, `:130`, `:160`.
- Test: new `tests/auto/test-axis-ticker-symlog/`, `TestScale`, reference pictures.

**Interfaces:**
- Consumes: Tasks 1-5.
- Produces: `QCPAxisTickerSymlog(const QCPScale&)`. Ticks follow matplotlib's `SymmetricalLogLocator`: decades from
  `floor(log_base(threshold))` outward on both sides plus 0 (so ±1 is a major tick for threshold 2), thinned by the
  log ticker's step formula `max(1, int(cleanMantissa(decades / tickCount)))` (`axistickerlog.cpp:190-192`); sub
  ticks 2..9 × base^k outside the band and linear inside it at multiples of `base^floor(log_base(threshold))`.
  `setScale` installs `QCPAxisTicker` (linear), `QCPAxisTickerLog(base)` (log) or `QCPAxisTickerSymlog` (symlog)
  every time it is called, an equal scale included (without `scaleChanged`); a later `setTicker` wins.

- [ ] **Step 1: failing tests:**
  - `symlogTicksMatchMatplotlib`: for `symlog(2)` on [-1000, 1000] and [-50, 50], and `symlog(0.015)` on [-1, 1], the
    major ticks equal matplotlib's `SymmetricalLogLocator(linthresh, base=10).tick_values(lo, hi)` (compute them on
    Coder with matplotlib in a venv and paste them as literals, with the matplotlib version in a comment).
  - `subTicksFillTheBand`: on [-50, 50], symlog(2): sub ticks inside [-2, 2] exist.
  - `setScaleBringsItsTicker` (Review Focus 4): after `setScale(log())` the ticker is a `QCPAxisTickerLog`; a
    `setTicker(dateTime)` after `setScale` stays; one before it is replaced, also by an equal scale.
  - Reference picture `symlog-single-rect`: the target's third panel (x and y symlog, the wave, `symlog(0.015)` on y,
    sub grid on) on one rect.
- [ ] **Step 2:** run; Expected: fail.
- [ ] **Step 3:** implement.
- [ ] **Step 4:** suite, Xvfb, pictures. The m4e bars, financial and statistical-box scenes move (their log axes now
  get a log ticker, decision 3): re-baseline those three on purpose and say so in the report; no other picture may
  move.
- [ ] **Step 5:** commit `feat(api): each scale brings its ticker`.

### Task 7: `ax->colorMap`

**Files:**
- Modify: `src/layoutelements/axisrect-shortcuts.h`, `src/layoutelements/layoutelement-axisrect.h` (declaration and
  `QCPColorMapOptions`), `docs/` colour-map guide section.
- Test: `tests/auto/test-qcpaxisrect/`, one reference picture.

**Interfaces:**
- Consumes: Tasks 1-6; n11's `QCPNumberRange`, `QCPColumns`, `qcp::toDoubles`, ruling P-7.
- Produces:
  ```cpp
  struct QCPColorMapOptions {
      QCPColorGradient gradient = QCPColorGradient::gpViridis;
      QCPScale dataScale = QCPScale::linear();
      bool colorBar = true;               // join the rect's colour scale; false: joined to none, bar invisible
      std::optional<QCPRange> dataRange;  // unset: rescaleDataRange(true) on the data given
      QString name;
  };
  template <QCPNumberRange X, QCPNumberRange Y, QCPNumberRange Z> requires(!QCPColumns<Z>)
  QCPHandle<QCPColorMap2> QCPAxisRect::colorMap(const X& x, const Y& y, const Z& z,
                                               const QCPColorMapOptions& options = {});
  template <QCPNumberRange X, QCPNumberRange Y, QCPColumns Z>   // one column of z per x
  QCPHandle<QCPColorMap2> QCPAxisRect::colorMap(const X& x, const Y& y, const Z& z,
                                               const QCPColorMapOptions& options = {});
  ```
  The ranges are copied with `qcp::toDoubles` into vectors moved into `add<QCPColorMap2>`; flat `z` is x-major
  (`z[i * y.size() + j]`); the columns form flattens x-major and reports ragged columns or a column count other than
  `x.size()` through the misuse tier (empty handle). The options are applied after the add through the map's own
  setters (which forward to the joined colour scale), then the range: `options.dataRange` or
  `rescaleDataRange(true)`. A second map on the same rect re-styles the shared colour scale: documented.

- [ ] **Step 1: failing tests:** `colorMapInOneCall` (3×4 grid, flat z: x size 3, y size 4, z at (2, 3) is the last
  value; the rect's colour scale has the gradient, the data scale and the fitted range); `colorMapFromColumns`
  (same data as columns: same source values); `raggedColumnsAreRefused`; `colorMapWithoutColorBar` (joined to no
  colour scale, the rect's bar invisible); `colorMapFromAView` (`std::views::iota(0.0) | std::views::take(3)` as x);
  reference picture `spectrogram-single-rect`: the target's spectrogram panel (viridis, log data scale, log y axis)
  on one rect, from the target's own `spectrogram()` function copied into the test.
- [ ] **Step 2:** run; Expected: does not compile.
- [ ] **Step 3:** implement as a thin shortcut over `add<QCPColorMap2>(...)`, as `line` is over `add<QCPGraph>`.
- [ ] **Step 4:** suite, Xvfb, pictures.
- [ ] **Step 5:** commit `feat(api): colorMap in one call`.

### Task 8: Review and finish

- [ ] Final review by a fresh reviewer over the plan's range; one fix batch.
- [ ] Whole offscreen suite, whole ASan+UBSan suite (`ASAN_OPTIONS=detect_leaks=0`), Xvfb `auto-tests`, local
  pictures, clang `-fsyntax-only` on changed `.cpp`, examples, `check-sources.py`, `doc-snippets`.
- [ ] `docs/plans/neoqcp-3/milestone-6f-rulings.md`; memory.
