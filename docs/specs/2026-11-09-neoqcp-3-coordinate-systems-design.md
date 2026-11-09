# NeoQCP 3.0 — coordinate systems (design addendum to milestone 4; replaces the M4f "polar" scope)

Status: draft 2, for the maintainer's review. Draft 1 was reviewed adversarially against the code
(`.superpowers/coordsys-design-review.md`, findings F1-F28); every finding is folded in below and cited.
Facts: `docs/plans/neoqcp-3/coordsys-inventory.md` (C-x) and `docs/plans/neoqcp-3/m4f-inventory.md` (P-x).

Maintainer decisions, 2026-11-09:

- D1. A plottable is drawn in a **coordinate system**, not "two `QCPAxis` in an axis rect". Polar is the
  second coordinate system, not a special case.
- D2. The coordinate system is **public API**: users can write their own (ternary, Smith chart, skew-T)
  and use NeoQCP's plottables in it without changing NeoQCP.
- D3. The cartesian path stays as fast as today.
- D4. A clean, consistent 3.0 API comes before the size of the change.

## 1. Intent

Today `QCPAxisRect` is the coordinate system without saying so (C-4): it owns the axes, gives the frame,
clip and hit area, holds the drag/zoom sets and the colour scale, and dirties layers. Plottables reach it
through `keyAxis()->axisRect()`. Polar duplicates all of it in classes that are not plottables (P-1..P-5).
This design names the concept, gives it a small public contract, makes the axis rect its cartesian
implementation and a new polar area its polar one, and moves plottables and items onto it.

Goals: one way for any plottable or item to get its frame, clip, hit area, ranges and axes; polar plots
made of ordinary plottables (legend, selection, rescale, ownership, recording, GPU); a documented
extension point exercised by a toy third system in the tests; no per-point virtual call.

Non-goals: new chart types beyond the toy test system; changing drawing-command semantics; the data
model and threading (project 3).

Anti-goals: a cartesian shortcut around the contract (the cartesian system is fast through the contract);
a plottable silently drawing nonsense in a system it cannot handle; two authoritative ways to say which
axes a plottable uses.

## 2. The model

### 2.1 Three roles

| Role | Cartesian | Polar | User's own |
|---|---|---|---|
| **Area**: a layout element; owns its axes and its coordinate systems; gets its rect from the layout; drag, zoom, wheel; dirty propagation | `QCPAxisRect` | `QCPPolarArea` (new; takes the layout role `QCPPolarAxisAngular` has today) | a `QCPAbstractArea` subclass |
| **Axis**: one dimension: range, scale type, reversal, ticker, `rangeChanged`, drag/zoom opt-outs, range link, part selection | `QCPAxis` (a straight line on a side) | `QCPPolarAxisAngular`, `QCPPolarAxisRadial` (plain axes, no layout role) | an axis-base subclass |
| **Coordinate system**: what a plottable or item is drawn in: one key axis and one value axis of one area | `QCPCartesianSystem` | `QCPPolarSystem` | a `QCPCoordinateSystem` subclass |

`QCPAbstractArea` is a `QCPLayoutElement` that owns axes and systems and keeps the per-system lists
below. `QCPAxisRect` derives from it. Because a layout element is already a `QCPLayerable`, no class is
both an area and an axis (F1): the angular axis loses its layout role before it gains the axis base.

### 2.2 The axis base

The range model the polar axes duplicate today (P-4) moves into one base under `QCPAxis`: range and its
setters, scale type, reversal, ticker and tick vectors, `rangeChanged(newRange, oldRange)`,
`scaleTypeChanged`, drag/zoom opt-outs (M4d), part selection (M4a), dirty rules, `area()`.
`QCPAxis` keeps everything about being a straight line on a side; the polar axes keep their circle and
spoke geometry. Every 2.x `QCPAxis` signature stays.

