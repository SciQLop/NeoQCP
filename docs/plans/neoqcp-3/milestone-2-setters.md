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
| QCPGrid | setSubGridVisible | src/axis/axis.cpp:84 | yes |own layer|
| QCPGrid | setAntialiasedSubGrid | src/axis/axis.cpp:98 | yes |own layer|
| QCPGrid | setAntialiasedZeroLine | src/axis/axis.cpp:109 | yes |own layer|
| QCPGrid | setPen | src/axis/axis.cpp:120 | yes |own layer|
| QCPGrid | setSubGridPen | src/axis/axis.cpp:134 | yes |own layer|
| QCPGrid | setZeroLinePen | src/axis/axis.cpp:151 | yes |own layer|
| QCPAxis | setScaleType | src/axis/axis.cpp:623 | yes |rect (ticks change even when the range does not)|
| QCPAxis | setRange(QCPRange) | src/axis/axis.cpp:647 | range |already: `rangeDidChange()` → `markAffectedLayersDirty()`|
| QCPAxis | setRange(double, double) | src/axis/axis.cpp:721 | range |already, same path|
| QCPAxis | setRange(position, size, align) | src/axis/axis.cpp:753 | range |already, same path|
| QCPAxis | setRangeLower | src/axis/axis.cpp:807 | range |already, same path|
| QCPAxis | setRangeUpper | src/axis/axis.cpp:829 | range |already, same path|
| QCPAxis | setRangeReversed | src/axis/axis.cpp:856 | range |already: reverses then `setRange()`s|
| QCPAxis | moveRange | src/axis/axis.cpp:1507 | range |already: `setRange()`|
| QCPAxis | scaleRange(center, factor) | src/axis/axis.cpp:1426 | range |already: `setRange()`|
| QCPAxis | scaleRange(center, hFactor, vFactor) | src/axis/axis.cpp:1440 | range |already: `setRange()`|
| QCPAxis | setScaleRatio | src/axis/axis.cpp:1591 | range |already: `setRange()`|
| QCPAxis | rescale | src/axis/axis.cpp:1615 | range |already: `setRange()`|
| QCPAxis | setSelectableParts | src/axis/axis.cpp:714 | no |— (enables selection, draws nothing)|
| QCPAxis | setSelectedParts | src/axis/axis.cpp:741 | yes |rect (selection decorations of the tick labels and the label)|
| QCPAxis | setTicker | src/axis/axis.cpp:877 | yes |rect|
| QCPAxis | setTicks | src/axis/axis.cpp:901 | yes |rect|
| QCPAxis | setTickLabels | src/axis/axis.cpp:914 | yes |rect|
| QCPAxis | setTickLabelPadding | src/axis/axis.cpp:930 | yes |rect|
| QCPAxis | setTickLabelFont | src/axis/axis.cpp:945 | yes |rect|
| QCPAxis | setTickLabelColor | src/axis/axis.cpp:960 | yes |rect|
| QCPAxis | setTickLabelRotation | src/axis/axis.cpp:977 | yes |rect|
| QCPAxis | setTickLabelSide | src/axis/axis.cpp:994 | yes |rect|
| QCPAxis | setNumberFormat | src/axis/axis.cpp:1038 | yes |rect (unknown codes are rejected, nothing changes)|
| QCPAxis | setNumberPrecision | src/axis/axis.cpp:1102 | yes |rect|
| QCPAxis | setTickLength | src/axis/axis.cpp:1120 | yes |rect|
| QCPAxis | setTickLengthIn | src/axis/axis.cpp:1132 | yes |rect|
| QCPAxis | setTickLengthOut | src/axis/axis.cpp:1148 | yes |rect|
| QCPAxis | setSubTicks | src/axis/axis.cpp:1165 | yes |rect|
| QCPAxis | setSubTickLength | src/axis/axis.cpp:1183 | yes |rect|
| QCPAxis | setSubTickLengthIn | src/axis/axis.cpp:1195 | yes |rect|
| QCPAxis | setSubTickLengthOut | src/axis/axis.cpp:1211 | yes |rect|
| QCPAxis | setBasePen | src/axis/axis.cpp:1226 | yes |rect|
| QCPAxis | setTickPen | src/axis/axis.cpp:1239 | yes |rect|
| QCPAxis | setSubTickPen | src/axis/axis.cpp:1252 | yes |rect|
| QCPAxis | setLabelFont | src/axis/axis.cpp:1265 | yes |rect|
| QCPAxis | setLabelColor | src/axis/axis.cpp:1280 | yes |rect|
| QCPAxis | setLabel | src/axis/axis.cpp:1292 | yes |rect|
| QCPAxis | setLabelRenderer | src/axis/axis.cpp:1302 | yes |rect|
| QCPAxis | setLabelPadding | src/axis/axis.cpp:1317 | yes |rect|
| QCPAxis | setPadding | src/axis/axis.cpp:1337 | yes |rect|
| QCPAxis | setOffset | src/axis/axis.cpp:1355 | yes |rect|
| QCPAxis | setSelectedTickLabelFont | src/axis/axis.cpp:1368 | yes |rect|
| QCPAxis | setSelectedLabelFont | src/axis/axis.cpp:1384 | yes |rect|
| QCPAxis | setSelectedTickLabelColor | src/axis/axis.cpp:1399 | yes |rect|
| QCPAxis | setSelectedLabelColor | src/axis/axis.cpp:1413 | yes |rect|
| QCPAxis | setSelectedBasePen | src/axis/axis.cpp:1426 | yes |rect|
| QCPAxis | setSelectedTickPen | src/axis/axis.cpp:1439 | yes |rect|
| QCPAxis | setSelectedSubTickPen | src/axis/axis.cpp:1452 | yes |rect|
| QCPAxis | setLowerEnding | src/axis/axis.cpp:1470 | yes |rect|
| QCPAxis | setUpperEnding | src/axis/axis.cpp:1490 | yes |rect|
| QCPAxisRect | setBackground(QPixmap) | src/layoutelements/layoutelement-axisrect.cpp:827 | yes |own layer|
| QCPAxisRect | setBackground(QBrush) | src/layoutelements/layoutelement-axisrect.cpp:846 | yes |own layer|
| QCPAxisRect | setBackground(QPixmap, scaled, mode) | src/layoutelements/layoutelement-axisrect.cpp:858 | yes |own layer|
| QCPAxisRect | setBackgroundScaled | src/layoutelements/layoutelement-axisrect.cpp:894 | yes |own layer|
| QCPAxisRect | setBackgroundScaledMode | src/layoutelements/layoutelement-axisrect.cpp:908 | yes |own layer|
| QCPAxisRect | setRangeDrag | src/layoutelements/layoutelement-axisrect.cpp:1025 | no |—|
| QCPAxisRect | setRangeZoom | src/layoutelements/layoutelement-axisrect.cpp:1046 | no |—|
| QCPAxisRect | setRangeDragAxes(QCPAxis*, QCPAxis*) | src/layoutelements/layoutelement-axisrect.cpp:1038 | no |—|
| QCPAxisRect | setRangeDragAxes(QList<QCPAxis*>) | src/layoutelements/layoutelement-axisrect.cpp:1081 | no |—|
| QCPAxisRect | setRangeDragAxes(QList<QCPAxis*>, QList<QCPAxis*>) | src/layoutelements/layoutelement-axisrect.cpp:1100 | no |—|
| QCPAxisRect | setRangeZoomAxes(QCPAxis*, QCPAxis*) | src/layoutelements/layoutelement-axisrect.cpp:1112 | no |—|
| QCPAxisRect | setRangeZoomAxes(QList<QCPAxis*>) | src/layoutelements/layoutelement-axisrect.cpp:1155 | no |—|
| QCPAxisRect | setRangeZoomAxes(QList<QCPAxis*>, QList<QCPAxis*>) | src/layoutelements/layoutelement-axisrect.cpp:1174 | no |—|
| QCPAxisRect | setRangeZoomFactor(double) | src/layoutelements/layoutelement-axisrect.cpp:1184 | no |—|
| QCPAxisRect | setRangeZoomFactor(double, double) | src/layoutelements/layoutelement-axisrect.cpp:1208 | no |—|
| QCPAxisRect | setupFullAxesBox | src/layoutelements/layoutelement-axisrect.cpp:617 | structural |no call: it only adds axes to a new rect, and they dirty their own layers when laid out|

