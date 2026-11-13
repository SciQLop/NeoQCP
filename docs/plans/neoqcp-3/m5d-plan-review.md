# Pre-flight review: NeoQCP 3.0 milestone 5d plan

Plan: `docs/superpowers/plans/2026-11-13-neoqcp-3-milestone-5d.md`. Checked against the tree at `d4d1a8a`
(worktree `/home/jeandet/Documents/prog/NeoQCP-3.0`, branch `neoqcp-3.0`). All paths are relative to the worktree
unless they start with `SciQLopPlots/`.

Tally: 2 blockers, 10 important, 7 minor.

Coverage summary. Every item of C6, C7, C9, A10 and A11 has a task. The data-source pixel API from C7 is already
gone (no `getLinesAll`/`getOptimizedLineData` left in `src/`), so it is right that the plan does not list it. Two
calls do not hold against the current code: D-3 (alignment groups cannot express every margin-group layout in the
tree) and D-4 (the members hold state the system does not have). A10 asks the plan to carry complete lists. The plan
points at the inventory, which is stale, and at "task reports" for C9.

---

## Blockers

### B1. D-4: the members hold axis state the system does not have

What the plan says: `keyAxis()`/`valueAxis()` are "computed from the system: the cartesian system's axes, null
otherwise". The same rule applies to the item position.

What the code does: in two cases the members hold axes and there is no system.

1. **One-axis item positions (4i R-2 / A6, a standing ruling).** `setAxes(nullptr, yAxis)` keeps the one axis and
   leaves the system null: `src/items/item.cpp:970-971` (`mKeyAxis = resolved ? (...) : keyAxis;`). Users:
   - `tests/auto/test-item-system/test-item-system.cpp:152-195` (`aOneAxisPositionKeepsTwoXPixels`: `keyAxis()`
     must equal the axis it was given while `system()` is null);
   - the same file, `:621-645`;
   - `examples/axis-tags/axistag.cpp:39` (`position->setAxes(0, mAxis)`).

   Pixel resolution also reads the single axis: `item.cpp:883-932` and `:1113-1172`. If the getters are computed
   from the system, this ruling breaks and the example draws in the wrong place.
2. **Half-set plottable pairs.** `QCPAbstractPlottable::resolveAxes` keeps an unresolved pair with no system
   (`src/plottables/plottable.cpp:887-898`). `setValueAxis` then reads the stored key axis (`:868-871`). The 2.x
   two-step pattern depends on this:
   - `tests/auto/test-errorbar-geometry/test-errorbar-geometry.cpp:312-313` sets key = yAxis, then value = xAxis.
     The middle state (yAxis, yAxis) resolves to no system.
   - `test-qcpbars.cpp:547-548` and `test-qcpaxisrect.cpp:237-238` do the same.

   With computed getters, `setValueAxis(xAxis)` would read the key axis from the old system and resolve to (xAxis,
   xAxis).

Amendment. Replace the D-4 text with:

> The protected projections stop being protected. `QCPAbstractPlottable` and `QCPItemPosition` keep one private
> raw axis pair. When the system is cartesian, the pair is that system's projection. When there is no system, the
> pair is the unresolved one: a one-axis position (4i R-2), or a half-set plottable pair. `setSystem`, `setAxes`
> and `resolveAxes` write it. Nothing else does. `keyAxis()`/`valueAxis()` return it as a handle. The library
> reads it through `keyAxisRaw()`/`valueAxisRaw()`/`axisRectRaw()`, which already exist (`plottable.h:330-331`,
> `item.cpp:386`). Out-of-tree subclasses port to `keyAxisRaw()`/`valueAxisRaw()` (protected) or to `system()`.

`QCPItemPosition` has no subclass anywhere in `src/`, `tests/` or `examples/`, so for the item position the change
is "protected becomes private". If the maintainer instead wants the members deleted outright, the plan needs a
ruling that drops 4i R-2 and changes `axistag.cpp`. Without that ruling, D-4 contradicts an earlier ruling.

### B2. D-3: alignment groups cannot express three in-tree margin-group layouts

