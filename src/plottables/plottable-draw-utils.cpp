#include "plottable-draw-utils.h"

#include "../core.h"
#include "../painting/line-extruder.h"
#include "../painting/painter.h"
#include "../painting/plottable-rhi-layer.h"

#include <cmath>
#include <span>

namespace {

void drawPolylineSplitNaN(QCPPainter* painter, const QVector<QPointF>& pts)
{
    int segStart = 0;
    for (int i = 0; i < pts.size(); ++i)
    {
        if (std::isnan(pts[i].x()) || std::isnan(pts[i].y()))
        {
            int segLen = i - segStart;
            if (segLen >= 2)
                painter->drawPolyline(pts.constData() + segStart, segLen);
            segStart = i + 1;
        }
    }
    int segLen = pts.size() - segStart;
    if (segLen >= 2)
        painter->drawPolyline(pts.constData() + segStart, segLen);
}

} // anonymous namespace

namespace qcp {

void drawPolylineWithGpuFallback(QCPPainter* painter,
                                  QCustomPlot* parentPlot,
                                  QCPLayer* layer,
                                  const QVector<QPointF>& pts,
                                  const QPen& pen,
                                  const QPointF& gpuOffset,
                                  const QRect& clipRect)
{
    if (auto* rhi = parentPlot ? parentPlot->rhi() : nullptr;
        rhi && !painter->modes().testFlag(QCPPainter::pmVectorized)
            && !painter->modes().testFlag(QCPPainter::pmNoCaching)
            && pen.style() == Qt::SolidLine)
    {
        if (auto* prl = parentPlot->plottableRhiLayer(layer))
        {
            const double dpr = parentPlot->bufferDevicePixelRatio();
            const float penWidth = (pen.isCosmetic() || qFuzzyIsNull(pen.widthF()))
                ? static_cast<float>(1.0 / dpr)
                : qMax(1.0f, static_cast<float>(pen.widthF()));
            auto strokeVerts = QCPLineExtruder::extrudePolyline(pts, penWidth, pen.color());
            if (!strokeVerts.isEmpty())
            {
                const QSize outputSize = parentPlot->rhiOutputSize();
                prl->addPlottable({}, strokeVerts, clipRect, dpr,
                                   outputSize.height(),
                                   static_cast<float>(gpuOffset.x()),
                                   static_cast<float>(gpuOffset.y()),
                                   static_cast<float>(painter->opacity()));
                return;
            }
        }
    }
    // Software fallback — split on NaN gap markers before drawing.
    // QPainter::drawPolyline does not handle NaN as line breaks.
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    if (!gpuOffset.isNull())
        painter->translate(gpuOffset);
    drawPolylineSplitNaN(painter, pts);
    if (!gpuOffset.isNull())
        painter->translate(-gpuOffset);
}

void drawPolylineCached(QCPPainter* painter,
                         QCustomPlot* parentPlot,
                         QCPLayer* layer,
                         const QVector<QPointF>& pts,
                         const QPen& pen,
                         const QPointF& gpuOffset,
                         const QRect& clipRect,
                         bool freshLines,
                         ExtrusionCache& cache)
{
    if (auto* rhi = parentPlot ? parentPlot->rhi() : nullptr;
        rhi && !painter->modes().testFlag(QCPPainter::pmVectorized)
            && !painter->modes().testFlag(QCPPainter::pmNoCaching)
            && pen.style() == Qt::SolidLine)
    {
        if (auto* prl = parentPlot->plottableRhiLayer(layer))
        {
            const double dpr = parentPlot->bufferDevicePixelRatio();
            const float penWidth = (pen.isCosmetic() || qFuzzyIsNull(pen.widthF()))
                ? static_cast<float>(1.0 / dpr)
                : qMax(1.0f, static_cast<float>(pen.widthF()));

            if (freshLines || cache.isEmpty()
                || cache.penWidth != penWidth || cache.penColor != pen.color().rgba())
            {
                QCPLineExtruder::extrudePolyline(pts, penWidth, pen.color(), cache.vertices);
                cache.penWidth = penWidth;
                cache.penColor = pen.color().rgba();
            }

            if (cache.isEmpty())
                return;

            const QSize outputSize = parentPlot->rhiOutputSize();
            prl->addPlottable({}, cache.vertices, clipRect, dpr,
                               outputSize.height(),
                               static_cast<float>(gpuOffset.x()),
                               static_cast<float>(gpuOffset.y()),
                               static_cast<float>(painter->opacity()));
            return;
        }
    }
    // Software fallback — no caching benefit, delegate to regular path
    drawPolylineWithGpuFallback(painter, parentPlot, layer, pts, pen, gpuOffset, clipRect);
}

void extrudeColorRuns(const QVector<QPointF>& points, const std::vector<ColorRun>& runs,
                      float penWidth, const ColorScalarMapper& mapper, std::vector<float>& out)
{
    out.clear();
    for (const auto& run : runs)
    {
        QCPLineExtruder::appendPolyline(
            std::span<const QPointF>(points.constData() + run.first, run.last - run.first + 1),
            penWidth, runColor(mapper, run.bucket), out);
    }
}

bool needsColoredReextrusion(const ExtrusionCache& cache, bool freshLines,
                             float penWidth, quint64 colorGeneration)
{
    return freshLines || cache.isEmpty() || cache.penWidth != penWidth
        || cache.colorGeneration != colorGeneration;
}

void drawColoredPolylineRuns(QCPPainter* painter, const QVector<QPointF>& points,
                             const std::vector<ColorRun>& runs, const ColorScalarMapper& mapper,
                             const QPen& pen, const QPointF& gpuOffset)
{
    painter->setBrush(Qt::NoBrush);
    if (!gpuOffset.isNull())
        painter->translate(gpuOffset);
    QPen runPen = pen;
    // Butt ends at colour changes, like the GPU extruder: a square/round cap would
    // overpaint pen/2 of the neighbouring run.
    runPen.setCapStyle(Qt::FlatCap);
    for (const auto& run : runs)
    {
        runPen.setColor(runColor(mapper, run.bucket));
        painter->setPen(runPen);
        painter->drawPolyline(points.constData() + run.first, run.last - run.first + 1);
    }
    if (!gpuOffset.isNull())
        painter->translate(-gpuOffset);
}

void drawColoredPolylineCached(QCPPainter* painter, QCustomPlot* parentPlot, QCPLayer* layer,
                               const QVector<QPointF>& points, const QVector<int>& indices,
                               const ColorScalarMapper& mapper, const QPen& pen,
                               const QPointF& gpuOffset, const QRect& clipRect,
                               bool freshLines, ExtrusionCache& cache)
{
    auto* prl = (parentPlot && parentPlot->rhi()
                 && !painter->modes().testFlag(QCPPainter::pmVectorized)
                 && !painter->modes().testFlag(QCPPainter::pmNoCaching)
                 && pen.style() == Qt::SolidLine)
        ? parentPlot->plottableRhiLayer(layer) : nullptr;
    if (!prl)
        return drawColoredPolylineRuns(painter, points, colorRuns(points, indices, mapper),
                                       mapper, pen, gpuOffset);

    const double dpr = parentPlot->bufferDevicePixelRatio();
    const float penWidth = (pen.isCosmetic() || qFuzzyIsNull(pen.widthF()))
        ? static_cast<float>(1.0 / dpr)
        : qMax(1.0f, static_cast<float>(pen.widthF()));
    if (needsColoredReextrusion(cache, freshLines, penWidth, mapper.generation()))
    {
        extrudeColorRuns(points, colorRuns(points, indices, mapper), penWidth, mapper, cache.vertices);
        cache.penWidth = penWidth;
        cache.colorGeneration = mapper.generation();
    }
    if (cache.isEmpty())
        return;
    prl->addPlottable({}, cache.vertices, clipRect, dpr, parentPlot->rhiOutputSize().height(),
                      static_cast<float>(gpuOffset.x()), static_cast<float>(gpuOffset.y()),
                      static_cast<float>(painter->opacity()));
}

} // namespace qcp
