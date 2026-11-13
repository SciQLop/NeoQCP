# NeoQCP 3.0 — milestone 5b rulings and deferred items

Milestone 5b: getters that hand out plot-owned objects return handles (maintainer answer M5-3). Branch `neoqcp-3.0`,
`e7c3043..` the commit that adds this file (local, not pushed). Design: `docs/specs/2026-11-11-neoqcp-3-milestone-5-design.md`
(M5-3, and the 2026-11-12 answers M5-5 "a handle converts implicitly to `T*`" and M5-6 "the default axes and the legend
are handle-typed members"). Plan: `docs/superpowers/plans/2026-11-12-neoqcp-3-milestone-5b.md` (calls H-1..H-10,
amendments K1-K12 after an opencode plan review). Inventory: `m5b-inventory.md`. The maintainer said "go ahead on 5b"
(2026-11-12); everything below beyond M5-5/M5-6 was decided by the controller.

## Results

- Offscreen suite 2505 passed, 0 failed, 136 skipped; Xvfb whole binary 2641/0/0; whole ASan suite clean after the
  last code change; clang clean on every changed `.cpp`; the guide snippets and the examples build.
- Bench: `baseline.md`, "End of milestone 5b" (measured on the Coder workspace). Four slowdowns found there were fixed in 6ba867b.

## What 5b built

- `QCPHandle<T>` converts to `T*` (null when dead), to a handle of a base class, assigns from `T*`, `nullptr` and a
  derived handle, hashes and prints. `qcp::pointers()` turns a handle list into a pointer list. `qcpConnect` is gone.
- Every getter that hands out a plot-owned `QObject` returns `QCPHandle<T>`; lists return lists of handles; the
  creators `addAxis`, `addAxes`, `addRadialAxis` and `addPlottableGroup` return handles; the default axes and the
  legend are handle members.
- Object-typed `Q_PROPERTY`s keep their pointer types; reads and writes through `QObject::property`/`setProperty`
  are tested for every one.

## Rulings (what, why, cost if wrong)

- R-1 (K1): getters return plain `QCPHandle<T>`, not the fluent ops handles: `QCPHandle` stores only a `QPointer`,
  so headers can forward-declare `T`; the ops handles need the complete class and would create include cycles. Cost:
  fluent setters only on what `add<T>()` returns.
- R-2 (K3): the graph family (`graph(i)`, `addGraph`, `selectedGraphs`, `graphs()`, `channelFillGraph`, the tracer's
  targets) stays raw until 5c renames the graph classes, so each user site ports once. Cost: one more port in 5c.
- R-3 (H-3, K4): raw exceptions — the `Q_GADGET`s (positions, anchors, the selection decorator), `interface1D()`,
  `QCPColorMapData`, `labelRenderer()`, the RHI layers and paint buffers, the virtual getters whose overrides return
  a derived pointer (`linkSource`, `moveTarget`, the axis `area()`), `takeAt()` (it hands ownership out), and the margin-group getters (`marginGroup`, `marginGroups`, `QCPMarginGroup::elements`), which 5d deletes with the class. Cost:
  a raw pointer from those can dangle, as in 2.x.
- R-4 (K5): list setters keep `QList<T*>`; `qcp::pointers()` converts. Cost: `QList<T*> x = getter()` stops
  compiling (the migration page has the fix).
- R-5 (K6): `QCPLayer::children`'s property reads through a private raw accessor (moc cannot convert a list).
- R-6 (K8): no public handle getter on a per-point, per-item-per-frame or per-re-record path. The library reads
  members or private raw accessors there (friendships where needed). The final review found three paths the tasks
  had missed (coordinate systems, axis-rect dirty marking, layout passes); fixed.
- R-7 (Task 1): `qHash`/`std::hash` hash the held pointer; a handle whose object dies changes hash, so a set of
  handles must not outlive its objects (as with `QPointer` keys). Cost: a stale entry in a long-lived set.
- R-8 (Task 3): `QCPArea` gains a protected virtual `allAxesRaw()` (its default converts `allAxes()`), for the
  per-position system lookup. Cost: one more virtual on `QCPArea`.
- R-9 (final): the default axis and legend members are cleared when the axis or legend is removed, as 2.x did, not
  only when the object dies. Types change in 5b; behaviour does not.
- R-10 (final): `QCPAxisBase::rescale` walks the virtual `plottables()` again (a user axis that overrides it is
  honoured), and one `plottablesRaw()` with one order replaces the two that differed.

## Found and fixed in review

- Plan review (opencode): ops handles in headers with incomplete types; members not assignable from raw pointers;
  the graph family would be ported twice; covariant getters; list conversions; `QPointer` copy-initialisation;
  the `children` property; hot paths without raw accessors. Folded in as K1-K12.
- Task 2: item creation built two handles per position (+5.6 % on `QCPItems_Add10k`); now raw.
- Task 3: the per-position system lookup built handles (+7.3 % on `QCPItems_Add10k`); raw area/system/axes views.
- Task 4: the per-object view builders built up to seven handles per re-record (`QCPItems_RestyleAll10k` up to
  +46 %); raw accessors; level in both run orders.
- Final review (fresh Opus reviewer and fresh opencode reviewer, one batch, a failing test first for each):
  assigning a derived handle (and `h = 0`, `h = {}`, `= nullptr`) was ambiguous; the coordinate systems, the
  axis-rect dirty marking and the layout passes still built handles; the default members were cleared later than in
  2.x; `rescale` bypassed a user axis' `plottables()`; detaching a system's plottables crashed when a hook deleted a
  sibling (null through a handle; a use-after-free before 5b); `labelLayer()` was a raw getter nobody listed; the
  property writes were untested; a misplaced doc block.

## Deferred (also in backlog.md)

- `Q_FUNC_INFO` strings of the changed getters now show the handle return type.
- The graph family's handles (5c).
