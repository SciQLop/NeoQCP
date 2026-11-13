# Milestone 5e inventory: the meta-object surface

State of branch `neoqcp-3.0` at `87b296b` (the working tree also carried uncommitted edits by another agent in
`async-pipeline.h`, `plottable-colormap2.{h,cpp}` and `test-colormap`; none touches a macro, a property or a setter).
Read-only except this file. No build, no test, no `meson setup`.

Sources: `docs/specs/2026-11-11-neoqcp-3-milestone-5-design.md` (C11, **A14 is the binding rule set**),
`.superpowers/m5-bindable-review.md`, `docs/plans/neoqcp-3/m5b-inventory.md` (format).

**Method.** The property, signal, slot and enum tables come from `moc --output-json` run over the 69 headers of `src/`
that contain `Q_OBJECT`, `Q_GADGET` or `Q_NAMESPACE` (Qt moc 6.11 from `/usr/lib64/qt6/libexec/moc`; the moc
JSON is the parse, not grep). The Coder tree `~/ci-fix` was **not** used: its `.git` is broken and its sources are not at
this commit, so the local moc over the exact worktree headers is the fresher source. Setter bodies, getter
existence, paint-path hits and the macro lists are from a script over `src/` read next to the moc output; the
scripts are not committed. Every `READ`/`WRITE` name in the 267 declarations resolves to a declared accessor and a
definition (moc output compiled, so it cannot be otherwise): **no property names a removed or renamed accessor.**
The four `Q_PROPERTY` hits that are not declarations are comments (`axis/axis.h:37`, `layer.cpp:112`, `layer.h:40`,
`layer.h:187`).

Reading guide for the columns. *decl*: R read, W write, N notify, C constant, F final, as declared today.
*form* is the A14 form, proposed: **constant** (`CONSTANT`, no `NOTIFY`), **plain** (setter plus `NOTIFY`, bindable
externally through `QBindable(obj, "name")`), **storage** (`BINDABLE` storage candidate: the setter is store, mark,
emit and nothing else), **derived** (read-only, `NOTIFY` on a real event), **drop** (off the surface), **gadget**
(`Q_GADGET`, property without `NOTIFY`), **out** (pipeline state). *hot*: **Y** = a paint/layout function (`draw*`,
`update`, `*SizeHint`, `calculateAutoMargin`, `getTick*`, `selectTest`, and so on) of the class or a relative reads
the member or getter (static scan), **p** = probable (a pen, brush, font or colour of a drawable class: the draw
path reads it through a getter). Every storage candidate with Y or p needs the 5 % bench A/B before the form is
chosen. The scan is indicative: it can miss a read behind a helper and cannot see the async worker.

## 0. Totals

| What | Count |
|---|---|
| Headers with `Q_OBJECT`/`Q_GADGET`/`Q_NAMESPACE` | 69 (68 distinct files hold classes; `plottable1d.h` only mentions it in a comment) |
| Meta-object classes (moc) | 82: 68 `Q_OBJECT`, 13 `Q_GADGET`, 1 `Q_NAMESPACE` (`QCP`) |
| `Q_PROPERTY` declarations | 267 (design said 274 mentions; 7 fewer since, 4 hits are comments) |
| ... in classes | 39 classes carry properties; 0 `Q_GADGET` carries any |
| ... with `NOTIFY` | 37 (16 of them `QCPTheme`, all on the one argument-less `changed()`) |
| ... `CONSTANT` / `FINAL` / `BINDABLE` / `RESET` | 0 / 0 / 0 / 0 |
| ... read-only (no `WRITE`) | 21 |
| ... writable without any `NOTIFY` | 209 |
| ... object- or shared-pointer typed (A6) | 21 |
| Signals / slots (moc) | 73 / 51 (slots are already invokable) |
| `Q_INVOKABLE` today | 0 |
| Enums registered with moc (`Q_ENUM`/`Q_FLAG`/`_NS`) | 69 entries (enums plus their flag types) |
| Public enums still missing `Q_ENUM` | 9 in meta-object classes (+ `EditModes` needing `Q_FLAG`), 3 public ones outside any meta-object scope, 9 protected (section 6.1) |
| `Q_OBJECT` classes with `Q_DISABLE_COPY` / without any | 23 / 45 (`Q_DISABLE_COPY_MOVE`: 0) |
| `Q_DECLARE_TYPEINFO` today | 11 (5 movable, 6 primitive) |
| `Q_DECLARE_METATYPE` today | 37 (33 are for enums) |

Proposed forms for the 267 existing properties:

| Form | Count | Note |
|---|---|---|
| storage | 143 | storage candidates (143 is the ceiling, not the plan: see open questions 1 and 2) |
| plain | 96 | plain `NOTIFY` |
| derived | 11 | read-only with a `NOTIFY` still to be added |
| drop | 9 | `tickVector`, `tickVectorLabels` and the 7 `QCustomPlot::theme*` forwarders |
| constant | 6 | `CONSTANT` |
| plain (split setter) | 2 | needs its own setter first |

Setter/getter pairs **without** a `Q_PROPERTY` (section 3): **211** one-argument pairs (storage 83, plain 68, gadget 58, out 2); plus 101 setters that are not a plain pair (no getter, several arguments, template or `std::` arguments), listed by class in 3.2.

## 1. Meta-object classes

`props` = `Q_PROPERTY` today, `N` = of them with `NOTIFY`. `sig`/`slot` = declared in the class itself. `enum` = enums
registered with moc in the class.

| Class | File | Kind | props | N | sig | slot | enum |
|---|---|---|---|---|---|---|---|
| `QCPAlignmentGroup` | `alignment-group.h` | Q_OBJECT | 0 | 0 | 1 | 0 | 0 |
| `QCPAxisBase` | `axis/axis-base.h` | Q_OBJECT | 8 | 4 | 4 | 4 | 3 |
| `QCPGrid` | `axis/axis.h` | Q_OBJECT | 6 | 0 | 0 | 0 | 0 |
| `QCPAxis` | `axis/axis.h` | Q_OBJECT | 35 | 0 | 0 | 0 | 3 |
| `QCPAxisTicker` | `axis/axisticker.h` | Q_GADGET | 0 | 0 | 0 | 0 | 1 |
| `QCPAxisTickerFixed` | `axis/axistickerfixed.h` | Q_GADGET | 0 | 0 | 0 | 0 | 1 |
| `QCPAxisTickerPi` | `axis/axistickerpi.h` | Q_GADGET | 0 | 0 | 0 | 0 | 1 |
| `QCPAxisTickerTime` | `axis/axistickertime.h` | Q_GADGET | 0 | 0 | 0 | 0 | 1 |
| `QCPLabelPainterPrivate` | `axis/labelpainter.h` | Q_GADGET | 0 | 0 | 0 | 0 | 3 |
| `QCPColorGradient` | `colorgradient.h` | Q_GADGET | 0 | 0 | 0 | 0 | 3 |
| `QCPPolarSystem` | `coordinate-system-polar.h` | Q_OBJECT | 0 | 0 | 0 | 0 | 0 |
| `QCPCoordinateSystem` | `coordinate-system.h` | Q_OBJECT | 0 | 0 | 1 | 0 | 0 |
| `QCPCartesianSystem` | `coordinate-system.h` | Q_OBJECT | 0 | 0 | 0 | 0 | 0 |
| `QCustomPlot` | `core.h` | Q_OBJECT | 16 | 0 | 19 | 8 | 2 |
| `QCPAsyncPipelineBase` | `datasource/async-pipeline.h` | Q_OBJECT | 0 | 0 | 2 | 0 | 0 |
| `QCPPipelineScheduler` | `datasource/pipeline-scheduler.h` | Q_OBJECT | 0 | 0 | 0 | 0 | 0 |
| `QCP` | `global.h` | Q_NAMESPACE | 0 | 0 | 0 | 0 | 16 |
| `QCPItemCreationState` | `item-creation-state.h` | Q_OBJECT | 0 | 0 | 2 | 0 | 0 |
| `QCPItemBracket` | `items/item-bracket.h` | Q_OBJECT | 4 | 0 | 0 | 0 | 1 |
| `QCPItemCurve` | `items/item-curve.h` | Q_OBJECT | 4 | 0 | 0 | 0 | 0 |
| `QCPItemEllipse` | `items/item-ellipse.h` | Q_OBJECT | 4 | 0 | 0 | 0 | 0 |
| `QCPItemLine` | `items/item-line.h` | Q_OBJECT | 4 | 0 | 0 | 0 | 0 |
| `QCPItemPixmap` | `items/item-pixmap.h` | Q_OBJECT | 6 | 0 | 0 | 0 | 0 |
| `QCPItemRect` | `items/item-rect.h` | Q_OBJECT | 4 | 0 | 0 | 0 | 0 |
| `QCPItemRichText` | `items/item-richtext.h` | Q_OBJECT | 1 | 0 | 0 | 0 | 0 |
| `QCPItemRSpan` | `items/item-rspan.h` | Q_OBJECT | 5 | 0 | 2 | 0 | 0 |
| `QCPItemSpan` | `items/item-span.h` | Q_OBJECT | 5 | 0 | 1 | 0 | 0 |
| `QCPAbstractSpanItem` | `items/item-spanbase.h` | Q_OBJECT | 0 | 0 | 0 | 0 | 0 |
| `QCPItemStraightLine` | `items/item-straightline.h` | Q_OBJECT | 2 | 0 | 0 | 0 | 0 |
| `QCPItemText` | `items/item-text.h` | Q_OBJECT | 13 | 0 | 0 | 0 | 0 |
| `QCPItemTracer` | `items/item-tracer.h` | Q_OBJECT | 9 | 0 | 0 | 0 | 1 |
| `QCPItemAnchor` | `items/item.h` | Q_GADGET | 0 | 0 | 0 | 0 | 0 |
| `QCPItemPosition` | `items/item.h` | Q_GADGET | 0 | 0 | 0 | 0 | 1 |
| `QCPAbstractItem` | `items/item.h` | Q_OBJECT | 4 | 2 | 3 | 2 | 0 |
| `QCPLayer` | `layer.h` | Q_OBJECT | 6 | 0 | 0 | 0 | 1 |
| `QCPLayerable` | `layer.h` | Q_OBJECT | 6 | 1 | 2 | 1 | 0 |
| `QCPLayoutElement` | `layout.h` | Q_OBJECT | 8 | 0 | 0 | 0 | 2 |
| `QCPLayout` | `layout.h` | Q_OBJECT | 0 | 0 | 0 | 0 | 0 |
| `QCPLayoutGrid` | `layout.h` | Q_OBJECT | 8 | 0 | 0 | 0 | 1 |
| `QCPLayoutInset` | `layout.h` | Q_OBJECT | 0 | 0 | 0 | 0 | 1 |
| `QCPArea` | `layoutelements/area.h` | Q_OBJECT | 3 | 0 | 0 | 0 | 0 |
| `QCPAxisRect` | `layoutelements/layoutelement-axisrect.h` | Q_OBJECT | 2 | 0 | 0 | 0 | 0 |
| `QCPColorScaleAxisRectPrivate` | `layoutelements/layoutelement-colorscale.h` | Q_OBJECT | 0 | 0 | 0 | 2 | 0 |
| `QCPColorScale` | `layoutelements/layoutelement-colorscale.h` | Q_OBJECT | 8 | 3 | 3 | 3 | 0 |
| `QCPGroupLegendItem` | `layoutelements/layoutelement-legend-group.h` | Q_OBJECT | 0 | 0 | 1 | 0 | 0 |
| `QCPIntervalsLegendItem` | `layoutelements/layoutelement-legend-intervals.h` | Q_OBJECT | 0 | 0 | 0 | 0 | 0 |
| `QCPAbstractLegendItem` | `layoutelements/layoutelement-legend.h` | Q_OBJECT | 7 | 2 | 2 | 2 | 0 |
| `QCPPlottableLegendItem` | `layoutelements/layoutelement-legend.h` | Q_OBJECT | 0 | 0 | 0 | 0 | 0 |
| `QCPLegend` | `layoutelements/layoutelement-legend.h` | Q_OBJECT | 14 | 2 | 2 | 2 | 2 |
| `QCPTextElement` | `layoutelements/layoutelement-textelement.h` | Q_OBJECT | 7 | 2 | 4 | 2 | 0 |
| `QCPLineEnding` | `lineending.h` | Q_GADGET | 0 | 0 | 0 | 0 | 1 |
| `QCPOverlay` | `overlay.h` | Q_OBJECT | 0 | 0 | 2 | 0 | 3 |
| `QCPPainter` | `painting/painter.h` | Q_GADGET | 0 | 0 | 0 | 0 | 2 |
| `QCPLaneLayout` | `plottables/lane-layout.h` | Q_OBJECT | 0 | 0 | 1 | 0 | 1 |
| `QCPBarsGroup` | `plottables/plottable-bars.h` | Q_OBJECT | 2 | 0 | 0 | 0 | 1 |
| `QCPBars` | `plottables/plottable-bars.h` | Q_OBJECT | 7 | 0 | 0 | 0 | 1 |
| `QCPColorMap` | `plottables/plottable-colormap.h` | Q_OBJECT | 2 | 0 | 0 | 3 | 0 |
| `QCPColorMap2` | `plottables/plottable-colormap2.h` | Q_OBJECT | 0 | 0 | 0 | 7 | 0 |
| `QCPAbstractColorMapped` | `plottables/plottable-colormapped.h` | Q_OBJECT | 4 | 3 | 4 | 3 | 0 |
| `QCPCurve` | `plottables/plottable-curve.h` | Q_OBJECT | 3 | 0 | 0 | 0 | 0 |
| `QCPErrorBars` | `plottables/plottable-errorbar.h` | Q_OBJECT | 5 | 0 | 0 | 0 | 1 |
| `QCPFinancial` | `plottables/plottable-financial.h` | Q_OBJECT | 8 | 0 | 0 | 0 | 2 |
| `QCPGraph` | `plottables/plottable-graph.h` | Q_OBJECT | 1 | 0 | 0 | 0 | 0 |
| `QCPPlottableGroup` | `plottables/plottable-group.h` | Q_OBJECT | 0 | 0 | 2 | 0 | 0 |
| `QCPPlottableGroupLegendItem` | `plottables/plottable-group.h` | Q_OBJECT | 0 | 0 | 0 | 0 | 0 |
| `QCPHistogram` | `plottables/plottable-histogram.h` | Q_OBJECT | 0 | 0 | 1 | 1 | 1 |
| `QCPHistogram2D` | `plottables/plottable-histogram2d.h` | Q_OBJECT | 0 | 0 | 1 | 8 | 0 |
| `QCPIntervals` | `plottables/plottable-intervals.h` | Q_OBJECT | 0 | 0 | 3 | 0 | 0 |
| `QCPAbstractLine` | `plottables/plottable-line.h` | Q_OBJECT | 3 | 0 | 1 | 0 | 1 |
| `QCPStatisticalBox` | `plottables/plottable-statisticalbox.h` | Q_OBJECT | 7 | 0 | 0 | 0 | 0 |
| `QCPWaterfallGraph` | `plottables/plottable-waterfall.h` | Q_OBJECT | 0 | 0 | 0 | 0 | 1 |
| `QCPSelectionDecorator` | `plottables/plottable.h` | Q_GADGET | 0 | 0 | 0 | 0 | 0 |
| `QCPAbstractPlottable` | `plottables/plottable.h` | Q_OBJECT | 10 | 2 | 4 | 2 | 0 |
| `QCPPolarAxisAngular` | `polar/angularaxis.h` | Q_OBJECT | 0 | 0 | 0 | 0 | 1 |
| `QCPPolarArea` | `polar/polar-area.h` | Q_OBJECT | 0 | 0 | 0 | 0 | 0 |
| `QCPPolarGraph` | `polar/polargraph.h` | Q_OBJECT | 0 | 0 | 0 | 0 | 1 |
| `QCPPolarGrid` | `polar/polargrid.h` | Q_OBJECT | 0 | 0 | 0 | 0 | 2 |
| `QCPPolarAxisRadial` | `polar/radialaxis.h` | Q_OBJECT | 0 | 0 | 0 | 0 | 2 |
| `QCPScatterStyle` | `scatterstyle.h` | Q_GADGET | 0 | 0 | 0 | 0 | 3 |
| `QCPSelectionDecoratorBracket` | `selectiondecorator-bracket.h` | Q_GADGET | 0 | 0 | 0 | 0 | 1 |
| `QCPSelectionRect` | `selectionrect.h` | Q_OBJECT | 0 | 0 | 4 | 1 | 0 |
| `QCPTheme` | `theme.h` | Q_OBJECT | 16 | 16 | 1 | 0 | 0 |

