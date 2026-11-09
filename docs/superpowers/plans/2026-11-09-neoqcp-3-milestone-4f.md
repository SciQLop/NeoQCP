# NeoQCP 3.0 milestone 4f — areas, the axis base, the polar area, the frame variant Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** steps 1-4 of the coordinate-systems design: a baseline of polar pictures and bench rows on the old
code; `QCPArea` under `QCPAxisRect` and a new `QCPPolarArea` that takes the angular axis's layout role;
`QCPAxisBase` under `QCPAxis` and both polar axes, with polar axis click selection and radial drag/zoom that
work; and `QCPTransform` as one frame type holding a cartesian, polar or custom mapping.

**Architecture:** first the polar layout role moves out of the angular axis into `QCPPolarArea` (a layout
element is already a layerable, so no class may be both an area and an axis — design review F1), and what
every area has (inset layout, background) moves to `QCPArea`. Then the range model shared by the three axis
classes moves into `QCPAxisBase`. Then `QCPTransform` keeps today's cartesian snapshot unchanged inside a
`std::variant` next to a polar and a custom mapping, dispatching once per call. Plottables, systems and items
are untouched until milestone 4g; the polar graph keeps painting until 4h, guarded by the Task 1 pictures.

**Tech Stack:** C++20, Qt 6.12, meson (and the mirrored `cmake/sources.cmake`), QtTest, `tests/benchmark`.

**Spec:** `docs/specs/2026-11-09-neoqcp-3-coordinate-systems-design.md` (approved; D1-D8; sections 2.1-2.4,
2.8, 3, 4 steps 1-4, 5). Facts: `docs/plans/neoqcp-3/m4f-inventory.md` (polar, cited by section and source
line), `docs/plans/neoqcp-3/coordsys-inventory.md`, the design review `.superpowers/coordsys-design-review.md`
(F1-F28) and this plan's review `.superpowers/m4f-plan-review.md` (R1-R25, all folded in).

## Calls made in this plan (labelled K-1..K-7; the maintainer can overrule any)

- **K-1 `QCPArea` holds what every area has**: it is a `QCPLayoutElement` with the inset layout
  (`insetLayout()`, today `QCPAxisRect`'s and the angular axis's), the background (`setBackground` family,
  today on both) and `virtual QCPColorScale* colorScale()` (default `nullptr`; `QCPAxisRect` overrides with
  its lazy scale). `virtual QList<QCPAxisBase*> allAxes() const = 0` arrives in Task 3 (the name avoids
  `QCPAxisRect::axes()`, which keeps its `QList<QCPAxis*>` signature, R3). Systems, per-system lists and
  area enumeration come with 4g.
- **K-2 The angular axis's inset layout becomes the polar area's** (the `QCPArea` one): today it is laid out
  each pass (`layoutelement-angularaxis.cpp:619`, `:628`) but nothing is ever put in it (R5).
- **K-3 `ScaleType` (`stLinear`, `stLogarithmic`) and `SelectablePart`/`SelectableParts` (`spNone`,
  `spAxis`, `spTickLabels`, `spAxisLabel`) move to `QCPAxisBase`** with their `Q_ENUM`/`Q_FLAG`, their
  `Q_DECLARE_METATYPE`/`Q_DECLARE_OPERATORS_FOR_FLAGS` (once, in `axis-base.h`; the copies in
  `axis.h:549-554`, `radialaxis.h:352-355`, `layoutelement-angularaxis.h:424-425` go, R6) and the
  `scaleType`, `selectableParts`, `selectedParts` `Q_PROPERTY`s (R7). `QCPAxis::stLogarithmic`,
  `QCPAxis::spAxis` and `QCPAxis::SelectableParts` keep compiling through inheritance.
- **K-4 Tick generation and label formatting stay in each axis class**; the base owns the ticker pointer
  and the tick, sub-tick and tick-label vectors. The polar axes' `rangeDrag`/`rangeZoom` become the base's
  `rangeDraggable`/`rangeZoomable` (polar 2.1 API break, D6, R9).
