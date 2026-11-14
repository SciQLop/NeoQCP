# Extension challenge: benchmark selection

Survey of 3.0 at 42956b6, milestone 6 evidence (spec `2026-11-14-neoqcp-3-milestone-6-design.md`).

Goal: 10 small programs a user might really write to extend NeoQCP 3.0 (branch neoqcp-3.0) with
their own plottables, items, tickers and coordinate systems, and the drawing (recording) API under
them. For each one: is writing your own plot type easy, elegant, complete, and fun?

Sources: docs/writing-a-plot-type.md, concepts.md, items.md, coordinate-systems.md, performance.md,
interactions.md, examples/custom-drawable/, the headers (painting/draw-context.h, drawable.h,
plottables/plottable.h, plottables/plottable1d.h, items/item.h, axis/axisticker.h,
coordinate-system.h, coords.h, coords-custom.h) and the in-tree types (QCPBars, QCPFinancial,
QCPStatisticalBox, QCPItemRect, QCPItemLine, QCPItemText). Nothing was built.

Verdicts: **easy** = the API covers it simply. **awkward** = possible, but clumsy (the reason says
why). **blocked** = a capability is missing.
Python: what a user subclassing from Python (Shiboken6) would hit. Today NeoQCP has no Python
binding at all; SciQLopPlots binds its own wrappers and not one `QCP*` class. So the column says
what a binding of *this* code path would trip on.
Joy (1-5): how fast a curious user goes from idea to pixels, how much ceremony comes first, whether
the calls read like speech ("a circle here, a label there"), and whether change-and-see is
immediate. Reference points: p5.js / Processing (`circle(x, y, d)`, `fill()`, a `draw()` loop),
canvas 2D (path builder, save/restore state), Dear ImGui draw lists (`AddCircleFilled(c, r, col)`).

## Summary

| # | Challenge | Exercises | Verdict | Python | Joy | One-line reason |
|---|---|---|---|---|---|---|
| 1 | Lollipop chart | own data, per-point hit test, selection look, legend icon | awkward | blocked: template 1D base, `shared_ptr<const vector>`, `optional` | 2: 4 overrides before selection works | commands carry no data index, so hit test and selected look are hand-written; docs name a `QCPDrawn<>` that does not exist |
| 2 | Callout & map pins | item, anchors, mixed coordinates, data- vs pixel-unit text | awkward | blocked: `std::variant` positions, int-id anchor virtual | 2: the snapshot dance in every draw | no "data point + pixel offset" position; 2.x position mixes silently re-record every frame; anchors are an int `switch` in pixels |
| 3 | Roman / musical-beat ticker | custom axis ticker | easy | friction: `QSharedPointer` ownership of a Python subclass | 4: one override, 2.x shape | `getTickLabel()` / `getTickStep()` overrides; in-place changes need a hidden `requestRepaint()` |
| 4 | Skew-T log-P diagram | custom coordinate system | awkward | blocked: `std::span`, `shared_ptr` mapping, revision cache | 2: three classes before a pixel | mapping + system + rect subclass, a hand-kept revision cache; grid and axes stay cartesian, so isotherms are drawn by hand |
| 5 | Oscilloscope with persistence | animation, live update, per-frame cost | easy | friction: per-tick GIL hop into `draw()` | 3: a `QTimer` and a setter, no frame clock | per-vertex colours do the fade; every tick re-records the whole trace and there is no frame/vsync hook |
| 6 | Spline editor | mouse drag on own points, hover, keys | awkward | blocked: protected event virtuals, `QVariant` details | 2: undocumented virtuals | mouse virtuals are protected 2.x leftovers, absent from the guide; no hover routing; keys go to every plottable |
| 7 | Game of Life, 1024 x 1024 | large custom drawing, GPU path, click a cell | easy | friction: `QImage` per generation crosses the binding | 4: one `image()` call shows the board | `image(Data, Data, QImage)` is a GPU quad; a million rects also batch, but only if you know the batching rule |
| 8 | Snake | game: data grid, timer, keyboard, score text, game-over overlay | awkward | blocked (as 1 and 6) | 3: plays quickly, then the text vanishes | keys only reach plottables after a click gives focus; score text recorded with the board is drawn *under* the GPU rects |
| 9 | Brick breaker | game: physics, paddle by mouse, collisions, many static bricks | awkward | blocked (as 1 and 6) | 2: hit tests are pixel-only | collision has to be your own data-space math; one change re-records all bricks; split into two plottables by hand |
| 10 | Python lollipop | the whole of challenge 1 from Python | blocked | blocked | 1: cannot start | no binding; the recording types (`variant`, `span`, `shared_ptr<const vector>`, `optional`, `std::function`) need custom conversions |

