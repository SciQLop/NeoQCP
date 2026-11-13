#pragma once
#include "abstract-multi-datasource.h"
#include "algorithms.h"
#include "../misuse.h"
#include <QVector>
#include <algorithm>
#include <cmath>
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
//! alreadySorted = false is sorted first. Keys must be finite and every column must have one value
//! per key: both are refused with a misuse before anything is sorted or indexed.
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
        if (!validShape())
        {
            rejectShape();
            return;
        }
        if (!alreadySorted)
            sortBlocks();
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

    std::vector<QPointF> linesInData(int column, int begin, int end, double pixelsPerKey,
                                     std::vector<int>* sourceIndices = nullptr,
                                     bool logKeys = false, bool cacheGaps = true) const override
    {
        Q_ASSERT(column >= 0 && column < columnCount());
        if (cacheGaps)
        {
            ensureGapCache(begin, end);
            return qcp::algo::linesInData(mKeys, mColumns[static_cast<std::size_t>(column)],
                                          begin, end, pixelsPerKey, sourceIndices, gapThreshold(),
                                          &mGapCache.gaps, logKeys);
        }
        // Per-call gaps: this is the branch a worker job may still be reading the rows for, so it
        // never writes the shared cache.
        const auto gaps = qcp::algo::detectKeyGaps(mKeys, begin, end, gapThreshold());
        return qcp::algo::linesInData(mKeys, mColumns[static_cast<std::size_t>(column)], begin, end,
                                      pixelsPerKey, sourceIndices, gapThreshold(), &gaps, logKeys);
    }

    qcp::algo::GapVector gapsBefore(int begin, int end, bool cacheGaps = true) const override
    {
        if (!cacheGaps)
            return qcp::algo::detectKeyGaps(mKeys, begin, end, gapThreshold());
        ensureGapCache(begin, end);
        return mGapCache.gaps;
    }

    const double* rawKeyData() const override { return mKeys.data(); }

    QCPRawColumn rawColumn(int column) const override
    {
        if (column >= 0 && column < columnCount())
            return QCPRawColumn::of(mColumns[static_cast<std::size_t>(column)].data());
        return {};
    }

    //! A deep copy, for a pipeline job that must keep reading the rows it started from. The copy
    //! carries the gap threshold: the job may run before the graph next sets it on a fresh source.
    [[nodiscard]] std::shared_ptr<QCPGrowableMultiDataSource> clone() const
    {
        auto copy = std::make_shared<QCPGrowableMultiDataSource>();
        copy->mKeys = mKeys;
        copy->mColumns = mColumns;
        copy->setGapThreshold(gapThreshold());
        return copy;
    }

    //! Appends rows. The column count must match an existing source (an empty one takes any).
    //! A block appended with alreadySorted = false is sorted first; keys below the last one merge
    //! in order. False and no change when the shape does not match or a key is not finite.
    bool append(const QVector<double>& keys, std::vector<QVector<double>> columns,
                bool alreadySorted = false)
    {
        if (keys.isEmpty())
            return true;
        if (!mColumns.empty() && columns.size() != mColumns.size())
        {
            rejectShape("the value column count does not match the source's");
            return false;
        }
        for (const auto& column : columns)
            if (column.size() != keys.size())
            {
                rejectShape("keys and a value column have different sizes");
                return false;
            }
        if (!std::all_of(keys.begin(), keys.end(), [](double key) { return std::isfinite(key); }))
        {
            rejectShape("keys must be finite");
            return false;
        }

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
        if (!std::isfinite(key))
        {
            rejectShape("keys must be finite");
            return false;
        }
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
            changed();
            return true;
        }
        const auto at = std::lower_bound(mKeys.begin(), mKeys.end(), key);
        const auto index = static_cast<std::size_t>(at - mKeys.begin());
        mKeys.insert(at, key);
        for (std::size_t c = 0; c < mColumns.size(); ++c)
            mColumns[c].insert(mColumns[c].begin() + static_cast<std::ptrdiff_t>(index), values[c]);
        changed();
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
            changed();
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
            changed();
            return;
        }

        if (keys.back() <= mKeys.front())
        {
            mKeys.insert(mKeys.begin(), keys.begin(), keys.end());
            for (std::size_t c = 0; c < mColumns.size(); ++c)
                mColumns[c].insert(mColumns[c].begin(), columns[c].begin(), columns[c].end());
            changed();
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
        changed();
    }

    //! Whether the keys and columns form one row per key, with keys a sort can order.
    [[nodiscard]] bool validShape() const
    {
        if (!std::all_of(mKeys.begin(), mKeys.end(), [](double key) { return std::isfinite(key); }))
            return false;
        for (const auto& column : mColumns)
            if (column.size() != mKeys.size())
                return false;
        return true;
    }

    static void rejectShape(const char* what) { qcp::reportMisuse(Q_FUNC_INFO, what); }

    //! A constructor has nothing to keep: the refused data is dropped.
    void rejectShape()
    {
        rejectShape("keys and value columns must have one value per key, and keys must be finite");
        mKeys.clear();
        mColumns.clear();
        resetGapCache();
    }

    void changed() { resetGapCache(); }

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
    mutable qcp::algo::GapCache mGapCache;
};
