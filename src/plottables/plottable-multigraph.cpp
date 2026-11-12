#include "plottable-multigraph.h"
#include "../axis/axis.h"
#include "../core.h"
#include "../datasource/graph-resampler.h"
#include "../datasource/resampled-multi-datasource.h"
#include "../layoutelements/layoutelement-axisrect.h"
#include "../layoutelements/layoutelement-legend-group.h"
#include "../misuse.h"
#include "../painting/draw-context.h"
#include "../painting/painter.h"
#include "../painting/plottable-rhi-layer.h"
#include "../painting/rhi-utils.h"
#include "../painting/scatter-rhi-layer.h"
#include "../painting/viewport-offset.h"
#include "../vector2d.h"
#include "plottable-draw-utils.h"
#include "plottable-l1-cache.h"
#include "plottable-linestyle.h"
#include <QPointer>
#include <algorithm>
#include <cmath>
#include <utility>

static void ensureL1TransformMulti(QCPMultiGraphPipeline& pipeline, int sourceSize, int colCount,
                                   std::shared_ptr<std::atomic<bool>> wantOrigin);
static bool needsResamplingMulti(const QCPAbstractMultiDataSource& src);

QCPMultiGraph::QCPMultiGraph(QCPAxis* keyAxis, QCPAxis* valueAxis, QCP::Capabilities required)
        : QCPAbstractLine(keyAxis, valueAxis, required)
    , mPipeline(parentPlot() ? parentPlot()->pipelineScheduler() : nullptr, this)
{
    // Base constructor auto-adds a QCPPlottableLegendItem (vtable not yet set up
    // for virtual addToLegend). Replace it with our group legend item. A refused object has no
    // system and no row to replace.
    if (mSystem && mParentPlot && mParentPlot->autoAddPlottableToLegend() && mParentPlot->legend) {
        QCPAbstractPlottable::removeFromLegend(mParentPlot->legend);
        addToLegend();
    }

    followAxes();

    connect(&mPipeline, &QCPMultiGraphPipeline::finished,
            this, [this](uint64_t gen) { onL1Ready(gen); });
    connect(&mPipeline, &QCPMultiGraphPipeline::busyChanged,
            this, [this](bool) { updateEffectiveBusy(); });

    mViewportDebounce.setSingleShot(true);
    mViewportDebounce.setInterval(150);
    connect(&mViewportDebounce, &QTimer::timeout, this, [this] { linesChanged(); });
}

QCPMultiGraph::~QCPMultiGraph() = default;

bool QCPMultiGraph::drawsInDataSpace() const
{
    return mSystem && mSystem->kind() != QCPTransform::Kind::Cartesian;
}

bool QCPMultiGraph::areaIsAxisRect() const
{
    return mSystem && qobject_cast<QCPAxisRect*>(mSystem->area());
}

bool QCPMultiGraph::hasExplicitColorScale() const
{
    return mExplicitColorScale && colorScale() != nullptr;
}

bool QCPMultiGraph::colourNeedsAScale() const
{
    return drawsInDataSpace() && !areaIsAxisRect() && !hasExplicitColorScale();
}

double QCPMultiGraph::dataSpaceBaseline() const
{
    // 2.x closes toward zero: on a linear axis that is value 0, on a log axis the range bound on
    // zero's side (upper on a negative range, lower on a positive one).
    const QCPAxis* valueAxis = qobject_cast<const QCPAxis*>(valueAxisBase());
    if (valueAxis && valueAxis->scaleType() == QCPAxis::stLogarithmic)
    {
        const QCPRange range = valueAxis->range();
        return range.upper < 0 ? range.upper : range.lower;
    }
    return 0.0;
}

double QCPMultiGraph::dataSpaceFillValue(double value) const
{
    // The sign-aware counterpart of fillValue(): a value the log frame cannot place goes to the
    // range bound on zero's side, never to the far bound. A linear frame takes the value itself.
    const QCPAxis* valueAxis = qobject_cast<const QCPAxis*>(valueAxisBase());
    if (!valueAxis || valueAxis->scaleType() != QCPAxis::stLogarithmic || std::isnan(value))
        return value;
    const QCPRange range = valueAxis->range();
    if (range.upper < 0)
        return value < 0 ? value : range.upper;
    return value > 0 ? value : range.lower;
}

quint64 QCPMultiGraph::dependencyRevision() const
{
    // B4: on the data-space branch the recording reads the system's key window and its baseline,
    // so a mapping change has to make it stale even though no data or style moved.
    quint64 revision = drawsInDataSpace() ? qHashMulti(0, mRevision, mSystem->revision())
                                          : mRevision;
    // B8: a fill to another graph reads that graph's data and system. Its revision moves on both,
    // so a partner data change or a partner system move makes this recording stale.
    for (const Fill& fill : mFills)
        if (const auto* target = std::get_if<FillToGraph>(&fill.target))
            revision = qHashMulti(revision, target->graph ? target->graph->mRevision : 0);
    return revision;
}

void QCPMultiGraph::followAxes()
{
    for (const auto& link : std::exchange(mAxisLinks, {}))
        disconnect(link);
    if (mKeyAxis)
    {
        mAxisLinks << connect(mKeyAxis.data(), &QCPAxis::rangeChanged, this,
                              &QCPMultiGraph::onViewportChanged);
        mAxisLinks << connect(mKeyAxis.data(), &QCPAxis::scaleTypeChanged, this,
                              [this] { linesChanged(); });
    }
    if (mValueAxis)
        mAxisLinks << connect(mValueAxis.data(), &QCPAxis::scaleTypeChanged, this,
                              [this] { linesChanged(); });
    linesChanged(); // pixels computed for the old axes mean nothing on the new ones
    mL2Dirty = mL1Cache != nullptr; // nor do the bins resampled for the old key range
}

void QCPMultiGraph::systemChanged()
{
    // B2: the pipeline belongs to the cartesian branch. Entering a user system drops its L1/L2 and
    // starts nothing; leaving one re-arms it for the displayed source. An axis change within the
    // cartesian branch is not a branch change: its L1 survives and followAxes() only dirties L2.
    followAxes();
    const bool dataSpace = drawsInDataSpace();
    if (dataSpace)
    {
        if (!mPipelineBypassed)
        {
            mL1Cache.reset();
            mL2Result.reset();
            mL2Dirty = false;
        }
        mPipelineBypassed = true;
        mNeedsResampling = false;
        mPipeline.clearTransform();
        mPipeline.setSource(nullptr);
        mAcceptedGeneration = mPipeline.currentGeneration();
        // I1: the job a pending source started on the cartesian branch has nothing to deliver to
        // this branch, and the staged source needs no L1 here. It is shown by the next swap.
        if (mPendingSource)
        {
            mPendingL1.reset();
            markPendingReady();
        }
        linesChanged();
        return;
    }
    if (!std::exchange(mPipelineBypassed, false))
        return; // a change within the cartesian branch
    mL1Cache.reset();
    mL2Result.reset();
    mL2Dirty = false;
    mNeedsResampling = mDataSource && needsResamplingMulti(*mDataSource);
    // I2: returning to cartesian with a staged source re-stages that source, so its job carries
    // its own generation and its L1 can never be the displayed source's. Colour values stashed for
    // the staged source survive the re-stage.
    if (mPendingSource)
    {
        auto pending = mPendingSource;
        auto colours = std::move(mPendingColorValues);
        stagePendingSource(std::move(pending));
        mPendingColorValues = std::move(colours);
        linesChanged();
        return;
    }
    if (mDataSource)
    {
        ensureL1TransformMulti(mPipeline, mDataSource->size(), mDataSource->columnCount(),
                               mWantOrigin);
        const QPointer<QCPMultiGraph> alive(this);
        mPipeline.setSource(mDataSource);
        if (!alive)
            return;
        mAcceptedGeneration = mPipeline.hasTransform() ? mPipeline.currentGeneration()
                                                        : mPipeline.currentGeneration() + 1;
    }
    else
    {
        mPipeline.clearTransform();
        mPipeline.setSource(nullptr);
        mAcceptedGeneration = mPipeline.currentGeneration();
    }
    linesChanged();
}

// A finished resample, a rebuilt L2 or the settle after a pan changes what the recording says
// without changing the object: the revision is what the adapter watches, and the layer is painted
// again so it is asked for one.
void QCPMultiGraph::linesChanged()
{
    ++mRevision;
    if (mLayer)
        mLayer->markDirty();
    // A graph filled to a component of this one draws this data: mark it wherever its layer is.
    markDependentsDirty();
}

void QCPMultiGraph::setDataSource(std::unique_ptr<QCPAbstractMultiDataSource> source)
{
    markDirty();
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
    // Any source installed this way is not the graph's: addData() appends only to the growable
    // source setData() built.
    mOwnedSource.reset();
    installSource(std::move(source));
}

void QCPMultiGraph::setOwnedSource(std::shared_ptr<QCPGrowableMultiDataSource> source)
{
    mOwnedSource = source;
    installSource(std::move(source));
}

void QCPMultiGraph::installSource(std::shared_ptr<QCPAbstractMultiDataSource> source)
{
    // Dropping the colour scale signals colorScaleChanged: deliver it once this graph is done,
    // so a slot may delete it.
    const QCPEmissionScope scope(mParentPlot);
    // Keep the displayed geometry (and its GPU translation) alive while the
    // replacement is prepared; the plot commits it in one debounced swap.
    const bool canDefer = mDeferredDataSwap && source && mDataSource && hasRenderedRange()
        && mParentPlot;
    if (canDefer)
        stagePendingSource(std::move(source));
    else
        applySourceNow(std::move(source));
}

std::shared_ptr<QCPGrowableMultiDataSource> QCPMultiGraph::ownedSourceForAppend() const
{
    if (mPendingSource)
    {
        if (mOwnedSource && mPendingSource == mOwnedSource)
            return mOwnedSource;
        QCP_CHECK_OR_RETURN(false, "addData needs a source the graph owns", {});
    }
    if (!mDataSource)
        return {}; // no data yet: addData() starts the owned source
    if (mOwnedSource && mDataSource == mOwnedSource)
        return mOwnedSource;
    QCP_CHECK_OR_RETURN(false, "addData needs a source the graph owns", {});
}

void QCPMultiGraph::appendChanged()
{
    if (mPendingSource)
    {
        // The staged source is the one that grew: re-stage it for a new generation, so the
        // pipeline resamples the appended rows. dataChanged() would re-stage the displayed source.
        auto pending = mPendingSource;
        stagePendingSource(std::move(pending));
        return;
    }
    dataChanged();
}

bool QCPMultiGraph::appendToOwnedSource(
    const std::function<bool(QCPGrowableMultiDataSource&)>& append)
{
    if (!mOwnedSource)
        return false;
    const bool pending = mPendingSource != nullptr;
    if (mPipeline.isBusy())
    {
        // A job is reading the rows of the source it was started for. Grow a copy and install it;
        // the old object stays alive in the job.
        auto grown = mOwnedSource->clone();
        if (!append(*grown))
            return false;
        mOwnedSource = grown;
        if (pending)
            stagePendingSource(std::move(grown));
        else
            installSource(std::move(grown)); // deferred swap on: keep the displayed geometry
        return true;
    }

    if (!append(*mOwnedSource))
        return false;
    appendChanged();
    return true;
}

