#include "drawable.h"

#include "coordinate-system.h"
#include "core.h"
#include "items/item.h"
#include "layer.h"
#include "layoutelements/layoutelement-axisrect.h"
#include "misuse.h"
#include "painting/command-replay-rhi.h"
#include "painting/draw-commands.h"
#include "painting/draw-context.h"
#include "painting/painter.h"
#include "painting/plottable-rhi-layer.h"
#include "painting/raster-fallback.h"
#include "painting/viewport-offset.h"
#include "theme.h"
#include "plottables/plottable-draw-utils.h"
#include "plottables/plottable.h"
#include "vector2d.h"

#include <QDebug>
#include <QPainter>
#include <QTransform>
#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
constexpr double kInfinity = std::numeric_limits<double>::infinity();

//! True when a replay of \a list depends on the view, not only on the transform: the commands that
//! measure or rasterise in pixels.
bool usesTheView(const QCPDrawList& list)
{
    if (list.viewDependent)
        return true;
    for (const qcp::QCPDrawCommand& command : list.commands)
    {
        if (std::holds_alternative<qcp::TextCmd>(command)
            || std::holds_alternative<qcp::PathCmd>(command)
            || std::holds_alternative<qcp::RoundedRectCmd>(command)
            || std::holds_alternative<qcp::BoxedTextCmd>(command)
            || std::holds_alternative<qcp::PainterCmd>(command))
            return true;
        // An image with two data corners is a textured quad, which a pan moves; any other corner
        // keeps it a picture the painter draws again.
        if (std::holds_alternative<qcp::ImageCmd>(command) && !qcp::isGpuCommand(command))
            return true;
    }
    return false;
}

//! Half the pen's stroke: how far a stroke reaches from its baseline.
double halfPen(const QPen& pen)
{
    return pen.style() == Qt::NoPen ? 0.0 : std::max(1.0, pen.widthF()) / 2.0;
}

//! True when a fill is painted at all.
bool visibleBrush(const QBrush& brush)
{
    return brush.style() != Qt::NoBrush && brush.color().alpha() > 0;
}

bool finite(const QPointF& point)
{
    return std::isfinite(point.x()) && std::isfinite(point.y());
}

//! True when \a value is within \a grow of the mapped \a side: \a lower..\a upper are pixels.
bool nearSide(double lower, double upper, double value, double grow)
{
    if (!std::isfinite(lower) || !std::isfinite(upper))
        return true; // an unbounded side holds every point, so it cannot reject one
    return value >= std::min(lower, upper) - grow && value <= std::max(lower, upper) + grow;
}

//! The distance from \a at to the polyline \a points, in pixels; infinite for an empty polyline.
double distanceTo(std::span<const QPointF> points, const QPointF& at, bool closed)
{
    if (points.empty())
        return kInfinity;
    if (points.size() == 1)
        return QCPVector2D(at - points.front()).length();
    const QCPVector2D point(at);
    double best = kInfinity;
    const auto segment = [&](const QPointF& from, const QPointF& to)
    {
        if (!finite(from) || !finite(to))
            return;
        best = std::min(best, point.distanceSquaredToLine(QCPVector2D(from), QCPVector2D(to)));
    };
    for (std::size_t i = 0; i + 1 < points.size(); ++i)
        segment(points[i], points[i + 1]);
    if (closed && points.size() > 2)
        segment(points.back(), points.front());
    return std::sqrt(best);
}

//! The distance from \a at to \a rect, 0 inside: the gap to its nearest point, not to a corner.
double distanceTo(const QRectF& rect, const QPointF& at)
{
    const double dx = std::max({ rect.left() - at.x(), 0.0, at.x() - rect.right() });
    const double dy = std::max({ rect.top() - at.y(), 0.0, at.y() - rect.bottom() });
    return std::hypot(dx, dy);
}

std::vector<QPointF> resolve(std::span<const QCP::Position> positions, const QCPDrawFrame& frame)
{
    std::vector<QPointF> pixels;
    qcpResolveInto(positions, frame, pixels);
    return pixels;
}

std::vector<QPointF> resolve(const std::vector<QPointF>& data, const QCPDrawFrame& frame)
{
    std::vector<QPointF> pixels;
    qcpMapDataInto(data, frame.transform, pixels);
    return pixels;
}

//! Where a hit test resolves the one polyline it measures: a click visits every candidate, and a
//! vector per candidate would allocate and free for each of them.
std::vector<QPointF>& measuredPixels()
{
    thread_local std::vector<QPointF> pixels;
    return pixels;
}

//! The distance from \a at to the text's box, in pixels: the box turns with the text.
double textDistance(const qcp::TextCmd& cmd, const QCPDrawFrame& frame, const QPointF& at)
{
    const QCPTextStyle& style = *cmd.style;
    QPointF local = at - qcpResolvePosition(frame, cmd.at);
    if (style.rotationDegrees != 0.0)
        local = QTransform().rotate(-style.rotationDegrees).map(local);
    return distanceTo(QRectF(qcpTextLayout(*cmd.style, cmd.glyphSize).box), local);
}

