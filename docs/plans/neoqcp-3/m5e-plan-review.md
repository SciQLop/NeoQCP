# Pre-flight review: NeoQCP 3.0 milestone 5e plan

Plan: `docs/superpowers/plans/2026-11-13-neoqcp-3-milestone-5e.md`. Checked against the tree at `31bf8cf` (worktree
`/home/jeandet/Documents/prog/NeoQCP-3.0`, branch `neoqcp-3.0`), against A14 of
`docs/specs/2026-11-11-neoqcp-3-milestone-5-design.md`, and against the Qt 6.12.0 headers in
`~/Qt/6.12.0/gcc_64/include/QtCore` (`qproperty.h`, `qbindingstorage.h`). Qt docs: `properties.html`,
`qbindable.html`. Paths are relative to the worktree unless they start with `SciQLopPlots/`.

Tally: 2 blockers, 10 important, 11 minor.

## Coverage summary

The calls against A14, one by one:

| Call | Verdict |
|---|---|
| E-1 | Agrees in intent. The three getters it benches are the wrong ones (I2). |
| E-2 | Agrees. Invalidating a cache is "mark dirty". Painter-resident state stays plain. |
| E-3 | Agrees. |
| E-4 | Agrees. A14 does not fix the signal shape. `rangeChanged(QCPRange, QCPRange)` stays an existing exception (`axis-base.h:190`). |
| E-5 | Agrees. The theme's `emit changed()` is the last statement of every setter (`theme.cpp:16-19`). The owned theme is a child of the plot (`core.cpp:632`). So a slot that deletes the plot returns into nothing. The planned test is enough. |
| E-6 | Does not contradict A14. It does contradict the 5b ruling it says it follows (M1). |
| E-7 | Agrees. The gadgets it leaves out are not value types users copy. The tickers are shared and `Q_DISABLE_COPY`; positions, anchors and decorators are owned and polymorphic; the painter and the label painter are internal. |
| E-8, E-9, E-10, E-12, E-15, E-16, E-17 | Agree. |
| E-11 | The rule agrees with A14. Its `CONSTANT` list is wrong in fact (I1). One of its drops (the seven `theme*` forwarders, from Task 2) breaks a documented feature (B1). |
| E-13 | Agrees. Its stated reason for the migration row is wrong (M3). |
| E-14 | Agrees. A14 asks for the list with signatures; the plan defers it to a task report (M2). |

What A14 asks for that has no task:

- **The per-property form list "in the 5e plan"** (A14, first bullet). The plan points at the inventory, whose forms
  are proposals ("storage 143 is the ceiling, not the plan"). See I6.
- **The 3.0 classes' read-only surface** (inventory 3.3: coordinate systems, areas, alignment groups). No task
  names them. See I9.
- **`DateStrategy`** is named in A14's `Q_ENUM` list. It has no disposition (M4).
- **`Q_DECLARE_METATYPE` "where a property, `QVariant` or PySide needs it."** Only the deletions have a task. A
  one-line "none needed, because Qt 6 registers complete types" in Task 5 closes it (inventory 6.5 has the facts).

Answer to question 4 of the brief (SciQLopPlots). SciQLopPlots uses none of the dropped names. Its typesystem binds
no `QCP` class. The only `QCP` mentions in `SciQLopPlots/SciQLopPlots/bindings/bindings.xml` are at `:518`,
`:1111`, `:1114` and `:2299`: one wrapper name and three comments. Its C++ reads no `QCP` property by name. The only
property on a `QCP` object is the dynamic `sqp_wrapper` (`SciQLopPlots/src/SciQLopWaterfallGraph.cpp:34`,
`SciQLopPlots/src/SciQLopCrosshair.cpp:248`), and no `Q_PROPERTY` is planned with that name. Two NeoQCP tests do use
the dropped names (I7). Review Focus 5 ("PySide property access for the names the SciQLopPlots typesystem uses")
therefore checks nothing. Replace it with I7's list.

---

## Blockers

### B1. Dropping the seven `theme*` forwarders removes stylesheet theming

What the plan says. Task 2 drops "the nine I-2 drops (`tickVector`, `tickVectorLabels`, seven `theme*`
forwarders)". The inventory's reason is "duplicate surface" (I-2, `QCustomPlot` table).

What the code does. These properties are the only way a Qt style sheet reaches the theme:

- `docs/styling.md:75-95`, "Theme from a Qt stylesheet", documents
  `qproperty-themeBackground: #1e1e1e;` and the six others.
