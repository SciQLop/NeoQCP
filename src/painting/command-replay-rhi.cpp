#include "command-replay-rhi.h"

#include "../plottables/plottable-draw-utils.h"
#include "Profiling.hpp"
#include "line-extruder.h"
#include "plottable-rhi-layer.h"
#include "raster-fallback.h"
#include "rhi-utils.h"
#include "scatter-rhi-layer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace
{

// Per-replay buffers: a frame allocates nothing once they have grown.
std::vector<QPointF>& dataScratch()
{
    static thread_local std::vector<QPointF> buffer;
    return buffer;
}

std::vector<QPointF>& cornerScratch()
{
    static thread_local std::vector<QPointF> buffer;
    return buffer;
}

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

void addPolyline(QCPPlottableRhiLayer& layer, std::span<const QPointF> points, const QPen& pen,
                 const QRect& clip, double dpr, int height, bool closed = false)
{
    if (!qcp::canStrokeOnGpu(pen) || points.size() < 2)
        return;
    const float width = qcp::extrusionPenWidth(pen, dpr);
    std::vector<float>& verts = strokeScratch();
    qcp::extrudeStroke(points, pen, width, verts, closed);
    qcp::addGpuStroke(layer, verts, pen, width, clip, dpr, height);
}

// Triangle strip between two pixel-space curves of the same length.
void addRibbonFill(QCPPlottableRhiLayer& layer, std::span<const QPointF> upper,
                   std::span<const QPointF> lower, const QColor& color, const QRect& clip,
                   double dpr, int height)
{
    const std::size_t count = std::min(upper.size(), lower.size());
    if (count < 2 || color.alpha() == 0)
        return;
    const std::array<float, 4> rgba = qcp::rhi::premultipliedColor(color);
    std::vector<float>& verts = fillScratch();
    verts.clear();
    verts.reserve((count - 1) * 36);
    const auto push = [&verts, &rgba](const QPointF& p)
    {
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
    layer.addPlottable(verts, { }, clip, dpr, height);
}

// A triangle fan from \a centre over the (open) \a ring: correct for any convex outline, which is
// what a rect and an ellipse are, and cheaper than clipping a baseline tessellator to its shape.
void fanFill(QCPPlottableRhiLayer& layer, const QPointF& centre, std::span<const QPointF> ring,
             const QColor& color, const QRect& clip, double dpr, int height)
{
    const std::vector<float> verts = qcpFanFillVertices(centre, ring, color);
    if (!verts.empty())
        layer.addPlottable(verts, { }, clip, dpr, height);
}

// A fill the GPU cannot express (a texture, a gradient, a hatch, a free-form outline) is painted
// into an image and drawn as a quad at the command's place in the layer.
void addRasterFill(QCPPlottableRhiLayer& layer, const qcp::QCPDrawCommand& command,
                   const QCPDrawFrame& frame, const QRect& clip, double dpr, int height)
{
    if (!qcp::fillNeedsRaster(command))
        return;
    QRectF bounds = qcp::fallbackPixelBounds(command, frame, dpr, false);
    if (!clip.isEmpty())
        bounds = bounds.intersected(QRectF(clip));
    if (bounds.width() < 1.0 || bounds.height() < 1.0)
        return;
    const QImage image = qcp::rasterizeFallback(command, bounds, frame, dpr, false);
    if (image.isNull())
        return;
    const QRectF device(bounds.left() * dpr, bounds.top() * dpr, image.width(), image.height());
    layer.addImage(image, device, clip, dpr, height);
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

// Pixel-space error bars from the four mapped corners of each bar: bar plus two caps, with gaps.
void errorBarPolyline(const std::vector<QPointF>& corners, std::vector<QPointF>& out)
{
    const QPointF gap(std::numeric_limits<double>::quiet_NaN(),
                      std::numeric_limits<double>::quiet_NaN());
    out.clear();
    const std::size_t count = corners.size() / 4;
    out.reserve(count * 9);
    for (std::size_t i = 0; i < count; ++i)
    {
        const QPointF& above = corners[4 * i + 0];
        const QPointF& below = corners[4 * i + 1];
        const QPointF& left = corners[4 * i + 2];
        const QPointF& right = corners[4 * i + 3];
        out.push_back(above);
        out.push_back(below);
        out.push_back(gap);
        out.push_back(QPointF(left.x(), above.y()));
        out.push_back(QPointF(right.x(), above.y()));
        out.push_back(gap);
        out.push_back(QPointF(left.x(), below.y()));
        out.push_back(QPointF(right.x(), below.y()));
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

void QCPResolveCache::beginReplay()
{
    // Sweep what the previous replay did not touch: a recording that is gone or no longer drawn.
    for (auto it = mEntries.begin(); it != mEntries.end();)
    {
        if (!it->used || it->recording.expired())
            it = mEntries.erase(it);
        else
        {
            it->used = false;
            ++it;
        }
    }
}

std::span<const QPointF>
QCPResolveCache::pixels(const std::shared_ptr<const std::vector<QPointF>>& recording,
                        quint64 generation, const QCPTransform& transform)
{
    for (Entry& entry : mEntries)
    {
        if (entry.recording.lock() != recording)
            continue;
        entry.used = true;
        if (entry.generation != generation || !(entry.transform == transform))
        {
            const std::span<const QPointF> data(*recording);
            entry.pixels.resize(data.size());
            transform.map(data, std::span<QPointF>(entry.pixels));
            entry.generation = generation;
            entry.transform = transform;
        }
        return std::span<const QPointF>(entry.pixels);
    }

    Entry& entry = mEntries.emplace_back();
    entry.recording = recording;
    entry.generation = generation;
    entry.transform = transform;
    entry.used = true;
    const std::span<const QPointF> data(*recording);
    entry.pixels.resize(data.size());
    transform.map(data, std::span<QPointF>(entry.pixels));
    return std::span<const QPointF>(entry.pixels);
}

void QCPResolveCache::clear()
{
    mEntries.clear();
}

void qcpReplayWithRhi(const QCPDrawList& list, QCPPlottableRhiLayer& plottables,
                      QCPScatterRhiLayer& scatter, const QCPDrawFrame& frame,
                      QCPResolveCache& cache, double dpr, int outputHeight, quint64 generation)
{
    PROFILE_HERE_N("qcpReplayWithRhi");
    cache.beginReplay();
    const QRect clip = frame.clip.isEmpty() ? QRect() : frame.clip.toAlignedRect();
    std::vector<QPointF>& pixels = pixelScratch();

    for (const qcp::QCPDrawCommand& command : list.commands)
    {
        std::visit(
            [&](const auto& cmd)
            {
                using T = std::decay_t<decltype(cmd)>;
                if constexpr (std::is_same_v<T, qcp::PolylineDataCmd>)
                {
                    const std::span<const QPointF> resolved
                        = cache.pixels(cmd.points, generation, frame.transform);
                    addPolyline(plottables, resolved, cmd.pen, clip, dpr, outputHeight);
                }
                else if constexpr (std::is_same_v<T, qcp::PolylinePositionsCmd>)
                {
                    resolveInto(cmd.points, frame, pixels);
                    addPolyline(plottables, pixels, cmd.pen, clip, dpr, outputHeight);
                }
                else if constexpr (std::is_same_v<T, qcp::PolygonCmd>)
                {
                    // A free-form outline has no fan centre: its fill is a raster image, submitted
                    // before the stroke exactly as the painter draws it.
                    addRasterFill(plottables, command, frame, clip, dpr, outputHeight);
                    resolveInto(cmd.points, frame, pixels);
                    addPolyline(plottables, ringOf(pixels), cmd.pen, clip, dpr, outputHeight, true);
                }
                else if constexpr (std::is_same_v<T, qcp::FillBetweenCmd>)
                {
                    const std::span<const QPointF> upper
                        = cache.pixels(cmd.upper, generation, frame.transform);
                    const std::span<const QPointF> lower
                        = cache.pixels(cmd.lower, generation, frame.transform);
                    // Pairing upper[i] with lower[i] is only a ribbon when the two series line up;
                    // otherwise the fill is an image.
                    if (qcpGpuFillsBrush(cmd.brush) && upper.size() == lower.size())
                        addRibbonFill(plottables, upper, lower, cmd.brush.color(), clip, dpr,
                                      outputHeight);
                    else
                        addRasterFill(plottables, command, frame, clip, dpr, outputHeight);
                }
                else if constexpr (std::is_same_v<T, qcp::RectCmd>)
                {
                    const QRectF box
                        = QRectF(qcpResolvePosition(frame, cmd.a), qcpResolvePosition(frame, cmd.b))
                              .normalized();
                    // The fill is submitted first: the stroke's inner half must stay visible.
                    if (qcpGpuFillsBrush(cmd.brush))
                    {
                        const std::array<QPointF, 4> corners { box.topLeft(), box.topRight(),
                                                               box.bottomRight(),
                                                               box.bottomLeft() };
                        fanFill(plottables, box.center(), corners, cmd.brush.color(), clip, dpr,
                                outputHeight);
                    }
                    else
                        addRasterFill(plottables, command, frame, clip, dpr, outputHeight);
                    pixels.assign({ box.topLeft(), box.topRight(), box.bottomRight(),
                                    box.bottomLeft(), box.topLeft() });
                    addPolyline(plottables, pixels, cmd.pen, clip, dpr, outputHeight, true);
                }
                else if constexpr (std::is_same_v<T, qcp::EllipseCmd>)
                {
                    const QPointF a = qcpResolvePosition(frame, cmd.a);
                    const QPointF b = qcpResolvePosition(frame, cmd.b);
                    const QPointF centre = QRectF(a, b).normalized().center();
                    const QPolygonF ring = ellipseRing(a, b);
                    pixels.assign(std::size_t(ring.size()), QPointF());
                    for (int i = 0; i < ring.size(); ++i)
                        pixels[std::size_t(i)] = ring.at(i);
                    if (qcpGpuFillsBrush(cmd.brush))
                        fanFill(plottables, centre,
                                std::span<const QPointF>(pixels.data(), pixels.size() - 1),
                                cmd.brush.color(), clip, dpr, outputHeight);
                    else
                        addRasterFill(plottables, command, frame, clip, dpr, outputHeight);
                    addPolyline(plottables, pixels, cmd.pen, clip, dpr, outputHeight, true);
                }
                else if constexpr (std::is_same_v<T, qcp::ErrorBarsCmd>)
                {
                    // The spans are half extents, not positions: the corners have to be built in
                    // data space and mapped, or a pan offset and a log axis land in the wrong
                    // place.
                    qcpErrorBarPixels(frame, std::span<const QPointF>(*cmd.centres),
                                      std::span<const QPointF>(*cmd.spans), pixels);
                    addPolyline(plottables, pixels, cmd.pen, clip, dpr, outputHeight);
                }
                else if constexpr (std::is_same_v<T, qcp::MarkersCmd>)
                {
                    const std::span<const QPointF> resolved
                        = cache.pixels(cmd.points, generation, frame.transform);
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
}

std::vector<float> qcpFanFillVertices(const QPointF& centre, std::span<const QPointF> ring,
                                      const QColor& color)
{
    std::vector<float> verts;
    if (color.alpha() == 0 || ring.size() < 3)
        return verts;
    const std::array<float, 4> rgba = qcp::rhi::premultipliedColor(color);
    verts.reserve(ring.size() * 18);
    const auto push = [&verts, &rgba](const QPointF& p)
    {
        verts.push_back(static_cast<float>(p.x()));
        verts.push_back(static_cast<float>(p.y()));
        verts.insert(verts.end(), rgba.begin(), rgba.end());
    };
    for (std::size_t i = 0; i < ring.size(); ++i)
    {
        push(centre);
        push(ring[i]);
        push(ring[(i + 1) % ring.size()]);
    }
    return verts;
}

void qcpErrorBarPixels(const QCPDrawFrame& frame, std::span<const QPointF> centres,
                       std::span<const QPointF> spans, std::vector<QPointF>& out)
{
    std::vector<QPointF>& data = dataScratch();
    qcpErrorBarCorners(centres, spans, data);
    // Not pixelScratch(): the caller may hand its own pixel buffer as \a out.
    std::vector<QPointF>& corners = cornerScratch();
    corners.resize(data.size());
    frame.transform.map(std::span<const QPointF>(data), std::span<QPointF>(corners));
    errorBarPolyline(corners, out);
}
