#include "plottable-graph2.h"
#include "plottable-draw-utils.h"
#include "plottable-l1-cache.h"
#include "plottable-linestyle.h"
#include "Profiling.hpp"
#include "../datasource/graph-resampler.h"

#include "../axis/axis.h"
#include "../core.h"
#include "../layoutelements/layoutelement-axisrect.h"
#include "../painting/painter.h"
#include "../painting/viewport-offset.h"
#include "../painting/scatter-rhi-layer.h"
#include "../vector2d.h"

QCPGraph2::QCPGraph2(QCPAxis* keyAxis, QCPAxis* valueAxis)
    : QCPAbstractPlottable(keyAxis, valueAxis)
    , mPipeline(parentPlot() ? parentPlot()->pipelineScheduler() : nullptr, this)
{
    setPen(QPen(Qt::blue, 0));
    setBrush(Qt::NoBrush);

    if (keyAxis)
    {
        connect(keyAxis, QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
                this, &QCPGraph2::onViewportChanged);
        connect(keyAxis, &QCPAxis::scaleTypeChanged,
                this, [this] { mLineCacheDirty = true; mCachedLines.clear(); });
    }
    if (valueAxis)
    {
        connect(valueAxis, &QCPAxis::scaleTypeChanged,
                this, [this] { mLineCacheDirty = true; mCachedLines.clear(); });
    }

    connect(&mPipeline, &QCPGraphPipeline::finished,
            this, [this](uint64_t) { onL1Ready(); });
    connect(&mPipeline, &QCPGraphPipeline::busyChanged,
            this, [this](bool) { updateEffectiveBusy(); });

    mViewportDebounce.setSingleShot(true);
    mViewportDebounce.setInterval(150);
    connect(&mViewportDebounce, &QTimer::timeout, this, [this] {
        mLineCacheDirty = true;
        if (mParentPlot)
            mParentPlot->replot(QCustomPlot::rpQueuedReplot);
    });
}

QCPGraph2::~QCPGraph2() = default;

static void ensureL1Transform(QCPGraphPipeline& pipeline, int sourceSize)
{
    if (sourceSize >= qcp::algo::kResampleThreshold)
    {
        if (!pipeline.hasTransform())
        {
            // Pipeline only builds L1 cache — L2 is done synchronously
            pipeline.setTransform(TransformKind::ViewportIndependent,
                [](const QCPAbstractDataSource& src,
                   const ViewportParams& vp,
                   std::any& cache) -> std::shared_ptr<QCPAbstractDataSource> {
                    return qcp::algo::buildL1Cache(src, vp, cache);
                });
        }
    }
    else if (pipeline.hasTransform())
    {
        pipeline.clearTransform();
    }
}

void QCPGraph2::setDataSource(std::unique_ptr<QCPAbstractDataSource> source)
{
    setDataSource(std::shared_ptr<QCPAbstractDataSource>(std::move(source)));
}

void QCPGraph2::setDataSource(std::shared_ptr<QCPAbstractDataSource> source)
{
    mDataSource = std::move(source);
    mL1Cache.reset();
    mL2Result.reset();
    mCachedLines.clear();
    mLineCacheDirty = true;
    mL2Dirty = false;
    mNeedsResampling = mDataSource && mDataSource->size() >= qcp::algo::kResampleThreshold;
    if (mDataSource)
        ensureL1Transform(mPipeline, mDataSource->size());
    mPipeline.setSource(mDataSource);
}

void QCPGraph2::dataChanged()
{
    mLineCacheDirty = true;

    bool wasResampling = mNeedsResampling;
    mNeedsResampling = mDataSource && mDataSource->size() >= qcp::algo::kResampleThreshold;

    // Update the L1 transform when crossing the resampling threshold in either direction
    if (mNeedsResampling != wasResampling)
    {
        if (mDataSource)
            ensureL1Transform(mPipeline, mDataSource->size());
        else if (mPipeline.hasTransform())
            mPipeline.clearTransform();
    }

    if (mPipeline.hasTransform())
    {
        mL1Cache.reset();
        mL2Dirty = false;
        mPipeline.onDataChanged();
    }
    else if (mParentPlot)
        mParentPlot->replot();
}

