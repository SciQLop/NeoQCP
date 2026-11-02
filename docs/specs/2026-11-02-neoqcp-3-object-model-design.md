# NeoQCP 3.0 — the object model

Design for the first of the NeoQCP 3.0 projects. Written 2026-11-02 from a conversation with the
maintainer and four read-only surveys of the current tree (see **Evidence** at the end).

## Why

NeoQCP's ownership model is not consistent across its API, and its object and drawing APIs grew from
QCustomPlot 2.1 one feature at a time. Users pay for it:

- Some objects are safe to `delete`, others leave the plot holding dead pointers. Plottables and items
  register themselves in their constructor; other objects must be added. Almost every `remove…` deletes,
  a few only detach. Nothing in the types says which is which.
- A slot may delete objects while NeoQCP code that emitted the signal is still running. Clients defend
  themselves: SciQLopPlots, NeoQCP's main client, has 62 "apply if still alive" calls, 85 "am I still
  alive?" checks after NeoQCP calls that emit, and disconnects everything before a plot dies.
- Simple tasks need many steps. A colour map with an aligned colour scale is five objects wired by hand.
  Extra axes are found again by index. Item positions take a type and coordinates set separately, in an
  order that matters.
- Writing a new plottable or item means learning internals: the coordinate conversions, layers, clip
  rects, hit testing and legend icons, with contracts that differ between classes and backends.

The maintainer's words for the goal: easy to use, hard to misuse, user friendly and safe; modern C++;
close to Qt's philosophy; and "more open and extensible with real helpers like matplotlib", so it is easy
to write any new graph or item and draw whatever you want, without "complicated Qt stuff where the
coordinate system is hard".

## Scope

NeoQCP 3.0 is split into projects, done in this order:

1. **The object model (this document):** ownership and lifetime, signal discipline, the base classes, and
   the drawing and extension API.
2. *(folded into 1)* signal discipline.
3. Data ownership and threading: borrowed buffers, implicitly shared data, what worker threads may hold,
   when new data becomes visible.
4. The API surface: naming, enums, properties, reading like a Qt class.
5. Speed, measured on the new structure.

This document covers project 1 only. It names what belongs to the later projects so it is not lost.

**NeoQCP only.** SciQLopPlots is out of scope for now. It will be ported to 3.0 later, as a separate
step. Its code is used here only as evidence of what NeoQCP makes users write.

## Goals

1. **One ownership rule everywhere.** The plot owns everything it draws, through the Qt parent tree.
   Ordinary user code never manages a lifetime.
2. **Nothing dangles.** Any object a user can reach may be deleted, by `delete`, `deleteLater()` or
   `remove()`, and every holder notices. No raw pointer to an object outlives it.
3. **Signals never pull the ground out.** NeoQCP emits a signal only after it is done with the object, so a
   slot may delete anything without crashing the code that emitted. Destroying a plot emits nothing to
   user code except `destroyed()`.
4. **Obvious code, no boilerplate.** One call creates and configures an object. No `new`, no `replot()`, no
   index lookups, no axes passed when the defaults are meant.
5. **Hard to misuse.** Mistakes are caught by types first, then by a debug check. A release build never
   turns a misuse into undefined behaviour.
6. **Extensible like matplotlib.** A new plottable or item is a small class that draws with public
   helpers in typed coordinates. The built-in plottables and items use the same helpers.
7. **Close to Qt.** Parent/child ownership, signals and slots, `QPointer`-style safe references, value
   types for values. Where Qt's own choices make misuse easy, ease of use and safety win.

## Non-goals

- Rendering and GPU techniques stay as they are, except where the stacking rule (decision 9) needs them
  to change.
- Data containers, data sources, borrowed buffers and threading (project 3).
- Naming and enum clean-up beyond what the new classes need (project 4).
- Performance work beyond not regressing (project 5).
- Porting SciQLopPlots.

## Anti-goals

These would make the result a failure even if it worked:

