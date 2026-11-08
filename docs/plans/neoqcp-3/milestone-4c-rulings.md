# NeoQCP 3.0 — milestone 4c rulings and deferred items

Milestone 4c: colour maps and the colour scale. One colour relation serves the three colour maps,
the 2D histogram and the line family. Each axis rect owns a colour scale, shown while something
uses it. Images are GPU textures moved by a pan. The two histograms share one API. Branch
`neoqcp-3.0`, commits `b3bc0bd..3783e07` plus this file (local, not pushed). Plan:
`docs/superpowers/plans/2026-11-07-neoqcp-3-milestone-4c.md`. Spec:
`docs/specs/2026-11-06-neoqcp-3-milestone-4-design.md` (M4c and the maintainer decisions of 11-07).
Benchmarks: `baseline.md` ("M4c start" and "End of milestone 4c").

## Results

- At `3783e07` on Qt 6.12: offscreen suite 2009 passed, 0 failed, 133 skipped; ASan the same, no
  sanitizer report (Qt's printing-library leaks suppressed); Xvfb whole binary 2142/0/0; clang
  `-fsyntax-only` clean on every `.cpp` changed in the milestone.
- src/: 48 files, +1729 / −2220. The colour maps' own GPU layer and its shaders are gone.
- Bench: a classic colour-map pan 0.48 -> 0.17 ms and a 1D histogram pan 0.18 -> 0.12 ms. Every row
  is inside its gate except the 1000-text zoom (R-21). See baseline.md, "End of milestone 4c".

## Rulings made during execution (what, why, cost if wrong)

- R-1 (plan): Task 7 updates every caller of the renamed histogram API, the benchmark included.
  Cost: none.
- R-2 (plan): Task 4 changes a 2.x test only where it asserts that a colour map starts with no
  scale. A 2.x program that sets its own scale must pass unchanged. Cost: a 2.x behaviour change
  slips through.
- R-3 (Task 1): the new rows are gated on the lower median of two series. The zoom row times the
  frame drawn while the resample is pending, which is what a user sees. Cost: a slower resample is
  not gated by that row (the resample rows gate it).
- R-4 (Task 2): images in a recording are never culled by the view, so a pan can bring them back.
  Cost: a texture per off-screen image, at most 16 (backlog).
- R-5 (Task 2): ColorMapPan read 0.44 against 0.41 on untouched code: noise, re-gated at Task 5.
- R-6 (Task 3): a user setter applies on the plottable before the scale echoes it back; an existing
  guard needs the plottable's own signal first. Cost: none seen; the scale stays authoritative.
- R-7 (Task 4): a visible colour bar costs about 0.07 ms per pan frame (bar hidden 0.13, shown
  0.20). It is the axes layer redrawing. Accepted; the colour-map gates were re-set with the bar
  shown: ColorMapPan 0.48, ColorMap2Pan 0.20, ColorMap2Zoom 0.22, Histogram2DPan 0.20. Cost:
  0.07 ms per pan frame for every coloured plot until the bar has its own layer (backlog).
- R-8 (Task 4): a scale a plottable creates starts with that plottable's gradient, which keeps
  colour map 2's transparent NaN. Cost: the rect scale's first gradient depends on which class
  arrived first.
- R-9 (Task 5): `QCPDrawable::dataRange()` became `drawnDataRange()`; the colour base's
  `dataRange()` (the colour range) clashed with it. Cost: one rename back.
- R-10 (Task 5): the classic map's image draws above painter content on its layer, as graphs do
  since milestone 3. Cost: an unported item under a map is covered until it is ported.
- R-11 (Task 5): QCPColorMap_Standard re-gated 0.22 -> 0.27 ms. With the scale detached it reads
  0.22: the cost is the visible bar (R-7). Cost: a 0.05 ms regression could hide under the gate.
- R-12 (Task 5): `draw()` writes the cached image through a const_cast; the image is a cache.
- R-13 (Task 6): contour labels are one raster tile over the visible grid, not text commands. Text
  commands are painter-only and would sit under the GPU image. Cost: labels are a picture in PDF
  and SVG exports (backlog).
- R-14 (Task 6): the "No QRhi" warnings in the bench come from its shared fixture plot, not from
  the GPU rows, which ran. Cost: none.
- R-15 (Task 6): ResampleFloat read 154 against 139 under load. The milestone-end bench read 136:
  it was load.
- R-16 (Task 7): plain members instead of wrapper classes for the bin count and the
  normalization. The shared part is the enum (`QCP::HistogramNormalization`) and two free
  functions. Cost: three setters repeat one `if`.
- R-17 (Task 7): the 2D histogram has `setBins(key, value)`, no `setBins(int)`: one count cannot
  name an axis. Cost: one overload added later.
- R-18 (Task 8): QCPGpu_MultiGraphColored100kPan re-gated 0.14 -> 0.21 ms. With the line detached
  from its scale it reads 0.13: the cost is the visible bar (R-7).
- R-19 (final review): a coloured line that joins an existing rect scale takes the scale's gradient
  and range, not its own. The scale is authoritative once it exists; a user who styles the bar
  first expects it to stick. Cost: a line's own setGradient before its first colouring is lost
  when the rect already has a scale.
- R-20 (final review): a vertical key axis on colour map 2 and the 2D histogram was fixed now,
  although it predates this milestone. A wrong picture is a bug. Cost: one transpose per image
  build, never per pan.
- R-21 (final): QCPGpu_Text1k re-gated 1.0 -> 1.1 ms. Against a build of the milestone's start it
  reads 1.039 against 1.091 ms. On the benchmark thread both builds run the same instructions, with
  the same cycles, cache misses and branch misses, and the text path's code did not change: the
  extra time is spent waiting on the software GPU. Cost: 0.05 ms per frame on a 1000-text plot,
  until it is checked on a real GPU (backlog).

## Found and fixed in review

- Task 2: a failed nearest sampler now returns like a failed linear one (no test: a sampler cannot
  be made to fail).
- Task 3: rescaling the data range ignored "recalculate" for colour map 2, the histogram's first
  binning and the classic map.
- Task 4: a visible scale on the main layer stopped a pan from shifting the data layer; the scale
  went to the axes layer.
- Task 5: no test pinned the hot cell's corner on every axis layout; one now does, and a mutation of
  the vertical-key corners makes it fail.
- Task 7: on a log value axis, bars and steps started at value 0, far below the axis; they now start
  at the axis bottom and follow it.
- Task 8: the fluent handle lost the line's colour setters with no replacement; it has the colour
  maps' `setGradient`, `setDataRange` and `setDataScaleType`.
- Final review (fresh reviewer, 10 findings, and opencode, 2 findings; one fix batch):
  - a line coloured through the deferred data swap never joined its rect's scale;
  - a 1D histogram exported while binning showed the old bars;
  - contours sat at the wrong key on a log key axis;
  - contour labels moved away from their lines on a reversed axis;
  - contour labels kept a stale, stretched tile after a zoom;
  - a slot deleting the scale on a gradient change was a use-after-free;
  - a slot deleting a line on a colour-scale change was a use-after-free;
  - a plottable moved to another plot kept its old plot's scale, and a multigraph was never taken
    off a scale on removal;
  - the rect's own scale ignored the theme;
  - new data dropped a scale the user had set on an uncoloured line;
  - a vector export tripled the size of an already large image;
  - colour map 2 and the 2D histogram drew a vertical key axis turned on its side;
  - the docs named removed functions.
- Fix batch review: keeping a user's scale skipped turning off the colour origin request, so an
  uncoloured line kept paying for it. The batch had also reconfigured the build against the
  system's Qt 6.11; every final number above was taken again on Qt 6.12.

Each fix has a test that failed first, except the sampler one above and the docs.

## Deferred minors (also in backlog.md)

- A colour map's legend icon stretches to the icon rect (Task 5).
- The export reference compare skips the plot's outer row and column (Task 5).
- A zero-height histogram bin draws no hairline at the base (Task 7).
- A step histogram's legend icon is a filled box (Task 7).
- A curve given data of another size keeps its colour values and its scale (Task 8).
- Moving a plottable to another rect's axis keeps the old rect's scale (F10d).