### Shared values: not dirtied by the axis

A ticker, a label renderer and the label painter are shared objects: the same ticker can sit on
several axes, and editing it in place changes nothing the axis can observe. These setters are
therefore **not** dirtied — call `plot.requestRepaint()` after changing a shared value in place.

| Class | setter | file:line | changes the drawing? | dirties |
| --- | --- | --- | --- | --- |
| QCPAxisTicker | setTickStepStrategy | src/axis/axisticker.cpp:104 | shared value |no|
| QCPAxisTicker | setTickCount | src/axis/axisticker.cpp:117 | shared value |no|
| QCPAxisTicker | setTickOrigin | src/axis/axisticker.cpp:133 | shared value |no|
| QCPAxisTickerDateTime | setDateTimeFormat | src/axis/axistickerdatetime.cpp:136 | shared value |no|
| QCPAxisTickerDateTime | setDateTimeSpec | src/axis/axistickerdatetime.cpp:154 | shared value |no|
| QCPAxisTickerDateTime | setTimeZone | src/axis/axistickerdatetime.cpp:165 | shared value |no|
| QCPAxisTickerDateTime | setTickOrigin | src/axis/axistickerdatetime.cpp:180 | shared value |no|
| QCPAxisTickerFixed | setTickStep | src/axis/axistickerfixed.cpp:67 | shared value |no|
| QCPAxisTickerFixed | setScaleStrategy | src/axis/axistickerfixed.cpp:82 | shared value |no|
| QCPAxisTickerLog | setLogBase | src/axis/axistickerlog.cpp:114 | shared value |no|
| QCPAxisTickerLog | setSubTickCount | src/axis/axistickerlog.cpp:135 | shared value |no|
| QCPAxisTickerPi | setPiSymbol | src/axis/axistickerpi.cpp:69 | shared value |no|
| QCPAxisTickerPi | setPiValue | src/axis/axistickerpi.cpp:80 | shared value |no|
| QCPAxisTickerPi | setPeriodicity | src/axis/axistickerpi.cpp:93 | shared value |no|
| QCPAxisTickerPi | setFractionStyle | src/axis/axistickerpi.cpp:102 | shared value |no|
| QCPAxisTickerText | setTicks | src/axis/axistickertext.cpp:80 | shared value |no|
| QCPAxisTickerText | setSubTickCount | src/axis/axistickertext.cpp:104 | shared value |no|
| QCPAxisTickerTime | setTimeFormat | src/axis/axistickertime.cpp:108 | shared value |no|
| QCPAxisTickerTime | setFieldWidth | src/axis/axistickertime.cpp:139 | shared value |no|
| QCPLabelRenderer | setDefaultRenderer | src/axis/labelrenderer.cpp:26 | shared value |no|
| QCPLabelPainterPrivate | setFont/setColor/setPadding/setRotation/... | src/axis/labelpainter.cpp:93 | shared value | no |

The axis rect's margin setters live in `src/layout.cpp` (`QCPLayoutGrid`), not in the rect: they are
handled with the layout family in Task 5.

## plottables

