# Milestone 4j inventory — the extension point (spec step 11, D9)

State of branch `neoqcp-3.0` at `0c45d28`. Facts only, `file:line`. Format and unchanged
facts from `docs/plans/neoqcp-3/m4i-inventory.md` (its §5 lists the systems side at `081e1eb`).

Scope: spec `docs/specs/2026-11-09-neoqcp-3-coordinate-systems-design.md` §2.3, §2.4, §2.5,
§2.6, §2.7, §2.9, §4 step 11, §7 D9/D10 (D9: `:248-254`, step 11: `:225-227`).

---

## 1. What a third-party system must write today, end to end

### 1.1 The interface

`QCPCoordinateSystem` (`src/coordinate-system.h:54`) is abstract on three methods:

| method | where | note |
|---|---|---|
| `QCPTransform transform() const` | pure, `:74` | must return `QCPTransform::custom(...)` for a non-cartesian, non-polar frame |
| `QRect clipRect() const` | pure, `:76` | pixel rect the recording is clipped to; used by `QCPAbstractPlottable::clipRect()` (`src/plottables/plottable.cpp:1301-1304`) |
| `QCP::Capabilities capabilities() const` | pure, `:97` | no default; every system states its own |

Defaults a user system inherits:

- `QPainterPath clipPath() const` — `:78`, defined `coordinate-system.cpp:60`; adds `rect(clipRect())`.
- `bool clipsToPath() const` — `:84`, `coordinate-system.cpp:55` returns `false`. `QCPPolarSystem` returns `true` (`coordinate-system-polar.h:36`); this is the only override. It decides `clippedToFrame` for items (`src/drawable.cpp:511-545`) and makes `modelView` clip to the path instead of the rect.
- `bool contains(QPointF) const` — `:87`, `coordinate-system.cpp:71` returns `true`. `QCPCartesianSystem` overrides it (`coordinate-system.cpp:181-190`) with the axis-rect test plus `iSelectPlottablesBeyondAxisRect`; `QCPPolarSystem` at `coordinate-system-polar.h:37`.
- `QCPTransform iconTransform(const QCPDataBox&, const QRectF&) const` — `:89`, `coordinate-system.cpp:80` = `QCPTransform::fitted(box, rect)`.
- `QCPRange keyWindow() const` — `:91`, `coordinate-system.cpp:89` returns `{}` (unbounded).
- `QCPTransform::Kind kind() const` — `:95`, returns `QCPTransform::Kind::Custom`. The comment there says the item index asks it once per item; the only two other values are `Cartesian` (`:148`) and `Polar` (`coordinate-system-polar.h:39`).
- `qint64 revision() const` — `:100`, from `qcp::nextSystemRevision()` in the protected ctor (`coordinate-system.cpp:25-29`, process-wide atomic counter).
- `QList<QCPAbstractPlottable*> plottables() const` — `:108`, `coordinate-system.cpp:99-113`: scans `plot->plottable(i)` keeping `plottable->system() == this`; carries a `simplify:` note.
- `bool listens() const` — `:105`, `mChangedListeners > 0`; `connectNotify`/`disconnectNotify` (`coordinate-system.cpp:134-147`) count listeners of `changed()`.
- signal `void changed()` — `:112`.
- protected `bumpRevision()` — `:118`, `coordinate-system.cpp:120-128`: assigns a new revision character and, only when `listens()`, emits `changed()` through `qcpEmit(this, mArea ? mArea->parentPlot() : nullptr, ...)`.
- protected ctor `QCPCoordinateSystem(QCPArea*, QCPAxisBase* key, QCPAxisBase* value)` — `:115`; `Q_DISABLE_COPY` `:130`; `friend class QCPArea` `:129`.

A user system therefore subclasses the base, calls the protected ctor, and implements the three pure methods. It must also call `bumpRevision()` from every setter that changes what `transform()` returns — nothing else can: the base ctor seeds one revision and `QCPArea::update()` only bumps when the area's rect moves (`src/layoutelements/area.cpp:243-263`), `QCPArea::axisMappingChanged()` only when one of its axes reports a mapping change (`area.cpp:128-136`, fed by `QCPAxisBase::notifyMappingChanged`, `src/axis/axis-base.cpp:568` and its call sites `:62,:106,:236,:489,:555,:642`).

### 1.2 The mapping

`QCPCustomMapping` (`src/coords-custom.h:38-53`):

- pure `void map(span<const double> keys, span<const double> values, span<QPointF> out) const` — `:42-43`;
- virtual `void map(span<const QPointF>, span<QPointF>) const` — `:45`;
- virtual `QPointF mapPoint(QCP::Data) const` — `:47`;
- pure `QCP::Data inverse(QPointF pixel) const` — `:49`;
- virtual `bool equivalentTo(const QCPCustomMapping&, QPointF* delta) const` — `:51`, default `false`;
- subclasses that override one `map` overload need `using QCPCustomMapping::map` (`:33`).

