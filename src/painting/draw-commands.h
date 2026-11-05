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
#include <QPolygonF>
#include <QRect>
#include <QString>
#include <algorithm>
#include <atomic>

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

//! A text command's two rects, relative to its anchor: the box (glyphs grown by the padding and
//! placed by the style's position alignment) and the glyphs inside it.
struct QCPTextLayout
{
    QRect box;
    QRect glyphs;
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
using QCPDrawCommand
    = std::variant<PolylineDataCmd, PolylinePositionsCmd, PolygonCmd, FillBetweenCmd, RectCmd,
                   EllipseCmd, MarkersCmd, ErrorBarsCmd, TextCmd, PathCmd, ImageCmd, PainterCmd>;
} // namespace qcp

/*!
  The layout of \a text in \a style, as QCPItemText has always drawn it: one place computes it, so
  the painter replay, the hit test and an item's anchors cannot disagree.
*/
[[nodiscard]] QCP_LIB_DECL qcp::QCPTextLayout qcpTextLayout(const QCPTextStyle& style,
                                                            const QString& text);

//! A new identity per recording; copies and re-records take a fresh one.
inline quint64 qcpNextDrawListId()
{
    static std::atomic<quint64> next { 1 };
    return next.fetch_add(1, std::memory_order_relaxed);
}

/*!
  \brief What a QCPDrawContext recorded: the commands in draw order and their data box.

  \a bounds follows the same rule as an item's data extent: a side is meaningful only while \a
  keyBounded / \a valueBounded is set, which they are only when every command recorded so far is
  data on that side. One pixel, ratio or anchored command makes that side unbounded for the rest of
  the recording, and a side that stayed bounded holds the min/max of the data coordinates.

  A log axis makes a non-positive lower bound unmappable, exactly as it does for an item's extent
  (QCPAbstractItem::positionsExtent drops that side). QCPDrawnCore::dataRange applies that rule when
  the box describes an item; a plottable's range is data, so its getters ask for the bounds without
  the rule and let the sign domain decide.
*/
struct QCP_LIB_DECL QCPDrawList
{
    QCPDrawList() = default;

    //! A copy is a new recording: it can never be matched by an image of the one it came from.
    QCPDrawList(const QCPDrawList& other) { *this = other; }

    QCPDrawList& operator=(const QCPDrawList& other)
    {
        commands = other.commands;
        bounds = other.bounds;
        keyBounded = other.keyBounded;
        valueBounded = other.valueBounded;
        hasSamples = other.hasSamples;
        viewDependent = other.viewDependent;
        anchorDependent = other.anchorDependent;
        transformDependent = other.transformDependent;
        id = qcpNextDrawListId();
        return *this;
    }

    std::vector<qcp::QCPDrawCommand> commands;
    QCPDataBox bounds { -std::numeric_limits<double>::infinity(),
                        std::numeric_limits<double>::infinity(),
                        -std::numeric_limits<double>::infinity(),
                        std::numeric_limits<double>::infinity() };
    bool keyBounded = false;
    bool valueBounded = false;
    //! False until a command with at least one sample set the box: empty commands are neutral.
    bool hasSamples = false;
    //! A command read the theme (or another view fact the commands do not carry): re-record when
    //! the view epoch moves, even if the command list itself looks view-independent.
    bool viewDependent = false;
    //! A command holds a QCP::AnchorOffset: an item owns that anchor, and removing the item
    //! invalidates this recording even though its generation did not move.
    bool anchorDependent = false;
    //! A command read the frame's transform and baked a pixel out of it (a bar width in pixels):
    //! the recording is still good after a pan, but not after a zoom or a resize.
    bool transformDependent = false;
    quint64 id = qcpNextDrawListId(); //!< unique: with a command's index it is a cache key
};

//! The one place a QCP::Position becomes pixels: every command and every caller goes through it.
[[nodiscard]] QPointF qcpResolvePosition(const QCPDrawFrame& frame, const QCP::Position& position);

//! \a positions mapped through \a frame into \a out, in pixels.
inline void qcpResolveInto(std::span<const QCP::Position> positions, const QCPDrawFrame& frame,
                           std::vector<QPointF>& out)
{
    out.resize(positions.size());
    for (std::size_t i = 0; i < positions.size(); ++i)
        out[i] = qcpResolvePosition(frame, positions[i]);
}

//! \a points as a polygon in \a out: no allocation once its storage has grown.
inline void qcpPolygonOf(std::span<const QPointF> points, QPolygonF& out)
{
    out.resize(int(points.size()));
    std::copy(points.begin(), points.end(), out.begin());
}

//! \a open with its first point appended: what a closed stroke extrudes.
inline std::vector<QPointF> qcpRingOf(std::span<const QPointF> open)
{
    std::vector<QPointF> ring(open.begin(), open.end());
    if (!ring.empty())
        ring.push_back(ring.front());
    return ring;
}

//! \a command's place in \a list: with the list's id it names the command across replays.
inline quint64 qcpCommandIndex(const QCPDrawList& list, const qcp::QCPDrawCommand& command)
{
    return quint64(&command - list.commands.data());
}

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

namespace qcp
{
class QCPRasterFallback;
}

//! The commands a full frame could not draw on the GPU (no QRhi, export, or the layer's image cap)
//! are painted here: in PainterOnly a GPU kind is drawn only when \a fallback says the layer left
//! it to this pass.
void qcpReplayWithPainter(const QCPDrawList& list, QCPPainter& painter, const QCPDrawFrame& frame,
                          QCPReplayKinds kinds = QCPReplayKinds::All,
                          const qcp::QCPRasterFallback* fallback = nullptr);