| class | setter | file:line | changes the drawing? | dirties |
| --- | --- | --- | --- | --- |
| QCPBarsGroup | setSpacingType | src/plottables/plottable-bars.cpp:121 | yes | `markDirty()` on every member bar (`bars()`) — a group has no layer |
| QCPBarsGroup | setSpacing | src/plottables/plottable-bars.cpp:136 | yes | `markDirty()` on every member bar (`bars()`) — a group has no layer |
| QCPBars | setData | src/plottables/plottable-bars.cpp:557 | data |`markDirty()` after the data is in place (the container has no signal)|
| QCPBars | setData | src/plottables/plottable-bars.cpp:573 | data |`markDirty()` after the data is in place (the container has no signal)|
| QCPBars | setWidth | src/plottables/plottable-bars.cpp:597 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPBars | setWidthType | src/plottables/plottable-bars.cpp:613 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPBars | setBarsGroup | src/plottables/plottable-bars.cpp:627 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPBars | setBaseValue | src/plottables/plottable-bars.cpp:653 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPBars | setStackingGap | src/plottables/plottable-bars.cpp:666 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPBars | moveBelow | src/plottables/plottable-bars.cpp:733 | yes | `markDirty()` — the restacking changes what is drawn |
| QCPBars | moveAbove | src/plottables/plottable-bars.cpp:770 | yes | `markDirty()` — the restacking changes what is drawn |
| QCPColorMapData | setSize | src/plottables/plottable-colormap.cpp:205 | yes | not dirtied: a data container has no layer; mutating it in place still needs `QCustomPlot::requestRepaint()` (follow-up: give it a back-pointer) |
| QCPColorMapData | setKeySize | src/plottables/plottable-colormap.cpp:259 | yes | not dirtied: a data container has no layer; mutating it in place still needs `QCustomPlot::requestRepaint()` (follow-up: give it a back-pointer) |
| QCPColorMapData | setValueSize | src/plottables/plottable-colormap.cpp:274 | yes | not dirtied: a data container has no layer; mutating it in place still needs `QCustomPlot::requestRepaint()` (follow-up: give it a back-pointer) |
| QCPColorMapData | setRange | src/plottables/plottable-colormap.cpp:289 | yes | not dirtied: a data container has no layer; mutating it in place still needs `QCustomPlot::requestRepaint()` (follow-up: give it a back-pointer) |
| QCPColorMapData | setKeyRange | src/plottables/plottable-colormap.cpp:305 | yes | not dirtied: a data container has no layer; mutating it in place still needs `QCustomPlot::requestRepaint()` (follow-up: give it a back-pointer) |
| QCPColorMapData | setValueRange | src/plottables/plottable-colormap.cpp:320 | yes | not dirtied: a data container has no layer; mutating it in place still needs `QCustomPlot::requestRepaint()` (follow-up: give it a back-pointer) |
| QCPColorMapData | setData | src/plottables/plottable-colormap.cpp:337 | data | not dirtied: a data container has no layer; mutating it in place still needs `QCustomPlot::requestRepaint()` (follow-up: give it a back-pointer) |
| QCPColorMapData | setCell | src/plottables/plottable-colormap.cpp:369 | yes | not dirtied: a data container has no layer; mutating it in place still needs `QCustomPlot::requestRepaint()` (follow-up: give it a back-pointer) |
| QCPColorMapData | setAlpha | src/plottables/plottable-colormap.cpp:404 | yes | not dirtied: a data container has no layer; mutating it in place still needs `QCustomPlot::requestRepaint()` (follow-up: give it a back-pointer) |
| QCPColorMap | setData | src/plottables/plottable-colormap.cpp:770 | data |`markDirty()` after the data is in place (the container has no signal)|
| QCPColorMap | setDataRange | src/plottables/plottable-colormap.cpp:799 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPColorMap | setDataScaleType | src/plottables/plottable-colormap.cpp:819 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPColorMap | setGradient | src/plottables/plottable-colormap.cpp:843 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPColorMap | setInterpolate | src/plottables/plottable-colormap.cpp:860 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPColorMap | setTightBoundary | src/plottables/plottable-colormap.cpp:880 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPColorMap | setColorScale | src/plottables/plottable-colormap.cpp:903 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPColorMap2 | setGapThreshold | src/plottables/plottable-colormap2.cpp:130 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPColorMap2 | setDataSource | src/plottables/plottable-colormap2.cpp:144 | data | `markDirty()` before the source is installed; the pipeline delivers later and marks again |
| QCPColorMap2 | setDataSource | src/plottables/plottable-colormap2.cpp:149 | data | `markDirty()` before the source is installed; the pipeline delivers later and marks again |
| QCPColorMap2 | setGradient | src/plottables/plottable-colormap2.cpp:176 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPColorMap2 | setColorScale | src/plottables/plottable-colormap2.cpp:185 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPColorMap2 | setDataRange | src/plottables/plottable-colormap2.cpp:219 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPColorMap2 | setDataScaleType | src/plottables/plottable-colormap2.cpp:231 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPColorMap2 | setContourLevels | src/plottables/plottable-colormap2.cpp:495 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPColorMap2 | setContourPen | src/plottables/plottable-colormap2.cpp:502 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPColorMap2 | setContourLabelEnabled | src/plottables/plottable-colormap2.cpp:509 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPColorMap2 | setAutoContourLevels | src/plottables/plottable-colormap2.cpp:518 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPCurve | setData | src/plottables/plottable-curve.cpp:207 | data |`markDirty()` after the data is in place (the container has no signal)|
| QCPCurve | setData | src/plottables/plottable-curve.cpp:223 | data |`markDirty()` after the data is in place (the container has no signal)|
| QCPCurve | setData | src/plottables/plottable-curve.cpp:241 | data |`markDirty()` after the data is in place (the container has no signal)|
| QCPCurve | setScatterStyle | src/plottables/plottable-curve.cpp:257 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPCurve | setScatterSkip | src/plottables/plottable-curve.cpp:274 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPCurve | setLineStyle | src/plottables/plottable-curve.cpp:287 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPErrorBars | setData | src/plottables/plottable-errorbar.cpp:159 | data |`markDirty()` after the data is in place (the container has no signal)|
| QCPErrorBars | setData | src/plottables/plottable-errorbar.cpp:173 | data |`markDirty()` after the data is in place (the container has no signal)|
| QCPErrorBars | setData | src/plottables/plottable-errorbar.cpp:189 | data |`markDirty()` after the data is in place (the container has no signal)|
| QCPErrorBars | setDataPlottable | src/plottables/plottable-errorbar.cpp:212 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPErrorBars | setErrorType | src/plottables/plottable-errorbar.cpp:237 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPErrorBars | setWhiskerWidth | src/plottables/plottable-errorbar.cpp:249 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPErrorBars | setSymbolGap | src/plottables/plottable-errorbar.cpp:262 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPFinancial | setData | src/plottables/plottable-financial.cpp:226 | data |`markDirty()` after the data is in place (the container has no signal)|
| QCPFinancial | setData | src/plottables/plottable-financial.cpp:242 | data |`markDirty()` after the data is in place (the container has no signal)|
| QCPFinancial | setChartStyle | src/plottables/plottable-financial.cpp:256 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPFinancial | setWidth | src/plottables/plottable-financial.cpp:269 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPFinancial | setWidthType | src/plottables/plottable-financial.cpp:285 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPFinancial | setTwoColored | src/plottables/plottable-financial.cpp:302 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPFinancial | setBrushPositive | src/plottables/plottable-financial.cpp:319 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPFinancial | setBrushNegative | src/plottables/plottable-financial.cpp:336 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPFinancial | setPenPositive | src/plottables/plottable-financial.cpp:353 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPFinancial | setPenNegative | src/plottables/plottable-financial.cpp:370 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPGraph | setData | src/plottables/plottable-graph.cpp:220 | data |`markDirty()` after the data is in place (the container has no signal)|
| QCPGraph | setData | src/plottables/plottable-graph.cpp:236 | data |`markDirty()` after the data is in place (the container has no signal)|
| QCPGraph | setLineStyle | src/plottables/plottable-graph.cpp:252 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPGraph | setScatterStyle | src/plottables/plottable-graph.cpp:267 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPGraph | setScatterSkip | src/plottables/plottable-graph.cpp:284 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPGraph | setChannelFillGraph | src/plottables/plottable-graph.cpp:302 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPGraph | setAdaptiveSampling | src/plottables/plottable-graph.cpp:356 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPGraph2 | setScatterColorGradient | src/plottables/plottable-graph2.cpp:68 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPGraph2 | setDataSource | src/plottables/plottable-graph2.cpp:106 | data | `markDirty()` before the source is installed; the pipeline delivers later and marks again |
| QCPGraph2 | setDataSource | src/plottables/plottable-graph2.cpp:111 | data | `markDirty()` before the source is installed; the pipeline delivers later and marks again |
| QCPPlottableGroup | setName | src/plottables/plottable-group.cpp:63 | yes | `markRowDirty()` — its legend row is the only surface a group draws |
| QCPPlottableGroup | setVisible | src/plottables/plottable-group.cpp:142 | yes | `markRowDirty()` — its legend row is the only surface a group draws |
| QCPHistogram | setDataSource | src/plottables/plottable-histogram.cpp:180 | data | `markDirty()` before the source is installed; the pipeline delivers later and marks again |
| QCPHistogram | setBinned | src/plottables/plottable-histogram.cpp:198 | data |`markDirty()` after the data is in place (the container has no signal)|
| QCPHistogram | setBins | src/plottables/plottable-histogram.cpp:219 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPHistogram | setFixedRange | src/plottables/plottable-histogram.cpp:228 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPHistogram | setNormalization | src/plottables/plottable-histogram.cpp:240 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPHistogram | setStyle | src/plottables/plottable-histogram.cpp:249 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPHistogram | setColumnColors | src/plottables/plottable-histogram.cpp:255 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPHistogram2D | setDataSource | src/plottables/plottable-histogram2d.cpp:77 | data | `markDirty()` before the source is installed; the pipeline delivers later and marks again |
| QCPHistogram2D | setDataSource | src/plottables/plottable-histogram2d.cpp:84 | data | `markDirty()` before the source is installed; the pipeline delivers later and marks again |
| QCPHistogram2D | setBins | src/plottables/plottable-histogram2d.cpp:118 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPHistogram2D | setKeyBinScale | src/plottables/plottable-histogram2d.cpp:131 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPHistogram2D | setValueBinScale | src/plottables/plottable-histogram2d.cpp:140 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPHistogram2D | setNormalization | src/plottables/plottable-histogram2d.cpp:159 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPHistogram2D | setGradient | src/plottables/plottable-histogram2d.cpp:168 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPHistogram2D | setColorScale | src/plottables/plottable-histogram2d.cpp:177 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPHistogram2D | setDataRange | src/plottables/plottable-histogram2d.cpp:211 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPHistogram2D | setDataScaleType | src/plottables/plottable-histogram2d.cpp:223 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPHistogram2D | setContourLevels | src/plottables/plottable-histogram2d.cpp:415 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPHistogram2D | setAutoContourLevels | src/plottables/plottable-histogram2d.cpp:422 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPHistogram2D | setContourPen | src/plottables/plottable-histogram2d.cpp:429 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPHistogram2D | setContourLabelEnabled | src/plottables/plottable-histogram2d.cpp:436 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPIntervals | setData | src/plottables/plottable-intervals.cpp:98 | data |`markDirty()` after the data is in place (the container has no signal)|
| QCPIntervals | setCategoryNames | src/plottables/plottable-intervals.cpp:184 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPIntervals | setCategoryColors | src/plottables/plottable-intervals.cpp:229 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPIntervals | setSelectedRows | src/plottables/plottable-intervals.cpp:423 | data | `markDirty()` first; the selection is stored and emitted afterwards |
| QCPIntervals | setEditable | src/plottables/plottable-intervals.cpp:805 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPIntervals | setEditModes | src/plottables/plottable-intervals.cpp:815 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPIntervals | setSnap | src/plottables/plottable-intervals.cpp:822 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPIntervals | setStackMode | src/plottables/plottable-intervals.cpp:831 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPIntervals | setCategoryOrder | src/plottables/plottable-intervals.cpp:842 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPIntervals | setSnapTimes | src/plottables/plottable-intervals.cpp:935 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPIntervals | setStyle | src/plottables/plottable-intervals.cpp:1152 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPMultiGraph | setDataSource | src/plottables/plottable-multigraph.cpp:88 | data | `markDirty()` before the source is installed; the pipeline delivers later and marks again |
| QCPMultiGraph | setDataSource | src/plottables/plottable-multigraph.cpp:122 | data | `markDirty()` before the source is installed; the pipeline delivers later and marks again |
| QCPMultiGraph | setComponentFillBrush | src/plottables/plottable-multigraph.cpp:431 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPMultiGraph | setComponentNames | src/plottables/plottable-multigraph.cpp:522 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPMultiGraph | setComponentColors | src/plottables/plottable-multigraph.cpp:533 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPMultiGraph | setComponentPens | src/plottables/plottable-multigraph.cpp:548 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPMultiGraph | setComponentInLegend | src/plottables/plottable-multigraph.cpp:563 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPMultiGraph | setComponentVisible | src/plottables/plottable-multigraph.cpp:588 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPMultiGraph | setLineStyle | src/plottables/plottable-multigraph.cpp:600 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPMultiGraph | setComponentLineStyle | src/plottables/plottable-multigraph.cpp:614 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPMultiGraph | setAdaptiveSampling | src/plottables/plottable-multigraph.cpp:623 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPMultiGraph | setGapThreshold | src/plottables/plottable-multigraph.cpp:632 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPMultiGraph | setColorValues | src/plottables/plottable-multigraph.cpp:784 | data |`markDirty()` after the data is in place (the container has no signal)|
| QCPMultiGraph | setColorValues | src/plottables/plottable-multigraph.cpp:813 | data |`markDirty()` after the data is in place (the container has no signal)|
| QCPMultiGraph | setColorGradient | src/plottables/plottable-multigraph.cpp:855 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPMultiGraph | setColorRange | src/plottables/plottable-multigraph.cpp:863 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPMultiGraph | setColorScaleType | src/plottables/plottable-multigraph.cpp:871 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPMultiGraph | setComponentSelection | src/plottables/plottable-multigraph.cpp:893 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPStatisticalBox | setData | src/plottables/plottable-statisticalbox.cpp:254 | data |`markDirty()` after the data is in place (the container has no signal)|
| QCPStatisticalBox | setData | src/plottables/plottable-statisticalbox.cpp:270 | data |`markDirty()` after the data is in place (the container has no signal)|
| QCPStatisticalBox | setWidth | src/plottables/plottable-statisticalbox.cpp:287 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPStatisticalBox | setWhiskerWidth | src/plottables/plottable-statisticalbox.cpp:303 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPStatisticalBox | setWhiskerPen | src/plottables/plottable-statisticalbox.cpp:322 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPStatisticalBox | setWhiskerBarPen | src/plottables/plottable-statisticalbox.cpp:339 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPStatisticalBox | setWhiskerAntialiased | src/plottables/plottable-statisticalbox.cpp:353 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPStatisticalBox | setMedianPen | src/plottables/plottable-statisticalbox.cpp:364 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPStatisticalBox | setOutlierStyle | src/plottables/plottable-statisticalbox.cpp:379 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPWaterfallGraph | setDataSource | src/plottables/plottable-waterfall.cpp:93 | data | `markDirty()` before the source is installed; the pipeline delivers later and marks again |
| QCPWaterfallGraph | setOffsetMode | src/plottables/plottable-waterfall.cpp:101 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPWaterfallGraph | setUniformSpacing | src/plottables/plottable-waterfall.cpp:109 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPWaterfallGraph | setOffsets | src/plottables/plottable-waterfall.cpp:117 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPWaterfallGraph | setNormalize | src/plottables/plottable-waterfall.cpp:125 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPWaterfallGraph | setGain | src/plottables/plottable-waterfall.cpp:134 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPSelectionDecorator | setPen | src/plottables/plottable.cpp:84 | yes | not dirtied: a decorator has no layer; the plottable that owns it marks its own layer when it restyles itself |
| QCPSelectionDecorator | setBrush | src/plottables/plottable.cpp:92 | yes | not dirtied: a decorator has no layer; the plottable that owns it marks its own layer when it restyles itself |
| QCPSelectionDecorator | setScatterStyle | src/plottables/plottable.cpp:104 | yes | not dirtied: a decorator has no layer; the plottable that owns it marks its own layer when it restyles itself |
| QCPSelectionDecorator | setUsedScatterProperties | src/plottables/plottable.cpp:118 | no | not dirtied: a decorator has no layer; the plottable that owns it marks its own layer when it restyles itself |
| QCPAbstractPlottable | setName | src/plottables/plottable.cpp:518 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPAbstractPlottable | setAntialiasedFill | src/plottables/plottable.cpp:533 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPAbstractPlottable | setAntialiasedScatters | src/plottables/plottable.cpp:547 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPAbstractPlottable | setPen | src/plottables/plottable.cpp:563 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPAbstractPlottable | setBrush | src/plottables/plottable.cpp:580 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPAbstractPlottable | setKeyAxis | src/plottables/plottable.cpp:599 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPAbstractPlottable | setValueAxis | src/plottables/plottable.cpp:617 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPAbstractPlottable | setSelection | src/plottables/plottable.cpp:643 | no |— (state only)|
| QCPAbstractPlottable | setSelectionDecorator | src/plottables/plottable.cpp:675 | no |— (state only)|
| QCPAbstractPlottable | setSelectable | src/plottables/plottable.cpp:702 | no |— (state only)|
| QCPAbstractPlottable | setWantsLegend | src/plottables/plottable.cpp:971 | yes |`QCPLayerable::markDirty()` — its own layer|
| QCPAbstractPlottable | setBusy | src/plottables/plottable.cpp:1179 | no |already invalidates (busy gate)|
| QCPAbstractPlottable | setBusyIndicatorSymbol | src/plottables/plottable.cpp:1218 | no |— (state only)|
| QCPAbstractPlottable | setBusyFadeAlpha | src/plottables/plottable.cpp:1228 | no |— (state only)|
| QCPAbstractPlottable | setBusyShowDelayMs | src/plottables/plottable.cpp:1238 | no |— (state only)|
| QCPAbstractPlottable | setBusyHideDelayMs | src/plottables/plottable.cpp:1248 | no |— (state only)|