`QCPTransform::custom(std::shared_ptr<const QCPCustomMapping>, quint64 revision)` — declared `src/coords.h:158`, defined `src/coords.cpp:158`; the variant member is `Custom { mapping, revision }` (`coords.h:288`); `Kind` enum at `coords.h:138-143`.

Facts that constrain a custom mapping:

- `QCPTransform::isTranslationOf` refuses a kind mismatch (`src/coords.cpp:590`): a custom frame never claims to be a pure translation of anything, so painters that would only scroll must re-record.
- `QCPTransform::cartesian()` returns non-null only for the `Cartesian` alternative (`coords.h:294`); every use of it is inside `coords.cpp` (`:411,:430,:542,:571,:592,:608,:644,:650`) — no library code dispatches on `cartesian()`/`polar()` outside the transform itself. There is no `polar()` accessor at all.
- No library code path constructs `QCPTransform::custom`; the only callers are `coords.cpp:158` itself and `tests/auto/test-transform/test-transform.cpp:246,:269`.

### 1.3 How a system reaches an area

`QCPArea::system(QCPAxisBase* key, QCPAxisBase* value)` (`src/layoutelements/area.h:91`, `area.cpp:76-109`) is the only factory. It prunes dead pointers, refuses null (`"a coordinate system needs two axes"`), refuses `key == value`, returns the existing system for the same ordered pair, requires both axes to be in `allAxes()` (`"both axes ... must belong to the area"`), then calls the area's `createSystem(key, value)` and appends the result to `mSystems`. It deliberately marks no layer dirty (`area.cpp:104-107`).

`createSystem` is protected pure virtual (`area.h:133`). Consequences:

- A user system cannot be produced by `QCPAxisRect` or `QCPPolarArea` as they stand: `QCPAxisRect::createSystem` refuses two axes of one orientation and always returns a `QCPCartesianSystem` (`src/layoutelements/layoutelement-axisrect.cpp:490-502`); `QCPPolarArea::createSystem` accepts only angular+radial (`src/polar/polar-area.cpp:123-133`).
- So the extension point is reached by subclassing an area, not by registering a system on an existing one. Subclassing `QCPArea` means implementing its two other pure virtuals, `allAxes()` (`area.h:82`) and `markAffectedLayersDirty(bool)` (`area.h:86`), plus `createSystem`; subclassing `QCPAxisRect` reuses those and overrides `createSystem` only.
- A `QCPArea` subclass also owns its ratio space: `virtual QCPRatioTransform ratioTransform() const` (`area.h:99`) defaults to an invalid transform (`area.cpp:180-183`); `QCPAxisRect` returns `QCPRatioTransform(this)` (`layoutelement-axisrect.cpp:357`) and `QCPPolarArea` a disc mapping (`polar/polar-area.cpp:201-207`). Without an override, every ratio position in that area resolves through the 2.x per-slot path (see 4i R-1).
- `clipPath()` / `clipsToPath()` (`area.h:101,:103`, `area.cpp:186-197`) are the area's own shape for items (`clippedToFrame`), independent of the system's `clipPath()`/`clipsToPath()`.

The bypass used by the tests: construct the system directly (its ctor can be public in the subclass) and hand it to a plottable with `QCPAbstractPlottable::setSystem` (`src/plottables/plottable.h:180`). `setSystem` (`plottable.cpp:812-840`) only checks `system->area()->parentPlot() == mParentPlot` and the capability mask; it never consults `area->systems()` or `createSystem`. An unregistered system is therefore invisible to `axis->plottables()` (`src/axis/axis-base.cpp:365-374`, which walks `mArea->systems()`), to `QCPArea::axisRemoved`, to `axisMappingChanged`, and to `~QCPArea`. `QCPAbstractPlottable` has no constructor taking a system: the 2.x `(keyAxis, valueAxis)` ctor resolves a system through `qcpSystemOfAxes` (`plottable.cpp:785-810`; `qcpSystemOfAxes` at `area.cpp:208-220`), so a custom-system plottable is always built on an axis pair and then re-parented with `setSystem`.

### 1.4 Ownership

- Systems are QObject children of the area (`area.h:122-126`), held as `QPointer` in `mSystems`.
- `~QCPArea` (`area.cpp:56-67`) deletes its systems after `detachFromSystem` (`area.cpp:50-54`, which calls `plottable->dropSystem()` for every `system->plottables()`). Comment at `area.cpp:48-49`: a plottable must not outlive its frame.
- `QCPArea::axisRemoved(const QCPAxisBase*)` (`area.cpp:143-160`) collects the systems using that axis, removes them from `mSystems`, detaches their plottables, deletes them.
- `QCPArea::pruneSystems` (`area.cpp:227-230`) drops null QPointers left by a direct `delete`.

### 1.5 Existing custom systems in tests

