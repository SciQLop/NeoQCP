#include "plottable-histogram2d.h"
#include "plottable-colormap.h" // for QCPColorMapData
#include <core.h>
#include <painting/painter.h>
#include <layoutelements/layoutelement-colorscale.h>
#include <layoutelements/layoutelement-axisrect.h>
#include <axis/axis.h>
#include <datasource/histogram-binner.h>
#include <Profiling.hpp>

QCPHistogram2D::QCPHistogram2D(QCPAxis* keyAxis, QCPAxis* valueAxis)
    : QCPAbstractPlottable(keyAxis, valueAxis)
    , mPipeline(parentPlot() ? parentPlot()->pipelineScheduler() : nullptr, this)
    , mRenderer(this)
{
    installTransform();

    connect(&mPipeline, &QCPHistogramPipeline::finished,
            this, [this](uint64_t) {
                mRenderer.invalidateMapImage();
                if (parentPlot())
                    parentPlot()->replot(QCustomPlot::rpQueuedReplot);
            });
}

QCPHistogram2D::~QCPHistogram2D()
{
    mRenderer.releaseRhiLayer();
}

void QCPHistogram2D::installTransform()
{
    int capturedKeyBins = mKeyBins;
    int capturedValueBins = mValueBins;

    mPipeline.setTransform(TransformKind::ViewportIndependent,
        [capturedKeyBins, capturedValueBins](
            const QCPAbstractDataSource& src,
            const ViewportParams& /*vp*/,
            std::any& /*cache*/) -> std::shared_ptr<QCPColorMapData> {
            auto* raw = qcp::algo::bin2d(src, capturedKeyBins, capturedValueBins);
            return std::shared_ptr<QCPColorMapData>(raw);
        });
}

void QCPHistogram2D::setDataSource(std::shared_ptr<QCPAbstractDataSource> source)
{
    mDataSource = std::move(source);
    mPipeline.setSource(mDataSource);
}

void QCPHistogram2D::setDataSource(std::unique_ptr<QCPAbstractDataSource> source)
{
    setDataSource(std::shared_ptr<QCPAbstractDataSource>(std::move(source)));
}

void QCPHistogram2D::dataChanged()
{
    mPipeline.onDataChanged();
}

void QCPHistogram2D::setBins(int keyBins, int valueBins)
{
    if (keyBins <= 0 || valueBins <= 0)
        return;
    if (keyBins == mKeyBins && valueBins == mValueBins)
        return;
    mKeyBins = keyBins;
    mValueBins = valueBins;
    installTransform();
    mPipeline.onDataChanged();
}

void QCPHistogram2D::setNormalization(Normalization norm)
{
    if (mNormalization == norm)
        return;
    mNormalization = norm;
    mRenderer.invalidateMapImage();
    if (mParentPlot)
        mParentPlot->replot(QCustomPlot::rpQueuedReplot);
}

