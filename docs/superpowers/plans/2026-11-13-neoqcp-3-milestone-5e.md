# NeoQCP 3.0 milestone 5e — the meta-object surface Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** every user-facing property of a `Q_OBJECT` class is a `Q_PROPERTY` that is constant, plain (`NOTIFY`) or
derived, with the macros A14 lists, and one generic test that checks the whole surface through `metaObject()`.

**Architecture:** plain `NOTIFY` only (maintainer, 2026-11-14, see E-0). Every property with a `NOTIFY` is bindable
from C++ through `QBindable(object, "name")`. Change signals keep 4a's deferred delivery (`qcpEmit`).

**Tech Stack:** C++20, Qt 6.12 (public API only), meson + `cmake/sources.cmake`, QtTest, `tests/benchmark`.

**Spec:** `docs/specs/2026-11-11-neoqcp-3-milestone-5-design.md`, **A14** (binding, amended by E-0). Facts:
`docs/plans/neoqcp-3/m5e-inventory.md` (moc-built; sections cited as I-n, open questions as Q1-Q16). Pre-flight
review: `docs/plans/neoqcp-3/m5e-plan-review.md` (B1-B2, I1-I10, M1-M11, all folded in below). Earlier review:
`.superpowers/m5-bindable-review.md`.

## Calls

- **E-0 (maintainer, 2026-11-14: "Plain only") No `BINDABLE` storage in 5e.** Qt's `QObjectBindableProperty::notify`
  runs the observers first and then the owner's handler without a liveness check (`qproperty.h:1205-1214`), so an
  observer that deletes the object makes Qt run the handler on freed memory (review B2). The plain form has no such
  step: the `NOTIFY` is the setter's last action. Storage goes to the backlog. This replaces A14's storage form, its
  `setValue`-first rule and the read-side bench; Q1, Q2, Q3, Q10 and review I2, I3, I6, M10, M11 lapse.
- **E-4 (Q4)** One signal per property, `xChanged(T)` with the new value, as Qt names them, through `qcpEmit`. The
  theme keeps its single `changed()`.
- **E-4b (review I4)** Every function that changes a property's value emits its `NOTIFY`, not only the setter:
  `QCPAxis::setColor` (five properties), the two-argument tick lengths, `setRangeLower/Upper`, and every other site
  each task's report lists.
- **E-4c (review I5)** `qcpEmit` returns before allocating when the signal has no receiver
  (`isSignalConnected`), since `setOuterRect` and friends run per element per layout pass. Tasks 2 and 3 are benched.
- **E-5 (Q5)** The theme keeps its direct `emit`; a test asserts a slot on `changed()` that deletes a plot is safe.
- **E-5b (review B1)** The seven `QCustomPlot::theme*` forwarders stay (stylesheet theming, `docs/styling.md:75-95`,
  `test-theme.cpp:158-166`), as plain properties with one `NOTIFY themeChanged()` emitted by `setTheme` and by the
  theme-signal relay (`core.cpp:656`).
