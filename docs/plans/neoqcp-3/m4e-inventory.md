# Milestone 4e — fact inventory

Repo `NeoQCP-3.0`, HEAD `d05222b`, read-only survey (no builds, no edits).

Scope, from `docs/specs/2026-11-06-neoqcp-3-milestone-4-design.md`:

- order item 5 (M4e, :59-60): "Error bars, financial, statistical box, intervals, the remaining
  items, one span class, and folding the `QCPDrawn` adapter into the bases."
- call 1 (:65-67): `QCPErrorBars`, `QCPFinancial`, `QCPStatisticalBox`, `QCPIntervals` and the 1D
  `QCPHistogram` stay and are ported; the graph's per-component error bars and `QCPErrorBars` both
  stay.
- call 6: one span class with an orientation replaces the H and V spans; the third span class joins
  only if its code says so.
- answer 7 (:86-87): plottables and items are what M4 ports; axes, grids, legend, layout, selection
  and overlay keep their internal `draw(QCPPainter*)`, which survives as an internal virtual.
- answer 8 (:92-94): `QCPGraph`/`QCPGraph2` are untouched and deprecated; "every plottable draws
  through the drawing API" means every plottable 3.0 keeps.

"Ported" below means the class draws through `draw(QCPDrawContext&) const` (recording); "painter"
means it still overrides `draw(QCPPainter*)`.

## 1. Plottables still on the painter path

| Class | header | `draw(QCPPainter*)` cpp | legend icon | `selectTest` | `selectTestRect` | LOC h+cpp |
| --- | --- | --- | --- | --- | --- | --- |
| `QCPErrorBars : QCPAbstractPlottable, QCPPlottableInterface1D` | plottable-errorbar.h:70 | :520 (decl :157) | :590 (decl :158) | :493 (decl :143) | :417 | 220 + 1076 |
| `QCPFinancial : QCPAbstractPlottable1D<QCPFinancialData>` | plottable-financial.h:82 | :588 (decl :196) | :625 (decl :197) | :469 (decl :174) | :439 | 276 + 1161 |
| `QCPStatisticalBox : QCPAbstractPlottable1D<QCPStatisticalBoxData>` | plottable-statisticalbox.h:89 | :560 (decl :168), `drawStatisticalBox` :645 (decl :172) | :626 (decl :169) | :482 (decl :151) | :452 | 234 + 756 |
| `QCPIntervals : QCPAbstractPlottable, QCPPlottableInterface1D` | plottable-intervals.h:21 | :959 (decl :150) | :968 (decl :151) | :354 (decl :107) | :453 | 305 + 1222 |

GPU layers: none for error bars, financial or statistical box (`grep` for `rhiLayer` in their .cpp
finds nothing; they are pure painter classes). `QCPIntervals` is the exception — it has its own
`drawBarsOnGpu(QCPPainter*)` at plottable-intervals.cpp:1161 which uses the generic
`mParentPlot->plottableRhiLayer(mLayer)` (:1166), plus a `LabelLayer` created at :89 and exposed by
`QCPLayer* labelLayer() const` (:245).

Special cases per class:

- **QCPErrorBars**: its data has no keys (`QCPErrorBarsDataContainer = QVector<QCPErrorBarsData>`,
  :65; data class :37): the keys come from the attached plottable (`Q_PROPERTY(QCPAbstractPlottable*
  dataPlottable …)` :75, doc :59). `ErrorType` enum :91, `setErrorType` :119, `setWhiskerWidth`
  :120, `setSymbolGap` :121, ops :184 (setErrorType :204). A port must keep one key source: the
  recorded geometry is a function of the other plottable's keys, so the recording's
  `dataSourceIdentity()` (see §4) has to include it, or the cache goes stale when the attached
  plottable's data changes.
- **QCPFinancial**: `QCPFinancialData` (:38), representation enum (candlestick/OHLC) and width type
  (absolute / axis-rect ratio / plot coords) are the shape of the drawing; both representation and
  width type decide whether the geometry can be recorded in data space (compare `QCPBars` in §5).
- **QCPStatisticalBox**: one `draw` per box is factored through protected `drawStatisticalBox`
  (:172, cpp:645); overlay coordinates and whisker caps are the per-box geometry.
