# m4c inventory — colour maps, histograms and the colour-scale relation

Read-only survey, branch `neoqcp-3.0`. Every claim carries `file:line`; guesses are marked
**[guess]**. Nothing was built or run. Paths are relative to
`/home/jeandet/Documents/prog/NeoQCP-3.0`.

Baseline: `QCPColorMap` colorises its own scalar grid on the paint thread and draws a
`QImage` through `QCPPainter`. `QCPColorMap2` and `QCPHistogram2D` resample or bin on a worker,
then colorise on the paint thread and upload one RGBA texture through
`QCPColormapRenderer`/`QCPColormapRhiLayer`; the scalar grid never reaches the GPU. `QCPHistogram`
draws bars on the plottable GPU layer and maps values to colour only per column, by a palette.
`QCPColorScale` is a layout element the user creates and places; it is not owned by an axis rect
and no base class knows about it. The line family keeps its own `qcp::ColorScalarMapper`. The
drawing API records images as a painter-only command, and its GPU layer caps rasterised images at
four textures.

## 1. `QCPColorMap`

File: `src/plottables/plottable-colormap.h` (251 lines), `src/plottables/plottable-colormap.cpp`
(1334 lines).

### Data class

`QCPColorMapData` is declared in the same header, not in its own file (`plottable-colormap.h:39`).
It owns a dense `double* mData` of key size × value size and an optional `unsigned char* mAlpha`
(`plottable-colormap.h:95-96`). Getters: `keySize` h:49, `valueSize` h:51, `keyRange` h:53,
`valueRange` h:55, `dataBounds` h:57, `rawData` h:59-60, `data(k,v)` h:62, `cell` h:63, `alpha`
h:64, `isEmpty` h:84. Setters: `setSize` h:67, `setKeySize` h:68, `setValueSize` h:69, `setRange`
h:70, `setKeyRange` h:71, `setValueRange` h:72, `setData(k,v,z)` h:73, `setCell` h:74, `setAlpha`
h:75. Operations: `recalculateDataBounds` h:78, `clear` h:79, `clearAlpha` h:80, `fill` h:81,
`fillAlpha` h:82, `coordToCell` h:86, `cellToCoord` h:87. Implementation at `.cpp:206` (`setSize`),
`:260`, `:275`, `:290`, `:306`, `:322`, `:340`, `:372`, `:407`, `:434` (`recalculateDataBounds`),
`:468`, `:476`, `:489`, `:510`, `:537`, `:563`, `:593`.

`dataChanged()` `.cpp:772-777` sets `mDataModified` with `std::exchange` and calls
`mOwner->markDirty()` only on the transition from clean to dirty, so a full grid fill marks the
layer once. The owner is a `QPointer<QCPColorMap>` (`plottable-colormap.h:104`), so a data object
outliving its map does not crash.

### Building the image

`draw` rebuilds only when the data or the image generation moved:
`if (mMapData->mDataModified || mMapImageInvalidated)` (`plottable-colormap.cpp:1242-1243`).
`updateMapImage()` `.cpp:1113-1231` does the work:

| Step | Detail | Line |
| --- | --- | --- |
| format | `QImage::Format_ARGB32_Premultiplied` | `.cpp:1115` |
| supersampling | 1 when `interpolate`, else `int(1+100/size)` per axis | `.cpp:1124-1131` |
| orientation | image sized by the key axis orientation | `.cpp:1135-1144` |
| allocation failure | a 10×10 black image, no crash | `.cpp:1146-1151` |
| colouring | `QCPColorGradient::colorize(rawData, [rawAlpha], mDataRange, pixels, rowCount, …)` per row, scanlines inverted | `.cpp:1176-1215` |
| upscale | the supersampled image scaled with `Qt::FastTransformation` | `.cpp:1157-1170`, `.cpp:1217-1227` |
| flags | `mDataModified = false`, `mMapImageInvalidated = false` | `.cpp:1229-1230` |

Caching is a `QImage mMapImage` plus a scratch `mUndersampledMapImage` and a
`QPixmap mLegendIcon` (`plottable-colormap.h:186-188`). There is no separate dirty bit for the
clip or the axis; a pan or zoom does not rebuild the image, only `draw` recomputes the target
rectangle.

`draw` `.cpp:1234-1313` itself:

- bails on empty data or missing axes `.cpp:1236-1239`;
- applies the default antialiasing hint `.cpp:1240`;
- rebuilds as above `.cpp:1242-1243`;
- in a vectorised export (`pmVectorized`) renders into a `QPixmap` at pixel ratio 3
  `.cpp:1246-1262`;
- maps the two data-range corners with `coordsToPixels` `.cpp:1264-1267`, then grows the rectangle
  by half a cell on each side `.cpp:1270-1286`;
