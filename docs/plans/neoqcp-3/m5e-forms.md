# Milestone 5e: the form of every property

Task 1 of the 5e plan (`docs/superpowers/plans/2026-11-13-neoqcp-3-milestone-5e.md`). One row per property of the
inventory (`m5e-inventory.md`, sections 2, 3.1 and 3.3). Tasks 2 and 3 implement exactly this table.

Forms (E-0: no `BINDABLE` storage in 5e, so every "storage" row of the inventory is plain here):

- **plain**: `Q_PROPERTY(T x READ x WRITE setX NOTIFY xChanged)`, the setter emits through `qcpEmit` when the value
  moved, last thing it does; every other writer of the member emits too (E-4b). Bindable as `QBindable(obj, "x")`.
- **constant**: `CONSTANT`, no `NOTIFY` (E-11).
- **derived**: read-only, `NOTIFY` on an existing event.
- **dropped**: off the surface (or not added); the accessor stays (E-11).
- **gadget**: `Q_PROPERTY` on a `Q_GADGET`, no `NOTIFY` (E-7: only `QCPScatterStyle`, `QCPLineEnding`,
  `QCPColorGradient`).
- **out**: not a property: pipeline state, an action, or a gadget outside E-7.

The NOTIFY column gives the signal's declaration. A new signal is `xChanged(T)` carrying the new value (E-4): by
value for pointers, enums, flags and built-in types, by `const&` otherwise. "Existing" signals keep their signature.

Totals over the 477 rows of sections 2 and 3.1: plain 387, constant 3, dropped 19, gadget 16, out 52. Section 3.3 adds
15 rows.

## Section 2: the existing `Q_PROPERTY` declarations


### `QCPAxisBase` (`axis/axis-base.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `scaleType` | `ScaleType` | plain | `scaleTypeChanged(QCPAxisBase::ScaleType)` | existing NOTIFY, kept; spike (Task 1): refused by the polar angular axis |
| `range` | `QCPRange` | plain | `rangeChanged(const QCPRange&, const QCPRange&)` | existing NOTIFY, kept (new range first) |
| `rangeReversed` | `bool` | plain | `rangeReversedChanged(bool)` |  |
| `ticker` | `QSharedPointer<QCPAxisTicker>` | plain | `tickerChanged(const QSharedPointer<QCPAxisTicker>&)` |  |
| `tickVector` | `QList<double>` | dropped | - | derived from range, ticker and layout; no signal (A14) |
| `tickVectorLabels` | `QList<QString>` | dropped | - | derived from range, ticker and layout; no signal (A14) |
| `selectedParts` | `SelectableParts` | plain | `selectionChanged()` | existing NOTIFY, kept |
| `selectableParts` | `SelectableParts` | plain | `selectableChanged()` | existing NOTIFY, kept |

### `QCPGrid` (`axis/axis.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `subGridVisible` | `bool` | plain | `subGridVisibleChanged(bool)` |  |
| `antialiasedSubGrid` | `bool` | plain | `antialiasedSubGridChanged(bool)` |  |
| `antialiasedZeroLine` | `bool` | plain | `antialiasedZeroLineChanged(bool)` |  |
| `pen` | `QPen` | plain | `penChanged(const QPen&)` |  |
| `subGridPen` | `QPen` | plain | `subGridPenChanged(const QPen&)` |  |
| `zeroLinePen` | `QPen` | plain | `zeroLinePenChanged(const QPen&)` |  |

### `QCPAxis` (`axis/axis.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `axisType` | `AxisType` | constant | - | fixed at construction (E-11) |
| `axisRect` | `QCPAxisRect*` | dropped | - | read-only, no signal (E-11 keeps CONSTANT to its three); accessor stays |
| `ticks` | `bool` | plain | `ticksChanged(bool)` |  |
| `tickLabels` | `bool` | plain | `tickLabelsChanged(bool)` |  |
| `tickLabelPadding` | `int` | plain | `tickLabelPaddingChanged(int)` |  |
| `tickLabelFont` | `QFont` | plain | `tickLabelFontChanged(const QFont&)` |  |
| `tickLabelColor` | `QColor` | plain | `tickLabelColorChanged(const QColor&)` |  |
| `tickLabelRotation` | `double` | plain | `tickLabelRotationChanged(double)` |  |
| `tickLabelSide` | `LabelSide` | plain | `tickLabelSideChanged(LabelSide)` |  |
| `numberFormat` | `QString` | plain | `numberFormatChanged(const QString&)` |  |
| `numberPrecision` | `int` | plain | `numberPrecisionChanged(int)` |  |
| `tickLengthIn` | `int` | plain | `tickLengthInChanged(int)` |  |
| `tickLengthOut` | `int` | plain | `tickLengthOutChanged(int)` |  |
| `subTicks` | `bool` | plain | `subTicksChanged(bool)` |  |
| `subTickLengthIn` | `int` | plain | `subTickLengthInChanged(int)` |  |
| `subTickLengthOut` | `int` | plain | `subTickLengthOutChanged(int)` |  |
| `basePen` | `QPen` | plain | `basePenChanged(const QPen&)` |  |
| `tickPen` | `QPen` | plain | `tickPenChanged(const QPen&)` |  |
| `subTickPen` | `QPen` | plain | `subTickPenChanged(const QPen&)` |  |
| `labelFont` | `QFont` | plain | `labelFontChanged(const QFont&)` |  |
| `labelColor` | `QColor` | plain | `labelColorChanged(const QColor&)` |  |
| `label` | `QString` | plain | `labelChanged(const QString&)` | spike (Task 1) |
| `labelPadding` | `int` | plain | `labelPaddingChanged(int)` |  |
| `padding` | `int` | plain | `paddingChanged(int)` |  |
| `offset` | `int` | plain | `offsetChanged(int)` |  |
| `selectedTickLabelFont` | `QFont` | plain | `selectedTickLabelFontChanged(const QFont&)` |  |
| `selectedLabelFont` | `QFont` | plain | `selectedLabelFontChanged(const QFont&)` |  |
| `selectedTickLabelColor` | `QColor` | plain | `selectedTickLabelColorChanged(const QColor&)` |  |
| `selectedLabelColor` | `QColor` | plain | `selectedLabelColorChanged(const QColor&)` |  |
| `selectedBasePen` | `QPen` | plain | `selectedBasePenChanged(const QPen&)` |  |
| `selectedTickPen` | `QPen` | plain | `selectedTickPenChanged(const QPen&)` |  |
| `selectedSubTickPen` | `QPen` | plain | `selectedSubTickPenChanged(const QPen&)` |  |
| `lowerEnding` | `QCPLineEnding` | plain | `lowerEndingChanged(const QCPLineEnding&)` |  |
| `upperEnding` | `QCPLineEnding` | plain | `upperEndingChanged(const QCPLineEnding&)` |  |
| `grid` | `QCPGrid*` | dropped | - | remade when deleted (`axis.cpp:521-530`): not constant, no signal |

