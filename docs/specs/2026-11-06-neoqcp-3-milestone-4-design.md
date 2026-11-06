# NeoQCP 3.0 — milestone 4: port everything

The authority is the object-model spec (`2026-11-02-neoqcp-3-object-model-design.md`, decisions 5 and
11-15, answers 5 and 7) and the drawing-API spec (`2026-11-04-neoqcp-3-drawing-api-design.md`). This
document only adds what those leave open for milestone 4: the order of work and the calls below. The
code inventory is `docs/plans/neoqcp-3/m4-inventory.md`.

## Intent

After milestones 1-3 the new model exists, but only bars and text labels use it. Milestone 4 moves every
remaining class onto it, so that milestone 5 can delete the old API. Nothing new for its own sake:
each step makes an existing feature follow a rule the spec already set.

Goals: every plottable and item draws through the drawing API; one line-plottable family; one
colour-scale relation; axes, linked axes and alignment as decisions 12-13 say; one selection model;
every signal through deferred delivery; drawing speed kept (each sub-milestone ends with the bench
table, gate 15 % per row against its start, the drawing rows 5 %).

Non-goals: the data model and threading (project 3), the API-surface cleanup (project 4), removing the
old API (milestone 5), Qt Canvas.

Anti-goals: a sub-milestone that leaves the suite red or a class half on each API at a release point;
a second way to do something kept "for compatibility" inside the new model (compatibility is the old
API's job until milestone 5).

## Maintainer decisions

- **The line family is two classes** *(2026-11-06)*: one graph built from today's `QCPMultiGraph` (one
  key column, N value columns, async resampler; a single line is one column), and `QCPCurve` for
  parametric data (x, y, t). `QCPGraph` (container) and `QCPGraph2` go; their users move to the graph
  with one column, as SciQLopPlots did in #113. Both share the decision 11 base.
- **New data replaces old data at once by default** *(2026-11-06)*: `setData` on a visible graph is
  seen by the getters, `rescaleAxes`, hit tests and exports when it returns, as 2.x `QCPGraph` does.
  Today's multigraph behaviour (keep drawing the old data while the new is prepared, then swap a
  debounce later, so a pan never flashes blank) becomes an option the client turns on; SciQLopPlots
  turns it on. Below the resample threshold (100 000 points) nothing is resampled, as today.

## Order (six sub-milestones, each its own plan, each leaves NeoQCP green)

1. **M4a — signals and selection.** The ~105 direct emits move to `deliver`; selection gets one
   shape (call 2). First because it touches the most files shallowly, and later steps build on
   signals that will not move again.
2. **M4b — the line family.** Decide R3 (call 4), then the shared base, the graph and the curve on
   the drawing API, waterfall on the graph. Biggest and riskiest for speed; M4c needs its base.
3. **M4c — colour maps, histograms, the colour-scale relation.** One relation in a base
   (decision 14); `QCPColorMap`, `QCPColorMap2`, `QCPHistogram2D`, `QCPHistogram` on the drawing API;
   the colour-map texture layer generic (images on the GPU); the graph's colour-by-scalar uses the
   relation; `QCPColorScale::rescaleDataRange` sees every user.
4. **M4d — axes and alignment.** Drag and zoom on by default with per-axis opt-out, linked axes on
   the transforms, the rect-owned colour scale, then the alignment decision (call 5).
5. **M4e — the rest.** Error bars, financial, statistical box, intervals, the remaining items, one span
   class, and folding the `QCPDrawn` adapter into the bases.
6. **M4f — polar** on the object model (answer 10 below).

## Calls made here (the maintainer can overrule any)

1. **Kept classes.** `QCPErrorBars`, `QCPFinancial`, `QCPStatisticalBox`, `QCPIntervals` and the 1D
   `QCPHistogram` stay and are ported; the spec does not ask to remove them. The graph's per-component
   error bars and `QCPErrorBars` both stay: one draws a graph's own columns, the other attaches to any
   1D plottable.
2. **Selection shape (decision 15).** Every selectable object (plottable, item, axis, legend, legend
   row, text element, polar parts) has `isSelected()`, `selectable()` and one signal
   `selectionChanged()` without arguments (decision 5: no overloads, no `QOverload`). The detail stays
   a typed getter where an object has one: `selection()` (`QCPDataSelection`) on a plottable,
   `selectedParts()` on an axis and a legend. The plot keeps `selectionChangedByUser()`. The 2.x
   overloads with arguments go in milestone 5 with the old API; until then they are emitted alongside,
   through the same delivery.
3. **Names during the move.** Milestone 4 builds the graph inside `QCPMultiGraph` and marks
   `QCPGraph` and `QCPGraph2` deprecated; the rename of `QCPMultiGraph` to `QCPGraph` happens in
   milestone 5, when the old class is deleted, so no release has two classes with one name.
4. **R3 (typed batch commands)** is decided at the start of M4b, from the format survey
   (`m3-command-optimisation-survey.md`), with one measurement on the line rows; the survey's
   recommendation stands unless the measurement contradicts it.
