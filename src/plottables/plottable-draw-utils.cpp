#include "plottable-draw-utils.h"

#include "../core.h"
#include "../painting/line-extruder.h"
#include "../painting/painter.h"
#include "../painting/plottable-rhi-layer.h"

#include <cmath>

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

} // namespace qcp
