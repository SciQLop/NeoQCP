#pragma once
#include "abstract-multi-datasource.h"
#include "algorithms.h"
#include <QVector>
#include <algorithm>
#include <cstddef>
#include <memory>
#include <numeric>
#include <span>
#include <type_traits>
#include <vector>

//! The source a graph owns: keys and N value columns in double, growable in place.
//!
//! setData() on double inputs builds one; addData() appends to it. Keys stay sorted: an append
//! whose keys fall below the last one is merged in order, and a block appended with
//! alreadySorted = false is sorted first. The revision moves on every append, so a caller that
//! caches a derived answer can tell.
class QCPGrowableMultiDataSource final : public QCPAbstractMultiDataSource
{
public:
    QCPGrowableMultiDataSource() = default;

    //! Takes any numeric ranges; the ones that are std::vector<double> are moved, others copied.
    //! The keys are sorted unless alreadySorted.
    template <IndexableNumericRange KC, IndexableNumericRange VC>
    explicit QCPGrowableMultiDataSource(KC&& keys, std::vector<VC>&& valueColumns,
                                       bool alreadySorted = false)
        : mKeys(toDoubles(std::forward<KC>(keys)))
    {
        mColumns.reserve(valueColumns.size());
        for (auto& column : valueColumns)
            mColumns.push_back(toDoubles(std::move(column)));
        if (!alreadySorted)
            sortBlocks();
        validateShape();
    }

    int columnCount() const override { return static_cast<int>(mColumns.size()); }
    int size() const override { return static_cast<int>(mKeys.size()); }

    double keyAt(int i) const override
    {
        Q_ASSERT(i >= 0 && i < size());
        return mKeys[static_cast<std::size_t>(i)];
    }

    double valueAt(int column, int i) const override
    {
        Q_ASSERT(column >= 0 && column < columnCount());
        Q_ASSERT(i >= 0 && i < size());
        return mColumns[static_cast<std::size_t>(column)][static_cast<std::size_t>(i)];
    }

    QCPRange keyRange(bool& found, QCP::SignDomain sd = QCP::sdBoth) const override
    {
        return qcp::algo::keyRangeSorted(mKeys, found, sd);
    }

    QCPRange valueRange(int column, bool& found, QCP::SignDomain sd = QCP::sdBoth,
                        const QCPRange& inKeyRange = QCPRange()) const override
    {
        Q_ASSERT(column >= 0 && column < columnCount());
        return qcp::algo::valueRange(mKeys, mColumns[static_cast<std::size_t>(column)], found, sd,
                                     inKeyRange);
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
        return qcp::algo::optimizedLineData(
            mKeys, mColumns[static_cast<std::size_t>(column)], begin, end, pixelWidth, keyAxis,
            valueAxis, &mGapCache.gaps);
    }

    QVector<QPointF> getLines(int column, int begin, int end, QCPAxis* keyAxis,
                              QCPAxis* valueAxis) const override
    {
        Q_ASSERT(column >= 0 && column < columnCount());
        ensureGapCache(begin, end);
        return qcp::algo::linesToPixels(mKeys, mColumns[static_cast<std::size_t>(column)], begin,
                                        end, keyAxis, valueAxis, gapThreshold(), &mGapCache.gaps);
    }

    std::vector<QPointF> linesInData(int column, int begin, int end, double pixelsPerKey,
                                     std::vector<int>* sourceIndices = nullptr,
                                     bool logKeys = false) const override
    {
        Q_ASSERT(column >= 0 && column < columnCount());
        ensureGapCache(begin, end);
        return qcp::algo::linesInData(mKeys, mColumns[static_cast<std::size_t>(column)], begin, end,
                                      pixelsPerKey, sourceIndices, gapThreshold(), &mGapCache.gaps,
                                      logKeys);
    }

    qcp::algo::GapVector gapsBefore(int begin, int end) const override
    {
        ensureGapCache(begin, end);
        return mGapCache.gaps;
    }

    QVector<QPointF> getLinesIndexed(int column, int begin, int end, QCPAxis* keyAxis,
                                     QCPAxis* valueAxis,
                                     QVector<int>& sourceIndices) const override
    {
        Q_ASSERT(column >= 0 && column < columnCount());
        ensureGapCache(begin, end);
        return qcp::algo::linesToPixelsIndexed(
            mKeys, mColumns[static_cast<std::size_t>(column)], begin, end, keyAxis, valueAxis,
            sourceIndices, gapThreshold(), &mGapCache.gaps);
    }

