# NeoQCP 3.0 speed baseline

Every later task must not regress these numbers (CPU side of `replot()`: paint-buffer rasterisation; GPU upload and
composite happen in `render()`, which an offscreen benchmark does not run).

- Machine: AMD Ryzen 7 5800X 8-Core, Linux 7.2.5 (Fedora)
- Qt: 6.12.0 (gcc_64), build `build-qt612`, debugoptimized
- Commit: 0040618 (neoqcp-3.0) plus the Task 1 benchmarks
- Command: `QT_QPA_PLATFORM=offscreen build-qt612/tests/benchmark/qcp_benchmark -median 5`, run three times
- Values are the median of the three runs, per iteration, in msecs (the `RemoveData*` and `AddDataSingle*`
  benchmarks are single-shot: one iteration, so noisier)

| Benchmark | msecs | run 1 / 2 / 3 |
|---|---|---|
| QCPGraph_Standard | 0.7 | 0.67 / 0.76 / 0.7 |
| QCPGraph_ManyPoints | 2.7 | 2.7 / 2.6 / 2.7 |
| QCPGraph_ManyLines | 1.3 | 1.3 / 1.3 / 1.3 |
| QCPGraph_ManyOffScreenLines | 0.15 | 0.15 / 0.15 / 0.15 |
| QCPGraph_RemoveDataBetween | 0.11 | 0.11 / 0.12 / 0.083 |
| QCPGraph_RemoveDataAfter | 0.00093 | 0.00093 / 0.00095 / 0.0009 |
| QCPGraph_RemoveDataBefore | 0.33 | 0.34 / 0.21 / 0.33 |
| QCPGraph_AddDataAtEndSorted | 5.1 | 5.1 / 5.1 / 5.1 |
| QCPGraph_AddDataAtBeginSorted | 5.3 | 5.4 / 5.2 / 5.3 |
| QCPGraph_AddDataMixedSorted | 10 | 10 / 10 / 10 |
| QCPGraph_AddDataAtEndUnsorted | 23 | 23 / 23 / 23 |
| QCPGraph_AddDataAtBeginUnsorted | 28 | 28 / 28 / 28 |
| QCPGraph_AddDataMixedUnsorted | 29 | 29 / 29 / 28 |
| QCPGraph_AddDataSingleAtEnd | 0.024 | 0.024 / 0.024 / 0.023 |
| QCPGraph_AddDataSingleAtBegin | 0.045 | 0.045 / 0.046 / 0.045 |
| QCPGraph_AddDataSingleRandom | 5.1 | 5.1 / 5.1 / 5 |
| QCPGraph2_SetDataDouble | 9.4 | 9.8 / 9.3 / 9.4 |
| QCPGraph2_SetDataFloat | 8.4 | 8.5 / 8.2 / 8.4 |
| QCPGraph2_RenderDouble | 0.14 | 0.14 / 0.14 / 0.14 |
| QCPGraph2_RenderFloat | 0.14 | 0.14 / 0.14 / 0.14 |
| QCPGraph2_RenderManyLines | 0.44 | 0.44 / 0.44 / 0.44 |
| QCPGraph2_ScatterDisc1M | 0.12 | 0.12 / 0.12 / 0.12 |
| QCPGraph2_ScatterSquare1M | 0.12 | 0.12 / 0.12 / 0.12 |
| QCPGraph2_ScatterWithLine1M | 0.13 | 0.12 / 0.13 / 0.13 |
| QCPColorMap_Standard | 0.22 | 0.22 / 0.22 / 0.21 |
| QCPColorMap_ColorizeMap | 0.27 | 0.28 / 0.27 / 0.27 |
| QCPColorMap2_ResampleDouble | 227 | 230 / 227 / 224 |
| QCPColorMap2_ResampleFloat | 150 | 150 / 150 / 139 |
| QCPColorMapData_ConstructLarge | 1.3 | 1.3 / 1.1 / 1.4 |
| QCPColorMap2_ResamplePanSequence | 181 | 242 / 181 / 176 |
| QCPColorMap2_ResamplePanSequenceSmall | 206 | 206 / 203 / 208 |
| QCPColorMap2_ResampleWideSerial | 768 | 766 / 771 / 768 |
| QCPColorMap2_ResampleWideParallel | 263 | 270 / 263 / 261 |
| QCPAxis_TickLabels | 0.24 | 0.24 / 0.25 / 0.24 |
| QCPAxis_TickLabelsCached | 0.089 | 0.088 / 0.089 / 0.089 |
| QCPAxis_SetRange10k | 1.2 | 1.2 / 1.2 / 1.3 |
| QCPItems_AddRemove10k | 80 | 78 / 80 / 82 |
| QCPItems_Replot10k | 0.13 | 0.13 / 0.13 / 0.14 |
| QCPAxisRects_Stack8_Replot | 1.8 | 1.8 / 1.9 / 1.8 |
| QCPPlots_Stack8_Replot | 0.26 | 0.26 / 0.26 / 0.26 |

## End of milestone 1

- Commit: 572c5e0 (neoqcp-3.0), same command and machine as above, three sequential runs
- Load average (uptime, 1/5/15 min) at the start of each run: run 1: 2.12 / 2.45 / 3.69, run 2: 2.69 / 2.59 / 3.65, run 3: 2.54 / 2.58 / 3.58 (the machine was not idle: the full test suites had just run)
- Ratio = end-of-milestone median / baseline median. The slowest rows are QCPGraph_AddDataMixedSorted (+10%, 10 vs 11 ms: two-digit rounding, the baseline's own runs were 10 / 10 / 10) and QCPAxis_SetRange10k (+8%, 1.2 vs 1.3 ms, the baseline's run 3 was 1.3); both inside the +-15% band.
- Rows outside the +-15% noise band, all faster: QCPGraph_RemoveDataBetween, QCPGraph_RemoveDataAfter,
  QCPGraph_RemoveDataBefore, QCPColorMapData_ConstructLarge (single-shot or noisy), QCPItems_AddRemove10k (0.76, cause not investigated)
- Guard rows for this milestone: QCPAxis_SetRange10k (std::function in deferred emissions) and the Replot rows show no regression.

| Benchmark | baseline msecs | end msecs | run 1 / 2 / 3 | ratio |
|---|---|---|---|---|
| QCPGraph_Standard | 0.7 | 0.67 | 0.68 / 0.67 / 0.67 | 0.96 |
| QCPGraph_ManyPoints | 2.7 | 2.6 | 2.6 / 2.6 / 2.6 | 0.96 |
| QCPGraph_ManyLines | 1.3 | 1.3 | 1.3 / 1.3 / 1.3 | 1.00 |
| QCPGraph_ManyOffScreenLines | 0.15 | 0.15 | 0.15 / 0.15 / 0.15 | 1.00 |
| QCPGraph_RemoveDataBetween | 0.11 | 0.086 | 0.086 / 0.085 / 0.092 | 0.78 |
| QCPGraph_RemoveDataAfter | 0.00093 | 0.00079 | 0.00082 / 0.00079 / 0.00075 | 0.85 |
| QCPGraph_RemoveDataBefore | 0.33 | 0.17 | 0.17 / 0.27 / 0.17 | 0.52 |
| QCPGraph_AddDataAtEndSorted | 5.1 | 5.1 | 5.1 / 5.2 / 5 | 1.00 |
| QCPGraph_AddDataAtBeginSorted | 5.3 | 5.4 | 5.7 / 5.3 / 5.4 | 1.02 |
| QCPGraph_AddDataMixedSorted | 10 | 11 | 11 / 10 / 11 | 1.10 |
| QCPGraph_AddDataAtEndUnsorted | 23 | 23 | 23 / 23 / 23 | 1.00 |
| QCPGraph_AddDataAtBeginUnsorted | 28 | 28 | 28 / 28 / 28 | 1.00 |
| QCPGraph_AddDataMixedUnsorted | 29 | 29 | 29 / 29 / 29 | 1.00 |
| QCPGraph_AddDataSingleAtEnd | 0.024 | 0.024 | 0.024 / 0.024 / 0.024 | 1.00 |
| QCPGraph_AddDataSingleAtBegin | 0.045 | 0.045 | 0.045 / 0.045 / 0.045 | 1.00 |
| QCPGraph_AddDataSingleRandom | 5.1 | 5.1 | 5.1 / 5.1 / 5.1 | 1.00 |
| QCPGraph2_SetDataDouble | 9.4 | 9.5 | 9.5 / 9.4 / 9.7 | 1.01 |
| QCPGraph2_SetDataFloat | 8.4 | 8.1 | 8.1 / 8 / 8.1 | 0.96 |
| QCPGraph2_RenderDouble | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 | 1.00 |
| QCPGraph2_RenderFloat | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 | 1.00 |
| QCPGraph2_RenderManyLines | 0.44 | 0.44 | 0.44 / 0.44 / 0.44 | 1.00 |
| QCPGraph2_ScatterDisc1M | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGraph2_ScatterSquare1M | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGraph2_ScatterWithLine1M | 0.13 | 0.12 | 0.12 / 0.13 / 0.12 | 0.92 |
| QCPColorMap_Standard | 0.22 | 0.21 | 0.22 / 0.21 / 0.21 | 0.95 |
| QCPColorMap_ColorizeMap | 0.27 | 0.27 | 0.27 / 0.27 / 0.27 | 1.00 |
| QCPColorMap2_ResampleDouble | 227 | 2.3e+02 | 2.3e+02 / 2.2e+02 / 2.3e+02 | 1.00 |
| QCPColorMap2_ResampleFloat | 150 | 1.4e+02 | 1.4e+02 / 1.4e+02 / 1.4e+02 | 0.93 |
| QCPColorMapData_ConstructLarge | 1.3 | 1.1 | 1.1 / 1.1 / 1.4 | 0.85 |
| QCPColorMap2_ResamplePanSequence | 181 | 1.8e+02 | 1.8e+02 / 2.1e+02 / 1.7e+02 | 1.01 |
| QCPColorMap2_ResamplePanSequenceSmall | 206 | 2.1e+02 | 2.1e+02 / 2.1e+02 / 2e+02 | 1.00 |
| QCPColorMap2_ResampleWideSerial | 768 | 7.7e+02 | 7.7e+02 / 7.7e+02 / 7.7e+02 | 1.00 |
| QCPColorMap2_ResampleWideParallel | 263 | 2.7e+02 | 2.7e+02 / 2.6e+02 / 2.7e+02 | 1.02 |
| QCPAxis_TickLabels | 0.24 | 0.24 | 0.24 / 0.24 / 0.24 | 1.00 |
| QCPAxis_TickLabelsCached | 0.089 | 0.089 | 0.089 / 0.088 / 0.09 | 1.00 |
| QCPAxis_SetRange10k | 1.2 | 1.3 | 1.3 / 1.3 / 1.3 | 1.08 |
| QCPItems_AddRemove10k | 80 | 61 | 61 / 63 / 60 | 0.76 |
| QCPItems_Replot10k | 0.13 | 0.13 | 0.13 / 0.13 / 0.13 | 1.00 |
| QCPAxisRects_Stack8_Replot | 1.8 | 1.8 | 1.8 / 1.8 / 1.8 | 1.00 |
| QCPPlots_Stack8_Replot | 0.26 | 0.26 | 0.26 / 0.26 / 0.26 | 1.00 |

## Milestone 2 Task 1 re-measure (2026-11-03)

- QCPGraph_AddDataMixedUnsorted, after the Task 1 fix round, three sequential runs of `-median 5`
  for this row only: 29 / 29 / 29 ms. Load average at each start: 1.60, 1.56, 1.56 (1 min). That
  matches the baseline (29) and the end of milestone 1 (29). The 33 ms seen once in the Task 1
  report came from a loaded machine (load about 3.8).

## End of milestone 2

- Commit: 72945bb (neoqcp-3.0), same command and machine, three sequential runs
- Load average (1/5/15 min) at the start of each run: run 1: 1.40 / 1.33 / 1.67, run 2: 1.77 / 1.46 / 1.69, run 3: 1.63 / 1.47 / 1.68
- Ratio = end-of-milestone-2 median / end-of-milestone-1 median.
- Single-shot rows (QCPGraph_RemoveData*, QCPColorMapData_ConstructLarge) run once per pass and are noisy, as in milestone 1.
- One multi-iteration row outside the +-15% band: QCPItems_AddRemove10k (1.18, 72 vs 61 ms). The row is known to move 61-80 ms with memory layout (profiled in milestone 2 Task 2: 65 % of it is four O(n^2) list scans); it is the target of the separate item-scaling milestone.
- New rows since milestone 1: QCPAxis_SetTickLength10kItems (an axis appearance setter with 10k items: the repaint marks), QCPInput_MouseMoveStorm.
- Added after these runs: QCPColorMap_SetCell1M (a 1000x1000 cell-by-cell fill on a live plot) — 2.0 ms before colour map data could repaint, 8.2 ms with a mark per cell, 2.8 ms with one mark per draw.
- Guard rows for this milestone: QCPAxis_SetRange10k, the Replot rows and the setter rows show no regression from automatic repaint.

| Benchmark | milestone 1 msecs | end msecs | run 1 / 2 / 3 | ratio |
|---|---|---|---|---|
| QCPGraph_Standard | 0.67 | 0.67 | 0.67 / 0.67 / 0.66 | 1.00 |
| QCPGraph_ManyPoints | 2.6 | 2.6 | 2.6 / 2.6 / 2.6 | 1.00 |
| QCPGraph_ManyLines | 1.3 | 1.3 | 1.3 / 1.3 / 1.3 | 1.00 |
| QCPGraph_ManyOffScreenLines | 0.15 | 0.15 | 0.15 / 0.15 / 0.15 | 1.00 |
| QCPGraph_RemoveDataBetween | 0.086 | 0.089 | 0.089 / 0.095 / 0.089 | 1.03 |
| QCPGraph_RemoveDataAfter | 0.00079 | 0.00072 | 0.00072 / 0.00099 / 0.0007 | 0.91 |
| QCPGraph_RemoveDataBefore | 0.17 | 0.19 | 0.19 / 0.18 / 0.28 | 1.12 |
| QCPGraph_AddDataAtEndSorted | 5.1 | 5.4 | 5.4 / 5.7 / 5.3 | 1.06 |
| QCPGraph_AddDataAtBeginSorted | 5.4 | 5.5 | 5.4 / 5.8 / 5.5 | 1.02 |
| QCPGraph_AddDataMixedSorted | 11.0 | 10 | 10 / 10 / 10 | 0.91 |
| QCPGraph_AddDataAtEndUnsorted | 23.0 | 23 | 23 / 23 / 23 | 1.00 |
| QCPGraph_AddDataAtBeginUnsorted | 28.0 | 28 | 28 / 28 / 28 | 1.00 |
| QCPGraph_AddDataMixedUnsorted | 29.0 | 29 | 29 / 29 / 29 | 1.00 |
| QCPGraph_AddDataSingleAtEnd | 0.024 | 0.024 | 0.023 / 0.024 / 0.024 | 1.00 |
| QCPGraph_AddDataSingleAtBegin | 0.045 | 0.045 | 0.045 / 0.045 / 0.045 | 1.00 |
| QCPGraph_AddDataSingleRandom | 5.1 | 5 | 5 / 5.1 / 5 | 0.98 |
| QCPGraph2_SetDataDouble | 9.5 | 9.5 | 9.5 / 9.6 / 9.4 | 1.00 |
| QCPGraph2_SetDataFloat | 8.1 | 8.1 | 8.1 / 8.1 / 8.1 | 1.00 |
| QCPGraph2_RenderDouble | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 | 1.00 |
| QCPGraph2_RenderFloat | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 | 1.00 |
| QCPGraph2_RenderManyLines | 0.44 | 0.44 | 0.44 / 0.44 / 0.44 | 1.00 |
| QCPGraph2_ScatterDisc1M | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGraph2_ScatterSquare1M | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGraph2_ScatterWithLine1M | 0.12 | 0.12 | 0.13 / 0.12 / 0.12 | 1.00 |
| QCPColorMap_Standard | 0.21 | 0.22 | 0.22 / 0.22 / 0.21 | 1.05 |
| QCPColorMap_ColorizeMap | 0.27 | 0.27 | 0.27 / 0.27 / 0.27 | 1.00 |
| QCPColorMap2_ResampleDouble | 230.0 | 234 | 234 / 235 / 234 | 1.02 |
| QCPColorMap2_ResampleFloat | 140.0 | 132 | 132 / 133 / 132 | 0.94 |
| QCPColorMapData_ConstructLarge | 1.1 | 1.1 | 1.1 / 1.2 / 1.1 | 1.00 |
| QCPColorMap2_ResamplePanSequence | 180.0 | 167 | 166 / 167 / 173 | 0.93 |
| QCPColorMap2_ResamplePanSequenceSmall | 210.0 | 210 | 209 / 213 / 210 | 1.00 |
| QCPColorMap2_ResampleWideSerial | 770.0 | 772 | 782 / 772 / 760 | 1.00 |
| QCPColorMap2_ResampleWideParallel | 270.0 | 264 | 264 / 260 / 265 | 0.98 |
| QCPAxis_TickLabels | 0.24 | 0.24 | 0.24 / 0.25 / 0.24 | 1.00 |
| QCPAxis_TickLabelsCached | 0.089 | 0.089 | 0.089 / 0.089 / 0.087 | 1.00 |
| QCPAxis_SetRange10k | 1.3 | 1.3 | 1.3 / 1.3 / 1.3 | 1.00 |
| QCPAxis_SetTickLength10kItems | - | 15 | 16 / 15 / 15 | new |
| QCPItems_AddRemove10k | 61.0 | 72 | 78 / 72 / 72 | 1.18 |
| QCPItems_Replot10k | 0.13 | 0.13 | 0.13 / 0.13 / 0.13 | 1.00 |
| QCPAxisRects_Stack8_Replot | 1.8 | 1.8 | 1.8 / 1.8 / 1.8 | 1.00 |
| QCPPlots_Stack8_Replot | 0.26 | 0.26 | 0.26 / 0.26 / 0.26 | 1.00 |
| QCPInput_MouseMoveStorm | - | 3.7 | 3.7 / 3.9 / 3.7 | new |

## End of item scaling

- Commit: 49bc340 (neoqcp-3.0), same command and machine, three sequential runs
- Load average (1/5/15 min) at the start of each run: run 1: 1.85 / 3.35 / 3.31, run 2: 1.75 / 2.98 / 3.18, run 3: 1.60 / 2.68 / 3.06
- Ratio = end-of-item-scaling median / end-of-milestone-2 median.
- The whole binary, 56 rows, `QT_QPA_PLATFORM=offscreen qcp_benchmark -median 5`, nothing else running.
- Single-shot rows (QCPGraph_RemoveData*, QCPColorMapData_ConstructLarge, the delete rows that run once per pass) are noisy, as in the earlier sections.
- Rows outside the +-15% band (four):
  - QCPItems_AddRemove10k 72 -> 28 ms (0.39): the point of this milestone. `registerItem`'s duplicate test is a pointer comparison, `clearItems` takes the items out of their layers in one pass and deletes them oldest first so Qt finds each at the front, and the item constructor no longer walks the layout for the whole axis-rect list (Tasks 2, docs/plans/neoqcp-3/item-scaling-profile.md).
  - QCPItems_Replot10k 0.13 -> 1.2 ms (9.23): not a regression in work done, but in work priced. Before 770b1eb an item-only layer was never redrawn on a range change, so the row timed an empty draw; items are now really redrawn, and Task 4's view culling brings the honest number to 1.2 ms (was 4.5 ms right after the fix).
  - QCPColorMap_ColorizeMap 0.27 -> 0.16 ms (0.59): not investigated. The row builds its colour map outside the measurement and times `setCell` + a synchronous `replot()`; the improvement is consistent across the three runs (0.16 / 0.16 / 0.16) but I did not bisect it. Checked that no work is skipped: gdb counts 11 `updateMapImage` and 11 `QCPColorMap::draw` calls for 10 iterations plus the warm-up.
  - QCPGraph_RemoveDataBetween 0.089 -> 0.11 ms (1.24): not investigated beyond noise: it is a single-shot row (0.076 / 0.14 / 0.076 in the three passes) with no code from this milestone on its path.
- New rows: the item-scaling benchmark set (QCPItems_Add10k, Clear10k, DeleteEach10k, RemoveEach10k, AddRemove20k, DeletePlot10k, Zoom10k, Zoom100, ZoomEmpty, Click10k, Click100, ClickEmpty10k, QObject_DeleteEachChild10k) and QCPColorMap_SetCell1M, which was added after the milestone-2 runs.
- Guard rows: QCPAxis_SetRange10k, QCPItems_AddRemove10k's neighbours, the Replot rows and the setter rows show no regression from the appearance-marking work; QCPAxis_SetTickLength10kItems is unchanged at 15 ms.

| Benchmark | milestone 2 msecs | end msecs | run 1 / 2 / 3 | ratio |
|---|---|---|---|---|
| QCPAxisRects_Stack8_Replot | 1.8 | 1.8 | 1.8 / 1.8 / 1.8 | 1.00 |
| QCPAxis_SetRange10k | 1.3 | 1.4 | 1.5 / 1.4 / 1.4 | 1.08 |
| QCPAxis_SetTickLength10kItems | 15 | 15 | 16 / 15 / 15 | 1.00 |
| QCPAxis_TickLabels | 0.24 | 0.24 | 0.25 / 0.24 / 0.24 | 1.00 |
| QCPAxis_TickLabelsCached | 0.089 | 0.089 | 0.088 / 0.09 / 0.089 | 1.00 |
| QCPColorMap2_ResampleDouble | 234 | 221 | 215 / 221 / 226 | 0.94 |
| QCPColorMap2_ResampleFloat | 132 | 131 | 128 / 137 / 131 | 0.99 |
| QCPColorMap2_ResamplePanSequence | 167 | 177 | 207 / 158 / 177 | 1.06 |
| QCPColorMap2_ResamplePanSequenceSmall | 210 | 205 | 205 / 205 / 206 | 0.98 |
| QCPColorMap2_ResampleWideParallel | 264 | 256 | 256 / 255 / 265 | 0.97 |
| QCPColorMap2_ResampleWideSerial | 772 | 757 | 754 / 769 / 757 | 0.98 |
| QCPColorMapData_ConstructLarge | 1.1 | 1.2 | 1.1 / 1.3 / 1.2 | 1.09 |
| QCPColorMap_ColorizeMap | 0.27 | 0.16 | 0.16 / 0.16 / 0.16 | 0.59 |
| QCPColorMap_SetCell1M | - | 3.2 | 3.2 / 3.2 / 3.2 | new |
| QCPColorMap_Standard | 0.22 | 0.22 | 0.22 / 0.22 / 0.22 | 1.00 |
| QCPGraph2_RenderDouble | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 | 1.00 |
| QCPGraph2_RenderFloat | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 | 1.00 |
| QCPGraph2_RenderManyLines | 0.44 | 0.44 | 0.44 / 0.45 / 0.44 | 1.00 |
| QCPGraph2_ScatterDisc1M | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGraph2_ScatterSquare1M | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGraph2_ScatterWithLine1M | 0.12 | 0.13 | 0.13 / 0.12 / 0.13 | 1.08 |
| QCPGraph2_SetDataDouble | 9.5 | 9.5 | 9.5 / 9.7 / 9.3 | 1.00 |
| QCPGraph2_SetDataFloat | 8.1 | 8.2 | 8.4 / 8.2 / 8.1 | 1.01 |
| QCPGraph_AddDataAtBeginSorted | 5.5 | 5.5 | 5.2 / 5.5 / 5.7 | 1.00 |
| QCPGraph_AddDataAtBeginUnsorted | 28 | 28 | 28 / 27 / 28 | 1.00 |
| QCPGraph_AddDataAtEndSorted | 5.4 | 4.9 | 4.9 / 4.8 / 5.2 | 0.91 |
| QCPGraph_AddDataAtEndUnsorted | 23 | 22 | 22 / 22 / 23 | 0.96 |
| QCPGraph_AddDataMixedSorted | 10 | 10 | 10 / 10 / 11 | 1.00 |
| QCPGraph_AddDataMixedUnsorted | 29 | 29 | 29 / 28 / 29 | 1.00 |
| QCPGraph_AddDataSingleAtBegin | 0.045 | 0.045 | 0.045 / 0.048 / 0.045 | 1.00 |
| QCPGraph_AddDataSingleAtEnd | 0.024 | 0.024 | 0.024 / 0.024 / 0.024 | 1.00 |
| QCPGraph_AddDataSingleRandom | 5 | 5 | 5 / 5 / 5 | 1.00 |
| QCPGraph_ManyLines | 1.3 | 1.3 | 1.3 / 1.3 / 1.2 | 1.00 |
| QCPGraph_ManyOffScreenLines | 0.15 | 0.15 | 0.15 / 0.15 / 0.15 | 1.00 |
| QCPGraph_ManyPoints | 2.6 | 2.6 | 2.6 / 2.6 / 2.6 | 1.00 |
| QCPGraph_RemoveDataAfter | 0.00072 | 0.00068 | 0.001 / 0.00068 / 0.00064 | 0.94 |
| QCPGraph_RemoveDataBefore | 0.19 | 0.17 | 0.24 / 0.17 / 0.16 | 0.89 |
| QCPGraph_RemoveDataBetween | 0.089 | 0.11 | 0.14 / 0.11 / 0.076 | 1.24 |
| QCPGraph_Standard | 0.67 | 0.71 | 0.71 / 0.72 / 0.66 | 1.06 |
| QCPInput_MouseMoveStorm | 3.7 | 3.8 | 3.8 / 3.9 / 3.7 | 1.03 |
| QCPItems_Add10k | - | 18 | 18 / 18 / 19 | new |
| QCPItems_AddRemove10k | 72 | 28 | 27 / 28 / 28 | 0.39 |
| QCPItems_AddRemove20k | - | 64 | 65 / 62 / 64 | new |
| QCPItems_Clear10k | - | 8.3 | 8.6 / 8.3 / 8.3 | new |
| QCPItems_Click100 | - | 0.014 | 0.013 / 0.014 / 0.014 | new |
| QCPItems_Click10k | - | 1.2 | 1.2 / 1.2 / 1.2 | new |
| QCPItems_ClickEmpty10k | - | 0.29 | 0.29 / 0.29 / 0.29 | new |
| QCPItems_DeleteEach10k | - | 24 | 27 / 24 / 24 | new |
| QCPItems_DeletePlot10k | - | 25 | 27 / 25 / 25 | new |
| QCPItems_RemoveEach10k | - | 25 | 32 / 25 / 25 | new |
| QCPItems_Replot10k | 0.13 | 1.2 | 1.2 / 1.2 / 1.2 | 9.23 |
| QCPItems_Zoom100 | - | 0.41 | 0.41 / 0.41 / 0.4 | new |
| QCPItems_Zoom10k | - | 1.2 | 1.2 / 1.2 / 1.2 | new |
| QCPItems_ZoomEmpty | - | 0.14 | 0.14 / 0.15 / 0.14 | new |
| QCPPlots_Stack8_Replot | 0.26 | 0.26 | 0.26 / 0.26 / 0.26 | 1.00 |
| QObject_DeleteEachChild10k | - | 7.8 | 8.3 / 7.8 / 7.7 | new |

## End of item index

- Commit: 1ac1ce5 (neoqcp-3.0), same command and machine, three sequential runs.
- Load average (1/5/15 min) at the start of each run: run 1: 2.99 / 3.64 / 3.48, run 2: 2.95 / 3.53 / 3.46, run 3: 1.66 / 2.97 / 3.26.
- Ratio = end-of-item-index median / end-of-item-scaling median.
- Plan targets: Zoom10k 0.42 <= 1.3 x Zoom100 (0.52), met. Click10k 0.052 and ClickEmpty10k 0.049 against 0.05, met within noise. DragOne10k 0.54 <= 1.5 x Zoom100 (0.60), met. FirstFrame10k - Add10k - Clear10k = 28 - 19 - 8.8, about 0 (target 2 ms), met: the row also clears its items.
- Rows outside the +-15% band:
  - QCPItems_Zoom10k 1.2 -> 0.42, Replot10k 1.2 -> 0.41, Click10k 1.2 -> 0.052, ClickEmpty10k 0.29 -> 0.049, Click100 0.014 -> 0.0036, QCPAxis_SetTickLength10kItems 15 -> 0.012: the point of this plan (index, click query, deselect pass, per-layer axes, per-layer plottable list).
  - QCPItems_RemoveEach10k 25 -> 32 (1.28): same work, slower placement. Bisected to 67ab0e1, which only edits code this row never runs. The time is in `QList::indexOf` over the item list (source and instructions unchanged); `perf stat` gives the same instruction count (1.774e9 vs 1.773e9), the same cache and branch misses, and 10% more cycles. A code-alignment effect; not chased.
  - QCPGraph_RemoveDataAfter 0.00068 -> 0.00091: a sub-microsecond single-shot row (0.00091 / 0.0012 / 0.00088), no code from this plan on its path.
- New rows: QCPSpatialIndex_Build10k, QCPSpatialIndex_Query10k, QCPItems_FirstFrame10k, DragOne10k, Pan10k, RestyleAll10k.

