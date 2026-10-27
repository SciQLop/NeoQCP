#include "plottable-histogram.h"
#include <QPointer>
#include "plottable-draw-utils.h"
#include <axis/axis.h>
#include <core.h>
#include <layer.h>
#include <layoutelements/layoutelement-axisrect.h>
#include <painting/line-extruder.h>
#include <painting/painter.h>
#include <painting/plottable-rhi-layer.h>
#include <painting/rhi-utils.h>
#include <Profiling.hpp>
#include <algorithm>
#include <limits>

using namespace qcp::algo;

namespace {

// Log bins need a positive range; BinAxis::make asserts it.
std::optional<QCPRange> usableRange(std::optional<QCPRange> range, bool log)
{
    if (range && log && range->lower <= 0)
        return std::nullopt;
    return range;
}

std::optional<QCPRange> binningRange(const QCPAbstractMultiDataSource& src,
                                     std::optional<QCPRange> fixed,
                                     std::optional<QCPRange> autoRange, bool log)
{
    std::optional<QCPRange> range = usableRange(fixed, log);
    if (!range)
    {
        const auto bounds = finiteValueBounds(src, log);
        if (!bounds)
            return std::nullopt;
        range = widenedRange(usableRange(autoRange, log), *bounds);
    }
    expandIfFlat(*range, log);
    return range;
}

bool validBinned(const std::vector<double>& edges, const std::vector<std::vector<double>>& heights)
{
    return edges.size() >= 2 && !heights.empty()
        && std::all_of(heights.begin(), heights.end(),
                       [&](const auto& column) { return column.size() + 1 == edges.size(); });
}

std::optional<double> smallestPositive(const std::vector<std::vector<double>>& heights)
{
    std::optional<double> lowest;
    for (const auto& column : heights)
        for (double v : column)
            if (v > 0 && (!lowest || v < *lowest))
                lowest = v;
    return lowest;
}

double largest(const std::vector<std::vector<double>>& heights)
{
    double highest = 0;
    for (const auto& column : heights)
        for (double v : column)
            highest = std::max(highest, v);
    return highest;
}

} // namespace

QCPHistogram::QCPHistogram(QCPAxis* keyAxis, QCPAxis* valueAxis)
    : QCPAbstractPlottable(keyAxis, valueAxis)
    , mPipeline(parentPlot() ? parentPlot()->pipelineScheduler() : nullptr, this)
{
    installTransform();
    connect(&mPipeline, &QCPHistogram1DPipeline::finished, this, &QCPHistogram::onPipelineFinished);
    connect(&mPipeline, &QCPHistogram1DPipeline::busyChanged,
            this, [this](bool) { updateEffectiveBusy(); });
    // Edges in one scale mean nothing in the other: re-fit them when the key axis toggles.
    if (keyAxis)
        connect(keyAxis, &QCPAxis::scaleTypeChanged, this, &QCPHistogram::resetEdges);
}

QCPHistogram::~QCPHistogram() = default;

bool QCPHistogram::keyLog() const
{
    return mKeyAxis && mKeyAxis->scaleType() == QCPAxis::stLogarithmic;
}

// The transform's scale alone is not enough: rememberEdges() reinstalls it for the current axis
// when an older job's result lands, and that result keeps its own scale.
bool QCPHistogram::scaleIsStale() const
{
    const bool shownStale = mDataSource && mResult && mResult->log != keyLog();
    return keyLog() != mBinnedKeyLog || shownStale;
}

bool QCPHistogram::autoEdges() const
{
    return !usableRange(mFixedRange, keyLog());
}

void QCPHistogram::installTransform()
{
    const int bins = mBins;
    const bool log = keyLog();
    mBinnedKeyLog = log;
    const auto normalization = mNormalization;
    const auto fixed = mFixedRange;
    const auto autoRange = mAutoRange;
    mPipeline.setTransform(TransformKind::ViewportIndependent,
        [bins, log, normalization, fixed, autoRange](const QCPAbstractMultiDataSource& src,
                                                     const ViewportParams&,
                                                     std::any&) -> std::shared_ptr<Histogram1D> {
            const auto range = binningRange(src, fixed, autoRange, log);
            // An empty histogram, not nullptr: the pipeline keeps its previous result on
            // nullptr, so "nothing to bin" would be indistinguishable from the old bars.
            if (!range)
                return std::make_shared<Histogram1D>();
            auto h = std::make_shared<Histogram1D>(bin1d(src, *range, bins, log));
            normalize(*h, normalization);
            return h;
        });
}

