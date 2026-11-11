# NeoQCP 3.0 — backlog

Work that was deferred during the 3.0 milestones, to triage before 3.0 ships. Each item says what a
user would see and where the code is. Fixed items are deleted, not ticked.

## From milestone 4j (coordinate systems of the user's own)

- **Culling and decimation on a user system** (J-6): every point is mapped on every repaint and every
  click scans every point. `QCPDrawnCore::hitTest`'s box gate (`drawable.cpp:902`) and the item index
  (`item.cpp:1860`) stay cartesian-only. Upgrade: a system virtual mapping a pixel rect to a data box,
  then decimate in data space by `keyWindow()`.
- **The bracket selection decorator on a user system**: `QCPSelectionDecoratorBracket::drawDecoration`
  bails without `keyAxis()`, so a selected skew graph shows pen and brush but no brackets
  (`selectiondecorator-bracket.cpp`). Map through `system()` or report the misuse once.
- **A tracer on a polar graph**: `QCPItemTracer::setGraph` still takes a `QCPGraph*`; a tracer on a
  polar graph stays unsupported. A tracer on a user system follows its graph (4j).
- **`area->add<Item>()`**: items are still built by the plot and bound with `setSystem()`; J-8 keeps
  the area factory out. Additive later.
- **A position whose frame goes away is inert without a report**: `QCPItemPosition` has no
  lost-system report (a plottable has one). A pixel read returns `(0, 0)` and draw-time placement is
  refused quietly. If a user needs a diagnostic, add one report per position.
- **A curve's colour scale on a user system**: `QCPCurve` no longer requires `capColourScale`, but
  `followKeyAxisRectScale()` reads `mKeyAxis`, so it does not follow the rect's scale on a user system.
  Give it one with `setColorScale()`, or map the scale through the system.
- **`QCPDrawn_*` A/B rows under Xvfb are bimodal** on this machine (repeated five-pair runs of one
  pair read +7 % then -26 %, QRhi software stack). Do not decide a 5 % gate on those rows alone.

## From milestone 4h (the polar graph is a plottable)

- **`QCPPolarArea::add<T>`** lacks the axis-rect form's friendly `static_assert`, builds the object before a
  capability refusal, and only builds types with a polar-axes constructor.
- **`QCPDrawnCore::hitTest` has no box cull for non-cartesian frames**: every command is scanned.

## From milestone 4g (coordinate systems and the plottables on them)

- **`QCPMultiGraph` requires `capColourScale`** (it is a `QCPAbstractLine`) besides `capLanes`.
- **Polar rubber-band selection selects through the disc**; `rescaleAxes` moves the radial axes of polar
  plottables with data (both follow the contract).
