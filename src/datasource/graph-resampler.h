#pragma once
#include "abstract-datasource.h"
#include "abstract-multi-datasource.h"
#include "async-pipeline.h"
#include "thread-naming.h"
#include "../Profiling.hpp"
#include <QSemaphore>
#include <QThread>
#include <QThreadPool>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <span>
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
#ifdef Q_OS_WASM
    // A wasm thread only starts once the main thread returns to the browser's event loop.
    // A job waiting for inner threads, while the main thread waits for that job
    // (~QCPPipelineScheduler's waitForDone), never wakes up: run the chunks serially.
    return 1;
#else
    return std::max(1, QThread::idealThreadCount() / 2);
#endif
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

// Waits for the tasks one parallel pass started on innerPool(). Each task holds the semaphore
// through a shared_ptr, so a task still inside release() never releases a semaphore the caller has
// already destroyed: the caller waits for exactly the tasks it started, however late they finish.
class QCPPoolJoin
{
public:
    template <typename F>
    void start(F&& task)
    {
        ++mStarted;
        innerPool().start([task = std::forward<F>(task), done = mDone]() mutable {
            task();
            done->release();
        });
    }

    void wait()
    {
        if (mStarted > 0)
            mDone->acquire(mStarted);
    }

private:
    std::shared_ptr<QSemaphore> mDone = std::make_shared<QSemaphore>();
    int mStarted = 0;
};

