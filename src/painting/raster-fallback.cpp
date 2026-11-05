#include "raster-fallback.h"

#include "../plottables/plottable-draw-utils.h"

#include <QPainter>

#include <algorithm>
#include <cmath>

namespace
{
std::vector<QPointF> resolved(std::span<const QCP::Position> positions, const QCPDrawFrame& frame)
{
    std::vector<QPointF> points(positions.size());
    for (std::size_t i = 0; i < positions.size(); ++i)
        points[i] = qcpResolvePosition(frame, positions[i]);
    return points;
}

QPolygonF polygonOf(std::span<const QPointF> points)
{
    QPolygonF polygon;
    polygon.reserve(int(points.size()));
    for (const QPointF& point : points)
        polygon << point;
    return polygon;
}

// Data-space series are positions like any other: they go through the frame's transform.
QPolygonF polygonOfData(const std::vector<QPointF>& data, const QCPDrawFrame& frame)
{
    QPolygonF polygon;
    polygon.reserve(int(data.size()));
    for (const QPointF& point : data)
        polygon << qcpResolvePosition(frame, QCP::Data { point.x(), point.y() });
    return polygon;
}

// The raster must keep the stroke's outer half and its antialiasing; the fill alone needs one
// pixel.
double marginOf(const QPen& pen, double dpr, bool withStroke)
{
    if (!withStroke || pen.style() == Qt::NoPen)
        return 1.0;
    return double(qcp::extrusionPenWidth(pen, dpr)) / 2.0 + 1.0;
}
} // namespace

bool qcp::fillNeedsRaster(const QCPDrawCommand& command)
{
    return std::visit(
        [](const auto& cmd)
        {
            using T = std::decay_t<decltype(cmd)>;
            if constexpr (std::is_same_v<T, PolygonCmd>)
                return cmd.brush.style() != Qt::NoBrush;
            else if constexpr (std::is_same_v<T, FillBetweenCmd>)
                return cmd.brush.style() != Qt::NoBrush
                    && (!qcpGpuFillsBrush(cmd.brush) || cmd.upper->size() != cmd.lower->size());
            else if constexpr (std::is_same_v<T, RectCmd> || std::is_same_v<T, EllipseCmd>)
                return cmd.brush.style() != Qt::NoBrush && !qcpGpuFillsBrush(cmd.brush);
            else
                return false;
        },
        command);
}

QRectF qcp::fallbackPixelBounds(const QCPDrawCommand& command, const QCPDrawFrame& frame,
                                double dpr, bool withStroke)
{
    QRectF box;
    QPen pen = Qt::NoPen;
    std::visit(
        [&](const auto& cmd)
        {
            using T = std::decay_t<decltype(cmd)>;
            if constexpr (std::is_same_v<T, PolygonCmd>)
            {
                box = polygonOf(resolved(cmd.points, frame)).boundingRect();
                pen = cmd.pen;
            }
            else if constexpr (std::is_same_v<T, FillBetweenCmd>)
            {
                box = polygonOfData(*cmd.upper, frame).boundingRect();
                box = box.united(polygonOfData(*cmd.lower, frame).boundingRect());
            }
            else if constexpr (std::is_same_v<T, RectCmd> || std::is_same_v<T, EllipseCmd>)
            {
                box = QRectF(qcpResolvePosition(frame, cmd.a), qcpResolvePosition(frame, cmd.b))
                          .normalized();
                pen = cmd.pen;
            }
        },
        command);
    if (box.isEmpty())
        return box;
    const double margin = marginOf(pen, dpr, withStroke);
    return box.adjusted(-margin, -margin, margin, margin);
}

void qcp::paintFallback(QPainter& painter, const QCPDrawCommand& command, const QCPDrawFrame& frame,
                        bool withStroke)
{
    std::visit(
        [&](const auto& cmd)
        {
            using T = std::decay_t<decltype(cmd)>;
            if constexpr (std::is_same_v<T, PolygonCmd>)
            {
                const QPolygonF polygon = polygonOf(resolved(cmd.points, frame));
                if (polygon.size() < 3)
                    return;
                painter.setBrush(cmd.brush);
                painter.setPen(withStroke ? cmd.pen : QPen(Qt::NoPen));
                painter.drawPolygon(polygon);
            }
            else if constexpr (std::is_same_v<T, FillBetweenCmd>)
            {
                QPolygonF polygon = polygonOfData(*cmd.upper, frame);
                const QPolygonF lower = polygonOfData(*cmd.lower, frame);
                for (auto it = lower.crbegin(); it != lower.crend(); ++it)
                    polygon << *it;
                painter.setBrush(cmd.brush);
                painter.setPen(Qt::NoPen);
                painter.drawPolygon(polygon);
            }
            else if constexpr (std::is_same_v<T, RectCmd> || std::is_same_v<T, EllipseCmd>)
            {
                const QRectF box
                    = QRectF(qcpResolvePosition(frame, cmd.a), qcpResolvePosition(frame, cmd.b))
                          .normalized();
                painter.setBrush(cmd.brush);
                painter.setPen(withStroke ? cmd.pen : QPen(Qt::NoPen));
                if constexpr (std::is_same_v<T, RectCmd>)
                    painter.drawRect(box);
                else
                    painter.drawEllipse(box);
            }
        },
        command);
}

QImage qcp::rasterizeFallback(const QCPDrawCommand& command, const QRectF& bounds,
                              const QCPDrawFrame& frame, double dpr, bool withStroke)
{
    const QSize device(int(std::ceil(bounds.width() * dpr)), int(std::ceil(bounds.height() * dpr)));
    if (device.isEmpty())
        return { };
    QImage image(device, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(dpr, dpr);
    painter.translate(-bounds.topLeft());
    paintFallback(painter, command, frame, withStroke);
    painter.end();
    return image;
}
