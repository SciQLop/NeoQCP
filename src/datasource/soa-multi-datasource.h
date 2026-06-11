#pragma once
#include "abstract-multi-datasource.h"
#include "algorithms.h"
#include <QtGlobal>
#include <memory>
#include <vector>

template <IndexableNumericRange KeyContainer, IndexableNumericRange ValueContainer>
class QCPSoAMultiDataSource final : public QCPAbstractMultiDataSource {
public:
    using K = std::ranges::range_value_t<KeyContainer>;
    using V = std::ranges::range_value_t<ValueContainer>;

    QCPSoAMultiDataSource(KeyContainer keys, std::vector<ValueContainer> valueColumns,
                           std::shared_ptr<const void> dataGuard = {})
        : mKeys(std::move(keys)), mValues(std::move(valueColumns)),
          mDataGuard(std::move(dataGuard))
    {
        // Shape lies from callers must degrade to an empty source, not become
        // OOB reads (release) or aborts (debug) — this is a public entry point.
        for (const auto& col : mValues)
        {
            if (std::ranges::size(col) != std::ranges::size(mKeys))
            {
                qWarning("QCPSoAMultiDataSource: column length mismatch (%zu vs %zu keys) — dropping data",
                         static_cast<std::size_t>(std::ranges::size(col)),
                         static_cast<std::size_t>(std::ranges::size(mKeys)));
                mKeys = {};
                mValues.clear();
                break;
            }
        }
    }

    int columnCount() const override { return static_cast<int>(mValues.size()); }
    int size() const override { return static_cast<int>(std::ranges::size(mKeys)); }

    double keyAt(int i) const override
    {
        Q_ASSERT(i >= 0 && i < size());
        return static_cast<double>(mKeys[i]);
    }

    double valueAt(int column, int i) const override
    {
        Q_ASSERT(column >= 0 && column < columnCount());
        Q_ASSERT(i >= 0 && i < size());
        return static_cast<double>(mValues[column][i]);
    }

    QCPRange keyRange(bool& found, QCP::SignDomain sd = QCP::sdBoth) const override
    {
        return qcp::algo::keyRangeSorted(mKeys, found, sd);
    }

    QCPRange valueRange(int column, bool& found, QCP::SignDomain sd = QCP::sdBoth,
                        const QCPRange& inKeyRange = QCPRange()) const override
    {
        Q_ASSERT(column >= 0 && column < columnCount());
        return qcp::algo::valueRange(mKeys, mValues[column], found, sd, inKeyRange);
    }

    int findBegin(double sortKey, bool expandedRange = true) const override
    {
        return qcp::algo::findBegin(mKeys, sortKey, expandedRange);
    }

    int findEnd(double sortKey, bool expandedRange = true) const override
    {
        return qcp::algo::findEnd(mKeys, sortKey, expandedRange);
    }

    QVector<QPointF> getOptimizedLineData(int column, int begin, int end, int pixelWidth,
                                           QCPAxis* keyAxis, QCPAxis* valueAxis) const override
    {
        Q_ASSERT(column >= 0 && column < columnCount());
        ensureGapCache(begin, end);
        return qcp::algo::optimizedLineData(mKeys, mValues[column], begin, end, pixelWidth,
                                             keyAxis, valueAxis, &mGapCache.gaps);
    }

    QVector<QPointF> getLines(int column, int begin, int end,
                               QCPAxis* keyAxis, QCPAxis* valueAxis) const override
    {
        Q_ASSERT(column >= 0 && column < columnCount());
        ensureGapCache(begin, end);
        return qcp::algo::linesToPixels(mKeys, mValues[column], begin, end, keyAxis, valueAxis,
                                         qcp::algo::kDefaultGapThreshold, &mGapCache.gaps);
    }

    void getOptimizedLineDataAll(int begin, int end, int /*pixelWidth*/,
                                  QCPAxis* keyAxis, QCPAxis* valueAxis,
                                  QVector<QPointF>* results, int numColumns) const override
    {
        ensureGapCache(begin, end);
        const int N = std::min(numColumns, columnCount());
        qcp::algo::optimizedLineDataMulti(
            mKeys, N,
            [this](int c, int i) -> double { return static_cast<double>(mValues[c][i]); },
            begin, end, keyAxis, valueAxis, &mGapCache.gaps, results);
    }

    const double* rawKeyData() const override
    {
        if constexpr (std::is_same_v<K, double> && ContiguousNumericRange<KeyContainer>)
            return std::ranges::data(mKeys);
        else
            return nullptr;
    }

    const double* rawColumnData(int column) const override
    {
        if constexpr (std::is_same_v<V, double> && ContiguousNumericRange<ValueContainer>)
        {
            if (column >= 0 && column < columnCount())
                return std::ranges::data(mValues[column]);
        }
        return nullptr;
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
    std::vector<ValueContainer> mValues;
    std::shared_ptr<const void> mDataGuard;
    mutable struct { int begin = -1; int end = -1; qcp::algo::GapVector gaps; } mGapCache;
};