## items

| class | setter | file:line | changes the drawing? | dirties |
| --- | --- | --- | --- | --- |
| QCPItemBracket | setPen | src/items/item-bracket.cpp:88 | yes | `markDirty()` (invalidates its layer) |
| QCPItemBracket | setSelectedPen | src/items/item-bracket.cpp:101 | yes | `markDirty()` (invalidates its layer) |
| QCPItemBracket | setLength | src/items/item-bracket.cpp:117 | yes | `markDirty()` (invalidates its layer) |
| QCPItemBracket | setStyle | src/items/item-bracket.cpp:130 | yes | `markDirty()` (invalidates its layer) |
| QCPItemCurve | setPen | src/items/item-curve.cpp:82 | yes | `markDirty()` (invalidates its layer) |
| QCPItemCurve | setSelectedPen | src/items/item-curve.cpp:95 | yes | `markDirty()` (invalidates its layer) |
| QCPItemCurve | setHead | src/items/item-curve.cpp:111 | yes | `markDirty()` (invalidates its layer) |
| QCPItemCurve | setTail | src/items/item-curve.cpp:127 | yes | `markDirty()` (invalidates its layer) |
| QCPItemEllipse | setPen | src/items/item-ellipse.cpp:81 | yes | `markDirty()` (invalidates its layer) |
| QCPItemEllipse | setSelectedPen | src/items/item-ellipse.cpp:94 | yes | `markDirty()` (invalidates its layer) |
| QCPItemEllipse | setBrush | src/items/item-ellipse.cpp:108 | yes | `markDirty()` (invalidates its layer) |
| QCPItemEllipse | setSelectedBrush | src/items/item-ellipse.cpp:122 | yes | `markDirty()` (invalidates its layer) |
| QCPItemHSpan | setRange | src/items/item-hspan.cpp:38 | yes | `markDirty()` (invalidates its layer) |
| QCPItemLine | setPen | src/items/item-line.cpp:72 | yes | `markDirty()` (invalidates its layer) |
| QCPItemLine | setSelectedPen | src/items/item-line.cpp:85 | yes | `markDirty()` (invalidates its layer) |
| QCPItemLine | setHead | src/items/item-line.cpp:101 | yes | `markDirty()` (invalidates its layer) |
| QCPItemLine | setTail | src/items/item-line.cpp:117 | yes | `markDirty()` (invalidates its layer) |
| QCPItemPixmap | setPixmap | src/items/item-pixmap.cpp:84 | yes | `markDirty()` (invalidates its layer) |
| QCPItemPixmap | setScaled | src/items/item-pixmap.cpp:99 | yes | `markDirty()` (invalidates its layer) |
| QCPItemPixmap | setPen | src/items/item-pixmap.cpp:117 | yes | `markDirty()` (invalidates its layer) |
| QCPItemPixmap | setSelectedPen | src/items/item-pixmap.cpp:130 | yes | `markDirty()` (invalidates its layer) |
| QCPItemRect | setPen | src/items/item-rect.cpp:77 | yes | `markDirty()` (invalidates its layer) |
| QCPItemRect | setSelectedPen | src/items/item-rect.cpp:90 | yes | `markDirty()` (invalidates its layer) |
| QCPItemRect | setBrush | src/items/item-rect.cpp:104 | yes | `markDirty()` (invalidates its layer) |
| QCPItemRect | setSelectedBrush | src/items/item-rect.cpp:118 | yes | `markDirty()` (invalidates its layer) |
| QCPItemRichText | setHtml | src/items/item-richtext.cpp:15 | yes | `markDirty()` (invalidates its layer) |
| QCPItemRSpan | setKeyRange | src/items/item-rspan.cpp:53 | yes | `markDirty()` (invalidates its layer) |
| QCPItemRSpan | setValueRange | src/items/item-rspan.cpp:66 | yes | `markDirty()` (invalidates its layer) |
| QCPAbstractSpanItem | setPen | src/items/item-spanbase.cpp:19 | yes | `markDirty()` (invalidates its layer) |
| QCPAbstractSpanItem | setSelectedPen | src/items/item-spanbase.cpp:28 | yes | `markDirty()` (invalidates its layer) |
| QCPAbstractSpanItem | setBrush | src/items/item-spanbase.cpp:37 | yes | `markDirty()` (invalidates its layer) |
| QCPAbstractSpanItem | setSelectedBrush | src/items/item-spanbase.cpp:46 | yes | `markDirty()` (invalidates its layer) |
| QCPAbstractSpanItem | setBorderPen | src/items/item-spanbase.cpp:55 | yes | `markDirty()` (invalidates its layer) |
| QCPAbstractSpanItem | setSelectedBorderPen | src/items/item-spanbase.cpp:64 | yes | `markDirty()` (invalidates its layer) |
| QCPGroupLegendItem | setExpanded | src/layoutelements/layoutelement-legend-group.cpp:40 | yes | `markDirty()` — the row's own layer |
| QCPAbstractSpanItem | setEdgePixel | src/items/item-spanbase.cpp:91 | no | — the edge position setters mark; this only records the pixel |
| QCPItemStraightLine | setPen | src/items/item-straightline.cpp:69 | yes | `markDirty()` (invalidates its layer) |
| QCPItemStraightLine | setSelectedPen | src/items/item-straightline.cpp:82 | yes | `markDirty()` (invalidates its layer) |
| QCPItemText | setColor | src/items/item-text.cpp:89 | yes | `markDirty()` (invalidates its layer) |
| QCPItemText | setSelectedColor | src/items/item-text.cpp:100 | yes | `markDirty()` (invalidates its layer) |
| QCPItemText | setPen | src/items/item-text.cpp:114 | yes | `markDirty()` (invalidates its layer) |
| QCPItemText | setSelectedPen | src/items/item-text.cpp:128 | yes | `markDirty()` (invalidates its layer) |
| QCPItemText | setBrush | src/items/item-text.cpp:142 | yes | `markDirty()` (invalidates its layer) |
| QCPItemText | setSelectedBrush | src/items/item-text.cpp:156 | yes | `markDirty()` (invalidates its layer) |
| QCPItemText | setFont | src/items/item-text.cpp:169 | yes | `markDirty()` (invalidates its layer) |
| QCPItemText | setSelectedFont | src/items/item-text.cpp:182 | yes | `markDirty()` (invalidates its layer) |
| QCPItemText | setText | src/items/item-text.cpp:196 | yes | `markDirty()` (invalidates its layer) |
| QCPItemText | setPositionAlignment | src/items/item-text.cpp:216 | yes | `markDirty()` (invalidates its layer) |
| QCPItemText | setTextAlignment | src/items/item-text.cpp:228 | yes | `markDirty()` (invalidates its layer) |
| QCPItemText | setRotation | src/items/item-text.cpp:240 | yes | `markDirty()` (invalidates its layer) |
| QCPItemText | setPadding | src/items/item-text.cpp:252 | yes | `markDirty()` (invalidates its layer) |
| QCPItemTracer | setPen | src/items/item-tracer.cpp:96 | yes | `markDirty()` (invalidates its layer) |
| QCPItemTracer | setSelectedPen | src/items/item-tracer.cpp:109 | yes | `markDirty()` (invalidates its layer) |
| QCPItemTracer | setBrush | src/items/item-tracer.cpp:122 | yes | `markDirty()` (invalidates its layer) |
| QCPItemTracer | setSelectedBrush | src/items/item-tracer.cpp:135 | yes | `markDirty()` (invalidates its layer) |
| QCPItemTracer | setSize | src/items/item-tracer.cpp:147 | yes | `markDirty()` (invalidates its layer) |
| QCPItemTracer | setStyle | src/items/item-tracer.cpp:161 | yes | `markDirty()` |
| QCPItemTracer | setGraph | src/items/item-tracer.cpp:180 | yes | `markDirty()` (invalidates its layer) |
| QCPItemTracer | setGraphKey | src/items/item-tracer.cpp:214 | yes | `markDirty()` (invalidates its layer) |
| QCPItemTracer | setInterpolating | src/items/item-tracer.cpp:233 | yes | `markDirty()` (invalidates its layer) |
| QCPItemVSpan | setRange | src/items/item-vspan.cpp:38 | yes | `markDirty()` (invalidates its layer) |
| QCPItemPosition | setType | setType | src/items/item.cpp:323 | `setTypeX()` and `setTypeY()` mark their item |
| QCPItemPosition | setType | setType | src/items/item.cpp:336 | `setTypeX()` and `setTypeY()` mark their item |
| QCPItemPosition | setType | setType | src/items/item.cpp:371 | `setTypeX()` and `setTypeY()` mark their item |
| QCPItemPosition | setParentAnchor | src/items/item.cpp:419 | yes | `markDirty()` (invalidates its layer) |
| QCPItemPosition | setParentAnchorX | src/items/item.cpp:436 | yes | `markDirty()` (invalidates its layer) |
| QCPItemPosition | setParentAnchorY | src/items/item.cpp:509 | yes | `markDirty()` (invalidates its layer) |
| QCPItemPosition | setCoords | src/items/item.cpp:591 | yes | `markDirty()` (invalidates its layer) |
| QCPItemPosition | setCoords | src/items/item.cpp:604 | yes | `markDirty()` (invalidates its layer) |
| QCPItemPosition | setAxes | src/items/item.cpp:767 | yes | `markDirty()` (invalidates its layer) |
| QCPItemPosition | setAxisRect | src/items/item.cpp:782 | yes | `markDirty()` (invalidates its layer) |
| QCPItemPosition | setPixelPosition | src/items/item.cpp:801 | yes | `markDirty()` (invalidates its layer) |
| QCPAbstractItem | setClipToAxisRect | src/items/item.cpp:1146 | yes | `markDirty()` (invalidates its layer) |
| QCPAbstractItem | setClipAxisRect | src/items/item.cpp:1162 | yes | `markDirty()` (invalidates its layer) |
| QCPAbstractItem | setSelectable | src/items/item.cpp:1181 | no | — (interaction or state only) |
| QCPAbstractItem | setSelected | src/items/item.cpp:1205 | yes | `markDirty()` (invalidates its layer) |


