# Milestone 5 inventory — the old API, the old drawing path, and the line family

State of branch `neoqcp-3.0` at `6015c0a` (end of milestone 4j). Facts only, `file:line`.
Read-only except this file. No build, no test.

Sources read: `docs/specs/2026-11-02-neoqcp-3-object-model-design.md`, `2026-11-04-neoqcp-3-drawing-api-design.md`,
`2026-11-06-neoqcp-3-milestone-4-design.md`, `2026-11-09-neoqcp-3-coordinate-systems-design.md`,
every `docs/plans/neoqcp-3/milestone-*-rulings.md`, `item-scaling-rulings.md`, `backlog.md`,
`sciqlopplots-migration.md`, `changelog.txt` (the 3.0 sections), `docs/migrating.md`, the source tree,
the tests, examples, snippets, `documentation/doc-image-generator`, `tests/benchmark/benchmark.cpp`, and a
read-only grep of `/home/jeandet/Documents/prog/SciQLopPlots/{src,include}` (SciQLop and TSCat untouched).

---

## 1. Every promise that names milestone 5 (or the old API)

Counts in the table are literal: "kept" means it exists in the tree at `6015c0a`.

| # | Promise (source) | Kept where today | Why it was kept | What milestone 5 must do |
|---|---|---|---|---|
| 1 | 3.0 never ships two ways to do one thing (`docs/specs/2026-11-02-...:77`). | Old painter plottables, old graph lists, raw getters, `QCPMarginGroup`, painter legend icons. | 2.x code must compile until the last milestone. | Delete the old half. |
| 2 | The old drawing base is removed before 3.0 ships (milestone 5) (`2026-11-04-...:73-77`). | `QCPDrawn<Base>` is gone (changelog `:403-405`, `:420`); `QCPDrawnCore` and the `draw(QCPPainter*)`/`drawLegendIcon(QCPPainter*)` virtuals remain (`src/drawable.h:3-5`, `src/layer.h:314`, `src/plottables/plottable.h:345,355`, `src/items/item.h:359`). | Milestone 4e folded the adapter into the bases and left the painter virtuals as the replay. | Remove or keep the remaining painter virtuals deliberately; they are the "old base" now. |
| 3 | `QCPGraph` and `QCPGraph2` are deleted in milestone 5; `QCPMultiGraph` is renamed `QCPGraph` then (`2026-11-06-...:28-31,76-78,96-100`; changelog `:309`; `sciqlopplots-migration.md:32-34`). | `QCPGraph` (`src/plottables/plottable-graph.h:75`), `QCPGraph2` (`plottable-graph2.h:17`), `QCPMultiGraph` (`plottable-multigraph.h:29`), all included from `src/qcp.h:91-93`. | Examples, docs and tests move in milestone 5 (`2026-11-06-...:96-100`; `milestone-4a-rulings.md:65-66`). | Delete the two classes, rename `QCPMultiGraph` (and `QCPMultiGraphOps`, its files, its users), port every user (§4). |
| 4 | 2.x selection signals with arguments "go in milestone 5 with the old API" (`2026-11-06-...:74,131-132`). | Nothing: milestone 4a removed them instead of emitting them alongside (`milestone-4a-rulings.md:24-25`; changelog `:270-280`). | Two signal forms made `&Class::selectionChanged` ambiguous. | Nothing. The sentence is superseded; no argument-carrying selection signal exists (`src/plottables/plottable.h:313-314`, `src/items/item.h:341-342`, `src/axis/axis-base.h:192-193`, `src/layoutelements/layoutelement-legend.h:84-85,263-264`, `layoutelement-textelement.h:99-100`). |
| 5 | Getters that hand out plot objects return handles (`2026-11-02-...:331-335`). | Getters return raw pointers: `QCustomPlot::plottable()` `src/core.h:354-355`, `graph()` `:410-411`, `item()` `:420-421`, `layer()` `:433-434`, `axisRect()` `:446`, `axisRects()` `:447`, `axisRect->axis()/axes()` `layoutelement-axisrect.h:91-93`, `QCPAxis::axisRect()/grid()` `axis/axis.h:203,268`, `QCPAbstractPlottable::keyAxis()/valueAxis()/selectionDecorator()` `plottables/plottable.h:162,164,185`, `QCPItemPosition::keyAxis()/valueAxis()/axisRect()` `items/item.h:158-162`. | "Getters stay raw pointers until milestone 5" (`milestone-2-rulings.md:11`; plan `docs/superpowers/plans/2026-11-03-...-2.md:7,17`); the handle exists (`src/handle.h:44-104`) but production getters do not use it (`milestone-1-rulings.md:19`; changelog `:78`). | Switch the getters to handles (or say which stay raw). |
| 6 | `xAxis, yAxis, xAxis2, yAxis2, legend` stay raw and cleared by the destructors (`milestone-1-rulings.md:28`). | Public members `src/core.h:480-481`; cleared in `src/core.cpp:4594-4614` and by `~QCPAxis`/`~QCPLegend`. | The destructors clear them, so 2.x access keeps working. | Decide their 3.0 form (handle or accessor); they are the last public axis/legend pointers. |
| 7 | The protected `mKeyAxis`/`mValueAxis` stay until milestone 5 as the cartesian projection (`2026-11-09-...:129-132`). | `QCPAbstractPlottable::mKeyAxis, mValueAxis` `src/plottables/plottable.h:331-333`; `QCPItemPosition::mKeyAxis, mValueAxis` `src/items/item.h:202-203`; kept in sync by `setSystem`/`resolveAxes` (`plottable.cpp:890-892`). | Out-of-tree 2.x subclasses keep compiling (F23). | Remove the protected projections (and the 2.x axis-pair setters/constructors they serve) or keep them as documented projections. |
| 8 | `QCPMarginGroup` stays 2.x API until milestone 5 (`docs/superpowers/plans/2026-11-08-...-4d.md:65`, P-10). | Class `src/layout.h:38-59`, implementation `src/layout.cpp:32-181`, per-element links `layout.h:140-159`; marked `\deprecated` in `layout.cpp:40-41`; changelog `:383`. | 2.x code and docs use it until then. | Delete it and `QCPLayoutElement::setMarginGroup`/`marginGroup`/`marginGroups`, or keep the class. |
| 9 | A 1D plottable that paints itself (empty recording) keeps the 2.x nearest-point click; about 50 lines kept to milestone 5 (`milestone-4e-rulings.md:32-34`). | `QCPAbstractPlottable1D::selectTest` fallback `src/plottables/plottable1d.h:509-519`; gates on `recordsTheDrawing()` (`drawable.h:189`, `drawable.cpp:666-671`). | A 2.x subclass painting through `draw(QCPPainter*)` still selects. | Delete the fallback with the last painter plottable, or keep it for out-of-tree painter subclasses. |
| 10 | `QCPAbstractItem::draw(QCPPainter*)` is final (plan P-4, 4e); 2.x is kept working to milestone 5 (`milestone-4e-rulings.md:44-47`). | `src/items/item.h:359`, definition `item.cpp:1516-1519`. | Approved specific call; a 2.x item that paints itself already fails to compile and must record. | Nothing new: the painter entry is a replay, not an overridable path. |
| 11 | Examples, docs and tests move in milestone 5 (`2026-11-06-...:96-100`; `milestone-4a-rulings.md:65-66`: "examples/ is not built by meson… Add a build option when milestone 5 moves the examples"; `docs/superpowers/plans/2026-11-06-...-4b.md:350`: "the 58 files milestone 5 moves"). | 56 occurrences in 7 example files; 63 mentions in 17 doc pages; 537 test occurrences (non-benchmark). | Kept for 2.x parity. | Port them and add the examples build option. |
| 12 | The data sources' pixel line API (`getLinesAll`, `getOptimizedLineDataAll`, indexed forms) is pruned with the data-source work that retires `QCPGraph2` (`milestone-4b-rulings.md:58-60`; `backlog.md:129-131`). | `src/datasource/abstract-multi-datasource.h:23-66`; overrides in `resampled-multi-datasource.h:63-74,163-167`; `abstract-datasource.h:143-154`; `soa-datasource.h:102-118`. | Public interface; only tests use the pixel forms now (per ruling). | Remove them with `QCPGraph2`. |
| 13 | "Work that was deferred during the 3.0 milestones, to triage before 3.0 ships" (`docs/plans/neoqcp-3/backlog.md:2`). | The whole backlog file (141 lines). | Triage at the release. | Triage every item. |
| 14 | The deprecated graph classes carry `\deprecated` documentation but no `[[deprecated]]` attribute, "it would warn in the 58 files milestone 5 moves" (`docs/superpowers/plans/2026-11-06-...-4b.md:350`). | `src/plottables/plottable-graph.cpp:128`, `plottable-graph2.h:17`; no `Q_DECL_DEPRECATED` or `[[deprecated]]` anywhere in `src/` for these classes (the only `[[deprecated]]` are `RefreshPriority` aliases, `src/core.h:153-157`). | Avoid warnings before the port. | The classes go, so the attribute question goes with them. |
| 15 | `axesChanged()` was to be kept to milestone 5; amended to `systemChanged()` with no forwarder (`docs/superpowers/plans/2026-11-10-...-4g.md:260`; `.superpowers/m4g-plan-review.md:168-171`). | `systemChanged()` only (`src/plottables/plottable.h:320`); changelog `:483`. | NeoQCP-only API (rule D6). | Nothing: already renamed. |
| 16 | Colour scales stay a cartesian-area feature in 3.0 (`2026-11-09-...:191-195`; `backlog.md:75`). | `QCPAxisRect::colorScale()` `src/layoutelements/layoutelement-axisrect.h:131-133`; `QCPArea::colorScale()` not added (`milestone-4f-rulings.md:21-23`). | Deliberate 3.0 limit. | Keep the limit (or extend later, not M5). |
| 17 | Polar 2.1 tech-preview API breaks now, no shims (`2026-11-09-...:240-246`, D6). | Done in 4f/4h (`changelog.txt:449-456,565-568`). | The coordinate-systems addendum replaced the polar scope. | Nothing; listed so M5 does not re-open it. |
| 18 | `QCPItemSpan` keeps the old `setPen` "for API compatibility and not drawn" (`src/items/item-spanbase.h:32-34`). | `mPen`/`mSelectedPen` (`item-spanbase.h:44-45`), accessors `:18-20,49`. | 2.x span code calls `setPen`. | Decide: delete the ignored setter or draw it; it is the one setter that silently does nothing. |
| 19 | `QCP::phImmediateRefresh` is kept and set by default "for compatibility with QCustomPlot" (`src/global.h:228-230`). | The enum value and the default `plottingHints()` (`global.h:228-230`; `docs/migrating.md` table). | 2.x hint vocabulary. | Decide whether the 2.x plotting hints stay. |
| 20 | `rpQueuedRefresh`/`rpRefreshHint` are deprecated aliases (`src/core.h:153-157`; `docs/migrating.md:52-54`). | Enum values, no API plan names milestone 5 for them. | Source compatibility (docs/migrating says code still compiles with a warning). | Decide with the API-surface project, not by an M5 promise. |
| 21 | "2.x QCustomPlot API (`QCPColorMap`, `QCPColorScale`) keeps working until milestone 5" (`docs/superpowers/plans/2026-11-07-...-4c.md:43`); the same wording for 2.x API in `docs/superpowers/plans/2026-11-08-...-4e.md:79-82`. | Both classes still exist: `QCPColorMap` (`src/plottables/plottable-colormap.h`), `QCPColorScale` (`src/layoutelements/layoutelement-colorscale.h`); changelog `:322-333` keeps both in the new colour relation. | 2.x colour-map code. | No M5 decision says they go; the object-model spec leaves merging the classic/2 colour maps to project 3 (`2026-11-02-...:345-347`). |