- `CountingSystem : QCPCoordinateSystem` — `tests/auto/test-coordinate-system/test-coordinate-system.cpp:43-72`. Public ctor forwarding to the protected base ctor, `transform()` returns a plain `QCPTransform(qobject_cast<const QCPAxis*>(keyAxis()), ...)` — *not* `QCPTransform::custom`, `clipRect()` = `static_cast<QCPAxisRect*>(area())->rect()`, `capabilities()` = `capRectangular|capStraightKey`, `iconTransform()` counts calls. Built directly on a rect (`:464-465`), never through `area->system()`. Covers: `theIconAsksTheSystem` (`:457-472`).
- `RecordingPlottable : QCPAbstractPlottable` — `:76-163`; draw modes polyline / ratioRect / mixedRect / coversItsClip / `readsTransform` (bakes `context.transform()` into the recording). `ArcPlottable : QCPAbstractPlottable1D<QCPGraphData>` — `:165-193`, `requiredCapabilities()` empty, records a data-space arc.
- `CountingMapping : QCPCustomMapping` — `tests/auto/test-transform/test-transform.cpp:56-72`, inverse = identity; used by `customMapsOncePerSpan` (`:~240`) and `customEqualityUsesTheRevision` (`:~270`).
- No test anywhere constructs a system whose `transform()` is `QCPTransform::custom`, and no test registers a system through `QCPArea::system()`.

---

## 2. Every switch on kind / cast to a concrete frame

`kind()` is read in exactly three places in `src/` (besides the three definitions):

1. `src/drawable.cpp:902` — `if (box && frame.transform.kind() == QCPTransform::Kind::Cartesian)`: the recording's index box is used to cull a hit test. Comment `:899-901`: a polar frame wraps its angle around a circle, so two box corners say nothing. **A `Custom` system is never culled: every command is scanned** (`:935-941`). Correct, slower; this is backlog item 2.
2. `src/items/item.cpp:480` — inside `QCPItemPosition::setType` (`:475-497`): the "framed fast path" (read the pixel once, switch both slots, write it back once) runs when `needsFrame()` **or** `mSystem->kind() != Cartesian` **or** the position keeps only an area (no axis rect). A `Custom` system takes the fast path: it round-trips through `pixelPosition()`/`setPixelPosition()`, i.e. through `transform().inverse()`.
3. `src/items/item.cpp:1860` — `QCPAbstractItem::positionsExtent` (`:1835-1900+`): a position bound to a system whose kind is not `Cartesian` returns `std::nullopt` before any box is built. **A `Custom` system's items are unindexed and therefore always candidates** (`layer.cpp:1277-1290` sets `mItemKind = 0`, `itemCandidates` initialises every slot to 1), even though the index itself is already system-keyed (`layer.h:157-166`, `layer.cpp:1296-1311`, `:1320-1335`).

Rest of the 4i cast list, refreshed (`grep` today):

| site | what it assumes | a `Custom` system gets |
|---|---|---|
| `drawable.cpp:549` | `axisRect = keyAxis ? keyAxis->axisRect() : qobject_cast<QCPAxisRect*>(system->area())` | null `axisRect` if its area is not a rect; the view is still framed by `system` (`:429-486`), so painting and items work |
| `items/item.cpp:381,444,942,1459` | `qobject_cast<QCPAxisRect*>` for `axisRect()`, `setArea`, `clipAxisRect()` | null axis rect; ratio/pixel positions fall back to `mArea` (`needsFrame`, `:427-435`) |
| `axis/axis.cpp:3020` | `QCPAxis::axisRect()` is `static_cast<QCPAxisRect*>(mArea)` | axes themselves only exist on rects, so untouched |
| `axis/axis.cpp:617-670` | `linkTo`/`unlink` compare `rect()`s | only reached through axes |
| `plottables/plottable.cpp:686` | `markLegendRows` recurses `QCPAxisRect::insetLayout()` | a legend inside a non-rect area is not found by this walk |
| `polar/polargraph.cpp:431` | `QCP_CHECK_OR_RETURN(!system || qobject_cast<QCPPolarSystem*>(system), ...)` | refuses a non-polar system outright (misuse) |
| `polar/polargraph.cpp:482` | `qobject_cast<QCPPolarArea*>(this->area())` | only reached on a polar system |
| `polar/angularaxis.h:172`, `polar/radialaxis.h:194` | `static_cast<QCPPolarArea*>(mArea)` | polar axes only |
| `coordinate-system-polar.cpp:39` | `static_cast<QCPPolarArea*>(area())` | polar system only |
| `core.cpp:2427,2472,2558` | `firstAxisRect`, `axisRects()`, layout-element walk: `QCPAxisRect` only | a custom area is invisible to `axisRect(i)`, `axisRects()`, `axisRectCount()` |
| `core.cpp:3861` | zoom-mode selection rect starts only over a `QCPAxisRect` | a custom area cannot start a rubber band zoom |
| `core.cpp:4725` | `processRectZoom` casts `areaAt(...)` to `QCPAxisRect` | a custom area is not zoomed by a rect |
| `core.cpp:4640-4656` | rubber band **selection** is already `QCPArea`-based (`areaAt`, `plottable->system()->area() != affectedArea`) | works for any area |
| `coordinate-system.cpp:170-188` | cartesian system reads `key->axisRect()->rect()` | cartesian only |

