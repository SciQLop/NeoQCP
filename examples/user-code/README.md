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
| `shared-x/` | Two signals on two panels sharing time: `addAxisRect({ .shareX = ax1 })` links x, hides the upper tick labels and aligns the margins in one call | mpl survey 4 (shared axes) |
| `symlog/` | Three panels with symmetric log axes, linear near zero; `setScale(QCPScale::symlog(0.015))` sets the mapping and its ticks in one call | mpl survey 3 (symlog) |
| `inset-zoom/` | An image with an inset zoomed on a sub-range; `zoomIndicator(inset)` draws the box and its two connectors and follows both rects' ranges | mpl survey 9 (zoom inset) |
| `fill-between/` | Two lines and the fill between them, green where the first is above and red where it is below, cut at the exact crossings; Up/Down move a line and the fill follows | mpl survey 20 (fill_between where=) |
| `spectrogram/` | A spectrogram panel above the signal, sharing time; log frequency axis and log power, the colour bar beside the spectrogram only | mpl survey 25 (specgram) |
| `annotations/` | matplotlib's fourteen connector styles, one panel each, and in the spare cell a text with its arrow as one annotation; arrows stop 5 px short of both ends | mpl survey 10 (connection styles) |

Why the lollipop derives from the 1D base, `QCPPlottable1D<QCPGraphData>`: it needs the least code. The base
already keeps sorted (key, value) points, per-point selection and the legend row, so the class is `draw()` and the
legend icon. `QCPBars` would bring a width, stacking, bar groups and a bar-rectangle hit test that a lollipop has to
undo; `QCPGraph` brings a resampled multi-column line pipeline that a `draw()` override would bypass.

## Line counts against matplotlib

Code lines: blank and comment-only lines left out, `#include` / `import` lines and the C++ `main` frame kept. The
matplotlib count is the gallery code that draws the same picture, its imports included.

| Target | NeoQCP C++ | Python sketch | matplotlib | Where the difference goes |
|---|---|---|---|---|
| `shared-x/` | 32 | 24 | 13 | 9 lines of C++ frame (`QApplication`, includes, `show`, `exec`); data built in a loop instead of numpy |
| `symlog/` | 38 | 31 | 21 | the frame; two lines hide the grid of the middle panel (matplotlib's default grid is off) |
| `inset-zoom/` | 48 | 33 | 17 | 14 lines rebuild matplotlib's sample image (`get_sample_data("axes_grid/bivariate_normal.npy")`) |
| `fill-between/` | 29 | 29 | 14 | the frame; 7 lines for the Up/Down keys that show the fill following (not in matplotlib) |
| `spectrogram/` | 70 | 32 | 20 | 27 lines are the DFT that matplotlib's `specgram` computes; the plot itself is 15 lines |
| `annotations/` | 66 | 51 | 35 | a 14-row table, one line per style; the spare cell's text annotation (8 lines, not in matplotlib); hiding the four axes' grid |
