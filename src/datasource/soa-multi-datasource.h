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

    std::vector<QPointF> linesInData(int column, int begin, int end, double pixelsPerKey,
                                     std::vector<int>* sourceIndices = nullptr,
                                     bool logKeys = false, bool cacheGaps = true) const override
    {
        Q_ASSERT(column >= 0 && column < columnCount());
        if (cacheGaps)
        {
            ensureGapCache(begin, end);
            return qcp::algo::linesInData(mKeys, mValues[column], begin, end, pixelsPerKey,
                                          sourceIndices, gapThreshold(), &mGapCache.gaps, logKeys);
        }
        // Per-call gaps: the data-space read never writes the shared cache.
        const auto gaps = qcp::algo::detectKeyGaps(mKeys, begin, end, gapThreshold());
        return qcp::algo::linesInData(mKeys, mValues[column], begin, end, pixelsPerKey,
                                      sourceIndices, gapThreshold(), &gaps, logKeys);
    }

    qcp::algo::GapVector gapsBefore(int begin, int end, bool cacheGaps = true) const override
    {
        if (!cacheGaps)
            return qcp::algo::detectKeyGaps(mKeys, begin, end, gapThreshold());
        ensureGapCache(begin, end);
        return mGapCache.gaps;
    }

    const double* rawKeyData() const override
    {
        if constexpr (std::is_same_v<K, double> && ContiguousNumericRange<KeyContainer>)
            return std::ranges::data(mKeys);
        else
            return nullptr;
    }

    QCPRawColumn rawColumn(int column) const override
    {
        if constexpr (ContiguousNumericRange<ValueContainer>)
        {
            if (column >= 0 && column < columnCount())
                return QCPRawColumn::of(std::ranges::data(mValues[column]));
        }
        return {};
    }

private:
    void ensureGapCache(int begin, int end) const
    {
        mGapCache.update(mKeys, begin, end, gapThreshold());
    }

    KeyContainer mKeys;
    std::vector<ValueContainer> mValues;
    std::shared_ptr<const void> mDataGuard;
    mutable qcp::algo::GapCache mGapCache;
};
