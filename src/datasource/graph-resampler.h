#pragma once
#include "abstract-datasource.h"
#include "soa-datasource.h"
#include "async-pipeline.h"
#include "../Profiling.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <thread>
#include <vector>

namespace qcp::algo {

struct BinResult {
    std::vector<double> keys;
    std::vector<double> values;
};

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

    out.keys.resize(numBins * 2);
    out.values.resize(numBins * 2);

    const double binWidth = keyRange.size() / numBins;
    const double halfWidth = binWidth * 0.5;
    const double keyLo = keyRange.lower;

    for (int b = 0; b < numBins; ++b)
    {
        double binCenter = keyLo + (b + 0.5) * binWidth;
        out.keys[b * 2 + 0] = binCenter;
        out.keys[b * 2 + 1] = binCenter + halfWidth;
        out.values[b * 2 + 0] = std::numeric_limits<double>::quiet_NaN();
        out.values[b * 2 + 1] = std::numeric_limits<double>::quiet_NaN();
    }

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

    out.keys.resize(numBins * 2);
    out.values.resize(numBins * 2);

    const double binWidth = keyRange.size() / numBins;
    const double halfWidth = binWidth * 0.5;
    const double keyLo = keyRange.lower;

    for (int b = 0; b < numBins; ++b)
    {
        double binCenter = keyLo + (b + 0.5) * binWidth;
        out.keys[b * 2 + 0] = binCenter;
        out.keys[b * 2 + 1] = binCenter + halfWidth;
        out.values[b * 2 + 0] = std::numeric_limits<double>::quiet_NaN();
        out.values[b * 2 + 1] = std::numeric_limits<double>::quiet_NaN();
    }

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
    int threadCount = std::max(1, static_cast<int>(std::thread::hardware_concurrency()) / 2);
    if (threadCount <= 1 || (end - begin) < 1'000'000)
        return binMinMax(src, begin, end, keyRange, numBins);

    BinResult out;
    if (numBins <= 0 || keyRange.size() <= 0)
        return out;

    out.keys.resize(numBins * 2);
    out.values.resize(numBins * 2);

    const double binWidth = keyRange.size() / numBins;
    const double halfWidth = binWidth * 0.5;
    const double keyLo = keyRange.lower;

    // Initialize all bins: keys are deterministic, values start as NaN
    for (int b = 0; b < numBins; ++b)
    {
        double binCenter = keyLo + (b + 0.5) * binWidth;
        out.keys[b * 2 + 0] = binCenter;
        out.keys[b * 2 + 1] = binCenter + halfWidth;
        out.values[b * 2 + 0] = std::numeric_limits<double>::quiet_NaN();
        out.values[b * 2 + 1] = std::numeric_limits<double>::quiet_NaN();
    }

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
    std::vector<std::thread> threads;
    threads.reserve(threadCount - 1);

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
            threads.emplace_back(worker, srcBegin_, srcEnd_, binBegin, binEnd);
        else
            worker(srcBegin_, srcEnd_, binBegin, binEnd); // current thread does last chunk
    }

    for (auto& t : threads)
        t.join();

    return out;
}

struct GraphResamplerCache {
    BinResult level1;
    QCPRange cachedKeyRange;
    int sourceSize = 0;
};

constexpr int kLevel1TargetBins = 500'000;
constexpr int kResampleThreshold = 10'000'000;
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
    int numBins = std::min(kLevel1TargetBins, srcSize / 2);

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

    auto beginIt = std::lower_bound(l1.keys.begin(), l1.keys.end(), vp.keyRange.lower);
    auto endIt = std::upper_bound(l1.keys.begin(), l1.keys.end(), vp.keyRange.upper);
    int l1Begin = std::max(0, static_cast<int>(beginIt - l1.keys.begin()) - 1);
    int l1End = std::min(l1Size, static_cast<int>(endIt - l1.keys.begin()) + 1);

    l1Begin = l1Begin & ~1;
    l1End = (l1End + 1) & ~1;
    l1End = std::min(l1End, l1Size);

    if (l1End <= l1Begin)
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

// Legacy combined function (kept for tests) — delegates to the two-phase functions.
inline std::shared_ptr<QCPAbstractDataSource> hierarchicalResample(
    const QCPAbstractDataSource& src,
    const ViewportParams& vp,
    std::any& cache)
{
    PROFILE_HERE_N("hierarchicalResample");
    const int srcSize = src.size();
    if (srcSize == 0 || srcSize < kResampleThreshold || vp.keyLogScale)
        return nullptr;

    buildL1Cache(src, vp, cache);
    auto* c = std::any_cast<GraphResamplerCache>(&cache);
    if (!c) return nullptr;
    return resampleL2(*c, vp);
}

} // namespace qcp::algo
