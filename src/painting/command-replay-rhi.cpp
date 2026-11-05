#include "command-replay-rhi.h"

#include "../plottables/plottable-draw-utils.h"
#include "line-extruder.h"
#include "plottable-rhi-layer.h"
#include "rhi-utils.h"
#include "scatter-rhi-layer.h"
#include "Profiling.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace
{

// Per-replay buffers: a frame allocates nothing once they have grown.
std::vector<QPointF>& pixelScratch()
{
    static thread_local std::vector<QPointF> points;
    return points;
}

std::vector<float>& strokeScratch()
{
    static thread_local std::vector<float> verts;
    return verts;
}

std::vector<float>& fillScratch()
{
    static thread_local std::vector<float> verts;
    return verts;
}

std::vector<float>& markerScratch()
{
    static thread_local std::vector<float> points;
    return points;
}

bool isFillable(const QBrush& brush)
{
    return brush.style() != Qt::NoBrush && brush.color().alpha() != 0;
}

void addPolyline(QCPPlottableRhiLayer& layer, std::span<const QPointF> points, const QPen& pen,
                 const QRect& clip, double dpr, int height)
{
    if (!qcp::canStrokeOnGpu(pen) || points.size() < 2)
        return;
    const float width = qcp::extrusionPenWidth(pen, dpr);
    std::vector<float>& verts = strokeScratch();
    verts.clear();
    if (pen.style() == Qt::SolidLine)
        QCPLineExtruder::appendPolyline(points, width, pen.color(), verts);
    else
        QCPLineExtruder::appendPolylineWithDistance(points, width, pen.color(), verts);
    qcp::addGpuStroke(layer, verts, pen, width, clip, dpr, height);
}

// Triangle strip between two pixel-space curves of the same length.
void addRibbonFill(QCPPlottableRhiLayer& layer, const std::vector<QPointF>& upper,
                   const std::vector<QPointF>& lower, const QColor& color, const QRect& clip,
                   double dpr, int height)
{
    const std::size_t count = std::min(upper.size(), lower.size());
    if (count < 2 || color.alpha() == 0)
        return;
    const std::array<float, 4> rgba = qcp::rhi::premultipliedColor(color);
    std::vector<float>& verts = fillScratch();
    verts.clear();
    verts.reserve((count - 1) * 36);
    const auto push = [&verts, &rgba](const QPointF& p) {
        verts.push_back(static_cast<float>(p.x()));
        verts.push_back(static_cast<float>(p.y()));
        verts.insert(verts.end(), rgba.begin(), rgba.end());
    };
    for (std::size_t i = 0; i + 1 < count; ++i)
    {
        push(upper[i]);
        push(upper[i + 1]);
        push(lower[i + 1]);
        push(upper[i]);
        push(lower[i + 1]);
        push(lower[i]);
    }
    layer.addPlottable(verts, {}, clip, dpr, height);
}

// A fill polygon in the baseline form tessellateFillPolygon expects: first and last share a y.
void addBaselineFill(QCPPlottableRhiLayer& layer, const QPolygonF& polygon, const QColor& color,
                     const QRect& clip, double dpr, int height)
{
    if (color.alpha() == 0 || polygon.size() < 4)
        return;
    const QVector<float> verts = QCPLineExtruder::tessellateFillPolygon(polygon, color);
    if (!verts.isEmpty())
        layer.addPlottable(std::span<const float>(verts.constData(), verts.size()), {}, clip, dpr,
                           height);
}

void resolveInto(std::span<const QCP::Position> positions, const QCPDrawFrame& frame,
                 std::vector<QPointF>& out)
{
    out.resize(positions.size());
    for (std::size_t i = 0; i < positions.size(); ++i)
        out[i] = qcpResolvePosition(frame, positions[i]);
}

std::vector<QPointF> ringOf(std::span<const QPointF> open)
{
    std::vector<QPointF> ring(open.begin(), open.end());
    if (!ring.empty())
        ring.push_back(ring.front());
    return ring;
}

// Pixel-space error bars: the bar and its two caps, gaps between them.
void errorBarPolyline(const std::vector<QPointF>& centres, const std::vector<QPointF>& spans,
                      std::vector<QPointF>& out)
{
    const QPointF gap(std::numeric_limits<double>::quiet_NaN(),
                      std::numeric_limits<double>::quiet_NaN());
    out.clear();
    const std::size_t count = std::min(centres.size(), spans.size());
    out.reserve(count * 9);
    for (std::size_t i = 0; i < count; ++i)
    {
        const QPointF centre = centres[i];
        const QPointF span = spans[i];
        out.push_back(QPointF(centre.x(), centre.y() - span.y()));
        out.push_back(QPointF(centre.x(), centre.y() + span.y()));
        out.push_back(gap);
        out.push_back(QPointF(centre.x() - span.x(), centre.y() + span.y()));
        out.push_back(QPointF(centre.x() + span.x(), centre.y() + span.y()));
        out.push_back(gap);
        out.push_back(QPointF(centre.x() - span.x(), centre.y() - span.y()));
        out.push_back(QPointF(centre.x() + span.x(), centre.y() - span.y()));
        out.push_back(gap);
    }
}

QPolygonF ellipseRing(QPointF a, QPointF b, int segments = 96)
{
    const QRectF box = QRectF(a, b).normalized();
    const QPointF centre = box.center();
    const double rx = box.width() / 2.0;
    const double ry = box.height() / 2.0;
    QPolygonF ring;
    ring.reserve(segments + 1);
    for (int i = 0; i < segments; ++i)
    {
        const double angle = 2.0 * M_PI * i / segments;
        ring << QPointF(centre.x() + rx * std::cos(angle), centre.y() + ry * std::sin(angle));
    }
    ring << ring.first();
    return ring;
}

} // namespace

