#include "draw-commands.h"

#include "draw-context.h"
#include "painter.h"
#include "raster-fallback.h"

#include "../axis/labelrenderer.h"
#include "../plottables/plottable-draw-utils.h"

#include <QPolygonF>

#include <algorithm>
#include <cmath>
#include <type_traits>

namespace
{
// Reused across replays on this thread: a steady-state replay allocates nothing.
struct Scratch
{
    std::vector<QPointF> data; // data-space points of one command, before the batch map
    std::vector<QPointF> pixels; // the same points in pixels, or the resolved positions
    QPolygonF polygon;
};

Scratch& scratch()
{
    thread_local Scratch local;
    return local;
}

//! Where a box of \a size sits relative to the anchor: QCPItemText::getTextDrawPoint, which halves
//! a double and rounds the point - an odd size does not truncate the same way.
QPoint boxTopLeft(const QSize& size, Qt::Alignment alignment)
{
    QPointF topLeft;
    if (alignment & Qt::AlignHCenter)
        topLeft.rx() = -size.width() / 2.0;
    else if (alignment & Qt::AlignRight)
        topLeft.rx() = -size.width();
    if (alignment & Qt::AlignVCenter)
        topLeft.ry() = -size.height() / 2.0;
    else if (alignment & Qt::AlignBottom)
        topLeft.ry() = -size.height();
    return topLeft.toPoint();
}

const QCPLabelRenderer* textRenderer(const QCPTextStyle& style)
{
    return style.mathText ? QCPLabelRenderer::defaultRenderer() : nullptr;
}
} // namespace

QSize qcpMeasureText(const QCPTextStyle& style, const QString& text)
{
    return QCPLabelRenderer::measureRectWith(textRenderer(style), style.font, text, style.textFlags)
        .size();
}

qcp::QCPTextLayout qcpTextLayout(const QCPTextStyle& style, const QSize& glyphSize)
{
    QRect glyphs(QPoint(0, 0), glyphSize);
    const int left = int(std::ceil(style.padding.left()));
    const int top = int(std::ceil(style.padding.top()));
    QRect box = glyphs.adjusted(-left, -top, int(std::ceil(style.padding.right())),
                                int(std::ceil(style.padding.bottom())));
    const QPoint topLeft = boxTopLeft(box.size(), style.alignment);
    glyphs.moveTopLeft(topLeft + QPoint(left, top));
    box.moveTopLeft(topLeft);
    return { box, glyphs };
}

qcp::QCPTextLayout qcpTextLayout(const QCPTextStyle& style, const QString& text)
{
    return qcpTextLayout(style, qcpMeasureText(style, text));
}

namespace
{
//! The box and glyphs, exactly as QCPItemText lays them out relative to the anchor.
void replayText(const qcp::TextCmd& cmd, QCPPainter& painter, const QCPDrawFrame& frame)
{
    const QCPTextStyle& style = *cmd.style;
    painter.save();
    painter.translate(qcpResolvePosition(frame, cmd.at));
    if (style.rotationDegrees != 0.0)
        painter.rotate(style.rotationDegrees);
    const qcp::QCPTextLayout layout = qcpTextLayout(style, cmd.glyphSize);
    if (style.brush.style() != Qt::NoBrush || style.pen.style() != Qt::NoPen)
    {
        painter.setPen(style.pen);
        painter.setBrush(style.brush);
        painter.drawRect(layout.box);
        painter.setBrush(Qt::NoBrush);
    }
    QCPLabelRenderer::drawWith(textRenderer(style), &painter, layout.glyphs, style.font,
                               style.color, cmd.text, style.textFlags);
    painter.restore();
}
} // namespace

namespace
{
//! The kinds the RHI replay draws: on the screen the painter must leave them to it.
bool isGpuKind(const qcp::QCPDrawCommand& command)
{
    return std::visit(
        [](const auto& cmd)
        {
            using Command = std::decay_t<decltype(cmd)>;
            return std::is_same_v<Command, qcp::PolylineDataCmd>
                || std::is_same_v<Command, qcp::PolylinePositionsCmd>
                || std::is_same_v<Command, qcp::PolygonCmd>
                || std::is_same_v<Command, qcp::FillBetweenCmd>
                || std::is_same_v<Command, qcp::RectCmd> || std::is_same_v<Command, qcp::EllipseCmd>
                || std::is_same_v<Command, qcp::ErrorBarsCmd>
                || std::is_same_v<Command, qcp::MarkersCmd>;
        },
        command);
}
} // namespace

