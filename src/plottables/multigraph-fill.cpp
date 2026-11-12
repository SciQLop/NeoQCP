#include "multigraph-fill.h"
#include "../axis/axis.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace qcp::fill {
namespace {

bool finite(const StripPoint& p)
{
    return std::isfinite(p.key) && std::isfinite(p.a) && std::isfinite(p.b);
}

bool joined(const StripPoint& prev, const StripPoint& next)
{
    return finite(prev) && finite(next) && !next.breakBefore;
}

void vertex(std::vector<float>& out, bool keyIsVertical, double key, double value,
            const std::array<float, 4>& c)
{
    const float x = static_cast<float>(keyIsVertical ? value : key);
    const float y = static_cast<float>(keyIsVertical ? key : value);
    out.insert(out.end(), { x, y, c[0], c[1], c[2], c[3] });
}

using KeyValue = std::pair<double, double>;

void triangle(std::vector<float>& out, bool keyIsVertical, std::array<KeyValue, 3> pts,
              const std::array<float, 4>& c)
{
    for (auto [key, value] : pts)
        vertex(out, keyIsVertical, key, value, c);
}

void quad(std::vector<float>& out, bool v, const StripPoint& p, const StripPoint& q,
          const std::array<float, 4>& c)
{
    const double d0 = p.b - p.a, d1 = q.b - q.a;
    if (d0 * d1 >= 0)
    {
        triangle(out, v, { { { p.key, p.a }, { q.key, q.a }, { q.key, q.b } } }, c);
        triangle(out, v, { { { p.key, p.a }, { q.key, q.b }, { p.key, p.b } } }, c);
        return;
    }
    const double t = d0 / (d0 - d1);
    const double crossKey = p.key + t * (q.key - p.key);
    const double crossValue = p.a + t * (q.a - p.a);
    triangle(out, v, { { { p.key, p.a }, { crossKey, crossValue }, { p.key, p.b } } }, c);
    triangle(out, v, { { { crossKey, crossValue }, { q.key, q.a }, { q.key, q.b } } }, c);
}

void widen(StripPoint& span, const StripPoint& r)
{
    span.a = std::min({ span.a, r.a, r.b });
    span.b = std::max({ span.b, r.a, r.b });
}

bool hasPredecessor(std::span<const StripPoint> rows, std::size_t i)
{
    return i > 0 && !rows[i].breakBefore;
}

bool endsRun(std::span<const StripPoint> rows, std::size_t i)
{
    return i + 1 == rows.size() || rows[i + 1].breakBefore;
}

StripPoint withValues(const StripPoint& values, double key, bool breakBefore)
{
    return { key, values.a, values.b, breakBefore };
}

void steppedLeft(std::span<const StripPoint> rows, std::size_t i, std::vector<StripPoint>& out)
{
    const auto& r = rows[i];
    if (hasPredecessor(rows, i))
        out.push_back(withValues(rows[i - 1], r.key, false));
    out.push_back(withValues(r, r.key, r.breakBefore));
}

void steppedRight(std::span<const StripPoint> rows, std::size_t i, std::vector<StripPoint>& out)
{
    const auto& r = rows[i];
    if (hasPredecessor(rows, i))
        out.push_back(withValues(r, rows[i - 1].key, false));
    out.push_back(withValues(r, r.key, r.breakBefore));
}

void steppedCenter(std::span<const StripPoint> rows, std::size_t i, std::vector<StripPoint>& out)
{
    const auto& r = rows[i];
    if (hasPredecessor(rows, i))
    {
        const auto& prev = rows[i - 1];
        const double mid = (r.key + prev.key) * 0.5;
        out.push_back(withValues(prev, mid, false));
        out.push_back(withValues(r, mid, false));
    }
    else
        out.push_back(withValues(r, r.key, r.breakBefore));
    if (hasPredecessor(rows, i) && endsRun(rows, i))
        out.push_back(withValues(r, r.key, false));
}

} // namespace

void appendStrip(std::span<const StripPoint> rows, bool keyIsVertical,
                 const std::array<float, 4>& rgba, std::vector<float>& out)
{
    for (std::size_t i = 1; i < rows.size(); ++i)
        if (joined(rows[i - 1], rows[i]))
            quad(out, keyIsVertical, rows[i - 1], rows[i], rgba);
}

