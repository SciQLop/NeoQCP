# NeoQCP 3.0 — milestone 6h rulings and deferred items

Milestone 6h: annotations. `QCPConnector` (five shapes: `arc`, `curve`, `elbow`, `arms`, `bracket` = matplotlib's
`arc3`, `angle3`, `angle`, `arc`, `bar`), `qcp::trimConnector` (cut at the text box's first exit, shrink at both
ends), `QCPItemAnnotation` made by `QCPAxisRect::annotation(target, at, text, QCPAnnotationOptions)`, a unary minus on
`QCP::PixelLength` (`-30_px`), and one glyph cache (`qcp::TextGlyphCache`) shared by `QCPItemText` and the
annotation. The `user-code-annotations` target builds. Branch `neoqcp-3.0`, `7e701845..` the commit that adds this
file (not pushed). Plan: `docs/superpowers/plans/2026-11-20-neoqcp-3-milestone-6h.md` (P-1..P-7 approved by the
maintainer). Spec: `docs/specs/2026-11-14-neoqcp-3-milestone-6-design.md` (annotation), `m6-gaps.md` order item 8 and
decision 15.
Implementer: opencode (DeepSeek V4.1 Flash, auto mode); task, re-review and final reviews by Claude subagents (opus).

## Results

- Coder (GCC): `build` (with examples), `build-asan` (ASan + UBSan, leak checks off under ptrace) and `build-fco`
  each 4577 passed, 0 failed, 164 skipped (the skips need a QRhi), no sanitizer report.
- Local Qt 6.12 under Xvfb: whole `auto-tests` 4741/0/0. Reference picture added: `annotations-target` (matplotlib's
  connectorstyle demo, panel for panel, plus a text-and-arrow cell); doc-shot `items-annotation`. Both looked at.
- Every connector shape checked against matplotlib 3.11 vertex by vertex (start (200,100), end (50,300)).
- Reviews: Task 1 one fix round (180-degree parallel lines, negative arms, docs); Task 2 two fix rounds (label colour,
  clipping, rotated text, rect edge, shrink direction). Final review: 2 Important + 6 Minor, four Minors regraded
  Important; one fix pass, then a scoped re-review whose residuals (CMake source list, a redundant flag, docs) the
  controller fixed.

## Rulings

- R-1 (Task 1): P-2's wording "a positive arc bend bows to the left" was wrong; matplotlib's numbers stand (a
  positive bend bows to the right of start→end). Cost if wrong: one doc line.
- R-2 (Task 1): a non-finite setting or position gives an empty connector (one guard on the built path), not a
  misuse report: the factories make value types with no plot to report to. Cost if wrong: a silent empty connector.
- R-3 (Task 1→2): `QCP::PixelLength` gets a unary minus so `-30_px` compiles (matplotlib takes negative arms and
  radii). Cost if wrong: one operator to remove.
- R-4 (Task 2): annotations follow matplotlib's clip rule: never clipped to their axis rect; hidden entirely when the
  target is in data on both axes and outside the rect grown by 1 px (matplotlib's `annotation_clip` and
  `contains_point(radius=1)`). The default text alignment (top-left) is unchanged. Cost if wrong: a label may hang
  over a neighbouring cell and be clicked from there.
- R-5 (Task 2): the target shrink cuts at the first approach, walking forward, as matplotlib's `split_path_inout`
  (a fix round had made it search backwards on a wrong reading of matplotlib; reverted).
- R-6 (final): four Minors that broke standing rules or hid bugs were regraded Important and fixed: one glyph cache
  for both text items (both copies missed `textFlags`), `draw()` split into small functions, the doc snippet in the
  3.0 API, the connector's default pen themed like the label (a pen equal to `QPen()` takes the theme's foreground).
- R-7 (final): the annotation's label keeps its own colour when selected (P-6), unlike `QCPItemText`, which turns to
  the theme's selection colour.

## Deferred

- Python binding: `QCPConnector::bracket(double, std::optional<double>)` and the designated-initialiser options need
  typesystem work (a `std::optional<double>` conversion, explicit defaults) in the bindings milestone.
- Two colour rules for text: `QCPItemText` has its colour pushed once by `applyThemeColors`; the annotation reads the
  theme at draw time, so after `setTheme` an annotation recolours and a text item does not. One rule should win (the
  draw-time read looks better).
- Text placed within `shrink` of its target draws no connector (P-3); matplotlib keeps a short stub.
- A target inside the text box draws no connector (P-1); matplotlib draws the uncut path.
- An annotation whose position is far off screen while its target is inside records its text there; the replay's
  `toRect()` has the same int-overflow exposure as `QCPItemText` (SciQLop#151 class).
