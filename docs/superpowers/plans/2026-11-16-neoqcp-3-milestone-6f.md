# NeoQCP 3.0 milestone 6f — scales Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** a scale is a value (`QCPScale::linear()`, `log()`, `symlog(threshold)`) set on any axis, colour scale or
plottable data scale; `setScale` brings its ticker; symlog draws, pans, zooms and ticks; `ax->colorMap(x, y, z,
{...})` makes a colour map in one call.

**Architecture:** `QCPScale` maps a value to *scale space*, where the pixel mapping is linear. Every place that
tests `stLogarithmic` today either keeps a log-only rule (sign sanitising, the log ticker) behind `scale.isLog()`, or
becomes scale-space math that serves log and symlog alike (pixel mapping, drag, zoom, pan reuse, colour lookup).
The log path keeps its current arithmetic, so no reference picture moves.

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
- Qt naming: `scale()` / `setScale()` / `scaleChanged()`; `dataScale()` / `setDataScale()` / `dataScaleChanged()`.
- Commit with explicit pathspecs; never commit `subprojects/.wraplock`, `subprojects/jkqtmathtext`, `qt.ini`,
  `.superpowers`. Never push.
- The two targets also need `setAxisRectGrid` (6e). Ruling: in 6f they are checked through single-rect reference
  programs in the tests; the targets themselves compile when 6e lands.

## Review Focus

1. Symlog data crossing zero, and a range that straddles the linear band: one continuous line, no NaN gap, no jump
   at ±threshold, on screen and in an export.
2. A pan on a symlog axis with GPU geometry kept: the picture equals a fresh replot at the new range (a pan is a
   translation in scale space, not in data space).
3. `setScale(log())` on an axis whose range includes zero or negative values, and on a colour scale whose data range
   does: the same sanitising as `stLogarithmic` today.
4. A user ticker set before `setScale` is replaced, one set after it stays (decision 3), and a `QCPAxisTickerDateTime`
   on a linear axis survives `setScale(QCPScale::linear())` only if set after.
