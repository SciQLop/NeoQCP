#pragma once
#include <QFontMetricsF>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QStringList>
#include <array>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace qcp::intervals {

struct Columns
{
    std::vector<double> start, stop;
    std::vector<int> lane, category;
    std::vector<qint64> ids;
    QStringList labels; // empty, or one per row
};

struct LaneRows
{
    std::vector<int> rows; // sorted by start
    std::vector<double> starts;
    double maxDuration = 0;
};

struct PixelBar
{
    double x0, x1, y0, y1;
    int category;
    int row; // -1 once merged with a neighbour
    bool instant;
};

std::optional<std::string> invalidColumns(const Columns& c);
std::vector<LaneRows> groupByLane(const Columns& c, int laneCount);
std::pair<int, int> candidateRange(const LaneRows& lane, double lower, double upper);
//! Puts each interval of \a lane in the first sub-row free at its start (greedy interval
//! packing), writing \a subRow[row]; returns the number of sub-rows used.
int packSubRows(const LaneRows& lane, const Columns& c, std::vector<int>& subRow);
//! Rank of each category index (-1 if the data never uses it): those in \a order first, in that
//! order, then the others in first-seen order.
std::vector<int> categoryRanks(const Columns& c, const std::vector<int>& order);
//! One sub-row per category the lane uses, ordered by \a rank; writes \a subRow[row] and
//! returns the number of sub-rows.
int packByCategory(const LaneRows& lane, const Columns& c, const std::vector<int>& rank,
                   std::vector<int>& subRow);
PixelBar toPixelBar(double xa, double xb, double y0, double y1, int category, int row);
//! Merges \a bar into the previous one when they overlap with the same category. The merged bar
//! keeps its row (and so its label) only if \a sameLabel says both rows show the same text.
void appendMerged(std::vector<PixelBar>& bars, const PixelBar& bar, bool sameLabel = false);
void appendQuad(std::vector<float>& out, const QRectF& rect, const std::array<float, 4>& rgba);
void appendDiamond(std::vector<float>& out, QPointF center, double halfSize,
                   const std::array<float, 4>& rgba);
//! Triangles of a convex polygon, fanned from its first point.
void appendFan(std::vector<float>& out, const QPolygonF& polygon, const std::array<float, 4>& rgba);
//! A bus value as wave viewers draw it: a bar whose ends are angled by \a slant pixels.
QPolygonF busShape(const QRectF& rect, double slant);
//! \a text if it fits in \a width, else elided with at least 3 characters kept, else nothing.
std::optional<QString> fittedLabel(const QFontMetricsF& fm, const QString& text, double width);

struct DraggedRow
{
    int row;
    double start, stop;
    int lane;
};

enum class DragKind
{
    Move,
    ResizeLeft,
    ResizeRight
};

struct Edit
{
    int row;
    double start, stop;
    int lane;
};

int shiftLane(int lane, int steps, const std::vector<int>& displayOrder);
std::vector<Edit> applyDrag(const std::vector<DraggedRow>& rows, DragKind kind, double dt,
                            int laneSteps, const std::vector<int>& displayOrder);
std::vector<double> movingEdges(const DraggedRow& grabbed, DragKind kind);

struct Span
{
    double start, stop;
};

//! Shifts [lo, hi] that keep [a, b] in its free gap among \a obstacles (touching is allowed).
//! Nothing if [a, b] already overlaps one: there is no gap to stay in.
std::optional<std::pair<double, double>> freeShiftRange(double a, double b,
                                                        const std::vector<Span>& obstacles);
double snapToStep(double edge, double dt, double step);
double snapToEdges(const std::vector<double>& movingEdges, double dt,
                   const std::vector<double>& sortedCandidates, double tolerance);

} // namespace qcp::intervals