//! The distance from \a at to one command's geometry, in pixels. For a batch, \a index receives
//! the place of the rect (or ellipse) that came nearest.
double distanceTo(const qcp::QCPDrawCommand& command, const QCPDrawFrame& frame, const QPointF& at,
                  int* index = nullptr)
{
    return std::visit(
        [&](const auto& cmd) -> double
        {
            using Command = std::decay_t<decltype(cmd)>;
            if constexpr (std::is_same_v<Command, qcp::PolylineDataCmd>)
            {
                std::vector<QPointF>& pixels = measuredPixels();
                qcpMapDataInto(*cmd.points, frame.transform, pixels);
                return distanceTo(pixels, at, false) - halfPen(cmd.pen);
            }
            else if constexpr (std::is_same_v<Command, qcp::PolylinePositionsCmd>)
            {
                std::vector<QPointF>& pixels = measuredPixels();
                qcpResolveInto(cmd.points, frame, pixels);
                return distanceTo(pixels, at, false) - halfPen(cmd.pen);
            }
            else if constexpr (std::is_same_v<Command, qcp::PolygonCmd>)
            {
                std::vector<QPointF>& pixels = measuredPixels();
                qcpResolveInto(cmd.points, frame, pixels);
                if (visibleBrush(cmd.brush)
                    && QPolygonF(QVector<QPointF>(pixels.begin(), pixels.end()))
                           .containsPoint(at, Qt::OddEvenFill))
                    return 0.0;
                return distanceTo(pixels, at, true) - halfPen(cmd.pen);
            }
            else if constexpr (std::is_same_v<Command, qcp::FillBetweenCmd>)
            {
                const std::vector<QPointF> upper = resolve(*cmd.upper, frame);
                const std::vector<QPointF> lower = resolve(*cmd.lower, frame);
                double best = std::min(distanceTo(upper, at, false), distanceTo(lower, at, false));
                if (visibleBrush(cmd.brush) && !upper.empty() && !lower.empty())
                {
                    QPolygonF ribbon;
                    for (const QPointF& point : upper)
                        ribbon << point;
                    for (auto it = lower.rbegin(); it != lower.rend(); ++it)
                        ribbon << *it;
                    if (ribbon.containsPoint(at, Qt::OddEvenFill))
                        return 0.0;
                }
                return best;
            }
            else if constexpr (std::is_same_v<Command, qcp::RectCmd>)
            {
                const QRectF box
                    = QRectF(qcpResolvePosition(frame, cmd.a), qcpResolvePosition(frame, cmd.b))
                          .normalized();
                if (visibleBrush(cmd.brush) && box.contains(at))
                    return 0.0;
                const std::array<QPointF, 4> corners { box.topLeft(), box.topRight(),
                                                       box.bottomRight(), box.bottomLeft() };
                return distanceTo(corners, at, true) - halfPen(cmd.pen);
            }
            else if constexpr (std::is_same_v<Command, qcp::EllipseCmd>)
            {
                const QRectF box
                    = QRectF(qcpResolvePosition(frame, cmd.a), qcpResolvePosition(frame, cmd.b))
                          .normalized();
                if (visibleBrush(cmd.brush) && box.contains(at))
                {
                    const double nx = 2.0 * (at.x() - box.center().x()) / box.width();
                    const double ny = 2.0 * (at.y() - box.center().y()) / box.height();
                    if (nx * nx + ny * ny <= 1.0)
                        return 0.0;
                }
                return distanceTo(qcpRingOf(qcpEllipseRing(box)), at, true) - halfPen(cmd.pen);
            }
            else if constexpr (std::is_same_v<Command, qcp::LineCmd>)
            {
                const std::array<QPointF, 2> ends { qcpResolvePosition(frame, cmd.a),
                                                    qcpResolvePosition(frame, cmd.b) };
                return distanceTo(ends, at, false) - halfPen(cmd.pen);
            }
            else if constexpr (std::is_same_v<Command, qcp::CircleCmd>)
            {
                const QPointF centre = qcpResolvePosition(frame, cmd.centre);
                const QRectF box(centre.x() - cmd.radius, centre.y() - cmd.radius,
                                 2.0 * cmd.radius, 2.0 * cmd.radius);
                if (visibleBrush(cmd.brush) && QLineF(centre, at).length() <= cmd.radius)
                    return 0.0;
                return distanceTo(qcpRingOf(qcpEllipseRing(box)), at, true) - halfPen(cmd.pen);
            }
            else if constexpr (std::is_same_v<Command, qcp::RoundedRectCmd>)
            {
                // Deliberate approximation: a rounded rect answers as its square corners do, so a
                // click beside a rounded corner of an unfilled one is a hit it does not draw. The
                // radius is a few pixels; the box is what the user aims at.
                const QRectF box
                    = QRectF(qcpResolvePosition(frame, cmd.a), qcpResolvePosition(frame, cmd.b))
                          .normalized();
                if (visibleBrush(cmd.shape->brush) && box.contains(at))
                    return 0.0;
                const std::array<QPointF, 4> corners { box.topLeft(), box.topRight(),
                                                       box.bottomRight(), box.bottomLeft() };
                return distanceTo(corners, at, true) - halfPen(cmd.shape->pen);
            }
            else if constexpr (std::is_same_v<Command, qcp::BoxedTextCmd>)
            {
                const QRectF box
                    = QRectF(qcpResolvePosition(frame, cmd.a), qcpResolvePosition(frame, cmd.b))
                          .normalized();
                return distanceTo(box, at);
            }
            else if constexpr (std::is_same_v<Command, qcp::RectsCmd>)
            {
                // A batch is one command over many rects: the caller gets the distance to the
                // nearest one and, in \a index, which one it was.
                const qcp::BatchBoxes& boxes = *cmd.boxes;
                const std::size_t count = std::min(boxes.aKey.size(), boxes.bKey.size());
                double best = kInfinity;
                for (std::size_t i = 0; i < count; ++i)
                {
                    const QRectF box
                        = QRectF(
                              qcpResolvePosition(frame, QCP::Data { boxes.aKey[i], boxes.aValue[i] }),
                              qcpResolvePosition(frame, QCP::Data { boxes.bKey[i], boxes.bValue[i] }))
                              .normalized();
                    const QBrush rectBrush
                        = i < boxes.fills.size() ? QBrush(QColor::fromRgba(boxes.fills[i])) : cmd.brush;
                    double distance = 0.0;
                    if (!visibleBrush(rectBrush) || !box.contains(at))
                    {
                        const std::array<QPointF, 4> corners { box.topLeft(), box.topRight(),
                                                               box.bottomRight(),
                                                               box.bottomLeft() };
                        distance = distanceTo(corners, at, true) - halfPen(cmd.pen);
                    }
                    if (distance < best)
                    {
                        best = distance;
                        if (index)
                            *index = int(i);
                    }
                }
                return best;
            }
            else if constexpr (std::is_same_v<Command, qcp::EllipsesCmd>)
            {
                const qcp::BatchBoxes& boxes = *cmd.boxes;
                const std::size_t count = std::min(boxes.aKey.size(), boxes.bKey.size());
                double best = kInfinity;
                for (std::size_t i = 0; i < count; ++i)
                {
                    const QRectF box
                        = QRectF(
                              qcpResolvePosition(frame, QCP::Data { boxes.aKey[i], boxes.aValue[i] }),
                              qcpResolvePosition(frame, QCP::Data { boxes.bKey[i], boxes.bValue[i] }))
                              .normalized();
                    const QBrush rectBrush
                        = i < boxes.fills.size() ? QBrush(QColor::fromRgba(boxes.fills[i])) : cmd.brush;
                    double distance = kInfinity;
                    if (visibleBrush(rectBrush) && box.contains(at))
                    {
                        const double nx = 2.0 * (at.x() - box.center().x()) / box.width();
                        const double ny = 2.0 * (at.y() - box.center().y()) / box.height();
                        if (nx * nx + ny * ny <= 1.0)
                            distance = 0.0;
                    }
                    if (distance > 0.0)
                        distance = distanceTo(qcpRingOf(qcpEllipseRing(box)), at, true)
                            - halfPen(cmd.pen);
                    if (distance < best)
                    {
                        best = distance;
                        if (index)
                            *index = int(i);
                    }
                }
                return best;
            }
            else if constexpr (std::is_same_v<Command, qcp::MarkersCmd>)
            {
                const std::vector<QPointF> pixels = resolve(*cmd.points, frame);
                return distanceTo(pixels, at, false) - cmd.style->size() / 2.0;
            }
            else if constexpr (std::is_same_v<Command, qcp::ErrorBarsCmd>)
            {
                std::vector<QPointF> pixels;
                qcpErrorBarPixels(frame, cmd, pixels);
                return distanceTo(pixels, at, false) - halfPen(cmd.pen);
            }
            else if constexpr (std::is_same_v<Command, qcp::TextCmd>)
                return textDistance(cmd, frame, at);
            else if constexpr (std::is_same_v<Command, qcp::PathCmd>)
            {
                QPainterPath path = cmd.path;
                path.translate(qcpResolvePosition(frame, cmd.origin));
                if (visibleBrush(cmd.brush) && path.contains(at))
                    return 0.0;
                double best = kInfinity;
                for (const QPolygonF& subpath : path.toSubpathPolygons())
                {
                    const std::span<const QPointF> points(subpath.constData(),
                                                          std::size_t(subpath.size()));
                    best = std::min(best, distanceTo(points, at, false));
                }
                return best - halfPen(cmd.pen);
            }
            else if constexpr (std::is_same_v<Command, qcp::ImageCmd>)
            {
                const QRectF box
                    = QRectF(qcpResolvePosition(frame, cmd.a), qcpResolvePosition(frame, cmd.b))
                          .normalized();
                return distanceTo(box, at);
            }
            else // painter(): opaque to the default hit test
                return kInfinity;
        },
        command);
}
} // namespace