    QVector<QPointF> getOptimizedLineDataIndexed(int column, int begin, int end, int pixelWidth,
                                                 QCPAxis* keyAxis, QCPAxis* valueAxis,
                                                 QVector<int>& sourceIndices) const override
    {
        Q_ASSERT(column >= 0 && column < columnCount());
        ensureGapCache(begin, end);
        return qcp::algo::optimizedLineDataIndexed(
            mKeys, mColumns[static_cast<std::size_t>(column)], begin, end, pixelWidth, keyAxis,
            valueAxis, sourceIndices, &mGapCache.gaps);
    }

    void getOptimizedLineDataAll(int begin, int end, int /*pixelWidth*/, QCPAxis* keyAxis,
                                 QCPAxis* valueAxis, QVector<QPointF>* results,
                                 int numColumns) const override
    {
        ensureGapCache(begin, end);
        const int N = std::min(numColumns, columnCount());
        qcp::algo::optimizedLineDataMulti(
            mKeys, N,
            [this](int c, int i) -> double
            { return mColumns[static_cast<std::size_t>(c)][static_cast<std::size_t>(i)]; },
            begin, end, keyAxis, valueAxis, &mGapCache.gaps, results);
    }

    const double* rawKeyData() const override { return mKeys.data(); }

    QCPRawColumn rawColumn(int column) const override
    {
        if (column >= 0 && column < columnCount())
            return QCPRawColumn::of(mColumns[static_cast<std::size_t>(column)].data());
        return {};
    }

    //! Every mutation moves the revision.
    [[nodiscard]] quint64 revision() const { return mRevision; }

    //! A deep copy, for a pipeline job that must keep reading the rows it started from.
    [[nodiscard]] std::shared_ptr<QCPGrowableMultiDataSource> clone() const
    {
        auto copy = std::make_shared<QCPGrowableMultiDataSource>();
        copy->mKeys = mKeys;
        copy->mColumns = mColumns;
        copy->mRevision = mRevision;
        return copy;
    }

    //! Appends rows. The column count must match an existing source (an empty one takes any).
    //! A block appended with alreadySorted = false is sorted first; keys below the last one merge
    //! in order. False and no change when the shape does not match.
    bool append(const QVector<double>& keys, std::vector<QVector<double>> columns,
                bool alreadySorted = false)
    {
        if (keys.isEmpty())
            return true;
        if (!mColumns.empty() && columns.size() != mColumns.size())
            return false;
        for (const auto& column : columns)
            if (column.size() != keys.size())
                return false;

        std::vector<double> newKeys(keys.begin(), keys.end());
        std::vector<std::vector<double>> newColumns;
        newColumns.reserve(columns.size());
        for (const auto& column : columns)
            newColumns.emplace_back(column.begin(), column.end());
        if (!alreadySorted)
            sortBlocks(newKeys, newColumns);
        mergeBlocks(std::move(newKeys), std::move(newColumns));
        return true;
    }

    //! Appends one row of \a values (one per column).
    bool append(double key, std::span<const double> values)
    {
        if (!mKeys.empty() && mColumns.empty())
            return false;
        if (!mColumns.empty() && values.size() != mColumns.size())
            return false;
        if (mColumns.empty() && values.empty())
            return false;

        if (mColumns.empty())
            mColumns.resize(values.size());
        if (mKeys.empty() || key >= mKeys.back())
        {
            mKeys.push_back(key);
            for (std::size_t c = 0; c < mColumns.size(); ++c)
                mColumns[c].push_back(values[c]);
            bumpRevision();
            return true;
        }
        const auto at = std::lower_bound(mKeys.begin(), mKeys.end(), key);
        const auto index = static_cast<std::size_t>(at - mKeys.begin());
        mKeys.insert(at, key);
        for (std::size_t c = 0; c < mColumns.size(); ++c)
            mColumns[c].insert(mColumns[c].begin() + static_cast<std::ptrdiff_t>(index), values[c]);
        bumpRevision();
        return true;
    }

private:
    template <typename Range>
    static std::vector<double> toDoubles(Range&& range)
    {
        if constexpr (std::is_same_v<std::remove_cvref_t<Range>, std::vector<double>>)
            return std::forward<Range>(range);
        else
            return std::vector<double>(std::ranges::begin(range), std::ranges::end(range));
    }

