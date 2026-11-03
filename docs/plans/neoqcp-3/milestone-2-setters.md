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
| QCPGrid | setAntialiasedSubGrid | src/axis/axis.cpp:95 | yes | own layer |
| QCPGrid | setAntialiasedZeroLine | src/axis/axis.cpp:103 | yes | own layer |
| QCPGrid | setPen | src/axis/axis.cpp:111 | yes | own layer |
| QCPGrid | setSubGridPen | src/axis/axis.cpp:122 | yes | own layer |
| QCPGrid | setZeroLinePen | src/axis/axis.cpp:136 | yes | own layer |
| QCPAxis | setScaleType | src/axis/axis.cpp:605 | yes | rect (ticks change even when the range does not) |
| QCPAxis | setRange(QCPRange) | src/axis/axis.cpp:647 | range | already: `rangeDidChange()` → `markAffectedLayersDirty()` |
| QCPAxis | setRange(double, double) | src/axis/axis.cpp:721 | range | already, same path |
| QCPAxis | setRange(position, size, align) | src/axis/axis.cpp:753 | range | already, same path |
| QCPAxis | setRangeLower | src/axis/axis.cpp:767 | range | already, same path |
| QCPAxis | setRangeUpper | src/axis/axis.cpp:789 | range | already, same path |
| QCPAxis | setRangeReversed | src/axis/axis.cpp:816 | range | already: reverses then `setRange()`s |
| QCPAxis | moveRange | src/axis/axis.cpp:1401 | range | already: `setRange()` |
| QCPAxis | scaleRange(center, factor) | src/axis/axis.cpp:1426 | range | already: `setRange()` |
| QCPAxis | scaleRange(center, hFactor, vFactor) | src/axis/axis.cpp:1440 | range | already: `setRange()` |
| QCPAxis | setScaleRatio | src/axis/axis.cpp:1485 | range | already: `setRange()` |
| QCPAxis | rescale | src/axis/axis.cpp:1509 | range | already: `setRange()` |
| QCPAxis | setSelectableParts | src/axis/axis.cpp:676 | no | — (enables selection, draws nothing) |
| QCPAxis | setSelectedParts | src/axis/axis.cpp:703 | yes | rect (selection decorations of the tick labels and the label) |
| QCPAxis | setTicker | src/axis/axis.cpp:834 | yes | rect |
| QCPAxis | setTicks | src/axis/axis.cpp:853 | yes | rect |
| QCPAxis | setTickLabels | src/axis/axis.cpp:865 | yes | rect |
| QCPAxis | setTickLabelPadding | src/axis/axis.cpp:880 | yes | rect |
| QCPAxis | setTickLabelFont | src/axis/axis.cpp:894 | yes | rect |
| QCPAxis | setTickLabelColor | src/axis/axis.cpp:908 | yes | rect |
| QCPAxis | setTickLabelRotation | src/axis/axis.cpp:922 | yes | rect |
| QCPAxis | setTickLabelSide | src/axis/axis.cpp:938 | yes | rect |
| QCPAxis | setNumberFormat | src/axis/axis.cpp:979 | yes | rect (unknown codes are rejected, nothing changes) |
| QCPAxis | setNumberPrecision | src/axis/axis.cpp:1049 | yes | rect |
| QCPAxis | setTickLength | src/axis/axis.cpp:1066 | yes | rect |
| QCPAxis | setTickLengthIn | src/axis/axis.cpp:1078 | yes | rect |
| QCPAxis | setTickLengthOut | src/axis/axis.cpp:1093 | yes | rect |
| QCPAxis | setSubTicks | src/axis/axis.cpp:1109 | yes | rect |
| QCPAxis | setSubTickLength | src/axis/axis.cpp:1126 | yes | rect |
| QCPAxis | setSubTickLengthIn | src/axis/axis.cpp:1138 | yes | rect |
| QCPAxis | setSubTickLengthOut | src/axis/axis.cpp:1153 | yes | rect |
| QCPAxis | setBasePen | src/axis/axis.cpp:1167 | yes | rect |
| QCPAxis | setTickPen | src/axis/axis.cpp:1177 | yes | rect |
| QCPAxis | setSubTickPen | src/axis/axis.cpp:1187 | yes | rect |
| QCPAxis | setLabelFont | src/axis/axis.cpp:1197 | yes | rect |
| QCPAxis | setLabelColor | src/axis/axis.cpp:1211 | yes | rect |
| QCPAxis | setLabel | src/axis/axis.cpp:1220 | yes | rect |
| QCPAxis | setLabelRenderer | src/axis/axis.cpp:1229 | yes | rect |
| QCPAxis | setLabelPadding | src/axis/axis.cpp:1243 | yes | rect |
| QCPAxis | setPadding | src/axis/axis.cpp:1262 | yes | rect |
| QCPAxis | setOffset | src/axis/axis.cpp:1279 | yes | rect |
| QCPAxis | setSelectedTickLabelFont | src/axis/axis.cpp:1289 | yes | rect |
| QCPAxis | setSelectedLabelFont | src/axis/axis.cpp:1304 | yes | rect |
| QCPAxis | setSelectedTickLabelColor | src/axis/axis.cpp:1316 | yes | rect |
| QCPAxis | setSelectedLabelColor | src/axis/axis.cpp:1329 | yes | rect |
| QCPAxis | setSelectedBasePen | src/axis/axis.cpp:1339 | yes | rect |
| QCPAxis | setSelectedTickPen | src/axis/axis.cpp:1349 | yes | rect |
| QCPAxis | setSelectedSubTickPen | src/axis/axis.cpp:1359 | yes | rect |
| QCPAxis | setLowerEnding | src/axis/axis.cpp:1374 | yes | rect |
| QCPAxis | setUpperEnding | src/axis/axis.cpp:1389 | yes | rect |
| QCPAxisRect | setBackground(QPixmap) | src/layoutelements/layoutelement-axisrect.cpp:827 | yes | own layer |
| QCPAxisRect | setBackground(QBrush) | src/layoutelements/layoutelement-axisrect.cpp:846 | yes | own layer |
| QCPAxisRect | setBackground(QPixmap, scaled, mode) | src/layoutelements/layoutelement-axisrect.cpp:858 | yes | own layer |
| QCPAxisRect | setBackgroundScaled | src/layoutelements/layoutelement-axisrect.cpp:876 | yes | own layer |
| QCPAxisRect | setBackgroundScaledMode | src/layoutelements/layoutelement-axisrect.cpp:887 | yes | own layer |
| QCPAxisRect | setRangeDrag | src/layoutelements/layoutelement-axisrect.cpp:1001 | no | — |
| QCPAxisRect | setRangeZoom | src/layoutelements/layoutelement-axisrect.cpp:1022 | no | — |
| QCPAxisRect | setRangeDragAxes(QCPAxis*, QCPAxis*) | src/layoutelements/layoutelement-axisrect.cpp:1038 | no | — |
| QCPAxisRect | setRangeDragAxes(QCPAxisRect*) | src/layoutelements/layoutelement-axisrect.cpp:1057 | no | — |
| QCPAxisRect | setRangeDragAxes(orientations, axes) | src/layoutelements/layoutelement-axisrect.cpp:1076 | no | — |
| QCPAxisRect | setRangeZoomAxes(QCPAxis*, QCPAxis*) | src/layoutelements/layoutelement-axisrect.cpp:1112 | no | — |
| QCPAxisRect | setRangeZoomAxes(QCPAxisRect*) | src/layoutelements/layoutelement-axisrect.cpp:1131 | no | — |
| QCPAxisRect | setRangeZoomAxes(orientations, axes) | src/layoutelements/layoutelement-axisrect.cpp:1150 | no | — |
| QCPAxisRect | setRangeZoomFactor(double) | src/layoutelements/layoutelement-axisrect.cpp:1184 | no | — |
| QCPAxisRect | setRangeZoomFactor(h, v) | src/layoutelements/layoutelement-axisrect.cpp:1194 | no | — |
| QCPAxisRect | setupFullAxesBox | src/layoutelements/layoutelement-axisrect.cpp:609 | structural | no call: it only adds axes to a new rect, and they dirty their own layers when laid out |
