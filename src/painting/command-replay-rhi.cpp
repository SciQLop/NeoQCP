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

#include <QDebug>

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

// Every fill the replay tessellates: the two builders below are the only places that make one.
quint64& fillBuilds()
{
    static quint64 count = 0;
    return count;
}

//! True when every position the command was recorded from is QCP::Data.
//!
//! Only then do the resolved pixels move by one delta when the transform pans: a pixel or ratio
//! position stays put, and an anchor follows its item. Kept triangles for anything else would be
//! shifted to the wrong place.
bool isDataOnlyCommand(const qcp::QCPDrawCommand& command)
{
    const auto allData = [](const std::vector<QCP::Position>& positions)
    {
        return std::all_of(positions.begin(), positions.end(), [](const QCP::Position& p)
                           { return std::holds_alternative<QCP::Data>(p); });
    };
    return std::visit(
        [&allData](const auto& cmd)
        {
            using T = std::decay_t<decltype(cmd)>;
            if constexpr (std::is_same_v<T, qcp::PolylineDataCmd>
                          || std::is_same_v<T, qcp::FillBetweenCmd>
                          || std::is_same_v<T, qcp::MarkersCmd>
                          || std::is_same_v<T, qcp::ErrorBarsCmd>)
                return true;
            else if constexpr (std::is_same_v<T, qcp::PolylinePositionsCmd>
                               || std::is_same_v<T, qcp::PolygonCmd>)
                return allData(cmd.points);
            else if constexpr (std::is_same_v<T, qcp::RectCmd>
                               || std::is_same_v<T, qcp::EllipseCmd>)
                return std::holds_alternative<QCP::Data>(cmd.a)
                    && std::holds_alternative<QCP::Data>(cmd.b);
            else
                return false;
        },
        command);
}

//! Strokes \a points on the layer, and keeps the triangles when the replay may re-emit them.
void addPolyline(QCPPlottableRhiLayer& layer, std::span<const QPointF> points, const QPen& pen,
                 const QRect& clip, double dpr, int height, float alpha, bool movesWithData,
                 QCPRecordingCache* cache, bool closed = false)
{
    if (!qcp::canStrokeOnGpu(pen) || points.size() < 2)
        return;
    const float width = qcp::extrusionPenWidth(pen, dpr);
    std::vector<float>& verts = strokeScratch();
    qcp::extrudeStroke(points, pen, width, verts, closed);
    const QPointF none;
    if (!qcp::addGpuStroke(layer, verts, pen, width, clip, dpr, height, none, alpha))
        return; // no dashed pipeline: nothing was drawn, so nothing is kept
    if (!cache)
        return;
    if (qcp::isDashed(pen))
    {
        const qcp::DashPattern pattern = qcp::dashPatternOf(pen, width);
        cache->keepDashed(
            verts, std::span<const float>(pattern.lengths.data(), std::size_t(pattern.count)),
            pattern.offset, movesWithData);
    }
    else
        cache->keepSolid(verts, 0, movesWithData);
}

// Triangle strip between two pixel-space curves of the same length.
void addRibbonFill(QCPPlottableRhiLayer& layer, std::span<const QPointF> upper,
                   std::span<const QPointF> lower, const QColor& color, const QRect& clip,
                   double dpr, int height, float alpha, bool movesWithData,
                   QCPRecordingCache* cache)
{
    const std::size_t count = std::min(upper.size(), lower.size());
    if (count < 2 || color.alpha() == 0)
        return;
    ++fillBuilds();
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
    layer.addPlottable(verts, { }, clip, dpr, height, 0, 0, alpha);
    if (cache)
        cache->keepSolid(verts, int(verts.size()) / 6, movesWithData);
}

