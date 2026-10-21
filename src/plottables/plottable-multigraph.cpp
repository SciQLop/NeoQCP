#include "plottable-multigraph.h"
#include "plottable-draw-utils.h"
#include "plottable-l1-cache.h"
#include "plottable-linestyle.h"
#include "../axis/axis.h"
#include "../core.h"
#include "../datasource/graph-resampler.h"
#include "../datasource/resampled-multi-datasource.h"
#include "../layoutelements/layoutelement-axisrect.h"
#include "../layoutelements/layoutelement-legend-group.h"
#include "../painting/painter.h"
#include "../painting/viewport-offset.h"
#include "../painting/scatter-rhi-layer.h"
#include "../painting/plottable-rhi-layer.h"
#include "../painting/rhi-utils.h"
#include "../vector2d.h"
#include <algorithm>
#include <cmath>

static QPen defaultSelectedPen(const QPen& pen)
{
    QPen sel = pen;
    sel.setWidthF(pen.widthF() + 1.5);
    QColor c = pen.color();
    c.setAlphaF(qMin(1.0, c.alphaF() * 1.3));
    sel.setColor(c);
    return sel;
}

static const QList<QColor> sDefaultColors = {
    QColor(31, 119, 180),  QColor(255, 127, 14), QColor(44, 160, 44),
    QColor(214, 39, 40),   QColor(148, 103, 189), QColor(140, 86, 75),
    QColor(227, 119, 194), QColor(127, 127, 127), QColor(188, 189, 34),
    QColor(23, 190, 207)
};

QCPMultiGraph::QCPMultiGraph(QCPAxis* keyAxis, QCPAxis* valueAxis)
    : QCPAbstractPlottable(keyAxis, valueAxis)
    , mPipeline(parentPlot() ? parentPlot()->pipelineScheduler() : nullptr, this)
{
    // Base constructor auto-adds a QCPPlottableLegendItem (vtable not yet set up
    // for virtual addToLegend). Replace it with our group legend item.
    if (mParentPlot && mParentPlot->autoAddPlottableToLegend() && mParentPlot->legend) {
        QCPAbstractPlottable::removeFromLegend(mParentPlot->legend);
        addToLegend();
    }

    if (keyAxis)
    {
        connect(keyAxis, QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
                this, &QCPMultiGraph::onViewportChanged);
        connect(keyAxis, &QCPAxis::scaleTypeChanged,
                this, [this] { invalidateLines(); });
    }
    if (valueAxis)
    {
        connect(valueAxis, &QCPAxis::scaleTypeChanged,
                this, [this] { invalidateLines(); });
    }

    connect(&mPipeline, &QCPMultiGraphPipeline::finished,
            this, [this](uint64_t gen) { onL1Ready(gen); });
    connect(&mPipeline, &QCPMultiGraphPipeline::busyChanged,
            this, [this](bool) { updateEffectiveBusy(); });

    mViewportDebounce.setSingleShot(true);
    mViewportDebounce.setInterval(150);
    connect(&mViewportDebounce, &QTimer::timeout, this, [this] {
        markLinesDirty();
        if (mParentPlot)
            mParentPlot->replot(QCustomPlot::rpQueuedReplot);
    });
}

QCPMultiGraph::~QCPMultiGraph() = default;

// Fresh lines are drawn only if the layer is repainted: a replot repaints dirty layers, and
// falls back to marking the plot's layers dirty only when none is, so mark this one here.
void QCPMultiGraph::markLinesDirty()
{
    mLineCacheDirty = true;
    if (mLayer)
        mLayer->markDirty();
}

void QCPMultiGraph::setDataSource(std::unique_ptr<QCPAbstractMultiDataSource> source)
{
    setDataSource(std::shared_ptr<QCPAbstractMultiDataSource>(std::move(source)));
}

static void ensureL1TransformMulti(QCPMultiGraphPipeline& pipeline, int sourceSize, int colCount,
                                   std::shared_ptr<std::atomic<bool>> wantOrigin)
{
    const bool needsResampling = colCount > 0
        && static_cast<int64_t>(sourceSize) * colCount >= qcp::algo::kResampleThreshold;
    if (needsResampling)
    {
        if (!pipeline.hasTransform())
        {
            pipeline.setTransform(TransformKind::ViewportIndependent,
                [wantOrigin](const QCPAbstractMultiDataSource& src,
                             const ViewportParams& vp,
                             std::any& cache) -> std::shared_ptr<QCPAbstractMultiDataSource> {
                    return qcp::algo::buildL1CacheMulti(src, vp, cache, wantOrigin->load());
                });
        }
    }
    else if (pipeline.hasTransform())
    {
        pipeline.clearTransform();
    }
}

static bool needsResamplingMulti(const QCPAbstractMultiDataSource& src)
{
    return src.columnCount() > 0
        && static_cast<int64_t>(src.size()) * src.columnCount() >= qcp::algo::kResampleThreshold;
}

void QCPMultiGraph::setDataSource(std::shared_ptr<QCPAbstractMultiDataSource> source)
{
    // Keep the displayed geometry (and its GPU translation) alive while the
    // replacement is prepared; the plot commits it in one debounced swap.
    const bool canDefer = source && mDataSource && mHasRenderedRange && mParentPlot;
    if (canDefer)
        stagePendingSource(std::move(source));
    else
        applySourceNow(std::move(source));
}

void QCPMultiGraph::applySourceNow(std::shared_ptr<QCPAbstractMultiDataSource> source)
{
    mPendingSource.reset();
    mPendingL1.reset();
    mPendingColorValues.reset();
    mPendingReady = false;
    mDataSource = std::move(source);
    applyColorSizeRule(mDataSource ? mDataSource->size() : 0);
    invalidateLines();
    mL2HasOrigin = false;
    syncComponentCount(mDataSource ? mDataSource->columnCount() : 0);
    mL1Cache.reset();
    mL2Result.reset();
    mL2Dirty = false;
    mNeedsResampling = mDataSource && needsResamplingMulti(*mDataSource);
    if (mDataSource)
        ensureL1TransformMulti(mPipeline, mDataSource->size(), mDataSource->columnCount(), mWantOrigin);
    mPipeline.setSource(mDataSource);
    // Without a transform setSource() does not bump the generation, so a job
    // still running for the previous source would otherwise pass onL1Ready's
    // staleness check below.
    mAcceptedGeneration = mPipeline.hasTransform() ? mPipeline.currentGeneration()
                                                    : mPipeline.currentGeneration() + 1;
    if (!mPipeline.hasTransform() && mParentPlot)
        mParentPlot->replot(QCustomPlot::rpQueuedReplot);
    updateEffectiveBusy();
}

void QCPMultiGraph::stagePendingSource(std::shared_ptr<QCPAbstractMultiDataSource> source)
{
    mPendingSource = std::move(source);
    mPendingL1.reset();
    // A newly-staged source supersedes whatever replacement (and colour values meant
    // for it) was staged before; setColorValues() re-stashes if called again.
    mPendingColorValues.reset();
    mPendingReady = false;
    syncComponentCount(mPendingSource->columnCount());
    ensureL1TransformMulti(mPipeline, mPendingSource->size(), mPendingSource->columnCount(), mWantOrigin);
    mPipeline.setSource(mPendingSource);
    // Without a transform setSource() does not bump the generation, so a job
    // still running for the previous source would pass the guard below.
    mPendingGeneration = mPipeline.hasTransform() ? mPipeline.currentGeneration()
                                                  : mPipeline.currentGeneration() + 1;
    if (!mPipeline.hasTransform())
        markPendingReady();
    updateEffectiveBusy();
}

void QCPMultiGraph::markPendingReady()
{
    mPendingReady = true;
    if (mParentPlot)
        mParentPlot->requestDataSwap();
}

bool QCPMultiGraph::commitPendingData()
{
    if (!mPendingSource || !mPendingReady)
        return false;
    mDataSource = std::move(mPendingSource);
    mL1Cache = std::move(mPendingL1);
    mPendingReady = false;
    mL2Result.reset();
    mL2Dirty = mL1Cache != nullptr;
    mNeedsResampling = needsResamplingMulti(*mDataSource);

    if (mPendingColorValues && static_cast<int>(mPendingColorValues->size()) == mDataSource->size())
    {
        const bool wasColoured = mColor.hasValues();
        mColor.setValues(std::move(mPendingColorValues));
        if (!wasColoured)
            requestOrigin();
    }
    else
    {
        applyColorSizeRule(mDataSource->size());
    }
    mPendingColorValues.reset();
    invalidateLines();
    mL2HasOrigin = false;

    // Carry the barrier established while staging forward past the commit:
    // a job dispatched for the source this replaced must not be able to land
    // in mL1Cache/mL2Result once this source is the one being displayed.
    mAcceptedGeneration = mPendingGeneration;
    updateEffectiveBusy();
    return true;
}

