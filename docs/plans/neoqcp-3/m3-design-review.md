# Review — NeoQCP 3.0 drawing API design (draft 2026-11-04)

Against HEAD `fa43577`. Read-only. Inventory used: `docs/plans/neoqcp-3/m3-drawing-inventory.md`;
parent spec: `docs/specs/2026-11-02-neoqcp-3-object-model-design.md` (decisions 8, 9, 10, anti-goals).
Ordered Critical / Important / Minor; each has where, a scenario, and a fix. A "checked and sound"
section follows.

---

## 1. Claims about today's code that are false

**F1 (Important) — "Each layer draws all its QPainter work into one picture."**
Paint buffers are **not** per layer. `QCustomPlot::setupPaintBuffers()` (`core.cpp:4440-4490`) gives one
buffer to every run of consecutive `lmLogical` layers and a dedicated one only to `lmBuffered` layers;
with the default layer list (`core.cpp:486-497`) `background`+`grid` share buffer 0, `axes`+`legend`
share buffer 2 (doc at `core.cpp:4423-4424`). `executeRenderPass` iterates *layers* and dedupes buffers
with a `QSet` (`core.cpp:3543-3556`).
Consequence for §4: a "run" cannot be per layer. Two runs in `axes` and `legend` are inside one texture,
so a GPU run that the design wants *between* them cannot exist without splitting a buffer that milestone 1
deliberately shared (and whose reuse `setupPaintBuffers`'s `removeLast()` loop depends on).
Fix: state that runs are cut per **buffer**, that a buffer may hold several layers, and either forbid a run
boundary between logical layers that share a buffer, or make the buffer assignment follow the run list.

**F2 (Important) — "Items have no data range."**
They do, since the item-index milestone: `QCPAbstractItem::dataExtent()` returns an optional
`QCPItemExtent{keyAxis, valueAxis, QCPDataBox box, pixelPad}` (`item.h:259`), overridden by line, rect,
ellipse, curve and bracket, and it is what the per-layer packed R-tree indexes
(`QCPLayer::itemCandidates`, `layer.cpp`, commit be9fa04). Presenting `dataRange()` as new work hides that
this exists, and dropping its rules (a side counts only when *every* position is `ptPlotCoords` with no
parent anchor; a non-positive bound on a log axis makes the side unbounded — `positionsExtent`,
`item.cpp:1398`) would make the index cull objects it must draw. Fix: present `dataRange()` as the
successor of `dataExtent()` and keep those two rules verbatim.

**F3 (Important, load-bearing for §4) — "The default layers keep it at one or two runs: data on `main`,
text and annotations on a layer above it."**
That default does not exist. Items are added to `parentPlot->currentLayer()` (`layer.cpp:589-590`), which
is `main` (`core.cpp:495`), and the item ctor does not redirect them (`item.cpp:1104-1105`). Text and
annotations therefore sit on `main`, interleaved with the graphs in child order. The run count of the
common case is *not* two; it is the number of alternations in `main`'s child list. Fix: either add the
"annotations" layer as part of this milestone (and migrate items to it, which is a behaviour change for
every existing 2.x program and for the stacking tests) or drop the claim and design for N runs.

---

## 2. Where the design cannot work with the current renderer

**F4 (Critical) — one texture per run does not scale, and the count is data structure, not style.**
Design §4: "A QPainter run paints into its own texture … composited inside the one render pass, in run
order." Every `QCPPaintBufferRhi` allocates an RGBA8 texture of `size * devicePixelRatio`
(`painting/paintbuffer-rhi.cpp:87-120`). At 1000×600 with a 2× screen that is **9.6 MB per run**
(paintbuffer + staging image ≈ double during upload). Scenario: a plot with 300 graphs and 300 text items
added alternately to `main` — a normal annotated plot — produces 600 runs, ~5.8 GB of textures, 600
texture uploads per frame, and OOM or 100 ms frames. Even the design's optimistic "one or two runs" costs
a second full-viewport texture per layer per buffer.
This is exactly the anti-goal "a slower library, in particular for … many items", and it re-introduces
what decision 9 rejected ("a global add order … would make the number of render passes depend on the
data") in a different currency: textures and uploads instead of passes.
Fix (pick one, and say which):
(a) keep one paint buffer per buffer and make stacking **backend-major within a buffer** (all QPainter
children, then GPU geometry), documented as "the layer decides, the backend orders" — no run textures at
all, and the pen-style invariance is preserved because the rule never looks at style;
(b) sort the replay order inside a buffer by backend at record time (QPainter first, GPU after) so the
count is ≤ 2 per buffer by construction, and document the order;
(c) sub-rect textures: each run's texture sized to the union of its commands' bounds — cheap for a label,
still one allocation per run, so cap it.
Give the plan a hard number: no more than N run textures alive, N small, with a misuse warning beyond.

**F5 (Critical) — replaying data-space commands every frame throws away the pan fast path.**
Design §3: "Converting to pixels happens at replay, so a pan or a zoom replays the same list." Today a pan
does **not** re-transform: `QCPLayer::pixelOffset()`/`canTranslateInsteadOfRepaint()` shift the old texture
and the compositor applies the offset (`layer.cpp:334-420`, `core.cpp:3550-3580`), and GPU lines keep
*untranslated extruded vertices* in `ExtrusionCache` (`plottable-draw-utils.h:29-45`) that are re-used with
a translation (`drawPolylineCached`). That is why Zoom10k is 0.55 ms and Pan10k 0.54 ms today.
Under the design a pan maps every point of every polyline command to pixels and re-extrudes it: for the
1M-point graph that is 1M transform calls plus ~24 MB of vertex data per frame, versus one uniform today.
Fix: decide where the transform happens. Either (a) keep geometry in data space in a GPU buffer and apply
the transform *in the vertex shader*, extruding to pen width in clip space (then a pan is a uniform
update — this also solves dashes, see F10), or (b) keep today's model: recordings store **untranslated
pixel vertices** plus the transform they were built with, and a pure pan/zoom reuses them with a
translation/scale. Add the acceptance target: pan and zoom of a 1M-point line must not regress against
`docs/plans/neoqcp-3/baseline.md`.

**F6 (Important) — export parity as stated is impossible, and the test list asks for it.**
"Exports replay the same runs through each command's QPainter form, so the PDF stacks exactly like the
screen", plus (Left to the plan) "pixel-identical screen vs export". Two problems.
(i) Today an export walks children object by object into one `QCPPainter` pass (`QCustomPlot::draw`,
`core.cpp:4320-4330`); the screen is backend-major per buffer. To stack identically the export must walk
the same run list — that is implementable, but it means the export is no longer a single child loop, and
each run's QPainter form needs the same clip the run had. Say so explicitly, otherwise the two orders
diverge silently.
(ii) GPU geometry is CPU-extruded quads + MSAA (`setSampleCount(4)` off HiDPI, `core.cpp:478`), QPainter
draws antialiased lines: the two rasterisations cannot be byte-identical. Every existing pixel test already
allows a tolerance (`pixelsOfColor(..., tolerance 40)`). Fix: make the test "same layout, same colours, same
stacking order, per-pixel difference bounded", not "pixel-identical", and state the tolerance.

**F7 (Important) — `painter(callback)` and raw-QRhi commands cannot be replayed across a view change.**
They are transform-dependent by construction (a callback draws in whatever space it likes; a QRhi command
uploads vertices). Design §2 says `draw()` runs "again only when the generation changed", and §3 says a pan
replays. Both cannot hold for these two command kinds. Fix: declare them "re-recorded whenever the
transform, viewport size, DPR or theme changes" and give the recording a `viewEpoch`; then F5's cache key
is `(generation, viewEpoch)` — pixel-space commands valid only inside one epoch.

---

## 3. Performance traps

**F8 (Important) — the default `hitTest()` is a scan of the whole recording, and hover is new work.**
Design §5: "the default computes it from the recording: the distance to each polyline segment …". For the
1M-point graph that is 1M segments per event; the design's mitigation is "an object can override
`hitTest()`". But the design also extends hit testing to **hover**, which does not exist today
(`QCustomPlot::mouseMoveEvent`, `core.cpp:3890-3910`, only forwards to the grabbed layerable after a
press). So the new rule adds a per-mouse-move cost of O(commands) across the topmost layer.
It also never mentions the item index the last milestone built: the per-layer R-tree in data coordinates
(`layer.cpp` `itemCandidates`) plus a candidate bit per child is the only thing keeping 10k items cheap,
and it is exactly the "fast reject" this design needs. Fix: state that the layer index is queried first and
`hitTest()` is only called for candidates (as `layerableListAt` does today behind
`QCustomPlot::setItemIndexing`), that the default `hitTest()` is bounds-first, and that hover uses the same
candidate set (or is dropped from §5).

**F9 (Important) — a selection change must not re-record anything, but it does invalidate a run texture.**
Today selecting one graph marks its layer (`markDirty` → `noteDirtied`, `layer.cpp:303-318`) and the whole
buffer repaints — 0.81 ms for 10k items, and the item index keeps hit testing cheap. Under per-run
textures, a selection change in one object of a QPainter run repaints *that run*; if the run is per object
(F4's worst case) that is fine, if the run holds 500 text items it repaints all 500. State the intended
cost and the cache identity: a run's texture must be keyed by (buffer, run index, viewEpoch), and a
recording change must not renumber runs (today `mItemSlot` exists precisely because a positional payload
breaks when the child list changes — `layer.cpp` `rebuildItemIndex`).

**F10 (Minor) — dash phase.** "the GPU line therefore has to draw dashes (by arc length in the shader)"
needs a per-command phase and a per-vertex arc length, built during extrusion; a translated-but-not-
re-extruded vertex buffer (F5's option b) keeps the phase correct only if the phase is in data units.
State it, and state that dash patterns in the export (QPainter) are matched by eye, not by pixel.

---

## 4. Contract holes

**F11 (Important) — `draw() const` "must not change the object" contradicts every cache the library has.**
Today `draw()` mutates constantly: `QCPColorMap::updateMapImage`, `QCPHistogram::binForExport`,
`QCPIntervals::rebuildBarsIfNeeded`, `QCPColorMap2`'s synchronous pipeline run, `QCPGraph2`'s L1/L2 line
caches, `ExtrusionCache`. The rendered result also depends on `painter->isExporting()` and on
`mParentPlot->rhi()`, i.e. on state the object does not own. A `const draw()` that may not cache is a
contract no big class can honour; a `const draw()` that may cache is not "must not change the object".
Fix: say "may update mutable, generation-keyed caches; must not change the rendering state other code
observes", and require the context (not the object) to be the only source of transform/export/DPR facts.
`drawLegendIcon` is declared non-const in the sketch while `draw` is const — make both const.

**F12 (Important) — object generation and layer dirtiness are two different counters and the design merges
them.** "a setter … (the milestone-2 setter rule already marks; marking now also bumps the generation)".
If `markDirty()` bumped the recording generation, every unrelated repaint would invalidate every recording
in the layer and the whole scheme degenerates to "record every frame". The correct statement:
`contentChanged()` = object generation++ **and** the existing mark-first path; a *layer* mark never touches
object generations; the recording cache key is (object generation, viewEpoch) and the layer's dirty flag
only decides whether the buffer is repainted. Write that sentence into the design, it is the core of the
whole feature.

**F13 (Important) — one hit-test rule silently changes `setSelectable(false)` and the wheel precedence.**
Decision 10 says picking and dispatch choose the same topmost object. Today the split is documented as
deliberate: `processPointSelection` re-runs `layerableAt(..., onlySelectable=true)` and the doc comment
says explicitly that a non-selectable layerable in front receives the mouse events while the selectable one
behind receives `selectEvent` (`core.cpp:4747-4757`). Under the unified rule the non-selectable object on
top absorbs the pick and nothing below is selectable — a behaviour regression for every plot that puts a
`setSelectable(false)` annotation over a graph.
Also unstated: the wheel is handled by the *axis rect* first today (axes layer, above `main`); "topmost
object, no exceptions" reads as changing zoom. Fix: state the precedence list (event grabber → axes/axis
rect → drawables) and define the unified pick as "topmost object **that can take the action**" (selection
skips non-selectable, the press does not) — that keeps decision 10's promise ("pick and dispatch agree")
for the objects that can act, without making a non-selectable object swallow clicks.

**F14 (Important) — legend icon default from "the first command".**
It is wrong for several built-ins, which is why each one hand-writes the icon today (inventory §1: financial
draws both chart styles, histogram one rect per column, multigraph shows the component spread, graph adds a
marker next to the line). Also: replaying a **data-space** command into an icon rect needs a synthetic
transform the design does not define, and an `image` first command has no gradient to strip unless the
command carries one. Fix: make the default "replay the whole recording, fitted into the icon rect" (the
same transform machinery, one flag) and keep per-class overrides; say so, and say that the icon context's
transform is built from the recording's data bounds.

**F15 (Important) — the design never says what happens to the old virtuals.** `QCPAbstractPlottable::draw(QCPPainter*)`
is pure (`plottable.h:275`), `drawLegendIcon(QCPPainter*, const QRectF&)` is pure (`:284`),
`selectTest(...)` is pure (`:169`). If `QCPDrawable` adds `draw(QCPDrawContext&)` and keeps the old pure
virtuals, every existing subclass must implement both; if it replaces them, every 2.x subclass breaks. The
parent spec allows breaks, but the anti-goal "two ways to do the same thing" forbids keeping both. Decide
explicitly: `QCPDrawable` *replaces* `draw(QPainter*)` with `draw(QCPDrawContext&)`, plottables lose the
painter overload, and the escape hatch is `context.painter(...)`; then the ported classes (§7) are the
only ones touched in M3 and the rest stay on the old base until M4 — which needs a transitional story the
design does not have (two bases in one release = two ways).

---

## 5. Missing decisions a plan writer would have to guess

- Where the extrusion happens (CPU per frame, CPU once per generation, or in the shader). This decides F5,
  F10 and the whole cost model; it is the biggest unstated decision.
- The recording's memory model: does a command *copy* the points or point at a span? §7 defers "points at
  the pipeline's buffers" to M4, which means a ported `QCPBars`/`QCPItemText` copies — fine — but a user
  writing a 1M-point line in M3 doubles memory unless commands can borrow from the start. Say "commands
  hold spans, and a recording may not outlive the storage it borrows" (or the opposite, explicitly).
- The recording's lifetime versus the object: cleared on generation change only, or also on layer change,
  DPR change, theme change, or `setItemIndexing` toggles?
- Interaction with the item index: `dataRange()` replaces `dataExtent()` (F2), and the index's payload
  (child slot) and query unit (data box) must be restated for the recording model.
- Reentrancy: what happens if a slot mutates an object while the run compositor is replaying it (the
  emission-queue machinery from M1 exists; the design does not mention it).
- The recording's public ABI: `std::variant` of commands in a public header freezes the command set for
  ABI compatibility; either version it or keep it pimpl.
- The "one small class" promise versus the ported `QCPBars`: its hit test is a per-bar rect including bar
  group spacing and absolute widths (`getBarRect`), and its data range is widened by half a bar. A
  recording of fills alone cannot reproduce either; the proof has to show where that logic now lives
  (override `hitTest`, override `dataRange`), or the "free" claim in §"What we want" is false for the very
  first port.
- Decision 8's "if an object's axes go away … the object is hidden and a debug check fires" is implemented
  for plottables (`drawableAxes()`) but not for items (a position with a null axis yields `QPointF()`).
  The ported item must state and test the new behaviour.

---

## 6. What I checked and found sound

- The four hit-test rules quoted in the design are exactly the code (inventory §5); the `!details`
  "anywhere in the axis rect" path is real (`plottable-graph2.cpp:31`, `plottable-multigraph.cpp:31`) and is
  what the wheel hits (`wheelEvent` calls `layerableListAt(pos, false)` with no details).
- "A graph goes to the GPU only with a solid pen and outside an export": `plottable1d.h:568-583`,
  `plottable-graph.cpp:1019-1020` (fill: solid brush too). Setter-side: dashed/gradient/export → QPainter,
  drawn into the buffer, under later items. The mechanism in §4a/4b of the inventory holds.
- "Every GPU path has a hand-written QPainter fallback": confirmed per class (inventory §3.7) — the
  one-form-per-command-kind idea is a genuine simplification, not a rewrite for its own sake.
- "No Qt API records mixed GPU and QPainter work", the QCanvasPainter licence/availability caveat: nothing
  in this repo contradicts it; Qt 6.12 is what the build uses.
- "GPU for polylines, fills, markers and images; QPainter for text and arbitrary paths" matches the split
  the code already makes (`plottable1d.h:569`, `plottable-graph.cpp:1019`, `item-spanbase.cpp:73-88`,
  `colormap-renderer.cpp:26`).
- Leaving graph2/multigraph/colormap2/histogram2d on their pipelines in M3 is consistent with the
  anti-goal; their draw() already routes through caches (`plottable-graph2.cpp`, `plottable-colormap2.cpp`).
- Typed positions (decision 8) already exist in embryo: `QCPItemPosition` has per-axis types, anchors and
  pixel offsets (`item.h:87-200`), so the ported `QCPItemText` is a fair proof, not a rewrite.
- The generation-counter cache idea is compatible with what exists: `QCPLayer::dirtiedAfterDraw()` and
  `mPaintGeneration` already give a monotone stamp (`layer.h:117`, `layer.cpp:312-318`), and the item index
  keys on it.