// A triangle fan from \a centre over the (open) \a ring: correct for any convex outline, which is
// what a rect and an ellipse are, and cheaper than clipping a baseline tessellator to its shape.
void fanFill(QCPPlottableRhiLayer& layer, const QPointF& centre, std::span<const QPointF> ring,
             const QColor& color, const QRect& clip, double dpr, int height, float alpha,
             bool movesWithData, QCPRecordingCache* cache)
{
    const std::vector<float> verts = qcpFanFillVertices(centre, ring, color);
    if (verts.empty())
        return;
    ++fillBuilds();
    layer.addPlottable(verts, { }, clip, dpr, height, 0, 0, alpha);
    if (cache)
        cache->keepSolid(verts, int(verts.size()) / 6, movesWithData);
}

// A fill or stroke the GPU cannot express (a texture, a gradient, a hatch, a free-form outline) is
// painted into an image and drawn as a quad at the command's place in the layer. The layer's image
// slots are the cap: past them the painter pass takes the command, under this layer's GPU content.
void addRasterCommand(const QCPDrawList& list, QCPPlottableRhiLayer& layer,
                      const qcp::QCPDrawCommand& command, const QCPDrawFrame& frame,
                      qcp::QCPRasterFallback& fallback, const QRect& clip, double dpr,
                      const QSize& viewport, quint64 generation, QCPRecordingCache* cache)
{
    if (!qcp::needsRaster(command))
        return;
    // An image is not vertex geometry: the kept runs cannot place it on a pan frame.
    if (cache)
        cache->invalidate();
    const quint64 listId = list.id;
    const quint64 index = qcpCommandIndex(list, command);
    if (!layer.canAddImage())
    {
        fallback.painterHandles(listId, index);
        if (layer.warnRasterCapOnce())
            qWarning() << "NeoQCP: more than" << QCPPlottableRhiLayer::kMaxImages
                       << "rasterised styles in one layer, the rest is painted by QPainter,"
                          " under this layer's GPU content";
        return;
    }
    const bool withStroke = qcp::strokeNeedsRaster(command);
    QRectF bounds = qcp::fallbackPixelBounds(command, frame, dpr, withStroke);
    bounds = bounds.intersected(QRectF(QPointF(0, 0), QSizeF(viewport) / dpr));
    if (!clip.isEmpty())
        bounds = bounds.intersected(QRectF(clip));
    if (bounds.width() < 1.0 || bounds.height() < 1.0)
    {
        fallback.painterHandles(listId, index);
        return;
    }
    const QImage& image = fallback.image(listId, index, command, frame, bounds, dpr, generation);
    const QRectF device(bounds.left() * dpr, bounds.top() * dpr, image.width(), image.height());
    if (!layer.addImage(image, device, clip, dpr, viewport.height()))
        fallback.painterHandles(listId, index);
}

// One markers command on the scatter layer. Its own layer is rebuilt every frame, so the points are
// mapped again from the resolve cache even when the plottable geometry is re-emitted.
void addMarker(QCPScatterRhiLayer& scatter, const qcp::MarkersCmd& markers, QCPResolveCache& cache,
               const QCPDrawFrame& frame, quint64 generation, const QRect& clip, double dpr,
               int height)
{
    const std::span<const QPointF> resolved
        = cache.pixels(markers.points, generation, frame.transform);
    std::vector<float>& xy = markerScratch();
    xy.resize(resolved.size() * 2);
    for (std::size_t i = 0; i < resolved.size(); ++i)
    {
        xy[2 * i] = static_cast<float>(resolved[i].x());
        xy[2 * i + 1] = static_cast<float>(resolved[i].y());
    }
    scatter.addScatter(xy, markers.style, clip, dpr, height);
}

void addMarkers(const QCPDrawList& list, QCPScatterRhiLayer& scatter, QCPResolveCache& cache,
                const QCPDrawFrame& frame, quint64 generation, const QRect& clip, double dpr,
                int height)
{
    for (const qcp::QCPDrawCommand& command : list.commands)
        if (const auto* markers = std::get_if<qcp::MarkersCmd>(&command))
            addMarker(scatter, *markers, cache, frame, generation, clip, dpr, height);
}