5. **Alignment.** M4d measures again, on a real GPU, why one plot with many axis rects is slow
   (answer 5; `multi-axisrect-perf.md`). If the per-layer repaint makes stacked rects fast enough,
   stacked rects in one plot are the way to align and alignment groups are not built; otherwise
   decision 13's groups are. The maintainer sees the numbers before either is built.
6. **Spans.** One span class with an orientation replaces the H and V spans; whether the third span
   class joins it is decided in M4e from its code.

## Answers to the builder check (calls, 2026-11-06)

7. **Scope of "every class".** M4 ports what users extend and stack: plottables and items. Axes,
   grids, the legend, layout elements, the selection rect and the overlay keep their internal
   `draw(QCPPainter*)`; it is not the extension API. `QCPLayerable::draw(QCPPainter*)` survives as an
   internal virtual, so folding `QCPDrawn` into the plottable and item bases does not make
   `draw(QCPDrawContext&)` pure on `QCPLayerable`.
8. **`QCPGraph` and `QCPGraph2` at the end of M4** are untouched and deprecated (option b): they are
   deleted in milestone 5 with the old API, and the examples, docs and tests move then (milestone 5 is
   the docs-and-examples milestone). The M4 goal "every plottable draws through the drawing API"
   reads "every plottable that 3.0 keeps". `QCPGraph2`'s L1/L2 pipeline is not ported: the multigraph
   pipeline already covers it.
9. **Signals without a plot or from another thread stay direct**, listed as exceptions in the code:
   `QCPTheme::changed()` (a theme can serve several plots or none; its slots are NeoQCP's own) and
   the async pipeline's cross-thread signals (already queued by Qt to the GUI thread). Every other
   direct emit goes through `deliver`.
10. **Polar** is put on the object model, as answer 7 says (polar graph a plottable, polar axes on
    the axis and transform model, selection unified with the rest in M4a). It is too big for M4e, so
    it becomes the last sub-milestone, **M4f**.
11. **Alignment.** `multi-axisrect-perf.md` already shows that per-layer repaint does not make a
    stacked plot cheap for a one-row change (it needs per-rect buffers and partial render-target
    updates). By answer 5's rule decision 13 therefore stands: M4d builds alignment groups. Call 5's
    re-measurement is dropped; this container and CI are llvmpipe only.
12. **R3** binds every recorder of many rects or ellipses once adopted (bars, intervals, histogram
    columns, error-bar caps, markers): one format, no per-class variants. It is measured on the bars
    and marker rows (`QCPGpu_Bars10k`, `QCPDrawn_Zoom10k`, `QCPItems_Zoom10k`), not on line rows
    (call 4 corrected): adopted if those rows are no slower than 5 % and the recording is smaller.
13. **Colour scale.** Each axis rect owns one scale, created on first use and placed by the rect.
    The plottables on one rect that use it share its gradient and range. A plottable may still be
    given another, user-made scale (`setColorScale`); the rect places only its own. The graph's colour
    setters move onto the relation; with a scale attached, the scale's range and gradient win, as for
    a colour map. `rescaleDataRange` unions every user of the scale.
14. **Bench gate.** Rows and method as `baseline.md` (llvmpipe, `-median 5`, three runs, same
    machine). The baseline is the end of milestone 3 for all of M4, not each sub-milestone's start,
    so regressions cannot add up. Drawing rows are `QCPGpu_*`, `QCPDrawn_*`, `QCPItems_*`, `QCPBars_*`
    and the ported class's own rows: 5 %; the rest 15 %. A row over the gate is re-run six times;
    the median decides.
15. **Linked axes** are in the default drag and zoom sets; dragging one writes back to its source
    through the inverse function, which is part of the linked-axis API (a pair: forward and inverse).
    An explicit `setRangeDragAxes` / `setRangeZoomAxes` still overrides the default.
16. **Selectability.** `selectable()` returns `QCP::SelectionType` on every object (an item is
    `stNone` or `stWhole`; an axis or a legend is `stWhole` when any part is selectable, with
    `selectableParts()` for the detail), and `selectableChanged()` loses its argument too. The 2.x
    signals with arguments are emitted alongside, through the same delivery, until milestone 5.

## Corrections made while planning M4a (2026-11-06)

- Calls 2 and 16: the 2.x selection signals with arguments are removed in M4a, not emitted
  alongside: a `selectionChanged()` next to `selectionChanged(bool)` makes every
  `&Class::selectionChanged` ambiguous, which breaks 2.x code anyway and breaks decision 5. The getter
  keeps its 2.x name `selected()`; no `isSelected()` is added.
- `rangeChanged` keeps only `(newRange, oldRange)`: a 2.x slot taking one range still connects.

## Left to the builder

Class and method names beyond the ones above, the base-class split, how the resampler outputs become
commands, the order inside each sub-milestone, and the tests.