void QCPMultiGraph::dataChanged()
{
    // A mutated displayed source supersedes whatever replacement was staged:
    // stage it like any other new data so the on-screen geometry stays put.
    if (mPendingSource)
    {
        stagePendingSource(mDataSource);
        return;
    }

    markLinesDirty();
    if (mDataSource)
    {
        applyColorSizeRule(mDataSource->size());
        mNeedsResampling = needsResamplingMulti(*mDataSource);
        ensureL1TransformMulti(mPipeline, mDataSource->size(), mDataSource->columnCount(), mWantOrigin);
    }

    mL1Cache.reset();
    mL2Dirty = false;

    if (mPipeline.hasTransform())
        mPipeline.onDataChanged();
    else if (mParentPlot)
        mParentPlot->replot();

    // A mutation can leave a job in flight for pre-mutation data: if the
    // transform stayed on, the job just queued behind it supersedes it; if
    // the transform was just cleared, no further job is coming at all and a
    // late result would otherwise land in mL1Cache permanently. Same +1
    // no-bump hazard as applySourceNow()/stagePendingSource().
    mAcceptedGeneration = mPipeline.hasTransform() ? mPipeline.currentGeneration()
                                                    : mPipeline.currentGeneration() + 1;
}

void QCPMultiGraph::onL1Ready(uint64_t generation)
{
    if (mPendingSource)
    {
        if (generation < mPendingGeneration)
            return; // result of a superseded pending source
        [[maybe_unused]] bool l2Dirty = false; // recomputed from mL1Cache by commitPendingData()
        qcp::extractL1Cache<qcp::algo::MultiGraphResamplerCache>(mPipeline.cache(), mPendingL1, l2Dirty);
        markPendingReady();
        return;
    }
    if (generation < mAcceptedGeneration)
        return; // result of a source that was superseded before this job finished
    qcp::extractL1Cache<qcp::algo::MultiGraphResamplerCache>(mPipeline.cache(), mL1Cache, mL2Dirty);
    markLinesDirty();
    if (parentPlot())
        parentPlot()->replot(QCustomPlot::rpQueuedReplot);
}

void QCPMultiGraph::rebuildL2(const ViewportParams& vp)
{
    if (!mL1Cache) return;
    mL2Result = qcp::algo::resampleL2Multi(*mL1Cache, vp, *mDataSource);
    mL2HasOrigin = mL2Result && !mL1Cache->level1.origin.empty();
}

void QCPMultiGraph::onViewportChanged()
{
    if (mL1Cache)
    {
        mL2Dirty = true;
        if (mHasRenderedRange && mKeyAxis)
        {
            const double ratio = qcp::axisRangeSizeRatio(mKeyAxis.data(), mRenderedRange.key);
            if (qAbs(ratio - 1.0) < 1e-4)
                mViewportDebounce.start();
        }
    }
}

void QCPMultiGraph::syncComponentCount(int newCount)
{
    int oldCount = mComponents.size();
    if (newCount > oldCount) {
        mComponents.resize(newCount);
        for (int i = oldCount; i < newCount; ++i) {
            auto& c = mComponents[i];
            QColor color = sDefaultColors[i % sDefaultColors.size()];
            c.pen = QPen(color, 0);
            c.selectedPen = defaultSelectedPen(c.pen);
            c.name = QString("Component %1").arg(i);
            c.lineStyle = mLineStyle;
            c.inLegend = !isErrorBound(i);
        }
    } else if (newCount < oldCount) {
        mComponents.resize(newCount);
        dropDecorationsBeyond(newCount);
    }
}

// A decoration pointing at a column that no longer exists is dropped for good: a later, wider
// source puts unrelated data in that column.
void QCPMultiGraph::dropDecorationsBeyond(int columnCount)
{
    const auto outside = [columnCount](const FillEntry& e) {
        const auto* col = std::get_if<int>(&e.spec.target);
        return e.spec.owner >= columnCount || (col && *col >= columnCount);
    };
    mFills.erase(std::remove_if(mFills.begin(), mFills.end(), outside), mFills.end());
    const auto barsOutside = [columnCount](const ErrorBarsEntry& e) {
        return std::max({e.spec.value, e.spec.lower, e.spec.upper}) >= columnCount;
    };
    mErrorBars.erase(std::remove_if(mErrorBars.begin(), mErrorBars.end(), barsOutside),
                     mErrorBars.end());
}

// Error bars can be set before the data that creates their bound components.
bool QCPMultiGraph::isErrorBound(int column) const
{
    return std::any_of(mErrorBars.begin(), mErrorBars.end(), [column](const ErrorBarsEntry& e) {
        return e.spec.lower == column || e.spec.upper == column;
    });
}

bool QCPMultiGraph::isValidColumn(int column) const
{
    const auto& source = mPendingSource ? mPendingSource : mDataSource;
    return column >= 0 && (!source || column < source->columnCount());
}

// A default-brush fill or error band takes its owner's pen colour, which the GPU vertices have
// baked in.
bool QCPMultiGraph::hasPenDerivedDecoration(int ownerCount) const
{
    return std::any_of(mFills.begin(), mFills.end(), [ownerCount](const FillEntry& e) {
        return e.spec.owner < ownerCount && e.spec.brush.style() == Qt::NoBrush;
    }) || std::any_of(mErrorBars.begin(), mErrorBars.end(), [ownerCount](const ErrorBarsEntry& e) {
        return e.spec.value < ownerCount && !e.spec.pen;
    });
}

void QCPMultiGraph::decorationsChanged()
{
    invalidateLines();
    if (mLayer)
        mLayer->invalidatePaintBuffer();
}

bool QCPMultiGraph::setFill(Fill fill)
{
    const auto* col = std::get_if<int>(&fill.target);
    if (!isValidColumn(fill.owner) || (col && (!isValidColumn(*col) || *col == fill.owner)))
        return false;
    clearComponentFill(fill.owner);
    mFills.append({std::move(fill), {}});
    decorationsChanged();
    return true;
}

bool QCPMultiGraph::setComponentFill(int owner, double value, const QBrush& brush)
{
    return setFill({owner, value, brush});
}

bool QCPMultiGraph::setComponentFillBetween(int owner, int target, const QBrush& brush)
{
    return setFill({owner, target, brush});
}

void QCPMultiGraph::setComponentFillBrush(int owner, const QBrush& brush)
{
    const auto it = std::find_if(mFills.begin(), mFills.end(),
                                 [owner](const FillEntry& e) { return e.spec.owner == owner; });
    if (it == mFills.end())
        return;
    it->spec.brush = brush;
    decorationsChanged();
}

void QCPMultiGraph::clearComponentFill(int owner)
{
    const auto before = mFills.size();
    mFills.erase(std::remove_if(mFills.begin(), mFills.end(),
                                [owner](const FillEntry& e) { return e.spec.owner == owner; }),
                 mFills.end());
    if (mFills.size() != before)
        decorationsChanged();
}

std::optional<QCPMultiGraph::Fill> QCPMultiGraph::componentFill(int owner) const
{
    for (const auto& e : mFills)
        if (e.spec.owner == owner)
            return e.spec;
    return std::nullopt;
}

QBrush QCPMultiGraph::effectiveFillBrush(const Fill& fill) const
{
    if (fill.brush.style() != Qt::NoBrush)
        return fill.brush;
    const QColor c = fill.owner < mComponents.size() ? mComponents[fill.owner].pen.color()
                                                     : QColor(Qt::gray);
    return QBrush(QColor(c.red(), c.green(), c.blue(), 64));
}

bool QCPMultiGraph::setComponentErrorBars(int value, int lower, int upper, double capWidthPx,
                                          std::optional<QPen> pen)
{
    if (!isValidColumn(value) || !isValidColumn(lower) || !isValidColumn(upper)
        || lower == upper || value == lower || value == upper)
        return false;
    clearComponentErrorBars(value);
    ErrorBarsEntry entry;
    entry.spec = {value, lower, upper, std::move(pen), capWidthPx};
    mErrorBars.append(std::move(entry));
    setComponentInLegend(lower, false);
    setComponentInLegend(upper, false);
    decorationsChanged();
    return true;
}

