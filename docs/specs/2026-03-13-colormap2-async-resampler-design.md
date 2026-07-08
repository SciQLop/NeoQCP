# QCPColorMap2 and Shared Async Resampler

**Date:** 2026-03-13
**Status:** Superseded — the shipped `QCPResamplerScheduler`/receiver-side-discard
design described below was replaced by `QCPPipelineScheduler` +
`QCPAsyncPipeline<In,Out>` with a mutex-guarded `DestroyGuard` (see
[async-pipeline.md](../architecture/async-pipeline.md) for the actual
architecture). Kept for historical context only; do not use this as a
reference for the current threading model.
**Scope:** QCPColorMap2 plottable, 2D data source, shared resampler scheduler
**Depends on:** Zero-Copy Data Source (2026-03-12)

## Problem

NeoQCP's `QCPColorMap` requires callers to pre-grid data into a fixed `QCPColorMapData` cell array. Scientific instruments produce raw `(x[], y[], z[N×M])` arrays where:

1. **Data is much larger than screen resolution** — millions of time×frequency cells need downsampling
2. **Y axes can be non-uniform** — energy bins shift per timestamp (Y depends on X)
3. **Resampling is expensive** — must happen off the GUI thread
4. **No zero-copy path** — data must be copied cell-by-cell into `QCPColorMapData`

SciQLopPlots solves this with a threaded `ColormapResampler` that bins raw data to screen resolution. The resampling algorithm and threading pattern are worth porting. The threading pattern (worker thread + request coalescing) is general-purpose and should be shared with future QCPGraph2 async rendering.

## Goals

- Accept raw `(x, y, z)` arrays via the QCPGraph2 data source pattern (any container type, zero-copy or owned)
- Auto-detect Y dimensionality: 1D (uniform y-axis) vs 2D (y varies with x)
- Resample to screen resolution in a worker thread, coalescing rapid requests
- Configurable data gap detection threshold
- Reusable async scheduler that QCPGraph2 can adopt later
- Coexist with legacy `QCPColorMap`

## Non-Goals

- GPU colorization shader (resampler reduces to screen resolution; CPU colorization is fast on the resampled grid)
- Retrofitting QCPGraph2 with async resampling (designed for it, but not in this scope)
- Incremental `addData()` appends (users create a new data source)
- Interpolation modes beyond what `QCPColorGradient` already provides
- `interpolate` / `tightBoundary` properties from `QCPColorMap` (resampler output is already at screen resolution — no interpolation needed; tight boundary is implicit)
- Log-scale z colorization (`dataScaleType`) — can be added later without architectural changes
- Python/numpy integration (pure C++ library)

## Design

### Architecture Overview

```
QCPColorMap2 (QObject, non-templated)
    │
    ├──→ QCPAbstractDataSource2D (virtual interface)
    │        │
    │        └──→ QCPSoADataSource2D<XC,YC,ZC> (template, holds/views typed data)
    │                 │
    │                 └──→ qcp::algo2d::* (free function templates)
    │
    └──→ QCPColormapResampler (owns QCPResamplerScheduler)
             │
             └──→ QCPResamplerScheduler (reusable worker thread + coalescing)
```

### Component 1: QCPResamplerScheduler

Reusable worker-thread scheduler with request coalescing. Shared infrastructure for any future resampler (colormap, graph, etc.).

**File:** `src/datasource/resampler-scheduler.h/.cpp`

```cpp
class QCP_LIB_DECL QCPResamplerScheduler : public QObject
{
    Q_OBJECT

public:
    explicit QCPResamplerScheduler(QObject* parent = nullptr);
    ~QCPResamplerScheduler();

    void start();
    void stop();
    void submit(std::function<void()> work);

private:
    QThread mThread;
    QMutex mMutex;
    std::function<void()> mPending;
    bool mBusy = false;

Q_SIGNALS:
    void workReady();

private Q_SLOTS:
    void runNext();
};
```

**Behavior:**
- `start()` starts the worker thread at `QThread::LowPriority`
- `submit(work)` stores the callable under mutex. If not busy, triggers immediately. If busy, replaces any existing pending work (coalescing — only the latest request survives)
- `runNext()` runs in the worker thread: sets `mBusy=true`, executes work (wrapped in try/catch — work functions that throw are silently discarded), then checks for pending work. If pending exists, runs it. Otherwise sets `mBusy=false`
- `stop()` calls `quit()` + `wait()` on the thread
- Destructor calls `stop()`, which blocks until any in-flight work finishes

