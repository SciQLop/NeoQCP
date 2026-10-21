#include "plottable-histogram.h"
#include <axis/axis.h>
#include <core.h>
#include <layer.h>
#include <painting/painter.h>
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

bool QCPHistogram::autoEdges() const
{
    return !usableRange(mFixedRange, keyLog());
}

void QCPHistogram::installTransform()
{
    const int bins = mBins;
    const bool log = keyLog();
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
    mPipeline.onDataChanged();
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
    auto result = pipelineResult();
    if (result && !coversAutoEdges(*result))
    {
        refreshBinning();
        return;
    }
    takeResult(std::move(result));
    requestReplot();
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
    mPipeline.setSource(mDataSource);
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
    mPipeline.setSource(nullptr);
    mAcceptedGeneration = mPipeline.currentGeneration();
    const std::size_t columns = heights.size();
    takeResult(std::make_shared<Histogram1D>(
        Histogram1D { std::move(edges), std::move(heights), std::vector<int>(columns, 0), keyLog() }));
    requestReplot();
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

void QCPHistogram::takeResult(std::shared_ptr<Histogram1D> result)
{
    if (storeResult(std::move(result)))
        Q_EMIT edgesChanged();
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
    // No signals: their receivers (auto-scale) must not run in the middle of a paint.
    if (mPipeline.runSynchronously(ViewportParams::fromAxes(mKeyAxis.data(), mValueAxis.data())))
        storeResult(pipelineResult());
}

bool QCPHistogram::canProduceContent() const
{
    return mKeyAxis && mValueAxis && (mResult || mPipeline.isBusy());
}

void QCPHistogram::draw(QCPPainter* painter)
{
    PROFILE_HERE_N("QCPHistogram::draw");
    // Export does not pump the event loop, so a background result would never arrive.
    if (!mResult && painter->modes().testFlag(QCPPainter::pmNoCaching))
        binForExport();
}

void QCPHistogram::drawLegendIcon(QCPPainter*, const QRectF&) const {}

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

double QCPHistogram::selectTest(const QPointF&, bool, QVariant*) const
{
    return -1;
}