void QCPMultiGraph::clearComponentErrorBars(int value)
{
    const auto before = mErrorBars.size();
    mErrorBars.erase(std::remove_if(mErrorBars.begin(), mErrorBars.end(),
                                    [value](const ErrorBarsEntry& e) { return e.spec.value == value; }),
                     mErrorBars.end());
    if (mErrorBars.size() != before)
        decorationsChanged();
}

std::optional<QCPMultiGraph::ErrorBars> QCPMultiGraph::componentErrorBars(int value) const
{
    for (const auto& e : mErrorBars)
        if (e.spec.value == value)
            return e.spec;
    return std::nullopt;
}

void QCPMultiGraph::updateBaseSelection()
{
    QCPDataSelection combined;
    for (const auto& c : mComponents) {
        for (int i = 0; i < c.selection.dataRangeCount(); ++i)
            combined.addDataRange(c.selection.dataRange(i), false);
    }
    combined.simplify();
    if (mSelection != combined)
    {
        mSelection = combined;
        emit selectionChanged(selected());
        emit selectionChanged(mSelection);
    }
}

// --- Component API ---

void QCPMultiGraph::setComponentNames(const QStringList& names)
{
    int n = qMin(names.size(), mComponents.size());
    for (int i = 0; i < n; ++i)
        mComponents[i].name = names[i];
}

void QCPMultiGraph::setComponentColors(const QList<QColor>& colors)
{
    int n = qMin(colors.size(), mComponents.size());
    for (int i = 0; i < n; ++i) {
        mComponents[i].pen.setColor(colors[i]);
        mComponents[i].selectedPen = defaultSelectedPen(mComponents[i].pen);
    }
    if (hasPenDerivedDecoration(n))
        decorationsChanged();
}

void QCPMultiGraph::setComponentPens(const QList<QPen>& pens)
{
    int n = qMin(pens.size(), mComponents.size());
    for (int i = 0; i < n; ++i) {
        mComponents[i].pen = pens[i];
        mComponents[i].selectedPen = defaultSelectedPen(pens[i]);
    }
    if (hasPenDerivedDecoration(n))
        decorationsChanged();
}

void QCPMultiGraph::setComponentInLegend(int index, bool inLegend)
{
    if (index < 0 || index >= mComponents.size() || mComponents[index].inLegend == inLegend)
        return;
    mComponents[index].inLegend = inLegend;
    Q_EMIT componentVisibilityChanged();
}

QVector<int> QCPMultiGraph::legendComponents() const
{
    QVector<int> shown;
    for (int i = 0; i < mComponents.size(); ++i)
        if (mComponents[i].inLegend)
            shown.append(i);
    return shown;
}

void QCPMultiGraph::setComponentVisible(int index, bool visible)
{
    if (index < 0 || index >= mComponents.size() || mComponents[index].visible == visible)
        return;
    mComponents[index].visible = visible;
    // A rebuild while the component was hidden dropped its cached lines: rebuild again.
    markLinesDirty();
    if (mLayer)
        mLayer->invalidatePaintBuffer();
    emit componentVisibilityChanged();
}

void QCPMultiGraph::setLineStyle(LineStyle style)
{
    mLineStyle = style;
    for (auto& c : mComponents)
        c.lineStyle = style;
    invalidateLines();
}

void QCPMultiGraph::setComponentLineStyle(int index, LineStyle style)
{
    if (index < 0 || index >= mComponents.size() || mComponents[index].lineStyle == style)
        return;
    mComponents[index].lineStyle = style;
    invalidateLines();
}

void QCPMultiGraph::setAdaptiveSampling(bool enabled)
{
    if (mAdaptiveSampling == enabled)
        return;
    mAdaptiveSampling = enabled;
    invalidateLines();
}

void QCPMultiGraph::setGapThreshold(double threshold)
{
    if (mGapThreshold == threshold)
        return;
    mGapThreshold = threshold;
    invalidateLines();
}

void QCPMultiGraph::invalidateLines()
{
    markLinesDirty();
    mCachedLines.clear();
    mCachedIndices.clear();
}

QVector<int> QCPMultiGraph::lineIndices(LineStyle style, const QVector<int>& dataIdx)
{
    switch (style)
    {
        case lsStepLeft:   return qcp::stepLeftIndices(dataIdx);
        case lsStepRight:  return qcp::stepRightIndices(dataIdx);
        case lsStepCenter: return qcp::stepCenterIndices(dataIdx);
        case lsImpulse:    return qcp::impulseIndices(dataIdx);
        default:           return dataIdx;
    }
}

// Batches impulse line pairs per colour bucket so each bucket is drawn with one
// drawLines() call (at most 256, one per bucket) instead of one call per pair.
void QCPMultiGraph::drawColoredImpulses(QCPPainter* painter, const QVector<QPointF>& pairs,
                                        const QVector<int>& indices, QPen pen,
                                        const QPointF& gpuOffset) const
{
    std::array<QVector<QLineF>, 256> byBucket;
    for (int i = 0; i + 1 < pairs.size(); i += 2)
    {
        const int b = mColor.bucket(indices[i + 1]);
        if (b != qcp::ColorScalarMapper::kGap)
            byBucket[b].append(QLineF(pairs[i], pairs[i + 1]));
    }
    if (!gpuOffset.isNull())
        painter->translate(gpuOffset);
    for (int b = 0; b < 256; ++b)
    {
        if (byBucket[b].isEmpty()) continue;
        pen.setColor(qcp::runColor(mColor, b));
        painter->setPen(pen);
        painter->drawLines(byBucket[b]);
    }
    if (!gpuOffset.isNull())
        painter->translate(-gpuOffset);
}

void QCPMultiGraph::drawColoredScatters(QCPPainter* painter, const QVector<QPointF>& points,
                                        const QVector<int>& indices, const QCPGraphComponent& comp,
                                        const QPointF& gpuOffset, bool isExportMode) const
{
    const int skip = mScatterSkip + 1;
    if (auto* srl = (!isExportMode && mParentPlot) ? mParentPlot->scatterRhiLayer(mLayer) : nullptr)
    {
        std::vector<float> xy, rgba;
        xy.reserve((points.size() / skip) * 2);
        rgba.reserve((points.size() / skip) * 4);
        for (int i = 0; i < points.size(); i += skip)
        {
            const int b = mColor.bucket(indices[i]);
            if (b == qcp::ColorScalarMapper::kGap || !qIsFinite(points[i].x()) || !qIsFinite(points[i].y()))
                continue;
            const QRgb c = mColor.color(b);   // premultiplied
            xy.insert(xy.end(), {float(points[i].x()), float(points[i].y())});
            rgba.insert(rgba.end(), {qRed(c) / 255.f, qGreen(c) / 255.f, qBlue(c) / 255.f, qAlpha(c) / 255.f});
        }
        if (!xy.empty())
            srl->addScatterColored(xy, rgba, comp.scatterStyle, clipRect(),
                                   mParentPlot->devicePixelRatioF(),
                                   mParentPlot->rhiOutputSize().height(),
                                   float(gpuOffset.x()), float(gpuOffset.y()),
                                   static_cast<float>(painter->opacity()));
        return;
    }
    applyScattersAntialiasingHint(painter);
    if (!gpuOffset.isNull())
        painter->translate(gpuOffset);
    std::array<QVector<QPointF>, 256> byBucket;
    for (int i = 0; i < points.size(); i += skip)
    {
        const int b = mColor.bucket(indices[i]);
        if (b != qcp::ColorScalarMapper::kGap && qIsFinite(points[i].x()) && qIsFinite(points[i].y()))
            byBucket[b].append(points[i]);
    }
    for (int b = 0; b < 256; ++b)
    {
        if (byBucket[b].isEmpty()) continue;
        const QColor color = qcp::runColor(mColor, b);
        QCPScatterStyle style = comp.scatterStyle;
        style.setPen(QPen(color));
        style.setBrush(color);
        style.applyTo(painter, style.pen());
        for (const auto& p : byBucket[b])
            style.drawShape(painter, p.x(), p.y());
    }
    if (!gpuOffset.isNull())
        painter->translate(-gpuOffset);
}

