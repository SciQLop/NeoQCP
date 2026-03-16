#include "colormap-renderer.h"
#include <plottables/plottable-colormap.h>
#include <plottables/plottable.h>
#include <painting/painter.h>
#include <painting/colormap-rhi-layer.h>
#include <core.h>
#include <layoutelements/layoutelement-axisrect.h>
#include <layer.h>
#include <Profiling.hpp>

QCPColormapRenderer::QCPColormapRenderer(QCPAbstractPlottable* owner)
    : mOwner(owner)
{
    mGradient.loadPreset(QCPColorGradient::gpCold);
    mGradient.setNanHandling(QCPColorGradient::nhTransparent);
}

QCPColormapRenderer::~QCPColormapRenderer()
{
    releaseRhiLayer();
}

void QCPColormapRenderer::setGradient(const QCPColorGradient& gradient)
{
    if (mGradient == gradient)
        return;
    mGradient = gradient;
    if (mGradient.nanHandling() == QCPColorGradient::nhNone)
        mGradient.setNanHandling(QCPColorGradient::nhTransparent);
    mMapImageInvalidated = true;
}

void QCPColormapRenderer::setDataRange(const QCPRange& range)
{
    if (!QCPRange::validRange(range))
        return;
    QCPRange newRange = (mDataScaleType == QCPAxis::stLogarithmic)
        ? range.sanitizedForLogScale() : range.sanitizedForLinScale();
    if (mDataRange.lower == newRange.lower && mDataRange.upper == newRange.upper)
        return;
    mDataRange = newRange;
    mMapImageInvalidated = true;
}

void QCPColormapRenderer::setDataScaleType(QCPAxis::ScaleType type)
{
    if (mDataScaleType == type)
        return;
    mDataScaleType = type;
    mMapImageInvalidated = true;
    if (type == QCPAxis::stLogarithmic && QCPRange::validRange(mDataRange))
        mDataRange = mDataRange.sanitizedForLogScale();
}

void QCPColormapRenderer::setColorScale(QCPColorScale* scale)
{
    mColorScale = scale;
}

void QCPColormapRenderer::rescaleDataRange(const QCPColorMapData* data, bool /*recalc*/)
{
    if (!data)
        return;
    QCPRange range = data->dataBounds();
    if (range.lower < range.upper)
        setDataRange(range);
}

void QCPColormapRenderer::updateMapImage(const QCPColorMapData* data, NormalizeFn normalize)
{
    PROFILE_HERE_N("QCPColormapRenderer::updateMapImage");
    if (!data) return;

    int keySize = data->keySize();
    int valueSize = data->valueSize();
    if (keySize == 0 || valueSize == 0)
        return;

    QImage argbImage(keySize, valueSize, QImage::Format_ARGB32_Premultiplied);

    std::vector<double> rowData(keySize);
    for (int y = 0; y < valueSize; ++y)
    {
        for (int x = 0; x < keySize; ++x)
        {
            double v = data->cell(x, y);
            rowData[x] = normalize ? normalize(v, x, y) : v;
        }

        QRgb* pixels = reinterpret_cast<QRgb*>(argbImage.scanLine(valueSize - 1 - y));
        mGradient.colorize(rowData.data(), mDataRange, pixels, keySize,
                           1, mDataScaleType == QCPAxis::stLogarithmic);
    }

    mMapImage = argbImage.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
    mMapImageInvalidated = false;
}

void QCPColormapRenderer::draw(QCPPainter* painter, QCPAxis* keyAxis, QCPAxis* valueAxis,
                               const QCPRange& keyRange, const QCPRange& valueRange)
{
    PROFILE_HERE_N("QCPColormapRenderer::draw");
    if (!keyAxis || !valueAxis || mMapImage.isNull())
        return;

    QPointF topLeft(keyAxis->coordToPixel(keyRange.lower),
                    valueAxis->coordToPixel(valueRange.upper));
    QPointF bottomRight(keyAxis->coordToPixel(keyRange.upper),
                        valueAxis->coordToPixel(valueRange.lower));
    QRectF imageRect(topLeft, bottomRight);

    bool mirrorX = keyAxis->rangeReversed();
    bool mirrorY = valueAxis->rangeReversed();
    Qt::Orientations flips;
    if (mirrorX) flips |= Qt::Horizontal;
    if (mirrorY) flips |= Qt::Vertical;

    if (auto* crl = ensureRhiLayer())
    {
        crl->setImage(mMapImage.flipped(flips));
        crl->setQuadRect(imageRect.normalized());
        crl->setLayer(mOwner->layer());

        auto* axisRect = keyAxis->axisRect();
        QCustomPlot* plot = mOwner->parentPlot();
        QRect clipRect = axisRect->rect();
        double dpr = plot->bufferDevicePixelRatio();
        int sx = static_cast<int>(clipRect.x() * dpr);
        int sy = static_cast<int>(clipRect.y() * dpr);
        int sw = static_cast<int>(clipRect.width() * dpr);
        int sh = static_cast<int>(clipRect.height() * dpr);
        if (auto* rhi = plot->rhi(); rhi && rhi->isYUpInNDC())
            sy = static_cast<int>(plot->height() * dpr) - sy - sh;
        crl->setScissorRect(QRect(sx, sy, sw, sh));
        return;
    }

    painter->drawImage(imageRect, mMapImage.convertToFormat(
        QImage::Format_ARGB32_Premultiplied).flipped(flips));
}

QCPColormapRhiLayer* QCPColormapRenderer::ensureRhiLayer()
{
    QCustomPlot* plot = mOwner->parentPlot();
    if (!mRhiLayer && plot && plot->rhi())
    {
        mRhiLayer = new QCPColormapRhiLayer(plot->rhi());
        plot->registerColormapRhiLayer(mRhiLayer);
    }
    return mRhiLayer;
}

void QCPColormapRenderer::releaseRhiLayer()
{
    if (!mRhiLayer)
        return;
    QCustomPlot* plot = mOwner ? mOwner->parentPlot() : nullptr;
    if (plot)
        plot->unregisterColormapRhiLayer(mRhiLayer);
    delete mRhiLayer;
    mRhiLayer = nullptr;
}
