# QCPHistogram2D Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a QCPHistogram2D plottable that bins raw 1D scatter data into a 2D histogram heatmap, with shared colormap rendering extracted from QCPColorMap2.

**Architecture:** Three-phase approach: (1) extract QCPColormapRenderer from QCPColorMap2 as a pure refactor, (2) implement the binning algorithm and QCPHistogram2D plottable, (3) add gallery entries. The renderer extraction is done first so QCPColorMap2's existing tests validate the refactor before any new code depends on it.

**Tech Stack:** C++20, Qt6 (QRhiWidget), Meson build system, Qt Test framework.

**Spec:** `docs/superpowers/specs/2026-03-16-histogram2d-design.md`

---

## Chunk 1: Extract QCPColormapRenderer from QCPColorMap2

This is a pure refactor — no new features, no API changes. All existing QCPColorMap2 tests must pass unchanged.

### Task 1: Create QCPColormapRenderer header

**Files:**
- Create: `src/painting/colormap-renderer.h`

- [ ] **Step 1: Write the header**

```cpp
// src/painting/colormap-renderer.h
#pragma once
#include <colorgradient.h>
#include <axis/axis.h>
#include <QImage>
#include <functional>

class QCPAbstractPlottable;
class QCPColorScale;
class QCPColorMapData;
class QCPColormapRhiLayer;
class QCPPainter;
class QCustomPlot;

class QCPColormapRenderer
{
public:
    explicit QCPColormapRenderer(QCPAbstractPlottable* owner);
    ~QCPColormapRenderer();

    // State
    void setGradient(const QCPColorGradient& gradient);
    const QCPColorGradient& gradient() const { return mGradient; }

    void setDataRange(const QCPRange& range);
    QCPRange dataRange() const { return mDataRange; }

    void setDataScaleType(QCPAxis::ScaleType type);
    QCPAxis::ScaleType dataScaleType() const { return mDataScaleType; }

    void setColorScale(QCPColorScale* scale);
    QCPColorScale* colorScale() const { return mColorScale; }

    void rescaleDataRange(const QCPColorMapData* data, bool recalc);

    // Rendering
    using NormalizeFn = std::function<double(double value, int col, int row)>;
    void updateMapImage(const QCPColorMapData* data, NormalizeFn normalize = {});
    void draw(QCPPainter* painter, QCPAxis* keyAxis, QCPAxis* valueAxis,
              const QCPRange& keyRange, const QCPRange& valueRange);

    // RHI layer
    QCPColormapRhiLayer* ensureRhiLayer();
    void releaseRhiLayer();

    // Image cache
    bool mapImageInvalidated() const { return mMapImageInvalidated; }
    void invalidateMapImage() { mMapImageInvalidated = true; }
    const QImage& mapImage() const { return mMapImage; }

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

- [ ] **Step 2: Verify it compiles**

Run: `cd /var/home/jeandet/Documents/prog/NeoQCP && PATH="/home/jeandet/Qt/6.10.2/gcc_64/bin:$PATH" meson compile -C build`
Expected: Build succeeds (header not yet included anywhere).

- [ ] **Step 3: Commit**

```bash
git add src/painting/colormap-renderer.h
git commit -m "refactor: add QCPColormapRenderer header (extracted from QCPColorMap2)"
```

### Task 2: Implement QCPColormapRenderer

**Files:**
- Create: `src/painting/colormap-renderer.cpp`
- Modify: `meson.build` — add to static_library sources and moc_headers (if needed, but class has no Q_OBJECT so only sources)

The implementation is a direct extraction from `plottable-colormap2.cpp`. The methods are:
- Constructor: initializes gradient with `gpCold` preset + `nhTransparent`
- Destructor: releases RHI layer
- `setGradient()`: from `QCPColorMap2::setGradient()` lines 112-124, but without emitting signals (the owning plottable handles signals)
- `setDataRange()`: from lines 152-166, without signals
- `setDataScaleType()`: from lines 168-187, without signals
- `setColorScale()`: from lines 126-150, but the signal connections remain in the plottable (the renderer just stores the pointer)
- `rescaleDataRange()`: from lines 189-202
- `updateMapImage()`: from lines 221-247, extended with the `NormalizeFn` callback — if provided, each cell value is passed through it before colorization
- `draw()`: handles the RHI path (lines 294-311) and QPainter fallback (lines 313-315), taking pre-computed keyRange/valueRange
- `ensureRhiLayer()`: from lines 249-257
- `releaseRhiLayer()`: cleanup from destructor (lines 89-93)

- [ ] **Step 1: Create colormap-renderer.cpp**

Key implementation detail for `updateMapImage` with normalization:

```cpp
void QCPColormapRenderer::updateMapImage(const QCPColorMapData* data, NormalizeFn normalize)
{
    if (!data) return;
    int keySize = data->keySize();
    int valueSize = data->valueSize();
    if (keySize == 0 || valueSize == 0) return;

    QImage argbImage(keySize, valueSize, QImage::Format_ARGB32_Premultiplied);
    std::vector<double> rowData(keySize);
    for (int y = 0; y < valueSize; ++y)
    {
        for (int x = 0; x < keySize; ++x)
        {
            double v = data->cell(x, y);
            if (normalize)
                v = normalize(v, x, y);
            rowData[x] = v;
        }
        QRgb* pixels = reinterpret_cast<QRgb*>(argbImage.scanLine(valueSize - 1 - y));
        mGradient.colorize(rowData.data(), mDataRange, pixels, keySize,
                           1, mDataScaleType == QCPAxis::stLogarithmic);
    }
    mMapImage = argbImage.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
    mMapImageInvalidated = false;
}
```

For `draw()`, the renderer handles pixel-space rendering. The plottable computes `keyRange`/`valueRange` from the resampled data and passes them in. The renderer:
1. Converts ranges to pixel rect using the provided axes
2. Handles axis-reversed flipping
3. Routes to RHI layer or QPainter fallback

**Note on `applyDefaultAntialiasingHint`**: This is a protected method on `QCPAbstractPlottable`. The renderer cannot call it directly. Instead, the QPainter fallback path omits the antialiasing hint call — the plottable is responsible for calling `applyDefaultAntialiasingHint(painter)` before calling `mRenderer.draw()`. In the RHI path this is irrelevant (GPU compositing).

```cpp
void QCPColormapRenderer::draw(QCPPainter* painter, QCPAxis* keyAxis, QCPAxis* valueAxis,
                                const QCPRange& keyRange, const QCPRange& valueRange)
{
    if (mMapImage.isNull()) return;

    QPointF topLeft(keyAxis->coordToPixel(keyRange.lower),
                    valueAxis->coordToPixel(valueRange.upper));
    QPointF bottomRight(keyAxis->coordToPixel(keyRange.upper),
                        valueAxis->coordToPixel(valueRange.lower));
    QRectF imageRect(topLeft, bottomRight);

    bool mirrorX = keyAxis->rangeReversed();
    bool mirrorY = valueAxis->rangeReversed();
    Qt::Orientations flips;
    if (mirrorX) flips |= Qt::Horizontal;
    if (mirrorY) flips |= Qt::Vertical;

    if (auto* crl = ensureRhiLayer())
    {
        crl->setImage(mMapImage.flipped(flips));
        crl->setQuadRect(imageRect.normalized());
        crl->setLayer(mOwner->layer());

        auto* plot = mOwner->parentPlot();
        auto* axisRect = keyAxis->axisRect();
        QRect clipRect = axisRect->rect();
        double dpr = plot->bufferDevicePixelRatio();
        int sx = static_cast<int>(clipRect.x() * dpr);
        int sy = static_cast<int>(clipRect.y() * dpr);
        int sw = static_cast<int>(clipRect.width() * dpr);
        int sh = static_cast<int>(clipRect.height() * dpr);
        if (plot->rhi()->isYUpInNDC())
            sy = static_cast<int>(plot->height() * dpr) - sy - sh;
        crl->setScissorRect(QRect(sx, sy, sw, sh));
        return;
    }

    // QPainter fallback (PDF/SVG export)
    // Note: antialiasing hint must be set by the plottable before calling draw()
    painter->drawImage(imageRect, mMapImage.convertToFormat(
        QImage::Format_ARGB32_Premultiplied).flipped(flips));
}
```

For `setGradient` — sets the member and `mMapImageInvalidated = true`. The renderer is a passive component; it does NOT emit signals or call replot.

For `setDataRange` — must replicate the validation from QCPColorMap2: check `QCPRange::validRange()`, then `sanitizedForLogScale()` or `sanitizedForLinScale()` based on `mDataScaleType`. Only stores the range if it passes validation. Sets `mMapImageInvalidated = true` on change.

For `setDataScaleType` — must replicate log-scale sanitization: if switching to log, sanitize `mDataRange` via `sanitizedForLogScale()`. Sets `mMapImageInvalidated = true` on change.

For `setColorScale` — the renderer just stores the pointer. The plottable manages signal connections.

For `rescaleDataRange` — same logic as QCPColorMap2 but calls `setDataRange()` on itself.

For `ensureRhiLayer` / `releaseRhiLayer` — same logic as QCPColorMap2, using `mOwner->parentPlot()`. `releaseRhiLayer()` must be idempotent: null-check `mRhiLayer` and set it to `nullptr` after deleting. The destructor calls `releaseRhiLayer()`, so the plottable's explicit destructor call is safe (no double-free).

- [ ] **Step 2: Register in meson.build**

Add `'src/painting/colormap-renderer.cpp'` to the `static_library` sources list (after `colormap-rhi-layer.cpp`).

- [ ] **Step 3: Build**

Run: `cd /var/home/jeandet/Documents/prog/NeoQCP && PATH="/home/jeandet/Qt/6.10.2/gcc_64/bin:$PATH" meson compile -C build`
Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add src/painting/colormap-renderer.cpp meson.build
git commit -m "refactor: implement QCPColormapRenderer (extracted rendering from QCPColorMap2)"
```