struct QCPDrawnCore::State
{
    QCPDrawList recording;
    QCPDrawList icon;
    QCPResolveCache resolve;
    qcp::QCPRasterFallback raster;
    QCPRecordingCache geometry;
    quint64 recordedGeneration = 0;
    quint64 recordedEpoch = 0;
    quint64 recordedItemsEpoch = 0;
    quint64 recordedFrame = 0;
    quint64 recordedDependency = 0;
    const void* recordedSource = nullptr;
    bool recordedExporting = false;
    QCPTransform recordedTransform;
    //! The ranges the recording was last painted at: the layer shifts from here on a pan.
    QCPRange paintedKey;
    QCPRange paintedValue;
    bool painted = false;
    bool movesWithTheData = false;
    quint64 iconGeneration = 0;
    quint64 iconEpoch = 0;
    quint64 iconDependency = 0;
    const void* iconSource = nullptr;
    int recordings = 0;
    bool recorded = false;
    bool viewDependent = false;
    bool anchorDependent = false;
    bool transformDependent = false;
    bool iconRecorded = false;
    bool reportedMissingAxis = false;
    //! The generation at which a recording without axes was refused, and whether that stands: the
    //! answer costs one draw to learn, and no other fact can change it while the content does not.
    quint64 refusedAxesGeneration = 0;
    bool refusedMissingAxis = false;
};

namespace
{
/*!
  Marks the object as drawing for the guard's lifetime: a update() from inside a recording
  is a misuse, since it would re-record on every frame.
*/
class InsideDraw
{
public:
    explicit InsideDraw(bool& flag) : mFlag(flag) { mFlag = true; }

    ~InsideDraw() { mFlag = false; }

    InsideDraw(const InsideDraw&) = delete;
    InsideDraw& operator=(const InsideDraw&) = delete;

private:
    bool& mFlag;
};
} // namespace

int qcpHitPart(const QVariant& details)
{
    const QVariantMap map = details.toMap();
    const auto part = map.constFind(QStringLiteral("part"));
    return part == map.constEnd() ? -1 : part->toInt();
}

QCPRange qcpDrawnRange(const std::optional<QCPDataBox>& box, QCPDrawnSide side, bool& foundRange,
                       QCP::SignDomain inSignDomain)
{
    foundRange = false;
    if (!box)
        return { };
    const bool key = side == QCPDrawnSide::Key;
    double lower = key ? box->keyLower : box->valueLower;
    double upper = key ? box->keyUpper : box->valueUpper;
    foundRange = std::isfinite(lower) && std::isfinite(upper) && lower <= upper;
    if (!foundRange)
        return { };
    if (inSignDomain == QCP::sdPositive)
        lower = std::max(lower, 0.0);
    else if (inSignDomain == QCP::sdNegative)
        upper = std::min(upper, 0.0);
    if (!(lower <= upper))
    {
        foundRange = false;
        return { };
    }
    return QCPRange(lower, upper);
}

namespace
{
// A layer paints its items through one frame: the same axes, rect and clip, within one replot.
// Taking the axes' snapshots per item costs more than the item's own recording check, so the frame
// of the last paint is kept and reused while nothing it was made from has changed. Proof that the
// key is enough: an axis moves only between frames - a range change, a layout pass and a viewport
// change all happen before the layers draw, and a frame epoch separates them.
struct FrameMemo
{
    const QCustomPlot* plot = nullptr;
    quint64 epoch = 0;
    const QCPCoordinateSystem* system = nullptr;
    quint64 revision = 0;
    const QCPAxis* keyAxis = nullptr;
    const QCPAxis* valueAxis = nullptr;
    const QCPAxisRect* axisRect = nullptr;
    const QCPArea* area = nullptr;
    QCPRange keyRange;
    QCPRange valueRange;
    QRect clip;
    QCPDrawFrame frame;
};

FrameMemo& frameMemo()
{
    thread_local FrameMemo memo;
    return memo;
}

//! The frame \a view draws through, built from nothing else: the system's, or the axes' pair.
//! Whoever must keep its own frame while something else resolves another one calls this, never
//! memoFrame: that one owns a single slot and rewrites it in place.
QCPDrawFrame makeFrame(const QCPDrawnView& view, const QRect& clip)
{
    if (view.system)
        return { view.system->transform(),
                 view.area ? view.area->ratioTransform() : QCPRatioTransform(), QRectF(clip) };
    return qcpMakeFrame(view.keyAxis, view.valueAxis, view.axisRect, QRectF(clip));
}

//! The frame for \a view and \a clip, rebuilt only when the plot, the frame epoch or one of the
//! axes, the rect and the clip changed: a layer paints all its children through one frame, and a
//! click pass asks every candidate for the same one.
const QCPDrawFrame& memoFrame(const QCPDrawnView& view, const QRect& clip)
{
    FrameMemo& memo = frameMemo();
    const quint64 epoch = view.plot ? view.plot->frameEpoch() : 0;
    // The ranges are part of the key although a frame's axes do not move while it paints: a click
    // may follow a setRange() without a replot, and the epoch alone would then measure it against
    // the frame of the range before.
    const QCPRange keyRange = view.keyAxis ? view.keyAxis->range() : QCPRange();
    const QCPRange valueRange = view.valueAxis ? view.valueAxis->range() : QCPRange();
    // A system names its own frame: its revision moves it when neither range nor rect does (a polar
    // start angle, a reversed radial axis), so it replaces the axis fields of the key.
    const quint64 revision = view.system ? view.system->revision() : 0;
    if (memo.plot != view.plot || memo.epoch != epoch || memo.system != view.system
        || memo.revision != revision || memo.keyAxis != view.keyAxis
        || memo.valueAxis != view.valueAxis || memo.axisRect != view.axisRect
        || memo.area != view.area || memo.keyRange != keyRange || memo.valueRange != valueRange
        || memo.clip != clip)
    {
        memo.frame = makeFrame(view, clip);
        memo.plot = view.plot;
        memo.epoch = epoch;
        memo.system = view.system;
        memo.revision = revision;
        memo.keyAxis = view.keyAxis;
        memo.valueAxis = view.valueAxis;
        memo.axisRect = view.axisRect;
        memo.area = view.area;
        memo.keyRange = keyRange;
        memo.valueRange = valueRange;
        memo.clip = clip;
    }
    return memo.frame;
}

//! The transform \a view draws through: its system's, or the axes' pair for the items.
QCPTransform qcpViewTransform(const QCPDrawnView& view)
{
    return view.system ? view.system->transform() : QCPTransform(view.keyAxis, view.valueAxis);
}

} // namespace

