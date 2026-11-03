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

## plottables

| class | setter | file:line | changes the drawing? | dirties |
| --- | --- | --- | --- | --- |
| QCPBarsGroup | setSpacingType | src/plottables/plottable-bars.cpp:121 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPBarsGroup | setSpacing | src/plottables/plottable-bars.cpp:136 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPBars | setData | src/plottables/plottable-bars.cpp:557 | data | `markDirty()` after the data is in place (the container has no signal) |
| QCPBars | setData | src/plottables/plottable-bars.cpp:573 | data | `markDirty()` after the data is in place (the container has no signal) |
| QCPBars | setWidth | src/plottables/plottable-bars.cpp:597 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPBars | setWidthType | src/plottables/plottable-bars.cpp:613 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPBars | setBarsGroup | src/plottables/plottable-bars.cpp:627 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPBars | setBaseValue | src/plottables/plottable-bars.cpp:651 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPBars | setStackingGap | src/plottables/plottable-bars.cpp:664 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPColorMapData | setSize | src/plottables/plottable-colormap.cpp:205 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPColorMapData | setKeySize | src/plottables/plottable-colormap.cpp:259 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPColorMapData | setValueSize | src/plottables/plottable-colormap.cpp:274 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPColorMapData | setRange | src/plottables/plottable-colormap.cpp:289 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPColorMapData | setKeyRange | src/plottables/plottable-colormap.cpp:305 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPColorMapData | setValueRange | src/plottables/plottable-colormap.cpp:320 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPColorMapData | setData | src/plottables/plottable-colormap.cpp:337 | data | `markDirty()` after the data is in place (the container has no signal) |
| QCPColorMapData | setCell | src/plottables/plottable-colormap.cpp:369 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPColorMapData | setAlpha | src/plottables/plottable-colormap.cpp:404 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPColorMap | setData | src/plottables/plottable-colormap.cpp:770 | data | `markDirty()` after the data is in place (the container has no signal) |
| QCPColorMap | setDataRange | src/plottables/plottable-colormap.cpp:799 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPColorMap | setDataScaleType | src/plottables/plottable-colormap.cpp:819 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPColorMap | setGradient | src/plottables/plottable-colormap.cpp:843 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPColorMap | setInterpolate | src/plottables/plottable-colormap.cpp:860 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPColorMap | setTightBoundary | src/plottables/plottable-colormap.cpp:878 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPColorMap | setColorScale | src/plottables/plottable-colormap.cpp:901 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPColorMap2 | setGapThreshold | src/plottables/plottable-colormap2.cpp:131 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPColorMap2 | setDataSource | src/plottables/plottable-colormap2.cpp:144 | data | `markDirty()` after the data is in place (the container has no signal) |
| QCPColorMap2 | setDataSource | src/plottables/plottable-colormap2.cpp:149 | data | `markDirty()` after the data is in place (the container has no signal) |
| QCPColorMap2 | setGradient | src/plottables/plottable-colormap2.cpp:177 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPColorMap2 | setColorScale | src/plottables/plottable-colormap2.cpp:186 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPColorMap2 | setDataRange | src/plottables/plottable-colormap2.cpp:220 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPColorMap2 | setDataScaleType | src/plottables/plottable-colormap2.cpp:232 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPColorMap2 | setContourLevels | src/plottables/plottable-colormap2.cpp:502 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPColorMap2 | setContourPen | src/plottables/plottable-colormap2.cpp:509 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPColorMap2 | setContourLabelEnabled | src/plottables/plottable-colormap2.cpp:516 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPColorMap2 | setAutoContourLevels | src/plottables/plottable-colormap2.cpp:525 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPCurve | setData | src/plottables/plottable-curve.cpp:207 | data | `markDirty()` after the data is in place (the container has no signal) |
| QCPCurve | setData | src/plottables/plottable-curve.cpp:223 | data | `markDirty()` after the data is in place (the container has no signal) |
| QCPCurve | setData | src/plottables/plottable-curve.cpp:241 | data | `markDirty()` after the data is in place (the container has no signal) |
| QCPCurve | setScatterStyle | src/plottables/plottable-curve.cpp:257 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPCurve | setScatterSkip | src/plottables/plottable-curve.cpp:274 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPCurve | setLineStyle | src/plottables/plottable-curve.cpp:287 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPErrorBars | setData | src/plottables/plottable-errorbar.cpp:159 | data | `markDirty()` after the data is in place (the container has no signal) |
| QCPErrorBars | setData | src/plottables/plottable-errorbar.cpp:173 | data | `markDirty()` after the data is in place (the container has no signal) |
| QCPErrorBars | setData | src/plottables/plottable-errorbar.cpp:189 | data | `markDirty()` after the data is in place (the container has no signal) |
| QCPErrorBars | setDataPlottable | src/plottables/plottable-errorbar.cpp:212 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPErrorBars | setErrorType | src/plottables/plottable-errorbar.cpp:237 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPErrorBars | setWhiskerWidth | src/plottables/plottable-errorbar.cpp:249 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPErrorBars | setSymbolGap | src/plottables/plottable-errorbar.cpp:262 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPFinancial | setData | src/plottables/plottable-financial.cpp:226 | data | `markDirty()` after the data is in place (the container has no signal) |
| QCPFinancial | setData | src/plottables/plottable-financial.cpp:242 | data | `markDirty()` after the data is in place (the container has no signal) |
| QCPFinancial | setChartStyle | src/plottables/plottable-financial.cpp:256 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPFinancial | setWidth | src/plottables/plottable-financial.cpp:269 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPFinancial | setWidthType | src/plottables/plottable-financial.cpp:285 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPFinancial | setTwoColored | src/plottables/plottable-financial.cpp:302 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPFinancial | setBrushPositive | src/plottables/plottable-financial.cpp:319 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPFinancial | setBrushNegative | src/plottables/plottable-financial.cpp:336 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPFinancial | setPenPositive | src/plottables/plottable-financial.cpp:353 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPFinancial | setPenNegative | src/plottables/plottable-financial.cpp:370 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPGraph | setData | src/plottables/plottable-graph.cpp:220 | data | `markDirty()` after the data is in place (the container has no signal) |
| QCPGraph | setData | src/plottables/plottable-graph.cpp:236 | data | `markDirty()` after the data is in place (the container has no signal) |
| QCPGraph | setLineStyle | src/plottables/plottable-graph.cpp:252 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPGraph | setScatterStyle | src/plottables/plottable-graph.cpp:267 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPGraph | setScatterSkip | src/plottables/plottable-graph.cpp:284 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPGraph | setChannelFillGraph | src/plottables/plottable-graph.cpp:299 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPGraph | setAdaptiveSampling | src/plottables/plottable-graph.cpp:353 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPGraph2 | setScatterColorGradient | src/plottables/plottable-graph2.cpp:72 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPGraph2 | setDataSource | src/plottables/plottable-graph2.cpp:106 | data | `markDirty()` after the data is in place (the container has no signal) |
| QCPGraph2 | setDataSource | src/plottables/plottable-graph2.cpp:111 | data | `markDirty()` after the data is in place (the container has no signal) |
| QCPPlottableGroup | setName | src/plottables/plottable-group.cpp:63 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPPlottableGroup | setVisible | src/plottables/plottable-group.cpp:141 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPHistogram | setDataSource | src/plottables/plottable-histogram.cpp:180 | data | `markDirty()` after the data is in place (the container has no signal) |
| QCPHistogram | setBinned | src/plottables/plottable-histogram.cpp:198 | data | `markDirty()` after the data is in place (the container has no signal) |
| QCPHistogram | setBins | src/plottables/plottable-histogram.cpp:219 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPHistogram | setFixedRange | src/plottables/plottable-histogram.cpp:228 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPHistogram | setNormalization | src/plottables/plottable-histogram.cpp:240 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPHistogram | setStyle | src/plottables/plottable-histogram.cpp:249 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPHistogram | setColumnColors | src/plottables/plottable-histogram.cpp:255 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPHistogram2D | setDataSource | src/plottables/plottable-histogram2d.cpp:77 | data | `markDirty()` after the data is in place (the container has no signal) |
| QCPHistogram2D | setDataSource | src/plottables/plottable-histogram2d.cpp:84 | data | `markDirty()` after the data is in place (the container has no signal) |
| QCPHistogram2D | setBins | src/plottables/plottable-histogram2d.cpp:119 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPHistogram2D | setKeyBinScale | src/plottables/plottable-histogram2d.cpp:132 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPHistogram2D | setValueBinScale | src/plottables/plottable-histogram2d.cpp:139 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPHistogram2D | setNormalization | src/plottables/plottable-histogram2d.cpp:156 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPHistogram2D | setGradient | src/plottables/plottable-histogram2d.cpp:166 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPHistogram2D | setColorScale | src/plottables/plottable-histogram2d.cpp:175 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPHistogram2D | setDataRange | src/plottables/plottable-histogram2d.cpp:209 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPHistogram2D | setDataScaleType | src/plottables/plottable-histogram2d.cpp:221 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPHistogram2D | setContourLevels | src/plottables/plottable-histogram2d.cpp:415 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPHistogram2D | setAutoContourLevels | src/plottables/plottable-histogram2d.cpp:422 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPHistogram2D | setContourPen | src/plottables/plottable-histogram2d.cpp:429 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPHistogram2D | setContourLabelEnabled | src/plottables/plottable-histogram2d.cpp:436 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPIntervals | setData | src/plottables/plottable-intervals.cpp:98 | data | `markDirty()` after the data is in place (the container has no signal) |
| QCPIntervals | setCategoryNames | src/plottables/plottable-intervals.cpp:184 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPIntervals | setCategoryColors | src/plottables/plottable-intervals.cpp:229 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPIntervals | setSelectedRows | src/plottables/plottable-intervals.cpp:424 | data | `markDirty()` after the data is in place (the container has no signal) |
| QCPIntervals | setEditable | src/plottables/plottable-intervals.cpp:806 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPIntervals | setEditModes | src/plottables/plottable-intervals.cpp:814 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPIntervals | setSnap | src/plottables/plottable-intervals.cpp:821 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPIntervals | setStackMode | src/plottables/plottable-intervals.cpp:828 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPIntervals | setCategoryOrder | src/plottables/plottable-intervals.cpp:839 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPIntervals | setSnapTimes | src/plottables/plottable-intervals.cpp:932 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPIntervals | setStyle | src/plottables/plottable-intervals.cpp:1147 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPMultiGraph | setDataSource | src/plottables/plottable-multigraph.cpp:88 | data | `markDirty()` after the data is in place (the container has no signal) |
| QCPMultiGraph | setDataSource | src/plottables/plottable-multigraph.cpp:122 | data | `markDirty()` after the data is in place (the container has no signal) |
| QCPMultiGraph | setComponentFillBrush | src/plottables/plottable-multigraph.cpp:437 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPMultiGraph | setComponentNames | src/plottables/plottable-multigraph.cpp:528 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPMultiGraph | setComponentColors | src/plottables/plottable-multigraph.cpp:536 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPMultiGraph | setComponentPens | src/plottables/plottable-multigraph.cpp:548 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPMultiGraph | setComponentInLegend | src/plottables/plottable-multigraph.cpp:560 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPMultiGraph | setComponentVisible | src/plottables/plottable-multigraph.cpp:581 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPMultiGraph | setLineStyle | src/plottables/plottable-multigraph.cpp:593 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPMultiGraph | setComponentLineStyle | src/plottables/plottable-multigraph.cpp:604 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPMultiGraph | setAdaptiveSampling | src/plottables/plottable-multigraph.cpp:613 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPMultiGraph | setGapThreshold | src/plottables/plottable-multigraph.cpp:622 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPMultiGraph | setColorValues | src/plottables/plottable-multigraph.cpp:784 | data | `markDirty()` after the data is in place (the container has no signal) |
| QCPMultiGraph | setColorValues | src/plottables/plottable-multigraph.cpp:813 | data | `markDirty()` after the data is in place (the container has no signal) |
| QCPMultiGraph | setColorGradient | src/plottables/plottable-multigraph.cpp:845 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPMultiGraph | setColorRange | src/plottables/plottable-multigraph.cpp:849 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPMultiGraph | setColorScaleType | src/plottables/plottable-multigraph.cpp:853 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPMultiGraph | setComponentSelection | src/plottables/plottable-multigraph.cpp:871 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPStatisticalBox | setData | src/plottables/plottable-statisticalbox.cpp:254 | data | `markDirty()` after the data is in place (the container has no signal) |
| QCPStatisticalBox | setData | src/plottables/plottable-statisticalbox.cpp:270 | data | `markDirty()` after the data is in place (the container has no signal) |
| QCPStatisticalBox | setWidth | src/plottables/plottable-statisticalbox.cpp:287 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPStatisticalBox | setWhiskerWidth | src/plottables/plottable-statisticalbox.cpp:303 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPStatisticalBox | setWhiskerPen | src/plottables/plottable-statisticalbox.cpp:322 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPStatisticalBox | setWhiskerBarPen | src/plottables/plottable-statisticalbox.cpp:339 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPStatisticalBox | setWhiskerAntialiased | src/plottables/plottable-statisticalbox.cpp:353 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPStatisticalBox | setMedianPen | src/plottables/plottable-statisticalbox.cpp:364 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPStatisticalBox | setOutlierStyle | src/plottables/plottable-statisticalbox.cpp:379 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPWaterfallGraph | setDataSource | src/plottables/plottable-waterfall.cpp:93 | data | `markDirty()` after the data is in place (the container has no signal) |
| QCPWaterfallGraph | setOffsetMode | src/plottables/plottable-waterfall.cpp:101 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPWaterfallGraph | setUniformSpacing | src/plottables/plottable-waterfall.cpp:104 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPWaterfallGraph | setOffsets | src/plottables/plottable-waterfall.cpp:107 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPWaterfallGraph | setNormalize | src/plottables/plottable-waterfall.cpp:110 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPWaterfallGraph | setGain | src/plottables/plottable-waterfall.cpp:113 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPSelectionDecorator | setPen | src/plottables/plottable.cpp:82 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPSelectionDecorator | setBrush | src/plottables/plottable.cpp:90 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPSelectionDecorator | setScatterStyle | src/plottables/plottable.cpp:102 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPSelectionDecorator | setUsedScatterProperties | src/plottables/plottable.cpp:116 | no | — (state only) |
| QCPAbstractPlottable | setName | src/plottables/plottable.cpp:485 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPAbstractPlottable | setAntialiasedFill | src/plottables/plottable.cpp:503 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPAbstractPlottable | setAntialiasedScatters | src/plottables/plottable.cpp:517 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPAbstractPlottable | setPen | src/plottables/plottable.cpp:533 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPAbstractPlottable | setBrush | src/plottables/plottable.cpp:550 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPAbstractPlottable | setKeyAxis | src/plottables/plottable.cpp:569 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPAbstractPlottable | setValueAxis | src/plottables/plottable.cpp:587 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPAbstractPlottable | setSelection | src/plottables/plottable.cpp:613 | no | — (state only) |
| QCPAbstractPlottable | setSelectionDecorator | src/plottables/plottable.cpp:645 | no | — (state only) |
| QCPAbstractPlottable | setSelectable | src/plottables/plottable.cpp:672 | no | — (state only) |
| QCPAbstractPlottable | setWantsLegend | src/plottables/plottable.cpp:941 | yes | `QCPLayerable::markDirty()` — its own layer |
| QCPAbstractPlottable | setBusy | src/plottables/plottable.cpp:1149 | no | already invalidates (busy gate) |
| QCPAbstractPlottable | setBusyIndicatorSymbol | src/plottables/plottable.cpp:1190 | no | — (state only) |
| QCPAbstractPlottable | setBusyFadeAlpha | src/plottables/plottable.cpp:1200 | no | — (state only) |
| QCPAbstractPlottable | setBusyShowDelayMs | src/plottables/plottable.cpp:1210 | no | — (state only) |
| QCPAbstractPlottable | setBusyHideDelayMs | src/plottables/plottable.cpp:1220 | no | — (state only) |