void QCPMultiGraph::setData(const QVector<double>& keys, const QVector<double>& values,
                            bool alreadySorted)
{
    QCP_CHECK_OR_RETURN(keys.size() == values.size(),
                        "keys and values have different sizes");
    auto source = std::make_shared<QCPGrowableMultiDataSource>(
        std::vector<double>(keys.begin(), keys.end()),
        std::vector<std::vector<double>>{std::vector<double>(values.begin(), values.end())},
        alreadySorted);
    if (source->size() != keys.size() || source->columnCount() != 1)
        return; // the source refused the shape or a non-finite key: no change
    setOwnedSource(std::move(source));
}

void QCPMultiGraph::setData(const std::vector<double>& keys, const std::vector<double>& values,
                            bool alreadySorted)
{
    QCP_CHECK_OR_RETURN(keys.size() == values.size(),
                        "keys and values have different sizes");
    auto source = std::make_shared<QCPGrowableMultiDataSource>(
        std::vector<double>(keys), std::vector<std::vector<double>>{std::vector<double>(values)},
        alreadySorted);
    if (source->size() != keys.size() || source->columnCount() != 1)
        return; // the source refused the shape or a non-finite key: no change
    setOwnedSource(std::move(source));
}

void QCPMultiGraph::addData(const QVector<double>& keys, const QVector<double>& values,
                            bool alreadySorted)
{
    QCP_CHECK_OR_RETURN(acceptsAppend(), "addData needs a source the graph owns");
    auto owned = ownedSourceForAppend();
    if (!owned && (mDataSource || mPendingSource))
        return; // a non-owned source: ownedSourceForAppend() reported it
    QCP_CHECK_OR_RETURN(keys.size() == values.size(),
                        "keys and values have different sizes");
    if (owned)
    {
        QCP_CHECK_OR_RETURN(owned->columnCount() == 0 || owned->columnCount() == 1,
                            "a one-column addData needs a one-column graph");
        QCP_CHECK_OR_RETURN(owned->columnCount() > 0 || owned->size() == 0,
                            "a graph with rows and no value columns takes no addData");
    }

    if (!owned)
    {
        auto source = std::make_shared<QCPGrowableMultiDataSource>();
        if (!source->append(keys, std::vector<QVector<double>>{values}, alreadySorted))
            return;
        setOwnedSource(std::move(source));
        return;
    }
    (void)appendToOwnedSource(
        [&](QCPGrowableMultiDataSource& source)
        { return source.append(keys, std::vector<QVector<double>>{values}, alreadySorted); });
}

void QCPMultiGraph::addData(double key, double value)
{
    QCP_CHECK_OR_RETURN(acceptsAppend(), "addData needs a source the graph owns");
    auto owned = ownedSourceForAppend();
    if (!owned && (mDataSource || mPendingSource))
        return;
    QCP_CHECK_OR_RETURN(std::isfinite(key), "keys must be finite");
    if (owned)
    {
        QCP_CHECK_OR_RETURN(owned->columnCount() == 0 || owned->columnCount() == 1,
                            "a single point needs a one-column graph");
        QCP_CHECK_OR_RETURN(owned->columnCount() > 0 || owned->size() == 0,
                            "a graph with rows and no value columns takes no addData");
    }
    const double values[] = { value };
    if (!owned)
    {
        auto source = std::make_shared<QCPGrowableMultiDataSource>();
        if (!source->append(key, std::span<const double>(values, 1)))
            return;
        setOwnedSource(std::move(source));
        return;
    }
    (void)appendToOwnedSource(
        [&](QCPGrowableMultiDataSource& source)
        { return source.append(key, std::span<const double>(values, 1)); });
}

void QCPMultiGraph::addData(const QVector<double>& keys, std::vector<QVector<double>> columns,
                            bool alreadySorted)
{
    QCP_CHECK_OR_RETURN(acceptsAppend(), "addData needs a source the graph owns");
    auto owned = ownedSourceForAppend();
    if (!owned && (mDataSource || mPendingSource))
        return;
    QCP_CHECK_OR_RETURN(!columns.empty(), "addData needs at least one value column");
    for (const auto& column : columns)
        QCP_CHECK_OR_RETURN(column.size() == keys.size(),
                            "keys and a value column have different sizes");
    if (owned)
    {
        QCP_CHECK_OR_RETURN(owned->columnCount() == 0
                                || owned->columnCount() == static_cast<int>(columns.size()),
                            "the value column count does not match the graph's");
        QCP_CHECK_OR_RETURN(owned->columnCount() > 0 || owned->size() == 0,
                            "a graph with rows and no value columns takes no addData");
    }

    if (!owned)
    {
        auto source = std::make_shared<QCPGrowableMultiDataSource>();
        if (!source->append(keys, std::move(columns), alreadySorted))
            return;
        setOwnedSource(std::move(source));
        return;
    }
    (void)appendToOwnedSource(
        [&](QCPGrowableMultiDataSource& source)
        { return source.append(keys, std::move(columns), alreadySorted); });
}

void QCPMultiGraph::addData(double key, std::span<const double> values)
{
    QCP_CHECK_OR_RETURN(acceptsAppend(), "addData needs a source the graph owns");
    auto owned = ownedSourceForAppend();
    if (!owned && (mDataSource || mPendingSource))
        return;
    QCP_CHECK_OR_RETURN(!values.empty(), "addData needs at least one value");
    QCP_CHECK_OR_RETURN(std::isfinite(key), "keys must be finite");
    if (owned)
    {
        QCP_CHECK_OR_RETURN(owned->columnCount() == 0
                                || owned->columnCount() == static_cast<int>(values.size()),
                            "the value column count does not match the graph's");
        QCP_CHECK_OR_RETURN(owned->columnCount() > 0 || owned->size() == 0,
                            "a graph with rows and no value columns takes no addData");
    }

    if (!owned)
    {
        auto source = std::make_shared<QCPGrowableMultiDataSource>();
        if (!source->append(key, values))
            return;
        setOwnedSource(std::move(source));
        return;
    }
    (void)appendToOwnedSource([&](QCPGrowableMultiDataSource& source)
                              { return source.append(key, values); });
}

void QCPMultiGraph::setDeferredDataSwap(bool enabled)
{
    if (enabled == mDeferredDataSwap)
        return;
    const QCPEmissionScope scope(mParentPlot);
    mDeferredDataSwap = enabled;
    if (!enabled && mPendingSource && !commitPendingData())
    {
        // Not ready yet: apply it now, with the colours set for it, which applySourceNow() drops.
        auto colours = std::move(mPendingColorValues);
        const QPointer<QCPMultiGraph> alive(this);
        applySourceNow(std::move(mPendingSource));
        if (alive && colours)
            setColorValues(std::move(colours));
    }
}

void QCPMultiGraph::applySourceNow(std::shared_ptr<QCPAbstractMultiDataSource> source)
{
    mPendingSource.reset();
    mPendingL1.reset();
    mPendingColorValues.reset();
    mPendingReady = false;
    mDataSource = std::move(source);
    if (mDataSource)
        mDataSource->setGapThreshold(mGapThreshold);
    applyColorSizeRule(mDataSource ? mDataSource->size() : 0);
    linesChanged();
    mL2HasOrigin = false;
    syncComponentCount(mDataSource.get());
    mL1Cache.reset();
    mL2Result.reset();
    mL2Dirty = false;
    mNeedsResampling = mDataSource && !drawsInDataSpace() && needsResamplingMulti(*mDataSource);
    if (drawsInDataSpace() || !mDataSource)
    {
        // B2: the displayed source is drawn directly; no job is started or consulted.
        mPipeline.clearTransform();
        mPipeline.setSource(nullptr);
        mPipelineBypassed = drawsInDataSpace();
        mAcceptedGeneration = mPipeline.currentGeneration();
        markDirty();
        updateEffectiveBusy();
        return;
    }
    mPipelineBypassed = false;
    ensureL1TransformMulti(mPipeline, mDataSource->size(), mDataSource->columnCount(), mWantOrigin);
    // The pipeline's busyChanged slot may delete this graph.
    const QPointer<QCPMultiGraph> alive(this);
    mPipeline.setSource(mDataSource);
    if (!alive)
        return;
    // Without a transform setSource() does not bump the generation, so a job
    // still running for the previous source would otherwise pass onL1Ready's
    // staleness check below.
    mAcceptedGeneration = mPipeline.hasTransform() ? mPipeline.currentGeneration()
                                                    : mPipeline.currentGeneration() + 1;
    if (!mPipeline.hasTransform())
        markDirty();
    updateEffectiveBusy();
}

void QCPMultiGraph::stagePendingSource(std::shared_ptr<QCPAbstractMultiDataSource> source)
{
    mPendingSource = std::move(source);
    mPendingL1.reset();
    mPendingSource->setGapThreshold(mGapThreshold);
    // A newly-staged source supersedes whatever replacement (and colour values meant
    // for it) was staged before; setColorValues() re-stashes if called again.
    mPendingColorValues.reset();
    mPendingReady = false;
    syncComponentCount(mPendingSource.get());
    if (drawsInDataSpace())
    {
        // A staged source is committed by the plot's debounce, but there is no job to wait for.
        mPipeline.clearTransform();
        mPipeline.setSource(nullptr);
        markPendingReady();
        updateEffectiveBusy();
        return;
    }
    ensureL1TransformMulti(mPipeline, mPendingSource->size(), mPendingSource->columnCount(), mWantOrigin);
    // The pipeline's busyChanged slot may delete this graph.
    const QPointer<QCPMultiGraph> alive(this);
    mPipeline.setSource(mPendingSource);
    if (!alive)
        return;
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
    // A busyChanged or colorScaleChanged slot may delete this graph mid-commit.
    const QCPEmissionScope scope(mParentPlot);
    mDataSource = std::move(mPendingSource);
    mDataSource->setGapThreshold(mGapThreshold);
    mL1Cache = std::move(mPendingL1);
    mPendingReady = false;
    mL2Result.reset();
    mL2Dirty = mL1Cache != nullptr;
    mNeedsResampling = !drawsInDataSpace() && needsResamplingMulti(*mDataSource);

    if (mPendingColorValues && static_cast<int>(mPendingColorValues->size()) == mDataSource->size())
    {
        const bool wasColoured = mColor.hasValues();
        mColor.setValues(std::move(mPendingColorValues));
        if (!wasColoured)
        {
            // The pipeline's busyChanged slot may delete this graph.
            const QPointer<QCPMultiGraph> alive(this);
            startColouring();
            if (!alive)
                return true;
        }
    }
    else
    {
        applyColorSizeRule(mDataSource->size());
    }
    mPendingColorValues.reset();
    linesChanged();
    mL2HasOrigin = false;

    // Carry the barrier established while staging forward past the commit:
    // a job dispatched for the source this replaced must not be able to land
    // in mL1Cache/mL2Result once this source is the one being displayed.
    if (mNeedsResampling && !mL1Cache && mDataSource)
    {
        // A source staged on the data-space branch had no job. Now that it is the displayed
        // cartesian source, start one; otherwise the frame has no L1 to draw until something
        // else invalidates the pipeline.
        ensureL1TransformMulti(mPipeline, mDataSource->size(), mDataSource->columnCount(),
                               mWantOrigin);
        const QPointer<QCPMultiGraph> alive(this);
        mPipeline.setSource(mDataSource);
        if (!alive)
            return true;
        mAcceptedGeneration = mPipeline.hasTransform() ? mPipeline.currentGeneration()
                                                        : mPipeline.currentGeneration() + 1;
    }
    else
    {
        mAcceptedGeneration = mPendingGeneration;
    }
    updateEffectiveBusy();
    return true;
}

