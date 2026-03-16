# QCPHistogram2D — 2D Histogram Plottable

**Date:** 2026-03-16
**Status:** Draft

## Problem

Producing a column-normalized 2D histogram (e.g. solar wind speed vs helium abundance) currently requires the user to bin their scatter data externally, then feed the pre-gridded result to QCPColorMap2. The binning, normalization, and rendering are all the user's responsibility. This is the wrong abstraction — the library should accept raw (key, value) scatter data and handle the rest.

## Design

### Input

Same 1D scatter data interface as QCPGraph2: the user provides keys and values via `QCPAbstractDataSource`. Ownership semantics are identical — `setData(keys, values)` for owning, `viewData(span, span)` for non-owning views, or `setDataSource(shared_ptr)` for shared sources.

### Binning

User specifies bin counts per axis via `setBins(int keyBins, int valueBins)` with sensible defaults (100, 100). Ranges are auto-computed from the data. The histogram is computed once per data change (not per viewport change) in a `ViewportIndependent` async pipeline transform.

The pipeline type is new: `QCPHistogramPipeline = QCPAsyncPipeline<QCPAbstractDataSource, QCPColorMapData>`. This alias is added to `src/datasource/async-pipeline.h` alongside the existing `QCPGraphPipeline` and `QCPColormapPipeline` aliases. Input is 1D scatter data, output is a regular 2D grid of raw counts.

### Binning Algorithm

```
1. Compute keyRange, valueRange from source (full scan)
2. For each point (key, value):
   - Skip if NaN or non-finite
   - Compute bin indices: kBin = clamp(int((key - keyLo) / keyBinWidth), 0, nk-1)
                          vBin = clamp(int((val - valLo) / valBinWidth), 0, nv-1)
   - Increment counts[kBin][vBin]
3. Store in QCPColorMapData(nk, nv, keyRange, valueRange)
```

The bin counts are captured by value into the async transform lambda at job-creation time (when `setBins()` triggers `onDataChanged()`). This avoids thread-safety issues — the background job never reads `mKeyBins`/`mValueBins` directly.

### Normalization

Raw counts are stored in the `QCPColorMapData`. Normalization is applied at display time (during colorization), not during binning. This means:

- Changing normalization mode only invalidates the cached QImage, no rebinning needed.
- The colorbar reflects normalized values.

Initial modes:

```cpp
enum Normalization { nNone, nColumn };
```

- `nNone`: raw counts displayed directly
- `nColumn`: each column divided by its column sum, producing a probability distribution per column (values in 0–1 range, each column sums to 1). This matches matplotlib's default column normalization.

Adding `nRow`, `nGlobal`, etc. later is just a new enum value + a case in the normalization function.

**Normalization implementation**: Before calling `updateMapImage`, the plottable precomputes a `std::vector<double> columnSums` (one entry per key bin) from the raw `QCPColorMapData`. This vector is captured by the normalization lambda passed to `updateMapImage`, so the renderer itself has no histogram knowledge.

### Shared Colormap Rendering — QCPColormapRenderer

QCPColorMap2's rendering logic is extracted into a reusable component class that both QCPColorMap2 and QCPHistogram2D compose:

```cpp
class QCPColormapRenderer
{
public:
    explicit QCPColormapRenderer(QCPAbstractPlottable* owner);

    // State
    void setGradient(const QCPColorGradient& gradient);
    void setDataRange(const QCPRange& range);
    void setDataScaleType(QCPAxis::ScaleType type);
    void setColorScale(QCPColorScale* scale);
    void rescaleDataRange(const QCPColorMapData* data, bool recalc);

    // Rendering
    // Takes grid data + optional normalization function, produces QImage
    void updateMapImage(const QCPColorMapData* data,
                        std::function<double(double, int col, int row)> normalize = {});
    void draw(QCPPainter* painter, QCPAxis* keyAxis, QCPAxis* valueAxis,
              const QCPRange& keyRange, const QCPRange& valueRange);

    // RHI layer management
    void ensureRhiLayer(QCustomPlot* plot);
    void releaseRhiLayer(QCustomPlot* plot);

    // Accessors
    const QCPColorGradient& gradient() const;
    QCPRange dataRange() const;
    QCPAxis::ScaleType dataScaleType() const;
    bool mapImageInvalidated() const;
    void invalidateMapImage();

private:
    QCPAbstractPlottable* mOwner;
    QCPColorGradient mGradient;
    QCPRange mDataRange;
    QCPAxis::ScaleType mDataScaleType = QCPAxis::stLinear;
    QImage mMapImage;
    bool mMapImageInvalidated = true;
    QCPColorScale* mColorScale = nullptr;
    QCPColormapRhiLayer* mRhiLayer = nullptr;
};
```

The `normalize` callback passed to `updateMapImage` allows QCPHistogram2D to inject column-normalization without the renderer knowing about histogram semantics. QCPColorMap2 passes no callback (or identity).

**Responsibility boundary**: The renderer owns gradient/colorization state, the QImage cache, and RHI layer management. The plottable owns viewport-to-pixel coordinate mapping (via `coordToPixel`) and computes the pixel-space `QRectF` that it passes to `draw()`. The renderer's `draw()` takes fully-resolved axis and range parameters — it does not call `coordToPixel` itself. `ensureRhiLayer`/`releaseRhiLayer` are called by the plottable's `draw()` and destructor respectively.