void QCPGraph2::onL1Ready()
{
    PROFILE_HERE_N("QCPGraph2::onL1Ready");
    qcp::extractL1Cache<qcp::algo::GraphResamplerCache>(mPipeline.cache(), mL1Cache, mL2Dirty);
    mLineCacheDirty = true;
    if (parentPlot())
        parentPlot()->replot(QCustomPlot::rpQueuedReplot);
}

void QCPGraph2::rebuildL2(const ViewportParams& vp)
{
    PROFILE_HERE_N("QCPGraph2::rebuildL2");
    if (!mL1Cache) return;
    if (vp.keyLogScale)
    {
        mL2Result.reset(); // L2 not supported for log scale — fall back to raw data
        return;
    }
    mL2Result = qcp::algo::resampleL2(*mL1Cache, vp);
}

// --- QCPPlottableInterface1D ---

int QCPGraph2::dataCount() const
{
    return mDataSource ? mDataSource->size() : 0;
}

double QCPGraph2::dataMainKey(int index) const
{
    return mDataSource ? mDataSource->keyAt(index) : 0.0;
}

double QCPGraph2::dataSortKey(int index) const
{
    return mDataSource ? mDataSource->keyAt(index) : 0.0;
}

double QCPGraph2::dataMainValue(int index) const
{
    return mDataSource ? mDataSource->valueAt(index) : 0.0;
}

QCPRange QCPGraph2::dataValueRange(int index) const
{
    if (!mDataSource)
        return QCPRange(0, 0);
    double v = mDataSource->valueAt(index);
    return QCPRange(v, v);
}

QPointF QCPGraph2::dataPixelPosition(int index) const
{
    if (!mDataSource || !mKeyAxis || !mValueAxis)
        return {};
    return coordsToPixels(mDataSource->keyAt(index), mDataSource->valueAt(index));
}

int QCPGraph2::findBegin(double sortKey, bool expandedRange) const
{
    return mDataSource ? mDataSource->findBegin(sortKey, expandedRange) : 0;
}

int QCPGraph2::findEnd(double sortKey, bool expandedRange) const
{
    return mDataSource ? mDataSource->findEnd(sortKey, expandedRange) : 0;
}

QCPDataSelection QCPGraph2::selectTestRect(const QRectF& rect, bool onlySelectable) const
{
    QCPDataSelection result;
    if ((onlySelectable && mSelectable == QCP::stNone) || !mDataSource || mDataSource->empty())
        return result;
    if (!mKeyAxis || !mValueAxis)
        return result;

    double key1, value1, key2, value2;
    pixelsToCoords(rect.topLeft(), key1, value1);
    pixelsToCoords(rect.bottomRight(), key2, value2);
    if (key1 > key2) qSwap(key1, key2);
    if (value1 > value2) qSwap(value1, value2);
    QCPRange keyRange(key1, key2);
    QCPRange valueRange(value1, value2);

    int begin = mDataSource->findBegin(keyRange.lower, false);
    int end = mDataSource->findEnd(keyRange.upper, false);

    int currentSegmentBegin = -1;
    for (int i = begin; i < end; ++i)
    {
        double k = mDataSource->keyAt(i);
        double v = mDataSource->valueAt(i);
        if (currentSegmentBegin == -1)
        {
            if (keyRange.contains(k) && valueRange.contains(v))
                currentSegmentBegin = i;
        }
        else if (!keyRange.contains(k) || !valueRange.contains(v))
        {
            result.addDataRange(QCPDataRange(currentSegmentBegin, i), false);
            currentSegmentBegin = -1;
        }
    }
    if (currentSegmentBegin != -1)
        result.addDataRange(QCPDataRange(currentSegmentBegin, end), false);

    result.simplify();
    return result;
}

// --- QCPAbstractPlottable ---