//! Hands a kept recording back to the layer: one submission per run, shifted by the pan delta when
//! the run moves with the data, and by nothing otherwise.
void replayKept(const QCPRecordingCache& kept, QCPPlottableRhiLayer& layer, const QRect& clip,
                double dpr, int height, const QPointF& delta, float alpha)
{
    const std::span<const float> solid = kept.solidVertices();
    for (const QCPRecordingCache::SolidRun& run : kept.solidRuns())
    {
        const QPointF offset = run.movesWithData ? delta : QPointF();
        const std::span<const float> vertices
            = solid.subspan(std::size_t(run.first) * 6, std::size_t(run.vertexCount) * 6);
        const auto fill = std::size_t(run.fillVertexCount) * 6;
        layer.addPlottable(vertices.first(fill), vertices.subspan(fill), clip, dpr, height,
                           static_cast<float>(offset.x()), static_cast<float>(offset.y()), alpha);
    }
    const std::span<const float> dashed = kept.dashedVertices();
    for (const QCPRecordingCache::DashedRun& run : kept.dashedRuns())
    {
        const QPointF offset = run.movesWithData ? delta : QPointF();
        const std::span<const float> vertices
            = dashed.subspan(std::size_t(run.first) * 7, std::size_t(run.vertexCount) * 7);
        layer.addDashedPlottable(vertices,
                                 std::span<const float>(run.pattern, std::size_t(run.patternCount)),
                                 run.dashOffset, clip, dpr, height, static_cast<float>(offset.x()),
                                 static_cast<float>(offset.y()), alpha);
    }
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
            ++mRemaps;
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
    ++mRemaps;
    return std::span<const QPointF>(entry.pixels);
}

void QCPResolveCache::clear()
{
    mEntries.clear();
}

bool QCPRecordingCache::reuse(const QCPDrawList& list, const QCPTransform& transform, double dpr,
                              int outputHeight, const QRect& clip, QPointF* delta)
{
    const bool same = mValid && list.id == mList && qFuzzyCompare(dpr, mDpr)
        && outputHeight == mHeight && clip == mClip && transform.isTranslationOf(mTransform, delta);
    if (!same)
        mValid = false;
    return same;
}

void QCPRecordingCache::beginBuild(const QCPDrawList& list, const QCPTransform& transform,
                                   double dpr, int outputHeight, const QRect& clip)
{
    mSolid.clear();
    mSolidRuns.clear();
    mDashed.clear();
    mDashedRuns.clear();
    mList = list.id;
    mTransform = transform;
    mDpr = dpr;
    mHeight = outputHeight;
    mClip = clip;
    mValid = false;
    mSpoiled = false;
}

void QCPRecordingCache::keepSolid(std::span<const float> vertices, int fillVertexCount,
                                  bool movesWithData)
{
    if (vertices.empty())
        return;
    const auto first = int(mSolid.size() / 6);
    mSolid.insert(mSolid.end(), vertices.begin(), vertices.end());
    // A run only ends where its pixels would: same transform sensitivity. The fill prefix of the
    // run stays the first submission's, since the layers draw a run as one range.
    if (!mSolidRuns.empty() && mSolidRuns.back().movesWithData == movesWithData
        && mSolidRuns.back().first + mSolidRuns.back().vertexCount == first)
    {
        mSolidRuns.back().vertexCount += int(vertices.size()) / 6;
        return;
    }
    SolidRun run;
    run.first = first;
    run.vertexCount = int(vertices.size()) / 6;
    run.fillVertexCount = fillVertexCount;
    run.movesWithData = movesWithData;
    mSolidRuns.push_back(run);
}