- **K-5 Links move to the base** (`linkTo(QCPAxisBase*)`, `unlink`, `linkSource() -> QCPAxisBase*`,
  `link()`, and `QCPAxisLink` into `axis-base.h`; `coords.h` and the handle op at `axis.h:727` follow, R8).
  They are NeoQCP-only API (milestone 4d), so no `QCPAxis*` wrappers. Linking a polar axis, or to one, is a
  misuse in 4f: no polar pixel rule exists yet (design 2.2).
- **K-6 The polar mapping clamps the radial fraction to [0, 1.05]**, i.e. the radius to `[0, 1.05 R]`: 5 %
  of the axis length beyond its upper end in the axis' own scale. On a linear axis that is today's margin
  above the range; below the range it now clamps to the centre where today's `coordToRadius` reflects
  through it (`radialaxis.cpp:1192-1218`) — a fix; on a log axis today's graph has no clamp (TODO
  `polargraph.cpp:1243`). This is new behaviour of the mapping, tested per scale (R17); the polar graph keeps
  its own clamp until 4h.
- **K-7 The polar graph keeps painting in 4f** with its own conversions; it becomes a plottable in 4h. The
  Task 1 polar pictures must keep matching through every task.

## Global Constraints

- One build or test at a time, in the foreground, `ninja -C build-qt612 -j 4`. Never two at once.
- Env in the SAME shell command as every meson/ninja call:
  `PATH=/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin:/home/jeandet/Qt/6.12.0/gcc_64/bin:$PATH`
  and `PKG_CONFIG_PATH=/home/jeandet/Qt/6.12.0/gcc_64/lib/pkgconfig`. Never `meson setup`; a source added to
  meson.build makes ninja re-run meson itself.
- Every added, renamed or removed source or public header is changed in `meson.build` (sources and headers),
  `cmake/sources.cmake` (both lists) and `src/qcp.h` (public headers) in the same task (R2).
- Offscreen suite: `QT_QPA_PLATFORM=offscreen QT_LOGGING_RULES="*.debug=true" meson test -C build-qt612 --num-processes 4`;
  totals = sum of the `Totals:` lines of `build-qt612/meson-logs/testlog.txt`. Xvfb: the whole `auto-tests`
  binary under `xvfb-run -a -s "-screen 0 1600x1200x24" env QT_QPA_PLATFORM=xcb`; afterwards
  `pgrep -x Xvfb -a | grep 1600` prints nothing.
- Bench gate (design 3): every row within 5 % (drawing rows: `QCPGpu_*`, `QCPDrawn_*`, `QCPItems_*`,
  `QCPBars_*`, `QCPPolar_*`, `QCPPlottables_*`) or 15 % (others) of the task's base, by A/B: five interleaved
  pairs of `-tickcounter -iterations 50 -median 5` against a build of the base commit in a scratchpad
  worktree (`QBENCHMARK_ONCE` rows: walltime `-median 7`). Never whole-table runs alone.
- Names (D7): `QCPArea`, `QCPPolarArea`, `QCPAxisBase`, `QCPCustomMapping`; no "Abstract" in new names.
- No shim, forwarder or intermediate form for the polar 2.1 API or NeoQCP-only API this milestone changes
  (D6); the 2.x cartesian API keeps every signature (design 5).
- Misuse through `QCP_CHECK_OR_RETURN`; signals through the M4a helpers exactly as today: `rangeChanged` via
  `qcpEmitIf`, `scaleTypeChanged` via `qcpEmitCurrent`, `selectionChanged`/`selectableChanged` via `qcpEmit`
  (R15); `contentChanged()`/`markDirty()` on every setter that changes what is drawn.
- A test fails before the change that makes it pass; a test that passes first is reported as a guard.
- Never bend library behaviour to please a test. Keep code size down: delete what a move replaces.
- Clang check (`clang++ -fsyntax-only` per changed `.cpp` from `compile_commands.json`) before each commit
  that adds a type or a template. No clang-format on whole files.