### `QCustomPlot` (`core.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `viewport` | `QRect` | plain | `viewportChanged(const QRect&)` |  |
| `background` | `QPixmap` | plain | `backgroundChanged(const QPixmap&)` |  |
| `backgroundScaled` | `bool` | plain | `backgroundScaledChanged(bool)` |  |
| `backgroundScaledMode` | `Qt::AspectRatioMode` | plain | `backgroundScaledModeChanged(Qt::AspectRatioMode)` |  |
| `plotLayout` | `QCPLayoutGrid*` | dropped | - | remade when deleted (`core.cpp:618-623`): not constant, no signal |
| `autoAddPlottableToLegend` | `bool` | plain | `autoAddPlottableToLegendChanged(bool)` |  |
| `selectionTolerance` | `int` | plain | `selectionToleranceChanged(int)` |  |
| `noAntialiasingOnDrag` | `bool` | plain | `noAntialiasingOnDragChanged(bool)` |  |
| `multiSelectModifier` | `Qt::KeyboardModifier` | plain | `multiSelectModifierChanged(Qt::KeyboardModifier)` |  |
| `themeBackground` | `QColor` | plain | `themeChanged()` | E-5b: kept for stylesheet theming; one shared signal, emitted by setTheme and the theme relay (`core.cpp:656`) |
| `themeForeground` | `QColor` | plain | `themeChanged()` | E-5b: kept for stylesheet theming; one shared signal, emitted by setTheme and the theme relay (`core.cpp:656`) |
| `themeGrid` | `QColor` | plain | `themeChanged()` | E-5b: kept for stylesheet theming; one shared signal, emitted by setTheme and the theme relay (`core.cpp:656`) |
| `themeSubGrid` | `QColor` | plain | `themeChanged()` | E-5b: kept for stylesheet theming; one shared signal, emitted by setTheme and the theme relay (`core.cpp:656`) |
| `themeSelection` | `QColor` | plain | `themeChanged()` | E-5b: kept for stylesheet theming; one shared signal, emitted by setTheme and the theme relay (`core.cpp:656`) |
| `themeLegendBackground` | `QColor` | plain | `themeChanged()` | E-5b: kept for stylesheet theming; one shared signal, emitted by setTheme and the theme relay (`core.cpp:656`) |
| `themeLegendBorder` | `QColor` | plain | `themeChanged()` | E-5b: kept for stylesheet theming; one shared signal, emitted by setTheme and the theme relay (`core.cpp:656`) |

### `QCPItemBracket` (`items/item-bracket.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `pen` | `QPen` | plain | `penChanged(const QPen&)` |  |
| `selectedPen` | `QPen` | plain | `selectedPenChanged(const QPen&)` |  |
| `length` | `double` | plain | `lengthChanged(double)` |  |
| `style` | `BracketStyle` | plain | `styleChanged(BracketStyle)` |  |

### `QCPItemCurve` (`items/item-curve.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `pen` | `QPen` | plain | `penChanged(const QPen&)` |  |
| `selectedPen` | `QPen` | plain | `selectedPenChanged(const QPen&)` |  |
| `head` | `QCPLineEnding` | plain | `headChanged(const QCPLineEnding&)` |  |
| `tail` | `QCPLineEnding` | plain | `tailChanged(const QCPLineEnding&)` |  |

### `QCPItemEllipse` (`items/item-ellipse.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `pen` | `QPen` | plain | `penChanged(const QPen&)` |  |
| `selectedPen` | `QPen` | plain | `selectedPenChanged(const QPen&)` |  |
| `brush` | `QBrush` | plain | `brushChanged(const QBrush&)` |  |
| `selectedBrush` | `QBrush` | plain | `selectedBrushChanged(const QBrush&)` |  |

### `QCPItemLine` (`items/item-line.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `pen` | `QPen` | plain | `penChanged(const QPen&)` |  |
| `selectedPen` | `QPen` | plain | `selectedPenChanged(const QPen&)` |  |
| `head` | `QCPLineEnding` | plain | `headChanged(const QCPLineEnding&)` |  |
| `tail` | `QCPLineEnding` | plain | `tailChanged(const QCPLineEnding&)` |  |

### `QCPItemPixmap` (`items/item-pixmap.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `pixmap` | `QPixmap` | plain | `pixmapChanged(const QPixmap&)` |  |
| `scaled` | `bool` | plain | `scaledChanged(bool)` |  |
| `aspectRatioMode` | `Qt::AspectRatioMode` | dropped | - | set only by setScaled(scaled, mode, mode); E-12 does not split it, so read-only without a signal (E-11) |
| `transformationMode` | `Qt::TransformationMode` | dropped | - | set only by setScaled(scaled, mode, mode); E-12 does not split it, so read-only without a signal (E-11) |
| `pen` | `QPen` | plain | `penChanged(const QPen&)` |  |
| `selectedPen` | `QPen` | plain | `selectedPenChanged(const QPen&)` |  |

### `QCPItemRect` (`items/item-rect.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `pen` | `QPen` | plain | `penChanged(const QPen&)` |  |
| `selectedPen` | `QPen` | plain | `selectedPenChanged(const QPen&)` |  |
| `brush` | `QBrush` | plain | `brushChanged(const QBrush&)` |  |
| `selectedBrush` | `QBrush` | plain | `selectedBrushChanged(const QBrush&)` |  |

### `QCPItemRichText` (`items/item-richtext.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `html` | `QString` | plain | `htmlChanged(const QString&)` |  |

### `QCPItemRSpan` (`items/item-rspan.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `brush` | `QBrush` | plain | `brushChanged(const QBrush&)` |  |
| `selectedBrush` | `QBrush` | plain | `selectedBrushChanged(const QBrush&)` |  |
| `borderPen` | `QPen` | plain | `borderPenChanged(const QPen&)` |  |
| `selectedBorderPen` | `QPen` | plain | `selectedBorderPenChanged(const QPen&)` |  |
| `movable` | `bool` | plain | `movableChanged(bool)` |  |

### `QCPItemSpan` (`items/item-span.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `brush` | `QBrush` | plain | `brushChanged(const QBrush&)` |  |
| `selectedBrush` | `QBrush` | plain | `selectedBrushChanged(const QBrush&)` |  |
| `borderPen` | `QPen` | plain | `borderPenChanged(const QPen&)` |  |
| `selectedBorderPen` | `QPen` | plain | `selectedBorderPenChanged(const QPen&)` |  |
| `movable` | `bool` | plain | `movableChanged(bool)` |  |

### `QCPItemStraightLine` (`items/item-straightline.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `pen` | `QPen` | plain | `penChanged(const QPen&)` |  |
| `selectedPen` | `QPen` | plain | `selectedPenChanged(const QPen&)` |  |

### `QCPItemText` (`items/item-text.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `color` | `QColor` | plain | `colorChanged(const QColor&)` |  |
| `selectedColor` | `QColor` | plain | `selectedColorChanged(const QColor&)` |  |
| `pen` | `QPen` | plain | `penChanged(const QPen&)` |  |
| `selectedPen` | `QPen` | plain | `selectedPenChanged(const QPen&)` |  |
| `brush` | `QBrush` | plain | `brushChanged(const QBrush&)` |  |
| `selectedBrush` | `QBrush` | plain | `selectedBrushChanged(const QBrush&)` |  |
| `font` | `QFont` | plain | `fontChanged(const QFont&)` |  |
| `selectedFont` | `QFont` | plain | `selectedFontChanged(const QFont&)` |  |
| `text` | `QString` | plain | `textChanged(const QString&)` |  |
| `positionAlignment` | `Qt::Alignment` | plain | `positionAlignmentChanged(Qt::Alignment)` |  |
| `textAlignment` | `Qt::Alignment` | plain | `textAlignmentChanged(Qt::Alignment)` |  |
| `rotation` | `double` | plain | `rotationChanged(double)` |  |
| `padding` | `QMargins` | plain | `paddingChanged(const QMargins&)` |  |

