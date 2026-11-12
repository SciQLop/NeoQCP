#include "colormap-renderer.h"
#include <Profiling.hpp>
#include <vector>
#include <axis/axis.h>
#include <layer.h>
#include <plottables/plottable-colormap.h>
#include <plottables/plottable.h>

// A new image is drawn only if the owner's layer is repainted: a replot repaints dirty layers,
// and marks the plot's layers dirty itself only when none is.
void QCPColormapRenderer::invalidateMapImage()
{
    mMapImageInvalidated = true;
    if (QCPLayer* layer = mOwner ? mOwner->layerRaw() : nullptr)
        layer->markDirty();
}

QCPColormapRenderer::QCPColormapRenderer(QCPAbstractPlottable* owner) : mOwner(owner)
{
    mGradient.loadPreset(QCPColorGradient::gpCold);
    mGradient.setNanHandling(QCPColorGradient::nhTransparent);
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
    QCPRange newRange = (mDataScaleType == QCPAxis::stLogarithmic) ? range.sanitizedForLogScale()
                                                                   : range.sanitizedForLinScale();
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

void QCPColormapRenderer::updateMapImage(const QCPColorMapData* data, bool keyIsVertical)
{
    PROFILE_HERE_N("QCPColormapRenderer::updateMapImage");
    if (!data)
        return;

    int keySize = data->keySize();
    int valueSize = data->valueSize();
    if (keySize == 0 || valueSize == 0)
        return;

    const bool isLog = (mDataScaleType == QCPAxis::stLogarithmic);
    const double* rawData = data->rawData();
    mKeyIsVertical = keyIsVertical;
    if (!keyIsVertical)
    {
        QImage argbImage(keySize, valueSize, QImage::Format_ARGB32_Premultiplied);
        for (int y = 0; y < valueSize; ++y)
        {
            QRgb* pixels = reinterpret_cast<QRgb*>(argbImage.scanLine(valueSize - 1 - y));
            mGradient.colorize(rawData + y * keySize, mDataRange, pixels, keySize, 1, isLog);
        }
        mMapImage = std::move(argbImage);
    }
    else
    {
        // Key down the rows, value along them: the picture the screen wants for a vertical key
        // axis. Built once here, not transposed on every pan.
        QImage argbImage(valueSize, keySize, QImage::Format_ARGB32_Premultiplied);
        for (int k = 0; k < keySize; ++k)
        {
            QRgb* pixels = reinterpret_cast<QRgb*>(argbImage.scanLine(keySize - 1 - k));
            mGradient.colorize(rawData + k, mDataRange, pixels, valueSize, keySize, isLog);
        }
        mMapImage = std::move(argbImage);
    }

    mMapImageInvalidated = false;
    ++mBuildCount;
}

QImage QCPColormapRenderer::gradientImage(int width, int height)
{
    const QCPRange range = QCPRange::validRange(mDataRange) ? mDataRange : QCPRange(0, 1);
    const bool isLog = mDataScaleType == QCPAxis::stLogarithmic;
    std::vector<double> values(static_cast<size_t>(width));
    for (int x = 0; x < width; ++x)
        values[size_t(x)] = range.lower + (x + 0.5) / width * range.size();
    QImage swatch(width, height, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < height; ++y)
        mGradient.colorize(values.data(), range, reinterpret_cast<QRgb*>(swatch.scanLine(y)), width, 1,
                           isLog);
    return swatch;
}
