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
#include "../vector2d.h"

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
                this, [this] { mLineCacheDirty = true; mCachedLines.clear(); });
    }
    if (valueAxis)
    {
        connect(valueAxis, &QCPAxis::scaleTypeChanged,
                this, [this] { mLineCacheDirty = true; mCachedLines.clear(); });
    }

    connect(&mPipeline, &QCPMultiGraphPipeline::finished,
            this, [this](uint64_t) { onL1Ready(); });
    connect(&mPipeline, &QCPMultiGraphPipeline::busyChanged,
            this, [this](bool) { updateEffectiveBusy(); });

    mViewportDebounce.setSingleShot(true);
    mViewportDebounce.setInterval(150);
    connect(&mViewportDebounce, &QTimer::timeout, this, [this] {
        mLineCacheDirty = true;
        if (mParentPlot)
            mParentPlot->replot(QCustomPlot::rpQueuedReplot);
    });
}

QCPMultiGraph::~QCPMultiGraph() = default;

void QCPMultiGraph::setDataSource(std::unique_ptr<QCPAbstractMultiDataSource> source)
{
    setDataSource(std::shared_ptr<QCPAbstractMultiDataSource>(std::move(source)));
}

static void ensureL1TransformMulti(QCPMultiGraphPipeline& pipeline, int sourceSize, int colCount)
{
    const bool needsResampling = colCount > 0
        && static_cast<int64_t>(sourceSize) * colCount >= qcp::algo::kResampleThreshold;
    if (needsResampling)
    {
        if (!pipeline.hasTransform())
        {
            pipeline.setTransform(TransformKind::ViewportIndependent,
                [](const QCPAbstractMultiDataSource& src,
                   const ViewportParams& vp,
                   std::any& cache) -> std::shared_ptr<QCPAbstractMultiDataSource> {
                    return qcp::algo::buildL1CacheMulti(src, vp, cache);
                });
        }
    }
    else if (pipeline.hasTransform())
    {
        pipeline.clearTransform();
    }
}

void QCPMultiGraph::setDataSource(std::shared_ptr<QCPAbstractMultiDataSource> source)
{
    mDataSource = std::move(source);
    syncComponentCount();
    mL1Cache.reset();
    mL2Result.reset();
    mCachedLines.clear();
    mL2Dirty = false;
    mLineCacheDirty = true;

    if (mDataSource)
    {
        mNeedsResampling = mDataSource->columnCount() > 0
            && static_cast<int64_t>(mDataSource->size()) * mDataSource->columnCount()
               >= qcp::algo::kResampleThreshold;
        ensureL1TransformMulti(mPipeline, mDataSource->size(), mDataSource->columnCount());
    }
    else
    {
        mNeedsResampling = false;
    }
    mPipeline.setSource(mDataSource);
}

void QCPMultiGraph::dataChanged()
{
    mLineCacheDirty = true;
    if (mDataSource)
    {
        mNeedsResampling = mDataSource->columnCount() > 0
            && static_cast<int64_t>(mDataSource->size()) * mDataSource->columnCount()
               >= qcp::algo::kResampleThreshold;
        ensureL1TransformMulti(mPipeline, mDataSource->size(), mDataSource->columnCount());
    }

    mL1Cache.reset();
    mL2Dirty = false;

    if (mPipeline.hasTransform())
        mPipeline.onDataChanged();
    else if (mParentPlot)
        mParentPlot->replot();
}

void QCPMultiGraph::onL1Ready()
{
    qcp::extractL1Cache<qcp::algo::MultiGraphResamplerCache>(mPipeline.cache(), mL1Cache, mL2Dirty);
    mLineCacheDirty = true;
    if (parentPlot())
        parentPlot()->replot(QCustomPlot::rpQueuedReplot);
}

void QCPMultiGraph::rebuildL2(const ViewportParams& vp)
{
    if (!mL1Cache) return;
    mL2Result = qcp::algo::resampleL2Multi(*mL1Cache, vp);
}

void QCPMultiGraph::onViewportChanged()
{
    if (mL1Cache)
    {
        mL2Dirty = true;
        if (mHasRenderedRange && mKeyAxis)
        {
            double ratio = mKeyAxis->range().size() / mRenderedRange.key.size();
            if (qAbs(ratio - 1.0) < 1e-4)
                mViewportDebounce.start();
        }
    }
}

void QCPMultiGraph::syncComponentCount()
{
    int newCount = mDataSource ? mDataSource->columnCount() : 0;
    int oldCount = mComponents.size();
    if (newCount > oldCount) {
        mComponents.resize(newCount);
        for (int i = oldCount; i < newCount; ++i) {
            auto& c = mComponents[i];
            QColor color = sDefaultColors[i % sDefaultColors.size()];
            c.pen = QPen(color, 0);
            c.selectedPen = defaultSelectedPen(c.pen);
            c.name = QString("Component %1").arg(i);
        }
    } else if (newCount < oldCount) {
        mComponents.resize(newCount);
    }
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
}

void QCPMultiGraph::setComponentPens(const QList<QPen>& pens)
{
    int n = qMin(pens.size(), mComponents.size());
    for (int i = 0; i < n; ++i) {
        mComponents[i].pen = pens[i];
        mComponents[i].selectedPen = defaultSelectedPen(pens[i]);
    }
}