What the plan says: "Alignment groups (4d) are the replacement … a test that used a margin group to align is
ported."

Facts:

- An element belongs to **one** alignment group: `QPointer<QCPAlignmentGroup> mAlignmentGroup`, `src/layout.h:184`.
- `add()` moves an element out of its previous group: `src/alignment-group.cpp:78-79`.
- The group's sides apply to every member: `mSides`, `alignment-group.h:56`.
- The 4d plan chose this on purpose ("One group per element; joining another leaves the …",
  call P-7, `docs/superpowers/plans/2026-11-08-neoqcp-3-milestone-4d.md:55-57`).

Margin groups allow per-element side subsets, and they allow one element in a group per side. These users need
that:

1. **The joint-density gallery snippet.** `docs/snippets/gallery.cpp:329-334`: `main` is in `columns` (left/right,
   with `top`) and in `rows` (top/bottom, with `right`). It needs two groups.
2. **The QCPColorScale doc image.** `documentation/doc-image-generator/mainwindow.cpp:927-945` has one group with
   three members:
   - the axis rect on `msAll`;
   - a vertical scale on top/bottom only;
   - a horizontal scale on left/right only.

   As one alignment group with uniform sides, the vertical scale's left/right would align to the rect's.
3. **The advanced-axes example.** `examples/plots/mainwindow.cpp:1179-1182`, one group:
   - `subRectLeft` on `msLeft`;
   - `subRectRight` on `msRight`;
   - `wideAxisRect` on left and right.

   With uniform sides, `subRectLeft`'s right edge would be pulled to the shared right edge.
4. **A manual test.** `tests/manual/mainwindow.cpp:678-680` uses `msAll` on two stacked elements, which aligns their
   top/bottom against each other. It is built: `tests/meson.build:3`.

Amendment. Add a call D-3a before Task 2, and let the maintainer pick one option:

- **(a)** Alignment membership becomes per side: an element may join one group per side. `create(members,
  sides)` keeps its signature. This reverses part of 4d P-7. It needs failing tests first. It is the only
  option that gives every 2.x mixed-side layout a port.
- **(b)** Keep one group per element. Restructure the three layouts above. List their pictures as "moved,
  looked at". Add a 3.0-limit row to `docs/migrating.md` ("mixed-side margin groups have no port").

Option (a) is the edge-case-correct choice. Option (b) is cheaper, but it leaves 2.x users without a port.

---

## Important

### I1. D-2: "final (not virtual)" is self-contradictory, and the icon entry should be `virtual … final`

1. `QCPAbstractPlottable::draw(QCPPainter*)` overrides the pure virtual `QCPLayerable::draw` (`src/layer.h:323`).
   It stays virtual and becomes `final`. That is the item precedent: `void draw(QCPPainter* painter) final;`,
   `src/items/item.h:375`.
2. `drawLegendIcon(QCPPainter*, const QRectF&)` is introduced in the plottable (`plottable.h:380`). Its callers use a
   base pointer:
   - the legend row, `src/layoutelements/layoutelement-legend.cpp:392`;
   - the group row, `src/plottables/plottable-group.cpp:278`.

   If it becomes plain non-virtual, a 2.x subclass that declares it without `override` still compiles. Its icon
   is then silently ignored. If it becomes `virtual … final`, any redeclaration fails to compile.

Amendment. D-2 reads: "`draw(QCPPainter*)` is `final`. `drawLegendIcon(QCPPainter*, const QRectF&)` is declared
`virtual … final`, so that a 2.x override fails to compile instead of being hidden."

### I2. The "requires check" for non-overridability cannot be written

A requires-expression tests whether an expression is valid. It cannot test whether a class definition with an
`override` is ill-formed. C++20 also cannot detect `final` on a member function. The test plan has no negative-compile
mechanism. There is no `should_fail` and no `cpp.compiles()` in `meson.build` or `tests/`, and the item's `final`
(4e P-4) had no such test.

Amendment. Pick one:

- drop that bullet;
- or add a configure-time `cpp.compiles()` check that is expected to fail, with a snippet that overrides each
  entry, wired as an assertion in `tests/meson.build`.

