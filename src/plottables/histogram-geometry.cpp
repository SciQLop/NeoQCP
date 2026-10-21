#include "histogram-geometry.h"
#include "multigraph-fill.h"
#include "../axis/axis.h"
#include "../datasource/histogram-binner-1d.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace qcp::histogram {
namespace {

const QPointF kBreak(std::numeric_limits<double>::quiet_NaN(),
                     std::numeric_limits<double>::quiet_NaN());

QPointF point(double key, double value, bool keyIsVertical)
{
    return keyIsVertical ? QPointF(value, key) : QPointF(key, value);
}

// A log axis has no position for a count <= 0: such a bar collapses onto its base.
double topPixel(const QCPAxis& valueAxis, double height, double base)
{
    const bool log = valueAxis.scaleType() == QCPAxis::stLogarithmic;
    if (!std::isfinite(height) || (log && height <= 0))
        return base;
    return valueAxis.coordToPixel(height);
}

bool joins(const BarRect& previous, const BarRect& next)
{
    return !isEmpty(previous) && !isEmpty(next) && previous.right == next.left;
}

void startRun(QVector<QPointF>& out)
{
    if (!out.isEmpty())
        out.append(kBreak);
}

} // namespace

std::vector<BarRect> barRects(const qcp::algo::Histogram1D& h, int column,
                              const QCPAxis* keyAxis, const QCPAxis* valueAxis)
{
    if (column < 0 || column >= int(h.heights.size()) || h.edges.size() < 2)
        return {};
    const double base = qcp::fill::baselinePixel(*valueAxis, 0);
    const auto& heights = h.heights[column];
    std::vector<BarRect> bars;
    bars.reserve(heights.size());
    for (std::size_t i = 0; i < heights.size() && i + 1 < h.edges.size(); ++i)
        bars.push_back({keyAxis->coordToPixel(h.edges[i]), keyAxis->coordToPixel(h.edges[i + 1]),
                        base, topPixel(*valueAxis, heights[i], base)});
    return bars;
}

QRectF pixelRect(const BarRect& bar, bool keyIsVertical)
{
    return QRectF(point(bar.left, bar.base, keyIsVertical), point(bar.right, bar.top, keyIsVertical))
        .normalized();
}

QVector<QPointF> barOutlines(std::span<const BarRect> bars, bool keyIsVertical)
{
    QVector<QPointF> out;
    for (const BarRect& bar : bars)
    {
        if (isEmpty(bar))
            continue;
        startRun(out);
        out << point(bar.left, bar.base, keyIsVertical) << point(bar.left, bar.top, keyIsVertical)
            << point(bar.right, bar.top, keyIsVertical) << point(bar.right, bar.base, keyIsVertical)
            << point(bar.left, bar.base, keyIsVertical);
    }
    return out;
}

QVector<QPointF> stepOutline(std::span<const BarRect> bars, bool keyIsVertical)
{
    QVector<QPointF> out;
    for (std::size_t i = 0; i < bars.size(); ++i)
    {
        const BarRect& bar = bars[i];
        if (isEmpty(bar))
            continue;
        if (i == 0 || !joins(bars[i - 1], bar))
        {
            startRun(out);
            out << point(bar.left, bar.base, keyIsVertical);
        }
        out << point(bar.left, bar.top, keyIsVertical) << point(bar.right, bar.top, keyIsVertical);
        if (i + 1 == bars.size() || !joins(bar, bars[i + 1]))
            out << point(bar.right, bar.base, keyIsVertical);
    }
    return out;
}

void appendBarQuads(std::span<const BarRect> bars, bool keyIsVertical,
                    const std::array<float, 4>& rgba, std::vector<float>& out)
{
    std::vector<qcp::fill::StripPoint> rows;
    rows.reserve(2 * bars.size());
    for (const BarRect& bar : bars)
        if (!isEmpty(bar))
            rows.insert(rows.end(), {{bar.left, bar.base, bar.top, true},
                                     {bar.right, bar.base, bar.top, false}});
    qcp::fill::appendStrip(rows, keyIsVertical, rgba, out);
}

bool contains(std::span<const BarRect> bars, const QPointF& pos, bool keyIsVertical)
{
    return std::any_of(bars.begin(), bars.end(), [&](const BarRect& bar) {
        return !isEmpty(bar) && pixelRect(bar, keyIsVertical).contains(pos);
    });
}

} // namespace qcp::histogram
