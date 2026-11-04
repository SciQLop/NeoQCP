# Item scaling — profile of the add/delete/draw paths

Rows added to `tests/benchmark/benchmark.cpp` next to `QCPItems_AddRemove10k`. Each row was run
three times with `QT_QPA_PLATFORM=offscreen build-qt612/tests/benchmark/qcp_benchmark -median 5
<Row>`; the table shows the three medians and their median. "per iteration" is what the runner
prints: for `QBENCHMARK_ONCE` rows that is the single pass.

| row | what it measures | run 1 | run 2 | run 3 | median |
| --- | --- | --- | --- | --- | --- |
| `QCPItems_Add10k` | 10 000 `QCPItemLine` added (`QBENCHMARK_ONCE`) | 41 ms | 41 ms | 41 ms | **41 ms** |
| `QCPItems_Clear10k` | `clearItems()` on 10 000 items (`QBENCHMARK_ONCE`) | 23 ms | 24 ms | 23 ms | **23 ms** |
| `QCPItems_DeleteEach10k` | `removeItem(p)` per item, pointers shuffled (`mt19937{42}`) | 29 ms | 29 ms | 29 ms | **29 ms** |
| `QCPItems_RemoveEach10k` | `removeItem` in list order | 31 ms | 35 ms | 30 ms | **31 ms** |
| `QObject_DeleteEachChild10k` | control: 10 000 plain `QObject` children deleted | 0.61 ms | 0.60 ms | 0.61 ms | **0.61 ms** |
| `QCPItems_DeletePlot10k` | a second plot with 10 000 items, `delete plot` | 63 ms | 64 ms | 64 ms | **63 ms** |
| `QCPItems_Zoom10k` | zoom + `replot(rpImmediateRefresh)`, 10 000 items, 100 in view | 0.14 ms | 0.14 ms | 0.14 ms | **0.14 ms** |
| `QCPItems_Zoom100` | the same with 100 items, all in view | 0.14 ms | 0.14 ms | 0.14 ms | **0.14 ms** |
| `QCPItems_MouseMove10k` | 10 000 synthetic mouse moves over 10 000 items | 4.2 ms | 4.3 ms | 3.9 ms | **4.2 ms** |
| `QCPItems_MouseMove100` | the same over 100 items | 4.0 ms | 4.0 ms | 3.8 ms | **3.9 ms** |
| `QCPItems_AddRemove10k` (existing) | add 10 000 + `clearItems()` | 62 ms | 62 ms | 62 ms | **62 ms** |

`perf` is available (`perf_event_paranoid = 2`, user-space profiling allowed; nothing was changed on
the system).

## What the numbers already say

1. **Adding and deleting are the whole cost.** 10 000 adds cost 41 ms, 10 000 removals 23–31 ms —
   one `QCPItems_AddRemove10k` pass is 62 ms, i.e. add + clear with nothing else in it.
2. **It is not Qt's child list.** Deleting 10 000 plain `QObject` children costs 0.61 ms
   (`QObject_DeleteEachChild10k`). NeoQCP's own lists account for the other ~62 ms.
3. **Drawing does not scale with the item count.** `QCPItems_Zoom10k` and `QCPItems_Zoom100` are both
   0.14 ms, and `QCPItems_MouseMove10k` (4.2 ms) is the same as `QCPItems_MouseMove100` (3.9 ms) —
   the mouse rows are dominated by sending 10 000 events, not by the items. There is nothing to cull:
   zooming and hit-testing 10 000 items with 100 in view cost exactly what 100 items cost.
4. **Deleting the plot costs the same as deleting the items**: `QCPItems_DeletePlot10k` (63 ms) ≈
   `CLEAR10k` (23 ms) + the rest of the plot teardown; the item list is cleared through
   `deleteAllItems()` either way, so Task 2 fixes both.

## Perf (top self-time frames, `--no-children`)

`perf record -g -o build-qt612/oc-scratch/<row>.data build-qt612/tests/benchmark/qcp_benchmark …`
then `perf report -i … --no-children --stdio`.

**QCPItems_Clear10k** (707 samples) — the cleanest picture, since the row contains nothing but the
deletion of 10 000 items:

| self | frame |
| --- | --- |
| 18.8 % | `QtPrivate::indexOf<QCPAbstractItem*>` — `mItems.contains`/`removeOne` in `itemDestroyed` and Qt's `children.indexOf` |
| 14.7 % | `QObjectPrivate::setParent_helper` — Qt unparenting each item |
| 10.3 % | `QCPLayer::addChild` — the `mChildren.contains` scan (runs when a layer is re-attached during teardown) |
| 10.2 % | `QCPLayer::removeChild` — `mChildren.removeOne` |
| 1.9 % | `QArrayDataPointer<QCPItemPosition*>::reallocateAndGrow` — anchor/position list growth |

**QCPItems_DeleteEach10k** (753 samples): `indexOf` 20.7 %, `QCPLayer::addChild` 10.8 %,
`__memmove_avx_unaligned_erms` 6.8 %, `sequential_erase_one<QList<QCPAbstractItem*>>` 5.8 %,
`setParent_helper` 5.1 %, `QCPLayer::removeChild` 3.9 %. The `memmove` and `sequential_erase_one`
shares are the `mItems.removeOne` erase in `removeItem()` — the scan-plus-shift pair the ledger keeps
on purpose.

**QCPItems_Add10k** (757 samples) — note that this trace also contains the destructor of the 10 000
items (the row's `cleanup()` runs inside the traced process), so add and delete shares are mixed:
`indexOf` 13.8 %, `QCPLayer::addChild` 13.6 %, `setParent_helper` 13.1 %, `QCPLayer::removeChild`
10.5 %, `QCPLayer::invalidatePaintBuffer` 3.0 %.

**QCPItems_Zoom10k** (2051 samples) and **QCPItems_MouseMove10k** (899 samples): both traces are
dominated by the same construction/teardown frames (`indexOf` 4.7 % / 15.0 %, `setParent_helper`
4.5 % / 12.7 %, `addChild` 4.2 % / 12.7 %, `removeChild` 3.5 % / 9.1 %) because the measured region is
0.14 ms / 4.2 ms while the 10 000 items are created before it and destroyed after it. The profiles say
nothing about the draw path; the equal 10k/100 rows above are the evidence that it does not scale.

## What Task 2 should attack, and what it should not

- Attack `mItems.contains`/`mItems.removeOne` (`indexOf` + erase), `QCPLayer::addChild`'s
  `mChildren.contains` and `QCPLayer::removeChild`'s `removeOne`: together ~50 % of the self time in
  both the add and the delete traces.
- `QObjectPrivate::setParent_helper` (5–15 %) is Qt's; the items must stay plot children (spec
  decision 2), so that share is a floor.
- Culling (Task 4 of the plan) has no measured basis: 10 000 items cost the same as 100 when drawing,
  zooming and hit-testing.
