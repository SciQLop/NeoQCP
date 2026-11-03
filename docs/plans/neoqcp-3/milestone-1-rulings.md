# NeoQCP 3.0 milestone 1: rulings and deferred minors

Every decision the executor made on the maintainer's behalf during milestone 1 (branch neoqcp-3.0, e9daa0c..db69cf3), and every finding deferred as minor. Copied from the execution ledger.

## Rulings

- Pre-flight: Ruling: work in a separate worktree, not the vendored checkout — SciQLopPlots compiles subprojects/NeoQCP — cost if wrong: none
- Pre-flight (opencode plan review, verified): Ruling: Task 1 benchmarks renamed *_Replot (they time CPU replot, not render()); QCPAxis_SetRange10k added; Task 2 measures the full frame under xcb — cost if wrong: one rename
- Pre-flight: Ruling: opencode's "QBENCHMARK needs -benchmark" rejected — Qt Test prints RESULT lines by default and has no -benchmark switch; Task 1 still checks for RESULT lines — cost if wrong: an empty baseline, caught by that check
- Pre-flight: Ruling: Task 4 null axis — the initializer list dereferences keyAxis (verified plottable.cpp:437-438): null-safe initializer + inert object, keeps "construction not aborted" — cost if wrong: a derived ctor touching the null plot
- Pre-flight: Ruling: Task 6 adds the draw-path guard and removeAxis handling its tests need; re-creating getters return nullptr during teardown (resolves the Task 6/7 conflict) — cost if wrong: none
- Pre-flight: Ruling: Task 6b added for tracer→graph (raw, verified item-tracer.h:120), colour map→scale, bars group, polar removeGraph, intervals without lane layout — the milestone goal is every reachable object; deferring known crashes contradicts "fix every known bug" — cost if wrong: one more task
- Pre-flight: Ruling: Task 7 order — the layout outlives children during teardown — cost if wrong: none
- Pre-flight: Ruling: Task 8 drops the implicit conversion to T* (it brings dangling raw pointers back); qcpConnect helper + static_assert — spec's "connect like the object" is met through the helper — cost if wrong: one more helper
- Pre-flight: Ruling: Task 8 dead-handle qFatal kept — spec answer 2 (maintainer) says qFatal in release; opencode read the older wording — cost if wrong: none (the spec decides)
- Pre-flight: Ruling: Task 9 keeps method-level deferral — spec answer 1 is the maintainer's choice; opencode's "over-reach" rejected; input-signal timing is the one sanctioned exception to "additive only"; a non-tautological test (isReplotting during beforeReplot) added — cost if wrong: test churn
- Pre-flight: Ruling: Task 9 keeps std::function for deferred emissions at the milestone-1 sites (per replot / per event, not per point); QCPAxis_SetRange10k guards it — cost if wrong: an allocation per range change, visible in that benchmark
- Pre-flight: Ruling: Task 9b added (handles + teardown + deferred delivery together) — cost if wrong: none
- Pre-flight: Ruling: QCPHandle stays unused by production getters in milestone 1 — returning handles changes public types, which belongs to milestone 2 (breaking) — cost if wrong: first real integration in milestone 2
- Pre-flight: Ruling: Task 3 default debug handler stays qFatal (spec: assert in debug); each task adding checks runs the full suite, which surfaces any trip — cost if wrong: a check firing in an existing test aborts the runner; seen at once in that task
- Pre-flight: Ruling: 2.x CI deferred until the branches are pushed (outward-facing, maintainer's call); back-port rule written in Task 10
- Task 1: Ruling: ⚠️ "meson test --benchmark prints RESULT lines" — meson hides output on success; the direct binary run shows them, accepted — cost if wrong: none
- Task 2: Ruling: "a real display" = xvfb-run + QT_QPA_PLATFORM=xcb (software rendering; no display on this machine) — relative comparison valid, absolute GPU numbers not — cost if wrong: GPU-specific costs unseen
- Task 2: Ruling: ⚠️ numbers from a throwaway program in the session scratchpad, not re-run by the reviewer; brief's Tracy capture replaced (no capture tool, no display) — accepted, both disclosed in the doc — cost if wrong: ratios off; the doc already asks for a real-GPU check before decision 13 closes
- Task 3: Ruling: ⚠️ QT_NO_DEBUG — build-qt612 (debugoptimized, b_ndebug=false) defines neither QT_NO_DEBUG nor NDEBUG, so tests take the qFatal branch; release takes the qWarning branch only when b_ndebug makes meson's Qt dependency define QT_NO_DEBUG — intended — cost if wrong: a release build that still aborts on misuse
- Task 5: Ruling: ⚠️ the span GPU test skips without QRhi (offscreen, meson test) — correct (no QRhi, no bug), but only a CI job under xvfb runs it; recorded for the final review and the maintainer — cost if wrong: an unguarded regression
- Task 6: Ruling: the "re-created parts return nullptr while being destroyed" half of Task 6 lands in Task 7, which introduces isBeingDestroyed() — cost if wrong: none
- Task 6: Ruling: keep xAxis/yAxis/xAxis2/yAxis2/legend raw (the destructors clear them), keep internal QPointers, revert the mechanical edits — the global constraint "2.x keeps compiling" outranks the brief's wording, and the spec's "no raw pointer outlives its target" holds since they are cleared — cost if wrong: a path that kills an axis without unlistAxis would dangle (reviewer found none)
- Task 6: Ruling: ⚠️ ASan leak ~640 KB per shown plot under xvfb predates this task (the plot destructor never frees span/grid GPU layers or composite resources; inventory "QCustomPlot: Not freed") — it is a known bug, so it is fixed in Task 7 (teardown), not deferred — cost if wrong: none
- Task 6b: Ruling: implementer concerns 2 (polar legend item raw graph pointer) and 3 (intervals null key axis crashes in pixelOf etc.) are in scope — same dangling-relation class, user rule "any known bug must be fixed" — cost if wrong: a slightly bigger diff
- Task 6b: Ruling: tracer whose graph is deleted becomes hidden, not free — release-note item — cost if wrong: one behaviour line to revert
- Task 6b: implementer DONE (97c6b7d + 437e5f0 polar legend row + intervals null axes); Ruling: once-per-loss reportMisuse instead of QCP_CHECK_OR_RETURN per call — matches the axis-check pattern of Task 6 — cost if wrong: misuse reported less often
- Task 7: Ruling: re-creating getters during teardown return the existing part, nullptr only when it would have to be built — "plot still whole" needs the existing parts — cost if wrong: none (refines the pre-flight ruling)
- Task 7: Ruling: concern 2 (internal callers using grid()/plotLayout() unchecked, e.g. axis-rect ctor, crash when built in a slot during teardown) is in scope — known crash — cost if wrong: small diff
- Task 7: Ruling: fix round 1 also takes minors 2 (sizeHint calls virtual minimumSizeHint: 2.x behaviour change), 3 (doc comment displaced), 5 (flag comment), 7 (GPU test asserts nothing offscreen -> QSKIP + check layers existed) — cheap, and 2/7 break stated rules — cost if wrong: none
- Task 8: Ruling: fix minor 1 now (requires-clause rejects forward-declared T; milestone 2 getters in core.h need it) -> static_assert in the members; plus cheap minors 3 (simplify: comment on remove = plain delete, safe for plottables/items only), 5 (as<U> on dead handle test), 6 (<utility>, qcpConnect on a deleted object), 7 (why-comment no implicit T*) — cost if wrong: one small commit
- Task 8: Ruling: two dead handles compare equal — accepted (handles are not keys); qOverload for overloaded signals accepted (same as QObject::connect)
- Task 9: Ruling: Tasks 9 and 9b go to one implementer (9b is tests on 9 primitives), two commits, one review — cost if wrong: a larger review package
- Task 9: Ruling: rangeChanged stays direct (not in the brief's site list; deferring it makes linked axes one frame stale) — milestone 4 design — cost if wrong: one open hazard site until then
- Task 9: Ruling: replot from a replot-signal slot stays a no-op as in 2.x — prevents afterReplot->replot recursion — cost if wrong: none
- Task 9: Ruling: I1 — beforeReplot emitted directly before mReplotting=true, afterLayout between layout and draw, each followed by a liveness return; only afterReplot stays queued — spec decision 5 (consistent object) is met, answer 1's "outermost call" yields to the 2.x purpose of "before" signals; replotSignalsWaitForReplotToFinish still passes — cost if wrong: re-queue two signals. Supersedes the USER FLAG above (report as resolved)
- Task 9: Ruling: I2 — deliver() with no scope open emits directly even with input signals pending; documented reorder — cost if wrong: an ordering note
- Task 9: Ruling: resizeEvent/showEvent get a QCPInputScope — same hazard class as the input handlers — cost if wrong: afterReplot after a resize arrives on the next turn
- Task 9: Ruling: open item (direct beforeReplot/afterLayout slot deleting the plot inside resize/show/paint) -> qcp::reportMisuse in ~QCustomPlot when the outermost open scope is an input scope ("use deleteLater()"); debug aborts, release warns — Qt's own rule; the crash cannot be prevented from the destructor — cost if wrong: a spurious misuse on a supported mouse-path delete
- Task 9: Ruling: post dedupe flag (at most one input flush outstanding) — burst of closes posted one call each — cost if wrong: none
- Task 9: Ruling: misuse narrowed to widget events (resize/show/initialize/render) — mouse/wheel/key-path deletes are a supported, tested 2.x pattern (5 *SlotDeletingThePlotIsSafe tests) and Qt does not touch the widget after those handlers — cost if wrong: none
- Task 10: Ruling: oc#9 (axis rect "couldn't take" on teardown) FALSE — releaseElement clears mParentLayout (layout.cpp:908) so detach's take never runs
- Task 10: Ruling: oc#3 example (setRange returns into freed axis) FALSE — rangeDidChange guards with QPointer (axis.cpp:633); the general "audit every emitting setter" goes to milestone 4 with the remaining direct emits
- Task 10: Ruling: oc#12 (removeItem keeps mParentLegend) FALSE — remove() deletes the item
- Task 10: Ruling: oc#6 (GPU tests skipped in CI) FALSE — tests.yml:127-136 runs the whole binary under xvfb
- Task 10: Ruling: oc#5 -benchmark switch FALSE (pre-flight ruling; real medians produced); its "new paths unmeasured" part ACCEPTED -> mouse-move storm benchmark
- Task 10: Ruling: oc#7 (handle unused in production) — pre-flight ruling, milestone 2
- Task 10: Ruling: setTheme still re-creates a deleted grid (hidden) — themes style axis parts; skipping would give a later grid unthemed colours — cost if wrong: a hidden QObject
- Final: Ruling: re-grade M1 (deleted axis leaves its plottables visible on a live widget until something dirties their layer), M2 (group/intervals legend rows raw back-pointers, UAF), M3 (deleting the last layer unguarded) to Important by effect — the milestone's own promise — fix in the pass
- Final: Ruling: M4 (examples/interactions sets drag axes from mousePress) and M5 (layerChanged reaches user slots during teardown -> document) — M5 doc in the pass; M4 waits for the maintainer's oc#4 decision

## Open decisions for the maintainer

- Task 9: USER FLAG: beforeReplot/afterLayout now arrive after drawing (plan-mandated by replotSignalsWaitForReplotToFinish), so their changes show one replot later; mousePress/mouseWheel slots can no longer steer the current event; guide rect-mode snippet moved to an event filter — biggest 2.x behaviour change, report to user
- Task 9: Ruling: I1 — beforeReplot emitted directly before mReplotting=true, afterLayout between layout and draw, each followed by a liveness return; only afterReplot stays queued — spec decision 5 (consistent object) is met, answer 1's "outermost call" yields to the 2.x purpose of "before" signals; replotSignalsWaitForReplotToFinish still passes — cost if wrong: re-queue two signals. Supersedes the USER FLAG above (report as resolved)
- Task 10: USER DECISION: oc#1 operator-> qFatal in release (spec answer 2) vs opencode "check-then-crash is worse than 2.x; make handles checkable only" — reported, not changed
- Task 10: USER DECISION: oc#4 + design critique: deliver input signals at the end of QCustomPlot::event() instead of next turn (keeps setRangeDragAxes-from-press and event->ignore(), no processEvents in tests) vs spec answer 1 — reported, not changed

## Release-note items

- Task 6: minor (deferred): release note — debug builds now abort at the next draw when a plottable's axis is removed (2.x printed a debug message)
- Task 6b: release note: tracer whose graph is deleted hides instead of becoming free
- Task 7: release note: deleting a plot from a child destroyed() slot refuses add/remove (misuse); objects built then are inert
- Task 9: release notes: afterReplot (and input-caused signals) arrive on the next event-loop turn after mouse/wheel/key/resize/show/paint; mousePress/mouseWheel slots cannot steer the current event; deleting the plot inside resize/show/paint reports misuse (use deleteLater)

## Deferred minors

- Task 1: minor (deferred): QCPItems_Replot10k includes setRange cost and mostly measures clipping (items over [0,100], view [0,1])
- Task 1: minor (deferred): single-shot benchmark rows (RemoveDataAfter ~1 µs) are noise-level, not comparable
- Task 1: minor (deferred): baseline.md names "0040618 plus Task 1" instead of 5ec70fa
- Task 1: minor (deferred): Stack8 benchmarks differ in widget geometry — carried into Task 2's dispatch (compare at equal geometry)
- Task 1: minor (deferred): sineKeys returns a ramp (rename rampKeys)
- Task 2: minor (deferred): the measuring program is not kept; the doc should say so or keep it under tests/perf
- Task 2: minor (deferred): "idle frame 3x a small widget" compares 640x360 to 640x45; the 7.3 ms all-changed frame is the fairer reference
- Task 3: minor (deferred): Q_UNLIKELY / cold attribute on the failing branch
- Task 3: minor (deferred): misuse.h listed in the moc header lists though it has no Q_OBJECT (precedent: datacontainer.h); drop if moc complains
- Task 3: minor (deferred): setMisuseHandler(nullptr) restore path untested
- Task 4: minor (deferred): QCPGroupLegendItem::mMultiGraph and QCPIntervalsLegendItem::mIntervals are raw; a destroyed() slot connected before the row was made that replots synchronously would draw a dead pointer — FINAL REVIEW: triage (2-line QPointer fix, on the milestone goal)
- Task 4: minor (deferred): mParentLegend updated only by QCPLegend::addItem, not raw addElement (now a QPointer: null deref, not UAF)
- Task 4: minor (deferred): mGraphs.removeIf compares QCPGraph* with the base pointer inside ~QCPAbstractPlottable (UB on paper, base at offset 0)
- Task 4: minor (deferred → Task 6b): QCPIntervals::requestRepaint (intervals.cpp:255) calls mParentPlot->replot unguarded; reachable on an inert intervals — carry into Task 6b's intervals step
- Task 5: minor (deferred): items a destroyed() slot adds during clearItems now survive (old loop deleted them); release-note it or simplify: note
- Task 5: minor (deferred): plain anchors (not positions) still reset children's pixel location when their item dies (~QCPItemAnchor runs after the subclass part) — pre-existing behaviour, visible jump; FINAL REVIEW: triage (cache last pixel position, or delete anchors while the item is complete)
- Task 5: minor (deferred): unused `second` in clearItemsWhileASlotDeletesAnother
- Task 5: minor (deferred): during clearItems, destroyed() slots see itemCount()==0; document on clearItems
- Task 6: minor (deferred): deleting the last layer leaves zero layers and a null current layer; untested (addGraph/replot after it)
- Task 6: minor (deferred): "report once" resets when both axes return, so it is once per loss (header says so; brief said once per plottable) — chosen reading
- Task 6: minor (deferred): release note — debug builds now abort at the next draw when a plottable's axis is removed (2.x printed a debug message)
- Task 6: minor (deferred): deleteLayerLeavesLists cannot assert the GPU-map entries are gone (protected maps); xvfb+ASan covers it
- Task 6: minor (deferred): a re-created grid starts hidden and can be re-created mid render pass; document on grid()
- Task 6: minor (deferred): stray double blank line axis.cpp:52
- Task 6b: minor -> Task 7: deleting an angular axis with live polar graphs untested (graph dtor edits the axis graph list during axis teardown); add deleteAngularAxisWithGraphs under ASan
- Task 6b: minor (deferred, FINAL REVIEW: triage): intervals inert test asserts gMisuses>=1, code reports exactly 2 — pin QCOMPARE(gMisuses,2)
- Task 6b: minor (deferred, FINAL REVIEW: triage): mid-drag tests check gestureActive() only after release — assert right after MouseMove
- Task 6b: minor (deferred, FINAL REVIEW: triage): deleteTracedGraph does not check selectTest == -1
- Task 6b: minor (deferred): pixelOf returns x=0 without key axis — document fallback values
- Task 6b: minor (deferred): misuse messages name the guard (Q_FUNC_INFO), not the public method
- Task 7: minor (deferred): colour scale detects teardown via "axis rect has no axes" instead of isBeingDestroyed() (colorscale.cpp:1101,1127)
- Task 7: minor (deferred, FINAL REVIEW: triage): setSelectionRect unguarded during teardown (core.cpp:1101)
- Task 7: minor (deferred): GPU-layer getters null-during-teardown untested (needs xvfb)
- Task 7: minor (deferred): releaseResources frees plottable/scatter GPU layers without std::exchange (core.cpp:3307)
- Task 8: minor (deferred): no converting ctor QCPHandle<Derived> -> QCPHandle<Base> (milestone 2); misuse location shows checkAlive
- Task 9: minor (deferred, milestone 4): replots from resizeEvent/showEvent/initialize flush inside the Qt handler; index walks in QCPLegend::clearItems / QCPLayout::clear can skip; nested event loop reorders flush; ~QCPAxisRect same flush shape (theoretical)
- Task 9: minor (deferred): Qt CUPS print plugin 80 B LSan leak via QPrinter in the savePdf test row — Qt code; consider an LSan suppression file
- Task 10: minor (deferred, FINAL REVIEW: triage): axis-rect dirty marking calls the creating GPU grid-layer accessor (pre-existing)
- Task 10: minor (deferred): plan doc still lists deliver as public

## Maintainer answers

- 2026-11-03: dead-handle operator-> keeps qFatal in release (spec answer 2 confirmed).