### Deprecation markers actually in the tree

- `\deprecated` doc comments: `QCPGraph` (`src/plottables/plottable-graph.cpp:128`), `QCPGraph2`
  (`plottable-graph2.h:17`), `QCPMarginGroup` (`src/layout.cpp:40-41`).
- Compiler deprecation: `QCustomPlot::RefreshPriority::rpQueuedRefresh` and `rpRefreshHint`
  (`src/core.h:153-157`) only.
- No `Q_DECL_DEPRECATED` in `src/`. No `[[deprecated]]` on a class.

---

## 2. The old drawing path (painter overrides vs the recording)

### 2.1 Classes that still paint through a `QCPPainter` override

| Kind | Class | Header | Definition | Still needs |
|---|---|---|---|---|
| Plottable | `QCPGraph` | `src/plottables/plottable-graph.h:163-164` | `plottable-graph.cpp:470-545`; legend icon `:548-590` | The whole pixel draw path (adaptive sampling at `:1485,1493,1635,1642`, line/step/impulse builders `:1027-1310`, fill `:1319`, channel-fill polygon `:2159`, direct GPU fill into `QCPPlottableRhiLayer` `:1348-1361`, scatter `:704-762`, selection decorator `:510-544`) is a recording candidate; the class is deleted instead (`2026-11-06-...:96-100`). |
| Plottable | `QCPGraph2` | `plottable-graph2.h:152-153` | `plottable-graph2.cpp:477-802`; legend icon `:804-825` | Its L1/L2 pipeline, cached pixel lines and `ExtrusionCache` (`plottable-graph2.h:72-78,162-185`) plus `drawPolylineWithGpuFallback` (`cpp:623`) are the old GPU path. The L1/L2 pipeline is explicitly not ported (`2026-11-06-...:99-100`); the class is deleted. |
| Plottable base | `QCPAbstractPlottable` | `src/plottables/plottable.h:345` | `plottable.cpp:596-599` | The painter virtual is a replay of the recording for recorded classes. It is the extension point the 4e fallback serves (`plottable1d.h:509-519`). |
| Item base | `QCPAbstractItem` | `src/items/item.h:359` | `item.cpp:1516-1519` | Final; already a replay. Every concrete item records (`changelog.txt:399-405`). |
| Plottable helper | `QCPIntervals::LabelLayer` | `plottable-intervals.cpp:29-48` | `draw` `:35-44` | Its labels/selection/preview stay painter-drawn; it is a helper layerable, not a plottable. |
| Axis | `QCPAxis` | `src/axis/axis.h:406` | `axis.cpp:1843-...` | Painter only; milestone-4 answer 7 keeps it internal (`2026-11-06-...:91-95`). |
| Axis | `QCPGrid` | `axis.h:107` | `axis.cpp:189-...` | Painter only; same ruling. |
| Axis | `QCPAxisPainterPrivate` | `axis.h:442` | `axis.cpp:2121-...` | Painter helper (tick labels, line endings). |
| Polar axis | `QCPPolarAxisAngular` | `src/polar/angularaxis.h:209` | `angularaxis.cpp:244-...` | Painter only; same ruling. |
| Polar axis | `QCPPolarAxisRadial` | `polar/radialaxis.h:232` | `radialaxis.cpp:964-...` | Painter only; same ruling. |
| Polar grid | `QCPPolarGrid` | `polar/polargrid.h:118` | `polargrid.cpp:199-...` | Painter only; same ruling. |
| Area | `QCPArea` | `src/layoutelements/area.h:129` | `area.cpp:285-...` | Background; painter only. |
| Layout | `QCPTextElement` | `layoutelement-textelement.h:117` | `textelement.cpp:346-...` | Painter only. |
| Legend | `QCPLegend` | `layoutelement-legend.h:284` | `legend.cpp:1055-...` | Painter only. |
| Legend row | `QCPAbstractLegendItem` / `QCPPlottableLegendItem` | `legend.h:108,135` | `legend.cpp:356-...` | Painter only; icons replay or use a painter override. |
| Legend row | `QCPGroupLegendItem` | `layoutelement-legend-group.h:34` | `.cpp:160-...` | Painter only. |
| Legend row | `QCPIntervalsLegendItem` | `layoutelement-legend-intervals.h:18` | `.cpp:45-...` | Painter only. |
| Legend row | `QCPPlottableGroupLegendItem` | `src/plottables/plottable-group.h:84` | `.cpp:228-...` | Painter only. |
| Colour scale | `QCPColorScaleAxisRectPrivate` | `layoutelement-colorscale.h:61` | `.cpp:722-...` | Painter only (the scale's private rect). |
| Selection | `QCPSelectionRect` | `src/selectionrect.h:84` | `selectionrect.cpp:267-...` | Painter only; internal. |
| Overlay | `QCPOverlay` | `src/overlay.h:51` | `overlay.cpp:182-...` | Painter only; internal. |
| Creation | `QCPItemCreationState` | `src/item-creation-state.h:31` | `.cpp:40-...` | Painter only; internal. |
| Top level | `QCustomPlot` | `src/core.h:663` | `core.cpp:4301-...` | Top-level widget painter. |
| Layer | `QCPLayer` | `src/layer.h:142` | `layer.cpp:213-...` | Layer pass; calls each layerable's painter virtual. |
| Line ending | `QCPLineEnding` | `src/lineending.h:100-101` | `lineending.cpp:184,343` | Painter helper; `QCPLineEnding::draw` builds a `QPainterPath` per call (`backlog.md:65`). |

Painter legend icons are deliberately kept by four recorded classes (4e R-15, `milestone-4e-rulings.md:71-72`):
`QCPFinancial` (`plottable-financial.h:197`), `QCPStatisticalBox` (`plottable-statisticalbox.h:170`),
`QCPErrorBars` (`plottable-errorbar.h:175`), `QCPIntervals` (`plottable-intervals.h:152`), plus
`QCPGraph`/`QCPGraph2` above. The base painter icon is the replay (`plottable.h:355`, `plottable.cpp:712-715`).

### 2.2 The adapter/bridge that keeps both paths

- `QCPDrawable` contract: `draw(QCPDrawContext&)`, `drawnDataRange()`, `hitTest()`,
  `drawLegendIcon(QCPDrawContext&)` — `src/drawable.h:87-116`.
- `QCPDrawnView`, the per-frame view: `src/drawable.h:38-65`; built for an item/plottable at
  `drawable.cpp:478-555`.
- `QCPDrawnCore` (the one recording per object, its keys, resolve cache, raster fallback,
  extrusion cache, re-entry guard): `src/drawable.h:140-253`, state at `drawable.cpp:319-...`.
- `QCPDrawnCore::recordsTheDrawing()` — "records on demand, and an empty recording means the object
  paints through `draw(QCPPainter*)` instead" — `drawable.h:187-189`, `drawable.cpp:666-671`. Its one
  caller is the 1D hit-test fallback `plottable1d.h:518`.
- `QCPAbstractPlottable::draw(QCPPainter*)` replays the recording (`plottable.cpp:596-599`); a class
  that still paints overrides it. `QCPAbstractItem::draw(QCPPainter*)` is final and replays
  (`item.h:359`, `item.cpp:1516-1519`).
- Painter-callback command: `PainterCmd` (`src/painting/draw-commands.h:235-244`),
  `QCPDrawContext::painter(callback)` (`draw-context.h:236`, `draw-context.cpp:690-...`), and
  `recordEveryFrame()` for pixel-only positions (`draw-context.h:226`). Call sites:
  `polar/polargraph.cpp:709`, `plottables/plottable-financial.cpp:623`,
  `plottable-curve.cpp:502`, `plottable-bars.cpp:1049`, `plottable-errorbar.cpp:588`,
  `plottable-statisticalbox.cpp:597`, `items/item-richtext.cpp:63`.
- Selection decoration is still a painter API: `QCPSelectionDecorator::applyPen/applyBrush/
  drawDecoration` (`plottables/plottable.h:84-90`, `plottable.cpp:139-196`). Recorded classes call
  `drawDecoration` from a painter callback: bars `plottable-bars.cpp:1049-1053`, curve
  `plottable-curve.cpp:502-506`, error bars `plottable-errorbar.cpp:588-592`, financial
  `plottable-financial.cpp:623-627`, statistical box `plottable-statisticalbox.cpp:597-601`.
  `QCPGraph` calls it directly in its painter draw (`plottable-graph.cpp:543-544`).
- `QCPDrawn<Base>` no longer exists (changelog `:403-405,420`); `src/drawable.h:3-5` still describes
  "answering the old drawing virtuals from the recording", which is the remaining bridge.

### 2.3 What the M4 rulings say about the non-plottable painters

Milestone-4 answer 7 (`2026-11-06-...:91-95`) explicitly says axes, grids, the legend, layout
elements, the selection rect and the overlay keep their internal `draw(QCPPainter*)` and that
`QCPLayerable::draw(QCPPainter*)` survives as an internal virtual. The drawing spec's "old base is
removed … (milestone 5)" (`2026-11-04-...:73-77`) is broader. The two statements are the design
choice tied in "Open questions" below.

---

## 3. The line family

### 3.1 The classes and their bases

| Class | Base | File:line |
|---|---|---|
| `QCPGraph` | `QCPAbstractPlottable1D<QCPGraphData>` | `plottables/plottable-graph.h:75` |
| `QCPGraph2` | `QCPAbstractPlottable` + `QCPPlottableInterface1D` | `plottables/plottable-graph2.h:18` |
| `QCPMultiGraph` | `QCPAbstractLine` + `QCPPlottableInterface1D` | `plottables/plottable-multigraph.h:29` |
| `QCPWaterfallGraph` | `QCPMultiGraph` | `plottables/plottable-waterfall.h:57` |
| `QCPCurve` | `QCPAbstractPlottable1D<QCPCurveData, QCPAbstractLine>` | `plottables/plottable-curve.h:87` |
| `QCPAbstractLine` (the shared line base) | `QCPAbstractColorMapped` | `plottables/plottable-line.h:16` |

Milestone 4 built the graph inside `QCPMultiGraph` and decided the rename happens in milestone 5
(`2026-11-06-...:28-31,76-78`). `QCPMultiGraph` occurrences: 467 in 41 files across
`src/`, `tests/`, `examples/`, `docs/snippets/`, `documentation/` (grep at `6015c0a`).

### 3.2 Feature matrix: what the old graph has and whether the replacement has it

| Feature | `QCPGraph` | `QCPGraph2` | `QCPMultiGraph` (future `QCPGraph`) | `QCPCurve` |
|---|---|---|---|---|
| Line styles, all six incl. steps and impulses | own `LineStyle` `plottable-graph.h:92-110`; used `cpp:518-528` | own `LineStyle` `plottable-graph2.h:25-33` | `QCPAbstractLine::LineStyle` `plottable-line.h:20-21`; per component `Component::lineStyle` `:33`; `setLineStyle` `:56-58`; step/impulse builders `plottable-multigraph.cpp:1203-1216` | aliases the shared enum `plottable-curve.h:102`; refuses step/impulse `:140-141` (4b behaviour, `changelog.txt:314`) |
| Scatter style + skip | `setScatterStyle`/`setScatterSkip` `plottable-graph.h:136-137` | `:89-92` | base `setScatterStyle`/`setComponentScatterStyle`/`setScatterSkip` `plottable-line.h:59-62`; markers `plottable-multigraph.cpp:1238-1257` | getter `plottable-curve.h:110`, setter inherited (`plottable-line.h:59`); markers `cpp:150-151` |
| Scatter max points | no | `setScatterMaxPoints` `plottable-graph2.h:104-105` | no | no |
| Adaptive sampling | `adaptiveSampling`/`setAdaptiveSampling` `plottable-graph.h:129,139`; used `cpp:1485,1493,1635,1642` | `:108-116` | `:123-124`; used `plottable-multigraph.cpp:1142-1143` | no (curves record whole) |
| Gap detection (key spacing) | no (NaN only; `docs/graphs.md:289,303`) | `setGapThreshold` `plottable-graph2.h:119-127` | `:126-127` | no |
| Fill under the line | base `setBrush` + `drawFill` `plottable-graph.cpp:1319-1450`; legend fill `:551-557` | none: ctor sets `NoBrush` (`plottable-graph2.cpp:26`), no fill (`docs/graphs.md:156`) | `setComponentFill`, `setComponentFillBetween`, `setComponentFillBrush`, `clearComponentFill`, `componentFill` `plottable-multigraph.h:89-104`; `setBrush` `plottable-line.h:68` | one data-space polygon (`plottable-curve.cpp:469-510`, `plottable-curve.h:82-86`) |
| Channel fill to another graph | `setChannelFillGraph(QCPGraph*)` `plottable-graph.h:82,127,138`; implementation `cpp:310-345`; polygon `:2159`; data-space variant `:906-1014` | no | no: fills are between a component and a constant or another column (`plottable-multigraph.h:94-97`), never to another plottable | no |
| Error-bar companion | separate `QCPErrorBars`, attached with `setDataPlottable(QCPAbstractPlottable*)` (`plottable-errorbar.h:125`) | same (any 1D plottable) | per component: `setComponentErrorBars`/`clearComponentErrorBars`/`componentErrorBars` `plottable-multigraph.h:106-118`; record `cpp:1366-1470` | separate `QCPErrorBars` (generic) |
| Colour by scalar | no | scatter only: `setScatterColorValues`/`setScatterColorGradient`/`clearScatterColorAxis` `plottable-graph2.h:94-101` | per key, all components: `setColorValues`/`clearColorValues`/`hasColorValues` `plottable-line.h:70-83`; mapped through `QCPAbstractColorMapped` `plottable-colormapped.h:6-75` | shared base (`plottable-curve.cpp` records coloured segments; changelog `:305-306`) |
| Selection | `QCPDataSelection` on the plottable (base `plottable.h:179-185`); selected-segment draw `plottable-graph.cpp:483-544` | same base | base + per component `componentSelection`/`setComponentSelection` `plottable-multigraph.h:129-131`; selection events `:180-182`; `updateBaseSelection` `cpp:511-...` | base |
| Selection decorator | painter call `applyPen/applyBrush/getFinalScatterStyle/drawDecoration` `plottable-graph.cpp:510-544` | base painter behaviour | component `selectedPen` (`plottable-line.h:26`); no `drawDecoration` call found in `plottable-multigraph.cpp` | painter decoration in a callback `plottable-curve.cpp:502-506` |
| Legend icon | painter override `plottable-graph.h:164`, `cpp:548-590` | painter override `plottable-graph2.h:153`, `cpp:804-840` | recording `drawLegendIcon(QCPDrawContext&)` `plottable-multigraph.h:176-177`; per-component helper `:160-162` | recording `plottable-curve.h:143` |
| Data API | `data()` returning `QSharedPointer<QCPGraphDataContainer>` `plottable-graph.h:119`; `setData(QSharedPointer)` `:132`; `setData(keys, values)` `:133-134`; `addData(keys, values)` `:142-143`; `addData(key, value)` `:144` | `setDataSource(unique_ptr/shared_ptr)` `plottable-graph2.h:39-40`; `setData(keys, values)` template `:44-51`; `viewData` `:54-67`; `dataSource()` `:41`; `dataChanged()` `:69` | `setDataSource(unique_ptr)`/`setDataSource(shared_ptr)` `plottable-multigraph.h:42-43`; `dataSource()` `:44`; `setData(keys, std::vector<VC>&&)` `:61-68`; `viewData(keys, vector<span>)` `:72-77`; `viewRowMajorData` `:81-87`; `dataChanged()` `:45`; **no `data()` and no `addData()`** | `data()` `plottable-curve.h:108`; `setData` `:113-116`; `addData` `:121-126` |
| Deferred data swap | no | staged source `plottable-graph2.h:187-203` | `setDeferredDataSwap(bool)` `plottable-multigraph.h:57-58`; staged source `:230-241` | no |
| Graph convenience API | `addGraph/graph(i)/graph(i default)/graphCount/removeGraph/clearGraphs/selectedGraphs` `src/core.h:409-417`; `mGraphs` `:535-536`; `registerGraph` `core.cpp:4846-4863`; ctor self-registers `plottable-graph.cpp:196-200` | no dedicated list | no dedicated list | no dedicated list |
| Axis/rect graph lists | `QCPAxis::graphs()` `axis/axis.h:336`, `axis.cpp:1576-1588`; `QCPAxisRect::graphs()` `layoutelement-axisrect.h:129`, `layoutelement-axisrect.cpp:779-789` | included in `mGraphs` only if it were a `QCPGraph` (it is not) | no dedicated list; use `plottables()` / `system()`/`area()` | no |
| Tracer attachment | `QCPItemTracer::setGraph(QCPGraph*)`/`graph()` `items/item-tracer.h:49,94,107`; only follows `QCPGraph` and reads its container `item-tracer.cpp:382-431` | not followable | not followable (`setGraph` still typed `QCPGraph*`) | not followable |
| Fluent handle | `QCPGraphOps` `plottable-graph.h:231-257`; `add<QCPGraph>()` works (`src/construct.h` has no graph special case) | `QCPGraph2Ops` `plottable-graph2.h:226-256` | `QCPMultiGraphOps` `plottable-multigraph.h:292-320` | `QCPCurveOps` `plottable-curve.h:161-200` |

Summary of the replacement's gaps (fact, not proposal): `QCPMultiGraph` has the line styles, steps,
impulses, scatters, skip, adaptive sampling, gap threshold, fills, per-component error bars, colour
by scalar, per-component selection, a recording legend icon and the zero-copy data API. It has no
`data()`/`addData()`, no channel fill to another plottable, and no `QCPGraph`-typed tracer.

### 3.3 What `QCPMultiGraph` reads that is rectangle-only

Its stored mask is `capRectangular | capStraightKey | capColourScale | capLanes`
(`plottables/plottable-multigraph.h:36-38`). The code reads:

| Read | Where | Why it is rectangle-only |
|---|---|---|
| Draw gate: both 2.x axis projections | `plottable-multigraph.cpp:1076,1078` | returns without `mKeyAxis`/`mValueAxis`, i.e. on any non-cartesian system |
| Axis ranges/scale type for the viewport | `cpp:1106,1127,1138` | reads the `QCPAxis` range and scale type directly |
| Resampler viewport | `ViewportParams::fromAxes(mKeyAxis, mValueAxis)` `cpp:1086,1096`; definition `datasource/async-pipeline.cpp:9-19` (axis ranges, `keyAxis->axisRect()->width()/height()`) | the resampler bins by axis ranges and rect pixels |
| Pixel density / thinning | `cpp:1127-1143` (`pixelsPerKey` from `coordToPixel`, `qcpKeyPixelsPerUnit`), passed to `ds->linesInData(..., pixelsPerKey, ...)` `cpp:1172-1174` | pixel columns are measured on the key `QCPAxis` |
| Impulse baseline | `cpp:1152-1159` (`mValueAxis->range().lower` on a log value axis; `0.0` otherwise) | reads the value `QCPAxis` range |
| Fills | `cpp:1284-1296` (`fillValue(*mValueAxis, ...)`), `:1300-1305`, `:1308-1313` (`mKeyAxis->scaleType()`), `:1316-1343` (`mKeyAxis->axisRect()->rect()`, `orientation()`), `:1360-1362` (log value axis) | `axisRect()` is null on a non-cartesian area; `fillValue` reads the value `QCPAxis` |
| Error bars | `cpp:1401-1402` (`mKeyAxis->axisRect()->rect()`, `orientation()`), `:1408`, `:1467-1469` (`coordToPixel`/`pixelToCoord` for the cap half width) | the view test and the pixel cap width use the key `QCPAxis` and its rect |
| Colour scale | `QCPAbstractLine::startColouring()` `plottables/plottable-line.cpp:233-240`; `QCPAbstractColorMapped::followKeyAxisRectScale()` `plottable-colormapped.cpp:153-159` reads `mKeyAxis->axisRect()` | a colour scale exists only on an axis rect (`QCPAxisRect::colorScale()` `layoutelement-axisrect.h:131-133`; `backlog.md:22-24` records that a curve does not follow it on a user system) |
| Lanes | the mask declares `capLanes` (`plottable-multigraph.h:38`), but no lane code was found in `plottable-multigraph.cpp` or `multigraph-*.cpp` (grep for `lane`/`Lane` returns nothing there) | declared, not read |
| GPU path | lines/markers/fills/error bars are recorded as data-space commands (`cpp:1146-1162,1197-1234`; `context.fillBetween` `:1362`; `recordErrorBars` `:1437-...`), replayed through the frame's transform (`painting/draw-context.h:140,169`; transform built from the system at `draw-context.cpp:440`) | not rectangle-only by itself; only the density/baseline/cap inputs above are |

`QCPGraph`'s 4j data-space path, by contrast, is built on `capStraightSegments`
(`plottable-graph.cpp:189-191`): `drawsInDataSpace()` (`plottable-graph.h:187-188`),
`dataSpaceLines()` (`cpp:775-849`, all six styles), `dataSpaceScatters()` (`:856-874`),
`mapToPixels()` (`:882-897`), `dataSpaceFillPolygon()` (`:931-951`, baseline from
`valueAxisBase()`/log range), `dataSpaceChannelFillPolygon()` (`:963-1014`),
`drawDataSpaceChannelFill()` (`:906-922`), and the scatter branch in `getScatters()` (`:708-712`).
It has no decimation and no visible-range cut by design (`cpp:770-773`), no error bars on that path,
and no culling (`2026-11-09-...:248-254` D9; `milestone-4j-rulings.md:26-30,44-46`).

A straight-segment path for the replacement therefore needs (facts, from those two paths):

- a key window/density answer from the system instead of axis `range()`/`coordToPixel()`:
  `QCPDrawContext::keyWindow()` exists (`draw-context.h:169`), and
  `QCPCoordinateSystem::keyWindow()` has an unbounded default (`coordinate-system.h:91`);
- a baseline for fills and impulses from `keyAxisBase()`/`valueAxisBase()` rather than
  `mValueAxis` (`plottable-graph.cpp:936-942` shows the data-space form);
- a pixel cap width converted through the transform rather than
  `mKeyAxis->coordToPixel()` (`plottable-multigraph.cpp:1467-1469`);
- a colour-scale route for a non-rect area, or the documented limit (`backlog.md:22-24`);
- a decision on the mask: `QCPGraph` requires only `capStraightSegments` today, `QCPMultiGraph`
  requires four capabilities (see "Open questions").

---

## 4. Every user of the deleted API

Occurrences are lines, `*.cpp` + `*.h`, grep at `6015c0a`.

| Directory | `QCPGraph` | `QCPGraph2` | `addGraph` | other deleted API |
|---|---:|---:|---:|---|
| `src/` | 154 | 56 | 14 | `graphCount` 5, `removeGraph` 8, `clearGraphs` 4, `selectedGraphs` 5, `QCPGraphData` 121 |
| `tests/` (all) | 255 | 210 | 213 | `graphCount` 31, `removeGraph` 6, `clearGraphs` 3 |
| `tests/benchmark/` | 38 | 30 | 33 | — |
| `examples/` | 52 | 0 | 4 | `graphCount` 3, `removeGraph` 1, `clearGraphs` 1, `selectedGraphs` 3 |
| `docs/snippets/` | 71 | 14 | 23 | `removeGraph` 1, `selectedGraphs` 1 |
| `documentation/doc-image-generator/` | 7 | 0 | 12 | — |
| `documentation/doc-code-snippets/` | 4 | 0 | 5 | — |

112 files across `src/ tests/ examples/ docs/snippets/ documentation/` mention one of the three.

### 4.1 `src/` — notable sites

- Old graph API implementation: `src/core.h:409-417` (declarations), `src/core.cpp:1808-1961`
  (`graph`, `addGraph`, `removeGraph`, `clearGraphs`, `graphCount`, `selectedGraphs`), `:4828-4829`
  (adoption special case), `:4846-4863` (`registerGraph`), `src/core.h:535-536` (`mGraphs`).
- Graph self-registration: `src/plottables/plottable-graph.cpp:196-200`.
- Axis/rect graph lists: `src/axis/axis.h:336`, `src/axis/axis.cpp:1576-1588`;
  `src/layoutelements/layoutelement-axisrect.h:129`, `src/layoutelements/layoutelement-axisrect.cpp:779-789`.
- Data locator type dispatch: `src/data-locator.cpp:32-45` (`locate`), `:50-95`
  (`locateGraph`, `locateGraph2`), `:240-245` (`locateAtKey` dispatch), `:259-308`
  (`locateGraphAtKey`, `locateGraph2AtKey`). It already has `locateMultiGraph` (`:44-45`), so the
  graph branches are the ones that go.
- Tracer: `src/items/item-tracer.h:49,94,107,131,180` (property, getter, setter, `QPointer<QCPGraph>`,
  handle setter); `item-tracer.cpp:181-216` (`setGraph`), `:382-431` (`updatePosition` reads
  `mGraph->data()`); the internal callers that clear a dying graph are `src/core.cpp:1480,1599`.
- `QCPGraph2` is otherwise referenced only by its own files, the include in `src/qcp.h:92`, the
  dispatch in `src/data-locator.cpp` (above), the build lists, and comments
  (`src/datasource/abstract-datasource.h:81`, `src/plottables/plottable-histogram2d.h:34`).
- `QCPGraph` doc comments in `src/doc-classoverview.dox:40`, `src/doc-dataselection.dox:60,67`,
  `src/doc-mainpage.dox:18,22,38`, `src/doc-performance.dox:46-48`, `src/layer.cpp:535-545,754,993,1061`,
  `src/scatterstyle.cpp:39-61`, `src/scatterstyle.h:73`, `src/datacontainer.h:180,192,678`,
  `src/layoutelements/layoutelement-legend.cpp:259,849,899`, `src/plottables/plottable.cpp:241,270,
  286,291,453,593,793,809,829`.
- Build lists: `meson.build:125-126,339-340`; `cmake/sources.cmake:82-83,173-174`; umbrella
  `src/qcp.h:91-93`.
- `QCPMarginGroup`: 63 occurrences across `src/ tests/ examples/ docs/snippets/ documentation/`
  (`src/layout.h:38-59`, `src/layout.cpp:32-181`, `src/layoutelements/layoutelement-colorscale.cpp:84`,
  `src/doc-layoutsystem.dox:42`, `docs/colormaps.md:177`, `docs/layout.md:135`).
- Wave: `QCPWaterfallGraph` derives `QCPMultiGraph` (`plottables/plottable-waterfall.h:57`), so the
  rename covers it; `using QCPGraphComponent = QCPMultiGraph::Component` (`plottable-multigraph.h:289`)
  is the compatibility alias.

### 4.2 Tests (names and counts)

QCPGraph/QCPGraph2 occurrences per file (`tests/`, `*.cpp` + `*.h`; benchmark included; `QCPGraphData`
is the data struct, listed separately because it survives the graph classes):

| File | `QCPGraph` | `QCPGraph2` | `addGraph` | `QCPGraphData` |
|---|---:|---:|---:|---:|
| `tests/benchmark/benchmark.cpp` | 38 | 30 | 33 | 0 |
| `tests/auto/test-builder/test-builder.cpp` | 58 | 4 | 3 | 2 |
| `tests/auto/test-builder/test-builder.h` | 0 | 0 | 2 | 0 |
| `tests/auto/test-plottable-group/test-plottable-group.cpp` | 0 | 0 | 33 | 0 |
| `tests/manual/mainwindow.cpp` | 17 | 8 | 22 | 6 |
| `tests/auto/test-ownership/test-ownership.cpp` | 10 | 0 | 22 | 0 |
| `tests/auto/test-ownership/test-ownership.h` | 1 | 0 | 0 | 0 |
| `tests/auto/test-qcustomplot/test-qcustomplot.cpp` | 17 | 0 | 7 | 0 |
| `tests/auto/test-user-system/test-user-system.cpp` | 15 | 0 | 4 | 0 |
| `tests/auto/test-user-system/test-user-system.h` | 0 | 0 | 2 | 0 |
| `tests/auto/test-repaint/test-repaint.cpp` | 5 | 0 | 12 | 0 |
| `tests/auto/test-qcpgraph/test-qcpgraph.cpp` | 12 | 0 | 4 | 3 |
| `tests/auto/test-qcpgraph/test-qcpgraph.h` | 2 | 0 | 0 | 0 |
| `tests/auto/test-handle/test-handle.cpp` | 14 | 0 | 7 | 0 |
| `tests/auto/test-errorbar-geometry/test-errorbar-geometry.cpp` | 11 | 0 | 0 | 4 |
| `tests/auto/test-reference-m4e/test-reference-m4e.cpp` | 12 | 0 | 0 | 0 |
| `tests/auto/test-item-recording/test-item-recording.cpp` | 10 | 0 | 0 | 12 |
| `tests/auto/test-coordinate-system/test-coordinate-system.cpp` | 11 | 1 | 3 | 6 |
| `tests/auto/test-signal-delivery/test-signal-delivery.cpp` | 5 | 0 | 6 | 0 |
| `tests/auto/test-selection-model/test-selection-model.cpp` | 4 | 0 | 5 | 0 |
| `tests/auto/test-scatter-rhi/test-scatter-rhi.cpp` | 0 | 36 | 10 | 0 |
| `tests/auto/test-pipeline/test-pipeline.cpp` | 0 | 36 | 1 | 0 |
| `tests/auto/test-pipeline/test-pipeline.h` | 0 | 1 | 0 | 0 |
| `tests/auto/test-pipeline/test-layer-translation.cpp` | 0 | 12 | 0 | 0 |
| `tests/auto/test-pipeline/test-gpu-translation.cpp` | 0 | 5 | 0 | 0 |
| `tests/auto/test-datasource/test-datasource.cpp` | 0 | 21 | 0 | 0 |
| `tests/auto/test-datasource/test-datasource.h` | 0 | 1 | 0 | 0 |
| `tests/auto/test-busy-indicator/test-busy-indicator.cpp` | 0 | 19 | 0 | 0 |
| `tests/auto/test-color-by-scalar/test-color-by-scalar.cpp` | 0 | 5 | 0 | 0 |
| `tests/auto/test-data-swap/test-data-swap.cpp` | 0 | 4 | 0 | 0 |
| `tests/auto/test-multigraph/test-multigraph.cpp` | 0 | 3 | 0 | 0 |
| `tests/auto/test-hit-rule/test-hit-rule.cpp` | 0 | 2 | 0 | 0 |
| `tests/auto/test-data-locator/test-data-locator.cpp` | 0 | 1 | 2 | 0 |
| `tests/auto/test-multi-datasource/test-multi-datasource.cpp` | 0 | 1 | 0 | 0 |
| `tests/auto/test-paintbuffer/test-paintbuffer.cpp` | 2 | 4 | 6 | 0 |
| `tests/auto/test-qcplegend/test-qcplegend.cpp` | 2 | 1 | 5 | 0 |
| `tests/auto/test-grid-rhi/test-grid-rhi.cpp` | 2 | 0 | 3 | 0 |
| `tests/auto/test-qcpaxisrect/test-qcpaxisrect.cpp` | 2 | 0 | 2 | 0 |
| `tests/auto/test-drawable/test-drawable.cpp` | 0 | 0 | 2 | 6 |
| `tests/auto/test-alignment-group/test-alignment-group.cpp` | 0 | 0 | 3 | 0 |
| `tests/auto/test-span-rhi/test-span-rhi.cpp` | 0 | 0 | 1 | 0 |
| `tests/auto/test-layer-removal/test-layer-removal.cpp` | 0 | 0 | 1 | 0 |
| `tests/auto/test-item-system/test-item-system.cpp` | 1 | 0 | 1 | 0 |
| `tests/auto/autotest.cpp` | 1 | 0 | 0 | 0 |
| `tests/manual/gallery/main.cpp` | 3 | 9 | 9 | 0 |
| `tests/manual/massive_graph2/main.cpp` | 0 | 1 | 0 | 0 |
| `tests/perf/single-column-perf.cpp` | 0 | 4 | 0 | 0 |
| `tests/big_data_for_tracy/mainwindow.cpp` | 0 | 1 | 0 | 0 |
| `tests/cmake/consumer/main.cpp` | 0 | 0 | 1 | 0 |
| `tests/special-qtflags/main.cpp` | 0 | 0 | 1 | 0 |

The dedicated module is `TestQCPGraph` (`tests/auto/test-qcpgraph/test-qcpgraph.h:4-18`; slots
`specializedGraphInterface`, `dataManipulation`, `dataSharing`, `channelFill`). QCPGraph2 lives
mostly in `test-pipeline`, `test-datasource`, `test-scatter-rhi`, `test-busy-indicator`,
`test-data-locator`, `test-data-swap`, `test-multi-datasource`, `test-multigraph`,
`test-color-by-scalar`, `test-layer-translation`, `test-gpu-translation`, `test-paintbuffer`,
`test-builder`, `test-hit-rule`, `test-qcplegend`, `test-coordinate-system`, and the manual/perf
programs.

### 4.3 Examples

| File | Occurrences |
|---|---:|
| `examples/plots/mainwindow.cpp` | 43 |
| `examples/axis-tags/mainwindow.cpp` + `.h` | 4 |
| `examples/interactions/mainwindow.cpp` | 3 |
| `examples/scrollbar-axis-range-control/mainwindow.cpp` | 2 |
| `examples/text-document-integration/mainwindow.cpp` | 2 |
| `examples/theme-demo/main.cpp` | 2 |

Notable: `examples/plots/mainwindow.cpp:199,483,1080,1214` use `setChannelFillGraph` (the feature the
replacement lacks); `:959,991` pass the graph handle to `QCPItemTracer::setGraph`; `:528,549,555,565`
still use `addGraph(axes)`; `:335-348` iterate `QCPGraph::LineStyle`. `examples/wasm-gallery/gallery.cpp`
has no graph reference.

### 4.4 Docs and snippets

- `docs/snippets/`: `graphs.cpp` 22 mentions (8 of them `QCPGraph2`), `styling.cpp` 14,
  `interactions.cpp` 14, `gallery.cpp` 8 (2 `QCPGraph2`), `layout.cpp` 7, `items.cpp` 7,
  `export.cpp` 7, `axes.cpp` 7, `performance.cpp` 5 (4 `QCPGraph2`), `concepts.cpp` 4,
  `systems.cpp` 3, `plottables.cpp` 2, `getting-started.cpp` 2, `migrating.cpp` 1.
  `QCPGraph2` sites: `graphs.cpp:142,162,187,193,219,354`; `performance.cpp:24,72,84,137`;
  `gallery.cpp:146,154`.
- Prose pages: 63 mentions in 17 pages — `docs/graphs.md` 19, `performance.md` 15, `migrating.md` 6,
  `interactions.md` 4, `styling.md` 3, `coordinate-systems.md` 3, `plottables.md` 2,
  `getting-started.md` 2, and one each in `polar.md`, `layout.md`, `items.md`, `index.md`,
  `gallery.md`, `export.md`, `concepts.md`, `axes.md`, `api.md`.
- Snippet/doc anchor examples of removed features: `docs/graphs.md:16` (`data()` container),
  `:154-156` (`QCPGraph2` has no fill/channel fill), `:303` (NaN gaps), `:312-320` (choice table);
  `docs/performance.md:190` (`QCPGraph` still has fills, channel fills, `addData` that `QCPGraph2`
  lacks), `:184-185`, `:47`; `docs/interactions.md:152` (`QCPGraph2` selection decoration),
  `:232,270`; `docs/items.md:55` (tracer follows `QCPGraph` only); `docs/concepts.md:50`,
  `docs/axes.md:152`, `docs/layout.md:107` (graphs on explicit axes).

### 4.5 `documentation/`

`documentation/doc-image-generator/mainwindow.cpp` uses `QCPGraph`/`addGraph` in seven generator
functions: `genScatterStyles()` (`:45-79`, 15 scatter-style PNGs), `genItemTracer()` (`:220-237`,
`QCPItemTracer.png`), `genLayoutsystem_AddingLegendTitle()` (`:551-572`),
`genLayoutsystem_MovingLegend()` (`:575-598`), `genQCPGraph()` (`:601-648`, `QCPGraph.png`),
`genQCPErrorBars()` (`:850-915`, `QCPErrorBars.png`), `genQCPSelectionType()` (`:1048-1108`, five
`selectiontype-*.png`). `genMarginGroup()` (`:269-319`) generates `QCPMarginGroup.png` without a graph
but uses the deprecated class. `documentation/doc-code-snippets/mainwindow.cpp` has 4 `QCPGraph` and
5 `addGraph` occurrences.

---

## 5. Other 2.x leftovers

### 5.1 The protected axis projections

- Plottables: `QPointer<QCPAxis> mKeyAxis, mValueAxis` `src/plottables/plottable.h:331-333`; public
  projections `keyAxis()`/`valueAxis()` `:162-164`; `keyAxisBase()`/`valueAxisBase()` `:168-174`.
  Kept in sync by `setSystem`/`resolveAxes` (`plottable.cpp:505-507,845-892`). The coordinate design
  says they stay until milestone 5 (`2026-11-09-...:129-132`).
- Item positions: `QPointer<QCPAxis> mKeyAxis, mValueAxis` and `QPointer<QCPAxisRect> mAxisRect`
  `src/items/item.h:202-203`; getters `:158-162`; 2.x setters `setType/setTypeX/setTypeY`
  `:174-176`, `setAxes`/`setAxisRect` `:182-183`; `PositionType` (`ptAbsolute`, `ptViewportRatio`,
  `ptAxisRectRatio`, `ptPlotCoords`, `ptAxisRectAbsolute`) `:106-134`; `needsFrame()` comment
  `:222-225`; `pixelPosition()` returns `(0,0)` outside a plot `:170-171`. 4i added
  `setSystem`/`setArea` alongside (`:184-190`).

### 5.2 Signals

- Selection signals with arguments: removed in 4a (`changelog.txt:270-280`); the single-signature
  forms are `plottable.h:313-314`, `item.h:341-342`, `axis-base.h:192-193`,
  `layoutelement-legend.h:84-85,263-264`, `layoutelement-textelement.h:99-100`.
- `rangeChanged` has one signature `(newRange, oldRange)` (`milestone-4a-rulings.md:27`); a 2.x
  one-argument slot still connects.
- Direct signals that stay direct (not a milestone-5 promise, listed for completeness):
  `QCPTheme::changed()`, `QCPLaneLayout::changed()`, the async pipeline's cross-thread signals
  (`changelog.txt:289-292`; `2026-11-06-...:101-104`).
- Signals that carry raw object pointers: `plottableClick(QCPAbstractPlottable*, int, QMouseEvent*)`
  and the item/axis/legend click signals `src/core.h:490-497`; `itemCreated(QCPAbstractItem*)`
  `:504`. The design keeps signal arguments as objects while getters move to handles
  (`2026-11-02-...:331-335`).

### 5.3 Raw-pointer getters

Beyond §1 row 5: `QCustomPlot::plotLayout()` `core.h:215`, `overlay()` `:306`,
`theme()` `:272`, `pipelineScheduler()` `:349`, `axisRectAt()` `:453`,
`selectedPlottables/Items/Axes/Legends()` `:364,426,456-457`;
`QCPAxisRect::rangeDragAxis/rangeZoomAxis/rangeDragAxes/rangeZoomAxes` `layoutelement-axisrect.h:68-71`,
`QCPArea::insetLayout()` `layoutelements/area.h:67`; `QCPLegend::item(int)` `layoutelement-legend.h:251`,
`QCPAbstractLegendItem::parentLegend()` `:56`; `QCPColorScale::axis()`
`layoutelement-colorscale.h:94`; `QCPItemTracer::graph()` `item-tracer.h:94`;
`QCPPlottableGroup::group()` `plottables/plottable-group.h:77`, `QCPBars::barsGroup()`
`plottables/plottable-bars.h:214`.
The handles that already exist: `QCPHandle<T>` (`src/handle.h:44-104`), `QCPAxisRect::add<QCPAxis>`
(`layoutelement-axisrect.h:97-104`), `QCPAxisRect::colorScale()` returns a `QCPColorScaleOps` handle
(`:131-133`).

### 5.4 `QCPAxisRect` 2.x helpers still present

`rangeDrag()/rangeZoom()` `layoutelement-axisrect.h:64-66`; drag/zoom axis getters `:68-71`;
`setRangeDrag/setRangeZoom` `:75-76`; the three `setRangeDragAxes` and three `setRangeZoomAxes`
overloads `:77-82`; `resetRangeDragAxes/resetRangeZoomAxes` `:84-85`;
`setRangeZoomFactor` `:86-87`; `axisCount()/axis()/axes()` `:90-93`; `allAxes()` `:94`;
`addAxis/addAxes/removeAxis` `:117-119`; `zoom/panByPixels/zoomAt` `:121-126`;
`setupFullAxesBox(bool)` `:127`; `plottables()/graphs()/items()` `:128-130`; the QRect-imitating
readers `:136-158`. The new forms beside them: handle `add<QCPAxis>` `:97-104`, per-axis
drag/zoom opt-outs `axis/axis-base.h:117-120`, linked axes `axis-base.h:174-176`, and
`QCPAlignmentGroup::create` (`docs/superpowers/plans/2026-11-08-...-4d.md` P-1..P-11).
Only `graphs()` is tied to a deleted class; no ruling proposes removing the others.

### 5.5 `qDebug` where the misuse tier is the 3.0 rule

274 `qDebug()` lines in `src/` (`*.cpp`); 108 `QCP_CHECK`/`reportMisuse` sites. The heaviest files:
`layout.cpp` 37, `core.cpp` 29, `items/item.cpp` 16, `plottables/plottable-graph.cpp` 15,
`plottable-errorbar.cpp` 14, `plottable-bars.cpp` 14, `layoutelement-colorscale.cpp` 12,
`layer.cpp` 12, `layoutelement-axisrect.cpp` 11, `axis/axis.cpp` 10. Graph-specific examples:
`core.cpp:1816` (graph index), `:1871,:1878` (`addGraph` axes), `:4852,:4857` (`registerGraph`);
`plottable-graph.cpp:316` (self channel fill), `:403` (data sizes), `:717,1034,1078,1132,1186,1252,
1477,1616,1862,2062,2171,2176` ("invalid key or value axis"); `item-tracer.cpp:207` (foreign graph).
Rulings already accept `qDebug` for two cases: the link inverse (`milestone-4d-rulings.md:37-39`) and
`setPosition()` refusal (`milestone-3-rulings.md:174-176`).

### 5.6 Forwarders, aliases and other compatibility code

- `using QCPGraphComponent = QCPMultiGraph::Component;` `plottables/plottable-multigraph.h:289`.
- `using LineStyle = QCPAbstractLine::LineStyle;` `plottables/plottable-curve.h:102` (so
  `QCPCurve::lsLine` compiles).
- `QCPGraphOps`/`QCPGraph2Ops` (`plottable-graph.h:231-257`, `plottable-graph2.h:226-256`) and
  `QCPItemTracerOps::setGraph(QCPHandle<QCPGraph>)` (`items/item-tracer.h:180-187`).
- `QCPItemSpan`'s ignored pen (`items/item-spanbase.h:32-34`).
- `QCP::phImmediateRefresh` compatibility hint (`global.h:228-230`).
- `RefreshPriority` aliases (`core.h:153-157`) — deprecated with a compiler attribute.
- No `axesChanged()` forwarder: the rename is outright (`plottable.h:320`; changelog `:483`).
- No `\deprecated` forwarder method anywhere else: 4e's rule is "no forwarder, alias or
  `\deprecated` method for a NeoQCP-only API this milestone replaces"
  (`docs/superpowers/plans/2026-11-08-...-4e.md:79-82`).

---

## 6. SciQLopPlots impact (read-only grep, 2026-11-11)

`docs/plans/neoqcp-3/sciqlopplots-migration.md` (89 lines) is the collected port list. Its tables:
compile breaks (spans → `QCPItemSpan`, line colour renames → `setGradient`/`setDataRange`/
`setDataScaleType`, `QCPHistogram2D::Normalization` → `setColumnNormalized`, `edgesChanged` →
`histogramReady`, `setSelectable(bool)` → `QCP::SelectionType`, selection signals without arguments,
`rangeChanged(new, old)`, item `draw(QCPPainter*)` final) at `:12-23`; behaviour changes (margin group,
linked axes, graph deprecation, immediate data, time-coloured curve painter override, annotations
layer, tracer, deferred delivery, misuse, colour-scale sharing) at `:25-52`; 4i/4j greps at `:61-89`.

Read-only grep of `/home/jeandet/Documents/prog/SciQLopPlots/src` and `include/` for what milestone 5
deletes:

| Symbol | Lines | Main sites |
|---|---:|---|
| `QCPGraph` | 24 in 4 files | `include/SciQLopPlots/Plotables/SciQLopGraphComponent.hpp` 11 (`std::variant<QCPGraph*, QCPCurve*, MultiGraphRef, …>` `:42`; `qobject_cast<QCPGraph*>` `:68`; visitors `:147,179,202,226,272,299,317,336`; `QCPGraph::LineStyle` `:282`), `include/SciQLopPlots/qcp_enums.hpp` 11 (`to_qcp`/`from_qcp` for `QCPGraph::LineStyle` `:107-135`), `include/SciQLopPlots/Plotables/QCPAbstractPlottableWrapper.hpp:100` (`std::is_same_v<T, QCPGraph>`), `src/SciQLopGraphComponent.cpp:36` (`qobject_cast<QCPGraph*>`) |
| `addGraph` | 1 | `include/SciQLopPlots/Plotables/QCPAbstractPlottableWrapper.hpp:102` (`plot->addGraph(keyAxis, valueAxis)`) |
| `selectedGraphs` | 1 | `src/SciQLopPlot.cpp:512` |
| `graphCount`, `removeGraph`, `clearGraphs`, `QCPAxis::graphs`, `QCPAxisRect::graphs` | 0 | — |
| `QCPGraph2` | 1 (comment) | `src/SciQLopPlot.cpp:457` |
| `QCPGraphData` | 2 | `include/SciQLopPlots/Plotables/Resamplers/AbstractResampler.hpp:36-41` (`copy_data` builds `QVector<QCPGraphData>`); survives with `QCPGraph` gone because `QCPPolarGraph` uses it (`src/polar/polargraph.h`) |
| `QCPItemTracer` | 3 | `src/SciQLopNDProjectionPlot.cpp:475-478` (markers, no `setGraph` call), `include/SciQLopPlots/SciQLopNDProjectionPlot.hpp:77` |
| `QCPMarginGroup` | 1 | `src/SciQLopPlot.cpp:141` |

Already-broken 4e API in SciQLopPlots: `QCPItemHSpan`/`QCPItemVSpan` 16 references
(`src/SpanTargets.cpp:52-54,71-73,143-145,184-187`, `src/SciQLopMultiPlotPanel.cpp:823,837,852,908`,
`include/.../SciQLopMultiPlotPanel.hpp:45,70`), which the migration file already lists (`:16`).
SciQLop and TSCat repositories were not read.

---

## 7. Bench rows and reference pictures that use the deleted classes

`tests/benchmark/benchmark.cpp` rows that construct or consume `QCPGraph`/`QCPGraph2` directly:

- `QCPGraph_Standard`, `QCPGraph_ManyPoints`, `QCPGraph_ManyLines`,
  `QCPGraph_ManyOffScreenLines`, `QCPGraph_RemoveDataBetween`, `QCPGraph_RemoveDataAfter`,
  `QCPGraph_RemoveDataBefore`, `QCPGraph_AddDataAtEndSorted`, `QCPGraph_AddDataAtBeginSorted`,
  `QCPGraph_AddDataMixedSorted`, `QCPGraph_AddDataAtEndUnsorted`, `QCPGraph_AddDataAtBeginUnsorted`,
  `QCPGraph_AddDataMixedUnsorted`, `QCPGraph_AddDataSingleAtEnd`, `QCPGraph_AddDataSingleAtBegin`,
  `QCPGraph_AddDataSingleRandom` — declarations `:25-40`, bodies `:179-656`.
- `QCPGraph2_SetDataDouble`, `QCPGraph2_SetDataFloat`, `QCPGraph2_RenderDouble`,
  `QCPGraph2_RenderFloat`, `QCPGraph2_RenderManyLines`, `QCPGraph2_ScatterDisc1M`,
  `QCPGraph2_ScatterSquare1M`, `QCPGraph2_ScatterWithLine1M` — declarations `:42-50`,
  bodies `:659-832`.
- `QCPInput_WheelOverGraph2_1M` (`:94`, body `:1542`) and `QCPGpu_Graph2Render1M`
  (`:102`, body `:1837`).

Rows that use `addGraph` indirectly (the fixture line is deleted with it):
`QCPAxis_SetRange10k` (`:1184`), `QCPAxisRects_Stack8_Replot` (`:1446`),
`QCPPlots_Stack8_Replot` (`:1467`), `QCPPlots_Stack8_Aligned_Replot` (`:1495`),
`QCPInput_MouseMoveStorm` (`:1520`), `QCPGpu_LineSolid100k` (`:1778`),
`QCPGpu_LineDashed100k` (`:1799`), `QCPGpu_Pan100k` (`:1820`),
`QCPGpu_MixedLayer` (`:2061`).

Total: 35 rows (26 direct, 9 indirect).

Reference pictures drawn with `QCPGraph`:

- `tests/auto/test-reference-m4e/reference/`: `errorbars-plain.png`,
  `errorbars-vertical-key.png`, `errorbars-log.png`, `errorbars-selected.png`,
  `errorbars-legend.png` (the companion graph in `errorBarsScene`,
  `test-reference-m4e.cpp:51-85`), `item-tracer.png` (`itemScene("tracer")`,
  `test-reference-m4e.cpp:204-286`), `user-system-skew.png` (`userSystemSkewScene`,
  `test-reference-m4e.cpp:536-609`, the 4j guard that draws filled/step/impulse/dots/channel fill with
  `QCPGraph`). Seven files.
- `documentation/doc-image-generator/` generated pictures listed in §4.5: `QCPGraph.png`,
  `QCPItemTracer.png`, `QCPErrorBars.png`, `layoutsystem-addinglegendtitle.png`
  (`mainwindow.cpp:572`), `layoutsystem-movinglegend.png` (`mainwindow.cpp:598`), five
  `selectiontype-*.png` (`mainwindow.cpp:1088-1108`), fifteen `ss*.png`
  (`mainwindow.cpp:77`) — 25 pictures.
- `QCPMarginGroup.png` (`genMarginGroup`, `mainwindow.cpp:269-319`) uses the deprecated
  `QCPMarginGroup`, not a graph.

---

## Open questions

Facts that force a design choice; no proposal beyond naming the choice.

1. **The renamed graph's data API.** `QCPGraph` has `data()` (a shared
   `QCPGraphDataContainer`), `setData(QSharedPointer)`, and two `addData()` forms
   (`plottable-graph.h:119-144`). `QCPMultiGraph` has none of them: `setDataSource`,
   `setData(keys, vector<columns>)`, `viewData`, `viewRowMajorData`, `dataSource()`
   (`plottable-multigraph.h:42-87`). `QCPGraphData` must survive regardless (`QCPPolarGraph` uses it,
   `SciQLopPlots`' resampler builds it). Which of `data()`/`addData()` the renamed class keeps is
   undecided.
2. **Channel fill.** `QCPGraph::setChannelFillGraph(QCPGraph*)` (`plottable-graph.h:127,138`) has no
   equivalent in `QCPMultiGraph` (fills are column-to-constant or column-to-column,
   `plottable-multigraph.h:94-97`). 24 sites use it across `src/ tests/ examples/ docs/snippets/`
   (e.g. `examples/plots/mainwindow.cpp:199,483,1080,1214`; `docs/snippets/graphs.cpp:108-119`).
   Whether the renamed graph keeps channel fill, loses it, or expresses it as a fill to another
   plot's column is a choice.
3. **The tracer.** `QCPItemTracer::setGraph` is typed `QCPGraph*` and `updatePosition()` reads
   `mGraph->data()` (`items/item-tracer.h:107,131`; `item-tracer.cpp:181-216,382-431`). The renamed
   graph has no `data()` container, and 4j's "tracer follows a graph on a user system"
   (`changelog.txt:503-505`) only works through that type; `backlog.md:15-16` says a tracer on a
   polar graph is unsupported. What a tracer follows after the rename is undecided.
4. **User systems and the straight-segment path.** D9 says `QCPGraph` needs only
   `capStraightSegments` and 4j built a data-space path for it
   (`plottable-graph.cpp:189-191,708-712,775-1014`; `2026-11-09-...:248-254`). The replacement
   requires `capRectangular|capStraightKey|capColourScale|capLanes` and reads the 2.x axes
   (`plottable-multigraph.h:36-38`; §3.3). If the old class is deleted and the multigraph renamed
   without that path, the 4j capability and the `user-system-skew.png` reference are lost. Whether the
   renamed class gains a straight-segment path (and how much of the 4j code moves) is the central
   choice.
5. **The `capLanes` flag.** `QCPMultiGraph` declares it (`plottable-multigraph.h:38`) but no lane code
   was found in its files; the intervals plottable uses lanes (`plottable-intervals.h:29-30`). Whether
   the renamed graph keeps the flag is a choice.
6. **Painter legend icons and the painter virtual.** `QCPFinancial` (`plottable-financial.h:197`),
   `QCPStatisticalBox` (`plottable-statisticalbox.h:170`), `QCPErrorBars`
   (`plottable-errorbar.h:175`), `QCPIntervals` (`plottable-intervals.h:152`), `QCPGraph`
   (`plottable-graph.h:164`) and `QCPGraph2` (`plottable-graph2.h:153`) override
   `drawLegendIcon(QCPPainter*)`; 4e kept them on purpose (`milestone-4e-rulings.md:71-72`). The drawing
   spec says the old base goes in M5 (`2026-11-04-...:73-77`) while M4 answer 7 keeps
   `QCPLayerable::draw(QCPPainter*)` as an internal virtual (`2026-11-06-...:91-95`). Which painter
   virtuals remain, and whether the four recorded classes' painter icons move, is undecided.
7. **Getters and signals.** Getters are raw until M5 (`milestone-2-rulings.md:11`) and the design
   wants handles (`2026-11-02-...:331-335`), while signals carry object pointers
   (`src/core.h:490-497`) and public axis/legend members exist (`core.h:480-481`). The boundary
   between handle getters and raw signal arguments is not written down.
8. **`QCPMarginGroup` removal.** It is deprecated (`layout.cpp:40-41`) but used by docs
   (`docs/colormaps.md:177`, `docs/layout.md:135`), the doc image generator
   (`mainwindow.cpp:269-319`), and SciQLopPlots (`src/SciQLopPlot.cpp:141`). Deleting it changes
   those pages and the image; keeping it leaves two alignment mechanisms, which the anti-goal
   (`2026-11-02-...:77`) rejects.
9. **The ignored span pen.** `QCPAbstractSpanItem::setPen` keeps a pen it never draws
   (`items/item-spanbase.h:32-34,44-45`), contrary to "hard to misuse" (`2026-11-02-...:56-58`) and
   decision 11's "no silently ignored setter" for line plottables (`:251-255`). Whether M5 removes
   the setter or draws the pen is undecided.
10. **Two colour-map families.** `QCPColorMap` (`plottable-colormap.h`) and `QCPColorMap2`
    (`plottable-colormap2.h`) still coexist; the object-model spec leaves merging them to project 3
    (`2026-11-02-...:345-347`), so milestone 5's "one way" rule does not reach them. Whether that is
    acceptable for 3.0 is a release decision, not an M5 promise.
11. **Examples build coverage.** `examples/` is not built by meson and can break unnoticed
    (`milestone-4a-rulings.md:65-66`); the fix is an option to add when M5 moves the examples, so the
    port and the build option are one task.
