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
