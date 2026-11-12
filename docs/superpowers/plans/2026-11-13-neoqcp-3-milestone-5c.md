# NeoQCP 3.0 milestone 5c — one graph Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 3.0 has one graph class. The old `QCPGraph` (container, painter) and `QCPGraph2` are deleted;
`QCPMultiGraph` becomes `QCPGraph`; every user (library, tests, benchmarks, snippets, docs, examples, doc image
generator) is ported once; the graph family's getters return handles (deferred from 5b, K3).

**Architecture:** a pure rename of the multigraph family (files, classes, ops handle, component struct, pipeline
type), the deletion of the two old classes and everything only they used (the data sources' pixel line API, the old
graph pipeline, the data-locator branches, the graph registration special case), then the user port. 5a gave the
multigraph every feature the old graph's users need (user systems, `addData`, one-column `setData`, fill to another
graph, the tracer); `setBrush` already fills every component to value 0, as 2.x did.

**Tech Stack:** C++20, Qt 6.12, meson + `cmake/sources.cmake`, QtTest, `tests/benchmark`.

**Spec:** `docs/specs/2026-11-11-neoqcp-3-milestone-5-design.md` (C1, C3, C4, C5, A4, A5, A7 "5c one graph", A8, A10,
A12, A13). Facts: `docs/plans/neoqcp-3/m5-inventory.md` §3 (feature matrix), §4 (every user, counts per directory),
§7 (bench rows and reference pictures); `milestone-5a-rulings.md`, `milestone-5b-rulings.md` (R-2: the graph family
stays raw until 5c). The maintainer said "go ahead on 5b" and, on 2026-11-12, "do a bit more with claude and less with
opencode" and "offload a bit of builds to the coder builder".

## Calls made in this plan (G-1..G-10; the maintainer can overrule any)

- **G-1 Names.** `QCPMultiGraph` → `QCPGraph`; `QCPMultiGraphOps` → `QCPGraphOps`; `QCPMultiGraph::Component` →
  a struct `QCPGraphComponent` (no alias, A13); `QCPMultiGraphPipeline` → `QCPGraphPipeline` (after the old class of
  that name is deleted with `QCPGraph2`); files `plottable-multigraph.{h,cpp}` → `plottable-graph.{h,cpp}`,
  `multigraph-fill.*` → `graph-fill.*`, `multigraph-errorbars.*` → `graph-errorbars.*`; `QCPWaterfallGraph` derives
  `QCPGraph`. `QCPGroupLegendItem::multiGraph()` → `graph()`. No `QCPMultiGraph` alias (no shims; NeoQCP-only name).