QCPDrawnView qcpDrawnViewOf(const QCPItem& item, QRect clip)
{
    QCPDrawnView view;
    view.plot = item.parentPlotRaw();
    view.layer = item.layerRaw();
    view.theme = item.themeRaw();
    view.selectable = item.selectable() != QCP::stNone;
    view.clip = clip;
    // simplify: an item mixing axis pairs across its positions is drawn through the first pair;
    // per-position frames when an item needs them.
    const QCPItemPosition* first = item.firstPosition();
    if (first)
    {
        view.keyAxis = first->keyAxisRaw();
        view.valueAxis = first->valueAxisRaw();
        view.system = first->systemRaw();
        // The frame is the one of the first position that maps plot coordinates through a system;
        // a position that only measures ratios does not name the frame.
        const bool firstMapsPlots = view.system
            && (first->typeX() == QCPItemPosition::ptPlotCoords
                || first->typeY() == QCPItemPosition::ptPlotCoords);
        if (!firstMapsPlots)
            for (const QCPItemPosition* position : item.positions())
                if (position->systemRaw()
                    && (position->typeX() == QCPItemPosition::ptPlotCoords
                        || position->typeY() == QCPItemPosition::ptPlotCoords))
                {
                    view.system = position->systemRaw();
                    break;
                }
    }
    // The frame's ratio is the position's area; the clip shape is the item's clip area, and only
    // while the item clips. An unclipped item draws anywhere, exactly as it does on an axis rect.
    view.area = first ? first->areaRaw() : nullptr;
    view.clipArea = item.clipToAxisRect() ? item.clipAreaRaw() : nullptr;
    view.axisRect = item.clipAxisRectRaw();
    if (!view.axisRect && view.keyAxis)
        view.axisRect = view.keyAxis->axisRectRaw();
    // An item with no position at all still measures in the rect it lives in: the callout hangs
    // its anchors from plot coordinates and has none to read a pair from. A position that has had
    // its axes taken away names none on purpose, and keeps being reported as such.
    // Only a rect answers with an axis pair. An item clipped to a system area (a polar disc, say)
    // measures in that area's system instead, as a position on it would: measuring its Data in the
    // plot's first Cartesian rect would put it somewhere else without a word.
    if (!view.keyAxis && !first)
    {
        const auto* clipRect = qobject_cast<const QCPAxisRect*>(view.clipArea);
        if (clipRect || !view.clipArea)
        {
            const QCPAxisRect* frameRect = clipRect ? clipRect : view.axisRect;
            if (!frameRect && item.parentPlotRaw())
                frameRect = item.parentPlotRaw()->axisRect();
            if (frameRect)
            {
                view.axisRect = frameRect;
                view.keyAxis = frameRect->axis(QCPAxis::atBottom).get();
                view.valueAxis = frameRect->axis(QCPAxis::atLeft).get();
            }
        }
        else if (const QList<QCPHandle<QCPCoordinateSystem>> systems = view.clipArea->systems();
                 !systems.isEmpty())
        {
            view.system = systems.first().get();
            view.area = view.clipArea;
            view.axisRect = nullptr;
        }
    }
    // An area that has not been laid out yet, or that was taken out of the layout, has no frame to
    // place anything in: the item is inert, and the missing-axis refusal says so once.
    if (view.area && view.area->rect().isEmpty())
    {
        view.system = nullptr;
        view.area = nullptr;
        view.clipArea = nullptr;
        view.axisRect = nullptr;
        view.keyAxis = nullptr;
        view.valueAxis = nullptr;
    }
    // A path-clipping area (a disc) bounds a clipped item's recording as its rect does: the replay
    // sets that clip and the painter adds the shape, as a plottable's frame always has.
    view.clippedToFrame = view.clipArea && view.clipArea->clipsToPath();
    return view;
}

QCPDrawnView qcpDrawnViewOf(const QCPPlottable& plottable, QRect clip)
{
    QCPDrawnView view;
    view.plot = plottable.parentPlotRaw();
    view.layer = plottable.layerRaw();
    view.theme = plottable.themeRaw();
    view.selectable = plottable.selectable() != QCP::stNone;
    view.clip = clip;
    view.system = plottable.systemRaw();
    view.keyAxis = plottable.keyAxisRaw();
    view.valueAxis = plottable.valueAxisRaw();
    view.axisRect = view.keyAxis ? view.keyAxis->axisRectRaw()
                                 : (view.system
                                        ? qobject_cast<QCPAxisRect*>(view.system->areaRaw())
                                        : nullptr);
    view.area = view.system ? view.system->areaRaw() : view.axisRect;
    view.clipArea = view.area;
    view.clippedToFrame = view.system != nullptr;
    return view;
}

QCPDrawnCore::QCPDrawnCore() = default;

QCPDrawnCore::~QCPDrawnCore() = default;

void QCPDrawnCore::ensureState()
{
    if (!mState)
        mState = std::make_shared<State>();
}

bool QCPDrawnCore::frameTransformMatches(const QCPDrawnView& view, const QCPDrawFrame* known,
                                        const QCPTransform& recorded)
{
    if (known)
        return known->transform == recorded;
    return qcpViewTransform(view) == recorded;
}

bool QCPDrawnCore::recordingIsStale(const QCPDrawable& object, const QCPDrawnView& view,
                                   const QCPDrawFrame* known) const
{
    if (!mState)
        return true; // it has never recorded, so nothing it holds can be replayed
    const quint64 epoch = view.plot ? view.plot->viewEpoch() : 0;
    const quint64 itemsEpoch = view.plot ? view.plot->itemsEpoch() : 0;
    // An everyFrame() recording reads something the object cannot declare, so it is rebuilt by each
    // frame - once: the frame's paint and the queries it makes see the recording it built.
    const bool everyFrame = mState->recording.everyFrame
        && (!view.plot || mState->recordedFrame != (view.plot ? view.plot->frameEpoch() : 0));
    const bool windowStale = mState->recording.keyWindow && [&]
    {
        // A key window only promises its key range and scale: a pan inside it is a translation, a
        // value zoom re-resolves the commands, and anything that changes the key scale remakes it.
        const QCPKeyWindow& window = *mState->recording.keyWindow;
        if (!view.keyAxis || !(view.keyAxis->scale() == window.scale))
            return true;
        const double pixelsPerScale = qcpKeyPixelsPerScaleUnit(view.keyAxis);
        if (!(pixelsPerScale > 0.0) || !(window.pixelsPerScale > 0.0))
            return true;
        if (qAbs(pixelsPerScale / window.pixelsPerScale - 1.0) > 1e-9)
            return true;
        const QCPRange keys = view.keyAxis->range();
        return keys.lower < window.keys.lower || keys.upper > window.keys.upper;
    }();
    return !mState->recorded || everyFrame || mState->recordedGeneration != mGeneration
        || mState->recordedSource != object.dataSourceIdentity()
        || mState->recordedDependency != object.dependencyRevision()
        || (object.resolvesForTheExport() && mState->recordedExporting != view.exporting)
        || (mState->viewDependent && mState->recordedEpoch != epoch)
        || (mState->anchorDependent && mState->recordedItemsEpoch != itemsEpoch)
        || (mState->transformDependent
            && !frameTransformMatches(view, known, mState->recordedTransform))
        || windowStale;
}

QPointF QCPDrawnCore::stallOffset(const QCPDrawable& object, const QCPDrawnView& view) const
{
    if (!mState || !mState->painted || mState->recording.everyFrame || mState->transformDependent
        || !mState->movesWithTheData || !view.keyAxis || !view.valueAxis)
        return { };
    if (recordingIsStale(object, view))
        return { }; // it has to be drawn again, whatever the pan says
    const double keyRatio = qcp::axisRangeSizeRatio(view.keyAxis, mState->paintedKey);
    const double valueRatio = qcp::axisRangeSizeRatio(view.valueAxis, mState->paintedValue);
    if (qAbs(keyRatio - 1.0) > 1e-4 || qAbs(valueRatio - 1.0) > 1e-4)
        return { }; // a zoom: the shift cannot express a new scale
    return qcp::computeViewportOffset(view.keyAxis, view.valueAxis, mState->paintedKey,
                                      mState->paintedValue);
}