Non-meta classes of interest (no `Q_OBJECT`/`Q_GADGET`): `QCPRange`, `QCPTransform` (holds a `std::variant` of
cartesian/polar/custom frames), `QCPRatioTransform`, `QCPDataSelection`, `QCPDataRange`, `QCPDataBox`, `QCPHit`,
`QCPIntervalEdit`, `QCPKeyWindow`, `QCPAxisLink`, the fluent `*Ops` mixins (`QCPItemOps`, `QCPAxisRectOps`, ...: their
`apply`-based setters return `T&` and duplicate the `void` setters moc resolves), and `QCPHandle<T>`.

## 2. Every existing `Q_PROPERTY` (267), with its proposed form

Setter bodies were read. Where a setter is overloaded or forwarded (fluent `apply`), the row judges the one-argument
overload that moc binds. `async` marks a property the async pipeline reads or re-bakes on the GUI thread: A14 keeps
pipeline *state* out of the surface; these are user settings that feed it, listed so the plan decides.

### `QCPAxisBase` (`axis/axis-base.h`, Q_OBJECT, 8 props, 4 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `scaleType` | `ScaleType` | RWN | plain | refuses via virtual scaleTypeWillChange, sanitises the range, refreshes linked axes (A14 spike); async: pipeline input (ViewportParams, log flag); hot mapping members at fixed offsets (`axis-base.h:195-211`) | - | `scaleTypeChanged` |
| `range` | `QCPRange` | RWN | plain | commitRange sanitises a log range and writes a linked source axis; async: pipeline input (ViewportParams::fromAxes, GUI thread); hot mapping members at fixed offsets (`axis-base.h:195-211`) | - | `rangeChanged` |
| `rangeReversed` | `bool` | RW | plain | refuses on a linked axis (QCP_CHECK_OR_RETURN), emits through a scope; async: pipeline input (ViewportParams); hot mapping members at fixed offsets (`axis-base.h:195-211`) | Y | none |
| `ticker` | `QSharedPointer<QCPAxisTicker>` | RW | plain | refuses a null ticker; shared-pointer value, not a copyable value type; A6 | - | none |
| `tickVector` | `QList<double>` | R | drop | derived from range, ticker and layout; no change signal; A14 names it | Y | none |
| `tickVectorLabels` | `QList<QString>` | R | drop | derived from range, ticker and layout; no change signal; A14 names it | Y | none |
| `selectedParts` | `SelectableParts` | RWN | plain | selection state: setter marks and emits qcpEmit | Y | `selectionChanged` |
| `selectableParts` | `SelectableParts` | RWN | plain | selection state: setter marks and emits qcpEmit | Y | `selectableChanged` |

### `QCPGrid` (`axis/axis.h`, Q_OBJECT, 6 props, 0 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `subGridVisible` | `bool` | RW | plain | also marks the grid RHI layer geometry dirty through the plot | Y | none |
| `antialiasedSubGrid` | `bool` | RW | storage | trivial store + markDirty | - | none |
| `antialiasedZeroLine` | `bool` | RW | storage | trivial store + markDirty | - | none |
| `pen` | `QPen` | RW | plain | also marks the grid RHI layer geometry dirty through the plot | p | none |
| `subGridPen` | `QPen` | RW | plain | also marks the grid RHI layer geometry dirty through the plot | p | none |
| `zeroLinePen` | `QPen` | RW | plain | also marks the grid RHI layer geometry dirty through the plot | p | none |

### `QCPAxis` (`axis/axis.h`, Q_OBJECT, 35 props, 0 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `axisType` | `AxisType` | R | constant | fixed at construction, no setter | Y | none |
| `axisRect` | `QCPAxisRect*` | R | constant | an axis never changes rect; leaving the rect deletes it (verify); A6 | Y | none |
| `ticks` | `bool` | RW | storage (bench) | trivial: store + mCachedMarginValid = false, markAppearanceDirty | Y | none |
| `tickLabels` | `bool` | RW | plain | clears mTickVectorLabels as well | Y | none |
| `tickLabelPadding` | `int` | RW | plain | value lives in the axis painter (not an own member) | - | none |
| `tickLabelFont` | `QFont` | RW | storage (bench) | trivial: store + mCachedMarginValid = false, markAppearanceDirty | Y | none |
| `tickLabelColor` | `QColor` | RW | storage (bench) | trivial: store + markAppearanceDirty | Y | none |
| `tickLabelRotation` | `double` | RW | plain | clamps to [-90, 90]; value lives in the axis painter | - | none |
| `tickLabelSide` | `LabelSide` | RW | plain | value lives in the axis painter (not an own member) | - | none |
| `numberFormat` | `QString` | RW | plain | parses and validates the format code (refuses) | - | none |
| `numberPrecision` | `int` | RW | storage | trivial: store + mCachedMarginValid = false, markAppearanceDirty | - | none |
| `tickLengthIn` | `int` | RW | plain | value lives in the axis painter (not an own member) | - | none |
| `tickLengthOut` | `int` | RW | plain | value lives in the axis painter (not an own member) | - | none |
| `subTicks` | `bool` | RW | storage (bench) | trivial: store + mCachedMarginValid = false, markAppearanceDirty | Y | none |
| `subTickLengthIn` | `int` | RW | plain | value lives in the axis painter (not an own member) | - | none |
| `subTickLengthOut` | `int` | RW | plain | value lives in the axis painter (not an own member) | - | none |
| `basePen` | `QPen` | RW | storage (bench) | trivial: store + markAppearanceDirty | p | none |
| `tickPen` | `QPen` | RW | storage (bench) | trivial: store + markAppearanceDirty | Y | none |
| `subTickPen` | `QPen` | RW | storage (bench) | trivial: store + markAppearanceDirty | p | none |
| `labelFont` | `QFont` | RW | storage (bench) | trivial: store + mCachedMarginValid = false, markAppearanceDirty | Y | none |
| `labelColor` | `QColor` | RW | storage (bench) | trivial: store + markAppearanceDirty | Y | none |
| `label` | `QString` | RW | storage (bench) | trivial: store + mCachedMarginValid = false, markAppearanceDirty | Y | none |
| `labelPadding` | `int` | RW | plain | value lives in the axis painter (not an own member) | - | none |
| `padding` | `int` | RW | storage | trivial: store + mCachedMarginValid = false, markAppearanceDirty | - | none |
| `offset` | `int` | RW | plain | value lives in the axis painter (not an own member) | - | none |
| `selectedTickLabelFont` | `QFont` | RW | storage (bench) | trivial: store + markAppearanceDirty | Y | none |
| `selectedLabelFont` | `QFont` | RW | storage (bench) | trivial: store + markAppearanceDirty | Y | none |
| `selectedTickLabelColor` | `QColor` | RW | storage (bench) | trivial: store + markAppearanceDirty | Y | none |
| `selectedLabelColor` | `QColor` | RW | storage (bench) | trivial: store + markAppearanceDirty | Y | none |
| `selectedBasePen` | `QPen` | RW | storage (bench) | trivial: store + markAppearanceDirty | p | none |
| `selectedTickPen` | `QPen` | RW | storage (bench) | trivial: store + markAppearanceDirty | Y | none |
| `selectedSubTickPen` | `QPen` | RW | storage (bench) | trivial: store + markAppearanceDirty | p | none |
| `lowerEnding` | `QCPLineEnding` | RW | plain | compares by components, value lives in the axis painter | - | none |
| `upperEnding` | `QCPLineEnding` | RW | plain | compares by components, value lives in the axis painter | - | none |
| `grid` | `QCPGrid*` | R | constant | created with the axis, owned by it; A6 | - | none |

### `QCustomPlot` (`core.h`, Q_OBJECT, 16 props, 0 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `viewport` | `QRect` | RW | plain | bumps mViewEpoch and relays out the root layout; layout path | - | none |
| `background` | `QPixmap` | RW | plain | also resets the scaled-pixmap cache; 3 overloads share the name | - | none |
| `backgroundScaled` | `bool` | RW | storage | trivial store; no markDirty even (replot-driven) | - | none |
| `backgroundScaledMode` | `Qt::AspectRatioMode` | RW | storage | trivial store; no markDirty even (replot-driven) | - | none |
| `plotLayout` | `QCPLayoutGrid*` | R | constant | the root layout; no setter in 3.0; A6 | - | none |
| `autoAddPlottableToLegend` | `bool` | RW | storage | plain store, nothing else, no signal at all | - | none |
| `selectionTolerance` | `int` | RW | storage | plain store, nothing else, no signal at all | - | none |
| `noAntialiasingOnDrag` | `bool` | RW | storage | plain store, nothing else, no signal at all | - | none |
| `multiSelectModifier` | `Qt::KeyboardModifier` | RW | storage | plain store, nothing else, no signal at all | - | none |
| `themeBackground` | `QColor` | RW | drop | forwarders into QCPTheme (which already carries each as a property); duplicate surface | - | none |
| `themeForeground` | `QColor` | RW | drop | forwarders into QCPTheme (which already carries each as a property); duplicate surface | - | none |
| `themeGrid` | `QColor` | RW | drop | forwarders into QCPTheme (which already carries each as a property); duplicate surface | - | none |
| `themeSubGrid` | `QColor` | RW | drop | forwarders into QCPTheme (which already carries each as a property); duplicate surface | - | none |
| `themeSelection` | `QColor` | RW | drop | forwarders into QCPTheme (which already carries each as a property); duplicate surface | - | none |
| `themeLegendBackground` | `QColor` | RW | drop | forwarders into QCPTheme (which already carries each as a property); duplicate surface | - | none |
| `themeLegendBorder` | `QColor` | RW | drop | forwarders into QCPTheme (which already carries each as a property); duplicate surface | - | none |

### `QCPItemBracket` (`items/item-bracket.h`, Q_OBJECT, 4 props, 0 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `pen` | `QPen` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `selectedPen` | `QPen` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `length` | `double` | RW | storage (bench) | trivial: store + markDirty | Y | none |
| `style` | `BracketStyle` | RW | storage (bench) | trivial: store + markDirty | Y | none |

### `QCPItemCurve` (`items/item-curve.h`, Q_OBJECT, 4 props, 0 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `pen` | `QPen` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `selectedPen` | `QPen` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `head` | `QCPLineEnding` | RW | storage (bench) | trivial: store + markDirty | Y | none |
| `tail` | `QCPLineEnding` | RW | storage (bench) | trivial: store + markDirty | Y | none |

### `QCPItemEllipse` (`items/item-ellipse.h`, Q_OBJECT, 4 props, 0 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `pen` | `QPen` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `selectedPen` | `QPen` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `brush` | `QBrush` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `selectedBrush` | `QBrush` | RW | storage (bench) | trivial: store + markDirty | p | none |

### `QCPItemLine` (`items/item-line.h`, Q_OBJECT, 4 props, 0 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `pen` | `QPen` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `selectedPen` | `QPen` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `head` | `QCPLineEnding` | RW | storage (bench) | trivial: store + markDirty | Y | none |
| `tail` | `QCPLineEnding` | RW | storage (bench) | trivial: store + markDirty | Y | none |

### `QCPItemPixmap` (`items/item-pixmap.h`, Q_OBJECT, 6 props, 0 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `pixmap` | `QPixmap` | RW | plain | invalidates the scaled-pixmap cache; qDebug on a null pixmap | - | none |
| `scaled` | `bool` | RW | plain | invalidates the scaled-pixmap cache; qDebug on a null pixmap | - | none |
| `aspectRatioMode` | `Qt::AspectRatioMode` | R | plain (split setter) | only set together with scaled by setScaled(scaled, mode, mode); a property needs its own setter | - | none |
| `transformationMode` | `Qt::TransformationMode` | R | plain (split setter) | only set together with scaled by setScaled(scaled, mode, mode); a property needs its own setter | - | none |
| `pen` | `QPen` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `selectedPen` | `QPen` | RW | storage (bench) | trivial: store + markDirty | p | none |

