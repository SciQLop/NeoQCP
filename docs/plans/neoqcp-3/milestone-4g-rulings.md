# NeoQCP 3.0 — milestone 4g rulings and deferred items

Milestone 4g: steps 5-8 of the coordinate-systems design. `QCPCoordinateSystem` (public) with
`QCPCartesianSystem` and `QCPPolarSystem`, one per axis pair, owned by their area (`area->system(key, value)`);
every plottable drawn in a system (`system()`, `setSystem()`; the 2.x axis-pair API is the cartesian
projection); capabilities and relationships checked; rescale, hit search, rubber-band selection, legend icons
and `add<T>` through areas and systems. Branch `neoqcp-3.0`, `048ad21..` the commit that adds this file (local,
not pushed). Plan: `docs/superpowers/plans/2026-11-10-neoqcp-3-milestone-4g.md` (calls G-1..G-7, amendments
A1-A20). The maintainer delegated the plan approval and every call for the night of 2026-11-09/10 ("make
decisions for me so you won't get stuck"); everything below was decided by the controller.

## Results

- Offscreen suite 2269 passed, 0 failed, 135 skipped; Xvfb whole binary 2404/0/0; ASan whole suite clean;
  clang clean on every changed `.cpp`.
- Bench: the whole table was taken under load 3-15 (other sessions) and reads 10-50 % slow on many rows, an
  untouched control row (`QCPColorMapData_ConstructLarge`) included. A/B against a build of 048ad21 (the
  milestone's base), steady state, settles them: drawing rows level (`QCPItems_Pan10k`, `QCPItems_Replot10k`,
  `QCPPlottables_Pan200`, `QCPDrawn_Zoom10k`, `QCPDrawContext_ReplayData1M`, `QCPCoords_CoordToPixel1M`,
  `QCPTransform_Map1M` +1 % over eight pairs, `QCPPolar_Draw10k`), `QCPAxis_SetRange10k` +5 % (gate 15 %) after
  39d7028.

## Rulings made during execution (what, why, cost if wrong)

- R-1 (Task 1): a pair is ordered: `(value, key)` is a second system with a vertical key (G-1 as written).
- R-2 (Task 2): capabilities are checked in `setSystem`, not in the base constructor (a virtual cannot be
  called there); 2.x axis-pair constructors always give a cartesian system. Cost: a 4h class with a system
  constructor checks in its own constructor.
- R-3 (Task 2): frame epochs come from one process-wide counter, and (final) so do system revisions: a frame
  memo keyed on addresses could otherwise hand a new plot or system the frame of a freed one (the
  intermittent `pixelAnchoredShapeIsHittable` failure since 4e; reproducer: 199 of 200 plots shared an epoch).
- R-4 (Task 3 and final): relationships follow 2.x where 2.x drew correctly: channel fill, bars groups and
  error bars need the same area; stacking needs the same system. Across areas or plots they are misuse
  (2.x printed a debug message or did nothing). Changelog.
- R-5 (final): a bar leaves its stack and its group only when it moves to another area or plot, or loses its
  system for good; a two-step 2.x axis swap within one rect keeps them.
- R-6 (Task 4): a system's `changed()` marks no layers: marking stays where the change happens (axis range
  and scale paths, `contentChanged`). Systems also bump for geometry and data swaps, and a connection
  scheduled replots nobody asked for. Cost: a custom system's own parameter change must mark its plottables
  itself (to document with the extension point in 4i).
- R-7 (final): a system's `changed()` and an axis' `rangeChanged` are delivered after the setter, never in
  the middle; the emission scope is opened only when someone listens (listener counts), so an unwatched
  `setRange` costs what it cost before (39d7028: +50 % -> +5 %).
- R-8 (final): a deliberate `setSystem(nullptr)` is silent; only a system lost under a plottable reports,
  once.
- R-9 (final): a plottable built from a misused axis pair (axes of two rects, same orientation) is owned by
  the plot, listed, inert and reported — never an orphan.

## Found and fixed in review

- Task 1: none. Task 2: the frame-memo collision (R-3). Task 3: the frame memo asked every system for a ratio
  transform, a misuse for polar (gated on `capRectangular`); bars moved to another system kept a stale stack
  and group.
- Final review (fresh Opus reviewer and fresh opencode reviewer; one fix batch, each fix with a test that
  failed first): a refused `QCPBarsGroup::insert` moved index -1 (out-of-bounds write in release builds);
  relationships stricter than 2.x (yAxis/yAxis2 groups and channel fills refused); misused axis pairs
  orphaned their plottable (leak, missing from `graphs()`); a polar area did not mark its plottables' layers;
  `changed()` could run in the middle of an axis setter (a slot deleting the axis: use-after-free); take/adopt
  bypassed `setSystem` (the plottable kept listening to the old plot's axes); a moved plottable kept the old
  area as parent (visibility, onlyVisible rescale); the 2.x axis swap lost bar stacks and groups; system
  revisions could collide at a recycled address; the painter ignored the disc clip; an axis-ratio position in
  a polar frame was dropped silently; `iconTransform()` was never called; rescale on a non-cartesian
  plottable printed a debug message; rubber-band selection changed the 2.x order; hygiene.
- End checks: `QCPAxis_SetRange10k` +55-60 % (R-7).

## Deferred minors (also in backlog.md)

- `QCPMultiGraph` requires `capColourScale` (it is a `QCPAbstractLine`) besides `capLanes`.
- Rubber-band selection over a polar area selects through the disc; `rescaleAxes` moves the radial axes of
  polar plottables with data (both follow the contract; changelog).
- A listener connected and disconnected within one setter lets that one emission take the direct path
  (no in-tree code does this).
- `defersEmissions()` is O(systems of the area) per mapping change.
- Resolving an item position outside the drawing path needs the new `report` flag to stay quiet.
- The colour-scale relationship check (a colour map using another rect's scale) is deferred with design
  section 2.10.