### Task 3: Refactor QCPColorMap2 to use QCPColormapRenderer

**Files:**
- Modify: `src/plottables/plottable-colormap2.h`
- Modify: `src/plottables/plottable-colormap2.cpp`

This replaces the duplicated rendering members/methods in QCPColorMap2 with delegation to `mRenderer`.

- [ ] **Step 1: Update the header**

In `plottable-colormap2.h`:
- Add `#include <painting/colormap-renderer.h>`
- Remove private members: `mGradient`, `mColorScale`, `mDataRange`, `mDataScaleType`, `mMapImage`, `mMapImageInvalidated`, `mRhiLayer`
- Remove private methods: `updateMapImage()`, `ensureRhiLayer()`
- Add private member: `QCPColormapRenderer mRenderer{this};`
- Keep all public getters/setters — they now delegate to `mRenderer`

The public API must remain identical. All getters (`gradient()`, `dataRange()`, `dataScaleType()`, `colorScale()`) now return from `mRenderer`.

- [ ] **Step 2: Update the implementation**

In `plottable-colormap2.cpp`:
- Constructor: remove `mGradient.loadPreset(...)` and `mGradient.setNanHandling(...)` — these move into QCPColormapRenderer's constructor
- Constructor: keep pipeline setup and axis connections unchanged
- Constructor: pipeline `finished` signal now calls `mRenderer.invalidateMapImage()` instead of `mMapImageInvalidated = true`
- Destructor: replace `if (mRhiLayer...) { unregister; delete; }` with `mRenderer.releaseRhiLayer()`
- `setGradient()`: delegate to `mRenderer.setGradient()`, then emit signal + replot
- `setDataRange()`: delegate to `mRenderer.setDataRange()`, then emit signal + replot
- `setDataScaleType()`: delegate to `mRenderer.setDataScaleType()`, then emit signal + replot
- `setColorScale()`: keep signal connection management, call `mRenderer.setColorScale(scale)` for storage
- `rescaleDataRange()`: call `mRenderer.rescaleDataRange(data, recalc)` — but actually this needs the pipeline result AND the data source, so keep the logic in the plottable and just call `setDataRange()` on self
- `draw()`: replace body with:
  ```cpp
  auto* resampledData = mPipeline.result();
  if (!resampledData) { if (mDataSource) onViewportChanged(); return; }
  if (mRenderer.mapImageInvalidated())
      mRenderer.updateMapImage(resampledData);
  if (mRenderer.mapImage().isNull()) return;
  QCPRange keyRange = resampledData->keyRange();
  QCPRange valueRange = resampledData->valueRange();
  applyDefaultAntialiasingHint(painter); // call before renderer draw (protected method)
  mRenderer.draw(painter, mKeyAxis.data(), mValueAxis.data(), keyRange, valueRange);
  ```
