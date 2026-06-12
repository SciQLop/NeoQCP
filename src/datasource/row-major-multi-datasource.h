#pragma once
#include "abstract-multi-datasource.h"
#include "algorithms.h"
#include <QtGlobal>
#include <cstddef>
#include <memory>
#include <span>

namespace qcp::detail {

// Lightweight non-owning view of a single column in a row-major 2D array.
// Satisfies IndexableNumericRange (random_access_range + arithmetic value_type).
// Uses index-based iteration to avoid out-of-bounds pointer arithmetic when
// the base pointer is offset into the middle of a buffer (e.g. values + column).
template <typename V>
class StridedColumnView {
public:
    StridedColumnView(const V* base, int count, int stride)
        : mBase(base), mCount(count), mStride(stride) { Q_ASSERT(stride > 0); }

    struct Iterator {
        using iterator_category = std::random_access_iterator_tag;
        using value_type = V;
        using difference_type = std::ptrdiff_t;
        using pointer = const V*;
        using reference = const V&;

        const V* base = nullptr;
        int index = 0;
        int stride = 0;

        const V& operator*() const { return base[static_cast<std::ptrdiff_t>(index) * stride]; }
        Iterator& operator++() { ++index; return *this; }
        Iterator operator++(int) { auto t = *this; ++index; return t; }
        Iterator& operator--() { --index; return *this; }
        Iterator operator--(int) { auto t = *this; --index; return t; }
        Iterator& operator+=(difference_type n) { index += static_cast<int>(n); return *this; }
        Iterator& operator-=(difference_type n) { index -= static_cast<int>(n); return *this; }
        Iterator operator+(difference_type n) const { return {base, index + static_cast<int>(n), stride}; }
        Iterator operator-(difference_type n) const { return {base, index - static_cast<int>(n), stride}; }
        difference_type operator-(const Iterator& o) const { return index - o.index; }
        const V& operator[](difference_type n) const { return base[static_cast<std::ptrdiff_t>(index + n) * stride]; }
        auto operator<=>(const Iterator& o) const { return index <=> o.index; }
        bool operator==(const Iterator& o) const { return index == o.index; }
    };

    friend Iterator operator+(typename Iterator::difference_type n, const Iterator& it)
    { return it + n; }

    Iterator begin() const { return {mBase, 0, mStride}; }
    Iterator end() const { return {mBase, mCount, mStride}; }
    int size() const { return mCount; }
    const V& operator[](int i) const { return mBase[static_cast<std::ptrdiff_t>(i) * mStride]; }

private:
    const V* mBase;
    int mCount;
    int mStride;
};

} // namespace qcp::detail

// Non-owning multi-column data source for row-major (C-order) 2D arrays.
// Data layout: values[row * stride + col], where stride >= columnCount.
// Zero-copy: wraps the caller's buffer directly.
template <typename K, typename V>
class QCPRowMajorMultiDataSource final : public QCPAbstractMultiDataSource {
public:
    // dataGuard: optional shared_ptr that keeps the backing buffer alive for the
    // lifetime of this view. Pass the owner of keys/values so that async pipeline
    // jobs (which hold a shared_ptr to this object) transitively prevent the
    // underlying memory from being freed.
    QCPRowMajorMultiDataSource(std::span<const K> keys,
                                const V* values,
                                int rows,
                                int columns,
                                int stride,
                                std::shared_ptr<const void> dataGuard = {})
        : mKeys(keys), mValues(values), mRows(rows), mColumns(columns), mStride(stride),
          mDataGuard(std::move(dataGuard))
    {
        // Shape lies from callers must degrade to an empty source, not become
        // OOB reads (release) or aborts (debug) — this is a public entry point.
        const bool valid = rows > 0 && columns >= 0 && stride > 0 && stride >= columns
            && static_cast<int>(keys.size()) == rows && values != nullptr;
        if (!valid)
        {
            // Genuinely empty input (rows == 0, no keys) stays silent; any
            // other combination is a shape lie worth a diagnostic.
            if (rows != 0 || !keys.empty())
                qWarning("QCPRowMajorMultiDataSource: invalid shape (rows=%d, columns=%d, "
                         "stride=%d, keys=%zu, values=%p) — dropping data",
                         rows, columns, stride, keys.size(),
                         static_cast<const void*>(values));
            mKeys = {};
            mValues = nullptr;
            mRows = 0;
            mColumns = 0;
            mStride = 1;
        }
    }

    int columnCount() const override { return mColumns; }
    int size() const override { return mRows; }

    double keyAt(int i) const override
    {
        Q_ASSERT(i >= 0 && i < mRows);
        return static_cast<double>(mKeys[i]);
    }

    double valueAt(int column, int i) const override
    {
        Q_ASSERT(column >= 0 && column < mColumns);
        Q_ASSERT(i >= 0 && i < mRows);
        return static_cast<double>(mValues[i * mStride + column]);
    }

    QCPRange keyRange(bool& found, QCP::SignDomain sd = QCP::sdBoth) const override
    {
        return qcp::algo::keyRange(mKeys, found, sd);
    }

    QCPRange valueRange(int column, bool& found, QCP::SignDomain sd = QCP::sdBoth,
                        const QCPRange& inKeyRange = QCPRange()) const override
    {
        Q_ASSERT(column >= 0 && column < mColumns);
        qcp::detail::StridedColumnView<V> colView(mValues + column, mRows, mStride);
        return qcp::algo::valueRange(mKeys, colView, found, sd, inKeyRange);
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
        Q_ASSERT(column >= 0 && column < mColumns);
        qcp::detail::StridedColumnView<V> colView(mValues + column, mRows, mStride);
        ensureGapCache(begin, end);
        return qcp::algo::optimizedLineData(mKeys, colView, begin, end, pixelWidth,
                                             keyAxis, valueAxis, &mGapCache.gaps);
    }

    QVector<QPointF> getLines(int column, int begin, int end,
                               QCPAxis* keyAxis, QCPAxis* valueAxis) const override
    {
        Q_ASSERT(column >= 0 && column < mColumns);
        qcp::detail::StridedColumnView<V> colView(mValues + column, mRows, mStride);
        ensureGapCache(begin, end);
        return qcp::algo::linesToPixels(mKeys, colView, begin, end, keyAxis, valueAxis,
                                         qcp::algo::kDefaultGapThreshold, &mGapCache.gaps);
    }

    void getOptimizedLineDataAll(int begin, int end, int /*pixelWidth*/,
                                  QCPAxis* keyAxis, QCPAxis* valueAxis,
                                  QVector<QPointF>* results, int numColumns) const override
    {
        ensureGapCache(begin, end);
        const int N = std::min(numColumns, mColumns);
        const V* vals = mValues;
        const int stride = mStride;
        qcp::algo::optimizedLineDataMulti(
            mKeys, N,
            [vals, stride](int c, int i) -> double {
                return static_cast<double>(vals[static_cast<std::ptrdiff_t>(i) * stride + c]);
            },
            begin, end, keyAxis, valueAxis, &mGapCache.gaps, results);
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

    std::span<const K> mKeys;
    const V* mValues;
    int mRows;
    int mColumns;
    int mStride;
    std::shared_ptr<const void> mDataGuard;
    mutable struct { int begin = -1; int end = -1; qcp::algo::GapVector gaps; } mGapCache;
};