### QCPHistogram2D Public API

```cpp
class QCPHistogram2D : public QCPAbstractPlottable
{
    Q_OBJECT
public:
    enum Normalization { nNone, nColumn };

    QCPHistogram2D(QCPAxis* keyAxis, QCPAxis* valueAxis);
    ~QCPHistogram2D() override;

    // Data — same pattern as QCPGraph2
    template<typename KC, typename VC>
    void setData(KC&& keys, VC&& values);
    template<typename KSpan, typename VSpan>
    void viewData(KSpan keys, VSpan values);
    void setDataSource(std::shared_ptr<QCPAbstractDataSource> source);
    void setDataSource(std::unique_ptr<QCPAbstractDataSource> source);
    const QCPAbstractDataSource* dataSource() const;
    void dataChanged();

    // Binning
    void setBins(int keyBins, int valueBins);
    int keyBins() const;
    int valueBins() const;

    // Normalization (display-time only)
    void setNormalization(Normalization norm);
    Normalization normalization() const;

    // Forwarded to QCPColormapRenderer
    void setGradient(const QCPColorGradient& gradient);
    void setDataRange(const QCPRange& range);
    void setDataScaleType(QCPAxis::ScaleType type);
    void setColorScale(QCPColorScale* scale);
    void rescaleDataRange(bool recalc = false);

    QCPHistogramPipeline& pipeline();
    const QCPHistogramPipeline& pipeline() const;

    // QCPAbstractPlottable overrides
    void draw(QCPPainter* painter) override;
    void drawLegendIcon(QCPPainter*, const QRectF&) const override;
    double selectTest(const QPointF&, bool, QVariant*) const override;
    QCPRange getKeyRange(bool&, QCP::SignDomain) const override;
    QCPRange getValueRange(bool&, QCP::SignDomain, const QCPRange&) const override;

private:
    std::shared_ptr<QCPAbstractDataSource> mDataSource;
    int mKeyBins = 100;
    int mValueBins = 100;
    Normalization mNormalization = nNone;
    QCPHistogramPipeline mPipeline;  // scheduler from mParentPlot->pipelineScheduler()
    QCPColormapRenderer mRenderer;
};
```

No `QCPPlottableInterface1D` — selecting individual scatter points in a histogram is not meaningful.

### Data Flow Summary

```
setData(keys, values)
  → shared_ptr<QCPAbstractDataSource>
  → mPipeline.setSource(...)
  → async transform (ViewportIndependent, Heavy priority):
      capture keyBins, valueBins by value
      bin all points → QCPColorMapData (raw counts)
  → mPipeline.result() = QCPColorMapData*
  → draw():
      precompute columnSums from result
      normalizeFn = [columnSums](double v, int col, int) { return v / columnSums[col]; }
      mRenderer.updateMapImage(result, normalizeFn)
      compute pixel QRectF from coordToPixel()
      mRenderer.draw(painter, keyAxis, valueAxis, ...)
  → screen
```

### File Layout

| File | Contents |
|---|---|
| `src/plottables/plottable-histogram2d.h` | `QCPHistogram2D` class declaration |
| `src/plottables/plottable-histogram2d.cpp` | Implementation |
| `src/painting/colormap-renderer.h` | `QCPColormapRenderer` (extracted from QCPColorMap2) |
| `src/painting/colormap-renderer.cpp` | Implementation |
| `src/datasource/histogram-binner.h` | `qcp::algo::bin2d()` — the binning algorithm (header-only, inline) |
| `src/datasource/async-pipeline.h` | `QCPHistogramPipeline` type alias (added alongside existing aliases) |
| `tests/auto/test-pipeline/` | Tests for binning, normalization, pipeline integration |
| `tests/manual/gallery/main.cpp` | New gallery tabs: Histogram2D (linear) and Histogram2D (log) |

### Gallery Entries

Two new tabs in the gallery:

1. **Histogram2D** — scatter data from a bivariate normal distribution, linear axes, column-normalized, with a colorscale.
2. **Histogram2D (log)** — same data but with logarithmic value axis, demonstrating log-scale rendering.

### Tests

- `bin2d` unit tests: correct counts, NaN/Inf skipping, empty input, single-bin edge case
- `QCPHistogram2D` pipeline: data → binned result, correct grid dimensions
- Normalization: column-norm produces expected values, toggling doesn't rebind
- Rendering smoke test: replot doesn't crash
- `QCPColormapRenderer` extraction: QCPColorMap2 still works identically after refactor

### Migration — QCPColorMap2 Refactor

QCPColorMap2 is refactored to use `QCPColormapRenderer` internally. This is a pure refactor with no API change:

- Extract `mGradient`, `mDataRange`, `mDataScaleType`, `mColorScale`, `mMapImage`, `mMapImageInvalidated`, `mRhiLayer`, and associated methods into `QCPColormapRenderer`.
- QCPColorMap2's `draw()` delegates to `mRenderer.updateMapImage(result)` and `mRenderer.draw(...)`.
- QCPColorMap2 retains viewport-to-pixel coordinate mapping and calls `mRenderer.draw()` with resolved parameters.
- All existing QCPColorMap2 tests must pass unchanged.

### Not In Scope

- Row normalization, global normalization (future enum values)
- Explicit bin edges (future API addition)
- Overlaid fit curves (user adds QCPGraph2 instances)
- Selection/interaction on individual histogram bins
- Adaptive bin sizing