### `QCPItemTracer` (`items/item-tracer.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `pen` | `QPen` | plain | `penChanged(const QPen&)` |  |
| `selectedPen` | `QPen` | plain | `selectedPenChanged(const QPen&)` |  |
| `brush` | `QBrush` | plain | `brushChanged(const QBrush&)` |  |
| `selectedBrush` | `QBrush` | plain | `selectedBrushChanged(const QBrush&)` |  |
| `size` | `double` | plain | `sizeChanged(double)` |  |
| `style` | `TracerStyle` | plain | `styleChanged(TracerStyle)` |  |
| `graph` | `QCPGraph*` | plain | `graphChanged(QCPGraph*)` |  |
| `graphKey` | `double` | plain | `graphKeyChanged(double)` |  |
| `interpolating` | `bool` | plain | `interpolatingChanged(bool)` |  |

### `QCPAbstractItem` (`items/item.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `clipToAxisRect` | `bool` | plain | `clipToAxisRectChanged(bool)` |  |
| `clipAxisRect` | `QCPAxisRect*` | plain | `clipAxisRectChanged(QCPAxisRect*)` |  |
| `selectable` | `QCP::SelectionType` | plain | `selectableChanged()` | existing NOTIFY, kept |
| `selected` | `bool` | plain | `selectionChanged()` | existing NOTIFY, kept |

### `QCPLayer` (`layer.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `parentPlot` | `QCustomPlot*` | constant | - | set at creation (E-11) |
| `name` | `QString` | constant | - | set at creation (E-11) |
| `index` | `int` | dropped | - | read-only, moves with no signal (E-11, review I1); accessor stays; test-handle ported (I7) |
| `children` | `QList<QCPLayerable*>` | dropped | - | read-only, moves with no signal (E-11, review I1); accessor stays; test-handle ported (I7) |
| `visible` | `bool` | plain | `visibleChanged(bool)` |  |
| `mode` | `LayerMode` | plain | `modeChanged(LayerMode)` |  |

### `QCPLayerable` (`layer.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `visible` | `bool` | plain | `visibleChanged(bool)` | existing signal, wired as NOTIFY (inventory 5.2) |
| `parentPlot` | `QCustomPlot*` | dropped | - | read-only, moves with no signal (E-11, review I1); accessor stays; test-handle ported (I7) |
| `parentLayerable` | `QCPLayerable*` | dropped | - | read-only, moves with no signal (E-11, review I1); accessor stays; test-handle ported (I7) |
| `layer` | `QCPLayer*` | plain | `layerChanged(QCPLayer*)` | existing NOTIFY, kept |
| `antialiased` | `bool` | plain | `antialiasedChanged(bool)` |  |
| `mouseTransparent` | `bool` | plain | `mouseTransparentChanged(bool)` |  |

### `QCPLayoutElement` (`layout.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `layout` | `QCPLayout*` | dropped | - | read-only, moves with no signal (E-11, review I1); accessor stays; test-handle ported (I7) |
| `rect` | `QRect` | dropped | - | read-only, moves with no signal (E-11, review I1); accessor stays; test-handle ported (I7) |
| `outerRect` | `QRect` | plain | `outerRectChanged(const QRect&)` |  |
| `margins` | `QMargins` | plain | `marginsChanged(const QMargins&)` |  |
| `minimumMargins` | `QMargins` | plain | `minimumMarginsChanged(const QMargins&)` |  |
| `minimumSize` | `QSize` | plain | `minimumSizeChanged(const QSize&)` |  |
| `maximumSize` | `QSize` | plain | `maximumSizeChanged(const QSize&)` |  |
| `sizeConstraintRect` | `SizeConstraintRect` | plain | `sizeConstraintRectChanged(SizeConstraintRect)` |  |

### `QCPLayoutGrid` (`layout.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `rowCount` | `int` | dropped | - | read-only, moves with no signal (E-11, review I1); accessor stays; test-handle ported (I7) |
| `columnCount` | `int` | dropped | - | read-only, moves with no signal (E-11, review I1); accessor stays; test-handle ported (I7) |
| `columnStretchFactors` | `QList<double>` | plain | `columnStretchFactorsChanged(const QList<double>&)` |  |
| `rowStretchFactors` | `QList<double>` | plain | `rowStretchFactorsChanged(const QList<double>&)` |  |
| `columnSpacing` | `int` | plain | `columnSpacingChanged(int)` |  |
| `rowSpacing` | `int` | plain | `rowSpacingChanged(int)` |  |
| `fillOrder` | `FillOrder` | plain | `fillOrderChanged(FillOrder)` |  |
| `wrap` | `int` | plain | `wrapChanged(int)` |  |

### `QCPArea` (`layoutelements/area.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `background` | `QPixmap` | plain | `backgroundChanged(const QPixmap&)` |  |
| `backgroundScaled` | `bool` | plain | `backgroundScaledChanged(bool)` |  |
| `backgroundScaledMode` | `Qt::AspectRatioMode` | plain | `backgroundScaledModeChanged(Qt::AspectRatioMode)` |  |

### `QCPAxisRect` (`layoutelements/layoutelement-axisrect.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `rangeDrag` | `Qt::Orientations` | plain | `rangeDragChanged(Qt::Orientations)` |  |
| `rangeZoom` | `Qt::Orientations` | plain | `rangeZoomChanged(Qt::Orientations)` |  |

### `QCPColorScale` (`layoutelements/layoutelement-colorscale.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `type` | `QCPAxis::AxisType` | plain | `typeChanged(QCPAxis::AxisType)` |  |
| `dataRange` | `QCPRange` | plain | `dataRangeChanged(const QCPRange&)` | existing NOTIFY, kept |
| `dataScaleType` | `QCPAxis::ScaleType` | plain | `dataScaleTypeChanged(QCPAxis::ScaleType)` | existing NOTIFY, kept |
| `gradient` | `QCPColorGradient` | plain | `gradientChanged(const QCPColorGradient&)` | existing NOTIFY, kept |
| `label` | `QString` | plain | `labelChanged(const QString&)` |  |
| `barWidth` | `int` | plain | `barWidthChanged(int)` |  |
| `rangeDrag` | `bool` | plain | `rangeDragChanged(bool)` |  |
| `rangeZoom` | `bool` | plain | `rangeZoomChanged(bool)` |  |

### `QCPAbstractLegendItem` (`layoutelements/layoutelement-legend.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `parentLegend` | `QCPLegend*` | dropped | - | read-only, moves with no signal (E-11, review I1); accessor stays; test-handle ported (I7) |
| `font` | `QFont` | plain | `fontChanged(const QFont&)` |  |
| `textColor` | `QColor` | plain | `textColorChanged(const QColor&)` |  |
| `selectedFont` | `QFont` | plain | `selectedFontChanged(const QFont&)` |  |
| `selectedTextColor` | `QColor` | plain | `selectedTextColorChanged(const QColor&)` |  |
| `selectable` | `QCP::SelectionType` | plain | `selectableChanged()` | existing NOTIFY, kept |
| `selected` | `bool` | plain | `selectionChanged()` | existing NOTIFY, kept |

