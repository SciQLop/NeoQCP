#include "drawable.h"

#include "core.h"
#include "layoutelements/layoutelement-axisrect.h"
#include "misuse.h"
#include "painting/command-replay-rhi.h"
#include "painting/draw-commands.h"
#include "painting/draw-context.h"
#include "painting/painter.h"
#include "painting/raster-fallback.h"
#include "plottables/plottable-draw-utils.h"
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
            || std::holds_alternative<qcp::ImageCmd>(command)
            || std::holds_alternative<qcp::PainterCmd>(command))
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

//! The outline of an ellipse inscribed in \a box, closed: the hit test samples what the GPU fills.
std::vector<QPointF> ellipseRing(const QRectF& box, int segments = 96)
{
    const QPointF centre = box.center();
    const double rx = box.width() / 2.0;
    const double ry = box.height() / 2.0;
    std::vector<QPointF> ring;
    ring.reserve(std::size_t(segments));
    for (int i = 0; i < segments; ++i)
    {
        const double angle = 2.0 * M_PI * i / segments;
        ring.emplace_back(centre.x() + rx * std::cos(angle), centre.y() + ry * std::sin(angle));
    }
    return ring;
}

std::vector<QPointF> resolve(std::span<const QCP::Position> positions, const QCPDrawFrame& frame)
{
    std::vector<QPointF> pixels(positions.size());
    qcpResolveInto(positions, frame, pixels);
    return pixels;
}

