#include "plottable-colormap2.h"
#include <QImage>
#include <QPointer>
#include <algorithm>
#include <cmath>
#include <utility>
#include "plottable-colormap.h" // for QCPColorMapData
#include <core.h>
#include <painting/draw-context.h>
#include <layoutelements/layoutelement-axisrect.h>
#include <axis/axis.h>
#include <layer.h>
#include <datasource/resample.h>
#include <Profiling.hpp>
#include <cmath>

QCPColorMap2::QCPColorMap2(QCPAxis* keyAxis, QCPAxis* valueAxis)
    : QCPAbstractColorMapped(keyAxis, valueAxis, true)
    , mPipeline(parentPlot() ? parentPlot()->pipelineScheduler() : nullptr, this)
    , mRenderer(this)
{
    installResampleTransform();

    followAxes();

    connect(&mPipeline, &QCPColormapPipeline::finished,
            this, [this](uint64_t) {
                mContours.dataChanged();
                mRenderer.invalidateMapImage();
                markDirty(); // marking schedules the repaint
            });
    connect(&mPipeline, &QCPColormapPipeline::busyChanged,
            this, [this](bool) { updateEffectiveBusy(); });

    // A resize changes the axis rect's pixel size with no axis signal: the sharper image would
    // never be asked for (the recording is reused, so draw() is not called again on its own).
    if (QCustomPlot* plot = parentPlot())
        connect(plot, &QCustomPlot::afterLayout, this, [this]
        {
            if (needsAnImageOfTheNewSize())
                queueViewportChange();
        });

    followKeyAxisRectScale();
}

void QCPColorMap2::followAxes()
{
    for (const auto& link : std::exchange(mAxisLinks, {}))
        disconnect(link);
    const bool oneAxis = keyAxisRaw() == valueAxisRaw();
    for (QCPAxis* axis : {keyAxisRaw(), oneAxis ? nullptr : valueAxisRaw()})
    {
        if (!axis)
            continue;
        mAxisLinks << connect(axis, &QCPAxis::rangeChanged,
                              this, &QCPColorMap2::onViewportChanged);
        mAxisLinks << connect(axis, &QCPAxis::scaleTypeChanged,
                              this, &QCPColorMap2::onViewportChanged);
    }
    onViewportChanged(); // the resample was made for the old axes
}

// The transform runs on the scheduler's pool, possibly after this plottable is
// deleted (queued jobs are not joined with destruction) — it must be
// self-contained: settings are captured BY VALUE and re-baked when they change.
void QCPColorMap2::installResampleTransform()
{
    mPipeline.setTransform(TransformKind::ViewportDependent,
        [gapThreshold = mGapThreshold,
         zLog = mRenderer.dataScaleType() == QCPAxis::stLogarithmic](
            const QCPAbstractDataSource2D& src,
            const ViewportParams& vp,
            std::any& cache) -> std::shared_ptr<QCPColorMapData> {
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
                if (data.lower <= 0 || vp.lower <= 0)
                {
                    double vpSz = vp.size();
                    return vpSz > 0 ? data.size() / vpSz : 1.0;
                }
                double denom = std::log10(vp.upper) - std::log10(vp.lower);
                return denom > 0 ? (std::log10(data.upper) - std::log10(data.lower)) / denom : 1.0;
            };
            double vpKeySz = vp.keyRange.size();
            double xFrac = vpKeySz > 0 ? xOut.size() / vpKeySz : 1.0;
            // NOTE: the key (X) axis is always sampled linearly here, even
            // when vp.keyLogScale is set -- a log key axis is unsupported by
            // this layer (image and contours alike, not just contours).
            double vpValSz = vp.valueRange.size();
            double yFrac = vp.valueLogScale ? logFrac(yOut, vp.valueRange)
                                            : (vpValSz > 0 ? yOut.size() / vpValSz : 1.0);
            int pixW = std::clamp(static_cast<int>(vp.plotWidthPx * xFrac), 1, 32768);
            int pixH = std::clamp(static_cast<int>(vp.plotHeightPx * yFrac), 1, 32768);

            int visibleSrcCols = xEnd - xBegin;
            int w = std::clamp(visibleSrcCols, pixW, pixW * 4);
            int h = std::clamp(src.ySize(), pixH, pixH * 4);
            if (w <= 0 || h <= 0) return nullptr;

            if (!cache.has_value())
                cache = qcp::algo2d::ResampleCache{};
            auto& rc = std::any_cast<qcp::algo2d::ResampleCache&>(cache);
            auto* raw = qcp::algo2d::resample(src, xBegin, xEnd,
                xOut, yOut, w, h, vp.valueLogScale, zLog, gapThreshold, &rc);
            return std::shared_ptr<QCPColorMapData>(raw);
        });
}

