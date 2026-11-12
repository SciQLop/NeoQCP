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

    constexpr double noReference = std::numeric_limits<double>::max();
    const auto step = [&](int i) {
        return static_cast<double>(keys[begin + i + 1]) - static_cast<double>(keys[begin + i]);
    };
    // A repeated key (zero step) draws a vertical edge: it says nothing about the sampling.
    const auto reference = [&](int i) { return step(i) > 0 ? step(i) : noReference; };

    for (int i = 0; i < count - 1; ++i)
    {
        double refDx = noReference;
        if (i > 0)
            refDx = std::min(refDx, reference(i - 1));
        if (i + 2 < count)
            refDx = std::min(refDx, reference(i + 1));
        if (refDx < noReference && step(i) > threshold * refDx)
            gapBefore.setGap(i + 1);
    }
    return gapBefore;
}

// How many times wider the step into key i is than the narrower of its neighbouring steps in
// [begin, end): the measure detectKeyGaps holds against its threshold. 0 when there is nothing
// to compare it to.
template <typename KeyAt>
double keyStepRatio(KeyAt key, int begin, int end, int i)
{
    if (i <= begin || i >= end) return 0;
    const double step = key(i) - key(i - 1);
    double ref = std::numeric_limits<double>::max();
    if (i - 1 > begin && key(i - 1) - key(i - 2) > 0)
        ref = std::min(ref, key(i - 1) - key(i - 2));
    if (i + 1 < end && key(i + 1) - key(i) > 0)
        ref = std::min(ref, key(i + 1) - key(i));
    return step > 0 && ref < std::numeric_limits<double>::max() ? step / ref : 0;
}

// Binned points say nothing about gaps through their keys: an empty bin is the binning, not a
// hole in the data. Each carries the keyStepRatio of the raw rows it came from instead, +inf
// where the rows were all NaN (a hole whatever the threshold).
inline GapVector gapsFromStepRatios(const std::vector<float>& ratios, int begin, int end,
                                    double threshold)
{
    GapVector gaps(end - begin);
    if (ratios.empty()) return gaps;
    for (int i = begin; i < end; ++i)
        if (std::isinf(ratios[i]) || (threshold > 0 && ratios[i] > threshold))
            gaps.setGap(i - begin);
    return gaps;
}

// The gaps of keys[begin, end), recomputed only when the range or the threshold changes.
struct GapCache {
    int begin = -1;
    int end = -1;
    double threshold = -1;
    GapVector gaps;

    template <IndexableNumericRange KC>
    void update(const KC& keys, int newBegin, int newEnd, double newThreshold)
    {
        if (begin == newBegin && end == newEnd && threshold == newThreshold)
            return;
        begin = newBegin;
        end = newEnd;
        threshold = newThreshold;
        gaps = detectKeyGaps(keys, begin, end, threshold);
    }

    void updateFromRatios(const std::vector<float>& ratios, int newBegin, int newEnd,
                          double newThreshold)
    {
        if (begin == newBegin && end == newEnd && threshold == newThreshold)
            return;
        begin = newBegin;
        end = newEnd;
        threshold = newThreshold;
        gaps = gapsFromStepRatios(ratios, begin, end, threshold);
    }
};

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