std::vector<QPointF> resolve(const std::vector<QPointF>& data, const QCPDrawFrame& frame)
{
    std::vector<QPointF> pixels(data.size());
    frame.transform.map(std::span<const QPointF>(data), std::span<QPointF>(pixels));
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

//! The distance from \a at to one command's geometry, in pixels.
double distanceTo(const qcp::QCPDrawCommand& command, const QCPDrawFrame& frame, const QPointF& at)
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
                return distanceTo(ellipseRing(box), at, true) - halfPen(cmd.pen);
            }
            else if constexpr (std::is_same_v<Command, qcp::MarkersCmd>)
            {
                const std::vector<QPointF> pixels = resolve(*cmd.points, frame);
                return distanceTo(pixels, at, false) - cmd.style.size() / 2.0;
            }
            else if constexpr (std::is_same_v<Command, qcp::ErrorBarsCmd>)
            {
                std::vector<QPointF> pixels;
                qcpErrorBarPixels(frame, std::span<const QPointF>(*cmd.centres),
                                  std::span<const QPointF>(*cmd.spans), pixels);
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
    QCPExtrusionCache extrusion;
    quint64 generation = 0;
    quint64 recordedGeneration = 0;
    quint64 recordedEpoch = 0;
    quint64 recordedItemsEpoch = 0;
    QCPTransform recordedTransform;
    quint64 iconGeneration = 0;
    int recordings = 0;
    bool recorded = false;
    bool viewDependent = false;
    bool anchorDependent = false;
    bool transformDependent = false;
    bool iconRecorded = false;
    bool insideDraw = false;
    bool reportedMissingAxis = false;
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
    view.selectable = item.selectable();
    view.clip = clip;
    // simplify: an item mixing axis pairs across its positions is drawn through the first pair;
    // per-position frames when an item needs them.
    const QList<QCPItemPosition*> positions = item.positions();
    if (!positions.isEmpty())
    {
        view.keyAxis = positions.first()->keyAxis();
        view.valueAxis = positions.first()->valueAxis();
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

QCPDrawnCore::QCPDrawnCore() : mState(std::make_unique<State>()) { }

QCPDrawnCore::~QCPDrawnCore() = default;

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

int QCPDrawnCore::recordingCount() const
{
    return mState->recordings;
}

quint64 QCPDrawnCore::resolveRemapCount() const
{
    return mState->resolve.remapCount();
}

bool QCPDrawnCore::ensureRecording(const QCPDrawable& object, const QCPDrawnView& view)
{
    if (!view.keyAxis || !view.valueAxis)
    {
        if (!mState->reportedMissingAxis)
        {
            mState->reportedMissingAxis = true;
            qDebug() << "NeoQCP: a drawn object has no key or value axis; it is not drawn";
        }
        return false;
    }
    mState->reportedMissingAxis = false;
    const quint64 epoch = view.plot ? view.plot->viewEpoch() : 0;
    const quint64 itemsEpoch = view.plot ? view.plot->itemsEpoch() : 0;
    const QCPTransform transform(view.keyAxis, view.valueAxis);
    const bool stale = !mState->recorded || mState->recordedGeneration != mState->generation
        || (mState->viewDependent && mState->recordedEpoch != epoch)
        || (mState->anchorDependent && mState->recordedItemsEpoch != itemsEpoch)
        || (mState->transformDependent && !transform.isTranslationOf(mState->recordedTransform));
    if (!stale)
        return true;
    QCPDrawContext context(view.keyAxis, view.valueAxis, view.axisRect, view.theme);
    const InsideDraw drawing(mState->insideDraw);
    object.draw(context);
    mState->recording = context.takeDrawList();
    mState->viewDependent = usesTheView(mState->recording);
    mState->anchorDependent = mState->recording.anchorDependent;
    mState->transformDependent = mState->recording.transformDependent;
    mState->recordedGeneration = mState->generation;
    mState->recordedEpoch = epoch;
    mState->recordedItemsEpoch = itemsEpoch;
    mState->recordedTransform = transform;
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
    bool keyBounded = list.keyBounded;
    bool valueBounded = list.valueBounded;
    if (applyLogRule)
    {
        const bool keyLog = view.keyAxis->scaleType() == QCPAxis::stLogarithmic;
        const bool valueLog = view.valueAxis->scaleType() == QCPAxis::stLogarithmic;
        if (keyBounded && keyLog && !(list.bounds.keyLower > 0))
            keyBounded = false;
        if (valueBounded && valueLog && !(list.bounds.valueLower > 0))
            valueBounded = false;
    }
    if (!keyBounded && !valueBounded)
        return std::nullopt;
    QCPDataBox box = list.bounds;
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
    const std::optional<QCPDataBox> box = dataRange(object, view);
    if (!box)
        return std::nullopt;
    return QCPItemExtent { const_cast<QCPAxis*>(view.keyAxis), const_cast<QCPAxis*>(view.valueAxis),
                           *box, pixelPad() };
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
                                   return cmd.style.size() / 2.0;
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

std::optional<double> QCPDrawnCore::hitTest(const QCPDrawable& object, QCP::Pixels at, QVariant*,
                                            const QCPDrawnView& view)
{
    if (!ensureRecording(object, view))
        return std::nullopt;
    const double tolerance = view.plot ? double(view.plot->selectionTolerance()) : 5.0;
    const QPointF point(at.x, at.y);
    const std::optional<QCPDataBox> box = dataRange(object, view);
    const QCPDrawFrame frame
        = qcpMakeFrame(view.keyAxis, view.valueAxis, view.axisRect, QRectF(view.clip));
    const double pad = pixelPad();
    if (box)
    {
        // Bounds first: the recording's box, mapped and grown by the widest stroke. Only the sides
        // the box really has are checked: an unbounded one is ±inf, which maps to a pixel far
        // outside the rect or to NaN on a log axis, and either would reject every point.
        const double grow = pad + tolerance;
        const auto keyPixels
            = [&](double key) { return frame.transform.map(QCP::Data { key, 0.0 }).x; };
        const auto valuePixels
            = [&](double value) { return frame.transform.map(QCP::Data { 0.0, value }).y; };
        if (!nearSide(keyPixels(box->keyLower), keyPixels(box->keyUpper), point.x(), grow)
            || !nearSide(valuePixels(box->valueLower), valuePixels(box->valueUpper), point.y(),
                         grow))
            return std::nullopt;
    }
    double best = kInfinity;
    for (const qcp::QCPDrawCommand& command : mState->recording.commands)
        best = std::min(best, distanceTo(command, frame, point));
    if (!(best <= tolerance))
        return std::nullopt;
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
    if (!ensureRecording(object, view))
        return;
    const QCPDrawFrame frame
        = qcpMakeFrame(view.keyAxis, view.valueAxis, view.axisRect, QRectF(view.clip));
    QCPPlottableRhiLayer* plottables
        = painter.isExporting() ? nullptr : qcp::gpuLayer(view.plot, view.layer, false);
    QCPScatterRhiLayer* scatter = plottables ? view.plot->scatterRhiLayer(view.layer) : nullptr;
    if (!plottables || !scatter)
    {
        // An export has no GPU pass, and so has a screen without a backend: the painter draws it
        // all, and never touches the caches a screen frame resolves into.
        qcpReplayWithPainter(mState->recording, painter, frame, QCPReplayKinds::All);
        return;
    }
    qcpReplayWithRhi(mState->recording, *plottables, *scatter, frame, mState->resolve,
                     mState->raster, view.plot->bufferDevicePixelRatio(),
                     view.plot->rhiOutputSize(), mState->recordedGeneration, &mState->extrusion);
    qcpReplayWithPainter(mState->recording, painter, frame, QCPReplayKinds::PainterOnly,
                         &mState->raster);
}

void QCPDrawnCore::paintIcon(const QCPDrawable& object, QCPPainter& painter, const QRectF& rect,
                             const QCPDrawnView& view)
{
    if (mState->iconGeneration != mState->generation || !mState->iconRecorded)
    {
        // An icon records through the same door, into a list of its own: the axes only feed the
        // context's unused frame, so an object outside a plot still gets its icon.
        QCPDrawContext context(view.keyAxis, view.valueAxis, view.axisRect, view.theme);
        const InsideDraw drawing(mState->insideDraw);
        object.drawLegendIcon(context);
        mState->icon = context.drawList();
        mState->iconGeneration = mState->generation;
        mState->iconRecorded = true;
    }
    if (!mState->icon.keyBounded || !mState->icon.valueBounded)
        return; // nothing to fit the recording into the rect with
    const QCPDrawFrame frame = qcpMakeFittedFrame(mState->icon.bounds, rect);
    qcpReplayWithPainter(mState->icon, painter, frame, QCPReplayKinds::All);
}