- **E-6 (review M1)** Object-typed properties keep `READ` on the public getter, handle-returning ones included
  (`QCPHandle::operator T*`, `handle.h:69`, 5b's ruling). `QCPHandle<T>` gets no metatype.
- **E-7 (Q7)** Gadget properties (no `NOTIFY`) only on `QCPScatterStyle`, `QCPLineEnding`, `QCPColorGradient`.
  Tickers keep `Q_DISABLE_COPY` (review M8).
- **E-8 (Q8)** `QCPTransform`, `QCP::Position`, `QCPTransform::Kind` and the protected `DateStrategy` stay off the
  surface (review M4).
- **E-9 (Q9)** `WRITE` names the `void` setter; the generic test writes through `QMetaProperty::write`.
- **E-11 (Q11, review I1)** A read-only property without a change signal is dropped from the surface; its accessor
  stays. `CONSTANT` only where fixed for the object's life: `QCPAxis::axisType`, `QCPLayer::parentPlot` and `name`.
  `parentPlot` of layerables and layout elements is not constant (`take`/`adopt`, `core.cpp:1515/1563/1628/1645`):
  dropped. The tests using dropped names are ported in the same commit (`test-handle.cpp:960-976`, `:1030`).
- **E-12 (Q12)** The non-pair setters are not split; E-14's scope gets `Q_INVOKABLE`, the rest stays C++.
- **E-13 (Q13, review M3)** The `Q_DECLARE_METATYPE` lines on enums moc registers are deleted, except the ones that
  name an alias (`QCPCurve::LineStyle` and any other the task finds): deleting those renames the registered type.
- **E-14 (Q14, review M2)** `Q_INVOKABLE` on the candidates of inventory section 6.2 in `QCustomPlot`, `QCPLayout*`,
  `QCPLegend`, `QCPAxisBase`/`QCPAxis`; already-slots and handle-returning actions excluded; one overload each.
- **E-15 (Q15)** No new `Q_DECLARE_TYPEINFO`; the 11 existing ones are re-checked, a wrong one removed. No
  `Q_DECLARE_METATYPE` is added unless a property type fails to register (the task lists it).
- **E-16 (Q16, review I8)** The generic test's fixtures are concrete classes on a real plot; mix-ins are covered
  through them. It carries two tables written in Task 1: a "different value" per property type (about 25 types) and
  an exception list of setters that refuse, clamp, trim or take ownership (`plottable.cpp:191-203`,
  `plottable-line.cpp:88-91`, ...), each checked by its own rule.
- **E-17** Builds as 5d: Claude subagents implement; builds, suites, ASan+UBSan and bench on the Coder workspace
  `neoqcp`; one build per tree at a time; every commit builds.

## Global Constraints

- Public Qt API only. No `BINDABLE`, no `QObjectBindableProperty` (E-0).
- Change signals go through `qcpEmit` (4a), except the theme (E-5); a no-receiver emit costs no allocation (E-4c).
- Bench: remote A/B against the 5e base after Tasks 2 and 3 (5 % drawing, item, axis, layout rows; 15 % others).
- No shims, no deprecated forwarders. Failing tests first. Never bend library behaviour to please a test.
- Each break goes into `docs/plans/neoqcp-3/sciqlopplots-migration.md` (dropped properties, deleted metatype lines).
  SciQLopPlots reads no QCP property by name today (review, question 4).
- Commit with explicit pathspecs; never `subprojects/.wraplock`, `subprojects/jkqtmathtext`, `qt.ini`, `.superpowers`.
  Push only when asked.

## Review Focus

1. A `NOTIFY` slot or `QBindable` observer that deletes the object or the plot: no use-after-free, no further signal.
2. A binding through `QBindable(obj, "x")` drives the setter and repaints exactly as a call would.
3. A `NOTIFY` fires once per change, never on an equal write, after delivery, also inside a batch and from the
   non-setter writers of E-4b.
4. A binding that outlives the object it reads (review I10): documented, and the spike shows what happens.
5. Layout and paint rows stay inside the gate with the new emits (E-4c).

---

### Task 1: The spike, the form table, the generic test skeleton

**Files:** `src/axis/axis.{h,cpp}` (label), `src/plottables/plottable.{h,cpp}`, `plottable-line.cpp:84`,
`plottable-curve.cpp:301` (pen and its overrides), `src/axis/axis-base.{h,cpp}` (scale type), `src/core.h` (E-4c);
new `tests/auto/test-meta-surface/` (meson, CMake list, runner); `docs/plans/neoqcp-3/m5e-forms.md` (new).

- [ ] Failing tests on the three spike properties (axis label, plottable pen, axis scale type), each plain: the
  deferred signal fires once after delivery; a `QBindable(obj, "x")` subscriber sees the change; a binding to a source
  property drives the setter and repaints; a slot and a `QBindable` observer that delete the object; one that deletes
  the plot; a binding driven to a refused scale type (the value stays, no signal); a binding whose source object dies
  first (review I10).
- [ ] E-4c in `qcpEmit`, with a test that a no-receiver emit inside a replot scope queues nothing.
- [ ] `m5e-forms.md`: one row per property (inventory sections 2, 3.1, 3.3) with its final form: constant, plain,
  derived-with-signal, dropped, gadget, out. Tasks 2-4 implement exactly this table.
- [ ] Generic test skeleton with E-16's two tables; runs on the spike classes.
- [ ] Commit `feat(meta): the spike — label, pen and scale type notify and bind`.

### Task 2: NOTIFY on the existing properties (E-4, E-4b, E-5, E-5b, E-6, E-11)

- [ ] Per area, every writable property in the form table gets its signal and every writer of E-4b emits it;
  existing signals wired as `NOTIFY` (`visibleChanged`, `colorScaleChanged`); constants and drops per E-11, tests
  ported; the theme forwarders per E-5b with the theme deletion test.
- [ ] Areas, one commit each, each building: axes and grids; layout, legend, text and colour scale; plottables;
  items; the plot and the theme; systems, areas and alignment groups; overlay, polar classes, selection rect
  (review M9).
- [ ] Generic test extended to every fixture. Remote bench A/B.

### Task 3: The unexposed pairs (inventory 3.1 and 3.3, review I9)

- [ ] The one-argument pairs and the 3.0 classes' read-only surface (coordinate systems, areas, alignment groups)
  get `Q_PROPERTY` per the form table; gadget pairs only for E-7's three types (`readOnGadget`/`writeOnGadget` in the
  generic test); pipeline state stays out.
- [ ] Commits per area as Task 2. Remote bench A/B.

### Task 4: Macros (E-12..E-15)

- [ ] `Q_ENUM`/`Q_FLAG` for the inventory 6.1 public enums (`EditModes` as `Q_FLAG`); the three outside any
  meta-object scope listed as moved or out.
- [ ] `Q_INVOKABLE` per E-14; `Q_DISABLE_COPY_MOVE` on every `QObject` class (replacing `Q_DISABLE_COPY`); metatype
  lines per E-13; TYPEINFO re-check per E-15.
- [ ] Commit `refactor(meta): enums, invokables and copy rules`.

### Task 5: Review and finish

- [ ] Final review by a fresh Opus reviewer; one fix batch.
- [ ] Whole suites (offscreen, Xvfb ratio 1 and 2), remote ASan+UBSan, clang, snippets, examples, `check-sources.py`,
  bench whole table; CI green after the push.
- [ ] `docs/migrating.md` (bindings: `QBindable(obj, "x")`, read with `value()` inside a binding, a plain write does
  not break it, review M7), `changelog.txt`, `baseline.md`, `backlog.md` (storage form, E-0), `milestone-5e-rulings.md`,
  `sciqlopplots-migration.md`. Commit `docs(neoqcp-3): end of milestone 5e`.
