#include "plottable-draw-utils.h"

#include "../core.h"
#include "../painting/line-extruder.h"
#include "../painting/painter.h"
#include "../painting/plottable-rhi-layer.h"

#include <cmath>
#include <span>

namespace qcp {

QColor defaultPlottableColor(int index)
{
    static const QList<QColor> colors = {
        QColor(31, 119, 180),  QColor(255, 127, 14), QColor(44, 160, 44),
        QColor(214, 39, 40),   QColor(148, 103, 189), QColor(140, 86, 75),
        QColor(227, 119, 194), QColor(127, 127, 127), QColor(188, 189, 34),
        QColor(23, 190, 207)
    };
    return colors[index % colors.size()];
}

QCPPlottableRhiLayer* gpuLayer(QCustomPlot* parentPlot, QCPLayer* layer, bool exporting)
{
    return parentPlot && parentPlot->rhi() && !exporting ? parentPlot->plottableRhiLayer(layer)
                                                         : nullptr;
}

void drawPolylineSplitNaN(QCPPainter* painter, std::span<const QPointF> pts)
{
    int segStart = 0;
    for (int i = 0; i < int(pts.size()); ++i)
    {
        if (std::isnan(pts[i].x()) || std::isnan(pts[i].y()))
        {
            int segLen = i - segStart;
            if (segLen >= 2)
                painter->drawPolyline(pts.data() + segStart, segLen);
            segStart = i + 1;
        }
    }
    int segLen = int(pts.size()) - segStart;
    if (segLen >= 2)
        painter->drawPolyline(pts.data() + segStart, segLen);
}

void drawPolylineSplitNaN(QCPPainter* painter, const QVector<QPointF>& pts)
{
    drawPolylineSplitNaN(painter, std::span<const QPointF>(pts.constData(), pts.size()));
}

float extrusionPenWidth(const QPen& pen, double devicePixelRatio)
{
    return (pen.isCosmetic() || qFuzzyIsNull(pen.widthF()))
        ? static_cast<float>(1.0 / devicePixelRatio)
        : qMax(1.0f, static_cast<float>(pen.widthF()));
}

bool needsReextrusion(const ExtrusionCache& cache, bool freshLines,
                      const QPen& pen, double devicePixelRatio)
{
    return freshLines || cache.isEmpty()
        || cache.withDistance != isDashed(pen)
        || cache.penWidth != extrusionPenWidth(pen, devicePixelRatio)
        || cache.penColor != pen.color().rgba();
}

bool canStrokeOnGpu(const QPen& pen)
{
    return pen.style() != Qt::NoPen;
}

DashPattern dashPatternOf(const QPen& pen, float penWidth)
{
    DashPattern result;
    std::array<double, 8> units = {};
    int count = 0;
    switch (pen.style())
    {
    case Qt::SolidLine:
    case Qt::NoPen:
        return result;
    case Qt::DashLine:
        units = { 4, 2 };
        count = 2;
        break;
    case Qt::DotLine:
        units = { 1, 2 };
        count = 2;
        break;
    case Qt::DashDotLine:
        units = { 4, 2, 1, 2 };
        count = 4;
        break;
    case Qt::DashDotDotLine:
        units = { 4, 2, 1, 2, 1, 2 };
        count = 6;
        break;
    case Qt::CustomDashLine:
    {
        const QList<qreal> custom = pen.dashPattern();
        count = qMin(int(custom.size()), 8);
        for (int i = 0; i < count; ++i)
            units[i] = custom.at(i);
        break;
    }
    }
    // QPen's Square and Round caps extend every dash by half the width at both ends, so QPainter
    // draws an "on" of L + width and an "off" of max(0, L - width): DashLine is 5w on / 1w off.
    const bool capsExtend = pen.capStyle() != Qt::FlatCap;
    // A pattern that draws nothing is not a dash: QPainter treats an empty or all-zero custom
    // pattern as solid, and the cap rule below would otherwise turn 0/0 into 1/0.
    float rawTotal = 0;
    for (int i = 0; i < count; ++i)
        rawTotal += static_cast<float>(units[i]);
    if (rawTotal <= 0)
        return {};
    float total = 0;
    for (int i = 0; i < count; ++i)
    {
        float length = static_cast<float>(units[i]) * penWidth;
        if (capsExtend)
            length += (i % 2) == 0 ? penWidth : -penWidth;
        if (length < 0)
            length = 0;
        result.lengths[i] = length;
        total += length;
    }
    if (total <= 0)
        return {};
    result.count = count;
    result.total = total;
    result.offset = static_cast<float>(pen.dashOffset()) * penWidth;
    return result;
}

bool isDashed(const QPen& pen)
{
    return dashPatternOf(pen, 1.0f).count > 0;
}

namespace
{
// Extrusions for the uncached GPU paths: one buffer reused across draws, so a frame does not
// allocate per plottable.
std::vector<float>& strokeScratch()
{
    static thread_local std::vector<float> buffer;
    return buffer;
}

} // namespace

void extrudeStroke(std::span<const QPointF> pts, const QPen& pen, float penWidth,
                   std::vector<float>& out, bool closed)
{
    out.clear();
    if (!isDashed(pen))
        QCPLineExtruder::appendPolyline(pts, penWidth, pen.color(), out, closed);
    else
        QCPLineExtruder::appendPolylineWithDistance(pts, penWidth, pen.color(), out, 0.0f, closed);
}

bool addGpuStroke(QCPPlottableRhiLayer& layer, std::span<const float> verts, const QPen& pen,
                  float penWidth, const QRect& clipRect, double dpr, int outputHeight,
                  const QPointF& offset, float alpha)
{
    if (verts.empty())
        return false;
    if (isDashed(pen))
    {
        // No dashed pipeline (it failed to build): the caller paints this stroke itself, rather
        // than adding 7-float vertices to the 6-float buffer.
        if (!layer.dashedAvailable())
            return false;
        const DashPattern pattern = dashPatternOf(pen, penWidth);
        layer.addDashedPlottable(verts, std::span<const float>(pattern.lengths.data(), pattern.count),
                                 pattern.offset, clipRect, dpr, outputHeight,
                                 static_cast<float>(offset.x()), static_cast<float>(offset.y()),
                                 alpha);
    }
    else
        layer.addPlottable({}, verts, clipRect, dpr, outputHeight,
                           static_cast<float>(offset.x()), static_cast<float>(offset.y()), alpha);
    return true;
}

bool strokePolylineOnGpu(QCPPainter* painter, QCustomPlot* parentPlot, QCPLayer* layer,
                         std::span<const QPointF> pts, const QPen& pen, const QPointF& gpuOffset,
                         const QRect& clipRect)
{
    QCPPlottableRhiLayer* prl = gpuLayer(parentPlot, layer, painter->isExporting());
    if (!prl || !canStrokeOnGpu(pen) || pts.size() < 2)
        return false;

    const double dpr = parentPlot->bufferDevicePixelRatio();
    const float penWidth = extrusionPenWidth(pen, dpr);
    std::vector<float>& verts = strokeScratch();
    extrudeStroke(pts, pen, penWidth, verts);
    if (verts.empty())
        return false;
    return addGpuStroke(*prl, verts, pen, penWidth, clipRect, dpr,
                        parentPlot->rhiOutputSize().height(), gpuOffset,
                        static_cast<float>(painter->opacity()));
}

bool strokePolylineCached(QCPPainter* painter, QCustomPlot* parentPlot, QCPLayer* layer,
                          std::span<const QPointF> pts, const QPen& pen, const QPointF& gpuOffset,
                          const QRect& clipRect, bool freshLines, ExtrusionCache& cache)
{
    QCPPlottableRhiLayer* prl = gpuLayer(parentPlot, layer, painter->isExporting());
    if (!prl || !canStrokeOnGpu(pen) || pts.size() < 2)
        return false;

    const double dpr = parentPlot->bufferDevicePixelRatio();
    const float penWidth = extrusionPenWidth(pen, dpr);
    if (needsReextrusion(cache, freshLines, pen, dpr))
    {
        extrudeStroke(pts, pen, penWidth, cache.vertices);
        cache.withDistance = isDashed(pen);
        cache.penWidth = penWidth;
        cache.penColor = pen.color().rgba();
    }
    if (cache.isEmpty())
        return false;
    return addGpuStroke(*prl, cache.vertices, pen, penWidth, clipRect, dpr,
                        parentPlot->rhiOutputSize().height(), gpuOffset,
                        static_cast<float>(painter->opacity()));
}

void drawPolylineWithGpuFallback(QCPPainter* painter,
                                  QCustomPlot* parentPlot,
                                  QCPLayer* layer,
                                  const QVector<QPointF>& pts,
                                  const QPen& pen,
                                  const QPointF& gpuOffset,
                                  const QRect& clipRect)
{
    if (strokePolylineOnGpu(painter, parentPlot, layer,
                            std::span<const QPointF>(pts.constData(), pts.size()), pen, gpuOffset,
                            clipRect))
        return;
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
    if (strokePolylineCached(painter, parentPlot, layer,
                             std::span<const QPointF>(pts.constData(), pts.size()), pen, gpuOffset,
                             clipRect, freshLines, cache))
        return;
    // Software fallback — no caching benefit, delegate to regular path
    drawPolylineWithGpuFallback(painter, parentPlot, layer, pts, pen, gpuOffset, clipRect);
}

bool needsColoredReextrusion(const ExtrusionCache& cache, bool freshLines,
                             float penWidth, quint64 colorGeneration, bool dashed)
{
    return freshLines || cache.isEmpty() || cache.penWidth != penWidth
        || cache.withDistance != dashed || cache.colorGeneration != colorGeneration;
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
    auto* prl = canStrokeOnGpu(pen) ? gpuLayer(parentPlot, layer, painter->isExporting()) : nullptr;
    if (!prl)
        return drawColoredPolylineRuns(painter, points, colorRuns(points, indices, mapper),
                                       mapper, pen, gpuOffset);

    const double dpr = parentPlot->bufferDevicePixelRatio();
    const float penWidth = (pen.isCosmetic() || qFuzzyIsNull(pen.widthF()))
        ? static_cast<float>(1.0 / dpr)
        : qMax(1.0f, static_cast<float>(pen.widthF()));
    const bool dashed = isDashed(pen);
    if (needsColoredReextrusion(cache, freshLines, penWidth, mapper.generation(), dashed))
    {
        const std::vector<ColorRun> runs = colorRuns(points, indices, mapper);
        cache.vertices.clear();
        // The runs share their end points, so carrying the arc length over keeps one dash phase
        // across a colour change instead of restarting the pattern at every run.
        float arcLength = 0.0f;
        for (const auto& run : runs)
        {
            const std::span<const QPointF> runPoints(points.constData() + run.first,
                                                    run.last - run.first + 1);
            if (dashed)
                arcLength = QCPLineExtruder::appendPolylineWithDistance(
                    runPoints, penWidth, runColor(mapper, run.bucket), cache.vertices, arcLength);
            else
                QCPLineExtruder::appendPolyline(runPoints, penWidth, runColor(mapper, run.bucket),
                                                cache.vertices);
        }
        cache.withDistance = dashed;
        cache.penWidth = penWidth;
        cache.colorGeneration = mapper.generation();
    }
    if (cache.isEmpty())
        return;
    addGpuStroke(*prl, cache.vertices, pen, penWidth, clipRect, dpr,
                 parentPlot->rhiOutputSize().height(), gpuOffset,
                 static_cast<float>(painter->opacity()));
}

} // namespace qcp