namespace detail {

// Shared point-sampling loop: one point per sample, a NaN marker for every key gap and
// every NaN value. mapPoint(k, v) maps a sample into the output space (pixels or data).
template <typename Out, typename Idx, IndexableNumericRange KC, IndexableNumericRange VC,
          typename MapPoint>
void linePointsInto(const KC& keys, const VC& values, int begin, int end, double gapThreshold,
                    const GapVector* precomputedGaps, Out& result, Idx* indices,
                    const MapPoint& mapPoint)
{
    using V = std::ranges::range_value_t<VC>;
    Q_ASSERT(begin >= 0 && end <= static_cast<int>(std::ranges::size(keys)));
    Q_ASSERT(begin >= 0 && end <= static_cast<int>(std::ranges::size(values)));
    result.clear();
    if (indices) indices->clear();
    const int count = end - begin;
    if (count <= 0) return;

    GapVector computedGaps;
    if (!precomputedGaps)
        computedGaps = detectKeyGaps(keys, begin, end, gapThreshold);
    const auto& gaps = precomputedGaps ? *precomputedGaps : computedGaps;

    result.reserve(count + count / 10);
    if (indices) indices->reserve(count + count / 10);

    const auto nanPt = QPointF(qQNaN(), qQNaN());
    for (int i = begin; i < end; ++i)
    {
        int ri = i - begin;
        if (gaps.hasAnyGap && gaps[ri])
        {
            result.push_back(nanPt);
            if (indices) indices->push_back(-1);
        }

        double v = static_cast<double>(values[i]);
        if constexpr (!std::is_integral_v<V>)
        {
            if (std::isnan(v))
            {
                result.push_back(nanPt);
                if (indices) indices->push_back(-1);
                continue;
            }
        }

        result.push_back(mapPoint(static_cast<double>(keys[i]), v));
        if (indices) indices->push_back(i);
    }
}

// The pixel path keeps its container: same loop, a fresh QVector per call.
template <bool WithIndices, IndexableNumericRange KC, IndexableNumericRange VC, typename MapPoint>
QVector<QPointF> linePointsImpl(const KC& keys, const VC& values, int begin, int end,
                                double gapThreshold, const GapVector* precomputedGaps,
                                QVector<int>* indices, const MapPoint& mapPoint)
{
    QVector<QPointF> result;
    linePointsInto(keys, values, begin, end, gapThreshold, precomputedGaps, result,
                   WithIndices ? indices : nullptr, mapPoint);
    return result;
}

// Sample -> pixel point, orientation-swapped for a vertical key axis.
struct PixelPointMap
{
    AffineTransform keyTf;
    AffineTransform valTf;
    QCPAxis* keyAxis;
    QCPAxis* valueAxis;
    bool isVertical;
    bool bothLinear;

    static PixelPointMap of(QCPAxis* keyAxis, QCPAxis* valueAxis)
    {
        PixelPointMap map;
        map.keyTf = AffineTransform::fromAxis(keyAxis);
        map.valTf = AffineTransform::fromAxis(valueAxis);
        map.keyAxis = keyAxis;
        map.valueAxis = valueAxis;
        map.isVertical = keyAxis->orientation() == Qt::Vertical;
        map.bothLinear = map.keyTf.isLinear && map.valTf.isLinear;
        return map;
    }

    QPointF operator()(double k, double v) const
    {
        if (bothLinear)
        {
            double kp = keyTf.toPixel(k);
            double vp = valTf.toPixel(v);
            return isVertical ? QPointF(vp, kp) : QPointF(kp, vp);
        }
        return isVertical ? QPointF(valueAxis->coordToPixel(v), keyAxis->coordToPixel(k))
                          : QPointF(keyAxis->coordToPixel(k), valueAxis->coordToPixel(v));
    }
};

// Sample -> data point.
struct DataPointMap
{
    QPointF operator()(double k, double v) const { return QPointF(k, v); }
};

template <bool WithIndices, IndexableNumericRange KC, IndexableNumericRange VC>
QVector<QPointF> linesToPixelsImpl(const KC& keys, const VC& values, int begin, int end,
                                   QCPAxis* keyAxis, QCPAxis* valueAxis,
                                   double gapThreshold, const GapVector* precomputedGaps,
                                   QVector<int>* indices)
{
    return linePointsImpl<WithIndices>(keys, values, begin, end, gapThreshold, precomputedGaps,
                                       indices, PixelPointMap::of(keyAxis, valueAxis));
}

// Bucket grid of the pixel-space envelope: one bucket per key pixel.
struct PixelBucketGrid
{
    AffineTransform keyTf;
    QCPAxis* keyAxis;
    bool useInlineTransform;
    int reversedFactor;
    int reversedRound;

    static PixelBucketGrid of(QCPAxis* keyAxis)
    {
        PixelBucketGrid grid;
        grid.keyTf = AffineTransform::fromAxis(keyAxis);
        grid.keyAxis = keyAxis;
        grid.useInlineTransform = grid.keyTf.isLinear;
        grid.reversedFactor = keyAxis->pixelOrientation();
        grid.reversedRound = grid.reversedFactor == -1 ? 1 : 0;
        return grid;
    }

