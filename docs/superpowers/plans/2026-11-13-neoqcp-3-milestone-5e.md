# NeoQCP 3.0 milestone 5e — the meta-object surface Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** every user-facing property of a `Q_OBJECT` class is a `Q_PROPERTY` in one of A14's four forms (constant,
plain `NOTIFY`, `BINDABLE` storage, derived), with the macros A14 lists, and one generic test that checks the whole
surface through `metaObject()`.

**Architecture:** plain `NOTIFY` is the default; storage only for trivial setters whose getter is off the paint and
layout path, or whose A/B passes the 5 % gate. Change signals keep 4a's deferred delivery (`qcpEmit`). Bindings are a
C++ feature; PySide sees `NOTIFY` semantics.

**Tech Stack:** C++20, Qt 6.12 (`QObjectBindableProperty`, `QBindable`, public API only), meson +
`cmake/sources.cmake`, QtTest, `tests/benchmark`.

**Spec:** `docs/specs/2026-11-11-neoqcp-3-milestone-5-design.md`, **A14** (replaces C11; binding). Facts:
`docs/plans/neoqcp-3/m5e-inventory.md` (moc-built, 267 properties, 211 unexposed pairs; its section numbers are cited
as I-n, its open questions as Q1-Q16). Earlier review: `.superpowers/m5-bindable-review.md`.

## Calls (E-1..E-16 answer Q1-Q16; delegated, the maintainer can overrule)

- **E-1 (Q1) Storage waves.** Wave 0 is the spike (Task 1). Wave 1 (Task 4) is every storage candidate the inventory
  marks neither Y nor p. The Y/p candidates are decided by one A/B (Task 4, step 1) on three representative getters
  read per frame (a plottable pen, `QCPLayerable::visible`, an axis label font). If the unbound read stays inside 5 %
  on the drawing rows, the Y/p candidates of the same kind join wave 1; if not, they stay plain and the plan says so.
- **E-2 (Q2)** Invalidating a cache (`mCachedMarginValid = false`) counts as "mark dirty": allowed in a storage
  handler. A setter that writes into `mAxisPainter` stays **plain**; painter state does not move in 5e.
- **E-3 (Q3)** The pinned hot block of `QCPAxisBase` (`axis-base.h:195-211`) is not touched: `rangeDraggable` and
  `rangeZoomable` are plain.
- **E-4 (Q4)** One signal per property, `xChanged(T)` with the new value, as Qt names them, delivered through
  `qcpEmit`. Exception: the theme keeps its single `changed()` (its sixteen values change together on a theme swap).
- **E-5 (Q5)** The theme keeps its direct `emit`: it belongs to no plot, and its only receivers mark plots dirty.
  A test asserts a slot on `changed()` that deletes a plot does not crash (Task 2). Cost if wrong: one deferral scope.
- **E-6 (Q6)** Object-typed properties have raw pointer types (moc, `QVariant` and PySide know `QObject*`; C8 keeps
  signal arguments raw too). Where the public getter returns a handle, `READ` names the existing raw accessor
  (`keyAxisRaw` style). `QCPHandle<T>` gets no metatype. Writable object properties stay writable only where the
  setter exists today.
- **E-7 (Q7)** Gadget properties (no `NOTIFY`) only on the value types users copy and compare: `QCPScatterStyle`,
  `QCPLineEnding`, `QCPColorGradient`. Tickers, painter, decorators, item positions, label painter: none.
- **E-8 (Q8)** `QCPTransform`, `QCP::Position` and `QCPTransform::Kind` stay off the surface (documented limit).
- **E-9 (Q9)** `WRITE` always names the `void` setter; the generic test writes through `QMetaProperty::write`, which
  proves moc picked it.
- **E-10 (Q10)** Only storage-backed setters are rewritten to call `setValue` first (A14); plain setters keep their
  early return.
- **E-11 (Q11)** A derived property without a real change event is dropped from the surface, unless it is fixed for
  the object's life: then `CONSTANT` (`parentPlot` of layers, layerables and layout elements; `axisType`). `index`,
  `children`, `parentLayerable`, `layout`, `rect`, `rowCount`, `columnCount`, `parentLegend`, `barBelow`, `barAbove`
  are dropped unless a signal for them already exists. The accessors stay.
- **E-12 (Q12)** The 101 non-pair setters are not split. The ones in E-14's scope become `Q_INVOKABLE`; the rest stay
  plain C++.
- **E-13 (Q13)** The 33 `Q_DECLARE_METATYPE` lines on enums moc already registers are deleted (Qt 6 needs none);
  migration row for SciQLopPlots (its shiboken typesystem uses the enum names, not the macro).
- **E-14 (Q14)** `Q_INVOKABLE` on the non-slot, non-const, non-overloaded actions of `QCustomPlot`, `QCPLayout*`,
  `QCPLegend` and `QCPAxisBase`/`QCPAxis` (`rescale`, `rescaleAxes`, ...). Overloaded ones get it only on one
  overload, listed in the task report. Handle-returning actions are excluded.
