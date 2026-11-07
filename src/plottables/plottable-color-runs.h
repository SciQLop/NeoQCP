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

} // namespace qcp
