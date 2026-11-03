# Milestone 2 — setters that schedule a repaint

One table per file family. Columns:

- **changes the drawing** — `yes` (the picture changes without a range change), `range` (already
  handled by `QCPAxis::rangeDidChange()`), `data` (the plottables' data, not the axis), `no`
  (interaction or state only).
- **dirties** — what the setter marks dirty once milestone 2 is done. For a `QCPAxis` the whole rect
  goes dirty: a margin change (tick labels, label, padding, tick length) moves the axis rect's inner
  rect, so every axis, grid and plottable in it moves with it. `QCPAxisRect::markAffectedLayersDirty()`
  already does that walk for range changes, and `markLayersUsingAxisDirty(axis)` adds the items
  attached to the axis. `QCPGrid` and the axis rect are layerables, so they dirty their own layer.

A `yes` row is a row of `TestRepaint::axisSettersScheduleRepaint`.

## axes

| class | setter | file:line | changes the drawing? | dirties |
|---|---|---|---|---|
| QCPGrid | setSubGridVisible | src/axis/axis.cpp:84 | yes | own layer |
| QCPGrid | setAntialiasedSubGrid | src/axis/axis.cpp:98 | yes | own layer |
| QCPGrid | setAntialiasedZeroLine | src/axis/axis.cpp:109 | yes | own layer |
| QCPGrid | setPen | src/axis/axis.cpp:120 | yes | own layer |
| QCPGrid | setSubGridPen | src/axis/axis.cpp:134 | yes | own layer |
| QCPGrid | setZeroLinePen | src/axis/axis.cpp:151 | yes | own layer |
| QCPAxis | setScaleType | src/axis/axis.cpp:623 | yes | rect (ticks change even when the range does not) |
| QCPAxis | setRange(QCPRange) | src/axis/axis.cpp:647 | range | already: `rangeDidChange()` → `markAffectedLayersDirty()` |
| QCPAxis | setRange(double, double) | src/axis/axis.cpp:721 | range | already, same path |
| QCPAxis | setRange(position, size, align) | src/axis/axis.cpp:753 | range | already, same path |
| QCPAxis | setRangeLower | src/axis/axis.cpp:807 | range | already, same path |
| QCPAxis | setRangeUpper | src/axis/axis.cpp:829 | range | already, same path |
| QCPAxis | setRangeReversed | src/axis/axis.cpp:856 | range | already: reverses then `setRange()`s |
| QCPAxis | moveRange | src/axis/axis.cpp:1507 | range | already: `setRange()` |
| QCPAxis | scaleRange(center, factor) | src/axis/axis.cpp:1426 | range | already: `setRange()` |
| QCPAxis | scaleRange(center, hFactor, vFactor) | src/axis/axis.cpp:1440 | range | already: `setRange()` |
| QCPAxis | setScaleRatio | src/axis/axis.cpp:1591 | range | already: `setRange()` |
| QCPAxis | rescale | src/axis/axis.cpp:1615 | range | already: `setRange()` |
| QCPAxis | setSelectableParts | src/axis/axis.cpp:714 | no | — (enables selection, draws nothing) |
| QCPAxis | setSelectedParts | src/axis/axis.cpp:741 | yes | rect (selection decorations of the tick labels and the label) |
| QCPAxis | setTicker | src/axis/axis.cpp:877 | yes | rect |
| QCPAxis | setTicks | src/axis/axis.cpp:901 | yes | rect |
| QCPAxis | setTickLabels | src/axis/axis.cpp:914 | yes | rect |
| QCPAxis | setTickLabelPadding | src/axis/axis.cpp:930 | yes | rect |
| QCPAxis | setTickLabelFont | src/axis/axis.cpp:945 | yes | rect |
| QCPAxis | setTickLabelColor | src/axis/axis.cpp:960 | yes | rect |
| QCPAxis | setTickLabelRotation | src/axis/axis.cpp:977 | yes | rect |
| QCPAxis | setTickLabelSide | src/axis/axis.cpp:994 | yes | rect |
| QCPAxis | setNumberFormat | src/axis/axis.cpp:1038 | yes | rect (unknown codes are rejected, nothing changes) |
| QCPAxis | setNumberPrecision | src/axis/axis.cpp:1102 | yes | rect |
| QCPAxis | setTickLength | src/axis/axis.cpp:1120 | yes | rect |
| QCPAxis | setTickLengthIn | src/axis/axis.cpp:1132 | yes | rect |
| QCPAxis | setTickLengthOut | src/axis/axis.cpp:1148 | yes | rect |
| QCPAxis | setSubTicks | src/axis/axis.cpp:1165 | yes | rect |
| QCPAxis | setSubTickLength | src/axis/axis.cpp:1183 | yes | rect |
| QCPAxis | setSubTickLengthIn | src/axis/axis.cpp:1195 | yes | rect |
| QCPAxis | setSubTickLengthOut | src/axis/axis.cpp:1211 | yes | rect |
| QCPAxis | setBasePen | src/axis/axis.cpp:1226 | yes | rect |
| QCPAxis | setTickPen | src/axis/axis.cpp:1239 | yes | rect |
| QCPAxis | setSubTickPen | src/axis/axis.cpp:1252 | yes | rect |
| QCPAxis | setLabelFont | src/axis/axis.cpp:1265 | yes | rect |
| QCPAxis | setLabelColor | src/axis/axis.cpp:1280 | yes | rect |
| QCPAxis | setLabel | src/axis/axis.cpp:1292 | yes | rect |
| QCPAxis | setLabelRenderer | src/axis/axis.cpp:1302 | yes | rect |
| QCPAxis | setLabelPadding | src/axis/axis.cpp:1317 | yes | rect |
| QCPAxis | setPadding | src/axis/axis.cpp:1337 | yes | rect |
| QCPAxis | setOffset | src/axis/axis.cpp:1355 | yes | rect |
| QCPAxis | setSelectedTickLabelFont | src/axis/axis.cpp:1368 | yes | rect |
| QCPAxis | setSelectedLabelFont | src/axis/axis.cpp:1384 | yes | rect |
| QCPAxis | setSelectedTickLabelColor | src/axis/axis.cpp:1399 | yes | rect |
| QCPAxis | setSelectedLabelColor | src/axis/axis.cpp:1413 | yes | rect |
| QCPAxis | setSelectedBasePen | src/axis/axis.cpp:1426 | yes | rect |
| QCPAxis | setSelectedTickPen | src/axis/axis.cpp:1439 | yes | rect |
| QCPAxis | setSelectedSubTickPen | src/axis/axis.cpp:1452 | yes | rect |
| QCPAxis | setLowerEnding | src/axis/axis.cpp:1470 | yes | rect |
| QCPAxis | setUpperEnding | src/axis/axis.cpp:1490 | yes | rect |
| QCPAxisRect | setBackground(QPixmap) | src/layoutelements/layoutelement-axisrect.cpp:827 | yes | own layer |
| QCPAxisRect | setBackground(QBrush) | src/layoutelements/layoutelement-axisrect.cpp:846 | yes | own layer |
| QCPAxisRect | setBackground(QPixmap, scaled, mode) | src/layoutelements/layoutelement-axisrect.cpp:858 | yes | own layer |
| QCPAxisRect | setBackgroundScaled | src/layoutelements/layoutelement-axisrect.cpp:894 | yes | own layer |
| QCPAxisRect | setBackgroundScaledMode | src/layoutelements/layoutelement-axisrect.cpp:908 | yes | own layer |
| QCPAxisRect | setRangeDrag | src/layoutelements/layoutelement-axisrect.cpp:1025 | no | — |
| QCPAxisRect | setRangeZoom | src/layoutelements/layoutelement-axisrect.cpp:1046 | no | — |
| QCPAxisRect | setRangeDragAxes(QCPAxis*, QCPAxis*) | src/layoutelements/layoutelement-axisrect.cpp:1038 | no | — |
| QCPAxisRect | setRangeDragAxes(QList<QCPAxis*>) | src/layoutelements/layoutelement-axisrect.cpp:1081 | no | — |
| QCPAxisRect | setRangeDragAxes(QList<QCPAxis*>, QList<QCPAxis*>) | src/layoutelements/layoutelement-axisrect.cpp:1100 | no | — |
| QCPAxisRect | setRangeZoomAxes(QCPAxis*, QCPAxis*) | src/layoutelements/layoutelement-axisrect.cpp:1112 | no | — |
| QCPAxisRect | setRangeZoomAxes(QList<QCPAxis*>) | src/layoutelements/layoutelement-axisrect.cpp:1155 | no | — |
| QCPAxisRect | setRangeZoomAxes(QList<QCPAxis*>, QList<QCPAxis*>) | src/layoutelements/layoutelement-axisrect.cpp:1174 | no | — |
| QCPAxisRect | setRangeZoomFactor(double) | src/layoutelements/layoutelement-axisrect.cpp:1184 | no | — |
| QCPAxisRect | setRangeZoomFactor(double, double) | src/layoutelements/layoutelement-axisrect.cpp:1208 | no | — |
| QCPAxisRect | setupFullAxesBox | src/layoutelements/layoutelement-axisrect.cpp:617 | structural | no call: it only adds axes to a new rect, and they dirty their own layers when laid out |

### Shared values: not dirtied by the axis

A ticker, a label renderer and the label painter are shared objects: the same ticker can sit on
several axes, and editing it in place changes nothing the axis can observe. These setters are
therefore **not** dirtied — call `plot.requestRepaint()` after changing a shared value in place.

| Class | setter | file:line | changes the drawing? | dirties |
| --- | --- | --- | --- | --- |
| QCPAxisTicker | setTickStepStrategy | src/axis/axisticker.cpp:101 | shared value | no |
| QCPAxisTicker | setTickCount | src/axis/axisticker.cpp:114 | shared value | no |
| QCPAxisTicker | setTickOrigin | src/axis/axisticker.cpp:130 | shared value | no |
| QCPAxisTickerDateTime | setDateTimeFormat | src/axis/axistickerdatetime.cpp:136 | shared value | no |
| QCPAxisTickerDateTime | setDateTimeSpec | src/axis/axistickerdatetime.cpp:154 | shared value | no |
| QCPAxisTickerDateTime | setTimeZone | src/axis/axistickerdatetime.cpp:165 | shared value | no |
| QCPAxisTickerDateTime | setTickOrigin | src/axis/axistickerdatetime.cpp:180 | shared value | no |
| QCPAxisTickerFixed | setTickStep | src/axis/axistickerfixed.cpp:67 | shared value | no |
| QCPAxisTickerFixed | setScaleStrategy | src/axis/axistickerfixed.cpp:82 | shared value | no |
| QCPAxisTickerLog | setLogBase | src/axis/axistickerlog.cpp:114 | shared value | no |
| QCPAxisTickerLog | setSubTickCount | src/axis/axistickerlog.cpp:135 | shared value | no |
| QCPAxisTickerPi | setPiSymbol | src/axis/axistickerpi.cpp:69 | shared value | no |
| QCPAxisTickerPi | setPiValue | src/axis/axistickerpi.cpp:80 | shared value | no |
| QCPAxisTickerPi | setPeriodicity | src/axis/axistickerpi.cpp:93 | shared value | no |
| QCPAxisTickerPi | setFractionStyle | src/axis/axistickerpi.cpp:102 | shared value | no |
| QCPAxisTickerText | setTicks | src/axis/axistickertext.cpp:80 | shared value | no |
| QCPAxisTickerText | setSubTickCount | src/axis/axistickertext.cpp:104 | shared value | no |
| QCPAxisTickerTime | setTimeFormat | src/axis/axistickertime.cpp:108 | shared value | no |
| QCPAxisTickerTime | setFieldWidth | src/axis/axistickertime.cpp:139 | shared value | no |
| QCPLabelRenderer | setDefaultRenderer | src/axis/labelrenderer.cpp:26 | shared value | no |
| QCPLabelPainterPrivate | setFont/setColor/setPadding/setRotation/... | src/axis/labelpainter.cpp:93 | shared value | no |

The axis rect's margin setters live in `src/layout.cpp` (`QCPLayoutGrid`), not in the rect: they are
handled with the layout family in Task 5.
