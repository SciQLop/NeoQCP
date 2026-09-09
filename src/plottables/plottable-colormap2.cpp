#include "plottable-colormap2.h"
#include "plottable-colormap.h" // for QCPColorMapData
#include <core.h>
#include <painting/painter.h>
#include <painting/contour-extractor.h> // kept for QPainter export fallback
#include <layoutelements/layoutelement-colorscale.h>
#include <layoutelements/layoutelement-axisrect.h>
#include <axis/axis.h>
#include <layer.h>
#include <datasource/resample.h>
#include <painting/viewport-offset.h>
#include <Profiling.hpp>
#include <cmath>

QCPColorMap2::QCPColorMap2(QCPAxis* keyAxis, QCPAxis* valueAxis)
    : QCPAbstractPlottable(keyAxis, valueAxis)
    , mPipeline(parentPlot() ? parentPlot()->pipelineScheduler() : nullptr, this)
    , mRenderer(this)
{
    installResampleTransform();

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
                ++mContourDataGen;
                mRenderer.invalidateMapImage();
                if (parentPlot())
                    parentPlot()->replot(QCustomPlot::rpQueuedReplot);
            });
    connect(&mPipeline, &QCPColormapPipeline::busyChanged,
            this, [this](bool) { updateEffectiveBusy(); });
}

QCPColorMap2::~QCPColorMap2()
{
    mRenderer.releaseRhiLayer();
}

// The transform runs on the scheduler's pool, possibly after this plottable is
// deleted (queued jobs are not joined with destruction) — it must be
// self-contained: settings are captured BY VALUE and re-baked when they change.
void QCPColorMap2::installResampleTransform()
{
    mPipeline.setTransform(TransformKind::ViewportDependent,
        [gapThreshold = mGapThreshold](
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
                xOut, yOut, w, h, vp.valueLogScale, gapThreshold, &rc);
            return std::shared_ptr<QCPColorMapData>(raw);
        });
}

void QCPColorMap2::setGapThreshold(double threshold)
{
    if (mGapThreshold == threshold)
        return;
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
    onViewportChanged();
    mPipeline.setSource(mDataSource);
}

void QCPColorMap2::dataChanged()
{
    mPipeline.onDataChanged();
}

