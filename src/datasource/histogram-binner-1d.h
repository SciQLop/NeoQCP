#pragma once
#include "abstract-multi-datasource.h"
#include "histogram-binner.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace qcp::algo {

struct Histogram1D
{
    std::vector<double> edges;                // bins + 1, increasing
    std::vector<std::vector<double>> heights; // one vector of `bins` per column
    std::vector<int> counted;                 // values counted per column (for normalization)
    bool log = false;
};

enum class HistogramNormalization { Count, Density, Probability };

namespace detail {

inline bool binnable(double v, bool positiveOnly)
{
    return std::isfinite(v) && !(positiveOnly && v <= 0);
}

} // namespace detail

//! Finite values of every column (positive only when positiveOnly); nullopt when there are none.
inline std::optional<QCPRange> finiteValueBounds(const QCPAbstractMultiDataSource& src,
                                                 bool positiveOnly)
{
    std::optional<QCPRange> bounds;
    for (int c = 0; c < src.columnCount(); ++c)
        for (int i = 0; i < src.size(); ++i)
        {
            const double v = src.valueAt(c, i);
            if (!detail::binnable(v, positiveOnly))
                continue;
            if (bounds)
                bounds = QCPRange(std::min(bounds->lower, v), std::max(bounds->upper, v));
            else
                bounds = QCPRange(v, v);
        }
    return bounds;
}

//! bins + 1 edges, evenly spaced in linear or log10 space.
inline std::vector<double> binEdges(QCPRange range, int bins, bool log)
{
    const double lo = log ? std::log10(range.lower) : range.lower;
    const double hi = log ? std::log10(range.upper) : range.upper;
    std::vector<double> edges(bins + 1);
    for (int i = 0; i <= bins; ++i)
    {
        const double mapped = lo + i * (hi - lo) / bins;
        edges[i] = log ? std::pow(10.0, mapped) : mapped;
    }
    return edges;
}

//! Counts finite values inside [range.lower, range.upper] of every column into shared bins.
//! Values outside the range are not counted. Log: non-positive values are skipped.
inline Histogram1D bin1d(const QCPAbstractMultiDataSource& src, QCPRange range, int bins, bool log)
{
    const BinAxis axis = BinAxis::make(range, bins, log);
    const int columns = src.columnCount();
    Histogram1D h { binEdges(range, bins, log), std::vector(columns, std::vector<double>(bins, 0.0)),
                    std::vector<int>(columns, 0), log };
    for (int c = 0; c < columns; ++c)
        for (int i = 0; i < src.size(); ++i)
        {
            const double v = src.valueAt(c, i);
            // The range check keeps BinAxis::index's clamp from piling outside values into edge bins.
            if (!detail::binnable(v, log) || v < range.lower || v > range.upper)
                continue;
            ++h.heights[c][axis.index(v)];
            ++h.counted[c];
        }
    return h;
}

//! Count: unchanged. Probability: count / N. Density: count / (N * width of that bin).
inline void normalize(Histogram1D& h, HistogramNormalization norm)
{
    if (norm == HistogramNormalization::Count)
        return;
    for (std::size_t c = 0; c < h.heights.size(); ++c)
    {
        if (h.counted[c] == 0)
            continue;
        for (std::size_t b = 0; b < h.heights[c].size(); ++b)
        {
            const double width = norm == HistogramNormalization::Density ? h.edges[b + 1] - h.edges[b] : 1.0;
            h.heights[c][b] /= h.counted[c] * width;
        }
    }
}

//! Auto-range policy: the current range if bounds fit inside it, else their union (edges only widen).
inline QCPRange widenedRange(std::optional<QCPRange> current, QCPRange bounds)
{
    if (!current)
        return bounds;
    return QCPRange(std::min(current->lower, bounds.lower), std::max(current->upper, bounds.upper));
}

} // namespace qcp::algo