### `QCPItemRect` (`items/item-rect.h`, Q_OBJECT, 4 props, 0 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `pen` | `QPen` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `selectedPen` | `QPen` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `brush` | `QBrush` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `selectedBrush` | `QBrush` | RW | storage (bench) | trivial: store + markDirty | p | none |

### `QCPItemRichText` (`items/item-richtext.h`, Q_OBJECT, 1 props, 0 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `html` | `QString` | RW | plain | does more than store (mDoc.setDefaultFont(textStyle().font)) | - | none |

### `QCPItemRSpan` (`items/item-rspan.h`, Q_OBJECT, 5 props, 0 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `brush` | `QBrush` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `selectedBrush` | `QBrush` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `borderPen` | `QPen` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `selectedBorderPen` | `QPen` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `movable` | `bool` | RW | storage | trivial store; no markDirty, no signal | - | none |

### `QCPItemSpan` (`items/item-span.h`, Q_OBJECT, 5 props, 0 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `brush` | `QBrush` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `selectedBrush` | `QBrush` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `borderPen` | `QPen` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `selectedBorderPen` | `QPen` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `movable` | `bool` | RW | storage | trivial store; no markDirty, no signal | - | none |

### `QCPItemStraightLine` (`items/item-straightline.h`, Q_OBJECT, 2 props, 0 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `pen` | `QPen` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `selectedPen` | `QPen` | RW | storage (bench) | trivial: store + markDirty | p | none |

### `QCPItemText` (`items/item-text.h`, Q_OBJECT, 13 props, 0 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `color` | `QColor` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `selectedColor` | `QColor` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `pen` | `QPen` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `selectedPen` | `QPen` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `brush` | `QBrush` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `selectedBrush` | `QBrush` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `font` | `QFont` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `selectedFont` | `QFont` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `text` | `QString` | RW | storage (bench) | trivial: store + markDirty | Y | none |
| `positionAlignment` | `Qt::Alignment` | RW | storage | trivial: store + markDirty | - | none |
| `textAlignment` | `Qt::Alignment` | RW | storage | trivial: store + markDirty | - | none |
| `rotation` | `double` | RW | storage | trivial: store + markDirty | - | none |
| `padding` | `QMargins` | RW | storage | trivial: store + markDirty | - | none |

### `QCPItemTracer` (`items/item-tracer.h`, Q_OBJECT, 9 props, 0 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `pen` | `QPen` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `selectedPen` | `QPen` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `brush` | `QBrush` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `selectedBrush` | `QBrush` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `size` | `double` | RW | storage (bench) | trivial: store + markDirty | Y | none |
| `style` | `TracerStyle` | RW | storage (bench) | trivial: store + markDirty | Y | none |
| `graph` | `QCPGraph*` | RW | plain | binds to a plottable; validates; pointer ownership (A6); A6 | - | none |
| `graphKey` | `double` | RW | plain | binds to a plottable; validates; pointer ownership (A6) | - | none |
| `interpolating` | `bool` | RW | plain | binds to a plottable; validates; pointer ownership (A6) | - | none |

### `QCPAbstractItem` (`items/item.h`, Q_OBJECT, 4 props, 2 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `clipToAxisRect` | `bool` | RW | storage (bench) | trivial: store + markDirty | Y | none |
| `clipAxisRect` | `QCPAxisRect*` | RW | plain | forwards to setClipArea (area bookkeeping); A6 | - | none |
| `selectable` | `QCP::SelectionType` | RWN | plain | validates stNone/stWhole and trims the selection | - | `selectableChanged` |
| `selected` | `bool` | RWN | plain | selection state; marks dirty and emits selectionChanged | - | `selectionChanged` |

### `QCPLayer` (`layer.h`, Q_OBJECT, 6 props, 0 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `parentPlot` | `QCustomPlot*` | R | constant | set at creation; A6 | Y | none |
| `name` | `QString` | R | constant | set at creation | - | none |
| `index` | `int` | R | derived | moves with moveLayer/removeLayer; no change signal | - | none |
| `children` | `QList<QCPLayerable*>` | R | derived | the layer order of its layerables; no change signal | Y | none |
| `visible` | `bool` | RW | plain | invalidates the paint buffer | p | none |
| `mode` | `LayerMode` | RW | plain | invalidates the paint buffer | - | none |

### `QCPLayerable` (`layer.h`, Q_OBJECT, 6 props, 1 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `visible` | `bool` | RW | plain | invalidates the layer buffer and emits visibleChanged (the property has no NOTIFY yet); paint path | Y | `visibleChanged` exists, not wired |
| `parentPlot` | `QCustomPlot*` | R | derived | changes on QCustomPlot::take/add; no change signal; A6 | Y | none |
| `parentLayerable` | `QCPLayerable*` | R | derived | follows the layout/legend the layerable sits in; no change signal; A6 | - | none |
| `layer` | `QCPLayer*` | RWN | plain | moveToLayer: re-parents between layers, repaints both; A6 | - | `layerChanged` |
| `antialiased` | `bool` | RW | storage (bench) | trivial store + markDirty; read in applyDefaultAntialiasingHint (paint path: bench) | Y | none |
| `mouseTransparent` | `bool` | RW | storage | trivial store, nothing else; not on the paint path | - | none |

### `QCPLayoutElement` (`layout.h`, Q_OBJECT, 8 props, 0 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `layout` | `QCPLayout*` | R | derived | follows the layout the element sits in; no change signal; A6 | - | none |
| `rect` | `QRect` | R | derived | outerRect minus margins; no change signal; paint path | Y | none |
| `outerRect` | `QRect` | RW | plain | also updates mRect from the margins; written by the layout pass (paint/layout path) | Y | none |
| `margins` | `QMargins` | RW | plain | updates mRect and dirties the layout; layout path | Y | none |
| `minimumMargins` | `QMargins` | RW | plain | dirties the layout; layout path | Y | none |
| `minimumSize` | `QSize` | RW | plain | notifies the parent layout (sizeConstraintsChanged) | - | none |
| `maximumSize` | `QSize` | RW | plain | notifies the parent layout (sizeConstraintsChanged) | - | none |
| `sizeConstraintRect` | `SizeConstraintRect` | RW | plain | notifies the parent layout (sizeConstraintsChanged) | - | none |

### `QCPLayoutGrid` (`layout.h`, Q_OBJECT, 8 props, 0 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `rowCount` | `int` | R | derived | follows insertRow/expandTo/addElement; no change signal | Y | none |
| `columnCount` | `int` | R | derived | follows insertRow/expandTo/addElement; no change signal | Y | none |
| `columnStretchFactors` | `QList<double>` | RW | plain | validates the list size (refuses a mismatch), sanitises entries | - | none |
| `rowStretchFactors` | `QList<double>` | RW | plain | validates the list size (refuses a mismatch), sanitises entries | - | none |
| `columnSpacing` | `int` | RW | storage (bench) | trivial store + markLayoutAppearanceDirty; layout path (bench) | Y | none |
| `rowSpacing` | `int` | RW | storage (bench) | trivial store + markLayoutAppearanceDirty; layout path (bench) | Y | none |
| `fillOrder` | `FillOrder` | RW | plain | rearranges the elements | - | none |
| `wrap` | `int` | RW | plain | normalises (max 0) | - | none |

### `QCPArea` (`layoutelements/area.h`, Q_OBJECT, 3 props, 0 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `background` | `QPixmap` | RW | plain | compares cacheKey, resets the scaled cache; 3 overloads share the name | - | none |
| `backgroundScaled` | `bool` | RW | storage | trivial store + markDirty | - | none |
| `backgroundScaledMode` | `Qt::AspectRatioMode` | RW | storage | trivial store + markDirty | - | none |

### `QCPAxisRect` (`layoutelements/layoutelement-axisrect.h`, Q_OBJECT, 2 props, 0 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `rangeDrag` | `Qt::Orientations` | RW | plain | sets drag/zoom axes from orientations (rebuilds lists) | - | none |
| `rangeZoom` | `Qt::Orientations` | RW | plain | sets drag/zoom axes from orientations (rebuilds lists) | - | none |

### `QCPColorScale` (`layoutelements/layoutelement-colorscale.h`, Q_OBJECT, 8 props, 3 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `type` | `QCPAxis::AxisType` | RW | plain | moves the axis side, rebuilds the internal layout | Y | none |
| `dataRange` | `QCPRange` | RWN | plain | validates, updates its users, emits | - | `dataRangeChanged` |
| `dataScaleType` | `QCPAxis::ScaleType` | RWN | plain | validates, updates its users, emits; async: forwards to the colour-map pipeline users | - | `dataScaleTypeChanged` |
| `gradient` | `QCPColorGradient` | RWN | plain | validates, updates its users, emits | - | `gradientChanged` |
| `label` | `QString` | RW | plain | validates or refuses | p | none |
| `barWidth` | `int` | RW | plain | calls into another object | Y | none |
| `rangeDrag` | `bool` | RW | plain | forwards to the internal axis rect; refuses when it is gone | - | none |
| `rangeZoom` | `bool` | RW | plain | forwards to the internal axis rect; refuses when it is gone | - | none |

### `QCPAbstractLegendItem` (`layoutelements/layoutelement-legend.h`, Q_OBJECT, 7 props, 2 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `parentLegend` | `QCPLegend*` | R | derived | follows the legend the item sits in; no change signal; A6 | Y | none |
| `font` | `QFont` | RW | storage (bench) | trivial: store + invalidatePaintBuffer | Y | none |
| `textColor` | `QColor` | RW | storage (bench) | trivial: store + invalidatePaintBuffer | Y | none |
| `selectedFont` | `QFont` | RW | storage (bench) | trivial store + invalidatePaintBuffer | Y | none |
| `selectedTextColor` | `QColor` | RW | storage (bench) | trivial store + invalidatePaintBuffer | Y | none |
| `selectable` | `QCP::SelectionType` | RWN | plain | validates stNone/stWhole and trims the selection | Y | `selectableChanged` |
| `selected` | `bool` | RWN | plain | selection state; marks dirty and emits selectionChanged | Y | `selectionChanged` |

### `QCPLegend` (`layoutelements/layoutelement-legend.h`, Q_OBJECT, 14 props, 2 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `borderPen` | `QPen` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `brush` | `QBrush` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `font` | `QFont` | RW | plain | font/textColor on the legend push into every item (fluent apply setter) | p | none |
| `textColor` | `QColor` | RW | plain | font/textColor on the legend push into every item (fluent apply setter) | p | none |
| `iconSize` | `QSize` | RW | storage | trivial: store + markDirty | - | none |
| `iconTextPadding` | `int` | RW | storage | trivial: store + markDirty | - | none |
| `iconBorderPen` | `QPen` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `selectableParts` | `SelectableParts` | RWN | plain | selection state of the parts; selectedParts also recomputes from items | Y | `selectableChanged` |
| `selectedParts` | `SelectableParts` | RWN | plain | selection state of the parts; selectedParts also recomputes from items | - | `selectionChanged` |
| `selectedBorderPen` | `QPen` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `selectedIconBorderPen` | `QPen` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `selectedBrush` | `QBrush` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `selectedFont` | `QFont` | RW | plain | pushes the value into every item | p | none |
| `selectedTextColor` | `QColor` | RW | plain | pushes the value into every item | p | none |

### `QCPTextElement` (`layoutelements/layoutelement-textelement.h`, Q_OBJECT, 7 props, 2 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `text` | `QString` | RW | storage (bench) | trivial: store + markDirty | Y | none |
| `font` | `QFont` | RW | storage (bench) | trivial: store + markDirty | Y | none |
| `textColor` | `QColor` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `selectedFont` | `QFont` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `selectedTextColor` | `QColor` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `selectable` | `QCP::SelectionType` | RWN | plain | validates stNone/stWhole and trims the selection | Y | `selectableChanged` |
| `selected` | `bool` | RWN | plain | selection state; marks dirty and emits selectionChanged | - | `selectionChanged` |

### `QCPBarsGroup` (`plottables/plottable-bars.h`, Q_OBJECT, 2 props, 0 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `spacingType` | `SpacingType` | RW | plain | dirties every member bar and bumps the revision | - | none |
| `spacing` | `double` | RW | plain | dirties every member bar and bumps the revision | - | none |

### `QCPBars` (`plottables/plottable-bars.h`, Q_OBJECT, 7 props, 0 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `width` | `double` | RW | plain | notifies the bars group revision after the store | - | none |
| `widthType` | `WidthType` | RW | plain | notifies the bars group revision after the store | - | none |
| `barsGroup` | `QCPBarsGroup*` | RW | plain | refuses a group whose members sit on other axes; updates the group; A6 | - | none |
| `baseValue` | `double` | RW | storage | trivial: store + markDirty | - | none |
| `stackingGap` | `double` | RW | storage | trivial: store + markDirty | - | none |
| `barBelow` | `QCPBars*` | R | derived | follows moveBelow/moveAbove; no change signal; A6 | - | none |
| `barAbove` | `QCPBars*` | R | derived | follows moveBelow/moveAbove; no change signal; A6 | - | none |

### `QCPColorMap` (`plottables/plottable-colormap.h`, Q_OBJECT, 2 props, 0 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `interpolate` | `bool` | RW | plain | also invalidates the cached map image | Y | none |
| `tightBoundary` | `bool` | RW | storage (bench) | trivial: store + markDirty | Y | none |

### `QCPAbstractColorMapped` (`plottables/plottable-colormapped.h`, Q_OBJECT, 4 props, 3 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `dataRange` | `QCPRange` | RWN | plain | forwards to the colour scale when one is shared; guards against deletion in the signal; async: colour-map pipeline re-bakes it (captured by value) | - | `dataRangeChanged` |
| `dataScaleType` | `QCPAxis::ScaleType` | RWN | plain | forwards to the colour scale when one is shared; guards against deletion in the signal; async: colour-map pipeline transform captures the scale type | Y | `dataScaleTypeChanged` |
| `gradient` | `QCPColorGradient` | RWN | plain | forwards to the colour scale when one is shared; guards against deletion in the signal; async: LUT rebuild (rebuildLut) | - | `gradientChanged` |
| `colorScale` | `QCPColorScale*` | RW | plain | virtual; re-registers with the scale (mUsers), may delete-watch; pointer ownership (A6); A6 | - | `colorScaleChanged` exists, not wired |