- `tests/auto/test-theme/test-theme.cpp:158-166` (`qssProxyProperties`) tests it.
- `QCPTheme` is a plain `QObject`, not a widget (`theme.cpp:3`). A style sheet sets `qproperty-*` only on widgets.
  The duplicate on `QCPTheme` cannot replace the plot's forwarders.

They are not duplicates. They are the widget-side end of the theme.

Amendment. Keep the seven as **plain** properties of `QCustomPlot`. Give them one `NOTIFY themeChanged()` (A14
allows a shared signal; E-4 already has the theme exception). Emit it from `setTheme` (`core.cpp:640-650`) and from
the lambda that `connectThemeSignal` connects to `QCPTheme::changed` (`core.cpp:656`). Remove them from Task 2's
drop list and from the migration page. The other option is a maintainer ruling that drops stylesheet theming. That
ruling would also delete the doc section and the test in the same commit.

### B2. A storage handler runs on freed memory when an observer deletes the object

What the plan says. Task 1 tests "a binding observer that deletes the object" on the storage label. If the spike
shows a form cannot keep things right, "stop and rule before Task 2". Task 4 builds every storage property on top.

What Qt does. `QObjectBindableProperty::notify` (`qproperty.h:1205-1214`) runs in this order:

1. `binding->notifyObservers(this, qGetBindingStorage(owner()))`. This runs every `QPropertyNotifier`,
   every `onValueChanged` and every dependent binding, synchronously.
2. `(owner()->*Signal)(...)`. This is the handler. `owner()` is computed from `this`, the property member inside the
   object (`:1101-1105`). Nothing checks whether the object still exists.

If step 1 deletes the axis, step 2 calls the handler on freed memory. A handler cannot defend itself, because its
`this` is already gone. `setValue` (`:1170-1179`) also has nothing after `notify`. So the only unsafe step is the
handler call, and Qt owns that step. The same order means an observer sees the new label before the handler marks
the axis dirty and invalidates its margin cache. That is B5 of `.superpowers/m5-bindable-review.md`, now confirmed
in the 6.12 header.

The plain form does not have this problem. A `QBindable(obj, "pen")` subscriber hangs off a slot object connected to
the `NOTIFY` signal. `QMetaObject::activate` holds a reference to that slot object for the whole call. The signal is
the last thing the setter does (`qcpEmit`, `core.h:793-807`). So an observer that deletes the plottable returns
through Qt's sender-deletion handling and never comes back into the setter.

The spike's required test therefore cannot pass for the storage form. The outcome is known today. The plan should
not discover it in Task 1 and only then stop.

Amendment. Put the ruling in the plan now, with two options:

- **(a) Documented limit.** "An observer of a storage property must not delete its object or its plot
  synchronously. Use `deleteLater`." Task 1 tests the `deleteLater` path under ASan. Task 1 tests the synchronous
  path on the plain form only. The migration page states the rule. Cost if wrong: a user's notifier crashes
  instead of being refused. No guard is possible.
- **(b) No storage form in 5e.** Every property is plain plus a `QBindable(this, "x")` accessor where a binding is a
  real use. Task 4 disappears. A14's "storage only for trivial setters" stays satisfied by an empty set.

Either way, record in the plan the second fact: observers run before the handler's dirty marks. A binding that reads
layout-derived state (margins, a size hint) sees a stale cache.

---

## Important

### I1. E-11: `parentPlot` of layerables and layout elements is not fixed for the object's life

What the plan says. "`CONSTANT` (`parentPlot` of layers, layerables and layout elements; `axisType`)."

What the code does.

- Plottables and items leave a plot and join another. `QCustomPlot::detach` sets `mParentPlot = nullptr`
  (`core.cpp:1515` for plottables, `:1628` for items). `adoptPlottable` and `adopt` call `initializeParentPlot(this)`
  (`core.cpp:1563`, `:1645`). `take(handle)` is public (`core.h:417`, `:865-871`).
- A layout element can be built with no plot (`layout.h:88`, `explicit QCPLayoutElement(QCustomPlot* parentPlot =
  nullptr)`). It gets one later through `initializeParentPlot` (`layer.cpp:822-835`), called when it is added to a
  layout (`layout.cpp:495`, `:792`).
- The inventory itself marks `QCPLayerable::parentPlot` as **derived**: "changes on QCustomPlot::take/add"
  (I-2, `QCPLayerable` table).

Qt: "For a given object instance, the READ method of a constant property must return the same value every time"
(`properties.html`).

