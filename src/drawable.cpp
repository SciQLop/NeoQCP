#include "drawable.h"

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
#include "plottables/plottable-draw-utils.h"
#include "plottables/plottable.h"
#include "vector2d.h"

#include <QDebug>
#include <QPainter>
#include <QTransform>
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
                const std::vector<QPointF> pixels = resolve(*cmd.points, frame);
                return distanceTo(pixels, at, false) - halfPen(cmd.pen);
            }
            else if constexpr (std::is_same_v<Command, qcp::PolylinePositionsCmd>)
            {
                const std::vector<QPointF> pixels = resolve(cmd.points, frame);
                return distanceTo(pixels, at, false) - halfPen(cmd.pen);
            }
            else if constexpr (std::is_same_v<Command, qcp::PolygonCmd>)
            {
                const std::vector<QPointF> pixels = resolve(cmd.points, frame);
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
    quint64 generation = 0;
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
    bool insideDraw = false;
    bool reportedMissingAxis = false;
    //! The generation at which a recording without axes was refused, and whether that stands: the
    //! answer costs one draw to learn, and no other fact can change it while the content does not.
    quint64 refusedAxesGeneration = 0;
    bool refusedMissingAxis = false;
};

namespace
{
/*!
  Marks the object as drawing for the guard's lifetime: a contentChanged() from inside a recording
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

bool qcpDrawnClickInsideAxisRect(const QCPAbstractPlottable& plottable, const QPointF& pos)
{
    const QCPAxis* keyAxis = plottable.keyAxis();
    QCustomPlot* plot = plottable.parentPlot();
    if (!keyAxis || !keyAxis->axisRect() || !plot)
        return true;
    return keyAxis->axisRect()->rect().contains(pos.toPoint())
        || plot->interactions().testFlag(QCP::iSelectPlottablesBeyondAxisRect);
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
    const QCPAxis* keyAxis = nullptr;
    const QCPAxis* valueAxis = nullptr;
    const QCPAxisRect* axisRect = nullptr;
    QRect clip;
    QCPDrawFrame frame;
};

FrameMemo& frameMemo()
{
    thread_local FrameMemo memo;
    return memo;
}

//! The part of a view every object has: its plot, layer and theme.
QCPDrawnView qcpLayerableView(const QCPLayerable& object)
{
    QCPDrawnView view;
    view.plot = object.parentPlot();
    view.layer = object.layer();
    view.theme = view.plot ? view.plot->theme() : nullptr;
    return view;
}
} // namespace

QCPDrawnView qcpDrawnViewOf(const QCPAbstractItem& item, QRect clip)
{
    QCPDrawnView view = qcpLayerableView(item);
    view.selectable = item.selectable() != QCP::stNone;
    view.clip = clip;
    // simplify: an item mixing axis pairs across its positions is drawn through the first pair;
    // per-position frames when an item needs them.
    if (const QCPItemPosition* first = item.firstPosition())
    {
        view.keyAxis = first->keyAxis();
        view.valueAxis = first->valueAxis();
    }
    view.axisRect = item.clipAxisRect();
    if (!view.axisRect && view.keyAxis)
        view.axisRect = view.keyAxis->axisRect();
    return view;
}

QCPDrawnView qcpDrawnViewOf(const QCPAbstractPlottable& plottable, QRect clip)
{
    QCPDrawnView view = qcpLayerableView(plottable);
    view.selectable = plottable.selectable() != QCP::stNone;
    view.clip = clip;
    view.keyAxis = plottable.keyAxis();
    view.valueAxis = plottable.valueAxis();
    view.axisRect = view.keyAxis ? view.keyAxis->axisRect() : nullptr;
    return view;
}

QCPDrawnCore::QCPDrawnCore() : mState(std::make_shared<State>()) { }

QCPDrawnCore::~QCPDrawnCore() = default;

bool QCPDrawnCore::frameTransformMatches(const QCPDrawnView& view, const QCPDrawFrame* known,
                                        const QCPTransform& recorded)
{
    if (known)
        return known->transform == recorded;
    return QCPTransform(view.keyAxis, view.valueAxis) == recorded;
}

bool QCPDrawnCore::recordingIsStale(const QCPDrawable& object, const QCPDrawnView& view,
                                   const QCPDrawFrame* known) const
{
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
        if (!view.keyAxis || view.keyAxis->scaleType() == QCPAxis::stLogarithmic)
            return true; // a window is only recorded on a linear key axis
        const double pixelsPerKey = qcpKeyPixelsPerUnit(view.keyAxis);
        if (!(pixelsPerKey > 0.0) || !(window.pixelsPerKey > 0.0))
            return true;
        if (qAbs(pixelsPerKey / window.pixelsPerKey - 1.0) > 1e-9)
            return true;
        const QCPRange keys = view.keyAxis->range();
        return keys.lower < window.keys.lower || keys.upper > window.keys.upper;
    }();
    return !mState->recorded || everyFrame || mState->recordedGeneration != mState->generation
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
    if (!mState->painted || mState->recording.everyFrame || mState->transformDependent
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
    if (!view.keyAxis || !view.valueAxis)
        return;
    mState->paintedKey = view.keyAxis->range();
    mState->paintedValue = view.valueAxis->range();
    mState->painted = true;
}

bool QCPDrawnCore::contentChanged()
{
    if (mState->insideDraw)
    {
        qcp::reportMisuse(Q_FUNC_INFO,
                          "contentChanged() from inside a drawing: ignored, it would "
                          "re-record on every frame");
        return false;
    }
    ++mState->generation;
    return true;
}

quint64 QCPDrawnCore::generation() const
{
    return mState->generation;
}

int QCPDrawnCore::recordingCount() const
{
    return mState->recordings;
}

bool QCPDrawnCore::hasRecording() const
{
    return mState->recorded;
}

bool QCPDrawnCore::recordsTheDrawing(const QCPDrawable& object, const QCPDrawnView& view)
{
    if (!ensureRecording(object, view))
        return false;
    return recordedCommandCount() > 0;
}

int QCPDrawnCore::recordedCommandCount() const
{
    return mState->recorded ? int(mState->recording.commands.size()) : 0;
}

const QCPDrawList& QCPDrawnCore::recordedList() const
{
    static const QCPDrawList empty;
    return mState->recorded ? mState->recording : empty;
}

const QCPDrawList& QCPDrawnCore::iconRecordedList() const
{
    static const QCPDrawList empty;
    return mState->iconRecorded ? mState->icon : empty;
}

int QCPDrawnCore::recordedPolylinePointCount() const
{
    int points = 0;
    if (mState->recorded)
        for (const qcp::QCPDrawCommand& command : mState->recording.commands)
            if (const auto* polyline = std::get_if<qcp::PolylineDataCmd>(&command))
                points += int(polyline->points->size());
    return points;
}

int QCPDrawnCore::recordedPolylineNaNs() const
{
    int breaks = 0;
    if (mState->recorded)
        for (const qcp::QCPDrawCommand& command : mState->recording.commands)
            if (const auto* polyline = std::get_if<qcp::PolylineDataCmd>(&command))
                for (const QPointF& point : *polyline->points)
                    if (!std::isfinite(point.x()) || !std::isfinite(point.y()))
                        ++breaks;
    return breaks;
}

quint64 QCPDrawnCore::resolveRemapCount() const
{
    return mState->resolve.remapCount();
}

int QCPDrawnCore::rasterCount() const
{
    return mState->raster.rasterCount();
}

bool QCPDrawnCore::ensureRecording(const QCPDrawable& object, const QCPDrawnView& view,
                                   const QCPDrawFrame* known)
{
    if (mState->insideDraw)
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
    const bool axes = view.keyAxis && view.valueAxis;
    if (!axes && mState->refusedMissingAxis && mState->refusedAxesGeneration == mState->generation)
        return false;
    const auto refuseWithoutAxes = [this]() {
        mState->refusedMissingAxis = true;
        mState->refusedAxesGeneration = mState->generation;
        if (!mState->reportedMissingAxis)
        {
            mState->reportedMissingAxis = true;
            qDebug() << "NeoQCP: a drawn object has no key or value axis; it is not drawn";
        }
        mState->recorded = false;
    };
    // The recording kept from the axes' time is placed through them: it cannot be reused now.
    if (!axes && mState->recorded && qcpNeedsAxes(mState->recording))
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
    const QRectF clip = axes ? QRectF() : QRectF(view.clip).translated(0, -1);
    QCPDrawContext context(view.keyAxis, view.valueAxis, view.axisRect, view.theme, view.exporting,
                           clip);
    const InsideDraw drawing(mState->insideDraw);
    object.draw(context);
    QCPDrawList recording = context.takeDrawList();
    if (!axes && qcpNeedsAxes(recording))
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
    mState->recordedGeneration = mState->generation;
    mState->recordedEpoch = epoch;
    mState->recordedItemsEpoch = itemsEpoch;
    mState->recordedFrame = frameEpoch;
    mState->recordedSource = source;
    mState->recordedDependency = dependency;
    mState->recordedExporting = view.exporting;
    // A caller that built the frame already has the transform; one that did not (a hit test, an
    // extent) builds it, and only a recording that reads it keeps it.
    if (mState->transformDependent)
        mState->recordedTransform = known ? known->transform
                                          : QCPTransform(view.keyAxis, view.valueAxis);
    mState->recorded = true;
    ++mState->recordings;
    return true;
}

std::optional<QCPDataBox> QCPDrawnCore::dataRange(const QCPDrawable& object,
                                                  const QCPDrawnView& view, bool applyLogRule)
{
    if (!ensureRecording(object, view))
        return std::nullopt;
    const QCPDrawList& list = mState->recording;
    // The recording already knows which sides every command was data on; a log axis cannot index a
    // box that reaches zero or below, exactly as QCPAbstractItem::positionsExtent decides it.
    const QCPDrawList::Box& side = list.data;
    bool keyBounded = side.keyBounded;
    bool valueBounded = side.valueBounded;
    if (applyLogRule)
    {
        const bool keyLog = view.keyAxis->scaleType() == QCPAxis::stLogarithmic;
        const bool valueLog = view.valueAxis->scaleType() == QCPAxis::stLogarithmic;
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
    double pad = 0.0;
    for (const qcp::QCPDrawCommand& command : mState->recording.commands)
        pad = std::max(pad,
                       std::visit(
                           [](const auto& cmd) -> double
                           {
                               using Command = std::decay_t<decltype(cmd)>;
                               if constexpr (std::is_same_v<Command, qcp::MarkersCmd>)
                                   return cmd.style->size() / 2.0;
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

std::optional<double> QCPDrawnCore::hitTest(const QCPDrawable& object, QCP::Pixels at,
                                            QVariant* details, const QCPDrawnView& view)
{
    if (!ensureRecording(object, view))
        return std::nullopt;
    const double tolerance = view.plot ? double(view.plot->selectionTolerance()) : 5.0;
    const QPointF point(at.x, at.y);
    // Culling uses the index box: the same box that decides whether the item index keeps it.
    const QCPDrawList::Box& side = mState->recording.index;
    std::optional<QCPDataBox> box;
    if (side.keyBounded || side.valueBounded)
        box = side.bounds;
    const QCPDrawFrame frame
        = qcpMakeFrame(view.keyAxis, view.valueAxis, view.axisRect, QRectF(view.clip));
    const double pad = pixelPad();
    if (box)
    {
        // Bounds first: the recording's box, mapped and grown by the widest stroke. Only the sides
        // the box really has are checked: an unbounded one is ±inf, which maps to a pixel far
        // outside the rect or to NaN on a log axis, and either would reject every point.
        const double grow = pad + tolerance;
        // Each side moves along its own pixel axis: with a vertical key axis, map() returns the
        // value pixel first (QCPTransform::map).
        const bool keyVertical = frame.transform.keyIsVertical();
        const auto keyPixels = [&](double key)
        {
            const QCP::Pixels pixels = frame.transform.map(QCP::Data { key, 0.0 });
            return keyVertical ? pixels.y : pixels.x;
        };
        const auto valuePixels = [&](double value)
        {
            const QCP::Pixels pixels = frame.transform.map(QCP::Data { 0.0, value });
            return keyVertical ? pixels.x : pixels.y;
        };
        // The click coordinate follows the same swap: with a vertical key axis the key runs down
        // the screen and the value across it.
        const double keyClick = keyVertical ? point.y() : point.x();
        const double valueClick = keyVertical ? point.x() : point.y();
        if (!nearSide(keyPixels(box->keyLower), keyPixels(box->keyUpper), keyClick, grow)
            || !nearSide(valuePixels(box->valueLower), valuePixels(box->valueUpper), valueClick,
                         grow))
            return std::nullopt;
    }
    double best = kInfinity;
    int hit = -1; //!< the rect of a batch the click came nearest: what \a details reports
    for (const qcp::QCPDrawCommand& command : mState->recording.commands)
    {
        int index = -1;
        const double distance = distanceTo(command, frame, point, &index);
        if (distance < best)
        {
            best = distance;
            hit = index;
        }
    }
    if (!(best <= tolerance))
        return std::nullopt;
    // A batch names the rect that was hit, so the caller can act on one bar without scanning the
    // geometry again. Commands of a single shape say nothing, as they always did.
    if (details && hit >= 0)
        *details = QVariant(hit);
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

void QCPDrawnCore::paint(const QCPDrawable& object, QCPPainter& painter, const QCPDrawnView& view)
{
    // An export resolves what a screen frame waits for: the recording knows which it was made for.
    QCPDrawnView painted = view;
    painted.exporting = painter.isExporting();
    // The layer clips every child it draws at clipRect().translated(0, -1), the half pixel
    // QCPPainter's antialiasing shifts it by: the replay sets the same clip, so a fill that ends on
    // the clip's edge lands on the same rows it does on screen.
    const QRect clip = view.clip.translated(0, -1);
    const quint64 frameEpoch = view.plot ? view.plot->frameEpoch() : 0;
    FrameMemo& memo = frameMemo();
    if (memo.plot != view.plot || memo.epoch != frameEpoch || memo.keyAxis != view.keyAxis
        || memo.valueAxis != view.valueAxis || memo.axisRect != view.axisRect
        || memo.clip != clip)
    {
        memo.frame = qcpMakeFrame(view.keyAxis, view.valueAxis, view.axisRect, QRectF(clip));
        memo.plot = view.plot;
        memo.epoch = frameEpoch;
        memo.keyAxis = view.keyAxis;
        memo.valueAxis = view.valueAxis;
        memo.axisRect = view.axisRect;
        memo.clip = clip;
    }
    const QCPDrawFrame& frame = memo.frame;
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
            qcpReplayWithPainter(mState->recording, painter, frame, QCPReplayKinds::All, nullptr,
                                 true, false, QCPReplayState::AlreadySaved);
            notePainted(view);
            return;
        }
        // Spec 5: an export stacks like the screen - this layer's painter content first, then the
        // GPU commands' QPainter forms over it, at the end of the layer's pass. Another object's
        // draw can delete this one before then, so the callback asks whether the recording is
        // still there: an object deleted mid-pass draws nothing more.
        qcpReplayWithPainter(mState->recording, painter, frame, QCPReplayKinds::PainterOnly, nullptr,
                             true, false, QCPReplayState::AlreadySaved);
        if (view.layer)
        {
            // The deferred pass paints with the antialiasing this object drew with: the layer's
            // painter is in whatever state the last layerable left, and a GPU command's QPainter
            // form must look like the command did on screen. QCPPainter::setAntialiasing also
            // restores the half-pixel shift the off state carried.
            // The busy fade the layer put on this object is part of that state too: the deferred
            // pass runs after the layer's restore(), where the painter is opaque again.
            // The busy fade the layer put on this object is part of that state too: the deferred
            // pass runs after the layer's restore(), where the painter is opaque again.
            const bool antialiased = painter.renderHints().testFlag(QPainter::Antialiasing);
            const qreal opacity = painter.opacity();
            view.layer->deferGpuPainter(
                [list = &mState->recording, state = std::weak_ptr<State>(mState), frame,
                 antialiased, opacity](QCPPainter& target)
                {
                    if (state.expired())
                        return;
                    target.save();
                    target.setOpacity(opacity);
                    qcpReplayWithPainter(*list, target, frame, QCPReplayKinds::GpuOnly, nullptr,
                                         true, antialiased);
                    target.restore();
                });
        }
        return;
    }
    qcpReplayWithRhi(mState->recording, *plottables, *scatter, frame, mState->resolve,
                     mState->raster, view.plot->bufferDevicePixelRatio(),
                     view.plot->rhiOutputSize(), mState->recordedGeneration, &mState->geometry,
                     static_cast<float>(painter.opacity()));
    // The painter pass completes what the layer refused, so it makes the same dashed decision.
    if (!plottables->dashedAvailable() && mState->movesWithTheData)
        mState->movesWithTheData = qcpRecordingMovesWithTheData(mState->recording, false);
    qcpReplayWithPainter(mState->recording, painter, frame, QCPReplayKinds::PainterOnly,
                         &mState->raster, plottables->dashedAvailable(), false,
                         QCPReplayState::AlreadySaved);
    notePainted(view);
}

void QCPDrawnCore::paintIcon(const QCPDrawable& object, QCPPainter& painter, const QRectF& rect,
                             const QCPDrawnView& view)
{
    const quint64 epoch = view.plot ? view.plot->viewEpoch() : 0;
    const void* source = object.dataSourceIdentity();
    const quint64 dependency = object.dependencyRevision();
    // The icon is a recording of its own, so it outlives a frame only when none of the facts it
    // read changed: its generation, the data it reads, and the view epoch a theme reader needs.
    const bool stale = !mState->iconRecorded || mState->iconGeneration != mState->generation
        || mState->iconSource != source || mState->iconDependency != dependency
        || (mState->icon.viewDependent && mState->iconEpoch != epoch);
    if (stale)
    {
        // An icon records through the same door, into a list of its own: the axes only feed the
        // context's unused frame, so an object outside a plot still gets its icon.
        QCPDrawContext context(view.keyAxis, view.valueAxis, view.axisRect, view.theme);
        const InsideDraw drawing(mState->insideDraw);
        object.drawLegendIcon(context);
        mState->icon = context.drawList();
        mState->iconGeneration = mState->generation;
        mState->iconEpoch = epoch;
        mState->iconSource = source;
        mState->iconDependency = dependency;
        mState->iconRecorded = true;
    }
    if (!mState->icon.data.keyBounded || !mState->icon.data.valueBounded)
        return; // nothing to fit the recording into the rect with
    const QCPDrawFrame frame = qcpMakeFittedFrame(mState->icon.data.bounds, rect);
    qcpReplayWithPainter(mState->icon, painter, frame, QCPReplayKinds::All);
}