Items, positions, plottables, recorder and layer were made system-generic in 4i: `QCPDrawnView` carries `system` (`drawable.h:36-76`), `memoFrame` keys on system+revision (`drawable.cpp:402-486`), `QCPItemExtent` carries `system` (`drawable.h:68-75`), `QCPItemPosition` resolves through `mSystem->transform()` (`items/item.cpp:~810`, `:~980`).

---

## 3. Capabilities and the line plottables

### 3.1 The enum

`src/global.h:282-298`: `capRectangular = 0x01`, `capStraightKey = 0x02` (comment: "lanes and bar stacks need it"), `capColourScale = 0x04`, `capLanes = 0x08`; `Q_DECLARE_FLAGS(Capabilities, Capability)` `:298`; registered `:460-464`. **There is no straight-segment capability**: D9's new flag does not exist anywhere in the tree, and `capStraightKey` is a different thing (data sorted by key, which lanes and stacks rely on).

### 3.2 Who declares what

- Base default: `QCPAbstractPlottable::requiredCapabilities()` = `capRectangular | capStraightKey` (`src/plottables/plottable.h:186-189`).
- Overrides: `QCPAbstractColormapped` adds `capColourScale` (`plottable-colormapped.h:25-27`); `QCPAbstractIntervals` adds `capLanes` (`plottable-intervals.h:32-34`); `QCPMultiGraph` returns `QCPAbstractLine::requiredCapabilities() | capLanes` (`plottable-multigraph.h:40-42`, plus `capColourScale` inherited — 4g backlog); `QCPPolarGraph` returns nothing (`polar/polargraph.h:132`, `polar/polargraph.cpp:252`, "the polar system is enough").
- **`QCPGraph` has no override** (`plottable-graph.h:70` onward) → it demands `capRectangular|capStraightKey` today. **`QCPCurve` has no override** (`plottable-curve.h:218`) → same. `QCPBars` has none either (`plottable-bars.h`), so it currently demands exactly what a graph does.
- Systems: `QCPCartesianSystem::capabilities()` = all four (`coordinate-system.cpp:202-205`); `QCPPolarSystem::capabilities()` = none (`coordinate-system-polar.h:41`, definition in `coordinate-system-polar.cpp`).

### 3.3 Where capabilities are checked

