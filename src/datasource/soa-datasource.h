#pragma once
#include "abstract-datasource.h"
#include "algorithms.h"
#include "../Profiling.hpp"
#include <QtGlobal>
#include <memory>

template <IndexableNumericRange KeyContainer, IndexableNumericRange ValueContainer>
class QCPSoADataSource final : public QCPAbstractDataSource {
public:
    using K = std::ranges::range_value_t<KeyContainer>;
    using V = std::ranges::range_value_t<ValueContainer>;

    QCPSoADataSource(KeyContainer keys, ValueContainer values,
                      std::shared_ptr<const void> dataGuard = {})
        : mKeys(std::move(keys)), mValues(std::move(values)),
          mDataGuard(std::move(dataGuard))
    {
        Q_ASSERT(std::ranges::size(mKeys) == std::ranges::size(mValues));
    }

    const KeyContainer& keys() const { return mKeys; }
    const ValueContainer& values() const { return mValues; }

    int size() const override
    {
        return static_cast<int>(std::ranges::size(mKeys));
    }

    double keyAt(int i) const override
    {
        Q_ASSERT(i >= 0 && i < size());
        return static_cast<double>(mKeys[i]);
    }

    double valueAt(int i) const override
    {
        Q_ASSERT(i >= 0 && i < size());
        return static_cast<double>(mValues[i]);
    }

    QCPRange keyRange(bool& foundRange, QCP::SignDomain sd = QCP::sdBoth) const override
    {
        PROFILE_HERE_N("SoA::keyRange");
        return qcp::algo::keyRangeSorted(mKeys, foundRange, sd);
    }

    QCPRange valueRange(bool& foundRange, QCP::SignDomain sd = QCP::sdBoth,
                        const QCPRange& inKeyRange = QCPRange()) const override
    {
        PROFILE_HERE_N("SoA::valueRange");
        return qcp::algo::valueRange(mKeys, mValues, foundRange, sd, inKeyRange);
    }

    bool finiteKeyValueBounds(QCPRange& keyOut, QCPRange& valueOut,
                              bool keyPositiveOnly = false,
                              bool valuePositiveOnly = false) const override
    {
        PROFILE_HERE_N("SoA::finiteKeyValueBounds");
        const int n = static_cast<int>(std::ranges::size(mKeys));
        double kLo = std::numeric_limits<double>::infinity(), kHi = -kLo;
        double vLo = kLo, vHi = -kLo;
        bool any = false;
        for (int i = 0; i < n; ++i)
        {
            const double k = static_cast<double>(mKeys[i]);
            const double v = static_cast<double>(mValues[i]);
            if (!std::isfinite(k) || !std::isfinite(v))
                continue;
            if ((keyPositiveOnly && k <= 0) || (valuePositiveOnly && v <= 0))
                continue;
            kLo = std::min(kLo, k); kHi = std::max(kHi, k);
            vLo = std::min(vLo, v); vHi = std::max(vHi, v);
            any = true;
        }
        if (!any)
            return false;
        keyOut = QCPRange(kLo, kHi);
        valueOut = QCPRange(vLo, vHi);
        return true;
    }

    int findBegin(double sortKey, bool expandedRange = true) const override
    {
        return qcp::algo::findBegin(mKeys, sortKey, expandedRange);
    }

    int findEnd(double sortKey, bool expandedRange = true) const override
    {
        return qcp::algo::findEnd(mKeys, sortKey, expandedRange);
    }

    QVector<QPointF> getOptimizedLineData(int begin, int end, int pixelWidth,
                                           QCPAxis* keyAxis, QCPAxis* valueAxis) const override
    {
        ensureGapCache(begin, end);
        return qcp::algo::optimizedLineData(mKeys, mValues, begin, end, pixelWidth,
                                             keyAxis, valueAxis, &mGapCache.gaps);
    }

    QVector<QPointF> getLines(int begin, int end,
                               QCPAxis* keyAxis, QCPAxis* valueAxis) const override
    {
        ensureGapCache(begin, end);
        return qcp::algo::linesToPixels(mKeys, mValues, begin, end, keyAxis, valueAxis,
                                         qcp::algo::kDefaultGapThreshold, &mGapCache.gaps);
    }

private:
    void ensureGapCache(int begin, int end) const
    {
        if (mGapCache.begin != begin || mGapCache.end != end)
        {
            mGapCache.begin = begin;
            mGapCache.end = end;
            mGapCache.gaps = qcp::algo::detectKeyGaps(mKeys, begin, end);
        }
    }

    KeyContainer mKeys;
    ValueContainer mValues;
    std::shared_ptr<const void> mDataGuard;
    mutable struct { int begin = -1; int end = -1; qcp::algo::GapVector gaps; } mGapCache;
};