Links (M4d) are range relations (`range = forward(source range)`) and stay on the base. How a linked axis
maps *pixels* (today: the source's pixel fraction along its own length) is the system's business:
the cartesian system keeps today's rule; another system either defines its own or refuses a linked axis
as misuse (F16). `moveTarget`/`pixelOnMoveTarget` stay cartesian.

### 2.3 `QCPCoordinateSystem` — the public contract

```cpp
class QCP_LIB_DECL QCPCoordinateSystem : public QObject
{
public:
    QCPAbstractArea* area() const;
    QCPAxisBase* keyAxis() const;
    QCPAxisBase* valueAxis() const;
    bool usesAxis(const QCPAxisBase*) const;

    virtual QCPTransform transform() const = 0;     // the frame; see 2.4 (taken once per paint)
    virtual QRect clipRect() const = 0;             // pixels: buffers, damage, GPU scissor
    virtual QPainterPath clipPath() const;          // painter replay; default: clipRect()
    virtual bool contains(QPointF pixel) const;     // hit area; true while the frame is invalid (F19)
    virtual QCPTransform iconTransform(const QCPDataBox&, const QRectF&) const; // legend icons (F7)
    virtual QCPKeyWindow keyWindow() const;         // resolution window; see 2.4 (F6)
    virtual Capabilities capabilities() const = 0;  // 2.6
    quint64 revision() const;                       // bumped on any mapping change (F4)

    QList<QCPAbstractPlottable*> plottables() const;  // maintained on attach/detach (F8)

signals:
    void changed();                                 // the mapping changed: dirty, re-resolve
protected:
    void bumpRevision();                            // subclasses call it when their parameters move
};
```

Validity of a recording compares `(system, revision)`; equivalence (`isTranslationOf` and the custom
`equivalentTo`) is only ever used for reuse (layer shift, raster shift), never for correctness (F4).

### 2.4 `QCPTransform`: one frame type, three mappings

`QCPTransform` stays the frame type of the drawing API (replay, extents, GPU, resamplers) and holds a
`std::variant` of:

- **Cartesian**: today's two-axis snapshot, unchanged, non-virtual (D3). It names its axis rect
  explicitly instead of discovering it per axis (F3); the `(QCPAxis*, QCPAxis*)` constructor stays.
- **Polar**: centre, radius, angular range, offset and direction, radial range, scale and reversal;
  batch map, inverse; radius clamped to the range (a pure per-point clamp, no topology).
- **Custom**: `std::shared_ptr<const QCPCustomMapping>` with virtual *batch* `map`, `inverse`,
  `equivalentTo`, plus the system revision it was taken at.

Every entry point dispatches once per call (one command, one span), never per point (C-6). The cartesian
mapping also exposes what the resamplers' fast path reads today (`keyIsLinear`, `valueIsLinear`, ranges,
key verticality, axis rect geometry), so `AffineTransform`/`PixelPointMap`/`PixelBucketGrid` keep their
inline arithmetic; a non-cartesian system routes plottables to the generic path (F17).

The per-paint **frame memo** (`drawable.cpp:410-463`) is keyed on `(plot, frame epoch, system,
revision, clip)` and owns the transform; objects of one system in one paint share it (F2).

`QCPKeyWindow` (one pixels-per-key) is defined by the system: cartesian keeps today's formula; polar
answers with the outer radius' arc length per angular unit (documented), which only drives recording
resolution (F6).

### 2.5 Plottables

`QCPAbstractPlottable` holds a strong reference to its system (owned by the area, 2.7).

| Call | Effect | Misuse |
|---|---|---|
| `QCPAbstractPlottable(QCPCoordinateSystem*)`, `setSystem(s)` | the primitive | `s` in another plot; missing capability (2.6) |
| 2.x `T(QCPAxis* key, QCPAxis* value)`, `setKeyAxis`, `setValueAxis` | resolve the cartesian system of that pair on the axes' rect (created on first use, reused after) | same as today: null pair inert, cross-plot, same orientation (now a real misuse, F26) |
| `keyAxis()`, `valueAxis()` returning `QCPAxis*` | a projection: the cartesian system's axes, null in any other system (F10, F23) | — |
| `system()`, `keyAxisBase()`, `valueAxisBase()` | the system and its axes, any system | — |