    // Key of the start of the pixel bucket holding k.
    double startFor(double k) const
    {
        if (useInlineTransform)
            return keyTf.toCoord(static_cast<int>(keyTf.toPixel(k)) + reversedRound);
        return keyAxis->pixelToCoord(static_cast<int>(keyAxis->coordToPixel(k)) + reversedRound);
    }

    // Key width of one pixel at start.
    double epsilonFor(double start) const
    {
        if (useInlineTransform) return qAbs(keyTf.invScale);
        return qAbs(start
                    - keyAxis->pixelToCoord(keyAxis->coordToPixel(start) + 1.0 * reversedFactor));
    }
};

// Bucket grid of the data-space envelope: buckets of a fixed key width.
struct KeyBucketGrid
{
    double anchor;
    double width;

    double startFor(double k) const { return anchor + std::floor((k - anchor) / width) * width; }
    double epsilonFor(double) const { return width; }
};

// The same buckets on a log key axis, where a bucket must hold an equal span of log10(key):
// equal key widths would thin a decade into one pixel and keep a thousand at the low end.
// \a width and \a anchor are in decades, the keys it returns are in key units.
struct LogKeyBucketGrid
{
    double anchor;
    double width;

    double startFor(double k) const
    {
        if (!(k > 0)) return k;
        return std::pow(10.0, anchor + std::floor((std::log10(k) - anchor) / width) * width);
    }
    double epsilonFor(double start) const
    {
        return start > 0 ? start * (std::pow(10.0, width) - 1.0) : width;
    }
};

// The min/max envelope: one bucket per grid cell, up to four points per bucket (first, min,
// max, last) so peaks survive when many samples land on one pixel.
template <typename Out, typename Idx, IndexableNumericRange KC, IndexableNumericRange VC,
          typename Grid, typename MapPoint>
void envelopeInto(const KC& keys, const VC& values, int begin, int end, int reserveHint,
                  double gapThreshold, const GapVector* precomputedGaps, Out& result,
                  Idx* indices, const Grid& grid, const MapPoint& mapPoint)
{
    PROFILE_HERE_N("optimizedLineData");
    GapVector computedGaps;
    if (!precomputedGaps)
        computedGaps = detectKeyGaps(keys, begin, end, gapThreshold);
    const auto& gaps = precomputedGaps ? *precomputedGaps : computedGaps;
    const auto nanPt = QPointF(qQNaN(), qQNaN());

    result.clear();
    if (indices) indices->clear();
    result.reserve(reserveHint);
    if (indices) indices->reserve(reserveHint);

    auto emitPoint = [&](const QPointF& p, int index) {
        result.push_back(p);
        if (indices) indices->push_back(index);
    };

    auto flushInterval = [&](int intervalFirst, int intervalLast, int intervalCount,
                             double intervalStartKey, double lastEndKey, double minVal,
                             double maxVal, int minIdx, int maxIdx, double epsilon, double nextKey) {
        if (intervalCount >= 2)
        {
            double firstVal = static_cast<double>(values[intervalFirst]);
            if (lastEndKey < intervalStartKey - epsilon)
                emitPoint(mapPoint(intervalStartKey + epsilon * 0.2, firstVal), intervalFirst);
            // In time order: min first on a falling line would zigzag inside every pixel.
            const bool maxFirst = maxIdx < minIdx;
            emitPoint(mapPoint(intervalStartKey + epsilon * 0.25, maxFirst ? maxVal : minVal),
                      maxFirst ? maxIdx : minIdx);
            emitPoint(mapPoint(intervalStartKey + epsilon * 0.75, maxFirst ? minVal : maxVal),
                      maxFirst ? minIdx : maxIdx);
            if (nextKey > intervalStartKey + epsilon * 2)
                emitPoint(mapPoint(intervalStartKey + epsilon * 0.8,
                                   static_cast<double>(values[intervalLast])), intervalLast);
        }
        else
        {
            emitPoint(mapPoint(static_cast<double>(keys[intervalFirst]),
                               static_cast<double>(values[intervalFirst])), intervalFirst);
        }
    };

    int i = begin;
    using V = std::ranges::range_value_t<VC>;
    if constexpr (!std::is_integral_v<V>)
    {
        while (i < end && std::isnan(static_cast<double>(values[i])))
            ++i;
    }
    if (i >= end) return;

    double minValue = static_cast<double>(values[i]);
    double maxValue = minValue;
    int minIndex = i;
    int maxIndex = i;
    int currentIntervalFirst = i;
    int currentIntervalLast = i;

    double currentIntervalStartKey = grid.startFor(static_cast<double>(keys[i]));
    double lastIntervalEndKey = currentIntervalStartKey;
    double keyEpsilon = grid.epsilonFor(currentIntervalStartKey);
    double nextBoundary = currentIntervalStartKey + keyEpsilon;

    int intervalDataCount = 1;
    ++i;

    const uint8_t* gapData = gaps.data.data();
    const bool hasAnyGap = gaps.hasAnyGap;

    auto startInterval = [&](int index, double k, double v, double endOfPrevious) {
        lastIntervalEndKey = endOfPrevious;
        minValue = v;
        maxValue = v;
        minIndex = index;
        maxIndex = index;
        currentIntervalFirst = index;
        currentIntervalLast = index;
        currentIntervalStartKey = grid.startFor(k);
        keyEpsilon = grid.epsilonFor(currentIntervalStartKey);
        nextBoundary = currentIntervalStartKey + keyEpsilon;
        intervalDataCount = 1;
    };
    auto flushCurrent = [&](double nextKey) {
        flushInterval(currentIntervalFirst, currentIntervalLast, intervalDataCount,
                      currentIntervalStartKey, lastIntervalEndKey, minValue, maxValue, minIndex,
                      maxIndex, keyEpsilon, nextKey);
    };

    // NaN means "no sample". A run of NaN wider than a pixel is a hole you can see,
    // so it breaks the line like a key gap; skipping it bridged the hole whenever
    // zoomed out. Narrower runs (e.g. every other sample NaN) are skipped, so the
    // line still shows at this zoom.
    bool inNaNRun = false;
    while (i < end)
    {
        double v = static_cast<double>(values[i]);
        double k = static_cast<double>(keys[i]);
        if constexpr (!std::is_integral_v<V>)
        {
            if (std::isnan(v))
            {
                inNaNRun = true;
                ++i;
                continue;
            }
        }
        const bool visibleNaNHole = inNaNRun
            && qAbs(k - static_cast<double>(keys[currentIntervalLast])) > keyEpsilon;
        inNaNRun = false;

        if (visibleNaNHole || (hasAnyGap && gapData[i - begin]))
        {
            flushCurrent(k);
            emitPoint(nanPt, -1);
            startInterval(i, k, v, currentIntervalStartKey);
            ++i;
            continue;
        }

        if (k < nextBoundary)
        {
            if (v < minValue) { minValue = v; minIndex = i; }
            if (v > maxValue) { maxValue = v; maxIndex = i; }
            ++intervalDataCount;
            currentIntervalLast = i;
        }
        else
        {
            flushCurrent(k);
            startInterval(i, k, v, static_cast<double>(keys[i - 1]));
        }
        ++i;
    }
    flushCurrent(currentIntervalStartKey + keyEpsilon * 3);
}

// The pixel path keeps its container: same envelope, a fresh QVector per call.
template <bool WithIndices, IndexableNumericRange KC, IndexableNumericRange VC, typename Grid,
          typename MapPoint>
QVector<QPointF> envelopeImpl(const KC& keys, const VC& values, int begin, int end,
                              int reserveHint, double gapThreshold,
                              const GapVector* precomputedGaps, QVector<int>* indices,
                              const Grid& grid, const MapPoint& mapPoint)
{
    QVector<QPointF> result;
    envelopeInto(keys, values, begin, end, reserveHint, gapThreshold, precomputedGaps, result,
                 WithIndices ? indices : nullptr, grid, mapPoint);
    return result;
}

} // namespace detail