void QCPColorMap2::setGradient(const QCPColorGradient& gradient)
{
    if (mRenderer.gradient() != gradient)
    {
        mRenderer.setGradient(gradient);
        Q_EMIT gradientChanged(mRenderer.gradient());
        if (mLayer)
            mLayer->markDirty();
        if (mParentPlot)
            mParentPlot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void QCPColorMap2::setColorScale(QCPColorScale* colorScale)
{
    if (mRenderer.colorScale())
    {
        disconnect(this, &QCPColorMap2::dataRangeChanged, mRenderer.colorScale(), &QCPColorScale::setDataRange);
        disconnect(this, &QCPColorMap2::gradientChanged, mRenderer.colorScale(), &QCPColorScale::setGradient);
        disconnect(this, &QCPColorMap2::dataScaleTypeChanged, mRenderer.colorScale(), &QCPColorScale::setDataScaleType);
        disconnect(mRenderer.colorScale(), &QCPColorScale::dataRangeChanged, this, &QCPColorMap2::setDataRange);
        disconnect(mRenderer.colorScale(), &QCPColorScale::gradientChanged, this, &QCPColorMap2::setGradient);
        disconnect(mRenderer.colorScale(), &QCPColorScale::dataScaleTypeChanged, this, &QCPColorMap2::setDataScaleType);
    }
    mRenderer.setColorScale(colorScale);
    if (colorScale)
    {
        setGradient(colorScale->gradient());
        setDataScaleType(colorScale->dataScaleType());
        setDataRange(colorScale->dataRange());
        connect(this, &QCPColorMap2::dataRangeChanged, colorScale, &QCPColorScale::setDataRange);
        connect(this, &QCPColorMap2::gradientChanged, colorScale, &QCPColorScale::setGradient);
        connect(this, &QCPColorMap2::dataScaleTypeChanged, colorScale, &QCPColorScale::setDataScaleType);
        connect(colorScale, &QCPColorScale::dataRangeChanged, this, &QCPColorMap2::setDataRange);
        connect(colorScale, &QCPColorScale::gradientChanged, this, &QCPColorMap2::setGradient);
        connect(colorScale, &QCPColorScale::dataScaleTypeChanged, this, &QCPColorMap2::setDataScaleType);
    }
}

void QCPColorMap2::setDataRange(const QCPRange& range)
{
    QCPRange prev = mRenderer.dataRange();
    mRenderer.setDataRange(range);
    QCPRange cur = mRenderer.dataRange();
    if (prev.lower != cur.lower || prev.upper != cur.upper)
    {
        Q_EMIT dataRangeChanged(cur);
        if (mLayer)
            mLayer->markDirty();
        if (mParentPlot)
            mParentPlot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void QCPColorMap2::setDataScaleType(QCPAxis::ScaleType type)
{
    if (mRenderer.dataScaleType() != type)
    {
        QCPRange prevRange = mRenderer.dataRange();
        mRenderer.setDataScaleType(type);
        QCPRange curRange = mRenderer.dataRange();
        if (prevRange.lower != curRange.lower || prevRange.upper != curRange.upper)
            Q_EMIT dataRangeChanged(curRange);
        Q_EMIT dataScaleTypeChanged(type);
        if (mLayer)
            mLayer->markDirty();
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

    mPipeline.onViewportChanged(ViewportParams::fromAxes(mKeyAxis.data(), mValueAxis.data()));

    // Dirty the layer on every viewport change so a pan is handled even when it
    // arrives via set_range (axis sync) rather than the interactive drag path
    // (which already calls markAffectedLayersDirty). stallPixelOffset() then lets
    // the compositor translate the existing texture instead of repainting it.
    if (mLayer)
        mLayer->markDirty();
}

QPointF QCPColorMap2::stallPixelOffset() const
{
    // Only valid when there is a rendered image to shift and no fresher one is
    // pending (a finished resample / data / gradient change must redraw, not
    // translate the stale texture).
    if (!mHasRenderedRange || !mKeyAxis || !mValueAxis
        || mRenderer.mapImage().isNull() || mRenderer.mapImageInvalidated())
        return {};

    // Pure translation only: reject genuine zoom. Scale-aware so a pure pan on a
    // log axis (which changes the linear range size) is not misread as a zoom.
    if (qAbs(qcp::axisRangeSizeRatio(mKeyAxis.data(), mRenderedKeyRange) - 1.0) > 1e-4
        || qAbs(qcp::axisRangeSizeRatio(mValueAxis.data(), mRenderedValueRange) - 1.0) > 1e-4)
        return {};

    const QPointF offset = qcp::computeViewportOffset(
        mKeyAxis.data(), mValueAxis.data(), mRenderedKeyRange, mRenderedValueRange);

    // Reject if panned beyond the axis rect (texture no longer covers viewport).
    const bool keyVert = mKeyAxis->orientation() == Qt::Vertical;
    const double keyDim = keyVert ? mKeyAxis->axisRect()->height() : mKeyAxis->axisRect()->width();
    const double valDim = keyVert ? mKeyAxis->axisRect()->width() : mKeyAxis->axisRect()->height();
    if (qAbs(keyVert ? offset.y() : offset.x()) > keyDim
        || qAbs(keyVert ? offset.x() : offset.y()) > valDim)
        return {};

    return offset;
}

bool QCPColorMap2::canProduceContent() const
{
    if (!mKeyAxis || !mValueAxis || !mDataSource)
        return false;
    return mPipeline.result() != nullptr;
}

void QCPColorMap2::draw(QCPPainter* painter)
{
    PROFILE_HERE_N("QCPColorMap2::draw");
    if (!mKeyAxis || !mValueAxis)
        return;

    auto* resampledData = mPipeline.result();
    if (!resampledData)
    {
        if (!mDataSource) return;
        if (painter->modes().testFlag(QCPPainter::pmNoCaching))
        {
            // Export path: run transform synchronously since event loop is not pumped
            if (!mPipeline.runSynchronously(ViewportParams::fromAxes(mKeyAxis.data(), mValueAxis.data())))
                return;
            resampledData = mPipeline.result();
            if (!resampledData) return;
        }
        else
        {
            onViewportChanged();
            return;
        }
    }

    // The pipeline can still be holding a result computed for an older
    // viewport (e.g. the user panned more than a screen-width while a slow
    // resample was in flight, and stallPixelOffset() rejected the translate
    // fast path as out-of-bounds -- see plottable-colormap2.h). Drawing that
    // stale image would position it via the CURRENT axes using its OLD
    // key/value range, which can land entirely outside the axis rect (a
    // blank-looking plot) and would also corrupt the stallPixelOffset()
    // baseline below. The colormap's GPU quad (QCPColormapRhiLayer) only
    // ever updates from the mRenderer.draw() call below, so skipping it
    // without also hiding the quad would freeze the LAST relevant frame on
    // screen indefinitely -- it does not fade or reposition on its own as
    // the axes keep moving. Hide it instead: no data belongs there anymore.
    if (mHasRenderedRange)
    {
        const QCPRange dataKeyRange = resampledData->keyRange();
        const QCPRange dataValueRange = resampledData->valueRange();
        auto overlaps = [](const QCPRange& a, const QCPRange& b)
        { return a.lower <= b.upper && b.lower <= a.upper; };
        if (!overlaps(dataKeyRange, mKeyAxis->range())
            || !overlaps(dataValueRange, mValueAxis->range()))
        {
            mRenderer.clearRhiContent();
            return;
        }
    }

    bool imageWasInvalidated = mRenderer.mapImageInvalidated();
    if (imageWasInvalidated)
        mRenderer.updateMapImage(resampledData);

    if (mRenderer.mapImage().isNull())
        return;

    QCPRange keyRange = resampledData->keyRange();
    QCPRange valRange = resampledData->valueRange();

    // Contours are (re)built BEFORE the renderer paints: the QPainter path
    // (exports, non-RHI compositing) draws the fallback lines inline, so
    // populating them after mRenderer.draw() would miss this frame.
    updateContours(resampledData, painter, imageWasInvalidated);

    applyDefaultAntialiasingHint(painter);
    mRenderer.draw(painter, mKeyAxis.data(), mValueAxis.data(), keyRange, valRange);

    // Baseline for stallPixelOffset: the axes this image was just drawn against.
    mRenderedKeyRange = mKeyAxis->range();
    mRenderedValueRange = mValueAxis->range();
    mHasRenderedRange = true;
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

double QCPColorMap2::selectTest(const QPointF& pos, bool onlySelectable, QVariant* details) const
{
    if (onlySelectable && !mSelectable)
        return -1;
    if (!mKeyAxis || !mValueAxis || !mDataSource)
        return -1;

    if (!mKeyAxis.data()->axisRect()->rect().contains(pos.toPoint())
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

void QCPColorMap2::setContourLevels(const QVector<double>& levels)
{
    mContourLevels = levels;
    mAutoContourCount = 0;
    invalidateContourCache();
    if (mLayer)
        mLayer->markDirty();
    if (mParentPlot)
        mParentPlot->replot(QCustomPlot::rpQueuedReplot);
}

void QCPColorMap2::setContourPen(const QPen& pen)
{
    mContourPen = pen;
    invalidateContourCache();
    if (mLayer)
        mLayer->markDirty();
    if (mParentPlot)
        mParentPlot->replot(QCustomPlot::rpQueuedReplot);
}

void QCPColorMap2::setContourLabelEnabled(bool enabled)
{
    mContourLabelEnabled = enabled;
}

void QCPColorMap2::setAutoContourLevels(int count)
{
    mAutoContourCount = qMax(0, count);
    mContourLevels.clear();
    invalidateContourCache();
    if (mLayer)
        mLayer->markDirty();
    if (mParentPlot)
        mParentPlot->replot(QCustomPlot::rpQueuedReplot);
}

void QCPColorMap2::invalidateContourCache()
{
    mContourCacheGen = 0;
    mFallbackGen = 0;
    mContourDataGen = std::max(mContourDataGen, uint64_t(1));
}

void QCPColorMap2::updateContourGpu(const QCPColorMapData* data)
{
    PROFILE_HERE_N("QCPColorMap2::updateContourGpu");
    if (!data)
        return;

    const int kSize = data->keySize();
    const int vSize = data->valueSize();
    if (kSize < 2 || vSize < 2)
    {
        clearContourState();
        return;
    }

    QCPRange bounds = data->dataBounds();
    if (!(bounds.lower < bounds.upper) || !std::isfinite(bounds.lower)
        || !std::isfinite(bounds.upper))
    {
        clearContourState();
        return;
    }

    QCPRange keyRange = data->keyRange();
    QCPRange valRange = data->valueRange();
    const double kSpan = keyRange.upper - keyRange.lower;
    const double vSpan = valRange.upper - valRange.lower;
    if (!(kSpan > 0) || !(vSpan > 0) || !std::isfinite(kSpan) || !std::isfinite(vSpan))
    {
        clearContourState();
        return;
    }

    QVector<double> levels = resolveContourLevels(bounds);
    mLastContourLevels = levels;
    QVector<float> uvVerts = QCPContourExtractor::extractUv(data, levels);

    // Mirror the UVs the same way the renderer flips the image when an axis
    // is range-reversed; otherwise the overlay reflects against the image.
    const bool mirrorX = mKeyAxis && mKeyAxis->rangeReversed();
    const bool mirrorY = mValueAxis && mValueAxis->rangeReversed();
    if (mirrorX)
        for (qsizetype i = 0; i < uvVerts.size(); i += 2)
            uvVerts[i] = 1.0f - uvVerts[i];
    if (mirrorY)
        for (qsizetype i = 1; i < uvVerts.size(); i += 2)
            uvVerts[i] = 1.0f - uvVerts[i];
    mContourMirrorX = mirrorX;
    mContourMirrorY = mirrorY;

    mLastContourUv = uvVerts;
    mRenderer.setContourLines(std::move(uvVerts), mContourPen.color());
    // Stamp the cache only once the layer holds the matching content, so a
    // degenerate rebuild can never leave stale lines behind.
    mContourCacheGen = mContourDataGen;
}

void QCPColorMap2::updateContours(const QCPColorMapData* data, QCPPainter* painter,
                                  bool imageWasInvalidated)
{
    const bool contoursActive = !mContourLevels.isEmpty() || mAutoContourCount > 0;
    if (!contoursActive)
    {
        clearContourState();
        return;
    }
    // setRangeReversed() emits no signal, so poll here: a reversal flips the
    // rendered image (see QCPColormapRenderer::draw) and the cached contour
    // UVs must be mirrored to match.
    const bool mirrorX = mKeyAxis && mKeyAxis->rangeReversed();
    const bool mirrorY = mValueAxis && mValueAxis->rangeReversed();
    if (imageWasInvalidated || mContourCacheGen != mContourDataGen
        || mirrorX != mContourMirrorX || mirrorY != mContourMirrorY)
        updateContourGpu(data);
    // The QPainter path cannot use GPU lines: it needs data-coordinate
    // fallback lines whenever it paints -- i.e. when nothing was uploaded
    // (no RHI layer) or when exporting (pmNoCaching bypasses the RHI layer
    // even if one holds lines). Generation-tracked so repeated exports of
    // the same frame reuse the cached segments instead of re-marching the
    // full grid on every paint pass.
    const bool exportMode = painter && painter->modes().testFlag(QCPPainter::pmNoCaching);
    const bool fallbackCurrent = mFallbackGen == mContourDataGen && mFallbackGen != 0;
    if ((!mRenderer.hasContourOnGpu() || exportMode) && !fallbackCurrent)
        updateContourFallback(data);
}

void QCPColorMap2::updateContourFallback(const QCPColorMapData* data)
{
    if (!data)
        return;
    // Degenerate input cannot yield segments; stamp the generation so
    // exporters don't re-march it on every paint pass.
    QCPRange bounds = data->dataBounds();
    if (!(bounds.lower < bounds.upper) || !std::isfinite(bounds.lower)
        || !std::isfinite(bounds.upper))
    {
        mFallbackGen = mContourDataGen;
        return;
    }
    QVector<double> levels = resolveContourLevels(bounds);
    QVector<QLineF> segments;
    for (const auto& line : QCPContourExtractor::extract(data, levels))
        segments.append(line.segments);
    mRenderer.setContourFallback(std::move(segments), mContourPen);
    mFallbackGen = mContourDataGen;
    ++mFallbackBuildCount;
}

void QCPColorMap2::clearContourState()
{
    mRenderer.clearContour();
    mLastContourUv.clear();
    mLastContourLevels.clear();
    mContourCacheGen = mContourDataGen;
    mFallbackGen = 0;
}

QVector<double> QCPColorMap2::resolveContourLevels(const QCPRange& bounds) const
{
    QVector<double> levels;
    if (!mContourLevels.isEmpty())
    {
        levels.reserve(mContourLevels.size());
        for (double level : mContourLevels)
        {
            if (std::isfinite(level))
                levels.append(level);
        }
    }
    else if (mAutoContourCount > 0)
    {
        levels.reserve(mAutoContourCount);
        if (mRenderer.dataScaleType() == QCPAxis::stLogarithmic && bounds.lower > 0
            && bounds.upper > 0)
        {
            // Geometric spacing: equal ratios put one line per decade band
            // instead of bunching every line at the top decade.
            const double ratio =
                std::pow(bounds.upper / bounds.lower, 1.0 / (mAutoContourCount + 1));
            for (int i = 1; i <= mAutoContourCount; ++i)
                levels.append(bounds.lower * std::pow(ratio, i));
        }
        else
        {
            const double step = (bounds.upper - bounds.lower) / (mAutoContourCount + 1);
            for (int i = 1; i <= mAutoContourCount; ++i)
                levels.append(bounds.lower + i * step);
        }
    }
    return levels;
}