Amendment. Make these `CONSTANT`: `QCPLayer::parentPlot`, `QCPAxis::axisType`, `QCPAxis::axisRect`,
`QCPAxis::grid`, and `QCustomPlot::plotLayout`. Drop `QCPLayerable::parentPlot` from the surface under E-11's own
rule (no change signal). Or give it a `parentPlotChanged(QCustomPlot*)`, emitted from `initializeParentPlot` and from
`detach`.

### I2. E-1 benches three getters, and two of them are never storage

What the plan says. The Y/p ruling is "one A/B ... on three representative getters read per frame (a plottable pen,
`QCPLayerable::visible`, an axis label font)".

What the inventory says.

- The plottable `pen` is **plain**: its setter is virtual, and subclasses copy it into components
  (I-2 `QCPAbstractPlottable`). Task 1 makes it plain too.
- `QCPLayerable::visible` is **plain**: its setter invalidates the layer buffer (`layer.cpp:674-688`).

Only `labelFont` is a storage candidate. So the A/B measures two getters that will never change form. It cannot
decide "Y/p candidates of the same kind".

Amendment. Bench three getters that are storage candidates with Y or p:

- `QCPAxis::tickPen` (Y);
- `QCPLayerable::antialiased` (Y; read for each layerable in `applyDefaultAntialiasingHint`);
- `QCPAxis::labelFont` (Y).

Add one more rule to Task 4. Internal reads on the paint and layout path use `valueBypassingBindings()`. Only the
public getter calls `value()`. `value()` goes through `qGetBindingStorage(owner())->registerDependency(this)` on every
read (`qproperty.h:1148-1151`, `qbindingstorage.h:59-64`), which is the cost being gated. A14 says "any getter on the
paint or layout path ... is A/B'd". So make the whole-table bench at the end of Task 4 a per-property gate: a
property whose row moves goes back to plain.

A fact for E-1's memory question (inventory Q1). A `QObjectBindableProperty<..., T, ...>` is a `QPropertyData<T>`:
it is the size of `T`. Binding data lives in the object's `QBindingStorage` and is only allocated when something
binds. An unbound storage property costs no memory.

### I3. A storage member is not a drop-in replacement for the member it replaces

What the plan says. Task 4: "setters rewritten per E-10; `bindableX()` accessors; handlers".

What changes for the rest of the class:

- `QObjectBindableProperty::operator->` returns `void` for a type that is neither a pointer nor dereferenceable
  (`qproperty.h:1154-1163`). So `mBasePen->color()` does not compile.
- `QCPAxis::setColor` takes the address of three pen members and changes them through `QPen*`:
  `for (QPen* pen : {&mBasePen, &mTickPen, &mSubTickPen}) pen->setColor(color);` (`axis.cpp:1102-1103`). That stops
  compiling. Written as `setValue` instead, it removes the user's bindings on those pens and runs their handlers.
- Every internal assignment `mX = ...` becomes `setValue`: a binding break plus a handler call, constructors
  included.

Amendment. Task 4, per class: list every internal read and write of each storage member (grep `mX\b`).

- Reads on the paint and layout path use `valueBypassingBindings()`.
- Writes that are a user's intent (the setter, `setColor`) use `setValue`.
- Constructor initialisation stays in the member initialiser, so no handler runs.

Count these sites before choosing the waves. They are the real size of Task 4.

### I4. Every function that changes a property value must emit its `NOTIFY`, not only "the setter"

What the plan says. Task 2: "every writable property gets its `xChanged(T)` signal, emitted through `qcpEmit` where
the setter changes the value".

What the code does. Several non-pair functions (inventory 3.2) write property members directly:

- `QCPAxis::setColor` writes `labelColor`, `tickLabelColor`, `basePen`, `tickPen` and `subTickPen`
  (`axis.cpp:1095-1105`).
- The two-argument tick-length setters write `tickLengthIn`/`tickLengthOut`.
- The theme recolouring writes grid pens through their setters (`core.cpp:688-690`). That one is fine.

If those writers do not emit, a `QBindable` subscriber and a plain `NOTIFY` listener miss real changes. The generic
test cannot see it, because it only writes through `QMetaProperty::write`.

Amendment. Task 2 records, per property, every function that writes its member, and emits from each one. Add one
targeted test per multi-property writer (`setColor`, the tick lengths), checking that each affected signal fires once.

### I5. New `NOTIFY` signals on the layout path allocate inside every replot

What the plan says. The bench rule covers "every task that changes a **getter** on the paint/layout path". Tasks 2
and 3 have no bench.

What the code does.

- `replot()` opens an emission scope (`core.cpp:2931`).
- Inside an open scope, `qcpEmit` always builds a tuple and a `std::function` and queues them (`core.h:803-806`),
  even when nothing is connected.