### `QCPCurve` (`plottables/plottable-curve.h`, Q_OBJECT, 3 props, 0 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `scatterStyle` | `QCPScatterStyle` | RW | plain | forwarded to the line family setter (components); async: components feed the graph pipeline | - | none |
| `scatterSkip` | `int` | RW | plain | forwarded to the line family setter (components); async: components feed the graph pipeline | - | none |
| `lineStyle` | `LineStyle` | RW | plain | forwarded to the line family setter (components); async: components feed the graph pipeline | - | none |

### `QCPErrorBars` (`plottables/plottable-errorbar.h`, Q_OBJECT, 5 props, 0 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `data` | `QSharedPointer<QCPErrorBarsDataContainer>` | RW | plain | swaps the shared data container (QSharedPointer) - not a bindable value; A6 | - | none |
| `dataPlottable` | `QCPAbstractPlottable*` | RW | plain | refuses another QCPErrorBars; attaches/detaches; pointer ownership (A6); A6 | Y | none |
| `errorType` | `ErrorType` | RW | storage | trivial: store + markDirty | - | none |
| `whiskerWidth` | `double` | RW | storage | trivial: store + markDirty | - | none |
| `symbolGap` | `double` | RW | storage | trivial: store + markDirty | - | none |

### `QCPFinancial` (`plottables/plottable-financial.h`, Q_OBJECT, 8 props, 0 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `chartStyle` | `ChartStyle` | RW | storage (bench) | trivial: store + markDirty | Y | none |
| `width` | `double` | RW | storage | trivial: store + markDirty | - | none |
| `widthType` | `WidthType` | RW | storage | trivial: store + markDirty | - | none |
| `twoColored` | `bool` | RW | storage | trivial: store + markDirty | - | none |
| `brushPositive` | `QBrush` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `brushNegative` | `QBrush` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `penPositive` | `QPen` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `penNegative` | `QPen` | RW | storage (bench) | trivial: store + markDirty | p | none |

### `QCPGraph` (`plottables/plottable-graph.h`, Q_OBJECT, 1 props, 0 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `adaptiveSampling` | `bool` | RW | plain | also calls linesChanged(); async: rebuilds the L2 resample (linesChanged) | Y | none |

### `QCPAbstractLine` (`plottables/plottable-line.h`, Q_OBJECT, 3 props, 0 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `lineStyle` | `LineStyle` | RW | plain | copies into every component (mComponents); the async pipeline reads components; async: components feed the graph pipeline; wantOrigin atomic is read by the worker | - | none |
| `scatterStyle` | `QCPScatterStyle` | RW | plain | copies into every component (mComponents); the async pipeline reads components; async: components feed the graph pipeline | - | none |
| `scatterSkip` | `int` | RW | plain | copies into every component (mComponents); the async pipeline reads components; async: components feed the graph pipeline | - | none |

### `QCPStatisticalBox` (`plottables/plottable-statisticalbox.h`, Q_OBJECT, 7 props, 0 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `width` | `double` | RW | storage | trivial: store + markDirty | - | none |
| `whiskerWidth` | `double` | RW | storage | trivial: store + markDirty | - | none |
| `whiskerPen` | `QPen` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `whiskerBarPen` | `QPen` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `whiskerAntialiased` | `bool` | RW | storage | trivial: store + markDirty | - | none |
| `medianPen` | `QPen` | RW | storage (bench) | trivial: store + markDirty | p | none |
| `outlierStyle` | `QCPScatterStyle` | RW | storage | store + markDirty (no equality guard: setValue-always, as A14 requires) | - | none |

### `QCPAbstractPlottable` (`plottables/plottable.h`, Q_OBJECT, 10 props, 2 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `name` | `QString` | RW | plain | marks the legend rows of the plot before the store; legend reads it | Y | none |
| `antialiasedFill` | `bool` | RW | storage | trivial store + markDirty | - | none |
| `antialiasedScatters` | `bool` | RW | storage | trivial store + markDirty | - | none |
| `pen` | `QPen` | RW | plain | virtual setter; subclasses copy it into components (A14 spike for pen) | p | none |
| `brush` | `QBrush` | RW | plain | virtual setter; subclasses copy it into components (A14 spike for pen) | Y | none |
| `keyAxis` | `QCPAxis*` | RW | plain | resolveAxes: re-binds systems, validates the pair; async: pipeline viewport source; A6 | - | none |
| `valueAxis` | `QCPAxis*` | RW | plain | resolveAxes: re-binds systems, validates the pair; async: pipeline viewport source; A6 | - | none |
| `selectable` | `QCP::SelectionType` | RWN | plain | enforces the selection type and trims the selection | Y | `selectableChanged` |
| `selection` | `QCPDataSelection` | RWN | plain | enforceType + assignSelection + emit; also written by user gestures | Y | `selectionChanged` |
| `selectionDecorator` | `QCPSelectionDecorator*` | RW | plain | registers with the plottable and deletes the old decorator; pointer ownership (A6); A6 | Y | none |

### `QCPTheme` (`theme.h`, Q_OBJECT, 16 props, 16 with NOTIFY)

| property | type | decl | form | reason (from the setter) | hot | notify today / signals on the class |
|---|---|---|---|---|---|---|
| `background` | `QColor` | RWN | storage | trivial: store + emit changed | - | `changed` |
| `foreground` | `QColor` | RWN | storage | trivial: store + emit changed | - | `changed` |
| `grid` | `QColor` | RWN | storage | trivial: store + emit changed | - | `changed` |
| `subGrid` | `QColor` | RWN | storage | trivial: store + emit changed | - | `changed` |
| `selection` | `QColor` | RWN | storage | trivial: store + emit changed | - | `changed` |
| `legendBackground` | `QColor` | RWN | storage | trivial: store + emit changed | - | `changed` |
| `legendBorder` | `QColor` | RWN | storage | trivial: store + emit changed | - | `changed` |
| `tooltipBackground` | `QColor` | RWN | storage | trivial: store + emit changed | - | `changed` |
| `tooltipBorder` | `QColor` | RWN | storage | trivial: store + emit changed | - | `changed` |
| `tooltipText` | `QColor` | RWN | storage | trivial: store + emit changed | - | `changed` |
| `tooltipCornerRadius` | `qreal` | RWN | storage | fuzzy-compare guard, store, emit changed(); trivial | - | `changed` |
| `crosshairColor` | `QColor` | RWN | storage | trivial: store + emit changed | - | `changed` |
| `busyIndicatorSymbol` | `QString` | RWN | storage | trivial: store + emit changed | - | `changed` |
| `busyFadeAlpha` | `qreal` | RWN | storage | fuzzy-compare guard, store, emit changed(); trivial | - | `changed` |
| `busyShowDelayMs` | `int` | RWN | storage | trivial: store + emit changed | - | `changed` |
| `busyHideDelayMs` | `int` | RWN | storage | trivial: store + emit changed | - | `changed` |

## 3. Setter/getter pairs with no `Q_PROPERTY` yet

### 3.1 One-argument pairs (`void setX(T)` plus a `T x() const`/`isX`/`hasX` in the same class)

Same columns. For `Q_GADGET` classes the form is **gadget** (a property is possible, `NOTIFY` and `BINDABLE` are not).
Polar axes mirror `QCPAxis`: the same property names exist there and the bodies are `markDirty()` plus a store.

### `QCPAxisBase` (`axis/axis-base.h`, Q_OBJECT, 2 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `rangeDraggable` | `bool` | `rangeDraggable` | storage | trivial: store | - | none |
| `rangeZoomable` | `bool` | `rangeZoomable` | storage | trivial: store | - | none |

### `QCPAxis` (`axis/axis.h`, Q_OBJECT, 1 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `labelRenderer` | `QCPLabelRenderer*` | `labelRenderer` | plain | object/shared-pointer typed; ownership and handles per A6; trivial: store + mCachedMarginValid = false, markAppearanceDirty | - | none |

### `QCPAxisTicker` (`axis/axisticker.h`, Q_GADGET, 3 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `tickStepStrategy` | `TickStepStrategy` | `tickStepStrategy` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14); ticker is shared by QSharedPointer, a change reaches no one | - | none |
| `tickCount` | `int` | `tickCount` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14); ticker is shared by QSharedPointer, a change reaches no one | - | none |
| `tickOrigin` | `double` | `tickOrigin` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14); ticker is shared by QSharedPointer, a change reaches no one | - | none |

### `QCPAxisTickerFixed` (`axis/axistickerfixed.h`, Q_GADGET, 2 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `tickStep` | `double` | `tickStep` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14); ticker is shared by QSharedPointer, a change reaches no one | - | none |
| `scaleStrategy` | `ScaleStrategy` | `scaleStrategy` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14); ticker is shared by QSharedPointer, a change reaches no one | - | none |

### `QCPAxisTickerPi` (`axis/axistickerpi.h`, Q_GADGET, 4 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `piSymbol` | `QString` | `piSymbol` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14); ticker is shared by QSharedPointer, a change reaches no one | - | none |
| `piValue` | `double` | `piValue` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14); ticker is shared by QSharedPointer, a change reaches no one | - | none |
| `periodicity` | `int` | `periodicity` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14); ticker is shared by QSharedPointer, a change reaches no one | - | none |
| `fractionStyle` | `FractionStyle` | `fractionStyle` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14); ticker is shared by QSharedPointer, a change reaches no one | - | none |

### `QCPAxisTickerTime` (`axis/axistickertime.h`, Q_GADGET, 1 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `timeFormat` | `QString` | `timeFormat` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14); ticker is shared by QSharedPointer, a change reaches no one | - | none |

### `QCPLabelPainterPrivate` (`axis/labelpainter.h`, Q_GADGET, 12 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `anchorSide` | `AnchorSide` | `anchorSide` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |
| `anchorMode` | `AnchorMode` | `anchorMode` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |
| `anchorReference` | `QPointF` | `anchorReference` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |
| `anchorReferenceType` | `AnchorReferenceType` | `anchorReferenceType` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |
| `font` | `QFont` | `font` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |
| `color` | `QColor` | `color` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |
| `padding` | `int` | `padding` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |
| `rotation` | `double` | `rotation` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |
| `substituteExponent` | `bool` | `substituteExponent` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |
| `multiplicationSymbol` | `QChar` | `multiplicationSymbol` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |
| `abbreviateDecimalPowers` | `bool` | `abbreviateDecimalPowers` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |
| `cacheSize` | `int` | `cacheSize` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |

### `QCPColorGradient` (`colorgradient.h`, Q_GADGET, 6 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `levelCount` | `int` | `levelCount` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |
| `colorStops` | `QMap<double, QColor>` | `colorStops` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |
| `colorInterpolation` | `ColorInterpolation` | `colorInterpolation` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |
| `nanHandling` | `NanHandling` | `nanHandling` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |
| `nanColor` | `QColor` | `nanColor` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |
| `periodic` | `bool` | `periodic` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |

