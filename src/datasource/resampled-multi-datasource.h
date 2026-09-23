#pragma once
#include "abstract-multi-datasource.h"
#include "graph-resampler.h"
#include "algorithms.h"
#include "../Profiling.hpp"
#include <algorithm>
#include <cmath>
#include <span>

class QCPResampledMultiDataSource final : public QCPAbstractMultiDataSource {
public:
    explicit QCPResampledMultiDataSource(qcp::algo::MultiColumnBinResult bins)
        : mBins(std::move(bins)) {}

    int columnCount() const override { return mBins.numColumns; }
    int size() const override { return static_cast<int>(mBins.keys.size()); }

    double keyAt(int i) const override { return mBins.keys[i]; }

    double valueAt(int column, int i) const override
    {
        return mBins.values[column * mBins.stride() + i];
    }

    QCPRange keyRange(bool& found, QCP::SignDomain sd = QCP::sdBoth) const override
    {
        return qcp::algo::keyRange(mBins.keys, found, sd);
    }

    QCPRange valueRange(int column, bool& found, QCP::SignDomain sd = QCP::sdBoth,
                        const QCPRange& inKeyRange = QCPRange()) const override
    {
        found = false;
        if (column < 0 || column >= mBins.numColumns) return {};
        int s = mBins.stride();
        const bool filterByKey = (inKeyRange.lower != 0 || inKeyRange.upper != 0);
        double lo = std::numeric_limits<double>::max();
        double hi = std::numeric_limits<double>::lowest();
        for (int i = 0; i < s; ++i)
        {
            if (filterByKey && !inKeyRange.contains(mBins.keys[i])) continue;
            double v = mBins.values[column * s + i];
            if (std::isnan(v)) continue;
            if (sd == QCP::sdPositive && v <= 0) continue;
            if (sd == QCP::sdNegative && v >= 0) continue;
            if (v < lo) lo = v;
            if (v > hi) hi = v;
            found = true;
        }
        return found ? QCPRange(lo, hi) : QCPRange();
    }

    int findBegin(double sortKey, bool expandedRange = true) const override
    {
        return qcp::algo::findBegin(mBins.keys, sortKey, expandedRange);
    }

    int findEnd(double sortKey, bool expandedRange = true) const override
    {
        return qcp::algo::findEnd(mBins.keys, sortKey, expandedRange);
    }

    QVector<QPointF> getOptimizedLineData(int column, int begin, int end, int pixelWidth,
                                           QCPAxis* keyAxis, QCPAxis* valueAxis) const override
    {
        if (column < 0 || column >= mBins.numColumns) return {};
        ensureGapCache(begin, end);
        const int s = mBins.stride();
        return qcp::algo::optimizedLineData(
            mBins.keys, std::span<const double>(mBins.values.data() + column * s, s),
            begin, end, pixelWidth, keyAxis, valueAxis, &mGapCache.gaps);
    }

    QVector<QPointF> getLines(int column, int begin, int end,
                               QCPAxis* keyAxis, QCPAxis* valueAxis) const override
    {
        if (column < 0 || column >= mBins.numColumns) return {};
        int s = mBins.stride();
        const bool keyIsVertical = keyAxis->orientation() == Qt::Vertical;
        const int count = end - begin;
        ensureGapCache(begin, end);
        const auto nanPt = QPointF(qQNaN(), qQNaN());
        const auto keyTf = qcp::algo::AffineTransform::fromAxis(keyAxis);
        const auto valTf = qcp::algo::AffineTransform::fromAxis(valueAxis);
        const bool bothLinear = keyTf.isLinear && valTf.isLinear;

        QVector<QPointF> lines;
        lines.reserve(count + count / 10);
        for (int i = begin; i < end; ++i)
        {
            if (mGapCache.gaps.hasAnyGap && mGapCache.gaps[i - begin])
                lines.append(nanPt);
            double v = mBins.values[column * s + i];
            if (std::isnan(v)) continue;
            double k = mBins.keys[i];
            if (bothLinear)
            {
                double kp = keyTf.toPixel(k);
                double vp = valTf.toPixel(v);
                lines.append(keyIsVertical ? QPointF(vp, kp) : QPointF(kp, vp));
            }
            else
            {
                double keyPx = keyAxis->coordToPixel(k);
                double valPx = valueAxis->coordToPixel(v);
                lines.append(keyIsVertical ? QPointF(valPx, keyPx) : QPointF(keyPx, valPx));
            }
        }
        return lines;
    }

    void getOptimizedLineDataAll(int begin, int end, int /*pixelWidth*/,
                                  QCPAxis* keyAxis, QCPAxis* valueAxis,
                                  QVector<QPointF>* results, int numColumns) const override
    {
        getLinesAll(begin, end, keyAxis, valueAxis, results, numColumns);
    }