void QCPMultiGraph::dataChanged()
{
    // The colour size rule may drop the scale and signal: deliver once this graph is done.
    const QCPEmissionScope scope(mParentPlot);
    // A mutated displayed source supersedes whatever replacement was staged:
    // stage it like any other new data so the on-screen geometry stays put.
    if (mPendingSource)
    {
        stagePendingSource(mDataSource);
        return;
    }

    linesChanged();
    if (mDataSource)
    {
        applyColorSizeRule(mDataSource->size());
        mNeedsResampling = !drawsInDataSpace() && needsResamplingMulti(*mDataSource);
        if (!drawsInDataSpace())
            ensureL1TransformMulti(mPipeline, mDataSource->size(), mDataSource->columnCount(),
                                   mWantOrigin);
    }

    mL1Cache.reset();
    mL2Dirty = false;

    const QPointer<QCPMultiGraph> alive(this);
    if (mPipeline.hasTransform())
        mPipeline.onDataChanged();
    else if (mParentPlot)
        mParentPlot->replot(); // data, not appearance: a mutation is shown at once (2.x behaviour)
    if (!alive)
        return; // a busyChanged slot deleted this graph

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
    if (drawsInDataSpace())
        return; // a stale job of the cartesian branch: this branch never consults the pipeline
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
    linesChanged();
}

void QCPMultiGraph::rebuildL2(const ViewportParams& vp) const
{
    if (!mL1Cache) return;
    mL2Result = qcp::algo::resampleL2Multi(*mL1Cache, vp, *mDataSource);
    if (mL2Result)
        mL2Result->setGapThreshold(mGapThreshold);
    mL2HasOrigin = mL2Result && !mL1Cache->level1.origin.empty();
}

void QCPMultiGraph::onViewportChanged(const QCPRange& newRange, const QCPRange& oldRange)
{
    if (!mL1Cache)
        return;
    // The bins are only stale; nothing recorded reads them until the recording is remade. A pan
    // inside the key window is served by a translation, so the fresh bins wait for the settle.
    mL2Dirty = true;
    // A zoom records on its own frame: only a pan, served by a translation, waits for the settle.
    const bool pan = oldRange.size() > 0
        && qAbs(newRange.size() / oldRange.size() - 1.0) < 1e-4;
    if (pan && hasRenderedRange())
        mViewportDebounce.start();
}

void QCPMultiGraph::syncComponentCount(const QCPAbstractMultiDataSource* source)
{
    // An empty fetch window says nothing about the columns: keep them and what they carry.
    if (source && source->size() == 0)
        return;
    const int newCount = source ? source->columnCount() : 0;
    int oldCount = mComponents.size();
    if (newCount > oldCount) {
        mComponents.resize(newCount);
        for (int i = oldCount; i < newCount; ++i) {
            auto& c = mComponents[i];
            QColor color = qcp::defaultPlottableColor(i);
            c.pen = QPen(color, 0);
            c.selectedPen = defaultSelectedPen(c.pen);
            c.name = QString("Component %1").arg(i);
            c.lineStyle = mLineStyle;
            c.inLegend = !isErrorBound(i);
            c.hiddenByErrorBars = !c.inLegend;
            applyRememberedStyle(i);
        }
    } else if (newCount < oldCount) {
        mComponents.resize(newCount);
    }
    if (source)
        dropDecorationsBeyond(newCount);
}

void QCPMultiGraph::markDirty()
{
    pruneLostPartnerFills();
    QCPAbstractLine::markDirty();
}

// M1: a partner that lost the component is not a temporary no-band case: the fill is gone for good,
// or a later, wider source would revive a band over unrelated data.
void QCPMultiGraph::pruneLostPartnerFills()
{
    const auto lost = [](const Fill& fill)
    {
        const auto* target = std::get_if<FillToGraph>(&fill.target);
        if (!target)
            return false;
        const QCPMultiGraph* partner = target->graph.data();
        if (!partner)
            return true;
        return partner->mDataSource
            && target->component >= partner->mDataSource->columnCount();
    };
    QVector<Fill> dropped;
    for (const Fill& fill : mFills)
        if (lost(fill))
            dropped.append(fill);
    if (dropped.isEmpty())
        return;
    mFills.erase(std::remove_if(mFills.begin(), mFills.end(), lost), mFills.end());
    releasePartnerDependencies(dropped);
}

// A decoration pointing at a column that no longer exists is dropped for good: a later, wider
// source puts unrelated data in that column.
void QCPMultiGraph::dropDecorationsBeyond(int columnCount)
{
    const auto outside = [columnCount](const Fill& e)
    {
        const auto* col = std::get_if<int>(&e.target);
        return e.owner >= columnCount || (col && *col >= columnCount);
    };
    QVector<Fill> droppedFills;
    for (const Fill& fill : mFills)
        if (outside(fill))
            droppedFills.append(fill);
    mFills.erase(std::remove_if(mFills.begin(), mFills.end(), outside), mFills.end());
    releasePartnerDependencies(droppedFills);
    const auto barsOutside = [columnCount](const ErrorBars& e)
    { return std::max({ e.value, e.lower, e.upper }) >= columnCount; };
    QVector<ErrorBars> dropped;
    for (const auto& e : mErrorBars)
        if (barsOutside(e))
            dropped.append(e);
    mErrorBars.erase(std::remove_if(mErrorBars.begin(), mErrorBars.end(), barsOutside),
                     mErrorBars.end());
    for (const auto& spec : dropped)
        releaseErrorBounds(spec);
}

void QCPMultiGraph::hideErrorBound(int column)
{
    if (column < 0 || column >= mComponents.size() || !mComponents[column].inLegend)
        return; // a bound the user already hid stays the user's
    setComponentInLegend(column, false);
    mComponents[column].hiddenByErrorBars = true;
}

// Bounds of removed bars come back to the legend, unless other bars still use them or the user
// hid them.
void QCPMultiGraph::releaseErrorBounds(const ErrorBars& removed)
{
    for (int bound : {removed.lower, removed.upper})
        if (bound >= 0 && bound < mComponents.size() && mComponents[bound].hiddenByErrorBars
            && !isErrorBound(bound))
            setComponentInLegend(bound, true);
}

// Error bars can be set before the data that creates their bound components.
bool QCPMultiGraph::isErrorBound(int column) const
{
    return std::any_of(mErrorBars.begin(), mErrorBars.end(), [column](const ErrorBars& e)
                       { return e.lower == column || e.upper == column; });
}

bool QCPMultiGraph::isValidColumn(int column) const
{
    const auto& source = mPendingSource ? mPendingSource : mDataSource;
    if (!source)
        return column >= 0;
    // An empty window keeps the components (syncComponentCount), so they stay editable.
    const int columns = source->size() == 0 ? qMax(componentCount(), source->columnCount())
                                            : source->columnCount();
    return column >= 0 && column < columns;
}

// A default-brush fill or error band takes its owner's pen colour, which the GPU vertices have
// baked in.
void QCPMultiGraph::decorationsChanged()
{
    linesChanged();
}

