#include "plottable-histogram2d.h"
#include "plottable-colormap.h" // for QCPColorMapData
#include <core.h>
#include <limits>
#include <painting/painter.h>
#include <layoutelements/layoutelement-colorscale.h>
#include <layoutelements/layoutelement-axisrect.h>
#include <axis/axis.h>
#include <datasource/histogram-binner.h>
#include <Profiling.hpp>

QCPHistogram2D::QCPHistogram2D(QCPAxis* keyAxis, QCPAxis* valueAxis)
    : QCPAbstractPlottable(keyAxis, valueAxis)
    , mPipeline(parentPlot()->pipelineScheduler(), this)
    , mRenderer(this)
{
    installTransform();

    connect(&mPipeline, &QCPHistogramPipeline::finished,
            this, [this](uint64_t) {
                mRenderer.invalidateMapImage();
                parentPlot()->replot(QCustomPlot::rpQueuedReplot);
            });
    connect(&mPipeline, &QCPHistogramPipeline::busyChanged,
            this, [this](bool) { updateEffectiveBusy(); });

    followAxisScales();
}

// The renderer can't reposition cells itself: a log/linear toggle needs a re-bin.
void QCPHistogram2D::followAxisScales()
{
    disconnect(mKeyScaleLink);
    disconnect(mValueScaleLink);
    if (mKeyAxis)
        mKeyScaleLink = connect(mKeyAxis.data(), &QCPAxis::scaleTypeChanged, this, &QCPHistogram2D::refreshBinning);
    if (mValueAxis)
        mValueScaleLink = connect(mValueAxis.data(), &QCPAxis::scaleTypeChanged, this, &QCPHistogram2D::refreshBinning);
}

void QCPHistogram2D::refreshBinning()
{
    installTransform();
    mPipeline.onDataChanged();
}

QCPHistogram2D::~QCPHistogram2D()
{
    mRenderer.releaseRhiLayer();
}

void QCPHistogram2D::installTransform()
{
    int capturedKeyBins = mKeyBins;
    int capturedValueBins = mValueBins;
    const bool keyLog = keyBinScale() == QCPAxis::stLogarithmic;
    const bool valueLog = valueBinScale() == QCPAxis::stLogarithmic;
    mBinnedKeyLog = keyLog;
    mBinnedValueLog = valueLog;

    mPipeline.setTransform(TransformKind::ViewportIndependent,
        [capturedKeyBins, capturedValueBins, keyLog, valueLog](
            const QCPAbstractDataSource& src,
            const ViewportParams& /*vp*/,
            std::any& /*cache*/) -> std::shared_ptr<QCPColorMapData> {
            auto* raw = qcp::algo::bin2d(src, capturedKeyBins, capturedValueBins,
                                         keyLog, valueLog);
            // Empty, not nullptr: the pipeline keeps its previous result on nullptr, so
            // "nothing to bin" would leave the old cells.
            if (!raw)
                return std::make_shared<QCPColorMapData>(0, 0, QCPRange(), QCPRange());
            return std::shared_ptr<QCPColorMapData>(raw);
        });
}

void QCPHistogram2D::setDataSource(std::shared_ptr<QCPAbstractDataSource> source)
{
    mDataSource = std::move(source);
    invalidateBoundsCache();
    mPipeline.setSource(mDataSource);
}

void QCPHistogram2D::setDataSource(std::unique_ptr<QCPAbstractDataSource> source)
{
    setDataSource(std::shared_ptr<QCPAbstractDataSource>(std::move(source)));
}

void QCPHistogram2D::dataChanged()
{
    invalidateBoundsCache();
    mPipeline.onDataChanged();
}

void QCPHistogram2D::invalidateBoundsCache() const
{
    for (auto& row : mBoundsCache)
        for (auto& entry : row)
            entry.valid = false;
}