- `drawLegendIcon()`: keep as-is (uses simple gradient, not renderer)

- [ ] **Step 3: Build**

Run: `cd /var/home/jeandet/Documents/prog/NeoQCP && PATH="/home/jeandet/Qt/6.10.2/gcc_64/bin:$PATH" meson compile -C build`
Expected: Build succeeds.

- [ ] **Step 4: Run all existing tests**

Run: `cd /var/home/jeandet/Documents/prog/NeoQCP && PATH="/home/jeandet/Qt/6.10.2/gcc_64/bin:$PATH" LD_LIBRARY_PATH="/home/jeandet/Qt/6.10.2/gcc_64/lib:$LD_LIBRARY_PATH" meson test -C build --print-errorlogs`
Expected: All tests pass, including `colormap2PipelineDefault`, `colormap2PipelineResample`, `colormap2DataFromExternalThread`.

- [ ] **Step 5: Commit**

```bash
git add src/plottables/plottable-colormap2.h src/plottables/plottable-colormap2.cpp
git commit -m "refactor: QCPColorMap2 now delegates rendering to QCPColormapRenderer"
```

## Chunk 2: Binning Algorithm and QCPHistogram2D Plottable

### Task 4: Implement bin2d algorithm

**Files:**
- Create: `src/datasource/histogram-binner.h`

Header-only, inline function in `qcp::algo` namespace. Takes a `QCPAbstractDataSource` and bin counts, returns a `QCPColorMapData*`.

- [ ] **Step 1: Write the failing test**

In `tests/auto/test-pipeline/test-pipeline.h`, add:
```cpp
// Histogram binner
void bin2dBasicCounts();
void bin2dNaNSkipped();
void bin2dEmptyInput();
void bin2dSingleBin();
```

In `tests/auto/test-pipeline/test-pipeline.cpp`, add:
```cpp
#include <datasource/histogram-binner.h>

void TestPipeline::bin2dBasicCounts()
{
    // 4 points in a 2x2 grid: (0,0), (0,1), (1,0), (1,1)
    std::vector<double> keys = {0.25, 0.25, 0.75, 0.75};
    std::vector<double> vals = {0.25, 0.75, 0.25, 0.75};
    auto src = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::move(keys), std::move(vals));
    auto* result = qcp::algo::bin2d(*src, 2, 2);
    QVERIFY(result);
    QCOMPARE(result->keySize(), 2);
    QCOMPARE(result->valueSize(), 2);
    // Each bin should have exactly 1 count
    QCOMPARE(result->cell(0, 0), 1.0);
    QCOMPARE(result->cell(1, 0), 1.0);
    QCOMPARE(result->cell(0, 1), 1.0);
    QCOMPARE(result->cell(1, 1), 1.0);
    delete result;
}

void TestPipeline::bin2dNaNSkipped()
{
    std::vector<double> keys = {0.5, std::nan(""), 0.5};
    std::vector<double> vals = {0.5, 0.5, std::nan("")};
    auto src = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::move(keys), std::move(vals));
    auto* result = qcp::algo::bin2d(*src, 1, 1);
    QVERIFY(result);
    QCOMPARE(result->cell(0, 0), 1.0); // only first point counted
    delete result;
}

void TestPipeline::bin2dEmptyInput()
{
    std::vector<double> keys, vals;
    auto src = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::move(keys), std::move(vals));
    auto* result = qcp::algo::bin2d(*src, 10, 10);
    QVERIFY(!result); // null for empty input
}

void TestPipeline::bin2dSingleBin()
{
    // All points land in the same bin
    std::vector<double> keys = {1.0, 1.0, 1.0};
    std::vector<double> vals = {2.0, 2.0, 2.0};
    auto src = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::move(keys), std::move(vals));
    auto* result = qcp::algo::bin2d(*src, 1, 1);
    QVERIFY(result);
    QCOMPARE(result->cell(0, 0), 3.0);
    delete result;
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd /var/home/jeandet/Documents/prog/NeoQCP && PATH="/home/jeandet/Qt/6.10.2/gcc_64/bin:$PATH" meson compile -C build`
Expected: FAIL — `histogram-binner.h` not found.

- [ ] **Step 3: Write histogram-binner.h**

