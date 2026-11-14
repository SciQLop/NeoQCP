# NeoQCP 3.0 milestone 6a — renames Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** the names the eleven milestone-6 targets use for the two base families and for "draw me again" exist in the
library, and the old names are gone.

**Architecture:** mechanical renames across `src/`, `tests/`, `examples/`, `docs/`, `documentation/`, with no
forwarders. One task per rename family, each building and passing the whole suite on its own.

**Tech Stack:** C++20, Qt 6.12, meson + `cmake/sources.cmake`, QtTest.

**Spec:** `docs/specs/2026-11-14-neoqcp-3-milestone-6-design.md`; the input is `docs/plans/neoqcp-3/m6-gaps.md`
(theme 3, rows "Drop Abstract" and "update()"; decision 9) and `m6-vocabulary.md`.

## Calls

- **A-1 (maintainer rule, "new 3.0 base classes drop Abstract")** Every public base class named `QCPAbstract*` drops
  the word, not only the two families the targets use, unless the short name is already taken (then the task lists
  it and keeps the old name, with the reason in the migration notes). `QCPAbstractPaintBuffer` is internal and stays.
- **A-2 (decision 9)** `QCPLayerable::markDirty()` and its overrides become `update()`; `contentChanged()` goes
  (`update()` replaces it). `QCPLayoutElement::update(UpdatePhase)` and its overrides become
  `updateLayout(UpdatePhase)`. `QCPLayer::markDirty()` is a layer-level repaint request, not the user's "draw me
  again": it stays.
- **A-3** No forwarders, no `[[deprecated]]` aliases (standing rule: no interim designs).
- **A-4** Builds and suites on the Coder workspace `neoqcp`; reference pictures locally; one build per tree at a time.

## Global Constraints

- Each break goes into `docs/migrating.md`, `changelog.txt` (milestone 6 entry) and, one line each,
  `docs/plans/neoqcp-3/sciqlopplots-migration.md`.
- `cmake/sources.cmake` and `meson.build` file lists stay in sync (`check-sources.py`); file names stay unless a file is
  named after a renamed class and every sibling follows the class name (then rename the file too, both lists).
- Commit with explicit pathspecs; never `subprojects/.wraplock`, `subprojects/jkqtmathtext`, `qt.ini`, `.superpowers`.
  Push only when asked.

## Review Focus

1. A `QCPAbstract*` name left in a string a user or a test reads: `metaObject()->className()`, `qobject_cast` targets,
   moc-registered type names, doc text, Doxygen `\class` lines. The meta-surface test (`tests/auto/test-meta-surface`)
   must pass with the new class names.
2. A virtual `update()` added to `QCPLayerable` that a subclass already declares with another signature (hidden
   overload): grep every `update(` in `src/` before and after.
3. A layout element that overrode `update(UpdatePhase)` and now silently stops being called because the override
   kept the old name: the build must fail on it (`override` on every one).
4. The `examples/user-code/` targets must keep the new names they already use; nothing in them changes.

---

### Task 1: The bases drop "Abstract" (A-1)

**Files:** every file under `src/`, `tests/`, `examples/` (not `examples/user-code/`), `docs/`, `documentation/`
naming a renamed class; `cmake/sources.cmake`, `meson.build` if a file is renamed.

- [ ] **Step 1:** List every `QCPAbstract*` class in `src/` (expected: `Plottable`, `Plottable1D`, `Item`,
  `LegendItem`, `Line`, `ColorMapped`, `SpanItem`, `DataSource`, `MultiDataSource`, `PaintBuffer`, and any other).
  For each, check whether the short name exists (`grep -rw QCP<Short> src`). Write the table (old, new or "kept: why")
  into the report before renaming.
- [ ] **Step 2:** Rename with whole-word replacement across the trees above. Rename files named after a renamed class
  only if Global Constraints allow.
- [ ] **Step 3:** Build on Coder (`meson compile -C ~/NeoQCP-3.0/build -j 16`) and run the whole offscreen suite.
  Expected: the 4117 baseline passes, 0 failed. `grep -rn "QCPAbstract" src tests examples docs documentation` shows
  only `QCPAbstractPaintBuffer` and the kept rows.
- [ ] **Step 4:** Migration notes and changelog lines (one table: old → new).
- [ ] **Step 5: Commit** `refactor(api): base classes drop "Abstract"`.

### Task 2: `update()` and `updateLayout()` (A-2)

**Files:** `src/layer.{h,cpp}`, `src/layout.{h,cpp}` and every override and caller; tests, examples, docs.

- [ ] **Step 1:** List every `update(` declaration in `src/` (Review Focus 2) and every `markDirty` / `contentChanged`
  caller outside `QCPLayer`. Note any existing `update` overload on a layerable in the report.
- [ ] **Step 2:** Rename `QCPLayerable::markDirty` and overrides to `update()`, delete `contentChanged()` and move
  its callers to `update()`, rename `QCPLayoutElement::update(UpdatePhase)` and overrides to `updateLayout`.
- [ ] **Step 3:** Build and whole offscreen suite on Coder, then the local `meson test -C build-qt612` (reference
  pictures). Expected: all pass. `grep -rnw "contentChanged" src tests examples docs` is empty; `markDirty` appears
  only on `QCPLayer`.
- [ ] **Step 4:** Migration notes, changelog, the guide pages that name `markDirty` / `contentChanged`
  (`docs/writing-a-plot-type.md`, `docs/items.md`, ...).
- [ ] **Step 5: Commit** `refactor(api): update() redraws, updateLayout() lays out`.

### Task 3: Review and finish

- [ ] Final review by a fresh Opus reviewer over the milestone range; one fix batch.
- [ ] Whole offscreen suite, whole ASan+UBSan suite (Coder), local `meson test`, clang `-fsyntax-only` on changed
  `.cpp` files, examples build, `check-sources.py`, guide snippets.
- [ ] `docs/plans/neoqcp-3/milestone-6a-rulings.md`; memory.
