#pragma once
#include <QPointF>
#include <QRectF>
#include <QStringList>
#include <array>
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
PixelBar toPixelBar(double xa, double xb, double y0, double y1, int category, int row);
void appendMerged(std::vector<PixelBar>& bars, const PixelBar& bar);
void appendQuad(std::vector<float>& out, const QRectF& rect, const std::array<float, 4>& rgba);
void appendDiamond(std::vector<float>& out, QPointF center, double halfSize,
                   const std::array<float, 4>& rgba);

} // namespace qcp::intervals