## polar

| class | setter | file:line | changes the drawing? | dirties |
| --- | --- | --- | --- | --- |
| QCPPolarAxisAngular | setBackground | src/polar/layoutelement-angularaxis.cpp:728 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setBackground | src/polar/layoutelement-angularaxis.cpp:748 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setBackground | src/polar/layoutelement-angularaxis.cpp:763 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setBackgroundScaled | src/polar/layoutelement-angularaxis.cpp:787 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setBackgroundScaledMode | src/polar/layoutelement-angularaxis.cpp:801 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setRangeDrag | src/polar/layoutelement-angularaxis.cpp:809 | no | — (interaction or state only) |
| QCPPolarAxisAngular | setRangeZoom | src/polar/layoutelement-angularaxis.cpp:814 | no | — (interaction or state only) |
| QCPPolarAxisAngular | setRangeZoomFactor | src/polar/layoutelement-angularaxis.cpp:819 | no | — (interaction or state only) |
| QCPPolarAxisAngular | setRange | src/polar/layoutelement-angularaxis.cpp:827 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setSelectableParts | src/polar/layoutelement-angularaxis.cpp:855 | no | — (interaction or state only) |
| QCPPolarAxisAngular | setSelectedParts | src/polar/layoutelement-angularaxis.cpp:882 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setRange | src/polar/layoutelement-angularaxis.cpp:896 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setRange | src/polar/layoutelement-angularaxis.cpp:922 | yes | `markDirty()` |
| QCPPolarAxisAngular | setRangeLower | src/polar/layoutelement-angularaxis.cpp:942 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setRangeUpper | src/polar/layoutelement-angularaxis.cpp:958 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setRangeReversed | src/polar/layoutelement-angularaxis.cpp:979 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setAngle | src/polar/layoutelement-angularaxis.cpp:987 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setTicker | src/polar/layoutelement-angularaxis.cpp:1009 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setTicks | src/polar/layoutelement-angularaxis.cpp:1032 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setTickLabels | src/polar/layoutelement-angularaxis.cpp:1045 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setTickLabelPadding | src/polar/layoutelement-angularaxis.cpp:1061 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setTickLabelFont | src/polar/layoutelement-angularaxis.cpp:1074 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setTickLabelColor | src/polar/layoutelement-angularaxis.cpp:1087 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setTickLabelRotation | src/polar/layoutelement-angularaxis.cpp:1104 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setTickLabelMode | src/polar/layoutelement-angularaxis.cpp:1112 | yes | `markDirty()` |
| QCPPolarAxisAngular | setNumberFormat | src/polar/layoutelement-angularaxis.cpp:1152 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setNumberPrecision | src/polar/layoutelement-angularaxis.cpp:1225 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setTickLength(int, int) | src/polar/layoutelement-angularaxis.cpp:1241 | yes | forwards to `setTickLengthIn()`/`setTickLengthOut()`, which mark |
| QCPPolarAxisAngular | setTickLength(int, int) | src/polar/layoutelement-angularaxis.cpp:1241 | yes | forwards to `setTickLengthIn()`/`setTickLengthOut()`, which mark |
| QCPPolarAxisAngular | setTickLength(int, int) | src/polar/layoutelement-angularaxis.cpp:1241 | yes | forwards to `setTickLengthIn()`/`setTickLengthOut()`, which mark |
| QCPPolarAxisAngular | setSubTicks | src/polar/layoutelement-angularaxis.cpp:1286 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setSubTickLength(int, int) | src/polar/layoutelement-angularaxis.cpp:1304 | yes | forwards to `setSubTickLengthIn()`/`setSubTickLengthOut()`, which mark |
| QCPPolarAxisAngular | setSubTickLength(int, int) | src/polar/layoutelement-angularaxis.cpp:1304 | yes | forwards to `setSubTickLengthIn()`/`setSubTickLengthOut()`, which mark |
| QCPPolarAxisAngular | setSubTickLength(int, int) | src/polar/layoutelement-angularaxis.cpp:1304 | yes | forwards to `setSubTickLengthIn()`/`setSubTickLengthOut()`, which mark |
| QCPPolarAxisAngular | setBasePen | src/polar/layoutelement-angularaxis.cpp:1345 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setTickPen | src/polar/layoutelement-angularaxis.cpp:1358 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setSubTickPen | src/polar/layoutelement-angularaxis.cpp:1371 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setLabelFont | src/polar/layoutelement-angularaxis.cpp:1384 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setLabelColor | src/polar/layoutelement-angularaxis.cpp:1399 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setLabel | src/polar/layoutelement-angularaxis.cpp:1411 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setLabelPadding | src/polar/layoutelement-angularaxis.cpp:1426 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setSelectedTickLabelFont | src/polar/layoutelement-angularaxis.cpp:1441 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setSelectedLabelFont | src/polar/layoutelement-angularaxis.cpp:1457 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setSelectedTickLabelColor | src/polar/layoutelement-angularaxis.cpp:1470 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setSelectedLabelColor | src/polar/layoutelement-angularaxis.cpp:1484 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setSelectedBasePen | src/polar/layoutelement-angularaxis.cpp:1497 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setSelectedTickPen | src/polar/layoutelement-angularaxis.cpp:1510 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisAngular | setSelectedSubTickPen | src/polar/layoutelement-angularaxis.cpp:1523 | yes | `markDirty()` |
| QCPPolarGraph | setName | src/polar/polargraph.cpp:192 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarGraph | setAntialiasedFill | src/polar/polargraph.cpp:206 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarGraph | setAntialiasedScatters | src/polar/polargraph.cpp:220 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarGraph | setPen | src/polar/polargraph.cpp:236 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarGraph | setBrush | src/polar/polargraph.cpp:253 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarGraph | setPeriodic | src/polar/polargraph.cpp:261 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarGraph | setKeyAxis | src/polar/polargraph.cpp:280 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarGraph | setValueAxis | src/polar/polargraph.cpp:303 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarGraph | setSelectable | src/polar/polargraph.cpp:321 | no | — (interaction or state only) |
| QCPPolarGraph | setSelection | src/polar/polargraph.cpp:355 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarGraph | setData | src/polar/polargraph.cpp:394 | no | — (interaction or state only) |
| QCPPolarGraph | setData | src/polar/polargraph.cpp:413 | no | — (interaction or state only) |
| QCPPolarGraph | setLineStyle | src/polar/polargraph.cpp:426 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarGraph | setScatterStyle | src/polar/polargraph.cpp:441 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarGraph | setSelectionDecorator | src/polar/polargraph.cpp:484 | no | — (interaction or state only) |
| QCPPolarGrid | setRadialAxis | src/polar/polargrid.cpp:72 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarGrid | setType | src/polar/polargrid.cpp:80 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarGrid | setSubGridType | src/polar/polargrid.cpp:88 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarGrid | setAntialiasedSubGrid | src/polar/polargrid.cpp:99 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarGrid | setAntialiasedZeroLine | src/polar/polargrid.cpp:110 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarGrid | setAngularPen | src/polar/polargrid.cpp:121 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarGrid | setAngularSubGridPen | src/polar/polargrid.cpp:132 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarGrid | setRadialPen | src/polar/polargrid.cpp:140 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarGrid | setRadialSubGridPen | src/polar/polargrid.cpp:148 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarGrid | setRadialZeroLinePen | src/polar/polargrid.cpp:156 | yes | `markDirty()` |
| QCPPolarAxisRadial | setRangeDrag | src/polar/radialaxis.cpp:250 | no | — (interaction or state only) |
| QCPPolarAxisRadial | setRangeZoom | src/polar/radialaxis.cpp:255 | no | — (interaction or state only) |
| QCPPolarAxisRadial | setRangeZoomFactor | src/polar/radialaxis.cpp:260 | no | — (interaction or state only) |
| QCPPolarAxisRadial | setScaleType | src/polar/radialaxis.cpp:279 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisRadial | setRange | src/polar/radialaxis.cpp:301 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisRadial | setSelectableParts | src/polar/radialaxis.cpp:331 | no | — (interaction or state only) |
| QCPPolarAxisRadial | setSelectedParts | src/polar/radialaxis.cpp:358 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisRadial | setRange | src/polar/radialaxis.cpp:377 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisRadial | setRange | src/polar/radialaxis.cpp:410 | yes | `markDirty()` |
| QCPPolarAxisRadial | setRangeLower | src/polar/radialaxis.cpp:425 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisRadial | setRangeUpper | src/polar/radialaxis.cpp:448 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisRadial | setRangeReversed | src/polar/radialaxis.cpp:476 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisRadial | setAngle | src/polar/radialaxis.cpp:484 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisRadial | setAngleReference | src/polar/radialaxis.cpp:492 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisRadial | setTicker | src/polar/radialaxis.cpp:513 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisRadial | setTicks | src/polar/radialaxis.cpp:536 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisRadial | setTickLabels | src/polar/radialaxis.cpp:549 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisRadial | setTickLabelPadding | src/polar/radialaxis.cpp:565 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisRadial | setTickLabelFont | src/polar/radialaxis.cpp:578 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisRadial | setTickLabelColor | src/polar/radialaxis.cpp:593 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisRadial | setTickLabelRotation | src/polar/radialaxis.cpp:610 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisRadial | setTickLabelMode | src/polar/radialaxis.cpp:618 | yes | `markDirty()` |
| QCPPolarAxisRadial | setNumberFormat | src/polar/radialaxis.cpp:658 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisRadial | setNumberPrecision | src/polar/radialaxis.cpp:731 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisRadial | setTickLength(int, int) | src/polar/radialaxis.cpp:749 | yes | forwards to `setTickLengthIn()`/`setTickLengthOut()`, which mark |
| QCPPolarAxisRadial | setTickLength(int, int) | src/polar/radialaxis.cpp:749 | yes | forwards to `setTickLengthIn()`/`setTickLengthOut()`, which mark |
| QCPPolarAxisRadial | setTickLength(int, int) | src/polar/radialaxis.cpp:749 | yes | forwards to `setTickLengthIn()`/`setTickLengthOut()`, which mark |
| QCPPolarAxisRadial | setSubTicks | src/polar/radialaxis.cpp:794 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisRadial | setSubTickLength(int, int) | src/polar/radialaxis.cpp:812 | yes | forwards to `setSubTickLengthIn()`/`setSubTickLengthOut()`, which mark |
| QCPPolarAxisRadial | setSubTickLength(int, int) | src/polar/radialaxis.cpp:812 | yes | forwards to `setSubTickLengthIn()`/`setSubTickLengthOut()`, which mark |
| QCPPolarAxisRadial | setSubTickLength(int, int) | src/polar/radialaxis.cpp:812 | yes | forwards to `setSubTickLengthIn()`/`setSubTickLengthOut()`, which mark |
| QCPPolarAxisRadial | setBasePen | src/polar/radialaxis.cpp:853 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisRadial | setTickPen | src/polar/radialaxis.cpp:866 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisRadial | setSubTickPen | src/polar/radialaxis.cpp:879 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisRadial | setLabelFont | src/polar/radialaxis.cpp:892 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisRadial | setLabelColor | src/polar/radialaxis.cpp:905 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisRadial | setLabel | src/polar/radialaxis.cpp:917 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisRadial | setLabelPadding | src/polar/radialaxis.cpp:932 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisRadial | setSelectedTickLabelFont | src/polar/radialaxis.cpp:947 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisRadial | setSelectedLabelFont | src/polar/radialaxis.cpp:963 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisRadial | setSelectedTickLabelColor | src/polar/radialaxis.cpp:976 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisRadial | setSelectedLabelColor | src/polar/radialaxis.cpp:990 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisRadial | setSelectedBasePen | src/polar/radialaxis.cpp:1003 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisRadial | setSelectedTickPen | src/polar/radialaxis.cpp:1016 | yes | `markDirty()` (invalidates its layer) |
| QCPPolarAxisRadial | setSelectedSubTickPen | src/polar/radialaxis.cpp:1029 | yes | `markDirty()` (invalidates its layer) |