### `QCustomPlot` (`core.h`, Q_OBJECT, 18 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `itemCreator` | `ItemCreator` | `itemCreator` | plain | updates other objects (loop) | - | `itemCreated` (setter emits it) |
| `itemPositioner` | `ItemPositioner` | `itemPositioner` | plain | updates other objects (loop) | - | none |
| `creationModeEnabled` | `bool` | `creationModeEnabled` | plain | calls into another object | - | none |
| `creationModifier` | `Qt::KeyboardModifier` | `creationModifier` | storage | trivial: store | - | none |
| `bufferDevicePixelRatio` | `double` | `bufferDevicePixelRatio` | plain | updates other objects (loop) | - | none |
| `antialiasedElements` | `QCP::AntialiasedElements` | `antialiasedElements` | plain | does more than store (if ((mNotAntialiasedElements & mAntialia) | - | none |
| `notAntialiasedElements` | `QCP::AntialiasedElements` | `notAntialiasedElements` | plain | does more than store (if ((mNotAntialiasedElements & mAntialia) | - | none |
| `interactions` | `QCP::Interactions` | `interactions` | plain | does more than store (updateGestureInput()) | - | none |
| `plottingHints` | `QCP::PlottingHints` | `plottingHints` | storage | trivial: store | - | none |
| `selectionRectMode` | `QCP::SelectionRectMode` | `selectionRectMode` | plain | calls into another object | - | none |
| `selectionRect` | `QCPSelectionRect*` | `selectionRect` | plain | object/shared-pointer typed; ownership and handles per A6; calls into another object | - | none |
| `theme` | `QCPTheme*` | `theme` | plain | object/shared-pointer typed; ownership and handles per A6; validates or refuses | - | none |
| `skipReplotsWhenHidden` | `bool` | `skipReplotsWhenHidden` | storage | trivial: store | - | none |
| `itemIndexing` | `bool` | `itemIndexing` | plain | updates other objects (loop) | - | none |
| `dataSwapDebounceMs` | `int` | `dataSwapDebounceMs` | plain | normalises the value; touches the async pipeline (rebuilds the transform/binning/adapter) | - | none |
| `dataSwapMaxWaitMs` | `int` | `dataSwapMaxWaitMs` | plain | normalises the value; touches the async pipeline (rebuilds the transform/binning/adapter) | - | none |
| `currentLayer` | `QString` | `currentLayer` | plain | validates or refuses | - | none |
| `currentLayer` | `QCPLayer*` | `currentLayer` | plain | object/shared-pointer typed; ownership and handles per A6; validates or refuses | - | none |

### `QCPAsyncPipelineBase` (`datasource/async-pipeline.h`, Q_OBJECT, 1 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `scheduler` | `QCPPipelineScheduler*` | `scheduler` | plain | object/shared-pointer typed; ownership and handles per A6; pipeline state or scheduler config; A14: pipeline state is not a property | - | none |

### `QCPPipelineScheduler` (`datasource/pipeline-scheduler.h`, Q_OBJECT, 1 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `maxThreads` | `int` | `maxThreads` | out | pipeline state or scheduler config; A14: pipeline state is not a property | - | none |

### `QCPItemRichText` (`items/item-richtext.h`, Q_OBJECT, 1 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `cornerRadius` | `double` | `cornerRadius` | storage | trivial: store | - | none |

### `QCPItemRSpan` (`items/item-rspan.h`, Q_OBJECT, 2 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `keyRange` | `QCPRange` | `keyRange` | plain | does more than store (leftEdge->setCoords(range.lower, leftEdg) | - | `keyRangeChanged` |
| `valueRange` | `QCPRange` | `valueRange` | plain | does more than store (bottomEdge->setCoords(bottomEdge->coords) | - | `valueRangeChanged` |

### `QCPItemSpan` (`items/item-span.h`, Q_OBJECT, 1 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `range` | `QCPRange` | `range` | plain | does more than store (const bool vertical = mOrientation == Qt) | - | `rangeChanged` |

### `QCPAbstractSpanItem` (`items/item-spanbase.h`, Q_OBJECT, 5 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `brush` | `QBrush` | `brush` | storage (bench) | trivial: store + markDirty | p | none |
| `selectedBrush` | `QBrush` | `selectedBrush` | storage (bench) | trivial: store + markDirty | p | none |
| `borderPen` | `QPen` | `borderPen` | storage (bench) | trivial: store + markDirty | p | none |
| `selectedBorderPen` | `QPen` | `selectedBorderPen` | storage (bench) | trivial: store + markDirty | p | none |
| `movable` | `bool` | `movable` | storage | trivial: store | - | none |

### `QCPItemTracer` (`items/item-tracer.h`, Q_OBJECT, 1 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `component` | `int` | `component` | plain | validates or refuses | - | none |

### `QCPItemPosition` (`items/item.h`, Q_GADGET, 8 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `type` | `PositionType` | `type` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14); position holds the item link (mParentItem), system/area are object-typed (A6) | - | none |
| `typeX` | `PositionType` | `typeX` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14); position holds the item link (mParentItem), system/area are object-typed (A6) | - | none |
| `typeY` | `PositionType` | `typeY` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14); position holds the item link (mParentItem), system/area are object-typed (A6) | - | none |
| `coords` | `QPointF` | `coords` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14); position holds the item link (mParentItem), system/area are object-typed (A6) | - | none |
| `axisRect` | `QCPAxisRect*` | `axisRect` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14); position holds the item link (mParentItem), system/area are object-typed (A6) | - | none |
| `system` | `QCPCoordinateSystem*` | `system` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14); position holds the item link (mParentItem), system/area are object-typed (A6) | - | none |
| `area` | `QCPArea*` | `area` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14); position holds the item link (mParentItem), system/area are object-typed (A6) | - | none |
| `pixelPosition` | `QPointF` | `pixelPosition` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14); position holds the item link (mParentItem), system/area are object-typed (A6) | - | none |

### `QCPAbstractItem` (`items/item.h`, Q_OBJECT, 1 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `clipArea` | `QCPArea*` | `clipArea` | plain | object/shared-pointer typed; ownership and handles per A6; does more than store (noteFramesChanged()) | - | none |

### `QCPLayoutElement` (`layout.h`, Q_OBJECT, 1 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `autoMargins` | `QCP::MarginSides` | `autoMargins` | storage | trivial: store + markLayoutAppearanceDirty | - | none |

### `QCPGroupLegendItem` (`layoutelements/layoutelement-legend-group.h`, Q_OBJECT, 2 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `expanded` | `bool` | `expanded` | storage | trivial: store + markDirty | - | none |
| `selectedComponent` | `int` | `selectedComponent` | storage | trivial: store | - | none |

### `QCPTextElement` (`layoutelements/layoutelement-textelement.h`, Q_OBJECT, 1 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `textFlags` | `int` | `textFlags` | storage | trivial: store + markDirty | - | none |

### `QCPLineEnding` (`lineending.h`, Q_GADGET, 4 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `style` | `EndingStyle` | `style` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |
| `width` | `double` | `width` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |
| `length` | `double` | `length` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |
| `inverted` | `bool` | `inverted` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |

### `QCPOverlay` (`overlay.h`, Q_OBJECT, 4 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `collapsible` | `bool` | `isCollapsible` | plain | does more than store (const bool unsetting = !mCollapsible && ) | - | `collapsedChanged` (setter emits it) |
| `collapsed` | `bool` | `isCollapsed` | plain | does more than store (redraw()) | - | `collapsedChanged` |
| `opacity` | `qreal` | `opacity` | plain | normalises the value | - | none |
| `font` | `QFont` | `font` | storage | trivial: store | - | none |

### `QCPPainter` (`painting/painter.h`, Q_GADGET, 2 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `antialiasing` | `bool` | `antialiasing` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |
| `modes` | `PainterModes` | `modes` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |

### `QCPBars` (`plottables/plottable-bars.h`, Q_OBJECT, 1 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `data` | `QSharedPointer<QCPBarsDataContainer>` | `data` | plain | object/shared-pointer typed; ownership and handles per A6; does more than store (notifyGroupRevision()) | - | none |

### `QCPColorMap2` (`plottables/plottable-colormap2.h`, Q_OBJECT, 4 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `gapThreshold` | `double` | `gapThreshold` | plain | does more than store (installResampleTransform()); touches the async pipeline (rebuilds the transform/binning/adapter) | - | none |
| `contourLevels` | `QVector<double>` | `contourLevels` | plain | does more than store (mContours.setLevels(levels)); touches the async pipeline (rebuilds the transform/binning/adapter) | - | none |
| `contourPen` | `QPen` | `contourPen` | plain | does more than store (mContours.setPen(pen)); touches the async pipeline (rebuilds the transform/binning/adapter) | p | none |
| `contourLabelEnabled` | `bool` | `contourLabelEnabled` | plain | does more than store (mContours.setLabelsEnabled(enabled)); touches the async pipeline (rebuilds the transform/binning/adapter) | - | none |

### `QCPCurve` (`plottables/plottable-curve.h`, Q_OBJECT, 1 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `data` | `QSharedPointer<QCPCurveDataContainer>` | `data` | plain | object/shared-pointer typed; ownership and handles per A6; trivial: store + markDirty | - | none |

### `QCPFinancial` (`plottables/plottable-financial.h`, Q_OBJECT, 1 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `data` | `QSharedPointer<QCPFinancialDataContainer>` | `data` | plain | object/shared-pointer typed; ownership and handles per A6; trivial: store + markDirty | - | none |

### `QCPGraph` (`plottables/plottable-graph.h`, Q_OBJECT, 2 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `deferredDataSwap` | `bool` | `deferredDataSwap` | plain | updates other objects (loop); touches the async pipeline (rebuilds the transform/binning/adapter) | - | none |
| `gapThreshold` | `double` | `gapThreshold` | plain | calls into another object; touches the async pipeline (rebuilds the transform/binning/adapter) | - | none |

### `QCPPlottableGroup` (`plottables/plottable-group.h`, Q_OBJECT, 2 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `name` | `QString` | `name` | plain | does more than store (markRowDirty()) | - | none |
| `visible` | `bool` | `visible` | plain | updates other objects (loop) | - | none |

### `QCPHistogram` (`plottables/plottable-histogram.h`, Q_OBJECT, 4 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `bins` | `int` | `bins` | plain | validates or refuses; touches the async pipeline (rebuilds the transform/binning/adapter) | - | none |
| `normalization` | `QCP::HistogramNormalization` | `normalization` | plain | does more than store (refreshBinning()); touches the async pipeline (rebuilds the transform/binning/adapter) | - | none |
| `style` | `Style` | `style` | plain | does more than store (requestReplot()); touches the async pipeline (rebuilds the transform/binning/adapter) | - | none |
| `columnColors` | `QList<QColor>` | `columnColors` | plain | does more than store (requestReplot()); touches the async pipeline (rebuilds the transform/binning/adapter) | - | none |

### `QCPHistogram2D` (`plottables/plottable-histogram2d.h`, Q_OBJECT, 5 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `normalization` | `QCP::HistogramNormalization` | `normalization` | plain | does more than store (invalidateShown()); touches the async pipeline (rebuilds the transform/binning/adapter) | - | none |
| `columnNormalized` | `bool` | `columnNormalized` | plain | does more than store (invalidateShown()); touches the async pipeline (rebuilds the transform/binning/adapter) | - | none |
| `contourLevels` | `QVector<double>` | `contourLevels` | plain | does more than store (mContours.setLevels(levels)); touches the async pipeline (rebuilds the transform/binning/adapter) | - | none |
| `contourPen` | `QPen` | `contourPen` | plain | does more than store (mContours.setPen(pen)); touches the async pipeline (rebuilds the transform/binning/adapter) | - | none |
| `contourLabelEnabled` | `bool` | `contourLabelEnabled` | plain | does more than store (mContours.setLabelsEnabled(enabled)); touches the async pipeline (rebuilds the transform/binning/adapter) | - | none |

### `QCPIntervals` (`plottables/plottable-intervals.h`, Q_OBJECT, 6 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `stackMode` | `StackMode` | `stackMode` | plain | does more than store (const QCPLaneLayout::Batch lanesChangedL) | - | none |
| `forbidOverlap` | `bool` | `forbidOverlap` | storage | trivial: store | - | none |
| `style` | `Style` | `style` | storage | trivial: store + markDirty | - | none |
| `selectedRows` | `QVector<int>` | `selectedRows` | plain | updates other objects (loop) | - | none |
| `editable` | `bool` | `editable` | storage | trivial: store + markDirty | - | none |
| `editModes` | `EditModes` | `editModes` | storage | trivial: store + markDirty | - | none |

### `QCPStatisticalBox` (`plottables/plottable-statisticalbox.h`, Q_OBJECT, 1 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `data` | `QSharedPointer<QCPStatisticalBoxDataContainer>` | `data` | plain | object/shared-pointer typed; ownership and handles per A6; trivial: store + markDirty | - | none |

### `QCPWaterfallGraph` (`plottables/plottable-waterfall.h`, Q_OBJECT, 5 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `offsetMode` | `OffsetMode` | `offsetMode` | plain | does more than store (rebuildAdapter()); touches the async pipeline (rebuilds the transform/binning/adapter) | - | none |
| `uniformSpacing` | `double` | `uniformSpacing` | plain | does more than store (rebuildAdapter()); touches the async pipeline (rebuilds the transform/binning/adapter) | - | none |
| `offsets` | `QVector<double>` | `offsets` | plain | does more than store (rebuildAdapter()); touches the async pipeline (rebuilds the transform/binning/adapter) | - | none |
| `normalize` | `bool` | `normalize` | plain | does more than store (rebuildAdapter()); touches the async pipeline (rebuilds the transform/binning/adapter) | - | none |
| `gain` | `double` | `gain` | plain | does more than store (rebuildAdapter()); touches the async pipeline (rebuilds the transform/binning/adapter) | - | none |

### `QCPSelectionDecorator` (`plottables/plottable.h`, Q_GADGET, 3 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `pen` | `QPen` | `pen` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |
| `brush` | `QBrush` | `brush` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |
| `usedScatterProperties` | `QCPScatterStyle::ScatterProperties` | `usedScatterProperties` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |

### `QCPAbstractPlottable` (`plottables/plottable.h`, Q_OBJECT, 3 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `system` | `QCPCoordinateSystem*` | `system`, virtual | plain | object/shared-pointer typed; ownership and handles per A6; virtual setter | - | none |
| `wantsLegend` | `bool` | `wantsLegend` | plain | does more than store (if (mGroup) mRowOnLeave = wants) | - | none |
| `busy` | `bool` | `busy` | out | pipeline state or scheduler config; A14: pipeline state is not a property | - | `busyChanged` |

### `QCPPolarAxisAngular` (`polar/angularaxis.h`, Q_OBJECT, 29 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `angle` | `double` | `angle` | plain | calls into another object | - | none |
| `ticks` | `bool` | `ticks` | storage | trivial: store + markDirty | - | none |
| `tickLabels` | `bool` | `tickLabels` | storage | trivial: store + markDirty | - | none |
| `tickLabelPadding` | `int` | `tickLabelPadding` | plain | does more than store (mLabelPainter.setPadding(padding)) | - | none |
| `tickLabelFont` | `QFont` | `tickLabelFont` | storage (bench) | trivial: store + markDirty | p | none |
| `tickLabelColor` | `QColor` | `tickLabelColor` | storage (bench) | trivial: store + markDirty | p | none |
| `tickLabelRotation` | `double` | `tickLabelRotation` | plain | does more than store (mLabelPainter.setRotation(degrees)) | - | none |
| `tickLabelMode` | `LabelMode` | `tickLabelMode` | plain | does more than store (const auto anchor = mode == lmUpright ? ) | - | none |
| `numberFormat` | `QString` | `numberFormat` | plain | validates or refuses | - | none |
| `numberPrecision` | `int` | `numberPrecision` | storage | trivial: store + markDirty | - | none |
| `tickLengthIn` | `int` | `tickLengthIn` | storage | trivial: store + markDirty | - | none |
| `tickLengthOut` | `int` | `tickLengthOut` | storage | trivial: store + markDirty | - | none |
| `subTicks` | `bool` | `subTicks` | storage | trivial: store + markDirty | - | none |
| `subTickLengthIn` | `int` | `subTickLengthIn` | storage | trivial: store + markDirty | - | none |
| `subTickLengthOut` | `int` | `subTickLengthOut` | storage | trivial: store + markDirty | - | none |
| `basePen` | `QPen` | `basePen` | storage (bench) | trivial: store + markDirty | p | none |
| `tickPen` | `QPen` | `tickPen` | storage (bench) | trivial: store + markDirty | p | none |
| `subTickPen` | `QPen` | `subTickPen` | storage (bench) | trivial: store + markDirty | p | none |
| `labelFont` | `QFont` | `labelFont` | storage (bench) | trivial: store + markDirty | p | none |
| `labelColor` | `QColor` | `labelColor` | storage (bench) | trivial: store + markDirty | p | none |
| `label` | `QString` | `label` | storage | trivial: store + markDirty | - | none |
| `labelPadding` | `int` | `labelPadding` | storage | trivial: store + markDirty | - | none |
| `selectedTickLabelFont` | `QFont` | `selectedTickLabelFont` | storage (bench) | trivial: store + markDirty | p | none |
| `selectedLabelFont` | `QFont` | `selectedLabelFont` | storage (bench) | trivial: store + markDirty | p | none |
| `selectedTickLabelColor` | `QColor` | `selectedTickLabelColor` | storage (bench) | trivial: store + markDirty | p | none |
| `selectedLabelColor` | `QColor` | `selectedLabelColor` | storage (bench) | trivial: store + markDirty | p | none |
| `selectedBasePen` | `QPen` | `selectedBasePen` | storage (bench) | trivial: store + markDirty | p | none |
| `selectedTickPen` | `QPen` | `selectedTickPen` | storage (bench) | trivial: store + markDirty | p | none |
| `selectedSubTickPen` | `QPen` | `selectedSubTickPen` | storage (bench) | trivial: store + markDirty | p | none |

