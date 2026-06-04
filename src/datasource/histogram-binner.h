#pragma once
#include "abstract-datasource.h"
#include <plottables/plottable-colormap.h>
#include <cmath>
#include <algorithm>

namespace qcp::algo {

// Maps a coordinate to a bin index, evenly spaced in linear or log10 space.
// For log, callers must have already excluded non-positive coordinates.
struct BinAxis
{
    double lo;       // lower edge in mapped space (coord, or log10(coord))
    double invWidth; // bins / span, in mapped space
    int bins;
    bool log;

    static BinAxis make(const QCPRange& range, int bins, bool log)
    {
        // Log bins require a positive range; callers (bin2d via the positive-only
        // finiteKeyValueBounds + expandIfFlat) must guarantee it. Asserting here
        // keeps that non-local invariant honest and catches a future regression
        // before it turns into log10(<=0) -> NaN -> UB in the index cast.
        Q_ASSERT(!log || (range.lower > 0 && range.upper > 0));
        const double lo = log ? std::log10(range.lower) : range.lower;
        const double hi = log ? std::log10(range.upper) : range.upper;
        const double span = hi - lo;
        return BinAxis { lo, span > 0 ? bins / span : 0.0, bins, log };
    }

    int index(double x) const
    {
        const double pos = log ? std::log10(x) : x;
        return std::clamp(static_cast<int>((pos - lo) * invWidth), 0, bins - 1);
    }
};

// Widen a zero-width range so binning has a non-degenerate span. Log ranges
// (lower > 0, guaranteed by the positive-only bounds) widen multiplicatively.
inline void expandIfFlat(QCPRange& range, bool log)
{
    if (range.size() != 0)
        return;
    if (log && range.lower > 0)
    {
        const double f = std::sqrt(10.0);
        range.lower /= f;
        range.upper *= f;
    }
    else
    {
        range.lower -= 0.5;
        range.upper += 0.5;
    }
}

inline QCPColorMapData* bin2d(const QCPAbstractDataSource& src, int keyBins, int valueBins,
                              bool keyLog = false, bool valueLog = false)
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
    if (!src.finiteKeyValueBounds(keyRange, valRange, keyLog, valueLog))
        return nullptr;

    expandIfFlat(keyRange, keyLog);
    expandIfFlat(valRange, valueLog);

    auto* data = new QCPColorMapData(keyBins, valueBins, keyRange, valRange);
    data->fill(0);

    // Bins are evenly spaced in log space when requested, so the grid's linear
    // coordinate extent [min, max] is preserved (the renderer stretches cells
    // uniformly in pixels, which on a log axis is uniform in log).
    const BinAxis kAxis = BinAxis::make(keyRange, keyBins, keyLog);
    const BinAxis vAxis = BinAxis::make(valRange, valueBins, valueLog);

    for (int i = 0; i < n; ++i)
    {
        double k = src.keyAt(i);
        double v = src.valueAt(i);
        if (!std::isfinite(k) || !std::isfinite(v))
            continue;
        if ((keyLog && k <= 0) || (valueLog && v <= 0))
            continue;

        const int kb = kAxis.index(k);
        const int vb = vAxis.index(v);
        data->setCell(kb, vb, data->cell(kb, vb) + 1.0);
    }

    data->recalculateDataBounds();
    return data;
}

} // namespace qcp::algo
