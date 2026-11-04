# Milestone 3 — GPU and drawing baseline

Measured at `2eac8a2` (before any milestone 3 library change), for the drawing-API work to compare
against. Rows added in `tests/benchmark/benchmark.cpp`; nothing else changed.

Machine: AMD Ryzen 7 5800X (16 threads), load average 3.34 / 2.83 / 2.08 (1 / 5 / 15 min) while the
three runs were taken. Renderer, from `qt.rhi.general` on the GPU path:

    OpenGL VENDOR: Mesa  RENDERER: llvmpipe (LLVM 22.1.8, 256 bits)  VERSION: 4.6 (Compatibility Profile) Mesa 26.2.2

i.e. **software rasterisation** (no GPU in this container), Xvfb 1600x1200x24, `QT_QPA_PLATFORM=xcb`.
The numbers are reproducible on this machine and comparable across milestone-3 commits; they are not a
hardware-GPU expectation.

## The rows

GPU rows (`QCPGpu_*`) build their own 1000x600 window per row, show it, wait for exposure, wait up to
5 s for `plot->rhi()` and `QSKIP` when the platform has none (offscreen). One `replot()` plus
`processEvents()` runs before the timed loop, so texture and pipeline creation are outside the
measurement.

| row | what it does |
| --- | --- |
| `QCPGpu_LineSolid100k` | one QCPGraph, 100 000 points, solid 1 px pen, zoom step (width alternates 1.0 / 0.98) + `replot(rpImmediateRefresh)` |
| `QCPGpu_LineDashed100k` | the same with `Qt::DashLine` — the QPainter branch on a GPU surface |
| `QCPGpu_Pan100k` | 100 000 points over 0..100, 1-wide window, the range slides by 1 % of the width per step (the pan fast-path candidate) |
| `QCPGpu_Graph2Render1M` | one QCPGraph2, 1 000 000 points, zoom step |
| `QCPGpu_Bars10k` | one QCPBars holding 10 000 bars, zoom step around 50.5 |
| `QCPGpu_Text1k` | 1 000 QCPItemText, zoom step around 50.5 |
| `QCPGpu_MixedLayer` | 100 graphs (1 000 points each) and 100 text items, created alternately on `main` — the GPU-drawn / QPainter-drawn interleaving a default plot has today |

Offscreen rows (they need no QRhi):

| row | what it does |
| --- | --- |
| `QCPBars_Zoom10k` | one QCPBars, 10 000 bars, zoom step around 50.5, 640x360 |
| `QCPBars_Pan10k` | the same bars, range slides by 1 % per step |
| `QCPItemText_Zoom1k` | 1 000 QCPItemText, zoom step around 50.5, 640x360 |

## Measurements

`-median 5`, three consecutive runs each, values are msecs per iteration as printed (`1.2e-01` = 0.12 ms).

GPU rows — `xvfb-run -a -s "-screen 0 1600x1200x24" env QT_QPA_PLATFORM=xcb build-qt612/tests/benchmark/qcp_benchmark -median 5 <rows>`:

| row | run 1 | run 2 | run 3 | median |
| --- | --- | --- | --- | --- |
| QCPGpu_LineSolid100k | 0.44 | 0.44 | 0.44 | **0.44** |
| QCPGpu_LineDashed100k | 0.43 | 0.43 | 0.44 | **0.43** |
| QCPGpu_Pan100k | 0.23 | 0.22 | 0.23 | **0.23** |
| QCPGpu_Graph2Render1M | 0.12 | 0.13 | 0.13 | **0.13** |
| QCPGpu_Bars10k | 8.7 | 8.9 | 8.8 | **8.8** |
| QCPGpu_Text1k | 2.5 | 2.4 | 2.5 | **2.5** |
| QCPGpu_MixedLayer | 8.3 | 8.7 | 8.8 | **8.7** |

Offscreen rows — `QT_QPA_PLATFORM=offscreen build-qt612/tests/benchmark/qcp_benchmark -median 5 <row>`:

| row | run 1 | run 2 | run 3 | median |
| --- | --- | --- | --- | --- |
| QCPBars_Zoom10k | 4.7 | 5.1 | 4.8 | **4.8** |
| QCPBars_Pan10k | 5.2 | 5.2 | 5.2 | **5.2** |
| QCPItemText_Zoom1k | 2.4 | 2.5 | 2.5 | **2.5** |

## What these numbers mean, and what they do not

- **They time the replot, not the frame.** A QRhiWidget frame is compositor-driven: `replot()` lays out,
   paints every dirty layer's QPainter work into its paint buffer, builds/uploads the GPU geometry and
   calls `update()`; the render pass happens later, outside the timed loop. So each row is the *CPU cost
   of producing one frame*, which is the cost the drawing API can change. GPU time itself is not
   measured here (it would need frame-boundary instrumentation, out of scope for this baseline).
- **The dash row does not show the stacking difference.** `QCPGpu_LineDashed100k` (QPainter branch) costs
  the same as the solid row (GPU extrusion) at 100k points, 0.43 against 0.44 ms. The visible difference
  is stacking, not time; the drawing API's job is to remove the branch, not this cost.
- **`QCPGpu_Graph2Render1M` is 0.13 ms for 1M points** because QCPGraph2's L1/L2 caches reduce the
  polyline to about one point per pixel column (`getOptimizedLineData*`), so the drawn geometry is small.
  It measures the cache path, not 1M transformed points.
- **`QCPGpu_Bars10k` and `QCPGpu_MixedLayer` are the slow rows** (8.8 and 8.7 ms): QPainter polygon
  rasterisation on llvmpipe dominates both. They are the rows a "one paint buffer per run" stacking
  design would multiply, and the rows to watch.
- `QCPItemText_Zoom1k` measures the same in both groups (2.5 ms) — the text path is QPainter either way,
  and the GPU window is larger, so the surfaces are not directly comparable across groups.

## How to reproduce

    export PATH="/home/jeandet/Qt/6.12.0/gcc_64/bin:/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin:$PATH"
    export PKG_CONFIG_PATH="/home/jeandet/Qt/6.12.0/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH"
    xvfb-run -a -s "-screen 0 1600x1200x24" env QT_QPA_PLATFORM=xcb \
      build-qt612/tests/benchmark/qcp_benchmark -median 5 \
      QCPGpu_LineSolid100k QCPGpu_LineDashed100k QCPGpu_Pan100k QCPGpu_Graph2Render1M \
      QCPGpu_Bars10k QCPGpu_Text1k QCPGpu_MixedLayer
    QT_QPA_PLATFORM=offscreen build-qt612/tests/benchmark/qcp_benchmark -median 5 \
      QCPBars_Zoom10k QCPBars_Pan10k QCPItemText_Zoom1k

The rows must be listed one by one: QtTest does not expand a wildcard in the test-function argument
(`qcp_benchmark 'QCPGpu_*'` answers *Unknown test function*). Offscreen, the GPU rows run and report
`SKIP: no QRhi on this platform (qoffscreen)` after waiting the full 5 s for a QRhi.
