#include "draw-commands.h"

#include "draw-context.h"
#include "painter.h"

#include "../axis/labelrenderer.h"
#include "../plottables/plottable-draw-utils.h"

#include <QPolygonF>

#include <cmath>
#include <type_traits>

namespace
{
QPointF dataPixel(const QCPDrawFrame& frame, const QPointF& point)
{
    const QCP::Pixels pixels = frame.transform.map(QCP::Data { point.x(), point.y() });
    return QPointF(pixels.x, pixels.y);
}

void resolveInto(const QCPDrawFrame& frame, const std::vector<QCP::Position>& positions,
                 std::vector<QPointF>& pixels)
{
    pixels.clear();
    pixels.reserve(positions.size());
    for (const QCP::Position& position : positions)
        pixels.push_back(qcpResolvePosition(frame, position));
}

QPolygonF polygonOf(const std::vector<QPointF>& pixels)
{
    QPolygonF polygon;
    polygon.reserve(int(pixels.size()));
    for (const QPointF& pixel : pixels)
        polygon.append(pixel);
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

void replayErrorBars(const qcp::ErrorBarsCmd& cmd, QCPPainter& painter, const QCPDrawFrame& frame)
{
    const std::size_t count = std::min(cmd.centres->size(), cmd.spans->size());
    painter.setPen(cmd.pen);
    for (std::size_t i = 0; i < count; ++i)
    {
        const QPointF centre = cmd.centres->at(i);
        const QPointF span = cmd.spans->at(i);
        const QPointF above = dataPixel(frame, QPointF(centre.x(), centre.y() + span.y()));
        const QPointF below = dataPixel(frame, QPointF(centre.x(), centre.y() - span.y()));
        const QPointF left = dataPixel(frame, QPointF(centre.x() - span.x(), centre.y()));
        const QPointF right = dataPixel(frame, QPointF(centre.x() + span.x(), centre.y()));
        painter.drawLine(above, below);
        painter.drawLine(QPointF(left.x(), above.y()), QPointF(right.x(), above.y()));
        painter.drawLine(QPointF(left.x(), below.y()), QPointF(right.x(), below.y()));
    }
}
} // namespace

void qcpReplayWithPainter(const QCPDrawList& list, QCPPainter& painter, const QCPDrawFrame& frame)
{
    painter.save();
    if (!frame.clip.isEmpty())
        painter.setClipRect(frame.clip);
    std::vector<QPointF> pixels;
    for (const qcp::QCPDrawCommand& command : list.commands)
    {
        std::visit(
            [&](const auto& cmd) {
                using Command = std::decay_t<decltype(cmd)>;
                if constexpr (std::is_same_v<Command, qcp::PolylineDataCmd>)
                {
                    pixels.clear();
                    pixels.reserve(cmd.points->size());
                    for (const QPointF& point : *cmd.points)
                        pixels.push_back(dataPixel(frame, point));
                    painter.setPen(cmd.pen);
                    qcp::drawPolylineSplitNaN(&painter, std::span<const QPointF>(pixels));
                }
                else if constexpr (std::is_same_v<Command, qcp::PolylinePositionsCmd>)
                {
                    resolveInto(frame, cmd.points, pixels);
                    painter.setPen(cmd.pen);
                    qcp::drawPolylineSplitNaN(&painter, std::span<const QPointF>(pixels));
                }
                else if constexpr (std::is_same_v<Command, qcp::PolygonCmd>)
                {
                    resolveInto(frame, cmd.points, pixels);
                    painter.setPen(cmd.pen);
                    painter.setBrush(cmd.brush);
                    painter.drawPolygon(polygonOf(pixels));
                }
                else if constexpr (std::is_same_v<Command, qcp::FillBetweenCmd>)
                {
                    QPolygonF polygon;
                    polygon.reserve(int(cmd.upper->size() + cmd.lower->size()));
                    for (const QPointF& point : *cmd.upper)
                        polygon.append(dataPixel(frame, point));
                    for (auto it = cmd.lower->crbegin(); it != cmd.lower->crend(); ++it)
                        polygon.append(dataPixel(frame, *it));
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(cmd.brush);
                    painter.drawPolygon(polygon);
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
                    cmd.style.applyTo(&painter, QPen(Qt::black));
                    for (const QPointF& point : *cmd.points)
                        cmd.style.drawShape(&painter, dataPixel(frame, point));
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
                    painter.translate(qcpResolvePosition(frame, cmd.origin));
                    painter.setPen(cmd.pen);
                    painter.setBrush(cmd.brush);
                    painter.drawPath(cmd.path);
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
                    cmd.callback(painter, frame.transform);
                }
            },
            command);
    }
    painter.restore();
}