void QCPColorMap2::setGapThreshold(double threshold)
{
    if (mGapThreshold == threshold)
        return;
    markDirty();
    mGapThreshold = threshold;
    installResampleTransform();
    // Re-resample so the new threshold shows now, not on the next pan. Without
    // a source there is no job to run and onDataChanged() would bump the
    // pipeline generation for nothing, leaving isBusy() stuck true.
    if (mDataSource)
        mPipeline.onDataChanged();
}

void QCPColorMap2::setDataSource(std::unique_ptr<QCPAbstractDataSource2D> source)
{
    markDirty();
    setDataSource(std::shared_ptr<QCPAbstractDataSource2D>(std::move(source)));
}

void QCPColorMap2::setDataSource(std::shared_ptr<QCPAbstractDataSource2D> source)
{
    mDataSource = std::move(source);
    // onViewportChanged() early-returns while mDataSource is null, so it never
    // updates the pipeline's cached viewport (mLastViewport) for any axis
    // change that happens before the first data arrives -- normal for a slow
    // producer (e.g. a remote channel with multi-second round trips), where
    // several pans can elapse with no data source yet. mPipeline.setSource()
    // below resamples against THAT stale (possibly still zero-initialized:
    // plotWidthPx=plotHeightPx=0) viewport, not the axes' actual current
    // state -- producing a degenerate ~1px resample (or nullptr, when the
    // zero-width default key/value range no longer intersects the data) that
    // nothing ever corrects once the user stops panning. Establish a real,
    // current viewport here, before the pipeline ever resamples this source.
    // The pipeline's busyChanged slot may delete this colour map.
    const QPointer<QCPColorMap2> alive(this);
    onViewportChanged();
    if (alive)
        mPipeline.setSource(mDataSource);
}

void QCPColorMap2::dataChanged()
{
    mPipeline.onDataChanged();
}

QCPRange QCPColorMap2::colorDataBounds(bool& found) const
{
    found = false;
    return mDataSource ? mDataSource->zRange(found) : QCPRange();
}

QCPRange QCPColorMap2::colorDataBounds(bool& found, bool recalc) const
{
    found = false;
    if (!mDataSource)
        return {};
    if (auto* data = mPipeline.result(); data && !recalc)
    {
        found = true;
        return data->dataBounds();
    }
    return mDataSource->zRange(found);
}

void QCPColorMap2::colorMappingChanged()
{
    const bool scaleTypeChanged = mRenderer.dataScaleType() != dataScaleType();
    mRenderer.setDataScaleType(dataScaleType());
    mRenderer.setDataRange(dataRange());
    mRenderer.setGradient(gradient());
    if (scaleTypeChanged)
    {
        // The resample transform captured the old colour scale type; a job may delete this map.
        const QPointer<QCPColorMap2> alive(this);
        installResampleTransform();
        if (mDataSource)
            mPipeline.onDataChanged();
        if (!alive)
            return;
    }
    markDirty();
}

// A job already running lands with finished(), which repaints and asks again if it was made for
// another size; asking while it runs would only supersede it.
bool QCPColorMap2::needsAnImageOfTheNewSize() const
{
    if (!mPipeline.result() || mPipeline.isBusy())
        return false;
    const auto now = ViewportParams::fromAxes(keyAxisRaw(), valueAxisRaw());
    const auto made = mPipeline.resultViewport();
    return now.plotWidthPx != made.plotWidthPx || now.plotHeightPx != made.plotHeightPx;
}

void QCPColorMap2::onViewportChanged()
{
    if (!keyAxisRaw() || !valueAxisRaw() || !mDataSource) return;
    auto* axisRect = axisRectOf(keyAxisRaw());
    if (!axisRect) return;

    mPipeline.onViewportChanged(ViewportParams::fromAxes(keyAxisRaw(), valueAxisRaw()));
}

// A viewport change turns the pipeline busy at once, and nothing may emit during a paint: a
// busyChanged slot may delete this colour map.
void QCPColorMap2::queueViewportChange()
{
    if (std::exchange(mViewportChangeQueued, true))
        return;
    QMetaObject::invokeMethod(this, [this] {
        mViewportChangeQueued = false;
        onViewportChanged();
    }, Qt::QueuedConnection);
}

bool QCPColorMap2::canProduceContent() const
{
    if (!keyAxisRaw() || !valueAxisRaw() || !mDataSource)
        return false;
    return mPipeline.result() != nullptr;
}