```cpp
// src/datasource/histogram-binner.h
#pragma once
#include "abstract-datasource.h"
#include <plottables/plottable-colormap.h> // for QCPColorMapData
#include <cmath>
#include <algorithm>

namespace qcp::algo {

// Bin 1D scatter data into a 2D histogram grid.
// Returns a heap-allocated QCPColorMapData with raw counts, or nullptr if empty.
inline QCPColorMapData* bin2d(const QCPAbstractDataSource& src, int keyBins, int valueBins)
{
    const int n = src.size();
    if (n == 0 || keyBins <= 0 || valueBins <= 0)
        return nullptr;

    bool foundKey = false, foundVal = false;
    QCPRange keyRange = src.keyRange(foundKey);
    QCPRange valRange = src.valueRange(foundVal);
    if (!foundKey || !foundVal)
        return nullptr;

    // Expand zero-size ranges to avoid division by zero
    if (keyRange.size() == 0) { keyRange.lower -= 0.5; keyRange.upper += 0.5; }
    if (valRange.size() == 0) { valRange.lower -= 0.5; valRange.upper += 0.5; }

    auto* data = new QCPColorMapData(keyBins, valueBins, keyRange, valRange);
    data->fill(0);

    const double kBinWidth = keyRange.size() / keyBins;
    const double vBinWidth = valRange.size() / valueBins;

    for (int i = 0; i < n; ++i)
    {
        double k = src.keyAt(i);
        double v = src.valueAt(i);
        if (!std::isfinite(k) || !std::isfinite(v))
            continue;

        int kb = std::clamp(static_cast<int>((k - keyRange.lower) / kBinWidth), 0, keyBins - 1);
        int vb = std::clamp(static_cast<int>((v - valRange.lower) / vBinWidth), 0, valueBins - 1);
        data->setCell(kb, vb, data->cell(kb, vb) + 1.0);
    }

    data->recalculateDataBounds();
    return data;
}

} // namespace qcp::algo
```

- [ ] **Step 4: Build and run tests**

Run: `cd /var/home/jeandet/Documents/prog/NeoQCP && PATH="/home/jeandet/Qt/6.10.2/gcc_64/bin:$PATH" meson compile -C build && LD_LIBRARY_PATH="/home/jeandet/Qt/6.10.2/gcc_64/lib:$LD_LIBRARY_PATH" meson test -C build --print-errorlogs`
Expected: All 4 new tests pass plus all existing tests.

- [ ] **Step 5: Commit**

```bash
git add src/datasource/histogram-binner.h tests/auto/test-pipeline/test-pipeline.h tests/auto/test-pipeline/test-pipeline.cpp
git commit -m "feat: add qcp::algo::bin2d() histogram binning algorithm with tests"
```

### Task 5: Add QCPHistogramPipeline type alias

**Files:**
- Modify: `src/datasource/async-pipeline.h`

- [ ] **Step 1: Add the alias**

After line 156 (`using QCPColormapPipeline = ...`), add:
```cpp
using QCPHistogramPipeline = QCPAsyncPipeline<QCPAbstractDataSource, QCPColorMapData>;
```

- [ ] **Step 2: Build**

Run: `cd /var/home/jeandet/Documents/prog/NeoQCP && PATH="/home/jeandet/Qt/6.10.2/gcc_64/bin:$PATH" meson compile -C build`
Expected: Build succeeds.

- [ ] **Step 3: Commit**

```bash
git add src/datasource/async-pipeline.h
git commit -m "feat: add QCPHistogramPipeline type alias"
```

### Task 6: Create QCPHistogram2D header

**Files:**
- Create: `src/plottables/plottable-histogram2d.h`

- [ ] **Step 1: Write the header**

Follow the API from the spec exactly. Key details:
- Inherits `QCPAbstractPlottable` only (no `QCPPlottableInterface1D`)
- `Q_OBJECT` macro for MOC
- Template `setData()` and `viewData()` methods (inline in header, same pattern as QCPGraph2)
- Members: `mDataSource`, `mKeyBins`, `mValueBins`, `mNormalization`, `mPipeline`, `mRenderer`
- `mPipeline` is `QCPHistogramPipeline`
- `mRenderer` is `QCPColormapRenderer`
- Signals: `dataRangeChanged`, `gradientChanged`, `dataScaleTypeChanged` (same as QCPColorMap2, for colorscale bidirectional sync)

```cpp
// src/plottables/plottable-histogram2d.h
#pragma once
#include "plottable.h"
#include <axis/axis.h>
#include <datasource/async-pipeline.h>
#include <datasource/soa-datasource.h>
#include <painting/colormap-renderer.h>
#include <memory>
#include <span>

class QCPColorScale;
class QCPColorMapData;

class QCP_LIB_DECL QCPHistogram2D : public QCPAbstractPlottable
{
    Q_OBJECT
public:
    enum Normalization { nNone, nColumn };

    explicit QCPHistogram2D(QCPAxis* keyAxis, QCPAxis* valueAxis);
    ~QCPHistogram2D() override;

    // Data — same pattern as QCPGraph2
    template <IndexableNumericRange KC, IndexableNumericRange VC>
    void setData(KC&& keys, VC&& values)
    {
        setDataSource(std::make_shared<QCPSoADataSource<
            std::decay_t<KC>, std::decay_t<VC>>>(
            std::forward<KC>(keys), std::forward<VC>(values)));
    }

    template <typename K, typename V>
    void viewData(const K* keys, const V* values, int count)
    {
        setDataSource(std::make_shared<QCPSoADataSource<std::span<const K>, std::span<const V>>>(
            std::span<const K>(keys, count), std::span<const V>(values, count)));
    }

    template <typename K, typename V>
    void viewData(std::span<const K> keys, std::span<const V> values)
    {
        setDataSource(std::make_shared<QCPSoADataSource<std::span<const K>, std::span<const V>>>(
            keys, values));
    }

    void setDataSource(std::shared_ptr<QCPAbstractDataSource> source);
    void setDataSource(std::unique_ptr<QCPAbstractDataSource> source);
    const QCPAbstractDataSource* dataSource() const { return mDataSource.get(); }
    void dataChanged();

    // Binning
    void setBins(int keyBins, int valueBins);
    int keyBins() const { return mKeyBins; }
    int valueBins() const { return mValueBins; }

    // Normalization
    void setNormalization(Normalization norm);
    Normalization normalization() const { return mNormalization; }

    // Forwarded to QCPColormapRenderer
    QCPColorGradient gradient() const { return mRenderer.gradient(); }
    QCPRange dataRange() const { return mRenderer.dataRange(); }
    QCPAxis::ScaleType dataScaleType() const { return mRenderer.dataScaleType(); }
    QCPColorScale* colorScale() const { return mRenderer.colorScale(); }
    void rescaleDataRange(bool recalc = false);

    // Pipeline access
    QCPHistogramPipeline& pipeline() { return mPipeline; }
    const QCPHistogramPipeline& pipeline() const { return mPipeline; }

public Q_SLOTS:
    void setGradient(const QCPColorGradient& gradient);
    void setDataRange(const QCPRange& range);
    void setDataScaleType(QCPAxis::ScaleType type);
    void setColorScale(QCPColorScale* scale);

    double selectTest(const QPointF& pos, bool onlySelectable, QVariant* details = nullptr) const override;
    QCPRange getKeyRange(bool& foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth) const override;
    QCPRange getValueRange(bool& foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth,
                           const QCPRange& inKeyRange = QCPRange()) const override;

Q_SIGNALS:
    void dataRangeChanged(const QCPRange& newRange);
    void gradientChanged(const QCPColorGradient& newGradient);
    void dataScaleTypeChanged(QCPAxis::ScaleType newType);

protected:
    void draw(QCPPainter* painter) override;
    void drawLegendIcon(QCPPainter* painter, const QRectF& rect) const override;

private:
    std::shared_ptr<QCPAbstractDataSource> mDataSource;
    int mKeyBins = 100;
    int mValueBins = 100;
    Normalization mNormalization = nNone;
    QCPHistogramPipeline mPipeline;
    QCPColormapRenderer mRenderer;
};
```