- mirrors according to `rangeReversed()` on each axis `.cpp:1287-1290`;
- `SmoothPixmapTransform = mInterpolate` `.cpp:1292`;
- with `tightBoundary`, intersects the clip with the exact data range `.cpp:1294-1302`;
- `painter->drawImage(imageRect, mMapImage.flipped(...))` `.cpp:1303`.

### Options

`setInterpolate` `.cpp:882-889` marks dirty and invalidates the image ("oversampling factors
might need to change"). `setTightBoundary` `.cpp:902-908` marks dirty but does **not** invalidate
the image: it only clips in `draw`. Defaults are `interpolate = true` and `tightBoundary = false`
`.cpp:753-756`.

### Legend icon, hit test, autoscale

`updateLegendIcon` `.cpp:999-1014` forces `updateMapImage()` when no image exists yet, mirrors on
reversed axes and scales the image to the thumbnail size. `drawLegendIcon` `.cpp:1316-1334` draws
the cached pixmap with `FastTransformation`, centred.

`selectTest` `.cpp:1017-1040` requires the point to be in the key axis rect (or
`iSelectPlottablesBeyondAxisRect`), then tests the data key/value ranges; a hit returns
`selectionTolerance()*0.99` and a one-range `QCPDataSelection` `.cpp:1030-1037`.

`getKeyRange` `.cpp:1043-1063` and `getValueRange` `.cpp:1066-1097` read
`mMapData->keyRange()/valueRange()` and apply the sign domain by trimming with `1e-3`; the value
range also refuses a key window with no overlap `.cpp:1085-1096`. `rescaleDataRange(bool)`
`.cpp:978-983` optionally calls `recalculateDataBounds()` and then `setDataRange(dataBounds())`.

### Colour-scale member and connections

One member: `QPointer<QCPColorScale> mColorScale` (`plottable-colormap.h:183`).
`setColorScale` `.cpp:925-956`:

1. `markDirty()` `.cpp:927`;
2. six disconnects from the old scale `.cpp:930-935` (`this`'s three setters → scale's three
   setters, and the scale's three signals → this's three slots);
3. stores the pointer `.cpp:937`;
4. with a new scale, copies gradient, range and scale type under `QPointer` guards
   `.cpp:941-948`;
5. six reconnects `.cpp:949-954`.

There is no signal when the scale is deleted, and `colorScale()` is a plain `QPointer::data()`
(`plottable-colormap.h:145-146`): the map silently detaches, which decision 14 forbids.
`grep friend class` finds no test friend in this header: the tests use the public API.

## 2. `QCPColorMap2` and `QCPHistogram2D`

Files: `src/plottables/plottable-colormap2.h` (243) / `.cpp` (528);
`src/plottables/plottable-histogram2d.h` (268) / `.cpp` (521).

### Data sources and pipelines

| | `QCPColorMap2` | `QCPHistogram2D` |
| --- | --- | --- |
| source type | `shared_ptr<QCPAbstractDataSource2D>` h:159 | `shared_ptr<QCPAbstractDataSource>` h:141 |
| setters | `setDataSource(unique_ptr)` h:32, `(shared_ptr)` h:33, `dataChanged()` h:35 | h:53-56 |
| convenience | `setData(x,y,z)` builds `QCPSoADataSource2D` h:38-44; `viewData` h:47-61 | `setData(keys,values)` builds `QCPSoADataSource` h:33-39; `viewData` h:40-51 |
| pipeline | `QCPColormapPipeline mPipeline` h:163; alias `QCPAsyncPipeline<QCPAbstractDataSource2D, QCPColorMapData>` `src/datasource/async-pipeline.h:260` | `QCPHistogramPipeline mPipeline` h:145; alias `QCPAsyncPipeline<QCPAbstractDataSource, QCPColorMapData>` `async-pipeline.h:261` |
| transform | resample, `ViewportDependent` `.cpp:66-128` | 2D bin, `ViewportIndependent` `.cpp:52-74` |
| result | a **scalar** `QCPColorMapData` grid, not an image | a **scalar** `QCPColorMapData` grid, not an image |

`QCPColorMap2::installResampleTransform` `.cpp:66-128` intersects the viewport with the data
extent `.cpp:89-94`, sizes the output to the plot in device pixels clamped 1..32768
`.cpp:105-114`, allows up to four times the pixel size in source columns `.cpp:116-118`, and calls
`qcp::algo2d::resample(...)` `.cpp:124-125`. The key axis is always sampled linearly
`.cpp:107-109`.

`QCPHistogram2D::installTransform` `.cpp:52-74` captures the bin counts and log flags and calls
`qcp::algo::bin2d(...)`; on failure it returns an empty `QCPColorMapData(0,0,...)` rather than null
so stale cells are dropped `.cpp:68-72`. `Normalization { nNone, nColumn }`
(`plottable-histogram2d.h:27`) is applied afterwards in `shown()` `.cpp:269-306`, lazily, into a
second grid.

Neither pipeline has viewport dependence for the histogram: a pan reuses the same bins, a zoom only
re-bins when the axis scale type changes (`followAxisScales` `.cpp:31-39`, `refreshBinning`
`.cpp:41-45`).

### `draw`

`QCPColorMap2::draw` `.cpp:346-438`:

1. bails without axes `.cpp:348-349`;
2. if a result exists but was resampled for another widget size, exports run synchronously and
   queue nothing, screen frames call `queueViewportChange()` `.cpp:351-372`;
3. no result: export runs `runSynchronously`, screen queues and returns `.cpp:373-392`;
4. if the recorded render range no longer overlaps the axes, `mRenderer.clearRhiContent()` and
   return `.cpp:401-413`;
5. `updateMapImage(resampledData)` when the renderer's image is invalid `.cpp:415-417`;
6. contours are updated **before** the image is drawn `.cpp:428-429`, labels after
   `.cpp:432`;
7. the axes' current ranges are recorded as the rendered range `.cpp:435-437`.

`QCPHistogram2D::draw` `.cpp:380-412` has the same shape: queue a re-bin when the axis scales are
stale, bin synchronously for an export or before the first frame, take `shown()`, rebuild the image
if invalid, then contours and image.

`drawLegendIcon` is a gradient swatch in both classes, not an image sample:
`plottable-colormap2.cpp:440-445`, `plottable-histogram2d.cpp:449-454`.

### Hit test, ranges, contours

| | `QCPColorMap2` | `QCPHistogram2D` |
| --- | --- | --- |
| key/value range | source `xRange`/`yRange` with sign domain `.cpp:447-466` | memoised `cachedFiniteBounds(...)` `.cpp:456-504` |
| `selectTest` | inside the source ranges → `selectionTolerance()*0.99` plus a `QCPDataSelection` `.cpp:468-493` | inside the bounds → 0, else -1, no details `.cpp:506-521` |

Contours are a shared `QCPContourOverlay mContours { mRenderer }`
(`plottable-colormap2.h:170`, `plottable-histogram2d.h:147`). `QCPContourOverlay` keeps lines and
labels in the image's UV space, marches at most 300 cells per dimension on screen and draws GPU
quads over the image, while exports get painter lines
(`src/painting/contour-overlay.h:22-34`, `:31-34`, `:53-57`). Its test hooks are `lastUv`,
`lastLevels`, `lastLabels`, `extractionCount` (`contour-overlay.h:59-64`), re-exported by the
plottables (`plottable-colormap2.h:114-126`).

Contour setters only reach the GPU as a colour: the pen width is a fixed hairline on screen and
the real pen is used only for the QPainter fallback (`plottable-colormap2.h:97-100`). All contour
setters end in `markDirty()` (`.cpp:495-528`, `plottable-histogram2d.cpp:414-447`).

### Colour-scale members and connections

Both classes keep no scale member of their own: the pointer lives in `QCPColormapRenderer`
(`src/painting/colormap-renderer.h:92`) and is only read through `mRenderer.colorScale()`
(`plottable-colormap2.h:69`, `plottable-histogram2d.h:87`).

`QCPColorMap2::setColorScale` `.cpp:187-219` and `QCPHistogram2D::setColorScale` `.cpp:179-211`
have the same shape as the classic map: `markDirty`, six disconnects from the old scale, store in
the renderer, then for a new scale copy gradient, scale type and range under `alive`/`live`
`QPointer` guards, then six reconnects. `QCPColormapRenderer::setColorScale`
`src/painting/colormap-renderer.cpp:68-71` only stores a `QPointer`.

Deletion is silent in both, exactly like the classic map; `test-colormap` pins the current
behaviour (`tests/auto/test-colormap/test-colormap.h:36-41`).

## 3. `QCPColormapRenderer` and the texture layer

Files: `src/painting/colormap-renderer.h` (98) / `.cpp` (260);
`src/painting/colormap-rhi-layer.h` (131) / `.cpp` (419).

### What is uploaded

An **RGBA `QImage`**, not a scalar grid and not a gradient lookup.
`QCPColormapRenderer::updateMapImage(const QCPColorMapData*)` `.cpp:78-101` builds
`QImage(keySize, valueSize, ARGB32_Premultiplied)`, colorises each row with
`mGradient.colorize(rawData + y*keySize, mDataRange, pixels, keySize, 1, isLog)` `.cpp:83-93`, and
clears the invalid flag `.cpp:100`. `draw` then hands the mirrored image to the layer:
`crl->setImage(flippedMapImage(flips))` `.cpp:122`, target rectangle `:123`, layer `:124`,
scissor `:131`.

### When it is re-uploaded

| Trigger | Path |
| --- | --- |
| data change | pipeline `finished` → `invalidateMapImage()` (`plottable-colormap2.cpp:31-36`), histogram `binsChanged()` (`plottable-histogram2d.cpp:262-267`) |
| gradient change | `QCPColormapRenderer::setGradient` `.cpp:36-44` |
| data range change | `setDataRange` `.cpp:46-56` |
| scale-type change | `setDataScaleType` `.cpp:58-66` |
| new widget size | `QCPColorMap2::draw` resamples, then rebuilds the image `.cpp:415-417` |
| pan | **not** re-uploaded: the quad moves by a pixel offset, see below |
| zoom | a colour map 2 resamples (new grid, new image); a histogram does not |

`QCPColormapRhiLayer::setImage` `.cpp:67-78` early-returns when the staged image has the same
`cacheKey()` and size, so an unchanged frame uploads nothing. `uploadQuad` `.cpp:309-323` uploads
only while `textureDirty`; `uploadResources` `.cpp:327-379` skips the quad when there is no
staging image.

### Shaders and passes

The layer owns two pipelines (`colormap-rhi-layer.h:112-118`): a textured-quad pipeline over
`composite_vert`/`composite_frag` with a `Nearest`/`ClampToEdge` sampler
(`.cpp:144-176`) and a `Lines` pipeline over `contour_line_vert`/`contour_line_frag` for contours
(`.cpp:180-213`). `render` `.cpp:397-419` draws the image quad, then the contour lines, then the
label overlay quad, all in the layer's paint buffer.

All colormap layers share the plot's composite uniform buffer: `core.cpp:3509-3514` calls
`ensurePipeline(renderPassDescriptor, sampleCount, mCompositeUbo)` and then `uploadResources(...,
mCompositeUbo)` for every registered layer. The quads are drawn inside the per-layer loop of the
compositor (`core.cpp:3637-3650`), so they layer with the plottables of their `QCPLayer`.

### How a pan is served

Two paths, both without a re-upload:

- a repaint: `QCPColormapRenderer::draw` runs again and only sets the new `setQuadRect(rect)`
  `.cpp:123`, which marks geometry dirty;
- a skipped repaint: the compositor shifts the quad by the layer's pixel offset
  `crl->setPixelOffset(offset)` (`core.cpp:2890-2905`, second site `core.cpp:3454-3463`), which
  translates both quads' geometry and marks the contour UBO dirty
  (`colormap-rhi-layer.cpp:100-109`).

`effectiveQuadRect` (`colormap-rhi-layer.h:31-34`) is the pixel rectangle plus that offset.
`ownerVisible()` (`colormap-rhi-layer.cpp:8-11`) keeps a hidden plottable from compositing a frozen
quad.

### QPainter fallback

`draw` `.cpp:118-153`: export (`pmNoCaching`) never takes the GPU branch; an image larger than
`QRhi::TextureSizeMax` (`fitsInTexture`, `.cpp:223-228`) clears any existing quad and forces the
painter path `.cpp:145-148`. The painter path is `painter->drawImage(rect, flippedMapImage(flips))`
plus `drawContourFallback` `.cpp:151-152`, which maps UV to pixels inside the same rectangle
`.cpp:205-220`.

## 4. `QCPHistogram` (1D)

Files: `src/plottables/plottable-histogram.h` (164) / `.cpp` (486).

Pipeline: `QCPHistogram1DPipeline mPipeline` (`plottable-histogram.h:83`), a
`ViewportIndependent` transform that calls `qcp::algo::bin1d` then `normalize`
`.cpp:111-132`. The result is a `qcp::algo::Histogram1D` (edges + per-column heights), taken under
generation control so an older job cannot replace a newer one (`onPipelineFinished` `.cpp:152-170`).

`draw` `.cpp:347-368` asks the plottable layer for a GPU target
(`qcp::gpuLayer(mParentPlot, mLayer, painter->isExporting())` `.cpp:352`) and, for each column,
builds bar rectangles and either `drawColumnOnGpu` or `paintColumn` `.cpp:355-366`.

`drawColumnOnGpu` `.cpp:399-416` appends fill quads with
`qcp::histogram::appendBarQuads(...)` and extrudes the outline with
`QCPLineExtruder::extrudePolyline(...)`, then calls
`layer->addPlottable(fill, stroke, clipRect(), dpr, rhiOutputSize().height(), 0, 0, opacity)`.
It passes zero pan offsets and rebuilds from the current axes every frame
`.cpp:398`, `.cpp:408-409`.

Legend icon `.cpp:431-443`: one rectangle per column in that column's fill/outline.
Hit test: `selectTest` always returns -1 `.cpp:481-486`; the histogram is not selectable in v1.

Does it map values to colours? Only per column, and not through a gradient: `columnLook`
`.cpp:370-380` takes `mColors` or `qcp::defaultPlottableColor(column)`, darkens it for the bar
outline and lightens the step fill. There is no `QCPColorScale`, no `QCPColorGradient`, and no
value-to-colour mapping anywhere in the class.

## 5. `QCPColorScale`

Files: `src/layoutelements/layoutelement-colorscale.h` (194) / `.cpp` (796).

Signals `layoutelement-colorscale.h:125-128`: `dataRangeChanged(QCPRange)`,
`dataScaleTypeChanged(QCPAxis::ScaleType)`, `gradientChanged(QCPColorGradient)`. Slots: the three
setters `setDataRange` h:110, `setDataScaleType` h:111, `setGradient` h:112 (declared `Q_SLOT`).
Property getters h:91-106, setters h:109-116, `colorMaps()` h:119, `rescaleDataRange(bool)` h:120,
`update(UpdatePhase)` h:123.

Internals: an internal `QCPAxisRect` subclass `QCPColorScaleAxisRectPrivate` (`h:40-63`) with its
own four axes, and one `QPointer<QCPAxis> mColorAxis` (`h:140`). `setType` `.cpp:202-250` moves the
range, label and ticker between axes and reconnects `rangeChanged` and `scaleTypeChanged`
`.cpp:246-247`, so the axis and the scale stay two-way synchronised. `setDataRange`
`.cpp:261-274` sets the axis range, marks the layer dirty and emits, all inside a `QCPEmissionScope`
`.cpp:265`. `setDataScaleType` `.cpp:295-309` sanitises the range for a log scale `.cpp:304-305`.
`setGradient` `.cpp:318-330` marks the gradient image invalid `.cpp:326-327` and emits.

The bar itself is drawn by `QCPColorScaleAxisRectPrivate::draw` `.cpp:672-692`: it lazily rebuilds
a gradient image (`updateGradientImage` `.cpp:699-738`, one colour per gradient level, replicated
across the bar) and paints it with `painter->drawImage` `.cpp:689-690`. Nothing goes to the GPU.

### `rescaleDataRange`

`.cpp:433-506`. It builds a union over **every plottable on the plot**, filtered by
`realVisibility()` when `onlyVisibleMaps` `.cpp:466-467`, and includes three kinds:

| Plottable | Test | Range source | Line |
| --- | --- | --- | --- |
| `QCPColorMap` | `qobject_cast` + `colorScale() == this` | `data()->dataBounds()` | `.cpp:470-474` |
| `QCPColorMap2` | same | `colorDataBounds(found, true)` (all data) | `.cpp:475-477` |
| `QCPHistogram2D` | same | `colorDataBounds(found)` | `.cpp:478-480` |

Sign-domain trimming and the zero-range widening happen in `include` `.cpp:440-460` and
`.cpp:484-505`. `colorMaps()` `.cpp:415-425` still returns `QCPColorMap` only, despite its name.
`QCPHistogram` (1D) and the line family are not seen.

### Placement and ownership

The scale is a `QCPLayoutElement` whose QObject parent is the plot (`h:68`, `.cpp:126-134`). It is
placed by the user through the plot's layout: `QCustomPlot::add<QCPColorScale>(row, column)`
(`src/construct.h:128-138`, handle in `layoutelement-colorscale.h:158-192`). No source file creates
one automatically, and `QCPAxisRect` has no colour-scale member or accessor (grep over
`layoutelement-axisrect.h` finds none). So today: one scale per user call, owned by the plot's
layout, not by a rect, not hidden until used, and not shared by the rect's plottables.

`update` `.cpp:518-558` sizes the element from `mBarWidth` plus the internal rect's margins
`.cpp:534-547` and lays the internal rect out over `rect()` `.cpp:552`. `calculateAutoMargin`
`.cpp:509-515` returns only the minimum margins: the comment explains that the base returns the
group's margin, which would stop a margin group from shrinking. `applyDefaultAntialiasingHint`
turns antialiasing off `.cpp:561-564`. Mouse and wheel events are forwarded to the internal rect
`.cpp:567-608`; `setRangeDrag`/`setRangeZoom` `.cpp:370-410` toggle those interactions.

### Deletion

The destructor deletes the internal axis rect `.cpp:148-151` and nothing else. Every user holds a
`QPointer` (`plottable-colormap.h:183`, `colormap-renderer.h:92`), and every connection is made
with `this` as context, so deleting the scale leaves the plottables alive and silently uncoloured:
`colorScale()` returns null with no signal (`plottable-colormap.h:145`,
`layoutelement-colorscale.cpp:120-125`, and the tests named `*_forgetsADeletedColorScale`,
`*_deletingTheScaleIsSafe` in `tests/auto/test-colormap/test-colormap.h:39-41`). Deleting a
plottable is safe: the scale's connections die with the plottable.

### `QCPColorGradient` change notification

There is none in the class. `src/colorgradient.h:32-187` is a `Q_GADGET` value type: setters
(`setLevelCount` h:149 … `setPeriodic` h:155) mutate fields, and the only invalidation is the lazy
`mColorBuffer`/`mColorBufferInvalidated` pair h:179-181. Callers compare with `operator==` h:131
and mark their own owner dirty (`plottable-colormap.cpp:863-873`,
`colormap-renderer.cpp:36-44`, `plottable-line.cpp:248-254`). A signal-based notification would be
a new design, not a wiring change.

## 6. The line family's colour-by-scalar

`qcp::ColorScalarMapper` (`src/plottables/plottable-color-mapper.h:14-94`) holds a shared
`const std::vector<double>` of per-point values h:88, a gradient h:89, a range h:90 and a scale type
h:91, and rebuilds a 256-entry `QRgb` LUT h:92 on any gradient change (`rebuildLut` h:79-86). The
drawing code asks `bucket(dataIndex)` h:39-50 and `color(bucket)` h:52-56. Non-finite values and,
on a log scale, non-positive values return `kGap = -1` h:17, h:44-48. Every setter bumps
`mGeneration` h:27-37; `toBucket` clamps the position before scaling h:70-78.

`QCPAbstractLine` API (`src/plottables/plottable-line.h:69-82`): `setColorValues` (shared and
by-value) h:73-74, `clearColorValues` h:75, `hasColorValues` h:76, `setColorGradient` h:77,
`colorGradient` h:78, `setColorRange` h:79, `colorRange` h:80, `setColorScaleType` h:81,
`colorScaleType` h:82. Implementation `plottable-line.cpp:194-276`: wrong length clears the
colouring and reports misuse `.cpp:208-213`, a pending data source stashes the values instead of
applying them `.cpp:214-218`, the first colouring calls `colorValuesStarted()` `.cpp:221-225`, and
the three property setters compare, `markDirty()` and forward `.cpp:248-274`.

**No `QCPColorScale` is involved.** The mapper lives in the plottable (`plottable-line.h:125`), the
range is whatever the user passed, and no axis rect or base class knows the line has colour values.
Decision 14 would move `gradient`/`range`/`scaleType` onto the shared relation; today they are four
independent setters on the line.

## 7. The drawing API's image support

`ImageCmd` (`src/painting/draw-commands.h:187-193`) is two `QCP::Position` corners plus a `QImage`.
It is one alternative of `QCPDrawCommand` h:202-205.

| Aspect | Today | Line |
| --- | --- | --- |
| kind | `CommandKind{false, true, false, false}`: painter-only, no GPU kind | `draw-commands.h:242-244` |
| `isGpuCommand` | false for `ImageCmd` | `draw-commands.h:260-270` |
| extent | box from the two resolved corners, cullable | `draw-commands.h:595-602` |
| painter replay | `painter.drawImage(normalized rect, cmd.image)` | `command-replay-painter.cpp:299-305` |
| RHI replay | no image branch: the whole command set is rasterised by the painter, then the resulting `QImage` is added as a textured quad | `command-replay-rhi.cpp:190-236` |
| cap | `QCPPlottableRhiLayer::kMaxImages = 4` per layer per frame; the overflow is painted by QPainter under the layer | `plottable-rhi-layer.h:91-93`, `command-replay-rhi.cpp:204-212` |
| over-cap whole-list rule | `rasterOverflows` sends the whole recording to the painter instead of splitting it | `command-replay-rhi.cpp:238-267` |
| texture count | one texture per distinct `QImage::cacheKey()` in a map, four entries referenced at a time | `plottable-rhi-layer.cpp:280-306`, `:561-575` |
| pan | the quad's offset is a uniform (`setAllOffsets`, `.cpp:141-166`); the texture is not re-uploaded | `plottable-rhi-layer.cpp:590-603` |

`QCPDrawContext::image(QCP::Position a, QCP::Position b, const QImage& image)`
(`src/painting/draw-context.h:221-222`) is the recording helper; there is no size, format or
texture-identity parameter.

What it would take for an image command to be a GPU texture that survives a pan:

1. a GPU `CommandKind` for `ImageCmd` (or a new `ImageTextureCmd`) so `isGpuCommand` is true and the
   RHI replay has a branch;
2. a draw-list-level image budget instead of `kMaxImages` per layer, because a colour map, a
   contour overlay and three rasters already reach a layer's four slots **[guess]**;
3. a stable identity for the texture across frames (the layer already keys by `cacheKey()`), plus a
   quad rectangle in the frame's pixel space that the pan offset can translate;
4. if the upload must be smaller than the current RGBA path, a scalar-grid command plus a gradient
   ramp texture or LUT uniform, which the current `composite_frag` does not sample
   (`colormap-rhi-layer.cpp:152-176`);
5. export must keep the painter replay, which already works
   (`command-replay-painter.cpp:299-305`).

## 8. Tests

| Class | File | Slots | What a port would break |
| --- | --- | --- | --- |
| `TestColorMap` | `tests/auto/test-colormap/test-colormap.h` | 41 | reads `cm->rhiLayer()->hasContent()/contourVertexCount()/hasOverlay()/overlayUploadPending()` (`.cpp:387,397,1047,1096-1118`), `lastContourUv()` (`.cpp:513,524,686,695`), `fallbackBuildCount()` (`.cpp:796-809`), plus colour-scale deletion and rescale slots |
| `TestPipeline` | `tests/auto/test-pipeline/test-pipeline.h` | 188 | `colormap2PipelineResample`, `colormap2ResamplesFor…` (12 slots) drive `pipeline()`; `histogram2dPipelineBins` and 20 `histogram2d*` slots; `colormapQuadOffsetTranslatesRect`, `colormapQuadFollowsPanWhileTranslating`, `colormap2TranslationOffsetWhenBusy` read the quad offset |
| `TestHistogramPlottable` | `tests/auto/test-histogram1d/test-histogram-plottable.h` | 83 | `barsAreDrawnOnTheGpu`, `gpuBarsHaveFillAndOutline`, `gpuQuadsOfBarsWithAGapStayInsideTheirBar`, `gpuBarsAreNotShifted`, `gpuStepOfOneColumnHasOnlyTheOutline`, `gpuAnAlmostTransparentFillStillDraws` inspect layer geometry |
| `TestHistogram1D` | `tests/auto/test-histogram1d/test-histogram1d.h` | 16 | pure binning of `qcp::algo::Histogram1D`, no plottable |
| `TestDataSource2D` | `tests/auto/test-datasource2d/test-datasource2d.h` | 46 | `colormap2Render`, `colormap2ColorScaleSync`, `colormap2Creation`, `colormap2ExportToPixmap/Pdf` exercise the public plottable |
| `TestColorByScalar` | `tests/auto/test-color-by-scalar/test-color-by-scalar.h` | 43 | `mapperBucketsLinearLogNaNAndGaps`, `mapperGenerationBumpsOnEverySetter`; the class is a friend (`plottable-line.h:133`) so private line state is reachable |
| `TestGradientPresets` | `tests/auto/test-gradient-presets/test-gradient-presets.h` | 4 | pure `QCPColorGradient` values |

No test friend exists in the colour-map, histogram, renderer, layer or colour-scale headers
(`grep friend class` finds only `QCPColorMap`, `QCustomPlot`, `QCPLegend`, `QCPColorMapData` and the
colour-scale's own private rect). The tests reach internals through deliberately public hooks:
`rhiLayer()` (`plottable-colormap2.h:85`), `pipeline()` (h:80-81), `lastContour*` (h:114-126),
`fallbackBuildCount()` (h:126), `stallPixelOffset()` (h:90), `hasRenderedRange()` (h:91).

## 9. Benchmarks

File: `tests/benchmark/benchmark.cpp` (2289 lines). Rows that time a colour map, colour map 2,
histogram 2D, histogram 1D or a colour scale:

| Row | Declaration | Body | What it does |
| --- | --- | --- | --- |
| `QCPColorMap_Standard` | `:51` | `:797-828` | 200×200 classic map with 50×50 NaNs, `interpolate=false`, one `replot()` per iteration: measures the whole paint path with a valid cached image |
| `QCPColorMap_ColorizeMap` | `:52` | `:830-862` | same map, but `setCell(0,0,0)` before each `replot()`: forces `updateMapImage`, so it measures colourisation + upload |
| `QCPColorMap_SetCell1M` | `:57` | `:987-999` | 1000×1000 cell-by-cell `setCell`: measures the per-cell dirty marking, no paint |
| `QCPColorMapData_ConstructLarge` | `:56` | `:976-984` | `new`/`delete` a 2000×2000 data object |
| `QCPColorMap2_ResampleDouble` | `:54` | `:893-899` | one 50000×1024 resample to 1500×800 |
| `QCPColorMap2_ResampleFloat` | `:55` | `:901-911` | same with a `float` Z span |
| `QCPColorMap2_ResampleWideSerial` | `:60` | `:934-953` | wide resample, serial |
| `QCPColorMap2_ResampleWideParallel` | `:61` | `:955-974` | wide resample, parallel |
| `QCPColorMap2_ResamplePanSequence` | `:58` | `:1001-1031` | 20 resamples at one size with only `xBegin/xEnd` moving: a pan of the resampler, not of the widget |
| `QCPColorMap2_ResamplePanSequenceSmall` | `:59` | `:1033-1063` | the same with a widget-sized target (800×400) |

There is **no** benchmark row for `QCPHistogram2D`, `QCPHistogram` (1D) or `QCPColorScale`, and no
row that paints a `QCPColorMap2` through a `QCustomPlot`.

Missing rows, in the naming of the existing pan/zoom gates
(`QCPGpu_MultiGraphPan1M` `:1790`, `QCPGpu_MultiGraphZoom1M` `:1813`, `QCPGpu_BarsPan10k` `:1916`,
`QCPDrawn_Pan10k` `:2253`):

- `QCPColorMap_Pan` and `QCPColorMap_Zoom`: a `replot()` per small axis move.
- `QCPColorMap2_Pan` and `QCPColorMap2_Zoom`: the full resample → colorise → upload path, not the
  bare `resample()` the existing rows time.
- `QCPHistogram2D_Pan` and `QCPHistogram2D_Zoom`.
- `QCPHistogram_Pan` and `QCPHistogram_Zoom`: the bar path already has `QCPGpu_BarsPan10k`, but no
  1D-histogram row.
- `QCPColorScale_Replot`: the gradient image and its `drawImage`, plus `rescaleDataRange` over N
  users.

## 10. Public API to keep

`QCPColorMap` (`plottable-colormap.h:129-173`): `data`, `dataRange`, `dataScaleType`,
`interpolate`, `tightBoundary`, `gradient`, `colorScale`; `setData(QCPColorMapData*, bool copy)`,
`setDataRange`, `setDataScaleType`, `setGradient`, `setInterpolate`, `setTightBoundary`,
`setColorScale`; `rescaleDataRange(bool)`, `updateLegendIcon(...)`; signals `dataRangeChanged`,
`dataScaleTypeChanged`, `gradientChanged`.

`QCPColorMapData` (h:39-107): the fourteen getters/setters listed in §1, `getKeyRange`/`getValueRange`
overrides live on the plottable (h:165-168).

`QCPColorMap2` (`plottable-colormap2.h`): `setDataSource` ×2, `dataSource`, `dataChanged`, `setData`
template, `viewData` ×2, `setGapThreshold`, `gapThreshold`, `gradient`, `colorScale`, `dataRange`,
`dataScaleType`, `setDataScaleType`, `setColorScale`, `setDataRange`, `setGradient`,
`rescaleDataRange`, `colorDataBounds`, `pipeline`, contour setters/getters (`setContourLevels`,
`contourLevels`, `setContourPen`, `contourPen`, `setContourLabelEnabled`,
`contourLabelEnabled`, `setAutoContourLevels`, `autoLevelCount`), `getKeyRange`, `getValueRange`;
signals `dataRangeChanged`, `gradientChanged`, `dataScaleTypeChanged` (h:138-141).

`QCPHistogram2D` (`plottable-histogram2d.h`): `setDataSource` ×2, `dataSource`, `dataChanged`,
`setData` template, `viewData` ×2, `setBins`, `keyBins`, `valueBins`, `setNormalization`,
`normalization`, `setKeyBinScale`, `setValueBinScale`, `keyBinScale`, `valueBinScale`,
`refreshBinning`, `gradient`, `dataRange`, `dataScaleType`, `setDataScaleType`, `colorScale`,
`setColorScale`, `setDataRange`, `setGradient`, `rescaleDataRange`, `colorDataBounds`, `pipeline`,
the same contour set, `getKeyRange`, `getValueRange`; the same three signals (h:123-126).

`QCPHistogram` (`plottable-histogram.h`): `setDataSource`, `setBinned`, `setBins`,
`setFixedRange`, `setNormalization`, `setStyle`, `setColumnColors`, `resetEdges`, `bins`,
`fixedRange`, `normalization`, `style`, `columnColors`, `histogram`, `pipeline`, `getKeyRange`,
`getValueRange`; signals `edgesChanged`, `histogramReady` (h:56-63).

`QCPColorScale` (`layoutelement-colorscale.h:91-128`): `axis`, `type`, `dataRange`,
`dataScaleType`, `gradient`, `label`, `barWidth`, `rangeDrag`, `rangeZoom`; `setType`,
`setDataRange`, `setDataScaleType`, `setGradient`, `setLabel`, `setBarWidth`, `setRangeDrag`,
`setRangeZoom`; `colorMaps`, `rescaleDataRange(bool)`; signals `dataRangeChanged`,
`dataScaleTypeChanged`, `gradientChanged`.

`QCPColormapRenderer` (`colormap-renderer.h:21-78`): `setGradient`, `gradient`, `setDataRange`,
`dataRange`, `setDataScaleType`, `dataScaleType`, `setColorScale`, `colorScale`,
`updateMapImage`, `draw`, static `imageRect`, `imageOnGpu`, `ensureRhiLayer`, `releaseRhiLayer`,
`mapImageInvalidated`, `invalidateMapImage`, `mapImage`, the contour and label-overlay setters,
`clearRhiContent`.

`QCPColorGradient` (`colorgradient.h:135-167`): every getter and setter, `colorize` ×2, `color`,
`loadPreset`, `clearColorStops`, `inverted`, `toLinearGradient`.

`qcp::ColorScalarMapper` (`plottable-color-mapper.h:21-56`): `hasValues`, `size`, `setValues`,
`clearValues`, `setGradient`, `gradient`, `setRange`, `range`, `setScaleType`, `scaleType`,
`generation`, `bucket`, `color`.

`QCPAbstractLine` colour API (`plottable-line.h:73-82`): `setColorValues` ×2, `clearColorValues`,
`hasColorValues`, `setColorGradient`, `colorGradient`, `setColorRange`, `colorRange`,
`setColorScaleType`, `colorScaleType`.
