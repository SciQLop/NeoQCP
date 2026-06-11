#pragma once
#include "abstract-datasource.h"
#include "axis/axis.h"
#include "layoutelements/layoutelement-axisrect.h"
#include "../Profiling.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <QtGlobal>

namespace qcp::algo {

constexpr double kDefaultGapThreshold = 1.5;

struct GapVector {
    std::vector<uint8_t> data;
    bool hasAnyGap = false;

    explicit GapVector(int count = 0) : data(count, 0) {}
    uint8_t operator[](int i) const { return data[i]; }
    void setGap(int i) { data[i] = 1; hasAnyGap = true; }
    int size() const { return static_cast<int>(data.size()); }
};

template <IndexableNumericRange KC>
GapVector detectKeyGaps(const KC& keys, int begin, int end,
                                 double threshold = kDefaultGapThreshold)
{
    const int count = end - begin;
    GapVector gapBefore(count);
    if (count < 3 || threshold <= 0) return gapBefore;

    for (int i = 0; i < count - 1; ++i)
    {
        double dx = static_cast<double>(keys[begin + i + 1]) - static_cast<double>(keys[begin + i]);
        double refDx = std::numeric_limits<double>::max();
        if (i > 0)
            refDx = std::min(refDx, static_cast<double>(keys[begin + i]) - static_cast<double>(keys[begin + i - 1]));
        if (i + 2 < count)
            refDx = std::min(refDx, static_cast<double>(keys[begin + i + 2]) - static_cast<double>(keys[begin + i + 1]));
        if (refDx < std::numeric_limits<double>::max() && dx > threshold * refDx)
            gapBefore.setGap(i + 1);
    }
    return gapBefore;
}

template <IndexableNumericRange KC>
int findBegin(const KC& keys, double sortKey, bool expandedRange = true)
{
    const int sz = static_cast<int>(std::ranges::size(keys));
    if (sz == 0) return 0;

    auto it = std::lower_bound(std::ranges::begin(keys), std::ranges::end(keys),
                                sortKey, [](const auto& elem, double sk) {
                                    return static_cast<double>(elem) < sk;
                                });
    int idx = static_cast<int>(it - std::ranges::begin(keys));
    if (expandedRange && idx > 0)
        --idx;
    return idx;
}

template <IndexableNumericRange KC>
int findEnd(const KC& keys, double sortKey, bool expandedRange = true)
{
    const int sz = static_cast<int>(std::ranges::size(keys));
    if (sz == 0) return 0;

    auto it = std::upper_bound(std::ranges::begin(keys), std::ranges::end(keys),
                                sortKey, [](double sk, const auto& elem) {
                                    return sk < static_cast<double>(elem);
                                });
    int idx = static_cast<int>(it - std::ranges::begin(keys));
    if (expandedRange && idx < sz)
        ++idx;
    return idx;
}

template <IndexableNumericRange KC>
QCPRange keyRange(const KC& keys, bool& foundRange, QCP::SignDomain sd = QCP::sdBoth)
{
    foundRange = false;
    const int sz = static_cast<int>(std::ranges::size(keys));
    if (sz == 0) return {};

    double lower = std::numeric_limits<double>::max();
    double upper = std::numeric_limits<double>::lowest();
    for (int i = 0; i < sz; ++i)
    {
        double k = static_cast<double>(keys[i]);
        if (sd == QCP::sdPositive && k <= 0) continue;
        if (sd == QCP::sdNegative && k >= 0) continue;
        if (k < lower) lower = k;
        if (k > upper) upper = k;
        foundRange = true;
    }
    return foundRange ? QCPRange(lower, upper) : QCPRange();
}

// O(1) key range for sorted data (sdBoth), O(log n) for sign-domain filtering.
template <IndexableNumericRange KC>
QCPRange keyRangeSorted(const KC& keys, bool& foundRange, QCP::SignDomain sd = QCP::sdBoth)
{
    foundRange = false;
    const int sz = static_cast<int>(std::ranges::size(keys));
    if (sz == 0) return {};

    if (sd == QCP::sdBoth)
    {
        foundRange = true;
        return QCPRange(static_cast<double>(keys[0]),
                        static_cast<double>(keys[sz - 1]));
    }

    // For sdPositive/sdNegative, binary search for the boundary
    if (sd == QCP::sdPositive)
    {
        auto it = std::upper_bound(std::ranges::begin(keys), std::ranges::end(keys),
                                    0.0, [](double val, const auto& elem) {
                                        return val < static_cast<double>(elem);
                                    });
        if (it == std::ranges::end(keys)) return {};
        foundRange = true;
        return QCPRange(static_cast<double>(*it),
                        static_cast<double>(keys[sz - 1]));
    }
    else // sdNegative
    {
        auto it = std::lower_bound(std::ranges::begin(keys), std::ranges::end(keys),
                                    0.0, [](const auto& elem, double val) {
                                        return static_cast<double>(elem) < val;
                                    });
        if (it == std::ranges::begin(keys)) return {};
        --it;
        foundRange = true;
        return QCPRange(static_cast<double>(keys[0]),
                        static_cast<double>(*it));
    }
}

template <IndexableNumericRange KC, IndexableNumericRange VC>
QCPRange valueRange(const KC& keys, const VC& values, bool& foundRange,
                    QCP::SignDomain sd = QCP::sdBoth,
                    const QCPRange& inKeyRange = QCPRange())
{
    foundRange = false;
    const int sz = static_cast<int>(std::ranges::size(values));
    if (sz == 0) return {};

    const bool hasKeyRestriction = inKeyRange.lower != inKeyRange.upper
                                    || inKeyRange.lower != 0.0;

    // Keys are sorted (data source contract): restrict the scan to the visible
    // window via binary search instead of testing every key. Callers with
    // unsorted keys (histogram scatter) must not pass a key restriction here.
    int i0 = 0;
    int i1 = sz;
    if (hasKeyRestriction)
    {
        i0 = findBegin(keys, inKeyRange.lower, false);
        i1 = findEnd(keys, inKeyRange.upper, false);
    }

    double lower = std::numeric_limits<double>::max();
    double upper = std::numeric_limits<double>::lowest();
    for (int i = i0; i < i1; ++i)
    {
        double v = static_cast<double>(values[i]);
        if (sd == QCP::sdPositive && v <= 0) continue;
        if (sd == QCP::sdNegative && v >= 0) continue;
        if (v < lower) lower = v;
        if (v > upper) upper = v;
        foundRange = true;
    }
    return foundRange ? QCPRange(lower, upper) : QCPRange();
}

// Pre-computed affine transform for coord<->pixel conversion.
// For linear axes: pixel = coord * scale + offset
struct AffineTransform {
    double scale;
    double offset;
    double invScale;
    double invOffset;
    bool isLinear;