// The layer now holds this object drawn at these ranges: a pan may shift them instead of
// repainting. An export never does (it paints into another image, not the layer's).
void QCPDrawnCore::notePainted(const QCPDrawnView& view)
{
    if (!mState || !view.keyAxis || !view.valueAxis)
        return;
    mState->paintedKey = view.keyAxis->range();
    mState->paintedValue = view.valueAxis->range();
    mState->painted = true;
}

bool QCPDrawnCore::newGeneration()
{
    if (mInsideDraw)
    {
        qcp::reportMisuse(Q_FUNC_INFO,
                          "update() from inside a drawing: ignored, it would "
                          "re-record on every frame");
        return false;
    }
    ++mGeneration;
    return true;
}

quint64 QCPDrawnCore::generation() const
{
    return mGeneration;
}

int QCPDrawnCore::recordingCount() const
{
    return mState ? mState->recordings : 0;
}

bool QCPDrawnCore::hasRecording() const
{
    return mState && mState->recorded;
}

int QCPDrawnCore::recordedCommandCount() const
{
    return mState && mState->recorded ? int(mState->recording.commands.size()) : 0;
}

const QCPDrawList& QCPDrawnCore::recordedList() const
{
    static const QCPDrawList empty;
    return mState && mState->recorded ? mState->recording : empty;
}

const QCPDrawList& QCPDrawnCore::iconRecordedList() const
{
    static const QCPDrawList empty;
    return mState && mState->iconRecorded ? mState->icon : empty;
}

int QCPDrawnCore::recordedPolylinePointCount() const
{
    int points = 0;
    if (mState && mState->recorded)
        for (const qcp::QCPDrawCommand& command : mState->recording.commands)
            if (const auto* polyline = std::get_if<qcp::PolylineDataCmd>(&command))
                points += int(polyline->points->size());
    return points;
}

int QCPDrawnCore::recordedPolylineNaNs() const
{
    int breaks = 0;
    if (mState && mState->recorded)
        for (const qcp::QCPDrawCommand& command : mState->recording.commands)
            if (const auto* polyline = std::get_if<qcp::PolylineDataCmd>(&command))
                for (const QPointF& point : *polyline->points)
                    if (!std::isfinite(point.x()) || !std::isfinite(point.y()))
                        ++breaks;
    return breaks;
}

quint64 QCPDrawnCore::resolveRemapCount() const
{
    return mState ? mState->resolve.remapCount() : 0;
}

int QCPDrawnCore::rasterCount() const
{
    return mState ? mState->raster.rasterCount() : 0;
}

bool QCPDrawnCore::ensureRecording(const QCPDrawable& object, const QCPDrawnView& view,
                                   const QCPDrawFrame* known)
{
    ensureState();
    if (mInsideDraw)
    {
        // The object is asking for the recording it is drawing right now: it has no answer yet, and
        // re-entering draw() for it would never end. The previous recording is the closest thing.
        qcp::reportMisuse(Q_FUNC_INFO,
                          "a drawn object read its own range, hit test or extent from inside its "
                          "draw(): the last recording is used");
        return mState->recorded;
    }
    // A plot may hold no axis at all: an item placed in pixels still draws there, as it did in 2.x,
    // and only a drawing that needs a transform is refused - after it was recorded, so the answer is
    // about what it actually holds.
    // A system names a frame of its own; without one, the objects that only read axes (the items)
    // still draw in pixels when a plot has no axis, exactly as they did in 2.x.
    const bool framed = view.framed();
    if (!framed && mState->refusedMissingAxis && mState->refusedAxesGeneration == mGeneration)
        return false;
    const auto refuseWithoutAxes = [this]() {
        mState->refusedMissingAxis = true;
        mState->refusedAxesGeneration = mGeneration;
        if (!mState->reportedMissingAxis)
        {
            mState->reportedMissingAxis = true;
            qDebug() << "NeoQCP: a drawn object has no key or value axis; it is not drawn";
        }
        mState->recorded = false;
    };
    // The recording kept from the axes' time is placed through them: it cannot be reused now.
    if (!framed && mState->recorded && qcpNeedsAxes(mState->recording))
    {
        refuseWithoutAxes();
        return false;
    }
    const quint64 epoch = view.plot ? view.plot->viewEpoch() : 0;
    const quint64 itemsEpoch = view.plot ? view.plot->itemsEpoch() : 0;
    const quint64 frameEpoch = view.plot ? view.plot->frameEpoch() : 0;
    const void* source = object.dataSourceIdentity();
    const quint64 dependency = object.dependencyRevision();
    // simplify: a translation still remakes a transform-dependent recording, so a pan of stacked,
    // grouped or pixel-width bars re-records every frame. A data-plus-pixel-offset position type
    // would keep them; it is on the deferred list.

    if (!recordingIsStale(object, view, known))
        return true;
    // Without axes the frame's clip is not the axis rect's: the object is painted into the viewport
    // its layer clips it with, so that is what its recording is measured against.
    const QRectF clip = framed && !view.clippedToFrame ? QRectF()
                                                       : QRectF(view.clip).translated(0, -1);
    QCPDrawContext context(view.keyAxis, view.valueAxis, view.area, view.theme, view.exporting,
                           clip, view.system);
    const InsideDraw drawing(mInsideDraw);
    object.draw(context);
    QCPDrawList recording = context.takeDrawList();
    if (!framed && qcpNeedsAxes(recording))
    {
        refuseWithoutAxes();
        return false;
    }
    mState->reportedMissingAxis = false;
    mState->refusedMissingAxis = false;
    mState->recording = std::move(recording);
    mState->viewDependent = usesTheView(mState->recording);
    mState->anchorDependent = mState->recording.anchorDependent;
    mState->transformDependent = mState->recording.transformDependent;
    // Asked once per recording: a pan then knows whether the whole of it may be shifted.
    mState->movesWithTheData = qcpRecordingMovesWithTheData(mState->recording);
    mState->recordedGeneration = mGeneration;
    mState->recordedEpoch = epoch;
    mState->recordedItemsEpoch = itemsEpoch;
    mState->recordedFrame = frameEpoch;
    mState->recordedSource = source;
    mState->recordedDependency = dependency;
    mState->recordedExporting = view.exporting;
    // A caller that built the frame already has the transform; one that did not (a hit test, an
    // extent) builds it, and only a recording that reads it keeps it.
    if (mState->transformDependent)
        mState->recordedTransform = known ? known->transform : qcpViewTransform(view);
    mState->recorded = true;
    ++mState->recordings;
    return true;
}

std::optional<QCPDataBox> QCPDrawnCore::dataRange(const QCPDrawable& object,
                                                  const QCPDrawnView& view, bool applyLogRule)
{
    ensureState();
    if (!ensureRecording(object, view))
        return std::nullopt;
    const QCPDrawList& list = mState->recording;
    // The recording already knows which sides every command was data on; a log axis cannot index a
    // box that reaches zero or below, exactly as QCPItem::positionsExtent decides it.
    const QCPDrawList::Box& side = list.data;
    bool keyBounded = side.keyBounded;
    bool valueBounded = side.valueBounded;
    if (applyLogRule)
    {
        const bool keyLog = view.keyAxis && view.keyAxis->scale().isLog();
        const bool valueLog = view.valueAxis && view.valueAxis->scale().isLog();
        if (keyBounded && keyLog && !(side.bounds.keyLower > 0))
            keyBounded = false;
        if (valueBounded && valueLog && !(side.bounds.valueLower > 0))
            valueBounded = false;
    }
    if (!keyBounded && !valueBounded)
        return std::nullopt;
    QCPDataBox box = side.bounds;
    if (!keyBounded)
    {
        box.keyLower = -kInfinity;
        box.keyUpper = kInfinity;
    }
    if (!valueBounded)
    {
        box.valueLower = -kInfinity;
        box.valueUpper = kInfinity;
    }
    return box;
}

