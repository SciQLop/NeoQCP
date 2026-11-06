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
