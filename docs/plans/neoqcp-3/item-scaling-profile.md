# Item scaling — what the rows measure

Ten thousand `QCPItemLine`s on one plot, `QT_QPA_PLATFORM=offscreen`, `-median 5`, three runs per row.
Row names and the benchmark source: `tests/benchmark/benchmark.cpp`.

Every row measures one user operation:

| row | the operation |
| --- | --- |
| `QCPItems_Add10k` | create 10 000 items (`new QCPItemLine(plot)`), clear outside the measurement |
| `QCPItems_Clear10k` | `clearItems()` |
| `QCPItems_DeleteEach10k` | `delete item` for each item, in a shuffled order (the user path: `delete`, or a handle's `remove()`) |
| `QCPItems_RemoveEach10k` | `removeItem(item)` for each item, same shuffled order (the API path, which looks the item up first) |
| `QObject_DeleteEachChild10k` | the control: 10 000 plain `QObject` children of one parent, deleted in the same shuffled order |
| `QCPItems_DeletePlot10k` | `delete plot` with 10 000 items in it |
| `QCPItems_Zoom10k` | a real zoom (range width alternates 1.0 / 0.98 around 50.5) on 10 000 items spread over 0..100, so 100 are in view |
| `QCPItems_Zoom100` | the same zoom over 100 items spread over 0..1, so 100 are in view |
| `QCPItems_ZoomEmpty` | the same zoom with no item at all — the floor |
| `QCPItems_Click10k` | one press + release pair, alternating between a point on an item and an empty point, 100 of 10 000 items in view |
| `QCPItems_Click100` | the same with 100 items |
| `QCPItems_AddRemove10k` | the pre-existing row: add all, `clearItems()`, as one measurement |

The shuffled order matters: Qt 6 removes the first element of its child list in O(1) (it moves the
begin pointer), so deleting in creation order would measure a case no program hits and would hide
Qt's real cost. With the shuffle, `QObject_DeleteEachChild10k` costs 7.8 ms instead of 0.6 ms.

## Numbers

| row | run 1 | run 2 | run 3 | median |
| --- | --- | --- | --- | --- |
| `QCPItems_Add10k` | 40 | 40 | 40 | **40 ms** |
| `QCPItems_Clear10k` | 23 | 23 | 23 | **23 ms** |
| `QCPItems_DeleteEach10k` | 26 | 25 | 25 | **25 ms** |
| `QCPItems_RemoveEach10k` | 31 | 31 | 31 | **31 ms** |
| `QObject_DeleteEachChild10k` | 7.9 | 7.8 | 7.8 | **7.8 ms** |
| `QCPItems_DeletePlot10k` | 62 | 62 | 62 | **62 ms** |
| `QCPItems_Zoom10k` | 0.14 | 0.14 | 0.14 | **0.14 ms** |
| `QCPItems_Zoom100` | 0.14 | 0.14 | 0.14 | **0.14 ms** |
| `QCPItems_ZoomEmpty` | 0.14 | 0.14 | 0.14 | **0.14 ms** |
| `QCPItems_Click10k` | 1.2 | 1.2 | 1.2 | **1.2 ms** |
| `QCPItems_Click100` | 0.013 | 0.013 | 0.013 | **0.013 ms** |
| `QCPItems_AddRemove10k` | 63 | 63 | 63 | **63 ms** |

Per item, on add: 4.0 µs. On `clearItems()`: 2.3 µs. On `delete`: 2.5 µs. On `removeItem()`: 3.1 µs.
Qt's own child bookkeeping for the same shape costs 0.78 µs per object.

## What the numbers say

1. **Add and delete are the whole cost, and most of it is per-item bookkeeping of two lists.** 40 + 23
   = 63 ms is exactly the pre-existing `AddRemove10k` row, so nothing else in the class is
   significant. `removeItem()` costs 0.6 µs more per item than a plain `delete`: that is its
   `mItems.contains` + `mItems.removeOne` lookup before the delete.
2. **Qt's child list is not the problem, but it is not free either.** The control row (10 000 objects,
   shuffled deletes, no QCP lists) is 7.8 ms; deleting the same number of items costs 25 ms, so about
   17 ms of the 25 is ours: the layer's `removeChild` erase and `invalidatePaintBuffer`, plus the item
   destructor's `itemDestroyed` → `mItems.removeOne`.
3. **Drawing does not scale with the number of items.** `Zoom10k` == `Zoom100` == `ZoomEmpty` == 0.14 ms:
   100 items in view cost the same as none, and 9 900 items outside the view cost nothing. A true zoom
   (the width changes, so the layer is redrawn, not translated) still shows no per-item cost at this
   size. Any culling proposal has to justify itself on something other than "drawing is slow".
4. **Hit testing does scale, linearly.** `Click10k` is 1.2 ms against 0.013 ms for `Click100`: 92× the
   time for 100× the items, i.e. ~120 ns per item per click. Every press and every release walks the
   item list and calls `selectTest` on each item. This is the one place where the item count is felt
   interactively.

## Profiles

