# User-code targets (milestone 6)

The programs a user should be able to write with NeoQCP 3.0. They do not compile yet: the library changes until they
do (spec `docs/specs/2026-11-14-neoqcp-3-milestone-6-design.md`). Every name they assume that does not exist today is
in `docs/plans/neoqcp-3/m6-vocabulary.md`. Each folder has the C++ target and a Python sketch of the same program.

| Target | What it shows | Survey row it answers |
|---|---|---|
| `lollipop/` | A plot type the user writes: stem and disc per point, click picks one lollipop and it is drawn thicker, a one-lollipop legend icon, created with `add<QCPLollipop>(month, rainfall)` | extension survey 1 (lollipop), pattern 4 (no per-datum identity) |
| `asteroids/` | Annotations the user derives: rotated polygons in data units, held keys without a click, wrap-around, a score on top of everything, a game-over banner, a timer as the frame clock | extension survey 8 (Snake, replaced by Asteroids), patterns 3 (call order) and 7 (keys) |
| `brick-breaker/` | 72 static bricks, a ball, a paddle dragged by the user's own mouse hook, collisions in data units from the items' recorded geometry, a hit brick removed alone | extension survey 9 (brick breaker), patterns 7 and 8 |
| `callout/` | An annotation mixing data and pixel positions: the tip on a data point, a rounded box in pixels from it, dragged and resized from a corner handle, with `tip` and `box` anchors other items hang on | extension survey 2 (callout), pattern 6 (coordinate gaps) |
| `peak-fit/` | Three peak annotations, centre dragged and width resized from two edges; a plain function refits; the model and a residual panel below, sharing x, update with no sync code | extension survey 6 (drag on own points), mpl survey 4 (shared axes) |

Why the lollipop derives from the 1D base, `QCPAbstractPlottable1D<QCPGraphData>`: it needs the least code. The base
already keeps sorted (key, value) points, per-point selection and the legend row, so the class is `draw()` and the
legend icon. `QCPBars` would bring a width, stacking, bar groups and a bar-rectangle hit test that a lollipop has to
undo; `QCPGraph` brings a resampled multi-column line pipeline that a `draw()` override would bypass.