Counts: easy 3, awkward 6, blocked 1.

## 1. Lollipop chart (custom plottable)

A stem from 0 to each value and a disc at its tip. Click selects one lollipop; the selected one is
drawn thicker. The legend shows one small lollipop.

- **Uses:** `QCPAbstractPlottable1D<LollipopData>` (its data type must provide `sortKey`,
  `fromSortKey`, `sortKeyIsMainKey`, `mainKey`, `mainValue`, `valueRange`), `draw(QCPDrawContext&)
  const`, `context.polyline(span<Position>)` or one `rect()` per stem, `context.markers()`,
  `hitTest(QCP::Pixels, QVariant*)`, `setSelectable(stSingleData)`, `getDataSegments()`,
  `selectionDecorator()->pen()`, `drawLegendIcon(QCPDrawContext&)`.
- **Verdict: awkward.**
  - The default hit test answers "the whole recording", so any per-point selection means a full
    `hitTest` override that recomputes geometry the recording already has.
  - The selected look is manual: read `getDataSegments`, pick the decorator's pen, record twice. A
    decorator subclass still draws through `drawDecoration(QCPPainter*)`, so `QCPBars` needs a
    `typeid` test and a painter callback to honour it.
  - The default legend icon fits the whole recording into the icon. A sensible icon needs the
    `QCPBars` trick: record an invisible rect to set the box, then the shape.
  - `docs/writing-a-plot-type.md` says "derive from `QCPDrawn<Base>`". No such class exists; the
    example derives from `QCPAbstractPlottable`. A first reader stops here.
  - A data type is a six-function duck-typed concept, checked only by template errors.
- **Lines:** ~110 (data type 20, class 90).
- **Python:** blocked. The 1D base is a template, and `markers()` wants a
  `shared_ptr<const vector<QPointF>>`.
- **Joy 2:** four overrides and a data concept before a click selects one stem.
- **API idea:** `context.index(i)` (or a scoped `context.forDatum(i)`) tags the next commands with a
  data index; the default hit test, the rubber band and the selected look then work per datum.

## 2. Callout and map pins (custom item, mixed coordinates)

A callout: a rounded box placed in axis-rect ratio, with a pointer whose tip sits on a data point.
Anchors `tip`, `boxTop`, `boxBottom` so other items can hang on it. On the same map: fixed-size pins
at data points with a name 8 px above each pin, and room names whose text grows with the zoom.

- **Uses:** `QCPAbstractItem`, `createPosition()`, `createAnchor(name, id)` +
  `anchorPixelPosition(int)`, `asPosition(&snapshot)` + `recordEveryFrame()`, `context.polygon()`,
  `context.text()`, `context.path(origin, pixelPath)`, `QCP::DataXRatioY`, `context.transform()`.