std::optional<QCPItemExtent> QCPDrawnCore::dataExtent(const QCPDrawable& object,
                                                      const QCPDrawnView& view)
{
    // The item index asks the index box, not the data one: a text's glyphs hang off its anchor, so
    // its side stays unbounded and no click on them is ever culled. No log rule: a box that cannot
    // be indexed simply is not.
    if (!ensureRecording(object, view))
        return std::nullopt;
    const QCPDrawList::Box& side = mState->recording.index;
    if (!side.keyBounded && !side.valueBounded)
        return std::nullopt;
    return QCPItemExtent { const_cast<QCPAxis*>(view.keyAxis), const_cast<QCPAxis*>(view.valueAxis),
                           side.bounds, pixelPad() };
}

double QCPDrawnCore::pixelPad() const
{
    if (!mState)
        return 0.0;
    double pad = 0.0;
    for (const qcp::QCPDrawCommand& command : mState->recording.commands)
        pad = std::max(pad,
                       std::visit(
                           [](const auto& cmd) -> double
                           {
                               using Command = std::decay_t<decltype(cmd)>;
                               if constexpr (std::is_same_v<Command, qcp::MarkersCmd>)
                                   return cmd.style->size() / 2.0;
                               else if constexpr (std::is_same_v<Command, qcp::ErrorBarsCmd>)
                                   return cmd.whiskerWidth / 2.0 + halfPen(cmd.pen);
                               else if constexpr (std::is_same_v<Command, qcp::CircleCmd>)
                                   return cmd.radius + halfPen(cmd.pen);
                               else if constexpr (std::is_same_v<Command, qcp::RoundedRectCmd>)
                                   return halfPen(cmd.shape->pen);
                               else if constexpr (std::is_same_v<Command, qcp::BoxedTextCmd>)
                                   return halfPen(cmd.box->style.pen);
                               else if constexpr (std::is_same_v<Command, qcp::TextCmd>
                                                  || std::is_same_v<Command, qcp::ImageCmd>
                                                  || std::is_same_v<Command, qcp::FillBetweenCmd>
                                                  || std::is_same_v<Command, qcp::PainterCmd>)
                                   return 0.0;
                               else
                                   return halfPen(cmd.pen);
                           },
                           command));
    return pad + 1.0; // antialiasing
}

/*! \internal

  The data position of \a at in the frame the object's recording replays in. A mouse event asks it
  through the item's own view, so a log or reversed axis answers what it draws, not what a default
  pair would. The frame is built here rather than memoised: a mouse event cannot be raised while a
  paint or a hit test is replaying, but a frame of one's own costs nothing next to the trap.
*/
QCP::Data QCPDrawnCore::dataAt(QCP::Pixels at, const QCPDrawnView& view) const
{
    const QCPDrawFrame frame = makeFrame(view, view.clip.translated(0, -1));
    return frame.transform.inverse(QPointF(at.x, at.y));
}

/*! \internal

  The pixels of \a position in the frame the object's recording replays in: what an anchor resolves
  through. The frame is built here, never taken from the memo: an anchor resolves while the outer
  paint or hit test still replays through the frame it took, and an anchor placed on another axis
  pair would rewrite that one shared slot under it.
*/
QCP::Pixels QCPDrawnCore::pixelsOf(const QCP::Position& position,
                                   const QCPDrawnView& view) const
{
    const QCPDrawFrame frame = makeFrame(view, view.clip.translated(0, -1));
    const QPointF pixels = qcpResolvePosition(frame, position, false);
    return { pixels.x(), pixels.y() };
}

std::optional<double> QCPDrawnCore::hitTest(const QCPDrawable& object, QCP::Pixels at,
                                            QVariant* details, const QCPDrawnView& view)
{
    const QCPDrawFrame& frame = memoFrame(view, view.clip.translated(0, -1));
    if (!ensureRecording(object, view, &frame))
        return std::nullopt;
    const double tolerance = view.plot ? double(view.plot->selectionTolerance()) : 5.0;
    const QPointF point(at.x, at.y);
    // Culling uses the index box: the same box that decides whether the item index keeps it.
    const QCPDrawList::Box& side = mState->recording.index;
    std::optional<QCPDataBox> box;
    if (side.keyBounded || side.valueBounded)
        box = side.bounds;
    const double pad = pixelPad();
    // Only a cartesian frame has independent, monotone axes: a polar frame maps its angle around a
    // circle, so the two corners of a data box say nothing about the pixels the recording covers.
    if (box && frame.transform.kind() == QCPTransform::Kind::Cartesian)
    {
        // Bounds first: the recording's box, mapped and grown by the widest stroke. Only the sides
        // the box really has are checked: an unbounded one is ±inf, which maps to a pixel far
        // outside the rect or to NaN on a log axis, and either would reject every point.
        const double grow = pad + tolerance;
        // Each side moves along its own pixel axis: with a vertical key axis, map() returns the
        // value pixel first (QCPTransform::map). The mapping is monotone on each axis, so the two
        // opposite corners carry both sides' pixels and two maps do for four.
        const bool keyVertical = frame.transform.keyIsVertical();
        const QCP::Pixels lower = frame.transform.map(QCP::Data { box->keyLower, box->valueLower });
        const QCP::Pixels upper = frame.transform.map(QCP::Data { box->keyUpper, box->valueUpper });
        const auto keyPixels = [&](bool high)
        { return keyVertical ? (high ? upper.y : lower.y) : (high ? upper.x : lower.x); };
        const auto valuePixels = [&](bool high)
        { return keyVertical ? (high ? upper.x : lower.x) : (high ? upper.y : lower.y); };
        // The click coordinate follows the same swap: with a vertical key axis the key runs down
        // the screen and the value across it.
        const double keyClick = keyVertical ? point.y() : point.x();
        const double valueClick = keyVertical ? point.x() : point.y();
        if (!nearSide(keyPixels(false), keyPixels(true), keyClick, grow)
            || !nearSide(valuePixels(false), valuePixels(true), valueClick, grow))
            return std::nullopt;
    }
    double best = kInfinity;
    int nearestIndex = -1;
    // The topmost tagged command within the tolerance names what was hit (decision P-2): a peak's
    // edge recorded over its body is grabbed anywhere within the tolerance of it, also from inside
    // the body, while the distance stays the nearest command's. The topmost rule decides among the
    // tagged commands only: an untagged line over an untagged batch names nothing, so the hit keeps
    // the batch index of the nearest command, as it always did.
    int named = -1;
    for (int i = 0; i < int(mState->recording.commands.size()); ++i)
    {
        int index = -1;
        const double distance = distanceTo(mState->recording.commands[size_t(i)], frame, point,
                                           &index);
        if (distance < best)
        {
            best = distance;
            nearestIndex = index;
        }
        if (distance <= tolerance && mState->recording.tagAt(i))
            named = i;
    }
    if (!(best <= tolerance))
        return std::nullopt;
    // A tagged command names the datum and the part it draws; anything else keeps what it always
    // reported, the nearest command's batch rect index, so a bar click still says which bar.
    if (details)
    {
        const QCPDrawList::Tag* tag = named >= 0 ? mState->recording.tagAt(named) : nullptr;
        if (tag)
        {
            QVariantMap map;
            if (tag->dataIndex >= 0)
                map.insert(QStringLiteral("dataIndex"), tag->dataIndex);
            if (tag->part >= 0)
                map.insert(QStringLiteral("part"), tag->part);
            *details = map;
        }
        else if (nearestIndex >= 0)
            *details = QVariant(nearestIndex);
    }
    // A distance is never negative: distanceTo() subtracts the stroke's half width, so a click
    // inside the stroke came out below zero, and the callers (the layerable selectTest contract)
    // reject a negative distance as a miss.
    return std::max(0.0, best);
}