### `QCPLegend` (`layoutelements/layoutelement-legend.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `borderPen` | `QPen` | plain | `borderPenChanged(const QPen&)` |  |
| `brush` | `QBrush` | plain | `brushChanged(const QBrush&)` |  |
| `font` | `QFont` | plain | `fontChanged(const QFont&)` |  |
| `textColor` | `QColor` | plain | `textColorChanged(const QColor&)` |  |
| `iconSize` | `QSize` | plain | `iconSizeChanged(const QSize&)` |  |
| `iconTextPadding` | `int` | plain | `iconTextPaddingChanged(int)` |  |
| `iconBorderPen` | `QPen` | plain | `iconBorderPenChanged(const QPen&)` |  |
| `selectableParts` | `SelectableParts` | plain | `selectableChanged()` | existing NOTIFY, kept |
| `selectedParts` | `SelectableParts` | plain | `selectionChanged()` | existing NOTIFY, kept |
| `selectedBorderPen` | `QPen` | plain | `selectedBorderPenChanged(const QPen&)` |  |
| `selectedIconBorderPen` | `QPen` | plain | `selectedIconBorderPenChanged(const QPen&)` |  |
| `selectedBrush` | `QBrush` | plain | `selectedBrushChanged(const QBrush&)` |  |
| `selectedFont` | `QFont` | plain | `selectedFontChanged(const QFont&)` |  |
| `selectedTextColor` | `QColor` | plain | `selectedTextColorChanged(const QColor&)` |  |

### `QCPTextElement` (`layoutelements/layoutelement-textelement.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `text` | `QString` | plain | `textChanged(const QString&)` |  |
| `font` | `QFont` | plain | `fontChanged(const QFont&)` |  |
| `textColor` | `QColor` | plain | `textColorChanged(const QColor&)` |  |
| `selectedFont` | `QFont` | plain | `selectedFontChanged(const QFont&)` |  |
| `selectedTextColor` | `QColor` | plain | `selectedTextColorChanged(const QColor&)` |  |
| `selectable` | `QCP::SelectionType` | plain | `selectableChanged()` | existing NOTIFY, kept |
| `selected` | `bool` | plain | `selectionChanged()` | existing NOTIFY, kept |

### `QCPBarsGroup` (`plottables/plottable-bars.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `spacingType` | `SpacingType` | plain | `spacingTypeChanged(SpacingType)` |  |
| `spacing` | `double` | plain | `spacingChanged(double)` |  |

### `QCPBars` (`plottables/plottable-bars.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `width` | `double` | plain | `widthChanged(double)` |  |
| `widthType` | `WidthType` | plain | `widthTypeChanged(WidthType)` |  |
| `barsGroup` | `QCPBarsGroup*` | plain | `barsGroupChanged(QCPBarsGroup*)` |  |
| `baseValue` | `double` | plain | `baseValueChanged(double)` |  |
| `stackingGap` | `double` | plain | `stackingGapChanged(double)` |  |
| `barBelow` | `QCPBars*` | dropped | - | read-only, moves with no signal (E-11, review I1); accessor stays; test-handle ported (I7) |
| `barAbove` | `QCPBars*` | dropped | - | read-only, moves with no signal (E-11, review I1); accessor stays; test-handle ported (I7) |

### `QCPColorMap` (`plottables/plottable-colormap.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `interpolate` | `bool` | plain | `interpolateChanged(bool)` |  |
| `tightBoundary` | `bool` | plain | `tightBoundaryChanged(bool)` |  |

### `QCPAbstractColorMapped` (`plottables/plottable-colormapped.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `dataRange` | `QCPRange` | plain | `dataRangeChanged(const QCPRange&)` | existing NOTIFY, kept |
| `dataScaleType` | `QCPAxis::ScaleType` | plain | `dataScaleTypeChanged(QCPAxis::ScaleType)` | existing NOTIFY, kept |
| `gradient` | `QCPColorGradient` | plain | `gradientChanged(const QCPColorGradient&)` | existing NOTIFY, kept |
| `colorScale` | `QCPColorScale*` | plain | `colorScaleChanged()` | existing signal, wired as NOTIFY (inventory 5.2) |

### `QCPCurve` (`plottables/plottable-curve.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `scatterStyle` | `QCPScatterStyle` | plain | `scatterStyleChanged(const QCPScatterStyle&)` |  |
| `scatterSkip` | `int` | plain | `scatterSkipChanged(int)` |  |
| `lineStyle` | `LineStyle` | plain | `lineStyleChanged(LineStyle)` |  |

### `QCPErrorBars` (`plottables/plottable-errorbar.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `data` | `QSharedPointer<QCPErrorBarsDataContainer>` | dropped | - | a shared data container: mutated in place with no signal, swapped by setData; `dataChanged` is taken by plottable hooks |
| `dataPlottable` | `QCPAbstractPlottable*` | plain | `dataPlottableChanged(QCPAbstractPlottable*)` |  |
| `errorType` | `ErrorType` | plain | `errorTypeChanged(ErrorType)` |  |
| `whiskerWidth` | `double` | plain | `whiskerWidthChanged(double)` |  |
| `symbolGap` | `double` | plain | `symbolGapChanged(double)` |  |

### `QCPFinancial` (`plottables/plottable-financial.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `chartStyle` | `ChartStyle` | plain | `chartStyleChanged(ChartStyle)` |  |
| `width` | `double` | plain | `widthChanged(double)` |  |
| `widthType` | `WidthType` | plain | `widthTypeChanged(WidthType)` |  |
| `twoColored` | `bool` | plain | `twoColoredChanged(bool)` |  |
| `brushPositive` | `QBrush` | plain | `brushPositiveChanged(const QBrush&)` |  |
| `brushNegative` | `QBrush` | plain | `brushNegativeChanged(const QBrush&)` |  |
| `penPositive` | `QPen` | plain | `penPositiveChanged(const QPen&)` |  |
| `penNegative` | `QPen` | plain | `penNegativeChanged(const QPen&)` |  |

### `QCPGraph` (`plottables/plottable-graph.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `adaptiveSampling` | `bool` | plain | `adaptiveSamplingChanged(bool)` |  |

### `QCPAbstractLine` (`plottables/plottable-line.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `lineStyle` | `LineStyle` | plain | `lineStyleChanged(LineStyle)` |  |
| `scatterStyle` | `QCPScatterStyle` | plain | `scatterStyleChanged(const QCPScatterStyle&)` |  |
| `scatterSkip` | `int` | plain | `scatterSkipChanged(int)` |  |

### `QCPStatisticalBox` (`plottables/plottable-statisticalbox.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `width` | `double` | plain | `widthChanged(double)` |  |
| `whiskerWidth` | `double` | plain | `whiskerWidthChanged(double)` |  |
| `whiskerPen` | `QPen` | plain | `whiskerPenChanged(const QPen&)` |  |
| `whiskerBarPen` | `QPen` | plain | `whiskerBarPenChanged(const QPen&)` |  |
| `whiskerAntialiased` | `bool` | plain | `whiskerAntialiasedChanged(bool)` |  |
| `medianPen` | `QPen` | plain | `medianPenChanged(const QPen&)` |  |
| `outlierStyle` | `QCPScatterStyle` | plain | `outlierStyleChanged(const QCPScatterStyle&)` |  |

### `QCPAbstractPlottable` (`plottables/plottable.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `name` | `QString` | plain | `nameChanged(const QString&)` |  |
| `antialiasedFill` | `bool` | plain | `antialiasedFillChanged(bool)` |  |
| `antialiasedScatters` | `bool` | plain | `antialiasedScattersChanged(bool)` |  |
| `pen` | `QPen` | plain | `penChanged(const QPen&)` | spike (Task 1); virtual, emitted by the base and by QCPAbstractLine only when pen() moves |
| `brush` | `QBrush` | plain | `brushChanged(const QBrush&)` |  |
| `keyAxis` | `QCPAxis*` | plain | `keyAxisChanged(QCPAxis*)` |  |
| `valueAxis` | `QCPAxis*` | plain | `valueAxisChanged(QCPAxis*)` |  |
| `selectable` | `QCP::SelectionType` | plain | `selectableChanged()` | existing NOTIFY, kept |
| `selection` | `QCPDataSelection` | plain | `selectionChanged()` | existing NOTIFY, kept |
| `selectionDecorator` | `QCPSelectionDecorator*` | plain | `selectionDecoratorChanged(QCPSelectionDecorator*)` |  |

