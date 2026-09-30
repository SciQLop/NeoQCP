#pragma once
#include "abstract-datasource.h"
#include "abstract-multi-datasource.h"
#include "soa-datasource.h"
#include "async-pipeline.h"
#include "thread-naming.h"
#include "../Profiling.hpp"
#include <QAtomicInt>
#include <QSemaphore>
#include <QThread>
#include <QThreadPool>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <vector>

namespace qcp::algo {

// innerPool() is a single process-wide shared pool (not one instance per
// caller), so its size directly bounds total concurrent inner-parallel OS
// threads regardless of how many outer resample jobs are in flight at once
// -- callers just queue for the shared slots instead of each spawning their
// own. Scales with hardware (matches the outer pipeline scheduler's own
// default sizing in pipeline-scheduler.cpp) rather than a fixed ceiling, so
// worst case is ~outer_pool_size + inner_pool_size concurrent threads, not a
// product of the two.
inline int innerThreadCount()
{
    return std::max(1, QThread::idealThreadCount() / 2);
}

inline QThreadPool& innerPool()
{
    static QThreadPool pool;
    static bool init = [&] {
        pool.setMaxThreadCount(innerThreadCount());
        pool.setExpiryTimeout(-1);
        return true;
    }();
    (void)init;
    return pool;
}

struct BinResult {
    std::vector<double> keys;
    std::vector<double> values;
};

// Initialize bin keys and values for numBins min/max pairs.
// keys: (binCenter, binCenter+halfWidth) per bin; values: NaN.
inline void initBinKeysAndValues(BinResult& out, int numBins, double keyLo, double binWidth)
{
    const double halfWidth = binWidth * 0.5;
    out.keys.resize(numBins * 2);
    out.values.resize(numBins * 2);
    for (int b = 0; b < numBins; ++b)
    {
        double binCenter = keyLo + (b + 0.5) * binWidth;
        out.keys[b * 2 + 0] = binCenter;
        out.keys[b * 2 + 1] = binCenter + halfWidth;
        out.values[b * 2 + 0] = std::numeric_limits<double>::quiet_NaN();
        out.values[b * 2 + 1] = std::numeric_limits<double>::quiet_NaN();
    }
}

struct L1ViewportBounds {
    int begin;
    int end;
};

// Find the L1 index range covering the viewport, snapped to even bin-pair boundaries.
inline L1ViewportBounds l1ViewportBounds(
    const std::vector<double>& l1Keys, int l1Size, const QCPRange& keyRange)
{
    auto beginIt = std::lower_bound(l1Keys.begin(), l1Keys.end(), keyRange.lower);
    auto endIt = std::upper_bound(l1Keys.begin(), l1Keys.end(), keyRange.upper);
    int l1Begin = std::max(0, static_cast<int>(beginIt - l1Keys.begin()) - 1);
    int l1End = std::min(l1Size, static_cast<int>(endIt - l1Keys.begin()) + 1);

    l1Begin = l1Begin & ~1;
    l1End = (l1End + 1) & ~1;
    l1End = std::min(l1End, l1Size);
    return {l1Begin, l1End};
}

// Bin source[begin..end) into numBins min/max pairs.
// Output: 2*numBins points — (binCenter, min), (binCenter+halfWidth, max) per bin.
// NaN values and non-finite keys are skipped. Empty bins produce NaN pairs.
inline BinResult binMinMax(
    const std::vector<double>& srcKeys,
    const std::vector<double>& srcValues,
    int begin, int end,
    const QCPRange& keyRange,
    int numBins)
{
    BinResult out;
    if (numBins <= 0 || keyRange.size() <= 0)
        return out;

    const double binWidth = keyRange.size() / numBins;
    const double keyLo = keyRange.lower;
    initBinKeysAndValues(out, numBins, keyLo, binWidth);

    for (int i = begin; i < end; ++i)
    {
        double k = srcKeys[i];
        double v = srcValues[i];
        if (std::isnan(v) || !std::isfinite(k)) continue;

        int bin = static_cast<int>((k - keyLo) / binWidth);
        bin = std::clamp(bin, 0, numBins - 1);

        double& mn = out.values[bin * 2 + 0];
        double& mx = out.values[bin * 2 + 1];
        if (std::isnan(mn) || v < mn) mn = v;
        if (std::isnan(mx) || v > mx) mx = v;
    }

    return out;
}

// Overload that bins directly from a QCPAbstractDataSource (no intermediate copy).
inline BinResult binMinMax(
    const QCPAbstractDataSource& src,
    int begin, int end,
    const QCPRange& keyRange,
    int numBins)
{
    BinResult out;
    if (numBins <= 0 || keyRange.size() <= 0)
        return out;

    const double binWidth = keyRange.size() / numBins;
    const double keyLo = keyRange.lower;
    initBinKeysAndValues(out, numBins, keyLo, binWidth);

    for (int i = begin; i < end; ++i)
    {
        double k = src.keyAt(i);
        double v = src.valueAt(i);
        if (std::isnan(v) || !std::isfinite(k)) continue;

        int bin = static_cast<int>((k - keyLo) / binWidth);
        bin = std::clamp(bin, 0, numBins - 1);

        double& mn = out.values[bin * 2 + 0];
        double& mx = out.values[bin * 2 + 1];
        if (std::isnan(mn) || v < mn) mn = v;
        if (std::isnan(mx) || v > mx) mx = v;
    }

    return out;
}

// Parallel Level 1 binning: splits source into N chunks with bin-aligned
// boundaries so each thread writes to disjoint output bins (zero synchronization).
// Falls back to single-threaded binMinMax when threadCount <= 1.
inline BinResult binMinMaxParallel(
    const QCPAbstractDataSource& src,
    int begin, int end,
    const QCPRange& keyRange,
    int numBins)
{
    PROFILE_HERE_N("binMinMaxParallel");
    int threadCount = innerThreadCount();
    if (threadCount <= 1 || (end - begin) < 1'000'000)
        return binMinMax(src, begin, end, keyRange, numBins);

    BinResult out;
    if (numBins <= 0 || keyRange.size() <= 0)
        return out;

    const double binWidth = keyRange.size() / numBins;
    const double keyLo = keyRange.lower;
    initBinKeysAndValues(out, numBins, keyLo, binWidth);

    // Clamp thread count to available bins
    threadCount = std::min(threadCount, numBins);

    // Worker: iterate source[srcBegin..srcEnd), update bins[binBegin..binEnd)
    auto worker = [&](int srcBegin, int srcEnd, int binBegin, int binEnd) {
        for (int i = srcBegin; i < srcEnd; ++i)
        {
            double k = src.keyAt(i);
            double v = src.valueAt(i);
            if (std::isnan(v) || !std::isfinite(k)) continue;

            int bin = static_cast<int>((k - keyLo) / binWidth);
            bin = std::clamp(bin, binBegin, binEnd - 1);

            double& mn = out.values[bin * 2 + 0];
            double& mx = out.values[bin * 2 + 1];
            if (std::isnan(mn) || v < mn) mn = v;
            if (std::isnan(mx) || v > mx) mx = v;
        }
    };

    // Partition bins evenly across threads, binary-search source for chunk boundaries
    int binsPerChunk = numBins / threadCount;
    QAtomicInt remaining(threadCount - 1);
    QSemaphore done;

    for (int t = 0; t < threadCount; ++t)
    {
        int binBegin = t * binsPerChunk;
        int binEnd = (t == threadCount - 1) ? numBins : (t + 1) * binsPerChunk;

        // Find source indices that map to this chunk's bin range
        double chunkKeyLo = keyLo + binBegin * binWidth;
        double chunkKeyHi = keyLo + binEnd * binWidth;
        int srcBegin_ = (t == 0) ? begin : src.findBegin(chunkKeyLo, false);
        int srcEnd_ = (t == threadCount - 1) ? end : src.findEnd(chunkKeyHi, false);
        srcBegin_ = std::clamp(srcBegin_, begin, end);
        srcEnd_ = std::clamp(srcEnd_, begin, end);

        if (t < threadCount - 1)
            innerPool().start([&, srcBegin_, srcEnd_, binBegin, binEnd] {
                nameThisPoolThreadOnce("binWorker");
                worker(srcBegin_, srcEnd_, binBegin, binEnd);
                if (remaining.fetchAndSubRelaxed(1) == 1)
                    done.release();
            });
        else
            worker(srcBegin_, srcEnd_, binBegin, binEnd); // current thread does last chunk
    }

    if (remaining.loadRelaxed() > 0)
        done.acquire();

    return out;
}

struct GraphResamplerCache {
    BinResult level1;
    QCPRange cachedKeyRange;
    int sourceSize = 0;
};

struct MultiColumnBinResult {
    std::vector<double> keys;    // 2 * numBins (shared across columns)
    std::vector<double> values;  // N * 2 * numBins, column-major
    std::vector<int> origin;     // empty, or same layout as values: source index, -1 for an empty slot
    int numColumns = 0;
    int stride() const { return static_cast<int>(keys.size()); }
};

struct MultiGraphResamplerCache {
    MultiColumnBinResult level1;
    QCPRange cachedKeyRange;
    int sourceSize = 0;
    int columnCount = 0;
};

inline MultiColumnBinResult binMinMaxMulti(
    const QCPAbstractMultiDataSource& src,
    int begin, int end,
    const QCPRange& keyRange,
    int numBins, bool withOrigin = false);

namespace detail {

// Min/max of every column into out (column c's 2 * numBins slots start at c * stride), and the
// source row of each into origin. bins[i] is the output bin of row first + i, -1 to skip it.
// Rows outer, columns inner: a row-major (N, k) array is then read once instead of k times.
template <bool WithOrigin>
inline void binRowsMinMax(const QCPAbstractMultiDataSource& src, int first,
                          const std::vector<int>& bins, double* out,
                          [[maybe_unused]] int* origin, int stride)
{
    const int N = src.columnCount();
    auto run = [&](auto valueAt) {
        const int count = static_cast<int>(bins.size());
        for (int i = 0; i < count; ++i)
        {
            const int bin = bins[i];
            if (bin < 0) continue;
            for (int c = 0; c < N; ++c)
            {
                const double v = valueAt(c, first + i);
                if (std::isnan(v)) continue;

                const int slot = c * stride + bin * 2;
                double& mn = out[slot + 0];
                double& mx = out[slot + 1];
                if (std::isnan(mn) || v < mn) { mn = v; if constexpr (WithOrigin) origin[slot + 0] = first + i; }
                if (std::isnan(mx) || v > mx) { mx = v; if constexpr (WithOrigin) origin[slot + 1] = first + i; }
            }
        }
    };
    const QCPRawColumn col0 = src.rawColumn(0);
    std::vector<QCPRawColumn> cols(N);
    for (int c = 0; c < N; ++c)
        cols[c] = src.rawColumn(c);
    const bool oneRawType = std::ranges::all_of(
        cols, [&](const QCPRawColumn& col) { return col && col.type == col0.type; });
    const bool ranRaw = oneRawType && visitRawColumn(col0, [&](auto raw0) {
        std::vector<decltype(raw0)> raw(N);
        for (int c = 0; c < N; ++c)
            raw[c] = {static_cast<decltype(raw0.data)>(cols[c].data), cols[c].stride};
        run([&raw](int c, int i) { return raw[c][i]; });
    });
    if (!ranRaw)
        run([&](int c, int i) { return src.valueAt(c, i); });
}

template <bool WithOrigin>
inline MultiColumnBinResult binMinMaxMultiImpl(
    const QCPAbstractMultiDataSource& src,
    int begin, int end,
    const QCPRange& keyRange,
    int numBins)
{
    MultiColumnBinResult out;
    int N = src.columnCount();
    if (numBins <= 0 || keyRange.size() <= 0 || N <= 0)
        return out;

    out.numColumns = N;
    out.keys.resize(numBins * 2);
    out.values.resize(N * numBins * 2, std::numeric_limits<double>::quiet_NaN());
    if constexpr (WithOrigin) out.origin.assign(N * numBins * 2, -1);

    const double binWidth = keyRange.size() / numBins;
    const double halfWidth = binWidth * 0.5;
    const double keyLo = keyRange.lower;
    const int s = numBins * 2;

    for (int b = 0; b < numBins; ++b)
    {
        double binCenter = keyLo + (b + 0.5) * binWidth;
        out.keys[b * 2 + 0] = binCenter;
        out.keys[b * 2 + 1] = binCenter + halfWidth;
    }

    // Pre-compute bin indices for all source points (keys are shared across columns)
    const double* rawKeys = src.rawKeyData();
    std::vector<int> bins(end - begin);
    int validCount = 0;
    for (int i = begin; i < end; ++i)
    {
        double k = rawKeys ? rawKeys[i] : src.keyAt(i);
        if (!std::isfinite(k)) { bins[i - begin] = -1; continue; }
        bins[i - begin] = std::clamp(static_cast<int>((k - keyLo) / binWidth), 0, numBins - 1);
        ++validCount;
    }
    if (validCount == 0) return out;

    binRowsMinMax<WithOrigin>(src, begin, bins, out.values.data(),
                              WithOrigin ? out.origin.data() : nullptr, s);

    return out;
}

template <bool WithOrigin>
inline MultiColumnBinResult binMinMaxMultiParallelImpl(
    const QCPAbstractMultiDataSource& src,
    int begin, int end,
    const QCPRange& keyRange,
    int numBins)
{
    PROFILE_HERE_N("binMinMaxMultiParallel");
    int threadCount = innerThreadCount();
    if (threadCount <= 1 || (end - begin) < 1'000'000)
        return binMinMaxMulti(src, begin, end, keyRange, numBins, WithOrigin);

    int N = src.columnCount();
    MultiColumnBinResult out;
    if (numBins <= 0 || keyRange.size() <= 0 || N <= 0)
        return out;

    out.numColumns = N;
    out.keys.resize(numBins * 2);
    out.values.resize(N * numBins * 2, std::numeric_limits<double>::quiet_NaN());
    if constexpr (WithOrigin) out.origin.assign(N * numBins * 2, -1);

    const double binWidth = keyRange.size() / numBins;
    const double halfWidth = binWidth * 0.5;
    const double keyLo = keyRange.lower;
    const int s = numBins * 2;

    for (int b = 0; b < numBins; ++b)
    {
        double binCenter = keyLo + (b + 0.5) * binWidth;
        out.keys[b * 2 + 0] = binCenter;
        out.keys[b * 2 + 1] = binCenter + halfWidth;
    }

    threadCount = std::min(threadCount, numBins);

    const double* rawKeys = src.rawKeyData();

    auto worker = [&](int srcBegin, int srcEnd, int binBegin, int binEnd) {
        // Pre-compute bin indices for this chunk
        int count = srcEnd - srcBegin;
        std::vector<int> bins(count);
        for (int i = 0; i < count; ++i)
        {
            double k = rawKeys ? rawKeys[srcBegin + i] : src.keyAt(srcBegin + i);
            if (!std::isfinite(k)) { bins[i] = -1; continue; }
            bins[i] = std::clamp(static_cast<int>((k - keyLo) / binWidth), binBegin, binEnd - 1);
        }

        binRowsMinMax<WithOrigin>(src, srcBegin, bins, out.values.data(),
                                  WithOrigin ? out.origin.data() : nullptr, s);
    };

    int binsPerChunk = numBins / threadCount;
    QAtomicInt remaining(threadCount - 1);
    QSemaphore done;

    for (int t = 0; t < threadCount; ++t)
    {
        int binBegin = t * binsPerChunk;
        int binEnd = (t == threadCount - 1) ? numBins : (t + 1) * binsPerChunk;

        double chunkKeyLo = keyLo + binBegin * binWidth;
        double chunkKeyHi = keyLo + binEnd * binWidth;
        int srcBegin_ = (t == 0) ? begin : src.findBegin(chunkKeyLo, false);
        int srcEnd_ = (t == threadCount - 1) ? end : src.findEnd(chunkKeyHi, false);
        srcBegin_ = std::clamp(srcBegin_, begin, end);
        srcEnd_ = std::clamp(srcEnd_, begin, end);

        if (t < threadCount - 1)
            innerPool().start([&, srcBegin_, srcEnd_, binBegin, binEnd] {
                nameThisPoolThreadOnce("binWorker");
                worker(srcBegin_, srcEnd_, binBegin, binEnd);
                if (remaining.fetchAndSubRelaxed(1) == 1)
                    done.release();
            });
        else
            worker(srcBegin_, srcEnd_, binBegin, binEnd);
    }

    if (remaining.loadRelaxed() > 0)
        done.acquire();

    return out;
}

} // namespace detail

inline MultiColumnBinResult binMinMaxMulti(
    const QCPAbstractMultiDataSource& src,
    int begin, int end,
    const QCPRange& keyRange,
    int numBins, bool withOrigin)
{
    return withOrigin ? detail::binMinMaxMultiImpl<true>(src, begin, end, keyRange, numBins)
                      : detail::binMinMaxMultiImpl<false>(src, begin, end, keyRange, numBins);
}

inline MultiColumnBinResult binMinMaxMultiParallel(
    const QCPAbstractMultiDataSource& src,
    int begin, int end,
    const QCPRange& keyRange,
    int numBins, bool withOrigin = false)
{
    return withOrigin ? detail::binMinMaxMultiParallelImpl<true>(src, begin, end, keyRange, numBins)
                      : detail::binMinMaxMultiParallelImpl<false>(src, begin, end, keyRange, numBins);
}

constexpr int kLevel1TargetBins = 100'000;
constexpr int kResampleThreshold = 100'000;
constexpr int kLevel2PixelMultiplier = 4;

// The keys a graph's line cache covers: one view width on each side, so a GPU-translated pan
// does not expose uncovered edges before the lines are rebuilt.
inline QCPRange lineCacheKeyRange(const QCPRange& view)
{
    return {view.lower - view.size(), view.upper + view.size()};
}

inline int l2BinCount(const ViewportParams& vp)
{
    const int bins = vp.plotWidthPx * kLevel2PixelMultiplier;
    return bins > 0 ? bins : 3200;
}

// Decided from the raw rows the line cache would hold, not from L1 bins: L1 bins are equal-width
// over the whole data, so with bursty data a single one can hide thousands of raw rows.
template <typename Source>
inline bool fewEnoughToDrawRaw(const Source& raw, const ViewportParams& vp)
{
    const QCPRange scan = lineCacheKeyRange(vp.keyRange);
    return raw.findEnd(scan.upper) - raw.findBegin(scan.lower) <= l2BinCount(vp);
}

// One L1 min/max pair per pixel is all a min/max envelope needs; coarser, L2 must bin raw rows.
inline bool l1ResolvesViewport(int l1RowsInView, const ViewportParams& vp)
{
    const int pixels = l2BinCount(vp) / kLevel2PixelMultiplier;
    return l1RowsInView >= 2 * pixels;
}

struct RowRange {
    int begin;
    int end;
};

template <typename Source>
inline RowRange rawRowsInView(const Source& raw, const QCPRange& keyRange)
{
    return {raw.findBegin(keyRange.lower, false), raw.findEnd(keyRange.upper, false)};
}

// L1 build only — heavy, meant for async pipeline.
// Returns the L1 cache via the std::any, result is nullptr (L2 is done synchronously).
inline std::shared_ptr<QCPAbstractDataSource> buildL1Cache(
    const QCPAbstractDataSource& src,
    const ViewportParams& /*vp*/,
    std::any& cache)
{
    PROFILE_HERE_N("buildL1Cache");
    const int srcSize = src.size();
    if (srcSize == 0 || srcSize < kResampleThreshold)
        return nullptr;

    bool foundRange = false;
    QCPRange fullKeyRange = src.keyRange(foundRange);
    if (!foundRange || fullKeyRange.size() <= 0)
        return nullptr;

    // Same size is not enough: replacing the data with an equally-sized batch
    // must not reuse a stale L1, so the cached key range participates too
    // (keyRange is O(1) on sorted sources).
    auto* c = std::any_cast<GraphResamplerCache>(&cache);
    if (c && c->sourceSize == srcSize && c->cachedKeyRange == fullKeyRange)
        return nullptr; // L1 already valid
    int numBins = std::min(kLevel1TargetBins, srcSize / 10);

    GraphResamplerCache newCache;
    newCache.level1 = binMinMaxParallel(src, 0, srcSize, fullKeyRange, numBins);
    newCache.cachedKeyRange = fullKeyRange;
    newCache.sourceSize = srcSize;
    cache = std::move(newCache);
    return nullptr;
}

// L2 viewport resampling — runs synchronously on the main thread.
// Returns nullptr when the raw rows are few enough to draw directly.
// simplify: when L1 is too coarse for the view, the raw rows in view are binned here on the GUI
// thread, O(rows in plotWidthPx L1 bins). Upgrade path: build that L2 in the async pipeline.
inline std::shared_ptr<QCPAbstractDataSource> resampleL2(
    const GraphResamplerCache& l1Cache,
    const ViewportParams& vp,
    const QCPAbstractDataSource& raw)
{
    PROFILE_HERE_N("resampleL2");
    const auto& l1 = l1Cache.level1;
    if (vp.keyLogScale || l1.keys.empty() || fewEnoughToDrawRaw(raw, vp))
        return nullptr;

    const int l2Bins = l2BinCount(vp);
    const auto [l1Begin, l1End] = l1ViewportBounds(l1.keys, static_cast<int>(l1.keys.size()), vp.keyRange);
    const auto rows = rawRowsInView(raw, vp.keyRange);
    auto l2 = l1ResolvesViewport(l1End - l1Begin, vp)
        ? binMinMax(l1.keys, l1.values, l1Begin, l1End, vp.keyRange, l2Bins)
        : binMinMaxParallel(raw, rows.begin, rows.end, vp.keyRange, l2Bins);

    std::vector<double> outKeys, outVals;
    outKeys.reserve(l2.keys.size());
    outVals.reserve(l2.values.size());
    for (size_t i = 0; i < l2.keys.size(); ++i)
    {
        if (!std::isnan(l2.values[i]))
        {
            outKeys.push_back(l2.keys[i]);
            outVals.push_back(l2.values[i]);
        }
    }

    // Empty, not nullptr, when nothing is in view: nullptr would make the graph draw raw rows.
    return std::make_shared<QCPSoADataSource<
        std::vector<double>, std::vector<double>>>(
        std::move(outKeys), std::move(outVals));
}

// L1 build for multi-column sources — heavy, meant for async pipeline.
// Stores the L1 cache in the std::any; returns nullptr (L2 is done synchronously).
inline std::shared_ptr<QCPAbstractMultiDataSource> buildL1CacheMulti(
    const QCPAbstractMultiDataSource& src,
    const ViewportParams& /*vp*/,
    std::any& cache, bool withOrigin = false)
{
    PROFILE_HERE_N("buildL1CacheMulti");
    const int srcSize = src.size();
    const int N = src.columnCount();
    if (srcSize == 0 || N == 0)
        return nullptr;

    bool foundRange = false;
    QCPRange fullKeyRange = src.keyRange(foundRange);
    if (!foundRange || fullKeyRange.size() <= 0)
        return nullptr;

    // The request itself is not stored in MultiGraphResamplerCache: extractL1Cache moves
    // the cache out of the pipeline slot after every build, so a flag stored there would
    // be lost. The graph owns that state instead; this check only keeps a stale
    // origin-less cache from being reused when origin is wanted.
    auto* c = std::any_cast<MultiGraphResamplerCache>(&cache);
    if (c && c->sourceSize == srcSize && c->columnCount == N
        && c->cachedKeyRange == fullKeyRange
        && (!withOrigin || !c->level1.origin.empty()))
        return nullptr;
    int numBins = std::min(kLevel1TargetBins, srcSize / 10);

    MultiGraphResamplerCache newCache;
    newCache.level1 = binMinMaxMultiParallel(src, 0, srcSize, fullKeyRange, numBins, withOrigin);
    newCache.cachedKeyRange = fullKeyRange;
    newCache.sourceSize = srcSize;
    newCache.columnCount = N;
    cache = std::move(newCache);
    return nullptr;
}

} // namespace qcp::algo
