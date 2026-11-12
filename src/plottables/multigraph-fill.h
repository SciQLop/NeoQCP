#pragma once

#include <array>
#include <span>
#include <utility>
#include <vector>

class QCPAxis;

namespace qcp::fill {

//! One row of a fill, in pixels: the key position and the two edges' value positions.
//! breakBefore: the strip does not join this row to the previous one (a key gap).
struct StripPoint
{
    double key;
    double a;
    double b;
    bool breakBefore = false;
};

//! One row of a series in data space, for fillToGraphRows: the key, its value and whether a key
//! gap starts a new run before it.
struct SeriesRow
{
    double key;
    double value;
    bool breakBefore = false;
};

//! The partner series sampled at the owner's keys over their common key range, ready for
//! fillSeries: both edges have the same number of rows. A NaN on either side, or a key gap in
//! either series, marks a break; the row after a NaN starts a new run.
std::vector<StripPoint> fillToGraphRows(std::span<const SeriesRow> owner,
                                        std::span<const SeriesRow> partner);

//! Triangles (6 floats per vertex: x, y, r, g, b, a premultiplied) between the edges of
//! consecutive rows, appended to out. A non-finite edge or breakBefore ends the strip; a segment
//! where the edges cross is split at the crossing.
void appendStrip(std::span<const StripPoint> rows, bool keyIsVertical,
                 const std::array<float, 4>& premultipliedRgba, std::vector<float>& out);

//! Rows binned into spans: per bin of binWidthPx along the key, one row at the bin centre with
//! a = min and b = max of both edges. Bins never join across a break.
//! \pre binWidthPx > 0
std::vector<StripPoint> binToSpans(std::span<const StripPoint> rows, double binWidthPx);

//! Step transforms applied to both edges, matching qcp::toStepLeftLines & co.
enum class Step { None, Left, Right, Center };
std::vector<StripPoint> stepped(std::span<const StripPoint> rows, Step step);

//! On a log value axis: the edge pixel on the side of zero, then the opposite edge.
std::pair<double, double> logZeroSideEdges(const QCPAxis& valueAxis);

//! The pixel of a fill edge or baseline at value; on a log axis a value <= 0 goes past the edge.
//! NaN stays NaN, so a missing row still breaks the strip.
double baselinePixel(const QCPAxis& valueAxis, double value);

} // namespace qcp::fill