### `QCPTheme` (`theme.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `background` | `QColor` | plain | `changed()` | E-4, E-5: the single changed(), emitted directly |
| `foreground` | `QColor` | plain | `changed()` | E-4, E-5: the single changed(), emitted directly |
| `grid` | `QColor` | plain | `changed()` | E-4, E-5: the single changed(), emitted directly |
| `subGrid` | `QColor` | plain | `changed()` | E-4, E-5: the single changed(), emitted directly |
| `selection` | `QColor` | plain | `changed()` | E-4, E-5: the single changed(), emitted directly |
| `legendBackground` | `QColor` | plain | `changed()` | E-4, E-5: the single changed(), emitted directly |
| `legendBorder` | `QColor` | plain | `changed()` | E-4, E-5: the single changed(), emitted directly |
| `tooltipBackground` | `QColor` | plain | `changed()` | E-4, E-5: the single changed(), emitted directly |
| `tooltipBorder` | `QColor` | plain | `changed()` | E-4, E-5: the single changed(), emitted directly |
| `tooltipText` | `QColor` | plain | `changed()` | E-4, E-5: the single changed(), emitted directly |
| `tooltipCornerRadius` | `qreal` | plain | `changed()` | E-4, E-5: the single changed(), emitted directly |
| `crosshairColor` | `QColor` | plain | `changed()` | E-4, E-5: the single changed(), emitted directly |
| `busyIndicatorSymbol` | `QString` | plain | `changed()` | E-4, E-5: the single changed(), emitted directly |
| `busyFadeAlpha` | `qreal` | plain | `changed()` | E-4, E-5: the single changed(), emitted directly |
| `busyShowDelayMs` | `int` | plain | `changed()` | E-4, E-5: the single changed(), emitted directly |
| `busyHideDelayMs` | `int` | plain | `changed()` | E-4, E-5: the single changed(), emitted directly |

## Section 3.1: the one-argument pairs without a property


### `QCPAxisBase` (`axis/axis-base.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `rangeDraggable` | `bool` | plain | `rangeDraggableChanged(bool)` |  |
| `rangeZoomable` | `bool` | plain | `rangeZoomableChanged(bool)` |  |

### `QCPAxis` (`axis/axis.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `labelRenderer` | `QCPLabelRenderer*` | plain | `labelRendererChanged(QCPLabelRenderer*)` |  |

### `QCPAxisTicker` (`axis/axisticker.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `tickStepStrategy` | `TickStepStrategy` | out | - | gadget properties only on the three value types (E-7) |
| `tickCount` | `int` | out | - | gadget properties only on the three value types (E-7) |
| `tickOrigin` | `double` | out | - | gadget properties only on the three value types (E-7) |

### `QCPAxisTickerFixed` (`axis/axistickerfixed.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `tickStep` | `double` | out | - | gadget properties only on the three value types (E-7) |
| `scaleStrategy` | `ScaleStrategy` | out | - | gadget properties only on the three value types (E-7) |

### `QCPAxisTickerPi` (`axis/axistickerpi.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `piSymbol` | `QString` | out | - | gadget properties only on the three value types (E-7) |
| `piValue` | `double` | out | - | gadget properties only on the three value types (E-7) |
| `periodicity` | `int` | out | - | gadget properties only on the three value types (E-7) |
| `fractionStyle` | `FractionStyle` | out | - | gadget properties only on the three value types (E-7) |

### `QCPAxisTickerTime` (`axis/axistickertime.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `timeFormat` | `QString` | out | - | gadget properties only on the three value types (E-7) |

### `QCPLabelPainterPrivate` (`axis/labelpainter.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `anchorSide` | `AnchorSide` | out | - | gadget properties only on the three value types (E-7) |
| `anchorMode` | `AnchorMode` | out | - | gadget properties only on the three value types (E-7) |
| `anchorReference` | `QPointF` | out | - | gadget properties only on the three value types (E-7) |
| `anchorReferenceType` | `AnchorReferenceType` | out | - | gadget properties only on the three value types (E-7) |
| `font` | `QFont` | out | - | gadget properties only on the three value types (E-7) |
| `color` | `QColor` | out | - | gadget properties only on the three value types (E-7) |
| `padding` | `int` | out | - | gadget properties only on the three value types (E-7) |
| `rotation` | `double` | out | - | gadget properties only on the three value types (E-7) |
| `substituteExponent` | `bool` | out | - | gadget properties only on the three value types (E-7) |
| `multiplicationSymbol` | `QChar` | out | - | gadget properties only on the three value types (E-7) |
| `abbreviateDecimalPowers` | `bool` | out | - | gadget properties only on the three value types (E-7) |
| `cacheSize` | `int` | out | - | gadget properties only on the three value types (E-7) |

### `QCPColorGradient` (`colorgradient.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `levelCount` | `int` | gadget | - |  |
| `colorStops` | `QMap<double, QColor>` | gadget | - |  |
| `colorInterpolation` | `ColorInterpolation` | gadget | - |  |
| `nanHandling` | `NanHandling` | gadget | - |  |
| `nanColor` | `QColor` | gadget | - |  |
| `periodic` | `bool` | gadget | - |  |

### `QCustomPlot` (`core.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `itemCreator` | `ItemCreator` | out | - | a std::function: no useful property type |
| `itemPositioner` | `ItemPositioner` | out | - | a std::function: no useful property type |
| `creationModeEnabled` | `bool` | plain | `creationModeEnabledChanged(bool)` |  |
| `creationModifier` | `Qt::KeyboardModifier` | plain | `creationModifierChanged(Qt::KeyboardModifier)` |  |
| `bufferDevicePixelRatio` | `double` | plain | `bufferDevicePixelRatioChanged(double)` |  |
| `antialiasedElements` | `QCP::AntialiasedElements` | plain | `antialiasedElementsChanged(QCP::AntialiasedElements)` |  |
| `notAntialiasedElements` | `QCP::AntialiasedElements` | plain | `notAntialiasedElementsChanged(QCP::AntialiasedElements)` |  |
| `interactions` | `QCP::Interactions` | plain | `interactionsChanged(QCP::Interactions)` |  |
| `plottingHints` | `QCP::PlottingHints` | plain | `plottingHintsChanged(QCP::PlottingHints)` |  |
| `selectionRectMode` | `QCP::SelectionRectMode` | plain | `selectionRectModeChanged(QCP::SelectionRectMode)` |  |
| `selectionRect` | `QCPSelectionRect*` | plain | `selectionRectChanged(QCPSelectionRect*)` |  |
| `theme` | `QCPTheme*` | plain | `themeChanged()` | E-5b: the same shared signal; object-typed, READ the public getter (E-6) |
| `skipReplotsWhenHidden` | `bool` | plain | `skipReplotsWhenHiddenChanged(bool)` |  |
| `itemIndexing` | `bool` | plain | `itemIndexingChanged(bool)` |  |
| `dataSwapDebounceMs` | `int` | plain | `dataSwapDebounceMsChanged(int)` |  |
| `dataSwapMaxWaitMs` | `int` | plain | `dataSwapMaxWaitMsChanged(int)` |  |
| `currentLayer` | `QCPLayer*` | plain | `currentLayerChanged(QCPLayer*)` | one property, typed QCPLayer* (E-6); the QString overload of the setter stays C++ |

### `QCPAsyncPipelineBase` (`datasource/async-pipeline.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `scheduler` | `QCPPipelineScheduler*` | out | - | pipeline state (A14) |