bool QCPHistogram2D::cachedFiniteBounds(QCPRange& keyOut, QCPRange& valueOut,
                                        bool keyPositiveOnly, bool valuePositiveOnly) const
{
    auto& entry = mBoundsCache[keyPositiveOnly ? 1 : 0][valuePositiveOnly ? 1 : 0];
    if (!entry.valid)
    {
        entry.found = mDataSource
            && mDataSource->finiteKeyValueBounds(entry.key, entry.value,
                                                 keyPositiveOnly, valuePositiveOnly);
        entry.valid = true;
    }
    keyOut = entry.key;
    valueOut = entry.value;
    return entry.found;
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

void QCPHistogram2D::setKeyBinScale(QCPAxis::ScaleType type)
{
    if (mKeyAxis)
        mKeyAxis->setScaleType(type); // scaleTypeChanged -> re-bin
}

void QCPHistogram2D::setValueBinScale(QCPAxis::ScaleType type)
{
    if (mValueAxis)
        mValueAxis->setScaleType(type);
}

QCPAxis::ScaleType QCPHistogram2D::keyBinScale() const
{
    return mKeyAxis ? mKeyAxis->scaleType() : QCPAxis::stLinear;
}

QCPAxis::ScaleType QCPHistogram2D::valueBinScale() const
{
    return mValueAxis ? mValueAxis->scaleType() : QCPAxis::stLinear;
}

void QCPHistogram2D::setNormalization(Normalization norm)
{
    if (mNormalization == norm)
        return;
    mNormalization = norm;
    mRenderer.invalidateMapImage();
    mParentPlot->replot(QCustomPlot::rpQueuedReplot);
}

void QCPHistogram2D::setGradient(const QCPColorGradient& gradient)
{
    if (mRenderer.gradient() == gradient)
        return;
    mRenderer.setGradient(gradient);
    redrawLater();
    Q_EMIT gradientChanged(mRenderer.gradient()); // last: a slot may delete this histogram
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
        // Each setter signals, and a slot may delete this histogram or the scale.
        const QPointer<QCPHistogram2D> alive(this);
        const QPointer<QCPColorScale> live(scale);
        setGradient(scale->gradient());
        if (alive && live)
            setDataScaleType(live->dataScaleType());
        if (alive && live)
            setDataRange(live->dataRange());
        if (!alive || !live)
            return;
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
        redrawLater();
        Q_EMIT dataRangeChanged(cur); // last: a slot may delete this histogram
    }
}

void QCPHistogram2D::setDataScaleType(QCPAxis::ScaleType type)
{
    if (mRenderer.dataScaleType() == type)
        return;
    const QPointer<QCPHistogram2D> alive(this); // a dataRangeChanged slot may delete this histogram
    const QCPRange prevRange = mRenderer.dataRange();
    mRenderer.setDataScaleType(type);
    redrawLater();
    const QCPRange curRange = mRenderer.dataRange();
    if (prevRange.lower != curRange.lower || prevRange.upper != curRange.upper)
        Q_EMIT dataRangeChanged(curRange);
    if (alive)
        Q_EMIT dataScaleTypeChanged(type);
}

void QCPHistogram2D::redrawLater()
{
    if (mLayer)
        mLayer->markDirty();
    mParentPlot->replot(QCustomPlot::rpQueuedReplot);
}

const QCPColorMapData* QCPHistogram2D::binned() const
{
    const auto* data = mPipeline.result();
    return data && !data->isEmpty() ? data : nullptr;
}

QCPRange QCPHistogram2D::colorDataBounds(bool& found) const
{
    found = false;
    auto* data = binned();
    if (!mDataSource || !data)
        return {};
    if (mNormalization == nNone)
    {
        found = true;
        return data->dataBounds();
    }

    // Bounds of the normalized values
    const int keySz = data->keySize();
    const int valSz = data->valueSize();
    if (keySz == 0 || valSz == 0)
        return {};

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
    found = lo <= hi;
    return found ? QCPRange(lo, hi) : QCPRange();
}

void QCPHistogram2D::rescaleDataRange(bool recalc)
{
    if (!mDataSource)
        return;
    if (!mPipeline.result())
    {
        if (recalc)
            mPipeline.onDataChanged();
        return;
    }
    bool found = false;
    const QCPRange range = colorDataBounds(found);
    if (found && range.lower < range.upper)
        setDataRange(range);
}

bool QCPHistogram2D::canProduceContent() const
{
    if (!mKeyAxis || !mValueAxis || !mDataSource)
        return false;
    return mPipeline.result() != nullptr || mPipeline.isBusy();
}

bool QCPHistogram2D::binnedForTheAxisScales() const
{
    return (keyBinScale() == QCPAxis::stLogarithmic) == mBinnedKeyLog
        && (valueBinScale() == QCPAxis::stLogarithmic) == mBinnedValueLog;
}