The first option keeps Task 1 small.

### I3. D-1: two of the four icons cannot be recorded natively as they look today

- **The financial icon** (`src/plottables/plottable-financial.cpp:633-670+`) needs things the recording API does not
  have:
  - it calls `painter->setAntialiasing(false)`;
  - it clips to two diagonal half-regions (`setClipRegion`) for the two-coloured style.

  `QCPDrawContext` has no clip command and no per-command antialiasing (`src/painting/draw-context.h:185-238`).
- **The error-bars icon** (`plottable-errorbar.cpp:598-620`) uses fixed pixel offsets from the rect edges (`top + 2`,
  `±4`). It also branches on the value axis orientation. The icon recording is cached across rect sizes
  (`src/drawable.cpp:1082-1084`; the key does not include the rect). So a rect-relative pixel offset has no native
  form.
- **Any icon recording needs a data box.** One without a data box draws nothing (`drawable.cpp:1099-1100`). A
  painter callback alone records no data box.
- **The statistical-box and intervals icons** (`plottable-statisticalbox.cpp:607-616`, `plottable-intervals.cpp:992-995`)
  are pure rect fractions. They map onto `AxisRatio`/`Data` commands.

Amendment. D-1 names the method:

- statistical box and intervals record native `rect` commands, with an invisible bounding rect, as the bars
  precedent does (`plottable-bars.cpp:1152-1159`);
- financial and error bars record that bounding rect plus a `painter()` callback, which keeps them pixel-identical;
- or they become native commands, and their pictures are listed as moved and looked at.

The reference pictures cover only one branch of each class (`tests/auto/test-reference-m4e/test-reference-m4e.cpp:736,
766, 786, 811`):

- error bars: value error on a vertical value axis only;
- financial: candlestick, two-coloured, only.

Add failing-first pictures for these before the port:

- OHLC;
- not two-coloured;
- a key error, or a horizontal value axis.

### I4. Task 1's list of "overriding tests" is wrong

Some tests in the list only **call** the painter entry, through `using`. They keep compiling as they are:

- `test-colormap.cpp:161-169`;
- `test-multigraph.cpp:3313, 4447, 4459`;
- `test-qcpcurve.cpp:24, 317`;
- `test-drawable.cpp:320, 346, 379, 395, 420`.

`test-user-system.cpp:217` overrides `draw(QCPPainter*)` on a `QCPAxisBase` subclass (`FlatAxis`, `:205`). Axes keep
the painter path, so it stays as it is.

The tests that really override are:

- `test-data-swap.cpp:55-56`;
- `test-repaint.cpp:112-114` and `:142-144` (stubs);
- `test-repaint.cpp:167-170` (`FillStub` paints and reads `mKeyAxis`);
- `test-drawable.cpp:484` (`PainterOnlyDots`).

`TestDrawable::aPainterOnly1DPlottableKeepsThePointClick` (`test-drawable.cpp:2365-2382`) tests the fallback that D-2
deletes. It is deleted, not ported.

Amendment. Replace the bullet's test list with:

> `test-data-swap` and `test-repaint` drop their empty painter overrides. `FillStub` records `rect(AxisRatio{0,0},
> AxisRatio{1,1})`; check that its translate tests (`:327-330`, `:931-965`, `:1390-1396`) still test the translate
> path. `PainterOnlyDots` and its fallback test go.

### I5. D-4 hot paths: computed projections need a `qobject_cast` per call

- `coordsToPixels`/`pixelsToCoords` (`plottable.cpp:1047-1120`) read the members per point.
- `keyAxisRaw()` (`plottable.h:330`) serves the per-bar paths (5b R-6).
- A cartesian system holds `QCPAxisBase*`. The projection is `qobject_cast<QCPAxis*>` (`plottable.cpp:505-509`,
  `:920-923`), because a user axis may not be a `QCPAxis` (`FlatAxis`, `test-user-system.cpp:205`). Computing it per
  call is a meta-object walk on a per-point path.