| Benchmark | item scaling msecs | end msecs | run 1 / 2 / 3 | ratio |
|---|---|---|---|---|
| QCPAxisRects_Stack8_Replot | 1.8 | 1.8 | 1.8 / 1.8 / 1.8 | 1.00 |
| QCPAxis_SetRange10k | 1.4 | 1.4 | 1.4 / 1.4 / 1.4 | 1.00 |
| QCPAxis_SetTickLength10kItems | 15 | 0.012 | 0.012 / 0.012 / 0.011 | 0.00 |
| QCPAxis_TickLabels | 0.24 | 0.24 | 0.24 / 0.24 / 0.24 | 1.00 |
| QCPAxis_TickLabelsCached | 0.089 | 0.088 | 0.092 / 0.088 / 0.087 | 0.99 |
| QCPColorMap2_ResampleDouble | 221 | 222 | 229 / 222 / 219 | 1.00 |
| QCPColorMap2_ResampleFloat | 131 | 133 | 129 / 135 / 133 | 1.02 |
| QCPColorMap2_ResamplePanSequence | 177 | 188 | 188 / 216 / 179 | 1.06 |
| QCPColorMap2_ResamplePanSequenceSmall | 205 | 207 | 207 / 203 / 207 | 1.01 |
| QCPColorMap2_ResampleWideParallel | 256 | 269 | 269 / 259 / 275 | 1.05 |
| QCPColorMap2_ResampleWideSerial | 757 | 766 | 768 / 766 / 766 | 1.01 |
| QCPColorMapData_ConstructLarge | 1.2 | 1.1 | 1.1 / 1.1 / 1.1 | 0.92 |
| QCPColorMap_ColorizeMap | 0.16 | 0.16 | 0.16 / 0.16 / 0.16 | 1.00 |
| QCPColorMap_SetCell1M | 3.2 | 3 | 3 / 3 / 3.1 | 0.94 |
| QCPColorMap_Standard | 0.22 | 0.21 | 0.21 / 0.21 / 0.22 | 0.95 |
| QCPGraph2_RenderDouble | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 | 1.00 |
| QCPGraph2_RenderFloat | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 | 1.00 |
| QCPGraph2_RenderManyLines | 0.44 | 0.44 | 0.44 / 0.44 / 0.44 | 1.00 |
| QCPGraph2_ScatterDisc1M | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGraph2_ScatterSquare1M | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGraph2_ScatterWithLine1M | 0.13 | 0.12 | 0.12 / 0.12 / 0.13 | 0.92 |
| QCPGraph2_SetDataDouble | 9.5 | 9.4 | 9.4 / 9.4 / 9.5 | 0.99 |
| QCPGraph2_SetDataFloat | 8.2 | 8.5 | 8.5 / 8.1 / 8.6 | 1.04 |
| QCPGraph_AddDataAtBeginSorted | 5.5 | 5.3 | 5.2 / 6 / 5.3 | 0.96 |
| QCPGraph_AddDataAtBeginUnsorted | 28 | 28 | 28 / 28 / 28 | 1.00 |
| QCPGraph_AddDataAtEndSorted | 4.9 | 5 | 5 / 5.3 / 5 | 1.02 |
| QCPGraph_AddDataAtEndUnsorted | 22 | 23 | 24 / 23 / 23 | 1.05 |
| QCPGraph_AddDataMixedSorted | 10 | 10 | 10 / 11 / 10 | 1.00 |
| QCPGraph_AddDataMixedUnsorted | 29 | 29 | 29 / 29 / 29 | 1.00 |
| QCPGraph_AddDataSingleAtBegin | 0.045 | 0.045 | 0.045 / 0.045 / 0.045 | 1.00 |
| QCPGraph_AddDataSingleAtEnd | 0.024 | 0.024 | 0.024 / 0.024 / 0.024 | 1.00 |
| QCPGraph_AddDataSingleRandom | 5 | 5.2 | 5.2 / 5.1 / 5.2 | 1.04 |
| QCPGraph_ManyLines | 1.3 | 1.3 | 1.3 / 1.3 / 1.3 | 1.00 |
| QCPGraph_ManyOffScreenLines | 0.15 | 0.15 | 0.15 / 0.15 / 0.15 | 1.00 |
| QCPGraph_ManyPoints | 2.6 | 2.6 | 2.6 / 2.6 / 2.6 | 1.00 |
| QCPGraph_RemoveDataAfter | 0.00068 | 0.00091 | 0.00091 / 0.0012 / 0.00088 | 1.34 |
| QCPGraph_RemoveDataBefore | 0.17 | 0.16 | 0.16 / 0.3 / 0.16 | 0.94 |
| QCPGraph_RemoveDataBetween | 0.11 | 0.1 | 0.081 / 0.13 / 0.1 | 0.91 |
| QCPGraph_Standard | 0.71 | 0.67 | 0.66 / 0.67 / 0.69 | 0.94 |
| QCPInput_MouseMoveStorm | 3.8 | 4.1 | 4.1 / 4.1 / 3.7 | 1.08 |
| QCPItems_Add10k | 18 | 19 | 19 / 18 / 19 | 1.06 |
| QCPItems_AddRemove10k | 28 | 27 | 27 / 27 / 28 | 0.96 |
| QCPItems_AddRemove20k | 64 | 64 | 64 / 64 / 66 | 1.00 |
| QCPItems_Clear10k | 8.3 | 8.8 | 8.8 / 8.2 / 9.5 | 1.06 |
| QCPItems_Click100 | 0.014 | 0.0036 | 0.0036 / 0.0037 / 0.0036 | 0.26 |
| QCPItems_Click10k | 1.2 | 0.052 | 0.052 / 0.052 / 0.051 | 0.04 |
| QCPItems_ClickEmpty10k | 0.29 | 0.049 | 0.049 / 0.05 / 0.049 | 0.17 |
| QCPItems_DeleteEach10k | 24 | 25 | 27 / 25 / 25 | 1.04 |
| QCPItems_DeletePlot10k | 25 | 27 | 27 / 27 / 26 | 1.08 |
| QCPItems_DragOne10k | - | 0.54 | 0.56 / 0.54 / 0.54 | new |
| QCPItems_FirstFrame10k | - | 28 | 29 / 28 / 28 | new |
| QCPItems_Pan10k | - | 0.41 | 0.42 / 0.41 / 0.41 | new |
| QCPItems_RemoveEach10k | 25 | 32 | 32 / 31 / 32 | 1.28 |
| QCPItems_Replot10k | 1.2 | 0.41 | 0.43 / 0.41 / 0.41 | 0.34 |
| QCPItems_RestyleAll10k | - | 1.5 | 1.6 / 1.4 / 1.5 | new |
| QCPItems_Zoom100 | 0.41 | 0.4 | 0.4 / 0.4 / 0.4 | 0.98 |
| QCPItems_Zoom10k | 1.2 | 0.42 | 0.43 / 0.42 / 0.42 | 0.35 |
| QCPItems_ZoomEmpty | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 | 1.00 |
| QCPPlots_Stack8_Replot | 0.26 | 0.26 | 0.26 / 0.26 / 0.26 | 1.00 |
| QCPSpatialIndex_Build10k | - | 0.88 | 0.89 / 0.88 / 0.88 | new |
| QCPSpatialIndex_Query10k | - | 0.00036 | 0.00036 / 0.00037 / 0.00036 | new |
| QObject_DeleteEachChild10k | 7.8 | 7.8 | 8 / 7.8 / 7.8 | 1.00 |

## End of milestone 3

- Commit: ba3bb01 (neoqcp-3.0), same command and machine as above, three sequential runs of the whole binary
  (70 rows; the 10 GPU rows skip here, they run under Xvfb below).
- Load average (1/5/15 min) at the start of each run: run 1: 1.04 / 1.80 / 2.07, run 2: 0.95 / 1.61 / 1.97, run 3: 1.96 / 2.24 / 2.18
- Ratio = end-of-milestone-3 median / baseline median (the baseline table at the top of this file).
- The drawing API did not change the data rows: everything on the graph, colour map, axis and item paths reads
  what it read at the end of the item index, inside the +-15 % band.
- Rows outside the +-15 % band, slower:
  - QCPItems_Replot10k 0.13 -> 0.42 (3.23): not a milestone 3 change. Before item scaling the row timed an
    empty draw (an item-only layer was never redrawn); it read 0.41 at the end of the item index and reads 0.42
    here. Three extra runs: 0.43 / 0.43 / 0.43.
  - QCPAxis_SetRange10k 1.2 -> 1.5 (1.25): the guard row, and the only row of this milestone outside the band
    that is not already explained. Three extra runs: 1.4 / 1.4 / 1.4 (median 1.4, +17 %). It read 1.4 at the end
    of the item index, so the creep is older than milestone 3: 1.2 baseline, 1.3 at the end of milestone 1 and 2,
    1.4 at the end of the item index. Not chased (this task changes no code).
  - QCPGraph_RemoveDataAfter 0.00093 -> 0.0012 (1.29): a sub-microsecond single-shot row, 0.0015 / 0.0012 /
    0.0011 in the three runs. Three extra runs: 0.0016 / 0.0012 / 0.0012. Noise, as in the earlier sections.
- Rows outside the band, faster:
  - QCPItems_AddRemove10k 80 -> 28 (0.35): the item-scaling milestone, unchanged here (27 / 28 / 28 at the end
    of the item index).
  - QCPGraph_RemoveDataBefore 0.33 -> 0.19 (0.58): single-shot row, 0.19 / 0.29 / 0.19 in the three passes.
  - QCPColorMap_ColorizeMap 0.27 -> 0.16 (0.59): unchanged since the end of item scaling, not investigated
    there either.
- New rows since the baseline: QCPCoords_CoordToPixel1M and QCPTransform_Map1M (the batch map is 8x the
  coordToPixel loop), QCPDrawContext_ReplayData1M and QCPDrawContext_ReplayMarkers100k,
  QCPInput_WheelOverGraph2_1M, QCPBars_Zoom10k, QCPBars_Pan10k and QCPItemText_Zoom1k. The item-scaling and
  item-index rows were new at their own milestones.
- Guard rows for this milestone: QCPAxis_SetRange10k (above), the drawn-item rows QCPItems_Zoom10k 0.43,
  QCPItems_Pan10k 0.41, QCPItems_Click10k 0.053 and QCPItems_Add10k 20 ms, QCPItems_Replot10k (above),
  QCPAxis_SetTickLength10kItems 0.012, QCPInput_MouseMoveStorm 3.9, and the two Stack8_Replot rows show no
  regression from the recording and replay path.

| Benchmark | baseline msecs | end msecs | run 1 / 2 / 3 | ratio |
|---|---|---|---|---|
| QCPAxisRects_Stack8_Replot | 1.8 | 1.9 | 1.9 / 1.9 / 1.9 | 1.06 |
| QCPAxis_SetRange10k | 1.2 | 1.5 | 1.4 / 1.5 / 1.5 | 1.25 |
| QCPAxis_SetTickLength10kItems | - | 0.012 | 0.011 / 0.013 / 0.012 | new |
| QCPAxis_TickLabels | 0.24 | 0.25 | 0.24 / 0.25 / 0.25 | 1.04 |
| QCPAxis_TickLabelsCached | 0.089 | 0.088 | 0.088 / 0.091 / 0.088 | 0.99 |
| QCPBars_Pan10k | - | 5.6 | 5.5 / 5.6 / 5.6 | new |
| QCPBars_Zoom10k | - | 5.4 | 5.4 / 5.8 / 5.2 | new |
| QCPColorMap2_ResampleDouble | 227 | 228 | 229 / 223 / 228 | 1.00 |
| QCPColorMap2_ResampleFloat | 150 | 142 | 134 / 142 / 156 | 0.95 |
| QCPColorMap2_ResamplePanSequence | 181 | 207 | 176 / 207 / 252 | 1.14 |
| QCPColorMap2_ResamplePanSequenceSmall | 206 | 211 | 211 / 206 / 316 | 1.02 |
| QCPColorMap2_ResampleWideParallel | 263 | 283 | 283 / 318 / 279 | 1.08 |
| QCPColorMap2_ResampleWideSerial | 768 | 784 | 784 / 777 / 808 | 1.02 |
| QCPColorMapData_ConstructLarge | 1.3 | 1.2 | 1.2 / 1.1 / 1.2 | 0.92 |
| QCPColorMap_ColorizeMap | 0.27 | 0.16 | 0.16 / 0.16 / 0.16 | 0.59 |
| QCPColorMap_SetCell1M | - | 3 | 3 / 3 / 3.2 | new |
| QCPColorMap_Standard | 0.22 | 0.22 | 0.21 / 0.22 / 0.35 | 1.00 |
| QCPCoords_CoordToPixel1M | - | 5.6 | 5.6 / 5.8 / 5.6 | new |
| QCPDrawContext_ReplayData1M | - | 1.3 | 1.3 / 1.4 / 1.3 | new |
| QCPDrawContext_ReplayMarkers100k | - | 13 | 13 / 13 / 13 | new |
| QCPGraph2_RenderDouble | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 | 1.00 |
| QCPGraph2_RenderFloat | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 | 1.00 |
| QCPGraph2_RenderManyLines | 0.44 | 0.44 | 0.44 / 0.44 / 0.45 | 1.00 |
| QCPGraph2_ScatterDisc1M | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGraph2_ScatterSquare1M | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGraph2_ScatterWithLine1M | 0.13 | 0.13 | 0.13 / 0.12 / 0.13 | 1.00 |
| QCPGraph2_SetDataDouble | 9.4 | 9.7 | 9.9 / 9.7 / 9.7 | 1.03 |
| QCPGraph2_SetDataFloat | 8.4 | 8.4 | 8.3 / 8.6 / 8.4 | 1.00 |
| QCPGraph_AddDataAtBeginSorted | 5.3 | 5.4 | 5.4 / 5.6 / 5.4 | 1.02 |
| QCPGraph_AddDataAtBeginUnsorted | 28 | 28 | 28 / 30 / 28 | 1.00 |
| QCPGraph_AddDataAtEndSorted | 5.1 | 5.2 | 5.1 / 5.2 / 5.3 | 1.02 |
| QCPGraph_AddDataAtEndUnsorted | 23 | 23 | 23 / 25 / 23 | 1.00 |
| QCPGraph_AddDataMixedSorted | 10 | 10 | 10 / 13 / 10 | 1.00 |
| QCPGraph_AddDataMixedUnsorted | 29 | 30 | 30 / 31 / 29 | 1.03 |
| QCPGraph_AddDataSingleAtBegin | 0.045 | 0.047 | 0.047 / 0.048 / 0.045 | 1.04 |
| QCPGraph_AddDataSingleAtEnd | 0.024 | 0.024 | 0.024 / 0.031 / 0.024 | 1.00 |
| QCPGraph_AddDataSingleRandom | 5.1 | 5.1 | 5.1 / 6.5 / 5 | 1.00 |
| QCPGraph_ManyLines | 1.3 | 1.3 | 1.3 / 1.3 / 1.3 | 1.00 |
| QCPGraph_ManyOffScreenLines | 0.15 | 0.15 | 0.198 / 0.15 / 0.15 | 1.00 |
| QCPGraph_ManyPoints | 2.7 | 2.7 | 2.7 / 2.8 / 2.6 | 1.00 |
| QCPGraph_RemoveDataAfter | 0.00093 | 0.0012 | 0.0015 / 0.0012 / 0.0011 | 1.29 |
| QCPGraph_RemoveDataBefore | 0.33 | 0.19 | 0.19 / 0.29 / 0.19 | 0.58 |
| QCPGraph_RemoveDataBetween | 0.11 | 0.11 | 0.11 / 0.12 / 0.091 | 1.00 |
| QCPGraph_Standard | 0.7 | 0.7 | 0.7 / 0.7 / 0.67 | 1.00 |
| QCPInput_MouseMoveStorm | - | 3.9 | 3.9 / 3.9 / 3.9 | new |
| QCPInput_WheelOverGraph2_1M | - | 0.00067 | 0.00066 / 0.00067 / 0.00067 | new |
| QCPItemText_Zoom1k | - | 0.85 | 0.85 / 0.85 / 0.88 | new |
| QCPItems_Add10k | - | 20 | 20 / 20 / 20 | new |
| QCPItems_AddRemove10k | 80 | 28 | 28 / 29 / 27 | 0.35 |
| QCPItems_AddRemove20k | - | 66 | 66 / 68 / 66 | new |
| QCPItems_Clear10k | - | 8.3 | 8.3 / 9 / 8.3 | new |
| QCPItems_Click100 | - | 0.0038 | 0.0038 / 0.004 / 0.0038 | new |
| QCPItems_Click10k | - | 0.053 | 0.054 / 0.053 / 0.052 | new |
| QCPItems_ClickEmpty10k | - | 0.05 | 0.05 / 0.056 / 0.05 | new |
| QCPItems_DeleteEach10k | - | 27 | 27 / 29 / 26 | new |
| QCPItems_DeletePlot10k | - | 27 | 27 / 29 / 27 | new |
| QCPItems_DragOne10k | - | 0.55 | 0.55 / 0.56 / 0.55 | new |
| QCPItems_FirstFrame10k | - | 30 | 30 / 30 / 30 | new |
| QCPItems_Pan10k | - | 0.41 | 0.41 / 0.45 / 0.41 | new |
| QCPItems_RemoveEach10k | - | 33 | 33 / 34 / 33 | new |
| QCPItems_Replot10k | 0.13 | 0.42 | 0.42 / 0.43 / 0.42 | 3.23 |
| QCPItems_RestyleAll10k | - | 1.6 | 1.6 / 1.6 / 1.5 | new |
| QCPItems_Zoom100 | - | 0.41 | 0.41 / 0.41 / 0.4 | new |
| QCPItems_Zoom10k | - | 0.43 | 0.42 / 0.43 / 0.43 | new |
| QCPItems_ZoomEmpty | - | 0.14 | 0.14 / 0.14 / 0.14 | new |
| QCPPlots_Stack8_Replot | 0.26 | 0.27 | 0.27 / 0.26 / 0.27 | 1.04 |
| QCPSpatialIndex_Build10k | - | 0.89 | 0.89 / 0.89 / 0.89 | new |
| QCPSpatialIndex_Query10k | - | 0.00036 | 0.00036 / 0.00037 / 0.00036 | new |
| QCPTransform_Map1M | - | 0.71 | 0.66 / 0.73 / 0.71 | new |
| QObject_DeleteEachChild10k | - | 7.9 | 7.9 / 7.9 / 7.9 | new |

### GPU rows (Xvfb, llvmpipe: CPU side only)

- Commit: ba3bb01 (neoqcp-3.0), `xvfb-run -a -s "-screen 0 1600x1200x24" env QT_QPA_PLATFORM=xcb
  build-qt612/tests/benchmark/qcp_benchmark -median 5 <rows>`, the same renderer (llvmpipe) and the same
  caveat as `m3-gpu-baseline.md`: these are the CPU msecs of one `replot()`, not GPU time.
- Load average (1/5/15 min) at the start of each run: run 1: 1.62 / 2.04 / 2.13, run 2: 1.54 / 1.95 / 2.09,
  run 3: 2.33 / 2.17 / 2.16.
- Ratio = end-of-milestone-3 median / the number in `m3-gpu-baseline.md` (the milestone 3 start, 2eac8a2).
- The bars rows are the point of the milestone: `QCPGpu_Bars10k` 8.8 -> 5.5 ms (0.63) and `QCPGpu_Text1k`
  2.5 -> 1.0 ms (0.40) now that both classes record commands instead of painting bar by bar.
- `QCPGpu_Bars10k` submits 1 draw call for 10 000 bars where it submitted 20 000 (20 000 solid entries to 1,
  measured with the temporary patch `build-qt612/oc-scratch/drawcalls.patch`, task-9b-report.md). llvmpipe does
  not price draw calls, so the row keeps paying for building the 20 000 rect vertices; a real driver should
  gain more than these 3.3 ms.
- The dashed row no longer takes the QPainter branch: `QCPGpu_LineDashed100k` 0.45 against `QCPGpu_LineSolid100k`
  0.45, 1.05x its own start (0.43) and the same cost as the solid row.
- Every other row is flat: LineSolid100k 1.02, Pan100k 0.96, Graph2Render1M 1.00, MixedLayer 1.00.
- The `QCPDrawn_*` rows did not exist at the milestone 3 start: QCPDrawn_Zoom10k 0.30, QCPDrawn_Pan10k 0.25,
  QCPDrawn_Pan10kOneRecording 0.32 (10 000 bars recorded as one object; the whole-recording pan cache).
- The three offscreen drawing rows of that same baseline, for completeness: QCPBars_Zoom10k 4.8 -> 5.4
  (+12.5 %, inside the gate), QCPBars_Pan10k 5.2 -> 5.6 (+7.7 %), QCPItemText_Zoom1k 2.5 -> 0.85 (0.34).

| row | milestone 3 start msecs | end msecs | run 1 / 2 / 3 | ratio |
|---|---|---|---|---|
| QCPGpu_LineSolid100k | 0.44 | 0.45 | 0.45 / 0.44 / 0.45 | 1.02 |
| QCPGpu_LineDashed100k | 0.43 | 0.45 | 0.45 / 0.44 / 0.47 | 1.05 |
| QCPGpu_Pan100k | 0.23 | 0.22 | 0.22 / 0.22 / 0.22 | 0.96 |
| QCPGpu_Graph2Render1M | 0.13 | 0.13 | 0.13 / 0.13 / 0.12 | 1.00 |
| QCPGpu_Bars10k | 8.8 | 5.5 | 5.5 / 5.5 / 5.5 | 0.62 |
| QCPGpu_Text1k | 2.5 | 1 | 1 / 1 / 1 | 0.40 |
| QCPGpu_MixedLayer | 8.7 | 8.7 | 8.7 / 8.5 / 9.1 | 1.00 |
| QCPDrawn_Zoom10k | - | 0.3 | 0.3 / 0.3 / 0.3 | new |
| QCPDrawn_Pan10k | - | 0.25 | 0.25 / 0.24 / 0.25 | new |
| QCPDrawn_Pan10kOneRecording | - | 0.32 | 0.32 / 0.31 / 0.34 | new |

## End of milestone 4a

- Commit: 7ed486f (neoqcp-3.0), same command and machine as above, three sequential runs of the whole binary
  (70 rows; the 11 GPU rows skip here, they run under Xvfb below).
- Load average (1/5/15 min) at the start of each run: run 1: 1.84 / 3.14 / 2.74, run 2: 4.71 / 3.41 / 2.87,
  run 3: 1.29 / 2.62 / 2.67. Another session's cargo test loop (access_matrix) ran during the window, which is
  why run 2 starts at 4.71; its rows sit inside the spread of runs 1 and 3, so the series was not repeated.
- Ratio = end-of-milestone-4a median / end-of-milestone-3 median: the M4 design takes the end of milestone 3 as
  the baseline for all of M4. Gate: 5 % for QCPGpu_*, QCPDrawn_*, QCPItems_* and QCPBars_*, 15 % for the rest.
  The ratio column below is the three-run median; the six rows that came out over gate were run six more times
  and the median of those six decides, as listed here.
- Rows over gate in the three runs:
  - QCPBars_Zoom10k 5.4 -> 5.8 (1.07, gate 5 %): the three runs (7.6 / 5.8 / 5.2) straddle the gate. Six more:
    5.5 / 5.2 / 5.2 / 5.2 / 5.6 / 5.3, median 5.25 (0.97). Inside.
  - QCPGraph_RemoveDataBefore 0.19 -> 0.22 (1.16, gate 15 %): six more: 0.18 / 0.28 / 0.18 / 0.2 / 0.22 / 0.31,
    median 0.21 (1.11). Inside. Single-shot sub-millisecond row, as noisy here as in the earlier sections.
  - QCPItems_Clear10k 8.3 -> 8.9 (1.07, gate 5 %): six more: 8.4 / 8.2 / 8.7 / 8.7 / 8.3 / 8.1, median 8.35
    (1.01). Inside.
  - QCPItems_Click100 0.0038 -> 0.0042 (1.11, gate 5 %): six more: 0.0041 / 0.0043 / 0.0044 / 0.0041 / 0.0042 /
    0.0041, median 0.00415 (1.09). Still outside, by 0.35 us per click on a 4.15 us row. Not chased (this task
    changes no code); its siblings QCPItems_Click10k 0.052 (0.98) and QCPItems_ClickEmpty10k 0.05 (1.00) are flat,
    so the 10 000 item hit path did not move.
  - QCPItems_RemoveEach10k 33 -> 36 (1.09, gate 5 %): six more: 32 / 30 / 30 / 30 / 33 / 30, median 30 (0.91).
    Inside.
  - QCPGpu_Text1k 1 -> 1.1 (1.10, gate 5 %, GPU table below): six more under Xvfb: 1.1 / 1.0 / 1.0 / 1.0 / 1.0 /
    1.1, median 1.0 (1.00). Inside.
  - A first batch of six extra runs was discarded: the cargo loop pushed the 1 min load average to 14 and inflated
    every row (QCPItems_Clear10k read 87 ms). The six runs above were taken at a 1 min load of 2.2-2.3.
- No row of milestone 4a is slower than the milestone 3 number beyond these; nothing was changed to chase a row.
- Notably faster: QCPCoords_CoordToPixel1M 5.6 -> 4.8 (0.86), QCPAxis_SetRange10k 1.5 -> 1.3 (0.87),
  QCPTransform_Map1M 0.71 -> 0.63 (0.89), QCPGpu_Bars10k 5.5 -> 4.9 (0.89), QCPGpu_MixedLayer 8.7 -> 8 (0.92),
  QCPGraph2_ScatterWithLine1M 0.13 -> 0.12 (0.92), QCPDrawContext_ReplayMarkers100k 13 -> 12 (0.92).
- New row since milestone 3: QCPGpu_BarsPan10k 0.12 (10 000 GPU bars panned once, added with 5fbf9fc).
- Guard rows for this milestone, all inside their 5 % gate: QCPItems_Replot10k 0.42 (1.00), QCPItems_Zoom10k
  0.43 (1.00), QCPItems_Pan10k 0.42 (1.02), QCPItems_Click10k 0.052 (0.98), QCPItems_Add10k 19 (0.95),
  QCPBars_Pan10k 5.7 (1.02), QCPDrawn_Zoom10k 0.3 (1.00), QCPDrawn_Pan10k 0.25 (1.00),
  QCPDrawn_Pan10kOneRecording 0.3 (0.94), QCPGpu_LineSolid100k 0.43 (0.96), QCPGpu_LineDashed100k 0.43 (0.96),
  QCPGpu_Pan100k 0.22 (1.00).

| Benchmark | milestone 3 msecs | end 4a msecs | run 1 / 2 / 3 | ratio |
|---|---|---|---|---|
| QCPAxisRects_Stack8_Replot | 1.9 | 1.8 | 1.8 / 1.8 / 1.8 | 0.95 |
| QCPAxis_SetRange10k | 1.5 | 1.3 | 1.4 / 1.3 / 1.3 | 0.87 |
| QCPAxis_SetTickLength10kItems | 0.012 | 0.012 | 0.013 / 0.012 / 0.011 | 1.00 |
| QCPAxis_TickLabels | 0.25 | 0.24 | 0.24 / 0.24 / 0.24 | 0.96 |
| QCPAxis_TickLabelsCached | 0.088 | 0.089 | 0.09 / 0.089 / 0.087 | 1.01 |
| QCPBars_Pan10k | 5.6 | 5.7 | 6.6 / 5.7 / 5.5 | 1.02 |
| QCPBars_Zoom10k | 5.4 | 5.8 | 7.6 / 5.8 / 5.2 | 1.07 |
| QCPColorMap2_ResampleDouble | 228 | 220 | 220 / 230 / 220 | 0.95 |
| QCPColorMap2_ResampleFloat | 142 | 130 | 130 / 140 / 130 | 0.93 |
| QCPColorMap2_ResamplePanSequence | 207 | 200 | 190 / 230 / 200 | 0.94 |
| QCPColorMap2_ResamplePanSequenceSmall | 211 | 210 | 210 / 210 / 200 | 0.98 |
| QCPColorMap2_ResampleWideParallel | 283 | 270 | 270 / 280 / 260 | 0.95 |
| QCPColorMap2_ResampleWideSerial | 784 | 770 | 770 / 780 / 760 | 0.98 |
| QCPColorMapData_ConstructLarge | 1.2 | 1.1 | 1.1 / 1.6 / 1.1 | 0.92 |
| QCPColorMap_ColorizeMap | 0.16 | 0.16 | 0.16 / 0.16 / 0.16 | 1.00 |
| QCPColorMap_SetCell1M | 3 | 3 | 3.1 / 3 / 3 | 1.00 |
| QCPColorMap_Standard | 0.22 | 0.22 | 0.22 / 0.22 / 0.21 | 1.00 |
| QCPCoords_CoordToPixel1M | 5.6 | 4.8 | 5 / 4.8 / 4.7 | 0.86 |
| QCPDrawContext_ReplayData1M | 1.3 | 1.4 | 1.4 / 1.3 / 1.6 | 1.08 |
| QCPDrawContext_ReplayMarkers100k | 13 | 12 | 19 / 12 / 12 | 0.92 |
| QCPGraph2_RenderDouble | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 | 1.00 |
| QCPGraph2_RenderFloat | 0.14 | 0.14 | 0.14 / 0.14 / 0.15 | 1.00 |
| QCPGraph2_RenderManyLines | 0.44 | 0.44 | 0.44 / 0.45 / 0.44 | 1.00 |
| QCPGraph2_ScatterDisc1M | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGraph2_ScatterSquare1M | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGraph2_ScatterWithLine1M | 0.13 | 0.12 | 0.13 / 0.12 / 0.12 | 0.92 |
| QCPGraph2_SetDataDouble | 9.7 | 9.6 | 9.6 / 10 / 9.5 | 0.99 |
| QCPGraph2_SetDataFloat | 8.4 | 8.4 | 8.8 / 8.4 / 8.3 | 1.00 |
| QCPGraph_AddDataAtBeginSorted | 5.4 | 5.6 | 5.6 / 8.3 / 5.3 | 1.04 |
| QCPGraph_AddDataAtBeginUnsorted | 28 | 29 | 29 / 29 / 28 | 1.04 |
| QCPGraph_AddDataAtEndSorted | 5.2 | 5.5 | 5.2 / 5.5 / 5.6 | 1.06 |
| QCPGraph_AddDataAtEndUnsorted | 23 | 24 | 24 / 26 / 24 | 1.04 |
| QCPGraph_AddDataMixedSorted | 10 | 11 | 11 / 13 / 10 | 1.10 |
| QCPGraph_AddDataMixedUnsorted | 30 | 30 | 30 / 30 / 29 | 1.00 |
| QCPGraph_AddDataSingleAtBegin | 0.047 | 0.046 | 0.045 / 0.046 / 0.046 | 0.98 |
| QCPGraph_AddDataSingleAtEnd | 0.024 | 0.024 | 0.024 / 0.024 / 0.023 | 1.00 |
| QCPGraph_AddDataSingleRandom | 5.1 | 5.1 | 5.1 / 5.1 / 5 | 1.00 |
| QCPGraph_ManyLines | 1.3 | 1.3 | 1.3 / 1.3 / 1.4 | 1.00 |
| QCPGraph_ManyOffScreenLines | 0.15 | 0.15 | 0.15 / 0.19 / 0.15 | 1.00 |
| QCPGraph_ManyPoints | 2.7 | 2.6 | 2.6 / 2.7 / 2.6 | 0.96 |
| QCPGraph_RemoveDataAfter | 0.0012 | 0.00097 | 0.00097 / 0.0014 / 0.00097 | 0.81 |
| QCPGraph_RemoveDataBefore | 0.19 | 0.22 | 0.21 / 0.24 / 0.22 | 1.16 |
| QCPGraph_RemoveDataBetween | 0.11 | 0.093 | 0.093 / 0.12 / 0.086 | 0.85 |
| QCPGraph_Standard | 0.7 | 0.69 | 0.69 / 1.1 / 0.67 | 0.99 |
| QCPInput_MouseMoveStorm | 3.9 | 3.9 | 4.1 / 3.9 / 3.7 | 1.00 |
| QCPInput_WheelOverGraph2_1M | 0.00067 | 0.00064 | 0.00066 / 0.00064 / 0.00064 | 0.96 |
| QCPItemText_Zoom1k | 0.85 | 0.88 | 0.91 / 0.88 / 0.87 | 1.04 |
| QCPItems_Add10k | 20 | 19 | 19 / 19 / 19 | 0.95 |
| QCPItems_AddRemove10k | 28 | 27 | 29 / 27 / 27 | 0.96 |
| QCPItems_AddRemove20k | 66 | 64 | 64 / 65 / 64 | 0.97 |
| QCPItems_Clear10k | 8.3 | 8.9 | 8.8 / 9.4 / 8.9 | 1.07 |
| QCPItems_Click100 | 0.0038 | 0.0042 | 0.0043 / 0.0042 / 0.0041 | 1.11 |
| QCPItems_Click10k | 0.053 | 0.052 | 0.053 / 0.052 / 0.052 | 0.98 |
| QCPItems_ClickEmpty10k | 0.05 | 0.05 | 0.05 / 0.05 / 0.05 | 1.00 |
| QCPItems_DeleteEach10k | 27 | 26 | 26 / 27 / 23 | 0.96 |
| QCPItems_DeletePlot10k | 27 | 27 | 28 / 27 / 26 | 1.00 |
| QCPItems_DragOne10k | 0.55 | 0.55 | 0.56 / 0.55 / 0.55 | 1.00 |
| QCPItems_FirstFrame10k | 30 | 29 | 29 / 29 / 29 | 0.97 |
| QCPItems_Pan10k | 0.41 | 0.42 | 0.42 / 0.42 / 0.42 | 1.02 |
| QCPItems_RemoveEach10k | 33 | 36 | 37 / 36 / 30 | 1.09 |
| QCPItems_Replot10k | 0.42 | 0.42 | 0.42 / 0.42 / 0.42 | 1.00 |
| QCPItems_RestyleAll10k | 1.6 | 1.6 | 1.7 / 1.6 / 1.5 | 1.00 |
| QCPItems_Zoom100 | 0.41 | 0.4 | 0.4 / 0.42 / 0.4 | 0.98 |
| QCPItems_Zoom10k | 0.43 | 0.43 | 0.43 / 0.43 / 0.42 | 1.00 |
| QCPItems_ZoomEmpty | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 | 1.00 |
| QCPPlots_Stack8_Replot | 0.27 | 0.26 | 0.26 / 0.26 / 0.26 | 0.96 |
| QCPSpatialIndex_Build10k | 0.89 | 0.89 | 0.89 / 0.9 / 0.89 | 1.00 |
| QCPSpatialIndex_Query10k | 0.00036 | 0.00037 | 0.00037 / 0.00037 / 0.00036 | 1.03 |
| QCPTransform_Map1M | 0.71 | 0.63 | 0.91 / 0.63 / 0.63 | 0.89 |
| QObject_DeleteEachChild10k | 7.9 | 8 | 8.4 / 8 / 7.9 | 1.01 |

### GPU rows (Xvfb, llvmpipe: CPU side only)

- Commit: 7ed486f (neoqcp-3.0), `xvfb-run -a -s "-screen 0 1600x1200x24" env QT_QPA_PLATFORM=xcb
  build-qt612/tests/benchmark/qcp_benchmark -median 5 <rows>`, the same renderer (llvmpipe) and the same caveat as
  `m3-gpu-baseline.md`: these are the CPU msecs of one `replot()`, not GPU time.
