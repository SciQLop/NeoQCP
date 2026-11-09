# NeoQCP 3.0 milestone 4f — areas, the axis base, the polar area, the frame variant Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** steps 1-4 of the coordinate-systems design: a baseline of polar pictures and bench rows on the old
code; `QCPArea` under `QCPAxisRect` and a new `QCPPolarArea` that takes the angular axis's layout role;
`QCPAxisBase` under `QCPAxis` and both polar axes, with polar axis click selection that works; and
`QCPTransform` as one frame type holding a cartesian, polar or custom mapping.

**Architecture:** first the polar layout role moves out of the angular axis into `QCPPolarArea` (a layout
element is already a layerable, so no class may be both an area and an axis — design F1). Then the range
model shared by the three axis classes moves into `QCPAxisBase`. Then `QCPTransform` keeps today's
cartesian snapshot unchanged inside a `std::variant` next to a polar and a custom mapping, dispatching once
per call. Plottables, systems and items are untouched until milestone 4g; the polar graph keeps painting
until 4h, and the Task 1 pictures guard it all along.

**Tech Stack:** C++20, Qt 6.12, meson, QtTest, `tests/benchmark`.

**Spec:** `docs/specs/2026-11-09-neoqcp-3-coordinate-systems-design.md` (approved; D1-D8; sections 2.1-2.4,
2.8, 3, 4 steps 1-4, 5). Facts: `docs/plans/neoqcp-3/m4f-inventory.md` (P-x),
`docs/plans/neoqcp-3/coordsys-inventory.md` (C-x), the design review `.superpowers/coordsys-design-review.md`
(F-x).

## Calls made in this plan (the maintainer can overrule any)

- **P-1 `QCPArea` is thin in 4f**: a `QCPLayoutElement` with `virtual QList<QCPAxisBase*> axes() const = 0`
  (Task 3) and `virtual QCPColorScale* colorScale()` (default `nullptr`; `QCPAxisRect` overrides with today's lazy
  scale). Systems, per-system lists and area enumeration arrive with 4g, where something uses them.
- **P-2 The angular axis's inset layout is deleted**: it was allocated and never wired (P-1 of the
  inventory, `layoutelement-angularaxis.cpp:202-205`).
- **P-3 `ScaleType` (`stLinear`, `stLogarithmic`) and `SelectablePart`/`SelectableParts` (`spNone`,
  `spAxis`, `spTickLabels`, `spAxisLabel`) move to `QCPAxisBase`**; the polar axes' copies go.
  `QCPAxis::stLogarithmic`, `QCPAxis::spAxis` and `QCPAxis::SelectableParts` keep compiling through
  inheritance; `scaleTypeChanged` takes `QCPAxisBase::ScaleType`. Q_ENUM/Q_FLAG move with them.
- **P-4 Tick generation and label formatting stay in each axis class** (straight, circle and spoke labels
  differ); the base owns the ticker pointer and the tick, sub-tick and tick-label vectors.
- **P-5 `linkTo`/`unlink`/`linkSource`/`link()` move to the base with `QCPAxisBase*`**; the range funnel
  (`applyRange`/`commitRange`, M4d) moves with the range. Linking a polar axis, or linking to one, is
  refused as misuse in 4f: no polar pixel rule exists yet (design 2.2).
