# matplotlib challenge: benchmark selection

Survey of 3.0 at 42956b6, milestone 6 evidence (spec `2026-11-14-neoqcp-3-milestone-6-design.md`).

Goal: 25 matplotlib gallery examples that are hard for a plotting API. For each one, how well NeoQCP 3.0
(branch neoqcp-3.0) builds the same plot, and how simply. The verdicts come from the user guide
(docs/*.md, docs/snippets/*.cpp) and a grep of src/ where the guide says nothing. Nothing was built.

Verdicts: **clean** = the API covers it simply. **awkward** = possible, but clumsy. **missing** = a capability is absent.
Python column: what a Shiboken6/PySide6 binding of the user's code path would trip on.

Gallery base URL: https://matplotlib.org/stable/gallery/

## Summary

| # | Example | Stress point | Verdict | Reason | Python |
|---|---|---|---|---|---|
| 1 | broken_axis | axis plumbing | awkward | no axis break: two rects, a link, hidden spines and hand-drawn break marks | ok |
| 2 | secondary_axis | axis plumbing | clean | `linkTo(source, forward, inverse)` does it, but the guide never mentions it | friction: two `std::function` callbacks, called on every range change |
| 3 | symlog_demo | axis plumbing | missing | `ScaleType` is a closed enum: linear or log, nothing else | ok |
| 4 | shared_axis_demo | axis plumbing / layout | awkward | no "share x" for N rects: one link and one alignment group by hand, inner tick labels hidden one by one | friction: `rect->add<T>()` template |
| 5 | ticks/date (concise) | date axis | awkward | one fixed date format; no format that changes with the zoom level, no offset label | friction: `QSharedPointer<QCPAxisTickerDateTime>::create()` |
| 6 | categorical_variables | categorical axis | awkward | no string keys: you number the categories and build a text ticker yourself | ok |
| 7 | gridspec_multicolumn | layout | awkward | cells do not span; nested grids cover some layouts, not all | ok |
| 8 | colorbar_placement | shared colour bar | awkward | one scale for many maps works; placing it beside a whole column of rects is manual layout + alignment group | ok |
| 9 | zoom_inset_axes | inset / zoom indicator | awkward | inset rect works; the indicator box and connectors are hand-kept items | friction: lambdas on `rangeChanged` to keep the box in sync |
| 10 | connectionstyle_demo | annotation | awkward | straight line or Bézier only; no elbow/arc connectors, no shrink, text + arrow are 2 items | ok |
| 11 | multiple_yaxis_with_spines | axis plumbing | clean | `addAxis(atRight)` stacks axes outward; colour each axis | friction: `add<QCPGraph>().setAxes()` template + fluent handle |
| 12 | pcolormesh_grids | irregular 2D field | awkward | centres + drifting y work; cell edges (N+1), 2D x and curvilinear meshes do not | friction: templated `setData(XC&&, YC&&, ZC&&)` |
| 13 | contour_label_demo | contour + clabel | awkward | contours exist only on top of a colour map; one pen, no per-level style, no label format | ok |
| 14 | colormap_normalizations | log/symlog/power norm, bad/over/under | missing | lin and log only; no symlog, power, two-slope or boundary norm; no over/under colours | ok |
| 15 | quiver_demo (+ plot_streamplot) | vector field | missing | no arrow-field or streamline plottable | n/a |
| 16 | violinplot | statistics | missing | no violin; closed `QCPCurve` shapes per category by hand | n/a |
| 17 | boxplot_demo | statistics | awkward | `QCPStatisticalBox` only draws: quartiles, whiskers and outliers are computed by the user | ok |
| 18 | hexbin_demo / hist2d | statistics, log colour | awkward | rectangular bins only (no hexagons); colour range needs a wait on the async `finished` | ok (signal) |
| 19 | errorbar_features | asymmetric x/y errors | awkward | two error-bar systems: graph columns hold bounds (not sizes) and only along y; x errors need `QCPErrorBars` | friction: `setData(keys, std::vector<columns>)` template |
| 20 | fill_between_demo | conditional fill | awkward | no `where=` mask and no "above/below" colours: NaN-masked helper columns | friction: same column template |
| 21 | stackplot_demo (+ hatch_demo) | stacked areas, hatch | awkward | no stacking for lines: cumulate sums by hand, chain fill-to-graph; hatch = fixed Qt patterns | ok |
| 22 | polar_bar (+ polar_scatter) | polar | missing | polar has one line graph: no wedges, no per-point size/colour | ok |
| 23 | scatter_with_legend | markers by size and colour | missing | colour by value exists; size by value and a size legend do not | ok |
| 24 | mathtext_demo | text / math | clean | `$...$` typeset in labels, legends, text items (tick labels stay plain) | ok |
| 25 | specgram_demo | spectrogram + time series | clean | `QCPColorMap2` + log axes + rect-owned colour scale; panel plumbing is the cost | friction: tickers via `QSharedPointer`, `qobject_cast` for legend |

Counts: clean 4, awkward 15, missing 6.

---

## 1. Broken axis

https://matplotlib.org/stable/gallery/subplots_axes_and_figures/broken_axis.html (~50 lines)

- **Hard:** one y axis cut in two ranges, outliers on top. Diagonal break marks drawn in axes coordinates.
- **Verdict: awkward.** Two `QCPAxisRect`s in a column, `linkTo` on the x axes, an alignment group, hide the facing axis lines and tick labels, then four `QCPItemLine`s with `ptAxisRectRatio` positions and `setClipToAxisRect(false)` for the marks. No concept of a break.
- **Classes:** `QCPAxisRect`, `QCPAxis::linkTo`, `QCPAlignmentGroup`, `QCPItemLine`, `QCPItemPosition::setType`.
- **API idea:** `axis->addBreak(QCPRange(from, to), style)`: one axis, the range has a gap, marks drawn by the axis.
- **Python:** ok.

## 2. Secondary axis with a function

https://matplotlib.org/stable/gallery/subplots_axes_and_figures/secondary_axis.html (~120 lines; deg/rad, frequency/period, date/day-of-year)

- **Hard:** the top axis shows the same data through a non-linear function and its inverse.
- **Verdict: clean.** `xAxis2->linkTo(xAxis, forward, inverse)` (axis-base.h) is exactly this. The guide's "Linking axes" still teaches two `QObject::connect` with `qOverload`, and never shows `linkTo`.
- **Classes:** `QCPAxisBase::linkTo`, `QCPAxisFunction`.
- **Python:** friction. `std::function<double(double)>` needs a custom converter, and it calls Python on every range change. A data form (`linkTo(src, QCPAxisLink::scale(a, b))`, `reciprocal()`, `log10()`) would bind trivially and stay off the GIL.

## 3. Symlog scale

https://matplotlib.org/stable/gallery/scales/symlog_demo.html (~40 lines)

- **Hard:** linear near zero, log beyond a threshold, both signs. Ticks on both regions.
- **Verdict: missing.** `QCPAxisBase::ScaleType` is `stLinear | stLogarithmic`. A user coordinate system can map data, but its axes and grid stay cartesian (coordinate-systems.md, Limits).
- **API idea:** a scale object (forward/inverse + default ticker) instead of the enum: `axis->setScale(QCPScale::symlog(linthresh))`. This also fixes pattern P3 (scale and ticker set separately).
- **Python:** ok if the scale is a value type.

## 4. Shared axes (and inverted)

https://matplotlib.org/stable/gallery/subplots_axes_and_figures/shared_axis_demo.html (~30 lines), with invert_axes.html

- **Hard:** `subplots(3, sharex=True)`: one call shares range, hides inner tick labels, aligns.
- **Verdict: awkward.** Per extra rect: `new QCPAxisRect`, `addElement`, `linkTo`, `setTickLabels(false)`, then one `QCPAlignmentGroup`. Inversion is clean (`setRangeReversed`).
- **Classes:** `QCPLayoutGrid::addElement`, `QCPAxisRect`, `linkTo`, `QCPAlignmentGroup::create`.
- **API idea:** `plot->addAxisRects(rows, cols, QCP::shareX)` returning a list of rect handles.
- **Python:** friction: plotting into the rects means `rect->add<QCPGraph>()`, a template the binding must expand per type.

## 5. Date tick labels (concise)

https://matplotlib.org/stable/gallery/ticks/date.html (~40 lines), see also ticks/date_concise_formatter.html

- **Hard:** labels that adapt from years to seconds as you zoom, with an offset ("2024-Mar") shown once.
- **Verdict: awkward.** `QCPAxisTickerDateTime::setDateTimeFormat` is one fixed format. Ticks fall on round dates, but the label does not change with the step. For a time-series library this is the most visible gap.
- **API idea:** `setDateTimeFormats({year: "yyyy", day: "MMM d", hour: "HH:mm", ...})` + `setOffsetLabel(true)`; make the concise mode the default.
- **Python:** friction: `QSharedPointer<T>::create()` for every ticker.

## 6. Categorical variables

https://matplotlib.org/stable/gallery/lines_bars_and_markers/categorical_variables.html (~25 lines)

- **Hard:** `ax.bar(["apple", "orange"], values)`: strings as keys, on bars, lines and scatters.
- **Verdict: awkward.** Number the categories 0..n-1, `QCPAxisTickerText::addTick(i, name)` per category, `setData(indices, values)`.
- **API idea:** `QCPAxisTickerCategory` owning a string list, plus `setData(QStringList, values)` on bars and graphs.
- **Python:** ok.

## 7. Uneven grids

https://matplotlib.org/stable/gallery/subplots_axes_and_figures/gridspec_multicolumn.html (~30 lines)

- **Hard:** one axes spans 3 columns, one 2 columns, one 2 rows.
- **Verdict: awkward.** "Cells do not span" (layout.md). Nested `QCPLayoutGrid`s express tree-shaped layouts only; interleaved spans (this example) need stretch-factor tricks. Then an alignment group so edges meet.
- **API idea:** `addElement(row, col, element, rowSpan, colSpan)`.
- **Python:** ok.

## 8. One colour bar for several maps

https://matplotlib.org/stable/gallery/subplots_axes_and_figures/colorbar_placement.html (~60 lines), images_contours_and_fields/multi_image.html

- **Hard:** a 2x2 grid of maps sharing a norm, one bar for a whole column or row.
- **Verdict: awkward.** Sharing is good: several maps on one `QCPColorScale` keep the same colours. Placement is not: the scale goes in its own cell, spanning rows means a nested grid, and its height needs an alignment group (top of the first rect, bottom of the last). The new rect-owned scale (`QCPAxisRect::colorScale()`, one per rect) does not cover "one bar for N rects". The guide still teaches the manual cell + group for the single-map case.
- **API idea:** `QCPColorScale::attachBeside(QList<QCPAxisRect*>, side)`: the scale sizes itself to the span of those rects.
- **Python:** ok.

## 9. Zoom inset

https://matplotlib.org/stable/gallery/subplots_axes_and_figures/zoom_inset_axes.html (~35 lines)

- **Hard:** an axes inside an axes, a box on the parent marking the zoomed region, two connector lines.
- **Verdict: awkward.** The inset is fine: `insetLayout()->addElement(rect, QRectF(...))`. The box is a `QCPItemRect` in parent data coordinates that you keep equal to the inset's ranges with two `rangeChanged` connections. The connectors join positions bound to two different rects, with clipping off.
- **API idea:** `parent->indicateInset(insetRect)`: one item, follows both ranges, draws box and connectors.
- **Python:** friction: the sync lambdas.

## 10. Annotation connection styles

https://matplotlib.org/stable/gallery/userdemo/connectionstyle_demo.html (~50 lines), with text_labels_and_annotations/annotation_demo.html

- **Hard:** text with an arrow to a data point: arc3, angle, angle3, arc, bar connectors; shrink at both ends; box styles.
- **Verdict: awkward.** `QCPItemText` + `QCPItemLine`/`QCPItemCurve`, tied by anchors (`setParentAnchor(label->left)`). Only straight or cubic Bézier, and the Bézier needs two control positions spelled out. No elbow (angle) or bar connectors, no shrink, rounded boxes only on rich text.
- **API idea:** `QCPItemAnnotation(text, target, textOffset)` with `setConnection(QCPConnection::angle(90, 0))` and `setShrink(px, px)`.
- **Python:** ok (public position members bind fine).

## 11. Several y axes

https://matplotlib.org/stable/gallery/subplots_axes_and_figures/multiple_yaxis_with_spines.html (~35 lines), two_scales.html

- **Hard:** three quantities, three y axes, axes coloured like their lines.
- **Verdict: clean.** `axisRect()->addAxis(QCPAxis::atRight)` stacks outward; `add<QCPGraph>().setAxes(x, y)`; `setBasePen`/`setLabelColor`. Same length as matplotlib.
- **Python:** friction: `add<QCPGraph>()` returns a fluent `qcp::HandleFor<T>`; both the template and the handle wrapper need per-type binding.

## 12. Irregular pcolormesh

https://matplotlib.org/stable/gallery/images_contours_and_fields/pcolormesh_grids.html (~70 lines), pcolormesh_levels.html

- **Hard:** non-uniform x and y, edges (N+1) vs centres (N), 2D coordinate arrays, gouraud shading.
- **Verdict: awkward.** `QCPColorMap2` takes sorted x centres and y per row or per cell, which covers spectrograms with drifting channels. Edges are not accepted, x is always 1D, and curvilinear (quad mesh) grids cannot be drawn. `QCPColorMap` is regular only.
- **API idea:** `setDataEdges(xEdges, yEdges, z)`; a `QCPQuadMesh` for 2D x/y.
- **Python:** friction: `setData(XC&&, YC&&, ZC&&)` is a template over containers; needs explicit numpy-span instantiations.

## 13. Contour labels

https://matplotlib.org/stable/gallery/images_contours_and_fields/contour_label_demo.html (~60 lines)

- **Hard:** contour lines alone (no image), labels with a custom format or a dict, negative levels dashed.
- **Verdict: awkward.** Contours are a feature of `QCPColorMap2`/`QCPHistogram2D`; contour-only means a transparent gradient. One pen for all levels; on screen always 1 px. Labels show the raw value, no formatter.
- **API idea:** a `QCPContour` plottable (or a component) with per-level pens and `setLabelFormat("%.1f m")`.
- **Python:** ok.

## 14. Colour normalisations and bad/over/under

https://matplotlib.org/stable/gallery/images_contours_and_fields/colormap_normalizations.html (~80 lines), with image_masked.html and contourf_log.html

- **Hard:** LogNorm, SymLogNorm, PowerNorm, TwoSlopeNorm, BoundaryNorm; NaN, over-range and under-range colours, extend triangles.
- **Verdict: missing.** Data scale is `ScaleType` (lin/log) again. NaN handling is good (`nhNanColor`). Over/under take the end colours, no own colours. Discrete bands come from `setLevelCount`, evenly spaced only.
- **API idea:** one `QCPColorNorm` value type (log, symlog, power, two-slope centre, boundaries) shared by maps and scales; `gradient.setUnderColor/setOverColor`.
- **Python:** ok.

## 15. Quiver (and streamplot)

https://matplotlib.org/stable/gallery/images_contours_and_fields/quiver_demo.html (~60 lines), plot_streamplot.html

- **Hard:** arrows on a grid, scaled to data, a reference-arrow key; streamlines with variable width and colour.
- **Verdict: missing.** Faking it with one `QCPItemLine` per arrow does not scale and is not a plottable (no legend, no rescale). Space physics needs this for flow and field maps.
- **API idea:** `QCPVectorField(x, y, u, v)` with `setScale`, `setColorValues`, `setKey(...)`; streamlines later.
- **Python:** n/a.

## 16. Violin plot

https://matplotlib.org/stable/gallery/statistics/violinplot.html (~50 lines)

- **Hard:** a mirrored density per category, optional means/medians/extrema.
- **Verdict: missing.** Closest is one closed `QCPCurve` per category with a brush.
- **API idea:** `QCPViolin::setDensities(key, values, density)`, like `QCPHistogram::setBinned`: user gives the density, no KDE in the library.
- **Python:** n/a.

## 17. Box plots

https://matplotlib.org/stable/gallery/statistics/boxplot_demo.html (~100 lines)

- **Hard:** stats computed from raw samples, notches, horizontal boxes, outlier markers.
- **Verdict: awkward.** `QCPStatisticalBox` draws boxes from numbers you computed (plottables.md says so). Horizontal = swap axes. No notches.
- **API idea:** `addSamples(key, samples, whis = 1.5)` that computes quartiles, whiskers and outliers.
- **Python:** ok.

## 18. Hexbin / hist2d with log counts

https://matplotlib.org/stable/gallery/statistics/hexbin_demo.html (~30 lines)

- **Hard:** density of many points, log colour, hexagonal cells.
- **Verdict: awkward.** `QCPHistogram2D` + `setDataScaleType(stLogarithmic)` gives the rectangular version. Hexagons are missing. `rescaleDataRange()` has to wait for the `finished` signal of the worker, a wart for a one-shot plot.
- **API idea:** auto-fit the data range on first result (as `rescaleDataRange(true)` does for `QCPColorMap2`); hexagonal binning as a bin shape.
- **Python:** ok (signal connect).

## 19. Asymmetric error bars

https://matplotlib.org/stable/gallery/statistics/errorbar_features.html (~40 lines)

- **Hard:** asymmetric errors on x and y, log axis, `errorevery`.
- **Verdict: awkward.** Two systems. `QCPGraph::setComponentErrorBars(value, lower, upper)` wants the bounds as extra columns (not error sizes), y only. X errors need a separate `QCPErrorBars` with `etKeyError`. No `errorevery`.
- **API idea:** one call `setErrors(column, QCPErrors::y(minus, plus).x(minus, plus))` on the graph; sizes or bounds by name.
- **Python:** friction: `setData(keys, std::vector<VC>)` with moved columns, a template.

## 20. fill_between with where

https://matplotlib.org/stable/gallery/lines_bars_and_markers/fill_between_demo.html (~90 lines)

- **Hard:** fill only where a condition holds, a different colour above and below, `interpolate=True` at crossings, `transform=ax.get_xaxis_transform()` fills across the full height.
- **Verdict: awkward.** `setComponentFillBetween` crosses with the edges but uses one brush. Masks mean extra NaN columns hidden with `setComponentVisible(false)`; the fill breaks at NaN, so crossings are cut, not interpolated. Full-height fills are spans (clean).
- **API idea:** `setComponentFillBetween(a, b, brushWhereAAbove, brushWhereBelow)` with crossing interpolation; an optional mask column.
- **Python:** friction: column template.

## 21. Stack plot (and hatching)

https://matplotlib.org/stable/gallery/lines_bars_and_markers/stackplot_demo.html (~50 lines), shapes_and_collections/hatch_demo.html

- **Hard:** stacked areas, streamgraph baseline; hatched fills.
- **Verdict: awkward.** Bars stack (`moveAbove`), lines do not. Cumulate the sums yourself, then fill each column to the one below. Hatching: Qt pattern brushes work but fall back to QPainter, with Qt's fixed density and no hatch colour separate from the brush.
- **API idea:** `QCPGraph::setStackMode(stacked | wiggle)` across components.
- **Python:** ok.

## 22. Polar bars (and polar scatter)

https://matplotlib.org/stable/gallery/pie_and_polar_charts/polar_bar.html (~25 lines), polar_scatter.html

- **Hard:** wedges by angle and width; scatter sized and coloured by value; partial sectors.
- **Verdict: missing.** polar.md: one graph type, `QCPPolarGraph`. No bars; no colour or size per point. A wind rose or a pitch-angle distribution cannot be drawn.
- **API idea:** let `QCPBars` run on a polar system (draw wedges through the mapping); share `setColorValues` with `QCPGraph`.
- **Python:** ok.

## 23. Scatter coloured and sized by value

https://matplotlib.org/stable/gallery/lines_bars_and_markers/scatter_with_legend.html (~70 lines)

- **Hard:** marker size and colour from two quantities, a legend for the sizes.
- **Verdict: missing.** `setColorValues` + `setGradient` cover colour. No per-point size, no size legend.
- **API idea:** `setSizeValues(values, QCPRange(minPx, maxPx))` + a legend item listing sample sizes.
- **Python:** ok.

## 24. Math text

https://matplotlib.org/stable/gallery/text_labels_and_annotations/mathtext_demo.html (~25 lines)

- **Hard:** TeX in title, labels, legend and text annotations.
- **Verdict: clean.** `$...$` is typeset everywhere (JKQTMathText, no TeX install). Tick labels stay plain, but `eb` covers powers of ten.
- **Python:** ok.

## 25. Spectrogram under a time series

https://matplotlib.org/stable/gallery/images_contours_and_fields/specgram_demo.html (~40 lines)

- **Hard:** signal on top, spectrogram below, shared time axis, colour bar beside the map.
- **Verdict: clean.** `QCPColorMap2` + log y + `stLogarithmic` data scale, gaps blank. The rect-owned colour scale (`rect->colorScale()`) removes the old cell + alignment group. The cost is plumbing: the gallery's space-weather plot spends ~25 of ~110 lines on rects, tickers, links and alignment. No `specgram` (FFT): that belongs to the user.
- **Python:** friction: `QSharedPointer<QCPAxisTickerLog>::create()` for each log axis; `qobject_cast<QCPGroupLegendItem*>` to expand the legend row.

---

## Patterns

Ranked by how many examples they hurt, then by how central they are to a scientist's plot.

1. **Multi-panel plumbing is manual (examples 1, 4, 7, 8, 9, 25).** Every extra panel is: new rect, `addElement`, link the axis, hide inner tick labels, alignment group. No spanning cells, no "share x", no colour bar spanning rects. This is the space-physics core use case. Fix: a subplots factory (`addAxisRects(rows, cols, share)`), cell spans, and a scale that attaches beside a list of rects.
2. **Scales are an enum, not an object (3, 14, and the ticker half of 5, 25).** Linear or log only, for axes and for colour data. The ticker has to be set separately each time. Fix: one `QCPScale`/`QCPColorNorm` value type (lin, log, symlog, power, two-slope, boundaries) that carries its default ticker.
3. **Missing plottables for whole families (15, 16, 22, 23, 12).** Vector fields, violins, polar bars, per-point size, curvilinear meshes. Polar is a tech preview with a single graph type. Fix: vector field first (space physics needs it), then let bars and colour/size values work on any system.
4. **Python friction on the main path (2, 4, 5, 9, 11, 12, 19, 20, 25).** The user always types `add<T>()`, fluent `HandleFor<T>`/`*Ops` wrappers, templated `setData(KC&&, std::vector<VC>&&)`, `QSharedPointer<Ticker>::create()`, and `std::function` for `linkTo`. Each needs hand-written binding code, and callbacks run Python on the render path. Fix: keep typed non-template entry points next to the templates (`addGraph`, `addColorMap2`, a span/buffer `setData` overload), tickers settable by value or enum preset, and data-form functions for links.
5. **Statistics plottables only draw, never compute (17, 18, 19, 16).** Box stats, histogram ranges after async binning, error sizes vs bounds. The user re-implements quartiles and waits on signals. Fix: `addSamples()` on boxes, auto-fit on first async result, errors given as sizes.
6. **Annotations are low-level parts (10, 9, 1).** Text, arrow and connector are separate items tied by anchors; connectors are straight or Bézier. Fix: one annotation item with connection styles and shrink; one zoom-indicator item.
7. **Fills lack conditions and stacking (20, 21).** One brush per band, no mask, no stack for lines. Fix: two-brush fill-between with crossing interpolation; component stack mode.
8. **Docs lag the API (2, 8, 25).** `linkTo` and the rect-owned colour scale exist in src but the guide still teaches `connect` + `qOverload` and the manual cell + alignment group. Cheap win: update axes.md and layout.md.
9. **Date labels do not adapt (5).** Single example, but every time-series plot sees it. Fix: zoom-dependent formats + offset label as the default.