    void getLinesAll(int begin, int end,
                     QCPAxis* keyAxis, QCPAxis* valueAxis,
                     QVector<QPointF>* results, int numColumns) const override
    {
        ensureGapCache(begin, end);
        const int N = std::min(numColumns, mBins.numColumns);
        const int s = mBins.stride();
        const bool keyIsVertical = keyAxis->orientation() == Qt::Vertical;
        const int count = end - begin;
        const auto nanPt = QPointF(qQNaN(), qQNaN());
        const auto keyTf = qcp::algo::AffineTransform::fromAxis(keyAxis);
        const auto valTf = qcp::algo::AffineTransform::fromAxis(valueAxis);
        const bool bothLinear = keyTf.isLinear && valTf.isLinear;

        for (int c = 0; c < N; ++c) { results[c].clear(); results[c].reserve(count + count / 10); }

        for (int i = begin; i < end; ++i)
        {
            bool isGap = mGapCache.gaps.hasAnyGap && mGapCache.gaps[i - begin];
            double k = mBins.keys[i];
            double kp = bothLinear ? keyTf.toPixel(k) : keyAxis->coordToPixel(k);

            for (int c = 0; c < N; ++c)
            {
                if (isGap) results[c].append(nanPt);
                double v = mBins.values[c * s + i];
                if (std::isnan(v)) continue;
                double vp = bothLinear ? valTf.toPixel(v) : valueAxis->coordToPixel(v);
                results[c].append(keyIsVertical ? QPointF(vp, kp) : QPointF(kp, vp));
            }
        }
    }

    QVector<QPointF> getLinesIndexed(int column, int begin, int end,
                                     QCPAxis* keyAxis, QCPAxis* valueAxis,
                                     QVector<int>& sourceIndices) const override
    {
        sourceIndices.clear();
        if (column < 0 || column >= mBins.numColumns) return {};
        const int s = mBins.stride();
        const bool keyIsVertical = keyAxis->orientation() == Qt::Vertical;
        ensureGapCache(begin, end);
        const auto nanPt = QPointF(qQNaN(), qQNaN());
        const bool hasOrigin = !mBins.origin.empty();

        QVector<QPointF> lines;
        lines.reserve(end - begin + (end - begin) / 10);
        sourceIndices.reserve(lines.capacity());
        for (int i = begin; i < end; ++i)
        {
            if (mGapCache.gaps.hasAnyGap && mGapCache.gaps[i - begin])
            {
                lines.append(nanPt);
                sourceIndices.append(-1);
            }
            const double v = mBins.values[column * s + i];
            if (std::isnan(v)) continue;
            const double kp = keyAxis->coordToPixel(mBins.keys[i]);
            const double vp = valueAxis->coordToPixel(v);
            lines.append(keyIsVertical ? QPointF(vp, kp) : QPointF(kp, vp));
            sourceIndices.append(hasOrigin ? mBins.origin[column * s + i] : -1);
        }
        return lines;
    }

    QVector<QPointF> getOptimizedLineDataIndexed(int column, int begin, int end, int /*pixelWidth*/,
                                                 QCPAxis* keyAxis, QCPAxis* valueAxis,
                                                 QVector<int>& sourceIndices) const override
    {
        return getLinesIndexed(column, begin, end, keyAxis, valueAxis, sourceIndices);
    }

    const double* rawKeyData() const override { return mBins.keys.data(); }
    const double* rawColumnData(int column) const override
    {
        if (column < 0 || column >= mBins.numColumns) return nullptr;
        return mBins.values.data() + column * mBins.stride();
    }

private:
    void ensureGapCache(int begin, int end) const
    {
        if (mGapCache.begin != begin || mGapCache.end != end)
        {
            mGapCache.begin = begin;
            mGapCache.end = end;
            mGapCache.gaps = qcp::algo::detectKeyGaps(mBins.keys, begin, end);
        }
    }

    qcp::algo::MultiColumnBinResult mBins;
    mutable struct { int begin = -1; int end = -1; qcp::algo::GapVector gaps; } mGapCache;
};