const std::vector<QPointF>& QCPResolveCache::pixels(const void* key, quint64 generation,
                                                    const QCPTransform& transform,
                                                    std::span<const QPointF> data)
{
    for (Entry& entry : mEntries)
    {
        if (entry.key != key)
            continue;
        if (entry.generation == generation && entry.transform == transform)
            return entry.pixels;
        entry.generation = generation;
        entry.transform = transform;
        entry.valid = true;
        entry.pixels.resize(data.size());
        transform.map(data, std::span<QPointF>(entry.pixels));
        return entry.pixels;
    }
    mEntries.push_back(Entry { key, generation, transform, true, {} });
    Entry& entry = mEntries.back();
    entry.pixels.resize(data.size());
    transform.map(data, std::span<QPointF>(entry.pixels));
    return entry.pixels;
}

void QCPResolveCache::clear()
{
    mEntries.clear();
}

bool qcpCanReplayOnRhi(const QCPDrawList& list)
{
    for (const qcp::QCPDrawCommand& command : list.commands)
    {
        bool replayable = true;
        std::visit(
            [&replayable](const auto& cmd) {
                using T = std::decay_t<decltype(cmd)>;
                if constexpr (std::is_same_v<T, qcp::PolygonCmd>)
                {
                    // A free-form polygon has a GPU stroke but no GPU fill: its shape is arbitrary,
                    // and the only tessellator at hand fills a baseline curve.
                    replayable = !isFillable(cmd.brush);
                }
                else if constexpr (std::is_same_v<T, qcp::PolylineDataCmd>
                                   || std::is_same_v<T, qcp::PolylinePositionsCmd>
                                   || std::is_same_v<T, qcp::FillBetweenCmd>
                                   || std::is_same_v<T, qcp::RectCmd>
                                   || std::is_same_v<T, qcp::EllipseCmd>
                                   || std::is_same_v<T, qcp::ErrorBarsCmd>
                                   || std::is_same_v<T, qcp::MarkersCmd>)
                {
                    // always replayable
                }
                else
                {
                    replayable = false; // text, path, image, free-form painter callback
                }
            },
            command);
        if (!replayable)
            return false;
    }
    return true;
}