The protected `mKeyAxis`/`mValueAxis` stay until milestone 5 as the same projection, kept in sync by
`setSystem` (out-of-tree 2.x subclasses keep compiling, F23). `axesChanged()` becomes `systemChanged()`
(old name kept as a call until milestone 5); the five plottables that wire axis signals (F18: graph2,
histogram, histogram2d, colormap2, multigraph) rewire to the system's axes and `changed()`.

Through the system: `view()`, `clipRect()`, `coordsToPixels`/`pixelsToCoords`, the hit gate
(`system->contains`), the selection rect (through `inverse`), rescale. Range queries already answer from
the recorded data box (C-1.2).

`add<T>` (F20): `area->add<T>()` uses the area's default key and value axes; `area->add<T>(key, value)`
is explicit; `plot.add<T>()` keeps today's rule (the first axis rect's bottom and left axes) and reports
misuse when the plot has no axis rect.

### 2.6 Capabilities and relationships

Per class, checked when a plottable joins a system (attach, `setSystem`, `take`/`adopt`): a flag set the
system offers (`Rectangular`, `StraightKey`, `ColourScale`, `Lanes`, ...) against a static set the class
requires. Missing → misuse, never silent wrong drawing.

Relationships, checked where they are made (F11): channel fill (both graphs in one system), bars stacking
and bars groups (one system), error bars (their data plottable's system), a colour scale and its users
(one area).

The cartesian system offers everything. The polar system offers points, lines, scatter, fills defined in
polar coordinates (2.8) and error bars along the radius. A custom system declares its own.

### 2.7 Ownership and lifetime (F9, F27)

- An area owns its axes and its systems. A system is created on first use for a (key, value) pair of the
  area's axes and reused after (one per pair, never a second, F9d).
- Deleting an axis deletes the systems that use it, after detaching their plottables and items with the
  milestone 1 rule (unlist, then delete); those become inert like a plottable whose axis was removed today.
- Many-to-one is normal: one angular axis, several radial axes, one system per radial; axis queries are
  unions over systems (F27).
- `take`/`adopt` and moving an area between plots carry the area's systems with it; attaching a plottable
  or item checks that its system's area is in the same plot (F9c).
- `QCPAxisBase::plottables()`/`items()` are "attached to a system that uses this axis"; for cartesian the
  list and order are today's (F22). The plot enumerates areas, not only axis rects, for `rescaleAxes`,
  `plottableAt`, selection and dirty propagation (F21). A system's `changed()` marks its area's affected
  layers (F8).

### 2.8 Polar

- `QCPPolarArea` owns one angular axis and one or more radial axes, computes centre and radius on layout
  (P-3), handles drag (rotation, radial pan) and wheel (radial zoom about the cursor), and draws the
  polar grid as the cartesian area draws its grid.
- `QCPPolarGraph` stays, as a real plottable in a `QCPPolarSystem` (F12, F14): it keeps its periodic
  wrap, radial clamp, fill to the centre and legend row, and records. The seam split at 0/360 and the
  inserted vertices happen **at record time** (topology), the radius clamp at mapping time (F13).
- Other plottables join polar only when they declare it (2.6); `QCPMultiGraph` and `QCPCurve` stay
  cartesian in this milestone (their lanes and hit gates are rect-based, F12).
- Clip: the GPU scissors to the disc's bounding rect; a polar graph's geometry stays inside the disc
  because its radius is clamped; the painter replay clips to the disc path (F5). A polar export
  reference image checks the corners.
- Axis click selection works (P-8).

### 2.9 Items

A position binds to a system (`setSystem`); `setAxes(key, value)` stays for cartesian. Plot coordinates
resolve through the system's transform; `ptAxisRectRatio` becomes "area ratio" (polar: angle fraction,
radius fraction). The item index groups items by system instead of by axis pair (C-3).

### 2.10 Colour scale