- Load average (1/5/15 min) at the start of each run: run 1: 2.31 / 3.99 / 3.49, run 2: 6.01 / 4.69 / 3.76,
  run 3: 5.05 / 4.67 / 3.81. The same cargo loop as above; no 1600x1200 Xvfb server was left behind.
- Ratio = end-of-milestone-4a median / end-of-milestone-3 median.
- All ten previously existing rows are inside their 5 % gate but one, and three of them are faster: QCPGpu_Bars10k
  5.5 -> 4.9 (0.89), QCPGpu_MixedLayer 8.7 -> 8 (0.92), QCPGpu_Graph2Render1M 0.13 -> 0.12 (0.92).
- QCPGpu_Text1k came out at 1.1 / 1.1 / 1 (+10 %); six more runs read 1.1 / 1.0 / 1.0 / 1.0 / 1.0 / 1.1, median
  1.0 (1.00), so it is inside.
- QCPGpu_BarsPan10k is new since milestone 3 (10 000 GPU bars panned once): 0.12 ms, 0.12 / 0.12 / 0.12.

| row | milestone 3 msecs | end 4a msecs | run 1 / 2 / 3 | ratio |
|---|---|---|---|---|
| QCPDrawn_Pan10k | 0.25 | 0.25 | 0.25 / 0.24 / 0.25 | 1.00 |
| QCPDrawn_Pan10kOneRecording | 0.32 | 0.3 | 0.3 / 0.3 / 0.31 | 0.94 |
| QCPDrawn_Zoom10k | 0.3 | 0.3 | 0.3 / 0.3 / 0.3 | 1.00 |
| QCPGpu_Bars10k | 5.5 | 4.9 | 4.9 / 4.9 / 4.8 | 0.89 |
| QCPGpu_BarsPan10k | - | 0.12 | 0.12 / 0.12 / 0.12 | new |
| QCPGpu_Graph2Render1M | 0.13 | 0.12 | 0.13 / 0.12 / 0.12 | 0.92 |
| QCPGpu_LineDashed100k | 0.45 | 0.43 | 0.43 / 0.42 / 0.43 | 0.96 |
| QCPGpu_LineSolid100k | 0.45 | 0.43 | 0.43 / 0.43 / 0.43 | 0.96 |
| QCPGpu_MixedLayer | 8.7 | 8 | 8 / 8 / 8.1 | 0.92 |
| QCPGpu_Pan100k | 0.22 | 0.22 | 0.21 / 0.22 / 0.22 | 1.00 |
| QCPGpu_Text1k | 1 | 1.1 | 1.1 / 1.1 / 1 | 1.10 |

## M4b start: rows added for the line family

- Commit: 65a6efe (neoqcp-3.0) plus the rows of `bench: line-family rows, measured before milestone 4b`. The
  numbers are measured on the pre-4b library code: task 1 of milestone 4b adds rows only, no library change.
  Machine, renderer and command shape as in "End of milestone 4a" above.
- Commands: `QT_QPA_PLATFORM=offscreen build-qt612/tests/benchmark/qcp_benchmark -median 5 QCPMultiGraph_SetData1M`
  for the CPU row; the four GPU rows as in "GPU rows" above: `xvfb-run -a -s "-screen 0 1600x1200x24" env
  QT_QPA_PLATFORM=xcb build-qt612/tests/benchmark/qcp_benchmark -median 5 <rows>` (llvmpipe, CPU side only).
  Three runs of each, one row set per invocation, median of the five measurements of a run.
- What each row times:
  - QCPMultiGraph_SetData1M: `setData` of 1 000 000 keys x 3 sine columns into a multigraph that never rendered,
    so the source installs at once; the vector fill is inside the loop as in QCPGraph2_SetDataDouble.
  - QCPGpu_MultiGraphPan1M: 3-column, 1 000 000-key multigraph (keys over 0..100), a 1-wide window over 50..51,
    panned by 1 % of the window per iteration. Same shape as QCPGpu_Pan100k.
  - QCPGpu_MultiGraphZoom1M: the same data, zoomed in place by 1 % (window 1.0 / 0.99 alternating), as
    QCPGpu_LineSolid100k.
  - QCPGpu_MultiGraphColored100kPan: 1-column, 100 000-key multigraph coloured by a scalar (`setColorValues`),
    panned by 1 % of the window per iteration.
  - QCPGpu_CurvePan100k: a 100 000-point Lissajous `QCPCurve`, panned by 1 % of the window per iteration.
- The GPU rows QSKIP without a QRhi (qoffscreen). Before QBENCHMARK they warm the plot as QCPGpu_Pan100k does and
  then pump the event loop until the multigraph's first L1 resample has landed (3 000 000 values is above the
  100 000-value async threshold, and the coloured row rebuilds L1 once for its origin), so the numbers are
  steady-state frames, not the first L1 build.
- Load average (1/5/15 min) at the start of each run. GPU rows, run 1: 10.34 / 7.85 / 6.69, run 2: 10.94 / 8.44 /
  6.94, run 3: 11.10 / 8.70 / 7.07. CPU row, run 1: 10.37 / 7.75 / 6.64, runs 2 and 3: 6.78 / 7.89 / 6.86. Higher
  than the 4a window and bursty: short-lived ~300 % CPU helpers of another session (`ui_csrf...`) appeared and
  vanished within seconds. The pan rows, the zoom row and the GPU series as a whole repeat to 1-2 % anyway. No
  1600x1200 Xvfb server was left behind.
- A first QCPMultiGraph_SetData1M run read 60 ms during one of those bursts; it is not one of the three runs below.
  The three left (40 / 38 / 38) agree to 5 %. This is the noisiest of the five rows across runs.
- QCPGpu_MultiGraphZoom1M is 4x QCPGpu_MultiGraphPan1M (0.62 vs 0.16 ms). Likely the line cache: a 1 % pan keeps
  the cached lines and translates the GPU entries, while a zoom in place changes the rendered range and rebuilds
  L2 in `draw()` every frame.
- These five numbers are the gate for the rows this task adds: 5 %, as for `QCPGpu_*` (plan Global Constraints).

| row | run 1 / 2 / 3 | median |
|---|---|---|
| QCPGpu_CurvePan100k | 0.4 / 0.394 / 0.43 | 0.4 |
| QCPGpu_MultiGraphColored100kPan | 0.14 / 0.14 / 0.14 | 0.14 |
| QCPGpu_MultiGraphPan1M | 0.16 / 0.17 / 0.15 | 0.16 |
| QCPGpu_MultiGraphZoom1M | 0.62 / 0.62 / 0.62 | 0.62 |
| QCPMultiGraph_SetData1M | 40 / 38 / 38 | 38 |

Controller re-measure (same build, load 3.3-8.4): QCPGpu_MultiGraphPan1M 0.15 / 0.15 / 0.15,
QCPGpu_MultiGraphZoom1M 0.61 / 0.61 / 0.64, QCPGpu_MultiGraphColored100kPan 0.13 / 0.14 / 0.13,
QCPGpu_CurvePan100k 0.40 / 0.41 / 0.39, QCPMultiGraph_SetData1M 38. The gate for these rows is the lower
median of the two series: 0.15, 0.61, 0.13, 0.40 and 38 ms.

## End of milestone 4b

- Commit: f6a110d (neoqcp-3.0), same machine and commands as "End of milestone 4a": three sequential runs of the
  whole binary offscreen (`QT_QPA_PLATFORM=offscreen build-qt612/tests/benchmark/qcp_benchmark -median 5`), and three
  under Xvfb for the GPU rows (`xvfb-run -a -s "-screen 0 1600x1200x24" env QT_QPA_PLATFORM=xcb ...`, llvmpipe, CPU
  side only).
- Load average (1/5/15 min) at the start of each run: offscreen 1.14, 1.76, 1.92; 0.89, 1.44, 1.77; 0.76, 1.32, 1.68.
  Xvfb 0.52, 1.01, 1.50; 2.11, 1.36, 1.53; 4.18, 2.44, 1.89.
- Ratio = end-of-milestone-4b median / end-of-milestone-4a median. The five rows added at the M4b start compare
  against their gates from that section. Gate: 5 % for QCPGpu_*, QCPDrawn_*, QCPItems_* and QCPBars_*, 15 % for the
  rest.
- Rows over gate in the three runs:
  - QCPGpu_MultiGraphZoom1M 0.61 -> 0.69 (1.13): accepted by ruling in Task 6 at 0.68. Each zoom step now extrudes
    about 2 150 points per column against 1 907 before (the data-space envelope keeps a few more points per pixel
    column); the extruder accounts for the difference.
  - QCPItems_Add10k 19 -> 20 (1.05, gate 5 %). Six more runs read 22 / 20 / 22 / 22 / 22 / 22 at a 1 min load near
    2. An interleaved A/B against a build of the milestone's start (65a6efe) read 21 for the start six times out of
    six, and 22 / 21 / 21 / 22 / 22 / 21 now. A perf profile of each holds the same samples (227 at the start, 224
    now), with no symbol that grows. Not a regression: QtTest rounds this row to two digits, and the end-4a 19 was
    taken at a lower load.
  - QCPGpu_Graph2Render1M 0.12 -> 0.13 (1.08, gate 5 %). QCPGraph2 did not change in this milestone. The A/B under
    Xvfb read 0.16 for the milestone's start and 0.13 now, six times out of six. Not a regression.
- New since the M4b start rows: QCPGpu_CurvePan100k 0.40 -> 0.12 (the curve records itself once; a pan moves it).
- Nothing was changed to chase a row.

| Benchmark | end 4a msecs | end 4b msecs | run 1 / 2 / 3 | ratio |
|---|---|---|---|---|
| QCPAxisRects_Stack8_Replot | 1.8 | 1.8 | 1.8 / 1.8 / 1.8 | 1.00 |
| QCPAxis_SetRange10k | 1.3 | 1.3 | 1.3 / 1.3 / 1.3 | 1.00 |
| QCPAxis_SetTickLength10kItems | 0.012 | 0.011 | 0.012 / 0.011 / 0.011 | 0.92 |
| QCPAxis_TickLabels | 0.24 | 0.24 | 0.24 / 0.24 / 0.24 | 1.00 |
| QCPAxis_TickLabelsCached | 0.089 | 0.088 | 0.088 / 0.088 / 0.087 | 0.99 |
| QCPBars_Pan10k | 5.7 | 0.14 | 0.14 / 0.13 / 0.14 | 0.02 |
| QCPBars_Zoom10k | 5.8 | 5.1 | 5.1 / 5.1 / 5.1 | 0.88 |
| QCPColorMap2_ResampleDouble | 220 | 235 | 235.0 / 233.0 / 239.0 | 1.07 |
| QCPColorMap2_ResampleFloat | 130 | 139 | 139.0 / 136.0 / 145.0 | 1.07 |
| QCPColorMap2_ResamplePanSequence | 200 | 189 | 170.0 / 192.0 / 189.0 | 0.94 |
| QCPColorMap2_ResamplePanSequenceSmall | 210 | 204 | 203.0 / 204.0 / 204.0 | 0.97 |
| QCPColorMap2_ResampleWideParallel | 270 | 258 | 263.0 / 256.0 / 258.0 | 0.96 |
| QCPColorMap2_ResampleWideSerial | 770 | 774 | 764.0 / 774.0 / 778.0 | 1.01 |
| QCPColorMapData_ConstructLarge | 1.1 | 1.2 | 1.2 / 1.3 / 1.2 | 1.09 |
| QCPColorMap_ColorizeMap | 0.16 | 0.16 | 0.16 / 0.15 / 0.16 | 1.00 |
| QCPColorMap_SetCell1M | 3 | 2.8 | 2.8 / 2.8 / 2.8 | 0.93 |
| QCPColorMap_Standard | 0.22 | 0.22 | 0.22 / 0.22 / 0.21 | 1.00 |
| QCPCoords_CoordToPixel1M | 4.8 | 4.5 | 4.5 / 4.4 / 4.5 | 0.94 |
| QCPDrawContext_ReplayData1M | 1.4 | 1.3 | 1.3 / 1.3 / 1.2 | 0.93 |
| QCPDrawContext_ReplayMarkers100k | 12 | 12 | 12.0 / 12.0 / 12.0 | 1.00 |
| QCPGraph2_RenderDouble | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 | 1.00 |
| QCPGraph2_RenderFloat | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 | 1.00 |
| QCPGraph2_RenderManyLines | 0.44 | 0.44 | 0.44 / 0.44 / 0.44 | 1.00 |
| QCPGraph2_ScatterDisc1M | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGraph2_ScatterSquare1M | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGraph2_ScatterWithLine1M | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGraph2_SetDataDouble | 9.6 | 9.5 | 9.8 / 9.5 / 9.5 | 0.99 |
| QCPGraph2_SetDataFloat | 8.4 | 8.1 | 8.1 / 8.1 / 8.4 | 0.96 |
| QCPGraph_AddDataAtBeginSorted | 5.6 | 5.4 | 5.6 / 5.4 / 5.3 | 0.96 |
| QCPGraph_AddDataAtBeginUnsorted | 29 | 28 | 28.0 / 28.0 / 29.0 | 0.97 |
| QCPGraph_AddDataAtEndSorted | 5.5 | 5.1 | 5.1 / 5.0 / 5.1 | 0.93 |
| QCPGraph_AddDataAtEndUnsorted | 24 | 23 | 23.0 / 23.0 / 24.0 | 0.96 |
| QCPGraph_AddDataMixedSorted | 11 | 10 | 10.0 / 11.0 / 10.0 | 0.91 |
| QCPGraph_AddDataMixedUnsorted | 30 | 30 | 30.0 / 30.0 / 30.0 | 1.00 |
| QCPGraph_ManyLines | 1.3 | 1.3 | 1.3 / 1.3 / 1.3 | 1.00 |
| QCPGraph_ManyOffScreenLines | 0.15 | 0.15 | 0.15 / 0.15 / 0.15 | 1.00 |
| QCPGraph_ManyPoints | 2.6 | 2.6 | 2.6 / 2.6 / 2.6 | 1.00 |
| QCPGraph_Standard | 0.69 | 0.7 | 0.7 / 0.69 / 0.71 | 1.01 |
| QCPInput_MouseMoveStorm | 3.9 | 3.8 | 3.8 / 3.7 / 3.8 | 0.97 |
| QCPInput_WheelOverGraph2_1M | 0.00064 | 0.00064 | 0.00064 / 0.00063 / 0.00064 | 1.00 |
| QCPItemText_Zoom1k | 0.88 | 0.87 | 0.87 / 0.87 / 0.86 | 0.99 |
| QCPItems_Add10k | 19 | 20 | 21.0 / 20.0 / 20.0 | 1.05 |
| QCPItems_AddRemove10k | 27 | 28 | 28.0 / 28.0 / 27.0 | 1.04 |
| QCPItems_AddRemove20k | 64 | 67 | 67.0 / 67.0 / 66.0 | 1.05 |
| QCPItems_Clear10k | 8.9 | 8.7 | 8.7 / 9.3 / 8.5 | 0.98 |
| QCPItems_Click100 | 0.0042 | 0.0042 | 0.0042 / 0.0041 / 0.0042 | 1.00 |
| QCPItems_Click10k | 0.052 | 0.052 | 0.052 / 0.052 / 0.052 | 1.00 |
| QCPItems_ClickEmpty10k | 0.05 | 0.05 | 0.05 / 0.05 / 0.05 | 1.00 |
| QCPItems_DeleteEach10k | 26 | 27 | 29.0 / 25.0 / 27.0 | 1.04 |
| QCPItems_DeletePlot10k | 27 | 27 | 26.0 / 27.0 / 27.0 | 1.00 |
| QCPItems_DragOne10k | 0.55 | 0.55 | 0.55 / 0.54 / 0.55 | 1.00 |
| QCPItems_FirstFrame10k | 29 | 29 | 29.0 / 29.0 / 29.0 | 1.00 |
| QCPItems_Pan10k | 0.42 | 0.41 | 0.41 / 0.41 / 0.41 | 0.98 |
| QCPItems_RemoveEach10k | 36 | 34 | 31.0 / 35.0 / 34.0 | 0.94 |
| QCPItems_Replot10k | 0.42 | 0.41 | 0.41 / 0.41 / 0.41 | 0.98 |
| QCPItems_RestyleAll10k | 1.6 | 1.5 | 1.7 / 1.5 / 1.5 | 0.94 |
| QCPItems_Zoom100 | 0.4 | 0.4 | 0.4 / 0.4 / 0.4 | 1.00 |
| QCPItems_Zoom10k | 0.43 | 0.42 | 0.43 / 0.42 / 0.42 | 0.98 |
| QCPItems_ZoomEmpty | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 | 1.00 |
| QCPMultiGraph_SetData1M | 38 | 33 | 32.0 / 33.0 / 33.0 | 0.87 |
| QCPPlots_Stack8_Replot | 0.26 | 0.26 | 0.26 / 0.26 / 0.26 | 1.00 |
| QCPSpatialIndex_Build10k | 0.89 | 0.88 | 0.88 / 0.88 / 0.88 | 0.99 |
| QCPSpatialIndex_Query10k | 0.00037 | 0.00036 | 0.00036 / 0.00036 / 0.00036 | 0.97 |
| QCPTransform_Map1M | 0.63 | 0.67 | 0.64 / 0.67 / 0.68 | 1.06 |
| QObject_DeleteEachChild10k | 8 | 7.8 | 7.8 / 7.8 / 8.1 | 0.97 |

### GPU rows (Xvfb, llvmpipe: CPU side only)

| Benchmark | end 4a msecs | end 4b msecs | run 1 / 2 / 3 | ratio |
|---|---|---|---|---|
| QCPGpu_Bars10k | 4.9 | 4.2 | 4.2 / 4.2 / 4.3 | 0.86 |
| QCPGpu_BarsPan10k | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGpu_CurvePan100k | 0.4 | 0.12 | 0.11 / 0.12 / 0.12 | 0.30 |
| QCPGpu_Graph2Render1M | 0.12 | 0.13 | 0.13 / 0.13 / 0.13 | 1.08 |
| QCPGpu_LineDashed100k | 0.43 | 0.43 | 0.43 / 0.43 / 0.43 | 1.00 |
| QCPGpu_LineSolid100k | 0.43 | 0.44 | 0.43 / 0.45 / 0.44 | 1.02 |
| QCPGpu_MixedLayer | 8 | 8.3 | 8.6 / 8.1 / 8.3 | 1.04 |
| QCPGpu_MultiGraphColored100kPan | 0.13 | 0.13 | 0.13 / 0.14 / 0.13 | 1.00 |
| QCPGpu_MultiGraphPan1M | 0.15 | 0.15 | 0.15 / 0.15 / 0.15 | 1.00 |
| QCPGpu_MultiGraphZoom1M | 0.61 | 0.69 | 0.69 / 0.68 / 0.69 | 1.13 |
| QCPGpu_Pan100k | 0.22 | 0.22 | 0.22 / 0.22 / 0.22 | 1.00 |
| QCPGpu_Text1k | 1.1 | 1 | 1.0 / 1.0 / 1.1 | 0.91 |

## M4c start: rows added for the colour maps

- Commit: b3bc0bd (neoqcp-3.0) plus the rows of `bench: rows for the colour maps and histograms, measured on the
  old code`. The numbers are measured on the pre-4c library code: task 1 of milestone 4c adds rows only, no library
  change. Machine, renderer and command shape as in "End of milestone 4a" above.
- Commands: the five GPU rows in one Xvfb invocation (`xvfb-run -a -s "-screen 0 1600x1200x24" env
  QT_QPA_PLATFORM=xcb build-qt612/tests/benchmark/qcp_benchmark -median 5 <rows>`, llvmpipe, CPU side only), then
  the CPU row offscreen (`QT_QPA_PLATFORM=offscreen build-qt612/tests/benchmark/qcp_benchmark -median 5
  QCPColorScale_Rescale8`). Three runs of the set, one set after another. Median of the five measurements of a run.
- What each row times:
  - QCPGpu_ColorMapPan: a 1000x1000 classic `QCPColorMap` over 0..10 on both axes, gpHot, a 2-wide by 10-tall
    window (a fifth of the data), panned by 1 % of the window per iteration. The map image is only rebuilt when the
    data, the data range, the gradient or the scale type changes, so a pan step draws the cached 1000x1000 image
    through the painter and lets the clip do the rest: the row times that frame, not a re-colour. The pan wraps to
    0 when it reaches the end of the data.
  - QCPGpu_ColorMap2Pan: a `QCPColorMap2` over a 4000x1000 source, a 400-wide window (a tenth of the source),
    panned by 1 % of the window per iteration. A range change submits an async resample; the row times the
    steady-state pan, so every frame draws the last result and translates it when it can.
  - QCPGpu_ColorMap2Zoom: the same source, zoomed in place by 1 % (window 400 / 396 alternating) around 2000.
  - QCPGpu_Histogram2DPan: a `QCPHistogram2D` over 1 000 000 scattered (x, y) pairs in 0..100 over both axes,
    200x200 bins, a 20-wide window, panned by 1 % of the window per iteration. The binning is viewport
    independent, so a pan step redraws the 200x200 image and does not re-bin.
  - QCPGpu_HistogramPan: a 1-D `QCPHistogram` over 1 000 000 keys in 0..100, 100 bins, a 20-wide window, panned by
    1 % of the window per iteration. `draw()` rebuilds the bar geometry every frame.
  - QCPColorScale_Rescale8: `rescaleDataRange(true)` on one `QCPColorScale` over 8 classic 500x500 maps that are
    all visible. It times the walk over the plot's plottables, each map's cached `dataBounds` and `setDataRange`.
    The cells are not read: `setCell` keeps the bounds, so this row cannot show a per-cell scan.
- The GPU rows QSKIP without a QRhi (qoffscreen). Before QBENCHMARK they warm the plot as QCPGpu_Pan100k does; the
  colour-map and histogram rows then replot and pump the event loop until their pipeline is idle, so the numbers are
  steady-state frames, not the first resample or the first binning.
- Load average (1/5/15 min) at the start of each run. GPU rows, run 1: 4.50 / 6.26 / 4.84, run 2: 5.63 / 6.36 /
  4.94, run 3: 6.75 / 6.58 / 5.09. CPU row, run 1: 6.03 / 6.45 / 4.97, run 2: 7.25 / 6.68 / 5.11, run 3: 4.46 /
  5.99 / 4.95. No 1600x1200 Xvfb server was left behind.
- Every row ran under Xvfb; none skipped. The medians of five repeat to 2-8 % across the three runs. The
  per-iteration spread inside one measurement is much wider than that for QCPGpu_ColorMapPan (std dev 0.17-0.28 on
  0.41 ms) and QCPGpu_ColorMap2Zoom (0.07-0.08 on 0.15 ms); the median of five absorbs it.
- QCPGpu_ColorMapPan is 3x QCPGpu_MultiGraphPan1M (0.41 vs 0.15 ms). Both draw a cached result, but the map frame
  draws a 1000x1000 image into the axis rect on every step, while a multigraph pan translates cached lines.
- QCPColorScale_Rescale8 is 0.00014 ms (0.14 us): 8 plottables, cached bounds, one range set. If the M4c code ever
  reads cells here, the number moves by orders of magnitude, not percent.
- These six numbers are the gate for the rows this task adds: 5 %, as for `QCPGpu_*` (plan Global Constraints).

| row | run 1 / 2 / 3 | median |
|---|---|---|
| QCPColorScale_Rescale8 | 0.00014 / 0.00014 / 0.00014 | 0.00014 |
| QCPGpu_ColorMap2Pan | 0.14 / 0.13 / 0.14 | 0.13 |
| QCPGpu_ColorMap2Zoom | 0.15 / 0.15 / 0.15 | 0.15 |
| QCPGpu_ColorMapPan | 0.41 / 0.41 / 0.42 | 0.41 |
| QCPGpu_Histogram2DPan | 0.14 / 0.13 / 0.13 | 0.13 |
| QCPGpu_HistogramPan | 0.19 / 0.18 / 0.18 | 0.18 |

## End of milestone 4c

- Commit: caf6581 (neoqcp-3.0), the last commit before the final-review fix batch. Same commands as "End of
  milestone 4b": three sequential runs of the whole binary offscreen and three under Xvfb (llvmpipe, CPU side only),
  `-median 5`. The rows the fix batch could move (the colour-map, histogram, coloured-line and text rows) were run
  again three times at 3783e07; they read the same (list below).
- Load average (1/5/15 min) at the start of each run: offscreen 6.62, 4.26, 3.08; 0.90, 2.71, 2.71; 1.03, 1.89, 2.38.
  Xvfb 0.96, 1.47, 2.12; 3.57, 2.50, 2.38; 5.52, 3.48, 2.76.
- Ratio = end-of-milestone-4c median / end-of-milestone-4b median. The rows added at the M4c start compare against
  their gates (re-set by rulings T4 and T8 for the visible colour bar: ColorMapPan 0.48, ColorMap2Pan 0.20,
  ColorMap2Zoom 0.22, Histogram2DPan 0.20, Colored100kPan 0.21). The offscreen and the Xvfb runs of a CPU row are
  both listed (six numbers).
- The point of the milestone: QCPGpu_ColorMapPan 0.48 -> 0.17 and QCPGpu_HistogramPan 0.18 -> 0.12. A pan moves the
  recorded image or bars instead of drawing them again.
- Rows over gate:
  - QCPGpu_Text1k 1.0 -> 1.1 (gate 5 %), accepted by ruling. An interleaved A/B against a build of the milestone's
    start (b3bc0bd), five pairs with `-csv`, read 1.039 against 1.091 ms (+5 %). On the benchmark thread the
    instructions, cycles, cache misses and branch misses are the same in both builds, and the text path's code did
    not change: the extra time is spent waiting on llvmpipe. To check on a real GPU (backlog).
  - QCPItems_RestyleAll10k 1.5 -> 1.6: the offscreen runs read 1.5 / 1.5 / 1.4, inside the gate; the Xvfb runs
    (1.7 / 1.9 / 1.7) were taken at a 1 min load of 3.6 to 5.5.
  - QCPTransform_Map1M 0.67 -> 0.755 (1.13, gate 15 %): the six reads spread from 0.63 to 0.88; src/coords is
    untouched.
- At 3783e07 (Qt 6.12, Xvfb, three runs): ColorMapPan 0.17 / 0.18 / 0.17, ColorMap2Pan 0.18 / 0.18 / 0.18,
  ColorMap2Zoom 0.22 / 0.22 / 0.23, Histogram2DPan 0.17 / 0.18 / 0.17, HistogramPan 0.13 / 0.12 / 0.13,
  MultiGraphColored100kPan 0.20 / 0.21 / 0.20, Pan100k 0.22 / 0.22 / 0.22, Text1k 1.2 / 1.1 / 1.1 (six more: 1.1).
- QCPColorMap2_ResampleFloat read 154 to 156 during Task 6 under load; it reads 136 here (gate 139).
- Nothing was changed to chase a row.