- **Verdict: awkward.**
  - Every item `draw()` must open with the same three lines: `asPosition(&snapshot)`, then
    `if (snapshot) recordEveryFrame()`. Forget them and a position silently stops following.
  - `ptAxisRectAbsolute` (which the items guide recommends), `ptViewportRatio`, anchored plot
    coordinates and pixel/data mixes are not modelled by `QCP::Position`. They fall back to a pixel
    snapshot, so the item re-records every frame.
  - There is no "data point plus a pixel offset" position. A label 8 px above a pin needs text
    alignment plus padding, or a `path()` at a data origin. A fixed-size circle around a data point
    needs `markers()` or a pixel path.
  - Anchors are a 2.x `switch` over an int id that returns pixels, recomputed from
    `pixelPosition()`. It is not tied to what `draw()` recorded.
  - Text that scales with the data has no command: read `transform()` and pick a font size. That
    re-records on every pan.
  - Text is painted with QPainter, under the same layer's GPU shapes. A pin label recorded after a
    GPU pin can end up under the next pin.
- **Lines:** ~120 for the callout, ~60 for the pins.
- **Python:** blocked. `QCP::Position` is a `std::variant`; `polygon` takes a `std::span`.
- **Joy 2:** the boilerplate is about the cache, not about the callout.
- **API idea:** a `QCP::Data{} + QCP::PixelOffset{}` position kind and an `AxisRectPixels` kind, so
  `asPosition()` never snapshots; `createAnchor(name, [](const Recorded&) -> QCP::Position)`.

## 3. Roman-numeral (or bars-and-beats) ticker

Ticks every 1, 5 or 10 units, labelled I, V, X... Or a music timeline in bars and beats.

- **Uses:** `QCPAxisTicker` subclass, `getTickStep()`, `getTickLabel()`, maybe `getSubTickCount()`,
  `axis->setTicker(QSharedPointer<...>)`.
- **Verdict: easy.** Two overrides. Two small catches: changing the ticker in place needs
  `plot->requestRepaint()` (a rule only stated in concepts.md), and `generate()` keeps the 2.x
  out-parameter shape (`QChar formatChar`, `QVector<double>&`).
- **Lines:** ~30.
- **Python:** friction. A Python subclass held by a `QSharedPointer` needs the wrapper kept alive
  by hand, or the axis calls into a collected object.
- **Joy 4:** idea to screen in minutes.
- **API idea:** a ticker from two functions: `QCPAxisTicker::from(stepFn, labelFn)`.

## 4. Skew-T log-P diagram (custom coordinate system)

Meteorology's standard sounding chart: temperature on x, log pressure on y, and x leaning right with
height. Plot the temperature and dew-point profiles as graphs, and draw the skewed isotherms.

- **Uses:** `QCPCustomMapping` (`map(span, span, span)`, `inverse()`), `QCPCoordinateSystem`
  (`transform()`, `clipRect()`, `contains()`, `capabilities()`, `parametersChanged()`),
  `QCPAxisRect::createSystem()`, a log ticker on the y axis.
- **Verdict: awkward.**
  - Three classes. The system must cache its mapping per revision in `mutable` members, or every
    frame looks new; the docs state this rule in prose only.
  - The mapping captures `rect().bottom()` by value, so it depends on the revision being bumped on
    resize (it is, by the area).
  - The grid and the axes stay cartesian. A skew-T without skewed isotherms is not a skew-T, so
    those are a second, hand-written plottable.
  - The default plottable capability (`capRectangular | capStraightKey`) refuses a custom system.
    A user's own plottable must know to pass `capStraightSegments`.
  - No culling or decimation on a custom system: fine for a sounding, slow for a long series.
- **Lines:** ~150 (mapping 30, system 40, rect 15, isotherm plottable 40, setup 25).
- **Python:** blocked: `std::span` arguments, a `shared_ptr` mapping, and `map()` called per frame.
- **Joy 2:** a lot of structure before a lean appears; the result is correct and it pans.
- **API idea:** `QCPAxisRect::setMapping(fn, inverseFn)` builds the system and caches it per
  revision; a `QCPGridSource` hook so the grid can follow the mapping.

## 5. Oscilloscope with persistence (animation, live update)

A 10 kHz signal swept at 60 Hz. The last 8 sweeps stay visible and fade out, like phosphor.