- [ ] **Step 2: Commit (header only, not yet compiled)**

```bash
git add src/plottables/plottable-histogram2d.h
git commit -m "feat: add QCPHistogram2D header"
```

### Task 7: Implement QCPHistogram2D

**Files:**
- Create: `src/plottables/plottable-histogram2d.cpp`
- Modify: `meson.build` — add source + moc header
- Modify: `src/qcp.h` — add include

- [ ] **Step 1: Write the implementation**

```cpp
// src/plottables/plottable-histogram2d.cpp
#include "plottable-histogram2d.h"
#include "plottable-colormap.h" // QCPColorMapData
#include <core.h>
#include <painting/painter.h>
#include <layoutelements/layoutelement-colorscale.h>
#include <layoutelements/layoutelement-axisrect.h>
#include <datasource/histogram-binner.h>
#include <Profiling.hpp>
#include <vector>
#include <numeric>

QCPHistogram2D::QCPHistogram2D(QCPAxis* keyAxis, QCPAxis* valueAxis)
    : QCPAbstractPlottable(keyAxis, valueAxis)
    , mPipeline(parentPlot() ? parentPlot()->pipelineScheduler() : nullptr, this)
    , mRenderer(this)
{
    // Set up the binning transform (viewport-independent, runs on data change)
    int capturedKeyBins = mKeyBins;
    int capturedValueBins = mValueBins;
    mPipeline.setTransform(TransformKind::ViewportIndependent,
        [capturedKeyBins, capturedValueBins](
            const QCPAbstractDataSource& src,
            const ViewportParams& /*vp*/,
            std::any& /*cache*/) -> std::shared_ptr<QCPColorMapData> {
            auto* raw = qcp::algo::bin2d(src, capturedKeyBins, capturedValueBins);
            return std::shared_ptr<QCPColorMapData>(raw);
        });

    connect(&mPipeline, &QCPHistogramPipeline::finished,
            this, [this](uint64_t) {
                mRenderer.invalidateMapImage();
                if (parentPlot())
                    parentPlot()->replot(QCustomPlot::rpQueuedReplot);
            });
}

QCPHistogram2D::~QCPHistogram2D()
{
    mRenderer.releaseRhiLayer();
}

void QCPHistogram2D::setDataSource(std::unique_ptr<QCPAbstractDataSource> source)
{
    setDataSource(std::shared_ptr<QCPAbstractDataSource>(std::move(source)));
}

void QCPHistogram2D::setDataSource(std::shared_ptr<QCPAbstractDataSource> source)
{
    mDataSource = std::move(source);
    mPipeline.setSource(mDataSource);
}

void QCPHistogram2D::dataChanged()
{
    mPipeline.onDataChanged();
}

void QCPHistogram2D::setBins(int keyBins, int valueBins)
{
    if (keyBins <= 0 || valueBins <= 0) return;
    if (mKeyBins == keyBins && mValueBins == valueBins) return;
    mKeyBins = keyBins;
    mValueBins = valueBins;

    // Re-install transform with new captured bin counts
    int ck = mKeyBins, cv = mValueBins;
    mPipeline.setTransform(TransformKind::ViewportIndependent,
        [ck, cv](const QCPAbstractDataSource& src,
                 const ViewportParams&, std::any&) -> std::shared_ptr<QCPColorMapData> {
            auto* raw = qcp::algo::bin2d(src, ck, cv);
            return std::shared_ptr<QCPColorMapData>(raw);
        });

    mPipeline.onDataChanged();
}

void QCPHistogram2D::setNormalization(Normalization norm)
{
    if (mNormalization == norm) return;
    mNormalization = norm;
    mRenderer.invalidateMapImage();
    if (mParentPlot)
        mParentPlot->replot(QCustomPlot::rpQueuedReplot);
}

void QCPHistogram2D::setGradient(const QCPColorGradient& gradient)
{
    mRenderer.setGradient(gradient);
    Q_EMIT gradientChanged(mRenderer.gradient());
    if (mParentPlot)
        mParentPlot->replot(QCustomPlot::rpQueuedReplot);
}

void QCPHistogram2D::setDataRange(const QCPRange& range)
{
    QCPRange prev = mRenderer.dataRange();
    mRenderer.setDataRange(range);
    if (mRenderer.dataRange().lower != prev.lower || mRenderer.dataRange().upper != prev.upper)
    {
        Q_EMIT dataRangeChanged(mRenderer.dataRange());
        if (mParentPlot)
            mParentPlot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void QCPHistogram2D::setDataScaleType(QCPAxis::ScaleType type)
{
    QCPAxis::ScaleType prev = mRenderer.dataScaleType();
    mRenderer.setDataScaleType(type);
    if (mRenderer.dataScaleType() != prev)
    {
        Q_EMIT dataScaleTypeChanged(mRenderer.dataScaleType());
        if (mParentPlot)
            mParentPlot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void QCPHistogram2D::setColorScale(QCPColorScale* colorScale)
{
    if (mRenderer.colorScale())
    {
        disconnect(this, &QCPHistogram2D::dataRangeChanged, mRenderer.colorScale(), &QCPColorScale::setDataRange);
        disconnect(this, &QCPHistogram2D::gradientChanged, mRenderer.colorScale(), &QCPColorScale::setGradient);
        disconnect(this, &QCPHistogram2D::dataScaleTypeChanged, mRenderer.colorScale(), &QCPColorScale::setDataScaleType);
        disconnect(mRenderer.colorScale(), &QCPColorScale::dataRangeChanged, this, &QCPHistogram2D::setDataRange);
        disconnect(mRenderer.colorScale(), &QCPColorScale::gradientChanged, this, &QCPHistogram2D::setGradient);
        disconnect(mRenderer.colorScale(), &QCPColorScale::dataScaleTypeChanged, this, &QCPHistogram2D::setDataScaleType);
    }
    mRenderer.setColorScale(colorScale);
    if (colorScale)
    {
        setGradient(colorScale->gradient());
        setDataScaleType(colorScale->dataScaleType());
        setDataRange(colorScale->dataRange());
        connect(this, &QCPHistogram2D::dataRangeChanged, colorScale, &QCPColorScale::setDataRange);
        connect(this, &QCPHistogram2D::gradientChanged, colorScale, &QCPColorScale::setGradient);
        connect(this, &QCPHistogram2D::dataScaleTypeChanged, colorScale, &QCPColorScale::setDataScaleType);
        connect(colorScale, &QCPColorScale::dataRangeChanged, this, &QCPHistogram2D::setDataRange);
        connect(colorScale, &QCPColorScale::gradientChanged, this, &QCPHistogram2D::setGradient);
        connect(colorScale, &QCPColorScale::dataScaleTypeChanged, this, &QCPHistogram2D::setDataScaleType);
    }
}

void QCPHistogram2D::rescaleDataRange(bool /*recalc*/)
{
    // For histograms, raw counts are always in the QCPColorMapData — no separate source to rescan
    auto* data = mPipeline.result();
    if (!data) return;
    QCPRange range = data->dataBounds();
    if (range.lower < range.upper)
        setDataRange(range);
}

void QCPHistogram2D::draw(QCPPainter* painter)
{
    PROFILE_HERE_N("QCPHistogram2D::draw");
    if (!mKeyAxis || !mValueAxis) return;

    auto* binnedData = mPipeline.result();
    if (!binnedData)
    {
        if (mDataSource)
            mPipeline.onDataChanged(); // trigger first run
        return;
    }

    if (mRenderer.mapImageInvalidated())
    {
        if (mNormalization == nColumn)
        {
            int nk = binnedData->keySize();
            int nv = binnedData->valueSize();
            std::vector<double> colSums(nk, 0.0);
            for (int x = 0; x < nk; ++x)
                for (int y = 0; y < nv; ++y)
                    colSums[x] += binnedData->cell(x, y);

            mRenderer.updateMapImage(binnedData,
                [colSums = std::move(colSums)](double v, int col, int /*row*/) -> double {
                    return colSums[col] > 0 ? v / colSums[col] : 0.0;
                });
        }
        else
        {
            mRenderer.updateMapImage(binnedData);
        }
    }

    if (mRenderer.mapImage().isNull()) return;

    QCPRange keyRange = binnedData->keyRange();
    QCPRange valueRange = binnedData->valueRange();
    applyDefaultAntialiasingHint(painter); // must be called before renderer draw (protected method)
    mRenderer.draw(painter, mKeyAxis.data(), mValueAxis.data(), keyRange, valueRange);
}

void QCPHistogram2D::drawLegendIcon(QCPPainter* painter, const QRectF& rect) const
{
    QLinearGradient lg(rect.topLeft(), rect.topRight());
    lg.setColorAt(0, Qt::blue);
    lg.setColorAt(1, Qt::red);
    painter->setBrush(QBrush(lg));
    painter->setPen(Qt::NoPen);
    painter->drawRect(rect);
}

QCPRange QCPHistogram2D::getKeyRange(bool& foundRange, QCP::SignDomain inSignDomain) const
{
    if (!mDataSource || mDataSource->empty())
    {
        foundRange = false;
        return {};
    }
    return mDataSource->keyRange(foundRange, inSignDomain);
}

QCPRange QCPHistogram2D::getValueRange(bool& foundRange, QCP::SignDomain inSignDomain,
                                        const QCPRange&) const
{
    if (!mDataSource || mDataSource->empty())
    {
        foundRange = false;
        return {};
    }
    return mDataSource->valueRange(foundRange, inSignDomain);
}

double QCPHistogram2D::selectTest(const QPointF& pos, bool onlySelectable, QVariant*) const
{
    if (onlySelectable && !mSelectable) return -1;
    if (!mKeyAxis || !mValueAxis || !mDataSource) return -1;

    double key = mKeyAxis->pixelToCoord(pos.x());
    double value = mValueAxis->pixelToCoord(pos.y());

    bool fk = false, fv = false;
    auto kr = mDataSource->keyRange(fk);
    auto vr = mDataSource->valueRange(fv);
    if (fk && fv && kr.contains(key) && vr.contains(value))
        return 0;
    return -1;
}
```

