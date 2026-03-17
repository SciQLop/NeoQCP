#pragma once
#include "abstract-datasource.h"
#include "axis/axis.h"
#include "../Profiling.hpp"
#include <algorithm>
#include <cmath>
#include <QtGlobal>

namespace qcp::algo {

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
                                QCPAxis* keyAxis, QCPAxis* valueAxis)
{
    using V = std::ranges::range_value_t<VC>;
    Q_ASSERT(begin >= 0 && end <= static_cast<int>(std::ranges::size(keys)));
    Q_ASSERT(begin >= 0 && end <= static_cast<int>(std::ranges::size(values)));
    QVector<QPointF> result;
    result.resize(end - begin);

    if (keyAxis->orientation() == Qt::Vertical)
    {
        for (int i = begin; i < end; ++i)
        {
            auto& pt = result[i - begin];
            double v = static_cast<double>(values[i]);
            if constexpr (!std::is_integral_v<V>)
            {
                // NaN values map to (0,0) — matches legacy QCPGraph behavior.
                // TODO: proper gap handling (break polyline) for NaN values.
                if (std::isnan(v)) { pt = QPointF(0, 0); continue; }
            }
            pt.setX(valueAxis->coordToPixel(v));
            pt.setY(keyAxis->coordToPixel(static_cast<double>(keys[i])));
        }
    }
    else
    {
        for (int i = begin; i < end; ++i)
        {
            auto& pt = result[i - begin];
            double v = static_cast<double>(values[i]);
            if constexpr (!std::is_integral_v<V>)
            {
                // NaN values map to (0,0) — matches legacy QCPGraph behavior.
                // TODO: proper gap handling (break polyline) for NaN values.
                if (std::isnan(v)) { pt = QPointF(0, 0); continue; }
            }
            pt.setX(keyAxis->coordToPixel(static_cast<double>(keys[i])));
            pt.setY(valueAxis->coordToPixel(v));
        }
    }
    return result;
}

// NaN values in the adaptive sampling path silently corrupt min/max clusters
// (matches legacy QCPGraph behavior). TODO: add NaN filtering in the sampling loop.
template <IndexableNumericRange KC, IndexableNumericRange VC>
QVector<QPointF> optimizedLineData(const KC& keys, const VC& values,
                                    int begin, int end,
                                    int /*pixelWidth*/,
                                    QCPAxis* keyAxis, QCPAxis* valueAxis)
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
        return linesToPixels(keys, values, begin, end, keyAxis, valueAxis);

    // Adaptive sampling: consolidate multiple data points per pixel into min/max clusters.
    QVector<QPointF> result;
    result.reserve(maxCount);

    const bool isVertical = keyAxis->orientation() == Qt::Vertical;
    auto toPixel = [&](double k, double v) -> QPointF {
        return isVertical ? QPointF(valueAxis->coordToPixel(v), keyAxis->coordToPixel(k))
                          : QPointF(keyAxis->coordToPixel(k), valueAxis->coordToPixel(v));
    };

    int i = begin;
    double minValue = static_cast<double>(values[i]);
    double maxValue = minValue;
    int currentIntervalFirst = i;
    int reversedFactor = keyAxis->pixelOrientation();
    int reversedRound = reversedFactor == -1 ? 1 : 0;
    double currentIntervalStartKey = keyAxis->pixelToCoord(
        int(keyAxis->coordToPixel(static_cast<double>(keys[begin])) + reversedRound));
    double lastIntervalEndKey = currentIntervalStartKey;
    double keyEpsilon = qAbs(currentIntervalStartKey
        - keyAxis->pixelToCoord(keyAxis->coordToPixel(currentIntervalStartKey)
                                + 1.0 * reversedFactor));
    bool keyEpsilonVariable = keyAxis->scaleType() == QCPAxis::stLogarithmic;
    int intervalDataCount = 1;
    ++i;

    while (i < end)
    {
        double k = static_cast<double>(keys[i]);
        double v = static_cast<double>(values[i]);
        if (k < currentIntervalStartKey + keyEpsilon)
        {
            if (v < minValue) minValue = v;
            else if (v > maxValue) maxValue = v;
            ++intervalDataCount;
        }
        else
        {
            if (intervalDataCount >= 2)
            {
                double firstVal = static_cast<double>(values[currentIntervalFirst]);
                if (lastIntervalEndKey < currentIntervalStartKey - keyEpsilon)
                    result.append(toPixel(currentIntervalStartKey + keyEpsilon * 0.2, firstVal));
                result.append(toPixel(currentIntervalStartKey + keyEpsilon * 0.25, minValue));
                result.append(toPixel(currentIntervalStartKey + keyEpsilon * 0.75, maxValue));
                if (k > currentIntervalStartKey + keyEpsilon * 2)
                {
                    double prevVal = static_cast<double>(values[i - 1]);
                    result.append(toPixel(currentIntervalStartKey + keyEpsilon * 0.8, prevVal));
                }
            }
            else
            {
                double fk = static_cast<double>(keys[currentIntervalFirst]);
                double fv = static_cast<double>(values[currentIntervalFirst]);
                result.append(toPixel(fk, fv));
            }
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
    if (intervalDataCount >= 2)
    {
        double firstVal = static_cast<double>(values[currentIntervalFirst]);
        if (lastIntervalEndKey < currentIntervalStartKey - keyEpsilon)
            result.append(toPixel(currentIntervalStartKey + keyEpsilon * 0.2, firstVal));
        result.append(toPixel(currentIntervalStartKey + keyEpsilon * 0.25, minValue));
        result.append(toPixel(currentIntervalStartKey + keyEpsilon * 0.75, maxValue));
    }
    else
    {
        double fk = static_cast<double>(keys[currentIntervalFirst]);
        double fv = static_cast<double>(values[currentIntervalFirst]);
        result.append(toPixel(fk, fv));
    }

    return result;
}

} // namespace qcp::algo
