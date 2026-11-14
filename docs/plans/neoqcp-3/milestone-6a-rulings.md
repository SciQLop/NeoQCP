# NeoQCP 3.0 — milestone 6a rulings and deferred items

Milestone 6a: the names the milestone-6 targets use. Every public `QCPAbstract*` base drops "Abstract", and "draw me
again" is `update()` while the layout pass is `updateLayout()`. Branch `neoqcp-3.0`, `cb73de6..` the commit that adds
this file (not pushed). Plan: `docs/superpowers/plans/2026-11-14-neoqcp-3-milestone-6a.md` (calls A-1..A-4). Input:
`m6-gaps.md` (theme 3, decision 9).

## Results

- Offscreen 4117 passed, 0 failed, 135 skipped (Coder, at c3c74ce). Whole ASan + UBSan suite 4117/0/135, no sanitizer
  report (leak checks off: LeakSanitizer does not run under the workspace's ptrace). Local Xvfb 4252/0.
- clang `-fsyntax-only` clean on the 97 changed `.cpp` files (examples/interactions/mainwindow.cpp is not in the
  local compile database).

## Rulings

- R-1 (A-1): ten classes renamed (`QCPPlottable`, `QCPPlottable1D`, `QCPItem`, `QCPLegendItem`, `QCPLine`,
  `QCPColorMapped`, `QCPSpanItem`, `QCPDataSource`, `QCPDataSource2D`, `QCPMultiDataSource`). No short name was taken.
  `QCPAbstractPaintBuffer` is internal and keeps its name. File names (e.g. `abstract-datasource.h`) stay.
- R-2: historical plans and specs keep the old names; they record what was true then.
- R-3: the protected `QCPLayout::updateLayout()` hook became `layoutChildren()`, since the renamed phase pass would
  have hidden it.
- R-4: `QCPDrawnCore::contentChanged()` became `newGeneration()`. Not forced by hiding, but it leaves one meaning per
  name.
- R-5 (process): the docs-only fix round of Task 2 was checked by the controller with grep, not by a re-review seat.

## Found and fixed

- Final review: the whole-word rename had also rewritten the released 1.x/2.x changelog notes (restored from cb73de6),
  and the 6a changelog entry sat after the 1.x releases (moved before 5f); layout-pass comments still said `update`;
  a stale `using QCPAxisRect::update;`; a doc block attached to the wrong function.

## Deferred

- `docs/writing-a-plot-type.md` still names the non-existent `QCPDrawn<Base>`: rewritten with the drawing API in
  6c/6d.
