# Milestone 3 — how to represent the draw-command list so it can be optimised

Date: 2026-11-05. Read-only survey: no code was built, run or changed.
Builds on `m3-prior-art-survey.md` (recording contracts, typed positions), `m3-drawing-inventory.md`
and `m3-gpu-baseline.md` (`QCPGpu_Bars10k` = 8.8 ms on llvmpipe). It does not repeat them.

Marks: **[V]** checked in this session against the cited page, the source file, or two independent
bibliographic records. **[K]** from my own knowledge, not re-checked. **[?]** could not be checked.

---------------------------------------------------------------------------------------------------

## (a) The answer in one paragraph

Production 2D renderers keep painter's order in one of two cheap ways. Either they only merge a draw
with the one just before it (Dear ImGui, the simplest and the one NeoQCP needs first), or they look
back a small, fixed number of draws and stop at the first one whose bounds overlap (Skia Ganesh: 10;
WebRender: 10). Full sorting with a painter's-order key (Skia Graphite, Rive) only pays when many
different pipelines interleave, which a plot rarely does. NeoQCP is in an unusually good spot: colour
already lives in the vertices, every solid fill and stroke uses the same pipeline, and the staging
buffer is already laid out in record order. So the 20 000 draw calls of 10 000 bars can become one
draw call by merging adjacent solid entries that share scissor, offset and alpha, with no reordering
at all and an O(1) check per entry. That fix is behind one blocker: the pan path today gives a cached
stroke a different offset from a freshly built fill, which would split every bar into two runs. The
second big step is what data-viz renderers do (deck.gl, Makie, Bokeh WebGL): one GPU instance per
mark, with the data-to-pixel transform in the shader, so a zoom costs no CPU work. That needs the
command list to hold marks as typed batches in structure-of-arrays form (a `rects` command of N
rects, not N rect commands). This shape must be fixed before milestone 4 freezes the representation.
The shader work itself can come later.

---------------------------------------------------------------------------------------------------

## What the code does today (only what matters here)

- One command becomes one or two `DrawEntry`s (the layer's per-draw record: vertex ranges, pixel
  offset, alpha, scissor). A rect is a fan fill entry, then a stroke entry
  (`command-replay-rhi.cpp`, rect branch; `QCPPlottableRhiLayer::addPlottable`).
- Vertices are pixel-space and carry colour: 6 floats (x, y, r, g, b, a). Dashed: 7 floats.
- The staging buffer is appended in record order. So fill A, stroke A, fill B, stroke B are already
  contiguous in memory (`stagingAppend`).
- `render()` walks `mDrawOrder` (the record-order list of entries). It rebinds the pipeline only when
  the kind changes. But for every solid entry it calls `setShaderResources` (a dynamic UBO offset),
  `setVertexInput`, `setScissor`, then up to two `draw`s.
- `uploadSolid` writes one uniform slot per entry on every frame, pans included.
- The extrusion cache keeps a command's stroke triangles. On a pan it reuses them and passes the pan
  delta as that entry's offset. Fills are rebuilt fresh with offset 0.
- Consequence: 10 000 bars = 20 000 entries, 20 000 uniform writes, 60 000 state calls,
  20 000 draws. On llvmpipe the per-call CPU overhead is the whole cost.
- Memory per bar today [estimate from the code]: fan fill 4 triangles = 12 vertices; closed stroke of
  5 points ≈ 30–50 vertices. At 24 B per vertex that is roughly 1–1.5 KB per bar, ~12 MB per
  10 000 bars, re-extruded and re-uploaded on every zoom.

---------------------------------------------------------------------------------------------------

## (b) Findings

### Q1. How production renderers merge while keeping painter's order

**Dear ImGui `ImDrawList`** [V] https://github.com/ocornut/imgui/blob/master/imgui_draw.cpp
- A new draw command merges into the previous one when the header (clip rect, texture, vertex offset)
  is byte-identical, the index ranges are contiguous, and neither has a user callback
  (`_TryMergeDrawCmds`, `_OnChangedClipRect`).