**Why this is separate from the resampler:** QObject can't be templated, and input/output types differ between graph and colormap resamplers. Composition avoids CRTP or type-erasure complexity. Each concrete resampler owns a scheduler and submits typed lambdas that emit typed signals.

### Component 2: QCPAbstractDataSource2D

Non-templated virtual interface for 2D data sources. Mirrors `QCPAbstractDataSource` pattern.

**File:** `src/datasource/abstract-datasource-2d.h`

```cpp
class QCP_LIB_DECL QCPAbstractDataSource2D
{
public:
    virtual ~QCPAbstractDataSource2D() = default;

    virtual int xSize() const = 0;
    virtual int ySize() const = 0;       // number of y bins per x row (M)
    virtual bool yIs2D() const = 0;

    virtual double xAt(int i) const = 0;
    virtual double yAt(int i, int j) const = 0;
    virtual double zAt(int i, int j) const = 0;

    virtual QCPRange xRange(bool& found, QCP::SignDomain sd = QCP::sdBoth) const = 0;
    virtual QCPRange yRange(bool& found, QCP::SignDomain sd = QCP::sdBoth) const = 0;
    virtual QCPRange zRange(bool& found, int xBegin = 0, int xEnd = -1) const = 0;

    virtual int findXBegin(double sortKey) const = 0;
    virtual int findXEnd(double sortKey) const = 0;
};
```

- `yAt(i, j)` returns `y[j]` when Y is 1D, `y[i * ySize + j]` when Y is 2D. The caller doesn't need to know which case applies.
- `ySize()` is named to match `QCPColorMapData::valueSize()` rather than `yCols` (which is ambiguous in a matrix context).
- `xRange`/`yRange` include `SignDomain` for log-scale axis support, consistent with `QCPAbstractDataSource`.
- `zRange` accepts an optional visible x window (`xBegin`/`xEnd`) so `rescaleDataRange` can scan only visible data. `xEnd = -1` means scan all.

### Component 3: QCPSoADataSource2D

Templated SoA container. Auto-detects Y dimensionality from array sizes.

**File:** `src/datasource/soa-datasource-2d.h`

```cpp
template <IndexableNumericRange XC, IndexableNumericRange YC, IndexableNumericRange ZC>
class QCPSoADataSource2D final : public QCPAbstractDataSource2D
{
public:
    using X = std::ranges::range_value_t<XC>;
    using Y = std::ranges::range_value_t<YC>;
    using Z = std::ranges::range_value_t<ZC>;

    QCPSoADataSource2D(XC x, YC y, ZC z)
        : mX(std::move(x)), mY(std::move(y)), mZ(std::move(z))
    {
        auto nx = std::ranges::size(mX);
        auto ny = std::ranges::size(mY);
        auto nz = std::ranges::size(mZ);
        Q_ASSERT(nx > 0 && nz > 0 && nz % nx == 0);
        mYSize = static_cast<int>(nz / nx);
        mYIs2D = (ny == nz);
        Q_ASSERT(ny == nz || ny == static_cast<decltype(ny)>(mYSize));
    }

    // Typed access for algorithms
    const XC& x() const { return mX; }
    const YC& y() const { return mY; }
    const ZC& z() const { return mZ; }

    // Virtual overrides
    int xSize() const override { return static_cast<int>(std::ranges::size(mX)); }
    int ySize() const override { return mYSize; }
    bool yIs2D() const override { return mYIs2D; }

    double xAt(int i) const override {
        return static_cast<double>(mX[i]);
    }
    double yAt(int i, int j) const override {
        return mYIs2D
            ? static_cast<double>(mY[i * mYSize + j])
            : static_cast<double>(mY[j]);
    }
    double zAt(int i, int j) const override {
        return static_cast<double>(mZ[i * mYSize + j]);
    }

    // Range queries delegate to qcp::algo2d (see Component 4)
    // ...

private:
    XC mX;
    YC mY;
    ZC mZ;
    int mYSize;
    bool mYIs2D;
};
```

**Y dimensionality auto-detection:**
- `size(z) / size(x)` → `ySize` (M, number of y values per row)
- `size(y) == size(z)` → Y is 2D (one y value per z value, varies with x)
- `size(y) == ySize` → Y is 1D (shared y-axis for all x rows)

**Ownership by container type** (same as QCPGraph2):
- `std::vector<T>` → owns data (moved in)
- `std::span<const T>` → zero-copy view (caller manages lifetime)

### Component 4: Algorithms

Free function templates for 2D data operations.

**File:** `src/datasource/algorithms-2d.h`

