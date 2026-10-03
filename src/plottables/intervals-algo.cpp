#include "intervals-algo.h"
#include <limits>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace qcp::intervals {

std::optional<std::string> invalidColumns(const Columns& c)
{
    const auto n = c.start.size();
    if (c.stop.size() != n || c.lane.size() != n || c.category.size() != n || c.ids.size() != n)
        return "start, stop, lane, category and ids must have the same length";
    if (!c.labels.isEmpty() && static_cast<std::size_t>(c.labels.size()) != n)
        return "labels must be empty or have one entry per interval";
    for (std::size_t i = 0; i < n; ++i)
    {
        if (std::isnan(c.start[i]) || std::isnan(c.stop[i]))
            return "start and stop must not contain NaN";
        if (c.stop[i] < c.start[i])
            return "stop must not be before start";
    }
    return std::nullopt;
}

std::vector<LaneRows> groupByLane(const Columns& c, int laneCount)
{
    std::vector<LaneRows> lanes(std::max(0, laneCount));
    for (int row = 0; row < static_cast<int>(c.start.size()); ++row)
        if (c.lane[row] >= 0 && c.lane[row] < laneCount)
            lanes[c.lane[row]].rows.push_back(row);
    for (auto& lane : lanes)
    {
        std::ranges::sort(lane.rows, {}, [&](int row) { return c.start[row]; });
        lane.starts.reserve(lane.rows.size());
        for (int row : lane.rows)
        {
            lane.starts.push_back(c.start[row]);
            lane.maxDuration = std::max(lane.maxDuration, c.stop[row] - c.start[row]);
        }
    }
    return lanes;
}

int packSubRows(const LaneRows& lane, const Columns& c, std::vector<int>& subRow)
{
    std::vector<double> rowEnds; // where each sub-row is busy until
    for (int row : lane.rows)
    {
        auto free = std::ranges::find_if(rowEnds, [&](double end) { return end <= c.start[row]; });
        if (free == rowEnds.end())
            free = rowEnds.insert(rowEnds.end(), c.stop[row]);
        else
            *free = c.stop[row];
        subRow[row] = static_cast<int>(free - rowEnds.begin());
    }
    return static_cast<int>(rowEnds.size());
}

std::vector<int> categoryRanks(const Columns& c, const std::vector<int>& order)
{
    const int count = c.category.empty() ? 0 : *std::ranges::max_element(c.category) + 1;
    std::vector<bool> used(std::max(0, count), false);
    for (int category : c.category)
        if (category >= 0)
            used[category] = true;
    std::vector<int> rank(used.size(), -1);
    int next = 0;
    auto place = [&](int category) {
        if (category >= 0 && category < count && used[category] && rank[category] < 0)
            rank[category] = next++;
    };
    std::ranges::for_each(order, place);
    std::ranges::for_each(c.category, place);
    return rank;
}

int packByCategory(const LaneRows& lane, const Columns& c, const std::vector<int>& rank,
                   std::vector<int>& subRow)
{
    std::vector<int> categories;
    for (int row : lane.rows)
        if (std::ranges::find(categories, c.category[row]) == categories.end())
            categories.push_back(c.category[row]);
    std::ranges::sort(categories, {}, [&](int category) { return rank[category]; });
    for (int row : lane.rows)
        subRow[row] = static_cast<int>(std::ranges::find(categories, c.category[row]) - categories.begin());
    return static_cast<int>(categories.size());
}

std::pair<int, int> candidateRange(const LaneRows& lane, double lower, double upper)
{
    const auto first = std::ranges::lower_bound(lane.starts, lower - lane.maxDuration);
    const auto last = std::ranges::upper_bound(lane.starts, upper);
    return { static_cast<int>(first - lane.starts.begin()),
             static_cast<int>(last - lane.starts.begin()) };
}

PixelBar toPixelBar(double xa, double xb, double y0, double y1, int category, int row)
{
    const bool instant = xa == xb;
    auto minmax_vals = std::minmax(xa, xb);
    double x0 = minmax_vals.first;
    double x1 = minmax_vals.second;
    if (x1 - x0 < 1.0)
    {
        const double centre = (x0 + x1) / 2;
        x0 = centre - 0.5;
        x1 = centre + 0.5;
    }
    return { x0, x1, y0, y1, category, row, instant };
}

void appendMerged(std::vector<PixelBar>& bars, const PixelBar& bar, bool sameLabel)
{
    if (!bars.empty())
    {
        auto& last = bars.back();
        const bool mergeable = !bar.instant && !last.instant && bar.category == last.category
            && bar.y0 == last.y0 && bar.x0 <= last.x1 && bar.x1 >= last.x0;
        if (mergeable)
        {
            last.x0 = std::min(last.x0, bar.x0);
            last.x1 = std::max(last.x1, bar.x1);
            if (!sameLabel)
                last.row = -1;
            return;
        }
    }
    bars.push_back(bar);
}

namespace {
void appendVertex(std::vector<float>& out, QPointF p, const std::array<float, 4>& rgba)
{
    out.insert(out.end(), { static_cast<float>(p.x()), static_cast<float>(p.y()), rgba[0], rgba[1],
                            rgba[2], rgba[3] });
}
} // namespace

