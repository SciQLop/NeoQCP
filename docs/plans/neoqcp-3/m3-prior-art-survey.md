# Prior-art survey for NeoQCP 3.0 draw API
# NeoQCP 3.0 drawing/extension API: prior-art survey

Date: 2026-11-04. Scope: literature and prior art only, no repository touched.

Legend: [V] verified against the cited page in this session. [K] from my own knowledge, not re-verified here. [?] could not verify.
Qt claims were checked against the Qt 6.12.0 documentation (installed doc set via the qt-docs tool, and doc.qt.io).

---------------------------------------------------------------------------------------------------

## 1. Qt itself

### 1.1 QPicture (recording and replay of QPainter commands)

- What it is: "a paint device that records and replays QPainter commands". [V] https://doc.qt.io/qt-6/qpicture.html
- API: `QPainter p(&picture); ...draw...; p.end();` then `picture.play(&painter)` or `painter.drawPicture(pos, picture)`. Also `boundingRect()`, `setBoundingRect()`, `save()/load()`, `data()`. [V]
- It records everything a QPainter can paint (text, images, transformed content), serialised in a platform-independent binary format via QDataStream. [V]
- `play()` "behaves like drawPicture at (0,0)" and the painter state is not preserved across the call. [V]
- Resolution independent: it replays at the default system DPI scaled to the target device, so one recording can go to screen, PDF and SVG. [V]
- Limits for us:
  - It records QPainter calls only. It cannot hold a QRhi draw. A GPU helper cannot be recorded into it. So it can be the CPU half of a draw list but not the whole list.
  - It is opaque: no way to iterate commands, no per-command bounds, so no hit testing from it. Only the whole `boundingRect()`. [K: no iteration API; doc shows none]
  - Text is stored as text plus font, so replay cost is the same as live drawing. Large polylines are stored point by point, it does not simplify. [K]
  - The binary format is "proprietary" and QDataStream based: not a stable interchange. [V]
- Module/licence: QtGui, LGPLv3/GPLv3/commercial like the rest of QtGui. [K]
- Usable from a QRhiWidget library: yes, it is plain QtGui. Replay goes through a QPainter, so in NeoQCP it would replay into the layer paint buffer (QImage/QPainter path) and into PDF/SVG/PNG export unchanged.
- Verdict: good model for the CPU half ("record QPainter commands, replay in the pass that owns the layer"). Not enough as the recording type, because the recording must also hold GPU commands.

### 1.2 QPainterPath

- A retained vector path: `moveTo/lineTo/cubicTo/addRect/addPolygon/...`. Value type, implicitly shared. [K]
- Useful members: `contains(QPointF)` (point-in-path with the fill rule), `intersects(QRectF|QPainterPath)`, `boundingRect()` exact, `controlPointRect()` "significantly faster to compute than the exact boundingRect()" and always a superset, `translated()`, `toFillPolygons(QTransform)`. [V] https://doc.qt.io/qt-6/qpainterpath.html
- `QPainterPathStroker` "generate fillable outlines for a given painter path": turns a pen stroke into a fill-able path, so stroke hit testing can be done as `contains` on the stroked path. [V]
- Relevance: the natural recorded geometry for shapes, and the natural input for hit testing. It is also what QCanvasPainter accepts (`addPath(const QPainterPath&)`). [V]
- Limit: it stores flattened-on-demand curves, it has no per-vertex attributes, no colour, no pen. It is geometry only. Dense polylines (millions of points) are better kept as arrays.

### 1.3 QGraphicsItem and the Graphics View framework

Closest retained-mode 2D API in Qt Widgets. https://doc.qt.io/qt-6/qgraphicsitem.html [V unless noted]

- Item contract: `virtual QRectF boundingRect() const` (pure), `virtual void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*)` (pure), `virtual QPainterPath shape() const`, `virtual bool contains(const QPointF&) const`, `virtual bool collidesWithItem/Path(...)`. [boundingRect/paint pure: K]
- Hit-testing rule: `shape()` "is used for many things, including collision detection, hit tests, and for the QGraphicsScene::items() functions". Default `shape()` "calls boundingRect() to return a simple rectangular shape". `contains()` calls `shape()` by default. Pen outline: "If you want to include this outline in the item's shape, you can create a shape from the stroke using QPainterPathStroker." So the framework's one rule is "shape path, point inside or not". There is no distance, no tolerance. Pick tolerance is the caller's job (QGraphicsView rubber band / `items(QRect)`).
- Z-order rules: parents are painted before children, "in ascending stacking order". Lower `zValue()` first, default 0, equal z "stacked by insertion order". Children are on top of the parent by default, `ItemStacksBehindParent` flips that. `stackBefore()` changes insertion order. Z is per sibling set: a parent's z governs all its descendants. Picking uses the same order reversed: `QGraphicsScene::items(pos, mode, Qt::DescendingOrder)` returns the topmost first (the default order is descending). [order rule V; DescendingOrder default K, the fetched page chunk did not reach it]
  - Key point for us: one ordering for painting and picking, independent of pen style. This is exactly the "layer decides, never pen style" ask.
- Caching: `cacheMode()` = `NoCache` (default), `ItemCoordinateCache` (offscreen buffer in local coords, quality degrades with scale), `DeviceCoordinateCache` (device coords, for items that move but are not rotated or scaled). Invalidation is explicit: `update()` marks dirty, `prepareGeometryChange()` must be called before the bounding rect changes. [V]
- `ItemIgnoresTransformations`: item anchored at a scene position but unscaled and unrotated by the view. This is the "anchor + pixel offset" case of the position model. [V]
- Usable from a QRhiWidget library: no for the framework itself (QGraphicsView is a QAbstractScrollArea painting with QPainter on a viewport, which can be a QOpenGLWidget but not a QRhiWidget). Yes as a design to imitate: boundingRect/shape/paint/update/prepareGeometryChange.
- Limits: `paint()` is immediate. The framework retains the items, not the drawing. Performance with 100k items is poor [K].

### 1.4 Qt Quick scene graph

Fully retained, GPU-first, and the closest model to "record, then replay in the right pass".

- `QQuickItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData*)`: called on the render thread while the GUI thread is blocked (sync point), only if the item sets `ItemHasContents`; the item calls `update()` to schedule it; the returned node tree is owned and deleted by the scene graph ("automatically deleted on the right thread at the right time"). [V partly: signature, ItemHasContents, render-thread warning, ownership. "GUI blocked" is in the sync docs, K]. https://doc.qt.io/qt-6/qquickitem.html
- Node types: `QSGGeometryNode` (geometry + material), `QSGTransformNode`, `QSGClipNode`, `QSGOpacityNode`, `QSGRenderNode`. [K] Geometry is vertex data, materials carry shaders. The renderer sorts and batches the tree.
- Invalidation is node based: `QSGNode::markDirty(DirtyState)` with flags such as DirtyGeometry, DirtyMaterial, DirtyMatrix; unchanged nodes are not re-uploaded. [K]
- `QSGRenderNode` https://doc.qt.io/qt-6/qsgrendernode.html [V]: "a set of custom rendering commands targeting the graphics API that is in use by the scenegraph". Flags: `BoundedRectRendering` (renders nothing outside `rect()`), `DepthAwareRendering`, `OpaqueRendering`, `NoExternalRendering` ("exclusively use the QRhi family of APIs"). `prepare()` (uploads, outside the pass) and `render(const RenderState*)` (inside the pass). This two-phase split (prepare vs render) is the same split NeoQCP has between "resource update" and "in-pass draw".
  - It is the integration point for Canvas Painter inside Qt Quick (see 1.6), and the interleaving rule: a render node's z is its place in the tree, and Qt Quick keeps "painter's order" between ordinary nodes and render nodes.
- Usable from a QRhiWidget library: no. It needs a QQuickWindow. The model to imitate: "item holds node tree, dirty flags tell the renderer what to re-upload".
- Licence: Qt Quick is LGPLv3/GPLv3/commercial. [K]

### 1.5 QQuickPaintedItem

https://doc.qt.io/qt-6/qquickpainteditem.html [V]

- "makes it possible to use the QPainter API with the QML Scene Graph". `paint(QPainter*)` is called on the render thread with the GUI thread blocked.
- Render targets: `Image` (default, raster QPainter into a QImage then upload), `FramebufferObject` (OpenGL only, since 6.9), `InvertedYFramebufferObject`.
- The painted result is cached as a texture and only redone on `update()`. So it is the "retain the pixels" answer, not "retain the commands". Resize and zoom re-run `paint()`.
- Same pattern as NeoQCP's per-layer paint buffers. Confirms the pattern is idiomatic, and that retained pixels are invalidated by a plain dirty flag.

### 1.6 Qt Canvas Painter (QCanvasPainter), the important one

Status and licence [V]:
- Module `Qt Canvas Painter`, `find_package(Qt6 COMPONENTS CanvasPainter)`, `Since: Qt 6.11`.
- 6.11: Technology Preview ("excluding their API from Qt's compatibility promises"), https://doc.qt.io/qt-6/whatsnew611.html.
- 6.12: "The Qt Canvas Painter module is moving out of Technology Preview state, and is now fully maintained (supported)." https://doc.qt.io/qt-6/whatsnew612.html. (The QML `QtCanvas2D` API stays in preview.)
- Licence: "Commercial or GPLv3" (header of the module page). Not LGPL. Third-party licence note: a FreeBSD port lists LGPL21, Debian's review says GPL-3 with Qt exception; the Qt docs are the authority. [V for docs; the others from a search result, treat as unverified]. https://doc.qt.io/qt-6/qtcanvaspainter-module.html
- Qt Graphs 6.12 can use it as a 2D backend (`-feature-graphs-2d-high-performance-backend`). [V, whatsnew612]. Precedent: Qt itself uses it for plotting.

