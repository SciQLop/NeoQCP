#pragma once
#include "plottable-color-mapper.h"
#include <QPointF>
#include <QVector>
#include <cmath>
#include <optional>
#include <span>
#include <vector>

namespace qcp
{

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
            runs.push_back({ k, k + 1, b });
    }
    return runs;
}

inline QColor runColor(const ColorScalarMapper& mapper, int bucket)
{
    return QColor::fromRgba(qUnpremultiply(mapper.color(bucket)));
}

//! Points [first, last] drawn in one colour: segments first->first+1 ... last-1->last.
struct ColorSpan
{
    int first;
    int last;
    QRgb colour;
};

//! One span per run of consecutive segments painted the same colour: segment k is colourAt(k),
//! a segment without a colour (or with a non-finite endpoint) is left out, and equal neighbours
//! merge, so a line painted in two colours is two spans rather than one per segment.
template <class ColourAt>
std::vector<ColorSpan> colorSpans(std::span<const QPointF> points, ColourAt colourAt)
{
    std::vector<ColorSpan> spans;
    auto finite = [](const QPointF& p) { return std::isfinite(p.x()) && std::isfinite(p.y()); };
    for (int k = 0; k + 1 < int(points.size()); ++k)
    {
        if (!finite(points[k]) || !finite(points[k + 1]))
            continue;
        const std::optional<QRgb> colour = colourAt(k);
        if (!colour)
            continue;
        if (!spans.empty() && spans.back().colour == *colour && spans.back().last == k)
            spans.back().last = k + 1;
        else
            spans.push_back({ k, k + 1, *colour });
    }
    return spans;
}

//! The colours the command API carries: segment k takes vertex k's colour, every point carrying the
//! colour of the segment it starts (the last vertex's colour is unused).
inline std::vector<ColorSpan> colorSpans(std::span<const QPointF> points,
                                         std::span<const QRgb> colours)
{
    return colorSpans(points, [colours](int k) { return std::optional<QRgb>(colours[k]); });
}

//! The mapper's colours, as spans: segment k keeps the bucket of point k + 1, as it always has.
inline std::vector<ColorSpan> colorSpans(const QVector<QPointF>& points,
                                         const QVector<int>& indices,
                                         const ColorScalarMapper& mapper)
{
    return colorSpans(std::span<const QPointF>(points.constData(), points.size()),
                      [&](int k) -> std::optional<QRgb>
                      {
                          const int bucket = mapper.bucket(indices[k + 1]);
                          return bucket == ColorScalarMapper::kGap
                              ? std::nullopt
                              : std::optional<QRgb>(runColor(mapper, bucket).rgba());
                      });
}

//! \a runs in the colour form the spans carry: the painter and the extruder take one shape per run.
inline std::vector<ColorSpan> colorSpans(const std::vector<ColorRun>& runs,
                                         const ColorScalarMapper& mapper)
{
    std::vector<ColorSpan> spans;
    spans.reserve(runs.size());
    for (const ColorRun& run : runs)
        spans.push_back({ run.first, run.last, runColor(mapper, run.bucket).rgba() });
    return spans;
}

} // namespace qcp
