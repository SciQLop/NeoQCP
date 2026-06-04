#pragma once
#include "global.h"
#include "axis/range.h"
#include <QPointF>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <limits>
#include <ranges>
#include <type_traits>

class QCPAxis;

// Concepts for data source container requirements
template <typename C>
concept IndexableNumericRange = std::ranges::random_access_range<C>
    && std::is_arithmetic_v<std::ranges::range_value_t<C>>;

template <typename C>
concept ContiguousNumericRange = IndexableNumericRange<C>
    && std::ranges::contiguous_range<C>;

// Non-templated abstract base class for all data sources.
// QCPGraph2 holds a pointer to this; virtual dispatch happens once per render.
class QCPAbstractDataSource {
public:
    virtual ~QCPAbstractDataSource() = default;

    virtual int size() const = 0;
    virtual bool empty() const { return size() == 0; }

    // Range queries (for axis auto-scaling)
    virtual QCPRange keyRange(bool& foundRange,
                              QCP::SignDomain sd = QCP::sdBoth) const = 0;
    virtual QCPRange valueRange(bool& foundRange,
                                QCP::SignDomain sd = QCP::sdBoth,
                                const QCPRange& inKeyRange = QCPRange()) const = 0;

    // Finite, pairwise bounds over (key, value) samples, for 2D binning.
    //
    // Unlike keyRange()/valueRange(), this makes NO sorted-key assumption and a
    // sample contributes only when BOTH coordinates are finite -- so the bounds
    // match exactly the set of points a histogram will accumulate. Returns false
    // when no finite pair exists. Default scans via keyAt()/valueAt(); typed
    // sources override for a non-virtual inner loop.
    virtual bool finiteKeyValueBounds(QCPRange& keyOut, QCPRange& valueOut) const
    {
        double kLo = std::numeric_limits<double>::infinity(), kHi = -kLo;
        double vLo = kLo, vHi = -kLo;
        bool any = false;
        const int n = size();
        for (int i = 0; i < n; ++i)
        {
            const double k = keyAt(i);
            const double v = valueAt(i);
            if (!std::isfinite(k) || !std::isfinite(v))
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

    // Binary search on sorted keys.
    // expandedRange=true includes one extra point beyond the boundary
    // (needed for correct line rendering at viewport edges).
    virtual int findBegin(double sortKey, bool expandedRange = true) const = 0;
    virtual int findEnd(double sortKey, bool expandedRange = true) const = 0;

    // Per-element access (slow path: selection, tooltips)
    virtual double keyAt(int i) const = 0;
    virtual double valueAt(int i) const = 0;

    // Processed outputs -- implementations run native-type algorithms internally,
    // cast to double/QPointF only at the pixel-coordinate output step.
    virtual QVector<QPointF> getOptimizedLineData(
        int begin, int end, int pixelWidth,
        QCPAxis* keyAxis, QCPAxis* valueAxis) const = 0;

    virtual QVector<QPointF> getLines(
        int begin, int end,
        QCPAxis* keyAxis, QCPAxis* valueAxis) const = 0;
};