void QCPColorMap2::draw(QCPDrawContext& context) const
{
    PROFILE_HERE_N("QCPColorMap2::draw");
    if (!keyAxisRaw() || !valueAxisRaw())
        return;

    auto* self = const_cast<QCPColorMap2*>(this);
    const QCPColorMapData* resampledData = mPipeline.result();
    const auto now = ViewportParams::fromAxes(keyAxisRaw(), valueAxisRaw());
    // An export draws an image made for its own viewport: the job asked for it may still run.
    if (resampledData && context.exporting() && mPipeline.resultViewport() != now)
    {
        if (self->mPipeline.runSynchronously(now))
        {
            // finished() only fires for async results
            self->mRenderer.invalidateMapImage();
            self->mContours.dataChanged();
        }
        resampledData = self->mPipeline.result();
    }
    // A colormap made before its plot was laid out was resampled for a tiny axis rect, and a
    // resize changes no axis range: nothing else would ask for a sharper image.
    else if (self->needsAnImageOfTheNewSize())
        self->queueViewportChange();
    if (!resampledData)
    {
        if (!mDataSource) return;
        if (context.exporting())
        {
            // Export path: run transform synchronously since the event loop is not pumped
            if (!self->mPipeline.runSynchronously(ViewportParams::fromAxes(keyAxisRaw(), valueAxisRaw())))
                return;
            self->mContours.dataChanged();
            resampledData = self->mPipeline.result();
            if (!resampledData) return;
        }
        else
        {
            self->queueViewportChange();
            return;
        }
    }

    const bool keyIsVertical = keyAxisRaw()->orientation() == Qt::Vertical;
    if (self->mRenderer.mapImageInvalidated() || self->mRenderer.keyIsVertical() != keyIsVertical)
        self->mRenderer.updateMapImage(resampledData, keyIsVertical);
    const QImage& image = mRenderer.mapImage();
    if (image.isNull())
        return;

    const QCPRange keyRange = resampledData->keyRange();
    const QCPRange valRange = resampledData->valueRange();

    // The grid carries its own data corners, so a pan moves the texture and a zoom stretches it:
    // the picture on a linear and on a log value axis alike. A vertical key axis puts the key
    // down the screen, so the transposed picture takes its corners with the key on y.
    const QCP::Data leftTop = keyIsVertical ? QCP::Data { keyRange.upper, valRange.lower }
                                            : QCP::Data { keyRange.lower, valRange.upper };
    const QCP::Data rightBottom = keyIsVertical ? QCP::Data { keyRange.lower, valRange.upper }
                                                : QCP::Data { keyRange.upper, valRange.lower };
    context.image(leftTop, rightBottom, image, {}, false);
    self->mContours.update(resampledData, context, dataScaleType(), keyAxisRaw(), valueAxisRaw());
}

void QCPColorMap2::drawLegendIcon(QCPDrawContext& icon) const
{
    // A gradient brush would be replayed in logical pixels, not fitted to the icon frame:
    // colourise a swatch and let the fitted frame place it.
    icon.image(QCP::Data { 0, 0 }, QCP::Data { 1, 1 }, const_cast<QCPColormapRenderer&>(mRenderer).gradientImage(32, 18));
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

double QCPColorMap2::selectTest(const QPointF& pos, bool onlySelectable, QVariant* details) const
{
    if (onlySelectable && !mSelectable)
        return -1;
    if (!keyAxisRaw() || !valueAxisRaw() || !mDataSource)
        return -1;

    if (!axisRectOf(keyAxisRaw())->rect().contains(pos.toPoint())
        && !mParentPlot->interactions().testFlag(QCP::iSelectPlottablesBeyondAxisRect))
        return -1;

    double key, value;
    pixelsToCoords(pos, key, value);

    bool foundKey = false, foundValue = false;
    auto kr = mDataSource->xRange(foundKey);
    auto vr = mDataSource->yRange(foundValue);

    if (foundKey && foundValue && kr.contains(key) && vr.contains(value))
    {
        if (details)
            details->setValue(QCPDataSelection(QCPDataRange(0, 1)));
        return mParentPlot->selectionTolerance() * 0.99;
    }
    return -1;
}

void QCPColorMap2::contoursChanged()
{
    markDirty();
}

void QCPColorMap2::setContourLevels(const QVector<double>& levels)
{
    markDirty();
    mContours.setLevels(levels);
    contoursChanged();
}

void QCPColorMap2::setContourPen(const QPen& pen)
{
    markDirty();
    mContours.setPen(pen);
    contoursChanged();
}

void QCPColorMap2::setContourLabelEnabled(bool enabled)
{
    if (enabled == mContours.labelsEnabled())
        return;
    markDirty();
    mContours.setLabelsEnabled(enabled);
    contoursChanged();
}

void QCPColorMap2::setAutoContourLevels(int count)
{
    markDirty();
    mContours.setAutoLevels(count);
    contoursChanged();
}

void QCPColorMap2::rescaleDataRange(bool recalc)
{
    bool found = false;
    const QCPRange range = colorDataBounds(found, recalc);
    if (found && range.lower < range.upper)
        setDataRange(range);
}
