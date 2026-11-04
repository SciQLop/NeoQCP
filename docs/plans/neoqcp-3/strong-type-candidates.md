# Strong-type candidates (milestone 4 input)

Every public signature outside drawing where a bare `double`, `int`, `QPointF` or `QString` accepts a
value from another space without a compiler complaint, with the misuse it allows. Milestone 3 types the
drawing half (`QCP::Position`, `QCPTransform`, `QCPRatioTransform`); the rest is for milestone 4. Line
numbers are at the milestone-3 branch head.

Kept out on purpose: the fluent handle/ops setters (`src/handle-ops.h`, `*Ops` classes) — they forward
the same arguments to the same functions, so a type fixes both at once; the entry is the 2.x signature.

## A. Key vs value vs pixels (the drawing spaces)

| signature | file:line | mix-up allowed today | type that stops it | status |
| --- | --- | --- | --- | --- |
| `QCPItemPosition::setCoords(double key, double value)` | src/items/item.h:166 | key/value swapped; the type flags are separate, so nothing checks that `x` is the key | `QCP::Data` / `QCP::Pixels` / `QCP::AxisRatio` via `set(QCP::Position)` | **m3 done** |
| `QCPItemPosition::setTypeX/setTypeY(PositionType)` | src/items/item.h:161 | one axis in data and the other in ratio — the `MixedX` shape only exists by convention | `QCP::MixedX` / `QCP::MixedY` | **m3 done** |
| `QCPAxis::coordToPixel(double)` / `pixelToCoord(double)` | src/axis/axis.h:383 / :382 | a pixel value passed where a coordinate is expected (and back) | `QCP::Data` ↔ `QCP::Pixels` through `QCPTransform` | **m3 done** |
| `QCPAxis::setRange(double lower, double upper)` | src/axis/axis.h:319 | lower > upper (silently "valid" until a range check), or a pixel value | `QCP::Interval` (constructor refuses inverted) | m4 |
| `QCPAxis::moveRange(double diff)` | src/axis/axis.h:377 | a pixel delta instead of a data delta | `QCP::DataDelta` vs `QCP::PixelOffset` | m4 |
| `QCPAxis::setScaleRatio(const QCPAxis*, double ratio)` | src/axis/axis.h:380 | the ratio of the wrong axis pair | `QCP::Ratio{QCPAxis*, QCPAxis*}` | m4 |
| `QCPAxisRect::setRangeDragAxes(QCPAxis* horizontal, QCPAxis* vertical)` | src/layoutelements/layoutelement-axisrect.h:90 | horizontal/vertical swapped — drag works, in the wrong direction | `QCP::HorizontalAxis` / `QCP::VerticalAxis` | m4 |
| `QCPAxisRect::setRangeZoomFactor(double horizontal, double vertical)` | src/layoutelements/layoutelement-axisrect.h:96 | the same swap, only visible as a wrong zoom | same | m4 |
| `QCPColorMapData::setRange(const QCPRange& keyRange, const QCPRange& valueRange)` | src/plottables/plottable-colormap.h:70 | the two ranges swapped: the map is transposed | `QCP::KeyRange` / `QCP::ValueRange` | m4 |
| `QCPColorMapData::setSize(int keySize, int valueSize)` / `setCell(int keyIndex, int valueIndex, double z)` / `setAlpha(int, int, unsigned char)` | src/plottables/plottable-colormap.h:67 / :74 / :75 | index order swapped; with a non-square map this writes the wrong cell or throws | `QCP::CellIndex{key, value}` | m4 |
| `QCPHistogram2D::setBins(int keyBins, int valueBins)` | src/plottables/plottable-histogram2d.h:59 | key/value bins swapped | `QCP::Bins{key, value}` | m4 |
| `QCPPolarAxisAngular::coordToPixel(double angle, double radius)` | src/polar/layoutelement-angularaxis.h:292 | angle/radius swapped (the same member name as the 1-D axes) | `QCP::Polar{angle, radius}` | m4 |
| `QCPItemTracer::setGraphKey(double key)` | src/items/item-tracer.h:108 | a graph key given as a data key on another axis pair | `QCP::Data` on the graph's key axis | m4 |

## B. Two numbers of the same kind

