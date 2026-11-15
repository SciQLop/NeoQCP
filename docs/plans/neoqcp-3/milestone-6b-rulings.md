# NeoQCP 3.0 — milestone 6b rulings and deferred items

Milestone 6b: construction. Items and plot types are built without a plot (`T(args...)`), `add<T>(args...)` places
them, built-in plot types take their data in the constructor, and `ax->line`, `ax->text`, `ax->ellipse` exist. Branch
`neoqcp-3.0`, `4ab1d35..` the commit that adds this file (not pushed). Plan:
`docs/superpowers/plans/2026-11-15-neoqcp-3-milestone-6b.md` (calls B-1..B-7). Implementer: opencode (DeepSeek V4.1
Flash, auto mode); task and final reviews by Claude subagents.

## Results

- Offscreen 4153 passed, 0 failed, 135 skipped (Coder, at 6541745). Whole ASan + UBSan suite 4153/0/135, no sanitizer
  report (leak checks off, as in 6a).
- Local Qt 6.12 picture suite (Xvfb): every test binary OK.
- clang `-fsyntax-only` clean on the 105 changed `.cpp` files in the local compile database (three example and
  doc-snippet files are not in it; they build in the Coder tree with examples on).
- Examples build; `check-sources.py`: the file lists match; guide snippets pass (`doc-snippets`).

## Rulings

- R-1: the changelog rows for `QCPNumberRange` and `QCPAxisRect::xAxis()` / `yAxis()` sit in the items entry; Task 1
  was additive.
- R-2: an item starts with clipping to its rect on, and adoption no longer forces it: a caller who turned clipping off
  before the add keeps that.
- R-3: a detached item's `setPixelPosition` does nothing (no plot to convert through).
- R-4: a plottable built without a plot keeps its data and defers the transform until it joins one; it never resamples
  inline on the GUI thread. One deferred slot: a newer job replaces the waiting one.
- R-5: a `QCPIntervals` arriving without a lane layout gets the default layout the old add made. A default layout is
  made again on arrival in another plot (or after its plot died); a layout the caller gave is never replaced.
- R-6: user-facing code (`docs/`, `documentation/doc-code-snippets`, `examples/`) uses handles and the data
  constructors; `.get()` only in tests and the doc-image generator.
- R-7: what the user set before the add wins. The defaults applied at the add (the theme's text colours, a rect colour
  scale's gradient and data range) fill only what was never set. `setColorScale()`, which names a scale, still takes
  that scale's values whole. Re-theming a plot still recolours every text item (pre-existing policy).
- R-8: `ax->text()` applies every `QCPTextStyle` field. An invalid `QCPTextStyle::color` (the new default) means the
  foreground of whoever draws: the theme's colour for an item, the painter's pen for a recording.
- R-9: `QCustomPlot::add` accepts an item on a plot with no axis rect (it used to refuse).
- R-10: an item's add opens an emission scope, as a plottable's does: an item's own slots run after the handle exists.
- R-11: an intervals' lane-label layerable belongs to the plot it draws in; a move to another plot rebuilds it.

## Carried to later sub-milestones

- `QCPTextStyle::mathText` has no receiver on `QCPItemText`: 6c.
- `anchor + QCP::PixelOffset` corners: 6c. Short calls for other types (`colorMap`, `fillBetween`): 6f/6g.

## Deferred minors

- Data-container constructors, and `IndexableNumericRange` vs `QCPNumberRange` (two concepts for one idea); n11's
  `qcp::numbers` hook replaces both.
- `QCPGraph(keys, values)` copies twice (into vectors, then `setData`); a `qcp::toDoubles` helper would do it once.
  A take view over an unbounded iota does not compile there (needs a common range).
- `QCPPlottable1D(keys, values)` truncates to the shorter range, while `setData` reports a length mismatch.
- Arrival hooks assume one plot: `QCPColorMap2` keeps listening to its first plot's layout after a move.
- Default lane layouts accumulate when a plottable moves back and forth between plots.
- Refusal comes after a partial adopt (the check could run first); an item position bound to another plot's axes
  before the add keeps them.
- Stale constructor Doxygen naming `keyAxis` in curve, bars, error bars, statistical box, financial, colour map.
- More Review Focus 1/2 tests for plottables (every built-in type used detached).
- A user plottable whose first constructor argument converts to `QCPAxis*` goes to the `add<T>(key, value, ...)`
  overload.