| Benchmark | end 4b msecs | end 4c msecs | runs | ratio |
|---|---|---|---|---|
| QCPAxisRects_Stack8_Replot | 1.8 | 1.8 | 1.8 / 1.8 / 1.8 / 1.8 / 1.8 / 1.8 | 1.00 |
| QCPAxis_SetRange10k | 1.3 | 1.3 | 1.3 / 1.3 / 1.3 / 1.3 / 1.3 / 1.3 | 1.00 |
| QCPAxis_SetTickLength10kItems | 0.011 | 0.011 | 0.011 / 0.012 / 0.012 / 0.011 / 0.011 / 0.011 | 1.00 |
| QCPAxis_TickLabels | 0.24 | 0.24 | 0.24 / 0.24 / 0.24 / 0.24 / 0.24 / 0.24 | 1.00 |
| QCPAxis_TickLabelsCached | 0.088 | 0.0875 | 0.088 / 0.087 / 0.088 / 0.087 / 0.089 / 0.087 | 0.99 |
| QCPBars_Pan10k | 0.14 | 0.13 | 0.14 / 0.13 / 0.14 / 0.13 / 0.13 / 0.13 | 0.93 |
| QCPBars_Zoom10k | 5.1 | 5.15 | 5.2 / 5.2 / 5.3 / 5.1 / 5.1 / 5 | 1.01 |
| QCPColorMap2_ResampleDouble | 235.0 | 224 | 228 / 225 / 227 / 222 / 216 / 215 | 0.95 |
| QCPColorMap2_ResampleFloat | 139.0 | 136 | 139 / 136 / 137 / 135 / 132 / 134 | 0.97 |
| QCPColorMap2_ResamplePanSequence | 189.0 | 184 | 189 / 181 / 187 / 174 / 194 / 164 | 0.97 |
| QCPColorMap2_ResamplePanSequenceSmall | 204.0 | 206 | 206 / 206 / 207 / 206 / 206 / 203 | 1.01 |
| QCPColorMap2_ResampleWideParallel | 258.0 | 264 | 261 / 268 / 258 / 259 / 268 / 267 | 1.02 |
| QCPColorMap2_ResampleWideSerial | 774.0 | 768 | 769 / 763 / 771 / 763 / 767 / 769 | 0.99 |
| QCPColorMapData_ConstructLarge | 1.2 | 1.2 | 1.2 / 1.1 / 1.1 / 1.3 / 1.2 / 1.3 | 1.00 |
| QCPColorMap_ColorizeMap | 0.16 | 0.16 | 0.16 / 0.16 / 0.16 / 0.16 / 0.16 / 0.16 | 1.00 |
| QCPColorMap_SetCell1M | 2.8 | 2.4 | 2.4 / 2.4 / 2.4 / 2.4 / 2.4 / 2.4 | 0.86 |
| QCPColorMap_Standard | 0.27 | 0.27 | 0.27 / 0.27 / 0.27 / 0.27 / 0.27 / 0.26 | 1.00 |
| QCPColorScale_Rescale8 | 0.00014 | 0.00015 | 0.00015 / 0.00015 / 0.00015 / 0.00015 / 0.00015 / 0.00016 | 1.07 |
| QCPCoords_CoordToPixel1M | 4.5 | 4.2 | 4.2 / 4.2 / 4.2 / 4.2 / 4.2 / 4.3 | 0.93 |
| QCPDrawContext_ReplayData1M | 1.3 | 1.3 | 1.3 / 1.3 / 1.3 / 1.4 / 1.3 / 1.2 | 1.00 |
| QCPDrawContext_ReplayMarkers100k | 12.0 | 12 | 12 / 12 / 12 / 12 / 12 / 12 | 1.00 |
| QCPDrawn_Pan10k | None | 0.25 | 0.25 / 0.24 / 0.25 |  |
| QCPDrawn_Pan10kOneRecording | None | 0.27 | 0.27 / 0.26 / 0.27 |  |
| QCPDrawn_Zoom10k | None | 0.3 | 0.3 / 0.3 / 0.3 |  |
| QCPGpu_Bars10k | 4.2 | 4.2 | 4.2 / 4.2 / 4.2 | 1.00 |
| QCPGpu_BarsPan10k | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGpu_ColorMap2Pan | 0.2 | 0.18 | 0.18 / 0.18 / 0.18 | 0.90 |
| QCPGpu_ColorMap2Zoom | 0.22 | 0.22 | 0.22 / 0.23 / 0.22 | 1.00 |
| QCPGpu_ColorMapPan | 0.48 | 0.17 | 0.17 / 0.17 / 0.17 | 0.35 |
| QCPGpu_CurvePan100k | 0.12 | 0.11 | 0.11 / 0.12 / 0.11 | 0.92 |
| QCPGpu_Graph2Render1M | 0.13 | 0.13 | 0.13 / 0.13 / 0.12 | 1.00 |
| QCPGpu_Histogram2DPan | 0.2 | 0.17 | 0.17 / 0.17 / 0.18 | 0.85 |
| QCPGpu_HistogramPan | 0.18 | 0.12 | 0.12 / 0.12 / 0.12 | 0.67 |
| QCPGpu_LineDashed100k | 0.43 | 0.43 | 0.43 / 0.43 / 0.43 | 1.00 |
| QCPGpu_LineSolid100k | 0.44 | 0.44 | 0.43 / 0.44 / 0.44 | 1.00 |
| QCPGpu_MixedLayer | 8.3 | 8.1 | 8.6 / 8.1 / 8 | 0.98 |
| QCPGpu_MultiGraphColored100kPan | 0.21 | 0.2 | 0.2 / 0.2 / 0.2 | 0.95 |
| QCPGpu_MultiGraphPan1M | 0.15 | 0.15 | 0.15 / 0.15 / 0.15 | 1.00 |
| QCPGpu_MultiGraphZoom1M | 0.69 | 0.68 | 0.67 / 0.69 / 0.68 | 0.99 |
| QCPGpu_Pan100k | 0.22 | 0.21 | 0.21 / 0.21 / 0.22 | 0.95 |
| QCPGpu_Text1k | 1.0 | 1.1 | 1.1 / 1 / 1.1 | 1.10 |
| QCPGraph2_RenderDouble | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 / 0.14 / 0.14 / 0.14 | 1.00 |
| QCPGraph2_RenderFloat | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 / 0.14 / 0.14 / 0.14 | 1.00 |
| QCPGraph2_RenderManyLines | 0.44 | 0.44 | 0.44 / 0.44 / 0.44 / 0.44 / 0.46 / 0.44 | 1.00 |
| QCPGraph2_ScatterDisc1M | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 / 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGraph2_ScatterSquare1M | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 / 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGraph2_ScatterWithLine1M | 0.12 | 0.12 | 0.13 / 0.12 / 0.13 / 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGraph2_SetDataDouble | 9.5 | 9.65 | 9.4 / 9.4 / 9.7 / 9.7 / 9.7 / 9.6 | 1.02 |
| QCPGraph2_SetDataFloat | 8.1 | 8.25 | 8.3 / 8.2 / 8.2 / 8.8 / 8.3 / 8.1 | 1.02 |
| QCPGraph_AddDataAtBeginSorted | 5.4 | 5.3 | 6.7 / 5.2 / 5.3 / 5.6 / 5.3 / 5.3 | 0.98 |
| QCPGraph_AddDataAtBeginUnsorted | 28.0 | 28 | 29 / 28 / 28 / 29 / 28 / 28 | 1.00 |
| QCPGraph_AddDataAtEndSorted | 5.1 | 5 | 6.1 / 5 / 5 / 5.1 / 5 / 5 | 0.98 |
| QCPGraph_AddDataAtEndUnsorted | 23.0 | 23 | 23 / 24 / 23 / 23 / 23 / 23 | 1.00 |
| QCPGraph_AddDataMixedSorted | 10.0 | 10.5 | 11 / 11 / 10 / 11 / 10 / 10 | 1.05 |
| QCPGraph_AddDataMixedUnsorted | 30.0 | 29 | 29 / 29 / 29 / 29 / 30 / 29 | 0.97 |
| QCPGraph_ManyLines | 1.3 | 1.3 | 1.3 / 1.3 / 1.3 / 1.3 / 1.3 / 1.3 | 1.00 |
| QCPGraph_ManyOffScreenLines | 0.15 | 0.15 | 0.15 / 0.15 / 0.15 / 0.15 / 0.15 / 0.15 | 1.00 |
| QCPGraph_ManyPoints | 2.6 | 2.6 | 2.7 / 2.6 / 2.6 / 2.6 / 2.6 / 2.6 | 1.00 |
| QCPGraph_Standard | 0.7 | 0.7 | 0.7 / 0.66 / 0.68 / 0.71 / 0.7 / 0.71 | 1.00 |
| QCPInput_MouseMoveStorm | 3.8 | 3.7 | 3.7 / 3.7 / 3.7 / 3.8 / 3.7 / 3.7 | 0.97 |
| QCPInput_WheelOverGraph2_1M | 0.00064 | 0.00063 | 0.00063 / 0.00067 / 0.00062 / 0.00063 / 0.00062 / 0.00063 | 0.98 |
| QCPItemText_Zoom1k | 0.87 | 0.87 | 0.88 / 0.86 / 0.87 / 0.88 / 0.87 / 0.87 | 1.00 |
| QCPItems_Add10k | 20.0 | 20 | 19 / 19 / 20 / 20 / 20 / 20 | 1.00 |
| QCPItems_AddRemove10k | 28.0 | 28 | 29 / 28 / 28 / 27 / 28 / 27 | 1.00 |
| QCPItems_AddRemove20k | 67.0 | 66 | 66 / 66 / 66 / 64 / 68 / 63 | 0.99 |
| QCPItems_Clear10k | 8.7 | 7.95 | 8.7 / 7.9 / 8.8 / 7.8 / 7.8 / 8 | 0.91 |
| QCPItems_Click100 | 0.0042 | 0.0042 | 0.0043 / 0.0042 / 0.0042 / 0.0042 / 0.0043 / 0.0042 | 1.00 |
| QCPItems_Click10k | 0.052 | 0.052 | 0.052 / 0.052 / 0.052 / 0.052 / 0.052 / 0.052 | 1.00 |
| QCPItems_ClickEmpty10k | 0.05 | 0.05 | 0.05 / 0.05 / 0.05 / 0.05 / 0.05 / 0.049 | 1.00 |
| QCPItems_DeleteEach10k | 27.0 | 25.5 | 23 / 25 / 26 / 26 / 27 / 23 | 0.94 |
| QCPItems_DeletePlot10k | 27.0 | 26 | 26 / 26 / 26 / 27 / 26 / 27 | 0.96 |
| QCPItems_DragOne10k | 0.55 | 0.555 | 0.56 / 0.56 / 0.55 / 0.55 / 0.56 / 0.55 | 1.01 |
| QCPItems_FirstFrame10k | 29.0 | 30 | 30 / 30 / 31 / 28 / 29 / 30 | 1.03 |
| QCPItems_Pan10k | 0.41 | 0.415 | 0.41 / 0.41 / 0.42 / 0.42 / 0.42 / 0.41 | 1.01 |
| QCPItems_RemoveEach10k | 34.0 | 31.5 | 30 / 30 / 34 / 33 / 33 / 29 | 0.93 |
| QCPItems_Replot10k | 0.41 | 0.42 | 0.42 / 0.42 / 0.43 / 0.41 / 0.42 / 0.42 | 1.02 |
| QCPItems_RestyleAll10k | 1.5 | 1.6 | 1.5 / 1.5 / 1.4 / 1.7 / 1.9 / 1.7 | 1.07 |
| QCPItems_Zoom100 | 0.4 | 0.4 | 0.4 / 0.4 / 0.4 / 0.4 / 0.4 / 0.4 | 1.00 |
| QCPItems_Zoom10k | 0.42 | 0.425 | 0.43 / 0.42 / 0.45 / 0.42 / 0.43 / 0.42 | 1.01 |
| QCPItems_ZoomEmpty | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 / 0.14 / 0.14 / 0.14 | 1.00 |
| QCPMultiGraph_SetData1M | 33.0 | 30.5 | 33 / 57 / 33 / 28 / 28 / 28 | 0.92 |
| QCPPlots_Stack8_Replot | 0.26 | 0.26 | 0.26 / 0.26 / 0.26 / 0.26 / 0.26 / 0.26 | 1.00 |
| QCPSpatialIndex_Build10k | 0.88 | 0.89 | 0.89 / 0.89 / 0.88 / 0.89 / 0.89 / 0.91 | 1.01 |
| QCPSpatialIndex_Query10k | 0.00036 | 0.00036 | 0.00036 / 0.00036 / 0.00036 / 0.00036 / 0.00036 / 0.00036 | 1.00 |
| QCPTransform_Map1M | 0.67 | 0.755 | 0.76 / 0.69 / 0.83 / 0.63 / 0.88 / 0.75 | 1.13 |
| QObject_DeleteEachChild10k | 7.8 | 7.8 | 7.8 / 7.8 / 7.9 / 7.8 / 7.8 / 7.8 | 1.00 |

## End of milestone 4d

- Commit: d858299 (neoqcp-3.0) for the whole-table runs, then 3f0e2ba (the coordToPixel fix) for the A/B below.
  Same commands as "End of milestone 4c": three sequential runs of the whole binary offscreen and three under Xvfb
  (llvmpipe, CPU side only), `-median 5`.
- Load average (1/5/15 min) at the start of each run: offscreen 10.41, 7.43, 7.37; 4.06, 7.73, 7.74; 15.27, 8.96,
  8.04. Xvfb 4.60, 6.86, 7.38; 8.34, 6.61, 7.07; 11.88, 12.67, 9.54. Other sessions were building on this machine
  all evening, so most rows read 5-25 % slower, rows in untouched code too (QCPColorMapData_ConstructLarge 1.17,
  QCPMultiGraph_SetData1M 1.25).
- Ratio = end-of-milestone-4d median / end-of-milestone-4c median.
- Because of the load, every row in code the milestone touched, and every drawing row over its gate, was settled by an
  interleaved A/B against a build of 5719902 (end of M4c), five pairs, `-csv`, same load for both sides:
  - offscreen: QCPTransform_Map1M 0.99, QCPAxis_SetRange10k 1.00, QCPInput_MouseMoveStorm 1.01,
    QCPDrawContext_ReplayData1M 1.07 (pairs 0.87 to 1.18), controls QCPMultiGraph_SetData1M 1.02 and
    QCPColorMapData_ConstructLarge 1.10 (pairs 0.26 to 1.41);
  - Xvfb: QCPGpu_Pan100k 1.01, QCPGpu_CurvePan100k 0.99, QCPGpu_LineDashed100k 1.00, QCPGpu_MultiGraphZoom1M 1.00,
    QCPGpu_MixedLayer 0.97, QCPItems_RestyleAll10k 0.90, QCPItems_Clear10k 0.85.
  - The one real cost: QCPCoords_CoordToPixel1M 1.16 (pairs 1.15 to 1.17). The link check in QCPAxis::coordToPixel
    read a shared_ptr in another cache line; 3f0e2ba reads a flag next to the range. A/B after: 0.99 (pairs 0.98 to
    1.00).
- New row: QCPPlots_Stack8_Aligned_Replot 0.27, against QCPPlots_Stack8_Replot 0.27 in the same runs: an aligned stack
  costs nothing per repaint.
- Nothing was changed to chase a row other than coordToPixel.

| Benchmark | end 4c msecs | end 4d msecs | runs | ratio |
|---|---|---|---|---|
| QCPAxisRects_Stack8_Replot | 1.8 | 1.9 | 1.8 / 1.8 / 1.9 / 1.8 / 2 / 2.2 | 1.03 |
| QCPAxis_SetRange10k | 1.3 | 1.3 | 1.3 / 1.4 / 1.3 / 1.3 / 1.3 / 1.3 | 1.00 |
| QCPAxis_SetTickLength10kItems | 0.011 | 0.012 | 0.012 / 0.012 / 0.012 / 0.012 / 0.011 / 0.012 | 1.09 |
| QCPAxis_TickLabels | 0.24 | 0.25 | 0.25 / 0.3 / 0.25 / 0.24 / 0.24 / 0.25 | 1.04 |
| QCPAxis_TickLabelsCached | 0.0875 | 0.089 | 0.089 / 0.091 / 0.088 / 0.089 / 0.089 / 0.09 | 1.02 |
| QCPBars_Pan10k | 0.13 | 0.14 | 0.14 / 0.14 / 0.14 / 0.14 / 0.14 / 0.13 | 1.08 |
| QCPBars_Zoom10k | 5.15 | 5.7 | 5.4 / 5.5 / 5.7 / 56 / 5.7 / 5.6 | 1.10 |
| QCPColorMap2_ResampleDouble | 224.0 | 2.4e+02 | 2.4e+02 / 2.2e+02 / 2.4e+02 / 2.3e+02 / 2.5e+02 / 2.6e+02 | 1.08 |
| QCPColorMap2_ResampleFloat | 136.0 | 1.5e+02 | 1.5e+02 / 1.4e+02 / 1.4e+02 / 1.6e+02 / 1.7e+02 / 1.6e+02 | 1.12 |
| QCPColorMap2_ResamplePanSequence | 184.0 | 2.3e+02 | 2.3e+02 / 2.2e+02 / 2.3e+02 / 2.2e+02 / 2.2e+02 / 2.4e+02 | 1.23 |
| QCPColorMap2_ResamplePanSequenceSmall | 206.0 | 2.1e+02 | 2.1e+02 / 2.1e+02 / 2.1e+02 / 2.1e+02 / 2.1e+02 / 2.1e+02 | 1.01 |
| QCPColorMap2_ResampleWideParallel | 264.0 | 2.9e+02 | 3e+02 / 2.9e+02 / 2.8e+02 / 2.9e+02 / 2.8e+02 / 3.1e+02 | 1.10 |
| QCPColorMap2_ResampleWideSerial | 768.0 | 7.9e+02 | 8.1e+02 / 8e+02 / 7.9e+02 / 7.8e+02 / 7.8e+02 / 1.6e+03 | 1.03 |
| QCPColorMapData_ConstructLarge | 1.2 | 1.4 | 1.3 / 1.1 / 1.2 / 2.1 / 3.4 / 1.5 | 1.17 |
| QCPColorMap_ColorizeMap | 0.16 | 0.16 | 0.16 / 0.16 / 0.16 / 0.16 / 0.17 / 0.18 | 1.00 |
| QCPColorMap_SetCell1M | 2.4 | 2.4 | 2.4 / 2.4 / 2.4 / 2.5 / 2.4 / 2.7 | 1.00 |
| QCPColorMap_Standard | 0.27 | 0.27 | 0.27 / 0.26 / 0.27 / 0.27 / 0.28 / 0.3 | 1.00 |
| QCPColorScale_Rescale8 | 0.00015 | 0.00016 | 0.00016 / 0.00022 / 0.00016 / 0.00016 / 0.00016 / 0.00017 | 1.07 |
| QCPCoords_CoordToPixel1M | 4.2 | 5 | 4.9 / 9.7 / 5 / 5 / 5 / 5 | 1.19 |
| QCPDrawContext_ReplayData1M | 1.3 | 1.6 | 1.3 / 1.8 / 1.7 / 1.6 / 1.9 / 1.5 | 1.27 |
| QCPDrawContext_ReplayMarkers100k | 12.0 | 12 | 12 / 13 / 12 / 18 / 12 / 12 | 1.00 |
| QCPDrawn_Pan10k | 0.25 | 0.25 | 0.26 / 0.25 / 0.25 | 1.00 |
| QCPDrawn_Pan10kOneRecording | 0.27 | 0.28 | 0.29 / 0.28 / 0.27 | 1.04 |
| QCPDrawn_Zoom10k | 0.3 | 0.32 | 0.32 / 0.32 / 0.32 | 1.07 |
| QCPGpu_Bars10k | 4.2 | 4.4 | 4.4 / 8.4 / 4.4 | 1.05 |
| QCPGpu_BarsPan10k | 0.12 | 0.12 | 0.12 / 0.26 / 0.12 | 1.00 |
| QCPGpu_ColorMap2Pan | 0.18 | 0.19 | 0.18 / 0.19 / 0.24 | 1.06 |
| QCPGpu_ColorMap2Zoom | 0.22 | 0.24 | 0.23 / 0.24 / 0.51 | 1.09 |
| QCPGpu_ColorMapPan | 0.17 | 0.17 | 0.17 / 0.17 / 0.24 | 1.00 |
| QCPGpu_CurvePan100k | 0.11 | 0.15 | 0.12 / 0.2 / 0.15 | 1.36 |
| QCPGpu_Graph2Render1M | 0.13 | 0.13 | 0.12 / 0.28 / 0.13 | 1.00 |
| QCPGpu_Histogram2DPan | 0.17 | 0.18 | 0.18 / 0.18 / 0.24 | 1.06 |
| QCPGpu_HistogramPan | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGpu_LineDashed100k | 0.43 | 0.52 | 0.44 / 0.96 / 0.52 | 1.21 |
| QCPGpu_LineSolid100k | 0.44 | 0.46 | 0.44 / 0.48 / 0.46 | 1.05 |
| QCPGpu_MixedLayer | 8.1 | 9.2 | 9.2 / 9.3 / 8.9 | 1.14 |
| QCPGpu_MultiGraphColored100kPan | 0.2 | 0.23 | 0.2 / 0.92 / 0.23 | 1.15 |
| QCPGpu_MultiGraphPan1M | 0.15 | 0.16 | 0.15 / 0.43 / 0.16 | 1.07 |
| QCPGpu_MultiGraphZoom1M | 0.68 | 0.82 | 0.7 / 4.1 / 0.82 | 1.21 |
| QCPGpu_Pan100k | 0.21 | 0.25 | 0.21 / 1.7 / 0.25 | 1.19 |
| QCPGpu_Text1k | 1.1 | 1.1 | 1.1 / 1.3 / 1.1 | 1.00 |
| QCPGraph2_RenderDouble | 0.14 | 0.14 | 0.14 / 0.14 / 0.15 / 0.14 / 0.14 / 0.14 | 1.00 |
| QCPGraph2_RenderFloat | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 / 0.14 / 0.14 / 0.14 | 1.00 |
| QCPGraph2_RenderManyLines | 0.44 | 0.45 | 0.45 / 0.44 / 0.47 / 0.45 / 0.44 / 0.51 | 1.02 |
| QCPGraph2_ScatterDisc1M | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 / 0.12 / 0.12 / 0.13 | 1.00 |
| QCPGraph2_ScatterSquare1M | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 / 0.12 / 0.12 / 0.14 | 1.00 |
| QCPGraph2_ScatterWithLine1M | 0.12 | 0.12 | 0.13 / 0.12 / 0.12 / 0.12 / 0.13 / 0.14 | 1.04 |
| QCPGraph2_SetDataDouble | 9.65 | 9.7 | 11 / 9.6 / 9.6 / 9.6 / 9.8 / 9.9 | 1.01 |
| QCPGraph2_SetDataFloat | 8.25 | 8.3 | 8.2 / 8.2 / 8.8 / 8.3 / 8.5 / 8.3 | 1.01 |
| QCPGraph_AddDataAtBeginSorted | 5.3 | 5.4 | 5.8 / 5.2 / 5.4 / 5.5 / 5.4 / 5.3 | 1.02 |
| QCPGraph_AddDataAtBeginUnsorted | 28.0 | 29 | 29 / 28 / 28 / 29 / 30 / 29 | 1.04 |
| QCPGraph_AddDataAtEndSorted | 5.0 | 5.2 | 5.4 / 5.1 / 5.3 / 5.3 / 5.2 / 5 | 1.05 |
| QCPGraph_AddDataAtEndUnsorted | 23.0 | 23 | 23 / 23 / 23 / 23 / 23 / 23 | 1.00 |
| QCPGraph_AddDataMixedSorted | 10.5 | 11 | 11 / 10 / 11 / 11 / 10 / 11 | 1.05 |
| QCPGraph_AddDataMixedUnsorted | 29.0 | 29 | 29 / 29 / 29 / 29 / 31 / 29 | 1.00 |
| QCPGraph_ManyLines | 1.3 | 1.3 | 9.3 / 1.3 / 1.4 / 1.3 / 1.3 / 1.3 | 1.00 |
| QCPGraph_ManyOffScreenLines | 0.15 | 0.15 | 0.53 / 0.15 / 0.16 / 0.15 / 0.15 / 0.16 | 1.03 |
| QCPGraph_ManyPoints | 2.6 | 2.6 | 51 / 2.6 / 2.6 / 2.6 / 2.7 / 2.6 | 1.00 |
| QCPGraph_Standard | 0.7 | 0.71 | 35 / 0.7 / 0.68 / 0.74 / 0.73 / 0.68 | 1.02 |
| QCPInput_MouseMoveStorm | 3.7 | 3.9 | 3.9 / 3.9 / 8.6 / 3.8 / 3.9 / 4.5 | 1.05 |
| QCPInput_WheelOverGraph2_1M | 0.00063 | 0.00068 | 0.00086 / 0.00065 / 0.0007 / 0.00064 / 0.00065 / 0.00073 | 1.07 |
| QCPItemText_Zoom1k | 0.87 | 0.93 | 0.91 / 0.94 / 0.93 / 0.95 / 0.92 / 0.91 | 1.06 |
| QCPItems_Add10k | 20.0 | 20 | 20 / 20 / 20 / 20 / 21 / 45 | 1.00 |
| QCPItems_AddRemove10k | 28.0 | 29 | 28 / 29 / 28 / 29 / 29 / 32 | 1.04 |
| QCPItems_AddRemove20k | 66.0 | 69 | 66 / 69 / 67 / 69 / 69 / 72 | 1.05 |
| QCPItems_Clear10k | 7.95 | 9.4 | 9.5 / 9.1 / 8.4 / 9.3 / 9.9 / 25 | 1.18 |
| QCPItems_Click100 | 0.0042 | 0.0042 | 0.0042 / 0.0042 / 0.0042 / 0.0041 / 0.0042 / 0.0043 | 1.00 |
| QCPItems_Click10k | 0.052 | 0.053 | 0.053 / 0.052 / 0.053 / 0.053 / 0.053 / 0.12 | 1.02 |
| QCPItems_ClickEmpty10k | 0.05 | 0.051 | 0.05 / 0.05 / 0.051 / 0.05 / 0.051 / 0.06 | 1.01 |
| QCPItems_DeleteEach10k | 25.5 | 28 | 29 / 27 / 25 / 29 / 26 / 1.2e+02 | 1.10 |
| QCPItems_DeletePlot10k | 26.0 | 28 | 27 / 26 / 28 / 29 / 29 / 61 | 1.10 |
| QCPItems_DragOne10k | 0.555 | 0.56 | 0.56 / 0.56 / 0.56 / 0.55 / 0.56 / 0.58 | 1.01 |
| QCPItems_FirstFrame10k | 30.0 | 30 | 29 / 29 / 30 / 29 / 32 / 31 | 0.98 |
| QCPItems_Pan10k | 0.415 | 0.42 | 0.42 / 0.42 / 0.43 / 0.41 / 0.42 / 1.6 | 1.01 |
| QCPItems_RemoveEach10k | 31.5 | 33 | 32 / 33 / 33 / 33 / 33 / 1.1e+02 | 1.05 |
| QCPItems_Replot10k | 0.42 | 0.43 | 0.42 / 0.42 / 0.42 / 0.44 / 0.44 / 0.46 | 1.02 |
| QCPItems_RestyleAll10k | 1.6 | 1.9 | 1.7 / 2.1 / 1.5 / 1.5 / 2.9 / 3.4 | 1.19 |
| QCPItems_Zoom100 | 0.4 | 0.41 | 0.41 / 0.4 / 0.41 / 0.4 / 0.42 / 1.6 | 1.02 |
| QCPItems_Zoom10k | 0.425 | 0.43 | 0.43 / 0.42 / 0.44 / 0.42 / 0.43 / 0.92 | 1.01 |
| QCPItems_ZoomEmpty | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 / 0.14 / 0.15 / 0.29 | 1.00 |
| QCPMultiGraph_SetData1M | 30.5 | 38 | 43 / 33 / 56 / 29 / 70 / 30 | 1.25 |
| QCPPlots_Stack8_Aligned_Replot | new | 0.27 | 0.27 / 0.26 / 0.27 / 0.26 / 0.28 / 0.31 | - |
| QCPPlots_Stack8_Replot | 0.26 | 0.27 | 0.46 / 0.26 / 0.26 / 0.26 / 0.27 / 0.3 | 1.02 |
| QCPSpatialIndex_Build10k | 0.89 | 0.91 | 0.94 / 0.91 / 0.91 / 0.88 / 0.9 / 0.97 | 1.02 |
| QCPSpatialIndex_Query10k | 0.00036 | 0.00037 | 0.00038 / 0.00036 / 0.00041 / 0.00036 / 0.00036 / 0.00042 | 1.03 |
| QCPTransform_Map1M | 0.755 | 0.88 | 0.61 / 6.4 / 0.88 / 0.87 / 0.88 / 0.74 | 1.16 |
| QObject_DeleteEachChild10k | 7.8 | 7.9 | 8 / 7.9 / 7.9 / 7.9 / 7.9 / 27 | 1.01 |

## End of milestone 4e

- Commit: 4daf40c (neoqcp-3.0) for the whole-table runs, after perf batch C; perf batch D (d2efd83..60ddb69) then
  settled the item lifecycle, restyle, first-frame and click rows by A/B (below).
- Same commands as "End of milestone 4d": three sequential runs of the whole binary offscreen and three under Xvfb
  (llvmpipe, CPU side only), `-median 5`.
- Load average (1/5/15 min) at the start of each run: offscreen 2.01, 2.08, 2.56; 0.80, 1.48, 2.22; 0.48, 1.04, 1.89. Xvfb 0.62, 0.87, 1.65;
  5.02, 2.65, 2.14; 2.52, 2.76, 2.30.
- Ratio = end-of-milestone-4e median / end-of-milestone-4d median. New rows (Task 1) have no 4d number.
- The rows over their gate in this table (item add/remove/clear/delete 1.14-1.28, first frame 1.27, click 1.33,
  restyle 1.71) were fixed by perf batch D and settled by interleaved A/B against a build of 4892174 (the code
  before the milestone, with its bench rows), five pinned pairs, CPU ticks: Add10k +1.7 %, AddRemove10k -1.6 %,
  AddRemove20k -1.6 %, Clear10k -0.4 %, DeleteEach10k -5.3 %, DeletePlot10k -0.7 %, FirstFrame10k +0.7 %,
  RemoveEach10k -3.1 %, RestyleAll10k -0.7 %, Click100 +23 % (accepted, ruling R-12 in milestone-4e-rulings.md).
- Steady state (`-iterations 1500`, pinned) against the same base: QCPItems_Pan10k +1.1 %, Zoom10k +0.8 %,
  Replot10k +0.4 %, MixedKinds_Pan1k +4.4 % (+5.6 % in a second A/B: at the gate, accepted in R-12).
- Ported classes against the old code (A/B, CPU ticks): QCPErrorBars_Pan10k -35 %, QCPFinancial_Pan10k -55 %,
  QCPStatisticalBox_Pan1k -29 %, QCPIntervals_Pan10k +0.4 %, QCPIntervals_Pan10kLabelled +1.4 %,
  QCPSpans_Pan100 level after 989976b.