What it is [V], https://doc.qt.io/qt-6/qcanvaspainter.html:
- "performs hardware-accelerated painting on QRhi". The API "closely follows the HTML Canvas 2D Context specification". Path based: `beginPath()`, `moveTo/lineTo/arc/roundRect/circle`, `fill()`, `stroke()`, `fillText()`, `drawImage`, `drawBoxShadow`, `setClipRect()`, `resetClipping()`, `setFillStyle(brush)`, `setFillRule` (6.12), gradients, image patterns, custom shader brushes (`QCanvasCustomBrush`), SDF text, `addPath(const QPainterPath&)`, `addPath(const QCanvasPath&, QTransform)`.
- `QCanvasPath`: "the native path format of QCanvasPainter". A retained, reusable path, with `addPath(path, start, count, transform)` to draw sub-ranges, and `fill(path, fillRule, pathGroup)`. This is the retained part. [V signatures; semantics of `pathGroup` not verified [?]]
- Immediate or recording? The drawing API is immediate-style (state machine) but commands are buffered inside the painter and only turned into QRhi work at `endPaint()` (or `renderPaint()`). There is no public way to keep, inspect or re-play the buffered command list across frames. So: immediate API, internally batched, not a reusable recording. You re-issue the calls each frame (or keep a `QCanvasPath`).

Can it target the QRhi pass inside an existing QRhiWidget? Yes [V], `QCanvasRhiPaintDriver` https://doc.qt.io/qt-6/qcanvasrhipaintdriver.html:
- `QCanvasPainterWidget` itself derives from QRhiWidget (hellowidget example). [V]
- Lower level: `QCanvasPainterFactory::create(QRhi*)` gives the `QCanvasPainter*` and `paintDriver()`.
  - `beginPaint(QRhiCommandBuffer*, QRhiRenderTarget*, QColor, QSize, qreal dpr, flags)`;
  - or `beginPaint(cb, rt, const QMatrix4x4 &matrix)` which "A common use case is to pass in Qt Quick's modelview-projection matrix when implementing QCanvasPainter-based rendering within a QSGRenderNode subclass";
  - `endPaint(EndPaintFlag::DoNotRecordRenderPass)` then, inside your own `cb->beginPass(...)`, `renderPaint()`: "Records all QRhi rendering ... The associated QRhi must be recording a render pass when this function is invoked." So Canvas Painter can draw into a pass that NeoQCP's own GPU layers also use. [V]
  - `resetForNewFrame()` once per frame after `QRhi::beginFrame()`.
  - `beginPaint` "must not be in render pass recording state when called", so the sequence is: beginPaint (outside pass), issue commands, endPaint(DoNotRecordRenderPass), then your pass: your own draws and `renderPaint()` where you want it in order.
- Constraints [V]: the render target needs a colour and a depth-stencil attachment (stencil used, depth not written or tested unless `BeginPaintFlag::DepthTest`). "Nesting is not currently supported." With a custom matrix, "Support for clipping is limited", only axis-aligned rect clips "when the matrix specifies an orthographic projection without any added scaling or rotation".
- Offscreen: `QCanvasOffscreenCanvas` renders to a texture (with `PreserveContents`, not with MSAA), `grabCanvas()` reads it back to QImage. That is a way to cache a recorded drawing as a texture.
- Limits for NeoQCP:
  - GPL/commercial only. NeoQCP is a QCustomPlot fork, QCustomPlot is GPLv3 [K], so a hard dependency is licence-compatible for GPL builds, but it forces GPL or Qt commercial on any LGPL-style downstream use. Needs a maintainer decision. Also SciQLopPlots' own licence must be checked (not looked at, repository off limits).
  - It cannot draw to a QPainter, so PDF/SVG/PNG export through QPainter needs a second path anyway. (QCanvasPainter can render offscreen and grab, giving a raster only, not vector PDF/SVG.)
  - Not available on every platform NeoQCP targets? WebAssembly: QRhi works on wasm; Canvas Painter on wasm not verified [?].
  - Text via SDF with a limited antialias range ("maximum antialiasing amount is quite limited") [V], not identical to QPainter text, so on-screen vs export text would differ.
  - Extra stencil requirement, extra render-pass assumptions, extra module to ship in the wheels.
- What to take from it: (a) retained path object (`QCanvasPath`) that can be drawn with a transform, partially, many times; (b) the HTML-canvas-like state-machine API is what users of "matplotlib-like" code know less than a typed list API; (c) its pass integration shows the right split: build commands outside the pass, flush inside it.
- Verdict: optional backend for helpers (polyline stroke with joins, shadows, gradients), not the base of the user-facing recording. Never the only path, because export needs QPainter.

### 1.7 Qt Quick Shapes

- `QtQuick.Shapes`: `Shape` item with `ShapePath` children, `PathLine/PathCubic/...`, renderer types `GeometryRenderer` (triangulates fills, stroker in the CPU), `CurveRenderer` (GPU curve evaluation, newer), `SoftwareRenderer`. Stroke geometry is generated on the CPU and cached until the path changes. [K, my two doc fetches 404ed, [?] for exact names and versions]
- Relevance: another confirmation that fill/stroke geometry is generated once per path change and cached as GPU vertex data.
- Not usable from QRhiWidget (needs Qt Quick). Its stroker is internal (`qtriangulator`, `qstroker` in QtGui private, [K]).

### 1.8 QtGraphs and QtCharts internals

- Qt Charts: built on Graphics View ("uses the Qt Graphics View Framework"), licensed commercial or GPLv3, and "deprecated since Qt 6.10", Qt Graphs recommended. [V] https://doc.qt.io/qt-6/qtcharts-index.html. The docs expose no public hook for user-defined series types [V: page shows none]. Internally each series has a private `*Chart` item (a QGraphicsItem) doing `paint()`. Extension story: none.
- Qt Graphs 2D: Qt Quick based, uses QSG geometry nodes for lines and a scene graph item per series; 6.12 adds the Canvas Painter backend and `QLogValueAxis`. [V whatsnew612] No public series extension API [K, [?]].

### 1.9 What this suggests for NeoQCP (Qt section)

- Nothing in Qt records a mixed GPU + CPU draw list. QPicture records the CPU half, QSG records the GPU half, QCanvasPainter batches GPU only. A NeoQCP draw list is a new thing, but it can be small: a vector of typed commands, each tagged with the pass it needs.
- Copy QGraphicsItem for the vocabulary: `boundingRect`, `shape`, `zValue`, `update()`, and "one order for paint and pick".
- Copy QSGRenderNode for the two phases: `prepare` (upload) before the pass and `render` inside it, plus its flags (`BoundedRectRendering`, `OpaqueRendering`) as optional hints on a recorded object.
- Use QPainterPath (+ `QPainterPathStroker`) as the shape type for the shape/fill commands and hit testing. Use `QPicture` only if a "raw QPainter escape hatch" is wanted: a `drawPainter(lambda)` command that is recorded into a QPicture so it replays in any QPainter pass and in export.
- Treat QCanvasPainter as an optional GPU helper backend behind the context helpers (needs a licence decision), not as the core.

---------------------------------------------------------------------------------------------------

## 2. Other Qt plotting libraries

### 2.1 Qwt

- `QwtPlotItem::draw`: `virtual void draw(QPainter*, const QwtScaleMap &xMap, const QwtScaleMap &yMap, const QRectF &canvasRect) const = 0`. The maps convert x and y values to pixels, `canvasRect` is the canvas contents rectangle. [V] https://qwt.sourceforge.io/class_qwt_plot_item.html
- `QwtScaleMap`: a 1D transform (`transform(double)`, `invTransform(double)`, `s1/s2/p1/p2`, `setTransformation(QwtTransform*)` for log, power, null). One map per axis, passed explicitly. Typed per axis, but x and y maps are both plain `QwtScaleMap`, so nothing stops mixing them up. [K]
- Item contract [V]:
  - `rtti()` type code, user items use `Rtti_PlotUserItem` (1000) upward. 
  - `z()/setZ()`: "Plot items are painted in increasing z-order." Ties: insertion order [K].
  - `boundingRect()`: default `QRectF(1.0, 1.0, -2.0, -2.0)`, i.e. an invalid rect, "The autoscaler ignores any item whose width or height is below zero". So autoscale data range is a virtual with a sentinel for "none".
  - `legendData()` returns `QList<QwtLegendData>`, default one entry with title and icon, items can return several (bar charts). `legendIcon(index, size)` returns a `QwtGraphic`, default invalid. `QwtGraphic` is Qwt's own recording type (a QPainter command recorder, like QPicture, but with scalable control of pen widths) [K].
  - `ItemAttribute` flags: `Legend`, `AutoScale`, `Margins` (needs space outside its bounding rect), plus `ItemInterest`: `ScaleInterest` gates `updateScaleDiv()` and `LegendInterest` gates `updateLegend()`.
  - `itemChanged()` updates the legend and asks the plot to refresh if auto-replot is on.
  - `setRenderHint(RenderAntialiased)`.
  - `setAxes(x, y)`: every item names its axis pair. [V]
- `QwtPlotSeriesItem`: `draw()` calls `drawSeries(painter, xMap, yMap, canvasRect, from, to)` (pure virtual), with `QwtSeriesData<T>` as the data and `dataChanged()` / `rectOfInterest` for partial draws. Eight series types implement it (curve, bar chart, histogram, interval curve, trading curve, spectrocurve, ...). [V partly]
- Hit testing: Qwt has no item-level hit test. Picking is done by `QwtPlotPicker`/`QwtPlotPicker`-derived classes and `QwtPlotCurve::closestPoint(pos, &dist)` (curve only) [K]. Each application writes its own.
- Lessons: the `(painter, xMap, yMap, canvasRect)` quadruple is the cleanest minimal context in Qt-land. Its weakness is that it is immediate, QPainter-only, and the item must do its own culling. The legend icon as a virtual returning a recorded graphic is good. The "default boundingRect is invalid = does not take part in autoscale" is a cheap sentinel.

### 2.2 QCustomPlot 2.x (our ancestor)

- `QCPAbstractItem` [V] https://www.qcustomplot.com/documentation/classQCPAbstractItem.html: subclass, implement `selectTest(pos, onlySelectable, details)` and `draw(QCPPainter*)`. Positions via `createPosition(name)`, anchors via `createAnchor(name, id)` + `anchorPixelPosition(id)`.
  - `selectTest` "returns the shortest pixel distance from the click point to the item", -1.0 for invisible/unknown/not-selectable. "Area-type items return a constant value above zero but below the selection tolerance (typically 0.99 × tolerance). This lets nearby line objects still be selected when an area overlaps them." This is the maintainer's "one rule" already, with the filled-shape wrinkle. [V]
  - `draw()`: clip rect is set to `clipRect()` before the call. "compute a bounding rect (including pen width), check it against the clip rect, and skip drawing" is left to the user. Default clip: the main axis rect; `setClipToAxisRect(false)`, `setClipAxisRect(rect)`. [V]
