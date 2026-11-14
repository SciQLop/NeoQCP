# Known limits of 3.0

NeoQCP 3.0 ships with a few known limits.
None of them loses data. Most cost speed, or look slightly off in a rare case.
Each entry says what you see, and what to do about it when there is something.
They are grouped by the guide page they belong to.

## Line and scatter graphs

See {doc}`graphs`.

- **Error-bar caps are slightly lopsided on a log key axis.**
  The cap width is set in data units, the same on both sides.
  On a log axis, the left and right halves then differ on screen.
- **Selected error bars all use the selected pen.**
  With part of a graph selected, every error bar of that component looks selected.
  The line and the markers show the selection correctly.
- **A column named "Component 0" by hand counts as unnamed.**
  A one-column graph's legend row then shows the graph's name instead.
  Workaround: pick any other name.

## Other plottables

See {doc}`plottables`.

- **Overlapping OHLC bars of two colours do not interleave.**
  The bars of one colour are drawn together, then those of the other.
  Where they overlap, the same colour always ends on top.
- **A zero-height histogram bin draws no hairline at the base.**
  An empty bin leaves a gap in the baseline on a linear axis.
- **A step histogram's legend icon is a filled box**, not a line sample.
- **`QCPIntervals::pixelOf()` returns -1 as y before the first layout, and 0 as x without a key axis.**
  Neither is reported. Replot once before asking for pixels.
- **Removing data at a NaN key does nothing on the classic containers.**
  `data()->remove(NaN)`, `removeBefore(NaN)` and `removeAfter(NaN)` on a curve, bars or financial chart remove nothing.
  They give no warning, as in 2.x. A graph reports the same call as a misuse.

## Colormaps

See {doc}`colormaps`.

- **A colour map may use a colour scale of another axis rect.**
  Nothing checks it. The bar then sits next to the other rect.
  Prefer the rect's own scale, or one you place yourself next to the right rect.
- **A colour map's legend icon stretches to the icon size.**
  It no longer keeps its aspect ratio. You only see it with a non-default icon size.

## Items

See {doc}`items`.

- **A tracer only follows a graph.**
  It cannot follow a curve or a polar graph. Workaround: place it yourself with `position->setCoords()`.
- **An unclipped tracer's crosshair spans the axis rect**, not the whole widget as in 2.x.
- **There is no "data point plus pixel offset" position.**
  Workaround: give the item a position in pixels (`ptAbsolute`) and anchor it to a position in data.
- **Adding an item taken from a plot resets its positions to the default axes**, even on the same plot.
  Set its axes again after `add()`.
- **During `clearItems()`, a `destroyed()` slot sees `itemCount() == 0`.**
  All items are unlisted before the first one is deleted.
- **An item a `destroyed()` slot adds during `clearItems()` survives the clear.**

## Interactions

See {doc}`interactions`.

- **Signals still queued when a plot is deleted are never delivered.**
  This only happens when a slot deletes the plot while other signals wait.
  `QObject::destroyed` still fires.

## Styling

See {doc}`styling`.

- **A graph guesses whether you set its selection decorator.**
  A decorator styled back to exactly the default looks untouched.
  The graph then draws the selection the 3.0 way, with its own colours.

## Polar plots

See {doc}`polar`.

- **A rubber-band selection selects through the whole disc**, not only the part under the band.
- **`rescaleAxes()` also moves the radial axes of polar plottables with data.**
- **A rotated radial tick label's click box is its upright bounding box.**
  A click in an empty corner of that box still hits the label.

## Coordinate systems

See {doc}`coordinate-systems`.

- **A mapping must be invertible.** `QCPCustomMapping::inverse` is required, so a one-way mapping cannot be built.
- **The polar inverse returns the angle modulo the angular range.**
- **A radius of the wrong sign on a log radial axis maps to NaN.**
- **Error bars on a user system draw one bar per row.**
  On a cartesian system crowded bars merge into a band. Here 100 000 rows record 100 000 bars per frame.
- **A custom area should override `allAxesRaw()`.**
  Its default converts `allAxes()` on every call, which is slow when axes are looked up often.

## Export

See {doc}`export`.

- **Contour labels are a picture in PDF and SVG**, not text.
- **Financial and error-bar legend icons can be misplaced under a scaled, rotated export.**
  They draw in pixels and assume a plain fit of the icon rect.

## Performance

See {doc}`performance`.

- **Clicking or hovering near a big graph or curve maps every point.**
  A hover over a million-point graph pays that on each mouse move.
- **A user system has no culling and no thinning.**
  Every point is mapped on every repaint, and every click scans every recorded shape.
- **A big graph shows blank edges during a pan**, until the redraw 150 ms after the pan stops.
- **Zooming a million-point graph costs about 0.7 ms**, a little over the target.
- **A pan on a log key axis records the graph again on every frame.**
  On a linear axis a pan only moves the picture.
- **A scatter-only graph of millions of points has no point cap.**
  `setScatterMaxPoints` is gone. Thin the markers yourself, or with `setScatterSkip()`.
- **A visible colour bar costs about 0.07 ms per pan frame.**
- **Images in a recording are never culled by the view.** Each one keeps its texture, at most 16.
- **A recording made again allocates fresh buffers.** A click that selects an item costs a few microseconds more.
- **Each item pays a small overhead per replay.**
- **A span whose edge is in pixels, or on an anchor, records again on every frame.**
- **Without the GPU on a HiDPI screen, lines stay one device pixel wide.**
  Plottables, grids and their painter callbacks keep thin lines in the painter fallback.
- **Dragging hundreds of items rebuilds the item index often.**
- **A click walks the plot's children three times.**
- **A layer with more than four raster styles draws the extra ones with `QPainter`.**
- **A free-form polygon with a solid brush is drawn with `QPainter`**, not on the GPU.
- **Markers are not instanced on the GPU, and commands are not culled by data range.**

## API reference

See {doc}`api`.

- **A handle's hash changes when its object dies.**
  Do not keep handles in a `QSet` or `std::unordered_set` longer than their objects live.
- **`QPointer<T> p = handle;` does not compile.** Write `QPointer<T> p(handle);` instead.
- **Properties have no `BINDABLE` storage.**
  Bind through Qt's adaptor: `QBindable<T>(object, "name")`.

## Migrating from QCustomPlot

See {doc}`migrating`.

- **A 2.x program now shows a colour bar it never asked for.**
  Each axis rect shows its own scale once a colour map or a coloured line uses it.
  To hide it, call `setColorScale(nullptr)` on the plottable. A program that made its own scale is unchanged.
- **A debug build aborts at the next draw when a plottable's axis is deleted.**
  The default misuse handler stops debug builds. Install your own with `qcp::setMisuseHandler()` to log instead.