void QCPHistogram::refreshBinning()
{
    installTransform();
    // Its busyChanged slot may delete this histogram.
    const QPointer<QCPHistogram> alive(this);
    mPipeline.onDataChanged();
    if (alive)
        mAcceptedGeneration = mPipeline.currentGeneration();
}

// A job queued behind a running one captured the edges from before that job's result,
// so its result can be narrower than the edges now shown: bin it again, never shrink.
bool QCPHistogram::coversAutoEdges(const Histogram1D& h) const
{
    const auto known = autoEdges() ? usableRange(mAutoRange, keyLog()) : std::nullopt;
    return !known || (h.edges.front() <= known->lower && h.edges.back() >= known->upper);
}

void QCPHistogram::onPipelineFinished(uint64_t generation)
{
    if (generation < mAcceptedGeneration)
        return;
    mResetForLog.reset();
    auto result = pipelineResult();
    // A job started before the key scale changed: its bins mean nothing in the new scale.
    if (result && result->log != keyLog())
    {
        resetEdges();
        return;
    }
    if (result && !coversAutoEdges(*result))
    {
        refreshBinning();
        return;
    }
    takeResult(std::move(result));
}

std::shared_ptr<Histogram1D> QCPHistogram::pipelineResult() const
{
    const auto* result = mPipeline.result();
    if (!result || result->edges.empty())
        return nullptr;
    return std::make_shared<Histogram1D>(*result);
}

void QCPHistogram::setDataSource(std::shared_ptr<QCPAbstractMultiDataSource> source)
{
    mDataSource = std::move(source);
    const QPointer<QCPHistogram> alive(this);
    mPipeline.setSource(mDataSource);
    if (alive && !mDataSource)
        clearResult();
}

// No job runs for a null source, so nothing would replace the old bars; one still running for
// the old source must not land either.
void QCPHistogram::clearResult()
{
    mAcceptedGeneration = mPipeline.currentGeneration();
    takeResult(nullptr);
}

void QCPHistogram::setBinned(std::vector<double> edges, std::vector<std::vector<double>> heights)
{
    if (!validBinned(edges, heights))
    {
        qWarning() << "QCPHistogram::setBinned: every column needs edges.size() - 1 heights, ignored";
        return;
    }
    mDataSource.reset();
    mAutoRange.reset();
    installTransform();
    const QPointer<QCPHistogram> alive(this);
    mPipeline.setSource(nullptr);
    if (!alive)
        return;
    mAcceptedGeneration = mPipeline.currentGeneration();
    const std::size_t columns = heights.size();
    takeResult(std::make_shared<Histogram1D>(
        Histogram1D { std::move(edges), std::move(heights), std::vector<int>(columns, 0), keyLog() }));
}

void QCPHistogram::setBins(int bins)
{
    if (bins <= 0 || bins == mBins)
        return;
    mBins = bins;
    refreshBinning();
}

void QCPHistogram::setFixedRange(std::optional<QCPRange> range)
{
    if (range && keyLog() && range->lower <= 0)
    {
        qWarning() << "QCPHistogram::setFixedRange: log bins need a positive range, ignored";
        return;
    }
    mFixedRange = range;
    refreshBinning();
}

void QCPHistogram::setNormalization(HistogramNormalization norm)
{
    if (norm == mNormalization)
        return;
    mNormalization = norm;
    refreshBinning();
}

void QCPHistogram::setStyle(Style style)
{
    mStyle = style;
    requestReplot();
}

void QCPHistogram::setColumnColors(const QList<QColor>& colors)
{
    mColors = colors;
    requestReplot();
}

void QCPHistogram::resetEdges()
{
    mAutoRange.reset();
    refreshBinning();
}