- The member readers inside plottable classes number about 236 lines:

  | File | Lines |
  |---|---|
  | `plottable-graph.cpp` | 43 |
  | `plottable-intervals.cpp` | 38 |
  | `plottable-financial.cpp` | 25 |
  | `plottable-bars.cpp` | 23 |
  | `plottable-errorbar.cpp` | 19 |
  | `plottable.cpp` | 19 |
  | `plottable-histogram2d.cpp` | 16 |
  | `plottable-colormap2.cpp` | 14 |
  | `plottable-histogram.cpp` | 11 |
  | `plottable-statisticalbox.cpp` | 8 |
  | `plottable-colormap.cpp` | 7 |
  | `plottable.h` | 5 |
  | `plottable1d.h` | 4 |
  | `core.cpp` (`:1497-1498`, direct writes) | 2 |
  | `plottable-curve.cpp` | 1 |
  | `plottable-colormapped.cpp` | 1 |

- On the item position: `item.cpp` has 28 lines reading `mKeyAxis`/`mValueAxis` and 7 reading `mAxisRect`.
  `item.cpp:833-835` reads all three per position per frame.

Amendment. This is the same fix as B1: one private cached raw pair, updated where the system changes. All readers
become `keyAxisRaw()`/`valueAxisRaw()`/`axisRectRaw()`. The bench gate stays as written.

### I6. A10 asks the plan for complete lists; the plan defers them

A10 says: "the 5c/5d plans carry the complete lists: the auto-margin branch with `QCPMarginGroup` and its users …;
the item position's … projections go together; the span items' pen, selected pen, accessors and constructor calls;
… the C9 touched-path list."

The plan instead points at `m5-inventory.md`, which was written at `6015c0a`, before 5a-5c. For C9 it says "the task
reports list each". That is the reverse of A10.

Amendment: paste the lists from I7, I8 and I5 into D-3, D-5 and D-4. For C9, add a rule for the most common case:

- There are 22 "invalid key/value axis" `qDebug` lines in `src/` (`grep -rn "invalid key\|invalid value"`). They sit
  in the functions D-4 rewrites.
- A plottable without a system is "inert on purpose" (`plottable.cpp:924-925`). That is a state, not a caller error.
- `QCP_CHECK` is fatal in debug builds. Converting these lines would abort on a legitimate state.

Proposed rule: "invalid axis" reports in drawing, range and hit paths are states. They are deleted, or stay
`qDebug`. Only setter or argument checks become `QCP_CHECK_OR_RETURN`.

The other `qDebug` counts in the files Tasks 1-3 touch:

| File | `qDebug` lines |
|---|---|
| `layout.cpp` | 39 |
| `core.cpp` | 31 |
| `items/item.cpp` | 18 |
| `plottable-errorbar.cpp` | 16 |
| `plottable-bars.cpp` | 15 |
| `plottable-financial.cpp` | 9 |
| `plottable.cpp` | 8 |
| `plottable-colormap.cpp` | 6 |
| `plottable-statisticalbox.cpp` | 4 |
| `plottable-curve.cpp` | 3 |
| `plottable1d.h` | 1 |

### I7. D-3's list of users is incomplete

The plan names tests, snippets, `docs/colormaps.md`, `docs/layout.md` and the generator's `genMarginGroup`. Current
grep, outside plans, specs and the changelog:

**Library (`src/`)**

- `src/layout.h`: 10 lines (`:38-65`, `:141-160`, `:183`, `:206`).
- `src/layout.cpp`: 28 lines (the class `:32-181`; `setMarginGroup` `:458-492`; the branch `:545-549`; the
  `setAutoMargins` doc `:343`).
- Doc comments: `src/alignment-group.h:16` ("wins over a QCPMarginGroup") and `src/doc-layoutsystem.dox:42`.
- `src/layoutelements/layoutelement-colorscale.cpp:84-85`. This is a `\snippet` of
  `documentation/doc-code-snippets/mainwindow.cpp`, tag `qcpcolorscale-margingroup`. The tag and the doc change
  together.

**Tests**