void QCPRecordingCache::keepDashed(std::span<const float> vertices, std::span<const float> pattern,
                                   float dashOffset, bool movesWithData)
{
    if (vertices.empty() || pattern.empty())
        return;
    const auto first = int(mDashed.size() / 7);
    mDashed.insert(mDashed.end(), vertices.begin(), vertices.end());
    // Same pattern and phase: the dashes continue across the commands, so one run draws them.
    if (!mDashedRuns.empty() && mDashedRuns.back().movesWithData == movesWithData
        && mDashedRuns.back().first + mDashedRuns.back().vertexCount == first
        && qFuzzyCompare(mDashedRuns.back().dashOffset, dashOffset)
        && mDashedRuns.back().patternCount == int(std::min<std::size_t>(pattern.size(), 8))
        && std::equal(pattern.begin(), pattern.end(), mDashedRuns.back().pattern))
    {
        mDashedRuns.back().vertexCount += int(vertices.size()) / 7;
        return;
    }
    DashedRun run;
    run.first = first;
    run.vertexCount = int(vertices.size()) / 7;
    run.movesWithData = movesWithData;
    run.dashOffset = dashOffset;
    run.patternCount = int(std::min<std::size_t>(pattern.size(), 8));
    std::copy_n(pattern.begin(), run.patternCount, run.pattern);
    mDashedRuns.push_back(run);
}

void QCPRecordingCache::commit()
{
    mValid = !mSpoiled;
    if (mSpoiled)
    {
        mSolid.clear();
        mSolidRuns.clear();
        mDashed.clear();
        mDashedRuns.clear();
    }
    mSpoiled = false;
}

void QCPRecordingCache::invalidate()
{
    mSpoiled = true;
}

void QCPRecordingCache::clear()
{
    mSolid.clear();
    mSolidRuns.clear();
    mDashed.clear();
    mDashedRuns.clear();
    mValid = false;
    mSpoiled = false;
}

