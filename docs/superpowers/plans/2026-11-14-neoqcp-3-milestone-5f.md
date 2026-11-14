# NeoQCP 3.0 milestone 5f — triage Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** every backlog item fixed, scheduled to project 3/4, or written down as a 3.0 limit; the guide pictures
regenerated and looked at; the SciQLopPlots migration file complete. No release (maintainer, 2026-11-14: "there won't
be any release for now"): no version, tag or release notes.

**Architecture:** the triage is done (`.superpowers/m5f/triage-a.md`, 88 backlog items; `triage-b.md`, 93 items of
the milestone 1-4a deferred lists). It found 14 bugs; the picture review found two more (F-1, F-2). Fix them, write
the limits on one page, replace the backlog by the disposition table.

**Tech Stack:** C++20, Qt 6.12, meson + `cmake/sources.cmake`, QtTest.

**Spec:** `docs/specs/2026-11-11-neoqcp-3-milestone-5-design.md`, 5f and A13 (one disposition line per item: fixed
(commit), project 3 or 4 (named), or 3.0 limit (doc page)).

## Calls

- **F-0** Triage rows are the source of truth for each item's evidence and test idea; the task reads its rows.
- **F-1 (pictures)** `QCPHistogram::sStep` draws one box per bin instead of a stepped outline
  (`docs/images/plottables-histogram.png` before/after). A regression; find when with the picture's history.
- **F-2 (pictures)** `colormaps_gradients` gives 19 maps of one rect 19 gradients; since 4c they share the rect's
  scale, so all show the last one. The example opts each stripe out (`setColorScale(nullptr)` first). The guide's
  "Colour scales" section never says a rect shares one scale by default, nor how to opt out: add it.
- **F-3 (triage a #83)** overrules 5c R-2: `setBrush` keeps a fill's target (constant, column) and changes its brush.
- **F-4 (triage a #34)** `draw(QCPDrawContext&)` becomes pure on `QCPAbstractPlottable`; test stubs that relied on
  the empty default record nothing explicitly.
- **F-5** Limits go on one new page, `docs/limits.md` ("Known limits of 3.0"), grouped by guide page, in the toctree.
  Rows marked "none (dev note)" stay in the rulings files only.
- **F-6** `backlog.md` becomes the disposition table (one line per item, both triage tables): fixed (commit),
  project 3/4, limit (`limits.md` anchor), dev note, stale. The "From earlier milestones" pointers go.
- **F-7** Builds and suites on the Coder workspace `neoqcp`; pictures locally (fonts); one build per tree at a time.

## Global Constraints

- Failing test first for every fix, watched failing. Never bend behaviour to please a test.
- Commit with explicit pathspecs; never `subprojects/.wraplock`, `subprojects/jkqtmathtext`, `qt.ini`, `.superpowers`.
- Push only when asked.

## Review Focus

1. A fix that emits or deletes (legend parent, stretch burst, scale follow) keeps the 5e delete-in-slot safety.
2. Pure `draw(QCPDrawContext&)` breaks no in-tree plottable, example or snippet.
3. Regenerated pictures: each changed picture looked at, not just diffed.

---

### Task 1: Drawing bugs (F-1, triage a #7 #21 #32 #61)

- [ ] Failing tests: step histogram outline (reference picture or command check), bracket on a user system (strokes
  or one misuse report), error-bar whisker hit through the index, polar label hit at 0/180 degrees, NaN in a filled
  curve (if it fails; otherwise stale with the test kept).
- [ ] Fix each; commit per fix.

### Task 2: State and signal bugs (F-3, F-4, triage a #35 #46 #47 #67 #80, triage b #12)

- [ ] Failing test per row (test ideas in the triage rows), including a delete-in-slot row for #12 and #67.
- [ ] Fix each; commit per fix.

### Task 3: Alignment groups hear an ancestor's move (triage a #37)

- [ ] Failing test: two plots in two containers, aligned; move one container. Fix with an event filter on the
  ancestors (the `simplify:` note goes); bench the layout rows.

### Task 4: Pictures and docs (F-2, F-5, F-6, triage a #87)

- [ ] F-2 example and paragraph; `adaptive-sampling.md` and `gap-detection.md` rewritten for `linesInData()`.
- [ ] `docs/limits.md`; `backlog.md` as the table.
- [ ] Regenerate every guide picture locally (`doc-shots <dir>`), look at each changed one, commit them;
  `documentation/images/QCPAlignmentGroup.png` too.

### Task 5: Migration file complete

- [ ] Compile SciQLopPlots main against this branch (probe on the `sciqlopplots` workspace); every distinct break is
  in `sciqlopplots-migration.md` or is added.

### Task 6: Review and finish

- [ ] Final review by a fresh Opus reviewer; one fix batch.
- [ ] Offscreen, Xvfb, ASan+UBSan suites, clang, snippets, examples, `check-sources.py`.
- [ ] `milestone-5f-rulings.md`, changelog (no release header), memory.
