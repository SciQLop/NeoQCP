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
- Culling is a cheap per-item rectangle test, still O(n) per frame, no spatial index (maintainer: "we
  don't usually put thousands, but it should scale"). Zoom over 10 000 items: 1.2 ms against a 1.0 ms
  target; the rest is computing 10 000 bounds. Cost: about 0.8 ms per frame at 10 000 items.
- Hit-test bounds are cached by the screen draw and trusted only while the plot that drew them still
  has the same paint generation and viewport: any dirty mark anywhere invalidates every cached bound.
  Cost: fewer cache hits, never a wrong hit. `QCPItems_Click10k` (a click that toggles a selection with
  no repaint between clicks) stays 1.2 ms; a click on a settled plot is 0.28 ms.
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
- `QCPItemBracket::pixelBounds` pads by 1.5 x its length, an envelope checked against the bundled styles.