bool qcpReplayWithRhi(const QCPDrawList& list, QCPPlottableRhiLayer& plottables,
                      QCPScatterRhiLayer& scatter, const QCPDrawFrame& frame,
                      QCPResolveCache& cache, double dpr, int outputHeight, quint64 generation)
{
    PROFILE_HERE_N("qcpReplayWithRhi");
    if (!qcpCanReplayOnRhi(list))
        return false;

    const QRect clip = frame.clip.isEmpty() ? QRect() : frame.clip.toAlignedRect();
    std::vector<QPointF>& pixels = pixelScratch();

    for (const qcp::QCPDrawCommand& command : list.commands)
    {
        std::visit(
            [&](const auto& cmd) {
                using T = std::decay_t<decltype(cmd)>;
                if constexpr (std::is_same_v<T, qcp::PolylineDataCmd>)
                {
                    const std::vector<QPointF>& resolved =
                        cache.pixels(cmd.points.get(), generation, frame.transform,
                                     std::span<const QPointF>(*cmd.points));
                    addPolyline(plottables, resolved, cmd.pen, clip, dpr, outputHeight);
                }
                else if constexpr (std::is_same_v<T, qcp::PolylinePositionsCmd>)
                {
                    resolveInto(cmd.points, frame, pixels);
                    addPolyline(plottables, pixels, cmd.pen, clip, dpr, outputHeight);
                }
                else if constexpr (std::is_same_v<T, qcp::PolygonCmd>)
                {
                    resolveInto(cmd.points, frame, pixels);
                    addPolyline(plottables, ringOf(pixels), cmd.pen, clip, dpr, outputHeight);
                }
                else if constexpr (std::is_same_v<T, qcp::FillBetweenCmd>)
                {
                    const std::vector<QPointF>& upper =
                        cache.pixels(cmd.upper.get(), generation, frame.transform,
                                     std::span<const QPointF>(*cmd.upper));
                    const std::vector<QPointF>& lower =
                        cache.pixels(cmd.lower.get(), generation, frame.transform,
                                     std::span<const QPointF>(*cmd.lower));
                    if (isFillable(cmd.brush))
                        addRibbonFill(plottables, upper, lower, cmd.brush.color(), clip, dpr,
                                      outputHeight);
                }
                else if constexpr (std::is_same_v<T, qcp::RectCmd>)
                {
                    const QRectF box =
                        QRectF(qcpResolvePosition(frame, cmd.a), qcpResolvePosition(frame, cmd.b))
                            .normalized();
                    pixels.assign({ box.topLeft(), box.topRight(), box.bottomRight(),
                                    box.bottomLeft(), box.topLeft() });
                    addPolyline(plottables, pixels, cmd.pen, clip, dpr, outputHeight);
                    if (isFillable(cmd.brush))
                    {
                        QPolygonF baseline;
                        baseline << QPointF(box.left(), box.bottom())
                                 << QPointF(box.left(), box.top())
                                 << QPointF(box.right(), box.top())
                                 << QPointF(box.right(), box.bottom());
                        addBaselineFill(plottables, baseline, cmd.brush.color(), clip, dpr,
                                        outputHeight);
                    }
                }
                else if constexpr (std::is_same_v<T, qcp::EllipseCmd>)
                {
                    const QPolygonF ring = ellipseRing(qcpResolvePosition(frame, cmd.a),
                                                       qcpResolvePosition(frame, cmd.b));
                    pixels.assign(std::size_t(ring.size()), QPointF());
                    for (int i = 0; i < ring.size(); ++i)
                        pixels[std::size_t(i)] = ring.at(i);
                    addPolyline(plottables, pixels, cmd.pen, clip, dpr, outputHeight);
                    if (isFillable(cmd.brush))
                    {
                        const QRectF box = ring.boundingRect();
                        QPolygonF upper;
                        QPolygonF lower;
                        for (int i = 0; i < ring.size(); ++i)
                        {
                            const QPointF p = ring.at(i);
                            if (p.y() <= box.center().y())
                                upper << p;
                            else
                                lower << p;
                        }
                        upper << QPointF(box.right(), box.center().y());
                        lower << QPointF(box.left(), box.center().y());
                        addBaselineFill(plottables, upper, cmd.brush.color(), clip, dpr,
                                        outputHeight);
                        addBaselineFill(plottables, lower, cmd.brush.color(), clip, dpr,
                                        outputHeight);
                    }
                }
                else if constexpr (std::is_same_v<T, qcp::ErrorBarsCmd>)
                {
                    const std::vector<QPointF>& centres =
                        cache.pixels(cmd.centres.get(), generation, frame.transform,
                                     std::span<const QPointF>(*cmd.centres));
                    const std::vector<QPointF>& spans =
                        cache.pixels(cmd.spans.get(), generation + 1, frame.transform,
                                     std::span<const QPointF>(*cmd.spans));
                    errorBarPolyline(centres, spans, pixels);
                    addPolyline(plottables, pixels, cmd.pen, clip, dpr, outputHeight);
                }
                else if constexpr (std::is_same_v<T, qcp::MarkersCmd>)
                {
                    const std::vector<QPointF>& resolved =
                        cache.pixels(cmd.points.get(), generation, frame.transform,
                                     std::span<const QPointF>(*cmd.points));
                    std::vector<float>& xy = markerScratch();
                    xy.resize(resolved.size() * 2);
                    for (std::size_t i = 0; i < resolved.size(); ++i)
                    {
                        xy[2 * i] = static_cast<float>(resolved[i].x());
                        xy[2 * i + 1] = static_cast<float>(resolved[i].y());
                    }
                    scatter.addScatter(xy, cmd.style, clip, dpr, outputHeight);
                }
            },
            command);
    }
    return true;
}