The colour scale stays a cartesian-area feature in 3.0 (`ColourScale` capability, offered only by
`QCPAxisRect`), with `QCPAbstractArea::colorScale()` nullable as the extension point. A polar or custom
colour scale is later work (F15).

## 3. Performance

- Cartesian: today's arithmetic behind one dispatch per call; the frame memo keyed on the system; the
  resamplers' fast path unchanged. A cartesian pan stays a pixel translation.
- Polar: a rotation or radial zoom re-resolves recordings, never re-records them (except the seam split,
  recorded once per data change and angular-range change that moves the seam).
- Gate: every bench row within 5 % (drawing) / 15 % (others) of the build before each step, A/B,
  steady state (`-iterations`). New rows before the work starts (F25): many plottables in one system,
  many items in one system, cartesian pan (reuse) and zoom (remake), polar draw 10k, polar rotation,
  polar radial zoom, polar export; a custom-system row once it exists.

## 4. Order of work (each step ends green and measured)

1. **Baseline**: polar reference images on the old code (`docs/snippets/polar.cpp` scenes, the gallery
   scene, the example), the new bench rows.
2. **Areas**: `QCPAbstractArea` under `QCPAxisRect`; `QCPPolarArea` takes the layout role of the angular
   axis (polar API change, 5).
3. **Axis base** under `QCPAxis` and the two polar axes; polar range model deduplicated; real polar
   axis selection.
4. **Transform**: the variant; polar and custom mappings; tests (map, inverse, clamp, equivalence,
   revision).
5. **Systems**: `QCPCoordinateSystem`, `QCPCartesianSystem`, `QCPPolarSystem`; ownership and lifetime
   (2.7); the frame memo; capability and relationship checks.
6. **Plottables, mechanical**: the recording plottables hold a system (most only need `view()`).
7. **Plottables, painter paths**: `QCPGraph`, `QCPGraph2`; `systemChanged` rewiring (F18, F24).
8. **Plot level**: area enumeration, `rescaleAxes`, `plottableAt`, selection rect, `construct.h` `add<T>`.
9. **Polar graph** as a plottable in `QCPPolarSystem`; grid, drag and zoom on the area.
10. **Items** on systems; the item index by system.
11. **Extension point**: documented contract; a toy third system with a test matrix (F28): graph,
    scatter, item × take/adopt, area moved between plots, axis deleted mid-drag, system switched;
    `QCPBars` refused; legend icon; rescale.

These become milestone plans 4f-4i (proposed: 4f = steps 1-4, 4g = 5-8, 4h = 9, 4i = 10-11).

## 5. 2.x compatibility

- Cartesian 2.x code compiles and behaves the same: axis-pair constructors, `keyAxis()`/`valueAxis()`
  (never null for a cartesian plottable), the protected members, `setKeyAxis`/`setValueAxis`, `addGraph`,
  `QCPAxisRect`. A same-orientation key/value pair becomes a reported misuse instead of a debug message.
- Polar was a QCustomPlot 2.1 tech preview (P-7). `QCPPolarAxisAngular` stops being a layout element:
  code adds a `QCPPolarArea` to the layout and gets the angular axis from it. `QCPPolarGraph` keeps its
  name and its constructor from (angular, radial) axes. Listed in the changelog and the migration page.

## 6. Questions for the maintainer

1. **Items** (2.9): in the same release as plottables (steps 10-11 before 3.0 ships, as proposed), or
   is a later 3.x acceptable?
2. **Polar 2.1 API** (5): break it now (the area must exist before the axis base, F1), or keep it
   compiling to milestone 5 with a forwarding shim?
3. **Names**: `QCPAbstractArea`, `QCPPolarArea`, `QCPCoordinateSystem`, `QCPCartesianSystem`,
   `QCPPolarSystem`, and for the axis base `QCPAxisBase` (the reviewer's preference: `QCPAxis` is already
   the concrete class) or `QCPAbstractAxis`.
4. **Milestone split** (4): four sub-milestones 4f-4i as proposed?