// refreshBinning() emits busyChanged at once, and nothing may emit during a paint. The scales
// may be fresh again when this runs (set back, or re-binned by an export): skip the re-bin then.
void QCPHistogram2D::queueRefreshBinning()
{
    if (mRebinQueued)
        return;
    mRebinQueued = true;
    QMetaObject::invokeMethod(this, [this] {
        mRebinQueued = false;
        if (!binnedForTheAxisScales())
            refreshBinning();
    }, Qt::QueuedConnection);
}

// Export does not pump the event loop, so a background result would never arrive. No signals.
void QCPHistogram2D::binForExport()
{
    if (!mDataSource)
        return;
    if (!binnedForTheAxisScales())
        installTransform();
    if (!mPipeline.runSynchronously(ViewportParams::fromAxes(mKeyAxis.data(), mValueAxis.data())))
        return;
    // A job still running may have the old scales: it must not replace these cells.
    mPipeline.supersedeEarlierJobs();
    mRenderer.invalidateMapImage();
}

void QCPHistogram2D::draw(QCPPainter* painter)
{
    PROFILE_HERE_N("QCPHistogram2D::draw");
    if (!mKeyAxis || !mValueAxis)
        return;
    const bool staleScales = !binnedForTheAxisScales();
    if (staleScales)
        queueRefreshBinning();

    const bool exporting = painter->modes().testFlag(QCPPainter::pmNoCaching);
    if (exporting && (staleScales || !mPipeline.result() || mPipeline.isBusy()))
        binForExport();
    // Not re-binned from here when there is nothing: no result with an idle pipeline means the
    // data bins to nothing, and a re-bin would loop through finished() -> replot -> draw.
    auto* binnedData = binned();
    if (!binnedData)
        return;

    if (mRenderer.mapImageInvalidated())
    {
        if (mNormalization == nColumn)
        {
            // qMax: GCC cannot see that keySize() is non-negative and reports a
            // bogus -Wstringop-overflow on QVector's fill otherwise.
            const int keySz = qMax(0, binnedData->keySize());
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
    painter->setBrush(QBrush(gradient().toLinearGradient(rect.topLeft(), rect.topRight())));
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
    // Histogram keys are scattered, not sorted, so keyRange()'s first/last
    // shortcut would be wrong. Report the true finite min/max (positive-only on
    // a log axis), matching what bin2d() accumulates.
    QCPRange kr, vr;
    foundRange = cachedFiniteBounds(kr, vr, inSignDomain == QCP::sdPositive, false);
    return foundRange ? kr : QCPRange();
}

QCPRange QCPHistogram2D::getValueRange(bool& foundRange, QCP::SignDomain inSignDomain,
                                        const QCPRange& inKeyRange) const
{
    if (!mDataSource || mDataSource->empty())
    {
        foundRange = false;
        return {};
    }
    const bool hasKeyRestriction = inKeyRange.lower != inKeyRange.upper
                                    || inKeyRange.lower != 0.0;
    if (!hasKeyRestriction)
        return mDataSource->valueRange(foundRange, inSignDomain, inKeyRange);

    // Histogram keys are scattered: the source's key-restricted valueRange
    // binary-searches sorted keys (graph contract) and would be wrong here —
    // scan linearly through the virtual accessors instead.
    foundRange = false;
    double lo = std::numeric_limits<double>::max();
    double hi = std::numeric_limits<double>::lowest();
    const int n = mDataSource->size();
    for (int i = 0; i < n; ++i)
    {
        const double k = mDataSource->keyAt(i);
        if (k < inKeyRange.lower || k > inKeyRange.upper)
            continue;
        const double v = mDataSource->valueAt(i);
        if (inSignDomain == QCP::sdPositive && v <= 0) continue;
        if (inSignDomain == QCP::sdNegative && v >= 0) continue;
        if (v < lo) lo = v;
        if (v > hi) hi = v;
        foundRange = true;
    }
    return foundRange ? QCPRange(lo, hi) : QCPRange();
}

double QCPHistogram2D::selectTest(const QPointF& pos, bool onlySelectable, QVariant*) const
{
    if (onlySelectable && !mSelectable)
        return -1;
    if (!mKeyAxis || !mValueAxis || !mDataSource)
        return -1;

    double key = mKeyAxis->pixelToCoord(pos.x());
    double value = mValueAxis->pixelToCoord(pos.y());

    // Use the true scattered min/max, not the sorted first/last shortcut.
    QCPRange kr, vr;
    if (cachedFiniteBounds(kr, vr, false, false) && kr.contains(key) && vr.contains(value))
        return 0;
    return -1;
}