## layout

| class | setter | file:line | changes the drawing? | dirties |
| --- | --- | --- | --- | --- |
| QCPLayoutElement | setOuterRect | src/layout.cpp:280 | no | — (interaction or state only) |
| QCPLayoutElement | setMargins | src/layout.cpp:301 | yes | `markLayoutAppearanceDirty()` — the element, and for a layout every child |
| QCPLayoutElement | setMinimumMargins | src/layout.cpp:321 | yes | `markLayoutAppearanceDirty()` — the element, and for a layout every child |
| QCPLayoutElement | setAutoMargins | src/layout.cpp:340 | yes | `markLayoutAppearanceDirty()` — the element, and for a layout every child |
| QCPLayoutElement | setMinimumSize(const QSize&) | src/layout.cpp:360 | yes | `markLayoutAppearanceDirty()` — the element, and for a layout every child |
| QCPLayoutElement | setMinimumSize(int, int) | src/layout.cpp:378 | no | forwards to the QSize overload |
| QCPLayoutElement | setMaximumSize(const QSize&) | src/layout.cpp:390 | yes | `markLayoutAppearanceDirty()` — the element, and for a layout every child |
| QCPLayoutElement | setMaximumSize(int, int) | src/layout.cpp:408 | no | forwards to the QSize overload |
| QCPLayoutElement | setSizeConstraintRect | src/layout.cpp:435 | yes | `markLayoutAppearanceDirty()` — the element, and for a layout every child |
| QCPLayoutElement | setMarginGroup | src/layout.cpp:459 | yes | `markLayoutAppearanceDirty()` |
| QCPLayoutGrid | setColumnStretchFactor | src/layout.cpp:1346 | yes | `markLayoutAppearanceDirty()` — the element, and for a layout every child |
| QCPLayoutGrid | setColumnStretchFactors | src/layout.cpp:1376 | yes | `markLayoutAppearanceDirty()` — the element, and for a layout every child |
| QCPLayoutGrid | setRowStretchFactor | src/layout.cpp:1411 | yes | `markLayoutAppearanceDirty()` — the element, and for a layout every child |
| QCPLayoutGrid | setRowStretchFactors | src/layout.cpp:1441 | yes | `markLayoutAppearanceDirty()` — the element, and for a layout every child |
| QCPLayoutGrid | setColumnSpacing | src/layout.cpp:1468 | yes | `markLayoutAppearanceDirty()` — the element, and for a layout every child |
| QCPLayoutGrid | setRowSpacing | src/layout.cpp:1481 | yes | `markLayoutAppearanceDirty()` — the element, and for a layout every child |
| QCPLayoutGrid | setWrap | src/layout.cpp:1506 | yes | `markLayoutAppearanceDirty()` — the element, and for a layout every child |
| QCPLayoutGrid | setFillOrder | src/layout.cpp:1539 | yes | `markLayoutAppearanceDirty()` — the element, and for a layout every child |
| QCPLayoutInset | setInsetPlacement | src/layout.cpp:2099 | yes | `markLayoutAppearanceDirty()` — the element, and for a layout every child |
| QCPLayoutInset | setInsetAlignment | src/layout.cpp:2120 | yes | `markLayoutAppearanceDirty()` — the element, and for a layout every child |
| QCPLayoutInset | setInsetRect | src/layout.cpp:2144 | yes | `markLayoutAppearanceDirty()` — the element, and for a layout every child |
| QCPLayer | setVisible | src/layer.cpp:164 | yes | `invalidatePaintBuffer()` — a stale translation must not survive a hidden layer |
| QCPLayer | setMode | src/layer.cpp:193 | no | — (interaction or state only) |
| QCPLayerable | setVisible | src/layer.cpp:592 | yes | `markDirty()` |
| QCPLayerable | setLayer | src/layer.cpp:617 | yes | `markDirty()` |
| QCPLayerable | setLayer | src/layer.cpp:627 | yes | `markDirty()` |
| QCPLayerable | setAntialiased | src/layer.cpp:651 | yes | `markDirty()` |
| QCPLayerable | setParentLayerable | src/layer.cpp:764 | yes | `invalidatePaintBuffer()` on the old and the new layer |
| QCPAbstractLegendItem | setFont | src/layoutelements/layoutelement-legend.cpp:96 | yes | `markDirty()` (invalidates its layer) |
| QCPAbstractLegendItem | setTextColor | src/layoutelements/layoutelement-legend.cpp:110 | yes | `markDirty()` (invalidates its layer) |
| QCPAbstractLegendItem | setSelectedFont | src/layoutelements/layoutelement-legend.cpp:125 | yes | `markDirty()` (invalidates its layer) |
| QCPAbstractLegendItem | setSelectedTextColor | src/layoutelements/layoutelement-legend.cpp:140 | yes | `markDirty()` (invalidates its layer) |
| QCPAbstractLegendItem | setSelectable | src/layoutelements/layoutelement-legend.cpp:154 | no | — (interaction or state only) |
| QCPAbstractLegendItem | setSelected | src/layoutelements/layoutelement-legend.cpp:171 | yes | `markDirty()` (invalidates its layer) |
| QCPLegend | setBorderPen | src/layoutelements/layoutelement-legend.cpp:536 | yes | `markDirty()` (invalidates its layer) |
| QCPLegend | setBrush | src/layoutelements/layoutelement-legend.cpp:547 | yes | `markDirty()` (invalidates its layer) |
| QCPLegend | setFont | src/layoutelements/layoutelement-legend.cpp:564 | yes | `markDirty()` (invalidates its layer) |
| QCPLegend | setTextColor | src/layoutelements/layoutelement-legend.cpp:586 | yes | `markDirty()` (invalidates its layer) |
| QCPLegend | setIconSize | src/layoutelements/layoutelement-legend.cpp:599 | yes | `markDirty()` (invalidates its layer) |
| QCPLegend | setIconSize | src/layoutelements/layoutelement-legend.cpp:609 | yes | `markDirty()` (invalidates its layer) |
| QCPLegend | setIconTextPadding | src/layoutelements/layoutelement-legend.cpp:627 | yes | `markDirty()` (invalidates its layer) |
| QCPLegend | setIconBorderPen | src/layoutelements/layoutelement-legend.cpp:641 | yes | `markDirty()` (invalidates its layer) |
| QCPLegend | setSelectableParts | src/layoutelements/layoutelement-legend.cpp:659 | no | — (interaction or state only) |
| QCPLegend | setSelectedParts | src/layoutelements/layoutelement-legend.cpp:705 | yes | `markDirty()` (invalidates its layer) |
| QCPLegend | setSelectedBorderPen | src/layoutelements/layoutelement-legend.cpp:737 | yes | `markDirty()` (invalidates its layer) |
| QCPLegend | setSelectedIconBorderPen | src/layoutelements/layoutelement-legend.cpp:750 | yes | `markDirty()` (invalidates its layer) |
| QCPLegend | setSelectedBrush | src/layoutelements/layoutelement-legend.cpp:764 | yes | `markDirty()` (invalidates its layer) |
| QCPLegend | setSelectedFont | src/layoutelements/layoutelement-legend.cpp:779 | yes | `markDirty()` (invalidates its layer) |
| QCPLegend | setSelectedTextColor | src/layoutelements/layoutelement-legend.cpp:799 | yes | `markDirty()` (invalidates its layer) |
| QCPTextElement | setText | src/layoutelements/layoutelement-textelement.cpp:206 | yes | `markDirty()` (invalidates its layer) |
| QCPTextElement | setTextFlags | src/layoutelements/layoutelement-textelement.cpp:234 | yes | `markDirty()` (invalidates its layer) |
| QCPTextElement | setFont | src/layoutelements/layoutelement-textelement.cpp:247 | yes | `markDirty()` (invalidates its layer) |
| QCPTextElement | setTextColor | src/layoutelements/layoutelement-textelement.cpp:260 | yes | `markDirty()` (invalidates its layer) |
| QCPTextElement | setSelectedFont | src/layoutelements/layoutelement-textelement.cpp:273 | yes | `markDirty()` (invalidates its layer) |
| QCPTextElement | setSelectedTextColor | src/layoutelements/layoutelement-textelement.cpp:287 | yes | `markDirty()` (invalidates its layer) |
| QCPTextElement | setSelectable | src/layoutelements/layoutelement-textelement.cpp:301 | no | — (interaction or state only) |
| QCPTextElement | setSelected | src/layoutelements/layoutelement-textelement.cpp:317 | yes | `markDirty()` (invalidates its layer) |
| QCPColorScale | setType | src/layoutelements/layoutelement-colorscale.cpp:202 | yes | `markDirty()` (invalidates its layer) |
| QCPColorScale | setDataRange | src/layoutelements/layoutelement-colorscale.cpp:261 | yes | `markDirty()` (invalidates its layer) |
| QCPColorScale | setDataScaleType | src/layoutelements/layoutelement-colorscale.cpp:293 | yes | `markDirty()` (invalidates its layer) |
| QCPColorScale | setGradient | src/layoutelements/layoutelement-colorscale.cpp:317 | yes | `markDirty()` (invalidates its layer) |
| QCPColorScale | setLabel | src/layoutelements/layoutelement-colorscale.cpp:335 | yes | `markDirty()` (invalidates its layer) |
| QCPColorScale | setBarWidth | src/layoutelements/layoutelement-colorscale.cpp:353 | yes | `markDirty()` (invalidates its layer) |
| QCPColorScale | setRangeDrag | src/layoutelements/layoutelement-colorscale.cpp:369 | no | — (interaction or state only) |
| QCPColorScale | setRangeZoom | src/layoutelements/layoutelement-colorscale.cpp:393 | no | — (interaction or state only) |