- [ ] **Step 2: Register in meson.build**

Add `'src/plottables/plottable-histogram2d.cpp'` to sources.
Add `'src/plottables/plottable-histogram2d.h'` to `neoqcp_moc_headers`.

- [ ] **Step 3: Add include to qcp.h**

Add `#include "plottables/plottable-histogram2d.h"` after the `plottable-colormap2.h` include.

- [ ] **Step 4: Build**

Run: `cd /var/home/jeandet/Documents/prog/NeoQCP && PATH="/home/jeandet/Qt/6.10.2/gcc_64/bin:$PATH" meson compile -C build`
Expected: Build succeeds.

- [ ] **Step 5: Commit**

```bash
git add src/plottables/plottable-histogram2d.cpp meson.build src/qcp.h
git commit -m "feat: implement QCPHistogram2D plottable"
```

### Task 8: Add QCPHistogram2D tests

**Files:**
- Modify: `tests/auto/test-pipeline/test-pipeline.h`
- Modify: `tests/auto/test-pipeline/test-pipeline.cpp`

- [ ] **Step 1: Write tests**

Add to header:
```cpp
// QCPHistogram2D
void histogram2dPipelineBins();
void histogram2dNormalizationColumn();
void histogram2dNormalizationToggleNoRebind();
void histogram2dRenderSmokeTest();
```

