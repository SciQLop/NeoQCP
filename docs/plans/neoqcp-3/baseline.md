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