## item handles

Every item gets a `QCPItem<Name>Ops` handle with one `set<Position>(key, value)` per public
position member, plus one `apply` per item setter. Anchors stay read-only (they are inputs to
`setParentAnchor` on another position, not setters).

| item | handle | positions exposed |
| --- | --- | --- |
| QCPItemLine | QCPItemLineOps | setStart, setEnd |
| QCPItemCurve | QCPItemCurveOps | setStart, setStartDir, setEndDir, setEnd |
| QCPItemBracket | QCPItemBracketOps | setLeft, setRight |
| QCPItemEllipse | QCPItemEllipseOps | setTopLeft, setBottomRight |
| QCPItemPixmap | QCPItemPixmapOps | setTopLeft, setBottomRight |
| QCPItemRect | QCPItemRectOps | setTopLeft, setBottomRight |
| QCPItemText | QCPItemTextOps | setPosition |
| QCPItemRichText | QCPItemRichTextOps | — (inherits its parent's positions) |
| QCPItemTracer | QCPItemTracerOps | setPosition |
| QCPItemStraightLine | QCPItemStraightLineOps | setPoint1, setPoint2 |
| QCPItemVSpan | QCPItemVSpanOps | setLowerEdge, setUpperEdge |
| QCPItemHSpan | QCPItemHSpanOps | setLowerEdge, setUpperEdge |
| QCPItemRSpan | QCPItemRSpanOps | setLeftEdge, setRightEdge, setTopEdge, setBottomEdge |

`QCPItemRichTextOps` derives from `QCPItemTextOps` (a rich text item is a text item): it keeps
`setPosition` and the text setters and adds its own.

