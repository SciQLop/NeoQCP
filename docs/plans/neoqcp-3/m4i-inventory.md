# Inventory for milestone 4i (items on systems)

Facts only, at `file:line`, read at branch `neoqcp-3.0`, HEAD `081e1eb`. No edits, builds or tests were
run for this inventory. Nothing here is a proposal; the last section lists the facts that force a
design choice.

Spec under work: `docs/specs/2026-11-09-neoqcp-3-coordinate-systems-design.md` §2.3, §2.4, §2.7, §2.9
and §4 steps 10-11. Ruling in force: `docs/plans/neoqcp-3/milestone-4g-rulings.md` R-6 (a system's
`changed()` marks no layers).

---

## 1. `QCPItemAnchor` / `QCPItemPosition` today

### 1.1 `QCPItemAnchor` (`src/items/item.h:45-91`)

| Fact | Where |
| --- | --- |
| `Q_GADGET`; ctor `(QCustomPlot*, QCPAbstractItem*, name, anchorId = -1)` | `item.h:45-52` |
| `virtual QPointF pixelPosition() const`; the base delegates to `mParentItem->anchorPixelPosition(mAnchorId)` | `item.h:58`, `item.cpp:206` |
| Members: `mName`, `mParentPlot`, `mParentItem`, `mAnchorId`, `QSet<QCPItemPosition*> mChildrenX/mChildrenY` | `item.h:63-69` |
| `mutable QPointF mLastPixel` — "the pixel position last reported, kept because a dying item can no longer compute one" | `item.h:70` |
| `virtual QCPItemPosition* toQCPItemPosition()`; `detachChildren(bool keepPixelPosition)` | `item.h:77-78` |
| Friends `QCPItemPosition`, `QCustomPlot`, `QCPAbstractItem`, `QCPLayer`; `QCPLayer` "reads mParentItem to follow an anchor chain for repaint marking" | `item.h:82-87` |

An anchor names no axis, no axis rect and no system.

### 1.2 Every `QCPItemPosition` member that names an axis or an axis rect

| Member | Where |
| --- | --- |
| `QCPAxis* keyAxis() const` / `valueAxis() const`, over `QPointer<QCPAxis>` | `item.h:156-158`, `:186` |
| `QCPAxisRect* axisRect() const`, out of line, over `QPointer<QCPAxisRect>` | `item.h:160`, `item.cpp:396`, `item.h:187` |
| `void setAxes(QCPAxis*, QCPAxis*)` — no-op when unchanged, else `markDirty()` and assign both `QPointer`s; nothing else (no system, no revision, no notification) | `item.h:173`, `item.cpp:827-840` |
| `void setAxisRect(QCPAxisRect*)` — same shape | `item.h:174`, `item.cpp:842-859` |
| `enum PositionType` with per-coordinate type `typeX()`/`typeY()`, `setType/setTypeX/setTypeY` | `item.h:104-131`, `:138-142`, `:165-167`, `item.cpp:426-499` |
| `setCoords`, `setPixelPosition`, `set(QCP::Position)`, `asPosition(bool*)` | `item.h:171-181`, `item.cpp:697-1064` |
| `QPointer<QCPAxisRect> mClipAxisRect` and `mClipToAxisRect` live on the *item*, not the position | `item.h:313-314` |

`QCPAxis` here is the cartesian axis only. `QCPPolarAxisAngular : public QCPAxisBase`
(`src/polar/angularaxis.h:26`) and `QCPPolarAxisRadial : public QCPAxisBase`
(`src/polar/radialaxis.h:49`) are not `QCPAxis`, so `setAxes()` cannot be given a polar axis at all
today: items are cartesian-only by type.

### 1.3 `PositionType` (`item.h:104-131`)