- **QCPIntervals**: the only one of the four with a GPU path and with a second layer (`LabelLayer`).
  Lane handling lives in `QCPLaneLayout : QObject` (src/plottables/lane-layout.h:19 — `Batch` :31-40,
  `laneIndex` :43, `laneIndices` :45, `laneNames` :46, `laneVisible` :52, one `changed` emission per
  batch). Its own comment at plottable-intervals.h:281 records the z-order constraint: drawing the
  intervals on the bars' layer would put them underneath. Helpers `rebuildBarsIfNeeded`,
  `rebuildBars`, `appendLaneBars`, `appendLabelRect`, `appendLaneBackdrops`, `barShape`, `fillColor`,
  `drawLabels`, `drawLaneNames`, `fillOpacity` are declared at plottable-intervals.h:153-175.

Tests and benches:

- `QCPErrorBars`: tests/auto/test-repaint and test-builder only — no dedicated class.
- `QCPFinancial`: tests/auto/test-qcpfinancial (dedicated), plus test-repaint, test-builder.
- `QCPStatisticalBox`: tests/auto/test-repaint and test-builder only.
- `QCPIntervals`: tests/auto/test-intervals (dedicated), plus test-plottable-group, test-ownership,
  test-signal-delivery.
- No benchmark row names any of the four (`grep` over tests/benchmark/benchmark.cpp finds nothing);
  the drawing gate rows are `QCPBars_*` / `QCPGpu_*`.

## 2. Items

`QCPAbstractItem` (item.h, 366+1719): `draw(QCPPainter*) override = 0` :308 (doc item.cpp:1243),
`selectTest` virtual :258, `positions()` :283, `positionsExtent(pixelPad, positions)` :311 (extent
comments :207 and :265), `clipToAxisRect` :222/:237, `setClipToAxisRect` item.cpp:1313,
`clipRect()` item.cpp:1456.

| Class | header | painter draw cpp | LOC h+cpp |
| --- | --- | --- | --- |
| `QCPItemBracket` | item-bracket.h:38 | :185 | 160 + 306 |
| `QCPItemCurve` | item-curve.h:39 | :169 | 146 + 218 |
| `QCPItemEllipse` | item-ellipse.h:38 | :159 | 157 + 243 |
| `QCPItemLine` | item-line.h:38 | :137 | 122 + 283 |
| `QCPItemPixmap` | item-pixmap.h:38 | :159 | 164 + 318 |
| `QCPItemRect` | item-rect.h:38 | :139 | 151 + 205 |
| `QCPItemStraightLine` | item-straightline.h:38 | :102 | 113 + 221 |
| `QCPItemTracer` | item-tracer.h:39 | :307 | 209 + 444 |
| `QCPItemHSpan` | item-hspan.h:12 | :102 | 85 + 190 |
| `QCPItemVSpan` | item-vspan.h:12 | :102 | 85 + 190 |
| `QCPItemRSpan` | item-rspan.h:12 | :151 | 112 + 287 |
| `QCPItemText` | item-text.h:40 | **ported**: `draw(QCPDrawContext&) const override` :152 | 66 + 153 |
| `QCPItemRichText` | item-richtext.h:12 | inherits `QCPItemText`'s recording | (in item-richtext) |

`QCPItemText` is the only ported item: `QCPItemText : QCPDrawn<QCPAbstractItem>` (item-text.h:40) and
it no longer declares a painter draw. `QCPItemRichText` derives from it and adds no draw.

Tests per item: test-item-position covers all; test-item-index covers bracket, curve, ellipse, line,
rect and text; test-repaint and test-builder cover all; test-richtext the rich text; test-hit-rule
text and the spans; test-drawable and test-coords text; test-multigraph and test-ownership text;
test-creation-mode line and spans; test-paintbuffer, test-qcpaxisrect, test-qcustomplot, test-handle
and test-selection-model line. Benches: only the generic `QCPItems_*` rows (benchmark.cpp:71-89 —
add/remove/replot/clear/delete/zoom/pan/click/first-frame/drag/restyle over 10k items), none per
class.

## 3. Spans: what the H, V and R spans share, and the one-class judgement