- A slower library, in particular for large data or many items.
- Two ways to do the same thing (an old and a new API side by side). 3.0 breaks the API; users who want
  2.x keep 2.x, which stays maintained.
- An API that needs exceptions crossing the Qt event loop.
- Extension points that only the library's own classes can use.

## Constraints

- C++20, Qt 6 (QRhiWidget-based rendering), the current build systems (Meson; the CMake build), the
  WebAssembly build and its gallery. Linux, Windows and macOS CI as today.
- API breaks are allowed when they make the API friendlier and safer. 2.x stays maintained in parallel.
- NeoQCP throws no exceptions (Qt's event loop is not exception-safe, and slots may run any code).

## Decisions

Each decision says who made it: **maintainer** (chosen in the conversation), or **agreed** (proposed and
accepted in a playback). The builder's freedom is in **Left to the builder** below.

### 1. Ownership: the plot owns everything it draws *(maintainer)*

- Every object that draws or belongs to a plot is a descendant of the plot in the Qt parent tree. The
  parent tree is the one source of truth for ownership.
- Registries the plot keeps (its plottables, items, layers, axes per rect, legend rows…) never hold a raw
  pointer that can outlive its target. They are updated when an object is destroyed by any means: either
  derived from the parent tree, or held through `QPointer` and cleaned on `destroyed()`.
- Everything a user can reach is safe to `delete` directly; the owners notice.
- Removal follows `QGraphicsScene`, where `removeItem` hands ownership back to the caller and `delete` is
  always safe. This was checked against Qt (`QGraphicsScene::removeItem`, `QTabWidget::removeTab`,
  `QLayout::removeWidget` detach; Qt's item models are the exception, where `removeRow` deletes and
  `takeRow` hands back; "take" means "it's yours now" everywhere in Qt).
- With handles (decision 4), ordinary code says `handle.remove()`, which means gone. The low-level API
  keeps a hand-back, `std::unique_ptr<T> take(Handle<T>)`, for moving objects between plots or keeping
  them. It takes a handle, not a raw pointer, so it is checked like every other use of a handle.
- Adding an object made by the user (a subclass instance, a taken object) passes a `std::unique_ptr<T>`,
  so the transfer of ownership is visible to the compiler.

### 2. What is a `QObject` and what is a value *(agreed)*

Qt's rule: a `QObject` has an identity, is observed through signals, and lives in a parent tree. Data and
lightweight things are values (`QPen`, `QFont`, `QColor`); numerous lightweight objects are not
`QObject`s (`QGraphicsItem`, `QStandardItem`, layout items).

- **`QObject`s:** the plot, axis rects, axes, layers, plottables, items, the legend, the colour scale, text
  elements, the theme, groups (plottable groups, bars groups, alignment groups), the async pipelines and
  the scheduler.
- **Owned by the plot, made by a factory** (they are user-owned and parentless today): bars groups,
  alignment groups (decision 13), the lane layout.
- **Internal, not public classes:** the item-creation state machine (`QCPItemCreationState` today), the
  colour scale's private axis rect.
- **Parts their owner cannot lose:** an axis's grid (a separately deletable `QObject` today, with a double
  delete when it is deleted). Users configure it through its axis; they cannot delete it. Item positions
  and anchors stay plain members of their item.
