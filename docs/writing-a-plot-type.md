# Writing a plot type

A plot type draws itself by recording commands. Derive from `QCPPlottable` (`QCPItem` for an
annotation), override `draw(QCPDrawContext&)`, and append one command per shape with the context's
helpers; the object records the list once and replays it on QPainter or on the GPU. Everything the
base already does for a plottable or an item (data, groups, legend row, selection, layers, hit
propagation) keeps working.

The page builds on the range band in `examples/custom-drawable/`: a filled band between two series,
with a label. It is a complete plot type in 71 lines that only uses public headers.

## The whole class

````{literalinclude} ../examples/custom-drawable/rangeband.h
:language: cpp
````

- `RangeBand` derives from `QCPPlottable`, so it is built without a plot
  (`plot.add<RangeBand>()` places it), appears in the legend and can be selected.
- A drawn item derives from `QCPItem` instead, and a plottable with a data container from
  `QCPPlottable1D<MyData>`. The base keeps its data and its ranges, owns the `QCPDrawnCore` that
  records and replays, and its `draw(QCPPainter*)` is final: the recording is the only way it draws
  (one exception: the value range ignores its key window, see
  [Ranges](#ranges-hit-tests-and-the-legend-icon)).
- `draw()` is `const` and runs only when the recording is stale. It must not change the object.

## draw() records commands

`draw()` appends commands; it paints nothing itself. Each helper takes the shape and the style:

- Lines and areas: `line(a, b, pen)`, `polyline(data, pen)`, `polygon(points, pen, brush)`,
  `fillBetween(upper, lower, brush)`.
- Boxes: `rect(a, b, pen, brush)`, `rect(QRectF, pen, brush)` in data coordinates,
  `roundedRect(a, b, 6_px, pen, brush)`, `ellipse(a, b, pen, brush)`.
- Discs: `circle(centre, radius, pen, brush)` with the radius in data units, and
  `circle(centre, 5_px, pen, brush)` with one in pixels.
- Symbols and bars: `markers(data, style)`, `errorBars(centres, spans, pen)`.
- Labels and pictures: `text(at, string, style)`, `text(a, b, string, style)`, `image(a, b, image)`,
  `path(origin, path, pen, brush)`.

The band records two of them:

````{literalinclude} ../examples/custom-drawable/rangeband.h
:language: cpp
:start-at: context.fillBetween
:end-at: context.text(
````

- `fillBetween` takes the upper series forward and the lower series backward, and fills between them.
- `text(at, ...)` takes a position, the string and a `QCPTextStyle`. `text(a, b, ...)` lays the string
  out inside the box from `a` to `b`: its words are wrapped at the box's width, `style.padding` keeps
  them off the edges, and an empty string draws the box alone.
- A pixel-radius `circle` is a command of its own rather than a marker: markers live on the scatter
  layer, above every plottable, while a disc keeps its place in the call order.
- Point data is passed as a `std::shared_ptr<const std::vector<QPointF>>`, so the recording shares
  your points instead of copying them. Build them in `draw()` from the data you own and move them
  into the `shared_ptr`.
- `QCPTextStyle` holds everything a text command needs: font, colour, the box's pen and brush,
  alignment, text flags, rotation, padding (`QMarginsF`) and `mathText` to measure and draw through
  the label renderer, as `QCPItemText` does.
- Two escapes exist for code that cannot be a command: `context.painter(callback)` runs raw
  `QPainter` code in pixels with the frame's transform to map with, and `context.recordEveryFrame()`
  marks a recording that must be made again for every frame (see below).

## Data units, pixels and the transform

Commands keep their positions in the space they were written in. A position is a `QCP::Position`:
`QCP::Data`, `QCP::Pixels`, `QCP::AxisRatio`, `QCP::DataOffset` (a data point plus a
`QCP::PixelOffset`), `QCP::AnchorOffset` (an anchor plus a `QCP::PixelOffset`) and the per-axis mixes
`QCP::DataXRatioY` and `QCP::RatioXDataY`. `QCP::Data` and `QCP::Pixels` do not add, so a data value
cannot drift into a pixel slot.

The replay resolves them through the frame's <a href="api/classQCPTransform.html">`QCPTransform`</a>.

- **Data positions are free to move.** A pan, a zoom, a resize and an export resolve the same
  recording again; nothing is recorded twice.
- **Pixels do not follow a zoom.** A command in pixels stays where it is on screen when the axes
  move, which is what a fixed-size marker or a hairline wants.
- **An offset riding on a data point.** `QCP::Data{1, 2} + QCP::PixelOffset{3, 4}` is a
  `QCP::DataOffset`: it follows the data through a pan and a zoom and keeps the pixels it sits at,
  where a `QCP::Pixels` snapshot of the same point would freeze where the frame first put it. Every
  helper that takes a `QCP::Position` takes one, a `polygon`'s vertices and `circle`'s centre
  included.
- **A length is a length.** The shape arguments that are distances rather than points take a
  `QCP::PixelLength`, written `6_px` with `using namespace QCP::Literals`, so a bare number cannot
  pass for one.
- **Reading `context.transform()` costs a re-record on every transform change.** Doing it marks the
  recording transform dependent: it is remade on a pan as well as on a zoom, a log/linear change or a
  resize, because the pixels it baked from the transform are not a translation of the old ones.
  Read it only when the geometry really depends on the pixel size, as `QCPBars` does for a bar width
  given in pixels; a bar width in plot coordinates never reads it and keeps pan and zoom reuse.
  Grouped or stacked bars, and any bar whose width is not in plot coordinates, are pixel geometry
  too, for the same reason.
- **The axes, when you need them.** The 2.x members `mKeyAxis` and `mValueAxis` are private. Code
  that needs an axis calls the protected `keyAxisRaw()` and `valueAxisRaw()` (raw pointers, cheap on a
  per-point path), or `system()` on any frame. The raw pair is null on a non-cartesian system.

## Telling the plot what changed

A setter stores the value and calls `update()`:

````{literalinclude} ../examples/custom-drawable/rangeband.h
:language: cpp
:start-at: void setData
:end-before: void setBrush
````

`update()` starts a new generation and marks the object dirty, which schedules the repaint.
The classic bases already make the same call from their setters (`setPen`, `setSelected`, ...),
so those keep working without a change.

The plot records again only when it must:

- after a setter, always;
- after a resize, a device-pixel-ratio change or a theme change, only when the recording holds text,
  a path, an image or `painter()` code, or when `draw()` read the theme: those are measured or
  rasterised in pixels;
- after a zoom or a pan, only when `draw()` read `transform()`;
- every frame, only when `draw()` called `recordEveryFrame()`.

A pan of a recording whose every position is `QCP::Data` never records again, and the layer it is on
shifts the texture it already painted instead of repainting: nothing is extruded and nothing is
uploaded. A recording that holds a pixel, a ratio or an anchor position, or one that read
`transform()`, is rebuilt for the new transform, because a single shift cannot express what those
pixels do. Painter-only content is the other thing a shift cannot carry: a text or a `painter()`
command recorded after GPU content is pixels of the viewport, so the frame that holds it is replayed
on every pan instead of being shifted.

## Ranges, hit tests and the legend icon

The defaults follow the recording, so most plot types need none of these overrides:

- `dataRange()` returns the recording's data box. An axis side bounds the box only when every
  position on that side is `QCP::Data` with no anchor; a log axis drops a box reaching zero or below.
  Return `std::nullopt` to say "no data range".
- `hitTest(QCP::Pixels at, QVariant* details)` measures the distance to the recorded geometry minus
  half the pen, answers 0 inside a visible fill, and uses the measured box for text. Return
  `std::nullopt` for a miss. `QCPBars` overrides it with its bar rectangles.
- `getValueRange(foundRange, signDomain, inKeyRange)` ignores `inKeyRange` here: a recording keeps
  one data box, not the values of each key, so the window cannot narrow it. Override the range if
  your type needs the visible-key rule (as `QCPBars` does).
- `drawLegendIcon(QCPDrawContext& icon)` replays the recording fitted into the icon rect. Override it
  to draw something else in the legend.

A hit test that depends on data the recording does not contain (a bar's full rectangle, a stacked
base) is worth an override; everything else is free.

## What is fast

- Data positions: a pan shifts the texture the layer already has, and a zoom resolves and replays
  the recording without recording it again.
- Shared point vectors: the recording holds your `shared_ptr`, so a large series costs one copy at
  most, in your own `draw()`.
- Many small shapes: neighbouring solid commands merge into one draw, so a bar chart does not pay one
  draw call per bar; a pan re-submits them in their recorded order, so stacking never changes.
- Recording once: put everything in `draw()`, and reach for `recordEveryFrame()` only when a position
  exists in pixels alone.
- Avoid `context.painter()` for shapes a helper covers: the GPU cannot merge a callback into the
  neighbouring geometry, and a callback is pixels of the frame, so a layer holding one is painted
  again on a pan instead of being shifted. Keep it for what the helpers do not model (a text
  document, an exotic path).
- Avoid reading `transform()` and `clip()` in a hot plot type: both are per-frame facts, and the first
  one costs a re-record per transform change, a pan included.
