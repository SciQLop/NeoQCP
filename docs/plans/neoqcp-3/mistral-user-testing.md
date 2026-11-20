# NeoQCP 3.0 — user testing by an outside model (Mistral Large)

Proposed by the maintainer on 2026-11-19. To run after milestone 6h and after the examples migration (`examples-migration.md`), before the Python bindings fix the API.

## Why

The milestone-6 targets were written by people who knew the API. A model that starts from the docs alone, like a
new user, finds two kinds of problem that reviews and fuzzing miss:

- **Difficulties:** a name it guesses wrong, a program that does not compile on the first try, a doc page it cannot
  find, an option it does not know exists. Each one is an API or docs failure, not a model failure.
- **Bugs:** crashes, wrong pictures, hangs, leaks, from programs no test would think to write ("monkey testing").

The SciQLopPlots API fuzzing campaign (11-01/02) found real bugs from the outside; this adds the "is it easy" side.

## Setup

1. **A separate copy with only what a user has:** `docs/` (guides, snippets, images), the public headers as
   installed, `examples/`, and a ready build of the library to link against. No `src/*.cpp`, no `docs/plans/`, no
   `docs/superpowers/`, no tests, no git history. Otherwise the model reads the implementation and stops being a user.
2. **Builds and runs on Coder** (workspace `neoqcp`), against the `build-asan` library, so a crash comes back with
   a stack trace. Pictures saved to files for us to look at.
3. **The model:** Mistral Large (the maintainer's 3-month free plan), driven as an agent that can write files, run
   the compiler and run its programs in that copy only.

## Tasks to give it (real jobs, not "find bugs")

- Recreate a list of matplotlib gallery plots (the ~20 of `m6-mpl-survey.md` plus others it picks), one program each.
- Build small interactive tools: a live-updating dashboard, a draggable cursor that reads values, a peak picker, a
  zoomable spectrogram with a time series below.
- Then free monkey testing: misuse the API on purpose (empty data, NaN, huge ranges, deleting things from slots,
  resizing, many plots), and report what happens.

For each program it writes down: what it tried first, every compile error, every doc page it looked for and did not
find, and what the picture or the run did against what it expected.

## Triage

Every report is a lead, not a bug. Each one gets verified against the code (reproduced, or shown to be the model's
mistake) before any fix, and each real bug gets a failing test first. Expect a good share of false positives.
Difficulties that several programs hit become API or docs changes; single ones go to the backlog.

## Output

A findings file in `docs/plans/neoqcp-3/` with each lead, its verdict (bug / difficulty / model mistake) and the
commit that fixed it, as the earlier milestones' rulings files do.
