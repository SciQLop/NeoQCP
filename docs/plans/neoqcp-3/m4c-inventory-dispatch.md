# Dispatch: m4c inventory (read-only)

Write a factual inventory for NeoQCP 3.0 milestone 4c: colour maps, histograms and the colour-scale
relation onto the drawing API. Output: `/home/jeandet/Documents/prog/NeoQCP-3.0/docs/plans/neoqcp-3/m4c-inventory.md`.
Model its shape on `docs/plans/neoqcp-3/m4b-inventory.md` (same repository): every claim carries `file:line`,
guesses are marked **[guess]**, paths relative to `/home/jeandet/Documents/prog/NeoQCP-3.0`, branch `neoqcp-3.0`.

Read for context first: `docs/specs/2026-11-06-neoqcp-3-milestone-4-design.md` (M4c, answers 13 and 14),
`docs/specs/2026-11-02-neoqcp-3-object-model-design.md` (decision 14), section 3 of
`docs/specs/2026-11-04-neoqcp-3-drawing-api-design.md`, and section 3 of `docs/plans/neoqcp-3/m4-inventory.md`
(older and coarser: re-check every line number, the code moved since).

## Sections to write

1. **`QCPColorMap`** (`src/plottables/plottable-colormap.{h,cpp}`): its data class, how `draw` builds the
   image (functions, caching, when it rebuilds), interpolation and tight-boundary options, legend icon,
   hit test, data range for autoscale, every colour-scale member and connection.
2. **`QCPColorMap2`** and **`QCPHistogram2D`**: data sources, pipelines (`QCPColormapPipeline`,
   `QCPHistogramPipeline`), what the pipeline result is (a resampled grid? an image?), `draw`, legend icon,
   hit test, contour overlay, every colour-scale member and connection, how they use `QCPColormapRenderer`.
3. **`QCPColormapRenderer`** (`src/painting/colormap-renderer.{h,cpp}`) and the texture layer
   (`src/painting/colormap-rhi-layer.{h,cpp}`): what is uploaded (scalar grid + gradient lookup, or RGBA?),
   when it is re-uploaded (data change, gradient change, range change, pan, zoom), shaders used, how a pan is
   served, the QPainter fallback, and how it composes with other layers (which pass, which buffer).
4. **`QCPHistogram`** (1D): pipeline, `draw`, `drawColumnOnGpu`, legend icon, hit test. Does it map values to
   colours at all?
5. **`QCPColorScale`** (`src/layoutelements/layoutelement-colorscale.{h,cpp}`): its signals and slots,
   `rescaleDataRange` (which plottables it sees and how), how it is placed today, who owns it, what happens
   when it or a plottable using it is deleted. Also `QCPColorGradient` change notification.
6. **The line family's colour-by-scalar** after milestone 4b: `qcp::ColorScalarMapper`
   (`src/plottables/plottable-color-mapper.h`), `QCPAbstractLine`'s colour API (`setColorValues`,
   `setColorGradient`, `setColorRange`, scale type), and whether any `QCPColorScale` is involved.
7. **The drawing API's image support**: `ImageCmd` in `src/painting/draw-commands.h`, its painter replay, its
   RHI replay (raster fallback, `kMaxImages`, texture count), `QCPDrawContext::image`. What it would take for an
   image command to be a GPU texture that survives a pan without re-upload.
8. **Tests**: every test class that covers these classes (name, file, slot count), which slots read private
   caches or renderer internals that a port would break.
9. **Benchmarks**: every row in `tests/benchmark/benchmark.cpp` that times a colour map, colour map 2,
   histogram 2D, histogram 1D or colour scale, with what it does. Name the rows missing for a pan and a zoom
   of each class.
10. **Public API to keep** for each class (setters, getters, signals), as a list.

## Rules

- Read only. Do not edit any source file, do not build, do not run tests.
- Never use `cat` (it is aliased); use `command cat`, `sed -n` or your read tool.
- Short plain sentences. No speculation without **[guess]**.
- When done, reply only: DONE and the line count of the file.
