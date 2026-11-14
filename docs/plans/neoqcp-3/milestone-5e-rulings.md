# NeoQCP 3.0 — milestone 5e rulings and deferred items

Milestone 5e: the meta-object surface. Every user-facing property of a `Q_OBJECT` class is now a `Q_PROPERTY`. Each
one is constant, plain (with a `NOTIFY`) or derived. Every property with a `NOTIFY` can be bound from C++ through
`QBindable<T>(object, "name")`. Change signals keep the deferred delivery of 4a. Public enums are registered,
actions are `Q_INVOKABLE`, and every `QObject` class says `Q_DISABLE_COPY_MOVE`. A generic test walks the whole
surface through `metaObject()`. Branch `neoqcp-3.0`, `bd42974..` the commit that adds this file (not pushed).
Plan: `docs/superpowers/plans/2026-11-13-neoqcp-3-milestone-5e.md` (calls E-0..E-17), pre-flight review
`m5e-plan-review.md`, facts `m5e-inventory.md`, form table `m5e-forms.md`. Design:
`docs/specs/2026-11-11-neoqcp-3-milestone-5-design.md`, A14 (amended by E-0).

Every task was implemented by a Claude subagent and reviewed by a fresh one. A fresh Opus reviewer read the whole
milestone at the end, and one fix batch followed. All builds, suites, ASan+UBSan runs and benches ran on the Coder
workspace `neoqcp`.

## Results

- Offscreen 4079 passed, 0 failed, 135 skipped. Xvfb 4214/0. The whole ASan + UBSan suite 4079/0/135, with no
  sanitizer report. All on the Coder workspace at 8977561. 5d ended at 2549 offscreen: most of the new rows are
  the generic test (`tests/auto/test-meta-surface`).
- Before the push, at e5dcd84 (the error-bar fix): offscreen 4081 passed, 0 failed, 135 skipped; Xvfb 4216/0; ASan
  `TestMetaSurface` 1526/0 with no sanitizer report; clang `-fsyntax-only` clean on all 101 files in `src/`.
- clang `-fsyntax-only` clean on every file each task touched. `check-sources.py`: the file lists match.
- Guide snippets green (`doc-snippets`, `doc-offscreen-export`), with two new binding snippets for `migrating.md`.
  Every example builds.
- CI: not run yet. The milestone is not pushed.
- Bench: each task's A/B stayed inside the gates, except the rows ruled below (R-17 to R-20, R-23). The whole-table
  end-of-milestone A/B is in `baseline.md`, "End of milestone 5e": 110 rows, 5 pairs, then 9 for the ten rows
  over their gate. Two rows stay over after 9 pairs:
  - `QCPItems_RestyleAll10k` +7.8 %, a layout-sensitive row (R-19).
  - `QCPDrawContext_ReplayMarkers100k` +10.2 %. It is code layout (R-23): over six linker layouts it is +0.2 %.

## Rulings (what, why, cost if wrong)

- R-1 (maintainer, E-0): plain `NOTIFY` only, no `BINDABLE` storage. Qt's `QObjectBindableProperty::notify` runs
  the observers first, then the owner's handler, with no liveness check. An observer that deletes the object makes
  Qt run the handler on freed memory. In the plain form the `NOTIFY` is the setter's last action, so nothing runs
  after it. Cost: no lazy bindings and no binding storage; it is in the backlog.
- R-2 (E-4c): only `qcpEmit`, the property-signal path, checks for a receiver before it queues. Range and state
  emits do not check. The coordinate system counts its own listeners (`qcpEmitHeard`). The check takes Qt's
  connection mutex: in all three paths it cost 5-6 % on `SetRange10k`. Cost: one queue entry per unheard range emit.
- R-3 (E-4c): a deferred signal that had no receiver when it was queued is not delivered to a receiver connected
  before delivery. This is plain Qt behaviour, but 4a delivered it. Cost: a late connect misses one change. It is
  in `migrating.md`.
- R-4 (E-11): `CONSTANT` means fixed for the object's life. The three names of E-11 are examples. So
  `QCPAxis::axisRect` is constant, and the six constants of the 3.0 classes stay. Cost: seven `CONSTANT` lines to
  drop.