- **Uses:** a plottable holding a ring buffer, `QTimer`, a setter that calls `contentChanged()`,
  `context.polyline(points, colours, pen)` with per-vertex alpha.
- **Verdict: easy.** The setter schedules one repaint per event-loop turn, so a timer is all you
  need. Per-vertex colours do the fade in one command.
  - Every tick re-records the whole trace: fresh `shared_ptr` vectors, one per sweep. For 8 x 2k
    points this is fine; there is no partial invalidation for "only the newest sweep changed".
  - No frame clock: no `t`/`dt`, no hook at vsync, so the timer and the display drift apart.
- **Lines:** ~70.
- **Python:** friction. `draw()` is called on the GUI thread each tick, so each frame pays a GIL
  round trip and a list-to-vector copy.
- **Joy 3:** moving pixels quickly, but you build the loop yourself.
- **API idea:** `plot->onFrame([](double t, double dt) {...})` driven by the widget's frame swap,
  stopping when nothing animates.

## 6. Spline editor (mouse and keyboard on a custom plottable)

Control points you drag with the mouse; the curve through them updates live. Hover highlights a
point. Delete removes the selected point; Insert adds one under the cursor.

- **Uses:** the protected `QCPLayerable::mousePressEvent(event, details)`, `mouseMoveEvent(event,
  startPos)`, `mouseReleaseEvent`, `pixelsToCoords()`, `QCPAbstractPlottable::keyPress()`, the plot's
  `mouseMove` signal for hover.
- **Verdict: awkward.**
  - The mouse virtuals work (2.x dispatch: topmost hit layerable, `accept()` to grab), but no guide
    page mentions them for user classes. `interactions.md` only covers signals.
  - Events arrive in pixels; every handler converts back to data by hand.
  - Hover is not routed to layerables; you connect to `QCustomPlot::mouseMove` and hit-test
    yourself.
  - `keyPress()` is offered to *every* plottable, selected or not, and items never get keys. The
    plot only takes focus on a click.
  - Accepting the press stops the axis pan for that drag, which is right, but undocumented.
- **Lines:** ~140.
- **Python:** blocked: protected virtuals with `QVariant` details and `QMouseEvent*` lifetimes.
- **Joy 2:** works once you find the 2.x virtuals in the source.
- **API idea:** documented, public `onPress/onDrag/onRelease/onHover/onKey` on layerables, with
  positions given as `QCP::Data`, and keys sent to the selected or focused plottable first.

## 7. Game of Life, 1024 x 1024 (large custom drawing)

A million cells, a new generation every 30 ms, click a cell to toggle it, pan and zoom freely.

- **Uses:** `context.image(QCP::Data, QCP::Data, QImage, {}, false)`; or one `rect()` per live
  cell; `mousePressEvent` or the plot's `mousePress` + `pixelToCoord`.
- **Verdict: easy.**
  - `image()` with two data corners is one GPU quad. Pan and zoom reuse the recording; a new
    generation re-records one command and uploads one texture.
  - The rect route also works: neighbouring data rects with solid brushes merge into one batch,
    even with different colours. That rule is internal (draw-context.cpp); the guide only says
    "neighbouring solid commands merge".
  - Mixing a text or a dashed pen into the same recording drops the layer off the fast path, and
    nothing warns.
- **Lines:** ~90.
- **Python:** friction: a `QImage` per generation crosses the binding (fine with numpy-backed
  images).
- **Joy 4:** one call to see a million cells, and it pans like a graph.
- **API idea:** say which commands are GPU and which batch in the API doc of each helper; a debug
  overlay that tints QPainter-replayed commands.

## 8. Snake (game 1)

A 30 x 20 grid in data coordinates. The body is a list of cells, food is a disc. Arrow keys steer,
a timer moves the snake, score text in a corner, a "game over" panel with a restart hint.

