#pragma once
#include "abstract-datasource.h"
#include "algorithms.h"
#include "../Profiling.hpp"
#include <QtGlobal>

template <IndexableNumericRange KeyContainer, IndexableNumericRange ValueContainer>
class QCPSoADataSource final : public QCPAbstractDataSource {
public:
    using K = std::ranges::range_value_t<KeyContainer>;
    using V = std::ranges::range_value_t<ValueContainer>;

    QCPSoADataSource(KeyContainer keys, ValueContainer values)
        : mKeys(std::move(keys)), mValues(std::move(values))
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
        return qcp::algo::optimizedLineData(mKeys, mValues, begin, end, pixelWidth,
                                             keyAxis, valueAxis);
    }

    QVector<QPointF> getLines(int begin, int end,
                               QCPAxis* keyAxis, QCPAxis* valueAxis) const override
    {
        return qcp::algo::linesToPixels(mKeys, mValues, begin, end, keyAxis, valueAxis);
    }

private:
    KeyContainer mKeys;
    ValueContainer mValues;
};