template <IndexableNumericRange KC, IndexableNumericRange VC>
QVector<QPointF> linesToPixels(const KC& keys, const VC& values, int begin, int end,
                               QCPAxis* keyAxis, QCPAxis* valueAxis,
                               double gapThreshold = kDefaultGapThreshold,
                               const GapVector* precomputedGaps = nullptr)
{
    return detail::linesToPixelsImpl<false>(keys, values, begin, end, keyAxis, valueAxis,
                                            gapThreshold, precomputedGaps, nullptr);
}

template <IndexableNumericRange KC, IndexableNumericRange VC>
QVector<QPointF> linesToPixelsIndexed(const KC& keys, const VC& values, int begin, int end,
                                      QCPAxis* keyAxis, QCPAxis* valueAxis, QVector<int>& indices,
                                      double gapThreshold = kDefaultGapThreshold,
                                      const GapVector* precomputedGaps = nullptr)
{
    return detail::linesToPixelsImpl<true>(keys, values, begin, end, keyAxis, valueAxis,
                                           gapThreshold, precomputedGaps, &indices);
}

namespace detail {

// Pixel-space envelope, as getOptimizedLineData has always computed it.
template <bool WithIndices, IndexableNumericRange KC, IndexableNumericRange VC>
QVector<QPointF> optimizedLineDataImpl(const KC& keys, const VC& values, int begin, int end,
                                       int /*pixelWidth*/, QCPAxis* keyAxis, QCPAxis* valueAxis,
                                       const GapVector* precomputedGaps, QVector<int>* indices)
{
    Q_ASSERT(begin >= 0 && end <= static_cast<int>(std::ranges::size(keys)));
    Q_ASSERT(begin >= 0 && end <= static_cast<int>(std::ranges::size(values)));
    if constexpr (WithIndices) indices->clear();
    const int dataCount = end - begin;
    if (dataCount <= 0) return {};

    double keyPixelSpan = qAbs(keyAxis->coordToPixel(static_cast<double>(keys[begin]))
                                - keyAxis->coordToPixel(static_cast<double>(keys[end - 1])));
    int maxCount = (std::numeric_limits<int>::max)();
    if (2 * keyPixelSpan + 2 < static_cast<double>((std::numeric_limits<int>::max)()))
        maxCount = int(2 * keyPixelSpan + 2);

    if (dataCount < maxCount)
        return linesToPixelsImpl<WithIndices>(keys, values, begin, end, keyAxis, valueAxis,
                                              kDefaultGapThreshold, precomputedGaps, indices);

    return envelopeImpl<WithIndices>(keys, values, begin, end, maxCount, kDefaultGapThreshold,
                                     precomputedGaps, indices, PixelBucketGrid::of(keyAxis),
                                     PixelPointMap::of(keyAxis, valueAxis));
}

// Data-space counterpart: the same envelope, one bucket per 1 / pixelsPerKey key units, or per
// 1 / pixelsPerKey decades when the key axis is logarithmic. The points are written straight into
// the vector that goes back to the caller.
template <bool WithIndices, IndexableNumericRange KC, IndexableNumericRange VC>
std::vector<QPointF> linesInDataImpl(const KC& keys, const VC& values, int begin, int end,
                                     double pixelsPerKey, double gapThreshold,
                                     const GapVector* precomputedGaps,
                                     [[maybe_unused]] std::vector<int>* sourceIndices,
                                     bool logKeys)
{
    Q_ASSERT(begin >= 0 && end <= static_cast<int>(std::ranges::size(keys)));
    Q_ASSERT(begin >= 0 && end <= static_cast<int>(std::ranges::size(values)));
    std::vector<QPointF> pts;
    std::vector<int> indices;
    std::vector<int>* idx = nullptr;
    if constexpr (WithIndices)
    {
        sourceIndices->clear();
        idx = &indices;
    }

    const int dataCount = end - begin;
    if (dataCount <= 0) return pts;

    const double firstKey = static_cast<double>(keys[begin]);
    const double lastKey = static_cast<double>(keys[end - 1]);
    const bool decades = logKeys && firstKey > 0 && lastKey > 0;
    const double keySpan = pixelsPerKey
        * (decades ? qAbs(std::log10(lastKey) - std::log10(firstKey)) : qAbs(lastKey - firstKey));
    // A NaN end key makes the span NaN: the envelope's reserve size would overflow, draw raw.
    if (pixelsPerKey <= 0 || !std::isfinite(keySpan) || static_cast<double>(dataCount) < 2 * keySpan + 2)
    {
        linePointsInto(keys, values, begin, end, gapThreshold, precomputedGaps, pts, idx,
                       DataPointMap {});
    }
    else
    {
        const int reserveHint = static_cast<int>(2 * keySpan + 2) + 4;
        if (decades)
            envelopeInto(keys, values, begin, end, reserveHint, gapThreshold, precomputedGaps, pts,
                         idx, LogKeyBucketGrid { std::log10(firstKey), 1.0 / pixelsPerKey },
                         DataPointMap {});
        else
            envelopeInto(keys, values, begin, end, reserveHint, gapThreshold, precomputedGaps, pts,
                         idx, KeyBucketGrid { firstKey, 1.0 / pixelsPerKey }, DataPointMap {});
    }

    if constexpr (WithIndices) *sourceIndices = std::move(indices);
    return pts;
}

} // namespace detail