| Benchmark | end 4d msecs | end 4e msecs | runs | ratio |
|---|---|---|---|---|
| QCPAxisRects_Stack8_Replot | 1.9 | 1.8 | 1.8 / 1.8 / 1.8 / 1.8 / 1.8 / 1.8 | 0.95 |
| QCPAxis_SetRange10k | 1.3 | 1.3 | 1.3 / 1.3 / 1.3 / 1.4 / 1.3 / 1.3 | 1.00 |
| QCPAxis_SetTickLength10kItems | 0.012 | 0.012 | 0.011 / 0.012 / 0.012 / 0.013 / 0.012 / 0.012 | 1.00 |
| QCPAxis_TickLabels | 0.25 | 0.24 | 0.24 / 0.24 / 0.24 / 0.24 / 0.24 / 0.24 | 0.96 |
| QCPAxis_TickLabelsCached | 0.089 | 0.087 | 0.088 / 0.091 / 0.089 / 0.087 / 0.087 / 0.087 | 0.98 |
| QCPBars_Pan10k | 0.14 | 0.14 | 0.14 / 0.13 / 0.13 / 0.13 / 0.14 / 0.14 | 0.96 |
| QCPBars_Zoom10k | 5.7 | 5 | 5.2 / 5 / 5 / 5.2 / 4.9 / 5 | 0.88 |
| QCPColorMap2_ResampleDouble | 240.0 | 2.2e+02 | 2.3e+02 / 2.2e+02 / 2.2e+02 / 2.2e+02 / 2.2e+02 / 2.2e+02 | 0.92 |
| QCPColorMap2_ResampleFloat | 150.0 | 1.4e+02 | 1.4e+02 / 1.4e+02 / 1.4e+02 / 1.4e+02 / 1.3e+02 / 1.3e+02 | 0.91 |
| QCPColorMap2_ResamplePanSequence | 230.0 | 1.8e+02 | 1.8e+02 / 2e+02 / 1.7e+02 / 1.7e+02 / 2e+02 / 1.8e+02 | 0.77 |
| QCPColorMap2_ResamplePanSequenceSmall | 210.0 | 2e+02 | 2.1e+02 / 2.1e+02 / 2e+02 / 2e+02 / 2e+02 / 2e+02 | 0.97 |
| QCPColorMap2_ResampleWideParallel | 290.0 | 2.7e+02 | 2.8e+02 / 2.6e+02 / 2.8e+02 / 2.8e+02 / 2.7e+02 / 2.6e+02 | 0.94 |
| QCPColorMap2_ResampleWideSerial | 790.0 | 7.7e+02 | 7.9e+02 / 7.6e+02 / 7.8e+02 / 7.6e+02 / 7.7e+02 / 7.6e+02 | 0.97 |
| QCPColorMapData_ConstructLarge | 1.4 | 1.1 | 1.3 / 1.1 / 1.1 / 1.1 / 1.1 / 1.1 | 0.79 |
| QCPColorMap_ColorizeMap | 0.16 | 0.16 | 0.16 / 0.16 / 0.16 / 0.16 / 0.16 / 0.16 | 1.00 |
| QCPColorMap_SetCell1M | 2.4 | 2.6 | 2.6 / 2.6 / 2.6 / 2.6 / 2.6 / 2.6 | 1.08 |
| QCPColorMap_Standard | 0.27 | 0.27 | 0.27 / 0.27 / 0.27 / 0.27 / 0.27 / 0.27 | 1.00 |
| QCPColorScale_Rescale8 | 0.00016 | 0.00015 | 0.00015 / 0.00015 / 0.00015 / 0.00016 / 0.00015 / 0.00015 | 0.94 |
| QCPCoords_CoordToPixel1M | 5.0 | 4.3 | 4.3 / 4.3 / 4.2 / 4.2 / 4.3 / 4.3 | 0.86 |
| QCPDrawContext_ReplayData1M | 1.6 | 1.3 | 1.3 / 1.3 / 1.3 / 1.4 / 1.4 / 1.3 | 0.81 |
| QCPDrawContext_ReplayMarkers100k | 12.0 | 12 | 12 / 12 / 12 / 12 / 12 / 12 | 1.00 |
| QCPDrawn_Pan10k | 0.25 | 0.23 | 0.23 / 0.22 / 0.23 | 0.92 |
| QCPDrawn_Pan10kOneRecording | 0.28 | 0.26 | 0.26 / 0.26 / 0.27 | 0.93 |
| QCPDrawn_Zoom10k | 0.32 | 0.28 | 0.28 / 0.28 / 0.28 | 0.88 |
| QCPErrorBars_Pan10k | new | 0.24 | 0.24 / 0.24 / 0.24 / 0.24 / 0.24 / 0.24 | - |
| QCPFinancial_Pan10k | new | 0.14 | 0.14 / 0.14 / 0.14 / 0.14 / 0.14 / 0.14 | - |
| QCPGpu_Bars10k | 4.4 | 4.3 | 4.3 / 4.3 / 4.3 | 0.98 |
| QCPGpu_BarsPan10k | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGpu_ColorMap2Pan | 0.19 | 0.18 | 0.18 / 0.18 / 0.18 | 0.95 |
| QCPGpu_ColorMap2Zoom | 0.24 | 0.22 | 0.22 / 0.22 / 0.23 | 0.92 |
| QCPGpu_ColorMapPan | 0.17 | 0.17 | 0.17 / 0.17 / 0.17 | 1.00 |
| QCPGpu_CurvePan100k | 0.15 | 0.11 | 0.11 / 0.11 / 0.12 | 0.73 |
| QCPGpu_Graph2Render1M | 0.13 | 0.12 | 0.12 / 0.13 / 0.12 | 0.92 |
| QCPGpu_Histogram2DPan | 0.18 | 0.17 | 0.17 / 0.17 / 0.17 | 0.94 |
| QCPGpu_HistogramPan | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGpu_LineDashed100k | 0.52 | 0.43 | 0.43 / 0.44 / 0.43 | 0.83 |
| QCPGpu_LineSolid100k | 0.46 | 0.44 | 0.45 / 0.44 / 0.44 | 0.96 |
| QCPGpu_MixedLayer | 9.2 | 8 | 8 / 8.6 / 8 | 0.87 |
| QCPGpu_MultiGraphColored100kPan | 0.23 | 0.2 | 0.2 / 0.2 / 0.2 | 0.87 |
| QCPGpu_MultiGraphPan1M | 0.16 | 0.15 | 0.15 / 0.15 / 0.15 | 0.94 |
| QCPGpu_MultiGraphZoom1M | 0.82 | 0.68 | 0.68 / 0.69 / 0.68 | 0.83 |
| QCPGpu_Pan100k | 0.25 | 0.22 | 0.22 / 0.22 / 0.21 | 0.88 |
| QCPGpu_Text1k | 1.1 | 0.84 | 0.85 / 0.84 / 0.83 | 0.76 |
| QCPGraph2_RenderDouble | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 / 0.14 / 0.14 / 0.14 | 1.00 |
| QCPGraph2_RenderFloat | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 / 0.14 / 0.14 / 0.14 | 1.00 |
| QCPGraph2_RenderManyLines | 0.45 | 0.44 | 0.44 / 0.44 / 0.44 / 0.44 / 0.44 / 0.44 | 0.98 |
| QCPGraph2_ScatterDisc1M | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 / 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGraph2_ScatterSquare1M | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 / 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGraph2_ScatterWithLine1M | 0.12 | 0.12 | 0.12 / 0.13 / 0.12 / 0.13 / 0.12 / 0.13 | 1.04 |
| QCPGraph2_SetDataDouble | 9.7 | 9.5 | 9.6 / 9.5 / 9.4 / 9.6 / 9.5 / 9.3 | 0.98 |
| QCPGraph2_SetDataFloat | 8.3 | 8.3 | 8.1 / 8.2 / 8.2 / 8.6 / 8.5 / 8.7 | 1.01 |
| QCPGraph_AddDataAtBeginSorted | 5.4 | 5.4 | 5.2 / 5.6 / 5.6 / 5.3 / 5.5 / 5.2 | 1.00 |
| QCPGraph_AddDataAtBeginUnsorted | 29.0 | 28 | 28 / 28 / 28 / 28 / 28 / 28 | 0.97 |
| QCPGraph_AddDataAtEndSorted | 5.2 | 5 | 4.9 / 4.9 / 5.3 / 5 / 5 / 5 | 0.96 |
| QCPGraph_AddDataAtEndUnsorted | 23.0 | 23 | 23 / 23 / 23 / 23 / 23 / 23 | 1.00 |
| QCPGraph_AddDataMixedSorted | 11.0 | 11 | 11 / 11 / 10 / 11 / 11 / 10 | 1.00 |
| QCPGraph_AddDataMixedUnsorted | 29.0 | 29 | 29 / 29 / 29 / 30 / 29 / 29 | 1.00 |
| QCPGraph_ManyLines | 1.3 | 1.3 | 1.3 / 1.3 / 1.3 / 1.3 / 1.3 / 1.3 | 1.00 |
| QCPGraph_ManyOffScreenLines | 0.15 | 0.15 | 0.15 / 0.15 / 0.15 / 0.15 / 0.15 / 0.15 | 1.00 |
| QCPGraph_ManyPoints | 2.6 | 2.6 | 2.6 / 2.6 / 2.6 / 2.7 / 2.6 / 2.6 | 1.00 |
| QCPGraph_Standard | 0.71 | 0.68 | 0.67 / 0.7 / 0.68 / 0.68 / 0.68 / 0.67 | 0.96 |
| QCPInput_MouseMoveStorm | 3.9 | 3.8 | 3.8 / 3.8 / 4.2 / 3.8 / 3.7 / 3.8 | 0.97 |
| QCPInput_WheelOverGraph2_1M | 0.00068 | 0.00061 | 0.00063 / 0.00061 / 0.00061 / 0.00061 / 0.00061 / 0.00063 | 0.90 |
| QCPIntervals_Pan10k | new | 0.32 | 0.32 / 0.33 / 0.32 / 0.32 / 0.32 / 0.32 | - |
| QCPIntervals_Pan10kLabelled | new | 0.32 | 0.32 / 0.33 / 0.32 / 0.32 / 0.32 / 0.32 | - |
| QCPItemText_Zoom1k | 0.93 | 0.65 | 0.64 / 0.64 / 0.65 / 0.65 / 0.65 / 0.65 | 0.70 |
| QCPItems_Add10k | 20.0 | 23 | 23 / 22 / 22 / 23 / 23 / 23 | 1.15 |
| QCPItems_AddRemove10k | 29.0 | 35 | 35 / 34 / 35 / 35 / 35 / 36 | 1.21 |
| QCPItems_AddRemove20k | 69.0 | 80 | 82 / 80 / 79 / 80 / 82 / 80 | 1.16 |
| QCPItems_Clear10k | 9.4 | 12 | 12 / 13 / 12 / 12 / 12 / 12 | 1.28 |
| QCPItems_Click100 | 0.0042 | 0.0056 | 0.0056 / 0.0056 / 0.0055 / 0.0057 / 0.0056 / 0.0056 | 1.33 |
| QCPItems_Click10k | 0.053 | 0.056 | 0.056 / 0.055 / 0.055 / 0.056 / 0.055 / 0.056 | 1.05 |
| QCPItems_ClickEmpty10k | 0.051 | 0.052 | 0.051 / 0.051 / 0.052 / 0.052 / 0.053 / 0.052 | 1.02 |
| QCPItems_DeleteEach10k | 28.0 | 32 | 32 / 31 / 32 / 31 / 32 / 32 | 1.14 |
| QCPItems_DeletePlot10k | 28.0 | 35 | 33 / 36 / 35 / 35 / 35 / 33 | 1.25 |
| QCPItems_DragOne10k | 0.56 | 0.57 | 0.57 / 0.57 / 0.58 / 0.57 / 0.57 / 0.57 | 1.02 |
| QCPItems_FirstFrame10k | 30.0 | 38 | 38 / 40 / 38 / 37 / 37 / 38 | 1.27 |
| QCPItems_MixedKinds_Pan1k | new | 5.3 | 5.4 / 5.2 / 5.3 / 5.4 / 5.4 / 5.3 | - |
| QCPItems_Pan10k | 0.42 | 0.42 | 0.42 / 0.42 / 0.42 / 0.42 / 0.42 / 0.42 | 1.00 |
| QCPItems_RemoveEach10k | 33.0 | 38 | 39 / 38 / 37 / 38 / 41 / 38 | 1.15 |
| QCPItems_Replot10k | 0.43 | 0.43 | 0.43 / 0.43 / 0.43 / 0.43 / 0.43 / 0.43 | 1.00 |
| QCPItems_RestyleAll10k | 1.9 | 3.2 | 3.3 / 3.2 / 3.2 / 3.4 / 3.1 / 3.5 | 1.71 |
| QCPItems_Tracers_Pan1k | new | 1.4 | 1.4 / 1.4 / 1.4 / 1.4 / 1.4 / 1.4 | - |
| QCPItems_Zoom100 | 0.41 | 0.4 | 0.4 / 0.4 / 0.41 / 0.4 / 0.41 / 0.4 | 0.98 |
| QCPItems_Zoom10k | 0.43 | 0.43 | 0.43 / 0.43 / 0.43 / 0.43 / 0.43 / 0.43 | 1.00 |
| QCPItems_ZoomEmpty | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 / 0.14 / 0.14 / 0.14 | 1.00 |
| QCPMultiGraph_SetData1M | 38.0 | 28 | 28 / 30 / 29 / 28 / 29 / 28 | 0.75 |
| QCPPlots_Stack8_Aligned_Replot | 0.27 | 0.26 | 0.26 / 0.26 / 0.26 / 0.26 / 0.26 / 0.26 | 0.96 |
| QCPPlots_Stack8_Replot | 0.27 | 0.26 | 0.26 / 0.26 / 0.26 / 0.26 / 0.26 / 0.26 | 0.96 |
| QCPSpans_Pan100 | new | 3.3 | 3.3 / 3.3 / 3.3 / 3.4 / 3.4 / 3.3 | - |
| QCPSpatialIndex_Build10k | 0.91 | 0.88 | 0.88 / 0.88 / 0.88 / 0.88 / 0.88 / 0.89 | 0.97 |
| QCPSpatialIndex_Query10k | 0.00037 | 0.00036 | 0.00036 / 0.00036 / 0.00036 / 0.00037 / 0.00036 / 0.00036 | 0.97 |
| QCPStatisticalBox_Pan1k | new | 0.13 | 0.13 / 0.13 / 0.13 / 0.13 / 0.14 / 0.14 | - |
| QCPTransform_Map1M | 0.88 | 0.66 | 0.71 / 0.62 / 0.65 / 0.62 / 0.66 / 0.66 | 0.74 |
| QObject_DeleteEachChild10k | 7.9 | 7.8 | 7.8 / 8.2 / 7.8 / 7.8 / 8 / 7.8 | 0.99 |

## End of milestone 4f

- Commit: a5db69d for the whole-table runs; a3d4399 (the axis hot block) then settled the rows over the gate by A/B.
- Three runs of the whole binary offscreen and three under Xvfb, `-median 5`. Load (1/5/15 min) at the start:
  offscreen 4.39, 3.04, 3.54; 2.02, 2.59, 3.26; 1.49, 2.06, 2.90. Xvfb 0.64, 1.44, 2.49; 2.28, 1.96, 2.45; 2.73, 2.33, 2.47.
- Ratio = end-of-milestone-4f median / end-of-milestone-4e median. Over the gate in this table:
  QCPCoords_CoordToPixel1M 1.31 (real: fixed in a3d4399, A/B vs fd60df3 now 1.02), QCPColorMap_SetCell1M 1.15
  (code placement: identical instruction counts, level after the rebuild), QCPGpu_CurvePan100k 1.09 and
  QCPGpu_Graph2Render1M 1.08 (two-digit rounding: A/B 1.006 and 0.99 under Xvfb).
- A/B against a build of fd60df3 (milestone base), five pairs, CPU ticks, after a3d4399: QCPItems_Pan10k 0.99,
  QCPAxis_TickLabels 0.99, QCPAxis_SetRange10k 1.05, QCPPolar_Draw10k/Rotate10k/RadialZoom10k/Export10k within
  -2..+1 % (vs f4d6120).

| Benchmark | end 4e msecs | end 4f msecs | runs | ratio |
|---|---|---|---|---|
| QCPAxisRects_Stack8_Replot | 1.8 | 1.8 | 1.8 / 1.8 / 1.8 / 1.8 / 1.8 / 1.8 | 1.00 |
| QCPAxis_SetRange10k | 1.3 | 1.3 | 1.4 / 1.3 / 1.3 / 1.3 / 1.3 / 1.3 | 1.00 |
| QCPAxis_SetTickLength10kItems | 0.012 | 0.011 | 0.012 / 0.011 / 0.011 / 0.011 / 0.011 / 0.011 | 0.92 |
| QCPAxis_TickLabels | 0.24 | 0.24 | 0.24 / 0.24 / 0.24 / 0.24 / 0.24 / 0.24 | 1.00 |
| QCPAxis_TickLabelsCached | 0.087 | 0.088 | 0.088 / 0.088 / 0.087 / 0.087 / 0.088 / 0.088 | 1.01 |
| QCPBars_Pan10k | 0.14 | 0.14 | 0.14 / 0.13 / 0.14 / 0.14 / 0.13 / 0.13 | 0.96 |
| QCPBars_Zoom10k | 5.0 | 5.1 | 5 / 5 / 5.1 / 5.2 / 5.1 / 5.1 | 1.02 |
| QCPColorMap2_ResampleDouble | 220.0 | 2.3e+02 | 2.3e+02 / 2.2e+02 / 2.2e+02 / 2.3e+02 / 2.3e+02 / 2.4e+02 | 1.05 |
| QCPColorMap2_ResampleFloat | 140.0 | 1.4e+02 | 1.4e+02 / 1.3e+02 / 1.4e+02 / 1.5e+02 / 1.4e+02 / 1.4e+02 | 1.01 |
| QCPColorMap2_ResamplePanSequence | 180.0 | 1.8e+02 | 1.8e+02 / 1.8e+02 / 1.9e+02 / 1.9e+02 / 1.8e+02 / 2e+02 | 1.01 |
| QCPColorMap2_ResamplePanSequenceSmall | 200.0 | 2e+02 | 2.1e+02 / 2e+02 / 2e+02 / 2.1e+02 / 2e+02 / 2e+02 | 1.02 |
| QCPColorMap2_ResampleWideParallel | 270.0 | 2.6e+02 | 2.6e+02 / 2.7e+02 / 2.6e+02 / 2.6e+02 / 2.8e+02 / 2.6e+02 | 0.98 |
| QCPColorMap2_ResampleWideSerial | 770.0 | 7.7e+02 | 7.7e+02 / 7.6e+02 / 7.7e+02 / 7.7e+02 / 7.6e+02 / 7.7e+02 | 1.00 |
| QCPColorMapData_ConstructLarge | 1.1 | 1.1 | 1.2 / 1.1 / 1.3 / 1.1 / 1.1 / 1.2 | 1.05 |
| QCPColorMap_ColorizeMap | 0.16 | 0.16 | 0.16 / 0.16 / 0.16 / 0.16 / 0.16 / 0.16 | 1.00 |
| QCPColorMap_SetCell1M | 2.6 | 3 | 3 / 3 / 3 / 2.9 / 3 / 3 | 1.15 |
| QCPColorMap_Standard | 0.27 | 0.27 | 0.27 / 0.27 / 0.26 / 0.27 / 0.27 / 0.27 | 1.00 |
| QCPColorScale_Rescale8 | 0.00015 | 0.00015 | 0.00015 / 0.00015 / 0.00015 / 0.00015 / 0.00015 / 0.00016 | 1.00 |
| QCPCoords_CoordToPixel1M | 4.3 | 5.7 | 5.6 / 5.6 / 5.7 / 5.6 / 5.8 / 5.7 | 1.31 |
| QCPDrawContext_ReplayData1M | 1.3 | 1.4 | 1.4 / 1.4 / 1.4 / 1.3 / 1.3 / 1.4 | 1.08 |
| QCPDrawContext_ReplayMarkers100k | 12.0 | 12 | 12 / 12 / 12 / 12 / 12 / 12 | 1.00 |
| QCPDrawn_Pan10k | 0.23 | 0.23 | 0.23 / 0.23 / 0.23 | 1.00 |
| QCPDrawn_Pan10kOneRecording | 0.26 | 0.26 | 0.26 / 0.26 / 0.27 | 1.00 |
| QCPDrawn_Zoom10k | 0.28 | 0.28 | 0.29 / 0.28 / 0.28 | 1.00 |
| QCPErrorBars_Pan10k | 0.24 | 0.24 | 0.24 / 0.24 / 0.24 / 0.25 / 0.24 / 0.24 | 1.00 |
| QCPFinancial_Pan10k | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 / 0.14 / 0.14 / 0.14 | 1.00 |
| QCPGpu_Bars10k | 4.3 | 4.4 | 4.4 / 4.3 / 4.4 | 1.02 |
| QCPGpu_BarsPan10k | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGpu_ColorMap2Pan | 0.18 | 0.18 | 0.18 / 0.18 / 0.18 | 1.00 |
| QCPGpu_ColorMap2Zoom | 0.22 | 0.22 | 0.23 / 0.22 / 0.22 | 1.00 |
| QCPGpu_ColorMapPan | 0.17 | 0.17 | 0.17 / 0.17 / 0.17 | 1.00 |
| QCPGpu_CurvePan100k | 0.11 | 0.12 | 0.12 / 0.12 / 0.12 | 1.09 |
| QCPGpu_Graph2Render1M | 0.12 | 0.13 | 0.13 / 0.13 / 0.13 | 1.08 |
| QCPGpu_Histogram2DPan | 0.17 | 0.17 | 0.17 / 0.17 / 0.17 | 1.00 |
| QCPGpu_HistogramPan | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGpu_LineDashed100k | 0.43 | 0.44 | 0.44 / 0.44 / 0.44 | 1.02 |
| QCPGpu_LineSolid100k | 0.44 | 0.44 | 0.44 / 0.45 / 0.44 | 1.00 |
| QCPGpu_MixedLayer | 8.0 | 8.4 | 8.4 / 8.6 / 8.2 | 1.05 |
| QCPGpu_MultiGraphColored100kPan | 0.2 | 0.2 | 0.2 / 0.2 / 0.2 | 1.00 |
| QCPGpu_MultiGraphPan1M | 0.15 | 0.15 | 0.15 / 0.15 / 0.15 | 1.00 |
| QCPGpu_MultiGraphZoom1M | 0.68 | 0.7 | 0.7 / 0.71 / 0.68 | 1.03 |
| QCPGpu_Pan100k | 0.22 | 0.22 | 0.22 / 0.22 / 0.22 | 1.00 |
| QCPGpu_Text1k | 0.84 | 0.85 | 0.85 / 0.88 / 0.83 | 1.01 |
| QCPGraph2_RenderDouble | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 / 0.14 / 0.14 / 0.14 | 1.00 |
| QCPGraph2_RenderFloat | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 / 0.14 / 0.14 / 0.14 | 1.00 |
| QCPGraph2_RenderManyLines | 0.44 | 0.44 | 0.44 / 0.44 / 0.44 / 0.44 / 0.44 / 0.44 | 1.00 |
| QCPGraph2_ScatterDisc1M | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 / 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGraph2_ScatterSquare1M | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 / 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGraph2_ScatterWithLine1M | 0.12 | 0.12 | 0.12 / 0.13 / 0.12 / 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGraph2_SetDataDouble | 9.5 | 9.6 | 9.7 / 9.3 / 9.7 / 9.5 / 9.4 / 9.6 | 1.01 |
| QCPGraph2_SetDataFloat | 8.3 | 8.3 | 8.3 / 8.3 / 8.3 / 8.3 / 8.4 / 8.3 | 1.00 |
| QCPGraph_AddDataAtBeginSorted | 5.4 | 5.3 | 5.3 / 5.3 / 5.3 / 5.8 / 5.8 / 5.2 | 0.98 |
| QCPGraph_AddDataAtBeginUnsorted | 28.0 | 28 | 28 / 28 / 28 / 28 / 28 / 28 | 1.00 |
| QCPGraph_AddDataAtEndSorted | 5.0 | 5 | 5.1 / 5 / 4.9 / 4.9 / 5 / 4.9 | 0.99 |
| QCPGraph_AddDataAtEndUnsorted | 23.0 | 23 | 24 / 23 / 23 / 23 / 23 / 23 | 1.00 |
| QCPGraph_AddDataMixedSorted | 11.0 | 10 | 10 / 11 / 10 / 11 / 11 / 10 | 0.95 |
| QCPGraph_AddDataMixedUnsorted | 29.0 | 29 | 29 / 29 / 29 / 29 / 29 / 29 | 1.00 |
| QCPGraph_ManyLines | 1.3 | 1.3 | 1.3 / 1.3 / 1.3 / 1.3 / 1.3 / 1.3 | 1.00 |
| QCPGraph_ManyOffScreenLines | 0.15 | 0.15 | 0.15 / 0.16 / 0.15 / 0.15 / 0.15 / 0.15 | 1.00 |
| QCPGraph_ManyPoints | 2.6 | 2.6 | 2.6 / 2.6 / 2.6 / 2.6 / 2.6 / 2.6 | 1.00 |
| QCPGraph_Standard | 0.68 | 0.68 | 0.68 / 0.7 / 0.67 / 0.68 / 0.67 / 0.69 | 1.00 |
| QCPInput_MouseMoveStorm | 3.8 | 3.8 | 3.8 / 3.7 / 4 / 3.7 / 3.8 / 3.7 | 0.99 |
| QCPInput_WheelOverGraph2_1M | 0.00061 | 0.00064 | 0.00064 / 0.00064 / 0.00063 / 0.00063 / 0.00064 / 0.00067 | 1.05 |
| QCPIntervals_Pan10k | 0.32 | 0.32 | 0.31 / 0.32 / 0.32 / 0.32 / 0.33 / 0.31 | 1.00 |
| QCPIntervals_Pan10kLabelled | 0.32 | 0.32 | 0.32 / 0.33 / 0.32 / 0.32 / 0.32 / 0.31 | 1.00 |
| QCPItemText_Zoom1k | 0.65 | 0.66 | 0.66 / 0.65 / 0.67 / 0.66 / 0.66 / 0.65 | 1.02 |
| QCPItems_Add10k | 23.0 | 19 | 19 / 20 / 19 / 19 / 19 / 19 | 0.83 |
| QCPItems_AddRemove10k | 35.0 | 28 | 29 / 27 / 29 / 27 / 27 / 30 | 0.80 |
| QCPItems_AddRemove20k | 80.0 | 65 | 64 / 65 / 65 / 67 / 66 / 64 | 0.81 |
| QCPItems_Clear10k | 12.0 | 8 | 8.3 / 8.5 / 7.7 / 7.6 / 7.6 / 8.4 | 0.67 |
| QCPItems_Click100 | 0.0056 | 0.0051 | 0.005 / 0.005 / 0.0051 / 0.005 / 0.0051 / 0.0051 | 0.90 |
| QCPItems_Click10k | 0.056 | 0.053 | 0.053 / 0.053 / 0.053 / 0.053 / 0.053 / 0.054 | 0.95 |
| QCPItems_ClickEmpty10k | 0.052 | 0.05 | 0.049 / 0.05 / 0.05 / 0.049 / 0.049 / 0.05 | 0.95 |
| QCPItems_DeleteEach10k | 32.0 | 24 | 24 / 23 / 25 / 24 / 24 / 23 | 0.75 |
| QCPItems_DeletePlot10k | 35.0 | 26 | 27 / 28 / 26 / 26 / 26 / 27 | 0.76 |
| QCPItems_DragOne10k | 0.57 | 0.56 | 0.55 / 0.56 / 0.56 / 0.56 / 0.56 / 0.55 | 0.98 |
| QCPItems_FirstFrame10k | 38.0 | 30 | 30 / 30 / 30 / 30 / 30 / 30 | 0.79 |
| QCPItems_MixedKinds_Pan1k | 5.3 | 5.3 | 5.3 / 5.4 / 5.2 / 5.5 / 5.2 / 5.5 | 1.01 |
| QCPItems_Pan10k | 0.42 | 0.42 | 0.42 / 0.42 / 0.42 / 0.42 / 0.42 / 0.42 | 1.00 |
| QCPItems_RemoveEach10k | 38.0 | 30 | 30 / 32 / 30 / 30 / 30 / 30 | 0.79 |
| QCPItems_Replot10k | 0.43 | 0.42 | 0.42 / 0.42 / 0.42 / 0.42 / 0.42 / 0.42 | 0.98 |
| QCPItems_RestyleAll10k | 3.2 | 1.4 | 1.4 / 1.4 / 1.4 / 1.8 / 1.8 / 1.5 | 0.45 |
| QCPItems_Tracers_Pan1k | 1.4 | 1.4 | 1.4 / 1.4 / 1.4 / 1.4 / 1.4 / 1.4 | 1.00 |
| QCPItems_Zoom100 | 0.4 | 0.4 | 0.41 / 0.41 / 0.4 / 0.4 / 0.4 / 0.4 | 1.00 |
| QCPItems_Zoom10k | 0.43 | 0.42 | 0.43 / 0.42 / 0.42 / 0.43 / 0.43 / 0.42 | 0.99 |
| QCPItems_ZoomEmpty | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 / 0.14 / 0.14 / 0.14 | 1.00 |
| QCPMultiGraph_SetData1M | 28.0 | 30 | 33 / 33 / 33 / 28 / 28 / 28 | 1.09 |
| QCPPlots_Stack8_Aligned_Replot | 0.26 | 0.26 | 0.26 / 0.26 / 0.26 / 0.26 / 0.27 / 0.26 | 1.00 |
| QCPPlots_Stack8_Replot | 0.26 | 0.26 | 0.26 / 0.26 / 0.26 / 0.26 / 0.26 / 0.26 | 1.00 |
| QCPPlottables_Pan200 | new | 9.2 | 9.2 / 9.2 / 9.3 / 9.3 / 9.2 / 9.3 | - |
| QCPPlottables_Zoom200 | new | 9.1 | 9.2 / 9.3 / 9.1 / 9.1 / 9.2 / 9.1 | - |
| QCPPolar_Draw10k | new | 0.93 | 0.91 / 0.93 / 0.92 / 0.93 / 0.92 / 0.93 | - |
| QCPPolar_Export10k | new | 0.95 | 0.96 / 0.94 / 0.95 / 0.94 / 0.95 / 0.95 | - |
| QCPPolar_RadialZoom10k | new | 0.9 | 0.9 / 0.91 / 0.91 / 0.9 / 0.9 / 0.9 | - |
| QCPPolar_Rotate10k | new | 0.91 | 0.91 / 0.91 / 0.9 / 0.91 / 0.91 / 0.91 | - |
| QCPSpans_Pan100 | 3.3 | 3.4 | 3.3 / 3.4 / 3.4 / 3.4 / 3.5 / 3.3 | 1.03 |
| QCPSpatialIndex_Build10k | 0.88 | 0.88 | 0.87 / 0.88 / 0.88 / 0.88 / 0.89 / 0.87 | 1.00 |
| QCPSpatialIndex_Query10k | 0.00036 | 0.00036 | 0.00036 / 0.00036 / 0.00037 / 0.00036 / 0.00036 / 0.00036 | 1.00 |
| QCPStatisticalBox_Pan1k | 0.13 | 0.14 | 0.13 / 0.14 / 0.14 / 0.14 / 0.13 / 0.13 | 1.04 |
| QCPTransform_Map1M | 0.66 | 0.69 | 0.62 / 0.72 / 0.62 / 0.68 / 0.71 / 0.69 | 1.04 |
| QObject_DeleteEachChild10k | 7.8 | 7.8 | 7.8 / 7.9 / 7.8 / 7.7 / 7.7 / 7.8 | 1.00 |

## End of milestone 4g

- Commit: 39d7028 for the whole-table runs. Three runs of the whole binary offscreen and three under Xvfb,
  `-median 5`. Load (1/5/15 min) at the start: offscreen 15.18, 15.68, 15.07; 4.43, 10.00, 12.94; 2.95, 5.99, 10.63. Xvfb 2.67, 4.06, 8.84;
  8.07, 11.29, 10.76; 4.23, 7.47, 9.29. Other sessions loaded the machine all afternoon.
- Ratio = end-of-milestone-4g median / end-of-milestone-4f median. Most rows read 10-50 % slow, an untouched
  control row (QCPColorMapData_ConstructLarge 1.27) included, so the table is load, not code.
- A/B against a build of 048ad21 (milestone base), CPU ticks, steady state: QCPTransform_Map1M +1 % (eight
  pairs), QCPDrawContext_ReplayData1M, QCPPlottables_Pan200, QCPDrawn_Zoom10k, QCPItems_Pan10k,
  QCPItems_Replot10k, QCPCoords_CoordToPixel1M, QCPPolar_Draw10k and the control level;
  QCPAxis_SetRange10k +5 % after 39d7028 (it was +55-60 % before: an emission scope per range set).