double QCPDrawnCore::selectTest(const QCPDrawable& object, const QPointF& pos, bool onlySelectable,
                                QVariant* details, const QCPDrawnView& view)
{
    if (onlySelectable && !view.selectable)
        return -1.0;
    // The object's own hit test: by default the recording's geometry, and its own rects where the
    // recording is not what the user sees (a bar is hit inside its rect, not within a tolerance).
    return object.hitTest(QCP::Pixels { pos.x(), pos.y() }, details).value_or(-1.0);
}

namespace
{
//! The shape an object's area clips its content to: empty where the area clips to its rect alone.
QPainterPath qcpSystemClipShape(const QCPDrawnView& view)
{
    if (!view.clipArea || !view.clipArea->clipsToPath())
        return { };
    return view.clipArea->clipPath();
}

//! Clips \a painter to the system's shape for as long as it lives, where the system has one.
struct QCPSystemClip
{
    QCPSystemClip(QCPPainter& painter, const QCPDrawnView& view) : mPainter(painter)
    {
        const QPainterPath shape = qcpSystemClipShape(view);
        if (shape.isEmpty())
            return;
        mPainter.save();
        mPainter.setClipPath(shape, Qt::IntersectClip);
        mClipped = true;
    }
    QCPSystemClip(const QCPSystemClip&) = delete;
    QCPSystemClip& operator=(const QCPSystemClip&) = delete;
    ~QCPSystemClip()
    {
        if (mClipped)
            mPainter.restore();
    }

private:
    QCPPainter& mPainter;
    bool mClipped = false;
};

//! Whether a command submits to this layer's GPU pipelines, and so closes the painter slice that is
//! open below it. A marker has a layer of its own, above everything on this one: it neither opens
//! nor closes a slice.
bool submitsToTheGpu(const qcp::QCPDrawCommand& command)
{
    return !std::holds_alternative<qcp::MarkersCmd>(command) && qcp::isGpuCommand(command);
}

//! Whether a command paints into the current painter: the painter-only content that has to land
//! over the GPU content drawn before it. The kind is asked the cheap way: painter-only and GPU are
//! complements, and commandKind() walks the positions of a polygon.
bool paintsIntoTheSlice(const qcp::QCPDrawCommand& command)
{
    return !qcp::isGpuCommand(command);
}

//! How many painter slices \a list opens when it replays as runs: one per painter run that lands
//! over GPU content, the slice already open counted once. The same walk as the replay below, so the
//! two never disagree on where a run starts.
int slicesNeeded(const QCPDrawList& list, bool gpuBefore, bool sliceOpen)
{
    int needed = 0;
    bool gpuSeen = gpuBefore;
    bool painterRun = false;
    for (const qcp::QCPDrawCommand& command : list.commands)
    {
        if (paintsIntoTheSlice(command))
        {
            if (gpuSeen && !painterRun)
                ++needed;
            painterRun = true;
            continue;
        }
        painterRun = false;
        if (submitsToTheGpu(command))
            gpuSeen = true;
    }
    return sliceOpen && needed > 0 ? needed - 1 : needed;
}

//! An export, or a screen without a backend, paints a recording without its markers and then its
//! markers: on screen the scatter layer draws after the painter content of the layer, so a marker
//! stays over a text recorded after it, and both paths have to agree on that.
void replayWithMarkersLast(const QCPDrawList& list, QCPPainter& painter, const QCPDrawFrame& frame)
{
    qcpReplayWithPainter(list, painter, frame, QCPReplayKinds::WithoutMarkers, nullptr, true, false,
                         QCPReplayState::AlreadySaved);
    qcpReplayWithPainter(list, painter, frame, QCPReplayKinds::MarkersOnly, nullptr, true, false,
                         QCPReplayState::AlreadySaved);
}

} // namespace

//! Replays \a list as runs of painter-only and of GPU commands, each in the order it was recorded:
//! a GPU run submits to the layer and closes the slice open below it, a painter-only run after GPU
//! content paints into a slice above it. Consecutive painter-only runs share one slice.
void QCPDrawnCore::paintInCallOrder(const QCPDrawList& list, QCPPlottableRhiLayer& plottables,
                                    QCPScatterRhiLayer& scatter, const QCPDrawnView& view,
                                    QCPPainter& painter, const QCPDrawFrame& frame,
                                    QCPResolveCache& resolve, qcp::QCPRasterFallback& fallback,
                                    quint64 generation, bool gpuBefore)
{
    const int count = int(list.commands.size());
    const bool dashed = plottables.dashedAvailable();
    bool gpuSeen = gpuBefore;
    // Once for the whole recording: a sweep per run would throw away the raster images the runs
    // before it built, and rasterise them again on every frame.
    resolve.beginReplay();
    fallback.beginReplay();
    // More slices than the layer has left: splitting the recording would put one object on both
    // sides of a stacking level and lose the order inside it. Its GPU content goes up once, its
    // painter content once over it, so the object stays whole and on top; with no slice left at all
    // the pixels land in the buffer, as a recording the raster budget refuses does.
    if (slicesNeeded(list, gpuBefore, view.layer->hasOpenPainterSlice())
        > view.layer->painterSlicesLeft())
    {
        if (list.hasGpuContent)
            view.layer->closePainterSlice(painter);
        qcpReplayWithRhi(list, plottables, scatter, frame, resolve, fallback,
                         view.plot->bufferDevicePixelRatio(), view.plot->rhiOutputSize(), generation,
                         nullptr, static_cast<float>(painter.opacity()));
        if (list.hasGpuContent)
            view.layer->noteGpuContent();
        QCPPainter* slice = view.layer->painterSlice(painter);
        qcpReplayWithPainter(list, slice ? *slice : painter, frame, QCPReplayKinds::PainterOnly,
                             &fallback, dashed, false, QCPReplayState::AlreadySaved);
        if (slice)
            view.layer->restoreSlicePainter();
        return;
    }
    for (int first = 0; first < count;)
    {
        const bool painterRun = paintsIntoTheSlice(list.commands[std::size_t(first)]);
        int last = first + 1;
        while (last < count && paintsIntoTheSlice(list.commands[std::size_t(last)]) == painterRun)
            ++last;
        if (painterRun)
        {
            // The slice takes this run's clip, fade and antialiasing for the length of the run;
            // without one the pixels fall back into the buffer, under the GPU content.
            QCPPainter* slice = gpuSeen ? view.layer->painterSlice(painter) : nullptr;
            qcpReplayWithPainter(list, slice ? *slice : painter, frame,
                                 QCPReplayKinds::PainterOnly, &fallback, dashed, false,
                                 QCPReplayState::AlreadySaved, first, last);
            if (slice)
                view.layer->restoreSlicePainter();
        }
        else
        {
            const auto begin = list.commands.begin() + first;
            const bool gpu = std::any_of(begin, list.commands.begin() + last, submitsToTheGpu);
            if (gpu)
                view.layer->closePainterSlice(painter);
            qcpReplayWithRhi(list, plottables, scatter, frame, resolve, fallback,
                             view.plot->bufferDevicePixelRatio(), view.plot->rhiOutputSize(),
                             generation, nullptr, static_cast<float>(painter.opacity()), first,
                             last);
            // What the layer refused of this run goes under it, in the buffer.
            qcpReplayWithPainter(list, painter, frame, QCPReplayKinds::PainterOnly, &fallback,
                                 dashed, false, QCPReplayState::AlreadySaved, first, last);
            if (gpu)
            {
                gpuSeen = true;
                view.layer->noteGpuContent();
            }
        }
        first = last;
    }
}