- Commit with explicit pathspecs; never `subprojects/.wraplock`, `subprojects/jkqtmathtext`, `qt.ini`.
- SciQLopPlots does not use polar; anything a task finds otherwise goes to
  `docs/plans/neoqcp-3/sciqlopplots-migration.md`, nowhere else.

## Review Focus

1. Deleting the polar area, its angular axis or a radial axis during a drag or a wheel does not crash and
   leaves the rest drawn (the `test-qcustomplot` polar deletion slots, moved in Task 2, plus
   `deletingTheAreaMidDragIsSafe`).
2. A cartesian plot draws, pans, zooms, links and repaints exactly as before (M4 references, `test-repaint`,
   `TestLinkedAxes`, the item and plottable bench rows; Tasks 3 and 4).
3. A log radial axis, a reversed angular axis and a rotated start angle map through the polar transform
   exactly as the old axes' `coordToPixel` did inside the range (Task 4 `polarMappingMatchesTheAxes`).
4. Clicking a polar axis selects the part under the cursor and nothing outside it; `onlySelectable` refuses
   an unselectable axis (Task 3 `polarAxisPartsAreClickable`).
5. 2.x cartesian code using `QCPAxis::stLogarithmic`, `QCPAxis::SelectableParts`, `&QCPAxis::scaleTypeChanged`
   and the `scaleType`/`selectedParts` properties still compiles and fires through the meta-object (Task 3
   guard `cartesianAxisApiStillCompiles` and `axisPropertiesStillNotify`).

---

### Task 1: Polar pictures and bench rows on the old code (baseline: green by construction)

**Files:**
- Modify: `tests/auto/test-reference-m4e/test-reference-m4e.{h,cpp}` (polar scenes; PNGs in its `reference/`)
- Modify: `tests/benchmark/benchmark.cpp`