void QCPHistogram2D::setGradient(const QCPColorGradient& gradient)
{
    if (mRenderer.gradient() != gradient)
    {
        mRenderer.setGradient(gradient);
        Q_EMIT gradientChanged(mRenderer.gradient());
        if (mParentPlot)
            mParentPlot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void QCPHistogram2D::setColorScale(QCPColorScale* scale)
{
    if (mRenderer.colorScale())
    {
        disconnect(this, &QCPHistogram2D::dataRangeChanged, mRenderer.colorScale(), &QCPColorScale::setDataRange);
        disconnect(this, &QCPHistogram2D::gradientChanged, mRenderer.colorScale(), &QCPColorScale::setGradient);
        disconnect(this, &QCPHistogram2D::dataScaleTypeChanged, mRenderer.colorScale(), &QCPColorScale::setDataScaleType);
        disconnect(mRenderer.colorScale(), &QCPColorScale::dataRangeChanged, this, &QCPHistogram2D::setDataRange);
        disconnect(mRenderer.colorScale(), &QCPColorScale::gradientChanged, this, &QCPHistogram2D::setGradient);
        disconnect(mRenderer.colorScale(), &QCPColorScale::dataScaleTypeChanged, this, &QCPHistogram2D::setDataScaleType);
    }
    mRenderer.setColorScale(scale);
    if (scale)
    {
        setGradient(scale->gradient());
        setDataScaleType(scale->dataScaleType());
        setDataRange(scale->dataRange());
        connect(this, &QCPHistogram2D::dataRangeChanged, scale, &QCPColorScale::setDataRange);
        connect(this, &QCPHistogram2D::gradientChanged, scale, &QCPColorScale::setGradient);
        connect(this, &QCPHistogram2D::dataScaleTypeChanged, scale, &QCPColorScale::setDataScaleType);
        connect(scale, &QCPColorScale::dataRangeChanged, this, &QCPHistogram2D::setDataRange);
        connect(scale, &QCPColorScale::gradientChanged, this, &QCPHistogram2D::setGradient);
        connect(scale, &QCPColorScale::dataScaleTypeChanged, this, &QCPHistogram2D::setDataScaleType);
    }
}

void QCPHistogram2D::setDataRange(const QCPRange& range)
{
    QCPRange prev = mRenderer.dataRange();
    mRenderer.setDataRange(range);
    QCPRange cur = mRenderer.dataRange();
    if (prev.lower != cur.lower || prev.upper != cur.upper)
    {
        Q_EMIT dataRangeChanged(cur);
        if (mParentPlot)
            mParentPlot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void QCPHistogram2D::setDataScaleType(QCPAxis::ScaleType type)
{
    if (mRenderer.dataScaleType() != type)
    {
        QCPRange prevRange = mRenderer.dataRange();
        mRenderer.setDataScaleType(type);
        QCPRange curRange = mRenderer.dataRange();
        if (prevRange.lower != curRange.lower || prevRange.upper != curRange.upper)
            Q_EMIT dataRangeChanged(curRange);
        Q_EMIT dataScaleTypeChanged(type);
        if (mParentPlot)
            mParentPlot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void QCPHistogram2D::rescaleDataRange(bool recalc)
{
    if (!mDataSource)
        return;
    auto* data = mPipeline.result();
    if (!data)
    {
        if (recalc)
            mPipeline.onDataChanged();
        return;
    }

    if (mNormalization == nNone)
    {
        QCPRange range = data->dataBounds();
        if (range.lower < range.upper)
            setDataRange(range);
        return;
    }

    // Compute bounds over normalized values
    const int keySz = data->keySize();
    const int valSz = data->valueSize();
    if (keySz == 0 || valSz == 0)
        return;

    QVector<double> colSums(keySz, 0.0);
    for (int ki = 0; ki < keySz; ++ki)
        for (int vi = 0; vi < valSz; ++vi)
            colSums[ki] += data->cell(ki, vi);

    double lo = std::numeric_limits<double>::max();
    double hi = std::numeric_limits<double>::lowest();
    for (int ki = 0; ki < keySz; ++ki)
    {
        if (colSums[ki] <= 0)
            continue;
        for (int vi = 0; vi < valSz; ++vi)
        {
            double norm = data->cell(ki, vi) / colSums[ki];
            if (norm < lo) lo = norm;
            if (norm > hi) hi = norm;
        }
    }
    if (lo < hi)
        setDataRange(QCPRange(lo, hi));
}

void QCPHistogram2D::draw(QCPPainter* painter)
{
    PROFILE_HERE_N("QCPHistogram2D::draw");
    if (!mKeyAxis || !mValueAxis)
        return;

    auto* binnedData = mPipeline.result();
    if (!binnedData)
    {
        if (mDataSource)
            mPipeline.onDataChanged();
        return;
    }

    if (mRenderer.mapImageInvalidated())
    {
        if (mNormalization == nColumn)
        {
            const int keySz = binnedData->keySize();
            const int valSz = binnedData->valueSize();
            QVector<double> colSums(keySz, 0.0);
            for (int ki = 0; ki < keySz; ++ki)
                for (int vi = 0; vi < valSz; ++vi)
                    colSums[ki] += binnedData->cell(ki, vi);

            mRenderer.updateMapImage(binnedData,
                [&colSums](double value, int col, int /*row*/) -> double {
                    double sum = colSums[col];
                    return sum > 0 ? value / sum : 0.0;
                });
        }
        else
        {
            mRenderer.updateMapImage(binnedData);
        }
    }

    if (mRenderer.mapImage().isNull())
        return;

    QCPRange keyRange = binnedData->keyRange();
    QCPRange valueRange = binnedData->valueRange();
    applyDefaultAntialiasingHint(painter);
    mRenderer.draw(painter, mKeyAxis.data(), mValueAxis.data(), keyRange, valueRange);
}

void QCPHistogram2D::drawLegendIcon(QCPPainter* painter, const QRectF& rect) const
{
    QLinearGradient lg(rect.topLeft(), rect.topRight());
    lg.setColorAt(0, Qt::blue);
    lg.setColorAt(1, Qt::red);
    painter->setBrush(QBrush(lg));
    painter->setPen(Qt::NoPen);
    painter->drawRect(rect);
}

QCPRange QCPHistogram2D::getKeyRange(bool& foundRange, QCP::SignDomain inSignDomain) const
{
    if (!mDataSource || mDataSource->empty())
    {
        foundRange = false;
        return {};
    }
    return mDataSource->keyRange(foundRange, inSignDomain);
}

QCPRange QCPHistogram2D::getValueRange(bool& foundRange, QCP::SignDomain inSignDomain,
                                        const QCPRange& inKeyRange) const
{
    if (!mDataSource || mDataSource->empty())
    {
        foundRange = false;
        return {};
    }
    return mDataSource->valueRange(foundRange, inSignDomain, inKeyRange);
}

double QCPHistogram2D::selectTest(const QPointF& pos, bool onlySelectable, QVariant*) const
{
    if (onlySelectable && !mSelectable)
        return -1;
    if (!mKeyAxis || !mValueAxis || !mDataSource)
        return -1;

    double key = mKeyAxis->pixelToCoord(pos.x());
    double value = mValueAxis->pixelToCoord(pos.y());

    bool foundKey = false, foundValue = false;
    auto kr = mDataSource->keyRange(foundKey);
    auto vr = mDataSource->valueRange(foundValue);

    if (foundKey && foundValue && kr.contains(key) && vr.contains(value))
        return 0;
    return -1;
}