- **P-6 The polar mapping clamps the radial pixel distance to `[0, 1.05 R]`** (R = the area's radius): no
  reflection through the centre, and the same ±5 % margin the polar graph clamps to today (P-6).
- **P-7 The polar graph keeps painting in 4f** with its own conversions; it becomes a plottable in 4h.
  The Task 1 polar pictures must keep matching through every task of 4f.

## Global Constraints

- One build or test at a time, in the foreground, `ninja -C build-qt612 -j 4`. Never two at once.
- Env in the SAME shell command as every meson/ninja call:
  `PATH=/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin:/home/jeandet/Qt/6.12.0/gcc_64/bin:$PATH`
  and `PKG_CONFIG_PATH=/home/jeandet/Qt/6.12.0/gcc_64/lib/pkgconfig`. Never `meson setup`; a source added to
  meson.build makes ninja re-run meson itself.
- Offscreen suite: `QT_QPA_PLATFORM=offscreen QT_LOGGING_RULES="*.debug=true" meson test -C build-qt612 --num-processes 4`;
  totals = sum of the `Totals:` lines of `build-qt612/meson-logs/testlog.txt`. Xvfb: the whole `auto-tests`
  binary under `xvfb-run -a -s "-screen 0 1600x1200x24" env QT_QPA_PLATFORM=xcb`; afterwards
  `pgrep -x Xvfb -a | grep 1600` prints nothing.
- Bench gate (design 3): every row within 5 % (drawing rows: `QCPGpu_*`, `QCPDrawn_*`, `QCPItems_*`,
  `QCPBars_*`, `QCPPolar_*`, the rows Task 1 adds) or 15 % (others) of the task's base, by A/B: five
  interleaved pairs of `-tickcounter -iterations N -median 5` against a build of the base commit in a
  scratchpad worktree (`QBENCHMARK_ONCE` rows: walltime `-median 7`). Never whole-table runs alone.
- Names (D7): `QCPArea`, `QCPPolarArea`, `QCPAxisBase`, `QCPCustomMapping`. No "Abstract" in new names.
- No shim, forwarder or intermediate form for the polar 2.1 API this milestone changes (D6); 2.x cartesian
  API keeps every signature (design 5).
- Misuse through `QCP_CHECK_OR_RETURN`, signals through `qcpEmit`/`deliver`, `contentChanged()` (or
  `markDirty()`) on every setter that changes what is drawn.
- A test fails before the change that makes it pass; a test that passes first is reported as a guard.
- Never bend library behaviour to please a test. Keep code size down: delete what a move replaces.
- Clang check (`clang++ -fsyntax-only` per changed `.cpp` from `compile_commands.json`) before each commit
  that adds a type or a template. No clang-format on whole files.
- Commit with explicit pathspecs; never `subprojects/.wraplock`, `subprojects/jkqtmathtext`, `qt.ini`.
- SciQLopPlots: nothing to port from 4f (it does not use polar); if a task finds otherwise, add it to
  `docs/plans/neoqcp-3/sciqlopplots-migration.md`, nowhere else.

## Review Focus

1. Deleting the polar area, its angular axis or a radial axis during a drag or a wheel does not crash and
   leaves the rest drawn (the existing `test-qcustomplot` polar deletion slots, moved to the new API in
   Task 2, plus `deletingTheAreaMidDragIsSafe` in Task 2).
2. A cartesian plot draws, pans, zooms and links exactly as before (the M4 references and the item and
   plottable bench rows; Tasks 3 and 4 run them).
3. A log radial axis, a reversed angular axis and a rotated start angle map through the polar transform
   exactly as the old axes' `coordToPixel` did (Task 4 `polarMappingMatchesTheAxes`, data-driven).
4. Clicking a polar axis selects the part under the cursor and nothing outside it; `onlySelectable`
   refuses an unselectable axis (Task 3 `polarAxisPartsAreClickable`).
5. 2.x cartesian code using `QCPAxis::stLogarithmic`, `QCPAxis::SelectableParts`,
   `&QCPAxis::scaleTypeChanged` and `linkTo(QCPAxis*)` still compiles and behaves the same (Task 3
   `cartesianAxisApiStillCompiles`).

---

### Task 1: Polar pictures and bench rows on the old code

**Files:**
- Modify: `tests/auto/test-reference-m4e/test-reference-m4e.{h,cpp}` (polar scenes; reference PNGs in
  `tests/auto/test-reference-m4e/reference/`)
- Modify: `tests/benchmark/benchmark.cpp`

**Interfaces:**
- Produces: `polarScene(const char* kind)` building, with the 2.1 API as it is today, the scenes
  `polar-basic` (as `docs/snippets/polar.cpp` `polar_basic`), `polar-radians` (`polar_radians`),
  `polar-gallery` (`docs/snippets/gallery.cpp:469-510`), `polar-log` (a log radial axis 1..1000),
  `polar-rotated` (`setAngle(30)`, reversed angular axis), `polar-selected` (one graph selected, the
  angular axis' `spAxis` selected), each a 400x300 `toPixmap`, checked with `qcp::test::matchesReference`.
  Later tasks change only how the scene is built, never the PNGs.
- Produces: rows `QCPPolar_Draw10k` (one graph, 10 000 points, full replot), `QCPPolar_Rotate10k` (pan
  step: angular range moved by 0.5 degree), `QCPPolar_RadialZoom10k` (radial range scaled by 1.01 about
  its centre), `QCPPolar_Export10k` (`toPixmap(400, 300)` per iteration), `QCPPlottables_Pan200` (200
  graphs of 1 000 points on one rect, pan step as `QCPItems_Pan10k`), `QCPPlottables_Zoom200`
  (`scaleRange(1.01)`). Each builds, replots once, then times its step.

- [ ] **Step 1:** Add the six scenes and their test slots; run with `QCP_WRITE_REFERENCE=1`, then without:
  all pass. Look at each PNG and describe it in the report (it must show the scene, not an empty disc).
- [ ] **Step 2:** Add the six bench rows; record three runs of each (`-tickcounter -iterations 50
  -median 5`, offscreen) in the report: their baseline.
- [ ] **Step 3: Commit** `test(m4f): polar pictures and bench rows on the old code`.

### Task 2: `QCPArea` and `QCPPolarArea`

**Files:**
- Create: `src/layoutelements/area.{h,cpp}` (`QCPArea`), `src/polar/polar-area.{h,cpp}` (`QCPPolarArea`)
- Modify: `src/layoutelements/axisrect.{h,cpp}` (derives `QCPArea`), `src/polar/layoutelement-angularaxis.*`
  (rename the files to `src/polar/angularaxis.{h,cpp}`; the class loses its layout role), `src/polar/radialaxis.*`,
  `src/polar/polargrid.*`, `src/polar/polargraph.*`, `src/construct.h`, `src/core.{h,cpp}`, `src/qcp.h`,
  `meson.build`
- Modify every user: tests (`test-qcustomplot` polar slots, `test-ownership`, `test-repaint`,
  `test-builder`, `test-reference-m4e` scenes), `docs/snippets/polar.cpp`, `docs/snippets/gallery.cpp`,
  `examples/plots/mainwindow.cpp` (polar demo), `tests/manual/mainwindow.cpp` (polar test), `docs/polar.md`
- Test: new `tests/auto/test-polar/` (`TestPolar`; register in `tests/auto/meson.build` and `autotest.cpp`)

**Interfaces:**
- Produces: `class QCPArea : public QCPLayoutElement` with `virtual QCPColorScale* colorScale()` returning
  `nullptr` (`QCPAxisRect` overrides it with today's lazy scale). `axes()` arrives in Task 3 with its type.
- Produces: `class QCPPolarArea : public QCPArea`: `explicit QCPPolarArea(QCustomPlot*)`;
  `QCPPolarAxisAngular* angularAxis() const`; `QCPPolarAxisRadial* radialAxis(int index = 0) const`,
  `int radialAxisCount() const`, `QCPPolarAxisRadial* addRadialAxis(QCPPolarAxisRadial* = nullptr)`,
  `bool removeRadialAxis(QCPPolarAxisRadial*)`; `QCPPolarGrid* grid() const`; `QPointF center() const`,
  `double radius() const`, `QRegion exactClipRegion() const`; `QList<QCPPolarGraph*> graphs() const`;
  `template <typename T> add(...)` for `QCPPolarGraph` (today's angular `add<T>`, `layoutelement-angularaxis.h:259-283`,
  without its try/catch rethrow: misuse instead); background (`setBackground` family, moved); drag and
  wheel (moved from the angular axis, P-4 of the inventory); `plot.add<QCPPolarArea>(row, col)` through
  `Construct<LayoutPart>`.
- `QCPPolarAxisAngular` becomes a plain `QCPLayerable` child of its area: `explicit
  QCPPolarAxisAngular(QCPPolarArea*)`, `QCPPolarArea* area() const`; it keeps its range, ticks, labels and
  drawing; everything listed for the area above leaves it (no forwarders, D6).
- `QCPPolarGraph(QCPPolarAxisAngular* key, QCPPolarAxisRadial* value)` keeps its signature (design 5) and
  registers with the key axis's area.

- [ ] **Step 1: Failing tests** in `TestPolar`: `areaOwnsItsAxes` (`plot.add<QCPPolarArea>(0, 0)`; the area
  has one angular axis, one radial axis and a grid; `area->angularAxis()->area() == area`);
  `areaLaysOutCentreAndRadius` (a 400x300 plot: `center()` is the area rect's centre, `radius()` half its
  smaller side, as `layoutelement-angularaxis.cpp:597-630` computes today); `deletingTheAreaMidDragIsSafe`
  (press on the area, `delete area` from a `rangeChanged` slot of the angular axis, move, release: no
  crash; for ASan); `aPolarGraphAddsToTheArea` (`area->add<QCPPolarGraph>()`; it is listed in
  `area->graphs()`). Expected: FAIL to compile.
- [ ] **Step 2: Implement**; move every polar user to the new API; delete the inset layout (P-2).
- [ ] **Step 3:** Run `TestPolar`, the moved polar slots, `TestReferenceM4e` (the Task 1 polar PNGs match
  unchanged), the suite offscreen and under Xvfb, and the `QCPPolar_*` rows (gate: their Task 1 numbers).
- [ ] **Step 4: Commit** `feat(polar): QCPPolarArea takes the layout role; QCPArea under QCPAxisRect`.

### Task 3: `QCPAxisBase` under the three axes; polar axis selection

**Files:**
- Create: `src/axis/axis-base.{h,cpp}` (`QCPAxisBase`)
- Modify: `src/axis/axis.{h,cpp}`, `src/polar/angularaxis.*`, `src/polar/radialaxis.*`,
  `src/layoutelements/area.h` (adds `axes()`), `src/layoutelements/axisrect.cpp`, `src/polar/polar-area.cpp`,
  every user of the moved enums (grep `QCPPolarAxisRadial::st`, `QCPPolarAxisAngular::sp`,
  `QCPPolarAxisRadial::sp`)
- Test: `tests/auto/test-polar/`, `tests/auto/test-axis-base/` (new; `TestAxisBase`)

**Interfaces:**
- Produces: `class QCPAxisBase : public QCPLayerable` holding: `ScaleType`, `SelectablePart`,
  `SelectableParts` (P-3); `range()`, `setRange` (all four overloads of `QCPAxis` today),
  `setRangeLower/Upper`, `moveRange`, `scaleRange` (both), `rangeReversed`/`setRangeReversed`,
  `scaleType`/`setScaleType`, `rangeDraggable`/`setRangeDraggable`, `rangeZoomable`/`setRangeZoomable`,
  `ticker`/`setTicker`, `tickVector`, `subTickVector`, `tickVectorLabels` (P-4), `selectedParts`,
  `selectableParts`, `selected`, `selectable`, `setSelectedParts`, `setSelectableParts`, `linkTo`,
  `unlink`, `linkSource`, `link` (P-5), `QCPArea* area() const`; signals `rangeChanged(const QCPRange&,
  const QCPRange&)`, `scaleTypeChanged(QCPAxisBase::ScaleType)`, `selectionChanged()`,
  `selectableChanged()`; protected `virtual void rangeHasChanged()` for each subclass's own reaction
  (tick vectors, dirty marking, link refresh) and `virtual SelectablePart getPartAt(const QPointF&) const = 0`.
- `QCPAxis : public QCPAxisBase`, `QCPPolarAxisAngular : public QCPAxisBase`, `QCPPolarAxisRadial : public
  QCPAxisBase`; each keeps only what is its own geometry, drawing and label formatting.
- Produces: `virtual QList<QCPAxisBase*> QCPArea::axes() const = 0`, implemented by `QCPAxisRect` (all its
  axes, today's order) and `QCPPolarArea` (angular, then radial axes).

- [ ] **Step 1: Failing tests:** `TestAxisBase::everyAxisSharesTheRangeModel` (data-driven over the three
  classes: `setRange`, `moveRange`, `scaleRange`, reversal, log scale sanitizing a range through 0, one
  `rangeChanged(new, old)` per change through `qcpEmit`, `rangeDraggable(false)` honoured by the area's
  drag); `cartesianAxisApiStillCompiles` (uses `QCPAxis::stLogarithmic`, `QCPAxis::spTickLabels`,
  `QCPAxis::SelectableParts`, `connect(axis, &QCPAxis::scaleTypeChanged, ...)`, `linkTo(QCPAxis*)` and
  checks one value each — compile is the main assertion); `linkingAPolarAxisIsRefused` (P-5);
  `TestPolar::polarAxisPartsAreClickable` (click on the angular base circle → `spAxis`; on an angular tick
  label → `spTickLabels`; on the radial spoke → `spAxis`; on its tick label → `spTickLabels`; 30 px away →
  -1; `setSelectableParts(spNone)` + `onlySelectable` → -1); `TestPolar::radialDragAndWheelWork` (the
  radial axis' drag and wheel bodies commented out today now act: a wheel over the area zooms the radial
  range about the cursor's radius). Expected: FAIL (compile, then assertions).
- [ ] **Step 2: Implement**; delete the moved members and the polar copies of the range model (P-4 of
  the inventory lists them).
- [ ] **Step 3:** Run the new tests, `TestLinkedAxes`, `TestQCPAxisRect`, the axis tests, the polar tests,
  `TestReferenceM4e`, the suite offscreen and under Xvfb, ASan on `TestAxisBase` and `TestPolar`, and the
  bench rows `QCPAxis_*`, `QCPItems_Pan10k`, `QCPPlottables_*`, `QCPPolar_*`, `QCPGpu_MultiGraphPan1M`
  (A/B against the task's base).
- [ ] **Step 4: Commit** `refactor(axis): QCPAxisBase under QCPAxis and the polar axes; polar parts clickable`.

### Task 4: `QCPTransform` as a variant: cartesian, polar, custom

**Files:**
- Modify: `src/coords.{h,cpp}`
- Create: `src/coords-custom.h` (`QCPCustomMapping`, public header, in `src/qcp.h`)
- Test: `tests/auto/test-transform/` (new `TestTransform`) plus the existing coordinate tests

**Interfaces:**
- Produces: `QCPTransform` holding `std::variant<Cartesian, Polar, Custom>` privately; public API
  unchanged (`QCPTransform()`, `QCPTransform(const QCPAxis*, const QCPAxis*)`, `fitted`, every `map`
  overload, `isTranslationOf`, `keyIsVertical`, `valid`, `operator==`) plus:
  - `static QCPTransform polar(const QCPPolarAxisAngular&, const QCPPolarAxisRadial&)`: snapshot of the
    area's centre and radius, the angular range, start angle and direction, the radial range, scale
    type and reversal;
  - `static QCPTransform custom(std::shared_ptr<const QCPCustomMapping>, quint64 revision)`;
  - `QCP::Data inverse(QPointF pixel) const`;
  - `enum class Kind { Cartesian, Polar, Custom }; Kind kind() const`.
- Produces in `src/coords-custom.h`:
  ```cpp
  class QCP_LIB_DECL QCPCustomMapping
  {
  public:
      virtual ~QCPCustomMapping() = default;
      virtual void map(std::span<const double> keys, std::span<const double> values,
                       std::span<QPointF> out) const = 0;
      virtual QCP::Data inverse(QPointF pixel) const = 0;
      virtual bool equivalentTo(const QCPCustomMapping& other, QPointF* delta) const;  // default false
  };
  ```
- Rules: every public call dispatches once on the kind, then runs a plain loop (no per-point virtual
  call, C-6). `operator==` compares values for cartesian and polar, and `(mapping pointer, revision)` for
  custom (F4). `isTranslationOf` is false for polar, `equivalentTo` for custom. `keyIsVertical` is false
  for polar and custom. Polar `map` clamps the radial pixel distance (P-6). The cartesian snapshot keeps
  its code; it stores its axis rect's geometry once instead of per axis (F3) only if that is free in the
  bench — otherwise leave it and say so.

- [ ] **Step 1: Failing tests** in `TestTransform`: `polarMappingMatchesTheAxes` (data-driven: linear and
  log radial axes, reversed radial, reversed angular, start angle 0/-90/30, angular range 0..360 and
  0..2π; for 50 points `polar(...).map(p)` equals `radialAxis->coordToPixel(angle, radius)` of today's
  axes within 1e-9 px); `polarInverseRoundTrips` (map then inverse within 1e-9 in data for points inside
  the range); `polarClampsTheRadius` (a radius below the radial lower bound maps to the centre; one far
  above maps at 1.05 R along its angle); `polarEqualityFollowsTheRanges` (equal for equal ranges;
  different after a rotation or a radial zoom; `isTranslationOf` false); `customMapsOncePerSpan` (a
  counting `QCPCustomMapping`: `map` over a 1 000-point span calls it once); `customEqualityUsesTheRevision`
  (same mapping, revision 1 vs 2 → not equal; same revision → equal); `cartesianIsUnchanged` (every
  existing `QCPTransform` test still passes — run `TestDrawContext`, `TestCoords` or wherever they live).
  Expected: FAIL to compile.
- [ ] **Step 2: Implement.**
- [ ] **Step 3:** Run `TestTransform`, the drawing and coordinate tests, `TestReferenceM4e`, the suite
  offscreen and under Xvfb, ASan on `TestTransform`, and the drawing bench rows (`QCPItems_Pan10k`,
  `QCPItems_Replot10k`, `QCPItems_MixedKinds_Pan1k`, `QCPPlottables_*`, `QCPBars_Pan10k`,
  `QCPErrorBars_Pan10k`, `QCPGpu_MultiGraphPan1M`, `QCPTransform_*`): 5 % gate, A/B against the task's base.
- [ ] **Step 4: Commit** `feat(coords): one frame type with cartesian, polar and custom mappings`.

### Task 5: Finish

**Files:** `changelog.txt` (milestone 4f section), `docs/migrating.md` (polar: the area replaces the angular
axis as the layout element), `docs/polar.md`, `docs/plans/neoqcp-3/baseline.md` ("End of milestone 4f"),
`docs/plans/neoqcp-3/backlog.md`, `docs/plans/neoqcp-3/milestone-4f-rulings.md` (new).

- [ ] **Step 1:** Changelog 4f: `QCPArea`, `QCPPolarArea`, `QCPAxisBase`, the transform's polar and custom
  mappings, polar axis click selection and radial drag/zoom; removed: the angular axis's layout role,
  inset layout and polar enum copies (no shims, D6).
- [ ] **Step 2:** Offscreen suite, Xvfb whole binary, ASan suite, clang check on every changed `.cpp`.
- [ ] **Step 3:** Bench: the whole table three runs offscreen and three under Xvfb against the end of 4e;
  every row over its gate settled by A/B against a build of the milestone's base (`-iterations`, steady
  state) — and A/B the item and plottable rows against the milestone's base whatever the table says
  (4e lesson: per-task gates can each pass while the milestone drifts).
- [ ] **Step 4:** Rulings file in the shape of `milestone-4e-rulings.md`; deferred minors also in `backlog.md`.
- [ ] **Step 5: Commit** `docs(neoqcp-3): end of milestone 4f — rulings, baseline, backlog, changelog`.
