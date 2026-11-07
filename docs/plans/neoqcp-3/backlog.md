# NeoQCP 3.0 — backlog

Work that was deferred during the 3.0 milestones, to triage before 3.0 ships. Each item says what a
user would see and where the code is. Fixed items are deleted, not ticked.

## From milestone 4b (the line family)

- **No test checks that marker colours reach the GPU.** Coloured markers go through the markers
  command into the scatter layer, and only the CPU replay is tested for colours.
  `src/painting/command-replay-rhi.cpp` (the markers branch).
- **Error-bar caps are slightly lopsided on a log key axis.** The cap half-width is converted from
  pixels to a symmetric width in data, so the left and right halves differ on screen.
  `QCPMultiGraph::recordErrorBars`.
- **A gap inside a filled curve goes into the fill polygon as is.** The polygon then has a vertex at
  NaN; 2.x did the same. `QCPCurve::draw`.
- **Clicking near a curve converts every point to pixels on each mouse move.** 2.x did the same.
  A cache keyed on the transform would make hover cheap on big curves. `QCPCurve::pointDistance`.
- **Zooming a 1M-point multigraph costs 0.69 ms against a 0.61 limit** (ruling R-8). The data-space
  envelope keeps about 2 150 points per column against 1 907 before; the extruder pays for them.
- **A big graph shows blank edges during a pan until the 150 ms settle** (ruling R-16). The reduced
  data covers the view while the recording claims three views. Reducing the whole window on the
  settle would fix it for three times the GUI-thread cost.
- **The data sources still carry the old pixel line API** (ruling R-17): `getLinesAll`,
  `getOptimizedLineDataAll` and the indexed forms are only used by tests. Remove them with the
  data-source work that retires QCPGraph2.

## From earlier milestones

Each rulings file keeps its own list; they are not copied here so they cannot drift.

- Milestone 1: `milestone-1-rulings.md`, "Deferred minors".
- Milestone 2: `milestone-2-rulings.md`, "Deferred minors (for the final triage)".
- Item scaling and item index: `item-scaling-rulings.md`, "Deferred minors".
- Milestone 3: `milestone-3-rulings.md`, "Deferred (for milestone 4 and the final triage)".
- Milestone 4a: `milestone-4a-rulings.md`, "Deferred minors".