void qcpReplayWithRhi(const QCPDrawList& list, QCPPlottableRhiLayer& plottables,
                      QCPScatterRhiLayer& scatter, const QCPDrawFrame& frame,
                      QCPResolveCache& cache, qcp::QCPRasterFallback& fallback, double dpr,
                      const QSize& outputSize, quint64 generation, QCPRecordingCache* geometry,
                      float alpha)
{
    PROFILE_HERE_N("qcpReplayWithRhi");
    const int outputHeight = outputSize.height();
    const QRect clip = frame.clip.isEmpty() ? QRect() : frame.clip.toAlignedRect();

    QPointF delta;
    if (geometry && geometry->reuse(list, frame.transform, dpr, outputHeight, clip, &delta))
    {
        // A pan (or a frame that changed nothing): the recording is already built, it only needs
        // its data-space runs shifted. Markers live in another layer, which is rebuilt anyway.
        replayKept(*geometry, plottables, clip, dpr, outputHeight, delta, alpha);
        addMarkers(list, scatter, cache, frame, generation, clip, dpr, outputHeight);
        return;
    }

    cache.beginReplay();
    fallback.beginReplay();
    if (geometry)
        geometry->beginBuild(list, frame.transform, dpr, outputHeight, clip);
    std::vector<QPointF>& pixels = pixelScratch();

    for (const qcp::QCPDrawCommand& command : list.commands)
    {
        // Both answers are the same in every branch: ask once per command, not once per path.
        const bool strokeRaster = qcp::strokeNeedsRaster(command);
        const bool dataOnly = isDataOnlyCommand(command);
        const auto raster = [&]
        {
            addRasterCommand(list, plottables, command, frame, fallback, clip, dpr, outputSize,
                             generation, geometry);
        };
        const auto stroke
            = [&](std::span<const QPointF> points, const QPen& pen, bool closed = false)
        {
            if (!strokeRaster)
                addPolyline(plottables, points, pen, clip, dpr, outputHeight, alpha, dataOnly,
                            geometry, closed);
        };
        std::visit(
            [&](const auto& cmd)
            {
                using T = std::decay_t<decltype(cmd)>;
                if constexpr (std::is_same_v<T, qcp::PolylineDataCmd>)
                {
                    const std::span<const QPointF> resolved
                        = cache.pixels(cmd.points, generation, frame.transform);
                    raster();
                    stroke(resolved, cmd.pen);
                }
                else if constexpr (std::is_same_v<T, qcp::PolylinePositionsCmd>)
                {
                    qcpResolveInto(cmd.points, frame, pixels);
                    raster();
                    stroke(pixels, cmd.pen);
                }
                else if constexpr (std::is_same_v<T, qcp::PolygonCmd>)
                {
                    // A free-form outline has no fan centre: its fill is a raster image, submitted
                    // before the stroke exactly as the painter draws it.
                    raster();
                    qcpResolveInto(cmd.points, frame, pixels);
                    stroke(qcpRingOf(pixels), cmd.pen, true);
                }
                else if constexpr (std::is_same_v<T, qcp::FillBetweenCmd>)
                {
                    const std::span<const QPointF> upper
                        = cache.pixels(cmd.upper, generation, frame.transform);
                    const std::span<const QPointF> lower
                        = cache.pixels(cmd.lower, generation, frame.transform);
                    // Pairing upper[i] with lower[i] is only a ribbon when the two series line up;
                    // otherwise the fill is an image.
                    if (qcp::gpuCanFill(cmd.brush) && upper.size() == lower.size())
                        addRibbonFill(plottables, upper, lower, cmd.brush.color(), clip, dpr,
                                      outputHeight, alpha, true, geometry);
                    else
                        raster();
                }
                else if constexpr (std::is_same_v<T, qcp::RectCmd>)
                {
                    const QRectF box
                        = QRectF(qcpResolvePosition(frame, cmd.a), qcpResolvePosition(frame, cmd.b))
                              .normalized();
                    // The fill is submitted first: the stroke's inner half must stay visible.
                    if (qcp::gpuCanFill(cmd.brush) && !qcp::needsRaster(command))
                    {
                        const std::array<QPointF, 4> corners { box.topLeft(), box.topRight(),
                                                               box.bottomRight(),
                                                               box.bottomLeft() };
                        fanFill(plottables, box.center(), corners, cmd.brush.color(), clip, dpr,
                                outputHeight, alpha, dataOnly, geometry);
                    }
                    raster();
                    pixels.assign({ box.topLeft(), box.topRight(), box.bottomRight(),
                                    box.bottomLeft(), box.topLeft() });
                    stroke(pixels, cmd.pen, true);
                }
                else if constexpr (std::is_same_v<T, qcp::EllipseCmd>)
                {
                    const QPointF a = qcpResolvePosition(frame, cmd.a);
                    const QPointF b = qcpResolvePosition(frame, cmd.b);
                    const QRectF box = QRectF(a, b).normalized();
                    const std::vector<QPointF> ring = qcpEllipseRing(box);
                    if (qcp::gpuCanFill(cmd.brush) && !qcp::needsRaster(command))
                        fanFill(plottables, box.center(), ring, cmd.brush.color(), clip, dpr,
                                outputHeight, alpha, dataOnly, geometry);
                    raster();
                    stroke(qcpRingOf(ring), cmd.pen, true);
                }
                else if constexpr (std::is_same_v<T, qcp::ErrorBarsCmd>)
                {
                    // The spans are half extents, not positions: the corners have to be built in
                    // data space and mapped, or a pan offset and a log axis land in the wrong
                    // place.
                    qcpErrorBarPixels(frame, std::span<const QPointF>(*cmd.centres),
                                      std::span<const QPointF>(*cmd.spans), pixels);
                    stroke(pixels, cmd.pen);
                }
                else if constexpr (std::is_same_v<T, qcp::MarkersCmd>)
                {
                    addMarker(scatter, cmd, cache, frame, generation, clip, dpr, outputHeight);
                }
            },
            command);
    }
    if (geometry)
        geometry->commit();
}

quint64 qcpFillBuildCount()
{
    return fillBuilds();
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