void QCPMultiGraph::drawPlainScatters(QCPPainter* painter, const QVector<QPointF>& dataLines,
                                      const QCPGraphComponent& comp, const QPointF& gpuOffset,
                                      bool isExportMode) const
{
    bool usedGpu = false;
    if (!isExportMode && mParentPlot)
    {
        if (auto* srl = mParentPlot->scatterRhiLayer(mLayer))
        {
            const int skip = mScatterSkip + 1;
            std::vector<float> pts;
            pts.reserve((dataLines.size() / skip) * 3);
            for (int i = 0; i < dataLines.size(); i += skip)
            {
                const double sx = dataLines[i].x(), sy = dataLines[i].y();
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
                    comp.scatterStyle, clipRect(),
                    mParentPlot->devicePixelRatioF(),
                    mParentPlot->rhiOutputSize().height(),
                    static_cast<float>(gpuOffset.x()),
                    static_cast<float>(gpuOffset.y()),
                    QImage{}, static_cast<float>(painter->opacity()));
            }
            usedGpu = true;
        }
    }
    if (!usedGpu)
    {
        applyScattersAntialiasingHint(painter);
        comp.scatterStyle.applyTo(painter, comp.pen);
        const int skip = mScatterSkip + 1;
        for (int i = 0; i < dataLines.size(); i += skip)
        {
            const double sx = dataLines[i].x(), sy = dataLines[i].y();
            if (qIsFinite(sx) && qIsFinite(sy))
                comp.scatterStyle.drawShape(painter, sx, sy);
        }
    }
}

// One async L1 rebuild, the first time the graph is coloured; later colour changes never touch L1/L2.
void QCPMultiGraph::requestOrigin()
{
    if (!mWantOrigin->exchange(true) && mPipeline.hasTransform())
        mPipeline.onDataChanged();
}

// Same-size-keeps/other-size-clears colour rule, applied whenever a new mDataSource is
// installed without new colour values to go with it (applySourceNow, commitPendingData
// when nothing was stashed for the committed source, and dataChanged on an in-place mutation).
void QCPMultiGraph::applyColorSizeRule(int newSize)
{
    if (mColor.hasValues() && mColor.size() != newSize)
        mColor.clearValues();
    if (!mColor.hasValues())
        mWantOrigin->store(false);
}

void QCPMultiGraph::setColorValues(std::shared_ptr<const std::vector<double>> values)
{
    if (!values || values->empty())
        return clearColorValues();
    // A pending source (staged but not yet committed) is what the values will apply to
    // once the swap lands — validate against its size, not the still-displayed source's.
    const QCPAbstractMultiDataSource* target = mPendingSource ? mPendingSource.get() : mDataSource.get();
    const int expected = target ? target->size() : 0;
    if (static_cast<int>(values->size()) != expected)
    {
        qWarning() << "QCPMultiGraph::setColorValues: expected" << expected
                   << "values (one per key), got" << values->size();
        clearColorValues();   // refused: the graph is drawn uncoloured, not left stale
        return;
    }
    if (mPendingSource)
    {
        mPendingColorValues = std::move(values);
        return;
    }
    const bool wasColoured = mColor.hasValues();
    mColor.setValues(std::move(values));
    if (!wasColoured)
    {
        invalidateLines();
        requestOrigin();
    }
}

void QCPMultiGraph::setColorValues(std::vector<double> values)
{
    setColorValues(std::make_shared<const std::vector<double>>(std::move(values)));
}

void QCPMultiGraph::clearColorValues()
{
    // Every "uncolour" route (wrong length, an empty vector, or an explicit call) must
    // also drop a stash left by an earlier, valid setColorValues() while a source was
    // pending — otherwise a graph "coloured" only through that stash survives this call
    // and commitPendingData() re-colours it from the stale stash.
    mPendingColorValues.reset();
    if (!mColor.hasValues())
        return;
    mColor.clearValues();
    invalidateLines();
}

bool QCPMultiGraph::hasColorValues() const { return mColor.hasValues(); }
void QCPMultiGraph::setColorGradient(const QCPColorGradient& gradient) { mColor.setGradient(gradient); }
QCPColorGradient QCPMultiGraph::colorGradient() const { return mColor.gradient(); }
void QCPMultiGraph::setColorRange(const QCPRange& range) { mColor.setRange(range); }
QCPRange QCPMultiGraph::colorRange() const { return mColor.range(); }
void QCPMultiGraph::setColorScaleType(QCPAxis::ScaleType type) { mColor.setScaleType(type); }
QCPAxis::ScaleType QCPMultiGraph::colorScaleType() const { return mColor.scaleType(); }

double QCPMultiGraph::componentValueAt(int column, int index) const
{
    // column comes from componentCount(), which may already reflect a wider
    // pending source than the one currently displayed.
    return (mDataSource && column < mDataSource->columnCount())
        ? mDataSource->valueAt(column, index) : 0.0;
}

QCPDataSelection QCPMultiGraph::componentSelection(int index) const
{
    return (index >= 0 && index < mComponents.size()) ? mComponents[index].selection : QCPDataSelection();
}

void QCPMultiGraph::setComponentSelection(int index, const QCPDataSelection& sel)
{
    if (index >= 0 && index < mComponents.size()) {
        mComponents[index].selection = sel;
        updateBaseSelection();
    }
}

// --- QCPPlottableInterface1D ---

int QCPMultiGraph::dataCount() const
{
    return mDataSource ? mDataSource->size() : 0;
}

double QCPMultiGraph::dataMainKey(int index) const
{
    return mDataSource ? mDataSource->keyAt(index) : 0.0;
}

double QCPMultiGraph::dataSortKey(int index) const
{
    return mDataSource ? mDataSource->keyAt(index) : 0.0;
}

double QCPMultiGraph::dataMainValue(int index) const
{
    return (mDataSource && mDataSource->columnCount() > 0)
        ? mDataSource->valueAt(0, index) : 0.0;
}

QCPRange QCPMultiGraph::dataValueRange(int index) const
{
    if (!mDataSource || mDataSource->columnCount() == 0)
        return QCPRange(0, 0);
    double vmin = std::numeric_limits<double>::max();
    double vmax = std::numeric_limits<double>::lowest();
    for (int c = 0; c < mDataSource->columnCount(); ++c) {
        if (c < mComponents.size() && !mComponents[c].visible) continue;
        double v = mDataSource->valueAt(c, index);
        if (v < vmin) vmin = v;
        if (v > vmax) vmax = v;
    }
    return QCPRange(vmin, vmax);
}

QPointF QCPMultiGraph::dataPixelPosition(int index) const
{
    if (!mDataSource || !mKeyAxis || !mValueAxis || mDataSource->columnCount() == 0)
        return {};
    return coordsToPixels(mDataSource->keyAt(index), mDataSource->valueAt(0, index));
}

int QCPMultiGraph::findBegin(double sortKey, bool expandedRange) const
{
    return mDataSource ? mDataSource->findBegin(sortKey, expandedRange) : 0;
}

int QCPMultiGraph::findEnd(double sortKey, bool expandedRange) const
{
    return mDataSource ? mDataSource->findEnd(sortKey, expandedRange) : 0;
}

// --- Range queries ---

QCPRange QCPMultiGraph::getKeyRange(bool& foundRange, QCP::SignDomain inSignDomain) const
{
    if (!mDataSource || mDataSource->empty()) {
        foundRange = false;
        return {};
    }
    return mDataSource->keyRange(foundRange, inSignDomain);
}

QCPRange QCPMultiGraph::getValueRange(bool& foundRange, QCP::SignDomain inSignDomain,
                                       const QCPRange& inKeyRange) const
{
    foundRange = false;
    if (!mDataSource || mDataSource->empty())
        return {};

    double lower = std::numeric_limits<double>::max();
    double upper = std::numeric_limits<double>::lowest();
    // mComponents may be sized for a wider pending source than the one
    // currently displayed — clamp to avoid reading past its columns.
    const int nc = qMin(mComponents.size(), mDataSource->columnCount());
    for (int c = 0; c < nc; ++c) {
        if (!mComponents[c].visible) continue;
        bool colFound = false;
        auto colRange = mDataSource->valueRange(c, colFound, inSignDomain, inKeyRange);
        if (colFound) {
            if (colRange.lower < lower) lower = colRange.lower;
            if (colRange.upper > upper) upper = colRange.upper;
            foundRange = true;
        }
    }
    return foundRange ? QCPRange(lower, upper) : QCPRange();
}

// --- Selection ---