bool QCPHistogram::storeResult(std::shared_ptr<Histogram1D> result)
{
    const bool edgesMoved = result && (!mResult || mResult->edges != result->edges);
    mResult = std::move(result);
    if (edgesMoved)
        rememberEdges();
    return edgesMoved;
}

// Repaints before it signals: a slot may delete this histogram.
void QCPHistogram::takeResult(std::shared_ptr<Histogram1D> result)
{
    const bool edgesMoved = storeResult(std::move(result));
    requestReplot();
    const QPointer<QCPHistogram> alive(this);
    if (edgesMoved)
        Q_EMIT edgesChanged();
    if (alive)
        Q_EMIT histogramReady();
}

// setTransform does not restart the pipeline, so reinstalling here cannot loop.
void QCPHistogram::rememberEdges()
{
    if (!mDataSource || !autoEdges())
        return;
    mAutoRange = widenedRange(usableRange(mAutoRange, keyLog()),
                              QCPRange(mResult->edges.front(), mResult->edges.back()));
    installTransform();
}

void QCPHistogram::requestReplot()
{
    if (mLayer)
        mLayer->markDirty();
    if (mParentPlot)
        mParentPlot->replot(QCustomPlot::rpQueuedReplot);
}

void QCPHistogram::binForExport()
{
    if (!mDataSource || !mKeyAxis || !mValueAxis)
        return;
    if (scaleIsStale())
    {
        mAutoRange.reset();
        installTransform();
    }
    // No signals: their receivers (auto-scale) must not run in the middle of a paint.
    if (mPipeline.runSynchronously(ViewportParams::fromAxes(mKeyAxis.data(), mValueAxis.data())))
        storeResult(pipelineResult());
}

// resetEdges() emits busyChanged at once, and nothing may emit during a paint. The scale may be
// fresh again when this runs (set back, or re-binned by an export): a reset would then only drop
// auto edges that are still right.
void QCPHistogram::queueResetEdges()
{
    if (mResetQueued)
        return;
    mResetQueued = true;
    QMetaObject::invokeMethod(this, [this] {
        mResetQueued = false;
        resetForTheStaleScale();
    }, Qt::QueuedConnection);
}

// One reset per scale until its job delivers: every frame showing stale bars asks again, and
// each new reset would supersede the job before it, so a steady replot stream would keep the
// stale bars up.
void QCPHistogram::resetForTheStaleScale()
{
    if (!scaleIsStale() || mResetForLog == keyLog())
        return;
    mResetForLog = keyLog();
    resetEdges();
}

bool QCPHistogram::canProduceContent() const
{
    return mKeyAxis && mValueAxis && (mResult || mPipeline.isBusy());
}

void QCPHistogram::draw(QCPPainter* painter)
{
    PROFILE_HERE_N("QCPHistogram::draw");
    const bool staleScale = scaleIsStale();
    if (staleScale)
        queueResetEdges();
    // Export does not pump the event loop, so a background result would never arrive.
    if ((staleScale || !mResult || mPipeline.isBusy())
        && painter->modes().testFlag(QCPPainter::pmNoCaching))
        binForExport();
    if (!mResult || !mKeyAxis || !mValueAxis)
        return;
    auto* layer = qcp::gpuLayer(mParentPlot, mLayer, painter->isExporting());
    for (int c = 0; c < int(mResult->heights.size()); ++c)
    {
        const auto bars = columnBars(c);
        if (layer)
            drawColumnOnGpu(painter, layer, bars, columnLook(c));
        else
            paintColumn(painter, bars, columnLook(c));
    }
}

QCPHistogram::ColumnLook QCPHistogram::columnLook(int column) const
{
    const QColor color = column < mColors.size() && mColors[column].isValid()
        ? mColors[column] : qcp::defaultPlottableColor(column);
    if (mStyle == sBars)
        return {color, QPen(color.darker(125), 1)};
    // One step outline reads alone; several overlap, and a light fill tells them apart.
    QColor fill = color;
    fill.setAlpha(mResult && mResult->heights.size() > 1 ? 64 : 0);
    return {fill, QPen(color, 1)};
}

bool QCPHistogram::keyIsVertical() const
{
    return mKeyAxis->orientation() == Qt::Vertical;
}

