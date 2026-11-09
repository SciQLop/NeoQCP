# Coordinate-system inventory brief — for the implementer agent

Worktree /home/jeandet/Documents/prog/NeoQCP-3.0, branch neoqcp-3.0, HEAD = current. READ-ONLY: no edits (except the output file), no builds, no tests, no subagents. Use `command cat`, not `cat`.

## Context

Maintainer decision 2026-11-09: NeoQCP 3.0 introduces a PUBLIC "coordinate system" abstraction. A plottable holds a coordinate system that provides its drawing frame (data -> pixels and back), its clip, its hit area and its axes, instead of holding two QCPAxis. The cartesian coordinate system wraps today's axis rect + key/value QCPAxis pair; a polar one wraps the angular + radial axes (see docs/plans/neoqcp-3/m4f-inventory.md for polar). Users must be able to write their own (e.g. ternary, Smith chart) without changing NeoQCP. The cartesian path must stay as fast as today.

This inventory feeds the design addendum. It must find EVERY place the abstraction would touch.

## Write docs/plans/neoqcp-3/coordsys-inventory.md with file:line for every fact

1. QCPAbstractPlottable and subclasses: every read of mKeyAxis / mValueAxis / keyAxis() / valueAxis() / axisRect() / coordsToPixels / pixelsToCoords / clipRect / setKeyAxis / setValueAxis, grouped by purpose (drawing frame, clipping, hit test, range queries getKeyRange/getValueRange/rescale, selection rect, legend, data -> pixel helpers, orientation handling: keyAxis()->orientation(), vertical key axis cases). Per plottable class (graph, graph2, multigraph, curve, bars, errorbars, financial, statbox, colormap, colormap2, histogram, histogram2d, intervals, any other), note what is genuinely cartesian-only (e.g. needs rect cells, axis-aligned bars) versus what only needs "map a point".
2. QCPDrawable / QCPDrawnCore / QCPDrawnView / QCPDrawFrame / QCPTransform / QCPRatioTransform / qcpMakeFrame / qcpDrawnViewOf / resolve cache / isTranslationOf / stallOffset / key window (QCPKeyWindow, qcpKeyPixelsPerUnit) / GPU replay (command-replay-rhi.cpp) / extents: where they assume two QCPAxis or an axis rect.
3. Items: QCPItemPosition axes (setAxes, axisRect, position types ptPlotCoords / ptAxisRectRatio ...), asPosition, the item spatial index (collectItemAxes, data extents), and what a polar item position would need.
4. Plot-level code walking plottables by axis: QCustomPlot (rescaleAxes, plottables lists, take/adopt/add<T>, removeObject, detach), QCPAxis (plottables(), graphs(), items(), rescale), QCPAxisRect (plottables(), range drag/zoom, markAffectedLayersDirty, colour scale relation), legend, selection rect, QCPPlottableGroup, construct.h (add<T> with axes), colour scale (QCPColorScale and the rect's shared scale from M4c), alignment groups, linked axes (M4d) — what each assumes.
5. Public API surface that names the axes: constructors taking (QCPAxis* key, QCPAxis* value), setKeyAxis/setValueAxis, handles/ops (QCPPlottableOps), signals, docs (docs/*.md, docs/snippets), examples, tests count per pattern.
6. Hot paths: where the cartesian mapping is on a per-point hot loop (batch map, extruders, resamplers, GPU uploads, QCPTransform::map spans) and would suffer from a virtual call per point; what is already batched.
7. Polar today: how the polar graph/axes would map onto this (from m4f-inventory.md, briefly, with what is missing).
8. Your assessment: a candidate minimal public interface (methods + what each caller needs it for), what must stay cartesian-only and how a plottable declares that, how 2.x code (QCPGraph(keyAxis, valueAxis), keyAxis()) keeps working, the risks, and an ordering of steps. Mark guesses as guesses.

Dense and factual, 400-700 lines. Reply only: DONE and the line count.