5. A symlog colour scale with data in [-1e4, 1e4]: colours symmetric about zero, the colour bar's axis shows the same
   scale and ticks.

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
      enum class Kind { Linear, Log, Symlog };
      Q_ENUM(Kind)
      static QCPScale linear();
      static QCPScale log(double base = 10.0);
      static QCPScale symlog(double threshold = 2.0, double base = 10.0, double linearScale = 1.0);
      Kind kind() const; bool isLinear() const; bool isLog() const; bool isSymlog() const;
      double base() const; double threshold() const; double linearScale() const;
      double toScale(double value) const;   // value -> scale space
      double fromScale(double s) const;     // inverse
      QCPRange sanitized(const QCPRange& range) const; // log: sanitizedForLogScale, else sanitizedForLinScale
      bool operator==(const QCPScale&) const = default;
  };
  Q_DECLARE_METATYPE(QCPScale)
  ```
  A default-constructed `QCPScale` is linear. Symlog is matplotlib's `SymmetricalLogTransform`:
  `a' = linearScale / (1 - 1/base)`; `|v| <= threshold`: `v * a'`; else
  `sign(v) * threshold * (a' + log_base(|v| / threshold))`. Log is `log_base(|v|)` with the sign rule of today's log
  axis (the range's sign decides; a value of the other sign has no position). A non-positive threshold or a base
  `<= 1` is refused through the misuse tier and gives the default.

- [ ] **Step 1: failing tests** (`TestScale`):
  - `linearIsIdentity`: `toScale(3.5) == 3.5`, default-constructed `== QCPScale::linear()`.
  - `logIsLogBase`: `QCPScale::log().toScale(1000) == 3` (fuzzy), `QCPScale::log(2).toScale(8) == 3`.
  - `symlogMatchesMatplotlib`: for `symlog(2)`: `toScale(1) ≈ 1.11111`, `toScale(20) ≈ 4.22222`,
    `toScale(-20) ≈ -4.22222`, `toScale(0) == 0`, and `toScale(2)` equals the limit from both sides (continuity).
  - `roundTrips`: `fromScale(toScale(v)) ≈ v` for v in {-1e6, -20, -2, -0.5, 0, 0.5, 2, 20, 1e6} on symlog, positive v on
    log.
  - `badParametersAreRefused`: `symlog(0)` and `log(1)` report once through the misuse recorder and equal the
    defaults.
- [ ] **Step 2:** run `QCP_TEST_CLASS=TestScale`; Expected: does not compile.
- [ ] **Step 3:** implement.
- [ ] **Step 4:** passes; whole suite green.
- [ ] **Step 5:** commit `feat(api): QCPScale`.

### Task 2: axes take a `QCPScale`

**Files:**
- Modify: `src/axis/axis-base.{h,cpp}`, `src/axis/axis.{h,cpp}`, `src/coords.{h,cpp}` (the `Axis` frame:
  `logarithmic`, `logSlope`, `logIntercept` become a scale-space slope and intercept with a log fast path kept),
  `src/painting/viewport-offset.h` (pan ratio in scale space), `src/painting/grid-rhi-layer.*`,
  `src/polar/radialaxis.cpp`, `src/polar/angularaxis.*`, `src/polar/polar-area.cpp`, `src/polar/polargraph.cpp`,
  `src/drawable.cpp`, `src/plottables/plottable.cpp`, `plottable-graph.*`, `plottable-line.cpp`,
  `plottable-histogram.cpp`, `src/layoutelements/layoutelement-axisrect.cpp`, `src/core.h`, `src/doc-mainpage.dox`,
  every test, example and doc snippet that names the enum. Everything that is not a colour or data scale (Task 3).
- Test: `TestScale` (axis cases) plus the existing suite migrated.

**Interfaces:**
- Consumes: `QCPScale` (Task 1).
- Produces: `QCPAxisBase::scale() const -> const QCPScale&`, `Q_SLOT void setScale(const QCPScale&)`,
  `signal scaleChanged(const QCPScale&)`, `Q_PROPERTY(QCPScale scale READ scale WRITE setScale NOTIFY scaleChanged)`;
  the protected hooks become `scaleWillChange(const QCPScale&)` / `scaleDidChange()`. `setScale` does not touch the
  ticker yet (Task 4).

- [ ] **Step 1: failing tests:** `anAxisTakesAScale` (`setScale(log())` → `scale().isLog()`, one `scaleChanged`, a
  second identical call emits nothing); `aLogAxisMapsAsBefore` (for 20 values on a log axis over [0.1, 1e4],
  `coordToPixel` equals the value recorded at BASE before the change: record them in the test as literals).
- [ ] **Step 2:** run; Expected: does not compile.
- [ ] **Step 3:** replace the enum on every axis. Each `== stLogarithmic` becomes `scale().isLog()` where the rule is
  log-only (sign sanitising, `sanitizedForLogScale`, the log ticker's choice) and scale-space math where it is a
  mapping (`coordToPixel`, `pixelToCoord`, drag, wheel zoom, `scaleRange`, the pan ratio, `QCPTransform::Axis`), so
  that Task 3's symlog needs no second pass over these files. Migrate every test and example mechanically.
- [ ] **Step 4:** whole suite on Coder, Xvfb `auto-tests`, local reference pictures. Expected: totals as at BASE
  plus the new tests, no picture moved.
- [ ] **Step 5:** commit `feat(api)!: axes take a QCPScale`.

### Task 3: colour scales and data scales take a `QCPScale`

**Files:**
- Modify: `src/layoutelements/layoutelement-colorscale.{h,cpp}`, `src/plottables/plottable-colormapped.{h,cpp}`,
  `plottable-color-mapper.h`, `plottable-colormap.{h,cpp}`, `plottable-colormap2.{h,cpp}`,
  `plottable-histogram2d.{h,cpp}`, `src/painting/colormap-renderer.{h,cpp}`, `src/painting/contour-overlay.{h,cpp}`,
  tests, examples, docs.
- Test: `TestScale` (colour cases), existing colour-map / colour-scale tests migrated.

**Interfaces:**
- Consumes: Tasks 1-2.
- Produces: `dataScale() const -> const QCPScale&`, `setDataScale(const QCPScale&)`,
  `dataScaleChanged(const QCPScale&)` on `QCPColorScale` and every colour-mapped plottable; the colour scale's axis
  follows its data scale as today.

- [ ] **Step 1: failing tests:** `aSymlogColourScaleIsSymmetric` (Review Focus 5: a `QCPColorMap2` over z in
  [-1e4, 1e4] with `setDataScale(QCPScale::symlog(1))` and a diverging gradient: the colour at z and -z are mirror
  entries of the gradient, z = 0 is the middle entry); `aLogColourScaleMapsAsBefore` (colour indices for 10 z values
  on a log data scale equal literals recorded at BASE).
- [ ] **Step 2:** run; Expected: does not compile.
- [ ] **Step 3:** replace the data-scale enum; the colour lookup maps through `toScale` (log keeps its arithmetic).
- [ ] **Step 4:** suite, Xvfb, pictures as in Task 2.
- [ ] **Step 5:** commit `feat(api)!: colour scales take a QCPScale`.

### Task 4: symlog on an axis, and the scale's ticker

**Files:**
- Create: `src/axis/axistickersymlog.{h,cpp}` (`QCPAxisTickerSymlog`).
- Modify: `src/axis/axis-base.cpp` (`setScale` installs the default ticker), `src/coords.cpp` if Task 2 left a
  log-only mapping branch.
- Test: `TestScale`, `tests/auto/test-axis-ticker-log/` or a new `test-axis-ticker-symlog/`, one reference picture.

**Interfaces:**
- Consumes: Tasks 1-3.
- Produces: `QCPAxisTickerSymlog(const QCPScale&)`; `setScale` installs `QCPAxisTicker` (linear),
  `QCPAxisTickerLog` with the scale's base (log) or `QCPAxisTickerSymlog` (symlog), replacing whatever ticker the
  axis had; a later `setTicker` wins (decision 3). Symlog major ticks: 0 and ±base^k for every base^k ≥ threshold
  inside the range, thinned by an integer step in k when more than the ticker's tick count would show; sub ticks:
  2..9 × base^k outside the band, none inside.

- [ ] **Step 1: failing tests:**
  - `symlogMapsBothSides`: an axis over [-100, 100] with `symlog(2)`: `coordToPixel(0)` is the middle pixel,
    `coordToPixel(-x)` mirrors `coordToPixel(x)` for x in {1, 2, 10, 100}.
  - `aLineCrossingZeroIsContinuous` (Review Focus 1): `ax->line` of x in [-50, 50] on a symlog x axis, painted and
    exported (`toPixmap`): no column inside the line's extent without ink.
  - `aSymlogPanMatchesAFreshReplot` (Review Focus 2, GPU, Xvfb): pan a GPU line on a symlog axis by a drag, compare
    the frame with a fresh plot at the same range (reference tolerance of the repaint tests).
  - `dragAndZoomWorkInScaleSpace`: a drag keeps `toScale(upper) - toScale(lower)`; a wheel zoom about 0 keeps 0 at its
    pixel.
  - `symlogTicks`: over [-1000, 1000] with `symlog(2)`: majors {-1000, -100, -10, 0, 10, 100, 1000}; with
    `symlog(0.015)` over [-1, 1]: majors {-1, -0.1, 0, 0.1, 1} (0.01 is inside the band, so no tick).
  - `setScaleBringsItsTicker` (Review Focus 4): after `setScale(log())` the ticker is a `QCPAxisTickerLog`; a
    `setTicker(dateTime)` after `setScale` stays; one before is replaced.
  - Reference picture `symlog-single-rect`: the target's third panel (x and y symlog, wave, `symlog(0.015)` on y),
    on one rect.
- [ ] **Step 2:** run; Expected: fail or not compile.
- [ ] **Step 3:** implement the ticker and the ticker install; finish any mapping branch Task 2 left log-only.
- [ ] **Step 4:** suite, Xvfb, pictures (the new picture baselined locally, fonts).
- [ ] **Step 5:** commit `feat(api): symlog axes and their ticker`.

### Task 5: `ax->colorMap`

**Files:**
- Modify: `src/layoutelements/axisrect-shortcuts.h`, `src/layoutelements/layoutelement-axisrect.h` (declaration and
  `QCPColorMapOptions`), `docs/` colour-map guide section.
- Test: `tests/auto/test-qcpaxisrect/`, one reference picture.

**Interfaces:**
- Consumes: Tasks 1-4; n11's `QCPNumberRange` / `QCPColumns`.
- Produces:
  ```cpp
  struct QCPColorMapOptions {
      QCPColorGradient gradient = QCPColorGradient::gpViridis; // a preset converts
      QCPScale dataScale = QCPScale::linear();
      bool colorBar = true;                 // join the rect's colour scale; false: a map of its own
      std::optional<QCPRange> dataRange;    // unset: fitted on the first data, as a rect colour scale does
      QString name;
  };
  template <QCPNumberRange X, QCPNumberRange Y, QCPNumberRange Z>
  QCPHandle<QCPColorMap2> QCPAxisRect::colorMap(const X& x, const Y& y, const Z& z,
                                               const QCPColorMapOptions& options = {});
  template <QCPNumberRange X, QCPNumberRange Y, QCPColumns Z>   // one column of z per x
  QCPHandle<QCPColorMap2> QCPAxisRect::colorMap(const X& x, const Y& y, const Z& z,
                                               const QCPColorMapOptions& options = {});
  ```
  `z` flat is x-major (`z[i * y.size() + j]`), the layout `QCPColorMap2` reads. With `colorBar` the map joins the
  rect's colour scale (`colorScale()`), which takes the options' gradient, data scale and range (6b R-7: what the
  user set wins, the options are what the user set here).

- [ ] **Step 1: failing tests:** `colorMapInOneCall` (3×4 grid, flat z: the map's data source has x size 3, y size 4,
  z at (2, 3) is the last value; `colorScale()` has the gradient and data scale given); `colorMapFromColumns` (same
  data as columns: same source values); `colorMapWithoutColorBar` (the rect's colour scale is not created or not
  joined); reference picture `spectrogram-single-rect`: the target's spectrogram panel (viridis, log data scale, log y
  axis) on one rect, from the target's own `spectrogram()` function copied into the test.
- [ ] **Step 2:** run; Expected: does not compile.
- [ ] **Step 3:** implement as a thin shortcut over `add<QCPColorMap2>(...)`, as `line` is over `add<QCPGraph>`.
- [ ] **Step 4:** suite, Xvfb, pictures.
- [ ] **Step 5:** commit `feat(api): colorMap in one call`.

### Task 6: Review and finish

- [ ] Final review by a fresh reviewer over the plan's range; one fix batch.
- [ ] Whole offscreen suite, whole ASan+UBSan suite (`ASAN_OPTIONS=detect_leaks=0`), Xvfb `auto-tests`, local
  pictures, clang `-fsyntax-only` on changed `.cpp`, examples, `check-sources.py`, `doc-snippets`.
- [ ] `docs/plans/neoqcp-3/milestone-6f-rulings.md`; memory.