double QCPGraph2::selectTest(const QPointF& pos, bool onlySelectable, QVariant* details) const
{
    if ((onlySelectable && mSelectable == QCP::stNone) || !mDataSource || mDataSource->empty())
        return -1;
    if (!mKeyAxis || !mValueAxis)
        return -1;

    auto* axisRect = mKeyAxis->axisRect();
    if (!axisRect || !axisRect->rect().contains(pos.toPoint()))
        return -1;

    // Fast path: when details aren't needed (e.g. wheel events), skip per-point work.
    if (!details)
        return mParentPlot->selectionTolerance() * 0.99;

    // Keys are sorted — binary search for the nearest key, then check
    // at most 2 neighbors.  O(log N) instead of O(M).
    const QCPAbstractDataSource* ds = mDataSource.get();
    const int n = ds->size();

    double posKey, dummy;
    pixelsToCoords(pos, posKey, dummy);

    int idx = ds->findEnd(posKey, /*expandedRange=*/false);
    int lo = qMax(0, idx - 1);
    int hi = qMin(idx, n - 1);

    double minDistSqr = (std::numeric_limits<double>::max)();
    int minDistIndex = -1;

    for (int i = lo; i <= hi; ++i)
    {
        double k = ds->keyAt(i);
        double v = ds->valueAt(i);
        double distSqr = QCPVector2D(coordsToPixels(k, v) - pos).lengthSquared();
        if (distSqr < minDistSqr)
        {
            minDistSqr = distSqr;
            minDistIndex = i;
        }
    }

    // Also check distance to the line segment between lo and hi
    if (mLineStyle != lsNone && lo < hi)
    {
        QPointF pLo = coordsToPixels(ds->keyAt(lo), ds->valueAt(lo));
        QPointF pHi = coordsToPixels(ds->keyAt(hi), ds->valueAt(hi));
        double lineDistSqr = QCPVector2D(pos).distanceSquaredToLine(pLo, pHi);
        if (lineDistSqr < minDistSqr)
            minDistSqr = lineDistSqr;
    }

    QCPDataSelection selectionResult;
    if (minDistIndex >= 0)
        selectionResult.addDataRange(QCPDataRange(minDistIndex, minDistIndex + 1), false);
    selectionResult.simplify();
    details->setValue(selectionResult);
    return minDistIndex >= 0 ? qSqrt(minDistSqr) : -1;
}

QCPRange QCPGraph2::getKeyRange(bool& foundRange, QCP::SignDomain inSignDomain) const
{
    PROFILE_HERE_N("QCPGraph2::getKeyRange");
    if (!mDataSource || mDataSource->empty())
    {
        foundRange = false;
        return {};
    }
    return mDataSource->keyRange(foundRange, inSignDomain);
}

QCPRange QCPGraph2::getValueRange(bool& foundRange, QCP::SignDomain inSignDomain,
                                   const QCPRange& inKeyRange) const
{
    PROFILE_HERE_N("QCPGraph2::getValueRange");
    if (!mDataSource || mDataSource->empty())
    {
        foundRange = false;
        return {};
    }
    return mDataSource->valueRange(foundRange, inSignDomain, inKeyRange);
}

// --- Drawing ---

bool QCPGraph2::canProduceContent() const
{
    if (!mKeyAxis || !mValueAxis || !mDataSource || mDataSource->empty())
        return false;
    if (mNeedsResampling && !mL1Cache && !mL2Result)
        return false;
    return true;
}

QPointF QCPGraph2::stallPixelOffset() const
{
    if (!mHasRenderedRange || mCachedLines.isEmpty() || !mKeyAxis || !mValueAxis)
        return {};
    // Only valid for pure translation (pan) — reject if zoom changed
    double keyRatio = mKeyAxis->range().size() / mRenderedRange.key.size();
    double valRatio = mValueAxis->range().size() / mRenderedRange.value.size();
    if (qAbs(keyRatio - 1.0) > 1e-4 || qAbs(valRatio - 1.0) > 1e-4)
        return {};
    QPointF offset = qcp::computeViewportOffset(mKeyAxis.data(), mValueAxis.data(),
                                                mRenderedRange.key, mRenderedRange.value);
    // Reject if panned too far (cached lines don't cover the viewport)
    const bool keyVert = mKeyAxis->orientation() == Qt::Vertical;
    double keyDim = keyVert ? mKeyAxis->axisRect()->height() : mKeyAxis->axisRect()->width();
    double valDim = keyVert ? mKeyAxis->axisRect()->width() : mKeyAxis->axisRect()->height();
    if (qAbs(keyVert ? offset.y() : offset.x()) > keyDim
        || qAbs(keyVert ? offset.x() : offset.y()) > valDim)
        return {};
    return offset;
}