- **G-2 Deleted:** the old `QCPGraph` and `QCPGraph2` (headers, sources, ops, `QCPGraph2`'s pipeline and caches), the
  data sources' pixel line API (`getLinesAll`, `getOptimizedLineDataAll`, the indexed forms and the single-column
  family, with the waterfall adapter's overrides; 4b R-17), the data-locator graph/graph2 branches, the
  `qobject_cast<QCPGraph*>` registration special case in adoption, every `\deprecated` comment about them.
  `QCPGraphData`/`QCPGraphDataContainer` stay in `graph-data.h` (the polar graph uses them; C5).
- **G-3 The graph list** (A5): the renamed class registers in the plot's graph list from its constructor when the
  base registered it; `addGraph(key, value)` returns the same handle as `add<QCPGraph>(key, value)`; `graph(i)`,
  `graph()`, `selectedGraphs()`, `QCPAxis::graphs()`, `QCPAxisRect::graphs()`, `QCPPolarArea::graphs()` return
  `QCPHandle<QCPGraph>` (lists of them), as 5b did for the other getters (R-2 lifted).
- **G-4 The tracer** (A8): `setLineGraph` becomes `setGraph(QCPGraph*, int component = 0)`; `lineGraph()` becomes
  `graph()` returning `QCPHandle<QCPGraph>`; the `Q_PROPERTY graph` keeps its pointer type (5b R-1/H-7); the ops
  handle follows.
- **G-5 Channel fill:** `setChannelFillGraph`/`channelFillGraph` go with the old class; the replacement is
  `setComponentFillToGraph(component, other, otherComponent)` (5a). Every user is ported to it.
- **G-6 2.x data API:** `data()` (the shared container) and `setData(QSharedPointer<QCPGraphDataContainer>)` go;
  `setData(keys, values)`, `addData` stay on the new class (5a). A 2.x user of `data()->remove*`/iteration ports to
  `dataSource()` reads or rebuilds; the migration page shows both.
- **G-7 Bench rows** (A12, 5a B10): the `QCPMultiGraph_*` rows are renamed `QCPGraph_*` (replacing the old rows of
  the same workload); `QCPGraph_RemoveData*` retire with `data()`; the `QCPGraph2_*`, `QCPInput_WheelOverGraph2_1M`
  and `QCPGpu_Graph2Render1M` rows are deleted (their multigraph twins carry the workload). The 5c gate compares each
  renamed row with its 5a twin (A/B against a build of the 5c base).
- **G-8 Reference pictures** drawn with the old graph (`errorbars-*` ×5, `item-tracer`, `user-system-skew`) are
  redrawn with the new graph. Each moved picture is looked at by the controller before it is accepted; an unchanged
  look is the goal (same data, same styles).
- **G-9 Docs:** `docs/graphs.md` describes one graph (the old/new choice table goes); every page, snippet and the doc
  image generator use `QCPGraph`; the migration page lists the 5c breaks (renames, `data()`, channel fill, tracer).
- **G-10 Builds** run on the Coder workspace `neoqcp` (skill `coder-remote-builds`; sources rsynced, `~/build.sh`),
  except the reference-picture classes, which only match locally (fonts) and are run locally at the end of each
  task when the machine has memory to spare.

## Global Constraints

- Builds and the non-picture suite on Coder: `rsync -a --delete --exclude 'build*/' --exclude .venv/ --exclude .git/
  --exclude .superpowers/ --exclude qt.ini -e coder-rsh ./ neoqcp:NeoQCP-3.0/` then
  `coder ssh neoqcp -- '~/build.sh'` (detached for long runs: `(nohup ... > ~/x.log 2>&1 &)`, then poll). The env file
  is `. ~/.config/lpp-coder/env`. One build per build directory at a time. On the workspace the reference-picture
  comparisons fail by fonts (71 known failures); every other test must pass there.
- Locally: at most `ninja -C build-qt612 -j 4`, one at a time, only when free memory allows (check `free -g`); never
  `meson setup` in the worktree.
- ASan: a remote ASan tree (`meson setup build-asan -Db_sanitize=address,undefined` on the workspace) for the touched
  classes per task and the whole suite after the last code change.
- clang check on every changed `.cpp` (remote: `clang++ -fsyntax-only` from `build/compile_commands.json`, or locally).
- Examples built (`with_examples=true`) and the guide snippets (`doc-snippets`) green.
- Bench gate: renamed graph rows within 5 % of their 5a twins, every drawing/item/axis row within 5 %, others 15 %;
  A/B on the workspace (interleaved base/new binaries) so local load does not matter.
- No shims, no aliases; misuse via `QCP_CHECK_OR_RETURN`; failing tests first where behaviour is new (G-3 registration,
  G-4); type changes pinned by compile-level tests in `tests/auto/test-handle`.
- Commit with explicit pathspecs; never `subprojects/.wraplock`, `subprojects/jkqtmathtext`, `qt.ini`. Never push.

## Review Focus

1. A 2.x graph program (lines, scatters, step styles, `setBrush` fill, `addData`, channel fill, error bars, tracer,
   legend, selection, rescale) ported mechanically draws the same picture.
2. `addGraph`/`graph(i)`/`graphCount`/`removeGraph`/`clearGraphs` with the renamed class: registration, removal,
   take/adopt, a graph refused at construction (5a), a graph deleted directly.
3. The waterfall after the base rename; the polar graph's `QCPGraphData` after the old header is gone.
4. Nothing in `src/` still names `QCPMultiGraph`, `QCPGraph2`, the old pixel line API, or `channelFill`.
5. Bench: the renamed rows against their 5a twins.

---

### Task 1: Rename the multigraph family to the graph family; delete the old classes

**Files:** the files in G-1 and G-2, `meson.build`, `cmake/sources.cmake`, `src/qcp.h`, `src/core.{h,cpp}`
(registration, graph list), `src/axis/*`, `src/layoutelements/*`, `src/polar/*` (graph lists),
`src/items/item-tracer.*` (G-4), `src/data-locator.*`, `src/datasource/*` (pixel API), `src/plottables/plottable-waterfall.*`,
`tests/auto/test-handle` (compile-level pins). Tests are ported in Task 2; this task may leave the test tree
temporarily uncompiled ONLY if Task 2 runs immediately after in the same branch state — do not commit a library
that does not build; commit Tasks 1 and 2 together if the tests cannot compile in between.

- [ ] **Step 1: Compile-level pins** in `TestHandle`: `QCPGraph` is the former multigraph (`std::is_base_of_v<
  QCPAbstractLine, QCPGraph>`), `graph(i)`/`addGraph`/`selectedGraphs`/`graphs()` return the G-3 handle types, the
  tracer's `setGraph(QCPGraph*, int)` and `graph()` (G-4); `aConstructedGraphIsInTheGraphList`,
  `addGraphReturnsTheSameHandleAsAdd` (now the new class). Expected: FAIL.
- [ ] **Step 2: Implement** G-1..G-4 and the src side of G-5/G-6 (no library code uses channel fill or `data()` after).
- [ ] **Step 3:** `grep -rn "QCPMultiGraph\|QCPGraph2\|getLinesAll\|ChannelFill\|channelFill" src` is empty (except
  changelog-style history comments, which go too). Remote build of the library.
- [ ] **Step 4: Commit** `refactor(graph): one graph — the multigraph is QCPGraph; the old graphs are gone`
  (together with Task 2 if needed).

### Task 2: Port the tests and the benchmarks

**Files:** every file in m5-inventory §4.2 and `tests/benchmark/benchmark.cpp` (G-7), `tests/manual/*`, `tests/perf/*`,
the reference scenes in `tests/auto/test-reference-m4e` (G-8).

- [ ] **Step 1:** port mechanically: old graph → new graph (`setData`, `addData`, `setBrush`, styles), `data()` reads
  → `dataSource()`, channel fill → fill to graph, `setLineGraph` → `setGraph`. Delete the tests whose subject is gone
  (the old graph's container tests, `QCPGraph2`'s pipeline tests whose behaviour the multigraph tests already cover —
  list each deleted slot and the multigraph test that covers it in the report; a slot with no counterpart is ported,
  not deleted).
- [ ] **Step 2:** the benchmark rows per G-7.
- [ ] **Step 3:** remote build and the whole non-picture suite on the workspace; locally the reference-picture classes
  (`TestReferenceM4e`, `TestErrorBarGeometry`, `TestSpan`, `TestIntervals`, `TestStatisticalBox`, `TestQCPFinancial`
  and any other picture class) when memory allows. Moved pictures (G-8): written, paths in the report, NOT accepted.
- [ ] **Step 4: Commit** `test(graph): the tests and benchmarks use the one graph`.

### Task 3: Snippets, docs, examples, the doc image generator

**Files:** `docs/snippets/*.cpp`, `docs/*.md` (G-9), `examples/*`, `documentation/doc-image-generator/*`,
`documentation/doc-code-snippets/*`, `docs/images/*` (regenerated where a snippet's picture changes; looked at by the
controller).

- [ ] **Step 1:** port; `docs/graphs.md` rewritten for one graph; regenerate the snippet pictures that changed.
- [ ] **Step 2:** `doc-snippets` green; examples built (`with_examples=true`) on the workspace.
- [ ] **Step 3: Commit** `docs(graph): the guide, examples and generator use the one graph`.

### Task 4: Review and finish

- [ ] **Step 1:** Final whole-branch review: a fresh Opus reviewer (Claude) and one opencode second opinion; one fix
  batch.
- [ ] **Step 2:** whole suite (remote non-picture + local pictures), whole ASan (remote), clang, snippets, examples.
- [ ] **Step 3:** Bench: A/B on the workspace against a build of the 5c base (every graph, drawing, item row).
- [ ] **Step 4:** `docs/migrating.md`, `changelog.txt` (milestone 5c), `docs/plans/neoqcp-3/baseline.md`,
  `backlog.md`, `milestone-5c-rulings.md`, `sciqlopplots-migration.md` (from m5-inventory §6: SciQLopPlots'
  `QCPGraph` variant visitors, `QCPGraph::LineStyle` conversions, `addGraph` in the wrapper, `selectedGraphs`).
- [ ] **Step 5: Commit** `docs(neoqcp-3): end of milestone 5c — rulings, baseline, backlog, changelog`.

---

## Amendments after review (binding; they override the calls and the tasks above)

Review: `.superpowers/m5c-plan-review.md` (a fresh Opus reviewer, Claude). The controller read the evidence for
C1-C3 and I5. The anti-goal decides: "a use case 2.x users had that 3.0 silently loses" is not allowed.

- **R1 (C1) Selection, as 2.x.**
  - `setSelection(sel)` on a `QCPGraph` (the base, non-virtual setter's path: hook the base's change, or make the
    graph's selection the union view, implementer's choice, stated in the report) applies `sel` to every component.
    Rows are shared across columns.
  - `selection()` stays the union (`updateBaseSelection`).
  - The selected rows are drawn as segments, with the selected pen and scatter style. The rest of the line keeps the
    normal pen.
  - The selection decorator is honoured: its pen, brush and scatter style when the user set one, and
    `drawDecoration` (the bracket) through the recording's painter callback, as the other recorded plottables do.
  - Tests first: `setSelectionDrawsTheSelectedSegments`, `theDecoratorPenIsUsed`, `theBracketDecoratorDraws`,
    `clearingTheSelectionClearsEveryComponent` (the SciQLopPlots Escape path), `aPartialSelectionKeepsTheRestNormal`.
- **R2 (C2) The legend row.** `QCPGroupLegendItem` derives `QCPPlottableLegendItem`; it is a plottable's row.
  `itemWithPlottable`/`hasItemWithPlottable` then find it, and `qobject_cast<QCPPlottableLegendItem*>` works. Its
  group drawing stays. Tests first: `itemWithPlottableFindsAGraphRow`, `aGraphRowCastsToAPlottableRow`.
- **R3 (C3) Brush and fills.**
  - `setBrush`, remembered or immediate, never replaces a component's fill to another graph. It changes that fill's
    brush instead, as 2.x's brush coloured the channel.
  - It fills to value 0 only the components with no explicit fill.
  - A fill to another graph made with the default brush takes the graph's brush when one is set; otherwise
    `effectiveFillBrush`.
  - The port rule for `setChannelFillGraph(other)` is `setComponentFillToGraph(0, other, 0)`.
  - Tests first: `setBrushBeforeAFillToGraphKeepsTheBand`, `setBrushAfterAFillToGraphRecoloursIt`,
    `aFillToGraphTakesTheGraphBrush`.
- **R4 (I5) The negative-log baseline, as 2.x.** On cartesian too, the fill and impulse baseline on a logarithmic
  value axis is the range bound nearer zero (`upper` on a negative range). The 5a deferral ends. Test first. The
  reference pictures must not move except where they draw a negative log range.
- **R5 (I1, I2) The graph list holds every `QCPGraph`.**
  - Constructor registration, plus the adoption/late-join registration (the cast stays, retyped to the new class).
  - Multi-column graphs and waterfalls are graphs, so they are listed. `clearGraphs()` removes them.
  - SciQLopPlots' migration note lists its `QCPMultiGraph` sites and every cast whose meaning changes.
- **R6 (I3, I4) Pipelines and data-source tests.**
  - The multigraph pipeline takes the name `QCPGraphPipeline` only after the old class of that name is deleted, in
    the same step. The generic pipeline tests are ported to it, not deleted.
  - Before deleting the pixel line API, its test coverage is ported to the multi-source API.
  - The report lists each deleted test with the test that now covers its behaviour. Nothing is deleted without one.
  - The single-column resampler's fate is stated: it goes if only `QCPGraph2` used it.
- **R7 (I6, I7) Complete lists in the tasks' reports.**
  - Every bench row with its disposition (renamed, retired, unchanged), including the GPU and input multigraph rows,
    with an old-name → new-name map for the A/B.
  - Every picture that moves: the reference scenes, the snippet pictures and the 25 doc-image-generator pictures.
  - The expected visual changes are listed up front: legend rows of graphs, and fill colours where 2.x took the
    brush. Anything else that moves is a finding.
- **R8 (I8) Remote and local.**
  - Pictures (reference, snippet, doc generator) are written and compared locally, because the fonts differ.
  - The A/B base tree reaches the workspace by rsync of a local worktree of the base commit (`.git` is excluded and
    the branch is unpushed).
  - The workspace has no QRhi offscreen, so the GPU rows and GPU tests run there under
    `xvfb-run -a -s "-screen 0 1600x1200x24" env QT_QPA_PLATFORM=xcb`.
- **R9 (I9) Five steps, each commit building.** They replace Tasks 1-2:
  1. Delete `QCPGraph2` and everything only it uses: its pipeline, the single-column pixel API, the data-locator
     branch. Port its tests.
  2. R1-R4 on the multigraph, with tests first.
  3. Port the old `QCPGraph`'s users that do not go through the graph list (direct construction, channel fill,
     `data()`, the tracer's old target) to the multigraph.
  4. Retype the graph list, `addGraph` and the tracer (G-3, G-4) to the multigraph. Delete the old `QCPGraph` and the
     rest of the pixel API.
  5. The pure `QCPMultiGraph` → `QCPGraph` rename (G-1). Then Task 3 (docs, examples, generator) and Task 4
     (review, finish) as written.