- `QCPAbstractPlottable::setSystem` — `plottable.cpp:820-823`: `missing = requiredCapabilities() & ~system->capabilities()`, misuse `"the coordinate system lacks a capability this plottable needs"`, the call is abandoned (old system kept).
- `QCPPolarArea::add<T>` — `src/construct.h:251` and `:272` (the two forms, plottable and re-added plottable).
- Nowhere else: `QCPAxisRect::add<T>` (`construct.h:206-236`) relies on the ctor → `resolveAxes` → `setSystem`; `QCustomPlot::add<T>`/`take`/`adopt` do not re-check; item positions do **not** capability-check at all (`QCPItemPosition::setSystem`, `items/item.cpp:396-425`, refuses only another plot's system). The only capability test in item code is `capRectangular`, used to decide whether to keep a pixel across a type change (`items/item.cpp:525`, `:568`).

Tests asserting the masks: `test-coordinate-system.cpp:669-675` (per class), `:701` (`ArcPlottable` = {}), `test-polar.cpp:506`, `test-item-index.cpp:578`, `test-repaint.cpp:125`.

### 3.4 What `QCPGraph` assumes (2.x painter class, 2135 lines)

`QCPGraph : QCPAbstractPlottable1D<QCPGraphData>` (`plottable-graph.h:70`); overrides `draw(QCPPainter*)` (`h:180`, `cpp:456-535`) and `drawLegendIcon(QCPPainter*, QRectF)` (`h:181`, `cpp:537-599`); no `draw(QCPDrawContext&)`, so it never records.

- **Draw gate**: `draw` returns at once unless `mKeyAxis && mValueAxis` (`cpp:456-459`). Those projections are `qobject_cast<QCPAxis*>(system->keyAxis()/valueAxis())` (`plottable.cpp:830-831`). So a custom system whose axes are `QCPAxis`s keeps the graph drawing — but through `coordToPixel`, i.e. **as a plain rect graph, silently ignoring `transform()`**. A custom system whose axes are not `QCPAxis`s draws nothing.
- `getLines` (`cpp:601-640`) reads `mKeyAxis->rangeReversed()` and `orientation()`, and reverses the line when `rangeReversed != (orientation == Vertical)`: it assumes key pixels sort ascending along the key axis.
- `dataToLines` (`cpp:724-765`), `dataToStepLeft/Right/Center` (`cpp:768-940`): pixels from `coordToPixel`; the step styles are built by swapping x/y in key pixels (vertical-key case).
- `dataToImpulseLines` (`cpp:942-1014`): an impulse runs to `valueAxis->coordToPixel(0)` — **the value zero line**, not the frame's baseline.
- `drawFill` (`cpp:1016-1079`): segments from `getNonNanSegments(lines, keyAxis()->orientation())` (dereferences the key axis); channel fill asks `mChannelFillGraph->getLines()` and `->keyAxis()->orientation()`; GPU fill when `rhi && !exporting && solid brush` → `QCPLineExtruder::tessellateFillPolygon(getFillPolygon(...))` → `prl->addPlottable(fillVerts, {}, clipRect(), ...)`.
- `getFillBasePoint` (`cpp:1727-1776`): linear value axis → pixel of value 0 on the value axis; **log value axis → the key axis rect's right/left/top/bottom edge** (`keyAxis->axisRect()->...`). `getFillPolygon` (`cpp:1797-1806`).
- `getScatters` (`cpp:658-722`) maps with `coordToPixel` per orientation (not `coordsToPixels`, so it ignores the system); `drawScatterPlot` (`cpp:1081-1097`).
- Optimised paths: `getOptimizedLineData` (`cpp:1142-1280`) and `getOptimizedScatterData` (`cpp:1281`) decimate in pixel space from the axes' ranges; `getVisibleDataBounds` (`cpp:1520`); `drawLinePlot` → `drawPolyline` with a GPU stroke path (`plottable1d.h:589`).
- Hit test: `selectTest` (`cpp:419-440`) returns -1 without both axes and gates on `mKeyAxis->axisRect()->rect().contains(pos)` **or** `iSelectPlottablesBeyondAxisRect` — a raw rect test, never `system->contains`. The 1D fallback `QCPAbstractPlottable1D::selectTest` (`plottable1d.h:471-545`) is used only when the core records (`:475`), which a graph never does; its else branch needs both axes, uses `pixelsToCoords` around the click and both axis ranges.
- Ranges: `getKeyRange`/`getValueRange` (`cpp:443-455`) come from the data container — system-independent.
- `setChannelFillGraph` (`cpp:305-364`) checks plot and key-axis orientation (`:319-320`) but not capabilities.
- Legend: `drawLegendIcon(QPainter*, QRectF)` draws a band, a line across the rect and a scatter symbol at its centre — rect-shaped, transform-free, and **never routed through `system->iconTransform()`** (the legend calls exactly this overload, `layoutelement-legend.cpp:392`).

Wrong on a curved-but-straight-segment frame (a ternary or skewed frame built on axes): the line and scatter geometry (ignore `transform()`), step styles, impulse baselines, the fill base point and its log-axis rect-edge variant, channel fill pairing, pixel-space decimation, and the rect hit test would all be wrong or missing. Drawing at all depends on the system returning `QCPAxis` projections.

### 3.5 What `QCPCurve` assumes (1D recording class, 688 lines)

`QCPCurve : QCPAbstractPlottable1D<QCPCurveData, QCPAbstractLine>` (`plottable-curve.cpp:218`); records.

- `draw(QCPDrawContext&)` (`cpp:469-510`): returns unless `mKeyAxis && mValueAxis` and data is non-empty — the same projection gate, so a system without `QCPAxis` projections draws nothing; with them it records points **in data space** (`context.polyline`/`context.markers`, `cpp:541-591`), i.e. it would already follow a custom mapping once the gate lets it through.
- Fill is one data-space polygon of the whole curve; markers recorded in data space.
- `selectTest` (`cpp:417-439`): same `mKeyAxis->axisRect()->rect().contains(pos)` gate and the same missing-axes bail; the recording path is only reached through the base `QCPAbstractPlottable::selectTest`, which first asks `system->contains` (`plottable.cpp:566-584`).
- `getKeyRange`/`getValueRange` from data (`cpp:441-455`); `acceptsLineStyle` accepts `lsNone`/`lsLine` only (`cpp:458-467`); `drawLegendIcon(QCPDrawContext&)` (`cpp:593`) records a rect in data space and **does** go through `system->iconTransform()` (`drawable.cpp:1074-1112`).
- `pointDistance` (`cpp:642+`) maps per point with `coordsToPixels`, which falls back to `mSystem->transform()` when the projections are null (`plottable.cpp:940-963`).

### 3.6 Neighbours that share code

- `QCPGraph2` (`plottables/plottable-graph2.h:17`) is deprecated ("Use QCPMultiGraph with one column"), but still takes `(QCPAxis*, QCPAxis*)` and owns a data source + `QCPGraphPipeline` + cached lines + GPU geometry. `QCPMultiGraph` requires `capLanes` (`plottable-multigraph.h:40-42`). Both are `QCPAbstractLine` subclasses and share the lane/colour-scale requirements.
- `QCPBars` (recording, `draw(QCPDrawContext&)` `plottable-bars.h:266`, `cpp:1017`) builds bar width and stacking from `keyAxis()->orientation()`, `axisRect()->width()/height()` (`cpp:349`, `:375-383`) and pads its data range by `axisRect()->width()/height()*mWidth*0.5` (`cpp:1289-1296`): rect-only by construction.
- `QCPErrorBars` is a separate plottable with its own axes (`plottable-errorbar.h`; ctor `plottable-errorbar.cpp:134-135`), reading its companion through `interface1D()` (`cpp:381-442`, `:491-512`, `:823`, `:936-937`, `:1040`) and mapping with `dataPixelPosition()` → `coordsToPixels` (`plottable1d.h:371-374`). It never consults the companion's system and sets no capability requirement of its own; on a custom-system graph it would be placed by the *error bars'* axes.

---

## 4. The 4i backlog items, current code

1. **A user system's area ratio** — `QCPArea::ratioTransform()` defaults invalid (`area.cpp:180-183`); `QCPAxisRect` (`layoutelement-axisrect.cpp:357`) and `QCPPolarArea` (`polar-area.cpp:201-207`) override. An area subclass that does not override gets no ratio space, and `QCPItemPosition::needsFrame()` (`item.cpp:427-435`) sends ratio positions down the 2.x per-slot path.
2. **`QCPDrawnCore::hitTest` box gate** — `drawable.cpp:886-950`; the gate is `kind() == Cartesian` (`:902`). A `Custom` frame scans every command (`:935-941`), correct but O(commands).
3. **Item index cull** — `positionsExtent` bails at `item.cpp:1860` for a non-cartesian system; the group machinery is already system-keyed (`layer.h:157-166`; `layer.cpp:1296-1311`, `:1320-1335`, `:1373-1390` with a `simplify:` note naming a system-side `dataBoxOf(pixelRect)`). The query box is built by `dataBoxOfRect` (`layer.cpp:1106-1122`), axis-pair only. `test-item-index.cpp:482 anItemOnAPolarSystemIsAlwaysACandidate` pins the current behaviour.
4. **Tracer on a polar/user system** — `QCPItemTracer::setGraph(QCPGraph*)` (`items/item-tracer.h:107`, `cpp:181-216`): it sets `position->setType(ptPlotCoords)`, `position->setSystem(graph->system())` (`cpp:195-196`) and `addDependent`. `updatePosition` (`cpp:383-455`) reads only the graph's data container (`findBegin`, interpolate, `assignCoords`) — no axes, no pixels — so the frame is carried entirely by the position. The type is `QCPGraph*` (`h:131`), so no curve, multigraph or generic `1D` plottable can be followed.
5. **`area->add<Item>()`** — there is no `add` in `QCPArea` (`area.h`, no `add<` hits) and none in `QCPAxisRect` for items: `QCPAxisRect::add<T>` is constrained to `std::derived_from<T, QCPAbstractPlottable>` (`construct.h:206-236`). Items come only from `QCustomPlot::add<Item>` (`construct.h:121-137`), which registers into the `"annotations"` layer; a position then binds to an area through `setAxes`/`setSystem`/`setArea` (`items/item.h:185-190`).
6. **Areas moving between plots** — no API. `QCPLayout::adoptElement` (`layout.cpp:923-936`) sets layout/parent layerable/QObject parent and calls `initializeParentPlot` **only if the element has none**; `releaseElement` (`layout.cpp:947-959`) keeps the element in its old plot on purpose. There is no check anywhere that an element's plot matches the layout's (grep: only `area.cpp:214` compares two axes' plots). So a layout-to-layout move leaves the area, its axes and its systems in the old plot, and every `setSystem` from the new plot is refused by `plottable.cpp:818-819`/`item.cpp:400-401`.

---

## 5. Rescale and legend on a custom system

- `QCustomPlot::rescaleAxes` (`core.cpp:2976-2986`) enumerates `areas()` and calls `rescale` on every axis of `allAxes()` (already area-based, 4i).
- `plottables()` of an axis is "the systems of my area that use me" (`axis-base.cpp:365-374`).
- `QCPAxis::rescale` (`axis.cpp:1352-1379`) is the override actually used on cartesian axes: for each plottable it asks `getKeyRange`/`getValueRange` depending on `plottable->keyAxis() == this`. `QCPAxisBase::rescale` (`axis-base.cpp:383-405`) is the system-aware variant: it asks `plottable->system()->keyAxis() == this` for the side and reads `plottable->drawnDataRange()` through `qcpDrawnRange`. A custom system returning `QCPAxis` projections is therefore rescaled by the axes (data-space ranges, so correct); one returning non-axis bases is reached by nobody and rescales nothing — `test-coordinate-system.cpp:472 aNonCartesianPlottableRescalesQuietly` and `:942 rescaleAxesVisitsEveryArea` pin these two behaviours.
- Legend icon: `QCPPlottableLegendItem::draw` calls `mPlottable->drawLegendIcon(painter, iconRect)` with a painter and a rect (`layoutelement-legend.cpp:392`). `QCPAbstractPlottable::drawLegendIcon(QPainter*, rect)` (`plottable.cpp:654`) delegates to `mCore.paintIcon` (`drawable.cpp:1074-1112`), which records through `drawLegendIcon(QCPDrawContext&)` and frames the recording with `view.system->iconTransform(bounds, rect)` (`:1104-1108`, default `QCPTransform::fitted`, `coordinate-system.cpp:80`). A class that overrides the painter overload directly (`QCPGraph` `cpp:537`, `QCPCurve` does not) bypasses the system's icon transform entirely. Only the recording path honours `iconTransform`.
- `plottableAt` (`core.h:857-889`) gates on `currentPlottable->clipRect().contains(pos)` — the system's `clipRect()` — then `selectTest`, which asks `system->contains` (`plottable.cpp:566-584`).
- `itemAt` (`core.h:905-938`) gates on `clipToAxisRect()`/`clipRect()` and iterates all items (it does not use the index).
- Rect selection: `processRectSelection` (`core.cpp:4636-4694`) picks the area from `areaAt(topLeft)` and filters `plottable->system()->area() != affectedArea`; each plottable's `interface1D()->selectTestRect(rectF, true)` (`plottable1d.h:392-441`) converts the pixel rect through `pixelsToCoords` (→ `transform().inverse`) and needs both projections; a custom system without axis projections selects nothing.
- Rect zoom never starts on a non-rect area (`core.cpp:3853-3865`, `:4725`).

---

## 6. Lifetime paths the step-11 matrix needs

| case | code path | cartesian/polar tests to mirror |
|---|---|---|
| graph on the new system | `setSystem` capability gate `plottable.cpp:820`; draw gate `plottable-graph.cpp:456` | `test-coordinate-system.cpp:507 aPlottableTakesASystem`, `:527 twoConstructorsShareTheSystem`, `:542 movingBetweenSystemsReRecords` |
| scatter | same draw gate; `getScatters` `plottable-graph.cpp:658` | `test-qcpgraph.cpp` scatter tests; `test-polar` polar-graph scatter rows |
| item × take/adopt | `QCustomPlot::take(item)` clears positions' axes and axis rect (`core.cpp:1582-1605`); `add/adopt` rebinds plot+axes+axisRect+clip (`core.cpp:1612-1633`) | `test-item-system.cpp:240 takeClearsAndAdoptRebindsItemSystems`, `:471 takeAndAdoptAPolarItem`, `:642 aTakeClearsAOneAxisPosition` |
| area moved between plots | no API (§4.6); `layout.cpp:923-959` | none exist — `test-coordinate-system.cpp:593 crossPlotSystemIsRefused` is the nearest |
| axis deleted mid-drag | `QCPAxisRect::removeAxis` → `unlistAxis` → `plot->axisRemoved` → `QCPArea::axisRemoved` → `detachFromSystem` + `delete` (`layoutelement-axisrect.cpp:477-531`, `area.cpp:143-160`) | `test-coordinate-system.cpp:612 deletingAnAxisDeletesItsSystems`, `:571 aPlottableOfADeletedSystemIsInert`, `:812 aChangedSlotMayDeleteTheAxis`; `test-item-system.cpp:220 aDeletedAxisLeavesThePositionInert`; `test-item-index.cpp:518 aDeletedAxisDropsItsGroup` |
| system switched | `setSystem`/`systemWillChange`/`contentChanged` (`plottable.cpp:826-840`) | `test-coordinate-system.cpp:542 movingBetweenSystemsReRecords`, `:836 aMovedPlottableFollowsItsNewArea`, `:859 aTakenPlottableFollowsItsNewPlot`; `test-item-system.cpp:197 aSystemMisuseKeepsTheOldSystem`, `:573 aSetAxesAfterAPolarSystemTakesTheRectsArea`, `:594 aBindingAPositionToASystemMovesTheClipArea`, `:626 setSystemTwiceRetakesTheArea` |
| `QCPBars` refused | capability gate `plottable.cpp:820-823` reached from `QCPPolarArea::add` (`construct.h:251`,`:272`) | `test-coordinate-system.cpp:636-686 barsAreRefusedInAPolarSystem` (+`_data`), `test-qcpbars.cpp:489 aBarLeavingItsSystemLeavesItsStackAndGroup` |
| legend icon | `layoutelement-legend.cpp:392` → `plottable.cpp:654` → `drawable.cpp:1074-1112` | `test-coordinate-system.cpp:457 theIconAsksTheSystem` |
| rescale | `core.cpp:2976` → `axis.cpp:1352` / `axis-base.cpp:383` | `test-coordinate-system.cpp:472 aNonCartesianPlottableRescalesQuietly`, `:942 rescaleAxesVisitsEveryArea` |
| item culling on the new system | `item.cpp:1860` → unindexed | `test-item-index.cpp:482 anItemOnAPolarSystemIsAlwaysACandidate`, `:419 itemsOfTwoSystemsOfOneRectAreIndexedApart`, `:584 aRetargetedItemFollowsItsNewArea` |
| clipping to a non-rect area | `clipsToPath` (`drawable.cpp:511-545`) | `test-coordinate-system.cpp:427 aPolarPlottableIsClippedToItsDisc`; `test-item-system.cpp:371 aPolarItemClipsToTheDisc`, `:397 anUnclippedPolarItemIsNotClipped`, `:425 aPolarItemIsHittable` |

`test-item-index.cpp:584 aRetargetedItemFollowsItsNewArea` and `:610 aDataExtentDoesNotBuildATransform` pin the extent/index contract; `tests/auto/test-repaint` holds the recording-invalidation rows for items.

---

## 7. Docs to update

- `docs/axes.md:4` — "Every plottable is drawn in a coordinate system ... `plottable->system()` returns it." The natural home for the contract and the capability list.
- `docs/items.md:129` (binding a position to a system), `:113`, `:162`; `docs/polar.md:96`; `docs/migrating.md:162` (`ratioTransform` moved to `QCPArea`).
- `docs/concepts.md`, `docs/graphs.md`, `docs/layout.md` — one `setAxes`/`setAxisRect`-family hit each.
- Snippets: `docs/snippets/items.cpp:132,172,178`; `gallery.cpp:104,106,168,557,559`; `concepts.cpp:42`; `axes.cpp`; `interactions.cpp`; `polar.cpp` (4i noted the missing polar item snippet).
- Examples: `examples/plots/mainwindow.cpp:908,969,1001,1204-1219`, `examples/plots/mainwindow.h`, `examples/axis-tags/axistag.cpp:36-39,97`, `documentation/doc-image-generator/mainwindow.cpp` (18 hits).
- No page documents `QCPCustomMapping`, `QCPTransform::custom` or the `capabilities()` vocabulary yet.

---

## 8. Bench rows a capability or dispatch change can move

`tests/benchmark/benchmark.cpp`:

- Graph/curve/bars draw and data rows: `QCPGraph_Standard` `:179`, `QCPGraph_ManyPoints` `:206`, `QCPGraph_ManyLines` `:239`, `QCPGraph_ManyOffScreenLines` `:272`, the `QCPGraph_AddData*`/`RemoveData*` rows `:305-660`; `QCPGraph2_*` `:42-50`; `QCPBars_Zoom10k` `:2266`, `QCPBars_Pan10k` `:2286`.
- Recording/dispatch: `QCPDrawn_Zoom10k` `:2501`, `QCPDrawn_Pan10k`, `QCPDrawn_Pan10kOneRecording` (`:127-129`), contract-only `BenchDrawnBar` `:2443` / `BenchDrawnBarSet` `:2474`.
- Items/index: `QCPItems_*` declarations `:71-87` and bodies `:1203-1667`, `QCPItemText_Zoom1k` `:2308`, `QCPItems_MixedKinds_Pan1k` `:2729`, `QCPItems_Tracers_Pan1k` `:2809`, `QCPAxis_SetTickLength10kItems` `:1192`, helper `addLineItems` `:1168-1191`.

---

## Open questions

1. **Where a third-party system can live.** `createSystem` is pure and `QCPArea::system()` is the only factory, so the extension point is an area subclass; `QCPAxisRect`'s factory is fixed to cartesian. Does the documented contract ask users to subclass `QCPAxisRect` (rect ratio, rect clip, rect-only casts) or `QCPArea` (must then implement `allAxes`, `markAffectedLayersDirty`, and a ratio space)? D9's "user declares the capability" does not say which object declares it.
2. **The new capability's name and its relation to `capStraightKey`.** D9 describes "straight pixel line per data segment"; `capStraightKey` already exists with a different meaning ("lanes and bar stacks need it"), and `QCPBars` currently has no override, so it silently requires the same mask as a graph. Splitting them changes `QCPBars`' mask from inherited to explicit.
3. **Which system offers the new capability.** D9 says cartesian and user systems offer it and polar does not, but `QCPPolarGraph` already requires nothing and records a disc polyline; nothing in the code distinguishes "straight segment" from "curved frame" today.
4. **`QCPGraph`'s frame.** A custom system that returns `QCPAxis` projections keeps `mKeyAxis`/`mValueAxis` non-null, so `QCPGraph` draws in rect coordinates and ignores `transform()`, while a system without projections draws nothing. Neither is the D9 behaviour; the graph's pixel-space decimation, step styles, impulse baseline, fill base and rect hit test all read the axes directly (`plottable-graph.cpp:601-1776`), so the contract must say what a graph may assume about the frame it is given.
5. **Paint-time hit/cull agreement for a custom frame.** The hit cull is cartesian-only (`drawable.cpp:902`) and the item index drops non-cartesian extents (`item.cpp:1860`); both are "correct but slower" today. Whether 4j keeps them (and says so) or adds a system-side box query (`dataBoxOf(pixelRect)`, `layer.cpp:1377-1379`) is a design choice.
6. **Areas moving between plots.** No API exists and `adoptElement` keeps the old parent plot (`layout.cpp:923-936`); the step-11 matrix lists "area moved between plots", so either the matrix is satisfied by documenting the refusal or an API and its system/axis re-homing rules must be decided.
7. **`QCPArea::add<Item>()`.** No such form exists and items are plot-level with a layer; whether an area-scoped item factory is part of the extension point or dropped from the matrix is undecided.
8. **Ratio space for a user area.** `ratioTransform()` defaults invalid, `QCPAxisRect` and `QCPPolarArea` supply their own; must a user area supply one and its inverse, or inherit an axis rect's?