- R-5 (E-4): the existing signals without a value keep their signatures and are wired as the `NOTIFY`. Changing
  them breaks every connect, and `QBindable` works on a no-argument `NOTIFY`. Cost: mixed signal shapes.
- R-6 (E-11): `QCPErrorBars::data` is dropped from the surface. It is a shared container, changed in place with no
  signal. Cost: re-add it with a signal.
- R-7: the protected hook `QCPAbstractPlottable::systemChanged()` is renamed `systemDidChange()`, to free the name
  for the `system` property's signal. Cost: one rename in each subclass that overrides it (migration row).
- R-8 (E-9): `setCurrentLayer(QCPLayer*)` keeps its `bool` return as the `WRITE`. moc accepts it, and the `void`
  rule yields to not breaking the API. Cost: none at run time.
- R-9 (E-4b): every function that really moves a value emits, even when the move is transient. An export sets
  the viewport and restores it. A grid reshape moves the stretch factors. `themeChanged` fires on every theme
  value. The task reviewer upheld the export case. Cost: transient notifications.
- R-10: a `QPointer` member that goes null because its target died emits nothing. This covers a plottable's axes,
  an error bar's data plottable and a tracer's graph. The target's `destroyed()` is the notification. Cost: a
  binding on those reads null only at its next evaluation. It is in `migrating.md`.
- R-11: signals sent from inside a destructor, and during plot teardown, are delivered at once. A scope cannot help
  there: it would close before the destructor returns. So a slot that deletes the plot from there is unprotected,
  as in plain Qt. `~QCPPolarArea` keeps no scope for this reason. Cost: such a slot crashes. It is in
  `migrating.md`.
- R-12: register calls made from constructors hold no scope. A plot deleted while one of its children is being
  built is outside what a scope can protect. Cost: a pathological slot crashes.
- R-13 (E-4b): `take()` clearing a plottable's system is a writer. It signals `systemChanged(nullptr)`,
  `keyAxisChanged(nullptr)` and `valueAxisChanged(nullptr)`, inside the detach scope. Cost: none found.
- R-14 (review I10): a binding that keeps a `QBindable` of its source crashes once the source is deleted. The
  adaptor dies with its object, and the captured `QBindable` dangles. This is Qt's design, not ours, so it is
  documented, not prevented. The spike test shows the safe form: hold the source by `QPointer` and build a fresh
  `QBindable` inside the binding. Cost: a user who misses the page crashes.
- R-15 (final review M7): no `bindableX()` accessors. E-0 makes `QBindable<T>(obj, "name")` the API, and no
  binding use needs more. Cost: an additive accessor later.
- R-16 (E-13): E-13's premise is false on Qt 6.12. `QMetaType::name()` comes from the compiler's type name, not from
  `Q_DECLARE_METATYPE`, so deleting an alias line renames nothing. The `QCPCurve::LineStyle` line stays: it is inert
  and harmless. A test pins every registered name. Cost: one dead line.
- R-17 (bench): `QCPAxis_SetTickLength10kItems` +3-4 % is accepted, inside the 5 % gate. It is the scope that
  `setTickLength(in, out)` opens around its two setters. The polar two-length setters open the same scope; they are
  not on a hot path. Cost: gate the scope on a receiver later.
- R-18 (bench): the scopes of `setTheme` and of the legend are not benched. They cost one `QPointer` and one flush
  per call, not per point, and no bench row reaches them. The removal and selection scopes of the final fix batch
  are in the same case. Cost: negligible.
- R-19 (bench): `QCPItems_Add10k` is judged on its layout-averaged number. Over six mold section shuffles it is
  +4.8 %. An external harness on the exact row body says +1.4 %. Every bisect point is within ±3 %. The three emits
  per item cost under 1 %. At the default layout the row reads +8-10 %, which is code placement. The one code fix
  tried moved the cost to `QCPItems_RestyleAll10k` (+31 %). `baseline.md` marks both rows as layout-sensitive.
  Cost: 1-5 % on item creation.
- R-20 (bench): `QCPPlots_Stack8_Replot` +6.0 % on the mean and +1.3 % on the median, after the final fix batch, is
  noise. Cost: a few % on a stacked replot.