```cpp
namespace qcp::algo2d {

// Binary search on sorted x (delegates to qcp::algo::findBegin/findEnd)
template <IndexableNumericRange XC>
int findXBegin(const XC& x, double sortKey);

template <IndexableNumericRange XC>
int findXEnd(const XC& x, double sortKey);

// Range queries with NaN handling and SignDomain filtering
template <IndexableNumericRange XC>
QCPRange xRange(const XC& x, bool& found, QCP::SignDomain sd = QCP::sdBoth);

template <IndexableNumericRange YC>
QCPRange yRange(const YC& y, bool& found, QCP::SignDomain sd = QCP::sdBoth);

// Z range within visible x window (xEnd = -1 means scan all)
template <IndexableNumericRange ZC>
QCPRange zRange(const ZC& z, int ySize, bool& found, int xBegin = 0, int xEnd = -1);

// Core resampling algorithm (ported from SciQLopPlots)
// Returns a new QCPColorMapData* at screen resolution. Caller owns it.
QCPColorMapData* resample(
    const QCPAbstractDataSource2D& src,
    int xBegin, int xEnd,         // visible x range (from findXBegin/End)
    int targetWidth, int targetHeight,  // pixel dimensions
    bool yLogScale,
    double gapThreshold);         // dx increase factor for gap detection

} // namespace qcp::algo2d
```

**Resampling algorithm** (ported from SciQLopPlots `_x_loop` / `_y_loop` / `_copy_and_average` / `_divide`):

1. Compute y bounds from visible data (handles NaN)
2. Generate target x-axis bins: `n_x = min(targetWidth, xEnd - xBegin)` linearly spaced
3. Generate target y-axis bins: `n_y = min(targetHeight, ySize)`, linearly or log-spaced
4. Allocate `QCPColorMapData(n_x, n_y, xRange, yRange)`
5. Allocate per-bin accumulation counters as `std::vector<uint32_t>` (not `uint16_t` — avoids overflow with millions of source cells mapping to few bins)
6. Walk source data:
   - For each source x index, find destination x bin
   - **Gap detection:** if both `dx > gapThreshold * prev_dx` AND `next_dx > gapThreshold * dx`, this is a data gap — skip (fill with NaN). The check is bidirectional: a single large step surrounded by normal steps is not a gap, it's a data edge. Only when both the backward and forward steps are large relative to their neighbors is it a true gap.
   - For each source y value at this x, find destination y bin
   - Accumulate z values and increment per-bin counters
7. Divide accumulated z by counts. Bins with zero counts → NaN
8. Call `recalculateDataBounds()` on result

The `resample()` function operates on `QCPAbstractDataSource2D` via virtual calls (`xAt`, `yAt`, `zAt`). This is acceptable because it runs once per resample request (not per pixel), and can be replaced with a template fast-path later if profiling shows it matters.

### Component 5: QCPColormapResampler

Concrete resampler. Owns a `QCPResamplerScheduler` and translates between QCPColorMap2's needs and the resampling algorithm.

**File:** `src/plottables/colormap-resampler.h/.cpp`

```cpp
class QCP_LIB_DECL QCPColormapResampler : public QObject
{
    Q_OBJECT

public:
    explicit QCPColormapResampler(QObject* parent = nullptr);
    ~QCPColormapResampler();

    void request(
        std::shared_ptr<QCPAbstractDataSource2D> source,
        QCPRange xRange,
        QSize plotSize,
        bool yLogScale,
        double gapThreshold);

Q_SIGNALS:
    void finished(uint64_t generation, QCPColorMapData* result);

private:
    QCPResamplerScheduler mScheduler;
    std::atomic<uint64_t> mGeneration{0};
};
```

**`request()` implementation:**
- Increments `mGeneration` and captures the new value
- Captures all parameters by value (shared_ptr keeps data source alive)
- Calls `mScheduler.submit(lambda)` where the lambda:
  1. Calls `findXBegin`/`findXEnd` on the data source
  2. Calls `qcp::algo2d::resample()`
  3. Emits `finished(generation, result)` (queued to main thread)
- The generation counter prevents stale results: if `setDataSource()` is called while a resample is in-flight, the old result arrives with an outdated generation and is discarded by the receiver

### Component 6: QCPColorMap2

The plottable class. Follows QCPGraph2's API conventions.

**File:** `src/plottables/plottable-colormap2.h/.cpp`