### `QCPPolarArea` (`polar/polar-area.h`, Q_OBJECT, 1 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `rangeZoomFactor` | `double` | `rangeZoomFactor` | storage | trivial: store | - | none |

### `QCPPolarGraph` (`polar/polargraph.h`, Q_OBJECT, 4 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `periodic` | `bool` | `periodic` | storage | trivial: store + markDirty | - | none |
| `lineStyle` | `LineStyle` | `lineStyle` | storage | trivial: store + markDirty | - | none |
| `scatterStyle` | `QCPScatterStyle` | `scatterStyle` | storage | trivial: store + markDirty | - | none |
| `data` | `QSharedPointer<QCPGraphDataContainer>` | `data` | plain | object/shared-pointer typed; ownership and handles per A6; trivial: store + markDirty | - | none |

### `QCPPolarGrid` (`polar/polargrid.h`, Q_OBJECT, 10 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `radialAxis` | `QCPPolarAxisRadial*` | `radialAxis` | plain | object/shared-pointer typed; ownership and handles per A6; trivial: store + markDirty | - | none |
| `type` | `GridTypes` | `type` | storage | trivial: store + markDirty | - | none |
| `subGridType` | `GridTypes` | `subGridType` | storage | trivial: store + markDirty | - | none |
| `antialiasedSubGrid` | `bool` | `antialiasedSubGrid` | storage | trivial: store + markDirty | - | none |
| `antialiasedZeroLine` | `bool` | `antialiasedZeroLine` | storage | trivial: store + markDirty | - | none |
| `angularPen` | `QPen` | `angularPen` | storage (bench) | trivial: store + markDirty | p | none |
| `angularSubGridPen` | `QPen` | `angularSubGridPen` | storage (bench) | trivial: store + markDirty | p | none |
| `radialPen` | `QPen` | `radialPen` | storage (bench) | trivial: store + markDirty | p | none |
| `radialSubGridPen` | `QPen` | `radialSubGridPen` | storage (bench) | trivial: store + markDirty | p | none |
| `radialZeroLinePen` | `QPen` | `radialZeroLinePen` | storage (bench) | trivial: store + markDirty | p | none |

### `QCPPolarAxisRadial` (`polar/radialaxis.h`, Q_OBJECT, 30 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `angle` | `double` | `angle` | plain | calls into another object | - | none |
| `angleReference` | `AngleReference` | `angleReference` | plain | calls into another object | - | none |
| `ticks` | `bool` | `ticks` | storage | trivial: store + markDirty | - | none |
| `tickLabels` | `bool` | `tickLabels` | storage | trivial: store + markDirty | - | none |
| `tickLabelPadding` | `int` | `tickLabelPadding` | plain | does more than store (mLabelPainter.setPadding(padding)) | - | none |
| `tickLabelFont` | `QFont` | `tickLabelFont` | storage (bench) | trivial: store + markDirty | p | none |
| `tickLabelColor` | `QColor` | `tickLabelColor` | storage (bench) | trivial: store + markDirty | p | none |
| `tickLabelRotation` | `double` | `tickLabelRotation` | plain | does more than store (mLabelPainter.setRotation(degrees)) | - | none |
| `tickLabelMode` | `LabelMode` | `tickLabelMode` | plain | does more than store (const auto anchor = mode == lmUpright ? ) | - | none |
| `numberFormat` | `QString` | `numberFormat` | plain | validates or refuses | - | none |
| `numberPrecision` | `int` | `numberPrecision` | storage | trivial: store + markDirty | - | none |
| `tickLengthIn` | `int` | `tickLengthIn` | storage | trivial: store + markDirty | - | none |
| `tickLengthOut` | `int` | `tickLengthOut` | storage | trivial: store + markDirty | - | none |
| `subTicks` | `bool` | `subTicks` | storage | trivial: store + markDirty | - | none |
| `subTickLengthIn` | `int` | `subTickLengthIn` | storage | trivial: store + markDirty | - | none |
| `subTickLengthOut` | `int` | `subTickLengthOut` | storage | trivial: store + markDirty | - | none |
| `basePen` | `QPen` | `basePen` | storage (bench) | trivial: store + markDirty | p | none |
| `tickPen` | `QPen` | `tickPen` | storage (bench) | trivial: store + markDirty | p | none |
| `subTickPen` | `QPen` | `subTickPen` | storage (bench) | trivial: store + markDirty | p | none |
| `labelFont` | `QFont` | `labelFont` | storage (bench) | trivial: store + markDirty | p | none |
| `labelColor` | `QColor` | `labelColor` | storage (bench) | trivial: store + markDirty | p | none |
| `label` | `QString` | `label` | storage | trivial: store + markDirty | - | none |
| `labelPadding` | `int` | `labelPadding` | storage | trivial: store + markDirty | - | none |
| `selectedTickLabelFont` | `QFont` | `selectedTickLabelFont` | storage (bench) | trivial: store + markDirty | p | none |
| `selectedLabelFont` | `QFont` | `selectedLabelFont` | storage (bench) | trivial: store + markDirty | p | none |
| `selectedTickLabelColor` | `QColor` | `selectedTickLabelColor` | storage (bench) | trivial: store + markDirty | p | none |
| `selectedLabelColor` | `QColor` | `selectedLabelColor` | storage (bench) | trivial: store + markDirty | p | none |
| `selectedBasePen` | `QPen` | `selectedBasePen` | storage (bench) | trivial: store + markDirty | p | none |
| `selectedTickPen` | `QPen` | `selectedTickPen` | storage (bench) | trivial: store + markDirty | p | none |
| `selectedSubTickPen` | `QPen` | `selectedSubTickPen` | storage (bench) | trivial: store + markDirty | p | none |

### `QCPScatterStyle` (`scatterstyle.h`, Q_GADGET, 6 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `size` | `double` | `size` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |
| `shape` | `ScatterShape` | `shape` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |
| `pen` | `QPen` | `pen` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |
| `brush` | `QBrush` | `brush` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |
| `pixmap` | `QPixmap` | `pixmap` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |
| `customPath` | `QPainterPath` | `customPath` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |

### `QCPSelectionDecoratorBracket` (`selectiondecorator-bracket.h`, Q_GADGET, 7 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `bracketPen` | `QPen` | `bracketPen` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |
| `bracketBrush` | `QBrush` | `bracketBrush` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |
| `bracketWidth` | `int` | `bracketWidth` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |
| `bracketHeight` | `int` | `bracketHeight` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |
| `bracketStyle` | `BracketStyle` | `bracketStyle` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |
| `tangentToData` | `bool` | `tangentToData` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |
| `tangentAverage` | `int` | `tangentAverage` | gadget | value type: Q_PROPERTY without NOTIFY, not bindable (A14) | - | none |

### `QCPSelectionRect` (`selectionrect.h`, Q_OBJECT, 2 pairs)

| property | type | getter | form | reason | hot | signal on the class |
|---|---|---|---|---|---|---|
| `pen` | `QPen` | `pen` | storage | trivial: store | - | none |
| `brush` | `QBrush` | `brush` | storage | trivial: store | - | none |

### 3.2 Setters that are not a plain pair (listed, not proposed as properties)

No getter of the same name, several required arguments, or a template/`std::` argument. Most are actions or
multi-argument setters; the "noget" ones may still deserve a getter and a property (flagged where the data is plain).

| Class | Setters |
|---|---|
| `QCPAxisBase` | `setRangeLower`, `setRangeUpper` |
| `QCPAxis` | `setTickLength`, `setSubTickLength`, `setColor`, `setScaleRatio` |
| `QCPAxisTickerTime` | `setFieldWidth` |
| `QCPColorGradient` | `setColorStopAt` |
| `QCustomPlot` | `setAntialiasedElement`, `setNotAntialiasedElement`, `setInteraction`, `setPlottingHint`, `setMaxPipelineThreads` |
| `QCP` | `setMarginValue` |
| `QCPItemText` | `setPosition` |
| `QCPItemPosition` | `setParentAnchor`, `setParentAnchorX`, `setParentAnchorY`, `setCoords`, `setAxes` |
| `QCPLayoutGrid` | `setColumnStretchFactor`, `setRowStretchFactor` |
| `QCPLayoutInset` | `setInsetPlacement`, `setInsetAlignment`, `setInsetRect` |
| `QCPAxisRect` | `setRangeDragAxes` x3, `setRangeZoomAxes` x3, `setRangeZoomFactor` x2 |
| `QCPPainter` | `setMode`, `setPen` x3 |
| `QCPBars` | `setData` |
| `QCPColorMap` | `setData` |
| `QCPColorMap2` | `setDataSource` x2, `setData`, `setAutoContourLevels` |
| `QCPCurve` | `setData` x2, `setPen` |
| `QCPFinancial` | `setData` |
| `QCPGraph` | `setDataSource` x2, `setData` x4, `setComponentFill`, `setComponentFillBetween`, `setComponentFillToGraph`, `setComponentFillBrush`, `setComponentErrorBars`, `setSystem`, `setColorValues` x2, `setComponentSelection` |
| `QCPHistogram` | `setDataSource`, `setBinned`, `setFixedRange` |
| `QCPHistogram2D` | `setData`, `setDataSource` x2, `setBins`, `setAutoContourLevels` |
| `QCPIntervals` | `setData`, `setCategoryColors`, `setCategoryNames`, `setCategoryOrder`, `setSnap`, `setSnapTimes` |
| `QCPAbstractLine` | `setComponentVisible`, `setComponentInLegend`, `setComponentNames`, `setComponentColors`, `setComponentPens`, `setComponentLineStyle`, `setComponentScatterStyle`, `setColorValues` x2 |
| `QCPStatisticalBox` | `setData` |
| `QCPWaterfallGraph` | `setDataSource` |
| `QCPSelectionDecorator` | `setScatterStyle` |
| `QCPAbstractPlottable` | `setPipelineScheduler`, `setBusyIndicatorSymbol`, `setBusyFadeAlpha`, `setBusyShowDelayMs`, `setBusyHideDelayMs` |
| `QCPPolarAxisAngular` | `setTickLength`, `setSubTickLength`, `setLabelPosition` |
| `QCPPolarGraph` | `setData`, `setSystem` |
| `QCPPolarAxisRadial` | `setTickLength`, `setSubTickLength` |
| `QCPScatterStyle` | `setFromOther` |

### 3.3 The 3.0 classes, read by hand

Read-only getters of the new model, which A14 puts on the surface as constant or derived properties:

| Class | Getter | Proposed | Reason |
|---|---|---|---|
| `QCPCoordinateSystem` | `area()`, `keyAxis()`, `valueAxis()` | constant (object-typed, A6) | fixed at construction (`coordinate-system.h:64-70`); handles today |
| `QCPCoordinateSystem` | `capabilities()` (`QCP::Capabilities`, `Q_FLAG_NS` exists) | constant per subclass | pure virtual, one answer per subclass (`coordinate-system.h:99`, `coordinate-system-polar.h:41`); `CONSTANT` on a read-only virtual getter is legal |
| `QCPCoordinateSystem` | `revision()` (quint64) | derived | moves on every parameter change; the signal is the existing `changed()` (connect-notify gated) |
| `QCPCoordinateSystem` | `transform()` (`QCPTransform`) | derived or drop | `QCPTransform` is a non-gadget `std::variant` value; not a property type until it is a gadget or opaque metatype |
| `QCPCoordinateSystem` | `plottables()` (handle list) | drop | a list of handles is not a property type (the layer made the same call, `layer.cpp:112`) |
| `QCPArea` | `insetLayout()`, `systems()`, `systemsListen()` | constant / derived / drop | inset layout is created with the area; the systems list changes with axes (no signal); `systemsListen` is bookkeeping |
| `QCPArea` | `background`, `backgroundScaled`, `backgroundScaledMode` | already properties (3), no `NOTIFY` | see section 2 |
| `QCPPolarArea` | `QCPPolarArea::rangeZoomFactor` | storage | trivial store (section 3.1) |
| `QCPItemPosition` (gadget) | `type`, `typeX`, `typeY`, `parentAnchor*`, `coords`, `keyAxis`, `valueAxis`, `axisRect`, `system`, `area`, `pixelPosition` | gadget properties, no `NOTIFY` | B2 of the bindable review: a gadget has no signal; `system` and `area` are object-typed (A6) and `setSystem` refuses a system of another plot |
| `QCPAlignmentGroup` | `sides()`, `elements()` | derived (`membersChanged` exists) | `sides` is fixed at construction (constant); `elements` changes with add/remove and `membersChanged()` already fires |
| `QCPTheme` | 16 properties | already properties, storage candidates | see section 2; `QCustomPlot::theme` (pair, pointer) is plain with ownership per A6; the 7 `theme*` forwarders on `QCustomPlot` duplicate them |
| `QCPAxisBase` | `linkSource()`, `link()` (shared `QCPAxisLink`), `unlink()` | derived / out | link is set by `linkTo(source, forward, inverse)`, an action with functions: not a property |

Not a typed-position property at all: `QCPItemPosition::set(const QCP::Position&)` / `asPosition()` (a `std::variant`).

