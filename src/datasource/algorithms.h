#pragma once
#include "abstract-datasource.h"
#include "axis/axis.h"
#include "../Profiling.hpp"
#include <algorithm>
#include <cmath>
#include <QtGlobal>

namespace qcp::algo {

constexpr double kDefaultGapThreshold = 1.5;

// Detect gaps between consecutive sorted keys using local neighbor spacing.
// Returns a bool vector where gapBefore[i] = true means there's a gap between
// keys[begin+i-1] and keys[begin+i]. Same algorithm as QCPColorMap2 gap detection.
template <IndexableNumericRange KC>
std::vector<bool> detectKeyGaps(const KC& keys, int begin, int end,
                                 double threshold = kDefaultGapThreshold)
{
    const int count = end - begin;
    std::vector<bool> gapBefore(count, false);
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
            gapBefore[i + 1] = true; // gap before point i+1
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

    double lower = std::numeric_limits<double>::max();
    double upper = std::numeric_limits<double>::lowest();
    for (int i = 0; i < sz; ++i)
    {
        if (hasKeyRestriction)
        {
            double k = static_cast<double>(keys[i]);
            if (k < inKeyRange.lower || k > inKeyRange.upper)
                continue;
        }
        double v = static_cast<double>(values[i]);
        if (sd == QCP::sdPositive && v <= 0) continue;
        if (sd == QCP::sdNegative && v >= 0) continue;
        if (v < lower) lower = v;
        if (v > upper) upper = v;
        foundRange = true;
    }
    return foundRange ? QCPRange(lower, upper) : QCPRange();
}

template <IndexableNumericRange KC, IndexableNumericRange VC>
QVector<QPointF> linesToPixels(const KC& keys, const VC& values,
                                int begin, int end,
                                QCPAxis* keyAxis, QCPAxis* valueAxis,
                                double gapThreshold = kDefaultGapThreshold,
                                const std::vector<bool>* precomputedGaps = nullptr)
{
    using V = std::ranges::range_value_t<VC>;
    Q_ASSERT(begin >= 0 && end <= static_cast<int>(std::ranges::size(keys)));
    Q_ASSERT(begin >= 0 && end <= static_cast<int>(std::ranges::size(values)));
    const int count = end - begin;
    if (count <= 0) return {};

    std::vector<bool> computedGaps;
    if (!precomputedGaps)
        computedGaps = detectKeyGaps(keys, begin, end, gapThreshold);
    const auto& gaps = precomputedGaps ? *precomputedGaps : computedGaps;

    QVector<QPointF> result;
    result.reserve(count + count / 10); // extra room for gap markers

    const bool isVertical = keyAxis->orientation() == Qt::Vertical;
    const auto nanPt = QPointF(qQNaN(), qQNaN());

    for (int i = begin; i < end; ++i)
    {
        int ri = i - begin;
        if (gaps[ri])
            result.append(nanPt);

        double v = static_cast<double>(values[i]);
        if constexpr (!std::is_integral_v<V>)
        {
            if (std::isnan(v)) { result.append(nanPt); continue; }
        }

        if (isVertical)
            result.append(QPointF(valueAxis->coordToPixel(v),
                                  keyAxis->coordToPixel(static_cast<double>(keys[i]))));
        else
            result.append(QPointF(keyAxis->coordToPixel(static_cast<double>(keys[i])),
                                  valueAxis->coordToPixel(v)));
    }
    return result;
}

template <IndexableNumericRange KC, IndexableNumericRange VC>
QVector<QPointF> optimizedLineData(const KC& keys, const VC& values,
                                    int begin, int end,
                                    int /*pixelWidth*/,
                                    QCPAxis* keyAxis, QCPAxis* valueAxis,
                                    const std::vector<bool>* precomputedGaps = nullptr)
{
    PROFILE_HERE_N("optimizedLineData");
    Q_ASSERT(begin >= 0 && end <= static_cast<int>(std::ranges::size(keys)));
    Q_ASSERT(begin >= 0 && end <= static_cast<int>(std::ranges::size(values)));
    const int dataCount = end - begin;
    if (dataCount <= 0) return {};

    // Determine if adaptive sampling should kick in
    double keyPixelSpan = qAbs(keyAxis->coordToPixel(static_cast<double>(keys[begin]))
                                - keyAxis->coordToPixel(static_cast<double>(keys[end - 1])));
    int maxCount = (std::numeric_limits<int>::max)();
    if (2 * keyPixelSpan + 2 < static_cast<double>((std::numeric_limits<int>::max)()))
        maxCount = int(2 * keyPixelSpan + 2);

    if (dataCount < maxCount)
        return linesToPixels(keys, values, begin, end, keyAxis, valueAxis,
                             kDefaultGapThreshold, precomputedGaps);

    // Adaptive sampling: consolidate multiple data points per pixel into min/max clusters.
    std::vector<bool> computedGaps;
    if (!precomputedGaps)
        computedGaps = detectKeyGaps(keys, begin, end);
    const auto& gaps = precomputedGaps ? *precomputedGaps : computedGaps;
    const auto nanPt = QPointF(qQNaN(), qQNaN());

    QVector<QPointF> result;
    result.reserve(maxCount);

    const bool isVertical = keyAxis->orientation() == Qt::Vertical;
    auto toPixel = [&](double k, double v) -> QPointF {
        return isVertical ? QPointF(valueAxis->coordToPixel(v), keyAxis->coordToPixel(k))
                          : QPointF(keyAxis->coordToPixel(k), valueAxis->coordToPixel(v));
    };

    // Flush the current interval to result
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
                // Find last valid value before nextKey position
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

    // Skip leading NaN values
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
    double currentIntervalStartKey = keyAxis->pixelToCoord(
        int(keyAxis->coordToPixel(static_cast<double>(keys[i])) + reversedRound));
    double lastIntervalEndKey = currentIntervalStartKey;
    double keyEpsilon = qAbs(currentIntervalStartKey
        - keyAxis->pixelToCoord(keyAxis->coordToPixel(currentIntervalStartKey)
                                + 1.0 * reversedFactor));
    bool keyEpsilonVariable = keyAxis->scaleType() == QCPAxis::stLogarithmic;
    int intervalDataCount = 1;
    ++i;

    while (i < end)
    {
        double v = static_cast<double>(values[i]);
        if constexpr (!std::is_integral_v<V>)
        {
            if (std::isnan(v)) { ++i; continue; }
        }

        // Key gap: flush current interval and insert a break
        if (gaps[i - begin])
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
            currentIntervalStartKey = keyAxis->pixelToCoord(
                int(keyAxis->coordToPixel(k) + reversedRound));
            if (keyEpsilonVariable)
                keyEpsilon = qAbs(currentIntervalStartKey
                    - keyAxis->pixelToCoord(keyAxis->coordToPixel(currentIntervalStartKey)
                                            + 1.0 * reversedFactor));
            intervalDataCount = 1;
            ++i;
            continue;
        }

        double k = static_cast<double>(keys[i]);
        if (k < currentIntervalStartKey + keyEpsilon)
        {
            if (v < minValue) minValue = v;
            else if (v > maxValue) maxValue = v;
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
            currentIntervalStartKey = keyAxis->pixelToCoord(
                int(keyAxis->coordToPixel(k) + reversedRound));
            if (keyEpsilonVariable)
                keyEpsilon = qAbs(currentIntervalStartKey
                    - keyAxis->pixelToCoord(keyAxis->coordToPixel(currentIntervalStartKey)
                                            + 1.0 * reversedFactor));
            intervalDataCount = 1;
        }
        ++i;
    }
    // Handle last interval
    flushInterval(currentIntervalFirst, intervalDataCount,
                  currentIntervalStartKey, lastIntervalEndKey,
                  minValue, maxValue, keyEpsilon,
                  currentIntervalStartKey + keyEpsilon * 3);

    return result;
}

} // namespace qcp::algo