- `setOuterRect` is called for each element in each layout pass: `layout.cpp:1663`, `:2091`, `area.cpp:286`,
  `layoutelement-colorscale.cpp:610`, `layoutelement-axisrect.cpp:945`.

An `outerRectChanged` emitted through `qcpEmit` therefore costs one heap allocation per element per relayout. The
same holds for any setter the library calls during layout or drawing (margins from the auto-margin pass, for example).

Amendment.

- Gate the deferred branch of `qcpEmit` on `sender->isSignalConnected(...)`. The pattern already exists: connect-notify
  gating in `axis-base.cpp:143` and `coordinate-system.cpp:186`. Note that a `QBindable(obj, "x")` adaptor counts as
  a connection, so gating stays correct for bindings.
- Or keep layout-driven properties (`outerRect`) without `NOTIFY` (derived, dropped).
- Bench Tasks 2 and 3 (layout and drawing rows) as Task 4 is benched.

### I6. Order: decide the forms first, then write each property once

What the plan says. Task 2 gives the 143 existing storage candidates plain `NOTIFY`. Task 3 does the same for the 83
new ones. Task 4 then rewrites the admitted ones as storage. So up to 226 setters, signals and tests are written twice.
A14 also wants the per-property form list in the plan, and the plan defers it.

Amendment.

- Move Task 4 step 1 (the A/B and the Y/p ruling) into Task 1. The spike already builds a storage property, the label.
- After Task 1, commit the final per-property form table, for example `docs/plans/neoqcp-3/m5e-forms.md` or an
  appendix to the plan. One row per property: form, signal, bench verdict.
- Tasks 2 and 3 then write each property in its final form. Task 4 shrinks to the handlers and the follow-and-break
  checks.

### I7. Drops break existing tests that the plan does not name

What the plan says. "Every commit builds". Nothing lists the tests that read the dropped names.

What the tests do.

- `tests/auto/test-handle/test-handle.cpp:960-976` reads `layout`, `children`, `parentLayerable`, `parentLegend`,
  `barAbove` and `barBelow` through `QObject::property`. These are six of E-11's drops.
- `:1030` checks that `children` is not writable. It would keep passing by accident on a dropped property, because
  `property(-1)` is invalid.
- `tests/auto/test-theme/test-theme.cpp:158-166` sets `themeBackground` and `themeForeground` (B1).

Amendment. Each drop commit edits these tests in the same commit. `:1030` becomes an explicit
`indexOfProperty(...) == -1` check. The list goes into Task 2.

### I8. The generic test needs a value table and an exception table, named up front

What the plan says. The test "reads, writes back what it read, NOTIFY once on change, none on equal write", for every
class with a fixture.

What it needs that the plan does not name:

- **A "different value" generator for each property type.** That covers about 25 metatypes: `QPen`, `QBrush`,
  `QFont`, `QColor`, `QString`, `bool`, `int`, `double`, every `Q_ENUM`/`Q_FLAG`, `QMargins`, `QRect`, `QSize`,
  `QPixmap`, `Qt::AspectRatioMode`, `QCPRange`, `QCPLineEnding`, `QCPScatterStyle`, `QCPColorGradient`,
  `QCPDataSelection`, `QSharedPointer<QCPAxisTicker>`, `QList<double>`, and object pointers. An object pointer needs a
  sibling object on the same plot.
- **Exceptions for setters that refuse, normalise or take ownership:**
  - `numberFormat` refuses;
  - `tickLabelRotation` clamps;
  - `selection` is trimmed by `selectable`;
  - writing back the same `selectionDecorator` is refused with a `qDebug` (`plottable.cpp:191-203`, `:966-976`);
  - `QCPAbstractLine::setPen` does work when `mPen` is equal but a component differs (`plottable-line.cpp:88-91`). So
    `penChanged` must fire only when `pen()` changes, or "none on equal write" fails.

Amendment. Make both tables Task 1 deliverables, as data (one entry per type, one per exception). Keep the fixture
list as a data table too (class → factory on a real plot).

### I9. The 3.0 classes' read-only surface has no task

Inventory 3.3 lists what goes on the surface:

- `QCPCoordinateSystem`: `area`, `keyAxis` and `valueAxis` are constant; `capabilities` is constant per subclass;
  `revision` is derived, on the existing `changed()`.
- `QCPArea::insetLayout` is constant.
- `QCPAlignmentGroup`: `sides` is constant; `elements` is derived, on `membersChanged()`.

Task 3 covers only I-3.1 pairs. Task 2 covers only existing properties.