**Interfaces:**
- Produces: `polarScene(const char* kind)` building, with today's API, `polar-basic` (as
  `docs/snippets/polar.cpp` `polar_basic`), `polar-radians` (`polar_radians`), `polar-gallery`
  (`docs/snippets/gallery.cpp:469-510`), `polar-log` (log radial axis 1..1000), `polar-rotated`
  (`setAngle(30)`, reversed angular axis), `polar-axis-selected` (the angular axis' `spAxis` and the radial
  axis' `spTickLabels` selected, R24); 400x300 `toPixmap`, checked with `qcp::test::matchesReference`. Later
  tasks change how a scene is built, never its PNG.
- Produces rows (each builds, replots once, then times its step): `QCPPolar_Draw10k` (one graph, 10 000
  points, full replot), `QCPPolar_Rotate10k` (full polar repaint after a 0.5° angular range step; no polar
  recording exists before 4h, R22), `QCPPolar_RadialZoom10k` (radial range scaled by 1.01), `QCPPolar_Export10k`
  (`toPixmap(400, 300)`), `QCPPlottables_Pan200` (200 graphs of 1 000 points on one rect, pan step as
  `QCPItems_Pan10k`), `QCPPlottables_Zoom200` (`scaleRange(1.01)`).

- [ ] **Step 1:** Add the six scenes and slots; run with `QCP_WRITE_REFERENCE=1`, then without: all pass.
  Look at each PNG and describe it in the report (it must show the scene, not an empty disc).
- [ ] **Step 2:** Add the six rows; record three runs of each (`-tickcounter -iterations 50 -median 5`,
  offscreen) in the report.
- [ ] **Step 3: Commit** `test(m4f): polar pictures and bench rows on the old code`.

### Task 2: `QCPArea` and `QCPPolarArea`

**Files:**
- Create: `src/layoutelements/area.{h,cpp}` (`QCPArea`), `src/polar/polar-area.{h,cpp}` (`QCPPolarArea`)
- Modify: `src/layoutelements/layoutelement-axisrect.{h,cpp}` (derives `QCPArea`; its inset layout and
  background move up), `src/polar/layoutelement-angularaxis.*` (renamed `src/polar/angularaxis.{h,cpp}`; the
  class loses its layout role, inset layout, background, drag/wheel, graph list, `add<T>`),
  `src/polar/radialaxis.*`, `src/polar/polargrid.*`, `src/polar/polargraph.*`, `src/core.{h,cpp}`
  (`adopt`/`removeObject`/`take` paths that name the angular axis), `src/qcp.h`, `meson.build`,
  `cmake/sources.cmake`
- Modify every user: `tests/auto/test-qcustomplot` (polar slots), `test-ownership`, `test-repaint`,
  `test-builder`, `test-selection-model` (`:91-109`, R12), `test-reference-m4e` (scene builders only),
  `docs/snippets/polar.cpp`, `docs/snippets/gallery.cpp`, `examples/plots/mainwindow.cpp`,
  `tests/manual/mainwindow.cpp`, `docs/polar.md`, `docs/interactions.md` (polar selection paragraph, R21).
  `src/handle.h` needs no change (`QCPPolarGraph` keeps its name).
- Test: new `tests/auto/test-polar/` (`TestPolar`; register in `tests/auto/meson.build` and `autotest.cpp`)

**Interfaces:**
- Produces: `class QCPArea : public QCPLayoutElement`: `QCPLayoutInset* insetLayout() const`; the background
  family moved from `QCPAxisRect` (`setBackground(QPixmap)`, `setBackground(QPixmap, bool, Qt::AspectRatioMode)`,
  `setBackground(QBrush)`, `setBackgroundScaled`, `setBackgroundScaledMode`, getters, `drawBackground`);
  `virtual QCPColorScale* colorScale()` returning `nullptr`. `QCPAxisRect` keeps every 2.x signature.
- Produces: `class QCPPolarArea : public QCPArea`: `explicit QCPPolarArea(QCustomPlot*)` setting the plot's
  current layer, `minimumSize(50, 50)`, `setMargins(30)` and creating one angular axis, one radial axis and
  the grid, exactly as the angular constructor does today (`layoutelement-angularaxis.cpp:137-227`, R11);
  `update(UpdatePhase)` with today's centre/radius formula (`:597-630`); `QCPPolarAxisAngular*
  angularAxis() const`; `QCPPolarAxisRadial* radialAxis(int index = 0) const`, `int radialAxisCount() const`,
  `QCPPolarAxisRadial* addRadialAxis(QCPPolarAxisRadial* = nullptr)`, `bool removeRadialAxis(QCPPolarAxisRadial*)`;
  `QCPPolarGrid* grid() const`; `QPointF center() const`, `double radius() const`, `QRegion exactClipRegion() const`;
  `QList<QCPPolarGraph*> graphs() const`, `bool removeGraph(QCPPolarGraph*)` (the area lists graphs, it does
  not own them: removing unlists then deletes, as `removeGraph` does today, R25); `template <typename T> add(...)`
  for `QCPPolarGraph` — today's angular `add<T>` (`layoutelement-angularaxis.h:259-283`) including its two
  misuse checks and its `try`/`catch`/remove/rethrow guard (R10); drag and wheel moved from the angular axis
  (`:1710-1867`); `plot.add<QCPPolarArea>(row, col)` through the existing `Construct<LayoutPart>`.
- `QCPPolarAxisAngular` becomes a plain `QCPLayerable` child of its area: `explicit QCPPolarAxisAngular(QCPPolarArea*)`,
  `QCPPolarArea* area() const`; it keeps its range, ticks, labels and drawing. No forwarders (D6).
- `QCPPolarGraph(QCPPolarAxisAngular* key, QCPPolarAxisRadial* value)` keeps its signature (design 5) and
  registers with `key->area()`.

- [ ] **Step 1: Failing tests** in `TestPolar`: `areaOwnsItsAxes` (`plot.add<QCPPolarArea>(0, 0)`: one
  angular axis, one radial axis, a grid; `area->angularAxis()->area() == area`); `areaLaysOutCentreAndRadius`
  (400x300 plot with the area alone in the layout: assert `rect()`, `center()` = rect centre, `radius()` =
  half the smaller side, before any pixel compare); `deletingTheAreaMidDragIsSafe` (press on the area,
  `delete area` from the angular axis' `rangeChanged` slot during the move, release: no crash);
  `aPolarGraphAddsToTheArea` (`area->add<QCPPolarGraph>()` is in `graphs()`; `removeGraph` unlists then
  deletes); `insetAndBackgroundAreAreaFeatures` (a legend in `area->insetLayout()` is laid out inside the
  area; `setBackground(QBrush)` paints the area). Expected: FAIL to compile.
- [ ] **Step 2: Implement**; move every listed user to the new API.
- [ ] **Step 3:** Run `TestPolar`, the moved polar slots, `TestSelectionModel`, `TestQCPAxisRect` (inset and
  background unchanged), `TestReferenceM4e` (polar PNGs unchanged), the suite offscreen and under Xvfb, ASan
  on `TestPolar`, and the `QCPPolar_*` rows (gate: their Task 1 numbers).
- [ ] **Step 4: Commit** `feat(polar): QCPPolarArea takes the layout role; QCPArea under QCPAxisRect`.

### Task 3: `QCPAxisBase` under the three axes; polar axis selection and radial drag/zoom

**Files:**
- Create: `src/axis/axis-base.{h,cpp}` (`QCPAxisBase`, `QCPAxisLink`)
- Modify: `src/axis/axis.{h,cpp}` (incl. the macros at `axis.h:549-554` and the handle op at `:727`),
  `src/polar/angularaxis.*` (incl. macros), `src/polar/radialaxis.*` (incl. macros `:352-355`, drag/wheel
  bodies `:1340-1435`), `src/coords.{h,cpp}` (`QCPAxisLink` include), `src/layoutelements/area.h` (adds
  `allAxes()`), `src/layoutelements/layoutelement-axisrect.cpp`, `src/polar/polar-area.cpp`, `src/qcp.h`,
  `meson.build`, `cmake/sources.cmake`, every user of the moved names (grep `QCPPolarAxisRadial::st`,
  `QCPPolarAxisRadial::sp`, `QCPPolarAxisAngular::sp`, `rangeDrag(`, `rangeZoom(`, `setRangeDrag(`,
  `setRangeZoom(` on polar axes)
- Test: `tests/auto/test-polar/`, new `tests/auto/test-axis-base/` (`TestAxisBase`), `tests/auto/test-repaint`

**Interfaces:**
- Produces: `class QCPAxisBase : public QCPLayerable` holding K-3's enums and properties; `range()`,
  `setRange` (the four `QCPAxis` overloads), `setRangeLower/Upper`, `moveRange`, `scaleRange` (both),
  `rangeReversed`/`setRangeReversed`, `scaleType`/`setScaleType`, `rangeDraggable`/`setRangeDraggable`,
  `rangeZoomable`/`setRangeZoomable`, `ticker`/`setTicker`, `tickVector`, `subTickVector`, `tickVectorLabels`,
  `selectedParts`, `selectableParts`, `selected`, `selectable`, `setSelectedParts`, `setSelectableParts`,
  K-5's link API, `QCPArea* area() const`; signals `rangeChanged(const QCPRange&, const QCPRange&)`,
  `scaleTypeChanged(QCPAxisBase::ScaleType)`, `selectionChanged()`, `selectableChanged()`.
- The one range funnel lives in the base (today's `applyRange`/`commitRange`, M4d): it sanitizes, stores,
  rebuilds tick vectors, emits through `qcpEmitIf`, then calls `protected virtual void rangeDidChange()`.
  `QCPAxis::rangeDidChange()` does what `axis.cpp:701-704`, `:731-736` do today (the rect's and items'
  dirty marking) and refreshes linked axes; the polar axes' override marks their area's layers (R19).
  `protected virtual SelectablePart getPartAt(const QPointF&) const = 0`.
- `QCPAxis`, `QCPPolarAxisAngular`, `QCPPolarAxisRadial` derive from `QCPAxisBase` and keep only their own
  geometry, drawing and label formatting.
- `virtual QList<QCPAxisBase*> QCPArea::allAxes() const = 0`: `QCPAxisRect` returns all its axes in
  `axes()` order; `QCPPolarArea` the angular axis then the radial axes.

- [ ] **Step 1: Failing tests:** `TestAxisBase::everyAxisSharesTheRangeModel` (data-driven over the three
  classes: `setRange`, `moveRange`, `scaleRange`, reversal, a log scale sanitizing a range through 0, one
  `rangeChanged(new, old)` per change, a queued then superseded `rangeChanged` dropped, `rangeDraggable(false)`
  honoured by the area's drag); `linkingAPolarAxisIsRefused` (K-5); `TestPolar::polarAxisPartsAreClickable`
  (angular base circle → `spAxis`; an angular tick label → `spTickLabels`; the radial spoke → `spAxis`; its
  tick label → `spTickLabels`; 30 px away → -1; `setSelectableParts(spNone)` with `onlySelectable` → -1);
  `TestPolar::radialDragAndWheelWork` (a wheel over the area zooms the radial range about the cursor's
  radius; a drag along a spoke pans it); `TestRepaint` rows: a cartesian and a polar range change repaint
  their layers (R19). Guards (pass before and after, reported as guards): `cartesianAxisApiStillCompiles`
  (`QCPAxis::stLogarithmic`, `QCPAxis::spTickLabels`, `QCPAxis::SelectableParts`,
  `connect(axis, &QCPAxis::scaleTypeChanged, ...)`, one value each) and `axisPropertiesStillNotify`
  (`QMetaProperty` of `scaleType`/`selectedParts` on a `QCPAxis` has its notify signal and it fires).
- [ ] **Step 2: Implement**; delete the moved members and the polar copies of the range model.
- [ ] **Step 3:** Run the new tests, `TestLinkedAxes`, `TestQCPAxisRect`, `TestSignalDelivery`, the axis and
  polar tests, `TestReferenceM4e`, the suite offscreen and under Xvfb, ASan on `TestAxisBase` and `TestPolar`,
  and the rows `QCPAxis_*`, `QCPItems_Pan10k`, `QCPPlottables_*`, `QCPPolar_*`, `QCPGpu_MultiGraphPan1M` (A/B).
- [ ] **Step 4: Commit** `refactor(axis): QCPAxisBase under QCPAxis and the polar axes; polar parts clickable`.

### Task 4: `QCPTransform` as a variant: cartesian, polar, custom

**Files:**
- Modify: `src/coords.{h,cpp}`; create `src/coords-custom.h` (`QCPCustomMapping`); `src/qcp.h`,
  `meson.build`, `cmake/sources.cmake`
- Test: new `tests/auto/test-transform/` (`TestTransform`); existing `tests/auto/test-coords`,
  `tests/auto/test-draw-context`

**Interfaces:**
- Produces: `QCPTransform` holding `std::variant<Cartesian, Polar, Custom>` privately; every current public
  member unchanged (`QCPTransform()`, `QCPTransform(const QCPAxis*, const QCPAxis*)`, `fitted`, the three
  `map` overloads, `isTranslationOf`, `keyIsVertical`, `valid`, `operator==`), plus:
  `static QCPTransform polar(const QCPPolarAxisAngular&, const QCPPolarAxisRadial&)` (snapshot of the area's
  centre and radius, angular range, start angle and direction, radial range, scale type, reversal);
  `static QCPTransform custom(std::shared_ptr<const QCPCustomMapping>, quint64 revision)`;
  `QCP::Data inverse(QPointF pixel) const` (cartesian: each axis' `pixelToCoord` arithmetic on the snapshot,
  orientation-aware, R18); `enum class Kind { Cartesian, Polar, Custom }; Kind kind() const`.
- Produces in `src/coords-custom.h`:
  ```cpp
  class QCP_LIB_DECL QCPCustomMapping
  {
  public:
      virtual ~QCPCustomMapping() = default;
      virtual void map(std::span<const double> keys, std::span<const double> values,
                       std::span<QPointF> out) const = 0;
      virtual void map(std::span<const QPointF> data, std::span<QPointF> out) const;  // default: split, call above
      virtual QCP::Pixels mapPoint(QCP::Data data) const;                               // default: one-element span
      virtual QCP::Data inverse(QPointF pixel) const = 0;
      virtual bool equivalentTo(const QCPCustomMapping& other, QPointF* delta) const;   // default false
  };
  ```
- Rules: every public call dispatches once on the kind, then runs a plain loop (no per-point virtual).
  `operator==` compares values for cartesian and polar, `(mapping pointer, revision)` for custom.
  `isTranslationOf` is false for polar and asks `equivalentTo` for custom. `keyIsVertical` is false for polar
  and custom. Polar `map` applies K-6. The cartesian snapshot keeps its code.

- [ ] **Step 1: Failing tests** in `TestTransform` (the red set): `polarMappingMatchesTheAxes` (data-driven:
  linear and log radial, reversed radial, reversed angular, start angle 0/-90/30, angular range 0..360 and
  0..2π; for 50 points inside the radial range `polar(...).map(p)` equals the radial axis'
  `coordToPixel(angle, radius)` within 1e-9 px); `polarInverseRoundTrips`; `polarClampsTheRadius` (pixel
  space, per scale: linear — a radius below the lower bound maps to the centre, one far above to 1.05 R
  along its angle; log — the same in log fraction); `polarEqualityFollowsTheRanges` (equal for equal
  ranges; different after a rotation or a radial zoom; `isTranslationOf` false); `customMapsOncePerSpan` (a
  counting mapping: a 1 000-point `map` over each overload calls the mapping once); `customEqualityUsesTheRevision`;
  `cartesianInverseMatchesPixelToCoord` (vertical key axis and log value axis included). Guards:
  `TestCoords` and `TestDrawContext` as they are. Expected: FAIL to compile.
- [ ] **Step 2: Implement.**
- [ ] **Step 3:** Run `TestTransform`, `TestCoords`, `TestDrawContext`, `TestReferenceM4e`, the suite
  offscreen and under Xvfb, ASan on `TestTransform`, and the drawing rows (`QCPItems_Pan10k`,
  `QCPItems_Replot10k`, `QCPItems_MixedKinds_Pan1k`, `QCPPlottables_*`, `QCPBars_Pan10k`, `QCPErrorBars_Pan10k`,
  `QCPGpu_MultiGraphPan1M`, `QCPTransform_*`): 5 % gate, A/B against the task's base.
- [ ] **Step 4: Commit** `feat(coords): one frame type with cartesian, polar and custom mappings`.

### Task 5: Finish

**Files:** `changelog.txt` (milestone 4f section), `docs/migrating.md` (polar: the area replaces the angular
axis as the layout element; `rangeDrag` → `rangeDraggable` on polar axes), `docs/polar.md`,
`docs/plans/neoqcp-3/baseline.md` ("End of milestone 4f"), `docs/plans/neoqcp-3/backlog.md`,
`docs/plans/neoqcp-3/milestone-4f-rulings.md` (new).

- [ ] **Step 1:** Changelog 4f: `QCPArea` (inset layout, background), `QCPPolarArea`, `QCPAxisBase`, the
  polar and custom mappings, polar axis click selection, radial drag/zoom; removed: the angular axis's layout
  role and the polar enum copies (no shims, D6); link API on `QCPAxisBase` (NeoQCP-only).
- [ ] **Step 2:** Offscreen suite, Xvfb whole binary, ASan suite, clang check on every changed `.cpp`.
- [ ] **Step 3:** Bench: the whole table three runs offscreen and three under Xvfb against the end of 4e;
  every row over its gate settled by A/B against a build of the milestone's base (steady state) — and A/B the
  item, plottable and polar rows against the milestone's base whatever the table says (4e lesson: per-task
  gates can each pass while the milestone drifts).
- [ ] **Step 4:** Rulings file in the shape of `milestone-4e-rulings.md`; deferred minors also in `backlog.md`.
- [ ] **Step 5: Commit** `docs(neoqcp-3): end of milestone 4f — rulings, baseline, backlog, changelog`.