- **Uses:** a plottable for the board (batched `rect()` per cell, `ellipse()` for food), `keyPress()`
  override or an event filter, `QTimer`, `contentChanged()`, `context.text(QCP::AxisRatio, ...)` or a
  `QCPItemText`, `plot->overlay()->showMessage(..., FullWidget)`.
- **Verdict: awkward.**
  - Keys reach `keyPress()` only once the plot has focus, which it takes on a click. A game must
    call `setFocus()` itself and turn off pan and zoom so arrow-key play does not fight the mouse.
  - Text is QPainter, the board is GPU. A score or "GAME OVER" recorded in the board's `draw()` is
    painted *under* the board's rects, whatever the order of the calls. You must move the text to
    an item on a layer above, or use the overlay.
  - `QCPOverlay` gives a game-over panel for free, but only in the three status colours.
  - The rest is pleasant: data cells, auto-batched rects, `rescaleAxes()` from the recording.
- **Lines:** ~170.
- **Python:** blocked (as 1 and 6).
- **Joy 3:** a snake moves within an hour; then the score vanishes under the board.
- **API idea:** record order is draw order within one recording (split the GPU batch around
  QPainter commands), or a `context.overlayText()` that always lands on top.

## 9. Brick breaker (game 2)

A paddle follows the mouse along x, a ball bounces, 60 bricks disappear when hit, lives and level in
a corner, space launches the ball.

- **Uses:** two plottables (static bricks, moving ball + paddle) on two layers, `QTimer`, the
  plot's `mouseMove` signal + `pixelToCoord`, `keyPress()`, `context.rect()`, `context.ellipse()`.
- **Verdict: awkward.**
  - The hit-test API is pixel-in, distance-out (`hitTest(QCP::Pixels)`). Collision is data-space
    geometry, so the user writes all of it; the library's hit tests do not help.
  - There is no partial invalidation. One brick gone re-records all bricks; the ball moving
    re-records the bricks too if they share a plottable. So you split into two plottables and keep
    them on separate layers by hand to keep the brick layer cached.
  - `ellipse(a, b)` needs two corners: `circle(centre, r)` is missing, and with unequal axis scales
    the ball is an oval unless you lock the aspect.
  - Mouse capture for the paddle fights `iRangeDrag`: turn interactions off.
- **Lines:** ~200.
- **Python:** blocked (as 1 and 6).
- **Joy 2:** the game is mostly your own physics, with the plot as a canvas.
- **API idea:** `circle(centre, radius, inPixels)`, a data-space `QCPDataBox` hit query
  (`itemsIn(box)`), and keyed sub-recordings (`context.group("bricks")`) re-recorded independently.

## 10. Python lollipop (Python subclass)

Challenge 1 written in Python: `class Lollipop(QCPAbstractPlottable)` with `def draw(self, ctx)`.

- **Uses:** a Shiboken6 binding of `QCPAbstractPlottable`, `QCPDrawContext`, `QCP::Position`,
  `QCPTextStyle`, `QCPScatterStyle`.
- **Verdict: blocked.** NeoQCP has no Python binding, and the recording API would resist one:
  - `QCP::Position` is a `std::variant`; `polyline`/`polygon` take `std::span`;
  - point data is `std::shared_ptr<const std::vector<QPointF>>`;
  - `hitTest` and `drawnDataRange` return `std::optional`;
  - `painter()` takes a `std::function`;
  - the 1D base is a template;
  - getters return `QCPHandle<T>`;
  - the plot owns the object, so every constructor needs `<parent>` markup, and a Python-side
    `draw()` that mutates `self` hits the "setter inside draw()" misuse only at runtime.
- **Lines:** ~60 if it worked.
- **Joy 1:** cannot start.
- **API idea:** keep one Python-shaped overload per helper: `polyline(numpy (N,2) array, pen)`,
  positions as small value classes (`QCPData(x, y)`, `QCPPixels(x, y)`), `hitTest` returning `-1`
  for a miss. Then a lambda drawable (pattern 1) makes subclassing optional.