void QCPDrawnCore::paint(const QCPDrawable& object, QCPPainter& painter, const QCPDrawnView& view)
{
    // The painter follows a system's shape (a polar disc); the GPU keeps its bounding rect.
    const QCPSystemClip systemClip(painter, view);
    // An export resolves what a screen frame waits for: the recording knows which it was made for.
    QCPDrawnView painted = view;
    painted.exporting = painter.isExporting();
    // The layer clips every child it draws at clipRect().translated(0, -1), the half pixel
    // QCPPainter's antialiasing shifts it by: the replay sets the same clip, so a fill that ends on
    // the clip's edge lands on the same rows it does on screen.
    const QCPDrawFrame& frame = memoFrame(view, view.clip.translated(0, -1));
    if (!ensureRecording(object, painted, &frame))
        return;
    QCPPlottableRhiLayer* plottables
        = painter.isExporting() ? nullptr : qcp::gpuLayer(view.plot, view.layer, false);
    QCPScatterRhiLayer* scatter = plottables ? view.plot->scatterRhiLayer(view.layer) : nullptr;
    if (!plottables || !scatter)
    {
        if (!painter.isExporting())
        {
            // A screen without a backend: the painter draws it all, and never touches the caches a
            // screen frame resolves into.
            // The layer brackets this draw with save()/restore(): the replay need not repeat it.
            replayWithMarkersLast(mState->recording, painter, frame);
            notePainted(view);
            return;
        }
        // Spec 5: an export stacks like the screen, so the recording replays in call order here
        // too. Its markers are the exception: the screen draws them in a scatter pass above the
        // whole layer, so the layer paints them after its last child. The layer brackets this draw
        // with save()/restore() and carries this object's clip, antialiasing hint and busy fade, so
        // the inline replay inherits them.
        qcpReplayWithPainter(mState->recording, painter, frame, QCPReplayKinds::WithoutMarkers,
                             nullptr, true, false, QCPReplayState::AlreadySaved);
        if (view.layer)
            view.layer->deferMarkers(mState->recording, frame, painter);
        else
            qcpReplayWithPainter(mState->recording, painter, frame, QCPReplayKinds::MarkersOnly,
                                 nullptr, true, false, QCPReplayState::AlreadySaved);
        return;
    }
    // The painter pass completes what the layer refused, so it makes the same dashed decision.
    if (!plottables->dashedAvailable() && mState->movesWithTheData)
        mState->movesWithTheData = qcpRecordingMovesWithTheData(mState->recording, false);
    // The layer keeps this flag for its whole pass, and the recording knows whether one of its own
    // painter-only commands lands over GPU content: both are O(1).
    const bool gpuBefore = view.layer->gpuContentSeen();
    const bool callOrder = mState->recording.painterOverGpu
        || (gpuBefore && mState->recording.hasPainterOnly);
    // A recording the raster budget cannot hold paints whole, under the layer's GPU content: a run
    // split would put one object on both sides of the buffer.
    if (!callOrder || qcpRasterOverflowsTheLayer(mState->recording, *plottables))
    {
        const bool gpu = mState->recording.hasGpuContent;
        // Anything GPU this recording submits comes after what its layer already shows: it closes
        // the slice an earlier layerable left open.
        if (gpu && view.layer->hasOpenPainterSlice())
            view.layer->closePainterSlice(painter);
        qcpReplayWithRhi(mState->recording, *plottables, *scatter, frame, mState->resolve,
                         mState->raster, view.plot->bufferDevicePixelRatio(),
                         view.plot->rhiOutputSize(), mState->recordedGeneration, &mState->geometry,
                         static_cast<float>(painter.opacity()));
        qcpReplayWithPainter(mState->recording, painter, frame, QCPReplayKinds::PainterOnly,
                             &mState->raster, plottables->dashedAvailable(), false,
                             QCPReplayState::AlreadySaved);
        if (gpu)
            view.layer->noteGpuContent();
        notePainted(view);
        return;
    }
    paintInCallOrder(mState->recording, *plottables, *scatter, view, painter, frame,
                     mState->resolve, mState->raster, mState->recordedGeneration, gpuBefore);
    notePainted(view);
}

void QCPDrawnCore::paintIcon(const QCPDrawable& object, QCPPainter& painter, const QRectF& rect,
                             const QCPDrawnView& view)
{
    ensureState();
    const quint64 epoch = view.plot ? view.plot->viewEpoch() : 0;
    const void* source = object.dataSourceIdentity();
    const quint64 dependency = object.dependencyRevision();
    // The icon is a recording of its own, so it outlives a frame only when none of the facts it
    // read changed: its generation, the data it reads, and the view epoch a theme reader needs.
    const bool stale = !mState->iconRecorded || mState->iconGeneration != mGeneration
        || mState->iconSource != source || mState->iconDependency != dependency
        || (mState->icon.viewDependent && mState->iconEpoch != epoch);
    if (stale)
    {
        // An icon records through the same door, into a list of its own: the axes only feed the
        // context's unused frame, so an object outside a plot still gets its icon.
        QCPDrawContext context(view.keyAxis, view.valueAxis, view.area, view.theme);
        const InsideDraw drawing(mInsideDraw);
        object.drawLegendIcon(context);
        mState->icon = context.drawList();
        mState->iconGeneration = mGeneration;
        mState->iconEpoch = epoch;
        mState->iconSource = source;
        mState->iconDependency = dependency;
        mState->iconRecorded = true;
    }
    const bool fitted = mState->icon.data.keyBounded && mState->icon.data.valueBounded;
    if (!fitted && (mState->icon.data.keyBounded || mState->icon.data.valueBounded))
        return; // data on one side only: no box to fit, and no frame to draw the other side in
    // An icon that names no data position records in the icon's own frame: its ratio positions are
    // fractions of the rect and its pixels are plot pixels inside it (P-6). The unit box gives it,
    // and the icons that paint through a callback, the transform they are laid out with.
    const QCPDataBox fitBox = fitted ? mState->icon.data.bounds : QCPDataBox { 0.0, 1.0, 0.0, 1.0 };
    // The frame the icon replays in is the system's: a cartesian system fits the recording into the
    // rect, as every 2.x icon did, but a system may lay its legend icon out its own way.
    const QCPTransform transform = fitted && view.system
        ? view.system->iconTransform(fitBox, rect)
        : QCPTransform::fitted(fitBox, rect);
    const QCPDrawFrame frame { transform, QCPRatioTransform::fitted(rect), rect };
    qcpReplayWithPainter(mState->icon, painter, frame, QCPReplayKinds::All);
}
