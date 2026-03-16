#include "plottable-colormap2.h"
#include "plottable-colormap.h" // for QCPColorMapData
#include <core.h>
#include <painting/painter.h>
#include <painting/colormap-rhi-layer.h>
#include <layoutelements/layoutelement-colorscale.h>
#include <layoutelements/layoutelement-axisrect.h>
#include <axis/axis.h>
#include <layer.h>
#include <datasource/resample.h>
#include <Profiling.hpp>

QCPColorMap2::QCPColorMap2(QCPAxis* keyAxis, QCPAxis* valueAxis)
    : QCPAbstractPlottable(keyAxis, valueAxis)
    , mPipeline(parentPlot() ? parentPlot()->pipelineScheduler() : nullptr, this)
{
    mGradient.loadPreset(QCPColorGradient::gpCold);
    mGradient.setNanHandling(QCPColorGradient::nhTransparent);

    mPipeline.setTransform(TransformKind::ViewportDependent,
        [&gapThreshold = mGapThreshold](
            const QCPAbstractDataSource2D& src,
            const ViewportParams& vp,
            std::any& /*cache*/) -> std::shared_ptr<QCPColorMapData> {
            if (src.xSize() < 2) return nullptr;

            bool found = false;
            auto xRange = src.xRange(found);
            if (!found) return nullptr;
            auto yRange = src.yRange(found);
            if (!found) return nullptr;

            int xBegin = src.findXBegin(vp.keyRange.lower);
            int xEnd = src.findXEnd(vp.keyRange.upper);
            if (xEnd <= xBegin) return nullptr;

            // Clamp output grid to intersection of viewport and data extent.
            // Without this, zooming out creates a grid spanning the full viewport
            // with bins wider than source spacing, leaving most bins empty (black).
            QCPRange xOut(std::max(vp.keyRange.lower, xRange.lower),
                          std::min(vp.keyRange.upper, xRange.upper));
            QCPRange yOut(std::max(vp.valueRange.lower, yRange.lower),
                          std::min(vp.valueRange.upper, yRange.upper));
            if (xOut.lower >= xOut.upper || yOut.lower >= yOut.upper)
                return nullptr;

            auto logFrac = [](const QCPRange& data, const QCPRange& vp) {
                if (data.lower <= 0 || vp.lower <= 0) return data.size() / vp.size();
                return (std::log10(data.upper) - std::log10(data.lower))
                     / (std::log10(vp.upper) - std::log10(vp.lower));
            };
            double xFrac = xOut.size() / vp.keyRange.size();
            double yFrac = vp.valueLogScale ? logFrac(yOut, vp.valueRange)
                                            : yOut.size() / vp.valueRange.size();
            int pixW = std::max(1, static_cast<int>(vp.plotWidthPx * xFrac));
            int pixH = std::max(1, static_cast<int>(vp.plotHeightPx * yFrac));

            int visibleSrcCols = xEnd - xBegin;
            int w = std::clamp(visibleSrcCols, pixW, pixW * 4);
            int h = std::clamp(src.ySize(), pixH, pixH * 4);
            if (w <= 0 || h <= 0) return nullptr;

            auto* raw = qcp::algo2d::resample(src, xBegin, xEnd,
                xOut, yOut, w, h, vp.valueLogScale, gapThreshold.load(std::memory_order_relaxed));
            return std::shared_ptr<QCPColorMapData>(raw);
        });

    if (keyAxis)
    {
        connect(keyAxis, QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
                this, &QCPColorMap2::onViewportChanged);
    }
    if (valueAxis)
    {
        connect(valueAxis, QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
                this, &QCPColorMap2::onViewportChanged);
        connect(valueAxis, &QCPAxis::scaleTypeChanged,
                this, [this](QCPAxis::ScaleType) { onViewportChanged(); });
    }

    connect(&mPipeline, &QCPColormapPipeline::finished,
            this, [this](uint64_t) {
                mMapImageInvalidated = true;
                if (parentPlot())
                    parentPlot()->replot(QCustomPlot::rpQueuedReplot);
            });
}

QCPColorMap2::~QCPColorMap2()
{
    if (mRhiLayer && mParentPlot)
        mParentPlot->unregisterColormapRhiLayer(mRhiLayer);
    delete mRhiLayer;
}

void QCPColorMap2::setDataSource(std::unique_ptr<QCPAbstractDataSource2D> source)
{
    setDataSource(std::shared_ptr<QCPAbstractDataSource2D>(std::move(source)));
}

void QCPColorMap2::setDataSource(std::shared_ptr<QCPAbstractDataSource2D> source)
{
    mDataSource = std::move(source);
    mPipeline.setSource(mDataSource);
}

void QCPColorMap2::dataChanged()
{
    mPipeline.onDataChanged();
}

