#include "draw-commands.h"

#include "draw-context.h"
#include "painter.h"

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
    std::vector<QPointF> data;   // data-space points of one command, before the batch map
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

void resolveInto(const QCPDrawFrame& frame, const std::vector<QCP::Position>& positions,
                 std::vector<QPointF>& pixels)
{
    pixels.resize(positions.size());
    for (std::size_t i = 0; i < positions.size(); ++i)
        pixels[i] = qcpResolvePosition(frame, positions[i]);
}

QPolygonF& polygonOf(const std::vector<QPointF>& pixels)
{
    QPolygonF& polygon = scratch().polygon;
    polygon.resize(int(pixels.size()));
    std::copy(pixels.begin(), pixels.end(), polygon.begin());
    return polygon;
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
    qcpErrorBarCorners(std::span<const QPointF>(*cmd.centres),
                       std::span<const QPointF>(*cmd.spans), local.data);
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
        [](const auto& cmd) {
            using Command = std::decay_t<decltype(cmd)>;
            return std::is_same_v<Command, qcp::PolylineDataCmd>
                || std::is_same_v<Command, qcp::PolylinePositionsCmd>
                || std::is_same_v<Command, qcp::PolygonCmd>
                || std::is_same_v<Command, qcp::FillBetweenCmd>
                || std::is_same_v<Command, qcp::RectCmd>
                || std::is_same_v<Command, qcp::EllipseCmd>
                || std::is_same_v<Command, qcp::ErrorBarsCmd>
                || std::is_same_v<Command, qcp::MarkersCmd>;
        },
        command);
}
} // namespace

namespace
{
// The fills a GPU frame leaves to this pass: a free-form polygon (no GPU tessellator) and any
// brush the GPU would flatten. Fill only, no outline: the GPU drew the stroke.
void paintSkippedFill(QCPPainter& painter, const QCPDrawFrame& frame,
                      const qcp::QCPDrawCommand& command)
{
    Scratch& local = scratch();
    painter.setPen(Qt::NoPen);
    std::visit(
        [&](const auto& cmd) {
            using Command = std::decay_t<decltype(cmd)>;
            if constexpr (std::is_same_v<Command, qcp::PolygonCmd>)
            {
                painter.setBrush(cmd.brush);
                resolveInto(frame, cmd.points, local.pixels);
                painter.drawPolygon(polygonOf(local.pixels));
            }
            else if constexpr (std::is_same_v<Command, qcp::FillBetweenCmd>)
            {
                if (qcpGpuFillsBrush(cmd.brush))
                    return;
                painter.setBrush(cmd.brush);
                const std::size_t count = std::min(cmd.upper->size(), cmd.lower->size());
                local.data.resize(2 * count);
                for (std::size_t i = 0; i < count; ++i)
                {
                    local.data[i] = (*cmd.upper)[i];
                    local.data[2 * count - 1 - i] = (*cmd.lower)[i];
                }
                frame.transform.map(std::span<const QPointF>(local.data),
                                    std::span<QPointF>(local.pixels));
                painter.drawPolygon(polygonOf(local.pixels));
            }
            else if constexpr (std::is_same_v<Command, qcp::RectCmd>
                               || std::is_same_v<Command, qcp::EllipseCmd>)
            {
                if (qcpGpuFillsBrush(cmd.brush))
                    return;
                painter.setBrush(cmd.brush);
                const QRectF box(qcpResolvePosition(frame, cmd.a), qcpResolvePosition(frame, cmd.b));
                if constexpr (std::is_same_v<Command, qcp::RectCmd>)
                    painter.drawRect(box.normalized());
                else
                    painter.drawEllipse(box.normalized());
            }
        },
        command);
}
} // namespace

void qcpReplayWithPainter(const QCPDrawList& list, QCPPainter& painter, const QCPDrawFrame& frame,
                          QCPReplayKinds kinds)
{
    painter.save();
    if (!frame.clip.isEmpty())
        painter.setClipRect(frame.clip);
    Scratch& local = scratch();
    for (const qcp::QCPDrawCommand& command : list.commands)
    {
        if (kinds == QCPReplayKinds::PainterOnly && isGpuKind(command))
        {
            paintSkippedFill(painter, frame, command);
            continue;
        }
        std::visit(
            [&](const auto& cmd) {
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
                    resolveInto(frame, cmd.points, local.pixels);
                    painter.setPen(cmd.pen);
                    qcp::drawPolylineSplitNaN(&painter, std::span<const QPointF>(local.pixels));
                }
                else if constexpr (std::is_same_v<Command, qcp::PolygonCmd>)
                {
                    resolveInto(frame, cmd.points, local.pixels);
                    painter.setPen(cmd.pen);
                    painter.setBrush(cmd.brush);
                    painter.drawPolygon(polygonOf(local.pixels));
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
                    painter.drawPolygon(polygonOf(local.pixels));
                }
                else if constexpr (std::is_same_v<Command, qcp::RectCmd>)
                {
                    const QRectF rect =
                        QRectF(qcpResolvePosition(frame, cmd.a), qcpResolvePosition(frame, cmd.b))
                            .normalized();
                    painter.setPen(cmd.pen);
                    painter.setBrush(cmd.brush);
                    painter.drawRect(rect);
                }
                else if constexpr (std::is_same_v<Command, qcp::EllipseCmd>)
                {
                    const QRectF rect =
                        QRectF(qcpResolvePosition(frame, cmd.a), qcpResolvePosition(frame, cmd.b))
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
                    const QRectF rect =
                        QRectF(qcpResolvePosition(frame, cmd.a), qcpResolvePosition(frame, cmd.b))
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
