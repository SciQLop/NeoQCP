#include "raster-fallback.h"

#include "../plottables/plottable-draw-utils.h"
#include "painter.h"
#include "plottable-rhi-layer.h"

#include <QDebug>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace qcp
{
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

QPolygonF polylineOfPositions(std::span<const QCP::Position> positions, const QCPDrawFrame& frame)
{
    QPolygonF polygon;
    polygon.reserve(int(positions.size()));
    for (const QCP::Position& position : positions)
        polygon << qcpResolvePosition(frame, position);
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

QPen penOf(const QCPDrawCommand& command)
{
    return std::visit(
        [](const auto& cmd)
        {
            using T = std::decay_t<decltype(cmd)>;
            if constexpr (requires { cmd.pen; })
                return cmd.pen;
            else
                return QPen(Qt::NoPen);
        },
        command);
}
} // namespace

bool strokeNeedsRaster(const QPen& pen)
{
    return pen.style() != Qt::NoPen && !gpuCanFill(pen.brush());
}

bool strokeNeedsRaster(const QCPDrawCommand& command)
{
    return strokeNeedsRaster(penOf(command));
}

bool fillNeedsRaster(const QCPDrawCommand& command)
{
    return std::visit(
        [](const auto& cmd)
        {
            using T = std::decay_t<decltype(cmd)>;
            if constexpr (std::is_same_v<T, PolygonCmd>)
                return cmd.brush.style() != Qt::NoBrush;
            else if constexpr (std::is_same_v<T, FillBetweenCmd>)
                return cmd.brush.style() != Qt::NoBrush
                    && (!gpuCanFill(cmd.brush) || cmd.upper->size() != cmd.lower->size());
            else if constexpr (std::is_same_v<T, RectCmd> || std::is_same_v<T, EllipseCmd>)
                return cmd.brush.style() != Qt::NoBrush && !gpuCanFill(cmd.brush);
            else
                return false;
        },
        command);
}

bool needsRaster(const QCPDrawCommand& command)
{
    return fillNeedsRaster(command) || strokeNeedsRaster(penOf(command));
}

QRectF fallbackPixelBounds(const QCPDrawCommand& command, const QCPDrawFrame& frame,
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
            else if constexpr (std::is_same_v<T, PolylinePositionsCmd>)
            {
                box = polylineOfPositions(cmd.points, frame).boundingRect();
                pen = cmd.pen;
            }
            else if constexpr (std::is_same_v<T, PolylineDataCmd>)
            {
                box = polygonOfData(*cmd.points, frame).boundingRect();
                pen = cmd.pen;
            }
        },
        command);
    if (box.isEmpty())
        return box;
    const double margin = marginOf(pen, dpr, withStroke);
    return box.adjusted(-margin, -margin, margin, margin);
}

void paintFallback(QCPPainter& painter, const QCPDrawCommand& command, const QCPDrawFrame& frame,
                        bool withStroke)
{
    std::visit(
        [&](const auto& cmd)
        {
            using T = std::decay_t<decltype(cmd)>;
            painter.setAntialiasing(true);
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
            else if constexpr (std::is_same_v<T, PolylinePositionsCmd>)
            {
                painter.setBrush(Qt::NoBrush);
                painter.setPen(cmd.pen);
                drawPolylineSplitNaN(&painter, polylineOfPositions(cmd.points, frame));
            }
            else if constexpr (std::is_same_v<T, PolylineDataCmd>)
            {
                painter.setBrush(Qt::NoBrush);
                painter.setPen(cmd.pen);
                drawPolylineSplitNaN(&painter, polygonOfData(*cmd.points, frame));
            }
        },
        command);
}

QImage rasterizeFallback(const QCPDrawCommand& command, const QRectF& bounds,
                              const QCPDrawFrame& frame, double dpr, bool withStroke)
{
    const QSize device(int(std::ceil(bounds.width() * dpr)), int(std::ceil(bounds.height() * dpr)));
    if (device.isEmpty())
        return { };
    QImage image(device, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QCPPainter painter(&image);
    painter.scale(dpr, dpr);
    painter.translate(-bounds.topLeft());
    paintFallback(painter, command, frame, withStroke);
    painter.end();
    return image;
}

void QCPRasterFallback::beginReplay()
{
    mPainterHandled.clear();
    for (auto it = mEntries.begin(); it != mEntries.end();)
    {
        if (!it->used)
            it = mEntries.erase(it);
        else
        {
            it->used = false;
            ++it;
        }
    }
}

const QImage& QCPRasterFallback::image(const QCPDrawCommand& command, const QCPDrawFrame& frame,
                                       const QRectF& bounds, double dpr, quint64 generation,
                                       bool& fresh)
{
    const quintptr key = reinterpret_cast<quintptr>(&command);
    const QSize size(int(std::ceil(bounds.width() * dpr)), int(std::ceil(bounds.height() * dpr)));
    for (Entry& entry : mEntries)
    {
        if (entry.used || entry.command != key || entry.generation != generation || entry.dpr != dpr
            || entry.size != size)
            continue;
        entry.used = true;
        fresh = false;
        return entry.image;
    }

    Entry& entry = mEntries.emplace_back();
    entry.command = key;
    entry.generation = generation;
    entry.dpr = dpr;
    entry.size = size;
    entry.used = true;
    entry.image = rasterizeFallback(command, bounds, frame, dpr, strokeNeedsRaster(penOf(command)));
    fresh = true;
    return entry.image;
}

void QCPRasterFallback::painterHandles(const QCPDrawCommand& command)
{
    mPainterHandled.insert(reinterpret_cast<quintptr>(&command));
    if (!mWarned)
    {
        mWarned = true;
        qWarning() << "NeoQCP: more than" << QCPPlottableRhiLayer::kMaxImages
                   << "rasterised styles in one layer, the rest is painted by QPainter";
    }
}

bool QCPRasterFallback::painterHandled(const QCPDrawCommand& command) const
{
    return mPainterHandled.contains(reinterpret_cast<quintptr>(&command));
}

void QCPRasterFallback::clear()
{
    mEntries.clear();
    mPainterHandled.clear();
}
} // namespace qcp