Shared base `QCPAbstractSpanItem : QCPAbstractItem` (item-spanbase.h:11, 60+109):

- appearance: `pen/selectedPen` (kept for compatibility, not drawn), `brush/selectedBrush`,
  `borderPen/selectedBorderPen`, with `mainPen()/mainBrush()/mainBorderPen()` selection accessors;
- interaction: `movable`, `mousePressEvent/mouseMoveEvent/mouseReleaseEvent` in the subclasses,
  `static void setEdgePixel(QCPItemPosition*, Qt::Orientation, double)` (moves one coordinate of an
  edge whatever the position type);
- drawing: `bool tryRhiDraw(QCPPainter*)` for the span RHI layer, and since I4
  `virtual bool usesLinkedAxis() const` (a span on a linked axis stays on the painter path).

| | HSpan | VSpan | RSpan |
| --- | --- | --- | --- |
| header | item-hspan.h:12 | item-vspan.h:12 | item-rspan.h:12 |
| LOC | 85+190 | 85+190 | 112+287 |
| edges | `lowerEdge`, `upperEdge` :35-36 | same | `leftEdge`, `rightEdge`, `topEdge`, `bottomEdge` :37-40 |
| range | `range()/setRange` :29-30 = the value range | = the key range | `left/right/top/bottom` get/set |
| positions pinned on | the key axis (the band stretches the whole key span) | the value axis | both axes for the opposite pair |
| edge motion | `setEdgePixel(edge, Qt::Vertical, …)` item-hspan.cpp:165-172 | `…Horizontal…` item-vspan.cpp:165-172 | both: `left/right Horizontal`, `top/bottom Vertical`, item-rspan.cpp:240-255 |
| ops | `setKey/setValue/setRange` :66-82 | same | rectangle setters |

Judgement for call 6: **H and V are one class with an orientation** — the two files are the same
size (85+190) and the same members, and only the axis the edges live on and the `Qt::Orientation`
passed to `setEdgePixel` differ. **R is not an orientation case**: it has four edges and draws a
rectangle (the intersection of both spans), so "one span class with an orientation" covers H+V only;
R either stays a separate class or needs a four-edge mode on top of the orientation (a rectangle is
not H or V).

Span tests: test-hspan, test-vspan, test-rspan, test-span-rhi (the GPU layer), plus test-creation-mode,
test-item-position, test-ownership, test-signal-delivery, test-repaint, test-builder, test-hit-rule and
test-linked-axes (the I4 linked-axis span/grid tests).

## 4. The `QCPDrawn` adapter (src/drawable.h 386, src/drawable.cpp 865)

What it adapts: any `QCPLayerable` subclass to the recording API. Four layers:

- `QCPDrawable` (drawable.h:56): the contract — `virtual void draw(QCPDrawContext&) const = 0` :62,
  `drawnDataRange()` :65, `drawLegendIcon(QCPDrawContext&) const = 0` :69, `hitTest`, and the cache
  keys `dataSourceIdentity()`, `dependencyRevision()`, `stallPixelOffset()`, `dataExtent()`.
- `QCPDrawnCore` (:94): the recording and its caches — `recordingCount()`/`recordedCommandCount()`
  test hooks, generation guard :102, `selectTest(const QCPDrawable&, const QPointF&, bool, QVariant*,
  const QCPDrawnView&)` :122, `dataRange`, `hitTest`, `paint`, `paintIcon`, `stallOffset`,
  `dataExtent`, `contentChanged()`.
- `QCPDrawnAdapter : QCPDrawable` (:216): answers the contract from the core, with a default
  `drawLegendIcon(icon) { draw(icon); }`, the protected pure `view()` and `mutable QCPDrawnCore mCore`.
- `QCPDrawnBase<Base> : Base, QCPDrawnAdapter` (:244): `markDirty()` → `mCore.contentChanged()`,
  `contentChanged()`, `selectTest` → `mCore.selectTest(...)`, and
  `void draw(QCPPainter* painter) final { mCore.paint(*this, *painter, view()); }` — the old painter
  virtual is *implemented* by the adapter instead of by the class.
