#pragma once
#include "abstract-datasource.h"
#include "abstract-multi-datasource.h"
#include "soa-datasource.h"
#include "async-pipeline.h"
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

inline QThreadPool& innerPool()
{
    static QThreadPool pool;
    static bool init = [&] {
        pool.setMaxThreadCount(
            std::min(4, std::max(1, QThread::idealThreadCount() / 2)));
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
// Cap inner parallelism — these functions already run inside the pipeline
// scheduler's thread pool, so spawning hardware_concurrency/2 threads here
// would produce O(pool_size × inner_threads) total OS threads.
constexpr int kMaxInnerThreads = 4;

inline BinResult binMinMaxParallel(
    const QCPAbstractDataSource& src,
    int begin, int end,
    const QCPRange& keyRange,
    int numBins)
{
    PROFILE_HERE_N("binMinMaxParallel");
    int threadCount = std::min(kMaxInnerThreads,
        std::max(1, QThread::idealThreadCount() / 2));
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
    int numBins)
{
    MultiColumnBinResult out;
    int N = src.columnCount();
    if (numBins <= 0 || keyRange.size() <= 0 || N <= 0)
        return out;

    out.numColumns = N;
    out.keys.resize(numBins * 2);
    out.values.resize(N * numBins * 2, std::numeric_limits<double>::quiet_NaN());

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

    // Outer loop over columns: each column's output region is contiguous in memory
    for (int c = 0; c < N; ++c)
    {
        double* colOut = out.values.data() + c * s;
        const double* rawCol = src.rawColumnData(c);
        for (int i = begin; i < end; ++i)
        {
            int bin = bins[i - begin];
            if (bin < 0) continue;
            double v = rawCol ? rawCol[i] : src.valueAt(c, i);
            if (std::isnan(v)) continue;

            double& mn = colOut[bin * 2 + 0];
            double& mx = colOut[bin * 2 + 1];
            if (std::isnan(mn) || v < mn) mn = v;
            if (std::isnan(mx) || v > mx) mx = v;
        }
    }

    return out;
}

inline MultiColumnBinResult binMinMaxMultiParallel(
    const QCPAbstractMultiDataSource& src,
    int begin, int end,
    const QCPRange& keyRange,
    int numBins)
{
    PROFILE_HERE_N("binMinMaxMultiParallel");
    int threadCount = std::min(kMaxInnerThreads,
        std::max(1, QThread::idealThreadCount() / 2));
    if (threadCount <= 1 || (end - begin) < 1'000'000)
        return binMinMaxMulti(src, begin, end, keyRange, numBins);

    int N = src.columnCount();
    MultiColumnBinResult out;
    if (numBins <= 0 || keyRange.size() <= 0 || N <= 0)
        return out;

    out.numColumns = N;
    out.keys.resize(numBins * 2);
    out.values.resize(N * numBins * 2, std::numeric_limits<double>::quiet_NaN());

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
    std::vector<const double*> rawCols(N);
    for (int c = 0; c < N; ++c)
        rawCols[c] = src.rawColumnData(c);

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

        for (int c = 0; c < N; ++c)
        {
            double* colOut = out.values.data() + c * s;
            const double* rawCol = rawCols[c];
            for (int i = 0; i < count; ++i)
            {
                int bin = bins[i];
                if (bin < 0) continue;
                double v = rawCol ? rawCol[srcBegin + i] : src.valueAt(c, srcBegin + i);
                if (std::isnan(v)) continue;

                double& mn = colOut[bin * 2 + 0];
                double& mx = colOut[bin * 2 + 1];
                if (std::isnan(mn) || v < mn) mn = v;
                if (std::isnan(mx) || v > mx) mx = v;
            }
        }
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

constexpr int kLevel1TargetBins = 100'000;
constexpr int kResampleThreshold = 100'000;
constexpr int kLevel2PixelMultiplier = 4;

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

    auto* c = std::any_cast<GraphResamplerCache>(&cache);
    if (c && c->sourceSize == srcSize)
        return nullptr; // L1 already valid

    bool foundRange = false;
    QCPRange fullKeyRange = src.keyRange(foundRange);
    if (!foundRange || fullKeyRange.size() <= 0)
        return nullptr;
    int numBins = std::min(kLevel1TargetBins, srcSize / 10);

    GraphResamplerCache newCache;
    newCache.level1 = binMinMaxParallel(src, 0, srcSize, fullKeyRange, numBins);
    newCache.cachedKeyRange = fullKeyRange;
    newCache.sourceSize = srcSize;
    cache = std::move(newCache);
    return nullptr;
}

// L2 viewport resampling — fast, runs synchronously on the main thread.
// Takes a shared L1 cache (read-only) and the current viewport.
inline std::shared_ptr<QCPAbstractDataSource> resampleL2(
    const GraphResamplerCache& l1Cache,
    const ViewportParams& vp)
{
    PROFILE_HERE_N("resampleL2");
    if (vp.keyLogScale)
        return nullptr;

    int l2Bins = vp.plotWidthPx * kLevel2PixelMultiplier;
    if (l2Bins <= 0) l2Bins = 3200;

    const auto& l1 = l1Cache.level1;
    int l1Size = static_cast<int>(l1.keys.size());
    if (l1Size == 0) return nullptr;

    auto [l1Begin, l1End] = l1ViewportBounds(l1.keys, l1Size, vp.keyRange);
    if (l1End <= l1Begin)
        return nullptr;

    // Skip L2 binning when visible points are sparse enough to draw directly
    if (l1End - l1Begin <= l2Bins)
        return nullptr;

    auto l2 = binMinMax(l1.keys, l1.values, l1Begin, l1End, vp.keyRange, l2Bins);

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

    if (outKeys.empty()) return nullptr;

    return std::make_shared<QCPSoADataSource<
        std::vector<double>, std::vector<double>>>(
        std::move(outKeys), std::move(outVals));
}

// L1 build for multi-column sources — heavy, meant for async pipeline.
// Stores the L1 cache in the std::any; returns nullptr (L2 is done synchronously).
inline std::shared_ptr<QCPAbstractMultiDataSource> buildL1CacheMulti(
    const QCPAbstractMultiDataSource& src,
    const ViewportParams& /*vp*/,
    std::any& cache)
{
    PROFILE_HERE_N("buildL1CacheMulti");
    const int srcSize = src.size();
    const int N = src.columnCount();
    if (srcSize == 0 || N == 0)
        return nullptr;

    auto* c = std::any_cast<MultiGraphResamplerCache>(&cache);
    if (c && c->sourceSize == srcSize && c->columnCount == N)
        return nullptr;

    bool foundRange = false;
    QCPRange fullKeyRange = src.keyRange(foundRange);
    if (!foundRange || fullKeyRange.size() <= 0)
        return nullptr;
    int numBins = std::min(kLevel1TargetBins, srcSize / 10);

    MultiGraphResamplerCache newCache;
    newCache.level1 = binMinMaxMultiParallel(src, 0, srcSize, fullKeyRange, numBins);
    newCache.cachedKeyRange = fullKeyRange;
    newCache.sourceSize = srcSize;
    newCache.columnCount = N;
    cache = std::move(newCache);
    return nullptr;
}

} // namespace qcp::algo