### `QCPPipelineScheduler` (`datasource/pipeline-scheduler.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `maxThreads` | `int` | out | - | pipeline configuration (A14) |

### `QCPItemRichText` (`items/item-richtext.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `cornerRadius` | `double` | plain | `cornerRadiusChanged(double)` |  |

### `QCPItemRSpan` (`items/item-rspan.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `keyRange` | `QCPRange` | plain | `keyRangeChanged(const QCPRange&)` | existing signal |
| `valueRange` | `QCPRange` | plain | `valueRangeChanged(const QCPRange&)` | existing signal |

### `QCPItemSpan` (`items/item-span.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `range` | `QCPRange` | plain | `rangeChanged(const QCPRange&)` | existing signal |

### `QCPAbstractSpanItem` (`items/item-spanbase.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `brush` | `QBrush` | plain | `brushChanged(const QBrush&)` |  |
| `selectedBrush` | `QBrush` | plain | `selectedBrushChanged(const QBrush&)` |  |
| `borderPen` | `QPen` | plain | `borderPenChanged(const QPen&)` |  |
| `selectedBorderPen` | `QPen` | plain | `selectedBorderPenChanged(const QPen&)` |  |
| `movable` | `bool` | plain | `movableChanged(bool)` |  |

### `QCPItemTracer` (`items/item-tracer.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `component` | `int` | plain | `componentChanged(int)` |  |

### `QCPItemPosition` (`items/item.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `type` | `PositionType` | out | - | E-7, E-8: the position stays off the surface |
| `typeX` | `PositionType` | out | - | E-7, E-8: the position stays off the surface |
| `typeY` | `PositionType` | out | - | E-7, E-8: the position stays off the surface |
| `coords` | `QPointF` | out | - | E-7, E-8: the position stays off the surface |
| `axisRect` | `QCPAxisRect*` | out | - | E-7, E-8: the position stays off the surface |
| `system` | `QCPCoordinateSystem*` | out | - | E-7, E-8: the position stays off the surface |
| `area` | `QCPArea*` | out | - | E-7, E-8: the position stays off the surface |
| `pixelPosition` | `QPointF` | out | - | E-7, E-8: the position stays off the surface |

### `QCPAbstractItem` (`items/item.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `clipArea` | `QCPArea*` | plain | `clipAreaChanged(QCPArea*)` |  |

### `QCPLayoutElement` (`layout.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `autoMargins` | `QCP::MarginSides` | plain | `autoMarginsChanged(QCP::MarginSides)` |  |

### `QCPGroupLegendItem` (`layoutelements/layoutelement-legend-group.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `expanded` | `bool` | plain | `expandedChanged(bool)` |  |
| `selectedComponent` | `int` | plain | `selectedComponentChanged(int)` |  |

### `QCPTextElement` (`layoutelements/layoutelement-textelement.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `textFlags` | `int` | plain | `textFlagsChanged(int)` |  |

### `QCPLineEnding` (`lineending.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `style` | `EndingStyle` | gadget | - |  |
| `width` | `double` | gadget | - |  |
| `length` | `double` | gadget | - |  |
| `inverted` | `bool` | gadget | - |  |

### `QCPOverlay` (`overlay.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `collapsible` | `bool` | plain | `collapsibleChanged(bool)` | new; the setter also emits collapsedChanged when it uncollapses |
| `collapsed` | `bool` | plain | `collapsedChanged(bool)` | existing signal |
| `opacity` | `qreal` | plain | `opacityChanged(qreal)` |  |
| `font` | `QFont` | plain | `fontChanged(const QFont&)` |  |

### `QCPPainter` (`painting/painter.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `antialiasing` | `bool` | out | - | gadget properties only on the three value types (E-7) |
| `modes` | `PainterModes` | out | - | gadget properties only on the three value types (E-7) |

### `QCPBars` (`plottables/plottable-bars.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `data` | `QSharedPointer<QCPBarsDataContainer>` | out | - | a shared data container: mutated in place with no signal; setData is an action |

### `QCPColorMap2` (`plottables/plottable-colormap2.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `gapThreshold` | `double` | plain | `gapThresholdChanged(double)` |  |
| `contourLevels` | `QVector<double>` | plain | `contourLevelsChanged(const QVector<double>&)` |  |
| `contourPen` | `QPen` | plain | `contourPenChanged(const QPen&)` |  |
| `contourLabelEnabled` | `bool` | plain | `contourLabelEnabledChanged(bool)` |  |

### `QCPCurve` (`plottables/plottable-curve.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `data` | `QSharedPointer<QCPCurveDataContainer>` | out | - | a shared data container: mutated in place with no signal; setData is an action |

### `QCPFinancial` (`plottables/plottable-financial.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `data` | `QSharedPointer<QCPFinancialDataContainer>` | out | - | a shared data container: mutated in place with no signal; setData is an action |

### `QCPGraph` (`plottables/plottable-graph.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `deferredDataSwap` | `bool` | plain | `deferredDataSwapChanged(bool)` |  |
| `gapThreshold` | `double` | plain | `gapThresholdChanged(double)` |  |

### `QCPPlottableGroup` (`plottables/plottable-group.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `name` | `QString` | plain | `nameChanged(const QString&)` |  |
| `visible` | `bool` | plain | `visibleChanged(bool)` |  |

### `QCPHistogram` (`plottables/plottable-histogram.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `bins` | `int` | plain | `binsChanged(int)` |  |
| `normalization` | `QCP::HistogramNormalization` | plain | `normalizationChanged(QCP::HistogramNormalization)` |  |
| `style` | `Style` | plain | `styleChanged(Style)` |  |
| `columnColors` | `QList<QColor>` | plain | `columnColorsChanged(const QList<QColor>&)` |  |

### `QCPHistogram2D` (`plottables/plottable-histogram2d.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `normalization` | `QCP::HistogramNormalization` | plain | `normalizationChanged(QCP::HistogramNormalization)` |  |
| `columnNormalized` | `bool` | plain | `columnNormalizedChanged(bool)` |  |
| `contourLevels` | `QVector<double>` | plain | `contourLevelsChanged(const QVector<double>&)` |  |
| `contourPen` | `QPen` | plain | `contourPenChanged(const QPen&)` |  |
| `contourLabelEnabled` | `bool` | plain | `contourLabelEnabledChanged(bool)` |  |

### `QCPIntervals` (`plottables/plottable-intervals.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `stackMode` | `StackMode` | plain | `stackModeChanged(StackMode)` |  |
| `forbidOverlap` | `bool` | plain | `forbidOverlapChanged(bool)` |  |
| `style` | `Style` | plain | `styleChanged(Style)` |  |
| `selectedRows` | `QVector<int>` | plain | `selectedRowsChanged(const QVector<int>&)` |  |
| `editable` | `bool` | plain | `editableChanged(bool)` |  |
| `editModes` | `EditModes` | plain | `editModesChanged(EditModes)` |  |

### `QCPStatisticalBox` (`plottables/plottable-statisticalbox.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `data` | `QSharedPointer<QCPStatisticalBoxDataContainer>` | out | - | a shared data container: mutated in place with no signal; setData is an action |

### `QCPWaterfallGraph` (`plottables/plottable-waterfall.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `offsetMode` | `OffsetMode` | plain | `offsetModeChanged(OffsetMode)` |  |
| `uniformSpacing` | `double` | plain | `uniformSpacingChanged(double)` |  |
| `offsets` | `QVector<double>` | plain | `offsetsChanged(const QVector<double>&)` |  |
| `normalize` | `bool` | plain | `normalizeChanged(bool)` |  |
| `gain` | `double` | plain | `gainChanged(double)` |  |