- R-21: the `followKeyAxisRectScale` fix is defensive. Every caller holds a scope, so no public path reaches the
  write after a delete. It has a guard row, which passed before and after. Cost: none.
- R-22 (process): two review findings were declined. Task 3's M2 (a double selection-rect signal) is a false
  positive: the sequence needs a use-after-free in user code first. Task 3's M7 (the polar two-length scopes) is
  R-17.
- R-23 (bench): `QCPDrawContext_ReplayMarkers100k` +10.2 % is code layout. Relinked with six mold section shuffles,
  each binary runs in one of two modes, 36.6 or 40.8 M ticks, for base and new alike. The layout picks the mode;
  the new default link picks the slow one. Averaged over the six layouts, new is +0.2 %. The row joins the
  layout-sensitive rows in `baseline.md`. Cost: none.

## Found and fixed

- Pre-flight review (fresh Opus reviewer, B1-B2, I1-I10, M1-M11): the binding storage could run a handler on a
  freed object, which led to E-0. Dropping the theme forwarders would break stylesheet theming. Writers other than
  the setters were missed. Constants that are not fixed for life. All folded into the plan before any code.
- The spike showed I10: a captured `QBindable` of a deleted source crashes (R-14).
- Task 2, each with a failing test first, most under ASan:
  - `setTheme` emitted with no scope, then kept using the plot: use-after-free.
  - `setViewport`, `QCPBarsGroup::insert` and `QCPLegend::addItem` emitted, then used `this`.
  - `adopt(item)`, `add(unique_ptr)` and `setSystem` did the same.
  - `setIconSize(int, int)` moved the icon size with no signal.
- Task 3:
  - `QCPPlottableGroup::setVisible` had no scope: use-after-free.
  - A group's `visible` did not move when a member was taken, removed or deleted.
  - A span's edge writes did not signal its ranges. A hook on the item position fixes it at the root.
  - Pre-existing: the polar axes reported one number format and drew another.
  - Pre-existing since 2.x: the antialiasing setters used `|= ~` where `&= ~` was meant. Forcing one element on put
    every other element into the never-antialiased set.
  - `QCPItems_Add10k` +8.7 %: every item paid for the position hook. Now only the spans pay.
  - ASan found a dangling nested `initializer_list` in the test's form table.
- Task 4: the metatype-name test skipped invalid names and covered 11 of 32 lines. It now checks all 37. `saveRastered`
  lost its `Q_INVOKABLE`: it takes a `const char*`.
- Final review (fresh Opus reviewer, one fix batch):
  - Removing a plottable relaid the legend out with no scope: use-after-free, then a double delete.
  - `dropSystem` wrote to the plottable after `setSystem` had delivered: use-after-free.
  - `QCPLegend::selectedParts` moved with no signal when an item was selected.
  - A deferred pointer argument could reach a slot after its object died. The queue now holds `QObject*`
    arguments in a `QPointer`.
  - `overlay()` kept working after it delivered the layer's `modeChanged`.
  - Pre-existing: `followKeyAxisRectScale` wrote to the plottable after a slot deleted it (R-21).
  - The migration page still stated E-13's false premise.
- Before the push, pre-existing: `QCPGraph::setComponentErrorBars` and `clearComponentErrorBars` sent
  `componentVisibilityChanged` with no scope, then kept using the graph. A slot that deleted the graph read freed
  memory (ASan, two new deleting-writer rows). Each holds one scope now. The other component setters send their
  signal last. A new source that drops error bars re-shows their bounds too: `installSource`, `dataChanged` and
  `setSystem` hold a scope already, and the owned-source edits that skip one cannot change the column count.

## Deferred (also in backlog.md)

- `BINDABLE` storage (R-1).
- A legend add or remove sends a burst of stretch-factor signals: one per intermediate grid shape, the last one
  final (final review M2).
- `QCPItems_Add10k`, `QCPItems_RestyleAll10k` and `QCPDrawContext_ReplayMarkers100k` move with code layout
  (R-19, R-23).
- A setter's scope costs a `QPointer` and a flush even when nothing listens (R-17, R-18).
- No bench row adds or removes plottables, or touches the legend or the theme.
- `QCPHitAction` is a global `enum class`, outside any meta-object scope. It stays unregistered: moving it into
  `QCP` would break `hitsAt` callers.
