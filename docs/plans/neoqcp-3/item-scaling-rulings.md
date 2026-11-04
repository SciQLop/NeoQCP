# NeoQCP 3.0 — item scaling rulings and deferred items

Item scaling: no quadratic step when adding or clearing items; items outside the view are not drawn
or hit-tested. Branch `neoqcp-3.0`, commits `3970d59..` (local, not pushed). Plan:
`docs/superpowers/plans/2026-11-04-neoqcp-3-item-scaling.md`. Measurements:
`item-scaling-profile.md`. Benchmark: `baseline.md`, "End of item scaling".

## Question for the maintainer

- After an export at another size (`savePng(file, 800, 600)`, `toPixmap(w, h)`, `toPainter`), the
  layout keeps the export's geometry until the next screen replot: `coordToPixel`, axis rects and hit
  tests describe the export, not the screen (pre-existing). This breaks the spec's "layout-dependent
  queries are always current". Re-running the layout after the export restores the viewport fixes it,
  but 45 tests read `coordToPixel` after an export to find pixels in the exported image, an idiom user
  code may share. Reverted; your call: fix it (and port those tests to the export's own geometry), or
  document it.

## Rulings (what was decided, why, cost if wrong)

- "Constant-time removal" (spec decision 2) means NeoQCP adds no scan beyond Qt's own children search,
  since items stay plot children. Removing items one by one in random order still costs two ordered
  list searches (item list, layer list; both carry order: `item(index)`, drawing order). Accepted with
  a `simplify:` comment. Cost: 10 000 random deletes take 24 ms instead of about 10.
- Only `registerItem` uses the item's plot pointer as its duplicate test. `hasItem`, `removeItem` and
  `detach` keep a list search, because they take user pointers that may dangle.
- The layer's duplicate check in `addChild` is dropped, not turned into a `Q_ASSERT` (asserts are live
  in the measured build). Only `moveToLayer` adds, right after leaving the old layer.
- `clearItems()` deletes oldest first (Qt erases a first element by moving the begin pointer). A
  `destroyed()` slot deleting a raw sibling pointer is unsafe in either order; one test now holds a
  `QPointer`. Cost: such user code may crash in a different order than in 2.x (changelog).
- (Replaced by the item index, below.) Culling was a per-item rectangle test, and hit testing used
  bounds cached by the last screen draw. Both are gone.
- Only line, rect, ellipse, curve and bracket opt in. The tracer moves inside its own draw, text and
  pixmap sizes need font metrics, the straight line is infinite.
- Adding 20 000 items costs 2.36x adding 10 000 (target 2.3x): linear within noise.

## Bugs found and fixed

- A range change never repainted a layer holding only items: such items stayed at their old pixels
  after a zoom or a pan (2.x too). Found because the item zoom benchmark drew nothing.
- Cached hit-test bounds had no plot identity: an item taken from one plot and added to another could
  be hit-tested with the old plot's rectangle until its first draw (final review).

## Review findings ruled false

- "Deleting oldest first makes every erase a memmove": Qt 6 erases a first element by moving the begin
  pointer (`qarraydataops.h`).
- "`refreshBoundsCache()` recomputes bounds on every hit test": no such function exists.

## Deferred minors

- `markItemsUsingAxisDirty` marks once per item (one mark per layer would do) and copies each item's
  position list; measured flat (`QCPAxis_SetTickLength10kItems` 15 ms).
- `exportDoesNotPoisonBounds` replots before its clicks, so it pins "a frame after an export restores
  hit testing", not the cache itself; tightening it waits for the export-layout decision.
- `QCPItemBracket::dataExtent` pads by 1.5 x its length, an envelope checked against the bundled styles.

# Item index (plan `docs/superpowers/plans/2026-11-04-neoqcp-3-item-index.md`, commits `fcad1ae..`)

Each layer keeps a packed R-tree of its items' boxes in data coordinates. Maintainer, 2026-11-04: "if
we could use smart and modern algorithms [...] a kd-tree or any smart data structure/algorithm here is
welcome". Benchmark: `baseline.md`, "End of item index". All four plan targets are met.

## Rulings

- A packed STR R-tree (Skia `SkRTree`, Chromium `cc/base/rtree.h`), not a kd-tree or a dynamic AABB
  tree: items are boxes, and they move rarely compared with how often frames are drawn. Moved items
  go to a side list tested one by one (Bentley and Saxe); the tree is rebuilt when that list passes
  max(64, n/16). Cost if wrong: a plot that drags hundreds of items at once rebuilds often.
- Boxes are in data coordinates, so zooms, pans and reversals never rebuild. A scale type change
  rebuilds every tree, because a log axis has no pixel for values at or below zero.
- Every "this item may have moved" event goes through the item's repaint mark: the setter rule of
  milestone 2 already makes every setter call it.
- A layer with fewer than 64 items keeps moved items on the side list until its child list changes.
  Cost: up to 63 items tested one by one.
- Clicks cost 0.05 ms at 10 000 items. The rest is three plain passes over the layer's children; a
  plot-wide set of selected layerables is the upgrade (`simplify:` comment in core.cpp).
- A selectionChanged slot that selects a not-yet-visited item during a deselect pass leaves it
  selected: the visit list is taken first. 2.x already depended on visit order.
- `scaleTypeChangeRebuildsTheIndex` checks the rebuild counter, not pixels: a stale box on a log axis
  maps to the fixed off-screen pixel and did not show in the probes tried.
- `add()` of a taken item resets its positions to the default axes, even when it goes back to the
  same plot (milestone-2 adopt rule). Kept.
- `QCPItems_RemoveEach10k` 25 -> 32 ms is a code-placement effect (same instruction count, 10% more
  cycles, bisected to a commit that edits only code the row never runs). Not chased.

## Bugs found and fixed

- An item with one point in data coordinates and another pinned to the axis rect got a box covering
  only the first point, so it could be culled while on screen (found by the randomized test).
- A click queried the whole view instead of the click point.
- A label anchored to an item on another layer kept its old pixels when that item moved, changed
  axes, or had its axis moved.
- Anchor cycles through another item's plain anchor, or through the other axis, were accepted (also
  in 2.x) and overflowed the stack. One walk in setParentAnchorX/Y now refuses them.
- Marking an item whose position follows its own other position recursed forever (found by the final
  review, introduced by the follower fix).
- Parallel resampling could return before its tasks finished (also in 2.x): a late task released the
  next pass's semaphore, and a task wrote into freed memory. One join helper replaces three copies.

## Review findings ruled false

- "The remaining zoom cost is the cast in the draw loop": the draw loop skips culled items first; the
  casts were three replot steps scanning every child for plottables (each layer keeps that list now).
- "The click still costs 0.24 ms because of the per-child loop": it queried the whole view.
