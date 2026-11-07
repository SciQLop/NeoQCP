#include "plottable-histogram.h"
#include <QPointer>
#include "plottable-draw-utils.h"
#include <axis/axis.h>
#include <core.h>
#include <layer.h>
#include <layoutelements/layoutelement-axisrect.h>
#include <misuse.h>
#include <painting/draw-context.h>
#include <Profiling.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

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
    : QCPDrawn<QCPAbstractPlottable>(keyAxis, valueAxis)
    , mPipeline(parentPlot() ? parentPlot()->pipelineScheduler() : nullptr, this)
{
    installTransform();
    connect(&mPipeline, &QCPHistogram1DPipeline::finished, this, &QCPHistogram::onPipelineFinished);
    connect(&mPipeline, &QCPHistogram1DPipeline::busyChanged,
            this, [this](bool) { updateEffectiveBusy(); });
    followKeyAxis();
}

// Edges in one scale mean nothing in the other: re-fit them when the key axis toggles.
void QCPHistogram::followKeyAxis()
{
    disconnect(mKeyScaleLink);
    if (mKeyAxis)
        mKeyScaleLink = connect(mKeyAxis.data(), &QCPAxis::scaleTypeChanged, this, &QCPHistogram::resetEdges);
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
    const int bins = mBins.get();
    const bool log = keyLog();
    mBinnedKeyLog = log;
    const auto normalization = mNormalization.get();
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
    markDirty();
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
    markDirty(); // before the pipeline signals below: a slot may delete this histogram
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
    QCP_CHECK_OR_RETURN(bins > 0, "a histogram needs at least one bin", );
    if (!mBins.set(bins))
        return;
    markDirty();
    refreshBinning();
}

void QCPHistogram::setFixedRange(std::optional<QCPRange> range)
{
    if (range && keyLog() && range->lower <= 0)
    {
        qWarning() << "QCPHistogram::setFixedRange: log bins need a positive range, ignored";
        return;
    }
    markDirty();
    mFixedRange = range;
    refreshBinning();
}

void QCPHistogram::setNormalization(QCP::HistogramNormalization norm)
{
    if (!mNormalization.set(norm))
        return;
    markDirty();
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

void QCPHistogram::storeResult(std::shared_ptr<Histogram1D> result)
{
    const bool edgesMoved = result && (!mResult || mResult->edges != result->edges);
    mResult = std::move(result);
    if (edgesMoved)
        rememberEdges();
}

// Repaints before it signals: a slot may delete this histogram.
void QCPHistogram::takeResult(std::shared_ptr<Histogram1D> result)
{
    storeResult(std::move(result));
    requestReplot();
    // A slot may delete this histogram: deliver at the outermost call.
    const QCPEmissionScope scope(mParentPlot);
    qcpEmit(this, mParentPlot, &QCPHistogram::histogramReady);
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
    markDirty(); // marking schedules the repaint
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

void QCPHistogram::draw(QCPDrawContext& context) const
{
    PROFILE_HERE_N("QCPHistogram::draw");
    auto* self = const_cast<QCPHistogram*>(this);
    const bool staleScale = scaleIsStale();
    if (staleScale)
        self->queueResetEdges();
    // Export does not pump the event loop, so a background result would never arrive.
    if (context.exporting() && (staleScale || !mResult || mPipeline.isBusy()))
        self->binForExport();
    if (!mResult || !mKeyAxis || !mValueAxis)
        return;
    for (int c = 0; c < int(mResult->heights.size()); ++c)
    {
        const ColumnLook look = columnLook(c);
        if (mStyle == sBars)
            recordBars(context, c, look);
        else
            recordStep(context, c, look);
    }
}

// One rect per drawable bin. The same pen and brush join the whole column into one batch, so a
// pan only moves it.
void QCPHistogram::recordRects(QCPDrawContext& context, int column, const QPen& pen,
                               const QBrush& brush) const
{
    const auto& heights = mResult->heights[std::size_t(column)];
    const bool log = mValueAxis->scaleType() == QCPAxis::stLogarithmic;
    for (std::size_t b = 0; b < heights.size(); ++b)
    {
        if (!drawableHeight(heights[b], log))
            continue;
        context.rect(QCP::Data{ mResult->edges[b], 0.0 },
                     QCP::Data{ mResult->edges[b + 1], heights[b] }, pen, brush);
    }
}

void QCPHistogram::recordBars(QCPDrawContext& context, int column, const ColumnLook& look) const
{
    recordRects(context, column, look.outline, look.fill);
}

// The top profile in data space: up over each bin, down to the base, one NaN break per gap. Same
// shape as the old pixel outline, so the recording survives a pan.
void QCPHistogram::recordStep(QCPDrawContext& context, int column, const ColumnLook& look) const
{
    // The fill has no edge of its own: the polyline below is the only outline of a step.
    if (look.fill.alpha() > 0)
        recordRects(context, column, QPen(Qt::NoPen), look.fill);
    const auto& heights = mResult->heights[std::size_t(column)];
    const bool log = mValueAxis->scaleType() == QCPAxis::stLogarithmic;
    const QPointF breakPoint(qQNaN(), qQNaN());
    std::vector<QPointF> points;
    points.reserve(4 * heights.size() + 2);
    bool inRun = false;
    for (std::size_t b = 0; b < heights.size(); ++b)
    {
        if (absentHeight(heights[b], log))
        {
            inRun = false;
            continue;
        }
        if (!inRun && !points.empty())
            points.push_back(breakPoint);
        inRun = true;
        const double left = mResult->edges[b];
        const double right = mResult->edges[b + 1];
        points.emplace_back(left, 0.0);
        points.emplace_back(left, heights[b]);
        points.emplace_back(right, heights[b]);
        points.emplace_back(right, 0.0);
    }
    if (!points.empty())
        context.polyline(std::make_shared<const std::vector<QPointF>>(std::move(points)),
                         look.outline);
}

// A zero height covers no pixels, and a log axis can only place positive ones.
bool QCPHistogram::drawableHeight(double height, bool log)
{
    return std::isfinite(height) && (log ? height > 0 : height != 0);
}

// A step run still visits an empty bin on a linear axis, at the base: only a bar the axis cannot
// place is a gap.
bool QCPHistogram::absentHeight(double height, bool log)
{
    return !std::isfinite(height) || (log && height <= 0);
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

// The icon is the unit square in data space; one slot per column, full height.
void QCPHistogram::drawLegendIcon(QCPDrawContext& icon) const
{
    icon.rect(QCP::Data{ 0, 0 }, QCP::Data{ 1, 1 }, QPen(Qt::NoPen), QBrush(Qt::transparent));
    const int columns = mResult ? std::max(1, int(mResult->heights.size())) : 1;
    for (int c = 0; c < columns; ++c)
    {
        const ColumnLook look = columnLook(c);
        icon.rect(QCP::Data{ double(c) / columns, 0.0 }, QCP::Data{ double(c + 1) / columns, 1.0 },
                  look.outline, look.fill);
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

// simplify: not selectable in v1, draw() has no selected look. Hit-test the recorded bar rects
// when a selected look is added.
double QCPHistogram::selectTest(const QPointF&, bool, QVariant*) const
{
    return -1;
}