- `test-qcplayout.cpp:314-341` and `:405-411` (+ `.h:14-15`): port.
- `test-colorscale.cpp:207-210`: port.
- `test-alignment-group.cpp:475-494` (`alignmentWinsOverMarginGroup`, + `.h:28`): **delete**. Its subject goes,
  which contradicts D-3's "ported, not deleted" as written.
- `test-handle.cpp:539-540`, a `static_assert` on `marginGroup()`: delete.
- `tests/manual/mainwindow.cpp`: 11 lines (`:37`, `:303-305`, `:669-680`, `:787-789`) + `.h`. This is built.

**Snippets**

| File | Margin groups |
|---|---|
| `gallery.cpp` (`:96`, `:175`, `:329`, `:332`, `:370`, `:453`, `:582`) | 7 |
| `layout.cpp` (`:137`, `:168`) | 2 |
| `axes.cpp:186` | 1 |
| `colormaps.cpp:32` | 1 |

**Doc pages**

- `docs/layout.md:135`, `:146`;
- `docs/colormaps.md:177`.

**Doc tooling**

- Doc image generator `documentation/doc-image-generator/mainwindow.cpp`, six groups:
  - `genMarginGroup` `:269-319` and its `QCPMarginGroup.png`;
  - `:927-945`;
  - `:992-994`;
  - `:1255-1259`;
  - `:1334-1338`;
  - plus `.h`.
- `documentation/doc-code-snippets/mainwindow.cpp:62-64`, `:127-138` (+ `.h`). Meson does not build it.
- `documentation/postprocess-doc-images.py:46`.

**Examples**

- `examples/plots/mainwindow.cpp`: three groups, `:1179`, `:1299`, `:1394`.

**Benchmarks**: none.

**SciQLopPlots**: `src/SciQLopPlot.cpp:141-143`.

Amendment: paste this list into D-3.

### I8. D-5 misses the `Q_PROPERTY` declarations and several users

**Properties.** `Q_PROPERTY(QPen pen READ pen WRITE setPen)` and `selectedPen` are declared on:

- `src/items/item-span.h:23-24`;
- `src/items/item-rspan.h:15-16`.

moc calls the accessors, so deleting the accessors without these lines breaks the build.

**Constructor calls**

- `item-span.cpp:58-59`;
- `item-rspan.cpp:31-32`.

**Dead code**: `mainPen()` at `item-spanbase.h:49` has no caller.

**Users**

| File | Lines |
|---|---|
| `test-span.cpp` | `:265`, `:287`, `:295` |
| `test-rspan.cpp` | `:109`, `:132` |
| `test-linked-axes.cpp` | `:572` |
| `test-repaint.cpp:1182` (the `"span.setPen"` setter-table row) | delete the row |
| `docs/snippets/gallery.cpp` | `:108` |

**The fluent handle.** The generic fluent `QCPItemOps::setPen` (`src/items/item.h:450-453`) stays. It fails to
compile only when someone calls it on a span handle. Accept that, or add a sentence about it to the migration row.

### I9. Task 3 is too big for one commit, and D-6 is placed late

Task 3 bundles:

- about 236 plottable lines in 16 files;
- about 35 item-position lines;
- the span pen;
- C9 across Tasks 1-3;
- a bench.

"D-6 in every function touched by Tasks 1-3", done inside Task 3, reopens functions that Tasks 1-2 already committed.

Amendment. Split it:

- **3a**: plottable projections (B1/I5), with its C9 lines.
- **3b**: item-position projections, with its C9 lines.
- **3c**: span pen.

Each task does D-6 on its own functions. The bench runs after 3a and after 3b, since each reaches different rows.
Every split commit builds on its own: 3a touches only plottables, 3b only `item.*`, 3c only spans.

### I10. SciQLopPlots' painter curve stops compiling in 5d

`SciQLopPlots/src/SciQLopTimeColoredCurve.cpp:209-293` and `include/.../SciQLopTimeColoredCurve.hpp:87`: a `QCPCurve`
subclass that overrides `draw(QCPPainter*)`. It also reads `mKeyAxis`/`mValueAxis` at `:217`, `:222`, `:232-233` and
`:292-293`. After D-2 and D-4 it is a compile error.