### `QCPSelectionDecorator` (`plottables/plottable.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `pen` | `QPen` | out | - | gadget properties only on the three value types (E-7) |
| `brush` | `QBrush` | out | - | gadget properties only on the three value types (E-7) |
| `usedScatterProperties` | `QCPScatterStyle::ScatterProperties` | out | - | gadget properties only on the three value types (E-7) |

### `QCPAbstractPlottable` (`plottables/plottable.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `system` | `QCPCoordinateSystem*` | plain | `systemChanged(QCPCoordinateSystem*)` | the protected virtual hook `systemChanged()` (`plottable.h:340`) is renamed `systemDidChange()` to free the name |
| `wantsLegend` | `bool` | plain | `wantsLegendChanged(bool)` |  |
| `busy` | `bool` | out | - | pipeline state (A14); busyChanged stays a plain signal |

### `QCPPolarAxisAngular` (`polar/angularaxis.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `angle` | `double` | plain | `angleChanged(double)` |  |
| `ticks` | `bool` | plain | `ticksChanged(bool)` |  |
| `tickLabels` | `bool` | plain | `tickLabelsChanged(bool)` |  |
| `tickLabelPadding` | `int` | plain | `tickLabelPaddingChanged(int)` |  |
| `tickLabelFont` | `QFont` | plain | `tickLabelFontChanged(const QFont&)` |  |
| `tickLabelColor` | `QColor` | plain | `tickLabelColorChanged(const QColor&)` |  |
| `tickLabelRotation` | `double` | plain | `tickLabelRotationChanged(double)` |  |
| `tickLabelMode` | `LabelMode` | plain | `tickLabelModeChanged(LabelMode)` |  |
| `numberFormat` | `QString` | plain | `numberFormatChanged(const QString&)` |  |
| `numberPrecision` | `int` | plain | `numberPrecisionChanged(int)` |  |
| `tickLengthIn` | `int` | plain | `tickLengthInChanged(int)` |  |
| `tickLengthOut` | `int` | plain | `tickLengthOutChanged(int)` |  |
| `subTicks` | `bool` | plain | `subTicksChanged(bool)` |  |
| `subTickLengthIn` | `int` | plain | `subTickLengthInChanged(int)` |  |
| `subTickLengthOut` | `int` | plain | `subTickLengthOutChanged(int)` |  |
| `basePen` | `QPen` | plain | `basePenChanged(const QPen&)` |  |
| `tickPen` | `QPen` | plain | `tickPenChanged(const QPen&)` |  |
| `subTickPen` | `QPen` | plain | `subTickPenChanged(const QPen&)` |  |
| `labelFont` | `QFont` | plain | `labelFontChanged(const QFont&)` |  |
| `labelColor` | `QColor` | plain | `labelColorChanged(const QColor&)` |  |
| `label` | `QString` | plain | `labelChanged(const QString&)` |  |
| `labelPadding` | `int` | plain | `labelPaddingChanged(int)` |  |
| `selectedTickLabelFont` | `QFont` | plain | `selectedTickLabelFontChanged(const QFont&)` |  |
| `selectedLabelFont` | `QFont` | plain | `selectedLabelFontChanged(const QFont&)` |  |
| `selectedTickLabelColor` | `QColor` | plain | `selectedTickLabelColorChanged(const QColor&)` |  |
| `selectedLabelColor` | `QColor` | plain | `selectedLabelColorChanged(const QColor&)` |  |
| `selectedBasePen` | `QPen` | plain | `selectedBasePenChanged(const QPen&)` |  |
| `selectedTickPen` | `QPen` | plain | `selectedTickPenChanged(const QPen&)` |  |
| `selectedSubTickPen` | `QPen` | plain | `selectedSubTickPenChanged(const QPen&)` |  |

### `QCPPolarArea` (`polar/polar-area.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `rangeZoomFactor` | `double` | plain | `rangeZoomFactorChanged(double)` |  |

### `QCPPolarGraph` (`polar/polargraph.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `periodic` | `bool` | plain | `periodicChanged(bool)` |  |
| `lineStyle` | `LineStyle` | plain | `lineStyleChanged(LineStyle)` |  |
| `scatterStyle` | `QCPScatterStyle` | plain | `scatterStyleChanged(const QCPScatterStyle&)` |  |
| `data` | `QSharedPointer<QCPGraphDataContainer>` | out | - | a shared data container: mutated in place with no signal; setData is an action |

### `QCPPolarGrid` (`polar/polargrid.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `radialAxis` | `QCPPolarAxisRadial*` | plain | `radialAxisChanged(QCPPolarAxisRadial*)` |  |
| `type` | `GridTypes` | plain | `typeChanged(GridTypes)` |  |
| `subGridType` | `GridTypes` | plain | `subGridTypeChanged(GridTypes)` |  |
| `antialiasedSubGrid` | `bool` | plain | `antialiasedSubGridChanged(bool)` |  |
| `antialiasedZeroLine` | `bool` | plain | `antialiasedZeroLineChanged(bool)` |  |
| `angularPen` | `QPen` | plain | `angularPenChanged(const QPen&)` |  |
| `angularSubGridPen` | `QPen` | plain | `angularSubGridPenChanged(const QPen&)` |  |
| `radialPen` | `QPen` | plain | `radialPenChanged(const QPen&)` |  |
| `radialSubGridPen` | `QPen` | plain | `radialSubGridPenChanged(const QPen&)` |  |
| `radialZeroLinePen` | `QPen` | plain | `radialZeroLinePenChanged(const QPen&)` |  |

### `QCPPolarAxisRadial` (`polar/radialaxis.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `angle` | `double` | plain | `angleChanged(double)` |  |
| `angleReference` | `AngleReference` | plain | `angleReferenceChanged(AngleReference)` |  |
| `ticks` | `bool` | plain | `ticksChanged(bool)` |  |
| `tickLabels` | `bool` | plain | `tickLabelsChanged(bool)` |  |
| `tickLabelPadding` | `int` | plain | `tickLabelPaddingChanged(int)` |  |
| `tickLabelFont` | `QFont` | plain | `tickLabelFontChanged(const QFont&)` |  |
| `tickLabelColor` | `QColor` | plain | `tickLabelColorChanged(const QColor&)` |  |
| `tickLabelRotation` | `double` | plain | `tickLabelRotationChanged(double)` |  |
| `tickLabelMode` | `LabelMode` | plain | `tickLabelModeChanged(LabelMode)` |  |
| `numberFormat` | `QString` | plain | `numberFormatChanged(const QString&)` |  |
| `numberPrecision` | `int` | plain | `numberPrecisionChanged(int)` |  |
| `tickLengthIn` | `int` | plain | `tickLengthInChanged(int)` |  |
| `tickLengthOut` | `int` | plain | `tickLengthOutChanged(int)` |  |
| `subTicks` | `bool` | plain | `subTicksChanged(bool)` |  |
| `subTickLengthIn` | `int` | plain | `subTickLengthInChanged(int)` |  |
| `subTickLengthOut` | `int` | plain | `subTickLengthOutChanged(int)` |  |
| `basePen` | `QPen` | plain | `basePenChanged(const QPen&)` |  |
| `tickPen` | `QPen` | plain | `tickPenChanged(const QPen&)` |  |
| `subTickPen` | `QPen` | plain | `subTickPenChanged(const QPen&)` |  |
| `labelFont` | `QFont` | plain | `labelFontChanged(const QFont&)` |  |
| `labelColor` | `QColor` | plain | `labelColorChanged(const QColor&)` |  |
| `label` | `QString` | plain | `labelChanged(const QString&)` |  |
| `labelPadding` | `int` | plain | `labelPaddingChanged(int)` |  |
| `selectedTickLabelFont` | `QFont` | plain | `selectedTickLabelFontChanged(const QFont&)` |  |
| `selectedLabelFont` | `QFont` | plain | `selectedLabelFontChanged(const QFont&)` |  |
| `selectedTickLabelColor` | `QColor` | plain | `selectedTickLabelColorChanged(const QColor&)` |  |
| `selectedLabelColor` | `QColor` | plain | `selectedLabelColorChanged(const QColor&)` |  |
| `selectedBasePen` | `QPen` | plain | `selectedBasePenChanged(const QPen&)` |  |
| `selectedTickPen` | `QPen` | plain | `selectedTickPenChanged(const QPen&)` |  |
| `selectedSubTickPen` | `QPen` | plain | `selectedSubTickPenChanged(const QPen&)` |  |

