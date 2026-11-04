# NeoQCP 3.0 item spatial index — implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** a frame and a click cost what the items in view cost, not what all items cost.

**Architecture:** each layer keeps a packed R-tree (STR bulk load) of its items' extents in **data
coordinates**. Axis transforms are monotone per axis, so a data box maps to a pixel box under every
zoom, pan, log or reversed axis: the tree survives range changes and is rebuilt only when the layer's
child list changes. Items whose extent changed since the build sit in a small side list tested one by
one (the Bentley–Saxe static-to-dynamic idea); the tree is rebuilt when that list grows. Drawing and hit
testing query the tree once per layer and keep the layer's child order.

**Tech Stack:** C++20, Qt 6.12, Meson, QTest.

**Spec:** `docs/specs/2026-11-02-neoqcp-3-object-model-design.md`, decision 2 (drawing and hit testing
skip items outside the view). Maintainer, 2026-11-04: "if we could use smart and modern algorithms […] a
kd-tree or any smart data structure/algorithm here is welcome". Previous milestone:
`docs/plans/neoqcp-3/item-scaling-rulings.md` (this plan replaces its per-frame bounds test and its
hit-test bounds cache).

## Prior art (why this structure)

- Packed R-tree with Sort-Tile-Recursive bulk load: Leutenegger, Edgington, López, "STR: A Simple and
  Efficient Algorithm for R-Tree Packing", ICDE 1997. Used by Skia (`SkRTree`) and Chromium
  (`cc/base/rtree.h`) to cull recorded draw operations — the same job. Bulk load only, flat arrays.
- Packed Hilbert R-tree (flatbush, Agafonkin): the same with a Hilbert sort; STR is chosen for its
  simplicity in 2D.
- Dynamic AABB tree (Box2D `b2DynamicTree`): O(log n) moves via fattened boxes and rotations; more code,
  and items here move rarely compared with how often frames are drawn.
- Moving items on a static tree: Bentley & Saxe, "Decomposable searching problems I: static-to-dynamic
  transformation", J. Algorithms 1980 — a static structure plus a small part searched by brute force.
- Qt `QGraphicsScene` BSP index: same purpose, documented as costly for moving items.

## Global Constraints

- As in `docs/superpowers/plans/2026-11-04-neoqcp-3-item-scaling.md` (env, one build at a time `-j 4`,
  offscreen tests, both builds, explicit-path commits with the Co-Authored-By line, no push).
- Item order is API: a layer draws and hit-tests its children in its child order.
- Layout-dependent queries are always current: the index answers for the current positions, never for
  "the last frame" (no draw-stamped cache).
- No new dependency (the tree is ~150 lines of our own code).

## Review Focus

1. An item moved by every route — `setCoords`, `setPixelPosition`, `setType*`, `setAxes`, a parent
   anchor, `setLayer`, `take`/`add`, a pen or line-ending change that grows its padding — is drawn and
   hit at its new place on the very next frame and click.
2. Log, reversed and log-reversed axes; an item exactly on the view edge; an item larger than the view.
3. Mixed coordinate types (x in plot coordinates, y as an axis-rect ratio — a vertical span): indexed
   on x only.
4. Items of one layer on two axis rects, or on x2/y2.
5. 10 000 adds then one frame (one rebuild), and dragging one item among 10 000 (side list, no rebuild).

---

### Task 1: `QCPSpatialIndex` (pure data structure)

**Files:** Create `src/spatial-index.h`, `src/spatial-index.cpp`; Test `tests/auto/test-spatial-index/`.

**Interface:**
```cpp
struct QCPDataBox { double keyLower, keyUpper, valueLower, valueUpper; }; // +-inf allowed
class QCPSpatialIndex {
public:
    void build(std::vector<std::pair<QCPDataBox, int>> entries); // int = payload (child position)
    template <typename F> void query(const QCPDataBox& box, F&& onHit) const; // onHit(int payload)
    [[nodiscard]] int size() const;
};
```
STR bulk load, fanout 16, nodes in one flat `std::vector` (Chromium/flatbush layout). Boxes are closed;
an infinite side is valid (an item unbounded on one axis).

- [ ] Tests: `emptyIndex`, `singleBox`, `edgeTouchIsAHit`, `infiniteSides`, and
  `randomMatchesBruteForce` (seeded: 1–20 000 boxes, 1 000 queries, result set == brute-force set).
- [ ] Commit "feat: a packed R-tree for item culling".

### Task 2: Layers index their items

**Files:** `src/layer.{h,cpp}`, `src/items/item.{h,cpp}`, the five item classes, `src/core.cpp`.

- `QCPAbstractItem::dataExtent()` (virtual, replaces `pixelBounds()`): the key/value axes, the data box
  of its positions, and a pixel padding (pen, selected pen, line endings, bracket length). A coordinate
  is boxed only when its type is `ptPlotCoords` with no parent anchor on that axis; otherwise that side is
  infinite. Both sides infinite, or nullopt (default): not indexed, always drawn and tested.
- `QCPLayer`: one `QCPSpatialIndex` per axis pair of its items, built lazily on the next query after any
  change of its child list; the payload is the child position. One function every extent change goes
  through (`QCPAbstractItem::noteExtentChanged()`) moves the item to the layer's side list; rebuild when
  the side list holds more than `max(64, n / 16)` items.
- Draw: per axis pair, the clip rect inflated by the pair's largest padding → data box (`pixelToCoord`
  on the corners, normalised) → mark hits in a per-layer `std::vector<uint8_t>`; the child loop skips an
  indexed child whose mark is unset without touching the child. Side-list items and unindexed children
  are drawn as today.
- Hit test: the same with the click point inflated by `selectionTolerance()`.
- Remove the draw-stamped bounds cache (`mPixelBounds*`, `mCachingBounds`) and `pixelBounds()`.
- [ ] Tests first: the existing culling tests (renamed where they name `pixelBounds`), and
  `randomItemsMatchBruteForce`: seeded random items of the five types, random coordinate types, axes
  (incl. log/reversed), layers and pens, random mutations through the public API, random views; the set
  drawn and the hit set equal a brute-force computation in the test (counting subclasses).
- [ ] Commit "perf(items): layers index their items in data coordinates".

### Task 3: Range changes mark only the layers using the axis

- Each layer keeps the set of axes its items use (rebuilt with the index, extended by side-list items).
  `QCPAxis::rangeDidChange` marks those layers; `markItemsUsingAxisDirty` stops walking every item.
- Benchmark rows: `QCPItems_Pan10k` (a pan step with 10k items), `QCPItems_DragOne10k` (move one item
  among 10k + replot), `QCPItems_FirstFrame10k` (add 10k, one replot: the rebuild).
- [ ] Commit "perf(items): a range change marks the item layers it moves".

### Task 4: Close

Targets (end of plan): `Zoom10k` <= 1.3 x `Zoom100`; `Click10k` and `ClickEmpty10k` <= 0.05 ms;
`DragOne10k` <= 1.5 x `Zoom100`; `FirstFrame10k` - `Add10k` <= 2 ms. Baseline section, changelog,
rulings doc update, final review (each finding verified), memory. No push.
