# NeoQCP 3.0 — milestone 5: delete the old API

The authorities are the object-model spec (`2026-11-02-neoqcp-3-object-model-design.md`), the drawing-API spec
(`2026-11-04-...`), the milestone-4 design (`2026-11-06-...`) and the coordinate-systems design (`2026-11-09-...`).
This document adds what they leave open for milestone 5: the maintainer's answers, the controller's calls, and the
order of work. The facts are in `docs/plans/neoqcp-3/m5-inventory.md`.

## Intent

Milestone 4 moved every class onto the new model and kept the 2.x API alive beside it. Milestone 5 removes the second
way, so 3.0 never ships two ways to do one thing (object-model spec, :77).

Goals: one graph class; no class that draws only through the old painter path except the internal ones milestone 4
answer 7 keeps; getters that hand out plot objects return handles; examples, docs and tests on the new API, and the
examples built.

Non-goals: the data model and threading (project 3, including merging `QCPColorMap` and `QCPColorMap2`), the rest of
the API-surface cleanup (project 4), Qt Canvas, new features beyond what a deletion needs to keep a use case alive.

Anti-goals: a release point with a class half deleted or the suite red; a use case 2.x users had that 3.0 silently
loses (each loss is listed in the migration page with its replacement); losing what 4j built (graphs on user
systems).

## Maintainer answers (2026-11-11)

- **M5-1. The new `QCPGraph` keeps `addData`.** `addData(keys, values)` and `addData(key, value)` append into the
  graph's own data source. `data()` returning the shared 2.x `QCPGraphDataContainer` goes.
- **M5-2. Channel fill becomes a fill to another graph.** A component fill whose other edge is a component of another
  graph on the same coordinate system (the 4j one-system rule). `setChannelFillGraph` goes.
- **M5-3. Getters return handles in milestone 5**, as the object-model spec says, in their own sub-milestone after
  the deletions, so SciQLopPlots ports once.