void QCPColorMap2::setGradient(const QCPColorGradient& gradient)
{
    if (mGradient != gradient)
    {
        mGradient = gradient;
        if (mGradient.nanHandling() == QCPColorGradient::nhNone)
            mGradient.setNanHandling(QCPColorGradient::nhTransparent);
        mMapImageInvalidated = true;
        Q_EMIT gradientChanged(mGradient);
        if (mParentPlot)
            mParentPlot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void QCPColorMap2::setColorScale(QCPColorScale* colorScale)
{
    if (mColorScale)
    {
        disconnect(this, &QCPColorMap2::dataRangeChanged, mColorScale, &QCPColorScale::setDataRange);
        disconnect(this, &QCPColorMap2::gradientChanged, mColorScale, &QCPColorScale::setGradient);
        disconnect(this, &QCPColorMap2::dataScaleTypeChanged, mColorScale, &QCPColorScale::setDataScaleType);
        disconnect(mColorScale, &QCPColorScale::dataRangeChanged, this, &QCPColorMap2::setDataRange);
        disconnect(mColorScale, &QCPColorScale::gradientChanged, this, &QCPColorMap2::setGradient);
        disconnect(mColorScale, &QCPColorScale::dataScaleTypeChanged, this, &QCPColorMap2::setDataScaleType);
    }
    mColorScale = colorScale;
    if (mColorScale)
    {
        setGradient(mColorScale->gradient());
        setDataScaleType(mColorScale->dataScaleType());
        setDataRange(mColorScale->dataRange());
        connect(this, &QCPColorMap2::dataRangeChanged, mColorScale, &QCPColorScale::setDataRange);
        connect(this, &QCPColorMap2::gradientChanged, mColorScale, &QCPColorScale::setGradient);
        connect(this, &QCPColorMap2::dataScaleTypeChanged, mColorScale, &QCPColorScale::setDataScaleType);
        connect(mColorScale, &QCPColorScale::dataRangeChanged, this, &QCPColorMap2::setDataRange);
        connect(mColorScale, &QCPColorScale::gradientChanged, this, &QCPColorMap2::setGradient);
        connect(mColorScale, &QCPColorScale::dataScaleTypeChanged, this, &QCPColorMap2::setDataScaleType);
    }
}

void QCPColorMap2::setDataRange(const QCPRange& range)
{
    if (!QCPRange::validRange(range))
        return;
    QCPRange newRange = (mDataScaleType == QCPAxis::stLogarithmic)
        ? range.sanitizedForLogScale() : range.sanitizedForLinScale();
    if (mDataRange.lower != newRange.lower || mDataRange.upper != newRange.upper)
    {
        mDataRange = newRange;
        mMapImageInvalidated = true;
        Q_EMIT dataRangeChanged(mDataRange);
        if (mParentPlot)
            mParentPlot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void QCPColorMap2::setDataScaleType(QCPAxis::ScaleType type)
{
    if (mDataScaleType != type)
    {
        mDataScaleType = type;
        mMapImageInvalidated = true;
        if (type == QCPAxis::stLogarithmic && QCPRange::validRange(mDataRange))
        {
            QCPRange sanitized = mDataRange.sanitizedForLogScale();
            if (mDataRange.lower != sanitized.lower || mDataRange.upper != sanitized.upper)
            {
                mDataRange = sanitized;
                Q_EMIT dataRangeChanged(mDataRange);
            }
        }
        Q_EMIT dataScaleTypeChanged(mDataScaleType);
        if (mParentPlot)
            mParentPlot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void QCPColorMap2::rescaleDataRange(bool recalc)
{
    if (!mDataSource)
        return;
    auto* data = mPipeline.result();
    bool found = false;
    QCPRange range;
    if (data && !recalc)
        range = data->dataBounds();
    else
        range = mDataSource->zRange(found);
    if (range.lower < range.upper)
        setDataRange(range);
}

void QCPColorMap2::onViewportChanged()
{
    if (!mKeyAxis || !mValueAxis || !mDataSource) return;
    auto* axisRect = mKeyAxis->axisRect();
    if (!axisRect) return;

    ViewportParams vp;
    vp.keyRange = mKeyAxis->range();
    vp.valueRange = mValueAxis->range();
    vp.plotWidthPx = axisRect->width();
    vp.plotHeightPx = axisRect->height();
    vp.keyLogScale = (mKeyAxis->scaleType() == QCPAxis::stLogarithmic);
    vp.valueLogScale = (mValueAxis->scaleType() == QCPAxis::stLogarithmic);

    mPipeline.onViewportChanged(vp);
}

void QCPColorMap2::updateMapImage()
{
    PROFILE_HERE_N("QCPColorMap2::updateMapImage");
    auto* data = mPipeline.result();
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
            rowData[x] = data->cell(x, y);

        QRgb* pixels = reinterpret_cast<QRgb*>(argbImage.scanLine(valueSize - 1 - y));
        mGradient.colorize(rowData.data(), mDataRange, pixels, keySize,
                           1, mDataScaleType == QCPAxis::stLogarithmic);
    }

    mMapImage = argbImage.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
    mMapImageInvalidated = false;
}

QCPColormapRhiLayer* QCPColorMap2::ensureRhiLayer()
{
    if (!mRhiLayer && mParentPlot && mParentPlot->rhi())
    {
        mRhiLayer = new QCPColormapRhiLayer(mParentPlot->rhi());
        mParentPlot->registerColormapRhiLayer(mRhiLayer);
    }
    return mRhiLayer;
}

void QCPColorMap2::draw(QCPPainter* painter)
{
    PROFILE_HERE_N("QCPColorMap2::draw");
    if (!mKeyAxis || !mValueAxis)
        return;

    auto* resampledData = mPipeline.result();
    if (!resampledData)
    {
        if (mDataSource)
            onViewportChanged();
        return;
    }

    if (mMapImageInvalidated)
        updateMapImage();

    if (mMapImage.isNull())
        return;

    QCPRange keyRange = resampledData->keyRange();
    QCPRange valueRange = resampledData->valueRange();

    QPointF topLeft = QPointF(mKeyAxis->coordToPixel(keyRange.lower),
                              mValueAxis->coordToPixel(valueRange.upper));
    QPointF bottomRight = QPointF(mKeyAxis->coordToPixel(keyRange.upper),
                                  mValueAxis->coordToPixel(valueRange.lower));
    QRectF imageRect(topLeft, bottomRight);

    bool mirrorX = mKeyAxis->rangeReversed();
    bool mirrorY = mValueAxis->rangeReversed();
    Qt::Orientations flips;
    if (mirrorX) flips |= Qt::Horizontal;
    if (mirrorY) flips |= Qt::Vertical;

    if (auto* crl = ensureRhiLayer())
    {
        crl->setImage(mMapImage.flipped(flips));
        crl->setQuadRect(imageRect.normalized());
        crl->setLayer(layer());

        auto* axisRect = mKeyAxis->axisRect();
        QRect clipRect = axisRect->rect();
        double dpr = mParentPlot->bufferDevicePixelRatio();
        int sx = static_cast<int>(clipRect.x() * dpr);
        int sy = static_cast<int>(clipRect.y() * dpr);
        int sw = static_cast<int>(clipRect.width() * dpr);
        int sh = static_cast<int>(clipRect.height() * dpr);
        if (mParentPlot->rhi()->isYUpInNDC())
            sy = static_cast<int>(mParentPlot->height() * dpr) - sy - sh;
        crl->setScissorRect(QRect(sx, sy, sw, sh));
        return;
    }

    applyDefaultAntialiasingHint(painter);
    painter->drawImage(imageRect, mMapImage.convertToFormat(
        QImage::Format_ARGB32_Premultiplied).flipped(flips));
}

void QCPColorMap2::drawLegendIcon(QCPPainter* painter, const QRectF& rect) const
{
    QLinearGradient lg(rect.topLeft(), rect.topRight());
    lg.setColorAt(0, Qt::blue);
    lg.setColorAt(1, Qt::red);
    painter->setBrush(QBrush(lg));
    painter->setPen(Qt::NoPen);
    painter->drawRect(rect);
}

QCPRange QCPColorMap2::getKeyRange(bool& foundRange, QCP::SignDomain inSignDomain) const
{
    if (!mDataSource)
    {
        foundRange = false;
        return {};
    }
    return mDataSource->xRange(foundRange, inSignDomain);
}

QCPRange QCPColorMap2::getValueRange(bool& foundRange, QCP::SignDomain inSignDomain,
                                     const QCPRange&) const
{
    if (!mDataSource)
    {
        foundRange = false;
        return {};
    }
    return mDataSource->yRange(foundRange, inSignDomain);
}

double QCPColorMap2::selectTest(const QPointF& pos, bool onlySelectable, QVariant*) const
{
    if (onlySelectable && !mSelectable)
        return -1;
    if (!mKeyAxis || !mValueAxis || !mDataSource)
        return -1;

    double key = mKeyAxis->pixelToCoord(pos.x());
    double value = mValueAxis->pixelToCoord(pos.y());

    bool foundKey = false, foundValue = false;
    auto kr = mDataSource->xRange(foundKey);
    auto vr = mDataSource->yRange(foundValue);

    if (foundKey && foundValue && kr.contains(key) && vr.contains(value))
        return 0;
    return -1;
}
