#pragma once

// Private to the library: what QCPDrawContext records and what qcpReplayWithPainter consumes.
// Not part of the public umbrella header (qcp.h).

#include "draw-context.h"

#include "../scatterstyle.h"
#include "../spatial-index.h"

#include <QBrush>
#include <QImage>
#include <QPainterPath>
#include <QPen>
#include <QPointF>
#include <algorithm>
#include <QString>

#include <functional>
#include <limits>
#include <memory>
#include <variant>
#include <vector>

class QCPPainter;

namespace qcp
{
//! A polyline of data-space vertices.
struct PolylineDataCmd
{
    std::shared_ptr<const std::vector<QPointF>> points;
    QPen pen;
};

//! A polyline whose vertices each carry their own space.
struct PolylinePositionsCmd
{
    std::vector<QCP::Position> points;
    QPen pen;
};

//! A filled and stroked polygon of position-space vertices.
struct PolygonCmd
{
    std::vector<QCP::Position> points;
    QPen pen;
    QBrush brush;
};

//! The area between two data-space series.
struct FillBetweenCmd
{
    std::shared_ptr<const std::vector<QPointF>> upper;
    std::shared_ptr<const std::vector<QPointF>> lower;
    QBrush brush;
};

//! A rectangle between two corners, each in its own space.
struct RectCmd
{
    QCP::Position a;
    QCP::Position b;
    QPen pen;
    QBrush brush;
};

//! An ellipse inside the rectangle between two corners.
struct EllipseCmd
{
    QCP::Position a;
    QCP::Position b;
    QPen pen;
    QBrush brush;
};

//! One scatter symbol per data-space point.
struct MarkersCmd
{
    std::shared_ptr<const std::vector<QPointF>> points;
    QCPScatterStyle style;
};

//! Vertical error bars around data-space centres; the spans are half extents in data units.
struct ErrorBarsCmd
{
    std::shared_ptr<const std::vector<QPointF>> centres;
    std::shared_ptr<const std::vector<QPointF>> spans;
    QPen pen;
};

//! Text anchored at a position. The style is shared: it carries a QFont and a QColor, which would
//! push the command past a cache line and a half, and text commands are never hot.
struct TextCmd
{
    QCP::Position at;
    QString text;
    std::shared_ptr<const QCPTextStyle> style;
};

//! A painter path whose origin is a position; the path is in pixels from there.
struct PathCmd
{
    QCP::Position origin;
    QPainterPath path;
    QPen pen;
    QBrush brush;
};

//! A raster image stretched between two corners.
struct ImageCmd
{
    QCP::Position a;
    QCP::Position b;
    QImage image;
};

//! Arbitrary painter code; the escape hatch for what has no command yet.
struct PainterCmd
{
    std::function<void(QCPPainter&, const QCPTransform&)> callback;
};

//! One draw command. Ordered by the alternatives below, so a replay is a single visit.
using QCPDrawCommand =
    std::variant<PolylineDataCmd, PolylinePositionsCmd, PolygonCmd, FillBetweenCmd, RectCmd,
                 EllipseCmd, MarkersCmd, ErrorBarsCmd, TextCmd, PathCmd, ImageCmd, PainterCmd>;
} // namespace qcp

/*!
  \brief What a QCPDrawContext recorded: the commands in draw order and their data box.

  \a bounds follows the same rule as an item's data extent: a side is meaningful only while \a
  keyBounded / \a valueBounded is set, which they are only when every command recorded so far is
  data on that side. One pixel, ratio or anchored command makes that side unbounded for the rest of
  the recording, and a side that stayed bounded holds the min/max of the data coordinates.

  A log axis makes a non-positive lower bound unmappable, exactly as it does for an item's extent
  (QCPAbstractItem::positionsExtent drops that side); that widening is not applied here yet because
  nothing culls on this box -- it becomes part of the contract when a consumer does.
*/
struct QCP_LIB_DECL QCPDrawList
{
    std::vector<qcp::QCPDrawCommand> commands;
    QCPDataBox bounds { -std::numeric_limits<double>::infinity(),
                        std::numeric_limits<double>::infinity(),
                        -std::numeric_limits<double>::infinity(),
                        std::numeric_limits<double>::infinity() };
    bool keyBounded = false;
    bool valueBounded = false;
};

//! The one place a QCP::Position becomes pixels: every command and every caller goes through it.
[[nodiscard]] QPointF qcpResolvePosition(const QCPDrawFrame& frame, const QCP::Position& position);

//! Draws \a list into \a painter through \a frame: the QPainter path (screen fallback and export).
//! What a replay is for: All paints everything (an export has no GPU pass), PainterOnly is the
//! other half of a screen frame and leaves the GPU kinds to the RHI replay.
enum class QCPReplayKinds
{
    All,
    PainterOnly
};

//! The four corners of each error bar in data space: above, below, left and right of the centre.
//! Both replays start here, so a span is never mistaken for a position.
inline void qcpErrorBarCorners(std::span<const QPointF> centres, std::span<const QPointF> spans,
                               std::vector<QPointF>& out)
{
    const std::size_t count = std::min(centres.size(), spans.size());
    out.resize(4 * count);
    for (std::size_t i = 0; i < count; ++i)
    {
        const QPointF centre = centres[i];
        const QPointF span = spans[i];
        out[4 * i + 0] = QPointF(centre.x(), centre.y() + span.y());
        out[4 * i + 1] = QPointF(centre.x(), centre.y() - span.y());
        out[4 * i + 2] = QPointF(centre.x() - span.x(), centre.y());
        out[4 * i + 3] = QPointF(centre.x() + span.x(), centre.y());
    }
}

void qcpReplayWithPainter(const QCPDrawList& list, QCPPainter& painter, const QCPDrawFrame& frame,
                          QCPReplayKinds kinds = QCPReplayKinds::All);