- `QCPDrawnPlottable<Base>` (:284): `dataSourceIdentity()` = the data container, `dependencyRevision()`
  = its revision, `selectTest` into a whole-recording `QCPDataSelection`, `stallPixelOffset()` (the
  pan fast path), `getKeyRange`/`getValueRange` from `mCore.dataRange`, legend icon via
  `mCore.paintIcon`.
- `QCPDrawnItem<Base>` (:367): only `dataExtent()`.
- `using QCPDrawn = std::conditional_t<std::is_base_of_v<QCPAbstractItem, Base>, QCPDrawnItem<Base>,
  QCPDrawnPlottable<Base>>;` (:383-385).

Users (eight, all `QCPDrawn<…>`): `QCPBars` (plottable-bars.h:173), `QCPCurve` (plottable-curve.h:87),
`QCPHistogram` (plottable-histogram.h:20), `QCPHistogram2D` (:23), `QCPColorMap` (:115),
`QCPColorMap2` (:22), `QCPMultiGraph` (:29), `QCPItemText` (item-text.h:40).

What folding it into `QCPAbstractPlottable`/`QCPAbstractItem` would remove: the `QCPDrawnAdapter`
class and the `QCPDrawnBase<Base>` CRTP step (one extra base and one extra indirection per class), the
`QCPDrawnPlottable`/`QCPDrawnItem` split and the `std::conditional_t` alias, and the repeated
`using QCPDrawable::draw;` plumbing in every ported class. What stays is `QCPDrawable`'s contract,
`QCPDrawnCore` and `mCore`, moved to the two bases as a member plus the delegating methods
(`selectTest`, `markDirty`, `draw(QCPPainter*) final`, range/hit-test/stall/legend), with the
plottable-only pieces (data source identity, revision, range interpolation) on the plottable base and
`dataExtent` on the item base.

## 5. QCPBars — the model to copy

- `QCPBars : QCPDrawn<QCPAbstractPlottable1D<QCPBarsData>>` (plottable-bars.h:173);
  `void draw(QCPDrawContext& context) const override;` :265; `drawLegendIcon(QCPDrawContext&) const`
  :266; `QCPBarsOps` :294; `QCPBarsGroup : QObject` :42; `QCPBarsData` :131.
- `draw` (plottable-bars.cpp:983) → `recordSegments` (:1027) → `recordBar` (:1047). `recordBar` emits
  a data-space rectangle `context.rect(QCP::Data{key-half, base}, QCP::Data{key+half, base+value}, …)`
  (:1056) when the bar is not sheared, otherwise it forces the pixel path:
  `(void)context.transform(); // the geometry is in pixels: only a pan keeps this recording good`
  (:1061) and `context.rect(QCP::Pixels{…}, QCP::Pixels{…})` (:1062).
- A painter fallback for what the context cannot express goes through
  `context.painter([this](QCPPainter&, const QCPTransform&) { … })` (:1015).
- Legend icon: `drawLegendIcon(QCPDrawContext&)` (:1107) records the same geometry into an icon
  context instead of a plot context.
- Pan fast path: the recorded commands carry data-space geometry, so a pure translation is replayed
  by shifting pixels (`QCPDrawnPlottable::stallPixelOffset`, drawable.h:284 region) and the recording
  is only rebuilt when the transform changes materially.

## 6. Gaps and guesses

- No benchmark row measures any of the four plottables or any individual item class; the only
  drawing gate rows are the `QCPBars_*` / `QCPGpu_*` / `QCPDrawn_*` families and the generic
  `QCPItems_*` set. A port of these four therefore cannot be gated per class with the current
  benchmark file.
- The default implementation of `QCPAbstractItem::selectTest` (declared item.h:258) was not read;
  whether a ported item can keep the base implementation for hit testing is unverified (guess: it
  iterates `positions()` like the painter path does).
- Log-axis and vertical-key handling inside `QCPErrorBars`, `QCPFinancial` and `QCPStatisticalBox`
  were not read line by line; the painter code uses `pixelOrientation()` in places (M4c/M4d
  consumers list), so a port must keep the same sign convention (guess).
- Locale-aware number formatting and the `QCPStatisticalBox` overlay label side were not checked.
- `QCPFinancial`'s width-type enum and `QCPIntervals`' label placement were not enumerated; the port
  plan should read those two `draw` bodies before deciding the recorded command kinds.