double QCPMultiGraph::selectTest(const QPointF& pos, bool onlySelectable, QVariant* details) const
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
    // each component at that index.  O(log N + C) instead of O(C × M).
    const QCPAbstractMultiDataSource* ds = mDataSource.get();
    const int n = ds->size();

    double posKey, dummy;
    pixelsToCoords(pos, posKey, dummy);

    // findEnd gives the first index > posKey; the nearest key is either
    // that index or the one before it.
    int idx = ds->findEnd(posKey, /*expandedRange=*/false);
    int lo = qMax(0, idx - 1);
    int hi = qMin(idx, n - 1);

    double minDistSqr = (std::numeric_limits<double>::max)();
    int minDistIndex = -1;
    int minDistComponent = -1;
    double minKey = 0, minValue = 0;

    const int nComponents = qMin(mComponents.size(), ds->columnCount());
    for (int i = lo; i <= hi; ++i) {
        double k = ds->keyAt(i);
        for (int c = 0; c < nComponents; ++c) {
            if (!mComponents[c].visible) continue;
            double v = ds->valueAt(c, i);
            double distSqr = QCPVector2D(coordsToPixels(k, v) - pos).lengthSquared();
            if (distSqr < minDistSqr) {
                minDistSqr = distSqr;
                minDistIndex = i;
                minDistComponent = c;
                minKey = k;
                minValue = v;
            }
        }
    }

    // Also check distance to line segments between lo and hi for each component
    if (lo < hi) {
        for (int c = 0; c < nComponents; ++c) {
            if (!mComponents[c].visible || mComponents[c].lineStyle == lsNone) continue;
            QPointF pLo = coordsToPixels(ds->keyAt(lo), ds->valueAt(c, lo));
            QPointF pHi = coordsToPixels(ds->keyAt(hi), ds->valueAt(c, hi));
            double lineDistSqr = QCPVector2D(pos).distanceSquaredToLine(pLo, pHi);
            if (lineDistSqr < minDistSqr) {
                minDistSqr = lineDistSqr;
                minDistComponent = c;
                minDistIndex = lo;
                minKey = ds->keyAt(lo);
                minValue = ds->valueAt(c, lo);
            }
        }
    }

    if (minDistIndex >= 0) {
        QVariantMap map;
        map["componentIndex"] = minDistComponent;
        map["dataIndex"] = minDistIndex;
        map["key"] = minKey;
        map["value"] = minValue;
        details->setValue(map);
    }
    return minDistIndex >= 0 ? qSqrt(minDistSqr) : -1;
}

QCPDataSelection QCPMultiGraph::selectTestRect(const QRectF& rect, bool onlySelectable) const
{
    QCPDataSelection unionResult;
    mLastRectSelections.clear();
    if ((onlySelectable && mSelectable == QCP::stNone) || !mDataSource || mDataSource->empty())
        return unionResult;
    if (!mKeyAxis || !mValueAxis)
        return unionResult;

    double key1, value1, key2, value2;
    pixelsToCoords(rect.topLeft(), key1, value1);
    pixelsToCoords(rect.bottomRight(), key2, value2);
    if (key1 > key2) qSwap(key1, key2);
    if (value1 > value2) qSwap(value1, value2);
    QCPRange keyRange(key1, key2);
    QCPRange valueRange(value1, value2);

    int begin = mDataSource->findBegin(keyRange.lower, false);
    int end = mDataSource->findEnd(keyRange.upper, false);

    mLastRectSelections.resize(mComponents.size());
    // mComponents may be sized for a wider pending source than the one
    // currently displayed — clamp to avoid reading past its columns.
    const int nc = qMin(mComponents.size(), mDataSource->columnCount());
    for (int c = 0; c < nc; ++c) {
        if (!mComponents[c].visible) continue;
        QCPDataSelection colSel;
        int segBegin = -1;
        for (int i = begin; i < end; ++i) {
            double k = mDataSource->keyAt(i);
            double v = mDataSource->valueAt(c, i);
            if (segBegin == -1) {
                if (keyRange.contains(k) && valueRange.contains(v))
                    segBegin = i;
            } else if (!keyRange.contains(k) || !valueRange.contains(v)) {
                colSel.addDataRange(QCPDataRange(segBegin, i), false);
                segBegin = -1;
            }
        }
        if (segBegin != -1)
            colSel.addDataRange(QCPDataRange(segBegin, end), false);
        colSel.simplify();
        mLastRectSelections[c] = colSel;
        for (int r = 0; r < colSel.dataRangeCount(); ++r)
            unionResult.addDataRange(colSel.dataRange(r), false);
    }
    unionResult.simplify();
    return unionResult;
}

void QCPMultiGraph::selectEvent([[maybe_unused]] QMouseEvent* event, bool additive,
                                 const QVariant& details, bool* selectionStateChanged)
{

    if (!additive) {
        for (auto& c : mComponents)
            c.selection = QCPDataSelection();
    }

    if (details.canConvert<QCPDataSelection>()) {
        // Rect selection: use per-component selections computed by selectTestRect,
        // which already filtered by the rect's key and value ranges.
        if (!mLastRectSelections.isEmpty()) {
            for (int c = 0; c < mComponents.size() && c < mLastRectSelections.size(); ++c)
                mComponents[c].selection = mLastRectSelections[c];
            mLastRectSelections.clear();
        }
    } else {
        // Point selection: details is a QVariantMap with componentIndex/dataIndex
        auto map = details.toMap();
        int compIdx = map.value("componentIndex", -1).toInt();
        int dataIdx = map.value("dataIndex", -1).toInt();
        if (compIdx < 0 || compIdx >= mComponents.size() || dataIdx < 0)
            return;
        mComponents[compIdx].selection.addDataRange(QCPDataRange(dataIdx, dataIdx + 1), false);
        mComponents[compIdx].selection.simplify();
    }

    updateBaseSelection();
    if (selectionStateChanged)
        *selectionStateChanged = true;
}

void QCPMultiGraph::deselectEvent(bool* selectionStateChanged)
{
    bool changed = false;
    for (auto& c : mComponents) {
        if (!c.selection.isEmpty()) {
            c.selection = QCPDataSelection();
            changed = true;
        }
    }
    if (changed) {
        updateBaseSelection();
        if (selectionStateChanged)
            *selectionStateChanged = true;
    }
}

// --- Drawing ---

bool QCPMultiGraph::canProduceContent() const
{
    if (!mKeyAxis || !mValueAxis || !mDataSource || mDataSource->empty())
        return false;
    // Pipeline active but L1 not ready — draw() will bail out
    if (mNeedsResampling && !mL1Cache && !mL2Result)
        return false;
    return true;
}