`sciqlopplots-migration.md:37-40` says it "bypasses the recording". Task 4 only names `QCPMarginGroup`.

Amendment. Task 4's migration bullet adds:

> `SciQLopTimeColoredCurve` no longer compiles (`draw(QCPPainter*)` is final, the projections are private). Port it
> to `draw(QCPDrawContext&)` with `keyAxisRaw()`/`valueAxisRaw()`, or drop it in favour of the curve's per-point
> colours.

---

## Minor

### M1. Stale comments for Task 1 to fix

- `plottable.h:368-369` and `plottable.cpp:611-613` say "the polar plottables" paint through the painter virtual.
  `QCPPolarGraph` records (`src/polar/polargraph.h:130`, `:134`).
- `drawable.h:3-5` and `:187-189` describe a bridge that goes away.
- `plottable1d.h:509-510` describes the fallback.

### M2. `recordsTheDrawing` certainly goes

Its only caller is `plottable1d.h:518`. No test uses it. Replace "goes if nothing else uses it" with "goes".

After the fallback goes, `QCPAbstractPlottable1D::selectTest` (`plottable1d.h:85`, `:515`) only forwards. Delete the
override.

`QCPBars` has no `selectTest` of its own, so it already answers through the recording. Review Focus 4 can name bars
explicitly.

### M3. Name the whole internal painter list

The Architecture paragraph lists the internal painter layerables. It omits:

- `QCPItemCreationState::draw` (`src/item-creation-state.h:31`);
- `QCPPlottableGroupLegendItem` (`plottable-group.h:91`);
- `QCPColorScaleAxisRectPrivate`;
- `QCPArea`'s background.

Name them, so that a reviewer does not flag them as missed painter paths.

### M4. Grep traps for implementers

These identifiers have the same names but are other classes' private members. They are out of scope:

- `mKeyAxis`/`mValueAxis` of `QCPItemCreationState` (`item-creation-state.h:37-38`);
- `mKeyAxis`/`mValueAxis` of `QCPDrawContext` (`draw-context.h:263-264`);
- `mKeyAxis`/`mValueAxis` of `QCPCoordinateSystem` (`coordinate-system.h:131-135`, read at
  `layoutelements/area.cpp:106`);
- `mAxisRect` of `QCPColorScale` (41 lines in `layoutelement-colorscale.cpp`);
- `mAxisRect` of the ratio transform (`coords.h:349`).

Put one line about this in Task 3.

### M5. Review Focus lines

- **#3 is weak.** The tests have exactly one member reader (`test-repaint.cpp:169`), and Task 1 rewrites it. Replace
  it with: "axis state the system does not hold survives: one-axis positions (4i R-2, `axistag.cpp`) and half-set
  plottable pairs (`test-errorbar-geometry:312`)".
- **#2**: add "the mixed-side layouts: gallery joint density, `QCPColorScale.png`, `examples/plots` advanced axes".
- **#1**: add "the branches with no reference picture yet (OHLC, single colour, horizontal error bars)".
- **#4**: low risk, since no in-tree 1D plottable relies on the fallback. Narrow it to bars, curve and polar graph,
  with an empty or pending recording.
- **#5** is right.

### M6. Alignment groups may need a second layout pass

A margin group settles within one layout pass. An alignment group compares window edges and relays out through
`edgeMoved` (`alignment-group.cpp:147-165`). `test-colorscale.cpp:217-218` replots twice before it checks.

The doc generator and the snippets render once (`savePng`). Task 2 should check each ported picture after a single
render, not after a settle.

### M7. Out-of-tree port target

D-4 tells out-of-tree subclasses to port to `keyAxis()`/`system()`. `keyAxis()` returns a handle (5b). For subclass
drawing code, the protected `keyAxisRaw()`/`valueAxisRaw()` (`plottable.h:330-331`) are the cheap port. Say both in
the migration row.

`docs/writing-a-plot-type.md` (the out-of-tree guide) should get the same sentence in Task 4.