## Patterns

Recurring weaknesses of the extension API, ranked by how many challenges they hurt and how much.

1. **Ceremony before the first pixel (joy).** Hurts 1, 2, 4, 5, 6, 8, 9, 10. A "hello circle"
   needs a subclass, a `(QCPAxis*, QCPAxis*)` constructor, a `const draw(QCPDrawContext&)`
   override, an include from `painting/`, and `contentChanged()` in every setter. p5.js needs 3
   lines, ImGui 1. Borrow: a closure drawable,
   `plot->add<QCPSketch>([&](QCPDrawContext& c) { c.circle(...); })`, re-recorded when you call
   `sketch.update()`. It also removes most of the Python problem.
2. **Invalidation rules are side effects, not types.** Hurts 1, 2, 4, 7, 8. Reading `transform()`,
   `clip()` or the theme silently changes when you re-record. `(void)context.transform();` is a
   real idiom in `QCPBars` and `QCPItemLine`. Every item opens with the `asPosition(&snapshot)` /
   `recordEveryFrame()` dance, and forgetting it fails silently. Borrow: say it out loud, e.g.
   `context.pixelGeometry()` returning a scoped object, and positions that never need a snapshot.
3. **Two renderers, one recording: z-order is not call order.** Hurts 2, 7, 8, 9. Text, paths,
   dashed pens and `painter()` replay on QPainter under the layer's GPU content. A label or a
   game-over text recorded after shapes is drawn under them. Canvas 2D, p5 and ImGui all promise
   that order of calls is order on screen; users will assume it. Fix the order, or warn in debug.
4. **No per-datum identity in a recording.** Hurts 1, 6, 9, 10. Commands do not know which data
   point made them. So hit test, rubber-band selection, the selected look and hover all need
   hand-written overrides that recompute the geometry. The selection decorator still draws through
   `QCPPainter`. Borrow: `context.index(i)` (ImGui's ID stack) tags commands, and the defaults go
   per datum.
5. **Verbose call shapes (joy).** Hurts every challenge. `std::make_shared<const
   std::vector<QPointF>>(...)` for every series; a `QCPTextStyle` struct for one label; `ellipse`
   from two corners and no `circle(centre, r)`; pen and brush on every call with no style state.
   Borrow: initializer-list and `std::span` overloads that copy, `circle`/`line`/`point`/`label`
   helpers, and a current style (`c.stroke(pen).fill(brush)`) defaulting to the theme.
6. **Coordinate gaps for common mixes.** Hurts 2, 4, 9. No "data point + pixel offset" position, no
   axis-rect pixels, no data-scaled text; anchors are an int `switch` in pixels. The 2.x position
   types not modelled fall back to per-frame re-recording.
7. **Input is 2.x and undocumented.** Hurts 6, 8, 9. Protected mouse virtuals in pixels, no hover,
   keys broadcast to every plottable and never to items, focus only on click.
8. **No frame clock, no partial invalidation.** Hurts 5, 8, 9. Animation is a `QTimer` and a
   setter; any change re-records the whole object. Borrow p5's `draw()` loop as an opt-in
   `onFrame(t, dt)`, and sub-recordings re-recorded on their own.
9. **Python is blocked by the types.** Hurts 3-10 for Python users. `variant`, `span`,
   `shared_ptr<const vector>`, `optional`, `std::function`, templates and handles all need custom
   conversions.
10. **Docs drift.** The plot-type guide's first paragraph names `QCPDrawn<Base>`, which does not
    exist. Capability defaults (`capRectangular | capStraightKey`), the batching rule and the
    mouse virtuals are only in the source.

What is already good, and worth keeping: data-space recordings that pan for free, automatic rect
batching (a million cells in one GPU batch), `image()` as a GPU quad, per-vertex colours on
polylines, the misuse report for a setter inside `draw()`, and the custom-system contract with its
capability check.