## 4. What the async pipeline touches

A14: only GUI-thread state is a property; pipeline state stays out. Worker code reads values captured by value
(`ViewportParams`, `std::atomic<bool> wantOrigin` in `plottable-graph.cpp:228-239`), not properties. These are the
GUI-thread settings that invalidate or re-bake a pipeline; their rows carry `async:` in sections 2 and 3.

| Not a property (pipeline state) | Why |
|---|---|
| `QCPAbstractPlottable::busy`/`setBusy`, `visuallyBusy`, `busyChanged`, `visuallyBusyChanged` | worker-driven effective state; `setBusy` is a pair with a getter today but A14 names `busy` |
| `setBusyIndicatorSymbol`, `setBusyFadeAlpha`, `setBusyShowDelayMs`, `setBusyHideDelayMs` and the four `reset*` on a plottable | per-plottable overrides of the theme, no getter; leave them on the theme |
| `QCPAsyncPipelineBase::setScheduler`, `QCPPipelineScheduler::setMaxThreads`, `QCustomPlot::setMaxPipelineThreads`, `setDataSwapDebounceMs`, `setDataSwapMaxWaitMs` | scheduler configuration, read by the swap scheduler |
| `finished(uint64_t)`, `histogramReady()`, `commitPendingData()` | pipeline events and actions |

GUI settings that feed the pipeline (property candidates with an `async:` note): `QCPAxisBase::range`, `scaleType`,
`rangeReversed`; `QCPAbstractColorMapped::dataRange`, `dataScaleType`, `gradient`; `QCPAbstractLine::lineStyle`,
`scatterStyle`, `scatterSkip`; `QCPGraph::adaptiveSampling`, `gapThreshold`, `deferredDataSwap`; `QCPColorMap2::gapThreshold`;
`QCPHistogram`/`QCPHistogram2D` bins, normalization, `columnNormalized`; `QCPWaterfallGraph` offsets/gain/normalize.

## 5. Change signals

### 5.1 Signals that exist today, per class (moc)

| Class | Signals |
|---|---|
| `QCPAlignmentGroup` | `membersChanged()` |
| `QCPAxisBase` | `rangeChanged(QCPRange,QCPRange)`, `scaleTypeChanged(QCPAxisBase::ScaleType)`, `selectionChanged()`, `selectableChanged()` |
| `QCPCoordinateSystem` | `changed()` |
| `QCustomPlot` | `mouseDoubleClick(QMouseEvent*)`, `mousePress(QMouseEvent*)`, `mouseMove(QMouseEvent*)`, `mouseRelease(QMouseEvent*)`, `mouseWheel(QWheelEvent*)`, `plottableClick(QCPAbstractPlottable*,int,QMouseEvent*)`, `plottableDoubleClick(QCPAbstractPlottable*,int,QMouseEvent*)`, `itemClick(QCPAbstractItem*,QMouseEvent*)`, `itemDoubleClick(QCPAbstractItem*,QMouseEvent*)`, `axisClick(QCPAxis*,QCPAxis::SelectablePart,QMouseEvent*)`, `axisDoubleClick(QCPAxis*,QCPAxis::SelectablePart,QMouseEvent*)`, `legendClick(QCPLegend*,QCPAbstractLegendItem*,QMouseEvent*)`, `legendDoubleClick(QCPLegend*,QCPAbstractLegendItem*,QMouseEvent*)`, `selectionChangedByUser()`, `beforeReplot()`, `afterLayout()`, `afterReplot()`, `itemCreated(QCPAbstractItem*)`, `itemCanceled()` |
| `QCPAsyncPipelineBase` | `finished(uint64_t)`, `busyChanged(bool)` |
| `QCPItemCreationState` | `itemCreated(QCPAbstractItem*)`, `itemCanceled()` |
| `QCPItemRSpan` | `keyRangeChanged(QCPRange)`, `valueRangeChanged(QCPRange)` |
| `QCPItemSpan` | `rangeChanged(QCPRange)` |
| `QCPAbstractItem` | `selectionChanged()`, `selectableChanged()`, `deleteRequested()` |
| `QCPLayerable` | `layerChanged(QCPLayer*)`, `visibleChanged(bool)` |
| `QCPColorScale` | `dataRangeChanged(QCPRange)`, `dataScaleTypeChanged(QCPAxis::ScaleType)`, `gradientChanged(QCPColorGradient)` |
| `QCPGroupLegendItem` | `componentClicked(int)` |
| `QCPAbstractLegendItem` | `selectionChanged()`, `selectableChanged()` |
| `QCPLegend` | `selectionChanged()`, `selectableChanged()` |
| `QCPTextElement` | `selectionChanged()`, `selectableChanged()`, `clicked(QMouseEvent*)`, `doubleClicked(QMouseEvent*)` |
| `QCPOverlay` | `messageChanged(QString,QCPOverlay::Level)`, `collapsedChanged(bool)` |
| `QCPLaneLayout` | `changed()` |
| `QCPAbstractColorMapped` | `dataRangeChanged(QCPRange)`, `dataScaleTypeChanged(QCPAxis::ScaleType)`, `gradientChanged(QCPColorGradient)`, `colorScaleChanged()` |
| `QCPPlottableGroup` | `membersChanged()`, `doubleClicked()` |
| `QCPHistogram` | `histogramReady()` |
| `QCPHistogram2D` | `histogramReady()` |
| `QCPIntervals` | `intervalsEdited(QList<QCPIntervalEdit>)`, `intervalCreated(double,double,int,int)`, `deleteRequested(QList<qint64>)` |
| `QCPAbstractLine` | `componentVisibilityChanged()` |
| `QCPAbstractPlottable` | `selectionChanged()`, `selectableChanged()`, `busyChanged(bool)`, `visuallyBusyChanged(bool)` |
| `QCPSelectionRect` | `started(QMouseEvent*)`, `changed(QRect,QMouseEvent*)`, `canceled(QRect,QInputEvent*)`, `accepted(QRect,QMouseEvent*)` |
| `QCPTheme` | `changed()` |

### 5.2 Property to signal

* **37 of 267 properties declare `NOTIFY`**; 230 do not. Of the 246 writable ones, 37 have it.
* Declared `NOTIFY` is always a signal the setter really emits (checked: either named in the body or reached through a
  helper: `commitRange`, `moveToLayer`, `applyDataRange`/`applyDataScaleType`/`applyGradient`, `assignSelection`).
* **Signal exists, `Q_PROPERTY` does not name it**: `QCPLayerable::visible` (`visibleChanged(bool)`, the setter emits it),
  `QCPAbstractColorMapped::colorScale` (`colorScaleChanged()`). Two wiring fixes, no new signal.
* **Signal exists, the pair has no `Q_PROPERTY`**: `QCPItemSpan::range` (`rangeChanged`), `QCPItemRSpan::keyRange`/`valueRange`
  (`keyRangeChanged`/`valueRangeChanged`), `QCPOverlay::collapsed` (`collapsedChanged`), `QCPPlottableGroup` membership
  (`membersChanged`), `QCPCoordinateSystem` parameters (`changed()`).
* **Shared signal**: the 16 `QCPTheme` properties all notify through `changed()` (no argument). Valid for moc and for
  `QBindable`; a binding observer cannot tell which value moved, it re-reads. The theme emits **directly** (`emit changed()`
  in `theme.cpp:16-91`), not through `qcpEmit`, so it does not have 4a's deferred delivery today.