void appendQuad(std::vector<float>& out, const QRectF& rect, const std::array<float, 4>& rgba)
{
    for (const QPointF p : { rect.topLeft(), rect.topRight(), rect.bottomRight(), rect.topLeft(),
                             rect.bottomRight(), rect.bottomLeft() })
        appendVertex(out, p, rgba);
}

void appendDiamond(std::vector<float>& out, QPointF c, double h, const std::array<float, 4>& rgba)
{
    const QPointF top(c.x(), c.y() - h), right(c.x() + h, c.y()), bottom(c.x(), c.y() + h),
        left(c.x() - h, c.y());
    for (const QPointF p : { top, right, bottom, top, bottom, left })
        appendVertex(out, p, rgba);
}

void appendFan(std::vector<float>& out, const QPolygonF& polygon, const std::array<float, 4>& rgba)
{
    for (qsizetype i = 1; i + 1 < polygon.size(); ++i)
        for (const QPointF p : { polygon[0], polygon[i], polygon[i + 1] })
            appendVertex(out, p, rgba);
}

QPolygonF busShape(const QRectF& rect, double slant)
{
    const double s = std::min(slant, rect.width() / 2);
    const double mid = rect.center().y();
    return QPolygonF({ { rect.left(), mid }, { rect.left() + s, rect.top() },
                       { rect.right() - s, rect.top() }, { rect.right(), mid },
                       { rect.right() - s, rect.bottom() }, { rect.left() + s, rect.bottom() } });
}

std::optional<QString> fittedLabel(const QFontMetricsF& fm, const QString& text, double width)
{
    if (fm.horizontalAdvance(text) <= width)
        return text;
    const QString elided = fm.elidedText(text, Qt::ElideRight, width);
    const qsizetype kept = elided.endsWith(QChar(0x2026)) ? elided.size() - 1 : elided.size();
    if (kept < std::min<qsizetype>(3, text.size()))
        return std::nullopt;
    return elided;
}

int shiftLane(int lane, int steps, const std::vector<int>& displayOrder)
{
    const auto it = std::ranges::find(displayOrder, lane);
    if (it == displayOrder.end())
        return lane;
    const auto position = std::clamp<std::ptrdiff_t>(it - displayOrder.begin() + steps, 0,
                                                     std::ssize(displayOrder) - 1);
    return displayOrder[position];
}

std::vector<Edit> applyDrag(const std::vector<DraggedRow>& rows, DragKind kind, double dt,
                            int laneSteps, const std::vector<int>& displayOrder)
{
    std::vector<Edit> edits;
    edits.reserve(rows.size());
    for (const auto& r : rows)
    {
        switch (kind)
        {
            case DragKind::Move:
                edits.push_back({ r.row, r.start + dt, r.stop + dt,
                                  shiftLane(r.lane, laneSteps, displayOrder) });
                break;
            case DragKind::ResizeLeft:
                edits.push_back({ r.row, std::min(r.start + dt, r.stop), r.stop, r.lane });
                break;
            case DragKind::ResizeRight:
                edits.push_back({ r.row, r.start, std::max(r.stop + dt, r.start), r.lane });
                break;
        }
    }
    return edits;
}

std::optional<std::pair<double, double>> freeShiftRange(double a, double b,
                                                        const std::vector<Span>& obstacles)
{
    double gapStart = -std::numeric_limits<double>::infinity();
    double gapEnd = std::numeric_limits<double>::infinity();
    for (const auto& o : obstacles)
    {
        if (o.start < b && o.stop > a)
            return std::nullopt;
        if (o.stop <= a)
            gapStart = std::max(gapStart, o.stop);
        if (o.start >= b)
            gapEnd = std::min(gapEnd, o.start);
    }
    return std::pair { gapStart - a, gapEnd - b };
}

std::vector<double> movingEdges(const DraggedRow& grabbed, DragKind kind)
{
    switch (kind)
    {
        case DragKind::ResizeLeft:
            return { grabbed.start };
        case DragKind::ResizeRight:
            return { grabbed.stop };
        case DragKind::Move:
            break;
    }
    return { grabbed.start, grabbed.stop };
}

double snapToStep(double edge, double dt, double step)
{
    if (step <= 0)
        return dt;
    return std::round((edge + dt) / step) * step - edge;
}

double snapToEdges(const std::vector<double>& movingEdges, double dt,
                   const std::vector<double>& sortedCandidates, double tolerance)
{
    double best = dt, bestDistance = tolerance;
    for (double edge : movingEdges)
    {
        const double target = edge + dt;
        const auto it = std::ranges::lower_bound(sortedCandidates, target);
        for (auto c : { it, it == sortedCandidates.begin() ? it : std::prev(it) })
            if (c != sortedCandidates.end() && std::abs(*c - target) <= bestDistance)
            {
                bestDistance = std::abs(*c - target);
                best = *c - edge;
            }
    }
    return best;
}

} // namespace qcp::intervals