void QCPGraph2::draw(QCPPainter* painter)
{
    PROFILE_HERE_N("QCPGraph2::draw");
    if (!mKeyAxis || !mValueAxis || !mDataSource)
        return;

    // Export path: synchronous fallback when no L1 cache yet
    if (!mL1Cache && mNeedsResampling
        && painter->modes().testFlag(QCPPainter::pmNoCaching))
    {
        auto vp = ViewportParams::fromAxes(mKeyAxis.data(), mValueAxis.data());
        mPipeline.runSynchronously(vp);
        onL1Ready();
    }

    // Rebuild L2 from L1 cache when dirty (viewport changed since last build)
    if (mL2Dirty && mL1Cache)
    {
        rebuildL2(ViewportParams::fromAxes(mKeyAxis.data(), mValueAxis.data()));
        mL2Dirty = false;
    }

    // Data source priority: L2 (viewport-optimized) > raw
    // When L1 exists but L2 is null (sparse enough to draw directly), use raw source.
    const QCPAbstractDataSource* ds = nullptr;
    if (mL2Result)
        ds = mL2Result.get();
    else if (!mNeedsResampling || mL1Cache
             || painter->modes().testFlag(QCPPainter::pmNoCaching)
             || mKeyAxis->scaleType() == QCPAxis::stLogarithmic)
        ds = mDataSource.get();
    else
        return; // Pipeline active, no L1 yet — wait

    if (!ds || ds->empty())
        return;

    PROFILE_PASS_VALUE(ds->size());

    if (mKeyAxis->range().size() <= 0)
        return;
    if (mLineStyle == lsNone && mScatterStyle.isNone())
        return;

    const QCPRange keyRange = mKeyAxis->range();
    int begin = ds->findBegin(keyRange.lower);
    int end = ds->findEnd(keyRange.upper);
    if (begin >= end)
        return;

    const bool keyIsVertical = mKeyAxis->orientation() == Qt::Vertical;

    // --- Line caching with GPU translation ---
    const QSize currentPlotSize(mKeyAxis->axisRect()->width(), mKeyAxis->axisRect()->height());
    const bool isExportMode = painter->modes().testFlag(QCPPainter::pmNoCaching)
                            || painter->modes().testFlag(QCPPainter::pmVectorized);
    auto [needFreshLines, gpuOffset] = qcp::evaluateLineCache(
        mLineCacheDirty, mCachedLines.isEmpty(),
        currentPlotSize, mCachedPlotSize,
        mHasRenderedRange, mRenderedRange.key, mRenderedRange.value,
        mKeyAxis.data(), mValueAxis.data(), isExportMode);

    QVector<QPointF> lines;
    if (needFreshLines)
    {
        // Expand data range by 100% on each side so GPU-translated pans
        // don't expose uncovered edges before the rebuild threshold triggers.
        const double margin = keyRange.size() * 1.0;
        int cacheBegin = ds->findBegin(keyRange.lower - margin);
        int cacheEnd = ds->findEnd(keyRange.upper + margin);

        if (mAdaptiveSampling)
        {
            const int pixDim = keyIsVertical
                ? static_cast<int>(mKeyAxis->axisRect()->height())
                : static_cast<int>(mKeyAxis->axisRect()->width());
            lines = ds->getOptimizedLineData(
                cacheBegin, cacheEnd, pixDim, mKeyAxis.data(), mValueAxis.data());
        }
        else
        {
            lines = ds->getLines(cacheBegin, cacheEnd, mKeyAxis.data(), mValueAxis.data());
        }

        if (!isExportMode)
        {
            mCachedLines = lines;
            mRenderedRange = {mKeyAxis->range(), mValueAxis->range()};
            mHasRenderedRange = true;
            mLineCacheDirty = false;
            mCachedPlotSize = currentPlotSize;
            mExtrusionCache.clear();
            gpuOffset = {};
        }
    }
    else
    {
        lines = mCachedLines;
    }

    if (lines.isEmpty())
        return;

    const QPen drawPen = selected() && mSelectionDecorator
        ? mSelectionDecorator->pen() : mPen;

    // Draw lines
    if (mLineStyle != lsNone && drawPen.style() != Qt::NoPen && drawPen.color().alpha() != 0)
    {
        auto drawPoly = [&](const QVector<QPointF>& pts) {
            applyDefaultAntialiasingHint(painter);
            if (!isExportMode) {
                qcp::drawPolylineCached(painter, mParentPlot, mLayer, pts,
                                         drawPen, gpuOffset, clipRect(),
                                         needFreshLines, mExtrusionCache);
            } else {
                qcp::drawPolylineWithGpuFallback(painter, mParentPlot, mLayer, pts,
                                                  drawPen, gpuOffset, clipRect());
            }
        };

        // Only compute step-transform when the extrusion cache needs rebuilding —
        // on cache-hit pan frames, drawPolylineCached ignores pts entirely.
        const bool needStyledLines = needFreshLines || mExtrusionCache.isEmpty();
        switch (mLineStyle)
        {
            case lsNone:
                break;
            case lsLine:
                drawPoly(lines);
                break;
            case lsStepLeft:
                drawPoly(needStyledLines ? qcp::toStepLeftLines(lines, keyIsVertical) : lines);
                break;
            case lsStepRight:
                drawPoly(needStyledLines ? qcp::toStepRightLines(lines, keyIsVertical) : lines);
                break;
            case lsStepCenter:
                drawPoly(needStyledLines ? qcp::toStepCenterLines(lines, keyIsVertical) : lines);
                break;
            case lsImpulse:
            {
                auto impulse = qcp::toImpulseLines(lines, keyIsVertical, mValueAxis->coordToPixel(0));
                applyDefaultAntialiasingHint(painter);
                QPen impulsePen = drawPen;
                impulsePen.setCapStyle(Qt::FlatCap);
                painter->setPen(impulsePen);
                painter->drawLines(impulse);
                break;
            }
        }
    }

    // Draw scatters (step transforms don't modify lines — they return new vectors)
    if (!mScatterStyle.isNone())
    {
        bool usedGpu = false;
        if (!isExportMode && mParentPlot)
        {
            if (auto* srl = mParentPlot->scatterRhiLayer(mLayer))
            {
                const int skip = mScatterSkip + 1;
                std::vector<float> pts;
                pts.reserve((lines.size() / skip) * 3);
                for (int i = 0; i < lines.size(); i += skip)
                {
                    const double sx = lines[i].x(), sy = lines[i].y();
                    if (qIsFinite(sx) && qIsFinite(sy))
                    {
                        pts.push_back(static_cast<float>(sx));
                        pts.push_back(static_cast<float>(sy));
                        pts.push_back(0.0f);
                    }
                }
                if (!pts.empty())
                {
                    srl->addScatter(
                        std::span<const float>(pts.data(), pts.size()),
                        mScatterStyle, clipRect(),
                        mParentPlot->devicePixelRatioF(),
                        mParentPlot->rhiOutputSize().height(),
                        static_cast<float>(gpuOffset.x()),
                        static_cast<float>(gpuOffset.y()));
                }
                usedGpu = true;
            }
        }
        if (!usedGpu)
        {
            applyScattersAntialiasingHint(painter);
            mScatterStyle.applyTo(painter, drawPen);
            const int skip = mScatterSkip + 1;
            for (int i = 0; i < lines.size(); i += skip)
            {
                const double sx = lines[i].x(), sy = lines[i].y();
                if (qIsFinite(sx) && qIsFinite(sy))
                    mScatterStyle.drawShape(painter, sx, sy);
            }
        }
    }
}