bool QCPMultiGraph::setFill(Fill fill)
{
    const auto* col = std::get_if<int>(&fill.target);
    if (!isValidColumn(fill.owner) || (col && (!isValidColumn(*col) || *col == fill.owner)))
        return false;
    const auto* toGraph = std::get_if<FillToGraph>(&fill.target);
    if (toGraph
        && (!toGraph->graph || toGraph->graph == this || toGraph->graph->system() != system()
            || !toGraph->graph->isValidColumn(toGraph->component)))
        return false;
    clearComponentFill(fill.owner);
    mFills.append(std::move(fill));
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

bool QCPMultiGraph::setComponentFillToGraph(int owner, QCPMultiGraph* other, int otherComponent,
                                            const QBrush& brush)
{
    QCP_CHECK_OR_RETURN(other, "a fill to another graph needs a graph", false);
    QCP_CHECK_OR_RETURN(other != this, "a fill to another graph needs another graph", false);
    QCP_CHECK_OR_RETURN(other->system() == system(),
                        "a fill to another graph needs both graphs on one system", false);
    QCP_CHECK_OR_RETURN(other->isValidColumn(otherComponent),
                        "the partner component does not exist", false);
    if (!setFill({owner, FillToGraph { other, otherComponent }, brush}))
        return false;
    // B8: the partner's data change repaints this graph wherever its own layer is, and a deleted
    // partner leaves its QPointer null; mark this graph so the recording drops the band.
    other->addDependent(this);
    connect(other, &QObject::destroyed, this, &QCPMultiGraph::partnerGone,
            Qt::UniqueConnection);
    return true;
}

void QCPMultiGraph::setComponentFillBrush(int owner, const QBrush& brush)
{
    markDirty();
    const auto it = std::find_if(mFills.begin(), mFills.end(),
                                 [owner](const Fill& e) { return e.owner == owner; });
    if (it == mFills.end())
        return;
    it->brush = brush;
    decorationsChanged();
}

void QCPMultiGraph::clearComponentFill(int owner)
{
    QVector<Fill> removed;
    for (const Fill& fill : mFills)
        if (fill.owner == owner)
            removed.append(fill);
    const auto before = mFills.size();
    mFills.erase(std::remove_if(mFills.begin(), mFills.end(),
                                [owner](const Fill& e) { return e.owner == owner; }),
                 mFills.end());
    if (mFills.size() != before)
    {
        releasePartnerDependencies(removed);
        decorationsChanged();
    }
}

std::optional<QCPMultiGraph::Fill> QCPMultiGraph::componentFill(int owner) const
{
    for (const auto& e : mFills)
        if (e.owner == owner)
            return e;
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

QBrush QCPMultiGraph::fillRunBrush(const Fill& fill, bool selected) const
{
    if (!selected)
        return effectiveFillBrush(fill);
    const QColor base = fill.owner < mComponents.size()
        ? mComponents[fill.owner].selectedPen.color()
        : QColor(Qt::gray);
    const int alpha = fill.brush.style() == Qt::NoBrush ? 64 : fill.brush.color().alpha();
    return QBrush(QColor(base.red(), base.green(), base.blue(), alpha));
}

void QCPMultiGraph::partnerGone()
{
    decorationsChanged();
}

void QCPMultiGraph::releasePartnerDependencies(const QVector<Fill>& removed)
{
    QVector<QCPMultiGraph*> candidates;
    for (const Fill& fill : removed)
    {
        if (const auto* target = std::get_if<FillToGraph>(&fill.target))
            if (target->graph && !candidates.contains(target->graph.data()))
                candidates.append(target->graph.data());
    }
    for (QCPMultiGraph* partner : candidates)
    {
        const bool stillUsed = std::any_of(
            mFills.begin(), mFills.end(),
            [partner](const Fill& fill)
            {
                const auto* target = std::get_if<FillToGraph>(&fill.target);
                return target && target->graph.data() == partner;
            });
        if (stillUsed)
            continue;
        partner->removeDependent(this);
        QObject::disconnect(partner, &QObject::destroyed, this, &QCPMultiGraph::partnerGone);
    }
}

bool QCPMultiGraph::setComponentErrorBars(int value, int lower, int upper, double capWidthPx,
                                          std::optional<QPen> pen)
{
    if (!isValidColumn(value) || !isValidColumn(lower) || !isValidColumn(upper)
        || lower == upper || value == lower || value == upper)
        return false;
    clearComponentErrorBars(value);
    mErrorBars.append({ value, lower, upper, std::move(pen), capWidthPx });
    hideErrorBound(lower);
    hideErrorBound(upper);
    decorationsChanged();
    return true;
}

void QCPMultiGraph::clearComponentErrorBars(int value)
{
    const auto old = componentErrorBars(value);
    if (!old)
        return;
    mErrorBars.erase(std::remove_if(mErrorBars.begin(), mErrorBars.end(),
                                    [value](const ErrorBars& e) { return e.value == value; }),
                     mErrorBars.end());
    releaseErrorBounds(*old);
    decorationsChanged();
}

std::optional<QCPMultiGraph::ErrorBars> QCPMultiGraph::componentErrorBars(int value) const
{
    for (const auto& e : mErrorBars)
        if (e.value == value)
            return e;
    return std::nullopt;
}

void QCPMultiGraph::updateBaseSelection()
{
    // The components' selections changed: their pens are in the recording.
    markDirty();
    QCPDataSelection combined;
    for (const auto& c : mComponents) {
        for (int i = 0; i < c.selection.dataRangeCount(); ++i)
            combined.addDataRange(c.selection.dataRange(i), false);
    }
    combined.simplify();
    if (mSelection != combined)
    {
        mSelection = combined;
        emitSelectionChanged();
    }
}

// --- Component API ---

void QCPMultiGraph::setSystem(QCPCoordinateSystem* system)
{
    // A9: an area that is not an axis rect hands out no colour scale to join, so colour values
    // need an explicit one before the frame is taken.
    if (system && system->kind() != QCPTransform::Kind::Cartesian && mColor.hasValues()
        && !hasExplicitColorScale() && !qobject_cast<QCPAxisRect*>(system->area()))
    {
        QCP_CHECK_OR_RETURN(false,
                            "colour values on a non-rect area need an explicit colour scale");
    }
    QCPAbstractLine::setSystem(system);
    // I6: colour values join the colour scale of the area they are on. A move inside one plot is
    // not an adoption, so the join has to be redone here; an explicit scale is the user's and stays.
    if (mColor.hasValues() && !hasExplicitColorScale() && areaIsAxisRect())
        followKeyAxisRectScale();
}

void QCPMultiGraph::setColorScale(QCPColorScale* colorScale)
{
    // Taking away the only scale on a non-rect frame would leave its colour values unmappable.
    if (!colorScale && drawsInDataSpace() && mColor.hasValues() && !areaIsAxisRect())
    {
        QCP_CHECK_OR_RETURN(false,
                            "colour values on a non-rect area need an explicit colour scale");
    }
    if (!mJoiningKeyAxisRectScale)
        mExplicitColorScale = colorScale != nullptr;
    QCPAbstractColorMapped::setColorScale(colorScale);
}

void QCPMultiGraph::setColorValues(std::shared_ptr<const std::vector<double>> values)
{
    if (values && !values->empty() && colourNeedsAScale())
    {
        QCP_CHECK_OR_RETURN(false,
                            "colour values on a non-rect area need an explicit colour scale");
    }
    QCPAbstractLine::setColorValues(std::move(values));
}

void QCPMultiGraph::setColorValues(std::vector<double> values)
{
    if (!values.empty() && colourNeedsAScale())
    {
        QCP_CHECK_OR_RETURN(false,
                            "colour values on a non-rect area need an explicit colour scale");
    }
    QCPAbstractLine::setColorValues(std::move(values));
}

void QCPMultiGraph::setAdaptiveSampling(bool enabled)
{
    if (mAdaptiveSampling == enabled)
        return;
    markDirty();
    mAdaptiveSampling = enabled;
    linesChanged();
}

void QCPMultiGraph::setGapThreshold(double threshold)
{
    if (mGapThreshold == threshold)
        return;
    markDirty();
    mGapThreshold = threshold;
    // The threshold lives on the source the drawing reads; it is set here and when a source is
    // installed, never from the draw, which a pipeline job may be running beside.
    if (mDataSource)
        mDataSource->setGapThreshold(threshold);
    if (mPendingSource)
        mPendingSource->setGapThreshold(threshold);
    if (mL2Result)
        mL2Result->setGapThreshold(threshold);
    linesChanged();
}

void QCPMultiGraph::lineChanged()
{
    linesChanged();
    markDirty();
}

void QCPMultiGraph::componentVisibilityToggled()
{
    // A rebuild while the component was hidden dropped its cached lines: rebuild again.
    linesChanged();
    if (mLayer)
        mLayer->invalidatePaintBuffer();
}

void QCPMultiGraph::applyBrushToComponent(int index, const QBrush& brush)
{
    if (brush.style() == Qt::NoBrush)
        clearComponentFill(index);
    else
        setComponentFill(index, 0.0, brush);
}

// A staged source is what the values will colour once the swap lands.
std::optional<int> QCPMultiGraph::colorValueCount() const
{
    const QCPAbstractMultiDataSource* target = mPendingSource ? mPendingSource.get() : mDataSource.get();
    return target ? std::optional<int>(target->size()) : std::nullopt;
}

bool QCPMultiGraph::hasPendingDataSource() const { return mPendingSource != nullptr; }

void QCPMultiGraph::colorValuesStarted()
{
    // The setter on this class refuses first; this guards the base pointer a generic caller may
    // hold, so colour values cannot reach a frame that has no scale to map them.
    if (colourNeedsAScale())
    {
        QCP_CHECK(false, "colour values on a non-rect area need an explicit colour scale");
        clearColorValues();
        return;
    }
    requestOrigin();
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

// One async L1 rebuild, the first time the graph is coloured; later colour changes never touch
// L1/L2.
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
    // A scale the user picked on an uncoloured graph is not this rule's to drop: only a resize
    // that clears the values this graph had leaves only an uncoloured graph behind.
    const bool clearedValues = mColor.hasValues() && mColor.size() != newSize;
    if (clearedValues)
        mColor.clearValues();
    if (mColor.hasValues())
        return;
    mWantOrigin->store(false);
    if (clearedValues && colorScale())
        setColorScale(nullptr);
}

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
    if (index < 0 || index >= mComponents.size())
        return;
    if (mComponents[index].selection == sel)
        return;
    markDirty();
    mComponents[index].selection = sel;
    updateBaseSelection();
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
    if (!mDataSource || mDataSource->columnCount() == 0)
        return {};
    const double key = mDataSource->keyAt(index);
    const double value = mDataSource->valueAt(0, index);
    if (drawsInDataSpace())
    {
        if (!mSystem)
            return {};
        const QCP::Pixels pixel = mSystem->transform().map(QCP::Data { key, value });
        return QPointF(pixel.x, pixel.y);
    }
    if (!mKeyAxis || !mValueAxis)
        return {};
    return coordsToPixels(key, value);
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
    if (drawsInDataSpace())
    {
        if (!mSystem)
            return -1;
        const bool beyond = mParentPlot
            && mParentPlot->interactions().testFlag(QCP::iSelectPlottablesBeyondAxisRect);
        if (!mSystem->contains(pos) && !beyond)
            return -1;
        return nearestMappedPoint(pos, details).value_or(-1);
    }
    if (!mKeyAxis || !mValueAxis)
        return -1;

    auto* axisRect = mKeyAxis->axisRect();
    if (!axisRect || !axisRect->rect().contains(pos.toPoint()))
        return -1;

    return nearestDataPoint(pos, details).value_or(-1);
}

QCPDataSelection QCPMultiGraph::selectTestRect(const QRectF& rect, bool onlySelectable) const
{
    QCPDataSelection unionResult;
    mLastRectSelections.clear();
    if ((onlySelectable && mSelectable == QCP::stNone) || !mDataSource || mDataSource->empty())
        return unionResult;

    if (drawsInDataSpace())
    {
        // B3: every candidate row is mapped once, in one batch, and kept when its pixel is in the
        // band. The key window reads a cartesian key axis, so there is none here.
        const int count = mDataSource->size();
        const int columns = qMin(mComponents.size(), mDataSource->columnCount());
        std::vector<int> shown;
        for (int c = 0; c < columns; ++c)
            if (mComponents[c].visible)
                shown.push_back(c);
        mLastRectSelections.resize(mComponents.size());
        if (shown.empty() || !mSystem)
            return unionResult;
        const std::size_t total = std::size_t(count) * shown.size();
        std::vector<double> keys;
        std::vector<double> values;
        keys.reserve(total);
        values.reserve(total);
        for (int c : shown)
            for (int i = 0; i < count; ++i)
            {
                keys.push_back(mDataSource->keyAt(i));
                values.push_back(mDataSource->valueAt(c, i));
            }
        std::vector<QPointF> pixels(total);
        mSystem->transform().map(std::span<const double>(keys), std::span<const double>(values),
                                 std::span<QPointF>(pixels));
        for (std::size_t k = 0; k < shown.size(); ++k)
        {
            const int c = shown[k];
            const std::size_t offset = k * std::size_t(count);
            QCPDataSelection colSel;
            int segmentBegin = -1;
            for (int i = 0; i < count; ++i)
            {
                const QPointF& pixel = pixels[offset + std::size_t(i)];
                const bool inside = std::isfinite(pixel.x()) && std::isfinite(pixel.y())
                    && rect.contains(pixel);
                if (segmentBegin == -1)
                {
                    if (inside)
                        segmentBegin = i;
                }
                else if (!inside)
                {
                    colSel.addDataRange(QCPDataRange(segmentBegin, i), false);
                    segmentBegin = -1;
                }
            }
            if (segmentBegin != -1)
                colSel.addDataRange(QCPDataRange(segmentBegin, count), false);
            colSel.simplify();
            mLastRectSelections[c] = colSel;
            for (int r = 0; r < colSel.dataRangeCount(); ++r)
                unionResult.addDataRange(colSel.dataRange(r), false);
        }
        unionResult.simplify();
        return unionResult;
    }

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
    if (!mDataSource || mDataSource->empty())
        return false;
    // B2: the data-space branch draws the displayed source, whatever the pipeline holds.
    if (drawsInDataSpace())
        return true;
    // A lost system is not "waiting for data": the layer must draw the object so drawableAxes()
    // gives its one report, which the stale-buffer path would otherwise skip forever.
    if (!mSystem)
        return true;
    if (!mKeyAxis || !mValueAxis)
        return false;
    // Pipeline active but L1 not ready — draw() will bail out
    if (mNeedsResampling && !mL1Cache && !mL2Result)
        return false;
    return true;
}

std::optional<double> QCPMultiGraph::hitTest(QCP::Pixels at, QVariant* details) const
{
    if (!mDataSource || mDataSource->empty())
        return std::nullopt;
    if (!drawsInDataSpace() && (!mKeyAxis || !mValueAxis))
        return std::nullopt;
    return nearestDataPoint(QPointF(at.x, at.y), details);
}

// B3: on the data-space branch a click is a 2D nearest search over the mapped rows, with no key
// window: the click's x is not the key under a shear.
std::optional<double> QCPMultiGraph::nearestMappedPoint(const QPointF& pos, QVariant* details) const
{
    const int count = mDataSource ? mDataSource->size() : 0;
    const int columns = mDataSource ? qMin(mComponents.size(), mDataSource->columnCount()) : 0;
    if (count <= 0 || columns <= 0 || !mSystem)
        return std::nullopt;

    std::vector<int> shown;
    for (int c = 0; c < columns; ++c)
        if (mComponents[c].visible)
            shown.push_back(c);
    if (shown.empty())
        return std::nullopt;

    // Every candidate row, once, in one transform batch.
    const std::size_t total = std::size_t(count) * shown.size();
    std::vector<double> keys;
    std::vector<double> values;
    keys.reserve(total);
    values.reserve(total);
    for (int c : shown)
        for (int i = 0; i < count; ++i)
        {
            keys.push_back(mDataSource->keyAt(i));
            values.push_back(mDataSource->valueAt(c, i));
        }
    std::vector<QPointF> pixels(total);
    mSystem->transform().map(std::span<const double>(keys), std::span<const double>(values),
                             std::span<QPointF>(pixels));

    double minDistSqr = (std::numeric_limits<double>::max)();
    int minIndex = -1;
    int minComponent = -1;
    double minKey = 0.0;
    double minValue = 0.0;
    for (std::size_t k = 0; k < shown.size(); ++k)
    {
        const int c = shown[k];
        const std::size_t offset = k * std::size_t(count);
        const bool segments = mComponents[c].lineStyle != lsNone;
        for (int i = 0; i < count; ++i)
        {
            const QPointF& pixel = pixels[offset + std::size_t(i)];
            if (!std::isfinite(pixel.x()) || !std::isfinite(pixel.y()))
                continue;
            const double distance = QCPVector2D(pixel - pos).lengthSquared();
            if (distance < minDistSqr)
            {
                minDistSqr = distance;
                minIndex = i;
                minComponent = c;
                minKey = mDataSource->keyAt(i);
                minValue = mDataSource->valueAt(c, i);
            }
            if (!segments || i + 1 >= count)
                continue;
            const QPointF& next = pixels[offset + std::size_t(i + 1)];
            if (!std::isfinite(next.x()) || !std::isfinite(next.y()))
                continue;
            const double lineDistance = QCPVector2D(pos).distanceSquaredToLine(pixel, next);
            if (lineDistance < minDistSqr)
            {
                minDistSqr = lineDistance;
                minIndex = i;
                minComponent = c;
                minKey = mDataSource->keyAt(i);
                minValue = mDataSource->valueAt(c, i);
            }
        }
    }

    if (minIndex < 0)
        return std::nullopt;
    if (details)
    {
        QVariantMap map;
        map["componentIndex"] = minComponent;
        map["dataIndex"] = minIndex;
        map["key"] = minKey;
        map["value"] = minValue;
        details->setValue(map);
    }
    return qSqrt(minDistSqr);
}

// The nearest data point of any visible component, in pixels: the point itself, or the segment
// that runs past it. Sets \a details to its component, row, key and value on a hit.
std::optional<double> QCPMultiGraph::nearestDataPoint(const QPointF& pos, QVariant* details) const
{
    if (drawsInDataSpace())
        return nearestMappedPoint(pos, details);
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

    if (minDistIndex < 0)
        return std::nullopt;
    if (details)
    {
        QVariantMap map;
        map["componentIndex"] = minDistComponent;
        map["dataIndex"] = minDistIndex;
        map["key"] = minKey;
        map["value"] = minValue;
            details->setValue(map);
    }
    return qSqrt(minDistSqr);
}

namespace
{

//! The colour of the data point \a dataIndex, transparent where it has none (a gap).
QRgb pointColour(const qcp::ColorScalarMapper& mapper, int dataIndex)
{
    const int bucket = mapper.bucket(dataIndex);
    return bucket == qcp::ColorScalarMapper::kGap ? qRgba(0, 0, 0, 0)
                                                  : qcp::runColor(mapper, bucket).rgba();
}

//! One colour per vertex of a shaped line. The index tables are built for a k+1 lookup, so a
//! segment takes the colour of the point it ends on and the last vertex repeats the one before it.
std::shared_ptr<const std::vector<QRgb>> segmentColours(const qcp::ColorScalarMapper& mapper,
                                                       const QVector<int>& vertexIndices)
{
    const int count = static_cast<int>(vertexIndices.size());
    auto colours
        = std::make_shared<std::vector<QRgb>>(static_cast<std::size_t>(count), qRgba(0, 0, 0, 0));
    for (int k = 0; k < count; ++k)
        (*colours)[static_cast<std::size_t>(k)]
            = pointColour(mapper, vertexIndices[std::min(k + 1, count - 1)]);
    return colours;
}

//! The same indices with a transparent vertex between each pair, aligned with separatedPairs().
QVector<int> separatedPairIndices(const QVector<int>& pairIndices)
{
    QVector<int> separated;
    separated.reserve(pairIndices.size() * 3 / 2);
    for (int i = 0; i + 1 < pairIndices.size(); i += 2)
    {
        if (i > 0)
            separated.append(-1);
        separated.append(pairIndices[i]);
        separated.append(pairIndices[i + 1]);
    }
    return separated;
}

//! One line of pairs with a vertex a polyline must not join through between them.
QVector<QPointF> separatedPairs(const QVector<QPointF>& vertices)
{
    const QPointF breakpoint(std::numeric_limits<double>::quiet_NaN(),
                             std::numeric_limits<double>::quiet_NaN());
    QVector<QPointF> separated;
    separated.reserve(vertices.size() * 3 / 2);
    for (int i = 0; i + 1 < vertices.size(); i += 2)
    {
        if (i > 0)
            separated.append(breakpoint);
        separated.append(vertices[i]);
        separated.append(vertices[i + 1]);
    }
    return separated;
}

// A centre step sits halfway between two points on screen: on a log key axis that is halfway in
// log10(key), so the steps are built there and mapped back.
QVector<QPointF> stepCentreLines(QVector<QPointF> points, bool logKeyAxis)
{
    if (!logKeyAxis)
        return qcp::toStepCenterLines(points, /*keyIsVertical=*/false);
    for (QPointF& point : points)
        point.setX(point.x() > 0.0 ? std::log10(point.x()) : qQNaN());
    QVector<QPointF> vertices = qcp::toStepCenterLines(points, /*keyIsVertical=*/false);
    for (QPointF& vertex : vertices)
        vertex.setX(std::pow(10.0, vertex.x()));
    return vertices;
}

std::vector<qcp::fill::StripPoint> steppedOnScreen(std::vector<qcp::fill::StripPoint> rows,
                                                   qcp::fill::Step step, bool logKeyAxis)
{
    if (!logKeyAxis || step != qcp::fill::Step::Center)
        return qcp::fill::stepped(rows, step);
    for (qcp::fill::StripPoint& row : rows)
        row.key = row.key > 0.0 ? std::log10(row.key) : qQNaN();
    std::vector<qcp::fill::StripPoint> stepped = qcp::fill::stepped(rows, step);
    for (qcp::fill::StripPoint& row : stepped)
        row.key = std::pow(10.0, row.key);
    return stepped;
}

std::shared_ptr<const std::vector<QPointF>> sharedPoints(const QVector<QPointF>& points)
{
    return std::make_shared<const std::vector<QPointF>>(points.begin(), points.end());
}

using FillSeries = std::pair<std::shared_ptr<const std::vector<QPointF>>,
                             std::shared_ptr<const std::vector<QPointF>>>;

// The two edges of a fill, as equal-length series. A gap pinches them together at the row before
// it and the row after it: the polygon and the ribbon then add no triangles across the break, and
// one fill stays one command.
FillSeries fillSeries(std::span<const qcp::fill::StripPoint> rows)
{
    std::vector<QPointF> upper, lower;
    upper.reserve(rows.size());
    lower.reserve(rows.size());
    bool havePrevious = false;
    bool pendingBreak = false;
    double previousKey = 0.0;
    double previousLower = 0.0;
    for (const qcp::fill::StripPoint& row : rows)
    {
        if (!std::isfinite(row.key) || !std::isfinite(row.a) || !std::isfinite(row.b))
        {
            pendingBreak = true;
            continue;
        }
        if ((row.breakBefore || pendingBreak) && havePrevious)
        {
            upper.emplace_back(previousKey, previousLower);
            lower.emplace_back(previousKey, previousLower);
            upper.emplace_back(row.key, row.b);
            lower.emplace_back(row.key, row.b);
        }
        pendingBreak = false;
        upper.emplace_back(row.key, row.a);
        lower.emplace_back(row.key, row.b);
        previousKey = row.key;
        previousLower = row.b;
        havePrevious = true;
    }
    if (upper.size() < 2)
        return { };
    return { std::make_shared<const std::vector<QPointF>>(std::move(upper)),
             std::make_shared<const std::vector<QPointF>>(std::move(lower)) };
}


} // namespace

void QCPMultiGraph::draw(QCPDrawContext& context) const
{
    PROFILE_HERE_N("QCPMultiGraph::draw");
    if (drawsInDataSpace())
    {
        drawInDataSpace(context);
        return;
    }
    if (!mKeyAxis || !mValueAxis || !mDataSource || mDataSource->empty())
        return;
    if (mKeyAxis->range().size() <= 0)
        return;

    // An export cannot wait for the scheduler: it resamples what it draws, now.
    if (!mL1Cache && mNeedsResampling)
    {
        if (!context.exporting())
            return;
        mPipeline.runSynchronously(ViewportParams::fromAxes(mKeyAxis.data(), mValueAxis.data()));
        bool level2Dirty = false;
        qcp::extractL1Cache<qcp::algo::MultiGraphResamplerCache>(mPipeline.cache(), mL1Cache,
                                                                 level2Dirty);
    }

    // The L2 bins are this frame's fresh lines: rebuilt here, and the revision does not move,
    // since the recording being made right now is made from them.
    if (mL1Cache && mL2Dirty)
    {
        rebuildL2(ViewportParams::fromAxes(mKeyAxis.data(), mValueAxis.data()));
        mL2Dirty = false;
    }

    // Data source priority: L2 (viewport-optimized) > raw. A recording is only made from what is
    // there: with no L1 yet and resampling needed, the frame it would record does not exist.
    QCPAbstractMultiDataSource* ds = nullptr;
    if (mL2Result)
        ds = mL2Result.get();
    else if (!mNeedsResampling || mL1Cache || context.exporting()
             || mKeyAxis->scaleType() == QCPAxis::stLogarithmic)
        ds = mDataSource.get();
    else
        return;

    // The bins cover the view, not the key window: an empty view says nothing about the window
    // around it, so its recording is remade on the next frame (it costs nothing).
    if (!ds || ds->empty())
    {
        (void)context.transform();
        return;
    }

    // An empty window is unbounded, not the range [0, 0]: the whole source is drawn.
    const QCPKeyWindow window = context.keyWindow();
    const bool unbounded = !(window.keys.size() > 0.0);

    const int begin = unbounded ? 0 : ds->findBegin(window.keys.lower);
    const int end = unbounded ? ds->size() : ds->findEnd(window.keys.upper);
    if (begin >= end)
        return;

    const bool logKeyAxis = mKeyAxis->scaleType() == QCPAxis::stLogarithmic;
    // The thinning buckets are equal spans of the key, mapped to equal pixel spans: on a log axis
    // that span is a decade, so pixelsPerKey counts pixels per decade there.
    const double axisPixels = window.pixelsPerKey > 0.0
        ? window.pixelsPerKey * mKeyAxis->range().size()
        : (mKeyAxis->coordToPixel(mKeyAxis->range().upper)
           - mKeyAxis->coordToPixel(mKeyAxis->range().lower));
    double pixelsPerKey = window.pixelsPerKey > 0.0 ? window.pixelsPerKey
                                                    : qcpKeyPixelsPerUnit(mKeyAxis.data());
    if (logKeyAxis)
    {
        const QCPRange keyRange = mKeyAxis->range();
        const double decades = std::log10(keyRange.upper / keyRange.lower);
        pixelsPerKey = decades > 0.0 ? axisPixels / decades : 0.0;
    }
    if (!mAdaptiveSampling)
        pixelsPerKey = 0.0; // every row
    const bool coloured = mColor.hasValues() && (ds == mDataSource.get() || mL2HasOrigin);

    // Fills and error bars are commands built in data space: the replays turn them into ribbons,
    // polygons or strokes. They record before the lines so both replays keep fills under bars
    // under lines.
    recordFills(context, *ds, begin, end);
    recordErrorBars(context, *ds, mL2Result == nullptr, begin, end);

    // In data space the impulse baseline is a value: zero, or the bottom of a log value axis,
    // which is where the pixel path clamps it.
    const bool logValueAxis = mValueAxis->scaleType() == QCPAxis::stLogarithmic;
    const double impulseBaseline = logValueAxis ? mValueAxis->range().lower : 0.0;
    const bool impulses = std::any_of(mComponents.begin(), mComponents.end(), [](const Component& c)
                                      { return c.visible && c.lineStyle == lsImpulse; });
    if (logValueAxis && impulses)
        (void)context.transform(); // the baseline follows the axis' lower bound

    const int columns = std::min(componentCount(), ds->columnCount());
    for (int c = 0; c < columns; ++c)
    {
        const Component& comp = component(c);
        if (!comp.visible)
            continue;
        if (comp.lineStyle == lsNone && comp.scatterStyle.isNone())
            continue;

        // The rows a vertex comes from are only asked for when a colour needs them.
        std::vector<int> sourceIndices;
        std::vector<QPointF> line
            = ds->linesInData(c, begin, end, pixelsPerKey, coloured ? &sourceIndices : nullptr,
                              logKeyAxis);
        if (line.empty())
            continue;
        // Defensive: indices that do not match the points are never used.
        const bool indexed = coloured && sourceIndices.size() == line.size();
        const QPen pen = comp.selection.isEmpty() ? comp.pen : comp.selectedPen;
        const bool hasMarkers = !comp.scatterStyle.isNone();
        // A plain line is the recorded line: it is handed over, not copied. A step, an impulse or
        // a marker set needs the points in another shape.
        const bool borrowsTheLine = comp.lineStyle == lsLine && !hasMarkers;
        QVector<QPointF> points;
        if (!borrowsTheLine)
            points = QVector<QPointF>(line.begin(), line.end());
        QVector<int> dataIdx;
        if (indexed)
            dataIdx = QVector<int>(sourceIndices.begin(), sourceIndices.end());

        if (comp.lineStyle != lsNone)
        {
            QPen linePen = pen;
            if (borrowsTheLine)
            {
                if (line.size() >= 2)
                    context.polyline(std::make_shared<const std::vector<QPointF>>(std::move(line)),
                                     indexed ? segmentColours(mColor, dataIdx) : nullptr, linePen);
            }
            else
            {
                QVector<QPointF> vertices;
                switch (comp.lineStyle)
                {
                    case lsStepLeft:
                        vertices = qcp::toStepLeftLines(points, /*keyIsVertical=*/false);
                        break;
                    case lsStepRight:
                        vertices = qcp::toStepRightLines(points, /*keyIsVertical=*/false);
                        break;
                    case lsStepCenter:
                        vertices = stepCentreLines(points, logKeyAxis);
                        break;
                    case lsImpulse:
                        vertices = separatedPairs(
                            qcp::toImpulseLines(points, /*keyIsVertical=*/false, impulseBaseline));
                        linePen.setCapStyle(Qt::FlatCap);
                        break;
                    default:
                        vertices = points;
                        break;
                }
                if (vertices.size() >= 2)
                {
                    std::shared_ptr<const std::vector<QRgb>> colours;
                    if (indexed)
                    {
                        QVector<int> vertexIdx = lineIndices(comp.lineStyle, dataIdx);
                        if (comp.lineStyle == lsImpulse)
                            vertexIdx = separatedPairIndices(vertexIdx);
                        colours = segmentColours(mColor, vertexIdx);
                    }
                    context.polyline(sharedPoints(vertices), colours, linePen);
                }
            }
        }

        // Markers sit at the data positions, not at the corners a step or an impulse adds.
        if (!hasMarkers)
            continue;
        const QCPScatterStyle style = markerStyle(comp.scatterStyle, comp.pen);
        auto markerPoints = std::make_shared<std::vector<QPointF>>();
        auto markerColours = std::make_shared<std::vector<QRgb>>();
        const int skip = mScatterSkip + 1;
        for (int i = 0; i < points.size(); i += skip)
        {
            if (!qIsFinite(points[i].x()) || !qIsFinite(points[i].y()))
                continue;
            markerPoints->push_back(points[i]);
            if (indexed)
                markerColours->push_back(
                    pointColour(mColor, sourceIndices[static_cast<std::size_t>(i)]));
        }
        if (!markerPoints->empty())
            context.markers(markerPoints, indexed ? markerColours : nullptr, style);
    }
}

// B2/P-4: the data-space branch on a system that is not cartesian. It records positions and the
// fill, error-bar and colour commands the replay maps through the frame's transform; it reads the
// displayed source directly, with no thinning (pixelsPerKey = 0) and no L1/L2.
void QCPMultiGraph::drawInDataSpace(QCPDrawContext& context) const
{
    PROFILE_HERE_N("QCPMultiGraph::drawInDataSpace");
    if (!mDataSource || mDataSource->empty() || !mSystem)
        return;

    QCPAbstractMultiDataSource& ds = *mDataSource;
    const QCPKeyWindow window = context.keyWindow();
    const bool unbounded = !(window.keys.size() > 0.0);
    const int begin = unbounded ? 0 : ds.findBegin(window.keys.lower);
    const int end = unbounded ? ds.size() : ds.findEnd(window.keys.upper);
    if (begin >= end)
        return;

    const double baseline = dataSpaceBaseline();
    const bool logKeyAxis = [this]
    {
        const QCPAxis* axis = qobject_cast<const QCPAxis*>(keyAxisBase());
        return axis && axis->scaleType() == QCPAxis::stLogarithmic;
    }();
    const bool logValueAxis = [this]
    {
        const QCPAxis* axis = qobject_cast<const QCPAxis*>(valueAxisBase());
        return axis && axis->scaleType() == QCPAxis::stLogarithmic;
    }();
    const bool impulses = std::any_of(mComponents.begin(), mComponents.end(), [](const Component& c)
                                      { return c.visible && c.lineStyle == lsImpulse; });
    if (logValueAxis && impulses)
        (void)context.transform(); // the baseline follows the value axis' range bound

    // Fills and error bars are data-space commands too; both record before the lines so the replay
    // keeps fills under bars under lines.
    recordFills(context, ds, begin, end);
    recordErrorBars(context, ds, /*rawRows=*/true, begin, end);

    const int columns = std::min(componentCount(), ds.columnCount());
    for (int c = 0; c < columns; ++c)
    {
        const Component& comp = component(c);
        if (!comp.visible)
            continue;
        if (comp.lineStyle == lsNone && comp.scatterStyle.isNone())
            continue;

        // simplify: no decimation on this branch; every row is recorded and mapped per frame.
        // Upgrade: the J-6 box query and data-space decimation by keyWindow().
        const bool coloured = mColor.hasValues();
        std::vector<int> sourceIndices;
        std::vector<QPointF> line
            = ds.linesInData(c, begin, end, /*pixelsPerKey=*/0.0,
                             coloured ? &sourceIndices : nullptr, /*logKeys=*/false,
                             /*cacheGaps=*/false);
        if (line.empty())
            continue;
        const bool indexed = coloured && sourceIndices.size() == line.size();
        const QPen pen = comp.selection.isEmpty() ? comp.pen : comp.selectedPen;
        const bool hasMarkers = !comp.scatterStyle.isNone();
        const bool borrowsTheLine = comp.lineStyle == lsLine && !hasMarkers;
        QVector<QPointF> points;
        if (!borrowsTheLine)
            points = QVector<QPointF>(line.begin(), line.end());
        QVector<int> dataIdx;
        if (indexed)
            dataIdx = QVector<int>(sourceIndices.begin(), sourceIndices.end());

        if (comp.lineStyle != lsNone)
        {
            QPen linePen = pen;
            if (borrowsTheLine)
            {
                if (line.size() >= 2)
                    context.polyline(std::make_shared<const std::vector<QPointF>>(std::move(line)),
                                     indexed ? segmentColours(mColor, dataIdx) : nullptr, linePen);
            }
            else
            {
                QVector<QPointF> vertices;
                switch (comp.lineStyle)
                {
                    case lsStepLeft:
                        vertices = qcp::toStepLeftLines(points, /*keyIsVertical=*/false);
                        break;
                    case lsStepRight:
                        vertices = qcp::toStepRightLines(points, /*keyIsVertical=*/false);
                        break;
                    case lsStepCenter:
                        vertices = stepCentreLines(points, logKeyAxis);
                        break;
                    case lsImpulse:
                        vertices = separatedPairs(
                            qcp::toImpulseLines(points, /*keyIsVertical=*/false, baseline));
                        linePen.setCapStyle(Qt::FlatCap);
                        break;
                    default:
                        vertices = points;
                        break;
                }
                if (vertices.size() >= 2)
                {
                    std::shared_ptr<const std::vector<QRgb>> colours;
                    if (indexed)
                    {
                        QVector<int> vertexIdx = lineIndices(comp.lineStyle, dataIdx);
                        if (comp.lineStyle == lsImpulse)
                            vertexIdx = separatedPairIndices(vertexIdx);
                        colours = segmentColours(mColor, vertexIdx);
                    }
                    context.polyline(sharedPoints(vertices), colours, linePen);
                }
            }
        }

        if (!hasMarkers)
            continue;
        const QCPScatterStyle style = markerStyle(comp.scatterStyle, comp.pen);
        auto markerPoints = std::make_shared<std::vector<QPointF>>();
        auto markerColours = std::make_shared<std::vector<QRgb>>();
        const int skip = mScatterSkip + 1;
        for (int i = 0; i < points.size(); i += skip)
        {
            if (!qIsFinite(points[i].x()) || !qIsFinite(points[i].y()))
                continue;
            markerPoints->push_back(points[i]);
            if (indexed)
                markerColours->push_back(
                    pointColour(mColor, sourceIndices[static_cast<std::size_t>(i)]));
        }
        if (!markerPoints->empty())
            context.markers(markerPoints, indexed ? markerColours : nullptr, style);
    }
}

bool QCPMultiGraph::fillDrawable(const Fill& fill, int columnCount) const
{
    const auto shown = [&](int column) {
        return column >= 0 && column < columnCount && column < mComponents.size()
            && mComponents[column].visible;
    };
    if (!shown(fill.owner) || mComponents[fill.owner].lineStyle == lsImpulse)
        return false;
    if (const auto* target = std::get_if<int>(&fill.target))
        return shown(*target);
    if (const auto* target = std::get_if<FillToGraph>(&fill.target))
    {
        const QCPMultiGraph* partner = target->graph.data();
        if (!partner || partner == this || partner->system() != system())
            return false;
        return partner->mDataSource
            && target->component >= 0
            && target->component < partner->mDataSource->columnCount();
    }
    return true;
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
    const auto gaps = ds.gapsBefore(begin, end, /*cacheGaps=*/!drawsInDataSpace());
    const auto* targetColumn = std::get_if<int>(&fill.target);
    const auto edge = [this](double value)
    {
        return drawsInDataSpace() ? dataSpaceFillValue(value) : fillValue(*mValueAxis, value);
    };
    Q_ASSERT(gaps.size() == std::max(0, end - begin)); // gapsBefore() holds one entry per row
    const double constant = targetColumn ? 0.0 : edge(std::get<double>(fill.target));
    std::vector<qcp::fill::StripPoint> rows;
    rows.reserve(std::max(0, end - begin));
    for (int i = begin; i < end; ++i)
        rows.push_back({ ds.keyAt(i), edge(ds.valueAt(fill.owner, i)),
                         targetColumn ? edge(ds.valueAt(*targetColumn, i)) : constant,
                         gaps[i - begin] != 0, i });
    return rows;
}

// A log value axis has no zero: a fill to zero stops at the axis' lower bound, where the pixel
// path clamped it. The recording then follows that bound, so it is transform-dependent.
double QCPMultiGraph::fillValue(const QCPAxis& valueAxis, double value)
{
    if (valueAxis.scaleType() == QCPAxis::stLinear || value > 0 || std::isnan(value))
        return value;
    return valueAxis.range().lower;
}

// Binned when the rows outnumber 4 per pixel, the lines' own L2 density (kLevel2PixelMultiplier).
std::vector<qcp::fill::StripPoint> QCPMultiGraph::shapeFillRows(
    const Fill& fill, std::vector<qcp::fill::StripPoint> rows) const
{
    // The binning is a pixel decision, and the data-space branch has no key pixel: it keeps every
    // row (the simplify note in drawInDataSpace names the ceiling).
    if (!drawsInDataSpace())
        rows = binnedWhenDense(std::move(rows));
    // The system's key axis is the one a straight-segment frame draws in log space.
    const QCPAxis* keyAxis = drawsInDataSpace() ? qobject_cast<const QCPAxis*>(keyAxisBase())
                                                : mKeyAxis.data();
    const bool logKeyAxis = keyAxis && keyAxis->scaleType() == QCPAxis::stLogarithmic;
    return steppedOnScreen(std::move(rows), ownerStep(mComponents[fill.owner]), logKeyAxis);
}

std::vector<qcp::fill::StripPoint> QCPMultiGraph::fillToGraphRows(
    const Fill& fill, const QCPAbstractMultiDataSource& ds, int begin, int end) const
{
    const auto* target = std::get_if<FillToGraph>(&fill.target);
    QCPMultiGraph* partner = target ? target->graph.data() : nullptr;
    if (!partner || partner == this || partner->system() != system())
        return {};
    QCPAbstractMultiDataSource* partnerSource = partner->mDataSource.get();
    if (!partnerSource || target->component < 0
        || target->component >= partnerSource->columnCount())
        return {};

    std::vector<qcp::fill::SeriesRow> ownerRows;
    ownerRows.reserve(std::size_t(std::max(0, end - begin)));
    const auto ownerGaps = ds.gapsBefore(begin, end, /*cacheGaps=*/!drawsInDataSpace());
    for (int i = begin; i < end; ++i)
        ownerRows.push_back({ ds.keyAt(i), ds.valueAt(fill.owner, i),
                              ownerGaps[i - begin] != 0, i });
    if (ownerRows.size() < 2)
        return {};

    // The partner is cropped to the owner's key span, with one row on each side for the
    // interpolation at the two ends.
    const double lower = ownerRows.front().key;
    const double upper = ownerRows.back().key;
    const int partnerBegin = std::max(0, partnerSource->findBegin(lower) - 1);
    const int partnerEnd = std::min(partnerSource->size(), partnerSource->findEnd(upper) + 1);
    const auto partnerGaps = partnerSource->gapsBefore(partnerBegin, partnerEnd,
                                                       /*cacheGaps=*/!drawsInDataSpace());
    std::vector<qcp::fill::SeriesRow> partnerRows;
    partnerRows.reserve(std::size_t(std::max(0, partnerEnd - partnerBegin)));
    for (int i = partnerBegin; i < partnerEnd; ++i)
        partnerRows.push_back({ partnerSource->keyAt(i),
                                partnerSource->valueAt(target->component, i),
                                partnerGaps[i - partnerBegin] != 0, i });

    std::vector<qcp::fill::StripPoint> rows
        = qcp::fill::fillToGraphRows(ownerRows, partnerRows);
    const bool dataSpace = drawsInDataSpace();
    const auto edge = [this, dataSpace](double value)
    { return dataSpace ? dataSpaceFillValue(value) : fillValue(*mValueAxis, value); };
    for (qcp::fill::StripPoint& row : rows)
    {
        row.a = edge(row.a);
        row.b = edge(row.b);
    }
    return rows;
}

std::vector<qcp::fill::StripPoint> QCPMultiGraph::binnedWhenDense(
    std::vector<qcp::fill::StripPoint> rows) const
{
    const QRect rect = mKeyAxis->axisRect()->rect();
    const int pixelLength = mKeyAxis->orientation() == Qt::Vertical ? rect.height() : rect.width();
    if (rows.size() <= 4 * std::size_t(std::max(0, pixelLength)))
        return rows;
    const double quarterPixel = 4.0 * std::max(1, pixelLength);
    if (mKeyAxis->scaleType() == QCPAxis::stLogarithmic)
    {
        const QCPRange range = mKeyAxis->range();
        if (range.lower <= 0.0 || range.upper <= 0.0)
            return rows;
        const double width = std::log10(range.upper / range.lower) / quarterPixel;
        if (!(width > 0.0))
            return rows;
        for (qcp::fill::StripPoint& row : rows)
            row.key = row.key > 0.0 && std::isfinite(row.key) ? std::log10(row.key) : std::nan("");
        std::vector<qcp::fill::StripPoint> spans = qcp::fill::binToSpans(rows, width);
        for (qcp::fill::StripPoint& span : spans)
            span.key = std::pow(10.0, span.key);
        return spans;
    }
    const double width = std::abs(mKeyAxis->range().size()) / quarterPixel;
    if (!(width > 0.0))
        return rows;
    return qcp::fill::binToSpans(rows, width);
}

bool QCPMultiGraph::rowIsSelected(const Fill& fill, const QCPAbstractMultiDataSource& ds,
                                  int row) const
{
    if (fill.owner < 0 || fill.owner >= mComponents.size())
        return false;
    const QCPDataSelection& selection = mComponents[fill.owner].selection;
    if (selection.isEmpty())
        return false;
    if (mDataSource && &ds == mDataSource.get())
    {
        for (int i = 0; i < selection.dataRangeCount(); ++i)
        {
            const QCPDataRange range = selection.dataRange(i);
            if (row >= range.begin() && row < range.end())
                return true;
        }
        return false;
    }

    // A resampled row is not a source row: a selection range becomes the key interval its first
    // and last source rows span.
    if (!mDataSource || row < 0 || row >= ds.size())
        return false;
    const double key = ds.keyAt(row);
    for (int i = 0; i < selection.dataRangeCount(); ++i)
    {
        const QCPDataRange range = selection.dataRange(i);
        const int begin = std::clamp(range.begin(), 0, mDataSource->size());
        const int end = std::clamp(range.end(), begin, mDataSource->size());
        if (begin >= end)
            continue;
        const double lower = mDataSource->keyAt(begin);
        const double upper = mDataSource->keyAt(end - 1);
        if (key >= lower && key <= upper)
            return true;
    }
    return false;
}

// One fillBetween command per selected/unselected run of this graph's component selection. The
// split is decided on the raw source rows, before binning or step expansion changes the indices. A
// gap pinches both series together, so neither the polygon nor the ribbon spans it.
void QCPMultiGraph::recordFills(QCPDrawContext& context, const QCPAbstractMultiDataSource& ds,
                                int begin, int end) const
{
    for (const Fill& fill : mFills)
    {
        if (!fillDrawable(fill, ds.columnCount()))
            continue;
        const std::vector<qcp::fill::StripPoint> raw
            = std::holds_alternative<FillToGraph>(fill.target)
                ? fillToGraphRows(fill, ds, begin, end)
                : fillRows(fill, ds, begin, end);
        if (raw.empty())
            continue;

        const QCPAxis* valueAxis = qobject_cast<const QCPAxis*>(valueAxisBase());
        if (valueAxis && valueAxis->scaleType() == QCPAxis::stLogarithmic)
            (void)context.transform(); // the baseline follows the axis' bound nearer zero

        const auto recordRun = [&](std::size_t first, std::size_t last, bool selected) {
            const QBrush brush = fillRunBrush(fill, selected);
            if (brush.color().alpha() == 0)
                return;
            std::vector<qcp::fill::StripPoint> run(raw.begin() + std::ptrdiff_t(first),
                                                   raw.begin() + std::ptrdiff_t(last));
            const std::vector<qcp::fill::StripPoint> rows = shapeFillRows(fill, std::move(run));
            if (rows.empty())
                return;
            const auto [upper, lower] = fillSeries(rows);
            if (!upper)
                return;
            context.fillBetween(upper, lower, brush);
        };
        const auto selectedAt = [&](std::size_t index)
        {
            const int row = raw[index].sourceRow >= 0 ? raw[index].sourceRow : int(index);
            return rowIsSelected(fill, ds, row);
        };

        if (mComponents[fill.owner].selection.isEmpty())
        {
            recordRun(0, raw.size(), false);
            continue;
        }

        std::size_t first = 0;
        while (first < raw.size())
        {
            const bool selected = selectedAt(first);
            std::size_t last = first + 1;
            while (last < raw.size() && selectedAt(last) == selected)
                ++last;
            // The next run's first row closes this run's last segment; both may draw it.
            recordRun(first, std::min(last + 1, raw.size()), selected);
            first = last;
        }
    }
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

std::vector<qcp::errorbars::Bar> QCPMultiGraph::errorBarRows(const ErrorBars& spec,
                                                             const QCPAbstractMultiDataSource& ds,
                                                             int begin, int end) const
{
    std::vector<qcp::errorbars::Bar> bars;
    bars.reserve(std::max(0, end - begin));
    const bool dataSpace = drawsInDataSpace();
    const auto edge = [this, dataSpace](double value)
    { return dataSpace ? value : fillValue(*mValueAxis, value); };
    for (int i = begin; i < end; ++i)
    {
        const double y = edge(ds.valueAt(spec.value, i));
        // std::min/max return their first argument when either is NaN, so a NaN bound stays NaN.
        // Unlike the helpers they replace, a -inf lower or +inf upper value now wins over the
        // bound. On the data-space branch the values stay raw: a log frame maps what it can and a
        // side it cannot place is dropped by the replay, never clamped onto another value.
        bars.push_back({ ds.keyAt(i), y,
                         edge(std::min(ds.valueAt(spec.lower, i), y)),
                         edge(std::max(ds.valueAt(spec.upper, i), y)) });
    }
    return bars;
}

// The fit question is asked in pixels: a bar outside the view, or one without a value, does not
// count.
bool QCPMultiGraph::errorBarsFitInView(std::span<const qcp::errorbars::Bar> bars,
                                       double minSpacingPx) const
{
    const QRect rect = mKeyAxis->axisRect()->rect();
    const bool vertical = mKeyAxis->orientation() == Qt::Vertical;
    const double first = vertical ? rect.top() : rect.left();
    const double last = vertical ? rect.bottom() : rect.right();
    std::vector<qcp::errorbars::Bar> shown;
    for (qcp::errorbars::Bar bar : bars)
    {
        const double pixel = mKeyAxis->coordToPixel(bar.key);
        if (std::isfinite(pixel) && pixel >= first && pixel <= last && qcp::errorbars::resolve(bar))
        {
            bar.key = pixel;
            shown.push_back(bar);
        }
    }
    return qcp::errorbars::barsFit(shown, minSpacingPx);
}

std::vector<qcp::fill::StripPoint> QCPMultiGraph::errorBandRows(
    const ErrorBars& spec, const QCPAbstractMultiDataSource& ds, int begin, int end) const
{
    const auto bars = errorBarRows(spec, ds, begin, end);
    const auto gaps = ds.gapsBefore(begin, end, /*cacheGaps=*/!drawsInDataSpace());
    Q_ASSERT(gaps.size() == int(bars.size())); // gapsBefore() holds one entry per row
    std::vector<qcp::fill::StripPoint> rows;
    rows.reserve(bars.size());
    for (int k = 0; k < int(bars.size()); ++k)
    {
        qcp::errorbars::Bar bar = bars[k];
        qcp::errorbars::resolve(bar);
        rows.push_back({bar.key, bar.lower, bar.upper, gaps[k] != 0});
    }
    return binnedWhenDense(std::move(rows));
}

// Sparse bars are one command: the replay draws every stem and cap. Dense bars are the band they
// were, a fillBetween at the bars' colour, alpha 64.
void QCPMultiGraph::recordErrorBars(QCPDrawContext& context, const QCPAbstractMultiDataSource& ds,
                                    bool rawRows, int begin, int end) const
{
    if (drawsInDataSpace())
    {
        // A bar is two data-space ends: the replay maps them and turns the caps into pixel
        // whiskers perpendicular to the mapped bar. There is no pixel fit and no dense band: the
        // band is a pixel binning of the key axis, which this frame does not have.
        for (const ErrorBars& spec : mErrorBars)
        {
            if (!errorBarsDrawable(spec, ds.columnCount()))
                continue;
            const std::vector<qcp::errorbars::Bar> bars = errorBarRows(spec, ds, begin, end);
            std::vector<QPointF> centres, ends;
            centres.reserve(bars.size());
            ends.reserve(2 * bars.size());
            for (qcp::errorbars::Bar bar : bars)
            {
                if (!qcp::errorbars::resolve(bar) || !std::isfinite(bar.key))
                    continue;
                centres.emplace_back(bar.key, 0.5 * (bar.lower + bar.upper));
                ends.emplace_back(bar.key, bar.lower);
                ends.emplace_back(bar.key, bar.upper);
            }
            if (centres.empty())
                continue;
            const Component& comp = mComponents[spec.value];
            const QPen pen = comp.selection.isEmpty() ? errorBarPen(spec) : comp.selectedPen;
            context.errorBars(std::make_shared<const std::vector<QPointF>>(std::move(centres)),
                              std::make_shared<const std::vector<QPointF>>(std::move(ends)),
                              /*alongKeys=*/false, spec.capWidthPx, /*symbolGap=*/0.0, pen);
        }
        return;
    }
    for (const ErrorBars& spec : mErrorBars)
    {
        if (!errorBarsDrawable(spec, ds.columnCount()))
            continue;
        (void)context.transform(); // the caps are a pixel width and the fit is a pixel question
        const std::vector<qcp::errorbars::Bar> bars = errorBarRows(spec, ds, begin, end);
        // A band builds its own rows: its bounds <= 0 on a log axis stop at the axis edge.
        const bool band = !rawRows || !errorBarsFitInView(bars, 3.0 * spec.capWidthPx);
        if (band)
        {
            const auto [upper, lower] = fillSeries(errorBandRows(spec, ds, begin, end));
            if (!upper)
                continue;
            QColor color = errorBarPen(spec).color();
            color.setAlpha(64);
            context.fillBetween(upper, lower, QBrush(color));
            continue;
        }

        std::vector<QPointF> centres, spans;
        centres.reserve(bars.size());
        spans.reserve(bars.size());
        const double halfCap = spec.capWidthPx / 2.0;
        for (qcp::errorbars::Bar bar : bars)
        {
            if (!qcp::errorbars::resolve(bar) || !std::isfinite(bar.key))
                continue;
            const double pixel = mKeyAxis->coordToPixel(bar.key);
            const double left = mKeyAxis->pixelToCoord(pixel - halfCap);
            const double right = mKeyAxis->pixelToCoord(pixel + halfCap);
            centres.emplace_back(bar.key, 0.5 * (bar.lower + bar.upper));
            spans.emplace_back(0.5 * std::abs(right - left), 0.5 * (bar.upper - bar.lower));
        }
        if (centres.empty())
            continue;
        const Component& comp = mComponents[spec.value];
        const QPen pen = comp.selection.isEmpty() ? errorBarPen(spec) : comp.selectedPen;
        context.errorBars(std::make_shared<const std::vector<QPointF>>(std::move(centres)),
                          std::make_shared<const std::vector<QPointF>>(std::move(spans)), pen);
    }
}

// The icon is a recording of its own: one unit-wide segment per shown component, fitted into the
// rect by the replay, so it never depends on the plot's data or axis ranges.
void QCPMultiGraph::drawLegendIcon(QCPDrawContext& icon) const
{
    const QVector<int> shown = legendComponents();
    if (shown.isEmpty())
        return;

    const double segments = shown.size();
    // The visible extent: it gives the fitted transform the icon's box.
    icon.rect(QCP::Data { 0.0, -0.5 }, QCP::Data { segments, 0.5 }, QPen(Qt::NoPen),
              QBrush(Qt::NoBrush));
    for (int k = 0; k < shown.size(); ++k)
    {
        const Component& comp = mComponents[shown[k]];
        if (!comp.visible)
            continue;
        const double x0 = k;
        const double x1 = k + 1;
        const double centre = 0.5 * (x0 + x1);
        if (const auto fill = componentFill(shown[k]))
            icon.rect(QCP::Data { x0, -0.3 }, QCP::Data { x1, 0.3 }, QPen(Qt::NoPen),
                      effectiveFillBrush(*fill));
        if (mColor.hasValues())
        {
            std::vector<QPointF> points;
            std::vector<QRgb> colours;
            for (int s = 0; s <= 4; ++s)
            {
                points.emplace_back(x0 + (x1 - x0) * s / 4.0, 0.0);
                colours.push_back(qcp::runColor(mColor, s * 255 / 4).rgba());
            }
            icon.polyline(std::make_shared<const std::vector<QPointF>>(std::move(points)),
                          std::make_shared<const std::vector<QRgb>>(std::move(colours)), comp.pen);
        }
        else if (comp.lineStyle != lsNone)
        {
            icon.polyline(sharedPoints(QVector<QPointF> { QPointF(x0, 0.0), QPointF(x1, 0.0) }),
                          comp.pen);
        }
        if (!comp.scatterStyle.isNone())
            icon.markers(sharedPoints(QVector<QPointF> { QPointF(centre, 0.0) }),
                         markerStyle(comp.scatterStyle, comp.pen));
        if (const auto bars = componentErrorBars(shown[k]))
        {
            // 5 px half stem, 3 px half cap on a 20 px segment, in segment fractions.
            icon.errorBars(sharedPoints(QVector<QPointF> { QPointF(centre, 0.0) }),
                           sharedPoints(QVector<QPointF> { QPointF(0.15, 0.25) }),
                           errorBarPen(*bars));
        }
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

