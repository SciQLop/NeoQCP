#pragma once

#include "../axis/scale.h"
#include <QPointF>
#include <array>
#include <memory>
#include <span>
#include <utility>
#include <vector>

class QCPAxis;
class QCPDrawContext;
class QCPGraph;

namespace qcp::fill {

//! One row of a fill, in pixels: the key position and the two edges' value positions.
//! breakBefore: the strip does not join this row to the previous one (a key gap).
//! sourceRow: the owner source row this row came from (-1 when the shaping made one up).
struct StripPoint
{
    double key;
    double a;
    double b;
    bool breakBefore = false;
    int sourceRow = -1;
};

//! One row of a series in data space, for unionRows: the key, its value, whether a key gap starts
//! a new run before it, and the owner source row it came from.
struct SeriesRow
{
    double key;
    double value;
    bool breakBefore = false;
    int row = -1;
};

//! Whether a fill may reach \a partner: the same coordinate system, or, on a cartesian frame,
//! across value axes.
bool canFillTo(const QCPGraph& owner, const QCPGraph& partner);

//! 2.x's channel between y and y2: both cartesian, on one key axis (so one area), with another
//! value axis of the same orientation. The partner's values then map through pixels.
bool fillsAcrossValueAxes(const QCPGraph& owner, const QCPGraph& partner);

//! The rows of a fill between two lines: the union of both lines' keys over the key range the two
//! spans share, each line interpolated linearly at the other's keys, with rows at the crop ends.
//! A line of fewer than two rows is a constant: it does not limit the range. An edge that is not
//! finite, or a row that follows one, carries breakBefore, so the fill breaks there.
//! sourceRow is the row of \a first the key came from, or -1 for a key the union made up.
std::vector<StripPoint> unionRows(std::span<const SeriesRow> first,
                                  std::span<const SeriesRow> second);

//! One side of a fill: the rows of one run, and whether \a first is the upper edge there.
struct SideRun
{
    std::vector<StripPoint> rows;
    bool firstAbove;
};

//! Splits rows where the two edges cross, so each run draws in one brush. A crossing inside a
//! segment makes a row at the exact crossing (both edges equal, sourceRow -1) that ends one run
//! and starts the next; a crossing on a data key shares that row between the two runs. Equal
//! edges belong to the run they are in, so lines that touch without crossing stay one run.
//! A break (breakBefore, or an edge that is not finite) ends a run without a made-up row.
std::vector<SideRun> splitAtCrossings(std::span<const StripPoint> rows);

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

//! The value a fill edge at \a value is drawn at: a value a log axis cannot place goes to the range
//! bound on zero's side (the upper bound above a positive range, the lower one below), as 2.x's
//! getFillBasePoint closed it. NaN stays NaN.
double fillValue(const QCPAxis& valueAxis, double value);

//! stepped(), in the space the axis' reader sees: on a log or symlog key axis a centre step cuts
//! its corners at equal screen spacing, not equal key spacing.
std::vector<StripPoint> steppedOnScreen(std::vector<StripPoint> rows, Step step,
                                        const QCPScale& keyScale);

//! Whether \a rowCount rows outnumber four per pixel of \a keyAxis' axis rect (the lines' own L2
//! density), i.e. whether binning narrows them at all.
bool binsWhenDense(const QCPAxis& keyAxis, std::size_t rowCount);

//! Rows binned when they outnumber four per pixel of \a keyAxis' axis rect, the bin a quarter
//! pixel wide in the axis' scale.
std::vector<StripPoint> binnedWhenDense(const QCPAxis& keyAxis, std::vector<StripPoint> rows);

//! binnedWhenDense(), for a recording that then depends on the transform: the bin still comes from
//! the axis' range, but a replay at another zoom has to remake it, so the transform is read when
//! and only when it bins.
std::vector<StripPoint> binnedForAxis(QCPDrawContext& context, const QCPAxis& keyAxis,
                                      std::vector<StripPoint> rows);

//! The two edges of a fill, as the equal-length data-space series a recording takes. A gap pinches
//! them together at the rows either side of it, so the fill crosses no break. Both are null when
//! the rows hold fewer than two drawable points.
using FillSeries = std::pair<std::shared_ptr<const std::vector<QPointF>>,
                             std::shared_ptr<const std::vector<QPointF>>>;
FillSeries fillSeries(std::span<const StripPoint> rows);

} // namespace qcp::fill