void QCPGraph2::drawLegendIcon(QCPPainter* painter, const QRectF& rect) const
{
    applyDefaultAntialiasingHint(painter);

    // Draw line sample
    if (mLineStyle != lsNone)
    {
        painter->setPen(mPen);
        painter->drawLine(QLineF(rect.left(), rect.center().y(),
                                  rect.right() + 5, rect.center().y()));
    }

    // Draw scatter sample
    if (!mScatterStyle.isNone())
    {
        applyScattersAntialiasingHint(painter);
        QCPScatterStyle iconStyle = mScatterStyle;
        iconStyle.setSize(qMin(mScatterStyle.size(), rect.height() * 0.8));
        iconStyle.applyTo(painter, mPen);
        iconStyle.drawShape(painter, rect.center().x(), rect.center().y());
    }
}

void QCPGraph2::onViewportChanged()
{
    if (mL1Cache)
    {
        mL2Dirty = true;
        // Only debounce pure translations (panning). Zoom changes need
        // immediate L2 rebuild because the cached lines are at wrong scale.
        if (mHasRenderedRange && mKeyAxis)
        {
            double ratio = mKeyAxis->range().size() / mRenderedRange.key.size();
            if (qAbs(ratio - 1.0) < 1e-4)
                mViewportDebounce.start();
        }
    }
}