- **A listener connected and disconnected within one setter** lets that emission take the direct path.
- **`defersEmissions()` is O(systems of the area)** per mapping change.
- **Resolving an item position outside the drawing path** needs the `report` flag to stay quiet.
- **Colour-scale relationship check** (a colour map using another rect's scale) deferred with design 2.10.

## From milestone 4f (areas, the axis base, the polar area, the frame variant)

- **Polar tick-label hit boxes** are the upright extent around the anchor: offset at 0 and 180 degrees on
  the angular axis, generous at the corners of rotated radial labels, hittable only after the first draw.
  Fix: the label painter returns the rect it drew.
- **`QCPCustomMapping::inverse` is pure**: a one-way mapping cannot be built. Polar `inverse` returns the
  angle modulo the angular range; a wrong-sign log radius maps to NaN.
- **The axis hot block can straddle a cache line**: 36 bytes in a 16-byte aligned object.
- **`QCPAxis_SetRange10k` is +5 %** (gate 15 %): look again if `setRange`'s path changes.
- **`TestDrawable::pixelAnchoredShapeIsHittable`** failed once in one full run, never again.

## From milestone 4e (the remaining plottables and items record; one span class)

- **A recording made again allocates fresh buffers** (command list, context, pens): a click that
  selects an item is +23 % (about 3.5 us). Upgrade: recycle the previous recording's buffers through
  `QCPDrawContext`.
- **Per-item replay overhead** (view, staleness check, frame; `qcpDrawnViewOf`,
  `QCPDrawnCore::ensureRecording`) is the rest of the item rows' cost (mixed kinds +4..6 %). A layer
  whose items all move with the data could shift its texture on a pan
  (`QCPLayer::canTranslateInsteadOfRepaint`).
- **A span edge in pixels or on an anchor records every frame** (`QCPItemSpan::draw`).
- **`QCPLineEnding::draw` builds a `QPainterPath` per call** for its painter users (axes).
- **An unclipped tracer's crosshair spans the axis rect**, not the widget (2.x spanned the viewport).
- **Rect and ellipse keep a positions-based `dataExtent`**: `asPosition()` gives pixels for a position
  mixing plot and absolute coordinates.
- **The layer index pads an error-bar command by its pen only**, not the whisker half-width; a click on
  a whisker tip just outside the data box can miss through the index (`selectTest` is exact).
- **Two-colour OHLC strokes are grouped per colour**: overlapping entries of different colours no
  longer interleave in entry order.
- **A painter-only plottable records an empty list when hit-tested**, so its `recordingCount()` counts
  hit tests; a subclass that forgets `draw(QCPDrawContext&)` draws nothing, silently.

## From milestone 4d (axes, linked axes, alignment groups)

- **A log linked axis is re-clamped late.** After `setScaleType(stLogarithmic)` on a linked axis whose
  source range crosses 0, the axis shows no ticks until the source's range next changes
  (`QCPAxis::refreshFromLink`, `src/axis/axis.cpp`). No drag or zoom reads that range.
- **The colour scale asks its rect for the drag and zoom axis twice per event**, building two short
  lists (`QCPColorScale::rangeDrag()`/`rangeZoom()`, `layoutelement-colorscale.cpp`). Negligible.
- **An alignment group hears a plot's own move, not an ancestor's move inside the window**
  (`simplify:` in `src/alignment-group.cpp`; upgrade: an event filter on the ancestors). A plot moved
  sideways by its container keeps its old edge until its next layout.

## From milestone 4c (colour maps and the colour scale)

- **A visible colour bar costs about 0.07 ms per pan frame.** The bar sits on the axes layer, which a
  pan repaints for its ticks. A layer of its own that a pan leaves alone would remove the cost
  (rulings T4, T5, T8). `QCPAxisRect::colorScale()`.
- **Contour labels are a picture in PDF and SVG exports.** They are one raster tile over the visible
  grid, not text (ruling T6). `QCPContourOverlay::recordLabels`.
- **Images in a recording are never culled by the view.** Each one keeps a texture even off screen,
  at most 16 (ruling T2). `src/painting/command-replay-rhi.cpp`.
- **A 2.x program now shows a colour bar it never asked for.** Each axis rect shows its own scale as
  soon as a colour map or a coloured line uses it. A 2.x program that made its own scale is
  unchanged. Release notes must say it.
- **A legend icon of a colour map stretches to the icon rect.** It used to keep its aspect ratio;
  only visible with non-default icon sizes (Task 5).
- **The export reference compare skips the plot's outer row and column.** The image clip differs by
  half a pixel from the old drawImage (Task 5). `tests/auto/test-colormap`.
- **A zero-height histogram bin draws no hairline at the base** on a linear axis (Task 7).
- **A step histogram's legend icon is a filled box**, not a line sample (Task 7).
- **A curve given data of another size keeps its colour values** and so stays on its scale: the
  bar stays visible while the curve draws uncoloured (Task 8). `QCPCurve`.
- **Moving a plottable to another rect's axis keeps the old rect's scale** (F10d).
  `setKeyAxis` does not re-run the join a fresh plottable does.
- **A 1000-text zoom is 5 % slower on llvmpipe** (ruling R-21): 1.04 -> 1.09 ms, with the same CPU
  work on the plot's thread. Check the row on a real GPU before chasing it.

## From milestone 4b (the line family)

- **No test checks that marker colours reach the GPU.** Coloured markers go through the markers
  command into the scatter layer, and only the CPU replay is tested for colours.
  `src/painting/command-replay-rhi.cpp` (the markers branch).
- **Error-bar caps are slightly lopsided on a log key axis.** The cap half-width is converted from
  pixels to a symmetric width in data, so the left and right halves differ on screen.
  `QCPMultiGraph::recordErrorBars`.
- **A gap inside a filled curve goes into the fill polygon as is.** The polygon then has a vertex at
  NaN; 2.x did the same. `QCPCurve::draw`.
- **Clicking near a curve converts every point to pixels on each mouse move.** 2.x did the same.
  A cache keyed on the transform would make hover cheap on big curves. `QCPCurve::pointDistance`.
- **Zooming a 1M-point multigraph costs 0.69 ms against a 0.61 limit** (ruling R-8). The data-space
  envelope keeps about 2 150 points per column against 1 907 before; the extruder pays for them.
- **A big graph shows blank edges during a pan until the 150 ms settle** (ruling R-16). The reduced
  data covers the view while the recording claims three views. Reducing the whole window on the
  settle would fix it for three times the GUI-thread cost.
- **The data sources still carry the old pixel line API** (ruling R-17): `getLinesAll`,
  `getOptimizedLineDataAll` and the indexed forms are only used by tests. Remove them with the
  data-source work that retires QCPGraph2.

## From earlier milestones

Each rulings file keeps its own list; they are not copied here so they cannot drift.

- Milestone 1: `milestone-1-rulings.md`, "Deferred minors".
- Milestone 2: `milestone-2-rulings.md`, "Deferred minors (for the final triage)".
- Item scaling and item index: `item-scaling-rulings.md`, "Deferred minors".
- Milestone 3: `milestone-3-rulings.md`, "Deferred (for milestone 4 and the final triage)".
- Milestone 4a: `milestone-4a-rulings.md`, "Deferred minors".