* **Everything else has no change signal at all**: the 209 writable properties without one (every pen, brush, font, colour,
  margin, spacing, tick setting, label, text and flag of axes, grids, items, legends, text elements, plottables, the plot) plus
  all read-only ones. Adding the signals is the bulk of 5e: one `xChanged(T)` per property or a few shared ones per class
  (a per-class `appearanceChanged()` is legal for moc but loses the binding observer's value).
* **Read-only properties that change with no signal today**: `QCPLayer::index`, `children`; `QCPLayerable::parentPlot`,
  `parentLayerable`; `QCPLayoutElement::layout`, `rect`; `QCPLayoutGrid::rowCount`, `columnCount`; `QCPAbstractLegendItem::parentLegend`;
  `QCPBars::barBelow`, `barAbove`. `CONSTANT` would be wrong for them (B3 of the bindable review).
* Setters that emit through `qcpEmit` today (deferred, 4a): the selection setters, `visible`, `layer`, ranges, colour-scale
  signals, `collapsed`. The theme and almost everything else emit nothing. Changed deferral rule from A14: "the change
  signals keep the deferred delivery of 4a".

## 6. Macros

### 6.1 `Q_ENUM` / `Q_FLAG`

Moc registers 69 enum entries today (the `QCP` namespace carries 16: 11 `Q_ENUM_NS`, 5 `Q_FLAG_NS`, `Capability`/`Capabilities`
included: `QCP::Capabilities` is `Q_FLAG_NS` already, `global.h:493`; `Capability` has `Q_ENUM_NS`, `global.h:492`). The enums
that are public in a meta-object class and **not** registered:

| Enum | Where | Proposed | Note |
|---|---|---|---|
| `QCPIntervals::HitPart` | `plottable-intervals.h:61` | `Q_ENUM` | A14 names it; also `QCPItemSpan::HitPart` (`item-span.h:47`), `QCPItemRSpan::HitPart` (`item-rspan.h:46`) |
| `QCPIntervals::EditMode` | :70 | `Q_ENUM` | values are bit flags (0x01..0x10) |
| `QCPIntervals::EditModes` | :71 (`Q_DECLARE_FLAGS`) | `Q_FLAG` | the only unregistered flag type in the tree; `EditModes` is a property type once `editModes` is a property |
| `QCPIntervals::SnapMode` | :73 | `Q_ENUM` | A14 names it |
| `QCPIntervals::Style` | :76 | `Q_ENUM` | A14 names it; `style` is a property candidate |
| `QCPIntervals::StackMode` | :80 | `Q_ENUM` | A14 names it |
| `QCPItemSpan::HitPart`, `QCPItemRSpan::HitPart` | above | `Q_ENUM` | A14 names `HitPart` |
| `QCPPipelineScheduler::Priority` | `pipeline-scheduler.h:15` | `Q_ENUM` | public, argument of `submit`; no property needs it |
| `QCPItemCreationState::State` | `item-creation-state.h:13` | `Q_ENUM` | public; a read-only `state` would use it |
| `QCPAxisTickerDateTime::DateStrategy` | `axistickerdatetime.h:63` | out | **protected**; the class is not a gadget |
| `QCPTransform::Kind` | `coords.h:244` | gadget or out | A14: `enum class` inside a non-gadget value; needs `Q_GADGET` on `QCPTransform` or stays out |
| `QCPHitAction` | `core.h:93` | `Q_ENUM_NS` in `QCP` or out | global `enum class`, not in a meta-object scope; check whether it is public API |
| `QCPRawColumn::Type` | `abstract-datasource.h:30` | out | data-source internals, not a gadget |

Not candidates (listed so they are not rediscovered): the seven `AnchorIndex` enums of the items (`item-bracket.h:100`,
`item-ellipse.h:89`, `item-pixmap.h:95`, `item-rect.h:86`, `item-rspan.h:53`, `item-span.h:59`, `item-text.h:122`: all **protected**),
`QCPIntervals::Gesture::Kind` (protected nested), and the internal `enum class`es `TransformKind`, `QCPDrawnSide`,
`QCPReplayKinds`, `QCPReplayState`, `QCPRecordingCache::RunKind`, `QCPPlottableRhiLayer::DrawKind`, `qcp::PanBehaviour`,
`qcp::Step`, `qcp::DragKind`, `QCPEmissionScope::Kind` (not in a meta-object scope and not API).

**Redundant `Q_DECLARE_METATYPE` on registered enums.** 33 of the 37 `Q_DECLARE_METATYPE` lines (`colorgradient.h:187-189`,
`scatterstyle.h:176-177`, `axis-base.h:293-294`, `layout.h:176,333,393`, `core.h:783-784`, ...) name enums that are `Q_ENUM` (`QCPCurve::LineStyle` through `QCPAbstractLine`)
in the same class: moc already gives them a `QMetaType`, so the lines can go with the first 5e commit (their number is a
maintenance cost, not a need). The four that are **not** enums: `QCPDataSelection` (`selection.h:143`),
`QSharedPointer<QCPAxisTicker>` (`axisticker.h:99`), `QCPSelectionDecorator*` and `QCPIntervalEdit`: kept.

### 6.2 `Q_INVOKABLE` candidates

Public, non-slot, non-`const`, non-`virtual-override` named actions of `Q_OBJECT` classes. A14: no `const`, no
overloaded name without a check. "ovl" = the name is overloaded in the class (needs one decision per overload).
Return types that are `QCPHandle<T>`, the `*Ops` fluent builders or templates are **not** invokable-compatible (no
`QMetaType` for the handle, 5b/A6 decides) and are listed as `-`.

| Class | Candidates |
|---|---|
| `QCPAxisBase` | `moveRange(double)`, `scaleRange(double)` ovl, `scaleRange(double,double)` ovl, `rescale(bool)` (virtual), `unlink()` |
| `QCustomPlot` | `requestRepaint()`, `settle()`, `requestDataSwap()`, `clearPlottables()`, `clearGraphs()`, `clearItems()`, `removePlottable(ptr)` ovl, `removePlottable(int)` ovl, `removeGraph(ptr)` ovl, `removeGraph(int)` ovl, `removeItem(ptr)` ovl, `removeItem(int)` ovl, `removePlottableGroup(ptr)`, `addLayer(QString, QCPLayer*, LayerInsertMode)`, `removeLayer`, `moveLayer`, `savePdf`, `savePng`, `saveJpg`, `saveBmp`, `saveRastered` (all return `bool`; `replot`, `rescaleAxes`, `deselectAll` are slots already); `addPlottableGroup(QString)`, `addGraph(...)`, `plottable(int)` return handles/builders: `-` |
| `QCPLayer` | `replot()` |
| `QCPLayout` | `take(QCPLayoutElement*)`, `remove(QCPLayoutElement*)`, `removeAt(int)`, `clear()`, `simplify()` (virtual), `takeAt(int)` (virtual; returns a raw element pointer) |
| `QCPLayoutGrid` | `addElement(row,col,el)` ovl, `addElement(el)` ovl, `expandTo(int,int)`, `insertRow(int)`, `insertColumn(int)` |
| `QCPLayoutInset` | `addElement(el,Qt::Alignment)` ovl, `addElement(el,QRectF)` ovl |
| `QCPAxisRect` | `resetRangeDragAxes()`, `resetRangeZoomAxes()`, `removeAxis(QCPAxis*)`, `zoom(QRectF[, axes])` ovl, `panByPixels(QPointF)`, `zoomAt(QPointF, ...)`, `setupFullAxesBox(bool)`; `addAxis`/`addAxes` return handles: `-` |
| `QCPColorScale` | `rescaleDataRange(bool)` |
| `QCPAbstractColorMapped` | `rescaleDataRange(bool)` (virtual) |
| `QCPLegend` | `addItem(item)`, `removeItem(int)` ovl, `removeItem(item)` ovl, `clearItems()` |
| `QCPOverlay` | `showMessage(QString, Level)`, `clearMessage()` |
| `QCPBarsGroup` | `clear()`, `append(bars)`, `insert(int, bars)`, `remove(bars)` |
| `QCPBars` | `addData(...)` ovl x2, `moveBelow(bars)`, `moveAbove(bars)` |
| `QCPGraph` | `addData(...)` ovl x4, `removeDataBefore(double)`, `removeDataAfter(double)`, `removeData(double,double)`, `clearComponentFill(int)`, `clearComponentErrorBars(int)` |
| `QCPCurve / QCPFinancial / QCPErrorBars / QCPStatisticalBox / QCPPolarGraph` | `addData(...)` ovl on each (2 to 4 overloads, `QVector<double>` or scalars) |
| `QCPPlottableGroup` | `addMember(plottable)`, `removeMember(plottable)` |
| `QCPAbstractPlottable` | `addToLegend(QCPLegend*)` ovl (virtual), `addToLegend()` ovl; `removeFromLegend()` is `const`: excluded, `commitPendingData()` (virtual, pipeline: A14 says out), the four `resetBusy*` (out) |
| `QCPAbstractLine` | `clearColorValues()` |
| `QCPItemRichText` | `clearHtml()` |
| `QCPHistogram` | `resetEdges()` |
| `QCPPolarArea` | `addRadialAxis(axis)`, `removeRadialAxis(axis)`, `rescale(bool)` |
| `QCPAlignmentGroup` | `add(el)`, `remove(el)`; `relayoutMembers()`, `invalidate()` are internal |
| `QCPItemCreationState` | `cancel()`; the `handle*` functions are event handlers, not actions |
| `QCPSelectionRect` | `cancel()` is a slot already |

Not candidates: `addData` templates and `viewData*`/`viewRowMajorData` (templates, spans), `pipeline()` accessors, `select*`,
`mark*`, event handlers, anything `const`. Existing slots (51, already invokable) include the setters of selection and
colour-scale state, `replot`, `rescaleAxes`, `deselectAll`, `cancel`, `refreshBinning`.

### 6.3 `Q_DISABLE_COPY_MOVE` (QObject classes only)

QObject is not copyable, so the macro documents and gives a clear compile error; it matters only where a
subclass adds a user-declared copy that could be defaulted. 23 of the 68 `Q_OBJECT` classes already carry
`Q_DISABLE_COPY`: `QCPAxisBase`, `QCPAxis`, `QCPCoordinateSystem`, `QCPAbstractItem`, `QCPLayer`, `QCPLayerable`,
`QCPLayoutElement`, `QCPLayout`, `QCPLayoutGrid`, `QCPLayoutInset`, `QCPArea`, `QCPAxisRect`, `QCPColorScale`,
`QCPAbstractLegendItem`, `QCPLegend`, `QCPTextElement`, `QCPOverlay`, `QCPBarsGroup`, `QCPAbstractPlottable`,
`QCPPolarAxisAngular`, `QCPPolarArea`, `QCPPolarGrid`, `QCPPolarAxisRadial`. The other **45** have neither macro:
`QCPAlignmentGroup`, `QCPGrid`, `QCPPolarSystem`, `QCPCartesianSystem`, `QCustomPlot`, `QCPAsyncPipelineBase`,
`QCPPipelineScheduler`, `QCPItemCreationState`, the 13 item classes (`QCPItemBracket`, `Curve`, `Ellipse`, `Line`,
`Pixmap`, `Rect`, `RichText`, `RSpan`, `Span`, `AbstractSpanItem`, `StraightLine`, `Text`, `Tracer`),
`QCPColorScaleAxisRectPrivate`, `QCPGroupLegendItem`, `QCPIntervalsLegendItem`, `QCPPlottableLegendItem`,
`QCPPlottableGroupLegendItem`, `QCPLaneLayout`, `QCPBars`, `QCPColorMap`, `QCPColorMap2`, `QCPAbstractColorMapped`,
`QCPCurve`, `QCPErrorBars`, `QCPFinancial`, `QCPGraph`, `QCPPlottableGroup`, `QCPHistogram`, `QCPHistogram2D`,
`QCPIntervals`, `QCPAbstractLine`, `QCPStatisticalBox`, `QCPWaterfallGraph`, `QCPPolarGraph`, `QCPSelectionRect`,
`QCPTheme`. The existing ones use `Q_DISABLE_COPY` (copy only); A14 says `_MOVE`. 5e changes 23 lines and adds
45. **Never on the 13 `Q_GADGET` value types** (`QCPScatterStyle`, `QCPLineEnding`, `QCPColorGradient`, `QCPPainter`,
`QCPItemAnchor`/`Position`, the four tickers, `QCPLabelPainterPrivate`, `QCPSelectionDecorator`/`Bracket`): the tickers carry
`Q_DISABLE_COPY` today (they are shared by `QSharedPointer`); `QCPItemAnchor` and `QCPSelectionDecorator` are polymorphic.

### 6.4 `Q_DECLARE_TYPEINFO`

Declared today (11): `QCPVector2D`, `QCPScatterStyle`, `QCPDataRange`, `QCPLineEnding`, `QCPRange`, `QCPStatisticalBoxData`
movable; `QCPFinancialData`, `QCPGraphData`, `QCPCurveData`, `QCPErrorBarsData`, `QCPBarsData` primitive. No audit record
exists in the tree: treat all eleven as **unaudited** for A14 purposes unless 5e re-checks.
Value types without one, with what the members say about relocatability (no audit done):

| Type | File | Members (relocatability evidence) | Status |
|---|---|---|---|
| `QCPDataSelection` | `selection.h` | a `QList<QCPDataRange>`: relocatable by construction | unaudited, likely `Q_MOVABLE_TYPE`; already `Q_DECLARE_METATYPE` |
| `QCPTransform` | `coords.h:~244-319` | `std::variant<Cartesian, Polar, Custom>` (`coords.h:319`); alternatives not read | unaudited; a `std::variant` is not declared trivially relocatable by the standard |
| `QCPRatioTransform` | `coords.h:361-370` | `QPointer<QCPAxisRect>`, doubles, `QPointF`, bool: Qt movable types | unaudited, likely movable |
| `QCPDataBox` | `spatial-index.h:12` | four doubles | unaudited, likely primitive |
| `QCPKeyWindow` | `draw-context.h` | not read | unaudited |
| `QCPHit` | `core.h:100` | `QCPLayerable*` plus a `QVariant` | unaudited, likely movable |
| `QCPIntervalEdit` | `plottable-intervals.h` | doubles and ints; `Q_DECLARE_METATYPE` exists (signal argument) | unaudited, likely primitive |
| `QCPAxisLink` | `axis-base.h:42` (forward-declared; `QCPAxisFunction` is a `std::function<double(double)>`) | forward and inverse functions, shared by `std::shared_ptr` | unaudited; `std::function` relocatability is not guaranteed |
| `QCPColorGradient` (gadget) | `colorgradient.h` | `QMap`, `QColor`, `QVector`, caches | unaudited |
| `QCPScatterStyle`, `QCPLineEnding` | `scatterstyle.h`, `lineending.h` | already declared movable | declared |
| `QCPRawColumn`, `QCPStridedValues`, `QCPDataLocator`, `QCPLaneBand` | datasource / lane layout | not read | unaudited; A14: only after auditing |

### 6.5 `Q_DECLARE_METATYPE` needs

Qt 6 registers every complete copyable type automatically for `QVariant`, so the macro is needed only for names
looked up as strings (queued connections with that argument by name) and for incomplete types. Property types today
that have none and are not Qt types: `QCPRange` (3 properties; used as a signal argument), `QCPLineEnding` (6),
`QCPScatterStyle` (3), `QCPColorGradient` (2), `QSharedPointer<QCPErrorBarsDataContainer>` (1). `QCPDataSelection` and
`QSharedPointer<QCPAxisTicker>` are declared. The 19 object-typed properties follow A6; forward-declared pointer types
already use `Q_MOC_INCLUDE("core.h")` (`layer.h:40`).
A new flag property type (`EditModes`) needs its `Q_FLAG`, not a metatype line. Types that PySide must name (`QCPRange`,
`QCP::Position`, `QCPTransform`) are the open question: A14 says "where a property, `QVariant` or PySide needs it".

## 7. Open questions for the plan

1. **How much storage, in what order?** 143 properties qualify as storage candidates by body alone; 92 of them are hot or probably hot (Y/p). The plan must pick the first wave (A14: the spike three, then the theme) and bench the paint/layout ones before it adds more. Memory and layout cost per bindable member is not measured.
2. **`mCachedMarginValid = false` and the axis painter.** 17 `QCPAxis` setters are "store, mark, and invalidate the margin cache"; 12 more write into `mAxisPainter->` (a helper struct, not an own member). Are cache invalidation in the handler acceptable as "trivial", and does the painter-resident state move into the axis? (A14 says only store, mark, emit.)
3. **Hot-member layout of `QCPAxisBase`.** `mRange`, `mRangeReversed`, `mLinked`, `mRangeDraggable`, `mRangeZoomable`, `mScaleType` are pinned to one cache line (`axis-base.h:195-211`). `range`, `rangeReversed`, `scaleType` are plain already; are `rangeDraggable`/`rangeZoomable` allowed to become storage and shift that block?
4. **Shared `changed()` vs one signal per property.** The 16 theme properties share an argument-less `changed()`. Keep the pattern (cheap, binding observers re-read) or split? Same question for the roughly 200 other writable properties: per-property signals (large moc) or a few per-class signals.
5. **Theme emits directly, not deferred.** `QCPTheme` uses `emit changed()`; A14 says change signals keep 4a deferral. Does the theme get a `qcpEmit`-style scope (it has no plot of its own), or is the direct emit allowed because nothing it triggers deletes a plot?
6. **Object-typed properties (19 raw pointers, 2 shared pointers).** `axisRect`, `grid`, `plotLayout`, `parentPlot`, `layout`, `parentLegend`, `barsGroup`, `colorScale`, `selectionDecorator`, `dataPlottable`, `keyAxis`/`valueAxis`, the tracer graph, `QCustomPlot::theme`/`selectionRect` and the item position `system`/`area`: raw pointer, `QPointer`, or handle? Follows A6, but 5e is the first place a handle meets `QVariant`/moc: does `QCPHandle<T>` get a `QMetaType` (and a `Q_DECLARE_METATYPE`) or do these stay raw read-only?
7. **`Q_GADGET` property surface.** 13 gadgets, 0 properties, the 211 pairs of section 3.1 include 58 gadget pairs: tickers (shared by `QSharedPointer`, a change reaches no one), `QCPScatterStyle`, `QCPLineEnding`, `QCPColorGradient`, `QCPItemPosition`, label painter, selection decorators, `QCPPainter`. Do all of them get properties (no `NOTIFY`), only the value types users copy, or none?
8. **`QCPTransform` and `QCP::Position`.** Both are `std::variant`-based value types without any meta-object macro, and `QCPTransform::Kind` is a scoped enum inside it (A14: gadget or stays out). Is a typed-position property (`QCPItemPosition::position`) wanted, or is the item position surface the 2.x components only?
9. **The `Ops` fluent mixins.** Properties bind to `void setX(T)`; the `*Ops` twins return `T&` and duplicate each name. Does moc resolve the one it should on every property (it compiles today)? The generic test needs a decision on which overload `WRITE` means.
10. **Setters that emit through `qcpEmit` have no equality-after-`setValue` order.** For storage-backed ones A14 says always call `setValue` first. Several current setters `return` early on equality *before* anything: does 5e rewrite them all, or only the storage ones? (about 140 bodies)
11. **Missing `NOTIFY` on read-only derived properties** (`index`, `children`, `parentPlot`, `parentLayerable`, `layout`, `rect`, `rowCount`, `columnCount`, `parentLegend`, `barBelow`, `barAbove`): 11 properties change with no signal today. Add the signals, make them non-properties, or accept "derived, no notify"?
12. **Properties for the setters with several arguments or no getter** (`setTickLength(in,out)`, `setSubTickLength`, `setColor`, `setRangeLower/Upper`, `setPlottingHint`, `setInteraction`, `setCoords`, `setAxes`, ...): 101 listed in 3.2. Convention: split into one-argument properties, or leave as actions and add `Q_INVOKABLE`?
13. **The 33 redundant `Q_DECLARE_METATYPE` lines** on registered enums: delete in 5e, or keep for PySide name lookups (a PySide registration by name needs the macro in Qt 5 idiom only)?
14. **Scope of `Q_INVOKABLE`.** Several dozen named actions qualify (6.2), about a quarter of them overloaded; handle-returning ones are excluded. Which classes get it in 5e: `QCustomPlot`, layouts and legends only, or every action?
15. **`Q_DECLARE_TYPEINFO` audit.** The existing 11 declarations have no recorded audit; the candidates need one. Is the audit a 5e task or does A14 mean "only the eleven already declared, re-checked"?
16. **The generic test.** A curated fixture list for 68 `Q_OBJECT` classes with 267+ properties; `QCPAbstractColorMapped` is a `QObject` that no one constructs alone (mix-in): fixture via `QCPColorMap2`?

