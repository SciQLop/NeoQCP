#include "plottable-histogram2d.h"
#include "plottable-colormap.h" // for QCPColorMapData
#include <core.h>
#include <misuse.h>
#include <limits>
#include <painting/draw-context.h>
#include <layoutelements/layoutelement-axisrect.h>
#include <axis/axis.h>
#include <datasource/histogram-binner.h>
#include <Profiling.hpp>

QCPHistogram2D::QCPHistogram2D(QCPAxis* keyAxis, QCPAxis* valueAxis)
    : QCPAbstractColorMapped(keyAxis, valueAxis, true)
    , mPipeline(parentPlot() ? parentPlot()->pipelineScheduler() : nullptr, this)
    , mRenderer(this)
{
    installTransform();

    connect(&mPipeline, &QCPHistogramPipeline::finished,
            this, [this](uint64_t) {
                invalidateShown();
                markDirty(); // marking schedules the repaint
                const QCPEmissionScope scope(mParentPlot);
                qcpEmit(this, mParentPlot, &QCPHistogram2D::histogramReady);
            });
    connect(&mPipeline, &QCPHistogramPipeline::busyChanged,
            this, [this](bool) { updateEffectiveBusy(); });

    followAxisScales();

    followKeyAxisRectScale();
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

void QCPHistogram2D::installTransform()
{
    int capturedKeyBins = mKeyBins;
    int capturedValueBins = mValueBins;
    const bool keyLog = mKeyAxis && mKeyAxis->scaleType() == QCPAxis::stLogarithmic;
    const bool valueLog = mValueAxis && mValueAxis->scaleType() == QCPAxis::stLogarithmic;
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
    markDirty();
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
    QCP_CHECK_OR_RETURN(keyBins > 0 && valueBins > 0, "a histogram needs at least one bin", );
    if (keyBins == mKeyBins && valueBins == mValueBins)
        return;
    markDirty();
    mKeyBins = keyBins;
    mValueBins = valueBins;
    installTransform();
    mPipeline.onDataChanged();
}

void QCPHistogram2D::setNormalization(QCP::HistogramNormalization norm)
{
    if (norm == mNormalization)
        return;
    markDirty();
    mNormalization = norm;
    invalidateShown();
}

void QCPHistogram2D::setColumnNormalized(bool normalized)
{
    if (mColumnNormalized == normalized)
        return;
    markDirty();
    mColumnNormalized = normalized;
    invalidateShown();
}

void QCPHistogram2D::colorMappingChanged()
{
    mRenderer.setDataScaleType(dataScaleType());
    mRenderer.setDataRange(dataRange());
    mRenderer.setGradient(gradient());
    markDirty();
}

void QCPHistogram2D::redrawLater()
{
    markDirty(); // marking schedules the repaint
}

const QCPColorMapData* QCPHistogram2D::binned() const
{
    const auto* data = mPipeline.result();
    return data && !data->isEmpty() ? data : nullptr;
}

namespace
{
QCPRange rangeOrNothing(double lo, double hi, bool& found)
{
    found = lo <= hi;
    return found ? QCPRange(lo, hi) : QCPRange();
}
}

void QCPHistogram2D::invalidateShown()
{
    mShown.stale = true;
    mRenderer.invalidateMapImage();
    mContours.dataChanged();
}

const QCPColorMapData* QCPHistogram2D::shown() const
{
    const auto* bins = binned();
    if (!bins || (!mColumnNormalized && mNormalization == QCP::HistogramNormalization::Count))
        return bins;
    if (mShown.stale)
    {
        // qMax: GCC cannot see that keySize() is non-negative and reports a
        // bogus -Wstringop-overflow on QVector's fill otherwise.
        const int keySz = qMax(0, bins->keySize());
        const int valSz = bins->valueSize();
        auto cells = std::make_shared<QCPColorMapData>(keySz, valSz, bins->keyRange(), bins->valueRange());
        // Density needs the cell area, also for log-spaced bins: the 1-D and 2-D
        // histograms read the same rule.
        const auto keyWidths = qcp::algo::binWidths(bins->keyRange(), keySz, mBinnedKeyLog);
        const auto valueWidths = qcp::algo::binWidths(bins->valueRange(), valSz, mBinnedValueLog);
        double total = 0;
        for (int ki = 0; ki < keySz; ++ki)
            for (int vi = 0; vi < valSz; ++vi)
                total += bins->cell(ki, vi);
        double lo = std::numeric_limits<double>::max();
        double hi = std::numeric_limits<double>::lowest();
        for (int ki = 0; ki < keySz; ++ki)
        {
            double sum = 0;
            for (int vi = 0; vi < valSz; ++vi)
                sum += bins->cell(ki, vi);
            const bool columnShown = mColumnNormalized ? sum > 0 : true;
            for (int vi = 0; vi < valSz; ++vi)
            {
                const double count = bins->cell(ki, vi);
                const double area = keyWidths[std::size_t(ki)] * valueWidths[std::size_t(vi)];
                const double shownCell = mColumnNormalized
                    ? (sum > 0 ? count / sum : 0.0)
                    : qcp::algo::normalized(count, total, area, mNormalization);
                cells->setCell(ki, vi, shownCell);
                // An empty column shows as 0 but does not stretch the colour range.
                if (columnShown)
                {
                    lo = std::min(lo, shownCell);
                    hi = std::max(hi, shownCell);
                }
            }
        }
        cells->recalculateDataBounds();
        mShown.normalized = std::move(cells);
        mShown.bounds = rangeOrNothing(lo, hi, mShown.found);
        mShown.stale = false;
    }
    return mShown.normalized.get();
}

QCPRange QCPHistogram2D::colorDataBounds(bool& found) const
{
    found = false;
    const auto* cells = shown();
    if (!mDataSource || !cells)
        return {};
    if (!mColumnNormalized && mNormalization == QCP::HistogramNormalization::Count)
    {
        found = true;
        return cells->dataBounds();
    }
    found = mShown.found;
    return mShown.bounds;
}

bool QCPHistogram2D::canProduceContent() const
{
    if (!mKeyAxis || !mValueAxis || !mDataSource)
        return false;
    return mPipeline.result() != nullptr || mPipeline.isBusy();
}

bool QCPHistogram2D::binnedForTheAxisScales() const
{
    const bool keyLog = mKeyAxis && mKeyAxis->scaleType() == QCPAxis::stLogarithmic;
    const bool valueLog = mValueAxis && mValueAxis->scaleType() == QCPAxis::stLogarithmic;
    return keyLog == mBinnedKeyLog && valueLog == mBinnedValueLog;
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
    invalidateShown();
}

void QCPHistogram2D::draw(QCPDrawContext& context) const
{
    PROFILE_HERE_N("QCPHistogram2D::draw");
    if (!mKeyAxis || !mValueAxis)
        return;
    auto* self = const_cast<QCPHistogram2D*>(this);
    const bool staleScales = !binnedForTheAxisScales();
    if (staleScales)
        self->queueRefreshBinning();

    if (context.exporting() && (staleScales || !mPipeline.result() || mPipeline.isBusy()))
        self->binForExport();
    // Not re-binned from here when there is nothing: no result with an idle pipeline means the
    // data bins to nothing, and a re-bin would loop through finished() -> replot -> draw.
    const auto* cells = shown();
    if (!cells)
        return;

    const bool keyIsVertical = mKeyAxis->orientation() == Qt::Vertical;
    if (self->mRenderer.mapImageInvalidated() || self->mRenderer.keyIsVertical() != keyIsVertical)
        self->mRenderer.updateMapImage(cells, keyIsVertical);
    const QImage& image = mRenderer.mapImage();
    if (image.isNull())
        return;

    const QCPRange keyRange = cells->keyRange();
    const QCPRange valueRange = cells->valueRange();
    // Data corners, then the contours in data space: a pan moves both, a zoom stretches both.
    // A vertical key axis puts the key down the screen, so the picture takes its corners that
    // way round.
    const QCP::Data leftTop = keyIsVertical ? QCP::Data { keyRange.upper, valueRange.lower }
                                            : QCP::Data { keyRange.lower, valueRange.upper };
    const QCP::Data rightBottom = keyIsVertical ? QCP::Data { keyRange.lower, valueRange.upper }
                                                : QCP::Data { keyRange.upper, valueRange.lower };
    context.image(leftTop, rightBottom, image, {}, false);
    self->mContours.update(cells, context, dataScaleType(), mKeyAxis.data(), mValueAxis.data());
}

void QCPHistogram2D::drawLegendIcon(QCPDrawContext& icon) const
{
    // A gradient brush would be replayed in logical pixels, not fitted to the icon frame:
    // colourise a swatch and let the fitted frame place it.
    icon.image(QCP::Data { 0, 0 }, QCP::Data { 1, 1 }, const_cast<QCPColormapRenderer&>(mRenderer).gradientImage(32, 18));
}

void QCPHistogram2D::contoursChanged()
{
    redrawLater();
}

void QCPHistogram2D::setContourLevels(const QVector<double>& levels)
{
    markDirty();
    mContours.setLevels(levels);
    contoursChanged();
}

void QCPHistogram2D::setAutoContourLevels(int count)
{
    markDirty();
    mContours.setAutoLevels(count);
    contoursChanged();
}

void QCPHistogram2D::setContourPen(const QPen& pen)
{
    markDirty();
    mContours.setPen(pen);
    contoursChanged();
}

void QCPHistogram2D::setContourLabelEnabled(bool enabled)
{
    if (enabled == mContours.labelsEnabled())
        return;
    markDirty();
    mContours.setLabelsEnabled(enabled);
    contoursChanged();
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
    QCPAbstractColorMapped::rescaleDataRange(recalc);
}
