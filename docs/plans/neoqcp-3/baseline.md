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