QPointF QCPMultiGraph::stallPixelOffset() const
{
    // Dirty lines (the pan debounce fired, or data changed) must be drawn again: offering a
    // translation would let the layer shift the stale texture forever, edges left blank.
    if (!mHasRenderedRange || mCachedLines.isEmpty() || mLineCacheDirty || !mKeyAxis || !mValueAxis)
        return {};
    // Only valid for pure translation (pan) — reject if zoom changed
    // Scale-aware: a pan on a log axis multiplies both ends, which changes the linear size.
    const double keyRatio = qcp::axisRangeSizeRatio(mKeyAxis.data(), mRenderedRange.key);
    const double valRatio = qcp::axisRangeSizeRatio(mValueAxis.data(), mRenderedRange.value);
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

void QCPMultiGraph::draw(QCPPainter* painter)
{
    PROFILE_HERE_N("QCPMultiGraph::draw");
    if (!mKeyAxis || !mValueAxis || !mDataSource || mDataSource->empty())
        return;
    if (mKeyAxis->range().size() <= 0)
        return;

    // Export path: synchronous fallback when no L1 cache yet
    if (!mL1Cache && mNeedsResampling
        && painter->modes().testFlag(QCPPainter::pmNoCaching))
    {
        auto vp = ViewportParams::fromAxes(mKeyAxis.data(), mValueAxis.data());
        mPipeline.runSynchronously(vp);
        onL1Ready(mPipeline.currentGeneration());
    }

    // Data source priority: L2 (viewport-optimized) > raw
    // When L1 exists but L2 is null (sparse enough to draw directly), use raw source.
    // Note: L2 rebuild is deferred until we know fresh lines are needed (below).
    QCPAbstractMultiDataSource* ds = nullptr;
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

    const QCPRange keyRange = mKeyAxis->range();
    int begin = ds->findBegin(keyRange.lower);
    int end = ds->findEnd(keyRange.upper);
    if (begin >= end)
        return;

    const bool keyIsVertical = mKeyAxis->orientation() == Qt::Vertical;
    const int pixelWidth = keyIsVertical
        ? static_cast<int>(mKeyAxis->axisRect()->height())
        : static_cast<int>(mKeyAxis->axisRect()->width());

    // --- Line caching with GPU translation ---
    const QSize currentPlotSize(mKeyAxis->axisRect()->width(), mKeyAxis->axisRect()->height());
    const bool isExportMode = painter->isExporting();
    auto [needFreshLines, gpuOffset] = qcp::evaluateLineCache(
        mLineCacheDirty, mCachedLines.isEmpty(),
        currentPlotSize, mCachedPlotSize,
        mHasRenderedRange, mRenderedRange.key, mRenderedRange.value,
        mKeyAxis.data(), mValueAxis.data(), isExportMode);

    // Rebuild L2 only when fresh lines are needed — on cache-hit pan frames
    // the previous L2 result + GPU offset is sufficient.
    if (needFreshLines && mL2Dirty && mL1Cache)
    {
        rebuildL2(ViewportParams::fromAxes(mKeyAxis.data(), mValueAxis.data()));
        mL2Dirty = false;
        // Re-resolve ds in case L2 changed (e.g. became null for sparse data)
        if (mL2Result)
            ds = mL2Result.get();
        else if (mL1Cache)
            ds = mDataSource.get();
    }

    // Sources are replaced on every data update: the graph's setting is applied at draw time.
    ds->setGapThreshold(mGapThreshold);

    const bool coloured = mColor.hasValues() && (ds == mDataSource.get() || mL2HasOrigin);

    QVector<QVector<QPointF>> exportLines;
    QVector<QVector<int>> exportIndices;
    auto& linesTarget = isExportMode ? exportLines : mCachedLines;
    auto& indicesTarget = isExportMode ? exportIndices : mCachedIndices;

    const auto [cacheBegin, cacheEnd] = lineCacheWindow(*ds);
    if (needFreshLines)
    {
        // The pending source may carry more columns than the displayed one
        // (mComponents was already synced to it) — clamp so a mid-window
        // replot never reads past the displayed source's columns.
        const int nc = qMin(mComponents.size(), ds->columnCount());
        resizePerColumn(nc, linesTarget, indicesTarget, coloured);

        bool allVisible = true;
        for (int c = 0; c < nc; ++c)
            if (!mComponents[c].visible) { allVisible = false; break; }

        if (coloured)
        {
            for (int c = 0; c < nc; ++c)
            {
                if (!mComponents[c].visible) { linesTarget[c].clear(); indicesTarget[c].clear(); continue; }
                linesTarget[c] = (mAdaptiveSampling && !mL2Result)
                    ? ds->getOptimizedLineDataIndexed(c, cacheBegin, cacheEnd, pixelWidth,
                                                      mKeyAxis.data(), mValueAxis.data(), indicesTarget[c])
                    : ds->getLinesIndexed(c, cacheBegin, cacheEnd,
                                          mKeyAxis.data(), mValueAxis.data(), indicesTarget[c]);
            }
        }
        else if (allVisible && nc > 1 && !mAdaptiveSampling)
        {
            ds->getLinesAll(cacheBegin, cacheEnd,
                            mKeyAxis.data(), mValueAxis.data(),
                            linesTarget.data(), nc);
        }
        else
        {
            for (int c = 0; c < nc; ++c)
            {
                if (!mComponents[c].visible) { linesTarget[c].clear(); continue; }
                if (mAdaptiveSampling)
                    linesTarget[c] = ds->getOptimizedLineData(c, cacheBegin, cacheEnd, pixelWidth,
                                                               mKeyAxis.data(), mValueAxis.data());
                else
                    linesTarget[c] = ds->getLines(c, cacheBegin, cacheEnd, mKeyAxis.data(), mValueAxis.data());
            }
        }
        if (!isExportMode)
        {
            mRenderedRange = {mKeyAxis->range(), mValueAxis->range()};
            mHasRenderedRange = true;
            mLineCacheDirty = false;
            mCachedPlotSize = currentPlotSize;
            for (auto& ec : mExtrusionCaches) ec.clear();
            for (auto& fill : mFills) fill.verts.clear();
            for (auto& bars : mErrorBars) { bars.bars.clear(); bars.band.clear(); }
            gpuOffset = {};
        }
    }

    drawFills(painter, *ds, cacheBegin, cacheEnd, needFreshLines, gpuOffset, isExportMode);
    drawErrorBars(painter, *ds, ds == mDataSource.get(), cacheBegin, cacheEnd, needFreshLines,
                  gpuOffset, isExportMode);

    for (int c = 0; c < mComponents.size(); ++c) {
        const auto& comp = mComponents[c];
        if (!comp.visible) continue;
        if (comp.lineStyle == lsNone && comp.scatterStyle.isNone()) continue;

        if (c >= linesTarget.size() || c >= mExtrusionCaches.size()) continue;
        const QVector<QPointF>& dataLines = linesTarget[c];
        if (dataLines.isEmpty()) continue;
        // Defensive: indices that do not match the lines are never used.
        const QVector<int>* dataIdx = (coloured && c < indicesTarget.size()
                                       && indicesTarget[c].size() == dataLines.size())
            ? &indicesTarget[c] : nullptr;

        // Only compute step-transform when the extrusion cache needs rebuilding —
        // on cache-hit pan frames, drawPolylineCached ignores pts entirely.
        // A coloured component may re-extrude for reasons only drawColoredPolylineCached
        // knows (pen width, colour generation), so it always gets its styled points.
        const QPen& activePen = comp.selection.isEmpty() ? comp.pen : comp.selectedPen;
        QVector<QPointF> styledLines;
        const bool needStyledLines = dataIdx != nullptr
            || qcp::needsReextrusion(mExtrusionCaches[c], needFreshLines, activePen,
                                     mParentPlot->bufferDevicePixelRatio());
        if (needStyledLines && comp.lineStyle != lsNone && comp.lineStyle != lsLine) {
            switch (comp.lineStyle) {
                case lsStepLeft:   styledLines = qcp::toStepLeftLines(dataLines, keyIsVertical); break;
                case lsStepRight:  styledLines = qcp::toStepRightLines(dataLines, keyIsVertical); break;
                case lsStepCenter: styledLines = qcp::toStepCenterLines(dataLines, keyIsVertical); break;
                case lsImpulse:    styledLines = qcp::toImpulseLines(dataLines, keyIsVertical, mValueAxis->coordToPixel(0)); break;
                default: break;
            }
        }
        const QVector<QPointF>& lines = styledLines.isEmpty() ? dataLines : styledLines;

        if (comp.lineStyle != lsNone) {
            if (comp.lineStyle == lsImpulse) {
                applyDefaultAntialiasingHint(painter);
                QPen impulsePen = activePen;
                impulsePen.setCapStyle(Qt::FlatCap);
                if (dataIdx)
                    drawColoredImpulses(painter, lines, qcp::impulseIndices(*dataIdx), impulsePen, gpuOffset);
                else {
                    painter->setPen(impulsePen);
                    if (!gpuOffset.isNull())
                        painter->translate(gpuOffset);
                    painter->drawLines(lines);
                    if (!gpuOffset.isNull())
                        painter->translate(-gpuOffset);
                }
            } else if (dataIdx) {
                applyDefaultAntialiasingHint(painter);
                if (!isExportMode)
                    qcp::drawColoredPolylineCached(painter, mParentPlot, mLayer, lines,
                                                   lineIndices(comp.lineStyle, *dataIdx), mColor, activePen,
                                                   gpuOffset, clipRect(), needFreshLines,
                                                   mExtrusionCaches[c]);
                else
                    qcp::drawColoredPolylineRuns(painter, lines,
                                                 qcp::colorRuns(lines, lineIndices(comp.lineStyle, *dataIdx), mColor),
                                                 mColor, activePen, gpuOffset);
            } else {
                applyDefaultAntialiasingHint(painter);
                if (!isExportMode) {
                    qcp::drawPolylineCached(painter, mParentPlot, mLayer, lines,
                                             activePen, gpuOffset, clipRect(),
                                             needFreshLines, mExtrusionCaches[c]);
                } else {
                    qcp::drawPolylineWithGpuFallback(painter, mParentPlot, mLayer, lines,
                                                      activePen, gpuOffset, clipRect());
                }
            }
        }

        // Draw scatters at original data positions (not step-transformed corners)
        if (!comp.scatterStyle.isNone()) {
            if (dataIdx) {
                drawColoredScatters(painter, dataLines, *dataIdx, comp, gpuOffset, isExportMode);
            } else {
                drawPlainScatters(painter, dataLines, comp, gpuOffset, isExportMode);
            }
        }
    }
}

void QCPMultiGraph::resizePerColumn(int nc, QVector<QVector<QPointF>>& lines,
                                    QVector<QVector<int>>& indices, bool coloured)
{
    lines.resize(nc);
    indices.resize(coloured ? nc : 0);
    mExtrusionCaches.resize(nc);
}

std::pair<int, int> QCPMultiGraph::lineCacheWindow(const QCPAbstractMultiDataSource& ds) const
{
    const QCPRange cacheRange = qcp::algo::lineCacheKeyRange(
        mKeyAxis->range(), mKeyAxis->scaleType() == QCPAxis::stLogarithmic);
    return {ds.findBegin(cacheRange.lower), ds.findEnd(cacheRange.upper)};
}

bool QCPMultiGraph::fillDrawable(const Fill& fill, int columnCount) const
{
    const auto shown = [&](int column) {
        return column >= 0 && column < columnCount && column < mComponents.size()
            && mComponents[column].visible;
    };
    const auto* target = std::get_if<int>(&fill.target);
    return shown(fill.owner) && mComponents[fill.owner].lineStyle != lsImpulse
        && (!target || shown(*target));
}

// A log axis has no pixel for a value <= 0: the baseline goes two axis lengths past the edge on
// the side of zero, as QCPGraph::getFillBasePoint picks it. Far enough to stay off screen during
// a GPU pan, near enough to keep float precision; the scissor clips it.
double QCPMultiGraph::fillBaselinePixel(double value) const
{
    if (mValueAxis->scaleType() == QCPAxis::stLinear || value > 0)
        return mValueAxis->coordToPixel(value);
    const auto [edge, other] = logZeroSideEdges();
    return edge + 2.0 * (edge - other);
}

// simplify: a value <= 0 is taken as invalid on every log axis, but on an all-negative range the
// invalid values are the ones >= 0. Callers then push valid negative values to the edge. Upgrade:
// test the value's sign against the range's sign in fillBaselinePixel and errorPixel.
std::pair<double, double> QCPMultiGraph::logZeroSideEdges() const
{
    const QCPRange range = mValueAxis->range();
    const bool positive = range.upper > 0;
    return {mValueAxis->coordToPixel(positive ? range.lower : range.upper),
            mValueAxis->coordToPixel(positive ? range.upper : range.lower)};
}

qcp::fill::Step QCPMultiGraph::ownerStep(const Component& owner)
{
    switch (owner.lineStyle) {
        case lsStepLeft:   return qcp::fill::Step::Left;
        case lsStepRight:  return qcp::fill::Step::Right;
        case lsStepCenter: return qcp::fill::Step::Center;
        default:           return qcp::fill::Step::None;
    }
}

std::vector<qcp::fill::StripPoint> QCPMultiGraph::fillRows(const Fill& fill,
                                                           const QCPAbstractMultiDataSource& ds,
                                                           int begin, int end) const
{
    const auto gaps = ds.gapsBefore(begin, end);
    const auto* targetColumn = std::get_if<int>(&fill.target);
    const double constantPixel = targetColumn ? 0.0 : fillBaselinePixel(std::get<double>(fill.target));
    std::vector<qcp::fill::StripPoint> rows;
    rows.reserve(std::max(0, end - begin));
    for (int i = begin; i < end; ++i)
        rows.push_back({mKeyAxis->coordToPixel(ds.keyAt(i)),
                        mValueAxis->coordToPixel(ds.valueAt(fill.owner, i)),
                        targetColumn ? mValueAxis->coordToPixel(ds.valueAt(*targetColumn, i))
                                     : constantPixel,
                        gaps.size() > i - begin && gaps[i - begin] != 0});
    return rows;
}

// Binned when the rows outnumber 4 per pixel, the lines' own L2 density (kLevel2PixelMultiplier).
std::vector<qcp::fill::StripPoint> QCPMultiGraph::shapedFillRows(
    const Fill& fill, const QCPAbstractMultiDataSource& ds, int begin, int end) const
{
    return qcp::fill::stepped(binnedWhenDense(fillRows(fill, ds, begin, end)),
                              ownerStep(mComponents[fill.owner]));
}

std::vector<qcp::fill::StripPoint> QCPMultiGraph::binnedWhenDense(
    std::vector<qcp::fill::StripPoint> rows) const
{
    const QRect rect = mKeyAxis->axisRect()->rect();
    const int pixelLength = mKeyAxis->orientation() == Qt::Vertical ? rect.height() : rect.width();
    if (rows.size() > 4 * std::size_t(std::max(0, pixelLength)))
        return qcp::fill::binToSpans(rows, 0.25);
    return rows;
}

QCPPlottableRhiLayer* QCPMultiGraph::gpuLayerFor(bool isExportMode) const
{
    return mParentPlot && mParentPlot->rhi() && !isExportMode
        ? mParentPlot->plottableRhiLayer(mLayer) : nullptr;
}

void QCPMultiGraph::drawFills(QCPPainter* painter, const QCPAbstractMultiDataSource& ds, int begin,
                              int end, bool fresh, const QPointF& gpuOffset, bool isExportMode)
{
    auto* gpuLayer = gpuLayerFor(isExportMode);
    for (auto& entry : mFills) {
        if (!fillDrawable(entry.spec, ds.columnCount())) continue;
        const QBrush brush = effectiveFillBrush(entry.spec);
        if (brush.style() == Qt::NoBrush || brush.color().alpha() == 0) continue;
        // The GPU fill is one flat colour: gradients and patterns are painted with QPainter.
        if (gpuLayer && brush.style() == Qt::SolidPattern)
            drawFillOnGpu(painter, gpuLayer, entry.verts, brush.color(),
                          fresh ? shapedFillRows(entry.spec, ds, begin, end)
                                : std::vector<qcp::fill::StripPoint>{},
                          fresh, gpuOffset);
        else
            paintFill(painter, brush, shapedFillRows(entry.spec, ds, begin, end));
    }
}

// rows are read only on fresh frames: pan frames translate the cached verts.
void QCPMultiGraph::drawFillOnGpu(QCPPainter* painter, QCPPlottableRhiLayer* layer,
                                  std::vector<float>& verts, const QColor& color,
                                  std::span<const qcp::fill::StripPoint> rows, bool fresh,
                                  const QPointF& gpuOffset)
{
    if (fresh)
        qcp::fill::appendStrip(rows, mKeyAxis->orientation() == Qt::Vertical,
                               qcp::rhi::premultipliedColor(color), verts);
    if (verts.empty())
        return;
    layer->addPlottable(verts, {}, clipRect(), mParentPlot->bufferDevicePixelRatio(),
                        mParentPlot->rhiOutputSize().height(), float(gpuOffset.x()),
                        float(gpuOffset.y()), float(painter->opacity()));
}

// Built from the current axes on every frame, so unlike the cached GPU vertices it needs no pan
// offset.
// simplify: O(rows in the cache window) per frame. Bounded by the binning to 4 rows per pixel, but
// the rows are still walked: raw sources below the resample threshold, or any source on a log key
// axis. Upgrade: cache the shaped rows per fill, like the GPU vertices, and translate them.
void QCPMultiGraph::paintFill(QCPPainter* painter, const QBrush& brush,
                              std::span<const qcp::fill::StripPoint> rows) const
{
    painter->save();
    applyFillAntialiasingHint(painter);
    painter->setPen(Qt::NoPen);
    painter->setBrush(brush);
    for (const QPolygonF& polygon : qcp::fill::stripPolygons(rows, mKeyAxis->orientation() == Qt::Vertical))
        painter->drawPolygon(polygon);
    painter->restore();
}

bool QCPMultiGraph::errorBarsDrawable(const ErrorBars& spec, int columnCount) const
{
    const int last = std::min<int>(columnCount, mComponents.size());
    return std::max({spec.value, spec.lower, spec.upper}) < last && mComponents[spec.value].visible;
}

QPen QCPMultiGraph::errorBarPen(const ErrorBars& spec) const
{
    return spec.pen.value_or(mComponents[spec.value].pen);
}

// A log axis has no pixel for a value <= 0: a bound there stops at the axis edge.
double QCPMultiGraph::errorPixel(double value) const
{
    if (mValueAxis->scaleType() == QCPAxis::stLinear || value > 0 || std::isnan(value))
        return mValueAxis->coordToPixel(value);
    return logZeroSideEdges().first;
}

static bool bothFinite(double a, double b) { return std::isfinite(a) && std::isfinite(b); }
static double lowerAtMost(double lower, double y) { return bothFinite(lower, y) ? std::min(lower, y) : lower; }
static double upperAtLeast(double upper, double y) { return bothFinite(upper, y) ? std::max(upper, y) : upper; }

std::vector<qcp::errorbars::Bar> QCPMultiGraph::errorBarRows(
    const ErrorBars& spec, const QCPAbstractMultiDataSource& ds, int begin, int end) const
{
    std::vector<qcp::errorbars::Bar> bars;
    bars.reserve(std::max(0, end - begin));
    for (int i = begin; i < end; ++i)
    {
        const double y = ds.valueAt(spec.value, i);
        bars.push_back({mKeyAxis->coordToPixel(ds.keyAt(i)), errorPixel(y),
                        errorPixel(lowerAtMost(ds.valueAt(spec.lower, i), y)),
                        errorPixel(upperAtLeast(ds.valueAt(spec.upper, i), y))});
    }
    return bars;
}

bool QCPMultiGraph::errorBarsFitInView(std::span<const qcp::errorbars::Bar> bars,
                                       double minSpacingPx) const
{
    const QRect rect = mKeyAxis->axisRect()->rect();
    const bool vertical = mKeyAxis->orientation() == Qt::Vertical;
    const double first = vertical ? rect.top() : rect.left();
    const double last = vertical ? rect.bottom() : rect.right();
    std::vector<qcp::errorbars::Bar> shown;
    for (qcp::errorbars::Bar bar : bars)
        if (bar.key >= first && bar.key <= last && qcp::errorbars::resolve(bar))
            shown.push_back(bar);
    return qcp::errorbars::barsFit(shown, minSpacingPx);
}

std::vector<qcp::fill::StripPoint> QCPMultiGraph::errorBandRows(
    std::span<const qcp::errorbars::Bar> bars, const QCPAbstractMultiDataSource& ds, int begin,
    int end) const
{
    const auto gaps = ds.gapsBefore(begin, end);
    std::vector<qcp::fill::StripPoint> rows;
    rows.reserve(bars.size());
    for (int k = 0; k < int(bars.size()); ++k)
    {
        qcp::errorbars::Bar bar = bars[k];
        qcp::errorbars::resolve(bar);
        rows.push_back({bar.key, bar.lower, bar.upper, gaps.size() > k && gaps[k] != 0});
    }
    return binnedWhenDense(std::move(rows));
}

// Pan frames reuse the mode and the geometry of the last fresh frame, translated on the GPU;
// only a QPainter band is rebuilt from the current axes on every frame, like a fill.
void QCPMultiGraph::drawErrorBars(QCPPainter* painter, const QCPAbstractMultiDataSource& ds,
                                  bool rawRows, int begin, int end, bool fresh,
                                  const QPointF& gpuOffset, bool isExportMode)
{
    auto* gpuLayer = gpuLayerFor(isExportMode);
    const bool rebuild = fresh || isExportMode;
    for (auto& entry : mErrorBars) {
        if (!errorBarsDrawable(entry.spec, ds.columnCount())) continue;
        const bool needRows = rebuild || (entry.asBand && !gpuLayer);
        const auto bars = needRows ? errorBarRows(entry.spec, ds, begin, end)
                                   : std::vector<qcp::errorbars::Bar>{};
        const bool asBand = rebuild
            ? !(rawRows && errorBarsFitInView(bars, 3 * entry.spec.capWidthPx))
            : entry.asBand;
        if (!isExportMode)
            entry.asBand = asBand;
        if (asBand)
            drawBarsAsBand(painter, gpuLayer, entry,
                           needRows ? errorBandRows(bars, ds, begin, end)
                                    : std::vector<qcp::fill::StripPoint>{},
                           fresh, gpuOffset);
        else
            drawBarsAsPolylines(painter, entry, bars, fresh, gpuOffset, isExportMode);
    }
}

void QCPMultiGraph::drawBarsAsPolylines(QCPPainter* painter, ErrorBarsEntry& entry,
                                        std::span<const qcp::errorbars::Bar> bars, bool fresh,
                                        const QPointF& gpuOffset, bool isExportMode)
{
    const Component& comp = mComponents[entry.spec.value];
    const QPen pen = comp.selection.isEmpty() ? errorBarPen(entry.spec) : comp.selectedPen;
    const bool keyIsVertical = mKeyAxis->orientation() == Qt::Vertical;
    const double capHalfWidth = entry.spec.capWidthPx / 2;
    applyDefaultAntialiasingHint(painter);
    if (isExportMode) {
        qcp::drawPolylineWithGpuFallback(painter, mParentPlot, mLayer,
                                         qcp::errorbars::barRuns(bars, capHalfWidth, keyIsVertical),
                                         pen, gpuOffset, clipRect());
        return;
    }
    if (fresh)
        entry.runs = qcp::errorbars::barRuns(bars, capHalfWidth, keyIsVertical);
    qcp::drawPolylineCached(painter, mParentPlot, mLayer, entry.runs, pen, gpuOffset, clipRect(),
                            fresh, entry.bars);
}

void QCPMultiGraph::drawBarsAsBand(QCPPainter* painter, QCPPlottableRhiLayer* gpuLayer,
                                   ErrorBarsEntry& entry,
                                   std::span<const qcp::fill::StripPoint> rows, bool fresh,
                                   const QPointF& gpuOffset)
{
    // Not the selected colour: the GPU band bakes its colour in, and selecting would rebuild it.
    QColor color = errorBarPen(entry.spec).color();
    color.setAlpha(64);
    if (gpuLayer)
        drawFillOnGpu(painter, gpuLayer, entry.band, color, rows, fresh, gpuOffset);
    else
        paintFill(painter, QBrush(color), rows);
}

void QCPMultiGraph::drawLegendIcon(QCPPainter* painter, const QRectF& rect) const
{
    applyDefaultAntialiasingHint(painter);
    const QVector<int> shown = legendComponents();
    if (shown.isEmpty()) return;

    double segWidth = rect.width() / shown.size();
    double y = rect.center().y();
    for (int k = 0; k < shown.size(); ++k) {
        if (!mComponents[shown[k]].visible) continue;
        double x0 = rect.left() + k * segWidth;
        double x1 = x0 + segWidth;
        drawComponentLegendIcon(painter, shown[k], QLineF(x0, y, x1, y));
    }
}

static void drawErrorBarGlyph(QCPPainter* painter, const QPointF& center, const QPen& pen)
{
    constexpr double halfStem = 5, halfCap = 3;
    painter->setPen(pen);
    painter->drawLine(QLineF(center.x(), center.y() - halfStem, center.x(), center.y() + halfStem));
    for (const double y : {center.y() - halfStem, center.y() + halfStem})
        painter->drawLine(QLineF(center.x() - halfCap, y, center.x() + halfCap, y));
}

static QRectF legendSwatchRect(const QLineF& line)
{
    constexpr double swatchHeight = 12; // ~60 % of a legend row at the default font
    return QRectF(line.x1(), line.y1() - swatchHeight / 2, line.length(), swatchHeight);
}

void QCPMultiGraph::drawComponentLegendIcon(QCPPainter* painter, int component, const QLineF& line) const
{
    const Component& comp = mComponents[component];
    if (const auto fill = componentFill(component))
        painter->fillRect(legendSwatchRect(line), effectiveFillBrush(*fill));
    QPen pen = comp.pen;
    if (mColor.hasValues())
    {
        QLinearGradient gradient(line.p1(), line.p2());
        for (int s = 0; s <= 4; ++s)
            gradient.setColorAt(s / 4.0, qcp::runColor(mColor, s * 255 / 4));
        pen.setBrush(gradient);
    }
    painter->setPen(pen);
    if (comp.lineStyle != lsNone)
        painter->drawLine(line);
    if (!comp.scatterStyle.isNone())
    {
        QCPScatterStyle icon = comp.scatterStyle;
        icon.setSize(qMin(icon.size(), line.length() * 0.5));
        icon.applyTo(painter, pen);
        icon.drawShape(painter, line.center().x(), line.center().y());
    }
    if (const auto bars = componentErrorBars(component))
        drawErrorBarGlyph(painter, line.center(), errorBarPen(*bars));
}

// --- Legend ---

bool QCPMultiGraph::addToLegend(QCPLegend* legend)
{
    if (!legendAllowed())
        return false;
    if (!legend) {
        if (mParentPlot)
            legend = mParentPlot->legend;
        else
            return false;
    }
    if (legend->parentPlot() != mParentPlot)
        return false;
    for (int i = 0; i < legend->itemCount(); ++i)
        if (auto* gi = qobject_cast<QCPGroupLegendItem*>(legend->item(i)))
            if (gi->multiGraph() == this)
                return false;
    auto* item = new QCPGroupLegendItem(legend, this);
    return legend->addItem(item);
}

bool QCPMultiGraph::removeFromLegend(QCPLegend* legend) const
{
    if (!legend) {
        if (mParentPlot)
            legend = mParentPlot->legend;
        else
            return false;
    }
    for (int i = 0; i < legend->itemCount(); ++i) {
        if (auto* groupItem = qobject_cast<QCPGroupLegendItem*>(legend->item(i))) {
            if (groupItem->multiGraph() == this)
                return legend->removeItem(i);
        }
    }
    return false;
}