### `QCPScatterStyle` (`scatterstyle.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `size` | `double` | gadget | - |  |
| `shape` | `ScatterShape` | gadget | - |  |
| `pen` | `QPen` | gadget | - |  |
| `brush` | `QBrush` | gadget | - |  |
| `pixmap` | `QPixmap` | gadget | - |  |
| `customPath` | `QPainterPath` | gadget | - |  |

### `QCPSelectionDecoratorBracket` (`selectiondecorator-bracket.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `bracketPen` | `QPen` | out | - | gadget properties only on the three value types (E-7) |
| `bracketBrush` | `QBrush` | out | - | gadget properties only on the three value types (E-7) |
| `bracketWidth` | `int` | out | - | gadget properties only on the three value types (E-7) |
| `bracketHeight` | `int` | out | - | gadget properties only on the three value types (E-7) |
| `bracketStyle` | `BracketStyle` | out | - | gadget properties only on the three value types (E-7) |
| `tangentToData` | `bool` | out | - | gadget properties only on the three value types (E-7) |
| `tangentAverage` | `int` | out | - | gadget properties only on the three value types (E-7) |

### `QCPSelectionRect` (`selectionrect.h`)

| property | type | form | NOTIFY | note |
|---|---|---|---|---|
| `pen` | `QPen` | plain | `penChanged(const QPen&)` |  |
| `brush` | `QBrush` | plain | `brushChanged(const QBrush&)` |  |

## Section 3.3: the 3.0 classes

| class | property | type | form | NOTIFY | note |
|---|---|---|---|---|---|
| `QCPCoordinateSystem` | `area` | `QCPArea*` | constant | - | fixed at construction (`coordinate-system.h:64-70`); READ the handle getter (E-6) |
| `QCPCoordinateSystem` | `keyAxis` | `QCPAxisBase*` | constant | - | fixed at construction; READ the handle getter (E-6) |
| `QCPCoordinateSystem` | `valueAxis` | `QCPAxisBase*` | constant | - | fixed at construction; READ the handle getter (E-6) |
| `QCPCoordinateSystem` | `capabilities` | `QCP::Capabilities` | constant | - | one answer per subclass (pure virtual getter) |
| `QCPCoordinateSystem` | `revision` | `quint64` | derived | `changed()` | existing signal (connect-notify gated) |
| `QCPCoordinateSystem` | `transform` | `QCPTransform` | out | - | E-8: a std::variant value, not a gadget |
| `QCPCoordinateSystem` | `plottables` | `QList<QCPHandle<…>>` | dropped | - | a list of handles is no property type (E-6) |
| `QCPArea` | `insetLayout` | `QCPLayoutInset*` | constant | - | created with the area, destroyed only by its teardown (`area.cpp:71`, `layoutelement-axisrect.cpp:252`, `polar-area.cpp:83`) |
| `QCPArea` | `systems` | `QList<…>` | dropped | - | changes with the axes, no signal |
| `QCPArea` | `systemsListen` | `bool` | dropped | - | bookkeeping, not user state |
| `QCPPolarArea` | `rangeZoomFactor` | `double` | plain | `rangeZoomFactorChanged(double)` | also in 3.1 |
| `QCPItemPosition` | `(all)` | `gadget` | out | - | E-7, E-8 |
| `QCPAlignmentGroup` | `sides` | `QCP::MarginSides` | constant | - | fixed at construction |
| `QCPAlignmentGroup` | `elements` | `QList<QCPHandle<QCPLayoutElement>>` | dropped | - | a list of handles is no property type (E-6); membersChanged() stays a plain signal |
| `QCPAxisBase` | `linkSource / link` | `QCPAxisBase* / QCPAxisLink` | out | - | set by linkTo(source, forward, inverse), an action with functions |
## Writers besides the setter (E-4b), known before Task 2

Each also emits the signal of every property it moves. Task 2 lists the rest per area.

- `QCPAxis::setColor`: `labelColor`, `tickLabelColor`, `basePen`, `tickPen`, `subTickPen` (`axis.cpp:1095-1105`).
- `QCPAxis::setTickLength(in, out)`, `setSubTickLength(in, out)`: the two lengths each; the polar axes the same.
- `QCPAxisBase::setRangeLower`, `setRangeUpper`: `range`, through `commitRange` like `setRange`.
- `QCPAbstractLine::setPen` and `QCPCurve::setPen` (spike): `penChanged` once, and only when `pen()` moves.

## Doubtful rows

Decided above; listed so the review can overturn them.

1. `QCPAxis::axisRect` is **dropped**, although an axis never changes rect. E-11 names its three constants and this is
   not one of them. `constant` is the alternative. (`grid` and `plotLayout` are remade when deleted, so they cannot be
   constant.)
2. The **existing signals keep their signatures**: `selectionChanged()`, `selectableChanged()`,
   `colorScaleChanged()` carry no value, `rangeChanged` carries two. E-4's `xChanged(T)` applies to new signals; a
   rename would break every user connection for no binding gain (`QBindable` ignores the arguments).
3. `QCPItemPixmap::aspectRatioMode` and `transformationMode` are **dropped** (E-12: no split setter). The alternative
   is derived with a `scaledChanged(bool)` emitted by `setScaled`.
4. The **shared data containers** are off the surface: `QCPErrorBars::data` is **dropped** (a property today, so a
   break for the migration page), the five `data` pairs of 3.1 stay **out**. The container is mutated in place with no
   signal, and `dataChanged` is a hook name of several plottables.
5. `QCustomPlot::itemCreator` and `itemPositioner` are **out**: `std::function` values.
6. `QCPAbstractPlottable::system` takes the signal `systemChanged(QCPCoordinateSystem*)`. The protected virtual hook of
   the same name (`plottable.h:340`, overridden in `plottable-graph.h:277`, `-colormap2.h:127`, `-histogram.h:75`,
   `-histogram2d.h:124`) becomes `systemDidChange()`. A user subclass overriding the hook breaks loudly (`override`).
7. `QCPAlignmentGroup::elements` is **dropped**: a list of handles has no metatype (E-6). `membersChanged()` stays.
8. `QCustomPlot::currentLayer` is one property typed `QCPLayer*`; the `QString` overload of the setter stays C++.
9. `QCPOverlay::collapsible` gets its own `collapsibleChanged(bool)`; its setter can also emit `collapsedChanged`.
10. The theme's 16 properties and the plot's 8 theme properties share one argument-less signal each (`changed()`,
    `themeChanged()`), as E-4 and E-5b rule. A `QBindable` observer re-reads; it cannot tell which colour moved.