```cpp
class QCP_LIB_DECL QCPColorMap2 : public QCPAbstractPlottable
{
    Q_OBJECT

public:
    explicit QCPColorMap2(QCPAxis* keyAxis, QCPAxis* valueAxis);
    ~QCPColorMap2();

    // --- Data source management (QCPGraph2 pattern) ---
    void setDataSource(std::unique_ptr<QCPAbstractDataSource2D> source);
    void setDataSource(std::shared_ptr<QCPAbstractDataSource2D> source);
    QCPAbstractDataSource2D* dataSource() const;

    // Notify that viewed data has been mutated externally.
    // Triggers a resample request (not just a replot, unlike QCPGraph2,
    // because the resampled grid must be regenerated from the new data).
    void dataChanged();

    // --- Owning setData (any container types) ---
    template <IndexableNumericRange XC, IndexableNumericRange YC, IndexableNumericRange ZC>
    void setData(XC&& x, YC&& y, ZC&& z)
    {
        setDataSource(std::make_shared<QCPSoADataSource2D<
            std::decay_t<XC>, std::decay_t<YC>, std::decay_t<ZC>>>(
            std::forward<XC>(x), std::forward<YC>(y), std::forward<ZC>(z)));
    }

    // --- Non-owning views ---
    template <typename X, typename Y, typename Z>
    void viewData(const X* x, int nx, const Y* y, int ny, const Z* z, int nz)
    {
        setDataSource(std::make_shared<QCPSoADataSource2D<
            std::span<const X>, std::span<const Y>, std::span<const Z>>>(
            std::span<const X>{x, static_cast<size_t>(nx)},
            std::span<const Y>{y, static_cast<size_t>(ny)},
            std::span<const Z>{z, static_cast<size_t>(nz)}));
    }

    template <typename X, typename Y, typename Z>
    void viewData(std::span<const X> x, std::span<const Y> y, std::span<const Z> z)
    {
        setDataSource(std::make_shared<QCPSoADataSource2D<
            std::span<const X>, std::span<const Y>, std::span<const Z>>>(x, y, z));
    }

    // --- Gap detection ---
    void setGapThreshold(double threshold);
    double gapThreshold() const;

    // --- Color gradient ---
    QCPColorGradient gradient() const;
    void setGradient(const QCPColorGradient& gradient);

    // --- Color scale ---
    QCPColorScale* colorScale() const;
    void setColorScale(QCPColorScale* colorScale);

    // --- Data range (z-axis range for colorization) ---
    QCPRange dataRange() const;
    void setDataRange(const QCPRange& range);
    void rescaleDataRange(bool recalc = false);

    // --- Selection ---
    double selectTest(const QPointF& pos, bool onlySelectable, QVariant* details) const override;

protected:
    // QCPAbstractPlottable overrides
    void draw(QCPPainter* painter) override;
    void drawLegendIcon(QCPPainter* painter, const QRectF& rect) const override;
    QCPRange getKeyRange(bool& foundRange, QCP::SignDomain inSignDomain) const override;
    QCPRange getValueRange(bool& foundRange, QCP::SignDomain inSignDomain,
                           const QCPRange& inKeyRange) const override;

private:
    std::shared_ptr<QCPAbstractDataSource2D> mDataSource;
    QCPColormapResampler* mResampler;
    QCPColorGradient mGradient;
    QCPColorScale* mColorScale = nullptr;
    QCPRange mDataRange;
    double mGapThreshold = 1.5;
    uint64_t mCurrentGeneration = 0;

    QCPColorMapData* mResampledData = nullptr;
    QImage mMapImage;
    bool mMapImageInvalidated = true;

    void requestResample();
    void onResampleFinished(uint64_t generation, QCPColorMapData* data);
    void updateMapImage();
};
```

**Resampling triggers** (each calls `requestResample()`):
- `setDataSource()` / `setData()` / `viewData()` / `dataChanged()` — new data
- Key axis `rangeChanged` signal — pan/zoom changes visible x window
- Value axis `rangeChanged` signal — y pan/zoom changes y binning
- Widget resize — target pixel dimensions change
- Value axis `scaleTypeChanged` — log/linear y toggle

**`requestResample()`:**
- Gathers current state: data source (shared_ptr), key axis range, axis rect pixel size, value axis log flag, gap threshold
- Calls `mResampler->request(...)` which increments the generation counter

**`onResampleFinished(uint64_t generation, QCPColorMapData* data)`:**
- Connected to `mResampler->finished()` via `Qt::QueuedConnection`
- If `generation < mCurrentGeneration`, discards the stale result (`delete data; return`)
- Otherwise: stores `generation` as `mCurrentGeneration`, replaces `mResampledData` (deletes old), sets `mMapImageInvalidated = true`, calls `mParentPlot->replot()`

