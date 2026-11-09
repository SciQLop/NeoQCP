# M4f inventory brief (polar on the object model) — for the implementer agent

Worktree /home/jeandet/Documents/prog/NeoQCP-3.0, branch neoqcp-3.0, HEAD c0c5b59. READ-ONLY task: do not edit source, do not build, do not run tests (unless a fact can only be settled by reading a build artefact — then say so and stop). No subagents. Use `command cat`, not `cat`.

Context: NeoQCP 3.0 milestone 4 moved every plottable and item onto the drawing API (a recording of commands in data / pixel / axis-rect-ratio positions, replayed through QCPTransform by a painter or the GPU; see docs/specs/2026-11-04-neoqcp-3-drawing-api-design.md and src/drawable.h, src/painting/draw-context.h, src/painting/draw-commands.h, src/coords.h). The polar classes (src/polar/*) are the last ones outside it. Spec lines: docs/specs/2026-11-02-neoqcp-3-object-model-design.md answer 7 ("Polar plots are kept and ported to the new base classes, polar coordinates and transforms included"), docs/specs/2026-11-06-neoqcp-3-milestone-4-design.md answer 10 (polar graph a plottable, polar axes on the axis and transform model, selection unified as in M4a). Earlier inventories to imitate in shape: docs/plans/neoqcp-3/m4e-inventory.md.

Write docs/plans/neoqcp-3/m4f-inventory.md with file:line for every fact:

1. Classes: each polar class, its base class, size, what it owns, its public API (setters, signals), and how it is created (plot.add<...>? constructors? layout insertion).
2. Drawing: how each class draws today (draw(QCPPainter*), helpers, GPU paths if any, export path), what it reads while drawing (axes, ranges, theme, selection, data), and its legend icon.
3. Coordinates: how angle/radius map to pixels (the angular axis, the radial axis, their ranges, scale types, reversal, the centre/radius in pixels); everything that converts coordinates (coordToPixel-like functions, pixelToCoord), and who calls them. Compare with QCPTransform / QCPRatioTransform (src/coords.h): what a polar transform would need.
4. Axes model: what the polar axes share with QCPAxis and what they duplicate (range, ticker, labels, grid, drag/zoom, rangeChanged signals); how drag and zoom work in polar; any link to QCPAxisRect.
5. Selection and signals: the polar parts' selection API after M4a (selectable(), selectionChanged()), hit testing (selectTest), and every emit (direct or through qcpEmit/deliver).
6. Data: the polar graph's data container (QCPGraphData? its own?), setData overloads, dependency on QCPGraph code.
7. Users: tests (tests/auto — any polar test?), examples, docs/snippets, docs/*.md, benchmark rows, the WASM gallery/JS bindings if any, SciQLopPlots? (do not read SciQLopPlots; just say whether NeoQCP docs mention it).
8. Known bugs, TODOs, simplify notes, and anything in the polar code that the milestone-1..4 rules (ownership, misuse via QCP_CHECK_OR_RETURN, qcpEmit delivery, contentChanged on every setter, no exceptions) do not yet follow.
9. Your assessment: the smallest set of steps to put polar on the object model, the open design questions (e.g. polar commands in data space with a polar transform vs pixel recording; one polar axis-rect-like layout element vs today's shape), and what a reference-image and bench baseline on the old code should cover.

Keep the file factual and dense; aim for 300-500 lines. Then reply only: DONE and the file's line count.
