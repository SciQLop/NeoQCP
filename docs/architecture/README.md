# NeoQCP Architecture Documentation

This directory documents the internal architecture of NeoQCP's rendering and data processing pipelines.

## Documents

| Document | Description |
|---|---|
| [QRhi Rendering Pipeline](qrhi-rendering-pipeline.md) | How NeoQCP composites layers on the GPU using Qt's Rendering Hardware Interface |
| [Adaptive Sampling](adaptive-sampling.md) | How QCPGraph reduces millions of data points to pixel resolution for line plots |
| [Colormap Resampling](colormap-resampling.md) | How QCPColorMap2 bins 2D data onto a screen-resolution grid in a worker thread |
| [Gap Detection](gap-detection.md) | How data gaps are detected in both 1D line plots and 2D colormaps |
| [Async Pipeline](async-pipeline.md) | The shared async transform infrastructure used by all plottables |

## Reading Order

1. Start with **QRhi Rendering Pipeline** to understand how pixels reach the screen
2. Read **Async Pipeline** to understand how data transforms are scheduled off the GUI thread
3. Then read **Adaptive Sampling** (1D graphs) or **Colormap Resampling** (2D heatmaps) depending on your interest
4. **Gap Detection** is a cross-cutting concern referenced by both sampling docs