`perf record -g` on the row, `perf report --no-children`. The `QBENCHMARK_ONCE` rows cannot be
repeated inside one process (`-iterations` does not apply to them), so those sessions run the row
sixty times and their measured region is ~35 % of the session: the item creation in the setup dwarfs
it. The tables below therefore give both the top frames and a bucket share; for `Clear10k` and
`DeleteEach10k` read the *differences* from the `Add10k` session, which is the same setup with no
clear.

Buckets: **item list** = `mItems` scans (`indexOf<QCPAbstractItem*>`,
`sequential_erase_one<QList<QCPAbstractItem*>>`, `itemDestroyed`, `removeItem`); **layer list** =
`QCPLayer::addChild`/`removeChild` and their `indexOf`/erase; **Qt child** = `setParent_helper` and
`QObjectPrivate`; **draw/hit** = drawing and `selectTest`/`layerableListAt`/`pixelPosition`/
`coordToPixel`.

| session | item list | layer list | Qt child | draw/hit | rest |
| --- | --- | --- | --- | --- | --- |
| `Add10k` (setup only) | 17.8 % | 28.9 % | 18.2 % | 4.6 % | 30.5 % (item constructors, `axisRects()`, allocator) |
| `Clear10k` | 17.3 % | 29.1 % | 18.3 % | 4.6 % | 30.8 % |
| `DeleteEach10k` | 22.8 % | 23.5 % | 9.7 % | 4.4 % | 39.6 % |
| `Zoom10k` | 3.7 % | 6.7 % | 4.1 % | 6.3 % | 79.1 % (Qt's rasterizer and text engine, inside our draw) |
| `Click10k` | 0.6 % | 1.7 % | 6.1 % | 79.0 % | 12.6 % |

Top self-time frames:

| session | frames |
| --- | --- |
| `Clear10k` | `QCPLayer::addChild` 16.7 % (its `mChildren.contains`), `setParent_helper` 16.4 %, `indexOf<QCPAbstractItem*>` 16.1 %, `QCPLayer::removeChild` 12.2 % |
| `DeleteEach10k` | `indexOf<QCPAbstractItem*>` 15.8 %, `QCPLayer::addChild` 15.6 %, `setParent_helper` 8.1 %, `__memmove` 7.9 % (the erase shifting the list), `QCPLayer::removeChild` 7.8 %, `QCustomPlot::itemDestroyed` 5.7 %, `QCPLayer::invalidatePaintBuffer` 3.2 % |
| `Zoom10k` | Qt rasterizer frames 4.8 %, `QCPLayer::addChild` 3.7 % (setup), `setParent_helper` 3.5 % (setup), `indexOf<QCPAbstractItem*>` 3.4 % (setup/teardown), `QCPLayer::removeChild` 3.0 % (teardown); no per-item draw frame in the top ten |
| `Click10k` | `QCPAxis::coordToPixel` 18.8 %, `QCPItemPosition::pixelPosition` 15.7 %, `QCPVector2D::distanceSquaredToLine` 13.7 %, `QCPVector2D::QCPVector2D` 13.7 %, `QCPItemLine::selectTest` 7.3 %, `QSharedPointer::ExternalRefCountData::getAndRef` 5.4 %, `QAbstractItem::deselectEvent` 3.3 %, `QCPLayerable::realVisibility` 2.4 %, `QCPLayerableListAt` 2.4 % |

### Why `QCPLayer::addChild` shows up in a *clear*

It should not be on that path: clearing deletes items, and the destructor only calls
`QCPLayer::removeChild`. The frame is there because the session is not a clear: sixty repetitions of
`QCPItems_Clear10k` also run the row's setup sixty times, i.e. 600 000 `new QCPItemLine` calls, and
each `QCPLayerable` constructor ends in `setLayer(currentLayer())` → `addChild` → `mChildren.contains`,
a scan that grows with the number of items already on the layer. 2.4 s of setup against 1.4 s of clear
is why `addChild` and `indexOf` share the top of the profile with the clear's own frames. The `Add10k`
session, which is the same setup with no clear, has the same shares (16.3 / 16.5 / 12.4 %), which is
the evidence that the clear contributes nothing to them. The clear's own contribution — the
`setParent_helper` / `removeChild` / `invalidatePaintBuffer` frames — is the difference between the
two sessions, ~10 % of the clear session.

## What Task 2 should attack, and what it should not

Attack, in this order: the `mItems` scans (`registerItem`'s `contains`, `itemDestroyed`'s/`removeItem`'s
`removeOne`), `QCPLayer::addChild`'s `mChildren.contains`, and `QCPLayer::removeChild`'s `removeOne`.
Those are 40-50 % of the add and delete rows and are what makes 10 000 items cost 10 000 × O(n).

Not worth attacking: `setParent_helper` (Qt's own child bookkeeping, 0.78 µs per object measured by the
control row), the drawing path (0.14 ms with 0 or 10 000 items), and the item constructors' fixed cost
(two `QCPItemPosition` allocations and two appearance setters each).

A separate question, outside this plan's O(n²) theme but measured here: hit testing is linear in the
item count (finding 4). If clicking must stay cheap with thousands of items, it needs a spatial index
or a per-layer bounding box — a different task with a different benchmark row (`QCPItems_Click10k`),
not a culling flag in the draw path.