    static AffineTransform fromAxis(const QCPAxis* axis)
    {
        AffineTransform t;
        t.isLinear = axis->scaleType() == QCPAxis::stLinear;
        if (!t.isLinear) { t.scale = t.offset = t.invScale = t.invOffset = 0; return t; }

        const auto range = axis->range();
        const double rangeSize = range.size();
        if (axis->orientation() == Qt::Horizontal)
        {
            double width = axis->axisRect()->width();
            if (!axis->rangeReversed())
            {
                t.scale = width / rangeSize;
                t.offset = axis->axisRect()->left() - range.lower * t.scale;
            }
            else
            {
                t.scale = -width / rangeSize;
                t.offset = axis->axisRect()->left() + range.upper * (-t.scale);
            }
        }
        else
        {
            double height = axis->axisRect()->height();
            if (!axis->rangeReversed())
            {
                t.scale = -height / rangeSize;
                t.offset = axis->axisRect()->bottom() - range.lower * t.scale;
            }
            else
            {
                t.scale = height / rangeSize;
                t.offset = axis->axisRect()->bottom() - range.upper * t.scale;
            }
        }
        t.invScale = 1.0 / t.scale;
        t.invOffset = -t.offset * t.invScale;
        return t;
    }

    double toPixel(double coord) const { return coord * scale + offset; }
    double toCoord(double pixel) const { return pixel * invScale + invOffset; }
};

template <IndexableNumericRange KC, IndexableNumericRange VC>
QVector<QPointF> linesToPixels(const KC& keys, const VC& values,
                                int begin, int end,
                                QCPAxis* keyAxis, QCPAxis* valueAxis,
                                double gapThreshold = kDefaultGapThreshold,
                                const GapVector* precomputedGaps = nullptr)
{
    using V = std::ranges::range_value_t<VC>;
    Q_ASSERT(begin >= 0 && end <= static_cast<int>(std::ranges::size(keys)));
    Q_ASSERT(begin >= 0 && end <= static_cast<int>(std::ranges::size(values)));
    const int count = end - begin;
    if (count <= 0) return {};

    GapVector computedGaps;
    if (!precomputedGaps)
        computedGaps = detectKeyGaps(keys, begin, end, gapThreshold);
    const auto& gaps = precomputedGaps ? *precomputedGaps : computedGaps;

    QVector<QPointF> result;
    result.reserve(count + count / 10);

    const bool isVertical = keyAxis->orientation() == Qt::Vertical;
    const auto nanPt = QPointF(qQNaN(), qQNaN());

    const auto keyTf = AffineTransform::fromAxis(keyAxis);
    const auto valTf = AffineTransform::fromAxis(valueAxis);
    const bool bothLinear = keyTf.isLinear && valTf.isLinear;

    for (int i = begin; i < end; ++i)
    {
        int ri = i - begin;
        if (gaps.hasAnyGap && gaps[ri])
            result.append(nanPt);

        double v = static_cast<double>(values[i]);
        if constexpr (!std::is_integral_v<V>)
        {
            if (std::isnan(v)) { result.append(nanPt); continue; }
        }

        double k = static_cast<double>(keys[i]);
        if (bothLinear)
        {
            double kp = keyTf.toPixel(k);
            double vp = valTf.toPixel(v);
            result.append(isVertical ? QPointF(vp, kp) : QPointF(kp, vp));
        }
        else
        {
            if (isVertical)
                result.append(QPointF(valueAxis->coordToPixel(v),
                                      keyAxis->coordToPixel(k)));
            else
                result.append(QPointF(keyAxis->coordToPixel(k),
                                      valueAxis->coordToPixel(v)));
        }
    }
    return result;
}

template <IndexableNumericRange KC, IndexableNumericRange VC>
QVector<QPointF> optimizedLineData(const KC& keys, const VC& values,
                                    int begin, int end,
                                    int /*pixelWidth*/,
                                    QCPAxis* keyAxis, QCPAxis* valueAxis,
                                    const GapVector* precomputedGaps = nullptr)
{
    PROFILE_HERE_N("optimizedLineData");
    Q_ASSERT(begin >= 0 && end <= static_cast<int>(std::ranges::size(keys)));
    Q_ASSERT(begin >= 0 && end <= static_cast<int>(std::ranges::size(values)));
    const int dataCount = end - begin;
    if (dataCount <= 0) return {};

    double keyPixelSpan = qAbs(keyAxis->coordToPixel(static_cast<double>(keys[begin]))
                                - keyAxis->coordToPixel(static_cast<double>(keys[end - 1])));
    int maxCount = (std::numeric_limits<int>::max)();
    if (2 * keyPixelSpan + 2 < static_cast<double>((std::numeric_limits<int>::max)()))
        maxCount = int(2 * keyPixelSpan + 2);

    if (dataCount < maxCount)
        return linesToPixels(keys, values, begin, end, keyAxis, valueAxis,
                             kDefaultGapThreshold, precomputedGaps);

    GapVector computedGaps;
    if (!precomputedGaps)
        computedGaps = detectKeyGaps(keys, begin, end);
    const auto& gaps = precomputedGaps ? *precomputedGaps : computedGaps;
    const auto nanPt = QPointF(qQNaN(), qQNaN());

    QVector<QPointF> result;
    result.reserve(maxCount);

    const bool isVertical = keyAxis->orientation() == Qt::Vertical;
    const auto keyTf = AffineTransform::fromAxis(keyAxis);
    const auto valTf = AffineTransform::fromAxis(valueAxis);
    const bool bothLinear = keyTf.isLinear && valTf.isLinear;

    auto toPixel = [&](double k, double v) -> QPointF {
        if (bothLinear)
        {
            double kp = keyTf.toPixel(k);
            double vp = valTf.toPixel(v);
            return isVertical ? QPointF(vp, kp) : QPointF(kp, vp);
        }
        return isVertical ? QPointF(valueAxis->coordToPixel(v), keyAxis->coordToPixel(k))
                          : QPointF(keyAxis->coordToPixel(k), valueAxis->coordToPixel(v));
    };

    auto flushInterval = [&](int intervalFirst, int intervalCount,
                              double intervalStartKey, double lastEndKey,
                              double minVal, double maxVal,
                              double epsilon, double nextKey) {
        if (intervalCount >= 2)
        {
            double firstVal = static_cast<double>(values[intervalFirst]);
            if (lastEndKey < intervalStartKey - epsilon)
                result.append(toPixel(intervalStartKey + epsilon * 0.2, firstVal));
            result.append(toPixel(intervalStartKey + epsilon * 0.25, minVal));
            result.append(toPixel(intervalStartKey + epsilon * 0.75, maxVal));
            if (nextKey > intervalStartKey + epsilon * 2)
            {
                int prev = intervalFirst + intervalCount - 1;
                result.append(toPixel(intervalStartKey + epsilon * 0.8,
                                       static_cast<double>(values[prev])));
            }
        }
        else
        {
            result.append(toPixel(static_cast<double>(keys[intervalFirst]),
                                   static_cast<double>(values[intervalFirst])));
        }
    };

    int i = begin;
    using V = std::ranges::range_value_t<VC>;
    if constexpr (!std::is_integral_v<V>)
    {
        while (i < end && std::isnan(static_cast<double>(values[i])))
            ++i;
    }
    if (i >= end) return {};

    double minValue = static_cast<double>(values[i]);
    double maxValue = minValue;
    int currentIntervalFirst = i;
    int reversedFactor = keyAxis->pixelOrientation();
    int reversedRound = reversedFactor == -1 ? 1 : 0;

    // For linear axes, pre-compute the constant key-space epsilon and use inline
    // affine transform for boundary updates (no function calls). For log axes,
    // fall back to axis method calls.
    const bool useInlineTransform = keyTf.isLinear;
    double currentIntervalStartKey = 0;
    double lastIntervalEndKey = 0;
    double keyEpsilon = 0;
    double nextBoundary = 0;  // currentIntervalStartKey + keyEpsilon, cached
    bool keyEpsilonVariable = false;

    if (useInlineTransform)
    {
        double kp = keyTf.toPixel(static_cast<double>(keys[i]));
        int pixel = static_cast<int>(kp) + reversedRound;
        currentIntervalStartKey = keyTf.toCoord(pixel);
        lastIntervalEndKey = currentIntervalStartKey;
        keyEpsilon = qAbs(keyTf.invScale);
        nextBoundary = currentIntervalStartKey + keyEpsilon;
    }
    else
    {
        currentIntervalStartKey = keyAxis->pixelToCoord(
            int(keyAxis->coordToPixel(static_cast<double>(keys[i])) + reversedRound));
        lastIntervalEndKey = currentIntervalStartKey;
        keyEpsilon = qAbs(currentIntervalStartKey
            - keyAxis->pixelToCoord(keyAxis->coordToPixel(currentIntervalStartKey)
                                    + 1.0 * reversedFactor));
        nextBoundary = currentIntervalStartKey + keyEpsilon;
        keyEpsilonVariable = keyAxis->scaleType() == QCPAxis::stLogarithmic;
    }

    int intervalDataCount = 1;
    ++i;

    const uint8_t* gapData = gaps.data.data();
    const bool hasAnyGap = gaps.hasAnyGap;

    while (i < end)
    {
        double v = static_cast<double>(values[i]);
        if constexpr (!std::is_integral_v<V>)
        {
            if (std::isnan(v)) { ++i; continue; }
        }

        if (hasAnyGap && gapData[i - begin])
        {
            double k = static_cast<double>(keys[i]);
            flushInterval(currentIntervalFirst, intervalDataCount,
                          currentIntervalStartKey, lastIntervalEndKey,
                          minValue, maxValue, keyEpsilon, k);
            result.append(nanPt);
            lastIntervalEndKey = currentIntervalStartKey;
            minValue = v;
            maxValue = v;
            currentIntervalFirst = i;
            if (useInlineTransform)
            {
                int pixel = static_cast<int>(keyTf.toPixel(k)) + reversedRound;
                currentIntervalStartKey = keyTf.toCoord(pixel);
            }
            else
            {
                currentIntervalStartKey = keyAxis->pixelToCoord(
                    int(keyAxis->coordToPixel(k) + reversedRound));
                if (keyEpsilonVariable)
                    keyEpsilon = qAbs(currentIntervalStartKey
                        - keyAxis->pixelToCoord(keyAxis->coordToPixel(currentIntervalStartKey)
                                                + 1.0 * reversedFactor));
            }
            nextBoundary = currentIntervalStartKey + keyEpsilon;
            intervalDataCount = 1;
            ++i;
            continue;
        }

        double k = static_cast<double>(keys[i]);
        if (k < nextBoundary)
        {
            minValue = std::min(minValue, v);
            maxValue = std::max(maxValue, v);
            ++intervalDataCount;
        }
        else
        {
            flushInterval(currentIntervalFirst, intervalDataCount,
                          currentIntervalStartKey, lastIntervalEndKey,
                          minValue, maxValue, keyEpsilon, k);
            lastIntervalEndKey = static_cast<double>(keys[i - 1]);
            minValue = v;
            maxValue = v;
            currentIntervalFirst = i;
            if (useInlineTransform)
            {
                int pixel = static_cast<int>(keyTf.toPixel(k)) + reversedRound;
                currentIntervalStartKey = keyTf.toCoord(pixel);
            }
            else
            {
                currentIntervalStartKey = keyAxis->pixelToCoord(
                    int(keyAxis->coordToPixel(k) + reversedRound));
                if (keyEpsilonVariable)
                    keyEpsilon = qAbs(currentIntervalStartKey
                        - keyAxis->pixelToCoord(keyAxis->coordToPixel(currentIntervalStartKey)
                                                + 1.0 * reversedFactor));
            }
            nextBoundary = currentIntervalStartKey + keyEpsilon;
            intervalDataCount = 1;
        }
        ++i;
    }
    flushInterval(currentIntervalFirst, intervalDataCount,
                  currentIntervalStartKey, lastIntervalEndKey,
                  minValue, maxValue, keyEpsilon,
                  currentIntervalStartKey + keyEpsilon * 3);

    return result;
}

// Multi-column optimizedLineData: processes all columns in a single pass over the key array.
// Reads keys once instead of N times, and shares pixel-boundary decisions across columns.
template <IndexableNumericRange KC>
void optimizedLineDataMulti(const KC& keys,
                             int numColumns,
                             // valueAt(column, dataIndex) -> double
                             auto&& valueAt,
                             int begin, int end,
                             QCPAxis* keyAxis, QCPAxis* valueAxis,
                             const GapVector* precomputedGaps,
                             QVector<QPointF>* results)
{
    PROFILE_HERE_N("optimizedLineDataMulti");
    Q_ASSERT(begin >= 0 && end <= static_cast<int>(std::ranges::size(keys)));
    const int dataCount = end - begin;
    if (dataCount <= 0)
    {
        for (int c = 0; c < numColumns; ++c) results[c].clear();
        return;
    }

    double keyPixelSpan = qAbs(keyAxis->coordToPixel(static_cast<double>(keys[begin]))
                                - keyAxis->coordToPixel(static_cast<double>(keys[end - 1])));
    int maxCount = (std::numeric_limits<int>::max)();
    if (2 * keyPixelSpan + 2 < static_cast<double>((std::numeric_limits<int>::max)()))
        maxCount = int(2 * keyPixelSpan + 2);

    GapVector computedGaps;
    if (!precomputedGaps)
        computedGaps = detectKeyGaps(keys, begin, end);
    const auto& gaps = precomputedGaps ? *precomputedGaps : computedGaps;

    if (dataCount < maxCount)
    {
        const auto nanPt = QPointF(qQNaN(), qQNaN());
        const bool isVertical = keyAxis->orientation() == Qt::Vertical;
        const auto keyTf = AffineTransform::fromAxis(keyAxis);
        const auto valTf = AffineTransform::fromAxis(valueAxis);
        const bool bothLinear = keyTf.isLinear && valTf.isLinear;

        for (int c = 0; c < numColumns; ++c) { results[c].clear(); results[c].reserve(dataCount + dataCount / 10); }

        for (int i = begin; i < end; ++i)
        {
            int ri = i - begin;
            bool isGap = gaps.hasAnyGap && gaps[ri];

            double k = static_cast<double>(keys[i]);
            double kp = bothLinear ? keyTf.toPixel(k) : keyAxis->coordToPixel(k);

            for (int c = 0; c < numColumns; ++c)
            {
                if (isGap) results[c].append(nanPt);

                double v = valueAt(c, i);
                if (std::isnan(v)) { results[c].append(nanPt); continue; }

                double vp = bothLinear ? valTf.toPixel(v) : valueAxis->coordToPixel(v);
                results[c].append(isVertical ? QPointF(vp, kp) : QPointF(kp, vp));
            }
        }
        return;
    }

    // Adaptive sampling: single pass over keys, per-column min/max tracking
    const auto nanPt = QPointF(qQNaN(), qQNaN());
    const bool isVertical = keyAxis->orientation() == Qt::Vertical;
    const auto keyTf = AffineTransform::fromAxis(keyAxis);
    const auto valTf = AffineTransform::fromAxis(valueAxis);
    const bool bothLinear = keyTf.isLinear && valTf.isLinear;

    auto toPixel = [&](double k, double v) -> QPointF {
        if (bothLinear)
        {
            double kp = keyTf.toPixel(k);
            double vp = valTf.toPixel(v);
            return isVertical ? QPointF(vp, kp) : QPointF(kp, vp);
        }
        return isVertical ? QPointF(valueAxis->coordToPixel(v), keyAxis->coordToPixel(k))
                          : QPointF(keyAxis->coordToPixel(k), valueAxis->coordToPixel(v));
    };

    for (int c = 0; c < numColumns; ++c) { results[c].clear(); results[c].reserve(maxCount); }

    // Per-column state
    struct ColState {
        double minVal;
        double maxVal;
        int intervalFirst;
        int intervalCount;
    };
    std::vector<ColState> cs(numColumns);

    int reversedFactor = keyAxis->pixelOrientation();
    int reversedRound = reversedFactor == -1 ? 1 : 0;

    const bool useInlineTransform = keyTf.isLinear;
    double currentIntervalStartKey = 0;
    double lastIntervalEndKey = 0;
    double keyEpsilon = 0;
    double nextBoundary = 0;
    bool keyEpsilonVariable = false;

    int i = begin;
    if (i >= end)
    {
        for (int c = 0; c < numColumns; ++c) results[c].clear();
        return;
    }

    if (useInlineTransform)
    {
        double kp = keyTf.toPixel(static_cast<double>(keys[i]));
        int pixel = static_cast<int>(kp) + reversedRound;
        currentIntervalStartKey = keyTf.toCoord(pixel);
        lastIntervalEndKey = currentIntervalStartKey;
        keyEpsilon = qAbs(keyTf.invScale);
        nextBoundary = currentIntervalStartKey + keyEpsilon;
    }
    else
    {
        currentIntervalStartKey = keyAxis->pixelToCoord(
            int(keyAxis->coordToPixel(static_cast<double>(keys[i])) + reversedRound));
        lastIntervalEndKey = currentIntervalStartKey;
        keyEpsilon = qAbs(currentIntervalStartKey
            - keyAxis->pixelToCoord(keyAxis->coordToPixel(currentIntervalStartKey)
                                    + 1.0 * reversedFactor));
        nextBoundary = currentIntervalStartKey + keyEpsilon;
        keyEpsilonVariable = keyAxis->scaleType() == QCPAxis::stLogarithmic;
    }

    for (int c = 0; c < numColumns; ++c)
    {
        double v = valueAt(c, i);
        cs[c] = {v, v, i, 1};
    }

    auto flushColumn = [&](int c, const ColState& s,
                            double intervalStartKey, double lastEndKey,
                            double epsilon, double nextKey) {
        if (s.intervalCount >= 2)
        {
            double firstVal = valueAt(c, s.intervalFirst);
            if (!std::isnan(firstVal))
            {
                if (lastEndKey < intervalStartKey - epsilon)
                    results[c].append(toPixel(intervalStartKey + epsilon * 0.2, firstVal));
            }
            if (!std::isnan(s.minVal))
                results[c].append(toPixel(intervalStartKey + epsilon * 0.25, s.minVal));
            if (!std::isnan(s.maxVal))
                results[c].append(toPixel(intervalStartKey + epsilon * 0.75, s.maxVal));
            if (nextKey > intervalStartKey + epsilon * 2)
            {
                int prev = s.intervalFirst + s.intervalCount - 1;
                double lastVal = valueAt(c, prev);
                if (!std::isnan(lastVal))
                    results[c].append(toPixel(intervalStartKey + epsilon * 0.8, lastVal));
            }
        }
        else
        {
            double val = valueAt(c, s.intervalFirst);
            if (!std::isnan(val))
                results[c].append(toPixel(static_cast<double>(keys[s.intervalFirst]), val));
            else
                results[c].append(nanPt);
        }
    };

    const uint8_t* gapData = gaps.data.data();
    const bool hasAnyGap = gaps.hasAnyGap;

    ++i;
    while (i < end)
    {
        if (hasAnyGap && gapData[i - begin])
        {
            double k = static_cast<double>(keys[i]);
            for (int c = 0; c < numColumns; ++c)
            {
                flushColumn(c, cs[c], currentIntervalStartKey, lastIntervalEndKey,
                            keyEpsilon, k);
                results[c].append(nanPt);
                double v = valueAt(c, i);
                cs[c] = {v, v, i, 1};
            }
            lastIntervalEndKey = currentIntervalStartKey;
            if (useInlineTransform)
            {
                int pixel = static_cast<int>(keyTf.toPixel(k)) + reversedRound;
                currentIntervalStartKey = keyTf.toCoord(pixel);
            }
            else
            {
                currentIntervalStartKey = keyAxis->pixelToCoord(
                    int(keyAxis->coordToPixel(k) + reversedRound));
                if (keyEpsilonVariable)
                    keyEpsilon = qAbs(currentIntervalStartKey
                        - keyAxis->pixelToCoord(keyAxis->coordToPixel(currentIntervalStartKey)
                                                + 1.0 * reversedFactor));
            }
            nextBoundary = currentIntervalStartKey + keyEpsilon;
            ++i;
            continue;
        }

        double k = static_cast<double>(keys[i]);
        if (k < nextBoundary)
        {
            for (int c = 0; c < numColumns; ++c)
            {
                double v = valueAt(c, i);
                if (!std::isnan(v))
                {
                    cs[c].minVal = std::min(cs[c].minVal, v);
                    cs[c].maxVal = std::max(cs[c].maxVal, v);
                }
                ++cs[c].intervalCount;
            }
        }
        else
        {
            for (int c = 0; c < numColumns; ++c)
            {
                flushColumn(c, cs[c], currentIntervalStartKey, lastIntervalEndKey,
                            keyEpsilon, k);
                double v = valueAt(c, i);
                cs[c] = {v, v, i, 1};
            }
            lastIntervalEndKey = static_cast<double>(keys[i - 1]);
            if (useInlineTransform)
            {
                int pixel = static_cast<int>(keyTf.toPixel(k)) + reversedRound;
                currentIntervalStartKey = keyTf.toCoord(pixel);
            }
            else
            {
                currentIntervalStartKey = keyAxis->pixelToCoord(
                    int(keyAxis->coordToPixel(k) + reversedRound));
                if (keyEpsilonVariable)
                    keyEpsilon = qAbs(currentIntervalStartKey
                        - keyAxis->pixelToCoord(keyAxis->coordToPixel(currentIntervalStartKey)
                                                + 1.0 * reversedFactor));
            }
            nextBoundary = currentIntervalStartKey + keyEpsilon;
        }
        ++i;
    }

    double finalNextKey = currentIntervalStartKey + keyEpsilon * 3;
    for (int c = 0; c < numColumns; ++c)
    {
        flushColumn(c, cs[c], currentIntervalStartKey, lastIntervalEndKey,
                    keyEpsilon, finalNextKey);
    }
}

} // namespace qcp::algo
