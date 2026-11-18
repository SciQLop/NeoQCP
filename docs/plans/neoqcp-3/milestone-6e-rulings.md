# NeoQCP 3.0 — milestone 6e rulings and deferred items

Milestone 6e: multi-panel plots. `setAxisRectGrid(rows, columns, { shareX, shareY, rowStretch, columnStretch })` makes
the plot a grid of axis rects in one call, `axisRect(row, column)` reads a cell, shared axes show one range and one
scale and hide their inner tick labels, rescale fits the union of a link, `QCPAxisRect::setTitle` titles one rect and
`QCPAxisRect::addInset` places a rect inside another, as matplotlib's `inset_axes`. The targets
`examples/user-code/{shared-x,symlog,spectrogram}` compile, are built with the examples and have reference pictures.
Branch `neoqcp-3.0`, `6764bd7..` the commit that adds this file (not pushed). Plan:
`docs/superpowers/plans/2026-11-17-neoqcp-3-milestone-6e.md` (rev. 2, pre-flight C1..C9, M1..M10, maintainer approved
P-1..P-9 on 2026-11-17). Implementers: opencode (DeepSeek V4.1 Flash, auto mode) for Tasks 1, the inset GPU rounds and
the pictures; the controller (Claude) for Tasks 2, 3, 4, 5 and 6 part 1 on a side branch merged twice; task,
re-review and final reviews by Claude subagents.

## Results

- `setAxisRectGrid`, `axisRect(row, column)`, `shareX` / `shareY`, the rescale union, `QCPAxisRect::setTitle` and
  `QCPAxisRect::addInset` are in, with docs (`docs/layout.md` "A grid of axis rects", `docs/axes.md`,
  `docs/migrating.md`, `docs/performance.md`, the changelog).
- The targets `shared-x`, `symlog` and `spectrogram` compile, run offscreen with the examples, and have reference pictures
  (`shared-x-target`, `symlog-target`, `spectrogram-target` in `TestQCPAxisRect`); the spectrogram one asserts the
  shared column's data edges are equal and the colour bar sits beside the top rect only (Review Focus 2).
- Final whole-branch review: 0 Critical, 1 Important (I1), 8 Minor; all nine fixed in one pass, each with a test seen
  failing first where it was a behaviour (I1, M1, M2, M6) or a coverage gap filled (M4, M5).
- Verification at the commit that adds this file: local Xvfb 76 classes 4546 passed / 0 failed; Coder offscreen 4381 passed / 0 failed / 165 skipped (GPU), doc snippets green; ASan + UBSan + float-cast-overflow 4381 / 0 / 165, 0 reports; clang++ -fsyntax-only clean on the changed files.

## Rulings

- R-1 (P-1): a grid's margins always align (each column left/right, each row top/bottom), shared or not; a rect's
  colour scale counts in its margin.
- R-2 (P-2): `shareX` links each column's bottom axes to the bottom rect's (which keeps the column's tick labels);
  `shareY` each row's left axes to the leftmost's. Only tick labels are hidden, not axis labels.
- R-3 (P-3, reverses the 6f deferral): a link made without functions shares the scale: the follower takes the
  source's at `linkTo` (only when different), the source hands each change to its identity followers, and
  `setScale` on an identity follower goes to the source. A link through functions keeps its own scale.
- R-4 (C4): identity links stay a star: `linkTo` to an identity follower links to its source, and an axis that drives
  identity followers hands them to its new source. Links through functions still refuse chains.
- R-5 (P-4, C2, C5, decision 13): `rescale()` is one non-virtual function over the virtual per-axis `ownDataRange()`;
  it fits the union of the identity source and its identity followers, set once on the source. `rescaleAxes()` fits
  each link once (a follower whose source is outside the layout fits the link itself).
- R-6 (P-5, M2): a second `setAxisRectGrid` undoes the previous shares first: links removed, axes the share handed
  over return to the source they had, only the tick labels the grid hid show again. Original sources are snapshotted
  before any link moves.
- R-7 (P-6, M8): the main layout must hold axis rects only (a title row, a legend, a nested layout or a polar area is
  refused). A share that would replace a link through functions, or reverse a user link, is refused whole before
  anything changes. Bad counts and stretch factors (<= 0, NaN) are misuse.
- R-8 (C1): a rect the grid drops takes its plottables, items, colour scale and insets with it; the legend, if it
  lived in a dropped rect's inset, goes back to (0, 0)'s inset; a legend the user took out stays out.
- R-9 (P-7, M1): `axisRects()` lists the grid's cells row after row, then nested rects breadth-first.
- R-10 (P-8, C6): the title is a text element the rect owns outside the layout tree, on the "axes" layer, its height
  in the top margin, font = the top axis' label font x1.2 in its label colour. A hidden rect hides its title and its
  own colour scale (both now child layerables of the rect).
- R-11 (P-9, C7, C8): an inset's DATA rect sits at the ratio of the parent's data rect (`ipInnerRect`). It has its own
  three layers (back and data buffered, axes logical) above every earlier inset, nested ones inside their container's
  stack, its grid drawn by the painter on its back layer; plottables and items added to or moved onto it go to its
  data layer unless they sit on a layer the user chose. Its background follows the plot's until the user sets one.
- R-12 (pre-existing bugs found and fixed): a plot being destroyed delivers no `qcpEmit` signal (reverses 5e R-11 and
  milestone-1 M5, matches the object-model spec); deleting the plot from a plottable's or item's `destroyed()` no longer
  double-frees; a centred text element is hit where it is drawn.
- R-13: `setAxisRectGrid` opens one emission scope, survives a slot deleting dropped content or the plot, and refuses
  re-entry; its running flag is guarded by a `QPointer`.
- R-14: `linkTo` opens its emission scope unconditionally before any change (it now emits `scaleChanged`).
- R-15 (final review M1): undoing the shares leaves an axis the user relinked since the share: it is the user's now.
- R-16 (final review M2): `QCPAlignmentGroup::membersChanged` goes through the plot's delivery like every other signal,
  so a grid call (or any emission scope) holds it until done.
- R-17 (final review M6): an inset removed through its handle hands the plottables and items on its data layer back
  to "main" / "annotations"; what sits on a layer the user chose stays there.

## Deferred

- Cost of `followAreaLayer` on every plottable `setSystem` and item rebind: it walks the layout (`axisRectsRaw()`) to
  recognise an inset data layer. Not measured; measure with the add benchmark before optimising (a set of inset data
  layers kept by the plot).
- A title in a row whose alignment group gives a rect a larger top margin than it needs sits at the top of that margin,
  away from the rect's top axis. Cosmetic.
- An inset of a cell goes above that cell's earlier insets only, not above a newer inset of another cell: it matters
  only for an inset that overflows its parent (ratio > 1).
- Whether a `scaleWillChange` refusal on an identity follower can leave follower and source on different scales: no
  axis kind that refuses was found.
- The other targets (`fill-between`, `inset-zoom`, `annotations`, `peak-fit`) need 6g / 6h / 6d.
