# NeoQCP 3.0 — milestone 4j rulings and deferred items

Milestone 4j: step 11 of the coordinate-systems design, with D9. A user writes a coordinate system of their own,
and `QCPGraph`, `QCPCurve` and items draw, hit, select, rescale and survive lifetime events in it. Plottables
that need a rectangle are refused with a misuse report. Branch `neoqcp-3.0`, `3abbec0..` the commit that adds
this file (local, not pushed). Plan: `docs/superpowers/plans/2026-11-11-neoqcp-3-milestone-4j.md` (calls J-1..J-8,
amendments A1-A12 after an opencode plan review, `.superpowers/m4j-plan-review.md`). Inventory: `m4j-inventory.md`.
Contract page: `docs/coordinate-systems.md`. The maintainer said "push and continue with 4j" (2026-11-11);
everything below was decided by the controller.

## Results

- Offscreen suite 2406 passed, 0 failed, 135 skipped; Xvfb whole binary 2541/0/0; whole ASan suite clean after
  the last code change; clang clean on every changed `.cpp`.
- Bench: `baseline.md`, "End of milestone 4j".

## Rulings made while planning

- R-1 (J-1): the new capability is `capStraightSegments` (0x10). Cartesian systems offer it, polar does not.
  `QCPGraph` and `QCPCurve` need only it. Every other plottable keeps needing a rectangle and a straight key.
- R-2 (J-2): `keyAxis()`/`valueAxis()` (plottables and item positions) are set only on a cartesian system, so 2.x
  pixel code cannot draw a user system as a rectangle. `keyAxisBase()`/`valueAxisBase()` reach the axes on any
  system. Cost: out-of-tree code reading `keyAxis()` gets null on user systems.
- R-3 (J-3): a user system lives on a `QCPAxisRect` subclass that overrides `createSystem`. Subclassing `QCPArea`
  is documented, not tested. A system's own parameters go through the new `parametersChanged()`.
- R-4 (J-4): `QCPGraph` keeps its cartesian path unchanged and gains a data-space path for other frames: lines,
  steps, impulses, scatters, fills and channel fills are built in data space and mapped once. No decimation, no
  visible-range cut and no key window on that path (`simplify:` notes).
- R-5 (J-6): culling stays cartesian-only (hit test box gate, item index). A user system is scanned in full.
- R-6 (J-7): an area does not move between plots; adding an element of another plot to a layout is a misuse,
  also through a nested plot-less layout.
- R-7 (J-8): no `area->add<Item>()`.

## Rulings made during execution (what, why, cost if wrong)

- R-8 (Task 1): a shear change replays the same recording through the new transform; the test asserts the moved
  pixel and an unchanged recording count. Cost: none.
- R-9 (Task 1, A11): the capability mask is stored and passed up the constructors (default `capRectangular |
  capStraightKey`), and `requiredCapabilities()` is a non-virtual accessor. A virtual cannot be asked in a
  constructor, and the 2.x axis-pair constructor joined user systems unchecked. Cost: out-of-tree overrides of
  `requiredCapabilities()` must pass the mask instead.
- R-10 (final): A12 is withdrawn. Polar graphs had their own rubber-band selection before 4j; the test stays as a
  pin, with no changelog line.
- R-11 (final): `capRectangular`, `capStraightKey`, `capLanes` and `capColourScale` are honoured only from a
  cartesian system, because the plottables needing them read the 2.x axis pair. Cost: a user system that wants
  bars must subclass `QCPCartesianSystem`.
- R-12 (final): an anchored plot-coordinate position on a non-cartesian system resolves as
  `map(inverse(anchor) + coords)`, which equals 2.x on a linear cartesian frame. Cost: one formula.
- R-13 (final): a plottable refused at construction is on no layer and in no list; `plot->add<T>()` and `adopt`
  return an empty handle; a later successful `setSystem()` registers it as `adopt` does. Cost: none known.
- R-14 (final): the bracket selection decorator stays off on user systems (documented limit).
- R-15 (final): an item position whose system dies goes inert without a per-item report (the 4i behaviour).

## Found and fixed in review

- Plan review (opencode): a click on a sheared graph would have crashed (the graph's line builder read a null key
  axis), a shear change would not repaint items on an axis rect, `keyAxisBase()` was missing, three side tests
  still compared projections, the channel-fill rule had no home, and the area-move refusal sat after the element
  was taken from its layout. All folded in as A1-A10 before execution.
- Task 1: the axis-pair constructor bypassed the capability check (R-9).
- Task 3: a use-after-free in `parametersChanged()`. It bumped the revision first, the change signal let a slot
  delete the axis and with it the system, and the function then read the freed system. Layers are now marked
  first and the revision moves last.
- Final review (fresh Opus reviewer and fresh opencode reviewer, one batch, each fix with a test that failed
  first unless noted):
  - `setAxes` still handed item positions the 2.x projection on a user system, so items built on the plot's
    default axes were drawn sheared but anchored and dragged in cartesian pixels; interactive creation landed at
    the unsheared point.
  - A plottable refused at construction stayed on its layer, a refused graph stayed in the graph list
    (`clearGraphs()` never returned), and `plot->add<T>()` and `adopt` succeeded after reporting the refusal.
  - A user system declaring `capRectangular` admitted bars that then drew nothing.
  - `plottable->rescaleAxes()` did nothing on a user system.
  - A selected data-space channel fill repainted the whole band once per segment.
  - An anchored position on a user system resolved to pixel (0, 0).
  - The data-space fill base went through `inverse()` (NaN for unrepresentable pixels) and closed on the wrong
    side on a negative log range; log impulses reached the painter as NaN pairs.
  - A nested plot-less layout carried an area into another plot.
  - A channel fill whose partner moved to another system kept drawing a band across two frames.
  - Older than 4j: `getVisibleDataBounds` left its iterators uninitialised without axes (undefined behaviour;
    the test is a pin, it passed by luck before the fix).
  - A curve hit mapped one point at a time; two stale header comments; a misplaced doc block.

## Deferred (also in backlog.md)

- Culling and decimation on user systems (a system virtual mapping a pixel rect to a data box; decimation by
  `keyWindow()` in data space).
- The bracket selection decorator on user systems; a tracer on a polar graph; `area->add<Item>()`; a report when
  an item position's system dies; a curve following the rect's colour scale on a user system.
- The Xvfb `QCPDrawn_*` rows are bimodal on this machine and cannot decide a 5 % gate.