`ptAbsolute`, `ptViewportRatio`, `ptAxisRectRatio` ("a fraction of the axis rect size (see
setAxisRect)"), `ptPlotCoords` ("a plot coordinate defined by two axes (see setAxes)"),
`ptAxisRectAbsolute` ("static positioning in pixels, starting from the top left corner of the axis
rect"). `Q_ENUM`. The type is per coordinate; the item-level helpers `type()` returns X's and
`setType()` sets both (`item.h:138`, `item.cpp:426-437`).

`setTypeX/setTypeY` save the current pixel and re-enter `setPixelPosition` to keep the pixel, except
when the target type's axes or rect are missing — the flag that avoids the warning path
(`item.cpp:439-499`).

### 1.4 How a position becomes pixels

Two independent routes exist.

**(a) Outside the drawing path — `QCPItemPosition::pixelPosition()`** (`item.cpp:729-820`):

- returns `QPointF()` when the position has no parent plot (`:731`);
- fast path (`:735-740`): types `(ptPlotCoords, ptAxisRectRatio)` or `(ptAxisRectRatio,
  ptPlotCoords)`, no parent anchor on either slot, and all of `mKeyAxis`, `mValueAxis`, `mAxisRect`
  present → `qcpResolvePosition(qcpMakeFrame(mKeyAxis, mValueAxis, mAxisRect, mAxisRect->rect()),
  asPosition(), false)`; the frame is built from the position's *own* pointers, not from the
  item/context frame, and `false` is the `report` flag (see 1.5);
- otherwise per-slot: `keyVertical` from `mKeyAxis->orientation()` (`:749`), a `crossing` case when
  the key is vertical and exactly one slot is a plot coordinate (`:750-752`), then three lambdas:
  `resolved` (reproduces the 2.x messages and returns 0.0, `:755-770`), `otherSlot` (non-plot slot,
  optional anchor shift, `:773-784`), `plotSlot` (`plotSlotAlongScreenAxis`, `:787-792`); assembly at
  `:794-811`; NaN clamped to 0 at `:815-818`.
- helpers: `slotAlongScreenAxis` (`item.cpp:56-83`, uses `QCustomPlot::viewport()` and
  `QCPAxisRect::width/height/left/top`, `nullopt` when a rect type has no rect),
  `plotSlotAlongScreenAxis` (`item.cpp:87-96`, `coordToPixel` on whichever axis runs along the screen
  axis).

`setPixelPosition` inverts the same arithmetic per type, `ptPlotCoords` through
`keyAxis->pixelToCoord` for the X slot / `valueAxis->pixelToCoord` for the Y slot, with the 2.x
qDebug message when neither axis exists (`item.cpp:861-1000`).

**(b) Inside the drawing path** — every item records (`item.h:325`, all twelve subclasses override
`draw(QCPDrawContext&)`):

- `QCPAbstractItem::draw(QCPPainter*) final` → `mCore.paint(*this, *painter, view())`
  (`item.h:325`, `item.cpp:1319`);
- `view()` → `qcpDrawnViewOf(*this, clipRect())` (`item.cpp:1325`);
- `qcpDrawnViewOf(item, clip)` (`drawable.cpp:484-500`): `keyAxis`/`valueAxis` come from
  `item.firstPosition()`, `axisRect = item.clipAxisRect()`, else `keyAxis->axisRect()`; comment
  `drawable.cpp:492`: "simplify: an item mixing axis pairs across its positions is drawn through the
  first pair; per-position frames when an item needs them"; `system` stays null ("a plottable's
  system; null for items, which read the axes", `drawable.h:49`);
- `memoFrame` (`drawable.cpp:432-461`): for a view without a system the frame is
  `qcpMakeFrame(keyAxis, valueAxis, axisRect, clip)` = `QCPDrawFrame{ QCPTransform(key, value),
  QCPRatioTransform(axisRect), clip }` (`painting/draw-context.cpp:387-392`);
- the replay resolves every `QCP::Position` through `qcpResolvePosition(frame, position)`
  (`painting/draw-commands.h:492-493`, `painting/command-replay-rhi.cpp:263-780`): `Data` →
  `frame.transform.map`, `AxisRatio` → `frame.ratio.map`, `AnchorOffset` → anchor pixel + offset,
  `DataXRatioY`/`RatioXDataY` → a mix of the two (`draw-context.cpp:349-387`).

Item code that draws through the context's transform: `item-line.cpp:34-46` (`pixelOf`),
`item-rect.cpp:131-159`, `item-ellipse.cpp:134-162`, `item-bracket.cpp:141-152`,
`item-curve.cpp:141-151`. Item code that calls `position->pixelPosition()` instead (own frame, not
the context's): `item-richtext.cpp:38,72,110`, `item-rspan.cpp:73-137` (hit test),
`item-rspan.cpp:139-183` (draw, with `recordEveryFrame()` when the corners are not all data,
`:156-157`).

**Polar support added in 4g: none on the item side.** `QCPTransform::polar` exists
(`coords.h:154`) and the polar system returns it (`coordinate-system-polar.cpp:52`), but no item path
builds one: `qcpMakeFrame` is the only frame factory an item can reach and it takes two `QCPAxis*`
and a `QCPAxisRect*` (`draw-context.cpp:387-392`, `draw-context.h:96`), `QCPRatioTransform` is
`QCPAxisRect`-typed (`coords.h:315-334`), and `QCPPolarSystem::ratioTransform` reports misuse and
returns an invalid one because "a polar area has no axis ratio: items and spans stay cartesian"
(`coordinate-system-polar.cpp:101-106`).

### 1.5 The 4g `report` flag

`qcpResolvePosition(frame, position, report = true)` (`painting/draw-commands.h:489-493`): "a
position asking for its own pixels before it has landed asks quietly". The misuse is
`reportRatioWithoutRect()`, static-once, "a position measured in axis ratios needs an axis rect: it
is not drawn" (`draw-context.cpp:337-347`), reported when `report` is set and `frame.ratio` is
invalid (`draw-context.cpp:349-387`). The only `false` call site in the library is
`item.cpp:740` (a position resolving itself); the replay and every command use the default `true`.

### 1.6 `asPosition` and its snapshot fallback

`item.cpp:1066-1097`: `Data` ← `ptPlotCoords|ptPlotCoords`; `AxisRatio` ←
`ptAxisRectRatio|ptAxisRectRatio`; `DataXRatioY` ← `ptPlotCoords|ptAxisRectRatio`; `RatioXDataY` ←
the reverse; `AnchorOffset`/`Pixels` for the `ptAbsolute` cases. **Every other 2.x combination**
(`ptViewportRatio`, `ptAxisRectAbsolute`, a single anchored slot, an anchored data or ratio slot)
falls back to `QCP::Pixels` of `pixelPosition()` with `*snapshot = true` (`item.cpp:1089-1092`,
documented `:1058-1064`); the recording then calls `recordEveryFrame()`
(`draw-context.h:219-225`).

### 1.7 Parent anchors

`setParentAnchor/setParentAnchorX/setParentAnchorY(parent, keepPixelPosition = false)`
(`item.h:168-170`, `item.cpp:551-690`): add/remove the child in the anchor's `mChildrenX/Y`, refuse a
cycle through `parentChainReachesThis` (`item.cpp:502-549`), optionally re-map the coords so the pixel
is kept, and `setParentAnchor(nullptr, false)` also turns `ptPlotCoords` into `ptAbsolute`
(`item.cpp:44-50` documents the reason). `detachChildren(bool keepPixelPosition)` is called from
`~QCPItemPosition`/`~QCPAbstractItem` paths (`item.h:77`, `item.cpp:1298-1308`). Anchor pixels are the
parent item's: `QCPItemAnchor::pixelPosition()` delegates to
`mParentItem->anchorPixelPosition(id)` (`item.cpp:206`), which subclasses compute from their own
positions (`item-rspan.cpp:185-200` reads `leftEdge->keyAxis()/valueAxis()` only as a guard).

Repaint follow-through uses anchors, not axes: `QCPAbstractItem::dependencyRevision()` hashes the
parent anchors' `mCore.generation()` and returns 0 when `mAnchoredPositions == 0`
(`item.cpp:1347-1364`), and `markFollowersDirty()` marks every item whose position is anchored on
this one (`item.cpp:1650-1663`).

---

## 2. Item classes and what they read from axes / axis rects

Sources in `src/items/`. Header sizes: `item-bracket` 158, `item-curve` 142, `item-ellipse` 155,
`item-line` 116, `item-pixmap` 168, `item-rect` 149, `item-richtext` 66, `item-rspan` 111,
`item-span` 106, `item-spanbase` 60, `item-straightline` 109, `item-text` 248, `item-tracer` 211.

**No item subclass header mentions `QCPAxis`, `QCPAxisRect`, `keyAxis`, `valueAxis`, `axisRect`,
`coordToPixel` or clip.** The only direct axis reads in the item sources are:

| Site | Read |
| --- | --- |
| `item-rspan.cpp:187-189` | `leftEdge->keyAxis()`, `valueAxis()`, null guard only |
| `item-tracer.cpp:193` | `position->setAxes(graph->keyAxis(), graph->valueAxis())` — the only place an item takes a plottable's axes |

| Class | Positions / anchors | `draw(QCPDrawContext&)` | `hitTest` | `dataExtent` | Draw geometry |
| --- | --- | --- | --- | --- | --- |
| `QCPItemLine` | 2 / 0 (`item-line.cpp`) | `item-line.cpp:71` | `mCore` | `positionsExtent` (`item-line.cpp:190-196`) | `pixelOf` on the context transform (`:34-46`) |
| `QCPItemStraightLine` | 2 / 0 | `item-straightline.cpp:63` | `mCore` | `mCore` | line through two pixels |
| `QCPItemCurve` | 4 (`start`, `startDir`, `end`, `endDir`) | `item-curve.cpp:72` | `mCore` | `positionsExtent` (`:198-204`) | context transform (`:141-151`) |
| `QCPItemRect` | 2 / 6 | `item-rect.cpp:71` | `mCore` | `positionsExtent` (`:208-211`) | context transform (`:131-159`) |
| `QCPItemEllipse` | 2 / 9 | `item-ellipse.cpp:71` | `mCore` | `positionsExtent` (`:217-220`) | context transform (`:134-162`) |
| `QCPItemPixmap` | 2 / 6 | `item-pixmap.cpp:83` | `mCore` | `mCore` | two corner positions |
| `QCPItemText` | 1 / 0 (plus auto anchors) | `item-text.cpp:152` | override (`item-text.cpp` header decl `item-text.h:153`) | `mCore` | position + glyph box |
| `QCPItemBracket` | 2 / 1 (`center`) | `item-bracket.cpp:89` | `mCore` | `positionsExtent` (`:256-260`) | context transform (`:141-152`) |
| `QCPItemTracer` | 1 / 0 | `item-tracer.cpp:112` | `mCore` | `mCore` | position, optionally a graph's axes |
| `QCPItemSpan` | 4 edges (via spanbase) | `item-span.cpp:42` | override (`item-span.h:43`) | `mCore` | `asPosition` → data or pixel corners, `recordEveryFrame` when not data |
| `QCPItemRSpan` | 4 edges | `item-rspan.cpp:34` | override (`item-rspan.h:35`) | `mCore` | same shape, `item-rspan.cpp:139-183` |
| `QCPItemRichText` | 2 (`topLeft`, `bottomRight`) | `item-richtext.cpp:34` | override (`item-richtext.h:35`) | `mCore` | `position->pixelPosition()`, `transform.mapRect` for the clip intersection (`:86`) |

What the item reads from the axis rect is entirely in the base class:

- clipping: `clipRect()` is `mClipAxisRect->rect()` when `mClipToAxisRect` and the rect is set, else
  `mParentPlot->viewport()` (`item.cpp:1531-1545`); `setClipToAxisRect` also makes the rect the
  parent layerable (`item.cpp:1388-1402`, `:1404-1420`);
- the recording frame's ratio transform: `qcpDrawnViewOf(item)` takes `axisRect` from
  `clipAxisRect()` or `keyAxis->axisRect()` (`drawable.cpp:484-500`);
- extents: `positionsExtent(pixelPad, positions)` returns `nullopt` when a position has no
  key/value axis, or when **any position's axis pair differs from the first's** (`item.cpp:1665-1681`,
  condition `:1670-1676`), builds the box from unanchored `ptPlotCoords` sides only, leaves a side
  unbounded on a log axis with a non-positive lower bound (`:1704-1707`), and returns
  `QCPItemExtent{keyAxis, valueAxis, box, pixelPad}` (`:1720`); the five overriding classes pass
  `positionsExtent(pad, positions())` (table above);
- `createPosition(name)` gives every new position `setAxes(mParentPlot->xAxis, mParentPlot->yAxis)`,
  `setType(ptPlotCoords)`, `setAxisRect(mParentPlot->axisRect())` and `setCoords(0, 0)`
  (`item.cpp:1723-1755`); `createAnchor` at `:1757`;
- `QCPAbstractItem::hitTest`, `selectTest`, `drawnDataRange` and `dataExtent` are the recording's
  (`mCore`) unless overridden (`item.cpp:1331-1386`).

`QCPItemExtent` is `{ QCPAxis* keyAxis; QCPAxis* valueAxis; QCPDataBox box; double pixelPad; }`
(`drawable.h:68-74`) — the axis pair is part of the type.

---

## 3. The item index

| Fact | Where |
| --- | --- |
| Purpose/comment: "one packed R-tree per (key axis, value axis) pair, in data coordinates so that it survives range changes. Kinds are 0 unindexed, 1 indexed, 2 side (extent changed since the build: tested one by one)" | `layer.h:146-152` |
| `struct ItemIndexGroup { QPointer<QCPAxis> keyAxis; QPointer<QCPAxis> valueAxis; QCPSpatialIndex index; double maxPad; }`, with "removeAxis() deletes an axis and this group outlives it until the next rebuild. A dead axis makes the group unusable, and its items are then simply not culled" | `layer.h:154-162` |
| State: `mItemGroups`, `mItemSide`, `mItemIndexDirty = true`, `mItemIndexMembershipChanged`, `mItemIndexBuilds`, `mItemAxes`, `mItemAxesDirty` | `layer.h:164-185` |
| `itemCandidates(const QRectF& pixelArea)` returns one byte per child (0 = indexed and outside, 1 = look at it) | `layer.h:94` |
| Built: `rebuildItemIndex()` assigns each child a kind/group, compares `groups[g].keyAxis == extent->keyAxis && groups[g].valueAxis == extent->valueAxis` | `layer.cpp:1233-1318`, compare at `:1265-1270` |
| Per-item cached extent: `mIndexExtent` / `mIndexExtentValid`, written at rebuild and compared to decide `moved` | `item.h:341-342`, `layer.cpp:1260-1288` |
| Membership/axes: `collectItemAxes(item, axes)` walks the item's positions plus the parent-anchor chain of other items (cycle-safe) collecting `position->keyAxis()`/`valueAxis()`; `rebuildItemAxes`, `noteItemAxes`, `itemsUseAxis(axis)` | `layer.cpp:1159-1206` |
| Invalidated: `childrenChanged()` (dirty + membership + axes), `invalidateItemIndex()` | `layer.cpp:1121-1132` |
| Extent change: `noteItemExtentChanged(item)` moves the slot from kind 1 to kind 2 and grows `mItemSide`; Bentley-Saxe threshold `mItemSide.size() > max(64, children/16)` sets the dirty flag | `layer.cpp:1208-1231` |
| Scale-type change: `QCPAxis::scaleTypeWillChange` invalidates every layer's index | `axis/axis.cpp:3028-3035` |
| `setItemIndexing(bool)` on/off invalidates all layers; default `true` | `core.cpp:815-823`, `core.h:330-331`, `:591` |

Query sites:

| Query | Where |
| --- | --- |
| Click/double-click/wheel hit rule (`hitsAt`): per layer, one `itemCandidates` for the click point inflated by `selectionTolerance`, non-candidates skipped before `selectTest` | `core.cpp:4927-4958`, index use `:4938-4946` |
| Layer draw: `itemCandidates(QRectF(viewport()))` before painting, non-candidates skipped | `layer.cpp:211-247`, `:216-225` |
| `QCPAxisRect::markItemsUsingAxisDirty(axis)` dirties every layer whose `itemsUseAxis(axis)` | `layoutelement-axisrect.cpp:547-556`, called from `:544`, `axis.cpp:3051`, `:3082` |
| `QCustomPlot::itemAt<T>()` does **not** use the index: it scans `mItems` and calls `selectTest` on each (after a `clipToAxisRect()` fast reject) | `core.h:905-938` |
| Rubber-band selection selects plottables only, and already by area: `areaAt(rect.topLeft())` + `plottable->system()->area() == affectedArea` | `core.cpp:4636-4694`, `:4640-4648` |
| There is no damage tracking anywhere: layers are dirtied wholesale; "damage" appears only in the `clipRect()` doc comment | `coordinate-system.h:75`, `layer.cpp:211-247` |

No `QCPCoordinateSystem*` appears in `src/items/*`, `src/layer.{h,cpp}` (grep "system" there returns
only prose about "coordinate system" and "layering system").

---

## 4. Item lifetime against axes, areas and plots

| Event | What happens to items today | Where |
| --- | --- | --- |
| `QCPAxisRect::removeAxis(axis)` | `unlistAxis()`: unlists, (if alive) `markLayersUsingAxisDirty(axis)`, `plot->axisRemoved(axis)`, then `QCPArea::axisRemoved(axis)` (which deletes the systems using it), then `delete axis` | `layoutelement-axisrect.cpp:477-531` (`:523-527`) |
| The item itself | not touched. Its positions keep `QPointer<QCPAxis>`/`QPointer<QCPAxisRect>` which go null; `pixelPosition()` then resolves 0.0 through the missing-axis messages, and the recording is refused once with "NeoQCP: a drawn object has no key or value axis; it is not drawn" | `item.cpp:749-770`, `drawable.cpp:699-717`, `:708` |
| The item index | the group keeps a `QPointer` to the dead axis; `itemCandidates` then leaves that group's items unculled until the next rebuild | `layer.h:154-162` |
| `QCPAxisRect` removal from the layout (rect deleted) | axes deleted → same as above for the axis pointers; `mClipAxisRect` nulls; positions of a ratio type report "no axis rect was defined" | `tests/auto/test-qcpaxisrect/test-qcpaxisrect.cpp:242-277` |
| Polar area deleted | `~QCPPolarArea` removes the radial axes (each `~QCPPolarAxisRadial` calls `polarArea()->axisRemoved(this)` while it is still whole) and deletes the angular axis (`~QCPPolarAxisAngular` does the same) | `polar/polar-area.cpp:72-96`, `polar/radialaxis.cpp:184-192`, `polar/angularaxis.cpp:140-147` |
| `QCustomPlot::take(handle)` on an item | `detach(item)`: cuts anchor links to items elsewhere, unlists, `mRegisteredPlot = nullptr`, `setClipAxisRect(nullptr)`, leaves the layer, `setParentLayerable(nullptr)`, and **every position gets `setAxes(nullptr, nullptr)` + `setAxisRect(nullptr)`**; the tracer's graph is cleared; anchors lose `mParentPlot` | `core.cpp:1582-1605` |
| `QCustomPlot::add(std::unique_ptr)` / `adopt` of an item | requires no parent plot and an existing `axisRect()`; sets the plot, `position->setAxes(xAxis, yAxis)` + `setAxisRect(axisRect())`, registers, clips to `axisRect()` | `core.cpp:1612-1633` |
| Areas moving between plots | not supported: `QCPLayout::adoptElement` only calls `initializeParentPlot` when the element has none, and `releaseElement` says "Don't initializeParentPlot(0) here, because layout element will stay in same parent plot" | `layout.cpp:926-939`, `:951-963` |
| Item construction | registered in the base constructor via `registerItem`; items go to the "annotations" layer when it exists | `item.cpp:1283`, `core.cpp:4863-4896` |
| Item destruction | `~QCPAbstractItem` detaches anchors, calls `mRegisteredPlot->itemDestroyed(this)` (which removes it from `mItems` and bumps `mItemsEpoch`), deletes the anchors | `item.cpp:1298-1308`, `core.cpp:4845-4852` |

Existing tests covering that ground:

`tests/auto/test-qcpaxisrect/test-qcpaxisrect.cpp`:
`axisRemovalConsequencesToItems` (:176), `axisRectRemovalConsequencesToItems` (:242),
`axisRectRemovalConsequencesToPlottables` (:211).
`tests/auto/test-ownership/test-ownership.cpp`: `deleteItemLeavesList` (:317), `deleteLaterItem`
(:330), `removeItemFromDestroyedSlot` (:342), `clearItemsWhileASlotDeletesAnother` (:354),
`clearItemsKeepsOtherLayersOrder` (:370), `clearItemsSlotDeletesAnother` (:399),
`itemOrderAfterRemove` (:415), `deleteParentAnchorItem` (:455),
`deleteAxisLeavesRectAndShortcuts` (:472), `removeAxisHidesItsPlottables` (:664),
`deleteTracedGraph` (:678), `deletePolarAreaWithGraphs` (:934).
`tests/auto/test-item-position/test-item-position.cpp`: `textAnchorParentDeletedKeepsPlace` (:135),
`parentDeletedBeforeAnyFrameKeepsPlace` (:217), `axisRectAbsoluteFollowsAxisRectOnResize` (:52),
`oneAxisAnchoredTextFollowsItsAnchor` (:161), `axisRectAbsoluteTextFollowsTheAxisRect` (:192),
`mixedPlotCoordinatePairsKeepTheKeyAndValueRole` (:297).
`tests/auto/test-polar/test-polar.cpp`: `deletingTheRadialAxisLeavesTheGraphInert` (:1215),
`deletingTheAreaDoesNotDeleteTheGraph` (:1263), `deletingTheAreaMidDragIsSafe` (:213),
`deletingTheAngularAxisMidDragIsSafe` (:1513).
`tests/auto/test-coordinate-system/test-coordinate-system.cpp`: `deletingAnAxisDeletesItsSystems`
(:598), `aPlottableOfADeletedSystemIsInert` (:557).

No test covers an item losing its system, because no item has one.

---

## 5. The systems side, and every place a custom system falls through

### 5.1 `QCPCoordinateSystem` public/protected API (`src/coordinate-system.h`)

| Member | Line |
| --- | --- |
| `class QCP_LIB_DECL QCPCoordinateSystem : public QObject` | `:54` |
| `QCPArea* area()`, `QCPAxisBase* keyAxis()`, `valueAxis()`, `bool usesAxis(const QCPAxisBase*)` | `:62-71` |
| `virtual QCPTransform transform() const = 0` | `:74` |
| `virtual QRect clipRect() const = 0` — "the pixels this system draws in: the buffers, the damage and the GPU scissor use it" | `:76` |
| `virtual QPainterPath clipPath() const` (the rect by default) | `:78` |
| `virtual bool clipsToPath() const` | `:84` |
| `virtual bool contains(QPointF) const` | `:87` |
| `virtual QCPTransform iconTransform(const QCPDataBox&, const QRectF&) const` | `:89` |
| `virtual QCPKeyWindow keyWindow() const` | `:91` |
| `virtual QCPRatioTransform ratioTransform() const = 0` | `:93` |
| `QCPRatioTransform frameRatioTransform() const` (non-virtual): `ratioTransform()` only when `capabilities().testFlag(QCP::capRectangular)`, else an empty one | `:96-100` |
| `virtual QCP::Capabilities capabilities() const = 0` | `:102` |
| `quint64 revision() const`, `bool listens() const`, `QList<QCPAbstractPlottable*> plottables()` | `:105`, `:110`, `:113` |
| `signals: void changed()` | `:117` |
| `protected: QCPCoordinateSystem(QCPArea*, QCPAxisBase*, QCPAxisBase*)`, `void bumpRevision()` | `:120-123` |
| `connectNotify`/`disconnectNotify` keep `mChangedListeners` | `:124-125`, `coordinate-system.cpp:130-153` |
| Members `mArea`, `mKeyAxis`, `mValueAxis`, `mRevision = qcp::nextSystemRevision()`, `mChangedListeners`; `friend class QCPArea`; `Q_DISABLE_COPY` | `:127-134` |
| `QCPKeyWindow { QCPRange keys; double pixelsPerKey; }` | `painting/draw-context.h:80-84` |

Base implementations (`coordinate-system.cpp`): `clipPath` = the rect's path (`:60`), `clipsToPath`
false (`:55`), `contains` true (`:71`), `iconTransform = QCPTransform::fitted(box, rect)` (`:80`),
`keyWindow` empty = "keeps no window: every transform change remakes" (`:89`), `plottables()` scans
`plot->plottable(i)` and keeps `plottable->system() == this` (`:99-118`, `simplify:` note `:105`),
`bumpRevision()` = new revision then, only if `listens()`, `qcpEmit(changed)` (`:120-128`).

**What a third-party system must implement:** the four pure virtuals (`transform`, `clipRect`,
`ratioTransform`, `capabilities`), the protected constructor with its area and the two axes, and its
own `bumpRevision()` calls; optional `clipPath`, `clipsToPath`, `contains`, `iconTransform`,
`keyWindow`. It cannot be constructed by anyone but `QCPArea::system()`: every system ctor is private
and `friend` to its area (`coordinate-system.h:158`, `coordinate-system-polar.h:46`), and
`QCPArea::system(key, value)` is the only factory (`area.h:88`, `area.cpp:74-108`).

### 5.2 `bumpRevision()` / `changed()` and who reacts

| Fact | Where |
| --- | --- |
| `bumpRevision()` on any axis mapping change: `QCPArea::axisMappingChanged(axis)` bumps every system that `usesAxis` | `area.cpp:124-137`, called from `QCPAxisBase::notifyMappingChanged` (`axis-base.cpp:570-571`) |
| `QCPArea::update()` bumps every system when `mSystemsRect != rect()` | `area.cpp:192-199` |
| `disconnectNotify`/`connectNotify` only count `changed()` listeners | `coordinate-system.cpp:130-153` |
| **No library code connects to `changed()`** — the only connects are in tests (`test-coordinate-system.cpp:299,339,809`); frames observe the system through `revision()` in the memo key | `drawable.cpp:443` (`memo.revision`) |
| A system's `changed()` marks no layer (4g R-6); `QCPArea::system()` also marks nothing, with the comment that an axis marks when its mapping moves and a plottable when its content does | `area.cpp:100-104` |
| `QCPArea::systemsListen()` = any system with listeners; an axis reports itself as listened when `mArea->systemsListen()` | `area.cpp:163-174`, `axis-base.cpp:140` |

### 5.3 Area subclassing (`src/layoutelements/area.h`)

`QCPArea : public QCPLayoutElement` (`:41`). Pure virtuals: `QList<QCPAxisBase*> allAxes()` (`:79`),
`void markAffectedLayersDirty(bool invalidate = false)` (`:83`), `QCPCoordinateSystem*
createSystem(QCPAxisBase* key, QCPAxisBase* value)` (`:122`). Non-virtual: `system(key, value)`
(`:88`), `systems()` (`:90`), `systemsListen()` (`:92`), `axisMappingChanged` (`:96`),
`axisRemoved` (`:99`), `insetLayout()` (`:70`). Virtuals a subclass may override: `update`
(`:75`), `elements(bool)` (`:76`), `drawBackground*` (`:127-130`). Protected state: `mSystems`
(`QList<QPointer<QCPCoordinateSystem>>`), `mSystemsRect` (`:111-114`).

`QCPArea::system()` misuse rules: a null axis ("a coordinate system needs two axes"), `key == value`,
and an axis not in `allAxes()` (`area.cpp:74-99`). Subclasses:
`QCPAxisRect::createSystem` refuses two axes of the same orientation and returns a
`QCPCartesianSystem` (`layoutelement-axisrect.cpp:490-502`); `QCPPolarArea::createSystem` refuses
anything but the angular axis and one of its radial axes (`polar/polar-area.cpp:123-133`).

Note the API gap: `QCPArea` has no virtual for item positions, item clipping or item hit testing.

### 5.4 `QCPCustomMapping` and `QCPTransform::custom`

`src/coords-custom.h`: `virtual dtor` (`:40`); pure virtual
`map(std::span<const QPointF> keysValues?, ...)` — precisely `map(span keys, span values, span out)`
(`:42-43`); `virtual map(span<QPointF> data, span out)` (`:45`); `virtual mapPoint(QCP::Data)`
(`:47`); pure virtual `inverse(QPointF)` (`:49`); `virtual equivalentTo(other, QPointF* delta)`
default false (`:51`). All batch; a subclass overriding one `map()` overload needs
`using QCPCustomMapping::map` (`:33`).

`QCPTransform::custom(std::shared_ptr<const QCPCustomMapping>, quint64 revision)` (`coords.h:158`,
`coords.cpp:158`) and `Kind::Custom` (`coords.h:138-143`), the variant member
`Custom { mapping, revision }` (`coords.h:288`), and `isTranslationOf` refusing a kind mismatch
(`coords.cpp:590`).

**No library path ever creates `QCPTransform::custom`**: grep finds the factory only in `coords.cpp`
and the tests (`test-transform.cpp:246,269`). A custom system would be the first producer, and the
mapping carries none of the frame's other parts — clip, hit area, key window, icon transform,
revision bumping and area all come from the `QCPCoordinateSystem` that returns it.

### 5.5 Every place that switches on transform kind or casts an area/system (fall-through list)

| Site | What it assumes |
| --- | --- |
| `coords.cpp:590` | `isTranslationOf` is false unless both kinds are equal and valid |
| `drawable.cpp:861` | recording box-culling only when `frame.transform.kind() == Kind::Cartesian` (a polar frame's data box corners say nothing, comment `:857-860`) |
| `drawable.cpp:511` | a plottable view's `axisRect` fallback is `qobject_cast<QCPAxisRect*>(view.system->area())` — null for any non-rect area |
| `drawable.cpp:484-500` | an item's view has no system at all, so per-position axes and `clipAxisRect` |
| `draw-context.cpp:387-392` | `qcpMakeFrame` takes `QCPAxis*`, `QCPAxis*`, `QCPAxisRect*` — the only item-side frame factory |
| `coordinate-system.cpp:202-205` | `QCPCartesianSystem::ratioTransform` = `qobject_cast`-free `static_cast<QCPAxisRect*>(area())` |
| `coordinate-system-polar.cpp:39` | `QCPPolarSystem` casts `area()`/`keyAxis()`/`valueAxis()` to the polar types |
| `axis.cpp:3020` | `QCPAxis::axisRect()` = `static_cast<QCPAxisRect*>(mArea)` (a cartesian axis is always in a rect) |
| `plottable.cpp:699` | `markLegendRows` recurses into a `QCPAxisRect`'s inset layout to dirty legend rows; a non-rect area's inset legend is not reached |
| `polar/polargraph.cpp:431` | a polar graph refuses a system that is not a `QCPPolarSystem` |
| `polar/polargraph.cpp:482` | `QCPPolarArea* area = qobject_cast<QCPPolarArea*>(this->area())` |
| `polar/angularaxis.h:172`, `polar/radialaxis.h:194` | `static_cast<QCPPolarArea*>(mArea)` in `polarArea()` |
| `core.cpp:2427`, `:2464`, `:2550` | `firstAxisRect`, `axisRects`, `axisRectAt` are `QCPAxisRect`-only |
| `core.cpp:3853` | zoom-mode selection rect starts only on a `QCPAxisRect` |
| `core.cpp:4717` | `processRectZoom` zooms only the `QCPAxisRect` under the press |
| `core.cpp:4640-4648` | rubber-band selection already goes by `QCPArea` and `plottable->system()->area()` |
| `core.cpp:2968-2978` | `rescaleAxes` already enumerates `areas()` and their `allAxes()` |

---

## 6. Capabilities

`src/global.h:283-298`: `capRectangular = 0x01` ("the plot area is a rectangle"),
`capStraightKey = 0x02` ("the key axis is straight: lanes and bar stacks need it"),
`capColourScale = 0x04` ("a colour scale can be placed next to the area"),
`capLanes = 0x08` ("the area can lay out lanes (multi-graphs, intervals)"); `Q_DECLARE_FLAGS`
plus `Q_ENUM_NS`/`Q_FLAG_NS` (`:462-463`) and operators (`:473`).

Offerers: `QCPCartesianSystem` = all four (`coordinate-system.cpp:207`); `QCPPolarSystem` = none
(`coordinate-system-polar.cpp:108`).

`QCPAbstractPlottable::requiredCapabilities()` default = `capRectangular | capStraightKey`
(`plottables/plottable.h:186-189`). Overrides: `QCPPlottableColourMapped` adds `capColourScale`
(`plottable-colormapped.h:25-27`), `QCPPlottableIntervals` adds `capLanes`
(`plottable-intervals.h:32-34`), `QCPPlottableMultiGraph` adds `capLanes`
(`plottable-multigraph.h:40-42`). No other override in `src/plottables/`.

Checks: `plottable.cpp:833` on the `setSystem` join (`requiredCapabilities() & ~system->capabilities()`);
`construct.h:251` and `:272` in the two `QCPPolarArea::add` forms
(`!(plottable->requiredCapabilities() & ~system->capabilities())`, misuse "the polar system lacks a
capability this plottable needs"), which also require `angularAxis()`, `radialAxis(0)`,
`system(angular, radial(0))` and `adoptPlottable(object, system, this)`.

---

## 7. Users to move (item axes / axis rect)

Occurrences of `setAxes(`, `setAxisRect(`, `setClipAxisRect(`, `clipAxisRect`,
`ptAxisRectRatio` (item-side and plottable-side together):

| File | Count |
| --- | --- |
| `documentation/doc-image-generator/mainwindow.cpp` | 18 |
| `tests/auto/test-repaint/test-repaint.cpp` | 14 |
| `tests/auto/test-qcpaxisrect/test-qcpaxisrect.cpp` | 12 |
| `tests/auto/test-coords/test-coords.cpp` | 12 |
| `tests/auto/test-item-position/test-item-position.cpp` | 9 |
| `tests/auto/test-drawable/test-drawable.cpp` | 8 |
| `tests/auto/test-builder/test-builder.cpp` | 8 |
| `tests/auto/test-item-recording/test-item-recording.cpp` | 6 |
| `examples/plots/mainwindow.cpp` | 6 |
| `docs/snippets/gallery.cpp` | 6 |
| `examples/axis-tags/axistag.cpp` | 4 |
| `docs/items.md` | 4 |
| `tests/manual/mainwindow.cpp` | 3 |
| `tests/auto/test-item-index/test-item-index.cpp` | 3 |
| `docs/snippets/items.cpp` | 3 |
| `tests/benchmark/benchmark.cpp` | 2 |
| `tests/auto/test-span/test-span.cpp` | 2 |
| `tests/auto/test-linked-axes/test-linked-axes.cpp` | 2 |
| `tests/auto/test-creation-mode/test-creation-mode.cpp` | 2 |
| `examples/axis-tags/mainwindow.cpp` | 2 |
| `docs/snippets/interactions.cpp` | 2 |
| `tests/manual/gallery/main.cpp` | 1 |
| `tests/auto/test-ownership/test-ownership.cpp` | 1 |
| `tests/auto/test-draw-context/test-draw-context.cpp` | 1 |
| `docs/snippets/concepts.cpp` | 1 |
| `docs/snippets/axes.cpp` | 1 |
| `docs/layout.md`, `docs/graphs.md`, `docs/concepts.md`, `docs/axes.md` | 1 each |

Item-specific places in the prose:

- `docs/items.md:113` (the `ptAxisRectRatio` row of the position-type table), `:128` ("Plot
  coordinates use `customPlot->xAxis` and `customPlot->yAxis` by default. `setAxes()` picks other
  axes"), `:129` ("The ratio and axis-rect types use the first axis rect by default. `setAxisRect()`
  picks another"), `:162` ("`setClipAxisRect(rect)` clips to another axis rect … for items in a
  second axis rect").
- `docs/snippets/items.cpp:132,172,178`; `docs/snippets/gallery.cpp:104,106,168,557,559`;
  `docs/snippets/concepts.cpp:42`.
- `examples/axis-tags/axistag.cpp:36-39` (`setTypeX(ptAxisRectRatio)`, `setAxisRect(mAxis->axisRect())`,
  `setAxes(0, mAxis)` — the one-axis-position idiom), `:97`.
- `examples/plots/mainwindow.cpp:908,969,1001` (label positions in ratio), `:1204-1219` (plottable
  axes on sub-rects).
- `documentation/doc-image-generator/mainwindow.cpp` (18 hits across the item and plottable setters).
- Tests by theme: `test-item-position` (whole file is position semantics), `test-item-recording`
  (`aRatioArrowFollowsTheAxisRect` :274, `aRatioPixmapFollowsTheAxisRect` :593,
  `aPixelOnlyItemDrawsWithoutAxes` :421, `aDataItemWithoutAxesDrawsNothing` :453),
  `test-item-index` (its `placeRandomly` helper takes a `QCPAxis*` key and value pair, `:46`),
  `test-coords` (`positionSetMatchesSetters` :305, `ratioTransformMatchesPixels` :437),
  `test-draw-context`, `test-drawable`, `test-repaint`, `test-span`/`test-rspan`,
  `test-creation-mode`, `test-builder`, `test-linked-axes`, `test-qcpaxisrect`, `test-ownership`.

---

## 8. Bench rows that touch items

`tests/benchmark/benchmark.cpp`, declared at `:71-89` and `:2308`, implemented around the listed
lines: `QCPItems_AddRemove10k` (:1203), `QCPItems_AddRemove20k` (:1212), `QCPItems_Replot10k`
(:1221), `QCPItems_Add10k` (:1256), `QCPItems_Clear10k` (:1265), `QCPItems_DeleteEach10k` (:1274),
`QCPItems_RemoveEach10k` (:1287), `QCPItems_DeletePlot10k` (:1316), `QCPItems_Zoom10k` (:1338),
`QCPItems_Zoom100` (:1352), `QCPItems_ZoomEmpty` (:1365), `QCPItems_ClickEmpty10k` (:1400),
`QCPItems_Click10k` (:1413), `QCPItems_Click100` (:1425), `QCPItems_FirstFrame10k` (:1611),
`QCPItems_DragOne10k` (:1621), `QCPItems_RestyleAll10k` (:1650), `QCPItems_Pan10k` (:1667),
`QCPItemText_Zoom1k` (:2308), `QCPItems_MixedKinds_Pan1k` (:2729), `QCPItems_Tracers_Pan1k` (:2809),
plus `QCPAxis_SetTickLength10kItems` (:1192).

The item population helper is `addLineItems(plot, count, xMax)` (`benchmark.cpp:1168-1191`); the
contract-only equivalents are `BenchDrawnBar` (`:2443`) and `BenchDrawnBarSet` (`:2474`).

---

## Open questions

Facts that make a design choice necessary, no proposals:

1. **The item index key cannot hold a system.** `QCPItemExtent` and `ItemIndexGroup` are
   `(QCPAxis*, QCPAxis*)` (`drawable.h:68-74`, `layer.h:154-162`), `collectItemAxes` collects
   `QCPAxis*` only (`layer.cpp:1159-1183`), and `itemCandidates` maps pixels with
   `dataBoxOfRect(keyAxis, valueAxis, …)` (`layer.cpp:1104-1118`, query at `:1336`). A system that is not an axis pair
   has no data box for a pixel rect unless the system supplies one.
2. **`QCPItemPosition` holds two loose axis pointers and one rect pointer, and nothing joins them**
   (`item.h:186-187`, `item.cpp:827-859`). A system-bound position needs a second identity, and
   `setAxes` must keep working for cartesian (§2.9, §5 of the spec).
3. **The draw frame has no system slot for items.** `QCPDrawnView::system` is documented as "null for
   items, which read the axes" (`drawable.h:49`), and `qcpMakeFrame` / `QCPRatioTransform` are
   `QCPAxisRect`-typed (`draw-context.cpp:387-392`, `coords.h:315-334`). The frame needs to be built from
   a system for an item, and the ratio half of it has no system-based source.
4. **`QCPItemExtent` needs a non-axis-pair key** for the same reason as 1; `positionsExtent` already
   refuses a mixed pair (`item.cpp:1670-1676`) and would have to express "one system".
5. **`QCPItemPosition::pixelPosition()` is public and asked outside any drawing** (`item.cpp:729`), and
   its fast path builds its own cartesian frame (`:738-740`). What it answers for a system that
   cannot map a single point (a batch-only custom mapping has `inverse` but `mapPoint` is optional,
   `coords-custom.h:47`), and whether it reports, is a decision.
6. **Item code reads the transform in two incompatible ways** — the context's (`item-line.cpp:34-46`)
   and the position's own pixels (`item-richtext.cpp:38`, `item-rspan.cpp:73-137`). Only the first
   follows a system; the second would need the position to know its system.
7. **Polar axes are not `QCPAxis`** (`angularaxis.h:26`, `radialaxis.h:49`), so an item cannot name a
   polar axis today, and a polar-bound position cannot be expressed with `setAxes`.
8. **Item lifetime on axis deletion is "pointer goes null"** (`item.cpp:749-770`,
   `drawable.cpp:699-717`), while spec §2.7 asks for items to be detached before systems are deleted.
   Nothing today detaches an item from a system, and no test covers it.
9. **`QCPCustomMapping` has no single-point map and no hit area** (`coords-custom.h:42-51`), so an
   item on a custom system needs the system for `contains`, clip and icon transform; `QCPTransform::
   custom` is currently produced only by tests.
10. **`QCPArea` exposes no item hook** — item positions, clipping and hit testing are all on the item
    and the plot (`area.h:79-99`); a system-owned item frame has to be routed through something that
    does not exist yet.
11. **No library listener to a system's `changed()` exists** (`coordinate-system.cpp:130-153`,
    `drawable.cpp:443`); items are re-recording through `mCore.contentChanged()` on `markDirty()`
    (`item.cpp:1630-1648`), and `setAxes`/`setAxisRect` cannot notify a system (`item.cpp:827-859`).
12. **`markItemsUsingAxisDirty` and the index use `itemsUseAxis`** (`layoutelement-axisrect.cpp:547-556`,
    `layer.cpp:1193-1206`); a system that is not an axis pair has no equivalent "does any item follow
    this system" question answered anywhere.
13. **Areas cannot move between plots** (`layout.cpp:951-963`), while spec §2.7 and §4 step 11 ask for
    the take/adopt and "area moved between plots" cases to be exercised.