namespace qcp::algo {

namespace detail {

template <bool WithOrigin>
inline std::shared_ptr<QCPResampledMultiDataSource> resampleL2MultiImpl(
    const MultiGraphResamplerCache& l1Cache,
    const ViewportParams& vp)
{
    PROFILE_HERE_N("resampleL2Multi");
    if (vp.keyLogScale)
        return nullptr;

    int l2Bins = vp.plotWidthPx * kLevel2PixelMultiplier;
    if (l2Bins <= 0) l2Bins = 3200;

    const auto& l1 = l1Cache.level1;
    int l1Size = static_cast<int>(l1.keys.size());
    if (l1Size == 0 || l1.numColumns == 0) return nullptr;

    auto [l1Begin, l1End] = l1ViewportBounds(l1.keys, l1Size, vp.keyRange);
    if (l1End <= l1Begin)
        return nullptr;

    // Skip L2 binning when visible points are sparse enough to draw directly
    if (l1End - l1Begin <= l2Bins)
        return nullptr;

    int N = l1.numColumns;
    int l1Stride = l1.stride();

    MultiColumnBinResult l2;
    l2.numColumns = N;

    const double binWidth = vp.keyRange.size() / l2Bins;
    const double halfWidth = binWidth * 0.5;
    const double keyLo = vp.keyRange.lower;
    const double invBinWidth = 1.0 / binWidth;
    int l2Stride = l2Bins * 2;

    l2.values.resize(N * l2Stride);
    [[maybe_unused]] std::vector<int> minRow, maxRow;
    if constexpr (WithOrigin) { minRow.assign(N * l2Bins, -1); maxRow.assign(N * l2Bins, -1); }

    constexpr double posInf = std::numeric_limits<double>::infinity();
    constexpr double negInf = -std::numeric_limits<double>::infinity();
    for (int c = 0; c < N; ++c)
    {
        double* col = l2.values.data() + c * l2Stride;
        for (int b = 0; b < l2Bins; ++b)
        {
            col[b * 2 + 0] = posInf;   // min slot
            col[b * 2 + 1] = negInf;   // max slot
        }
    }

    // Bitset to track which bins received data — avoids scanning all bins during compaction
    std::vector<bool> binHasData(l2Bins, false);

    // Single pass over L1 points: compute bin index once, scatter into all columns
    int l1Count = l1End - l1Begin;
    for (int i = 0; i < l1Count; ++i)
    {
        double k = l1.keys[l1Begin + i];
        int bin = std::clamp(static_cast<int>((k - keyLo) * invBinWidth), 0, l2Bins - 1);
        int slot = bin * 2;

        bool anyValid = false;
        const int row = l1Begin + i;
        for (int c = 0; c < N; ++c)
        {
            double v = l1.values[c * l1Stride + row];
            if (v != v) continue;  // NaN

            double* colOut = l2.values.data() + c * l2Stride;
            if (v < colOut[slot])     { colOut[slot] = v;     if constexpr (WithOrigin) minRow[c * l2Bins + bin] = row; }
            if (v > colOut[slot + 1]) { colOut[slot + 1] = v; if constexpr (WithOrigin) maxRow[c * l2Bins + bin] = row; }
            anyValid = true;
        }
        if (anyValid)
            binHasData[bin] = true;
    }

    if constexpr (WithOrigin) l2.origin.resize(N * l2Stride);

    // Compact: emit only populated bins, guided by the bitset
    int outSize = 0;
    constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
    // Allocate keys now — we know at most l2Stride entries
    l2.keys.resize(l2Stride);
    for (int b = 0; b < l2Bins; ++b)
    {
        if (!binHasData[b]) continue;

        double binCenter = keyLo + (b + 0.5) * binWidth;
        int srcSlot = b * 2;
        l2.keys[outSize] = binCenter;
        l2.keys[outSize + 1] = binCenter + halfWidth;
        for (int c = 0; c < N; ++c)
        {
            double* colOut = l2.values.data() + c * l2Stride;
            double mn = colOut[srcSlot];
            double mx = colOut[srcSlot + 1];
            // Convert remaining sentinels to NaN for columns with no data in this bin
            colOut[outSize] = (mn == posInf) ? NaN : mn;
            colOut[outSize + 1] = (mx == negInf) ? NaN : mx;
            if constexpr (WithOrigin)
            {
                const int mnRow = minRow[c * l2Bins + b], mxRow = maxRow[c * l2Bins + b];
                int* orgOut = l2.origin.data() + c * l2Stride;
                orgOut[outSize]     = mnRow < 0 ? -1 : l1.origin[c * l1Stride + mnRow];
                orgOut[outSize + 1] = mxRow < 0 ? -1 : l1.origin[c * l1Stride + mxRow];
            }
        }
        outSize += 2;
    }
    if (outSize == 0) return nullptr;

    l2.keys.resize(outSize);
    // Compact column data: shift each column's data to final stride.
    for (int c = 1; c < N; ++c)
        for (int i = 0; i < outSize; ++i)
        {
            l2.values[c * outSize + i] = l2.values[c * l2Stride + i];
            if constexpr (WithOrigin) l2.origin[c * outSize + i] = l2.origin[c * l2Stride + i];
        }
    l2.values.resize(N * outSize);
    if constexpr (WithOrigin) l2.origin.resize(N * outSize);

    return std::make_shared<QCPResampledMultiDataSource>(std::move(l2));
}

} // namespace detail

inline std::shared_ptr<QCPResampledMultiDataSource> resampleL2Multi(
    const MultiGraphResamplerCache& l1Cache,
    const ViewportParams& vp)
{
    return l1Cache.level1.origin.empty() ? detail::resampleL2MultiImpl<false>(l1Cache, vp)
                                         : detail::resampleL2MultiImpl<true>(l1Cache, vp);
}

} // namespace qcp::algo