// Min/max bins hold (min, max) whatever order their samples came in. Drawn as a polyline, a
// falling stretch then goes up and down inside every bin and looks thicker than a rising one.
// This puts each pair in the direction the line travels; origin, if given, follows its values.
// simplify: the direction comes from the neighbouring bins, not from the samples' true order.
// It is exact on monotonic stretches; at a turning point the two orders differ inside one bin.
inline void orderPairsByTrend(std::span<double> values, std::span<int> origin = {})
{
    const int pairs = static_cast<int>(values.size() / 2);
    const auto level = [&](int p) { return 0.5 * (values[2 * p] + values[2 * p + 1]); };
    for (int p = 0; p < pairs; ++p)
    {
        const double here = level(p);
        const double before = p > 0 && std::isfinite(level(p - 1)) ? level(p - 1) : here;
        const double after = p + 1 < pairs && std::isfinite(level(p + 1)) ? level(p + 1) : here;
        if (before > after)
        {
            std::swap(values[2 * p], values[2 * p + 1]);
            if (!origin.empty())
                std::swap(origin[2 * p], origin[2 * p + 1]);
        }
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

struct MultiColumnBinResult {
    std::vector<double> keys;    // 2 * numBins (shared across columns)
    std::vector<double> values;  // N * 2 * numBins, column-major
    std::vector<int> origin;     // empty, or same layout as values: source index, -1 for an empty slot
    std::vector<float> gapRatio; // same layout as keys, see gapsFromStepRatios (0 on max slots)
    int numColumns = 0;
    int stride() const { return static_cast<int>(keys.size()); }
};

struct GraphResamplerCache {
    MultiColumnBinResult level1;
    QCPRange cachedKeyRange;
    int sourceSize = 0;
    int columnCount = 0;
};

inline MultiColumnBinResult orderColumnsByTrend(MultiColumnBinResult bins)
{
    const int s = bins.stride();
    for (int c = 0; c < bins.numColumns; ++c)
        orderPairsByTrend(std::span(bins.values).subspan(c * s, s),
                          bins.origin.empty() ? std::span<int>{}
                                              : std::span(bins.origin).subspan(c * s, s));
    return bins;
}

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

// binRatio[b]: the largest keyStepRatio of the rows of bin b (see binRowsMinMax for bins),
// left at -1 for a bin without rows.
inline void foldRowStepRatios(const QCPAbstractMultiDataSource& src, int first,
                              const std::vector<int>& bins, float* binRatio)
{
    const double* rawKeys = src.rawKeyData();
    const auto key = [&](int i) { return rawKeys ? rawKeys[i] : src.keyAt(i); };
    const int size = src.size();
    for (int i = 0; i < static_cast<int>(bins.size()); ++i)
        if (bins[i] >= 0)
            binRatio[bins[i]] = std::max(binRatio[bins[i]],
                                         static_cast<float>(keyStepRatio(key, 0, size, first + i)));
}

// A bin with rows but no value in any column holds NaN only: +inf, see DroppedBins. One column NaN
// while others have data stays NaN in the bin's values: adaptive sampling (the default) breaks
// that column's line on it when wider than a pixel; the unthinned line bridges the hole.
inline void setGapRatios(MultiColumnBinResult& out, const std::vector<float>& binRatio)
{
    const int s = out.stride();
    out.gapRatio.assign(s, 0.f);
    for (int b = 0; b < static_cast<int>(binRatio.size()); ++b)
    {
        if (binRatio[b] < 0) continue;
        bool anyValue = false;
        for (int c = 0; c < out.numColumns && !anyValue; ++c)
            anyValue = !std::isnan(out.values[c * s + b * 2]);
        out.gapRatio[b * 2] = anyValue ? binRatio[b] : std::numeric_limits<float>::infinity();
    }
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

    std::vector<float> binRatio(numBins, -1.f);
    foldRowStepRatios(src, begin, bins, binRatio.data());
    binRowsMinMax<WithOrigin>(src, begin, bins, out.values.data(),
                              WithOrigin ? out.origin.data() : nullptr, s);
    setGapRatios(out, binRatio);

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
    std::vector<float> binRatio(numBins, -1.f);

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

        foldRowStepRatios(src, srcBegin, bins, binRatio.data());
        binRowsMinMax<WithOrigin>(src, srcBegin, bins, out.values.data(),
                                  WithOrigin ? out.origin.data() : nullptr, s);
    };

    int binsPerChunk = numBins / threadCount;
    QCPPoolJoin join;

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
            join.start([&, srcBegin_, srcEnd_, binBegin, binEnd] {
                nameThisPoolThreadOnce("binWorker");
                worker(srcBegin_, srcEnd_, binBegin, binEnd);
            });
        else
            worker(srcBegin_, srcEnd_, binBegin, binEnd);
    }
    join.wait();

    setGapRatios(out, binRatio);
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

// The gap ratio of a drawn L2 bin, from the bins dropped just before it for having no value: their
// largest raw step ratio, or +inf when they held NaN rows over at least a pixel. A narrower NaN run
// cannot be seen, and is skipped as the raw envelope skips it (linesInData).
class DroppedBins {
public:
    void drop(float ratio, bool nanRows)
    {
        ++mCount;
        mNaN = mNaN || nanRows;
        if (std::isfinite(ratio))
            mRatio = std::max(mRatio, ratio);
    }

    float keep(float ratio)
    {
        const float out = mNaN && mCount >= kLevel2PixelMultiplier
            ? std::numeric_limits<float>::infinity()
            : std::max(mRatio, std::isfinite(ratio) ? ratio : 0.f);
        *this = {};
        return out;
    }

private:
    float mRatio = 0.f;
    int mCount = 0;
    bool mNaN = false;
};

// The keys a graph's line cache covers: one view width on each side, so a GPU-translated pan
// does not expose uncovered edges before the lines are rebuilt. On a log axis a width is a
// ratio, so [1, 10] covers [0.1, 100].
inline QCPRange lineCacheKeyRange(const QCPRange& view, bool logScale)
{
    if (logScale && view.lower > 0)
    {
        const double ratio = view.upper / view.lower;
        return {view.lower / ratio, view.upper * ratio};
    }
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
    const QCPRange scan = lineCacheKeyRange(vp.keyRange, vp.keyLogScale);
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

    // The request itself is not stored in GraphResamplerCache: extractL1Cache moves
    // the cache out of the pipeline slot after every build, so a flag stored there would
    // be lost. The graph owns that state instead; this check only keeps a stale
    // origin-less cache from being reused when origin is wanted.
    auto* c = std::any_cast<GraphResamplerCache>(&cache);
    if (c && c->sourceSize == srcSize && c->columnCount == N
        && c->cachedKeyRange == fullKeyRange
        && (!withOrigin || !c->level1.origin.empty()))
        return nullptr;
    int numBins = std::min(kLevel1TargetBins, srcSize / 10);

    GraphResamplerCache newCache;
    newCache.level1 = orderColumnsByTrend(
        binMinMaxMultiParallel(src, 0, srcSize, fullKeyRange, numBins, withOrigin));
    newCache.cachedKeyRange = fullKeyRange;
    newCache.sourceSize = srcSize;
    newCache.columnCount = N;
    cache = std::move(newCache);
    return nullptr;
}

} // namespace qcp::algo