| Benchmark | end 4f msecs | end 4g msecs | runs | ratio |
|---|---|---|---|---|
| QCPAxisRects_Stack8_Replot | 1.8 | 1.9 | 1.8 / 1.9 / 1.9 / 2.2 / 1.8 / 1.8 | 1.03 |
| QCPAxis_SetRange10k | 1.3 | 1.5 | 1.5 / 1.4 / 1.7 / 1.5 / 1.5 / 1.5 | 1.15 |
| QCPAxis_SetTickLength10kItems | 0.011 | 0.012 | 0.012 / 0.013 / 0.012 / 0.012 / 0.012 / 0.012 | 1.09 |
| QCPAxis_TickLabels | 0.24 | 0.24 | 0.25 / 0.24 / 0.25 / 0.25 / 0.24 / 0.24 | 1.02 |
| QCPAxis_TickLabelsCached | 0.088 | 0.092 | 0.09 / 0.093 / 0.093 / 0.091 / 0.09 / 0.094 | 1.05 |
| QCPBars_Pan10k | 0.14 | 0.14 | 0.14 / 0.14 / 0.13 / 0.21 / 0.14 / 0.14 | 1.00 |
| QCPBars_Zoom10k | 5.1 | 5.5 | 5.2 / 6.1 / 5.1 / 20 / 5.8 / 5.3 | 1.09 |
| QCPColorMap2_ResampleDouble | 230.0 | 2.3e+02 | 3.6e+02 / 2.3e+02 / 2.2e+02 / 2.3e+02 / 2.2e+02 / 2.5e+02 | 1.00 |
| QCPColorMap2_ResampleFloat | 140.0 | 1.4e+02 | 1.6e+02 / 1.4e+02 / 1.3e+02 / 1.4e+02 / 1.4e+02 / 1.5e+02 | 1.02 |
| QCPColorMap2_ResamplePanSequence | 180.0 | 2.3e+02 | 2.6e+02 / 2.3e+02 / 2.2e+02 / 2.3e+02 / 2.2e+02 / 2.4e+02 | 1.28 |
| QCPColorMap2_ResamplePanSequenceSmall | 200.0 | 2.1e+02 | 2.2e+02 / 2.1e+02 / 2.1e+02 / 2.1e+02 / 2.1e+02 / 2.1e+02 | 1.04 |
| QCPColorMap2_ResampleWideParallel | 260.0 | 2.9e+02 | 3.1e+02 / 2.8e+02 / 3e+02 / 2.9e+02 / 3e+02 / 2.8e+02 | 1.13 |
| QCPColorMap2_ResampleWideSerial | 770.0 | 7.9e+02 | 8.8e+02 / 7.8e+02 / 8.1e+02 / 7.8e+02 / 8e+02 / 7.7e+02 | 1.03 |
| QCPColorMapData_ConstructLarge | 1.1 | 1.4 | 1.9 / 1.1 / 1.3 / 1.7 / 1.2 / 1.5 | 1.27 |
| QCPColorMap_ColorizeMap | 0.16 | 0.16 | 0.47 / 0.16 / 0.16 / 0.17 / 0.16 / 0.16 | 1.00 |
| QCPColorMap_SetCell1M | 3.0 | 3 | 3 / 3 / 3 / 4.4 / 3 / 3 | 1.00 |
| QCPColorMap_Standard | 0.27 | 0.27 | 0.53 / 0.27 / 0.27 / 0.28 / 0.27 / 0.27 | 1.00 |
| QCPColorScale_Rescale8 | 0.00015 | 0.00016 | 0.00016 / 0.00016 / 0.00017 / 0.00016 / 0.00015 / 0.00016 | 1.07 |
| QCPCoords_CoordToPixel1M | 5.7 | 4.5 | 4.4 / 4.5 / 4.1 / 4.8 / 4.6 / 4.2 | 0.78 |
| QCPDrawContext_ReplayData1M | 1.4 | 2 | 1.7 / 2.5 / 1.5 / 3.6 / 1.5 / 2.4 | 1.46 |
| QCPDrawContext_ReplayMarkers100k | 12.0 | 13 | 13 / 14 / 12 / 21 / 13 / 12 | 1.07 |
| QCPDrawn_Pan10k | 0.23 | 0.24 | 0.24 / 0.24 / 0.24 | 1.04 |
| QCPDrawn_Pan10kOneRecording | 0.26 | 0.3 | 0.36 / 0.29 / 0.3 | 1.15 |
| QCPDrawn_Zoom10k | 0.28 | 0.34 | 0.67 / 0.34 / 0.29 | 1.21 |
| QCPErrorBars_Pan10k | 0.24 | 0.25 | 0.24 / 0.26 / 0.25 / 0.28 / 0.25 / 0.24 | 1.04 |
| QCPFinancial_Pan10k | 0.14 | 0.14 | 0.14 / 0.14 / 0.15 / 0.14 / 0.16 / 0.14 | 1.00 |
| QCPGpu_Bars10k | 4.4 | 4.6 | 16 / 4.5 / 4.6 | 1.05 |
| QCPGpu_BarsPan10k | 0.12 | 0.13 | 0.13 / 0.14 / 0.13 | 1.08 |
| QCPGpu_ColorMap2Pan | 0.18 | 0.2 | 0.23 / 0.19 / 0.2 | 1.10 |
| QCPGpu_ColorMap2Zoom | 0.22 | 0.28 | 0.28 / 0.23 / 0.29 | 1.27 |
| QCPGpu_ColorMapPan | 0.17 | 0.22 | 0.25 / 0.22 / 0.18 | 1.29 |
| QCPGpu_CurvePan100k | 0.12 | 0.12 | 0.38 / 0.12 / 0.12 | 1.00 |
| QCPGpu_Graph2Render1M | 0.13 | 0.14 | 0.14 / 0.13 / 0.42 | 1.08 |
| QCPGpu_Histogram2DPan | 0.17 | 0.25 | 0.25 / 0.22 / 0.64 | 1.47 |
| QCPGpu_HistogramPan | 0.12 | 0.14 | 0.16 / 0.14 / 0.13 | 1.17 |
| QCPGpu_LineDashed100k | 0.44 | 0.5 | 0.5 / 0.44 / 1.2 | 1.14 |
| QCPGpu_LineSolid100k | 0.44 | 0.56 | 0.56 / 0.44 / 2.1 | 1.27 |
| QCPGpu_MixedLayer | 8.4 | 9.7 | 9.7 / 9.6 / 42 | 1.15 |
| QCPGpu_MultiGraphColored100kPan | 0.2 | 0.21 | 0.37 / 0.2 / 0.21 | 1.05 |
| QCPGpu_MultiGraphPan1M | 0.15 | 0.16 | 0.16 / 0.15 / 0.16 | 1.07 |
| QCPGpu_MultiGraphZoom1M | 0.7 | 0.7 | 0.91 / 0.69 / 0.7 | 1.00 |
| QCPGpu_Pan100k | 0.22 | 0.26 | 0.26 / 0.23 / 0.6 | 1.18 |
| QCPGpu_Text1k | 0.85 | 1.7 | 1.7 / 1.2 / 2.6 | 2.00 |
| QCPGraph2_RenderDouble | 0.14 | 0.14 | 0.15 / 0.14 / 0.14 / 0.14 / 0.16 / 0.14 | 1.00 |
| QCPGraph2_RenderFloat | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 / 0.14 / 0.14 / 0.14 | 1.00 |
| QCPGraph2_RenderManyLines | 0.44 | 0.45 | 0.48 / 0.45 / 0.45 / 0.45 / 0.51 / 0.45 | 1.02 |
| QCPGraph2_ScatterDisc1M | 0.12 | 0.12 | 0.18 / 0.12 / 0.12 / 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGraph2_ScatterSquare1M | 0.12 | 0.12 | 0.2 / 0.12 / 0.12 / 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGraph2_ScatterWithLine1M | 0.12 | 0.12 | 0.13 / 0.12 / 0.12 / 0.12 / 0.13 / 0.13 | 1.04 |
| QCPGraph2_SetDataDouble | 9.6 | 10 | 11 / 9.6 / 10 / 9.4 / 13 / 10 | 1.04 |
| QCPGraph2_SetDataFloat | 8.3 | 8.6 | 10 / 8.3 / 8.6 / 8.5 / 9.2 / 8.6 | 1.04 |
| QCPGraph_AddDataAtBeginSorted | 5.3 | 5.7 | 8.2 / 5.4 / 5.5 / 5.2 / 6.4 / 5.8 | 1.07 |
| QCPGraph_AddDataAtBeginUnsorted | 28.0 | 29 | 31 / 29 / 29 / 29 / 30 / 28 | 1.04 |
| QCPGraph_AddDataAtEndSorted | 5.0 | 5.2 | 5.2 / 5.1 / 5.1 / 4.9 / 6.8 / 5.3 | 1.03 |
| QCPGraph_AddDataAtEndUnsorted | 23.0 | 23 | 25 / 23 / 23 / 23 / 23 / 23 | 1.00 |
| QCPGraph_AddDataMixedSorted | 10.0 | 11 | 14 / 11 / 11 / 10 / 11 / 12 | 1.10 |
| QCPGraph_AddDataMixedUnsorted | 29.0 | 30 | 33 / 30 / 29 / 29 / 29 / 30 | 1.02 |
| QCPGraph_ManyLines | 1.3 | 1.4 | 1.4 / 1.3 / 1.7 / 1.3 / 1.4 / 1.3 | 1.04 |
| QCPGraph_ManyOffScreenLines | 0.15 | 0.15 | 0.16 / 0.15 / 0.15 / 0.15 / 0.2 / 0.16 | 1.03 |
| QCPGraph_ManyPoints | 2.6 | 2.8 | 3.2 / 2.6 / 3.4 / 2.7 / 2.6 / 2.8 | 1.06 |
| QCPGraph_Standard | 0.68 | 0.8 | 0.99 / 0.71 / 0.71 / 0.68 / 0.89 / 0.89 | 1.18 |
| QCPInput_MouseMoveStorm | 3.8 | 3.7 | 3.7 / 3.7 / 4.2 / 4.1 / 3.7 / 3.7 | 0.97 |
| QCPInput_WheelOverGraph2_1M | 0.00064 | 0.00064 | 0.00067 / 0.00062 / 0.00064 / 0.00063 / 0.00064 / 0.00066 | 1.00 |
| QCPIntervals_Pan10k | 0.32 | 0.32 | 0.31 / 0.34 / 0.31 / 0.42 / 0.32 / 0.31 | 0.98 |
| QCPIntervals_Pan10kLabelled | 0.32 | 0.33 | 0.31 / 0.41 / 0.31 / 0.43 / 0.36 / 0.31 | 1.05 |
| QCPItemText_Zoom1k | 0.66 | 0.69 | 0.68 / 0.69 / 0.67 / 0.77 / 0.67 / 0.69 | 1.04 |
| QCPItems_Add10k | 19.0 | 22 | 22 / 29 / 22 / 22 / 22 / 22 | 1.16 |
| QCPItems_AddRemove10k | 28.0 | 32 | 32 / 35 / 32 / 55 / 31 / 31 | 1.14 |
| QCPItems_AddRemove20k | 65.0 | 72 | 71 / 80 / 73 / 89 / 72 / 71 | 1.12 |
| QCPItems_Clear10k | 8.0 | 9.2 | 8.5 / 9.7 / 9.9 / 9.4 / 7.9 / 9.1 | 1.16 |
| QCPItems_Click100 | 0.0051 | 0.0052 | 0.0051 / 0.0051 / 0.0052 / 0.0056 / 0.0052 / 0.0051 | 1.01 |
| QCPItems_Click10k | 0.053 | 0.056 | 0.055 / 0.06 / 0.056 / 0.056 / 0.054 / 0.054 | 1.05 |
| QCPItems_ClickEmpty10k | 0.05 | 0.051 | 0.055 / 0.051 / 0.05 / 0.054 / 0.05 / 0.05 | 1.01 |
| QCPItems_DeleteEach10k | 24.0 | 26 | 25 / 33 / 24 / 28 / 27 / 26 | 1.10 |
| QCPItems_DeletePlot10k | 26.0 | 30 | 30 / 37 / 29 / 31 / 29 / 31 | 1.17 |
| QCPItems_DragOne10k | 0.56 | 0.57 | 0.58 / 0.59 / 0.57 / 0.62 / 0.57 / 0.57 | 1.03 |
| QCPItems_FirstFrame10k | 30.0 | 33 | 32 / 35 / 33 / 33 / 31 / 33 | 1.10 |
| QCPItems_MixedKinds_Pan1k | 5.3 | 6.2 | 5.5 / 8 / 5.4 / 7.8 / 6.6 / 5.8 | 1.17 |
| QCPItems_Pan10k | 0.42 | 0.43 | 0.43 / 0.43 / 0.47 / 0.42 / 0.43 / 0.42 | 1.02 |
| QCPItems_RemoveEach10k | 30.0 | 36 | 36 / 46 / 31 / 36 / 33 / 38 | 1.20 |
| QCPItems_Replot10k | 0.42 | 0.43 | 0.44 / 0.49 / 0.43 / 0.45 / 0.43 / 0.42 | 1.04 |
| QCPItems_RestyleAll10k | 1.4 | 2.4 | 1.6 / 3.5 / 2.2 / 3.1 / 2.5 / 1.6 | 1.68 |
| QCPItems_Tracers_Pan1k | 1.4 | 1.6 | 1.5 / 1.7 / 1.4 / 2.4 / 1.9 / 1.5 | 1.16 |
| QCPItems_Zoom100 | 0.4 | 0.41 | 0.41 / 0.41 / 0.57 / 0.41 / 0.41 / 0.43 | 1.02 |
| QCPItems_Zoom10k | 0.42 | 0.43 | 0.44 / 0.5 / 0.46 / 0.43 / 0.43 / 0.43 | 1.04 |
| QCPItems_ZoomEmpty | 0.14 | 0.14 | 0.14 / 0.14 / 0.18 / 0.14 / 0.14 / 0.14 | 1.00 |
| QCPMultiGraph_SetData1M | 30.0 | 32 | 32 / 33 / 35 / 30 / 29 / 73 | 1.08 |
| QCPPlots_Stack8_Aligned_Replot | 0.26 | 0.26 | 0.26 / 0.26 / 0.26 / 0.26 / 0.26 / 0.26 | 1.00 |
| QCPPlots_Stack8_Replot | 0.26 | 0.26 | 0.26 / 0.26 / 0.26 / 0.26 / 0.26 / 0.27 | 1.00 |
| QCPPlottables_Pan200 | 9.2 | 13 | 9 / 13 / 8.9 / 12 / 13 / 18 | 1.36 |
| QCPPlottables_Zoom200 | 9.1 | 9.4 | 9 / 9.3 / 9 / 14 / 9.6 / 15 | 1.04 |
| QCPPolar_Draw10k | 0.93 | 0.94 | 0.94 / 0.95 / 0.94 / 1.2 / 1 / 0.94 | 1.02 |
| QCPPolar_Export10k | 0.95 | 0.96 | 0.95 / 1.1 / 0.96 / 1.1 / 0.96 / 0.96 | 1.01 |
| QCPPolar_RadialZoom10k | 0.9 | 0.93 | 0.92 / 0.92 / 0.91 / 1.1 / 0.93 / 0.93 | 1.03 |
| QCPPolar_Rotate10k | 0.91 | 0.93 | 0.93 / 0.93 / 0.92 / 1.1 / 0.94 / 0.93 | 1.02 |
| QCPSpans_Pan100 | 3.4 | 3.5 | 3.4 / 5.4 / 3.4 / 4.5 / 3.6 / 3.5 | 1.04 |
| QCPSpatialIndex_Build10k | 0.88 | 0.91 | 0.94 / 0.89 / 0.92 / 0.89 / 0.9 / 1.2 | 1.03 |
| QCPSpatialIndex_Query10k | 0.00036 | 0.00037 | 0.00038 / 0.00037 / 0.00037 / 0.00036 / 0.00037 / 0.00061 | 1.03 |
| QCPStatisticalBox_Pan1k | 0.14 | 0.14 | 0.14 / 0.17 / 0.14 / 0.14 / 0.15 / 0.14 | 1.00 |
| QCPTransform_Map1M | 0.69 | 1.1 | 1.3 / 1.7 / 0.82 / 1.3 / 0.82 / 0.89 | 1.59 |
| QObject_DeleteEachChild10k | 7.8 | 8.1 | 8 / 8.3 / 7.8 / 8.1 / 8 / 8.9 | 1.03 |

## End of milestone 4h

- Commit: 67c954f for the whole-table runs. Three runs of the whole binary offscreen and three under Xvfb,
  `-median 5`. Load (1/5/15 min) at the start: offscreen 2.23, 3.94, 5.66; 3.71, 3.52, 5.10; 3.08, 3.33, 4.67. Xvfb 7.65, 4.06, 4.55;
  8.00, 13.50, 9.52; 14.71, 20.86, 14.14. Other sessions loaded the machine during the Xvfb runs.
- Ratio = end-of-milestone-4h median / end-of-milestone-4g median. Rows over the gate here are load: the A/B below
  settles them, and untouched item rows (QCPItems_RestyleAll10k 1.69, QCPItems_FirstFrame10k 1.50) read as slow
  as any.
- A/B against a build of 6226ebd (milestone base), CPU ticks, calm pairs: QCPItems_RestyleAll10k, FirstFrame10k,
  Click100, DragOne10k, ZoomEmpty, QCPAxisRects_Stack8_Replot, QCPPlots_Stack8_Aligned_Replot, QCPAxis_TickLabels
  level (RestyleAll +4-7 % over eight pairs, noise on untouched code). QCPPolar_RadialZoom10k was +21 % (a per-point
  map in the record-time drawn check), fixed in 53fdd70: now +3 % over six pairs; QCPPolar_Draw10k -6 %,
  QCPPolar_Rotate10k -6 % (no re-record), QCPPolar_Export10k -4 %.

| Benchmark | end 4g msecs | end 4h msecs | runs | ratio |
|---|---|---|---|---|
| QCPAxisRects_Stack8_Replot | 1.9 | 2.6 | 1.8 / 2 / 1.9 / 3.2 / 7.3 / 7.3 | 1.37 |
| QCPAxis_SetRange10k | 1.5 | 1.5 | 1.4 / 1.9 / 1.4 / 1.6 / 1.6 / 1.4 | 1.00 |
| QCPAxis_SetTickLength10kItems | 0.012 | 0.013 | 0.012 / 0.014 / 0.012 / 0.015 / 0.013 / 0.012 | 1.04 |
| QCPAxis_TickLabels | 0.24 | 0.28 | 0.25 / 0.31 / 0.24 / 0.31 / 0.25 / 0.52 | 1.17 |
| QCPAxis_TickLabelsCached | 0.092 | 0.096 | 0.089 / 0.1 / 0.09 / 0.1 / 0.091 / 0.12 | 1.04 |
| QCPBars_Pan10k | 0.14 | 0.14 | 0.15 / 0.14 / 0.16 / 0.14 / 0.13 / 0.14 | 1.00 |
| QCPBars_Zoom10k | 5.5 | 5.3 | 5.3 / 5.3 / 5.3 / 5.5 / 5.2 / 5.2 | 0.96 |
| QCPColorMap2_ResampleDouble | 230.0 | 2.4e+02 | 2.4e+02 / 2.3e+02 / 2.3e+02 / 3e+02 / 2.5e+02 / 2.2e+02 | 1.03 |
| QCPColorMap2_ResampleFloat | 140.0 | 1.4e+02 | 1.4e+02 / 1.4e+02 / 1.4e+02 / 1.5e+02 / 1.4e+02 / 1.4e+02 | 1.01 |
| QCPColorMap2_ResamplePanSequence | 230.0 | 2.3e+02 | 1.9e+02 / 2.3e+02 / 2.3e+02 / 2.7e+02 / 2.3e+02 / 2.4e+02 | 1.01 |
| QCPColorMap2_ResamplePanSequenceSmall | 210.0 | 2.1e+02 | 2.1e+02 / 2.1e+02 / 2.1e+02 / 2.6e+02 / 2.1e+02 / 2.1e+02 | 0.99 |
| QCPColorMap2_ResampleWideParallel | 290.0 | 3e+02 | 2.7e+02 / 3e+02 / 3.1e+02 / 3.2e+02 / 3.1e+02 / 3e+02 | 1.05 |
| QCPColorMap2_ResampleWideSerial | 790.0 | 8e+02 | 7.7e+02 / 7.8e+02 / 7.9e+02 / 9.4e+02 / 8.2e+02 / 8e+02 | 1.01 |
| QCPColorMapData_ConstructLarge | 1.4 | 1.2 | 1.2 / 1.3 / 1.3 / 1.6 / 1.2 / 1.2 | 0.89 |
| QCPColorMap_ColorizeMap | 0.16 | 0.16 | 0.16 / 0.16 / 0.16 / 0.19 / 0.17 / 0.16 | 1.00 |
| QCPColorMap_SetCell1M | 3.0 | 3 | 3 / 3 / 3 / 4.9 / 3.1 / 3.1 | 1.02 |
| QCPColorMap_Standard | 0.27 | 0.27 | 0.27 / 0.27 / 0.27 / 0.28 / 0.28 / 0.27 | 1.00 |
| QCPColorScale_Rescale8 | 0.00016 | 0.00016 | 0.00017 / 0.00016 / 0.00016 / 0.00018 / 0.00016 / 0.00016 | 1.00 |
| QCPCoords_CoordToPixel1M | 4.5 | 4 | 4 / 4 / 4.6 / 4.3 / 4.1 / 4 | 0.90 |
| QCPDrawContext_ReplayData1M | 2.0 | 1.6 | 1.7 / 1.7 / 1.6 / 1.5 / 1.5 / 1.4 | 0.79 |
| QCPDrawContext_ReplayMarkers100k | 13.0 | 12 | 12 / 12 / 12 / 12 / 12 / 12 | 0.92 |
| QCPDrawn_Pan10k | 0.24 | 0.23 | 0.23 / 0.24 / 0.23 | 0.96 |
| QCPDrawn_Pan10kOneRecording | 0.3 | 0.28 | 0.28 / 0.29 / 0.28 | 0.93 |
| QCPDrawn_Zoom10k | 0.34 | 0.29 | 0.29 / 0.29 / 0.28 | 0.85 |
| QCPErrorBars_Pan10k | 0.25 | 0.26 | 0.28 / 0.26 / 0.25 / 0.26 / 0.24 / 0.24 | 1.02 |
| QCPFinancial_Pan10k | 0.14 | 0.14 | 0.16 / 0.14 / 0.15 / 0.14 / 0.14 / 0.14 | 1.00 |
| QCPGpu_Bars10k | 4.6 | 5.1 | 4.9 / 5.7 / 5.1 | 1.11 |
| QCPGpu_BarsPan10k | 0.13 | 0.14 | 0.13 / 0.15 / 0.14 | 1.08 |
| QCPGpu_ColorMap2Pan | 0.2 | 0.21 | 0.21 / 0.23 / 0.19 | 1.05 |
| QCPGpu_ColorMap2Zoom | 0.28 | 0.28 | 0.28 / 0.37 / 0.23 | 1.00 |
| QCPGpu_ColorMapPan | 0.22 | 0.25 | 0.18 / 0.25 / 0.3 | 1.14 |
| QCPGpu_CurvePan100k | 0.12 | 0.14 | 0.14 / 0.14 / 0.14 | 1.17 |
| QCPGpu_Graph2Render1M | 0.14 | 0.15 | 0.13 / 0.15 / 0.16 | 1.07 |
| QCPGpu_Histogram2DPan | 0.25 | 0.17 | 0.2 / 0.17 / 0.17 | 0.68 |
| QCPGpu_HistogramPan | 0.14 | 0.13 | 0.13 / 0.13 / 0.12 | 0.93 |
| QCPGpu_LineDashed100k | 0.5 | 0.52 | 0.46 / 0.53 / 0.52 | 1.04 |
| QCPGpu_LineSolid100k | 0.56 | 0.54 | 0.54 / 0.55 / 0.46 | 0.96 |
| QCPGpu_MixedLayer | 9.7 | 9.7 | 9.5 / 11 / 9.7 | 1.00 |
| QCPGpu_MultiGraphColored100kPan | 0.21 | 0.23 | 0.2 / 0.25 / 0.23 | 1.10 |
| QCPGpu_MultiGraphPan1M | 0.16 | 0.18 | 0.16 / 0.2 / 0.18 | 1.12 |
| QCPGpu_MultiGraphZoom1M | 0.7 | 0.8 | 0.78 / 0.81 / 0.8 | 1.14 |
| QCPGpu_Pan100k | 0.26 | 0.25 | 0.22 / 0.27 / 0.25 | 0.96 |
| QCPGpu_Text1k | 1.7 | 1.1 | 1.1 / 1.1 / 1.8 | 0.65 |
| QCPGraph2_RenderDouble | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 / 2.2 / 0.14 / 0.14 | 1.00 |
| QCPGraph2_RenderFloat | 0.14 | 0.14 | 0.14 / 0.15 / 0.14 / 0.28 / 0.14 / 0.14 | 1.00 |
| QCPGraph2_RenderManyLines | 0.45 | 0.45 | 0.44 / 0.47 / 0.44 / 6.6 / 0.45 / 0.45 | 1.00 |
| QCPGraph2_ScatterDisc1M | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 / 0.2 / 0.12 / 0.12 | 1.00 |
| QCPGraph2_ScatterSquare1M | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 / 0.12 / 0.13 / 0.12 | 1.00 |
| QCPGraph2_ScatterWithLine1M | 0.12 | 0.12 | 0.12 / 0.12 / 0.13 / 0.12 / 0.13 / 0.12 | 1.00 |
| QCPGraph2_SetDataDouble | 10.0 | 10 | 9.9 / 10 / 10 / 27 / 9.9 / 10 | 1.00 |
| QCPGraph2_SetDataFloat | 8.6 | 8.9 | 8.5 / 8.7 / 11 / 25 / 8.8 / 9 | 1.03 |
| QCPGraph_AddDataAtBeginSorted | 5.7 | 5.5 | 5.4 / 5.4 / 5.6 / 24 / 5.4 / 5.6 | 0.96 |
| QCPGraph_AddDataAtBeginUnsorted | 29.0 | 28 | 28 / 28 / 28 / 55 / 29 / 29 | 0.98 |
| QCPGraph_AddDataAtEndSorted | 5.2 | 5.2 | 5 / 5 / 5.2 / 14 / 5.1 / 5.3 | 0.99 |
| QCPGraph_AddDataAtEndUnsorted | 23.0 | 24 | 23 / 23 / 24 / 59 / 24 / 23 | 1.02 |
| QCPGraph_AddDataMixedSorted | 11.0 | 11 | 10 / 10 / 11 / 48 / 11 / 11 | 1.00 |
| QCPGraph_AddDataMixedUnsorted | 30.0 | 30 | 29 / 30 / 30 / 93 / 30 / 29 | 1.00 |
| QCPGraph_ManyLines | 1.4 | 1.3 | 1.3 / 1.4 / 1.3 / 8.8 / 1.3 / 1.3 | 0.93 |
| QCPGraph_ManyOffScreenLines | 0.15 | 0.16 | 0.15 / 0.17 / 0.16 / 1 / 0.15 / 0.16 | 1.07 |
| QCPGraph_ManyPoints | 2.8 | 2.7 | 2.6 / 2.7 / 2.6 / 31 / 2.7 / 2.6 | 0.95 |
| QCPGraph_Standard | 0.8 | 0.76 | 0.7 / 0.8 / 0.71 / 6.8 / 0.81 / 0.71 | 0.95 |
| QCPInput_MouseMoveStorm | 3.7 | 3.9 | 3.7 / 4 / 3.7 / 6.7 / 27 / 3.7 | 1.04 |
| QCPInput_WheelOverGraph2_1M | 0.00064 | 0.00065 | 0.00064 / 0.00066 / 0.00063 / 0.00066 / 0.0026 / 0.00065 | 1.02 |
| QCPIntervals_Pan10k | 0.32 | 0.31 | 0.31 / 0.31 / 0.42 / 0.31 / 0.31 / 0.31 | 0.97 |
| QCPIntervals_Pan10kLabelled | 0.33 | 0.31 | 0.31 / 0.31 / 0.38 / 0.31 / 0.31 / 0.31 | 0.94 |
| QCPItemText_Zoom1k | 0.69 | 0.69 | 0.75 / 0.69 / 0.7 / 0.69 / 0.68 / 0.7 | 1.01 |
| QCPItems_Add10k | 22.0 | 22 | 22 / 23 / 22 / 27 / 22 / 25 | 1.02 |
| QCPItems_AddRemove10k | 32.0 | 34 | 30 / 33 / 32 / 44 / 37 / 34 | 1.05 |
| QCPItems_AddRemove20k | 72.0 | 80 | 71 / 80 / 73 / 99 / 79 / 81 | 1.10 |
| QCPItems_Clear10k | 9.2 | 9.8 | 7.8 / 9.7 / 9.6 / 13 / 9.9 / 12 | 1.07 |
| QCPItems_Click100 | 0.0052 | 0.0077 | 0.0051 / 0.0059 / 0.0052 / 0.0094 / 0.011 / 0.01 | 1.47 |
| QCPItems_Click10k | 0.056 | 0.059 | 0.053 / 0.055 / 0.055 / 0.077 / 0.063 / 0.19 | 1.05 |
| QCPItems_ClickEmpty10k | 0.051 | 0.052 | 0.05 / 0.05 / 0.051 / 0.089 / 0.053 / 0.23 | 1.02 |
| QCPItems_DeleteEach10k | 26.0 | 28 | 24 / 28 / 28 / 31 / 26 / 34 | 1.08 |
| QCPItems_DeletePlot10k | 30.0 | 32 | 29 / 31 / 29 / 36 / 35 / 33 | 1.07 |
| QCPItems_DragOne10k | 0.57 | 0.7 | 0.57 / 0.58 / 0.58 / 0.83 / 4.7 / 2.4 | 1.24 |
| QCPItems_FirstFrame10k | 33.0 | 50 | 32 / 47 / 34 / 52 / 1e+02 / 82 | 1.50 |
| QCPItems_MixedKinds_Pan1k | 6.2 | 5.7 | 5.5 / 5.6 / 12 / 5.6 / 5.8 / 5.9 | 0.92 |
| QCPItems_Pan10k | 0.43 | 0.44 | 0.42 / 0.44 / 0.42 / 0.47 / 0.48 / 0.44 | 1.02 |
| QCPItems_RemoveEach10k | 36.0 | 36 | 34 / 37 / 33 / 50 / 35 / 40 | 1.00 |
| QCPItems_Replot10k | 0.43 | 0.44 | 0.43 / 0.44 / 0.44 / 0.53 / 0.44 / 0.46 | 1.02 |
| QCPItems_RestyleAll10k | 2.4 | 4.1 | 2.2 / 5.4 / 2 / 2.7 / 6.7 / 7.4 | 1.69 |
| QCPItems_Tracers_Pan1k | 1.6 | 1.5 | 1.4 / 1.5 / 3.2 / 1.7 / 1.5 / 1.5 | 0.94 |
| QCPItems_Zoom100 | 0.41 | 0.46 | 0.43 / 0.42 / 0.41 / 0.49 / 0.51 / 4.2 | 1.12 |
| QCPItems_Zoom10k | 0.43 | 0.45 | 0.43 / 0.45 / 0.43 / 0.49 / 0.55 / 0.45 | 1.05 |
| QCPItems_ZoomEmpty | 0.14 | 0.18 | 0.14 / 0.15 / 0.14 / 0.23 / 0.21 / 1.4 | 1.29 |
| QCPMultiGraph_SetData1M | 32.0 | 32 | 32 / 34 / 32 / 30 / 38 / 33 | 1.02 |
| QCPPlots_Stack8_Aligned_Replot | 0.26 | 0.33 | 0.26 / 0.29 / 0.26 / 0.37 / 0.52 / 0.43 | 1.27 |
| QCPPlots_Stack8_Replot | 0.26 | 0.3 | 0.25 / 0.26 / 0.26 / 0.34 / 1.6 / 1.2 | 1.15 |
| QCPPlottables_Pan200 | 13.0 | 9.1 | 9.1 / 9.1 / 9.8 / 9 / 8.9 / 9.6 | 0.70 |
| QCPPlottables_Zoom200 | 9.4 | 9.2 | 9.2 / 9.2 / 10 / 9.2 / 9 / 9.2 | 0.98 |
| QCPPolar_Draw10k | 0.94 | 0.9 | 0.87 / 0.88 / 1.1 / 0.9 / 0.92 / 0.89 | 0.95 |
| QCPPolar_Export10k | 0.96 | 0.92 | 0.9 / 0.92 / 1.1 / 0.91 / 0.91 / 0.92 | 0.95 |
| QCPPolar_RadialZoom10k | 0.93 | 1.1 | 1.1 / 1.1 / 1.4 / 1.1 / 1.1 / 1.2 | 1.18 |
| QCPPolar_Rotate10k | 0.93 | 0.88 | 0.86 / 0.88 / 1 / 0.88 / 0.88 / 0.87 | 0.95 |
| QCPSpans_Pan100 | 3.5 | 3.5 | 3.4 / 3.3 / 4.8 / 3.5 / 4.1 / 3.6 | 1.01 |
| QCPSpatialIndex_Build10k | 0.91 | 0.94 | 0.89 / 0.9 / 0.89 / 1 / 2.2 / 0.97 | 1.03 |
| QCPSpatialIndex_Query10k | 0.00037 | 0.00037 | 0.00036 / 0.00037 / 0.00037 / 0.0004 / 0.0011 / 0.00037 | 1.00 |
| QCPStatisticalBox_Pan1k | 0.14 | 0.13 | 0.13 / 0.13 / 0.15 / 0.14 / 0.13 / 0.13 | 0.93 |
| QCPTransform_Map1M | 1.1 | 0.82 | 0.8 / 0.88 / 1.7 / 0.79 / 0.85 / 0.76 | 0.75 |
| QObject_DeleteEachChild10k | 8.1 | 8.6 | 7.9 / 8.3 / 8 / 8.8 / 13 / 9.1 | 1.06 |

## End of milestone 4i

- Commit: 6d03307 for the whole-table runs (before d88fe37). Three runs of the whole binary offscreen and three
  under Xvfb, `-median 5`. Load (1/5/15 min) at the start: offscreen 3.44, 3.15, 3.05; 1.00, 2.07, 2.64; 0.73, 1.44, 2.27. Xvfb
  0.60, 1.03, 1.93; 3.31, 2.54, 2.35; 5.00, 3.72, 2.84. A quiet machine.
- Ratio = end-of-milestone-4i median / end-of-milestone-4h median. No row over its gate.
- A/B against a build of 5d326e0 (milestone base), CPU ticks, five pairs: QCPItems_FirstFrame10k was +5.1 % at
  6d03307 (QPointer and qobject_cast per position); d88fe37 (axisRect(0) no longer lists every rect, a cost that
  predates 4i) brings it to -24 %, QCPItems_Add10k -34 %, QCPItems_AddRemove10k -26 %. QCPItems_Replot10k,
  Pan10k, Zoom10k, Click10k, Tracers_Pan1k, QCPPolar_Draw10k and Rotate10k level; QCPItemText_Zoom1k +2-3 %.