- `QCPItemPosition::PositionType` [V] https://www.qcustomplot.com/documentation/classQCPItemPosition.html: `ptAbsolute` (pixels from the viewport top-left), `ptViewportRatio`, `ptAxisRectRatio` (can exceed 0..1), `ptPlotCoords` (two axes). `setTypeX()/setTypeY()` allow a mixed type, "an item can be fixed in pixels on one axis while following plot coordinates on the other". `setParentAnchor` turns coordinates into pixel offsets from an anchor ("anchor + offset"). `pixelPosition()` and `setPixelPosition()` convert. So QCP already has runtime-tagged coordinate types, per axis, with anchors. What the 3.0 design adds is making the tag a compile-time type and the conversion a public object.
- `QCPAbstractPlottable` [V] https://www.qcustomplot.com/documentation/classQCPAbstractPlottable.html: five pure virtuals: `selectTest`, `draw`, `drawLegendIcon(QCPPainter*, const QRectF&)` (clip already set to the rect), `getKeyRange(bool &foundRange, SignDomain)`, `getValueRange(bool &foundRange, SignDomain, QCPRange inKeyRange)`. `foundRange=false` is the "no data" sentinel, the sign domain argument serves log axes. `selectTest` for area plottables as above. `clipRect()` defaults to the viewport and can be overridden. `QCPLayerable` supplies `layer()`, `setLayer`, `moveToLayer`. [V]
- Layers: `QCPLayer` with `QCPLayer::LayerMode` `lmLogical` or `lmBuffered`; order = layer index, inside a layer the order of `children()` (insertion, with `QCPLayer::addChild(layerable, prepend)`); buffered layers get an own paint buffer. Default layers: background, grid, main, axes, legend, overlay. Plottables and items default to `main` (items to `main` too, ...). [K, partly: the doc fetch of the layer page did not define them]
- Lesson: the existing five-virtual plottable contract is the baseline the new base class must beat. The new contract merges `draw+drawLegendIcon+selectTest+getKeyRange/getValueRange` into one `draw(context)` plus defaults derived from the recording.

### 2.3 JKQTPlotter