std::vector<StripPoint> fillToGraphRows(std::span<const SeriesRow> owner,
                                        std::span<const SeriesRow> partner)
{
    std::vector<StripPoint> rows;
    if (owner.empty() || partner.empty())
        return rows;
    const double lower = std::max(owner.front().key, partner.front().key);
    const double upper = std::min(owner.back().key, partner.back().key);
    if (!(upper > lower))
        return rows;

    const auto partnerAt = [&partner](double key)
    {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const auto it = std::lower_bound(
            partner.begin(), partner.end(), key,
            [](const SeriesRow& row, double at) { return row.key < at; });
        if (it != partner.end() && it->key == key)
            return std::pair<double, bool> { it->value,
                                             it->breakBefore || !std::isfinite(it->value) };
        if (it == partner.begin() || it == partner.end())
            return std::pair<double, bool> { nan, true };
        const SeriesRow& before = *(it - 1);
        const SeriesRow& after = *it;
        if (!std::isfinite(before.value) || !std::isfinite(after.value) || after.breakBefore)
            return std::pair<double, bool> { nan, true };
        const double span = after.key - before.key;
        if (span <= 0.0)
            return std::pair<double, bool> { after.value, false };
        const double t = (key - before.key) / span;
        return std::pair<double, bool> { before.value + t * (after.value - before.value), false };
    };

    bool pendingBreak = false;
    for (const SeriesRow& row : owner)
    {
        if (row.key < lower)
            continue;
        if (row.key > upper)
            break;
        const auto [partnerValue, partnerBreak] = partnerAt(row.key);
        const bool broken = pendingBreak || row.breakBefore || partnerBreak
            || !std::isfinite(row.value) || !std::isfinite(partnerValue);
        rows.push_back({ row.key, row.value, partnerValue, broken });
        pendingBreak = !std::isfinite(row.value) || !std::isfinite(partnerValue);
    }
    return rows;
}

std::vector<StripPoint> binToSpans(std::span<const StripPoint> rows, double binWidthPx)
{
    Q_ASSERT(binWidthPx > 0);
    std::vector<StripPoint> spans;
    long long currentBin = 0;
    bool pendingBreak = false;
    for (const auto& r : rows)
    {
        if (!finite(r))
        {
            pendingBreak = true;
            continue;
        }
        const auto bin = static_cast<long long>(std::floor(r.key / binWidthPx));
        const bool startsSpan = spans.empty() || pendingBreak || r.breakBefore || bin != currentBin;
        if (startsSpan)
        {
            const double centre = (static_cast<double>(bin) + 0.5) * binWidthPx;
            spans.push_back({ centre, std::min(r.a, r.b), std::max(r.a, r.b),
                              !spans.empty() && (pendingBreak || r.breakBefore) });
            currentBin = bin;
        }
        else
            widen(spans.back(), r);
        pendingBreak = false;
    }
    return spans;
}

std::vector<StripPoint> stepped(std::span<const StripPoint> rows, Step step)
{
    if (step == Step::None)
        return { rows.begin(), rows.end() };
    std::vector<StripPoint> out;
    out.reserve(rows.size() * 2);
    for (std::size_t i = 0; i < rows.size(); ++i)
        switch (step)
        {
            case Step::Left: steppedLeft(rows, i, out); break;
            case Step::Right: steppedRight(rows, i, out); break;
            default: steppedCenter(rows, i, out); break;
        }
    return out;
}

// simplify: a value <= 0 is taken as invalid on every log axis, but on an all-negative range the
// invalid values are the ones >= 0. Callers then push valid negative values to the edge. Upgrade:
// test the value's sign against the range's sign in baselinePixel and QCPMultiGraph::fillValue.
std::pair<double, double> logZeroSideEdges(const QCPAxis& valueAxis)
{
    const QCPRange range = valueAxis.range();
    const bool positive = range.upper > 0;
    return {valueAxis.coordToPixel(positive ? range.lower : range.upper),
            valueAxis.coordToPixel(positive ? range.upper : range.lower)};
}

// A log axis has no pixel for a value <= 0: the baseline goes two axis lengths past the edge on
// the side of zero, as QCPGraph::getFillBasePoint picks it. Far enough to stay off screen during
// a GPU pan, near enough to keep float precision; the scissor clips it.
double baselinePixel(const QCPAxis& valueAxis, double value)
{
    if (valueAxis.scaleType() == QCPAxis::stLinear || value > 0 || std::isnan(value))
        return valueAxis.coordToPixel(value);
    const auto [edge, other] = logZeroSideEdges(valueAxis);
    return edge + 2.0 * (edge - other);
}

} // namespace qcp::fill