| Benchmark | end 4h msecs | end 4i msecs | runs | ratio |
|---|---|---|---|---|
| QCPAxisRects_Stack8_Replot | 2.6 | 1.8 | 1.8 / 1.8 / 1.8 / 1.8 / 1.8 / 1.8 | 0.69 |
| QCPAxis_SetRange10k | 1.5 | 1.5 | 1.5 / 1.5 / 1.5 / 1.5 / 1.6 / 1.5 | 1.00 |
| QCPAxis_SetTickLength10kItems | 0.013 | 0.012 | 0.012 / 0.012 / 0.013 / 0.012 / 0.013 / 0.012 | 0.92 |
| QCPAxis_TickLabels | 0.28 | 0.24 | 0.25 / 0.25 / 0.24 / 0.24 / 0.24 / 0.24 | 0.86 |
| QCPAxis_TickLabelsCached | 0.096 | 0.087 | 0.089 / 0.088 / 0.089 / 0.087 / 0.086 / 0.087 | 0.91 |
| QCPBars_Pan10k | 0.14 | 0.13 | 0.14 / 0.13 / 0.13 / 0.13 / 0.13 / 0.13 | 0.93 |
| QCPBars_Zoom10k | 5.3 | 5.1 | 5 / 5.1 / 5 / 5.1 / 5.2 / 5.1 | 0.96 |
| QCPColorMap2_ResampleDouble | 240.0 | 2.2e+02 | 2.3e+02 / 2.1e+02 / 2.2e+02 / 2.2e+02 / 2.3e+02 / 2.3e+02 | 0.92 |
| QCPColorMap2_ResampleFloat | 140.0 | 1.4e+02 | 1.5e+02 / 1.4e+02 / 1.4e+02 / 1.4e+02 / 1.4e+02 / 1.4e+02 | 0.99 |
| QCPColorMap2_ResamplePanSequence | 230.0 | 2e+02 | 2e+02 / 2.1e+02 / 2e+02 / 2.1e+02 / 1.8e+02 / 1.8e+02 | 0.88 |
| QCPColorMap2_ResamplePanSequenceSmall | 210.0 | 2.1e+02 | 2.1e+02 / 2.1e+02 / 2.1e+02 / 2.1e+02 / 2.1e+02 / 2.1e+02 | 0.98 |
| QCPColorMap2_ResampleWideParallel | 300.0 | 2.6e+02 | 2.6e+02 / 2.6e+02 / 2.6e+02 / 2.7e+02 / 2.6e+02 / 2.6e+02 | 0.86 |
| QCPColorMap2_ResampleWideSerial | 800.0 | 7.7e+02 | 7.8e+02 / 7.7e+02 / 7.7e+02 / 7.6e+02 / 7.6e+02 / 7.8e+02 | 0.96 |
| QCPColorMapData_ConstructLarge | 1.2 | 1.1 | 1.1 / 1.2 / 1.2 / 1.2 / 1.1 / 1.1 | 0.96 |
| QCPColorMap_ColorizeMap | 0.16 | 0.16 | 0.16 / 0.17 / 0.16 / 0.16 / 0.16 / 0.16 | 1.00 |
| QCPColorMap_SetCell1M | 3.0 | 3 | 3 / 3 / 3 / 3 / 3 / 3 | 1.00 |
| QCPColorMap_Standard | 0.27 | 0.27 | 0.27 / 0.27 / 0.27 / 0.27 / 0.27 / 0.27 | 1.00 |
| QCPColorScale_Rescale8 | 0.00016 | 0.00015 | 0.00016 / 0.00015 / 0.00015 / 0.00015 / 0.00015 / 0.00015 | 0.94 |
| QCPCoords_CoordToPixel1M | 4.0 | 3.9 | 3.9 / 3.9 / 3.9 / 3.9 / 3.9 / 3.9 | 0.97 |
| QCPDrawContext_ReplayData1M | 1.6 | 1.4 | 1.5 / 1.4 / 1.4 / 1.5 / 1.6 / 1.4 | 0.91 |
| QCPDrawContext_ReplayMarkers100k | 12.0 | 12 | 12 / 12 / 12 / 12 / 12 / 12 | 1.00 |
| QCPDrawn_Pan10k | 0.23 | 0.23 | 0.23 / 0.23 / 0.23 | 1.00 |
| QCPDrawn_Pan10kOneRecording | 0.28 | 0.27 | 0.27 / 0.28 / 0.27 | 0.96 |
| QCPDrawn_Zoom10k | 0.29 | 0.28 | 0.28 / 0.28 / 0.28 | 0.97 |
| QCPErrorBars_Pan10k | 0.26 | 0.24 | 0.24 / 0.24 / 0.24 / 0.24 / 0.24 / 0.24 | 0.92 |
| QCPFinancial_Pan10k | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 / 0.14 / 0.14 / 0.14 | 1.00 |
| QCPGpu_Bars10k | 5.1 | 4.3 | 4.3 / 4.3 / 4.6 | 0.84 |
| QCPGpu_BarsPan10k | 0.14 | 0.12 | 0.12 / 0.12 / 0.12 | 0.86 |
| QCPGpu_ColorMap2Pan | 0.21 | 0.18 | 0.18 / 0.18 / 0.18 | 0.86 |
| QCPGpu_ColorMap2Zoom | 0.28 | 0.22 | 0.22 / 0.22 / 0.22 | 0.79 |
| QCPGpu_ColorMapPan | 0.25 | 0.17 | 0.17 / 0.17 / 0.17 | 0.68 |
| QCPGpu_CurvePan100k | 0.14 | 0.12 | 0.12 / 0.12 / 0.12 | 0.86 |
| QCPGpu_Graph2Render1M | 0.15 | 0.13 | 0.13 / 0.13 / 0.12 | 0.87 |
| QCPGpu_Histogram2DPan | 0.17 | 0.17 | 0.17 / 0.17 / 0.17 | 1.00 |
| QCPGpu_HistogramPan | 0.13 | 0.12 | 0.12 / 0.12 / 0.12 | 0.92 |
| QCPGpu_LineDashed100k | 0.52 | 0.44 | 0.43 / 0.44 / 0.44 | 0.85 |
| QCPGpu_LineSolid100k | 0.54 | 0.44 | 0.44 / 0.45 / 0.43 | 0.81 |
| QCPGpu_MixedLayer | 9.7 | 8.3 | 8.3 / 8 / 8.3 | 0.86 |
| QCPGpu_MultiGraphColored100kPan | 0.23 | 0.2 | 0.2 / 0.2 / 0.2 | 0.87 |
| QCPGpu_MultiGraphPan1M | 0.18 | 0.15 | 0.15 / 0.15 / 0.15 | 0.83 |
| QCPGpu_MultiGraphZoom1M | 0.8 | 0.68 | 0.68 / 0.68 / 0.68 | 0.85 |
| QCPGpu_Pan100k | 0.25 | 0.22 | 0.22 / 0.22 / 0.22 | 0.88 |
| QCPGpu_Text1k | 1.1 | 0.87 | 0.86 / 0.89 / 0.87 | 0.79 |
| QCPGraph2_RenderDouble | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 / 0.14 / 0.14 / 0.14 | 1.00 |
| QCPGraph2_RenderFloat | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 / 0.14 / 0.14 / 0.14 | 1.00 |
| QCPGraph2_RenderManyLines | 0.45 | 0.44 | 0.44 / 0.45 / 0.44 / 0.44 / 0.44 / 0.44 | 0.98 |
| QCPGraph2_ScatterDisc1M | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 / 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGraph2_ScatterSquare1M | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 / 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGraph2_ScatterWithLine1M | 0.12 | 0.12 | 0.13 / 0.13 / 0.12 / 0.12 / 0.12 / 0.13 | 1.04 |
| QCPGraph2_SetDataDouble | 10.0 | 9.5 | 9.3 / 9.4 / 9.6 / 9.6 / 9.5 / 9.5 | 0.95 |
| QCPGraph2_SetDataFloat | 8.9 | 8.3 | 8.3 / 8.1 / 8.2 / 8.3 / 8.5 / 8.3 | 0.93 |
| QCPGraph_AddDataAtBeginSorted | 5.5 | 5.5 | 5.8 / 5.5 / 5.6 / 5.5 / 5.3 / 5.3 | 1.00 |
| QCPGraph_AddDataAtBeginUnsorted | 28.0 | 28 | 28 / 28 / 28 / 28 / 28 / 28 | 1.00 |
| QCPGraph_AddDataAtEndSorted | 5.2 | 5 | 5.1 / 5.2 / 5 / 5.1 / 4.9 / 5 | 0.97 |
| QCPGraph_AddDataAtEndUnsorted | 24.0 | 23 | 23 / 23 / 23 / 23 / 23 / 23 | 0.96 |
| QCPGraph_AddDataMixedSorted | 11.0 | 10 | 10 / 11 / 11 / 10 / 10 / 10 | 0.91 |
| QCPGraph_AddDataMixedUnsorted | 30.0 | 29 | 29 / 29 / 29 / 29 / 29 / 29 | 0.97 |
| QCPGraph_ManyLines | 1.3 | 1.3 | 1.3 / 1.3 / 1.3 / 1.3 / 1.3 / 1.3 | 1.00 |
| QCPGraph_ManyOffScreenLines | 0.16 | 0.15 | 0.15 / 0.15 / 0.15 / 0.15 / 0.15 / 0.15 | 0.94 |
| QCPGraph_ManyPoints | 2.7 | 2.6 | 2.6 / 2.6 / 2.6 / 2.6 / 2.6 / 2.6 | 0.96 |
| QCPGraph_Standard | 0.76 | 0.69 | 0.66 / 0.69 / 0.68 / 0.69 / 0.7 / 0.67 | 0.90 |
| QCPInput_MouseMoveStorm | 3.9 | 3.8 | 3.7 / 3.9 / 4.2 / 4.2 / 3.7 / 3.7 | 0.97 |
| QCPInput_WheelOverGraph2_1M | 0.00065 | 0.00064 | 0.00064 / 0.00063 / 0.00066 / 0.00068 / 0.00063 / 0.00063 | 0.98 |
| QCPIntervals_Pan10k | 0.31 | 0.31 | 0.31 / 0.31 / 0.31 / 0.31 / 0.31 / 0.31 | 1.00 |
| QCPIntervals_Pan10kLabelled | 0.31 | 0.31 | 0.31 / 0.31 / 0.31 / 0.31 / 0.31 / 0.3 | 1.00 |
| QCPItemText_Zoom1k | 0.69 | 0.69 | 0.69 / 0.69 / 0.69 / 0.7 / 0.7 / 0.7 | 1.01 |
| QCPItems_Add10k | 22.0 | 22 | 22 / 22 / 22 / 22 / 22 / 22 | 1.00 |
| QCPItems_AddRemove10k | 34.0 | 30 | 30 / 30 / 31 / 30 / 30 / 32 | 0.88 |
| QCPItems_AddRemove20k | 80.0 | 71 | 71 / 70 / 71 / 71 / 70 / 72 | 0.89 |
| QCPItems_Clear10k | 9.8 | 8.4 | 7.9 / 9.2 / 8.1 / 8.5 / 9 / 8.4 | 0.86 |
| QCPItems_Click100 | 0.0077 | 0.0056 | 0.0056 / 0.0056 / 0.0056 / 0.0057 / 0.0057 / 0.0056 | 0.73 |
| QCPItems_Click10k | 0.059 | 0.055 | 0.054 / 0.055 / 0.054 / 0.055 / 0.055 / 0.055 | 0.93 |
| QCPItems_ClickEmpty10k | 0.052 | 0.05 | 0.05 / 0.05 / 0.05 / 0.05 / 0.05 / 0.05 | 0.96 |
| QCPItems_DeleteEach10k | 28.0 | 27 | 27 / 27 / 26 / 26 / 29 / 27 | 0.96 |
| QCPItems_DeletePlot10k | 32.0 | 30 | 29 / 29 / 31 / 30 / 30 / 29 | 0.92 |
| QCPItems_DragOne10k | 0.7 | 0.57 | 0.57 / 0.58 / 0.58 / 0.58 / 0.57 / 0.57 | 0.82 |
| QCPItems_FirstFrame10k | 50.0 | 32 | 31 / 32 / 32 / 33 / 32 / 33 | 0.64 |
| QCPItems_MixedKinds_Pan1k | 5.7 | 5.5 | 5.6 / 5.4 / 5.3 / 5.5 / 5.2 / 5.7 | 0.96 |
| QCPItems_Pan10k | 0.44 | 0.42 | 0.42 / 0.42 / 0.42 / 0.42 / 0.42 / 0.42 | 0.95 |
| QCPItems_RemoveEach10k | 36.0 | 32 | 35 / 33 / 32 / 32 / 34 / 32 | 0.90 |
| QCPItems_Replot10k | 0.44 | 0.43 | 0.42 / 0.42 / 0.43 / 0.43 / 0.43 / 0.43 | 0.98 |
| QCPItems_RestyleAll10k | 4.1 | 2.1 | 2 / 2.8 / 2.2 / 1.7 / 3 / 1.7 | 0.51 |
| QCPItems_Tracers_Pan1k | 1.5 | 1.5 | 1.5 / 1.5 / 1.4 / 1.5 / 1.5 / 1.5 | 1.00 |
| QCPItems_Zoom100 | 0.46 | 0.41 | 0.41 / 0.41 / 0.41 / 0.41 / 0.41 / 0.41 | 0.89 |
| QCPItems_Zoom10k | 0.45 | 0.43 | 0.43 / 0.43 / 0.43 / 0.43 / 0.43 / 0.43 | 0.96 |
| QCPItems_ZoomEmpty | 0.18 | 0.14 | 0.14 / 0.14 / 0.14 / 0.14 / 0.14 / 0.14 | 0.78 |
| QCPMultiGraph_SetData1M | 32.0 | 30 | 33 / 32 / 33 / 28 / 29 / 28 | 0.95 |
| QCPPlots_Stack8_Aligned_Replot | 0.33 | 0.26 | 0.26 / 0.26 / 0.26 / 0.26 / 0.26 / 0.26 | 0.79 |
| QCPPlots_Stack8_Replot | 0.3 | 0.26 | 0.26 / 0.26 / 0.25 / 0.26 / 0.26 / 0.26 | 0.87 |
| QCPPlottables_Pan200 | 9.1 | 9.1 | 9 / 9.1 / 8.8 / 8.8 / 9.3 / 9.2 | 0.99 |
| QCPPlottables_Zoom200 | 9.2 | 9.1 | 8.9 / 9.1 / 9 / 8.9 / 9.3 / 9.1 | 0.98 |
| QCPPolar_Draw10k | 0.9 | 0.87 | 0.87 / 0.87 / 0.87 / 0.88 / 0.88 / 0.87 | 0.97 |
| QCPPolar_Export10k | 0.92 | 0.91 | 0.89 / 0.89 / 0.91 / 0.91 / 0.93 / 0.91 | 0.99 |
| QCPPolar_RadialZoom10k | 1.1 | 0.94 | 0.95 / 0.94 / 0.95 / 0.93 / 0.94 / 0.95 | 0.86 |
| QCPPolar_Rotate10k | 0.88 | 0.86 | 0.87 / 0.87 / 0.86 / 0.86 / 0.87 / 0.86 | 0.98 |
| QCPSpans_Pan100 | 3.5 | 3.3 | 3.3 / 3.3 / 3.3 / 3.2 / 3.3 / 3.3 | 0.94 |
| QCPSpatialIndex_Build10k | 0.94 | 0.89 | 0.89 / 0.89 / 0.89 / 0.89 / 0.89 / 0.89 | 0.95 |
| QCPSpatialIndex_Query10k | 0.00037 | 0.00036 | 0.00036 / 0.00036 / 0.00036 / 0.00036 / 0.00036 / 0.00036 | 0.97 |
| QCPStatisticalBox_Pan1k | 0.13 | 0.14 | 0.14 / 0.13 / 0.14 / 0.13 / 0.13 / 0.14 | 1.04 |
| QCPTransform_Map1M | 0.82 | 0.68 | 0.72 / 0.68 / 0.66 / 0.68 / 0.71 / 0.68 | 0.83 |
| QObject_DeleteEachChild10k | 8.6 | 7.8 | 8.5 / 7.9 / 7.8 / 7.8 / 7.9 / 7.7 | 0.91 |


## End of milestone 4j

- Commit: f91c905 for the whole-table runs. Three runs of the whole binary offscreen and three under Xvfb,
  `-median 5`. Load (1/5/15 min) at the start of each run: 0.77, 0.82, 3.11;1.63, 1.69, 3.01;1.47, 1.68, 2.72;3.08, 2.26, 2.73;6.40, 4.85, 3.71;9.59, 6.99, 4.79. The Xvfb runs shared the machine with other work,
  and the parallel colour-map rows load it themselves.
- Ratio = end-of-milestone-4j median / end-of-milestone-4i median. Fifteen rows read over their gate on that ratio,
  half of them on code 4j does not touch (colour-map data, the transform map, the colour-map resampler), and the 4i
  medians are rounded to two digits.
- So every flagged row was measured again head to head: A/B against a build of 0c45d28 (end of 4i), CPU ticks,
  five pairs, ten for any row still over (`.superpowers/sdd/2026-11-11-neoqcp-3-milestone-4j/end-ab.md`). All
  level or faster (QCPItems_RestyleAll10k -14 %, QCPItems_Clear10k -13 %, QCPAxis_SetTickLength10kItems -9 % on ten
  pairs, QCPBars_Pan10k -2 %, QCPGpu_MultiGraphPan1M -6 %), except QCPDrawn_Pan10kOneRecording under Xvfb: bimodal
  on both binaries (base 1.25-7.7 ms, new 1.21-6.2 ms, same fastest time), on a cartesian pan path 4j does not
  change. Recorded as noise (backlog).
- The user-system paths have no bench row: a graph there maps every point per frame by design (backlog).

| Benchmark | end 4i msecs | end 4j msecs | runs | ratio |
|---|---|---|---|---|
| QCPAxisRects_Stack8_Replot | 1.8 | 1.9 | 1.9 / 1.9 / 1.8 / 1.8 / 2 / 1.8 | 1.03 |
| QCPAxis_SetRange10k | 1.5 | 1.6 | 3.9 / 1.6 / 1.6 / 1.6 / 1.6 / 1.8 | 1.07 |
| QCPAxis_SetTickLength10kItems | 0.012 | 0.013 | 0.013 / 0.013 / 0.013 / 0.013 / 0.013 / 0.013 | 1.08 |
| QCPAxis_TickLabels | 0.24 | 0.24 | 0.44 / 0.24 / 0.24 / 0.25 / 0.24 / 0.28 | 1.02 |
| QCPAxis_TickLabelsCached | 0.087 | 0.09 | 0.15 / 0.088 / 0.092 / 0.089 / 0.089 / 0.12 | 1.04 |
| QCPBars_Pan10k | 0.13 | 0.14 | 0.13 / 0.14 / 0.14 / 0.14 / 0.14 / 0.14 | 1.08 |
| QCPBars_Zoom10k | 5.1 | 5.3 | 5.1 / 5.3 / 5.1 / 5.4 / 7.7 / 5.3 | 1.04 |
| QCPColorMap2_ResampleDouble | 220.0 | 2.4e+02 | 2.2e+02 / 2.4e+02 / 2.4e+02 / 2.3e+02 / 2.3e+02 / 2.5e+02 | 1.07 |
| QCPColorMap2_ResampleFloat | 140.0 | 1.4e+02 | 1.4e+02 / 1.4e+02 / 1.4e+02 / 1.5e+02 / 1.4e+02 / 1.5e+02 | 1.03 |
| QCPColorMap2_ResamplePanSequence | 200.0 | 2.3e+02 | 2.3e+02 / 2.4e+02 / 2.3e+02 / 2.3e+02 / 2e+02 / 2.4e+02 | 1.16 |
| QCPColorMap2_ResamplePanSequenceSmall | 210.0 | 2.1e+02 | 2.1e+02 / 2.1e+02 / 2.1e+02 / 2.1e+02 / 2.1e+02 / 2.2e+02 | 0.99 |
| QCPColorMap2_ResampleWideParallel | 260.0 | 3.1e+02 | 3.9e+02 / 3e+02 / 3.1e+02 / 3.9e+02 / 2.8e+02 / 3e+02 | 1.18 |
| QCPColorMap2_ResampleWideSerial | 770.0 | 8e+02 | 8e+02 / 7.9e+02 / 8e+02 / 1.2e+03 / 8.1e+02 / 8.4e+02 | 1.04 |
| QCPColorMapData_ConstructLarge | 1.1 | 1.5 | 1.6 / 1.6 / 1.5 / 1.5 / 1.2 / 1.5 | 1.36 |
| QCPColorMap_ColorizeMap | 0.16 | 0.16 | 0.16 / 0.16 / 0.16 / 0.16 / 0.16 / 0.18 | 1.00 |
| QCPColorMap_SetCell1M | 3.0 | 3 | 3 / 3 / 3.1 / 3.1 / 3 / 3.6 | 1.02 |
| QCPColorMap_Standard | 0.27 | 0.27 | 0.27 / 0.27 / 0.27 / 0.28 / 0.27 / 0.3 | 1.00 |
| QCPColorScale_Rescale8 | 0.00015 | 0.00016 | 0.00033 / 0.00016 / 0.00016 / 0.00016 / 0.00016 / 0.00018 | 1.07 |
| QCPCoords_CoordToPixel1M | 3.9 | 4 | 4 / 4 / 3.9 / 4 / 7.2 / 3.9 | 1.03 |
| QCPDrawContext_ReplayData1M | 1.4 | 1.6 | 1.8 / 1.5 / 1.3 / 1.7 / 3.6 / 1.4 | 1.14 |
| QCPDrawContext_ReplayMarkers100k | 12.0 | 12 | 12 / 12 / 12 / 12 / 20 / 12 | 1.00 |
| QCPDrawn_Pan10k | 0.23 | 0.23 | 0.24 / 0.23 / 0.23 | 1.00 |
| QCPDrawn_Pan10kOneRecording | 0.27 | 0.3 | 0.3 / 0.3 / 0.27 | 1.11 |
| QCPDrawn_Zoom10k | 0.28 | 0.29 | 0.29 / 0.98 / 0.29 | 1.04 |
| QCPErrorBars_Pan10k | 0.24 | 0.24 | 0.24 / 0.25 / 0.24 / 0.25 / 0.25 / 0.24 | 1.02 |
| QCPFinancial_Pan10k | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 / 0.14 / 0.14 / 0.14 | 1.00 |
| QCPGpu_Bars10k | 4.3 | 4.4 | 4.3 / 4.4 / 4.5 | 1.02 |
| QCPGpu_BarsPan10k | 0.12 | 0.13 | 0.12 / 0.13 / 0.13 | 1.08 |
| QCPGpu_ColorMap2Pan | 0.18 | 0.18 | 0.19 / 0.18 / 0.18 | 1.00 |
| QCPGpu_ColorMap2Zoom | 0.22 | 0.23 | 0.23 / 0.23 / 0.23 | 1.05 |
| QCPGpu_ColorMapPan | 0.17 | 0.17 | 0.17 / 0.17 / 0.17 | 1.00 |
| QCPGpu_CurvePan100k | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGpu_Graph2Render1M | 0.13 | 0.13 | 0.13 / 0.13 / 0.13 | 1.00 |
| QCPGpu_Histogram2DPan | 0.17 | 0.17 | 0.17 / 0.18 / 0.17 | 1.00 |
| QCPGpu_HistogramPan | 0.12 | 0.13 | 0.13 / 0.13 / 0.13 | 1.08 |
| QCPGpu_LineDashed100k | 0.44 | 0.44 | 0.43 / 0.44 / 0.44 | 1.00 |
| QCPGpu_LineSolid100k | 0.44 | 0.44 | 0.43 / 0.45 / 0.44 | 1.00 |
| QCPGpu_MixedLayer | 8.3 | 8.5 | 8.3 / 8.7 / 8.5 | 1.02 |
| QCPGpu_MultiGraphColored100kPan | 0.2 | 0.21 | 0.2 / 0.21 / 0.21 | 1.05 |
| QCPGpu_MultiGraphPan1M | 0.15 | 0.16 | 0.15 / 0.16 / 0.16 | 1.07 |
| QCPGpu_MultiGraphZoom1M | 0.68 | 0.71 | 0.7 / 0.74 / 0.71 | 1.04 |
| QCPGpu_Pan100k | 0.22 | 0.22 | 0.22 / 0.22 / 0.23 | 1.00 |
| QCPGpu_Text1k | 0.87 | 0.87 | 0.88 / 0.87 / 0.87 | 1.00 |
| QCPGraph2_RenderDouble | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 / 0.14 / 0.14 / 0.15 | 1.00 |
| QCPGraph2_RenderFloat | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 / 0.14 / 0.14 / 0.16 | 1.00 |
| QCPGraph2_RenderManyLines | 0.44 | 0.45 | 0.45 / 0.44 / 0.44 / 0.44 / 0.45 / 0.51 | 1.01 |
| QCPGraph2_ScatterDisc1M | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 / 0.12 / 0.12 / 0.14 | 1.00 |
| QCPGraph2_ScatterSquare1M | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 / 0.12 / 0.12 / 0.14 | 1.00 |
| QCPGraph2_ScatterWithLine1M | 0.12 | 0.12 | 0.13 / 0.12 / 0.13 / 0.12 / 0.12 / 0.13 | 1.04 |
| QCPGraph2_SetDataDouble | 9.5 | 9.9 | 9.9 / 9.9 / 9.7 / 9.8 / 11 / 11 | 1.04 |
| QCPGraph2_SetDataFloat | 8.3 | 8.8 | 8.9 / 8.6 / 8.6 / 8.4 / 9.5 / 9.2 | 1.05 |
| QCPGraph_AddDataAtBeginSorted | 5.5 | 5.9 | 5.8 / 5.7 / 6.3 / 6 / 5.4 / 6.3 | 1.07 |
| QCPGraph_AddDataAtBeginUnsorted | 28.0 | 29 | 29 / 29 / 29 / 28 / 28 / 29 | 1.04 |
| QCPGraph_AddDataAtEndSorted | 5.0 | 5.4 | 5.3 / 5.4 / 5.6 / 5.5 / 5.1 / 5.4 | 1.08 |
| QCPGraph_AddDataAtEndUnsorted | 23.0 | 23 | 23 / 25 / 23 / 23 / 23 / 24 | 1.00 |
| QCPGraph_AddDataMixedSorted | 10.0 | 11 | 11 / 11 / 14 / 11 / 10 / 11 | 1.10 |
| QCPGraph_AddDataMixedUnsorted | 29.0 | 30 | 29 / 30 / 30 / 30 / 29 / 32 | 1.03 |
| QCPGraph_ManyLines | 1.3 | 1.3 | 1.3 / 1.3 / 1.3 / 1.3 / 1.3 / 1.4 | 1.00 |
| QCPGraph_ManyOffScreenLines | 0.15 | 0.15 | 0.15 / 0.15 / 0.15 / 0.15 / 0.16 / 0.23 | 1.00 |
| QCPGraph_ManyPoints | 2.6 | 2.6 | 2.6 / 2.6 / 2.7 / 2.6 / 2.6 / 2.6 | 1.00 |
| QCPGraph_Standard | 0.69 | 0.71 | 0.71 / 0.73 / 0.71 / 0.7 / 0.76 / 0.71 | 1.03 |
| QCPInput_MouseMoveStorm | 3.8 | 3.7 | 3.7 / 3.8 / 3.7 / 3.7 / 3.7 / 3.8 | 0.97 |
| QCPInput_WheelOverGraph2_1M | 0.00064 | 0.00061 | 0.00061 / 0.00061 / 0.00062 / 0.00061 / 0.00061 / 0.00087 | 0.95 |
| QCPIntervals_Pan10k | 0.31 | 0.31 | 0.31 / 0.31 / 0.31 / 0.32 / 0.31 / 0.31 | 1.00 |
| QCPIntervals_Pan10kLabelled | 0.31 | 0.31 | 0.31 / 0.31 / 0.31 / 0.31 / 0.31 / 0.31 | 1.00 |
| QCPItemText_Zoom1k | 0.69 | 0.7 | 0.7 / 0.69 / 0.69 / 0.74 / 0.72 / 0.7 | 1.01 |
| QCPItems_Add10k | 22.0 | 14 | 14 / 14 / 14 / 14 / 14 / 22 | 0.64 |
| QCPItems_AddRemove10k | 30.0 | 24 | 22 / 23 / 24 / 25 / 22 / 60 | 0.78 |
| QCPItems_AddRemove20k | 71.0 | 58 | 54 / 58 / 58 / 58 / 57 / 84 | 0.82 |
| QCPItems_Clear10k | 8.4 | 9.1 | 8.7 / 9.2 / 7.8 / 11 / 9.5 / 8.9 | 1.08 |
| QCPItems_Click100 | 0.0056 | 0.0055 | 0.0055 / 0.0055 / 0.0054 / 0.0055 / 0.0056 / 0.0068 | 0.98 |
| QCPItems_Click10k | 0.055 | 0.054 | 0.054 / 0.055 / 0.054 / 0.054 / 0.056 / 0.057 | 0.99 |
| QCPItems_ClickEmpty10k | 0.05 | 0.051 | 0.05 / 0.051 / 0.051 / 0.05 / 0.051 / 0.065 | 1.02 |
| QCPItems_DeleteEach10k | 27.0 | 28 | 28 / 29 / 28 / 33 / 26 / 29 | 1.06 |
| QCPItems_DeletePlot10k | 30.0 | 22 | 21 / 22 / 22 / 22 / 23 / 26 | 0.73 |
| QCPItems_DragOne10k | 0.57 | 0.58 | 0.58 / 0.58 / 0.57 / 0.58 / 0.59 / 0.58 | 1.02 |
| QCPItems_FirstFrame10k | 32.0 | 26 | 24 / 27 / 24 / 25 / 27 / 26 | 0.80 |
| QCPItems_MixedKinds_Pan1k | 5.5 | 5.7 | 5.4 / 5.9 / 5.3 / 6.8 / 5.7 / 5.6 | 1.03 |
| QCPItems_Pan10k | 0.42 | 0.42 | 0.42 / 0.43 / 0.42 / 0.43 / 0.42 / 0.73 | 1.01 |
| QCPItems_RemoveEach10k | 32.0 | 35 | 33 / 37 / 31 / 37 / 33 / 37 | 1.09 |
| QCPItems_Replot10k | 0.43 | 0.43 | 0.42 / 0.43 / 0.43 / 0.43 / 0.43 / 0.67 | 1.00 |
| QCPItems_RestyleAll10k | 2.1 | 2.5 | 2.5 / 2.9 / 2.4 / 1.6 / 3.6 / 1.6 | 1.17 |
| QCPItems_Tracers_Pan1k | 1.5 | 1.4 | 1.4 / 1.5 / 1.4 / 1.5 / 1.4 / 1.4 | 0.93 |
| QCPItems_Zoom100 | 0.41 | 0.41 | 0.41 / 0.42 / 0.41 / 0.41 / 0.41 / 0.79 | 1.00 |
| QCPItems_Zoom10k | 0.43 | 0.43 | 0.43 / 0.43 / 0.43 / 0.44 / 0.43 / 0.44 | 1.00 |
| QCPItems_ZoomEmpty | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 / 0.14 / 0.15 / 0.15 | 1.00 |
| QCPMultiGraph_SetData1M | 30.0 | 31 | 33 / 35 / 32 / 29 / 30 / 29 | 1.03 |
| QCPPlots_Stack8_Aligned_Replot | 0.26 | 0.26 | 0.27 / 0.26 / 0.26 / 0.26 / 0.27 / 0.26 | 1.00 |
| QCPPlots_Stack8_Replot | 0.26 | 0.27 | 0.26 / 0.26 / 0.26 / 0.27 / 0.27 / 0.27 | 1.02 |
| QCPPlottables_Pan200 | 9.1 | 9.3 | 9.2 / 9.6 / 9.2 / 9.9 / 9.5 / 9.1 | 1.03 |
| QCPPlottables_Zoom200 | 9.1 | 9.4 | 9.3 / 9.5 / 9.4 / 9.6 / 9.4 / 9.2 | 1.03 |
| QCPPolar_Draw10k | 0.87 | 0.88 | 0.88 / 0.88 / 0.87 / 0.88 / 0.89 / 0.87 | 1.01 |
| QCPPolar_Export10k | 0.91 | 0.91 | 0.91 / 0.91 / 0.89 / 0.91 / 0.92 / 0.91 | 1.00 |
| QCPPolar_RadialZoom10k | 0.94 | 0.95 | 0.98 / 0.94 / 0.93 / 0.95 / 0.95 / 0.96 | 1.01 |
| QCPPolar_Rotate10k | 0.86 | 0.87 | 0.87 / 0.87 / 0.86 / 0.87 / 0.87 / 0.87 | 1.01 |
| QCPSpans_Pan100 | 3.3 | 3.5 | 3.4 / 3.4 / 3.3 / 3.6 / 3.5 / 3.5 | 1.05 |
| QCPSpatialIndex_Build10k | 0.89 | 0.9 | 0.9 / 0.89 / 0.9 / 0.9 / 0.91 / 1.2 | 1.01 |
| QCPSpatialIndex_Query10k | 0.00036 | 0.00037 | 0.00036 / 0.00037 / 0.00037 / 0.00038 / 0.00037 / 0.00062 | 1.03 |
| QCPStatisticalBox_Pan1k | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 / 0.14 / 0.14 / 0.13 | 1.00 |
| QCPTransform_Map1M | 0.68 | 0.87 | 0.92 / 0.65 / 0.82 / 0.93 / 2.2 / 0.65 | 1.28 |
| QObject_DeleteEachChild10k | 7.8 | 8 | 7.9 / 7.9 / 8 / 7.9 / 8.4 / 8.3 | 1.02 |

## End of milestone 5a

- Commit: 5926a44 for the whole-table runs. Three runs of the whole binary offscreen and three under
  Xvfb, `-median 5`. Load (1/5/15 min) at the start of each run: 2.41, 3.47, 2.59;0.60, 1.83, 2.12;0.69, 1.16, 1.76;0.71, 1.08, 1.58;2.68, 1.68, 1.67;5.42, 3.77, 2.51.
- Ratio = end-of-milestone-5a median / end-of-milestone-4j median. One row read over its gate:
  QCPItems_Tracers_Pan1k (ratio 1.07, gate 5 %). A/B against a build of 3e95d2e (milestone base), CPU ticks, ten
  pairs: 5.468 -> 5.512 M ticks, +0.8 %. Level.
