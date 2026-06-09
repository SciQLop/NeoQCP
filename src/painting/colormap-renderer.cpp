#include "colormap-renderer.h"
#include "rhi-utils.h"
#include <plottables/plottable-colormap.h>
#include <plottables/plottable.h>
#include <painting/painter.h>
#include <painting/colormap-rhi-layer.h>
#include <core.h>
#include <layoutelements/layoutelement-axisrect.h>
#include <layer.h>
#include <Profiling.hpp>
#include <utility>
#include <vector>

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
    const bool isLog = (mDataScaleType == QCPAxis::stLogarithmic);

    if (normalize)
    {
        std::vector<double> rowData(keySize);
        for (int y = 0; y < valueSize; ++y)
        {
            for (int x = 0; x < keySize; ++x)
                rowData[x] = normalize(data->cell(x, y), x, y);

            QRgb* pixels = reinterpret_cast<QRgb*>(argbImage.scanLine(valueSize - 1 - y));
            mGradient.colorize(rowData.data(), mDataRange, pixels, keySize, 1, isLog);
        }
    }
    else
    {
        const double* rawData = data->rawData();
        for (int y = 0; y < valueSize; ++y)
        {
            QRgb* pixels = reinterpret_cast<QRgb*>(argbImage.scanLine(valueSize - 1 - y));
            mGradient.colorize(rawData + y * keySize, mDataRange, pixels, keySize, 1, isLog);
        }
    }

    mFlippedMapImage = {};
    mMapImage = std::move(argbImage);
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
    Qt::Orientations flips {};
    if (mirrorX) flips |= Qt::Horizontal;
    if (mirrorY) flips |= Qt::Vertical;

    if (!painter->modes().testFlag(QCPPainter::pmNoCaching))
    {
        if (auto* crl = ensureRhiLayer())
        {
            crl->setImage(flippedMapImage(flips));
            crl->setQuadRect(imageRect.normalized());
            crl->setLayer(mOwner->layer());

            auto* axisRect = keyAxis->axisRect();
            QCustomPlot* plot = mOwner->parentPlot();
            QRect clipRect = axisRect->rect();
            double dpr = plot->bufferDevicePixelRatio();
            int outH = static_cast<int>(plot->height() * dpr);
            crl->setScissorRect(qcp::rhi::computeScissor(clipRect, dpr, outH));
            return;
        }
    }

    painter->drawImage(imageRect, flippedMapImage(flips));
}

const QImage& QCPColormapRenderer::flippedMapImage(Qt::Orientations flips)
{
    if (mFlippedMapImage.isNull() || flips != mLastFlips)
    {
        mFlippedMapImage = flips ? mMapImage.flipped(flips) : mMapImage;
        mLastFlips = flips;
    }
    return mFlippedMapImage;
}

void QCPColormapRenderer::setContourLines(QVector<float> uvVertices, const QColor& color)
{
    if (mRhiLayer)
        mRhiLayer->setContourLines(std::move(uvVertices), color);
}

void QCPColormapRenderer::clearContour()
{
    if (mRhiLayer)
        mRhiLayer->clearContourLines();
}

QCPColormapRhiLayer* QCPColormapRenderer::ensureRhiLayer()
{
    QCustomPlot* plot = mOwner->parentPlot();
    if (!mRhiLayer && plot && plot->rhi())
    {
        mRhiLayer = new QCPColormapRhiLayer(plot->rhi());
        mRhiLayer->setOwner(mOwner);
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