namespace
{
//! How far a command's stroke reaches beyond the geometry it is given.
double strokePad(const QPen& pen)
{
    return pen.style() == Qt::NoPen ? 1.0 : std::max(1.0, pen.widthF()) + 1.0;
}

//! True only when the command is provably outside \a clip: a recording may cover far more than the
//! frame shows (a bar chart records every bar), and drawing what nobody sees costs the most.
bool outsideClip(const qcp::QCPDrawCommand& command, const QCPDrawFrame& frame)
{
    if (frame.clip.isEmpty())
        return false;
    return std::visit(
        [&](const auto& cmd)
        {
            using Command = std::decay_t<decltype(cmd)>;
            if constexpr (std::is_same_v<Command, qcp::RectCmd>
                          || std::is_same_v<Command, qcp::EllipseCmd>
                          || std::is_same_v<Command, qcp::ImageCmd>)
            {
                double pad = 1.0;
                if constexpr (!std::is_same_v<Command, qcp::ImageCmd>)
                    pad = strokePad(cmd.pen);
                const QPointF a = qcpResolvePosition(frame, cmd.a);
                const QPointF b = qcpResolvePosition(frame, cmd.b);
                return !QRectF(a, b)
                            .normalized()
                            .adjusted(-pad, -pad, pad, pad)
                            .intersects(frame.clip);
            }
            else if constexpr (std::is_same_v<Command, qcp::PolygonCmd>
                               || std::is_same_v<Command, qcp::PolylinePositionsCmd>)
            {
                if (cmd.points.empty())
                    return true;
                QRectF box;
                for (const QCP::Position& position : cmd.points)
                {
                    const QPointF point = qcpResolvePosition(frame, position);
                    box |= QRectF(point.x(), point.y(), 0.0, 0.0);
                }
                const double pad = strokePad(cmd.pen);
                return !box.normalized().adjusted(-pad, -pad, pad, pad).intersects(frame.clip);
            }
            else if constexpr (std::is_same_v<Command, qcp::TextCmd>)
            {
                QTransform transform;
                const QPointF at = qcpResolvePosition(frame, cmd.at);
                transform.translate(at.x(), at.y());
                transform.rotate(cmd.style->rotationDegrees);
                return !transform.mapRect(QRectF(qcpTextLayout(*cmd.style, cmd.glyphSize).box))
                            .intersects(frame.clip);
            }
            else if constexpr (std::is_same_v<Command, qcp::MarkersCmd>)
            {
                if (cmd.points->empty())
                    return true;
                QRectF box;
                for (const QPointF& point : *cmd.points)
                {
                    const QCP::Pixels pixel
                        = frame.transform.map(QCP::Data { point.x(), point.y() });
                    box |= QRectF(pixel.x, pixel.y, 0.0, 0.0);
                }
                const double pad = cmd.style.size() + 1.0;
                return !box.normalized().adjusted(-pad, -pad, pad, pad).intersects(frame.clip);
            }
            else
            {
                return false; // a data polyline: mapping its points costs what drawing it costs
            }
        },
        command);
}
} // namespace

