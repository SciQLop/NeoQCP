# NeoQCP 3.0 — milestone 5d rulings and deferred items

Milestone 5d: the rest of the old API. Every plottable records its drawing and its legend icon; the plottable painter
entries are final; alignment groups take one group per side and replace `QCPMarginGroup`, which is deleted; the 2.x
axis members of plottables and item positions are private; the span items lose their never-drawn pen. Branch
`neoqcp-3.0`, `872f543..bb870bf` (pushed). Plan: `docs/superpowers/plans/2026-11-13-neoqcp-3-milestone-5d.md`
(calls D-1..D-9), pre-flight review `m5d-plan-review.md`. Design: `docs/specs/2026-11-11-neoqcp-3-milestone-5-design.md`.

Every task was implemented by a Claude subagent and reviewed by a fresh one; all builds, suites, the ASan+UBSan run
and the bench ran on the Coder workspace `neoqcp` (maintainer, 2026-11-13: builds on Coder by default). In parallel,
CI was made green on Linux (it had been red since 4i); those fixes are in the same range.

## Results

- Offscreen 2549 passed, 0 failed, 133 skipped; Xvfb 2682/0/0; whole ASan + UBSan suite 2549/0/134 with no sanitizer
  report; clang `-fsyntax-only` 387 files clean; guide snippets green; every example builds (text-document-integration
  repaired). All on the Coder workspace at bb870bf.
- CI: Linux (x86-64 and arm64) and the CMake job green; macOS and Windows still have platform-only failures (text
  rendering, one Metal grid test), being fixed after the milestone.
- Bench: per task A/B within the gates; whole-table end-of-milestone A/B in `baseline.md`, "End of milestone 5d".

## Rulings (what, why, cost if wrong)

- R-1 (maintainer, B2): alignment membership is per side; `create(members, sides)` unchanged; `alignmentGroup(side)`.
  It is the only form that ports every 2.x margin-group layout.
- R-2 (B1): the 2.x axis members stay as a private cached pair, not computed from the system: a one-axis item position
  (4i R-2) and a half-set plottable pair hold axes the system does not. Cost: one pointer pair per object.
- R-3 (I3): the financial and error-bar legend icons record a bounding rect plus a painter callback (clip regions,
  per-icon antialiasing and fixed pixel offsets have no recording command); statistical box and intervals record native
  rects. Pixel-identical. Cost: a later native port.
- R-4 (I1/I2): `draw(QCPPainter*)` is `final`, `drawLegendIcon(QCPPainter*, QRectF)` is `virtual … final`, so a 2.x
  override fails to compile. No compile-failure test (C++ cannot express one).
- R-5 (2a): stacked rows whose margin group equalised their top/bottom margins have no alignment form (those edges are
  a row apart); migration row. Cost: such a 2.x layout is laid out by hand.
- R-6 (2a): a plot reruns its margin/layout passes (at most 3) when an aligned edge moved, so exports and pictures of
  never-shown plots are aligned after one render; a layout that never settles stops after one more replot and reports
  misuse (fatal in debug, like every misuse report). Cost: a custom element whose size follows its alignment aborts a
  debug build.
- R-7 (3a/3b): "invalid axis" `qDebug` lines on drawing, range and hit paths were states (a plottable without a system
  is inert on purpose) and are deleted; no line became a misuse check.
- R-8 (3b): inside `QCPItemPosition` the class reads its rect member directly: the out-of-line raw accessor is a real
  call in the PIC build (+5-7 % on item creation). Addendum to the 5b hot-path rule: an out-of-line accessor on a hot
  path costs a call.
- R-9 (CI): `-ffp-contract=off` for NeoQCP's own targets (not users): FMA contraction changed truncated gradient
  colours on Ubuntu's GCC and arm64. Cost: no FMA in NeoQCP's own code.
- R-10 (maintainer, HiDPI): thin painter lines are one logical pixel on screens with device ratio > 1, like GPU lines
  and exports. Plottables, grids (linked-axis and colour-scale grids included) and plottable painter callbacks keep the
  cosmetic pen in the painter fallback: one logical pixel there made dense polylines 5x slower, and on a real screen
  the GPU draws them. Cost: mixed widths in no-GPU paths on Retina. Ratio 1 unchanged.
- R-11 (CI): reference pictures draw text with a bundled DejaVu Sans; macOS tests run cocoa with the FreeType engine;
  on macOS under the offscreen platform (macos-15-intel) only the text-picture comparisons are skipped, since that
  platform renders text with CoreText. Cost: Intel-mac text regressions go unseen.
- R-12 (process): the small fix rounds of 2a and the mechanical deletion of 3c had no separate re-review; their tests
  and the final review covered them.

## Found and fixed

- Pre-flight review (fresh Opus reviewer): computed projections would lose one-axis positions and half-set pairs;
  alignment groups could not express four mixed-side layouts; two icons could not be recorded natively; wrong test
  lists; incomplete user lists. Folded into the plan before any code.
- During the tasks, each with a failing test first: legend icons ignored the plottable's antialiasing; a hidden plot's
  aligned member repainted every visible peer; colour-scale ticks were not drawn on screen (the GPU grid layer only knew
  the plot's own axis rects); thin painter lines were half width on HiDPI; item creation +5-7 % from an out-of-line
  accessor.
- Final review (fresh Opus reviewer, one fix batch): a side switched to a manual margin kept its group unsettled and
  replotted forever; an oscillating layout replotted forever; a zero-length flat-cap line drew a square in the new fast
  path; deleting an alignment group left its members on the shared edge; a stray `qDebug` printed on every bracket draw;
  clang-cl got a GCC flag; the grid check walked the layout per axis. 
- CI since 4i: CMake source lists, a missing `<QTest>` include and Meson's missing Qt module macros on macOS, a GCC 13
  nested-struct rejection, an MSVC handle comparison, a libc++ command-size overflow, a truncated macOS log, fonts,
  FMA, tests reading pixels on dotted grid lines, a test comparing screen and export at different ratios.

## Deferred (also in backlog.md)

- Financial and error-bar icons assume a plain fit transform (final review M7).
- 19 tests assume device ratio 1 (fail under `QT_SCALE_FACTOR=2`): being fixed with the macOS CI round.
- macOS / Windows CI: remaining text and Metal-grid failures (CI round 4).
- `documentation/images/QCPAlignmentGroup.png` was rendered with the workspace's fonts: regenerate with the guide
  pictures at release triage.