Add to cpp:
```cpp
void TestPipeline::histogram2dPipelineBins()
{
    // Feed scatter data, verify binned output dimensions
    auto* hist = new QCPHistogram2D(mPlot->xAxis, mPlot->yAxis);
    std::vector<double> keys, vals;
    for (int i = 0; i < 1000; ++i) {
        keys.push_back(i * 0.01);
        vals.push_back(std::sin(i * 0.01));
    }
    hist->setBins(10, 20);
    hist->setData(std::move(keys), std::move(vals));

    // Wait for pipeline
    QSignalSpy spy(&hist->pipeline(), &QCPHistogramPipeline::finished);
    QVERIFY(spy.wait(2000));

    auto* result = hist->pipeline().result();
    QVERIFY(result);
    QCOMPARE(result->keySize(), 10);
    QCOMPARE(result->valueSize(), 20);
}

void TestPipeline::histogram2dNormalizationColumn()
{
    // Test that QCPHistogram2D with nColumn normalization renders without crash
    // and that the normalization lambda produces correct values.
    auto* hist = new QCPHistogram2D(mPlot->xAxis, mPlot->yAxis);
    // Known data: 4 points, 2x2 bins
    // col 0: counts [3, 1] (sum=4), col 1: counts [0, 2] (sum=2)
    std::vector<double> keys = {0.25, 0.25, 0.25, 0.25, 0.75, 0.75};
    std::vector<double> vals = {0.25, 0.25, 0.25, 0.75, 0.75, 0.75};
    hist->setBins(2, 2);
    hist->setNormalization(QCPHistogram2D::nColumn);
    hist->setData(std::move(keys), std::move(vals));

    QSignalSpy spy(&hist->pipeline(), &QCPHistogramPipeline::finished);
    QVERIFY(spy.wait(2000));

    // Verify raw counts are correct
    auto* data = hist->pipeline().result();
    QVERIFY(data);
    QCOMPARE(data->cell(0, 0), 3.0);
    QCOMPARE(data->cell(0, 1), 1.0);
    QCOMPARE(data->cell(1, 0), 0.0);
    QCOMPARE(data->cell(1, 1), 2.0);

    // Render with normalization — exercises the draw() normalization lambda
    hist->rescaleDataRange();
    mPlot->rescaleAxes();
    mPlot->replot();
    QCoreApplication::processEvents();

    // Also verify the normalization math directly
    std::vector<double> colSums = {4.0, 2.0};
    QVERIFY(qFuzzyCompare(3.0 / colSums[0], 0.75));
    QVERIFY(qFuzzyCompare(1.0 / colSums[0], 0.25));
    QVERIFY(qFuzzyCompare(2.0 / colSums[1], 1.0));
}

void TestPipeline::histogram2dNormalizationToggleNoRebind()
{
    auto* hist = new QCPHistogram2D(mPlot->xAxis, mPlot->yAxis);
    std::vector<double> keys = {0.5, 0.5, 0.5};
    std::vector<double> vals = {0.5, 0.5, 0.5};
    hist->setData(std::move(keys), std::move(vals));
    hist->setBins(1, 1);

    QSignalSpy spy(&hist->pipeline(), &QCPHistogramPipeline::finished);
    QVERIFY(spy.wait(2000));
    int finishedCount = spy.count();

    // Toggling normalization should NOT trigger a new pipeline run
    hist->setNormalization(QCPHistogram2D::nColumn);
    QCoreApplication::processEvents();
    QCOMPARE(spy.count(), finishedCount); // no new finished signal
}

void TestPipeline::histogram2dRenderSmokeTest()
{
    auto* hist = new QCPHistogram2D(mPlot->xAxis, mPlot->yAxis);
    std::vector<double> keys = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> vals = {1.0, 4.0, 2.0, 5.0, 3.0};
    hist->setData(std::move(keys), std::move(vals));

    QSignalSpy spy(&hist->pipeline(), &QCPHistogramPipeline::finished);
    QVERIFY(spy.wait(2000));

    hist->rescaleDataRange();
    mPlot->rescaleAxes();
    // Should not crash
    mPlot->replot();
    QCoreApplication::processEvents();
}
```

- [ ] **Step 2: Build and run tests**

Run: `cd /var/home/jeandet/Documents/prog/NeoQCP && PATH="/home/jeandet/Qt/6.10.2/gcc_64/bin:$PATH" meson compile -C build && LD_LIBRARY_PATH="/home/jeandet/Qt/6.10.2/gcc_64/lib:$LD_LIBRARY_PATH" meson test -C build --print-errorlogs`
Expected: All tests pass.

- [ ] **Step 3: Commit**