- New rows (5a): the `QCPMultiGraph_*` twins of every `QCPGraph_*` and `QCPGraph2_*` workload, the AddData rows,
  `QCPMultiGraph_FillToGraphPan10k`, `QCPMultiGraph_UserSystem100k`, `QCPInput_WheelOverMultiGraph_1M` and
  `QCPGpu_MultiGraphRender1M`. Their medians below are the gate for 5c, when the class is renamed and the old graph
  rows retire.

| Benchmark | end 4j msecs | end 5a msecs | runs | ratio |
|---|---|---|---|---|
| QCPAxisRects_Stack8_Replot | 1.9 | 1.8 | 1.8 / 1.8 / 1.8 / 1.8 / 1.8 / 1.8 | 0.95 |
| QCPAxis_SetRange10k | 1.6 | 1.5 | 1.6 / 1.5 / 1.6 / 1.5 / 1.5 / 1.5 | 0.94 |
| QCPAxis_SetTickLength10kItems | 0.013 | 0.012 | 0.012 / 0.012 / 0.012 / 0.012 / 0.012 / 0.012 | 0.92 |
| QCPAxis_TickLabels | 0.24 | 0.24 | 0.24 / 0.24 / 0.24 / 0.24 / 0.24 / 0.24 | 1.00 |
| QCPAxis_TickLabelsCached | 0.09 | 0.088 | 0.087 / 0.088 / 0.087 / 0.089 / 0.088 / 0.088 | 0.98 |
| QCPBars_Pan10k | 0.14 | 0.13 | 0.14 / 0.13 / 0.14 / 0.13 / 0.13 / 0.13 | 0.93 |
| QCPBars_Zoom10k | 5.3 | 5.1 | 5.2 / 5.1 / 5.2 / 5.1 / 5 / 4.9 | 0.96 |
| QCPColorMap2_ResampleDouble | 240.0 | 2.2e+02 | 2.2e+02 / 2.2e+02 / 2.3e+02 / 2.2e+02 / 2.2e+02 / 2.3e+02 | 0.93 |
| QCPColorMap2_ResampleFloat | 140.0 | 1.4e+02 | 1.4e+02 / 1.3e+02 / 1.4e+02 / 1.4e+02 / 1.4e+02 / 1.3e+02 | 1.00 |
| QCPColorMap2_ResamplePanSequence | 230.0 | 2e+02 | 2e+02 / 1.8e+02 / 2e+02 / 2e+02 / 2.1e+02 / 2.1e+02 | 0.87 |
| QCPColorMap2_ResamplePanSequenceSmall | 210.0 | 2.1e+02 | 2.1e+02 / 2e+02 / 2.1e+02 / 2.1e+02 / 2.1e+02 / 2e+02 | 0.98 |
| QCPColorMap2_ResampleWideParallel | 310.0 | 2.6e+02 | 2.6e+02 / 2.7e+02 / 2.6e+02 / 2.6e+02 / 2.6e+02 / 2.8e+02 | 0.85 |
| QCPColorMap2_ResampleWideSerial | 800.0 | 7.7e+02 | 7.7e+02 / 7.7e+02 / 7.7e+02 / 7.7e+02 / 7.7e+02 / 7.7e+02 | 0.96 |
| QCPColorMapData_ConstructLarge | 1.5 | 1.1 | 1.2 / 1.2 / 1.2 / 1.1 / 1.1 / 1.1 | 0.77 |
| QCPColorMap_ColorizeMap | 0.16 | 0.16 | 0.16 / 0.17 / 0.16 / 0.16 / 0.16 / 0.16 | 1.00 |
| QCPColorMap_SetCell1M | 3.0 | 3 | 3 / 3 / 3 / 3 / 3 / 3 | 1.00 |
| QCPColorMap_Standard | 0.27 | 0.27 | 0.27 / 0.27 / 0.27 / 0.27 / 0.27 / 0.27 | 1.00 |
| QCPColorScale_Rescale8 | 0.00016 | 0.00016 | 0.00016 / 0.00016 / 0.00016 / 0.00016 / 0.00016 / 0.00016 | 1.00 |
| QCPCoords_CoordToPixel1M | 4.0 | 3.9 | 3.9 / 3.9 / 3.9 / 3.9 / 3.9 / 3.9 | 0.97 |
| QCPDrawContext_ReplayData1M | 1.6 | 1.4 | 1.4 / 1.4 / 1.3 / 1.4 / 1.3 / 1.4 | 0.87 |
| QCPDrawContext_ReplayMarkers100k | 12.0 | 12 | 12 / 12 / 12 / 12 / 12 / 12 | 1.00 |
| QCPDrawn_Pan10k | 0.23 | 0.23 | 0.23 / 0.23 / 0.23 | 1.00 |
| QCPDrawn_Pan10kOneRecording | 0.3 | 0.28 | 0.27 / 0.28 / 0.28 | 0.93 |
| QCPDrawn_Zoom10k | 0.29 | 0.29 | 0.28 / 0.29 / 0.29 | 1.00 |
| QCPErrorBars_Pan10k | 0.24 | 0.24 | 0.24 / 0.24 / 0.24 / 0.24 / 0.25 / 0.24 | 1.00 |
| QCPFinancial_Pan10k | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 / 0.13 / 0.14 / 0.14 | 1.00 |
| QCPGpu_Bars10k | 4.4 | 4.4 | 4.4 / 4.5 / 4.4 | 1.00 |
| QCPGpu_BarsPan10k | 0.13 | 0.12 | 0.12 / 0.12 / 0.12 | 0.92 |
| QCPGpu_ColorMap2Pan | 0.18 | 0.18 | 0.18 / 0.18 / 0.18 | 1.00 |
| QCPGpu_ColorMap2Zoom | 0.23 | 0.23 | 0.22 / 0.23 / 0.23 | 1.00 |
| QCPGpu_ColorMapPan | 0.17 | 0.17 | 0.17 / 0.17 / 0.17 | 1.00 |
| QCPGpu_CurvePan100k | 0.12 | 0.11 | 0.11 / 0.11 / 0.12 | 0.92 |
| QCPGpu_Graph2Render1M | 0.13 | 0.13 | 0.12 / 0.13 / 0.13 | 1.00 |
| QCPGpu_Histogram2DPan | 0.17 | 0.17 | 0.17 / 0.17 / 0.17 | 1.00 |
| QCPGpu_HistogramPan | 0.13 | 0.12 | 0.13 / 0.12 / 0.12 | 0.92 |
| QCPGpu_LineDashed100k | 0.44 | 0.43 | 0.44 / 0.43 / 0.43 | 0.98 |
| QCPGpu_LineSolid100k | 0.44 | 0.44 | 0.44 / 0.44 / 0.44 | 1.00 |
| QCPGpu_MixedLayer | 8.5 | 8.3 | 8.3 / 8.1 / 8.3 | 0.98 |
| QCPGpu_MultiGraphColored100kPan | 0.21 | 0.2 | 0.2 / 0.2 / 0.2 | 0.95 |
| QCPGpu_MultiGraphPan1M | 0.16 | 0.15 | 0.15 / 0.15 / 0.15 | 0.94 |
| QCPGpu_MultiGraphRender1M | new | 1.5 | 1.5 / 1.6 / 1.5 | - |
| QCPGpu_MultiGraphZoom1M | 0.71 | 0.67 | 0.67 / 0.67 / 0.68 | 0.94 |
| QCPGpu_Pan100k | 0.22 | 0.22 | 0.21 / 0.22 / 0.22 | 1.00 |
| QCPGpu_Text1k | 0.87 | 0.87 | 0.86 / 0.87 / 0.89 | 1.00 |
| QCPGraph2_RenderDouble | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 / 0.13 / 0.14 / 0.14 | 1.00 |
| QCPGraph2_RenderFloat | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 / 0.14 / 0.14 / 0.14 | 1.00 |
| QCPGraph2_RenderManyLines | 0.45 | 0.44 | 0.44 / 0.44 / 0.44 / 0.44 / 0.44 / 0.45 | 0.98 |
| QCPGraph2_ScatterDisc1M | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 / 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGraph2_ScatterSquare1M | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 / 0.12 / 0.12 / 0.12 | 1.00 |
| QCPGraph2_ScatterWithLine1M | 0.12 | 0.12 | 0.12 / 0.12 / 0.12 / 0.12 / 0.12 / 0.13 | 1.00 |
| QCPGraph2_SetDataDouble | 9.9 | 9.4 | 9.5 / 9.4 / 9.3 / 9.4 / 9.7 / 9.7 | 0.95 |
| QCPGraph2_SetDataFloat | 8.8 | 8.2 | 8.2 / 8.2 / 8 / 8.2 / 8.2 / 8.3 | 0.93 |
| QCPGraph_AddDataAtBeginSorted | 5.9 | 5.3 | 5.3 / 5.3 / 5.3 / 5.6 / 5.7 / 5.3 | 0.90 |
| QCPGraph_AddDataAtBeginUnsorted | 29.0 | 28 | 28 / 28 / 28 / 28 / 28 / 28 | 0.97 |
| QCPGraph_AddDataAtEndSorted | 5.4 | 5 | 5.1 / 5 / 5 / 5 / 5.3 / 5.2 | 0.94 |
| QCPGraph_AddDataAtEndUnsorted | 23.0 | 23 | 23 / 23 / 23 / 23 / 23 / 23 | 1.00 |
| QCPGraph_AddDataMixedSorted | 11.0 | 10 | 10 / 11 / 10 / 10 / 11 / 10 | 0.91 |
| QCPGraph_AddDataMixedUnsorted | 30.0 | 29 | 29 / 29 / 29 / 29 / 30 / 29 | 0.97 |
| QCPGraph_ManyLines | 1.3 | 1.3 | 1.3 / 1.3 / 1.3 / 1.2 / 1.3 / 1.3 | 1.00 |
| QCPGraph_ManyOffScreenLines | 0.15 | 0.15 | 0.15 / 0.15 / 0.15 / 0.15 / 0.15 / 0.15 | 1.00 |
| QCPGraph_ManyPoints | 2.6 | 2.6 | 2.6 / 2.6 / 2.6 / 2.6 / 2.7 / 2.6 | 1.00 |
| QCPGraph_Standard | 0.71 | 0.7 | 0.68 / 0.71 / 0.7 / 0.7 / 0.68 / 0.7 | 0.99 |
| QCPInput_MouseMoveStorm | 3.7 | 3.7 | 3.6 / 3.7 / 3.7 / 3.7 / 3.6 / 3.7 | 1.00 |
| QCPInput_WheelOverGraph2_1M | 0.00061 | 0.00063 | 0.00063 / 0.00064 / 0.00063 / 0.00063 / 0.00064 / 0.00063 | 1.03 |
| QCPInput_WheelOverMultiGraph_1M | new | 0.00091 | 0.00089 / 0.00091 / 0.0009 / 0.00094 / 0.00091 / 0.00095 | - |
| QCPIntervals_Pan10k | 0.31 | 0.31 | 0.31 / 0.31 / 0.31 / 0.31 / 0.31 / 0.3 | 1.00 |
| QCPIntervals_Pan10kLabelled | 0.31 | 0.3 | 0.31 / 0.31 / 0.31 / 0.3 / 0.3 / 0.3 | 0.98 |
| QCPItemText_Zoom1k | 0.7 | 0.69 | 0.68 / 0.69 / 0.69 / 0.68 / 0.69 / 0.7 | 0.99 |
| QCPItems_Add10k | 14.0 | 14 | 14 / 14 / 14 / 14 / 14 / 14 | 1.00 |
| QCPItems_AddRemove10k | 24.0 | 22 | 22 / 22 / 22 / 22 / 21 / 22 | 0.92 |
| QCPItems_AddRemove20k | 58.0 | 54 | 54 / 56 / 54 / 53 / 55 / 56 | 0.94 |
| QCPItems_Clear10k | 9.1 | 8.8 | 7.7 / 8.8 / 7.7 / 8.9 / 8.8 / 9.2 | 0.97 |
| QCPItems_Click100 | 0.0055 | 0.0055 | 0.0055 / 0.0055 / 0.0056 / 0.0055 / 0.0055 / 0.0055 | 1.00 |
| QCPItems_Click10k | 0.054 | 0.054 | 0.054 / 0.054 / 0.054 / 0.054 / 0.055 / 0.054 | 1.00 |
| QCPItems_ClickEmpty10k | 0.051 | 0.05 | 0.051 / 0.05 / 0.051 / 0.05 / 0.05 / 0.05 | 0.98 |
| QCPItems_DeleteEach10k | 28.0 | 26 | 25 / 25 / 27 / 28 / 28 / 25 | 0.93 |
| QCPItems_DeletePlot10k | 22.0 | 21 | 22 / 21 / 21 / 20 / 22 / 21 | 0.95 |
| QCPItems_DragOne10k | 0.58 | 0.58 | 0.57 / 0.58 / 0.58 / 0.57 / 0.58 / 0.59 | 1.00 |
| QCPItems_FirstFrame10k | 26.0 | 25 | 25 / 25 / 24 / 24 / 26 / 25 | 0.96 |
| QCPItems_MixedKinds_Pan1k | 5.7 | 5.6 | 5.4 / 5.6 / 5.6 / 5.6 / 5.8 / 5.9 | 0.98 |
| QCPItems_Pan10k | 0.42 | 0.42 | 0.42 / 0.42 / 0.42 / 0.42 / 0.42 / 0.43 | 1.00 |
| QCPItems_RemoveEach10k | 35.0 | 32 | 31 / 31 / 33 / 31 / 32 / 35 | 0.90 |
| QCPItems_Replot10k | 0.43 | 0.43 | 0.43 / 0.42 / 0.43 / 0.42 / 0.43 / 0.43 | 1.00 |
| QCPItems_RestyleAll10k | 2.5 | 1.8 | 1.6 / 2 / 1.6 / 2.2 / 1.6 / 2.3 | 0.72 |
| QCPItems_Tracers_Pan1k | 1.4 | 1.5 | 1.4 / 1.5 / 1.5 / 1.5 / 1.5 / 1.5 | 1.07 |
| QCPItems_Zoom100 | 0.41 | 0.41 | 0.41 / 0.41 / 0.4 / 0.41 / 0.41 / 0.41 | 1.00 |
| QCPItems_Zoom10k | 0.43 | 0.43 | 0.43 / 0.43 / 0.43 / 0.43 / 0.43 / 0.44 | 1.00 |
| QCPItems_ZoomEmpty | 0.14 | 0.14 | 0.14 / 0.14 / 0.14 / 0.14 / 0.14 / 0.14 | 1.00 |
| QCPMultiGraph_AddDataAtBeginSorted | new | 3.2 | 3.5 / 3.4 / 3.1 / 2.8 / 2.8 / 3.5 | - |
| QCPMultiGraph_AddDataAtBeginUnsorted | new | 19 | 19 / 18 / 19 / 20 / 19 / 19 | - |
| QCPMultiGraph_AddDataAtEndSorted | new | 3 | 3 / 2.7 / 3.6 / 2.8 / 2.9 / 3.3 | - |
| QCPMultiGraph_AddDataAtEndUnsorted | new | 19 | 19 / 19 / 19 / 19 / 19 / 19 | - |
| QCPMultiGraph_AddDataMixedSorted | new | 4.1 | 4.1 / 4.1 / 3.9 / 3.8 / 4.2 / 4.2 | - |
| QCPMultiGraph_AddDataMixedUnsorted | new | 21 | 21 / 21 / 21 / 21 / 21 / 21 | - |
| QCPMultiGraph_FillToGraphPan10k | new | 0.16 | 0.16 / 0.16 / 0.16 / 0.16 / 0.16 / 0.16 | - |
| QCPMultiGraph_ManyLines | new | 0.16 | 0.16 / 0.16 / 0.16 / 0.16 / 0.16 / 0.16 | - |
| QCPMultiGraph_ManyOffScreenLines | new | 0.14 | 0.14 / 0.14 / 0.14 / 0.14 / 0.14 / 0.15 | - |
| QCPMultiGraph_ManyPoints | new | 0.16 | 0.16 / 0.16 / 0.16 / 0.16 / 0.16 / 0.16 | - |
| QCPMultiGraph_RenderDouble | new | 0.15 | 0.15 / 0.15 / 0.15 / 0.15 / 0.15 / 0.15 | - |
| QCPMultiGraph_RenderFloat | new | 0.15 | 0.15 / 0.15 / 0.15 / 0.15 / 0.15 / 0.15 | - |
| QCPMultiGraph_RenderManyLines | new | 0.48 | 0.48 / 0.48 / 0.48 / 0.48 / 0.48 / 0.47 | - |
| QCPMultiGraph_ScatterDisc1M | new | 0.13 | 0.13 / 0.13 / 0.13 / 0.13 / 0.13 / 0.13 | - |
| QCPMultiGraph_ScatterSquare1M | new | 0.13 | 0.13 / 0.13 / 0.13 / 0.13 / 0.13 / 0.13 | - |
| QCPMultiGraph_ScatterWithLine1M | new | 0.13 | 0.13 / 0.14 / 0.13 / 0.13 / 0.13 / 0.13 | - |
| QCPMultiGraph_SetData1M | 31.0 | 31 | 33 / 33 / 33 / 29 / 29 / 29 | 1.00 |
| QCPMultiGraph_SetDataDouble | new | 9.4 | 9.2 / 9.3 / 9.6 / 9.3 / 9.5 / 9.5 | - |
| QCPMultiGraph_SetDataFloat | new | 8.4 | 8.5 / 8.7 / 8.4 / 8.3 / 8.4 / 8.5 | - |
| QCPMultiGraph_Standard | new | 0.79 | 0.81 / 0.79 / 0.77 / 0.78 / 0.78 / 0.8 | - |
| QCPMultiGraph_UserSystem100k | new | 2.4 | 2.4 / 2.4 / 2.4 / 2.4 / 2.4 / 2.4 | - |
| QCPPlots_Stack8_Aligned_Replot | 0.26 | 0.26 | 0.26 / 0.26 / 0.26 / 0.26 / 0.26 / 0.26 | 1.00 |
| QCPPlots_Stack8_Replot | 0.27 | 0.26 | 0.26 / 0.26 / 0.26 / 0.26 / 0.26 / 0.25 | 0.96 |
| QCPPlottables_Pan200 | 9.3 | 9 | 9 / 9 / 8.9 / 9 / 8.9 / 9.1 | 0.97 |
| QCPPlottables_Zoom200 | 9.4 | 9 | 9 / 9 / 8.9 / 9.2 / 9 / 8.9 | 0.96 |
| QCPPolar_Draw10k | 0.88 | 0.87 | 0.87 / 0.87 / 0.86 / 0.87 / 0.86 / 0.87 | 0.99 |
| QCPPolar_Export10k | 0.91 | 0.91 | 0.91 / 0.9 / 0.9 / 0.89 / 0.91 / 0.91 | 0.99 |
| QCPPolar_RadialZoom10k | 0.95 | 0.94 | 0.94 / 0.95 / 0.93 / 0.94 / 0.94 / 0.94 | 0.99 |
| QCPPolar_Rotate10k | 0.87 | 0.86 | 0.86 / 0.89 / 0.86 / 0.86 / 0.86 / 0.86 | 0.99 |
| QCPSpans_Pan100 | 3.5 | 3.4 | 3.4 / 3.4 / 3.4 / 3.5 / 3.4 / 3.3 | 0.97 |
| QCPSpatialIndex_Build10k | 0.9 | 0.88 | 0.88 / 0.89 / 0.88 / 0.88 / 0.88 / 0.89 | 0.98 |
| QCPSpatialIndex_Query10k | 0.00037 | 0.00036 | 0.00036 / 0.00036 / 0.00036 / 0.00036 / 0.00037 / 0.00036 | 0.97 |
| QCPStatisticalBox_Pan1k | 0.14 | 0.13 | 0.13 / 0.13 / 0.13 / 0.13 / 0.13 / 0.13 | 0.93 |
| QCPTransform_Map1M | 0.87 | 0.7 | 0.71 / 0.68 / 0.63 / 0.74 / 0.76 / 0.64 | 0.80 |
| QObject_DeleteEachChild10k | 8.0 | 7.9 | 7.9 / 7.8 / 7.9 / 7.8 / 7.9 / 8.3 | 0.99 |

## End of milestone 5b

- Measured on the Coder workspace `neoqcp` (LPP cluster, 16 CPUs, dedicated), not on the usual machine: that machine
  was loaded to 18 by another session's build, so the local whole-table runs were void. The numbers below are
  therefore not comparable to the sections above; the comparison is within this section only.
- Whole table: a build of e83708c (end of 5a) and a build of 8ba6da9 (end of 5b, before the perf fix) on the same
  workspace, alternating base and new, three offscreen and three Xvfb runs each, `-median 5`. Ratio = 5b median / 5a
  median (QtTest's msecs, two digits).
- The rows over their gate were measured again with a tick-counter A/B (10 pairs). Four were real:
  QCPAxis_SetRange10k +27 %, QCPAxis_SetTickLength10kItems +14 %, QCPBars_Zoom10k +13 %, QCPItems_RestyleAll10k +9 %
  — handles built on range-change and appearance paths (K8 misses). Fixed in 6ba867b (5c branch); the same A/B
  against end of 5a after the fix: SetRange10k -4.4 %, SetTickLength10kItems +1.5 %, Bars_Zoom10k -1.9 %,
  RestyleAll10k -4.9 %. The others (QCPItems_*, QCPTransform_Map1M, QCPMultiGraph_SetData*) were level on ticks.

| Benchmark | end 5a msecs | end 5b msecs | ratio |
|---|---|---|---|
| QCPAxisRects_Stack8_Replot | 4.4 | 4.4 | 1.00 |
| QCPAxis_SetRange10k | 3.7 | 4.8 | 1.30 |
| QCPAxis_SetTickLength10kItems | 0.033 | 0.044 | 1.33 |
| QCPAxis_TickLabels | 0.965 | 1 | 1.04 |
| QCPAxis_TickLabelsCached | 0.3 | 0.315 | 1.05 |
| QCPBars_Pan10k | 0.56 | 0.52 | 0.93 |
| QCPBars_Zoom10k | 14 | 15 | 1.07 |
| QCPColorMap2_ResampleDouble | 454 | 418 | 0.92 |
| QCPColorMap2_ResampleFloat | 282 | 260 | 0.93 |
| QCPColorMap2_ResamplePanSequence | 326 | 294 | 0.90 |
| QCPColorMap2_ResamplePanSequenceSmall | 546 | 554 | 1.01 |
| QCPColorMap2_ResampleWideParallel | 451 | 460 | 1.02 |
| QCPColorMap2_ResampleWideSerial | 1.93e+03 | 1.9e+03 | 0.99 |
| QCPColorMapData_ConstructLarge | 3 | 3 | 1.00 |
| QCPColorMap_ColorizeMap | 0.39 | 0.38 | 0.97 |
| QCPColorMap_SetCell1M | 8.05 | 8.25 | 1.02 |
| QCPColorMap_Standard | 0.88 | 0.86 | 0.98 |
| QCPColorScale_Rescale8 | 0.00036 | 0.00035 | 0.97 |
| QCPCoords_CoordToPixel1M | 9.9 | 8.9 | 0.90 |
| QCPDrawContext_ReplayData1M | 3.8 | 4 | 1.05 |
| QCPDrawContext_ReplayMarkers100k | 14.5 | 14 | 0.97 |
| QCPDrawn_Pan10k | 0.88 | 0.8 | 0.91 |
| QCPDrawn_Pan10kOneRecording | 3.3 | 2.3 | 0.70 |
| QCPDrawn_Zoom10k | 1 | 1 | 1.00 |
| QCPErrorBars_Pan10k | 0.82 | 0.81 | 0.99 |
| QCPFinancial_Pan10k | 0.55 | 0.525 | 0.95 |
| QCPGpu_Bars10k | 13 | 12 | 0.92 |
| QCPGpu_BarsPan10k | 0.53 | 0.54 | 1.02 |
| QCPGpu_ColorMap2Pan | 0.84 | 0.73 | 0.87 |
| QCPGpu_ColorMap2Zoom | 0.89 | 0.91 | 1.02 |
| QCPGpu_ColorMapPan | 0.79 | 0.66 | 0.84 |
| QCPGpu_CurvePan100k | 0.5 | 0.5 | 1.00 |
| QCPGpu_Graph2Render1M | 0.49 | 0.51 | 1.04 |
| QCPGpu_Histogram2DPan | 0.76 | 0.8 | 1.05 |
| QCPGpu_HistogramPan | 0.52 | 0.54 | 1.04 |
| QCPGpu_LineDashed100k | 1.1 | 1.1 | 1.00 |
| QCPGpu_LineSolid100k | 1.1 | 1.1 | 1.00 |
| QCPGpu_MixedLayer | 23 | 21 | 0.91 |
| QCPGpu_MultiGraphColored100kPan | 0.85 | 0.802 | 0.94 |
| QCPGpu_MultiGraphPan1M | 0.57 | 0.57 | 1.00 |
| QCPGpu_MultiGraphRender1M | 2.8 | 2.8 | 1.00 |
| QCPGpu_MultiGraphZoom1M | 1.7 | 1.7 | 1.00 |
| QCPGpu_Pan100k | 0.69 | 0.7 | 1.01 |
| QCPGpu_Text1k | 3.28 | 3.4 | 1.04 |
| QCPGraph2_RenderDouble | 0.54 | 0.545 | 1.01 |
| QCPGraph2_RenderFloat | 0.545 | 0.54 | 0.99 |
| QCPGraph2_RenderManyLines | 1.35 | 1.35 | 1.00 |
| QCPGraph2_ScatterDisc1M | 0.45 | 0.45 | 1.00 |
| QCPGraph2_ScatterSquare1M | 0.455 | 0.445 | 0.98 |
| QCPGraph2_ScatterWithLine1M | 0.48 | 0.475 | 0.99 |
| QCPGraph2_SetDataDouble | 33 | 33 | 1.00 |
| QCPGraph2_SetDataFloat | 28.5 | 28 | 0.98 |
| QCPGraph_AddDataAtBeginSorted | 12.5 | 12 | 0.96 |
| QCPGraph_AddDataAtBeginUnsorted | 49 | 47.5 | 0.97 |
| QCPGraph_AddDataAtEndSorted | 11 | 10 | 0.91 |
| QCPGraph_AddDataAtEndUnsorted | 38 | 36.5 | 0.96 |
| QCPGraph_AddDataMixedSorted | 21.5 | 21.5 | 1.00 |
| QCPGraph_AddDataMixedUnsorted | 49.5 | 48 | 0.97 |
| QCPGraph_ManyLines | 3.3 | 3.25 | 0.98 |
| QCPGraph_ManyOffScreenLines | 0.59 | 0.6 | 1.02 |
| QCPGraph_ManyPoints | 6.1 | 6 | 0.98 |
| QCPGraph_Standard | 2.15 | 2.15 | 1.00 |
| QCPInput_MouseMoveStorm | 9.6 | 9.95 | 1.04 |
| QCPInput_WheelOverGraph2_1M | 0.0017 | 0.00185 | 1.09 |
| QCPInput_WheelOverMultiGraph_1M | 0.0025 | 0.00265 | 1.06 |
| QCPIntervals_Pan10k | 1.05 | 1 | 0.95 |
| QCPIntervals_Pan10kLabelled | 1.05 | 0.995 | 0.95 |
| QCPItemText_Zoom1k | 2.45 | 2.2 | 0.90 |
| QCPItems_Add10k | 41.5 | 44.5 | 1.07 |
| QCPItems_AddRemove10k | 64.5 | 67 | 1.04 |
| QCPItems_AddRemove20k | 158 | 168 | 1.06 |
| QCPItems_Clear10k | 23 | 23.5 | 1.02 |
| QCPItems_Click100 | 0.019 | 0.018 | 0.95 |
| QCPItems_Click10k | 0.145 | 0.14 | 0.97 |
| QCPItems_ClickEmpty10k | 0.13 | 0.13 | 1.00 |
| QCPItems_DeleteEach10k | 67.5 | 70.5 | 1.04 |
| QCPItems_DeletePlot10k | 60 | 62 | 1.03 |
| QCPItems_DragOne10k | 1.8 | 1.7 | 0.94 |
| QCPItems_FirstFrame10k | 72 | 69.5 | 0.97 |
| QCPItems_MixedKinds_Pan1k | 19.5 | 18 | 0.92 |
| QCPItems_Pan10k | 1.4 | 1.45 | 1.04 |
| QCPItems_RemoveEach10k | 73 | 75.5 | 1.03 |
| QCPItems_Replot10k | 1.4 | 1.5 | 1.07 |
| QCPItems_RestyleAll10k | 4.7 | 4.95 | 1.05 |
| QCPItems_Tracers_Pan1k | 4.95 | 4.8 | 0.97 |
| QCPItems_Zoom100 | 1.4 | 1.45 | 1.04 |
| QCPItems_Zoom10k | 1.4 | 1.5 | 1.07 |
| QCPItems_ZoomEmpty | 0.55 | 0.57 | 1.04 |
| QCPMultiGraph_AddDataAtBeginSorted | 10.5 | 10.4 | 1.00 |
| QCPMultiGraph_AddDataAtBeginUnsorted | 34 | 35 | 1.03 |
| QCPMultiGraph_AddDataAtEndSorted | 10.5 | 11 | 1.05 |
| QCPMultiGraph_AddDataAtEndUnsorted | 33.5 | 35 | 1.04 |
| QCPMultiGraph_AddDataMixedSorted | 10.5 | 11 | 1.05 |
| QCPMultiGraph_AddDataMixedUnsorted | 37.5 | 37.5 | 1.00 |
| QCPMultiGraph_FillToGraphPan10k | 0.615 | 0.645 | 1.05 |
| QCPMultiGraph_ManyLines | 0.62 | 0.63 | 1.02 |
| QCPMultiGraph_ManyOffScreenLines | 0.565 | 0.57 | 1.01 |
| QCPMultiGraph_ManyPoints | 0.61 | 0.63 | 1.03 |
| QCPMultiGraph_RenderDouble | 0.59 | 0.6 | 1.02 |
| QCPMultiGraph_RenderFloat | 0.59 | 0.6 | 1.02 |
| QCPMultiGraph_RenderManyLines | 1.45 | 1.5 | 1.03 |
| QCPMultiGraph_ScatterDisc1M | 0.5 | 0.51 | 1.02 |
| QCPMultiGraph_ScatterSquare1M | 0.5 | 0.51 | 1.02 |
| QCPMultiGraph_ScatterWithLine1M | 0.525 | 0.535 | 1.02 |
| QCPMultiGraph_SetData1M | 79 | 88.5 | 1.12 |
| QCPMultiGraph_SetDataDouble | 32 | 34.5 | 1.08 |
| QCPMultiGraph_SetDataFloat | 28.5 | 29.5 | 1.04 |
| QCPMultiGraph_Standard | 2.3 | 2.3 | 1.00 |
| QCPMultiGraph_UserSystem100k | 5.9 | 5.9 | 1.00 |
| QCPPlots_Stack8_Aligned_Replot | 0.7 | 0.71 | 1.01 |
| QCPPlots_Stack8_Replot | 0.685 | 0.68 | 0.99 |
| QCPPlottables_Pan200 | 21.5 | 21 | 0.98 |
| QCPPlottables_Zoom200 | 22 | 21 | 0.95 |
| QCPPolar_Draw10k | 2.45 | 2.3 | 0.94 |
| QCPPolar_Export10k | 2.5 | 2.45 | 0.98 |
| QCPPolar_RadialZoom10k | 2.6 | 2.55 | 0.98 |
| QCPPolar_Rotate10k | 2.45 | 2.3 | 0.94 |
| QCPSpans_Pan100 | 10.9 | 9.1 | 0.83 |
| QCPSpatialIndex_Build10k | 1.35 | 1.4 | 1.04 |
| QCPSpatialIndex_Query10k | 0.000975 | 0.000985 | 1.01 |
| QCPStatisticalBox_Pan1k | 0.55 | 0.525 | 0.95 |
| QCPTransform_Map1M | 2.3 | 2.65 | 1.15 |
| QObject_DeleteEachChild10k | 23 | 24 | 1.04 |