std::vector<qcp::histogram::BarRect> QCPHistogram::columnBars(int column) const
{
    return qcp::histogram::barRects(*mResult, column, mKeyAxis.data(), mValueAxis.data());
}

QVector<QPointF> QCPHistogram::outline(std::span<const qcp::histogram::BarRect> bars) const
{
    return mStyle == sBars ? qcp::histogram::barOutlines(bars, keyIsVertical())
                           : qcp::histogram::stepOutline(bars, keyIsVertical());
}

// Built from the current axes on every frame: no pan offset.
void QCPHistogram::drawColumnOnGpu(QCPPainter* painter, QCPPlottableRhiLayer* layer,
                                   std::span<const qcp::histogram::BarRect> bars,
                                   const ColumnLook& look)
{
    std::vector<float> fill;
    if (look.fill.alpha() > 0)
        qcp::histogram::appendBarQuads(bars, keyIsVertical(), qcp::rhi::premultipliedColor(look.fill),
                                       fill);
    const double dpr = mParentPlot->bufferDevicePixelRatio();
    // simplify: re-extruded per frame; fine for the hundreds of bins histograms use. Cache with
    // qcp::ExtrusionCache (drawPolylineCached) if a 10^5-bin case appears.
    const QVector<float> stroke = QCPLineExtruder::extrudePolyline(
        outline(bars), qcp::extrusionPenWidth(look.outline, dpr), look.outline.color());
    if (fill.empty() && stroke.isEmpty())
        return;
    layer->addPlottable(fill, stroke, clipRect(), dpr, mParentPlot->rhiOutputSize().height(), 0, 0,
                        float(painter->opacity()));
}

void QCPHistogram::paintColumn(QCPPainter* painter, std::span<const qcp::histogram::BarRect> bars,
                               const ColumnLook& look) const
{
    painter->save();
    painter->setPen(Qt::NoPen);
    painter->setBrush(look.fill);
    for (const auto& bar : bars)
        if (look.fill.alpha() > 0 && !qcp::histogram::isEmpty(bar))
            painter->drawRect(qcp::histogram::pixelRect(bar, keyIsVertical()));
    painter->setPen(look.outline);
    painter->setBrush(Qt::NoBrush);
    qcp::drawPolylineSplitNaN(painter, outline(bars));
    painter->restore();
}

void QCPHistogram::drawLegendIcon(QCPPainter* painter, const QRectF& rect) const
{
    applyDefaultAntialiasingHint(painter);
    const int columns = mResult ? std::max<int>(1, mResult->heights.size()) : 1;
    const double width = rect.width() / columns;
    for (int c = 0; c < columns; ++c)
    {
        const ColumnLook look = columnLook(c);
        painter->setPen(look.outline);
        painter->setBrush(look.fill);
        painter->drawRect(QRectF(rect.left() + c * width, rect.top(), width, rect.height()));
    }
}

QCPRange QCPHistogram::getKeyRange(bool& foundRange, QCP::SignDomain inSignDomain) const
{
    foundRange = false;
    if (!mResult)
        return {};
    const auto& edges = mResult->edges;
    auto lower = edges.begin();
    if (inSignDomain == QCP::sdPositive)
        lower = std::find_if(edges.begin(), edges.end(), [](double e) { return e > 0; });
    if (lower == edges.end())
        return {};
    foundRange = true;
    return QCPRange(*lower, edges.back());
}

QCPRange QCPHistogram::getValueRange(bool& foundRange, QCP::SignDomain inSignDomain,
                                     const QCPRange&) const
{
    foundRange = false;
    if (!mResult)
        return {};
    const double upper = largest(mResult->heights);
    if (!(upper > 0))
        return {};
    if (inSignDomain != QCP::sdPositive)
    {
        foundRange = true;
        return QCPRange(0, upper);
    }
    const auto lower = smallestPositive(mResult->heights);
    if (!lower)
        return {};
    foundRange = true;
    return QCPRange(*lower, upper);
}

// simplify: not selectable in v1, draw() has no selected look. Hit-test the bar rects of
// qcp::histogram::contains(columnBars(c), ...) when a selected look is added.
double QCPHistogram::selectTest(const QPointF&, bool, QVariant*) const
{
    return -1;
}