**Destruction ordering:** `QCPColorMap2` destructor calls `mResampler->stop()` (which blocks until the worker thread finishes) before deleting anything. This guarantees no in-flight `finished()` signal can arrive after destruction.

**`draw()`:**
- If `mResampledData` is null, return (first data not yet ready)
- If `mMapImageInvalidated`, call `updateMapImage()` (colorize via `mGradient.colorize()` into `mMapImage`)
- Compute pixel rect from axis ranges
- `painter->drawImage(imageRect, mMapImage)`

**`updateMapImage()`:**
- Allocates/resizes `mMapImage` (Format_ARGB32_Premultiplied)
- Iterates scanlines, calls `mGradient.colorize(data_row, mDataRange, pixelRow, ...)`
- Reuses existing `QCPColorGradient` colorization logic from `QCPColorMap`

### Y 1D vs Y 2D — Usage Examples

**Case 1 — Y is 1D (uniform grid):** Fixed-frequency FFT spectrogram

```cpp
// x = timestamps (N), y = frequency bins (M), z = power (N*M)
std::vector<double> times(N), freqs(M), power(N * M);
// ... populate ...
colorMap->setData(std::move(times), std::move(freqs), std::move(power));
// Auto-detected: size(y)==M, size(z)==N*M, yCols=M, yIs2D=false
```

**Case 2 — Y is 2D (variable grid):** Particle energy spectra where bins shift with spacecraft potential

```cpp
// x = timestamps (N), y = energy bins (N*M, vary per timestamp), z = flux (N*M)
std::vector<double> times(N), energies(N * M), flux(N * M);
// ... populate ...
colorMap->setData(std::move(times), std::move(energies), std::move(flux));
// Auto-detected: size(y)==N*M==size(z), yCols=N*M/N=M, yIs2D=true
```

**Case 3 — Zero-copy view from external data:**

```cpp
// Data owned by caller (e.g., memory-mapped file, shared buffer)
colorMap->viewData(x_ptr, nx, y_ptr, ny, z_ptr, nz);
// Caller must ensure lifetime and call dataChanged() after mutations
```

### File Layout

```
src/
  datasource/
    abstract-datasource.h          # existing (QCPGraph2)
    abstract-datasource-2d.h       # NEW — QCPAbstractDataSource2D
    soa-datasource.h               # existing (QCPGraph2)
    soa-datasource-2d.h            # NEW — QCPSoADataSource2D<XC,YC,ZC>
    algorithms.h                   # existing (QCPGraph2)
    algorithms-2d.h                # NEW — qcp::algo2d
    resampler-scheduler.h/.cpp     # NEW — QCPResamplerScheduler (shared)
  plottables/
    colormap-resampler.h/.cpp      # NEW — QCPColormapResampler
    plottable-colormap2.h/.cpp     # NEW — QCPColorMap2
```

Umbrella header `src/qcp.h` includes the new headers. `meson.build` adds the new `.cpp` files.

### What Stays Untouched

- `QCPColorMap`, `QCPColorMapData`, `QCPColorGradient` — fully preserved
- `QCPGraph2` and its data source infrastructure — unchanged (scheduler designed for future adoption)
- All existing plottables, layout system, selection system
- RHI compositing pipeline, paint buffers, layers

### Thread Safety

- The resampler captures a `shared_ptr<QCPAbstractDataSource2D>` when a request is submitted, ensuring the data source stays alive for the duration of the resample
- For owned data sources (`std::vector`-based), data is immutable once set — no synchronization needed
- For non-owning views (`std::span`-based), concurrent mutation during resampling is undefined behavior (same as QCPGraph2)
- `QCPResamplerScheduler` synchronizes access to the pending work queue via `QMutex`
- Resampler output is delivered to the main thread via `Qt::QueuedConnection`

### Testing

- **QCPResamplerScheduler unit tests:** submit while idle, submit while busy (coalescing), stop while busy, rapid sequential submits
- **QCPSoADataSource2D unit tests:** auto-detection (1D Y vs 2D Y), element access, range queries, mixed types, span views
- **Algorithm unit tests:** `resample()` with known data — uniform grid, variable-y grid, gap detection, log-scale y, NaN handling, empty bins
- **QCPColormapResampler integration tests:** async round-trip (submit → signal → receive result), coalescing behavior
- **QCPColorMap2 integration tests:** full render cycle with various data types, axis rescaling, gradient changes, colorscale sync
- **Benchmarks:** resample time for large datasets (1M+ cells), compare with direct QCPColorMap cell-by-cell population