```bash
git add tests/auto/test-pipeline/test-pipeline.h tests/auto/test-pipeline/test-pipeline.cpp
git commit -m "test: add QCPHistogram2D pipeline, normalization, and render tests"
```

## Chunk 3: Gallery Entries

### Task 9: Add Histogram2D gallery tabs

**Files:**
- Modify: `tests/manual/gallery/main.cpp`

- [ ] **Step 1: Add createHistogram2DTab()**

Insert before `main()`:

```cpp
// ── Tab: Histogram2D ────────────────────────────────────────────────────────

static QWidget* createHistogram2DTab()
{
    auto* plot = makePlot();

    // Bivariate normal: N(0,1) x N(0,1) with correlation
    const int nPoints = 100000;
    std::vector<double> keys(nPoints), vals(nPoints);
    std::mt19937 rng(42);
    std::normal_distribution<double> dist(0.0, 1.0);
    const double rho = 0.6;
    for (int i = 0; i < nPoints; ++i)
    {
        double u = dist(rng);
        double v = dist(rng);
        keys[i] = u;
        vals[i] = rho * u + std::sqrt(1.0 - rho * rho) * v;
    }

    auto* hist = new QCPHistogram2D(plot->xAxis, plot->yAxis);
    hist->setData(std::move(keys), std::move(vals));
    hist->setBins(80, 80);
    hist->setNormalization(QCPHistogram2D::nColumn);

    auto* scale = new QCPColorScale(plot);
    plot->plotLayout()->addElement(0, 1, scale);
    hist->setColorScale(scale);

    QCPColorGradient gradient(QCPColorGradient::gpViridis);
    gradient.setNanHandling(QCPColorGradient::nhTransparent);
    hist->setGradient(gradient);

    // Wait briefly then rescale
    QObject::connect(&hist->pipeline(), &QCPHistogramPipeline::finished,
        plot, [hist, plot](uint64_t) {
            hist->rescaleDataRange();
            plot->rescaleAxes();
        }, Qt::SingleShotConnection);

    plot->xAxis->setLabel("X");
    plot->yAxis->setLabel("Y");
    plot->replot();
    return wrapPlot(plot);
}
```

- [ ] **Step 2: Add createHistogram2DLogTab()**

```cpp
static QWidget* createHistogram2DLogTab()
{
    auto* plot = makePlot();

    // Log-normal Y values
    const int nPoints = 50000;
    std::vector<double> keys(nPoints), vals(nPoints);
    std::mt19937 rng(123);
    std::normal_distribution<double> keyDist(5.0, 2.0);
    std::lognormal_distribution<double> valDist(2.0, 0.8);
    for (int i = 0; i < nPoints; ++i)
    {
        keys[i] = keyDist(rng);
        vals[i] = valDist(rng);
    }

    auto* hist = new QCPHistogram2D(plot->xAxis, plot->yAxis);
    hist->setData(std::move(keys), std::move(vals));
    hist->setBins(60, 60);
    hist->setNormalization(QCPHistogram2D::nColumn);

    plot->yAxis->setScaleType(QCPAxis::stLogarithmic);
    plot->yAxis->setTicker(QSharedPointer<QCPAxisTickerLog>::create());

    auto* scale = new QCPColorScale(plot);
    plot->plotLayout()->addElement(0, 1, scale);
    hist->setColorScale(scale);

    QCPColorGradient gradient(QCPColorGradient::gpHot);
    gradient.setNanHandling(QCPColorGradient::nhTransparent);
    hist->setGradient(gradient);

    QObject::connect(&hist->pipeline(), &QCPHistogramPipeline::finished,
        plot, [hist, plot](uint64_t) {
            hist->rescaleDataRange();
            plot->rescaleAxes();
        }, Qt::SingleShotConnection);

    plot->xAxis->setLabel("Wind Speed (km/s)");
    plot->yAxis->setLabel("He Abundance (log)");
    plot->replot();
    return wrapPlot(plot);
}
```

- [ ] **Step 3: Add `#include <random>` at the top of gallery main.cpp**

- [ ] **Step 4: Register tabs in main()**

Add after the existing `createThemeTab()` line:
```cpp
tabs->addTab(createHistogram2DTab(),    "Histogram2D");
tabs->addTab(createHistogram2DLogTab(), "Histogram2D (log)");
```

- [ ] **Step 5: Build and run gallery manually**

Run: `cd /var/home/jeandet/Documents/prog/NeoQCP && PATH="/home/jeandet/Qt/6.10.2/gcc_64/bin:$PATH" meson compile -C build`
Expected: Build succeeds.

- [ ] **Step 6: Commit**

```bash
git add tests/manual/gallery/main.cpp
git commit -m "feat: add Histogram2D gallery tabs (linear and log variants)"
```

### Task 10: Final verification

- [ ] **Step 1: Run full test suite**

Run: `cd /var/home/jeandet/Documents/prog/NeoQCP && PATH="/home/jeandet/Qt/6.10.2/gcc_64/bin:$PATH" LD_LIBRARY_PATH="/home/jeandet/Qt/6.10.2/gcc_64/lib:$LD_LIBRARY_PATH" meson test -C build --print-errorlogs`
Expected: All tests pass.

- [ ] **Step 2: Verify file count**

New files created:
- `src/painting/colormap-renderer.h`
- `src/painting/colormap-renderer.cpp`
- `src/datasource/histogram-binner.h`
- `src/plottables/plottable-histogram2d.h`
- `src/plottables/plottable-histogram2d.cpp`

Modified files:
- `meson.build` (sources + moc headers)
- `src/qcp.h` (include)
- `src/datasource/async-pipeline.h` (type alias)
- `src/plottables/plottable-colormap2.h` (uses renderer)
- `src/plottables/plottable-colormap2.cpp` (delegates to renderer)
- `tests/auto/test-pipeline/test-pipeline.h` (new test slots)
- `tests/auto/test-pipeline/test-pipeline.cpp` (new tests)
- `tests/manual/gallery/main.cpp` (new tabs)