- Graph classes derive from `JKQTPPlotElement` (`JKQTPGraph`, `JKQTPXYGraph`, ...). Users implement `draw(JKQTPEnhancedPainter&)`, `getXMinMax/getYMinMax`, `drawKeyMarker(painter, rect)`, `getKeyLabelColor()`; the plotter provides the transforms `transformX/transformY` (and inverse) on the parent `JKQTBasePlotter`; plot order via `zOrder`-like "plot order" and drawing in a fixed order. Data comes from a shared `JKQTPDatastore` of columns. [K, [?]: my two documentation fetches returned 404, so none of this is verified from the page. Repository: https://github.com/jkriege2/JKQtPlotter]
- Lessons (K): shared column store with column ids rather than per-graph copies (cheap data-changed detection), `JKQTPEnhancedPainter` as a QPainter subclass (same trick as QCPPainter) carrying extra pen/antialias helpers.

### 2.4 KD Chart

- `KDChart::AbstractDiagram` (`paint(PaintContext*)` implemented per diagram type, `PaintContext` carries painter, rectangle and coordinate plane), `AbstractCoordinatePlane::translate(QPointF diagramPoint)` as the data to pixel map, `AbstractDiagram::dataBoundaries()` for the range, `AbstractDiagram::paint` is immediate, QPainter only; data via Qt model/view (`QAbstractItemModel`) so "data changed" is `dataChanged` from the model. Commercial/GPL. [K, [?] not re-fetched]
- Lessons: a `PaintContext` object carrying the painter and plane is the same idea as the proposed `draw(context)`; the model/view data binding means invalidation arrives through `QAbstractItemModel` signals.

### 2.5 QtCharts

- Extension story: none public. Series types are closed. Deprecated since 6.10. [V] (see 1.8).

### 2.6 What this suggests for NeoQCP (Qt libraries section)

- Everything in this family passes `(painter, maps, clip)` and draws immediately. None records. NeoQCP's change from `draw(QCPPainter*)` to `draw(context)` is the same shape of contract, so porting existing plottables is mechanical.
- Keep from QCP: pixel-distance `selectTest` with the 0.99*tolerance rule for areas, `getKeyRange/getValueRange` sign domains for log axes, per-axis position types (`setTypeX/Y`), anchors. Keep from Qwt: invalid-rect-means-no-autoscale, `legendIcon` returning a recorded graphic, ItemAttribute flags for Legend/AutoScale/Margins, an explicit `rtti`-like kind id (here it drives the default layer).
- Qwt's `QwtGraphic` is prior art for "record a legend icon once, replay at any size", which fits the draw-list idea directly.

---------------------------------------------------------------------------------------------------

## 3. Non-Qt plotting and rendering systems

### 3.1 matplotlib (the maintainer's reference)

Artist model:
- Everything drawn is an `Artist` with `draw(self, renderer)`. The renderer is a small backend interface (`RendererBase`: `draw_path`, `draw_markers`, `draw_path_collection`, `draw_image`, `draw_text`, `draw_gouraud_triangles`, ...) implemented by Agg (raster), SVG, PDF, PS, Cairo. [K] Artists never talk to a canvas directly. This is the analogue of the "helpers choose GPU or QPainter" ask: the artist calls primitives, the backend decides how.
- Mixed vector and raster in one drawing: `Artist.set_rasterized(True)` makes a PDF/SVG backend render that artist into a bitmap and embed it (`MixedModeRenderer`). [K] A direct precedent for "one list, two drawing modes, chosen per primitive".
- Invalidation, "stale": every artist has `.stale` (property) and a `stale_callback` that propagates up to the Figure, which triggers `draw_idle`. [K] Drawing itself is immediate (`draw()` re-runs the Python each frame), no recording. Blitting (`Axes.draw_artist`, `set_animated`) is the manual retained-pixels optimisation.
- Clipping: `set_clip_box(bbox)`, `set_clip_path(path, transform)`, `set_clip_on`; Axes sets clip to the axes patch for artists added with `add_artist/add_line/...` (`clip_on=True` default). [V names from index, K semantics] https://matplotlib.org/stable/api/artist_api.html

Transform tree (the part to study) [V] https://matplotlib.org/stable/users/explain/artists/transforms_tutorial.html:
- Coordinate systems and their transform objects: data `ax.transData`, axes `ax.transAxes` ((0,0) bottom-left, (1,1) top-right), subfigure `subfig.transSubfigure`, figure `fig.transFigure`, figure-inches `fig.dpi_scale_trans`, "xaxis, yaxis: Blended coordinate systems, using data coordinates on one direction and axes coordinates on the other" (`ax.get_xaxis_transform()`), display (`IdentityTransform`).
- `blended_transform_factory(x_transform, y_transform)`, example verbatim: `trans = transforms.blended_transform_factory(ax.transData, ax.transAxes)`; "x coords data, y coords axes". Caveat from the same page: this only works for separable transforms (Cartesian), not for `PolarTransform`. For NeoQCP: per-axis mixing is fine with 1D axis transforms, would break if a non-separable projection plot (curved) were added.
- Offset transforms: `ScaledTranslation(xt, yt, scale_trans)`, and "anchor + offset in physical units" as `ax.transData + ScaledTranslation(dx, dy, fig.dpi_scale_trans)`; `offset_copy(trans, fig, x, y, units='inches')`. "The order matters." The 1/72 inch point means the offset is the same at any output DPI, which is how pen widths and marker offsets must work for export at 1x/2x.
- Transforms compose with `+` (a then b), invert with `.inverted()` (`ax.transData.inverted()` for mouse events). Log, symlog, reversed and date axes all live inside `transData` (non-affine part first, then affine). [K] So a user never writes log handling.
- Each artist carries its own `transform` (default `transData` for Line2D/Patch added to an Axes). The artist is typed by which transform it was given, not by the point type. `Text(x, y, transform=ax.transAxes)`.
- Type safety: none. A point is a bare tuple. Mistakes (axes coords with data transform) are silent. This is the weakness the typed-position design fixes.

Hit testing [K, matplotlib API; signature quoted by the fetch tool as general knowledge]:
- `Artist.contains(mouseevent) -> (bool, dict)`, `Artist.pickable()`, `Artist.pick(mouseevent)`; `picker` attribute (None, bool, float radius in points, or callable). `Line2D.contains` uses `pickradius` (default 5 pt) and returns the nearest point indices in the dict. `Patch.contains` uses the path (point inside the transformed path, filled or not, with the `radius` slop). `Text.contains` uses the bbox.
- Picking visits artists in draw order: `Figure.pick`/`Artist.pick` call `contains` on each child and fire `pick_event` for every hit, not only the topmost. Choosing the topmost is the user's job. [K] So matplotlib does not do "topmost wins": a known gap. NeoQCP's rule (topmost within tolerance wins) is better.
- Hit test is a separate method per class, not derived from drawing.

Z-order [K, the fetched pages did not contain the table]:
- `zorder` attribute; defaults by class: images 0, `Patch` and `Collection` 1, `Line2D` 2, `Text` 3, axis and spines 2.5 (grid and ticks), `Legend` 5. `Axes.draw` stable-sorts children by zorder, so ties are insertion order. `axisbelow` rcParam moves the axis to 0.5 or 1.5. `Figure.patch` zorder 1 [V from the page's `getp` output].
- Ordering does not depend on pen style, nor on the backend. All artist types live in one sorted list. This is precisely the "layer decides, each kind has a default layer, ties by insertion" ask, with a float instead of named layers.
- Known gotcha: zorder is only compared among siblings inside one Axes; artists in different Axes follow the Axes order.

Legend handlers [V] https://matplotlib.org/stable/users/explain/axes/legend_guide.html:
- `legend_artist(self, legend, orig_handle, fontsize, handlebox)` returns a single artist. `handler_map` is a dict from handle (instance, then type, then each type in the MRO) to a handler. A default map has handlers for line, patch, collection, errorbar, stem, `tuple` (`HandlerTuple`: each handle drawn over the others). `Legend.update_default_handler_map({Type: Handler()})` registers globally. `HandlerPatch.create_artists(legend, orig_handle, xdescent, ydescent, width, height, fontsize, trans)` is the easier hook.
- Lesson: the legend icon is a function of (object, size box), registered by type, with a default derived from the object's own pen/brush. Unknown object types get no icon rather than a wrong one.

Data range / autoscale: artists do not report limits. Methods that add them call `Axes.update_datalim(xys)` / `Axes._request_autoscale_view()`; `Collection.get_datalim` exists; `sticky_edges` stop margins at 0. [K] A mixed model: the add call updates the data limits, so a hand-written artist must do it itself. This is the weak spot NeoQCP's `getKeyRange/getValueRange` and the proposed default data range already beat.

### 3.2 Makie.jl

- Plots are a tree: `Scene` -> `Plot` -> child plots. A recipe is declared with `@recipe MyPlot (x, y, z) begin ... end` and implemented by specialising `plot!(p::MyPlot)`, which "builds the visualization from existing plot functions". `convert_arguments` "must always return a Tuple" and normalises user input to the recipe's declared arguments; if nothing matches it tries `convert_single_argument` repeatedly. Child plots are created by calling `lines!(myplot, ...)` inside `plot!`; `myplot.attributes` forwards matching attributes. [V] https://docs.makie.org/stable/explanations/recipes
- Reactivity: legacy Observables (`onany`, `notify`), and "Since Makie 0.24, recipes use a `ComputeGraph` in `plot.attributes`", with `map!` registering a computation from input nodes to output nodes; the docs warn the Observable path "may run into update synchronization issues". [V] So Makie moved from push-notification to a dependency graph: invalidation by dirty inputs, recompute only affected outputs. Equivalent of "keep the recording until the object or its data changes".
- Backends: GLMakie (OpenGL), WGLMakie (WebGL), CairoMakie (vector, raster), RPRMakie. They implement a small set of primitives (`lines`, `linesegments`, `scatter`, `mesh`, `meshscatter`, `image`, `heatmap`, `surface`, `text`, `poly` ...) and every recipe reduces to those. [K] Same plot tree, different backend per output, GPU for the screen and Cairo for export. Exactly NeoQCP's situation (QRhi on screen, QPainter on export).
- Coordinate spaces as an attribute: every plot has `space` (default `:data`), and `markerspace` (default `:pixel`) "sets the space in which markersize is given". [V default values from the scatter page: https://docs.makie.org/stable/reference/plots/scatter]. Allowed values via `Makie.spaces()`: `:data, :pixel, :relative, :clip` [K, the page did not list them]. So the space tag is per plot (and per size attribute), not per point. The same tuple means different things depending on the tag.
- Z-order in 2D: GL backends use depth; `depth_shift` "adjusts the depth value ... in clip space" and `overdraw` ignores depth checks; Cairo draws in insertion order within a scene and `translate!(plot, 0, 0, z)` sets the z. [V depth_shift and overdraw descriptions; Cairo ordering K] A reminder that depth-based ordering leaks backend behaviour: two backends can disagree. NeoQCP's explicit layer plus defined within-layer order avoids that.
- Data limits and bounding box: `data_limits(plot)` and `boundingbox(plot)`, recipes inherit them from their children. [K, not on the page I fetched]
- Hit testing: `pick(scene, xy)` renders object ids into an offscreen id buffer (GLMakie) and reads one pixel; `mouseover`, `onpick`. [K] This is "derive hit testing from the rendering itself", only works with a GPU backend.
- Legend: `axislegend`, `LegendElement` types (`LineElement`, `MarkerElement`, `PolyElement`); recipes get a default legend element from their primary child plot via `legend_elements`-style conversion. [K]

### 3.3 ImPlot (Dear ImGui)

- Immediate-mode API: `ImPlot::BeginPlot(...)`, `PlotLine(label_id, xs, ys, count, spec)`, `EndPlot()`. Custom data by getter: `PlotLineG(label_id, ImPlotGetter getter, void* data, int count, spec)` with `typedef ImPlotPoint (*ImPlotGetter)(int idx, void* user_data)`. `ImPlotPoint { double x, y; }`. [V] https://raw.githubusercontent.com/epezent/implot/master/implot.h
- Custom plotters: inside the plot you may do `ImDrawList* dl = ImPlot::GetPlotDrawList(); ImPlot::PushPlotClipRect(expand); ... dl->AddLine(ImPlot::PlotToPixels(x, y), ...)`; `PlotToPixels(const ImPlotPoint&, ImAxis x_axis, ImAxis y_axis)` and `PixelsToPlot(...)`; `GetPlotLimits(x_axis, y_axis)`. [V] The helper takes the axis pair as optional arguments, so a plot with several axes works.
- Axis transforms: `typedef double (*ImPlotTransform)(double value, void* user_data)` for custom axis scales, plus built-in log/symlog; `ImAxis` indexes X1..Y3. [V]
- `ImDrawList` is the recording: vertex/index buffers plus a vector of `ImDrawCmd` (clip rect, texture id, element count, optional user callback), rebuilt every frame, then replayed by the backend renderer (OpenGL, Vulkan, Metal, DX, ...). A draw command can be a `UserCallback`, which lets a user inject raw GPU work in the middle of the recorded 2D list [K]. This is the best-known example of "one recorded list, GPU-submitted, with an escape hatch to arbitrary GPU commands in the middle".
- Z-order: items are drawn in call order. ImPlot internally splits the draw list into channels so that grid, items, axes and legend end up in a fixed relative order whatever the call order is (`ImDrawListSplitter`) [K, the internal header excerpt I fetched did not show it: [?]]. Same idea as "the layer decides".
- Legend: automatic, from `label_id`; items hold `ImPlotItem{ ID, Color, Marker, LegendHovered, SeenThisFrame, Show }` and an `ImPlotItemGroup` with `ImPlotLegend`. [V] Marker and colour come from automatic cycling (`ColormapIdx`, `MarkerIdx`). Items not seen this frame vanish: retained state is rebuilt from calls. [V member names]
- Hit testing: none per item except `IsPlotHovered()`, legend hover, `GetPlotMousePos()`; apps pick by hand with `PixelsToPlot`. [V `IsPlotHovered`; rest K]
- Lesson: PlotToPixels in a context object plus a clip push is the smallest honest extension API. No recording per object though: everything is redone every frame (ImGui is immediate by design), so no invalidation problem exists.

### 3.4 VisPy

- `Visual` [V, source: https://github.com/vispy/vispy/blob/main/vispy/visuals/visual.py]: "Base class for all visuals that can be drawn using a single shader program". "Subclasses generally only need to reimplement `_compute_bounds`, `_prepare_draw`, and `_prepare_transforms`." Shared state in `VisualShare` (GL state, bounds cache, program, views). A `MultiProgram` "internally manages multiple ModularProgram instances, one per view". `draw()` returns if invisible, calls `_prepare_draw`, applies GL state, draws the program. `update()` emits an event, transform and visibility changes call it. `bounds(axis)` caches per axis and is invalidated by `_bounds_changed`. So: bounds is a cached virtual with explicit invalidation.
- Transforms: `TransformSystem` per view with `canvas_transform`, `visual_transform`, `scene_transform`, `document_transform`, `framebuffer_transform`, `get_transform(map_from, map_to)` ("a transform mapping between any two coordinate systems"). Transform classes (`STTransform`, `MatrixTransform`, `LogTransform`, `PolarTransform`, ...) expose `shader_map()` and `shader_imap()` returning GLSL `Function` objects; chains are composed into the vertex shader (`FunctionChain`). [V names from the API index at https://vispy.org/api/vispy.visuals.html, composition behaviour K]. Relevant: the same transform object runs on the CPU (`map(points)`) and compiles to GLSL, so log/reversed axes are not re-implemented on the GPU. For NeoQCP's GPU layers this is the design to look at: one public transform object with `map` on the CPU and a shader snippet or uniform description for the GPU.
- Paper: Campagnola et al., VisPy, SciPy 2015 (see references).

### 3.5 Bokeh

- Renderers: `GlyphRenderer` holds a `data_source`, a `glyph`, and optional `selection_glyph`, `nonselection_glyph`, `hover_glyph`. A glyph class is declared in Python (`bokeh.models.glyphs`) and has a BokehJS twin with `_render(ctx, indices, data)`, `_hit_point`, `_hit_span`, `_hit_rect`, `_hit_poly`, `draw_legend_for_index`. [K, the docs page I fetched was an index only]. Custom extension: write a TS model; documented as "Extending Bokeh". [K]
- Hit testing is done on the data, through a spatial index (flatbush, an R-tree) over glyph bounds, then exact per-glyph tests, returning indices (not a distance). [K] Selection is expressed in data indices, which is what a plotting library needs, not "which object".
- z-order: renderers have a `level` (`"image" < "underlay" < "glyph" < "guide" < "annotation" < "overlay"`), within a level the order of `renderers` list. [K, [?] not verified from the docs] A named-layer system with defaults per renderer kind and insertion order inside, same as proposed.
- Backends: canvas 2D by default, WebGL for some glyphs (markers, lines, hex tiles) with fallback to canvas for the rest, in one plot. [K] Mixed GPU/CPU in one figure, ordered by layer.

### 3.6 Vega / Vega-Lite / D3

- Declarative spec compiled into a dataflow (reactive) graph, then into a scenegraph of marks and items with bounds; rendered by canvas or SVG renderers. `zindex` on marks "Sets the layering of a mark set relative to other marks, axes, and legends. The default is 0, and higher values draw on top. It applies to the whole set, not individual items. To order items within a set, use the `zindex` encoding channel." [V] https://vega.github.io/vega/docs/marks/ Encode sets `enter`, `update`, `exit`, `hover` [V]; `interactive: false` removes events from a mark; `clip` default false (opt-in, clipped to the group) [V].
- Picking: scenegraph items have bounds; the canvas renderer finds the item under the pointer by testing each mark type's own pick function using `isPointInPath/isPointInStroke` (Canvas2D path tests) after a bounds check, topmost first. [K, not on the page fetched]. This is hit testing derived from recorded geometry, via a path test, no extra code per mark.
- Scales: `d3-scale` objects (`linear`, `log`, `pow`, `time`, `band`, `ordinal`) with `domain()`, `range()`, `invert()`, `ticks()`; scale == coordinate transform with a uniform API. Vega-Lite `Bostock/Heer` design. [K]

### 3.7 Flutter

- `CustomPainter`: `paint(Canvas, Size)` ("Called whenever the object needs to paint"), `shouldRepaint(covariant CustomPainter oldDelegate)` "runs when a new painter instance is supplied, and it determines whether that instance holds different information that requires a repaint", `hitTest(Offset position)` optional, `repaint` a `Listenable` that triggers repaint without rebuild, `shouldRebuildSemantics`. [V] https://api.flutter.dev/flutter/rendering/CustomPainter-class.html
- Recording: the `Canvas` handed to `paint` records into a `Picture` through a `PictureRecorder` ("Records a Picture containing a sequence of graphical operations", `endRecording()` returns the `Picture`). [V] https://api.flutter.dev/flutter/dart-ui/PictureRecorder-class.html The `Picture` is attached to a `PictureLayer`; the layer tree is replayed by the engine (Skia/Impeller) on the raster thread. A `RepaintBoundary` makes a subtree own its layer so only that layer re-records. [K]
- Invalidation: two explicit hooks. Value comparison (`shouldRepaint(old)`) when the painter is replaced, and notification (`repaint` listenable, `markNeedsPaint`). That is the pattern for "keep the recording until the object or its data changes": compare a cheap version or parameters, or have the data signal.
- Hit test: default is the whole box (if `hitTest` is not overridden, the painter is hit anywhere in its bounds [K]), overridable with a point test. Not derived from the picture.
- Opacity: "Widgets share a minimum number of canvases", so the painter's canvas may be shared; for blend modes use `saveLayer` "relatively expensive" [V]. Mixed recorded content is a normal thing.

### 3.8 Skia: SkPicture and SkDrawable

- `SkPicture` "records drawing commands made to SkCanvas" via `SkPictureRecorder`; plays back with `SkCanvas::drawPicture`. Immutable once recorded. `cullRect()` gives bounds. [K, plus the sentence quoted on the SkDrawable page [V]]
- `SkDrawable` [V] https://api.skia.org/classSkDrawable.html: abstract base "for objects that draw into SkCanvas". `onDraw(SkCanvas*)` (protected pure virtual), `onGetBounds()` (pure), `onMakePictureSnapshot()` "By default, it calls onDraw() with a canvas that records into a picture. Subclasses may override if they have a more efficient way to return a picture for the current state". `getGenerationID()` is "meant for caching. If two calls return the same value, the drawable is presumed to render the same output". `notifyDrawingChanged()` "invalidates the previous generation ID". `getBounds()` "conservative bounds ... must remain valid for every possible state". `draw()` leaves canvas save level, matrix and clip unchanged. 
- This is the closest published match to the proposed contract: a small base class with `onDraw`, `onGetBounds`, a generation counter bumped by `notifyDrawingChanged()`, and a snapshot that records once and replays while the generation is unchanged.
- Skia GPU: Graphite has `Recorder` -> `Recording` -> `Context::insertRecording`: a GPU-command recording that is built on any thread and submitted later. [K] A recording that holds GPU work, not just canvas calls.
- Licence BSD-3 [K].

### 3.9 Chromium cc and RenderingNG

- `cc/paint` "replaces the SkPicture, SkCanvas, and SkPaint recording structures" for faster recording and transport; "Skia remains the final raster backend". [V] https://chromium.googlesource.com/chromium/src/+/HEAD/cc/paint/README.md
- `DisplayItemList` [V] (`cc/paint/display_item_list.h`): "a container of paint operations", ref-counted. Holds a `PaintOpBuffer`, and after `Finalize()` an `RTree<size_t>` of byte offsets, built from per-op `visual_rects_`. "builds an rtree so that rasterizing a rect replays only the ops that rect needs". `push<T>(args...)` constructs an op in the buffer and records its offset; `EndPaintOfUnpaired(visual_rect)` / `EndPaintOfPairedBegin()` record the visual rect; paired begin/end items (save/restore, clip, transform) are tracked on a stack that must be empty at `Finalize()`. `SearchOpsByRect(query, op_indices)` returns ops whose visual rects intersect. `bounds()` is the rtree bounds. Raster overloads take an `SkCanvas*`.
- RenderingNG pipeline [V] https://developer.chrome.com/docs/chromium/renderingng-architecture: paint "compute a display list that describes how to raster GPU texture tiles from the DOM"; commit copies "property trees and the display list to the compositor thread"; layerize splits into layers; raster turns display lists into GPU texture tiles; "pre-paint invalidates" existing display lists and tiles "as appropriate"; property trees (transform, clip, effect, scroll) are separate from the display list so animations mutate the trees instead of re-recording. That separation (record once in local coordinates, change transforms and clip outside the recording) is the key trick for pan and zoom: a recording of a polygon in data coordinates does not need re-recording on pan if the transform is applied at replay.
- Hit testing in Blink is not derived from the display list; hit-test data is stored next to it (paint chunks carry hit-test rectangles) [K].

### 3.10 Others worth knowing

- egui_plot (Rust) [V] https://docs.rs/egui_plot/latest/egui_plot/trait.PlotItem.html: the nearest match to the proposed API.
  - `trait PlotItem`: `fn shapes(&self, ui: &Ui, transform: &PlotTransform, shapes: &mut Vec<Shape>)` (records into a list), `fn bounds(&self) -> PlotBounds`, `fn geometry(&self) -> PlotGeometry`, `fn color(&self) -> Color32`, `fn initialize(&mut self, x_range)`, provided `name()`, `highlight()`, `allow_hover()`, `find_closest(&self, point: Pos2, transform: &PlotTransform) -> Option<ClosestElem>`, `on_hover(...)`.
  - So: one record method into a `Vec<Shape>`, bounds for autoscale, a "closest element" hit test returning a distance, defaults for the rest. The `PlotGeometry` enum (`None`, `Points(&[PlotPoint])`, `Rects`) lets the library implement the default `find_closest` from the item's declared geometry. That is "derive hit testing from the geometry" with a small enum.
  - Each frame egui re-runs `shapes()`; the shapes are retained only until the next frame.
- Plotters (Rust) [V] https://docs.rs/plotters/latest/plotters/coord/index.html: `CoordTranslate` "translates some customized object to the backend coordinate", `ReverseCoordTranslate` for the inverse (interactive plots), `Cartesian2d` combinator, `ranged1d` for 1D coordinate abstraction, combinators such as "logarithmic coordinate, nested coordinate", `Shift`. Typed: a `Cartesian2d<X, Y>` is parametrised by the two axis types (f64, Date, ...), and drawing elements take points of the associated logical type, and backend pixel coordinates `BackendCoord` are a different type. So the type system separates "data" from "backend pixels", and the per-axis kind (log, date) is a type parameter. [signatures not verified [?]]
- euclid / WebRender [V] https://docs.rs/euclid/latest/euclid/: `Point2D<T, U>`, `Transform2D<T, Src, Dst>`, `Scale<T, Src, Dst>`; "tagged with a generic Unit parameter which is useful to prevent mixing values from different spaces", example: "it should not be legal to translate a screen-space position by a world-space vector". Compile-time phantom units. Used by Servo and Firefox WebRender [K]. The Rust analogue of `Data{x,y}` vs `Pixels{x,y}`.
- Vello [V] https://github.com/linebender/vello: Rust 2D renderer, Apache-2.0 or MIT; Vello CPU, Vello GPU (CPU path preprocessing, GPU raster, wgpu or WebGL2), and the compute-shader renderer ("experimental"). The API builds a `Scene` (recording: `fill`, `stroke`, `draw_image`, `push_layer`) which is then rendered; [K for the Scene API, the page did not describe it]. Another instance of "record scene, replay on the GPU or CPU backend".
- WebRender (Firefox) [K]: display lists built by Gecko, serialised, resolved on a render-backend thread into a frame, with spatial and clip trees separate from items, hit-test items recorded in the same list. Same split as Chromium.
- Cairo / pango style `cairo_recording_surface_t` (record cairo calls, replay on any surface), and SVG/PDF surfaces. [K] Same family as QPicture/SkPicture.

### 3.11 What this suggests for NeoQCP (non-Qt section)

- The published design nearest to the target is egui_plot's `PlotItem` plus Skia's `SkDrawable` (generation id, bounds, snapshot). Take: one record method, a bounds method, a generation counter, defaults for the rest.
- matplotlib's transform tree is the best reference for positions: named systems (data, axes, figure, display), per-axis mixing by blending, physical-unit offsets, `inverted()`. Reproduce the semantics, add types so mixing is a compile error.
- Chromium and Flutter show where invalidation belongs: record in local coordinates, keep transform, clip and "theme" outside the recording and apply at replay, so pan, zoom and restyle do not re-record. Only a change of the object's own data re-records.
- Makie and matplotlib show the backend split: a small primitive set (polyline, markers, filled polygon, image, text, path collection) that every backend implements, and recipes that only compose those.

---------------------------------------------------------------------------------------------------

## 4. Cross-cutting questions

### 4a. Recorded vs immediate drawing

Who records and who is immediate:

| System | Model | Re-record trigger |
|---|---|---|
| QPainter, matplotlib `draw`, Qwt, QCP, ImPlot | immediate (matplotlib/ImPlot re-run per frame) | n/a, caller decides when to repaint |
| QPicture, SkPicture, Cairo recording surface | record once, replay many; immutable | caller re-records |
| SkDrawable | record on demand, cached by generation id | `notifyDrawingChanged()` |
| Flutter CustomPainter | record each paint into a PictureLayer; layer retained | `shouldRepaint(old)` / repaint listenable / `markNeedsPaint` |
| Chromium DisplayItemList | record per paint chunk; rtree of visual rects | pre-paint invalidation of the chunk |
| Qt Quick | retained node tree with dirty bits | `update()` then `markDirty(flags)` in `updatePaintNode` |
| QGraphicsItem | retained items, immediate `paint`; optional pixel cache | `update()`, `prepareGeometryChange()` |
| Makie | retained plot tree, ComputeGraph | dirty input nodes |
| egui_plot / ImPlot / ImDrawList | rebuilt every frame | every frame |
| QCanvasPainter | immediate API, internal command buffer flushed at `endPaint()` | every frame (keeps `QCanvasPath`, images) |

Patterns for invalidation:
1. Generation counter (SkDrawable `getGenerationID`, mirrors a `data version`): cheap, caller-visible, lets a cache key be (object, generation, transform-version).
2. Pure comparison of the new description with the old one (Flutter `shouldRepaint`): works for value-style objects, costs a compare per frame.
3. Dirty flag + notification up the tree (matplotlib `stale`, QGraphicsItem `update`, Qt Quick `markDirty`): push-style, simplest for NeoQCP, which already has `replot` scheduling.
4. Dependency graph (Makie ComputeGraph): right for derived values such as "tick positions from axis range" or "resampled polyline from data and pixel width".
5. Separate what the recording depends on: Chromium property trees (transform, clip, effect) are not in the display list, Flutter transform layers, Skia matrix at `drawPicture`. Decide up front what the recording captures (data, local coordinates) and what it does not (view transform, clip, theme colours, DPR).

Hidden dependency to decide explicitly: a polyline that was decimated for the current pixel width, a text label measured with the current font, a stroke width in physical units. These depend on the view or DPR, so the recording must either record in a view-independent form (resample at replay) or key its cache on (view range, DPR) too. Chromium re-rasters tiles when scale changes, Flutter re-records on resize, QGraphicsItem `DeviceCoordinateCache` re-renders "if the item is transformed". Pan is the cheap case if recording is in data space.

Mixing GPU and CPU in one recording:
- ImDrawList: one list of commands, the backend renderer submits them, `UserCallback` commands run arbitrary code between them. [K]
- QSG: GPU geometry nodes plus `QSGRenderNode` (arbitrary QRhi) in one tree; the renderer orders them as a painter's algorithm. Software draw is a `QSGTexture` made from a QImage painted by QPainter (QQuickPaintedItem). [V/K]
- matplotlib mixed mode: rasterised artists embedded as images into a vector backend. [K]
- Bokeh: WebGL for some glyphs, canvas for the rest, same z-order list. [K]
- Skia Graphite recordings, Vello CPU/GPU: the same scene can go to a CPU or GPU backend. [K]
- Practical recipe from these: tag each recorded command with the pass it needs (`GpuLine`, `GpuColorMap`, `Painter`) but keep a single ordered list per layer; at replay, a layer is split into runs of same-pass commands. If a layer has runs `[G G P P G]` the renderer must either order passes (all P over all G, loses interleaving) or flush the GPU pass, draw the painter run into a texture, composite, and continue. The systems that ship (Qt Quick, ImGui, Bokeh) accept that interleaving costs a pass break and keep it rare by putting the GPU items at the bottom and annotations on top (layers).

### 4b. Typed coordinates

How systems type positions:
- matplotlib: no point type. The coordinate system is the transform attached to the artist (`transData`, `transAxes`, blended ...). `Artist.set_transform`. Runtime, per artist, silent on errors. [V]
- QCustomPlot: runtime tag on each position: `QCPItemPosition::PositionType` (`ptAbsolute`, `ptViewportRatio`, `ptAxisRectRatio`, `ptPlotCoords`), x and y tags separate (`setTypeX/Y`), parent anchor for "anchor + offset" (`setParentAnchor`), conversion `pixelPosition()`. [V] The positions are objects bound to an item, not values.
- Makie: attribute `space` on the plot (default `:data`), `markerspace` (default `:pixel`) for sizes. [V defaults] Per plot, not per point.
- Qwt: x and y maps passed in; `QwtScaleMap::transform` for one axis; points are bare `QPointF` (pixel or value by convention). [K]
- Plotters: coordinate system is a type (`Cartesian2d<X,Y>`), logical points have the axis element types (f64, Date), backend coordinates are a different type (`BackendCoord`). [V that traits exist, [?] signatures]
- Rust `euclid`: phantom unit tags on points, vectors, transforms: `Point2D<T,U>`, `Transform2D<T,Src,Dst>`. Mixing spaces does not compile. [V]
- VisPy: transform objects between named systems (`canvas`, `framebuffer`, `render`, `visual`, `scene`, `document`) via `TransformSystem.get_transform(from, to)`. [V names]
- ImPlot: pixel vs plot, by function name (`PlotToPixels`, `PixelsToPlot`) with optional axis index. [V]
- Vega: scales (domain to range), mark encodings name the scale per channel; per-channel mixing is natural (`x` on a scale, `y` as a pixel `signal`). [K]
- Skia/Qt: untyped. `QPointF` is data, pixel or ratio by convention.

Mixed per-axis systems ("x in data, y in axes fraction"): matplotlib `blended_transform_factory` (separable only), QCP `setTypeX/setTypeY`, Vega channels. Needed for spans, vertical lines, labels along an axis, rubber bands. Typed form: `Pos{ AxisCoord<X>, AxisCoord<Y> }` where each coordinate carries its own tag; `Data{x,y}` is the case where both tags are data.

Log, reversed, time axes: matplotlib puts them in the non-affine part of `transData`; Qwt via `QwtTransform` in `QwtScaleMap`; ImPlot via `ImPlotTransform` function pointers; Plotters via combinators; VisPy via GLSL-capable transform classes. In all cases the user does not handle log themselves, they call `map`.

Cost of typed: a conversion that is O(1) per point is fine, but converting a million points per frame must be a batch call (`map(span<Data>) -> vector<Pixels>`, or a GPU uniform + shader function as VisPy does). A typed API that only has scalar conversions pushes users to write slow loops.

What it suggests: `Data`, `Pixels`, `AxisRatio` as distinct structs, a `Pos`-like pair with per-axis tags for mixed cases, `Anchor + Offset` as its own value, and transform objects with `map(Data)->Pixels`, `inverse`, plus batch `map(range)`. matplotlib's semantics, euclid's compile-time safety, QCP's anchor idea.

### 4c. Default hit testing for custom drawn objects

Options in the wild:
1. Bounding box: QGraphicsItem default `shape()` = `boundingRect()` [V]; Flutter default whole box [K]; matplotlib `Text` bbox [K].
2. Declared shape or geometry: QGraphicsItem `shape()` + `QPainterPathStroker` [V]; egui_plot `geometry()` -> `find_closest` [V, signature]; Qwt `QwtPlotCurve::closestPoint` [K].
3. Stroke distance: QCP `selectTest` returns pixel distance, area items 0.99 x tolerance [V]; matplotlib `Line2D.contains` with `pickradius` [K].
4. Spatial index over data, then exact test: Bokeh (flatbush) [K]; Chromium rtree of visual rects for raster [V], not hit testing.
5. Derived from the rendering: Makie GL id-buffer pick [K]; Vega `isPointInPath/isPointInStroke` on the item's path [K]; Qt Quick `QQuickItem::contains` on the bounding shape (no derivation from nodes) [K].
6. Recorded separately: WebRender hit-test items [K], Blink hit-test data per paint chunk [K].

Can it be derived from the recording itself? Yes, in the useful subset. The recording holds typed geometry: polylines, polygons, markers, rects, text boxes, images. For each command there is a cheap distance in pixels:
- polyline: point-to-segment distance (with a simple grid or bbox culling on a long list), dominated by a binary search on x if sorted;
- filled polygon/shape: inside test gives distance 0 (or 0.99 x tolerance rule from QCP), outside gives distance to the outline;
- marker: distance to centre minus size;
- text/image: rect distance, inside = hit;
- error bar: segment distance.
Each command carries the pen width, so the "stroke distance minus half width" is correct. The result is `min over commands`, and the rule is one: "a pixel distance or missed". That is QCP `selectTest` computed from the draw list instead of written by hand. The recording must keep geometry in a form that survives (not just a QPicture, which cannot be iterated). Recorded in data space it can also be tested in data space with a tolerance converted through the transform, avoiding a per-pan rebuild.
Caveats: (a) GPU-drawn heavy data (a 10M point line, a colour map) should not be traversed per pointer move, so those commands should carry an optional `hit` callback or data-backed index (`findNearest(x)`, bin lookup) like Bokeh and egui `geometry`; (b) text is rect-only; (c) anything drawn via a raw QPainter escape hatch has only its declared bounds; (d) transparent fill pixels and `NoBrush` are not hit ("filled shapes hit where visible").
Topmost-wins: collect (distance, layer order, in-layer order) and take the one with distance within tolerance and highest order, break ties by smaller distance. matplotlib does not do it (fires on every hit), QGraphicsScene does (descending z) with no tolerance. QCP picks the closest distance rather than the topmost [K]. The maintainer's rule is the combination of both.

### 4d. Default z-order and independence from backend passes

- matplotlib: float `zorder` per artist, class defaults (image 0, patch/collection 1, line 2, text 3, axis 2.5, legend 5), stable sort so ties are insertion order. Independent of backend and pen. [K]
- QGraphicsItem: `zValue` (default 0), then insertion order, parents before children, same order for picking (descending). [V]
- Qwt: `z()` increasing, items sorted. [V]
- QCP: layers (named, ordered), inside a layer insertion order, `lmBuffered` layers get a paint buffer. [K]
- Bokeh: `level` enum then list order. [K]
- Vega: `zindex` default 0, per mark set; `zindex` encode channel inside the set. [V]
- ImPlot: call order, with fixed channels for grid, items, legend. [K, [?]]
- Makie GL: depth + `depth_shift`/`overdraw` (backend dependent), Cairo: insertion. [V attributes]
- Qt Quick: z property among siblings then tree order; `QSGRenderNode` is placed in tree order, batched but painter-order preserved by the renderer (`DepthAwareRendering` flag tells it the node respects the Z convention). [V flag]
- Chromium: paint order is defined by the stacking context rules; display list order is the order; GPU raster preserves it. [K]

How they keep order independent of the pass: all of them define the order on one list of objects (or layers), before any backend sees it. The backend consumes it in order, and if it has two sub-passes, it flushes at pass changes (Qt Quick) or uses separate surfaces per layer composited in layer order (QCP buffered layers, Flutter layers, Chromium composited layers). Matplotlib's mixed-mode renderer does the second form. Never does a system order by "which pass drew it", and no system orders by pen style. An order that depends on pen style in QCP comes from the data-dependent code path (solid lines go to GPU, dashed to QPainter), exactly the bug class the maintainer wants to remove; the fix is a per-layer total order, plus the rule "a layer is replayed as ordered runs".

Default layer per kind: matplotlib per class zorder; QCP per layerable class; Bokeh per renderer kind. A `kind` enum with a table `kind -> default layer` is the standard approach, user override through `setLayer`.

### 4e. Default legend icons for custom objects

- matplotlib: handler map by type, MRO lookup, default handlers for line, patch, collection, errorbar, stem, tuple; custom via `legend_artist(legend, orig_handle, fontsize, handlebox)`; objects without a handler are skipped with a warning. [V]
- Qwt: `legendIcon(index, size) -> QwtGraphic`, default invalid; `legendData()` can return several entries. [V]
- QCP: `drawLegendIcon(QCPPainter*, const QRectF&)` pure virtual on plottables, clip already set to the rect. [V]
- Bokeh: each glyph draws `draw_legend_for_index`. [K]
- Makie: `LegendElement`s derived from the child plots. [K]
- ImPlot: automatic from item colour and marker. [V member names]
- Derivable from the recording: yes for a common subset. Take the recorded commands, find the "primary" one (first polyline, first filled shape, first marker) and render it scaled into the icon rect: a polyline becomes a horizontal line in the same pen; a filled shape becomes a filled rect with the same brush and pen; markers become a centred marker; error bars become the marker plus a vertical bar; images and colour maps become a gradient strip (the colour scale). This is what matplotlib's default handlers do by type, but derived from the recording rather than by class. Qwt's `QwtGraphic` (a recorded graphic scaled to a size) is the precedent for storing a legend icon as a scalable recording, matching a `legendIcon()` that returns a draw list. An override hook stays (matplotlib `handler_map`, QCP `drawLegendIcon`).

### 4f. What this suggests for NeoQCP (cross-cutting)

1. Draw list = ordered vector of small typed commands in local (data) coordinates, each tagged with the pass it needs. Not a QPicture, not a QCanvasPainter command buffer.
2. Invalidation = a generation counter on the object (SkDrawable), bumped by the object's own data change; view change does not bump it, because the list is in data space and the transform is applied at replay (Chromium property trees).
3. Hit test, legend icon and bounds are three default functions over the list, each with an override hook (egui_plot, QCP, Qwt).
4. Ordering: layer index, then in-layer insertion index, defined before replay; passes are only a replay detail (matplotlib, QGraphicsItem, Vega).
5. Types: structs per space, plus a mixed per-axis pair, public transform objects with batch `map` (matplotlib semantics, euclid safety, VisPy CPU/GPU sharing).

---------------------------------------------------------------------------------------------------

## 5. Bibliography

Citation details are from memory unless marked; DOIs should be checked before they go into a paper or design doc. [K] = from memory, [V] = a page I fetched confirmed it exists.

1. Hunter, J. D. (2007). "Matplotlib: A 2D Graphics Environment." Computing in Science & Engineering 9(3): 90-95. doi:10.1109/MCSE.2007.55. [K] Relevant: the Artist/Renderer/Transform architecture that the maintainer's "like matplotlib" refers to. Architecture chapter of the book "The Architecture of Open Source Applications, vol. 2" (Hunter and Droettboom, "matplotlib", https://aosabook.org/en/matplotlib.html) [K] is the longer, better text on Artists and the transform stack.
2. Wilkinson, L. (2005). "The Grammar of Graphics", 2nd ed., Springer. doi:10.1007/0-387-28695-0. [K] Relevant: scales, coordinates and guides as separate concepts (a scale maps data to a unit interval, a coordinate system maps that to the plane); justifies separating the axis transform from the object.
3. Wickham, H. (2010). "A Layered Grammar of Graphics." Journal of Computational and Graphical Statistics 19(1): 3-28. doi:10.1198/jcgs.2009.07098. [K] Relevant: layers as the unit of composition (data + mapping + stat + geom + position), the ggplot2 model behind "each object kind has a default layer".
4. Bostock, M. and Heer, J. (2009). "Protovis: A Graphical Toolkit for Visualization." IEEE Trans. Visualization and Computer Graphics 15(6): 1121-1128. doi:10.1109/TVCG.2009.174. [K] Relevant: marks with per-property functions of data, scales as first-class, retained mark tree.
5. Bostock, M., Ogievetsky, V., Heer, J. (2011). "D3: Data-Driven Documents." IEEE TVCG 17(12): 2301-2309. doi:10.1109/TVCG.2011.185. [K] Relevant: scale objects with `domain/range/invert`, the d3-scale API that Vega and many plotting libraries imitate.
6. Heer, J. and Bostock, M. (2010). "Declarative Language Design for Interactive Visualization." IEEE TVCG 16(6): 1149-1156. [K] Relevant: why a retained, declarative scene with explicit invalidation beats immediate callbacks for interaction.
7. Satyanarayan, A., Russell, R., Hoffswell, J., Heer, J. (2016). "Reactive Vega: A Streaming Dataflow Architecture for Declarative Interactive Visualization." IEEE TVCG 22(1): 659-668. doi:10.1109/TVCG.2015.2467091. [K] Relevant: dataflow graph that re-evaluates only what changed (scenegraph items are updated, not rebuilt). Same idea as Makie's ComputeGraph.
8. Satyanarayan, A., Moritz, D., Wongsuphasawat, K., Heer, J. (2017). "Vega-Lite: A Grammar of Interactive Graphics." IEEE TVCG 23(1): 341-350. doi:10.1109/TVCG.2016.2599030. [K]
9. Danisch, S. and Krumbiegel, J. (2021). "Makie.jl: Flexible high-performance data visualization for Julia." Journal of Open Source Software 6(65): 3349. doi:10.21105/joss.03349. [K] Relevant: recipes, backends sharing one plot tree, Observables.
10. Campagnola, L., Klein, A., Larson, E., Rougier, N. P. (2015). "VisPy: Harnessing the GPU for Fast, High-Level Visualization." Proceedings of the 14th Python in Science Conference (SciPy 2015). [K, author list [?]] Relevant: Visuals plus transform chains compiled to GLSL.
11. Rougier, N. P. (2013). "Shader-Based Antialiased, Dashed, Stroked Polylines." Journal of Computer Graphics Techniques 2(2): 105-121. [K] Relevant: GPU polylines with joins, caps and dashes; what a "GPU polyline helper" has to solve and why dashes are the awkward case.
12. Loop, C. and Blinn, J. (2005). "Resolution Independent Curve Rendering Using Programmable Graphics Hardware." ACM Trans. Graphics 24(3) (SIGGRAPH 2005): 1000-1009. [K] Relevant: GPU curve filling, basis of Qt Quick Shapes' curve renderer and others.
13. Levien, R. and Uguray, A. (2024). "GPU-friendly Stroke Expansion." arXiv:2405.00127. [K, [?] id not re-checked] Relevant: stroke expansion on the GPU in Vello.
14. Muratori, C. (2005). "Immediate-Mode Graphical User Interfaces" (video talk), https://caseymuratori.com/blog_0001 [K, [?] URL]. And Dear ImGui wiki "About the IMGUI paradigm", https://github.com/ocornut/imgui/wiki/About-the-IMGUI-paradigm [K]. Relevant: the definition of immediate vs retained, and why immediate sidesteps invalidation at the cost of re-running the code each frame.
15. Strauss, P. and Carey, R. (1992). "An Object-Oriented 3D Graphics Toolkit." SIGGRAPH '92: 341-349. [K, [?]] Relevant: Open Inventor scene graph, node-based retained rendering with notification of changes upward (the ancestor of the Qt Quick node tree and QGraphicsItem).
16. Foley, van Dam, Feiner, Hughes (1990). "Computer Graphics: Principles and Practice", 2nd ed., Addison-Wesley, chapters on display lists and retained vs immediate-mode (PHIGS, SPHIGS). [K] Relevant: the classical vocabulary: structure storage, editing, traversal.
17. Leijen, D. and Meijer, E. (1999). "Domain Specific Embedded Compilers." 2nd USENIX Conf. on DSLs. [K, [?]] Relevant: phantom types, the technique behind compile-time coordinate-space tags. A more practical source: the `euclid` crate documentation (see references).
18. Quiet note: I did not find a peer-reviewed paper specifically about recording drawing commands for hit testing; the closest are engineering design docs (Chromium "Slimming Paint" / "CompositeAfterPaint", WebRender docs) [K, not fetched].

---------------------------------------------------------------------------------------------------

## 6. Top 10 design ideas for NeoQCP, ranked

1. A tiny base-class contract in the SkDrawable mould: `draw(context)` (records), `bounds()`, and a generation counter that the object bumps when its own data or style changes; the recording is cached against that counter. Source: Skia `SkDrawable` (`onDraw`, `onGetBounds`, `getGenerationID`, `notifyDrawingChanged`, `onMakePictureSnapshot`) https://api.skia.org/classSkDrawable.html [V]; same trigger idea in Flutter `shouldRepaint` [V].
2. Record in local (data) coordinates and keep the view transform, the clip and the theme OUTSIDE the recording, applied at replay. Pan, zoom and restyle then never re-record. Source: Chromium RenderingNG property trees separate from display lists https://developer.chrome.com/docs/chromium/renderingng-architecture [V]; Skia `drawPicture` with a matrix; Flutter transform layers [K].
3. Three overridable defaults, all computed from the recording: data range (egui_plot `bounds()`, QCP `getKeyRange/getValueRange` with a "found" flag and sign domain, Qwt's invalid rect = "not in autoscale"), hit test, legend icon. Source: egui_plot `PlotItem` https://docs.rs/egui_plot/latest/egui_plot/trait.PlotItem.html [V]; QCP https://www.qcustomplot.com/documentation/classQCPAbstractPlottable.html [V]; Qwt https://qwt.sourceforge.io/class_qwt_plot_item.html [V].
4. One hit rule: each recorded command knows its pixel distance (polyline segment distance minus half the pen, filled shape 0 inside and outline distance outside, marker, rect for text and images); the object's distance is the minimum; the plot picks the topmost object whose distance is within the tolerance, ties by smaller distance. Filled-area wrinkle from QCP: area objects return a constant just below the tolerance (0.99 x) so lines on top of them stay selectable. Order for picking = order for painting, reversed (QGraphicsScene descending order). Sources: QCP `selectTest` [V]; QGraphicsItem `shape()` and stacking rules https://doc.qt.io/qt-6/qgraphicsitem.html [V]; Vega pick via path tests [K]; matplotlib fires on every hit and has no topmost rule [K], which is what to avoid.
5. Positions: distinct structs `Data`, `Pixels`, `AxisRatio`, plus an `Anchor + Offset` value and a mixed per-axis pair (`x` data, `y` axis ratio); transforms are public objects with `map`, `inverse` and a batch `map(range)`; log, reversed and time axes live inside the data transform, never in user code. Offsets in physical units (1/72 inch) so they survive export at other DPI. Sources: matplotlib transforms tutorial (`transData`, `transAxes`, `blended_transform_factory`, `ScaledTranslation`, `.inverted()`) https://matplotlib.org/stable/users/explain/artists/transforms_tutorial.html [V]; euclid phantom units https://docs.rs/euclid/latest/euclid/ [V]; QCP `setTypeX/setTypeY` and `setParentAnchor` https://www.qcustomplot.com/documentation/classQCPItemPosition.html [V]; VisPy transforms that exist on CPU and compile to GLSL [V names].
6. Stacking authority is the layer. A `kind -> default layer` table, then in-layer insertion order, fixed before any pass sees the list; pen style, GPU-versus-painter and data never influence it. Allow a user override per object. Sources: matplotlib class-default zorder and stable sort [K], QGraphicsItem (`zValue`, then insertion order, same order for picking) [V], Vega `zindex` default 0 per mark set [V], Qwt `z()` [V], Bokeh render `level` [K].
7. Replay a layer as ordered runs of same-pass commands (`GpuLine`, `GpuFill`, `ColorMap`, `Painter`). A run change costs a flush or a composite, so default layers put GPU data at the bottom and annotations on top. Keep a raw `Painter` escape hatch command (record a QPicture or a lambda replayed in the painter pass and in export) like ImDrawList's `UserCallback` and `set_rasterized` in matplotlib's mixed mode. Split work like `QSGRenderNode`: `prepare()` (upload, outside the pass) then `render()` (in the pass). Sources: Dear ImGui `ImDrawList` / `ImDrawCmd` [K], `QSGRenderNode` https://doc.qt.io/qt-6/qsgrendernode.html [V], matplotlib `MixedModeRenderer` [K], QPicture https://doc.qt.io/qt-6/qpicture.html [V].
8. Legend icon derived from the recording (primary command rescaled into the icon rect: line, filled box, marker, marker plus error bar, gradient strip for images and colour maps), stored as a small scalable draw list (Qwt `QwtGraphic`), with a per-type override registry (matplotlib `handler_map`, MRO lookup). Sources: matplotlib legend guide https://matplotlib.org/stable/users/explain/axes/legend_guide.html [V]; Qwt `legendIcon` / `legendData` [V]; QCP `drawLegendIcon` [V].
9. Keep the helper set small and backend-neutral, a handful of primitives (polyline, polygon fill, fill-between, markers, error bars, text, image, shape/path) and let every higher recipe be a composition of them; each helper picks GPU or QPainter itself. Sources: Makie recipes `plot!`, `convert_arguments`, child plots https://docs.makie.org/stable/explanations/recipes [V]; matplotlib `RendererBase` primitives [K]; egui_plot `Shape` list [V].
10. Qt building blocks to reuse without adopting a framework: `QPainterPath` (+ `QPainterPathStroker`, `contains`, `controlPointRect`) as the shape/hit type https://doc.qt.io/qt-6/qpainterpath.html [V]; QPicture for raw painter escape; and, only if the maintainer accepts GPL-3.0/commercial licensing, QCanvasPainter as one optional GPU helper backend inside the existing render pass via `beginPaint(...)`, `endPaint(DoNotRecordRenderPass)`, then `renderPaint()` https://doc.qt.io/qt-6/qcanvasrhipaintdriver.html [V]. It needs a stencil attachment, has limited clipping with a custom matrix, cannot feed QPainter export, so it cannot be the only path.

Cross-cutting caveats to settle early:
- Decide what a recording depends on (DPR, font, resample to pixel width). If a helper resamples to the current pixel width, either keep the full data and decimate at replay, or key the cache on the view as well.
- Non-separable axes (polar, projections) break per-axis mixing (matplotlib's own caveat in its transforms tutorial [V]). NeoQCP's `SciQLopNDProjectionPlot` is Cartesian per plot, so this is fine now; do not promise it.
- A batch conversion path for millions of points is required, or users will write scalar loops with the typed API.

---------------------------------------------------------------------------------------------------

## 7. References (URLs)

Qt (verified in the Qt 6.12 doc set or doc.qt.io):
- QPicture: https://doc.qt.io/qt-6/qpicture.html
- QPainterPath: https://doc.qt.io/qt-6/qpainterpath.html
- QGraphicsItem: https://doc.qt.io/qt-6/qgraphicsitem.html
- QQuickItem (updatePaintNode): https://doc.qt.io/qt-6/qquickitem.html
- QQuickPaintedItem: https://doc.qt.io/qt-6/qquickpainteditem.html
- QSGRenderNode: https://doc.qt.io/qt-6/qsgrendernode.html
- Scene Graph custom QSGRenderNode example: https://doc.qt.io/qt-6/qtquick-scenegraph-customrendernode-example.html
- Qt Canvas Painter module: https://doc.qt.io/qt-6/qtcanvaspainter-module.html
- QCanvasPainter: https://doc.qt.io/qt-6/qcanvaspainter.html
- QCanvasRhiPaintDriver: https://doc.qt.io/qt-6/qcanvasrhipaintdriver.html
- QCanvasPainterWidget hello example: https://doc.qt.io/qt-6/qtcanvaspainter-hellowidget-example.html
- What's new in Qt 6.11 (Canvas Painter tech preview): https://doc.qt.io/qt-6/whatsnew611.html
- What's new in Qt 6.12 (Canvas Painter supported, Qt Graphs backend): https://doc.qt.io/qt-6/whatsnew612.html
- Qt Charts (deprecated since 6.10): https://doc.qt.io/qt-6/qtcharts-index.html
- Qt Canvas Painter index / licence: https://doc.qt.io/qt-6/qtcanvaspainter-index.html

Qt plotting libraries:
- Qwt QwtPlotItem: https://qwt.sourceforge.io/class_qwt_plot_item.html
- Qwt QwtPlotSeriesItem: https://qwt.sourceforge.io/class_qwt_plot_series_item.html
- QCustomPlot QCPAbstractItem: https://www.qcustomplot.com/documentation/classQCPAbstractItem.html
- QCustomPlot QCPItemPosition: https://www.qcustomplot.com/documentation/classQCPItemPosition.html
- QCustomPlot QCPAbstractPlottable: https://www.qcustomplot.com/documentation/classQCPAbstractPlottable.html
- JKQTPlotter: https://github.com/jkriege2/JKQtPlotter (docs 404 from the tool; unverified)
- KD Chart: https://github.com/KDAB/KDChart (not fetched; unverified)

Non-Qt:
- matplotlib transforms: https://matplotlib.org/stable/users/explain/artists/transforms_tutorial.html
- matplotlib Artist API: https://matplotlib.org/stable/api/artist_api.html
- matplotlib legend guide: https://matplotlib.org/stable/users/explain/axes/legend_guide.html
- AOSA matplotlib chapter: https://aosabook.org/en/matplotlib.html (not fetched)
- Makie recipes: https://docs.makie.org/stable/explanations/recipes
- Makie scatter attributes (`space`, `markerspace`): https://docs.makie.org/stable/reference/plots/scatter
- ImPlot header: https://raw.githubusercontent.com/epezent/implot/master/implot.h ; internals: https://raw.githubusercontent.com/epezent/implot/master/implot_internal.h
- VisPy Visual source: https://github.com/vispy/vispy/blob/main/vispy/visuals/visual.py ; API index: https://vispy.org/api/vispy.visuals.html
- Bokeh: https://docs.bokeh.org/en/latest/docs/reference/models/renderers.html (index only; details unverified)
- Vega marks: https://vega.github.io/vega/docs/marks/
- Flutter CustomPainter: https://api.flutter.dev/flutter/rendering/CustomPainter-class.html
- Flutter PictureRecorder: https://api.flutter.dev/flutter/dart-ui/PictureRecorder-class.html
- Skia SkDrawable: https://api.skia.org/classSkDrawable.html
- Chromium cc/paint README: https://chromium.googlesource.com/chromium/src/+/HEAD/cc/paint/README.md
- Chromium DisplayItemList: https://chromium.googlesource.com/chromium/src/+/HEAD/cc/paint/display_item_list.h
- RenderingNG architecture: https://developer.chrome.com/docs/chromium/renderingng-architecture
- egui_plot PlotItem: https://docs.rs/egui_plot/latest/egui_plot/trait.PlotItem.html
- Plotters coord: https://docs.rs/plotters/latest/plotters/coord/index.html
- euclid: https://docs.rs/euclid/latest/euclid/
- Vello: https://github.com/linebender/vello

Not verified at all in this session (listed from memory in the text, marked [K]/[?]): Qt Quick Shapes renderer names, KDChart internals, JKQTPlotter internals, Bokeh `level` values and hit-test internals, ImPlot draw channel splitting, Qt Quick `markDirty` details, matplotlib zorder table (the fetched tutorial pages did not carry it), `QGraphicsScene::items` default order, Vega pick implementation, WebRender hit-test items, Skia Graphite recordings, Wasm support for Qt Canvas Painter, meaning of `pathGroup` in `QCanvasPainter::fill(path, rule, pathGroup)`.