double QCPMultiGraph::componentValueAt(int column, int index) const
{
    return mDataSource ? mDataSource->valueAt(column, index) : 0.0;
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
    for (int c = 0; c < mComponents.size(); ++c) {
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
    if (mLineStyle != lsNone && lo < hi) {
        for (int c = 0; c < nComponents; ++c) {
            if (!mComponents[c].visible) continue;
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
    for (int c = 0; c < mComponents.size(); ++c) {
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

void QCPMultiGraph::draw(QCPPainter* painter)
{
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
        onL1Ready();
    }

    // Data source priority: L2 (viewport-optimized) > raw
    // When L1 exists but L2 is null (sparse enough to draw directly), use raw source.
    // Note: L2 rebuild is deferred until we know fresh lines are needed (below).
    const QCPAbstractMultiDataSource* ds = nullptr;
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
    const bool isExportMode = painter->modes().testFlag(QCPPainter::pmNoCaching)
                            || painter->modes().testFlag(QCPPainter::pmVectorized);
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

    QVector<QVector<QPointF>> exportLines;
    auto& linesTarget = isExportMode ? exportLines : mCachedLines;

    if (needFreshLines)
    {
        // Expand data range by 100% on each side so GPU-translated pans
        // don't expose uncovered edges before the rebuild threshold triggers.
        const double margin = keyRange.size() * 1.0;
        int cacheBegin = ds->findBegin(keyRange.lower - margin);
        int cacheEnd = ds->findEnd(keyRange.upper + margin);

        linesTarget.resize(mComponents.size());
        for (int c = 0; c < mComponents.size(); ++c)
        {
            if (!mComponents[c].visible) { linesTarget[c].clear(); continue; }
            if (mAdaptiveSampling && !mL2Result)
                linesTarget[c] = ds->getOptimizedLineData(c, cacheBegin, cacheEnd, pixelWidth,
                                                           mKeyAxis.data(), mValueAxis.data());
            else
                linesTarget[c] = ds->getLines(c, cacheBegin, cacheEnd, mKeyAxis.data(), mValueAxis.data());
        }
        if (!isExportMode)
        {
            mRenderedRange = {mKeyAxis->range(), mValueAxis->range()};
            mHasRenderedRange = true;
            mLineCacheDirty = false;
            mCachedPlotSize = currentPlotSize;
            for (auto& ec : mExtrusionCaches) ec.clear();
            gpuOffset = {};
        }
    }

    mExtrusionCaches.resize(mComponents.size());

    for (int c = 0; c < mComponents.size(); ++c) {
        const auto& comp = mComponents[c];
        if (!comp.visible) continue;
        if (mLineStyle == lsNone && comp.scatterStyle.isNone()) continue;

        if (c >= linesTarget.size()) continue;
        const QVector<QPointF>& dataLines = linesTarget[c];
        if (dataLines.isEmpty()) continue;

        // Only compute step-transform when the extrusion cache needs rebuilding —
        // on cache-hit pan frames, drawPolylineCached ignores pts entirely.
        QVector<QPointF> styledLines;
        const bool needStyledLines = needFreshLines || mExtrusionCaches[c].isEmpty();
        if (needStyledLines && mLineStyle != lsNone && mLineStyle != lsLine) {
            switch (mLineStyle) {
                case lsStepLeft:   styledLines = qcp::toStepLeftLines(dataLines, keyIsVertical); break;
                case lsStepRight:  styledLines = qcp::toStepRightLines(dataLines, keyIsVertical); break;
                case lsStepCenter: styledLines = qcp::toStepCenterLines(dataLines, keyIsVertical); break;
                case lsImpulse:    styledLines = qcp::toImpulseLines(dataLines, keyIsVertical, mValueAxis->coordToPixel(0)); break;
                default: break;
            }
        }
        const QVector<QPointF>& lines = styledLines.isEmpty() ? dataLines : styledLines;

        if (mLineStyle != lsNone) {
            const QPen& activePen = comp.selection.isEmpty() ? comp.pen : comp.selectedPen;
            if (mLineStyle == lsImpulse) {
                applyDefaultAntialiasingHint(painter);
                QPen impulsePen = activePen;
                impulsePen.setCapStyle(Qt::FlatCap);
                painter->setPen(impulsePen);
                painter->drawLines(lines);
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
}

void QCPMultiGraph::drawLegendIcon(QCPPainter* painter, const QRectF& rect) const
{
    applyDefaultAntialiasingHint(painter);
    if (mComponents.isEmpty()) return;

    int n = mComponents.size();
    double segWidth = rect.width() / n;
    double y = rect.center().y();
    for (int i = 0; i < n; ++i) {
        if (!mComponents[i].visible) continue;
        painter->setPen(mComponents[i].pen);
        double x0 = rect.left() + i * segWidth;
        double x1 = x0 + segWidth;
        painter->drawLine(QLineF(x0, y, x1, y));
    }
}

// --- Legend ---

bool QCPMultiGraph::addToLegend(QCPLegend* legend)
{
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