template <IndexableNumericRange KC, IndexableNumericRange VC>
QVector<QPointF> optimizedLineData(const KC& keys, const VC& values,
                                   int begin, int end, int pixelWidth,
                                   QCPAxis* keyAxis, QCPAxis* valueAxis,
                                   const GapVector* precomputedGaps = nullptr)
{
    return detail::optimizedLineDataImpl<false>(keys, values, begin, end, pixelWidth,
                                                keyAxis, valueAxis, precomputedGaps, nullptr);
}

template <IndexableNumericRange KC, IndexableNumericRange VC>
QVector<QPointF> optimizedLineDataIndexed(const KC& keys, const VC& values,
                                          int begin, int end, int pixelWidth,
                                          QCPAxis* keyAxis, QCPAxis* valueAxis,
                                          QVector<int>& indices,
                                          const GapVector* precomputedGaps = nullptr)
{
    return detail::optimizedLineDataImpl<true>(keys, values, begin, end, pixelWidth,
                                               keyAxis, valueAxis, precomputedGaps, &indices);
}

// Data-space line points: the same points as linesToPixels, in (key, value) units,
// NaN-separated exactly where the pixel path breaks the line. pixelsPerKey <= 0 keeps every
// point; otherwise the line is thinned to the min/max envelope, one bucket per key pixel (per
// pixel of log10(key) when \a logKeys: pixelsPerKey then counts pixels per decade).
template <IndexableNumericRange KC, IndexableNumericRange VC>
std::vector<QPointF> linesInData(const KC& keys, const VC& values, int begin, int end,
                                 double pixelsPerKey, std::vector<int>* sourceIndices = nullptr,
                                 double gapThreshold = kDefaultGapThreshold,
                                 const GapVector* precomputedGaps = nullptr,
                                 bool logKeys = false)
{
    if (!sourceIndices)
        return detail::linesInDataImpl<false>(keys, values, begin, end, pixelsPerKey, gapThreshold,
                                             precomputedGaps, nullptr, logKeys);
    return detail::linesInDataImpl<true>(keys, values, begin, end, pixelsPerKey, gapThreshold,
                                         precomputedGaps, sourceIndices, logKeys);
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
        int intervalFirst; // first accepted (non-NaN) sample of the interval
        int intervalLast;  // last accepted sample, across intervals; -1 after a break
        int intervalCount; // accepted samples in the interval
        bool maxFirst;     // the max came before the min: emit them in that order
        bool inNaNRun;     // NaN samples since intervalLast
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

    auto startIntervalAt = [&](double k) {
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
    };

    // Linear axes have one pixel width; otherwise the first interval measures it, and only
    // a log axis measures it again for every interval.
    if (useInlineTransform)
        keyEpsilon = qAbs(keyTf.invScale);
    keyEpsilonVariable = !useInlineTransform;
    startIntervalAt(static_cast<double>(keys[i]));
    keyEpsilonVariable = keyEpsilonVariable && keyAxis->scaleType() == QCPAxis::stLogarithmic;
    lastIntervalEndKey = currentIntervalStartKey;

    auto flushColumn = [&](int c, const ColState& s,
                            double intervalStartKey, double lastEndKey,
                            double epsilon, double nextKey) {
        if (s.intervalCount >= 2)
        {
            double firstVal = valueAt(c, s.intervalFirst);
            if (lastEndKey < intervalStartKey - epsilon)
                results[c].append(toPixel(intervalStartKey + epsilon * 0.2, firstVal));
            const double first = s.maxFirst ? s.maxVal : s.minVal;
            const double second = s.maxFirst ? s.minVal : s.maxVal;
            results[c].append(toPixel(intervalStartKey + epsilon * 0.25, first));
            results[c].append(toPixel(intervalStartKey + epsilon * 0.75, second));
            if (nextKey > intervalStartKey + epsilon * 2)
                results[c].append(toPixel(intervalStartKey + epsilon * 0.8,
                                          valueAt(c, s.intervalLast)));
        }
        else if (s.intervalCount == 1)
        {
            results[c].append(toPixel(static_cast<double>(keys[s.intervalFirst]),
                                      valueAt(c, s.intervalFirst)));
        }
    };

    // NaN means "no sample". A run of NaN wider than a pixel is a hole you can see, so
    // it breaks the column's line like a key gap; skipping it bridged the hole whenever
    // zoomed out. Narrower runs (e.g. every other sample NaN) are skipped, so the line
    // still shows at this zoom.
    auto accept = [&](int c, int index, double k) {
        ColState& s = cs[c];
        const double v = valueAt(c, index);
        if (std::isnan(v))
        {
            s.inNaNRun = true;
            return;
        }
        if (s.inNaNRun && s.intervalLast >= 0
            && qAbs(k - static_cast<double>(keys[s.intervalLast])) > keyEpsilon)
        {
            flushColumn(c, s, currentIntervalStartKey, lastIntervalEndKey, keyEpsilon, k);
            results[c].append(nanPt);
            s.intervalCount = 0;
        }
        s.inNaNRun = false;
        if (s.intervalCount == 0)
        {
            s.minVal = s.maxVal = v;
            s.intervalFirst = index;
            s.maxFirst = false;
        }
        else
        {
            if (v < s.minVal) { s.minVal = v; s.maxFirst = true; }
            if (v > s.maxVal) { s.maxVal = v; s.maxFirst = false; }
        }
        s.intervalLast = index;
        ++s.intervalCount;
    };
    // Closes every column's interval at a pixel boundary; a key gap also breaks the lines.
    auto closeIntervals = [&](double nextKey, bool gap) {
        for (int c = 0; c < numColumns; ++c)
        {
            if (cs[c].intervalCount > 0)
                flushColumn(c, cs[c], currentIntervalStartKey, lastIntervalEndKey, keyEpsilon,
                            nextKey);
            if (gap)
            {
                results[c].append(nanPt);
                cs[c].intervalLast = -1;
            }
            cs[c].intervalCount = 0;
        }
    };

    for (int c = 0; c < numColumns; ++c)
    {
        cs[c] = {qQNaN(), qQNaN(), i, -1, 0, false, false};
        accept(c, i, static_cast<double>(keys[i]));
    }

    const uint8_t* gapData = gaps.data.data();
    const bool hasAnyGap = gaps.hasAnyGap;

    ++i;
    while (i < end)
    {
        const double k = static_cast<double>(keys[i]);
        const bool gap = hasAnyGap && gapData[i - begin];
        if (gap || k >= nextBoundary)
        {
            closeIntervals(k, gap);
            lastIntervalEndKey = gap ? currentIntervalStartKey : static_cast<double>(keys[i - 1]);
            startIntervalAt(k);
        }
        for (int c = 0; c < numColumns; ++c)
            accept(c, i, k);
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