Amendment. Add these to Task 2, as their own "3.0 classes" commit, with fixtures in the generic test.

### I10. A binding can outlive the object it reads

Qt's `QBindable(QObject*, const char*)` page recommends capturing the `QBindable` by value in a binding lambda. Once
that object is deleted, the next evaluation of the binding (triggered by another dependency) reads a dead object.
The same holds for `bindableLabel()` of an axis that is later removed. Plottables, items and axes are deleted
routinely: `removePlottable`, `take`, axis removal.

Amendment. Task 1 adds one more spike case: "the source object is deleted while a binding to it lives; another
dependency then changes." Expected result: documented, not crashing. Either the user captures a `QCPHandle` and checks
it, or the binding must be removed first. The bindings section of `docs/migrating.md` states the rule.

---

## Minor

### M1. E-6 changes READs that work today, against 5b's ruling

Today `READ` names the handle getter, and moc converts through `QCPHandle::operator T*()` (`handle.h:69`). Examples:
`plottable.h:117` (`keyAxis`), with every object property read and written in `test-handle.cpp:930-1033`.

5b ruled that object properties keep pointer types, with a raw accessor only where moc cannot convert a list (R-5,
`layer.h:65`, `:187`). Some raw accessors are protected (`keyAxisRaw`, `plottable.h:329`), others do not exist.

Change E-6 to "keep today's READs; a raw accessor only for lists (R-5)", or state why the switch is worth the churn.

### M2. E-14's example names a slot, and its list is not in the plan

`rescaleAxes` is already a slot (inventory 6.2). A14 asks for "named actions ... listed with their signatures".
Attach inventory 6.2 as the list, with the overload chosen for each `ovl` row.

### M3. E-13: the stated reason is wrong, and one deletion renames a type

The SciQLopPlots typesystem names no `QCP` enum (see the coverage summary). The migration row is still useful, but
for out-of-tree string lookups, not for that reason.

`Q_DECLARE_METATYPE(QCPCurve::LineStyle)` (`plottable-curve.h:158`) names an alias of `QCPAbstractLine::LineStyle`
(`:102`, `plottable-line.h:26-27`). Deleting it changes the registered name to `QCPAbstractLine::LineStyle`. Mention
it in the migration row.

### M4. `DateStrategy` needs a disposition

A14 names it. It is protected in a non-gadget class (`axistickerdatetime.h:63`). Task 5 should say "out" explicitly.

### M5. Task 1's file list is missing the pen overrides

`QCPAbstractLine::setPen` (`plottable-line.cpp:84`) and `QCPCurve::setPen` (`plottable-curve.cpp:301`) must emit
`penChanged` exactly once, and the curve must not emit twice through the base.

### M6. Two more spike cases

- **A binding that drives `scaleType` to a refused value.** The polar angular axis refuses logarithmic. The expected
  result: the value stays, there is no notify, and the binding stays.
- **A `QScopedPropertyUpdateGroup` around storage writes.** The handlers (and so `qcpEmit` and the dirty marks) run
  when the group ends. If an object is deleted inside the group, that is the B2 case again.

### M7. A plain write never breaks a `QBindable(obj, "x")` binding

Qt's adaptor does not notify while it holds a binding. So `setPen` after `bindablePen().setBinding(...)` is overwritten
on the next source change. Review Focus 2 is right to limit break checks to storage. The migration page should say
that plain and storage behave differently here.

### M8. `Q_DISABLE_COPY` on the tickers stays

There are 28 `Q_DISABLE_COPY` occurrences in `src/`; 23 are on `QObject` classes (inventory 6.3). Task 5 should say
that the gadget tickers keep theirs, per A14 ("never on value types").

### M9. Task 2's areas are incomplete

The list "axes, layout and legend, plottables, items, plot" misses the theme, coordinate systems and areas, the
overlay, the polar classes and the selection rect.

### M10. `BINDABLE` only on storage properties

The `bindablePen()` wrapper must not be named in `Q_PROPERTY ... BINDABLE`. Qt's `QBindable(QObject*, const char*)`
page: "This constructor should not be used to implement BINDABLE". The generic test should pick storage checks by
`QMetaProperty::isBindable()`.

### M11. Handler mechanics to write into Task 1

- The handler is declared before its `Q_OBJECT_BINDABLE_PROPERTY` line, because it is a template argument.
- `qcpEmit` is the handler's last statement.
- A storage setter does nothing after `setValue`. With no scope open, `qcpEmit` emits at once, from inside `notify`
  (`core.h:798-801`).