- It only ever looks at the last command. Cost: one `memcmp` per command. Order is trivially kept.
- `ImDrawListSplitter` gives fixed "channels" that are concatenated at the end [K].
- **Lesson for NeoQCP: this is exactly the merge NeoQCP can do today.**

**ImPlot** [K] draws every bar of `PlotBars` as `PrimRect` quads into the same `ImDrawList`. One plot
item = one ImGui draw command, whatever the bar count. Same mechanism as above.

**Skia Ganesh (`GrOp`, `OpsTask`)** [V]
https://skia.googlesource.com/skia/+/refs/heads/main/src/gpu/ganesh/ops/OpsTask.cpp
- When an op is recorded, Skia searches back through earlier op chains to find one it can combine
  with. It stops at the first chain whose bounds overlap ("Stop going backwards if we would cause a
  painter's order violation"). `can_reorder(a, b)` is just `!GrRectsOverlap(a, b)`.
- The search is capped: `kMaxOpChainDistance = 10`, and `kMaxOpMergeDistance = 10` with the comment
  "Experimentally we have found that most combining occurs within the first 10 comparisons."
- A forward pass (`forwardCombine`) tries the same in the other direction, also capped at 10.
- Ops are typed batches: a fill-rect op holds many rects with per-rect colour [K].

**Skia Graphite (`DrawPass`, `BoundsManager`)** [V, via the source files' search excerpts]
https://skia.googlesource.com/skia/+/chrome/m105/src/gpu/graphite/DrawPass.cpp ,
https://skia.googlesource.com/skia/+/chrome/m144/src/gpu/graphite/geom/BoundsManager.h
- Every draw gets a `CompressedPaintersOrder`. A `BoundsManager` returns the most recent earlier draw
  that intersects the new draw's bounds. The new draw's order is one more than that. So draws that do
  not overlap share the same order value and may be freely sorted among themselves.
- The sort key puts painter's order and stencil index in the top bits, then render step, then
  pipeline and uniforms. Sorting groups same-pipeline draws inside one order level.
- The authors measured the cost: a 24-byte key sorts about 30 % slower than a 16-byte one would.
- `HybridBoundsManager` uses brute force below a draw count threshold and switches to a grid above it.
  The overlap test is itself an optimisation target, bounded by a threshold.

**Firefox WebRender** [V] https://github.com/servo/webrender/blob/main/webrender/src/batch.rs ,
https://bugzilla.mozilla.org/show_bug.cgi?id=1616592 , https://bugzilla.mozilla.org/show_bug.cgi?id=1594474
- Alpha batches: search back for a compatible batch, stop at the first one that intersects.
  `BATCH_LOOKBACK_COUNT = 10`. Bug 1616592 says this trades CPU time for batch count.
- Bug 1594474 measured that the limit was hit in only 3–10 % of batch breaks: the cap costs little.
- Opaque batches are drawn front to back with a depth buffer and need no overlap test.

**Qt Quick scene graph renderer** [V] https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph-renderer.html
- Opaque primitives: front to back, depth test on, no blending, freely reordered.
- Alpha primitives: back to front. Merging checks bounding rects as well as material state:
  "Relying on the Z-buffer alone is not enough."
- One batch per unique material state and geometry type under a batch root.
- Batches live in vertex buffers "retained between frames and updated when the part of the scene
  graph that it represents changes". When only a batch root's transform changes, only its matrix is
  updated. This is the NeoQCP pan fast path, generalised to a whole batch.
- Batch-root thresholds (`QSG_RENDERER_BATCH_NODE_THRESHOLD`, `..._VERTEX_THRESHOLD`) bound the work.

**Rive renderer** [V] https://skia.googlesource.com/external/github.com/rive-app/rive-cpp/+/c238af43d5c482e793dcb85187eab20d0f42c214
- In its "atomic" mode, overlapping draws need a barrier. Rive gives each draw a group index from its
  bounding box (overlap with earlier draws), then sorts by group, draw type, texture. This is
  Graphite's compressed order idea, applied to reduce barriers.

**Chromium cc `PaintOpBuffer` / `DisplayItemList`** [V]
https://chromium.googlesource.com/chromium/src/+/lkgr-ios-internal/cc/paint/display_item_list.h
- Ops are packed into one contiguous buffer. An R-tree over op bounds stores offsets into it.
  Raster queries the R-tree with the tile's clip and plays back only those offsets.
- Built once per recording, queried per tile. Culling, not batching.

**Flutter DisplayList / Impeller** [V partial] https://github.com/flutter/flutter/issues/93899 ,
https://dart.googlesource.com/external/github.com/flutter/engine/+/ca329dd84207702a95167b8510a55f3e29de8154
- A DisplayList has an R-tree used to cull at dispatch. The "opacity peephole" pushes a group alpha
  into children, but first only for a single child: extending it needed an overlap check on the
  children's bounds. Again, the overlap test is the gate for every reorder or merge.

**Vello / piet-gpu, Pathfinder** [V] https://dotat.at/:/WJ92V (Levien, "Fast 2D rendering on GPU"),
https://skia.googlesource.com/external/github.com/linebender/vello/+/180e72ddbd21c20e7bafd916bdd67ab9c7e17f4c/doc/vision.md ,
https://linebender.org/blog/tmil-25/ ,
https://docs.google.com/document/d/146WIsAu7aYC_uvinpCURLS1K8TTSVREAorBi0GCIAMw/export?format=pdf
- Vello encodes the scene into a flat binary stream, then runs compute passes that bin paths into
  tiles and composite per pixel in order. Order is never a batching problem: each tile walks its own
  ordered command list. The global transform is kept out of the path encoding, so an encoding is
  reused under a new affine transform (Levien excludes encoding time for that reason).
- Vello's newer "sparse strips" CPU and hybrid renderers (beta in 2026) target GPUs without compute,
  such as WebGL. Hybrid: CPU builds strips, GPU fills them.
- Pathfinder 3 tiles all paths; a tile fully covered by an opaque path culls everything below it and
  is drawn as a plain quad. Its draft notes report a 2x gain from parallel tiling on one machine and
  none on a two-core laptop.
- Lesson: compute-based tiling is the long-term high end. It does not fit QRhi on GLES/WebGL
  (NeoQCP targets wasm), and it is far more than a plot needs. Not recommended.

**NanoVG** [K] One draw call per path fill and stroke, a flush at the end; no merging across paths.
Known to be call-bound with many small paths. **Blend2D** [K] CPU rasteriser; its asynchronous
context queues commands and renders them in horizontal bands on worker threads, in order.
Not a GPU batching model.

### Q2. Literature

GPU vector graphics (the rasterisation side, background only):

| work | venue | id | mark |
|---|---|---|---|
| Loop, Blinn. Resolution independent curve rendering using programmable graphics hardware | ACM TOG 24(3) (SIGGRAPH 2005) | doi:10.1145/1073204.1073303 | [V] |
| Kokojima, Sugita, Saito, Takemoto. Resolution independent rendering of deformable vector objects using graphics hardware | SIGGRAPH 2006 Sketches, art. 118 | no DOI found; https://history.siggraph.org/?p=149197 | [V title/venue, DOI ?] |
| Nehab, Hoppe. Random-access rendering of general vector graphics | ACM TOG 27(5) (SIGGRAPH Asia 2008) | doi:10.1145/1409060.1409088 (aggregator; not seen on ACM) ; https://hhoppe.com/proj/ravg/ | [V] |
| Kilgard, Bolz. GPU-accelerated path rendering (NV_path_rendering, "stencil, then cover") | ACM TOG 31(6) (SIGGRAPH Asia 2012) | doi:10.1145/2366145.2366191 | [V] |
| Ganacim, Lima, de Figueiredo, Nehab. Massively-parallel vector graphics (shortcut tree) | ACM TOG 33(6):229 (SIGGRAPH Asia 2014) | doi:10.1145/2661229.2661274 ; dblp journals/tog/GanacimLFN14 | [V] |
| Li, Hou, Zhou. Efficient GPU path rendering using scanline rasterization | ACM TOG 35(6) (SIGGRAPH Asia 2016) | doi:10.1145/2980179.2982434 | [V] |
| Levien, Uguray. GPU-friendly stroke expansion | PACMCGIT 7(3) (HPG 2024) | doi:10.1145/3675390 ; arXiv:2405.00127 | [V] |

What they say about ordering: Nehab–Hoppe, Ganacim et al. and Li et al. all keep the scene's paint
order per pixel or per cell (the shortcut tree, per-cell lists, scanline segments). Order is resolved
spatially, not by reordering draws. Kilgard–Bolz is one stencil+cover pair per path, ordered as
issued. Levien–Uguray is relevant to NeoQCP's strokes: it expands strokes in parallel on the GPU,
which is the long-term alternative to CPU extrusion.

Draw-call reordering under ordering constraints: **I found no peer-reviewed paper specific to 2D
painter's order batching.** The knowledge lives in engine code (Ganesh, WebRender, Graphite, Rive,
Qt Quick). In general, "fewest state changes subject to a partial order from overlaps" is a
scheduling problem that is hard to solve optimally [K: a shortest-common-supersequence style
problem]. Every engine uses a greedy rule with a bounded window instead. [?] for any formal result.

### Q3. Scientific and data-viz renderers

- **deck.gl** [V] https://deck.gl/docs/developer-guide/performance ,
  https://deck.gl/docs/api-reference/core/attribute-manager : one instance per data object, with
  instanced attribute arrays (`instancePositions`, colours, sizes) in SoA form. `updateTriggers`
  recompute only the attributes whose inputs changed; the attribute manager can update a row range.
  Binary SoA input skips CPU packing. Position precision: deck.gl uses offset/64-bit emulation modes
  for large coordinates [K].
- **Bokeh WebGL** [V] https://docs.bokeh.org/en/3.0.2/docs/releases.html ,
  https://docs.bokeh.org/en/dev-3.10/docs/user_guide/output/webgl.html : rewritten on regl; markers
  and rects unified into one instanced path; `vbar`/`hbar`/`quad` supported; other glyphs fall back
  to canvas. A direct precedent for "bars are instanced rects".
- **Makie (GLMakie)** [K] scatter and `meshscatter` are instanced; `barplot` is a recipe over `poly`,
  i.e. meshes, not instancing. Transforms (`transform_func`, log) run on the CPU per update, the
  projection on the GPU.
- **VisPy** [K] markers as point sprites with SDF fragment shaders; transforms compile to GLSL so the
  data-to-pixel map runs on the GPU (also in the earlier survey).
- **pygfx / fastplotlib** [K, could not confirm details] points and lines read per-element data from
  storage buffers in the vertex shader ("vertex pulling"), several vertices per element.
- **plotly scattergl (regl-scatter2d, regl-line2d)** [K] instanced points and instanced line segments;
  range updates are uniforms.
- **Datashader** [K] aggregates on the CPU into a pixel raster, then shades. Level of detail by
  binning, not by GPU tricks. Right for 1e8+ points, not for 10 000 bars.
- **ImPlot** [K] one `ImDrawList` command per plot item (see Q1).

Visualization literature on scale (level of detail more than batching):
- Jugel, Jerzak, Hackenbroich, Markl. M4: a visualization-oriented time series data aggregation.
  PVLDB 7(10):797–808, 2014. doi:10.14778/2732951.2732953 [V]. Min/max/first/last per pixel column
  gives an exact line raster: the basis of NeoQCP's existing L2 resampling.
- Liu, Jiang, Heer. imMens: real-time visual querying of big data. Computer Graphics Forum 32(3)
  (EuroVis 2013), 421–430. doi:10.1111/cgf.12129 [V]. Pre-binned data tiles rendered with WebGL.
- Moritz, Fisher. Visualizing a million time series with the density line chart. arXiv:1808.06019,
  2018 [V]. Density instead of overdraw when marks are too many.

Common pattern: marks are instances with SoA attributes in data space. The view transform is a
uniform. A zoom or pan changes uniforms only. Culling and LOD happen in data space (sorted keys,
binning), not per draw.

### Q4. What this means for the NeoQCP representation

1. **Merging needs a key per entry, not per command.** The fields that decide a merge are the
   pipeline kind, scissor, pixel offset, alpha (and for dashes the pattern and phase). Today they are
   all known when an entry is added, so no extra field is needed in the command. A merge is legal when
   those are equal and the vertex ranges are contiguous: ImGui's rule.
2. **The pan delta must be per run, not per command.** Today a cached stroke gets the delta and its
   fill gets 0. That alternation defeats merging. The fix is to keep the whole recording's built
   geometry (fills and strokes, in order) with its transform, and on a translation reuse all of it
   with one delta. Qt Quick does this per batch root.
3. **Typed batches.** A `rects` command holding N rects (SoA: x0, x1, y0, y1 in data units, plus an
   optional per-rect colour array and one pen/brush) replaces N `RectCmd`s. The same for ellipses.
   Markers, error bars and data polylines are already batches. One command per mark kind makes the
   cache one slot, the hit test one scan (and it returns the rect's index), and instancing possible.
4. **Bounds per command.** Data bounds per command (not only for the list) allow culling a whole
   command and a Ganesh-style look-back. Pixel bounds are derived at replay. Storing data bounds costs
   32 bytes per command, computed once per recording.
5. **A "sorted by key" flag** on batch commands lets a zoom draw only the visible sub-range by binary
   search (firstInstance + count). Computed once per recording (one linear check).
6. **Opaque versus translucent** matters only with a depth buffer (Qt Quick, WebRender). NeoQCP's 2D
   pass has no depth attachment, and plots have little overdraw. Not worth adding.
7. **Do not store a sort key in the command.** A key depends on the backend (pipeline, scissor), and
   the command list is backend-neutral. Compute it at replay if a reorder pass ever exists.

---------------------------------------------------------------------------------------------------

## The cost of optimising (the analysis must not eat the gain)

What the sources report:
- Ganesh: look-back capped at 10, because "most combining occurs within the first 10 comparisons".
- WebRender: look-back 10, described as a CPU-versus-batch-count trade-off; the cap was hit in only
  3–10 % of breaks.
- Graphite: the sort itself is measurable (24-byte key ≈ 30 % slower to sort than 16-byte), and the
  overlap structure switches from brute force to a grid above a threshold.
- Qt Quick: batching runs when the scene structure changes. A transform-only frame updates one matrix.
  The batching cost is paid on change, not per frame.
- deck.gl: recompute only attributes whose triggers changed. A user report notes that binary input
  helps start-up, not animation, when data is not re-created each frame.
- Pathfinder: parallel tiling doubled speed on one machine and gave nothing on a two-core laptop.
- Flutter: the opacity peephole started with a single child because the multi-child overlap check
  had a cost.

Rules for NeoQCP, from that:
1. Prefer passes that run **once per recording** and are cached with it (coalescing rects,
   bounds, sorted flag, the merged entry list).
2. Allow **per-zoom** work only if it is O(commands) or O(log N) per command, never O(marks).
3. Allow **nothing new on a pure pan frame**. A pan must stay "move offsets, upload uniforms".
4. Any look-back or reorder is capped (window of ~10, like Ganesh and WebRender) and skipped below a
   threshold (e.g. under ~64 entries per layer, the binds cost less than the analysis).

---------------------------------------------------------------------------------------------------

## (c) Recommendations, ranked

**R1. Merge adjacent solid entries (ImGui rule).**
- Change: in `addPlottable`, if the previous ordered draw is solid with equal scissor, offset and
  alpha, and its range ends where the new one starts, extend it instead of adding an entry. Treat an
  entry as one vertex range (fill and stroke ranges become one).
- Gain: 10 000 bars go from 20 000 draws and 20 000 uniform writes to 1 and 1. Expected to remove most
  of the 8.8 ms of `QCPGpu_Bars10k` on llvmpipe [estimate, to be measured]. It also helps every
  multi-command recording and many objects on one layer.
- Cost: one comparison of ~7 fields per entry, at build time (record or zoom). Zero memory. Nothing
  on a pan frame. It can never cost more than it saves: the comparison is cheaper than one bind.
- Representation: none. Can land in milestone 3 or later. **Blocked by R2** for the pan case.

**R2. Cache built geometry per recording, translate it as one block.**
- Change: the extrusion cache keeps the whole recording's built vertices and entry list (fills
  included), keyed on (list id, transform). On a translation, reuse all of it with one delta. Commands
  that do not move with the data (pixel, ratio, anchor positions) go in their own runs with delta 0.
- Gain: pans keep R1's single draw; fills stop being rebuilt on pans.
- Cost: memory equal to the built geometry (already held by the GPU buffer; ~12 MB for 10 000 bars
  today, ~0.3 MB after R4). Runs once per zoom or re-record.
- Representation: the cache key moves from command index to recording. **Decide before milestone 4**
  (it changes `QCPExtrusionCache`'s contract and the "per-command extrusion cache" in the spec).

**R3. Typed batch commands: `rects` and `ellipses` with SoA arrays.**
- Change: `rect()` stays as the helper. The recorder appends to the previous command when it is a
  `RectsCmd` with the same pen and brush kind (O(1) check at record time). Data is SoA: four double
  arrays plus an optional colour array. A per-bar colour (colour by value, selection highlight) is a
  colour entry, not a new command. Add data bounds and a "sorted by key" flag per batch command.
- Gain: one command, one cache slot, one hit-test scan for N rects; prerequisite for R4 and R5.
  Hit tests can return the index of the rect.
- Cost: once per recording, O(1) per rect. Memory: 32–36 B per rect instead of a 2×Position+QPen+
  QBrush command (well over 100 B each [estimate]).
- Representation: **must be decided before milestone 4 freezes the command set.** The helpers do not
  change; the list is private, so this is ABI-free.

**R4. Instanced marks with the data transform on the GPU.**
- Change: a rect pipeline with one instance per rect (data-space corners, fill colour, stroke colour,
  width) and the key/value transform as uniforms (linear and log in the shader). Fill and stroke in
  one quad via a signed-distance fragment shader, so fill A, stroke A, fill B keep their order inside
  one instanced draw (instances rasterise in order [K]). Same for markers (already sprites) and
  error bars.
- Gain: zoom becomes a uniform update for bars: no CPU extrusion, no vertex upload. Memory ~30x lower.
  Pays off above roughly a few hundred marks; below that R1 alone is enough.
- Cost: a new pipeline and shader, precision work (float32 cannot hold epoch seconds: subtract a
  per-recording origin in double on the CPU, as deck.gl does), antialias parity with QPainter.
  Needs `QRhi::Instancing` (all backends but GLES 2 [K]).
- Representation: needs R3's data-space SoA. The pipeline can come in milestone 4 or after.

**R5. Cull by data range, per command and inside sorted batches.**
- Change: skip a command whose data bounds miss the view; inside a sorted batch draw only the visible
  sub-range (binary search on keys → firstInstance, count).
- Gain: matters for 1e5+ marks zoomed in. For 10 000 bars on the GPU it is small.
- Cost: O(commands + log N) per zoom frame, nothing on a pan (the run is reused). Skip it below a
  threshold of marks.
- Representation: per-command data bounds and the sorted flag (R3). The pass can come later.

**R6. Bounded look-back merge across pipelines (Ganesh/WebRender rule) — only if measured.**
- Change: when a solid entry follows a dashed or image entry, look back up to ~10 entries for a
  compatible solid run and join it if no entry in between overlaps it in pixels.
- Gain: only when a recording or a layer alternates styles often (mixed dashed and solid series).
  Rare in plots.
- Cost: up to 10 rect tests per entry, at build time; pixel bounds per entry (16 B).
- Representation: none in the command; pixel bounds live in the entry. Later, behind a benchmark.

**R7. Dashed runs merge only with equal pattern and phase.**
- The dash pattern and offset are per-entry uniforms. Two dashed entries merge when those are equal;
  otherwise keep them apart. Moving the pattern into the vertex stream to merge all dashes is not
  worth it for plots. Keep the phase in data units (spec) so a translated run keeps it.

Not recommended: depth-buffer opaque/alpha split (no depth attachment, little overdraw), full sort
keys (Graphite) or compute tiling (Vello) — large cost for a scene with 3–4 pipelines.

Decide before milestone 4: R2 (cache contract), R3 (typed batches, SoA, per-command data bounds,
sorted flag, optional per-element colour). Can come later: R1, R4, R5, R6, R7.

---------------------------------------------------------------------------------------------------

## (d) Risks

- **Order bugs.** R1 and R2 never reorder, so they are safe by construction if ranges stay
  contiguous. R6 does reorder: a wrong bounds (too small: pen width, antialias fringe, the offset)
  shows a stroke under the wrong fill. Bounds must include half the pen plus 1 px. Test: overlapping
  bars with alternating styles, compared to the QPainter replay.
- **Merge across objects.** Two objects on one layer may merge if their scissor matches. That is fine
  for order, but a later per-object change (busy fade alpha, `setVisible`) must split the run. Alpha
  is in the merge key, so a fade on one object breaks its run; a hidden object must not be merged in.
- **Cache invalidation.** R2 keys on the whole recording: any re-record drops it, as today. One bar's
  change still re-records all bars (generation per object). Partial updates (deck.gl ranges) are a
  later step, not part of the representation.
- **Dash phase.** A merged dashed run with one uniform phase is wrong if the entries started at
  different arc lengths. Merge only with equal phase (R7). Arc length must be continuous across a
  translation (data-unit phase).
- **Selection.** Selected bars change colour per vertex or per instance; that keeps one run. A
  selection that changes the pen width must stay a per-instance attribute in R4, not a uniform.
- **Hit testing.** Stays on the CPU, from the commands, never from merged GPU runs. R3 helps: the
  batch returns the rect index. Pixel-snapped bars (transform-dependent recordings) must hit-test the
  same pixels the GPU draws.
- **QPainter/PDF export parity.** Export replays commands, not runs, so merging cannot change an
  export. R4's SDF antialiasing differs from QPainter's; compare with a pixel tolerance, as the spec
  already says. A `RectsCmd` exports as N `drawRect`s (or `QPainter::drawRects`), so a PDF keeps one
  vector rect per bar.
- **Precision (R4).** float32 data in shaders breaks time axes and large offsets; use a per-recording
  origin. Log axes with non-positive values must be dropped the same way the CPU path drops them.
- **Small instances (R4).** Some GPUs run tiny instances inefficiently [K, not verified]. Keep R1's
  merged triangles as the fallback and measure on real hardware, not only llvmpipe.

---------------------------------------------------------------------------------------------------

## R3 decision (milestone 4b, 2026-11-06)

**Adopted** (commits 8da0e07, cc066f6). `qcp::RectsCmd` and `qcp::EllipsesCmd` hold a run of
data-space boxes: four corner arrays, an optional colour per box, the shared pen and brush, the run's
data box and a sorted-by-key flag. The arrays sit behind one `shared_ptr`, so the command variant
stays 96 B. `rect()` and `ellipse()` join the previous command when both corners are data, the pen is
equal and the brush has the same style; a solid colour may differ and becomes a per-box fill. A brush
the GPU cannot fill never joins a batch: one image for the run would put every fill under every
stroke.

Measured (llvmpipe, `-median 5`, interleaved with the old binary):

| row | gate | before | after |
|---|---|---|---|
| QCPGpu_Bars10k | 5.5 | 5.0 | 4.2 (-16 %) |
| QCPGpu_BarsPan10k | 0.12 | 0.12 | 0.12 |
| QCPBars_Pan10k | 5.6 | 5.4 | 5.5 |
| QCPDrawn_Zoom10k | 0.30 | 0.30 | 0.31 |
| QCPItems_Zoom10k | 0.43 | 0.42 | 0.42 |

Ten thousand bars record in 524 KB instead of 1 573 KB.

What later recorders inherit:
- Culling moves from the command to the box: the painter replay tests each box against the clip
  (without it `QCPBars_Pan10k` was +24 %). The GPU replay builds the whole run, so its kept geometry
  survives a pan.
- Every recorder of many rects or ellipses uses the batch (answer 12): intervals, histogram columns,
  error-bar caps.
- `sortedByKey` waits for R5; the SoA layout is what R4 needs.