void qcpReplayWithPainter(const QCPDrawList& list, QCPPainter& painter, const QCPDrawFrame& frame,
                          QCPReplayKinds kinds, const qcp::QCPRasterFallback* fallback)
{
    // PainterOnly leaves the GPU kinds to the RHI replay: without the fallback it cannot know
    // which of them the layer refused, so it would silently drop them.
    Q_ASSERT(kinds == QCPReplayKinds::All || fallback);
    painter.save();
    if (!frame.clip.isEmpty())
        painter.setClipRect(frame.clip);
    Scratch& local = scratch();
    for (const qcp::QCPDrawCommand& command : list.commands)
    {
        if (kinds == QCPReplayKinds::PainterOnly && isGpuKind(command))
        {
            // The layer could not take this command's image: paint it here, in the buffer the GPU
            // geometry draws over. A stroke the GPU cannot express is part of it.
            if (fallback && fallback->painterHandled(list.id, qcpCommandIndex(list, command)))
                qcp::paintFallback(painter, command, frame, qcp::strokeNeedsRaster(command));
            continue;
        }
        if (outsideClip(command, frame))
            continue;
        std::visit(
            [&](const auto& cmd)
            {
                using Command = std::decay_t<decltype(cmd)>;
                if constexpr (std::is_same_v<Command, qcp::PolylineDataCmd>)
                {
                    const std::span<const QPointF> data(*cmd.points);
                    local.pixels.resize(data.size());
                    frame.transform.map(data, std::span<QPointF>(local.pixels));
                    painter.setPen(cmd.pen);
                    qcp::drawPolylineSplitNaN(&painter, std::span<const QPointF>(local.pixels));
                }
                else if constexpr (std::is_same_v<Command, qcp::PolylinePositionsCmd>)
                {
                    qcpResolveInto(cmd.points, frame, local.pixels);
                    painter.setPen(cmd.pen);
                    qcp::drawPolylineSplitNaN(&painter, std::span<const QPointF>(local.pixels));
                }
                else if constexpr (std::is_same_v<Command, qcp::PolygonCmd>)
                {
                    qcpResolveInto(cmd.points, frame, local.pixels);
                    painter.setPen(cmd.pen);
                    painter.setBrush(cmd.brush);
                    qcpPolygonOf(local.pixels, local.polygon);
                    painter.drawPolygon(local.polygon);
                }
                else if constexpr (std::is_same_v<Command, qcp::FillBetweenCmd>)
                {
                    const std::size_t upper = cmd.upper->size();
                    const std::size_t lower = cmd.lower->size();
                    local.data.resize(upper + lower);
                    std::copy(cmd.upper->cbegin(), cmd.upper->cend(), local.data.begin());
                    std::reverse_copy(cmd.lower->cbegin(), cmd.lower->cend(),
                                      local.data.begin() + std::ptrdiff_t(upper));
                    local.pixels.resize(local.data.size());
                    frame.transform.map(std::span<const QPointF>(local.data),
                                        std::span<QPointF>(local.pixels));
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(cmd.brush);
                    qcpPolygonOf(local.pixels, local.polygon);
                    painter.drawPolygon(local.polygon);
                }
                else if constexpr (std::is_same_v<Command, qcp::RectCmd>)
                {
                    const QRectF rect
                        = QRectF(qcpResolvePosition(frame, cmd.a), qcpResolvePosition(frame, cmd.b))
                              .normalized();
                    painter.setPen(cmd.pen);
                    painter.setBrush(cmd.brush);
                    painter.drawRect(rect);
                }
                else if constexpr (std::is_same_v<Command, qcp::EllipseCmd>)
                {
                    const QRectF rect
                        = QRectF(qcpResolvePosition(frame, cmd.a), qcpResolvePosition(frame, cmd.b))
                              .normalized();
                    painter.setPen(cmd.pen);
                    painter.setBrush(cmd.brush);
                    painter.drawEllipse(rect);
                }
                else if constexpr (std::is_same_v<Command, qcp::MarkersCmd>)
                {
                    const std::span<const QPointF> data(*cmd.points);
                    local.pixels.resize(data.size());
                    frame.transform.map(data, std::span<QPointF>(local.pixels));
                    cmd.style.applyTo(&painter, QPen(Qt::black));
                    for (const QPointF& point : local.pixels)
                        cmd.style.drawShape(&painter, point);
                }
                else if constexpr (std::is_same_v<Command, qcp::ErrorBarsCmd>)
                {
                    qcpErrorBarPixels(frame, std::span<const QPointF>(*cmd.centres),
                                      std::span<const QPointF>(*cmd.spans), local.data);
                    painter.setPen(cmd.pen);
                    qcp::drawPolylineSplitNaN(&painter, std::span<const QPointF>(local.data));
                }
                else if constexpr (std::is_same_v<Command, qcp::TextCmd>)
                {
                    replayText(cmd, painter, frame);
                }
                else if constexpr (std::is_same_v<Command, qcp::PathCmd>)
                {
                    painter.save(); // the origin translation is this command's own
                    painter.translate(qcpResolvePosition(frame, cmd.origin));
                    painter.setPen(cmd.pen);
                    painter.setBrush(cmd.brush);
                    painter.drawPath(cmd.path);
                    painter.restore();
                }
                else if constexpr (std::is_same_v<Command, qcp::ImageCmd>)
                {
                    const QRectF rect
                        = QRectF(qcpResolvePosition(frame, cmd.a), qcpResolvePosition(frame, cmd.b))
                              .normalized();
                    painter.drawImage(rect, cmd.image);
                }
                else if constexpr (std::is_same_v<Command, qcp::PainterCmd>)
                {
                    painter.save(); // a callback may leave any state changed
                    cmd.callback(painter, frame.transform);
                    painter.restore();
                }
            },
            command);
    }
    painter.restore();
}