- **M5-4. Use Qt properties and the useful Qt macros, bindable properties where it makes sense** (2026-11-11: "also
  try to use Qt properties and macros that are useful"; "I had in mind bindable properties and similar stuff where it
  makes sense"). Today: 274 `Q_PROPERTY`, 36 of them with `NOTIFY`, none `BINDABLE`; 27 of 60 `Q_OBJECT` classes
  declare none, the new 3.0 classes among them; no `Q_INVOKABLE`; `Q_DISABLE_COPY` but no `Q_DISABLE_COPY_MOVE`.
  See C11.

## Calls (the controller's; the maintainer can overrule any)

- **C1. The graph.** `QCPMultiGraph` is renamed `QCPGraph` (files, ops, users); `QCPWaterfallGraph` follows. The old
  `QCPGraph` and `QCPGraph2` are deleted. `QCPGraphComponent` stays as the name of the component type, not an alias.
- **C2. The graph keeps D9.** Before the rename, the multigraph gains the straight-segment path 4j gave the old
  graph: it requires `capStraightSegments` only, and on a non-cartesian system it reads the key window, fill and
  impulse baselines and error-bar cap widths through the system instead of the 2.x axes. `capLanes` leaves its mask
  (no lane code reads it). Colour by scalar follows the rect's colour scale only on an axis rect; elsewhere it needs
  an explicit scale (the curve's documented limit). Error bars of a graph on a user system: the per-component error
  bars draw through the transform; if that proves rectangle-bound, they are a documented limit, decided in the plan.
- **C3. The graph list API stays, typed on the new class.** `addGraph`, `graph(i)`, `graphCount`, `removeGraph`,
  `clearGraphs`, `selectedGraphs`, `QCPAxis::graphs()` and `QCPAxisRect::graphs()` list `QCPGraph` (the renamed
  class). 2.x code ports by recompiling where it only uses lines, scatters, styles and `addData`.
- **C4. The tracer follows any 1D line plottable.** `QCPItemTracer::setGraph` takes the new `QCPGraph` (one component,
  chosen by index, default 0) and reads data through the plottable's 1D interface, not a container. A tracer on a
  curve or a polar graph stays in the backlog.
- **C5. `QCPGraphData` stays** (`QCPPolarGraph` and SciQLopPlots' resampler use it).
- **C6. The painter path.** Milestone 4 answer 7 stands: `QCPLayerable::draw(QCPPainter*)` stays an internal virtual
  for axes, grids, legend, layout elements, the selection rect, the overlay and the top level. Every plottable and
  item records; the four recorded plottables that still paint their legend icon (`QCPFinancial`,
  `QCPStatisticalBox`, `QCPErrorBars`, `QCPIntervals`) record it. The 1D nearest-point fallback for a painting
  plottable is deleted with the last painting plottable; `QCPAbstractPlottable::draw(QCPPainter*)` becomes the final
  replay, as the item one already is.
- **C7. 2.x leftovers that go.** `QCPMarginGroup` (alignment groups replace it, 4d); the protected `mKeyAxis`/
  `mValueAxis` projections (the public `keyAxis()`/`valueAxis()` projections stay); the span items' pen that is never
  drawn (the setter goes); the data sources' pixel line API (`getLinesAll` and friends, 4b R); `QCP::phImmediateRefresh`
  stays (it is a hint, not a second way).
- **C8. Handles (M5-3).** Getters that return a plot-owned object return its handle (`QCPHandle<T>` or the class's ops
  handle). Signal arguments stay raw object pointers (they are delivered while the object lives). The public
  `xAxis`/`yAxis`/`xAxis2`/`yAxis2`/`legend` members become handle-returning accessors. The exact list is the 5d plan's
  first task.
- **C9. Misuse.** A `qDebug` in a path milestone 5 touches becomes a `QCP_CHECK`/`reportMisuse` where it reports a
  caller error; the untouched ones stay (project 4).
- **C11. The meta-object surface (M5-4).** After the deletions and the handles:
  - Every user-facing setter/getter pair of a `Q_OBJECT` or `Q_GADGET` class is a `Q_PROPERTY` (`READ`/`WRITE`,
    `NOTIFY` where the class has a change signal, `CONSTANT`/`FINAL` where they hold), the 3.0 classes included
    (areas, systems, axis bases, item positions' system and area, capabilities). Properties declared for removed or
    renamed accessors are deleted.
  - **Every property gets a `NOTIFY`.** Qt can bind any property that has one from outside, through
    `QBindable(object, "name")` (Qt 6.12 "Qt Bindable Properties", "Interaction with Q_PROPERTY"), so this alone
    makes the whole surface bindable one way at no cost to the setters. The change signals keep the deferred
    delivery of 4a.
  - **`BINDABLE` storage where it makes sense:** a property a user would reasonably bind in both directions and whose
    setter is not a hot path. First candidates: pens, brushes, colours, visibility, names and labels, tick and label
    settings, margins and spacing, selectable/interaction flags, the theme's values, an axis' scale type, reversal
    and label text, a system's own parameters (the pattern is documented for user systems). Qt skips the setter when
    a binding updates the value, so a setter's side effects (`markDirty`, `contentChanged`, revision bumps, the
    deferred change signal) move into the property's change handler (`Q_OBJECT_BINDABLE_PROPERTY` with a handler
    member); a property whose setter validates or normalises its value uses `Q_OBJECT_COMPAT_PROPERTY`, which runs
    the setter on each update. The 5e plan starts with a spike on three properties (a pen, an axis label, a system
    parameter) that proves both forms keep the recording, the repaint and the deferred signal right.
  - Bindings are single-thread (Qt's rule): only GUI-thread properties are bindable; nothing the async pipeline
    touches.
  - **Hot paths are bindable only if the bench allows:** an axis range (it would let users bind ranges, next to the 4d
    linked axes), a system revision, data. Each is A/B'd against the 5 % drawing gate with no binding installed; if
    the unbound setter moves, it stays a plain property and the plan says so. Data is never a property.
  - `QBindable<T> bindableX()` accessors next to the getters, named as Qt names them.
  - Every public enum is `Q_ENUM`/`Q_ENUM_NS`, every flag type `Q_FLAG`/`Q_FLAG_NS` (`QCP::Capabilities` included).
    Actions callers trigger by name (`rescaleAxes`, `replot` where not already a slot) are `Q_INVOKABLE`. QObject
    classes use `Q_DISABLE_COPY_MOVE`; value types carry `Q_DECLARE_TYPEINFO` (primitive or relocatable) and
    `Q_DECLARE_METATYPE` only where Qt 6 still needs it.
  - A generic test walks every class's `metaObject()` and checks each property: it reads, it writes back what it
    read, a write that changes the value fires its `NOTIFY` once (after delivery), and a bindable one follows a
    binding to a source property and stops following when a plain write breaks it.
- **C10. Examples.** A meson option builds `examples/` (off by default), and CI-equivalent local checks build it once
  per sub-milestone.

## Order of work (each sub-milestone ends green and measured)

- **5a. The graph grows up** (old classes still present): the multigraph gains the straight-segment path (C2),
  `addData` (M5-1), fill to another graph (M5-2), and the tracer reads through the 1D interface (C4). Tests mirror the
  4j matrix on the multigraph.
- **5b. One graph:** delete `QCPGraph` and `QCPGraph2`, rename `QCPMultiGraph`, prune the data-source pixel API, port
  every user in `src/`, tests, benchmarks (graph rows renamed onto the new class), reference pictures (each moved
  picture looked at), snippets, docs, examples, the doc image generator; add the examples build option (C10).
- **5c. The rest of the old API:** recorded legend icons and the final painter replay (C6), `QCPMarginGroup`, the
  protected projections, the span pen (C7), misuse in touched paths (C9).
- **5d. Handles** (M5-3, C8).
- **5e. The meta-object surface** (M5-4, C11).
- **5f. Release triage:** every backlog item fixed, scheduled to project 3/4, or documented as a 3.0 limit; the
  SciQLopPlots migration file complete.

## Performance

The graph rows move from the old class to the renamed one in 5b; the gate for the renamed rows is the matching
`QCPMultiGraph` rows of the end of 4j (5 % for drawing rows, 15 % otherwise). 5a must not move the cartesian
multigraph rows.

## Amendments after review (binding; they override the calls and the order above)

Review: `.superpowers/m5-design-review.md` (opencode). One line per finding.

- **A1 (R1) The graph on a non-cartesian system** takes its own branch: no resampling (the displayed source,
  `pixelsPerKey = 0`, no L1/L2), the key window from the system (`QCPDrawContext::keyWindow()` asks the system when it
  has one; an empty window means unbounded, never [0, 0]), and the gates, `selectTest`, `selectTestRect`,
  `canProduceContent`, `hitTest` and `dataPixelPosition` go through the system and its transform. The cartesian
  branch is unchanged.
- **A2 (R2) `addData`.** It appends only to a source the graph owns: the one `setData`/`addData` built. On a view, a
  shared source, a waterfall adapter or a user source it is a misuse and nothing changes. With a deferred swap
  pending it appends to the staged source. Signatures: `addData(keys, values)` and `addData(key, value)` for a
  one-column graph; `addData(keys, columns)` and `addData(key, values)` for N columns; a column-count mismatch is a
  misuse. The owned source grows in place (amortised append); the async pipeline sees a new revision.
- **A3 (R3) Fill to another graph.** The target is (graph, component). The band is built like 4j's data-space
  channel fill: both series cropped to their common key range, and the partner interpolated onto this graph's keys,
  so the two series have equal length (the GPU ribbon stays on). Both graphs must share the system (else misuse). The
  partner is held by `QPointer` and the fill drops when the partner or its component goes. This graph's selection
  splits the band, as 2.x did.
- **A4 (R4)** `QCPGraphData` and `QCPGraphDataContainer` move to their own public header (`graph-data.h`) in 5a;
  the polar graph, the tracer and the old graph include it.
- **A5 (I1, I2, I3) The graph list and 2.x spellings.** The renamed class registers in the graph list from its
  constructor, as the old class did. It has one-column `setData(keys, values)` overloads (class and ops, `QVector`
  and `std::vector`). After the handle sub-milestone, `graph(i)`, `selectedGraphs()` and the axis/rect `graphs()`
  return handles. `addGraph(key, value)` stays as the 2.x spelling of `add<QCPGraph>(key, value)` and returns the
  same handle: a documented exception to "one way" for the most used 2.x call.
- **A6 (I4) Handles and properties.** The handle sub-milestone's first task is the exception list: non-`QObject`
  types (`QCPSelectionDecorator`, positions and anchors as `Q_GADGET`s, `QCPPlottableInterface1D`, paint buffers)
  and the `Q_PROPERTY` policy for object-typed properties (a handle is not a metatype). Neither is decided here.
- **A7 (I5) New order: handles before the big port.** The user port (examples, snippets, docs, doc generator) is done
  once, against the final API.
  - **5a** the graph grows up (A1-A5 except the list types, the tracer A8, the per-feature rule A9, a new-class
    `user-system-skew` reference picture, and new-class bench twins of every graph workload, measured at the end of
    5a).
  - **5b** handles (M5-3, C8, A6), with their users.
  - **5c** one graph: delete, rename, port every user, the data-source pixel API with the waterfall adapter's
    overrides (4b R-17), and the examples built with the existing `with_examples` option.
  - **5d** the rest of the old API (C6, C7, C9).
  - **5e** the meta-object surface (M5-4, C11).
  - **5f** release triage.
- **A8 (I7, I11, M2) The tracer.** In 5a it gains `setGraph(QCPMultiGraph*, int component = 0)` and
  `setComponent(int)`, and reads keys through the 1D interface and values through `componentValueAt`. The old
  `setGraph(QCPGraph*)` lives until 5c (5a is not a release point). Its scope is the new graph. A tracer on a curve or
  a polar graph is a scope choice left to the backlog.
- **A9 (I8) Features on a non-cartesian system:**
  - fills: data-space baselines (value 0, or the log bound nearer zero);
  - error bars: their caps measured through the transform;
  - colour by scalar: joins the scale of `system()->area()` when that area is an axis rect (so a user system on a
    rect subclass joins it); otherwise it needs an explicit scale.
  - A feature 5a cannot make right on such a frame is refused with a misuse when it is set or when the graph joins
    the system, never drawn wrong.
- **A10 (I9)** The 5c/5d plans carry the complete lists: the auto-margin branch with `QCPMarginGroup` and its users
  (docs pages, doc generator, the SciQLopPlots migration note); the item position's `mKeyAxis`, `mValueAxis` and
  `mAxisRect` projections go together; the span items' pen, selected pen, accessors and constructor calls; the
  single-column pixel API with the multi one; the C9 touched-path list.
- **A11 (I10)** `drawLegendIcon(QCPPainter*, QRectF)` stays as the final, non-virtual entry the legend rows and the
  group row call (it replays the recorded icon); the classes and tests that override it move to the recording form.
  `QCPIntervals::LabelLayer` joins answer 7's internal painter list.
- **A12 (I6)** Graph bench rows: each gets a disposition in the 5c plan. The container rows (`RemoveData*`) retire
  with `data()`. The `QCPGraph2_*`, `QCPInput_WheelOverGraph2_1M` and `QCPGpu_Graph2Render1M` rows move to the new
  graph's pipeline. Every other row is gated against its 5a new-class twin.
- **A13 (M1, M3, M4, M5)**
  - `QCPMultiGraph::Component` is renamed `QCPGraphComponent` (a real struct, no alias). The pipeline type is named
    in the 5c plan (`QCPGraphPipeline` already names the old graph's pipeline; it goes with it).
  - C10 uses the existing `with_examples` option.
  - C5's reason is the polar graph only.
  - Release triage writes one disposition line per backlog item: fixed (commit), project 3 or project 4 (named), or
    3.0 limit (doc page).
- **A14 (bindable review, `.superpowers/m5-bindable-review.md`) The meta-object surface, made precise.** C11 is
  replaced by this.
  - **Each property gets one of four forms, listed per property in the 5e plan** (built from moc output, not grep
    counts):
    - *constant*: `CONSTANT`, no `NOTIFY`;
    - *plain*: a setter plus `NOTIFY`;
    - *bindable storage*: `BINDABLE`;
    - *derived*: read-only with `NOTIFY` on a real event, or dropped from the surface (for example `tickVector`).
  - **Plain `NOTIFY` is the default form.** It is right for every setter that validates, refuses, normalises, is
    virtual or updates other state (scale type, range, pen, selectable, selection). Such a property is bindable from
    C++ through `QBindable(object, "name")`, and gets a `bindableX()` accessor returning that wrapper where a binding
    is a real use.
  - **`BINDABLE` storage only for trivial setters**: store, mark dirty, and emit, nothing else (an axis label, a
    text, a colour that nothing else copies). It uses `Q_OBJECT_BINDABLE_PROPERTY` with a handler that does the
    marking and the deferred `qcpEmit`.
  - `Q_OBJECT_COMPAT_PROPERTY` is not used: it is private Qt API.
  - **Setters of storage-backed properties always call `setValue`.** This removes a binding even on an equal value,
    as Qt requires. The equality early return moves after it.
  - **Value types (`Q_GADGET`) get `Q_PROPERTY` without `NOTIFY`**, and are not bindable.
  - **Bindings are a C++ feature.** Python and PySide keep the `NOTIFY` semantics, delivered after the call as 4a
    made them.
  - **Only GUI-thread state is a property.** A debug check of thread affinity goes in the storage handlers. Pipeline
    state such as `busy` stays out of the surface.
  - **5e opens with a spike on three properties**, each with the deferred signal, a `QBindable` subscriber, a binding
    whose observer deletes the object, and a slot that deletes the plot:
    - an axis label (storage);
    - a plottable pen (plain, `QBindable` wrapper);
    - an axis scale type (plain, can refuse).
  - **The read side is benched.** Any getter on the paint or layout path that becomes storage-backed (visibility,
    pens, label fonts, margins) is A/B'd against the 5 % gate before the form is chosen. If it moves, it stays plain.
  - **The macros get per-type lists, not blanket rules:**
    - `Q_ENUM` for the public enums missing it (`QCPIntervals::Style`, `SnapMode`, `StackMode`, `EditMode`,
      `HitPart`, `DateStrategy`; `QCPTransform::Kind` needs a gadget or stays out);
    - `Q_INVOKABLE` for named actions that are not slots, listed with their signatures (no `const` and no overloaded
      ones without a check);
    - `Q_DISABLE_COPY_MOVE` on `QObject` classes only, never on value types;
    - `Q_DECLARE_TYPEINFO` only after auditing that each type is relocatable;
    - `Q_DECLARE_METATYPE` where a property, `QVariant` or PySide needs it.
  - **5e follows 5b's handle policy (A6).** Object-typed properties follow whatever A6 decides.
  - **The generic test is split by form.** `QObject` instances come from a curated list of constructible fixtures;
    gadgets go through `readOnGadget`/`writeOnGadget`. Constant and derived properties are checked as such.
    Follow-and-break binding checks are made only on storage-backed properties.