    static void sortBlocks(std::vector<double>& keys, std::vector<std::vector<double>>& columns)
    {
        if (keys.size() < 2 || std::is_sorted(keys.begin(), keys.end()))
            return;
        std::vector<int> order(keys.size());
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(),
                  [&keys](int a, int b) { return keys[a] < keys[b]; });
        std::vector<double> sortedKeys;
        sortedKeys.reserve(keys.size());
        std::vector<std::vector<double>> sortedColumns(
            columns.size(), std::vector<double>(keys.size()));
        for (std::size_t i = 0; i < order.size(); ++i)
        {
            sortedKeys.push_back(keys[static_cast<std::size_t>(order[i])]);
            for (std::size_t c = 0; c < columns.size(); ++c)
                sortedColumns[c][i] = columns[c][static_cast<std::size_t>(order[i])];
        }
        keys = std::move(sortedKeys);
        columns = std::move(sortedColumns);
    }

    void sortBlocks()
    {
        sortBlocks(mKeys, mColumns);
    }

    void mergeBlocks(std::vector<double> keys, std::vector<std::vector<double>> columns)
    {
        if (mKeys.empty())
        {
            mKeys = std::move(keys);
            if (mColumns.empty())
                mColumns = std::move(columns);
            else
                for (std::size_t c = 0; c < mColumns.size(); ++c)
                    mColumns[c] = std::move(columns[c]);
            bumpRevision();
            return;
        }

        if (keys.front() >= mKeys.back())
        {
            mKeys.reserve(mKeys.size() + keys.size());
            mKeys.insert(mKeys.end(), keys.begin(), keys.end());
            for (std::size_t c = 0; c < mColumns.size(); ++c)
            {
                mColumns[c].reserve(mColumns[c].size() + columns[c].size());
                mColumns[c].insert(mColumns[c].end(), columns[c].begin(), columns[c].end());
            }
            bumpRevision();
            return;
        }

        if (keys.back() <= mKeys.front())
        {
            mKeys.insert(mKeys.begin(), keys.begin(), keys.end());
            for (std::size_t c = 0; c < mColumns.size(); ++c)
                mColumns[c].insert(mColumns[c].begin(), columns[c].begin(), columns[c].end());
            bumpRevision();
            return;
        }

        std::vector<double> mergedKeys;
        mergedKeys.reserve(mKeys.size() + keys.size());
        std::vector<std::vector<double>> mergedColumns(mColumns.size());
        for (auto& column : mergedColumns)
            column.reserve(mKeys.size() + keys.size());

        std::size_t oldIndex = 0;
        std::size_t newIndex = 0;
        while (oldIndex < mKeys.size() && newIndex < keys.size())
        {
            const bool takeNew = keys[newIndex] < mKeys[oldIndex];
            if (takeNew)
            {
                mergedKeys.push_back(keys[newIndex]);
                for (std::size_t c = 0; c < mColumns.size(); ++c)
                    mergedColumns[c].push_back(columns[c][newIndex]);
                ++newIndex;
            }
            else
            {
                mergedKeys.push_back(mKeys[oldIndex]);
                for (std::size_t c = 0; c < mColumns.size(); ++c)
                    mergedColumns[c].push_back(mColumns[c][oldIndex]);
                ++oldIndex;
            }
        }
        for (; oldIndex < mKeys.size(); ++oldIndex)
        {
            mergedKeys.push_back(mKeys[oldIndex]);
            for (std::size_t c = 0; c < mColumns.size(); ++c)
                mergedColumns[c].push_back(mColumns[c][oldIndex]);
        }
        for (; newIndex < keys.size(); ++newIndex)
        {
            mergedKeys.push_back(keys[newIndex]);
            for (std::size_t c = 0; c < mColumns.size(); ++c)
                mergedColumns[c].push_back(columns[c][newIndex]);
        }

        mKeys = std::move(mergedKeys);
        mColumns = std::move(mergedColumns);
        bumpRevision();
    }

    void validateShape()
    {
        for (const auto& column : mColumns)
        {
            if (column.size() != mKeys.size())
            {
                qWarning("QCPGrowableMultiDataSource: column length mismatch (%zu vs %zu keys) — "
                         "dropping data",
                         column.size(), mKeys.size());
                mKeys.clear();
                mColumns.clear();
                resetGapCache();
                return;
            }
        }
    }

    void bumpRevision()
    {
        ++mRevision;
        resetGapCache();
    }

    void resetGapCache()
    {
        mGapCache.begin = -1;
        mGapCache.end = -1;
        mGapCache.threshold = -1;
        mGapCache.gaps = qcp::algo::GapVector();
    }

    void ensureGapCache(int begin, int end) const
    {
        mGapCache.update(mKeys, begin, end, gapThreshold());
    }

    std::vector<double> mKeys;
    std::vector<std::vector<double>> mColumns;
    quint64 mRevision = 0;
    mutable qcp::algo::GapCache mGapCache;
};
