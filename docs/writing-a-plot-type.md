# Writing a plot type

A plot type draws itself by recording commands. Derive from `QCPDrawn<Base>`, override
`draw(QCPDrawContext&)`, and append one command per shape with the context's helpers; the plot
records the list once and replays it on QPainter or on the GPU. Everything the base already does for
a plottable or an item (data, groups, legend row, selection, layers, hit propagation) keeps working.

The page builds on the range band in `examples/custom-drawable/`: a filled band between two series,
with a label. It is a complete plot type in 73 lines that only uses public headers.

## The whole class

````{literalinclude} ../examples/custom-drawable/rangeband.h
:language: cpp
````

- `RangeBand` derives from `QCPDrawn<QCPAbstractPlottable>`, so it is added to the plot by its
  constructor (`new RangeBand(plot.xAxis, plot.yAxis)`), appears in the legend and can be selected.
- A drawn item derives from `QCPDrawn<QCPAbstractItem>` instead, and a plottable with a data
  container from `QCPDrawn<QCPAbstractPlottable1D<MyData>>`. The base class keeps its data and its
  ranges; the adapter only answers the drawing virtuals.
- `draw()` is `const` and runs only when the recording is stale. It must not change the object.

## draw() records commands

`draw()` appends commands; it paints nothing itself. Each helper takes the shape and the style:
`polyline`, `polygon`, `fillBetween`, `rect`, `ellipse`, `markers`, `errorBars`, `text`, `path` and
`image`. The band records two of them:

````{literalinclude} ../examples/custom-drawable/rangeband.h
:language: cpp
:start-at: context.fillBetween
:end-at: context.text(
````

- `fillBetween` takes the upper series forward and the lower series backward, and fills between them.
- `text` takes a position, the string and a `QCPTextStyle`.
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
`QCP::Data`, `QCP::Pixels`, `QCP::AxisRatio`, `QCP::AnchorOffset` (an anchor plus a
`QCP::PixelOffset`) and the per-axis mixes `QCP::DataXRatioY` and `QCP::RatioXDataY`. `QCP::Data` and
`QCP::Pixels` do not add, so a data value cannot drift into a pixel slot.

The replay resolves them through the frame's <a href="api/classQCPTransform.html">`QCPTransform`</a>.

- **Data positions are free to move.** A pan, a zoom, a resize and an export resolve the same
  recording again; nothing is recorded twice.
- **Pixels do not follow a zoom.** A command in pixels stays where it is on screen when the axes
  move, which is what a fixed-size marker or a hairline wants.
- **Reading `context.transform()` costs a re-record on zoom.** Doing it marks the recording
  transform dependent: it is remade on a zoom, a log/linear change or a resize, and kept across a
  pan. Read it only when the geometry really depends on the pixel size, as `QCPBars` does for a bar
  width given in pixels; a bar width in plot coordinates never reads it and keeps zoom reuse.

## Telling the plot what changed

A setter stores the value and calls `contentChanged()`:

````{literalinclude} ../examples/custom-drawable/rangeband.h
:language: cpp
:start-at: void setData
:end-before: void setBrush
````

`contentChanged()` starts a new generation and marks the object dirty, which schedules the repaint.
It is the same call as `markDirty()`, which the classic bases already make from their setters
(`setPen`, `setSelected`, ...), so those keep working without a change.

The plot records again only when it must:

- after a setter, always;
- after a resize, a device-pixel-ratio change or a theme change, only when the recording holds text,
  a path, an image or `painter()` code, or when `draw()` read the theme: those are measured or
  rasterised in pixels;
- after a zoom, only when `draw()` read `transform()`;
- every frame, only when `draw()` called `recordEveryFrame()`.

A pan never records again. It replays the recording with a translation offset, and the GPU keeps
the geometry it already built.

## Ranges, hit tests and the legend icon

The defaults follow the recording, so most plot types need none of these overrides:

- `dataRange()` returns the recording's data box. An axis side bounds the box only when every
  position on that side is `QCP::Data` with no anchor; a log axis drops a box reaching zero or below.
  Return `std::nullopt` to say "no data range".
- `hitTest(QCP::Pixels at, QVariant* details)` measures the distance to the recorded geometry minus
  half the pen, answers 0 inside a visible fill, and uses the measured box for text. Return
  `std::nullopt` for a miss. `QCPBars` overrides it with its bar rectangles.
- `drawLegendIcon(QCPDrawContext& icon)` replays the recording fitted into the icon rect. Override it
  to draw something else in the legend.

A hit test that depends on data the recording does not contain (a bar's full rectangle, a stacked
base) is worth an override; everything else is free.

## What is fast

- Data positions: a pan and a zoom resolve and replay, and the GPU reuses its built geometry across
  a pan.
- Shared point vectors: the recording holds your `shared_ptr`, so a large series costs one copy at
  most, in your own `draw()`.
- Many small shapes: neighbouring solid commands merge into one draw, so a bar chart does not pay one
  draw call per bar.
- Recording once: put everything in `draw()`, and reach for `recordEveryFrame()` only when a position
  exists in pixels alone.
- Avoid `context.painter()` for shapes a helper covers: painter code goes into the buffer's QPainter
  pass, which the same layer's GPU content is drawn over, so a painter command can end up under a
  later object; the GPU also cannot merge it. Keep it for what the helpers do not model (a text
  document, an exotic path).
- Avoid reading `transform()` and `clip()` in a hot plot type: both are per-frame facts, and the
  first one costs a re-record per zoom.
