#pragma once
#include "abstract-datasource.h"
#include "soa-datasource.h"
#include "async-pipeline.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
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

struct GraphResamplerCache {
    BinResult level1;
    QCPRange cachedKeyRange;
    int sourceSize = 0;
};

constexpr int kLevel1TargetBins = 500'000;
constexpr int kResampleThreshold = 10'000'000;
constexpr int kLevel2PixelMultiplier = 4;

inline std::shared_ptr<QCPAbstractDataSource> hierarchicalResample(
    const QCPAbstractDataSource& src,
    const ViewportParams& vp,
    std::any& cache)
{
    const int srcSize = src.size();
    if (srcSize == 0 || srcSize < kResampleThreshold || vp.keyLogScale)
        return nullptr;

    auto* c = std::any_cast<GraphResamplerCache>(&cache);
    bool cacheValid = c && c->sourceSize == srcSize;

    if (!cacheValid)
    {
        bool foundRange = false;
        QCPRange fullKeyRange = src.keyRange(foundRange);
        if (!foundRange || fullKeyRange.size() <= 0)
            return nullptr;
        int numBins = std::min(kLevel1TargetBins, srcSize / 2);

        // Bin directly from source — no full copy
        GraphResamplerCache newCache;
        newCache.level1 = binMinMax(src, 0, srcSize, fullKeyRange, numBins);
        newCache.cachedKeyRange = fullKeyRange;
        newCache.sourceSize = srcSize;
        cache = std::move(newCache);
        c = std::any_cast<GraphResamplerCache>(&cache);
    }

    int l2Bins = vp.plotWidthPx * kLevel2PixelMultiplier;
    if (l2Bins <= 0) l2Bins = 3200;

    const auto& l1 = c->level1;
    int l1Size = static_cast<int>(l1.keys.size());

    auto beginIt = std::lower_bound(l1.keys.begin(), l1.keys.end(), vp.keyRange.lower);
    auto endIt = std::upper_bound(l1.keys.begin(), l1.keys.end(), vp.keyRange.upper);
    int l1Begin = std::max(0, static_cast<int>(beginIt - l1.keys.begin()) - 1);
    int l1End = std::min(l1Size, static_cast<int>(endIt - l1.keys.begin()) + 1);

    // Align to pair boundaries so we never split a min/max pair
    l1Begin = l1Begin & ~1;          // round down to even
    l1End = (l1End + 1) & ~1;        // round up to even
    l1End = std::min(l1End, l1Size); // clamp after rounding

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

    return std::make_shared<QCPSoADataSource<
        std::vector<double>, std::vector<double>>>(
        std::move(outKeys), std::move(outVals));
}

} // namespace qcp::algo
