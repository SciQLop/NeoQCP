#include "multigraph-errorbars.h"
#include <cmath>
#include <limits>

namespace qcp::errorbars {

namespace {

constexpr double kNan = std::numeric_limits<double>::quiet_NaN();

double orValue(double bound, double value) { return std::isfinite(bound) ? bound : value; }

QPointF point(double key, double v, bool keyIsVertical)
{
    return keyIsVertical ? QPointF(v, key) : QPointF(key, v);
}

QPointF separator() { return { kNan, kNan }; }

void appendRun(QVector<QPointF>& out, QPointF a, QPointF b) { out << a << b; }

void appendBar(QVector<QPointF>& out, const Bar& b, double halfCap, bool vertical)
{
    appendRun(out, point(b.key, b.lower, vertical), point(b.key, b.upper, vertical));
    for (const double end : { b.lower, b.upper })
    {
        out << separator();
        appendRun(out, point(b.key - halfCap, end, vertical), point(b.key + halfCap, end, vertical));
    }
}

} // namespace

bool resolve(Bar& bar)
{
    const int finiteCount = std::isfinite(bar.value) + std::isfinite(bar.lower)
                            + std::isfinite(bar.upper);
    if (finiteCount < 2)
        return false;
    bar.lower = orValue(bar.lower, bar.value);
    bar.upper = orValue(bar.upper, bar.value);
    return true;
}

QVector<QPointF> barRuns(std::span<const Bar> bars, double capHalfWidthPx, bool keyIsVertical)
{
    QVector<QPointF> out;
    for (Bar bar : bars)
    {
        if (!std::isfinite(bar.key) || !resolve(bar))
            continue;
        if (!out.isEmpty())
            out << separator();
        appendBar(out, bar, capHalfWidthPx, keyIsVertical);
    }
    return out;
}

bool barsFit(std::span<const Bar> bars, double minSpacingPx)
{
    double previous = kNan;
    for (const Bar& bar : bars)
    {
        if (!std::isfinite(bar.key))
            continue;
        if (std::abs(bar.key - previous) < minSpacingPx)
            return false;
        previous = bar.key;
    }
    return true;
}

} // namespace qcp::errorbars