- **Legend rows** are made by the legend from the plottables; users never own them.
- **Values, as today:** ranges, scatter styles, line endings, gradients, data selections, tickers (shared).
- **Items stay `QObject`s and must scale** *(maintainer: "we don't usually put thousands, but it should
  scale")*. The plot keeps items in a structure with constant-time add and remove, and drawing and hit
  testing skip items outside the view. Massive homogeneous sets belong in one collection plottable (as
  `QCPIntervals` does for intervals), not in thousands of items.

### 3. Creation: `plot.add<T>(…)` with a fluent builder *(maintainer)*

- Objects are created by the owner: `auto g = plot.add<QCPGraph>(x, y).name("Bx").pen(p);`. Users do
  not write `new` for anything NeoQCP owns.
- `add<T>` works for derived classes: `plot.add<MyGraph>(…)`. A subclass's builder extends its base's, so
  a user graph is configured exactly like a built-in one.
- The builder holds everything that can be set at creation; the data comes as `add`'s arguments. One
  chained call creates and fully configures an object, and converts to a `Handle<T>` (decision 4).
- Builder setters check their values (tier 2 of decision 6), and adding an option never breaks an
  existing call. This was chosen over C++20 designated-initializer options structs, which must follow
  declaration order and cannot validate.
- Defaults carry intent: a plottable lands on the default axis rect's x and y axes unless `.axes(…)` says
  otherwise.
- `add` also accepts a `std::unique_ptr<T>` for an object the user built or took (decision 1).

### 4. Handles *(maintainer)*

- `add<T>` returns a `Handle<T>`: a small, copyable value that never dangles. It behaves like a typed
  `QPointer` or `QPersistentModelIndex`.
- `if (h)` tells whether the object is still there. `h->method()` reaches `T`'s own methods, a subclass's
  included. C++ cannot intercept a call made through `operator->`, so `h->` on a dead handle cannot be
  turned into a no-op: it asserts in a debug build, and what it does in a release build is an open
  question for the maintainer (see **Open questions**).
- The operations every object has (visibility, name, style, data, `remove()`) are also members of the
  handle itself (`h.setVisible(false)`), and those are true no-ops when the object is gone. C++ cannot
  overload `operator.`, so a call with a dot is always one of these handle members; anything else goes
  through `h->`.
- A handle can be connected to like the object (`QObject::connect(h, &QCPGraph::selectionChanged, …)`).
- `h.remove()` deletes the object; every copy of the handle then tests false.
- Handles do not own. The plot owns.

### 5. Signal discipline *(agreed; project 2 folded in)*

- A method emits its signals last, after its own work is complete and the object is consistent (the
  delivery mechanism that makes this hold across call chains and event handlers is answer 1 below). A slot
  connected to any of them may delete the emitter, its plot, or anything else.
- No NeoQCP loop iterates a container a slot can change; it iterates a snapshot of guarded references.
- Destroying a plot emits none of its own signals except `destroyed()`. Its children are deleted first,
  while the plot is still whole; each child's `destroyed()` still fires, because Qt emits `destroyed()`
  even when signals are blocked, and cutting users' connections would be hostile. During teardown the
  plot is marked as being destroyed: mutators called on it or its children do nothing (and assert in a
  debug build), so a `destroyed()` slot cannot re-enter a half-finished teardown.
- Signals are not overloaded: connecting needs no `QOverload` (an API-surface detail decided here because
  it is part of the signal contract).

### 6. Errors: three tiers, no exceptions *(maintainer)*

1. **Always on.** The cheap checks that keep a release build from undefined behaviour: is the object
   behind a handle alive, is a pointer null, is an index in range. A misuse becomes a no-op, never a crash.
2. **Debug only.** A NeoQCP check macro, built on `Q_ASSERT_X`, catches programmer errors (an axis from
   another plot, a handle used after `remove()`, a bad argument) loudly in a debug build and costs nothing
   in a release build. It works the same inside slots and event handlers.
3. **Types first.** Where a type can make a misuse impossible, it does: ownership moves through
   `std::unique_ptr`, handles are typed, positions are typed (decision 8). (A plottable taking only its
   own plot's axes is a tier-2 check, not a type guarantee: making it one would need a type per axis
   rect, a viral change for little gain.)

Bindings on top of NeoQCP (such as SciQLopPlots for Python) do their own release-mode checks and raise
their own errors; that is outside this design.

NeoQCP objects are used on the GUI thread only. A call from another thread is a programmer error: tier 2
asserts on it in a debug build. (Making calls from worker threads safe is a binding's job, as SciQLopPlots
does today.)

### 7. Repaint *(maintainer)*

- Repainting is automatic, asynchronous and deduplicated. Any change marks what it touches dirty.
  However many changes happen in one event-loop turn, they produce one repaint, scheduled on the event
  loop.
- Every setter marks its object dirty; no setter calls `replot()`. Users do not call `replot()` either
  (a synchronous repaint stays available for export and tests).
- It must stay fast: dirtiness is tracked per layer as today, so a change repaints only what it touches.

### 8. Coordinates are typed *(maintainer: "not … complicated Qt stuff where the coordinate system is hard")*

- A position is one value that says what it is: `QCP::Data{x, y}` (in an axis pair's coordinates),
  `QCP::Pixels{x, y}`, `QCP::AxisRatio{x, y}` (fractions of an axis rect), or an anchor plus an offset
  (`tracer.anchor() + QCP::Pixels{10, -6}`). There is no separate "type" to set, and no order to get wrong.
- Conversions are **transform objects**, first-class and public: data↔pixels for an axis or an axis pair,
  axis-rect ratio↔pixels. Log, reversed and time axes are handled inside the transform. Code written for a
  linear axis works on a log axis unchanged.
- If an object's axes go away, its positions do not silently fall back to pixel 0: the object is hidden
  and a debug check fires.

### 9. Drawing and extension API *(maintainer: "like matplotlib")*

- A new plottable or item derives from a small public base class and implements `draw(QCPDrawContext&)`.
  `draw()` **records** what to draw into a draw list rather than painting at once, because one call may
  use both GPU-drawn and QPainter-drawn helpers, which run in different passes. The recording is replayed
  in the right pass and kept until the object or its data changes, so `draw()` must not change the
  object's state. A low-level hook for raw QRhi drawing stays available for authors who need it.
  The context carries the transforms (decision 8), the clip region, the plot's theme, and drawing helpers
  that take typed positions directly: polylines, fills between curves, markers, error bars, text, images,
  shapes.
- Helpers use the GPU path where one exists and QPainter otherwise; the author does not choose.
- The base class provides defaults for what every subclass rewrites today: clipping to the axis rect, a
  hit test (decision 10), a legend icon, the data range used by autoscale, and repaint requests. A
  subclass overrides only what differs.
- The built-in plottables and items are written against the same public base classes and helpers. There
  is no private drawing API that user classes cannot reach.
- **Stacking follows one rule: the layer decides.** Every object kind has a sensible default layer (text
  and annotations above plottables, the grid below them), so the common case needs no thought. Within a
  layer the order is defined and documented, and never changes with the pen style. (Today a text item
  added after a graph can end up under the graph line, and a dashed pen changes the stacking.) A global
  "add order across GPU and QPainter drawing" was considered and rejected: on this renderer it would
  make the number of render passes depend on the data, which the performance anti-goal forbids.

### 10. One hit-test rule *(agreed)*

- A hit test answers a distance in pixels, or "missed". Filled shapes, spans and bars count as hits
  where they are visible. There is no "hit anywhere in the axis rect" shortcut.
- Picking a plottable or item and dispatching a click or wheel event choose the same winner: the topmost
  object (decision 9's order) among those within the selection tolerance.

### 11. One family for line plottables *(agreed)*

- The four line classes today (graph, graph2, multigraph, curve) share one base with one API for pen,
  line style, scatter style, fill, gradient and components. A setter that is silently ignored by a
  subclass (as `setPen` is by the multigraph today) is not possible.
- Which concrete classes remain is the builder's call (it touches project 3's data model); the shared
  interface is not.

### 12. Axes, axis rects and their relations *(maintainer)*

- Axes are created like everything else: `auto rh = plot.add<QCPAxis>(QCP::Left).label(…).color(…);`,
  returning a handle. Several axes on one side stack themselves. `.color(…)` colours the
  label, ticks and tick labels together.
- A plottable or item belongs to one axis rect. Giving it other axes is one call, checked to be axes of the
  same plot (debug check, decision 6).
- Multiple x and y axes, extra axis rects, and axis rects sharing an axis (stacked panels in one plot) are
  ordinary uses of `add`.
- Dragging and zooming follow every axis by default; an axis opts out instead of each one opting in.
- **Linked axes** *(maintainer: "nice to have; at least make the implementation of this kind of feature
  trivial and obvious")*: an axis whose range is derived from another axis through a pair of functions,
  showing the same coordinate in other units (energy and wavelength, UTC and local time, time and orbit
  number), like matplotlib's `secondary_xaxis`. It is built on the transforms of decision 8, so
  implementing it, or any similar feature, takes a few lines.

### 13. Alignment groups replace margin groups *(maintainer: margin groups were too limited)*

- Today a margin group belongs to one plot and only makes margin *sizes* equal inside it. Aligning a stack
  of separate plot widgets needs hand-written code (SciQLopPlots' `VPlotsAlign` repurposes minimum margins,
  subtracts colour-scale widths itself and replots every plot twice).
- In 3.0 an alignment group aligns *edges* (axis-rect positions), not sizes. It works across several plot
  widgets. Colour scales and other side elements are accounted for automatically. It is made by a factory
  and owned like everything else.

### 14. The colour-scale relation *(agreed)*

- "Uses a colour scale" is one relation that any plottable mapping values to colours can have, line
  plottables with a colour axis included. It is set up once in the base class, not wired by hand in each
  class.
- Each axis rect has its colour scale, placed and aligned by the rect (decision 13), hidden until a
  plottable uses it. A colour map uses it unless told otherwise:
  `plot.add<QCPColorMap>(x, y, z).gradient(…).scaleLabel("amplitude")`.
- Deleting a colour scale is noticed (decision 1); nothing unlinks silently.

### 15. One selection model *(agreed)*

- Plottables, items, axis parts and legend rows report selection the same way, with one selection type and
  one signal per object. Data-point selection within a plottable stays a value (`QCPDataSelection`).

## Maintainer's answers to the review questions (2026-11-02)

1. **Signal delivery.** Signals caused by a method call are collected and delivered when the outermost
   NeoQCP call returns: still synchronous, and every level of the call chain is safe. Signals caused by
   user input (mouse, wheel, keyboard), which NeoQCP emits from inside Qt event handlers where deleting
   the widget is forbidden, are delivered just after the event finishes, on the next event-loop turn.
   This replaces the per-method "emit last" rule of decision 5 as the mechanism; the rule stays as the
   guarantee.
2. **A dead handle through `h->` in a release build** stops the program with a clear message (`qFatal`),
   never reading freed memory. The handle's own common operations stay safe no-ops (decision 4).
3. **Creation uses a fluent builder**, not an options struct: `plot.add<QCPGraph>(x, y).name("Bx").pen(p)`.
   It does not depend on declaration order, adding an option never breaks a call site, a setter can
   refuse a bad value, and a subclass's builder extends its base's without a second shape. This replaces
   the options struct in decision 3 (and the "options structs" call below); examples in `before-after.md`
   read accordingly.
4. **A misuse in a release build** is refused and logged with a warning, never silent; in a debug build it
   asserts. This sharpens decision 6's tier 2: the check macro asserts in debug and warns-and-refuses in
   release.
5. **Alignment across plots is not decided.** NeoQCP can already stack several axis rects in one plot, so
   the layout aligns them; SciQLopPlots did not use that because it was slow. The plan's first step for
   decision 13 is to measure why one plot with many axis rects was slow. If the new per-layer repaint
   fixes it, stacked axis rects in one plot become the way to align, and cross-widget alignment groups may
   not be needed. Decision 13 stands as the fallback until that is measured.
6. **Where 3.0 lives:** `main` becomes the 3.0 line, and a `2.x` branch carries 2.x fixes. Clients pin
   `2.x` until ported. The two versions need not install side by side (the author's call: clients pin one).
7. **Polar plots are kept and ported** to the new base classes, polar coordinates and transforms included.

## Calls made while writing this up

The design review raised these; they are the author's calls, not the maintainer's. Check them while
reading.

- **Handles in the public API, objects in signals and drawing.** Getters that hand out plot objects
  (an axis rect's axes, its colour scale, a plot's plottables) return handles. Signal arguments carry the
  object itself (`QCPAbstractPlottable*`): it is alive for the duration of a synchronous slot, and a slot
  that wants to keep it makes a handle. `draw()` gets references. The low-level object API (`take`, `add`
  with a `std::unique_ptr`) is the only other place raw objects appear.
- **Teardown.** When a plot is destroyed it deletes its children first, while the plot itself is still
  whole. Each child's `destroyed()` still fires (users rely on it), but the slots see a consistent plot.
  The plot's own signals other than `destroyed()` do not fire during teardown.
- **Alignment groups are shared by their members.** A group lives as long as it has at least one member
  plot; joining and leaving are explicit, and a plot leaves its groups when it is destroyed. Closing the
  plot that created the group does not dissolve it for the others.
- **Layout-dependent queries are always current.** Transforms, axis-rect geometry and hit tests bring the
  layout up to date first if something changed (computed on demand and cached until something dirties
  it), so they can be trusted right after a change, without a repaint.
- **The one-interface rule covers every duplicated family**, not only the line plottables: the colour maps
  (classic and 2) and the histograms share one base API each. Whether the duplicate classes merge is left
  to project 3, which owns the data model.
- **Performance is part of done:** the first commit adds a benchmark harness that measures the existing
  benchmarks (`tests/benchmark`) plus items added and removed per second and the frame time with 10 000
  items. Today's numbers are the baseline, and each step may not regress them beyond measurement noise,
  so a design choice that costs speed is caught while it is still a choice.
- **What a type must provide to work with `add<T>`:** a builder type (its own, or its base's) and a
  constructor taking the owner context. This contract is written in the base class's documentation, so a
  user subclass knows what to implement.

## Left to the builder

- Concrete class names, the exact options-struct fields per type, the handle's template details, and the
  name of the debug check macro.
- How registries stay correct (derived from the parent tree, or `QPointer` plus `destroyed()`), per class.
- The structure that makes item add, remove and culling scale.
- How the stacking rule is implemented across the GPU and QPainter paths.
- Which line-plottable classes remain behind the shared interface (with project 3).
- The transform objects' API, and how linked axes are exposed.
- The order of migration inside NeoQCP, and how the examples, docs and WebAssembly gallery are ported.

## Later projects (recorded so they are not lost)

- **Threading (project 3):** a review argued NeoQCP should make its own setters safe to call from any
  thread, with one queueing helper in the base class, instead of leaving it to bindings. Decide with the
  data and threading design.

- **Data (project 3):** a second `setData` is staged and invisible to getters for 200 to 1000 ms, with no
  "data committed" signal; borrowed buffers (`viewData`) are read by worker threads with no lifetime guard
  (a data view should require a keeper); several data-source classes chosen by memory layout; `int` sizes
  truncated without a check; each plot owns a thread pool whose threads never expire.
- **API surface (project 4):** enum prefixes that collide, overloaded signal names beyond decision 5,
  index-based access left over, `bool` returns vs `void`, `int` vs `qsizetype`, theme setters that write
  into a shared theme.
- **Speed (project 5):** measured once the structure is in place.

## Evidence

Read-only surveys of the live tree (this repository at e9daa0c), in `docs/plans/neoqcp-3/`:

- `ownership-inventory.md`: who creates, owns and deletes each object; inconsistencies, misuse traps and
  re-entrancy hazards, with file:line evidence.
- `use-cases.md`: 23 real use cases from NeoQCP's docs, examples and SciQLopPlots, with today's code, its
  boilerplate and its unsafe parts; the guard patterns SciQLopPlots added around NeoQCP.
- `api-incoherences.md`: the drawing API, the core objects and their relations, duplicated concepts,
  naming, margin groups, threading.
- `before-after.md`: nine use cases set beside a 3.0 version (a sketch of the shape, not final names).
