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

QPointF dataPixel(const QCPDrawFrame& frame, const QPointF& point)
{
    const QCP::Pixels pixels = frame.transform.map(QCP::Data { point.x(), point.y() });
    return QPointF(pixels.x, pixels.y);
}

void replayText(const qcp::TextCmd& cmd, QCPPainter& painter, const QCPDrawFrame& frame)
{
    const QCPTextStyle& style = *cmd.style;
    const QPointF anchor = qcpResolvePosition(frame, cmd.at);
    const int flags = int(style.alignment);
    painter.save();
    painter.translate(anchor);
    if (style.rotationDegrees != 0.0)
        painter.rotate(style.rotationDegrees);
    QRect rect = QCPLabelRenderer::measureRectWith(nullptr, style.font, cmd.text, flags);
    if (style.padding > 0.0)
    {
        const int pad = int(std::ceil(style.padding));
        rect = rect.adjusted(-pad, -pad, pad, pad);
    }
    QCPLabelRenderer::drawWith(nullptr, &painter, rect, style.font, style.color, cmd.text, flags);
    painter.restore();
}

// The four corners of each bar in one batch: the centre plus and minus the span on each axis.
void replayErrorBars(const qcp::ErrorBarsCmd& cmd, QCPPainter& painter, const QCPDrawFrame& frame)
{
    const std::size_t count = std::min(cmd.centres->size(), cmd.spans->size());
    Scratch& local = scratch();
    qcpErrorBarCorners(std::span<const QPointF>(*cmd.centres), std::span<const QPointF>(*cmd.spans),
                       local.data);
    local.pixels.resize(local.data.size());
    frame.transform.map(std::span<const QPointF>(local.data), std::span<QPointF>(local.pixels));
    painter.setPen(cmd.pen);
    for (std::size_t i = 0; i < count; ++i)
    {
        const QPointF& above = local.pixels[4 * i + 0];
        const QPointF& below = local.pixels[4 * i + 1];
        const QPointF& left = local.pixels[4 * i + 2];
        const QPointF& right = local.pixels[4 * i + 3];
        painter.drawLine(above, below);
        painter.drawLine(QPointF(left.x(), above.y()), QPointF(right.x(), above.y()));
        painter.drawLine(QPointF(left.x(), below.y()), QPointF(right.x(), below.y()));
    }
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
                    replayErrorBars(cmd, painter, frame);
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