- **E-15 (Q15)** No new `Q_DECLARE_TYPEINFO`. The existing 11 are re-checked (each type's members are relocatable);
  a wrong one is removed, not "fixed".
- **E-16 (Q16)** The generic test's fixtures are concrete classes on a real plot; a mix-in (`QCPAbstractColorMapped`,
  `QCPAbstractPlottable`, `QCPLayerable`) is covered through its concrete classes. Classes no user constructs alone
  (the plot's internal layerables) are reached through the plot.
- **E-17 Builds** as 5d: Claude subagents implement; builds, suites, ASan+UBSan and bench on the Coder workspace
  `neoqcp`; one build per tree at a time; every commit builds.

## Global Constraints

- Public Qt API only: never `Q_OBJECT_COMPAT_PROPERTY`, never `QtCore/private` (A14).
- Storage setters call `setValue` first, then the equality early return (A14).
- Bindings are single-thread: storage handlers carry a debug thread-affinity check; no pipeline state is a property.
- Change signals go through `qcpEmit` (4a), except the theme (E-5).
- Bench: remote A/B against the 5e base for every task that changes a getter on the paint/layout path (5 % drawing,
  item, axis, layout rows; 15 % others).
- No shims, no deprecated forwarders (NeoQCP-only APIs). Failing tests first. Never bend library behaviour to please a
  test.
- Each break goes into `docs/plans/neoqcp-3/sciqlopplots-migration.md` (dropped properties, deleted metatype lines).
- Commit with explicit pathspecs; never `subprojects/.wraplock`, `subprojects/jkqtmathtext`, `qt.ini`, `.superpowers`.
  Push only when asked.

## Review Focus

1. A binding whose observer deletes the object or the plot (spike) — no use-after-free, no signal after deletion.
2. A plain write breaks a binding on a storage property, even when it writes the current value.
3. A binding update repaints and re-records exactly as the setter did (dirty marks, revision bumps, margin caches).
4. A `NOTIFY` fires once per change and never on an equal write, after delivery, also inside a batch.
5. PySide: property access keeps working for the names the SciQLopPlots typesystem uses (dropped ones listed).

---

### Task 1: The spike and the generic test skeleton (A14 spike, E-16)

**Files:** `src/axis/axis.{h,cpp}` (label), `src/plottables/plottable.{h,cpp}` (pen), `src/axis/axis-base.{h,cpp}`
(scale type); new `tests/auto/test-meta-surface/` (meson, CMake list, runner registration).

- [ ] Failing tests per spike property: the deferred signal fires once after delivery; a `QBindable` subscriber sees
  the change; a binding to a source property drives the value and repaints; a plain write breaks it (storage one,
  also on an equal value); a binding observer that deletes the object; a slot that deletes the plot.
- [ ] Axis label: `Q_OBJECT_BINDABLE_PROPERTY` with a handler that marks dirty, invalidates the margin cache and
  `qcpEmit`s `labelChanged(QString)`; `QBindable<QString> bindableLabel()`.
- [ ] Plottable pen: plain `NOTIFY penChanged(QPen)`; `bindablePen()` returns `QBindable<QPen>(this, "pen")`.
- [ ] Scale type: plain `NOTIFY` (it can refuse), no storage.
- [ ] Generic test skeleton: walks a fixture's `metaObject()` properties and checks by form (read, write back what it
  read, NOTIFY once on change, none on equal write; constant: no WRITE/NOTIFY). Runs on the spike classes.
- [ ] Bench A/B of axis and plottable rows; suites; ASan on the new test. Commit
  `feat(meta): the spike — a bindable axis label, a pen and a scale type with NOTIFY`.
- [ ] Ruling recorded: if the spike shows a form cannot keep recording, repaint and deferral right, stop and rule
  before Task 2.

### Task 2: Plain NOTIFY everywhere, constant and derived (E-4, E-5, E-6, E-11)

- [ ] Per class (I-2), every writable property gets its `xChanged(T)` signal, emitted through `qcpEmit` where the
  setter changes the value; existing signals wired as `NOTIFY` (`visibleChanged`, `colorScaleChanged`).
- [ ] `CONSTANT` and drops per E-11; the nine I-2 drops (`tickVector`, `tickVectorLabels`, seven `theme*`
  forwarders); object-typed properties per E-6.
- [ ] Theme deletion test (E-5). Generic test extended to every class with fixtures (E-16).
- [ ] Commits per area (axes, layout and legend, plottables, items, plot), each building.

### Task 3: The unexposed pairs (I-3.1)

- [ ] The 211 one-argument pairs get `Q_PROPERTY` (plain unless Task 4 makes them storage); gadget pairs only for
  E-7's three types (no `NOTIFY`; `readOnGadget`/`writeOnGadget` in the generic test); pipeline state stays out.
- [ ] Commits per area.

### Task 4: Storage waves (E-1, E-2, E-10)

- [ ] Step 1: A/B of the three representative getters as storage, unbound, against the drawing rows. Ruling on Y/p.
- [ ] Wave 1: storage for the candidates E-1 admits; setters rewritten per E-10; `bindableX()` accessors; handlers
  with the thread check; generic test follow-and-break checks on every storage property.
- [ ] Remote bench A/B of the whole table; commit per area.

### Task 5: Macros (E-12..E-15, A14 lists)

- [ ] `Q_ENUM`/`Q_FLAG` for the I-6.1 public enums (`EditModes` as `Q_FLAG`); the three outside any meta-object scope
  move into one or stay out (listed).
- [ ] `Q_INVOKABLE` per E-14; `Q_DISABLE_COPY_MOVE` on every `QObject` class (replacing `Q_DISABLE_COPY`); the 33
  metatype lines deleted (E-13); the 11 TYPEINFO re-checked (E-15).
- [ ] Commit `refactor(meta): enums, invokables and copy rules`.

### Task 6: Review and finish

- [ ] Final review by a fresh Opus reviewer; one fix batch.
- [ ] Whole suites (offscreen, Xvfb ratio 1 and 2), remote ASan+UBSan, clang, snippets, examples, `check-sources.py`,
  bench whole table; CI green after the push.
- [ ] `docs/migrating.md` (bindings section), `changelog.txt`, `baseline.md`, `backlog.md`, `milestone-5e-rulings.md`,
  `sciqlopplots-migration.md`. Commit `docs(neoqcp-3): end of milestone 5e`.
