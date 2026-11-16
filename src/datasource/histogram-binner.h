#pragma once
#include "abstract-datasource.h"
#include <plottables/plottable-colormap.h>
#include <cmath>
#include <algorithm>
#include <vector>

namespace qcp::algo {

// Maps a coordinate to a bin index, evenly spaced in scale space.
// For log, callers must have already excluded non-positive coordinates.
struct BinAxis
{
    // Log10, not QCPScale::toScale: log bins have always been exact decades (bin edges of
    // [1, 1000] in 3 bins are exactly 10 and 100), and toScale computes log/ln(base).
    static double position(double x, const QCPScale& scale)
    {
        if (scale.isLog()) return std::log10(x);
        return scale.isSymlog() ? scale.toScale(x) : x;
    }

    double lo;       // lower edge in scale space
    double invWidth; // bins / span, in scale space
    int bins;
    QCPScale scale;

    static BinAxis make(const QCPRange& range, int bins, const QCPScale& scale)
    {
        // Log bins require a positive range; callers (bin2d via the positive-only
        // finiteKeyValueBounds + expandIfFlat) must guarantee it. Asserting here
        // keeps that non-local invariant honest and catches a future regression
        // before it turns into log10(<=0) -> NaN -> UB in the index cast.
        Q_ASSERT(!scale.isLog() || (range.lower > 0 && range.upper > 0));
        const double lo = position(range.lower, scale);
        const double hi = position(range.upper, scale);
        const double span = hi - lo;
        return BinAxis { lo, span > 0 ? bins / span : 0.0, bins, scale };
    }

    int index(double x) const
    {
        const double pos = position(x, scale);
        return std::clamp(static_cast<int>((pos - lo) * invWidth), 0, bins - 1);
    }
};

// Widen a zero-width range so binning has a non-degenerate span. Log ranges
// (lower > 0, guaranteed by the positive-only bounds) widen multiplicatively; any other scale
// widens by half a unit of scale space on each side.
inline void expandIfFlat(QCPRange& range, const QCPScale& scale)
{
    if (range.size() != 0)
        return;
    if (scale.isLog() && range.lower > 0)
    {
        const double f = std::sqrt(10.0);
        range.lower /= f;
        range.upper *= f;
    }
    else if (scale.isSymlog())
    {
        range.lower = scale.fromScale(scale.toScale(range.lower) - 0.5);
        range.upper = scale.fromScale(scale.toScale(range.upper) + 0.5);
    }
    else
    {
        range.lower -= 0.5;
        range.upper += 0.5;
    }
}

//! One bin count in display units. `measure` is the bin width in data units (2D: the cell area);
//! both histograms go through this, so they share their normalization.
inline double normalized(double count, double total, double measure, QCP::HistogramNormalization norm)
{
    switch (norm)
    {
    case QCP::HistogramNormalization::Count:
        return count;
    case QCP::HistogramNormalization::Probability:
        return total > 0 ? count / total : 0;
    case QCP::HistogramNormalization::Density:
        return total > 0 && measure > 0 ? count / (total * measure) : 0;
    }
    return count;
}

//! Width of each bin in data units, matching BinAxis's even spacing in scale space.
inline std::vector<double> binWidths(const QCPRange& range, int bins, const QCPScale& scale)
{
    std::vector<double> widths(static_cast<std::size_t>(std::max(0, bins)), 0.0);
    const BinAxis axis = BinAxis::make(range, bins, scale);
    if (!(axis.invWidth > 0))
        return widths;
    const auto data = [&](double pos) {
        if (scale.isLog()) return std::pow(10.0, pos);
        return scale.isSymlog() ? scale.fromScale(pos) : pos;
    };
    for (int i = 0; i < bins; ++i)
    {
        const double lo = axis.lo + i / axis.invWidth;
        const double hi = axis.lo + (i + 1) / axis.invWidth;
        widths[std::size_t(i)] = data(hi) - data(lo);
    }
    return widths;
}

inline QCPColorMapData* bin2d(const QCPDataSource& src, int keyBins, int valueBins,
                              const QCPScale& keyScale = QCPScale(),
                              const QCPScale& valueScale = QCPScale())
{
    const int n = src.size();
    if (n == 0 || keyBins <= 0 || valueBins <= 0)
        return nullptr;

    // Histogram input is scattered, not sorted by key, and may contain NaN/Inf.
    // finiteKeyValueBounds() gives the true min/max over finite (key, value)
    // pairs -- never the first/last sample. On a log axis, non-positive samples
    // can't be placed, so they are excluded from the bounds too. No qualifying
    // pair => no histogram.
    QCPRange keyRange, valRange;
    if (!src.finiteKeyValueBounds(keyRange, valRange, keyScale.isLog(), valueScale.isLog()))
        return nullptr;

    expandIfFlat(keyRange, keyScale);
    expandIfFlat(valRange, valueScale);

    auto* data = new QCPColorMapData(keyBins, valueBins, keyRange, valRange);
    data->fill(0);

    // Bins are evenly spaced in log space when requested, so the grid's linear
    // coordinate extent [min, max] is preserved (the renderer stretches cells
    // uniformly in pixels, which on a log axis is uniform in log).
    const BinAxis kAxis = BinAxis::make(keyRange, keyBins, keyScale);
    const BinAxis vAxis = BinAxis::make(valRange, valueBins, valueScale);

    for (int i = 0; i < n; ++i)
    {
        double k = src.keyAt(i);
        double v = src.valueAt(i);
        if (!std::isfinite(k) || !std::isfinite(v))
            continue;
        if ((keyScale.isLog() && k <= 0) || (valueScale.isLog() && v <= 0))
            continue;

        const int kb = kAxis.index(k);
        const int vb = vAxis.index(v);
        data->setCell(kb, vb, data->cell(kb, vb) + 1.0);
    }

    data->recalculateDataBounds();
    return data;
}

} // namespace qcp::algo
