#pragma once
#include "plottable-color-mapper.h"
#include <QPointF>
#include <QVector>
#include <cmath>
#include <vector>

namespace qcp {

//! Points [first, last] drawn in one colour: segments first->first+1 ... last-1->last.
struct ColorRun
{
    int first;
    int last;
    int bucket;
};

//! Segment k -> k+1 takes the colour of point k+1; it is not drawn when either point
//! is a gap (non-finite) or point k+1 has no colour. Equal consecutive colours merge.
inline std::vector<ColorRun> colorRuns(const QVector<QPointF>& points, const QVector<int>& indices,
                                       const ColorScalarMapper& mapper)
{
    std::vector<ColorRun> runs;
    auto finite = [](const QPointF& p) { return std::isfinite(p.x()) && std::isfinite(p.y()); };
    for (int k = 0; k + 1 < points.size(); ++k)
    {
        const int b = finite(points[k]) && finite(points[k + 1]) ? mapper.bucket(indices[k + 1])
                                                                  : ColorScalarMapper::kGap;
        if (b == ColorScalarMapper::kGap)
            continue;
        if (!runs.empty() && runs.back().bucket == b && runs.back().last == k)
            runs.back().last = k + 1;
        else
            runs.push_back({k, k + 1, b});
    }
    return runs;
}

inline QColor runColor(const ColorScalarMapper& mapper, int bucket)
{
    return QColor::fromRgba(qUnpremultiply(mapper.color(bucket)));
}

} // namespace qcp
