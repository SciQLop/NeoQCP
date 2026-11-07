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

// A new image is drawn only if the owner's layer is repainted: a replot repaints dirty layers,
// and marks the plot's layers dirty itself only when none is.
void QCPColormapRenderer::invalidateMapImage()
{
    mMapImageInvalidated = true;
    if (mOwner && mOwner->layer())
        mOwner->layer()->markDirty();
}

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

void QCPColormapRenderer::updateMapImage(const QCPColorMapData* data)
{
    PROFILE_HERE_N("QCPColormapRenderer::updateMapImage");
    if (!data) return;

    int keySize = data->keySize();
    int valueSize = data->valueSize();
    if (keySize == 0 || valueSize == 0)
        return;

    QImage argbImage(keySize, valueSize, QImage::Format_ARGB32_Premultiplied);
    const bool isLog = (mDataScaleType == QCPAxis::stLogarithmic);

    const double* rawData = data->rawData();
    for (int y = 0; y < valueSize; ++y)
    {
        QRgb* pixels = reinterpret_cast<QRgb*>(argbImage.scanLine(valueSize - 1 - y));
        mGradient.colorize(rawData + y * keySize, mDataRange, pixels, keySize, 1, isLog);
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

    const QRectF rect = imageRect(keyAxis, valueAxis, keyRange, valueRange);

    bool mirrorX = keyAxis->rangeReversed();
    bool mirrorY = valueAxis->rangeReversed();
    Qt::Orientations flips {};
    if (mirrorX) flips |= Qt::Horizontal;
    if (mirrorY) flips |= Qt::Vertical;

    if (!painter->modes().testFlag(QCPPainter::pmNoCaching))
    {
        if (auto* crl = ensureRhiLayer(); crl && crl->fitsInTexture(mMapImage.size()))
        {
            crl->setImage(flippedMapImage(flips));
            crl->setQuadRect(rect);
            crl->setLayer(mOwner->layer());

            auto* axisRect = keyAxis->axisRect();
            QCustomPlot* plot = mOwner->parentPlot();
            QRect clipRect = axisRect->rect();
            double dpr = plot->bufferDevicePixelRatio();
            int outH = static_cast<int>(plot->height() * dpr);
            crl->setScissorRect(qcp::rhi::computeScissor(clipRect, dpr, outH));
            mImageOnGpu = true;
            return;
        }
        // Too big for a GPU texture on this backend: drop any GPU quad left over from
        // an earlier, smaller frame so it does not keep compositing under the
        // QPainter image drawn below (render() is only skipped by hasContent(), not
        // by re-checking the size). Export mode (pmNoCaching) never reaches here, so
        // this cannot clear content a later on-screen frame still needs.
        //
        // mContourOnGpu also needs to follow: updateContours() (called before draw(),
        // see plottable-colormap2.cpp) only builds the CPU contour fallback when this
        // is false, so leaving it true here would draw an empty or stale contour
        // overlay on every frame the colormap stays oversized, not just this one.
        if (mRhiLayer)
            mRhiLayer->clear();
        mContourOnGpu = false;
        mImageOnGpu = false;
    }

    painter->drawImage(rect, flippedMapImage(flips));
    drawContourFallback(painter, rect, keyAxis);
}

QRectF QCPColormapRenderer::imageRect(const QCPAxis* keyAxis, const QCPAxis* valueAxis,
                                      const QCPRange& keyRange, const QCPRange& valueRange)
{
    return QRectF(QPointF(keyAxis->coordToPixel(keyRange.lower), valueAxis->coordToPixel(valueRange.upper)),
                  QPointF(keyAxis->coordToPixel(keyRange.upper), valueAxis->coordToPixel(valueRange.lower)))
        .normalized();
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
    {
        mRhiLayer->setContourLines(std::move(uvVertices), color);
        mContourOnGpu = true;
    }
}

void QCPColormapRenderer::setContourFallback(QVector<QLineF> uvSegments, const QPen& pen)
{
    mContourFallback = std::move(uvSegments);
    mContourFallbackPen = pen;
}

void QCPColormapRenderer::setLabelOverlay(const QImage& image, const QRectF& pixelRect)
{
    if (mRhiLayer)
        mRhiLayer->setOverlay(image, pixelRect);
}

bool QCPColormapRenderer::hasLabelOverlay() const
{
    return mRhiLayer && mRhiLayer->hasOverlay();
}

void QCPColormapRenderer::clearLabelOverlay()
{
    if (mRhiLayer)
        mRhiLayer->clearOverlay();
}

void QCPColormapRenderer::drawContourFallback(QCPPainter* painter, const QRectF& imageRect,
                                              const QCPAxis* keyAxis)
{
    if (mContourFallback.isEmpty())
        return;
    painter->save();
    if (const QCPAxisRect* axisRect = keyAxis->axisRect())
        painter->setClipRect(axisRect->rect());
    painter->setPen(mContourFallbackPen);
    painter->setBrush(Qt::NoBrush);
    const auto toPixel = [&](double u, double v)
    { return QPointF(imageRect.left() + u * imageRect.width(), imageRect.top() + v * imageRect.height()); };
    for (const QLineF& s : mContourFallback)
        painter->drawLine(toPixel(s.x1(), s.y1()), toPixel(s.x2(), s.y2()));
    painter->restore();
}

void QCPColormapRenderer::clearContour()
{
    mContourFallback.clear();
    mContourOnGpu = false;
    if (mRhiLayer)
        mRhiLayer->clearContourLines();
}

void QCPColormapRenderer::clearRhiContent()
{
    if (mRhiLayer)
        mRhiLayer->clear();
    mContourOnGpu = false;
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
    mContourOnGpu = false;
    mImageOnGpu = false;
    if (!mRhiLayer)
        return;
    QCustomPlot* plot = mOwner ? mOwner->parentPlot() : nullptr;
    if (plot)
        plot->unregisterColormapRhiLayer(mRhiLayer);
    delete mRhiLayer;
    mRhiLayer = nullptr;
}