| signature | file:line | mix-up allowed today | type that stops it | status |
| --- | --- | --- | --- | --- |
| `QCPAxis::setTickLength(int inside, int outside = 0)` | src/axis/axis.h:334 | inside/outside swapped (both are pixels, the plot still draws) | `QCP::TickLength{inside, outside}` | m4 |
| `QCPAxis::setSubTickLength(int inside, int outside = 0)` | src/axis/axis.h:335 | the same | same | m4 |
| `QCPAxisRect::setRowSpacing(int)` / `setColumnSpacing(int)` | src/layout.h:304 / :303 | a zero passed for the other one; also pixel vs ratio confusion with `stretchFactor` | `QCP::Pixels` | m4 |
| `QCPLayoutGrid::setRowStretchFactor(int, double)` / `setColumnStretchFactor(int, double)` | src/layout.h (same family) | `(row, factor)` vs `(column, factor)`; an int row index and a `double` factor look alike | `QCP::Row` / `QCP::Column` | m4 |
| `QCPLegend::setIconSize(int width, int height)` | src/layoutelements/layoutelement-legend.h:227 | width/height swapped | `QCP::PixelSize` | m4 |
| `QCPColorScale::setBarWidth(int)` | src/layoutelements/layoutelement-colorscale.h:114 | pixels vs a ratio of the rect | `QCP::Pixels` | m4 |
| `QCPItemText::setPositionAlignment(Qt::Alignment)` next to `setTextAlignment(Qt::Alignment)` | src/items/item-text.h:101 / :102 | the text jumps to a corner: both take the same enum, one means the anchor, the other the text | two distinct enums (`QCP::AnchorAlignment`) | m4 |
| `QCPFinancial::setWidth(double)` / `QCPBars::setWidth(double)` with `setWidthType(WidthType)` | src/plottables/plottable-financial.h:157 / src/plottables/plottable-bars.h | a width in axis units given where the type says pixels (and back) | `QCP::DataLength` / `QCP::PixelLength` | m4 |
| `QCPItemBracket::setLength(double)` / `QCPLineEnding::setLength(double)` | src/items/item-bracket.h:85 / src/lineending.h:91 | a data length vs a pixel length | `QCP::PixelLength` | m4 |
| `QCPErrorBars::setWhiskerWidth(double pixels)` / `QCPStatisticalBox::setWhiskerWidth(double width)` | src/plottables/plottable-errorbar.h:120 / plottable-statisticalbox.h:132 | same name, one is pixels and the other is axis units | two names or one typed length | m4 |
| `QCustomPlot::toPixmap(int width, int height, double scale)` and `savePng(..., int width, int height, double scale, ...)` | src/core.h:424 / :413 | width/height swapped, or a scale passed where a size is expected | `QCP::ExportSize{width, height, scale}` | m4 |

## C. Strings that name something

| signature | file:line | mix-up allowed today | type that stops it | status |
| --- | --- | --- | --- | --- |
| `QCPLayerable::setLayer(const QString& name)` | src/layer.h:242 | a typo is a `qDebug` and a no-op, so the object silently stays on its old layer | an interned `QCP::LayerName` (or a `bool` return the caller must look at — it already returns `bool`, the fluent form drops it) | m4 |
| `QCPLayerable::setLayer(QCPLayer*)` next to the name overload | src/layer.h:241 | the same call reads as either, and `nullptr` is legal | `QCP::LayerName` only, pointer form private | m4 |
| `QCustomPlot::addLayer(const QString& name, ...)` | src/core.h:393 | a duplicate name silently returns false; a caller who ignores it keeps drawing on the old layer | `QCP::LayerName` with a checked factory | m4 |
| `QCPAbstractPlottable::setName(const QString&)` | src/plottables/plottable.h | identity strings shared by legends and lookups; two plots with the same name are indistinguishable | `QCP::PlottableName` (unique) | m4 |
| `QCPAxis::setTicker(QSharedPointer<QCPAxisTicker>)` | src/axis/axis.h:324 | a null ticker is a `qDebug` and a no-op, so a failed construction leaves the old ticker | `QCP::NotNull<QCPAxisTicker>` | m4 |
| `QCPAxis::setNumberFormat(const QString&)` | src/axis/axis.h (same family) | a format string validated at call time with a `qDebug`, not a compile error | a typed format enum + precision | m4 |

## D. Booleans and modes in one list

| signature | file:line | mix-up allowed today | type that stops it | status |
| --- | --- | --- | --- | --- |
| `QCPAxis::setSelectedParts` / `setSelectableParts` (`QCP::SelectionParts` flags) | src/axis/axis.h | a flag set for one part used on another; the two functions take the same flag type and differ only by name | two distinct flag types (`Selected` / `Selectable`) | m4 |
| `QCPColorGradient::setColorStopAt(double position, QColor)` | src/colorgradient.h:151 | a data position given as a 0..1 gradient position — accepted, the stop lands outside the scale | `QCP::GradientPosition` (clamped 0..1, refuses outside) | m4 |
| `QCPAxisRect::setBackgroundScaledMode(Qt::AspectRatioMode)` / `setInsetPlacement(int, InsetPlacement)` | src/layoutelements/layoutelement-axisrect.h:87 / src/layout.h:376 | a raw `int` element index where a mode is expected (compiles, wrong overload choice) | `QCP::InsetIndex` | m4 |

## What milestone 3 already removed from this list

`QCPItemPosition` no longer needs `setCoords(key, value)` with two untyped doubles in new code: `QCP::Position`
carries the space, `QCPTransform::map(std::span, std::span, std::span<QPointF>)` maps a whole range in one
call, and `Data + Pixels` does not compile (`tests/auto/test-coords/test-coords.cpp`,
`mixingSpacesDoesNotCompile`). Everything above is outside that boundary.
