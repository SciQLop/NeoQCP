#include "plottable-graph.h"
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
#include "../selectiondecorator-bracket.h"
#include "../vector2d.h"
#include "plottable-draw-utils.h"
#include "plottable-l1-cache.h"
#include "plottable-linestyle.h"
#include <QPointer>
#include <algorithm>
#include <cmath>
#include <typeinfo>
#include <utility>

static void ensureL1Transform(QCPGraphPipeline& pipeline, int sourceSize, int colCount,
                              std::shared_ptr<std::atomic<bool>> wantOrigin);
static bool isAboveResampleThreshold(const QCPAbstractMultiDataSource& src);

QCPGraph::QCPGraph(QCPAxis* keyAxis, QCPAxis* valueAxis, QCP::Capabilities required)
        : QCPAbstractLine(keyAxis, valueAxis, required)
    , mPipeline(parentPlot() ? parentPlot()->pipelineScheduler() : nullptr, this)
{
    // The graph list (2.x's addGraph/graph(i)): a graph its frame refused registered nowhere and
    // must not leave a dangling entry there either. A graph the base registered (a cross-plot pair
    // it owns but keeps inert included) is listed.
    if (mParentPlot && isRegistered())
        mParentPlot->registerGraph(this);

    // Base constructor auto-adds a QCPPlottableLegendItem (vtable not yet set up
    // for virtual addToLegend). Replace it with our group legend item. A refused object has no
    // system and no row to replace.
    if (mSystem && mParentPlot && mParentPlot->autoAddPlottableToLegend() && mParentPlot->legend) {
        QCPAbstractPlottable::removeFromLegend(mParentPlot->legend);
        addToLegend();
    }

    followAxes();

    connect(&mPipeline, &QCPGraphPipeline::finished,
            this, [this](uint64_t gen) { onL1Ready(gen); });
    connect(&mPipeline, &QCPGraphPipeline::busyChanged,
            this, [this](bool) { updateEffectiveBusy(); });

    mViewportDebounce.setSingleShot(true);
    mViewportDebounce.setInterval(150);
    connect(&mViewportDebounce, &QTimer::timeout, this, [this] { linesChanged(); });
}

QCPGraph::~QCPGraph() = default;

bool QCPGraph::drawsInDataSpace() const
{
    return mSystem && mSystem->kind() != QCPTransform::Kind::Cartesian;
}

bool QCPGraph::areaIsAxisRect() const
{
    return mSystem && qobject_cast<QCPAxisRect*>(mSystem->area());
}

bool QCPGraph::hasExplicitColorScale() const
{
    return mExplicitColorScale && colorScale().get() != nullptr;
}

bool QCPGraph::colourNeedsAScale() const
{
    return drawsInDataSpace() && !areaIsAxisRect() && !hasExplicitColorScale();
}

double QCPGraph::dataSpaceBaseline() const
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

double QCPGraph::dataSpaceFillValue(double value) const
{
    const QCPAxis* valueAxis = qobject_cast<const QCPAxis*>(valueAxisBase());
    return valueAxis ? fillValue(*valueAxis, value) : value;
}

quint64 QCPGraph::dependencyRevision() const
{
    // B4: on the data-space branch the recording reads the system's key window and its baseline,
    // so a mapping change has to make it stale even though no data or style moved.
    quint64 revision = drawsInDataSpace() ? qHashMulti(0, mRevision, mSystem->revision())
                                          : mRevision;
    // B8: a fill to another graph reads that graph's data and system. Its revision moves on both,
    // so a partner data change or a partner system move makes this recording stale.
    // Across value axes the band is the partner's pixels on this graph's value axis: it moves with
    // either value axis, so both systems' revisions count.
    for (const Fill& fill : mFills)
        if (const auto* target = std::get_if<FillToGraph>(&fill.target))
        {
            const QCPGraph* partner = target->graph.data();
            revision = qHashMulti(revision, partner ? partner->mRevision : 0);
            if (partner && fillsAcrossValueAxes(*partner))
                revision = qHashMulti(revision, mSystem->revision(), partner->mSystem->revision());
        }
    return revision;
}

void QCPGraph::followAxes()
{
    for (const auto& link : std::exchange(mAxisLinks, {}))
        disconnect(link);
    if (keyAxisRaw())
    {
        mAxisLinks << connect(keyAxisRaw(), &QCPAxis::rangeChanged, this,
                              &QCPGraph::onViewportChanged);
        mAxisLinks << connect(keyAxisRaw(), &QCPAxis::scaleTypeChanged, this,
                              [this] { linesChanged(); });
    }
    if (valueAxisRaw())
        mAxisLinks << connect(valueAxisRaw(), &QCPAxis::scaleTypeChanged, this,
                              [this] { linesChanged(); });
    linesChanged(); // pixels computed for the old axes mean nothing on the new ones
    mL2Dirty = mL1Cache != nullptr; // nor do the bins resampled for the old key range
}

void QCPGraph::systemChanged()
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
    mNeedsResampling = mDataSource && isAboveResampleThreshold(*mDataSource);
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
        ensureL1Transform(mPipeline, mDataSource->size(), mDataSource->columnCount(),
                               mWantOrigin);
        const QPointer<QCPGraph> alive(this);
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
void QCPGraph::linesChanged()
{
    ++mRevision;
    if (mLayer)
        mLayer->markDirty();
    // A graph filled to a component of this one draws this data: mark it wherever its layer is.
    markDependentsDirty();
}

void QCPGraph::setDataSource(std::unique_ptr<QCPAbstractMultiDataSource> source)
{
    markDirty();
    setDataSource(std::shared_ptr<QCPAbstractMultiDataSource>(std::move(source)));
}

static void ensureL1Transform(QCPGraphPipeline& pipeline, int sourceSize, int colCount,
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
                    return qcp::algo::buildL1Cache(src, vp, cache, wantOrigin->load());
                });
        }
    }
    else if (pipeline.hasTransform())
    {
        pipeline.clearTransform();
    }
}

static bool isAboveResampleThreshold(const QCPAbstractMultiDataSource& src)
{
    return src.columnCount() > 0
        && static_cast<int64_t>(src.size()) * src.columnCount() >= qcp::algo::kResampleThreshold;
}

void QCPGraph::setDataSource(std::shared_ptr<QCPAbstractMultiDataSource> source)
{
    // Any source installed this way is not the graph's: addData() appends only to the growable
    // source setData() built.
    mOwnedSource.reset();
    installSource(std::move(source));
}

void QCPGraph::setOwnedSource(std::shared_ptr<QCPGrowableMultiDataSource> source)
{
    mOwnedSource = source;
    installSource(std::move(source));
}

void QCPGraph::installSource(std::shared_ptr<QCPAbstractMultiDataSource> source)
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

std::shared_ptr<QCPGrowableMultiDataSource> QCPGraph::ownedSourceForAppend() const
{
    if (mPendingSource)
    {
        if (mOwnedSource && mPendingSource == mOwnedSource)
            return mOwnedSource;
        QCP_CHECK_OR_RETURN(false, "adding or removing data needs a source the graph owns", {});
    }
    if (!mDataSource)
        return {}; // no data yet: addData() starts the owned source
    if (mOwnedSource && mDataSource == mOwnedSource)
        return mOwnedSource;
    QCP_CHECK_OR_RETURN(false, "adding or removing data needs a source the graph owns", {});
}

void QCPGraph::ownedSourceChanged()
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

bool QCPGraph::editOwnedSource(
    const std::function<bool(QCPGrowableMultiDataSource&)>& edit)
{
    if (!mOwnedSource)
        return false;
    const bool pending = mPendingSource != nullptr;
    if (mPipeline.isBusy())
    {
        // A job is reading the rows of the source it was started for. Edit a copy and install
        // it; the old object stays alive in the job.
        auto edited = mOwnedSource->clone();
        if (!edit(*edited))
            return false;
        mOwnedSource = edited;
        if (pending)
            stagePendingSource(std::move(edited));
        else
            installSource(std::move(edited)); // deferred swap on: keep the displayed geometry
        return true;
    }

    if (!edit(*mOwnedSource))
        return false;
    ownedSourceChanged();
    return true;
}

void QCPGraph::setData(const QVector<double>& keys, const QVector<double>& values,
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

void QCPGraph::setData(const std::vector<double>& keys, const std::vector<double>& values,
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

void QCPGraph::addData(const QVector<double>& keys, const QVector<double>& values,
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
    (void)editOwnedSource(
        [&](QCPGrowableMultiDataSource& source)
        { return source.append(keys, std::vector<QVector<double>>{values}, alreadySorted); });
}

void QCPGraph::addData(double key, double value)
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
    (void)editOwnedSource(
        [&](QCPGrowableMultiDataSource& source)
        { return source.append(key, std::span<const double>(values, 1)); });
}

void QCPGraph::addData(const QVector<double>& keys, std::vector<QVector<double>> columns,
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
    (void)editOwnedSource(
        [&](QCPGrowableMultiDataSource& source)
        { return source.append(keys, std::move(columns), alreadySorted); });
}

void QCPGraph::removeDataBefore(double key)
{
    removeOwnedRows([key](QCPGrowableMultiDataSource& source) { return source.removeBefore(key); });
}

void QCPGraph::removeDataAfter(double key)
{
    removeOwnedRows([key](QCPGrowableMultiDataSource& source) { return source.removeAfter(key); });
}

void QCPGraph::removeData(double fromKey, double toKey)
{
    removeOwnedRows([fromKey, toKey](QCPGrowableMultiDataSource& source)
                    { return source.removeBetween(fromKey, toKey); });
}

void QCPGraph::removeOwnedRows(const std::function<bool(QCPGrowableMultiDataSource&)>& remove)
{
    QCP_CHECK_OR_RETURN(acceptsAppend(), "removing data needs a source the graph owns");
    // Null: no data yet (nothing to remove), or a source the graph does not own (reported).
    if (ownedSourceForAppend())
        (void)editOwnedSource(remove);
}

void QCPGraph::addData(double key, std::span<const double> values)
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
    (void)editOwnedSource([&](QCPGrowableMultiDataSource& source)
                              { return source.append(key, values); });
}

void QCPGraph::setDeferredDataSwap(bool enabled)
{
    if (enabled == mDeferredDataSwap)
        return;
    const QCPEmissionScope scope(mParentPlot);
    mDeferredDataSwap = enabled;
    if (!enabled && mPendingSource && !commitPendingData())
    {
        // Not ready yet: apply it now, with the colours set for it, which applySourceNow() drops.
        auto colours = std::move(mPendingColorValues);
        const QPointer<QCPGraph> alive(this);
        applySourceNow(std::move(mPendingSource));
        if (alive && colours)
            setColorValues(std::move(colours));
    }
}

void QCPGraph::applySourceNow(std::shared_ptr<QCPAbstractMultiDataSource> source)
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
    mNeedsResampling = mDataSource && !drawsInDataSpace() && isAboveResampleThreshold(*mDataSource);
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
    ensureL1Transform(mPipeline, mDataSource->size(), mDataSource->columnCount(), mWantOrigin);
    // The pipeline's busyChanged slot may delete this graph.
    const QPointer<QCPGraph> alive(this);
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

void QCPGraph::stagePendingSource(std::shared_ptr<QCPAbstractMultiDataSource> source)
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
    ensureL1Transform(mPipeline, mPendingSource->size(), mPendingSource->columnCount(), mWantOrigin);
    // The pipeline's busyChanged slot may delete this graph.
    const QPointer<QCPGraph> alive(this);
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

void QCPGraph::markPendingReady()
{
    mPendingReady = true;
    if (mParentPlot)
        mParentPlot->requestDataSwap();
}

bool QCPGraph::commitPendingData()
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
    mNeedsResampling = !drawsInDataSpace() && isAboveResampleThreshold(*mDataSource);

    if (mPendingColorValues && static_cast<int>(mPendingColorValues->size()) == mDataSource->size())
    {
        const bool wasColoured = mColor.hasValues();
        mColor.setValues(std::move(mPendingColorValues));
        if (!wasColoured)
        {
            // The pipeline's busyChanged slot may delete this graph.
            const QPointer<QCPGraph> alive(this);
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
        ensureL1Transform(mPipeline, mDataSource->size(), mDataSource->columnCount(),
                               mWantOrigin);
        const QPointer<QCPGraph> alive(this);
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

void QCPGraph::dataChanged()
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
        mNeedsResampling = !drawsInDataSpace() && isAboveResampleThreshold(*mDataSource);
        if (!drawsInDataSpace())
            ensureL1Transform(mPipeline, mDataSource->size(), mDataSource->columnCount(),
                                   mWantOrigin);
    }

    mL1Cache.reset();
    mL2Dirty = false;

    const QPointer<QCPGraph> alive(this);
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

void QCPGraph::onL1Ready(uint64_t generation)
{
    if (drawsInDataSpace())
        return; // a stale job of the cartesian branch: this branch never consults the pipeline
    if (mPendingSource)
    {
        if (generation < mPendingGeneration)
            return; // result of a superseded pending source
        [[maybe_unused]] bool l2Dirty = false; // recomputed from mL1Cache by commitPendingData()
        qcp::extractL1Cache<qcp::algo::GraphResamplerCache>(mPipeline.cache(), mPendingL1, l2Dirty);
        markPendingReady();
        return;
    }
    if (generation < mAcceptedGeneration)
        return; // result of a source that was superseded before this job finished
    qcp::extractL1Cache<qcp::algo::GraphResamplerCache>(mPipeline.cache(), mL1Cache, mL2Dirty);
    linesChanged();
}

void QCPGraph::rebuildL2(const ViewportParams& vp) const
{
    if (!mL1Cache) return;
    mL2Result = qcp::algo::resampleL2Multi(*mL1Cache, vp, *mDataSource);
    if (mL2Result)
        mL2Result->setGapThreshold(mGapThreshold);
    mL2HasOrigin = mL2Result && !mL1Cache->level1.origin.empty();
}

void QCPGraph::onViewportChanged(const QCPRange& newRange, const QCPRange& oldRange)
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

QString QCPGraph::defaultComponentName(int index)
{
    return QStringLiteral("Component %1").arg(index);
}

void QCPGraph::syncComponentCount(const QCPAbstractMultiDataSource* source)
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
            c.name = defaultComponentName(i);
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

void QCPGraph::markDirty()
{
    pruneLostPartnerFills();
    QCPAbstractLine::markDirty();
}

bool QCPGraph::partnerDied(const Fill& fill)
{
    const auto* target = std::get_if<FillToGraph>(&fill.target);
    return target && !target->graph;
}

// M1: a partner that lost the component is not a temporary no-band case: the fill is gone for good,
// or a later, wider source would revive a band over unrelated data. So is a partner that left this
// graph's plot or frame: setComponentFillToGraph() would refuse it now. A graph that is itself off
// any frame (taken, or between two areas) judges nothing until it has one again.
void QCPGraph::pruneLostPartnerFills()
{
    const auto lost = [this](const Fill& fill)
    {
        if (partnerDied(fill))
            return true;
        const auto* target = std::get_if<FillToGraph>(&fill.target);
        if (!target)
            return false;
        const QCPGraph* partner = target->graph.data();
        if (mSystem && (partner->mParentPlot != mParentPlot || !canFillTo(*partner)))
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
void QCPGraph::dropDecorationsBeyond(int columnCount)
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

void QCPGraph::hideErrorBound(int column)
{
    if (column < 0 || column >= mComponents.size() || !mComponents[column].inLegend)
        return; // a bound the user already hid stays the user's
    setComponentInLegend(column, false);
    mComponents[column].hiddenByErrorBars = true;
}

// Bounds of removed bars come back to the legend, unless other bars still use them or the user
// hid them.
void QCPGraph::releaseErrorBounds(const ErrorBars& removed)
{
    for (int bound : {removed.lower, removed.upper})
        if (bound >= 0 && bound < mComponents.size() && mComponents[bound].hiddenByErrorBars
            && !isErrorBound(bound))
            setComponentInLegend(bound, true);
}

// Error bars can be set before the data that creates their bound components.
bool QCPGraph::isErrorBound(int column) const
{
    return std::any_of(mErrorBars.begin(), mErrorBars.end(), [column](const ErrorBars& e)
                       { return e.lower == column || e.upper == column; });
}

bool QCPGraph::isValidColumn(int column) const
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
void QCPGraph::decorationsChanged()
{
    linesChanged();
}

bool QCPGraph::setFill(Fill fill)
{
    const auto* col = std::get_if<int>(&fill.target);
    if (!isValidColumn(fill.owner) || (col && (!isValidColumn(*col) || *col == fill.owner)))
        return false;
    const auto* toGraph = std::get_if<FillToGraph>(&fill.target);
    if (toGraph
        && (!toGraph->graph || !canFillTo(*toGraph->graph)
            || !toGraph->graph->isValidColumn(toGraph->component)))
        return false;
    clearComponentFill(fill.owner);
    mFills.append(std::move(fill));
    decorationsChanged();
    return true;
}

bool QCPGraph::setComponentFill(int owner, double value, const QBrush& brush)
{
    return setFill({owner, value, brush});
}

bool QCPGraph::setComponentFillBetween(int owner, int target, const QBrush& brush)
{
    return setFill({owner, target, brush});
}

bool QCPGraph::canFillTo(const QCPGraph& partner) const
{
    if (&partner == this || !mSystem)
        return false;
    return partner.mSystem == mSystem || fillsAcrossValueAxes(partner);
}

bool QCPGraph::fillsAcrossValueAxes(const QCPGraph& partner) const
{
    // The projections exist only on a cartesian frame; one key axis lives in one area. A graph
    // whose axes lie in two areas keeps them but has no system: it draws nowhere.
    return mSystem && partner.mSystem && partner.mSystem != mSystem && keyAxisRaw() && valueAxisRaw()
        && partner.keyAxisRaw() == keyAxisRaw()
        && partner.valueAxisRaw() && partner.valueAxisRaw() != valueAxisRaw()
        && partner.valueAxisRaw()->orientation() == valueAxisRaw()->orientation();
}

bool QCPGraph::setComponentFillToGraph(int owner, QCPGraph* other, int otherComponent,
                                            const QBrush& brush)
{
    QCP_CHECK_OR_RETURN(other, "a fill to another graph needs a graph", false);
    QCP_CHECK_OR_RETURN(other != this, "a fill to another graph needs another graph", false);
    QCP_CHECK_OR_RETURN(canFillTo(*other),
                        "a fill to another graph needs both graphs on one system, or on one key "
                        "axis of a cartesian frame",
                        false);
    QCP_CHECK_OR_RETURN(other->isValidColumn(otherComponent),
                        "the partner component does not exist", false);
    if (!setFill({owner, FillToGraph { other, otherComponent }, brush}))
        return false;
    // B8: the partner's data change repaints this graph wherever its own layer is, and a deleted
    // partner leaves its QPointer null; mark this graph so the recording drops the band.
    other->addDependent(this);
    connect(other, &QObject::destroyed, this, &QCPGraph::partnerGone,
            Qt::UniqueConnection);
    return true;
}

void QCPGraph::setComponentFillBrush(int owner, const QBrush& brush)
{
    const auto it = std::find_if(mFills.begin(), mFills.end(),
                                 [owner](const Fill& e) { return e.owner == owner; });
    if (it == mFills.end() || it->brush == brush)
        return;
    markDirty();
    it->brush = brush;
    decorationsChanged();
}

void QCPGraph::clearComponentFill(int owner)
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

// A fill whose partner died stays in mFills until the next prune, but it is gone (2.x reset
// channelFillGraph()).
std::optional<QCPGraph::Fill> QCPGraph::componentFill(int owner) const
{
    for (const auto& e : mFills)
        if (e.owner == owner && !partnerDied(e))
            return e;
    return std::nullopt;
}

QBrush QCPGraph::effectiveFillBrush(const Fill& fill) const
{
    if (fill.brush.style() != Qt::NoBrush)
        return fill.brush;
    // 2.x filled the channel to another graph with the graph's brush.
    if (std::holds_alternative<FillToGraph>(fill.target) && mBrush.style() != Qt::NoBrush)
        return mBrush;
    const QColor c = fill.owner < mComponents.size() ? mComponents[fill.owner].pen.color()
                                                     : QColor(Qt::gray);
    return QBrush(QColor(c.red(), c.green(), c.blue(), 64));
}

QBrush QCPGraph::fillRunBrush(const Fill& fill, bool selected) const
{
    const QBrush unselected = effectiveFillBrush(fill);
    if (!selected)
        return unselected;
    if (decoratorIsTheUsers() && mSelectionDecorator->brush().style() != Qt::NoBrush)
        return mSelectionDecorator->brush();
    const QColor base = fill.owner < mComponents.size()
        ? mComponents[fill.owner].selectedPen.color()
        : QColor(Qt::gray);
    return QBrush(QColor(base.red(), base.green(), base.blue(), unselected.color().alpha()));
}

void QCPGraph::partnerGone()
{
    decorationsChanged();
}

void QCPGraph::releasePartnerDependencies(const QVector<Fill>& removed)
{
    QVector<QCPGraph*> candidates;
    for (const Fill& fill : removed)
    {
        if (const auto* target = std::get_if<FillToGraph>(&fill.target))
            if (target->graph && !candidates.contains(target->graph.data()))
                candidates.append(target->graph.data());
    }
    for (QCPGraph* partner : candidates)
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
        QObject::disconnect(partner, &QObject::destroyed, this, &QCPGraph::partnerGone);
    }
}

bool QCPGraph::setComponentErrorBars(int value, int lower, int upper, double capWidthPx,
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

void QCPGraph::clearComponentErrorBars(int value)
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

std::optional<QCPGraph::ErrorBars> QCPGraph::componentErrorBars(int value) const
{
    for (const auto& e : mErrorBars)
        if (e.value == value)
            return e;
    return std::nullopt;
}

void QCPGraph::updateBaseSelection()
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

void QCPGraph::setSystem(QCPCoordinateSystem* system)
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

void QCPGraph::setColorScale(QCPColorScale* colorScale)
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

void QCPGraph::setColorValues(std::shared_ptr<const std::vector<double>> values)
{
    if (values && !values->empty() && colourNeedsAScale())
    {
        QCP_CHECK_OR_RETURN(false,
                            "colour values on a non-rect area need an explicit colour scale");
    }
    QCPAbstractLine::setColorValues(std::move(values));
}

void QCPGraph::setColorValues(std::vector<double> values)
{
    if (!values.empty() && colourNeedsAScale())
    {
        QCP_CHECK_OR_RETURN(false,
                            "colour values on a non-rect area need an explicit colour scale");
    }
    QCPAbstractLine::setColorValues(std::move(values));
}

void QCPGraph::setAdaptiveSampling(bool enabled)
{
    if (mAdaptiveSampling == enabled)
        return;
    markDirty();
    mAdaptiveSampling = enabled;
    linesChanged();
}

void QCPGraph::setGapThreshold(double threshold)
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

void QCPGraph::lineChanged()
{
    linesChanged();
    markDirty();
}

void QCPGraph::componentVisibilityToggled()
{
    // A rebuild while the component was hidden dropped its cached lines: rebuild again.
    linesChanged();
    if (mLayer)
        mLayer->invalidatePaintBuffer();
}

void QCPGraph::applyBrushToComponent(int index, const QBrush& brush)
{
    // 2.x's brush coloured the channel: a fill to another graph keeps its partner, and no brush
    // meant no channel.
    const auto fill = componentFill(index);
    if (brush.style() == Qt::NoBrush)
        clearComponentFill(index);
    else if (fill && std::holds_alternative<FillToGraph>(fill->target))
        setComponentFillBrush(index, brush);
    else
        setComponentFill(index, 0.0, brush);
}

// A staged source is what the values will colour once the swap lands.
std::optional<int> QCPGraph::colorValueCount() const
{
    const QCPAbstractMultiDataSource* target = mPendingSource ? mPendingSource.get() : mDataSource.get();
    return target ? std::optional<int>(target->size()) : std::nullopt;
}

bool QCPGraph::hasPendingDataSource() const { return mPendingSource != nullptr; }

void QCPGraph::colorValuesStarted()
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

QVector<int> QCPGraph::lineIndices(LineStyle style, const QVector<int>& dataIdx)
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
void QCPGraph::requestOrigin()
{
    if (!mWantOrigin->exchange(true) && mPipeline.hasTransform())
        mPipeline.onDataChanged();
}

// Same-size-keeps/other-size-clears colour rule, applied whenever a new mDataSource is
// installed without new colour values to go with it (applySourceNow, commitPendingData
// when nothing was stashed for the committed source, and dataChanged on an in-place mutation).
void QCPGraph::applyColorSizeRule(int newSize)
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

double QCPGraph::componentValueAt(int column, int index) const
{
    // column comes from componentCount(), which may already reflect a wider
    // pending source than the one currently displayed.
    return (mDataSource && column < mDataSource->columnCount())
        ? mDataSource->valueAt(column, index) : 0.0;
}

QCPDataSelection QCPGraph::componentSelection(int index) const
{
    return (index >= 0 && index < mComponents.size()) ? mComponents[index].selection : QCPDataSelection();
}

void QCPGraph::setComponentSelection(int index, QCPDataSelection sel)
{
    if (index < 0 || index >= mComponents.size())
        return;
    sel.simplify(); // the draw searches the ranges: sorted and disjoint
    if (mComponents[index].selection == sel)
        return;
    markDirty();
    mComponents[index].selection = sel;
    updateBaseSelection();
}

// --- QCPPlottableInterface1D ---

int QCPGraph::dataCount() const
{
    return mDataSource ? mDataSource->size() : 0;
}

// 2.x's 1D accessors answered 0 for a row outside the data: a selection is not clamped when rows
// go, and the decorators read its ends.
bool QCPGraph::isDataRow(int index) const
{
    return mDataSource && mDataSource->columnCount() > 0 && index >= 0
        && index < mDataSource->size();
}

double QCPGraph::dataMainKey(int index) const
{
    return isDataRow(index) ? mDataSource->keyAt(index) : 0.0;
}

double QCPGraph::dataSortKey(int index) const
{
    return dataMainKey(index);
}

double QCPGraph::dataMainValue(int index) const
{
    return isDataRow(index) ? mDataSource->valueAt(0, index) : 0.0;
}

QCPRange QCPGraph::dataValueRange(int index) const
{
    if (!isDataRow(index))
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

QPointF QCPGraph::dataPixelPosition(int index) const
{
    if (!isDataRow(index))
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
    if (!keyAxisRaw() || !valueAxisRaw())
        return {};
    return coordsToPixels(key, value);
}

int QCPGraph::findBegin(double sortKey, bool expandedRange) const
{
    return mDataSource ? mDataSource->findBegin(sortKey, expandedRange) : 0;
}

int QCPGraph::findEnd(double sortKey, bool expandedRange) const
{
    return mDataSource ? mDataSource->findEnd(sortKey, expandedRange) : 0;
}

// --- Range queries ---

QCPRange QCPGraph::getKeyRange(bool& foundRange, QCP::SignDomain inSignDomain) const
{
    if (!mDataSource || mDataSource->empty()) {
        foundRange = false;
        return {};
    }
    return mDataSource->keyRange(foundRange, inSignDomain);
}

QCPRange QCPGraph::getValueRange(bool& foundRange, QCP::SignDomain inSignDomain,
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

double QCPGraph::selectTest(const QPointF& pos, bool onlySelectable, QVariant* details) const
{
    if ((onlySelectable && mSelectable == QCP::stNone) || !mDataSource || mDataSource->empty())
        return -1;
    const bool beyond = mParentPlot
        && mParentPlot->interactions().testFlag(QCP::iSelectPlottablesBeyondAxisRect);
    if (drawsInDataSpace())
    {
        if (!mSystem)
            return -1;
        if (!mSystem->contains(pos) && !beyond)
            return -1;
        return nearestMappedPoint(pos, details).value_or(-1);
    }
    if (!keyAxisRaw() || !valueAxisRaw())
        return -1;

    auto* axisRect = axisRectOf(keyAxisRaw());
    if (!axisRect || (!axisRect->rect().contains(pos.toPoint()) && !beyond))
        return -1;

    return nearestDataPoint(pos, details).value_or(-1);
}

QCPDataSelection QCPGraph::selectTestRect(const QRectF& rect, bool onlySelectable) const
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

    if (!keyAxisRaw() || !valueAxisRaw())
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

// The rows each component's hit holds: a rect's per-component rows, or the clicked row.
QVector<QCPDataSelection> QCPGraph::componentHits(const QVariant& details) const
{
    QVector<QCPDataSelection> hits(mComponents.size());
    if (details.canConvert<QCPDataSelection>())
    {
        // A rect selection: selectTestRect kept each component's rows; a caller without them hands
        // the union, which every component shares.
        const auto rects = std::exchange(mLastRectSelections, {});
        for (int c = 0; c < hits.size(); ++c)
            hits[c] = rects.isEmpty() ? details.value<QCPDataSelection>()
                                      : c < rects.size() ? rects[c] : QCPDataSelection();
        return hits;
    }
    const QVariantMap map = details.toMap();
    const int component = map.value(QStringLiteral("componentIndex"), -1).toInt();
    const int row = map.value(QStringLiteral("dataIndex"), -1).toInt();
    if (component >= 0 && component < hits.size() && row >= 0)
        hits[component] = QCPDataSelection(QCPDataRange(row, row + 1));
    return hits;
}

// 2.x's rules (QCPAbstractPlottable::selectEvent): the type decides. A whole or single-row
// selection selects the graph's rows on every component, through the base. Ranges stay per
// component (a click picks one column of a multi-column graph), with the same additive toggle.
void QCPGraph::selectEvent(QMouseEvent* event, bool additive, const QVariant& details,
                           bool* selectionStateChanged)
{
    const QVector<QCPDataSelection> hits = componentHits(details);
    if (mSelectable == QCP::stWhole || mSelectable == QCP::stSingleData)
    {
        QCPDataSelection hit;
        for (const QCPDataSelection& rows : hits)
            hit += rows;
        QCPAbstractPlottable::selectEvent(event, additive, QVariant::fromValue(hit),
                                          selectionStateChanged);
        return;
    }
    if (mSelectable == QCP::stNone)
        return;
    selectComponentRows(hits, additive, selectionStateChanged);
}

void QCPGraph::selectComponentRows(const QVector<QCPDataSelection>& hits, bool additive,
                                   bool* selectionStateChanged)
{
    bool changed = false;
    for (int c = 0; c < mComponents.size(); ++c)
    {
        QCPDataSelection& current = mComponents[c].selection;
        const QCPDataSelection& hit = hits[c];
        QCPDataSelection next = !additive ? hit
            : hit.isEmpty()               ? current
            : current.contains(hit)       ? current - hit
                                          : current + hit;
        next.enforceType(mSelectable);
        if (next != current)
        {
            current = next;
            changed = true;
        }
    }
    const QPointer<QCPGraph> alive(this);
    if (changed)
        updateBaseSelection();
    if (selectionStateChanged)
        *selectionStateChanged = !alive || changed;
}

bool QCPGraph::assignSelection(const QCPDataSelection& selection)
{
    bool changed = mSelection != selection;
    for (auto& c : mComponents)
    {
        if (c.selection != selection)
        {
            c.selection = selection;
            changed = true;
        }
    }
    mSelection = selection;
    return changed;
}

void QCPGraph::deselectEvent(bool* selectionStateChanged)
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

bool QCPGraph::canProduceContent() const
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
    if (!keyAxisRaw() || !valueAxisRaw())
        return false;
    // Pipeline active but L1 not ready — draw() will bail out
    if (mNeedsResampling && !mL1Cache && !mL2Result)
        return false;
    return true;
}

std::optional<double> QCPGraph::hitTest(QCP::Pixels at, QVariant* details) const
{
    if (!mDataSource || mDataSource->empty())
        return std::nullopt;
    if (!drawsInDataSpace() && (!keyAxisRaw() || !valueAxisRaw()))
        return std::nullopt;
    return nearestDataPoint(QPointF(at.x, at.y), details);
}

// B3: on the data-space branch a click is a 2D nearest search over the mapped rows, with no key
// window: the click's x is not the key under a shear.
std::optional<double> QCPGraph::nearestMappedPoint(const QPointF& pos, QVariant* details) const
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
                const bool nearerFirst = QCPVector2D(pixel - pos).lengthSquared()
                                         <= QCPVector2D(next - pos).lengthSquared();
                const int end = nearerFirst ? i : i + 1;
                minDistSqr = lineDistance;
                minIndex = end;
                minComponent = c;
                minKey = mDataSource->keyAt(end);
                minValue = mDataSource->valueAt(c, end);
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
std::optional<double> QCPGraph::nearestDataPoint(const QPointF& pos, QVariant* details) const
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
                const bool nearerLo = QCPVector2D(pLo - pos).lengthSquared()
                                      <= QCPVector2D(pHi - pos).lengthSquared();
                const int end = nearerLo ? lo : hi;
                minDistSqr = lineDistSqr;
                minDistComponent = c;
                minDistIndex = end;
                minKey = ds->keyAt(end);
                minValue = ds->valueAt(c, end);
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

void QCPGraph::draw(QCPDrawContext& context) const
{
    PROFILE_HERE_N("QCPGraph::draw");
    if (drawsInDataSpace())
    {
        drawInDataSpace(context);
        return;
    }
    if (!keyAxisRaw() || !valueAxisRaw() || !mDataSource || mDataSource->empty())
        return;
    if (keyAxisRaw()->range().size() <= 0)
        return;

    // An export cannot wait for the scheduler: it resamples what it draws, now.
    if (!mL1Cache && mNeedsResampling)
    {
        if (!context.exporting())
            return;
        mPipeline.runSynchronously(ViewportParams::fromAxes(keyAxisRaw(), valueAxisRaw()));
        bool level2Dirty = false;
        qcp::extractL1Cache<qcp::algo::GraphResamplerCache>(mPipeline.cache(), mL1Cache,
                                                                 level2Dirty);
    }

    // The L2 bins are this frame's fresh lines: rebuilt here, and the revision does not move,
    // since the recording being made right now is made from them.
    if (mL1Cache && mL2Dirty)
    {
        rebuildL2(ViewportParams::fromAxes(keyAxisRaw(), valueAxisRaw()));
        mL2Dirty = false;
    }

    // Data source priority: L2 (viewport-optimized) > raw. A recording is only made from what is
    // there: with no L1 yet and resampling needed, the frame it would record does not exist.
    QCPAbstractMultiDataSource* ds = nullptr;
    if (mL2Result)
        ds = mL2Result.get();
    else if (!mNeedsResampling || mL1Cache || context.exporting()
             || keyAxisRaw()->scaleType() == QCPAxis::stLogarithmic)
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

    const bool logKeyAxis = keyAxisRaw()->scaleType() == QCPAxis::stLogarithmic;
    // The thinning buckets are equal spans of the key, mapped to equal pixel spans: on a log axis
    // that span is a decade, so pixelsPerKey counts pixels per decade there.
    const double axisPixels = window.pixelsPerKey > 0.0
        ? window.pixelsPerKey * keyAxisRaw()->range().size()
        : (keyAxisRaw()->coordToPixel(keyAxisRaw()->range().upper)
           - keyAxisRaw()->coordToPixel(keyAxisRaw()->range().lower));
    double pixelsPerKey = window.pixelsPerKey > 0.0 ? window.pixelsPerKey
                                                    : qcpKeyPixelsPerUnit(keyAxisRaw());
    if (logKeyAxis)
    {
        const QCPRange keyRange = keyAxisRaw()->range();
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

    // In data space the impulse baseline is a value: zero, or on a log value axis the range bound
    // nearer zero, as 2.x closed it.
    const bool logValueAxis = valueAxisRaw()->scaleType() == QCPAxis::stLogarithmic;
    const double impulseBaseline = dataSpaceBaseline();
    const bool impulses = std::any_of(mComponents.begin(), mComponents.end(), [](const QCPGraphComponent& c)
                                      { return c.visible && c.lineStyle == lsImpulse; });
    if (logValueAxis && impulses)
        (void)context.transform(); // the baseline follows the axis' range bound

    const int columns = std::min(componentCount(), ds->columnCount());
    for (int c = 0; c < columns; ++c)
    {
        const QCPGraphComponent& comp = component(c);
        if (!comp.visible)
            continue;
        if (comp.lineStyle == lsNone && comp.scatterStyle.isNone())
            continue;

        // The rows a vertex comes from are only asked for when a colour or a selection needs them.
        const bool wantsRows = coloured || !comp.selection.isEmpty();
        std::vector<int> sourceIndices;
        std::vector<QPointF> line
            = ds->linesInData(c, begin, end, pixelsPerKey, wantsRows ? &sourceIndices : nullptr,
                              logKeyAxis);
        if (line.empty())
            continue;
        recordComponent(context, c, std::move(line), sourceIndices, *ds, coloured, impulseBaseline,
                        logKeyAxis);
    }
    recordSelectionDecoration(context);
}

// B2/P-4: the data-space branch on a system that is not cartesian. It records positions and the
// fill, error-bar and colour commands the replay maps through the frame's transform; it reads the
// displayed source directly, with no thinning (pixelsPerKey = 0) and no L1/L2.
void QCPGraph::drawInDataSpace(QCPDrawContext& context) const
{
    PROFILE_HERE_N("QCPGraph::drawInDataSpace");
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
    const bool impulses = std::any_of(mComponents.begin(), mComponents.end(), [](const QCPGraphComponent& c)
                                      { return c.visible && c.lineStyle == lsImpulse; });
    if (logValueAxis && impulses)
        (void)context.transform(); // the baseline follows the value axis' range bound

    // Fills and error bars are data-space commands too; both record before the lines so the replay
    // keeps fills under bars under lines.
    recordFills(context, ds, begin, end);
    recordErrorBars(context, ds, /*rawRows=*/true, begin, end);

    const int columns = std::min(componentCount(), ds.columnCount());
    const bool coloured = mColor.hasValues();
    for (int c = 0; c < columns; ++c)
    {
        const QCPGraphComponent& comp = component(c);
        if (!comp.visible)
            continue;
        if (comp.lineStyle == lsNone && comp.scatterStyle.isNone())
            continue;

        // simplify: no decimation on this branch; every row is recorded and mapped per frame.
        // Upgrade: the J-6 box query and data-space decimation by keyWindow().
        const bool wantsRows = coloured || !comp.selection.isEmpty();
        std::vector<int> sourceIndices;
        std::vector<QPointF> line
            = ds.linesInData(c, begin, end, /*pixelsPerKey=*/0.0,
                             wantsRows ? &sourceIndices : nullptr, /*logKeys=*/false,
                             /*cacheGaps=*/false);
        if (line.empty())
            continue;
        recordComponent(context, c, std::move(line), sourceIndices, ds, coloured, baseline,
                        logKeyAxis);
    }
}

namespace
{
struct SelectionRun
{
    std::size_t first = 0, last = 0;
    bool selected = false;
};

// Runs of equally selected vertices, the unselected ones first so the selected pen draws over the
// vertex they share. An unselected run reaches one vertex into each selected neighbour, so the two
// pens meet, as 2.x widened its unselected segments.
std::vector<SelectionRun> selectionRuns(const std::vector<char>& selected)
{
    const std::size_t count = selected.size();
    std::vector<SelectionRun> runs;
    for (std::size_t first = 0; first < count;)
    {
        std::size_t last = first + 1;
        while (last < count && selected[last] == selected[first])
            ++last;
        if (selected[first])
            runs.push_back({ first, last, true });
        else
            runs.push_back({ first > 0 ? first - 1 : 0, std::min(count, last + 1), false });
        first = last;
    }
    std::stable_partition(runs.begin(), runs.end(), [](const SelectionRun& run) { return !run.selected; });
    return runs;
}
} // namespace

std::vector<char> QCPGraph::selectedVertices(int column, const QCPAbstractMultiDataSource& ds,
                                                  const std::vector<int>& rows) const
{
    std::vector<char> selected(rows.size(), 0);
    char previous = 0;
    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        if (rows[i] >= 0)
            previous = rowIsSelected(column, ds, rows[i]) ? 1 : 0;
        selected[i] = previous;
    }
    return selected;
}

void QCPGraph::recordComponent(QCPDrawContext& context, int column, std::vector<QPointF> line,
                                    const std::vector<int>& rows,
                                    const QCPAbstractMultiDataSource& ds, bool coloured,
                                    double impulseBaseline, bool logKeyAxis) const
{
    const QCPGraphComponent& comp = component(column);
    // Defensive: indices that do not match the points are never used.
    const bool aligned = rows.size() == line.size();
    const bool indexed = coloured && aligned;
    QVector<int> dataIdx;
    if (indexed)
        dataIdx = QVector<int>(rows.begin(), rows.end());
    const bool hasMarkers = !comp.scatterStyle.isNone();

    // Without the rows a selection cannot be placed: the whole line takes it, as a whole selection
    // does.
    if (comp.selection.isEmpty() || !aligned || mSelectable == QCP::stWhole)
    {
        const bool selected = !comp.selection.isEmpty();
        // A plain line without markers is the recorded line: it is handed over, not copied.
        std::vector<QPointF> markerLine = hasMarkers ? line : std::vector<QPointF>();
        recordLine(context, comp.lineStyle, std::move(line), dataIdx,
                   selected ? selectedLinePen(comp) : comp.pen, impulseBaseline, logKeyAxis);
        if (hasMarkers)
            recordMarkers(context, comp, markerLine, rows, indexed, {}, selected);
        return;
    }

    const std::vector<char> selected = selectedVertices(column, ds, rows);
    for (const SelectionRun& run : selectionRuns(selected))
    {
        const auto first = std::ptrdiff_t(run.first);
        const auto last = std::ptrdiff_t(run.last);
        recordLine(context, comp.lineStyle,
                   std::vector<QPointF>(line.begin() + first, line.begin() + last),
                   indexed ? dataIdx.mid(first, last - first) : QVector<int>(),
                   run.selected ? selectedLinePen(comp) : comp.pen, impulseBaseline, logKeyAxis);
    }
    if (hasMarkers)
        recordMarkers(context, comp, line, rows, indexed, selected, false);
}

void QCPGraph::recordLine(QCPDrawContext& context, LineStyle style, std::vector<QPointF> line,
                               const QVector<int>& dataIdx, QPen pen, double impulseBaseline,
                               bool logKeyAxis) const
{
    const bool indexed = !dataIdx.isEmpty();
    if (style == lsNone)
        return;
    if (style == lsLine)
    {
        if (line.size() >= 2)
            context.polyline(std::make_shared<const std::vector<QPointF>>(std::move(line)),
                             indexed ? segmentColours(mColor, dataIdx) : nullptr, pen);
        return;
    }
    const QVector<QPointF> points(line.begin(), line.end());
    QVector<QPointF> vertices;
    switch (style)
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
            pen.setCapStyle(Qt::FlatCap);
            break;
        default:
            vertices = points;
            break;
    }
    if (vertices.size() < 2)
        return;
    std::shared_ptr<const std::vector<QRgb>> colours;
    if (indexed)
    {
        QVector<int> vertexIdx = lineIndices(style, dataIdx);
        if (style == lsImpulse)
            vertexIdx = separatedPairIndices(vertexIdx);
        colours = segmentColours(mColor, vertexIdx);
    }
    context.polyline(sharedPoints(vertices), colours, pen);
}

// Markers sit at the data positions, not at the corners a step or an impulse adds.
void QCPGraph::recordMarkers(QCPDrawContext& context, const QCPGraphComponent& comp,
                                  const std::vector<QPointF>& line, const std::vector<int>& rows,
                                  bool indexed, const std::vector<char>& selected,
                                  bool allSelected) const
{
    struct Markers
    {
        std::shared_ptr<std::vector<QPointF>> points = std::make_shared<std::vector<QPointF>>();
        std::shared_ptr<std::vector<QRgb>> colours = std::make_shared<std::vector<QRgb>>();
    };
    Markers normal, chosen;
    const std::size_t skip = std::size_t(mScatterSkip) + 1;
    for (std::size_t i = 0; i < line.size(); i += skip)
    {
        if (!qIsFinite(line[i].x()) || !qIsFinite(line[i].y()))
            continue;
        Markers& markers = (selected.empty() ? allSelected : selected[i] != 0) ? chosen : normal;
        markers.points->push_back(line[i]);
        if (indexed)
            markers.colours->push_back(pointColour(mColor, rows[i]));
    }
    if (!normal.points->empty())
        context.markers(normal.points, indexed ? normal.colours : nullptr,
                        markerStyle(comp.scatterStyle, comp.pen));
    if (!chosen.points->empty())
        context.markers(chosen.points, indexed ? chosen.colours : nullptr, selectedMarkerStyle(comp));
}

bool QCPGraph::decoratorIsTheUsers() const
{
    if (!mSelectionDecorator)
        return false;
    if (typeid(*mSelectionDecorator) != typeid(QCPSelectionDecorator))
        return true;
    // simplify: a decorator styled back to exactly the default look reads as untouched, and the
    // components' selected pens draw. Upgrade: a "styled" flag set by the decorator's setters.
    static const QCPSelectionDecorator pristine;
    return mSelectionDecorator->pen() != pristine.pen()
        || mSelectionDecorator->brush() != pristine.brush()
        || mSelectionDecorator->usedScatterProperties() != pristine.usedScatterProperties();
}

QPen QCPGraph::selectedLinePen(const QCPGraphComponent& comp) const
{
    return decoratorIsTheUsers() ? mSelectionDecorator->pen() : comp.selectedPen;
}

QCPScatterStyle QCPGraph::selectedMarkerStyle(const QCPGraphComponent& comp) const
{
    return decoratorIsTheUsers() ? mSelectionDecorator->getFinalScatterStyle(comp.scatterStyle)
                                 : markerStyle(comp.scatterStyle, comp.selectedPen);
}

void QCPGraph::recordSelectionDecoration(QCPDrawContext& context) const
{
    // Only a decorator subclass draws a decoration of its own: the base class's is empty, and
    // recording a painter callback for it would make every selection change a view change.
    if (!mSelectionDecorator || selection().isEmpty()
        || typeid(*mSelectionDecorator) == typeid(QCPSelectionDecorator))
        return;
    // The built-in bracket's shapes are recorded after the lines, so the replay draws them over
    // what the GPU draws. A custom style, or a subclass that may redraw its brackets, stays on the
    // painter callback.
    const auto* bracket = dynamic_cast<const QCPSelectionDecoratorBracket*>(mSelectionDecorator);
    const bool builtInBracket = bracket && typeid(*bracket) == typeid(QCPSelectionDecoratorBracket);
    if (builtInBracket && bracket->recordDecoration(context, selection()))
        return;
    context.painter(
        [this](QCPPainter& painter, const QCPTransform&)
        {
            if (mSelectionDecorator)
                mSelectionDecorator->drawDecoration(&painter, selection());
        });
}

bool QCPGraph::fillDrawable(const Fill& fill, int columnCount) const
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
        const QCPGraph* partner = target->graph.data();
        if (!partner || !canFillTo(*partner))
            return false;
        return partner->mDataSource
            && target->component >= 0
            && target->component < partner->mDataSource->columnCount();
    }
    return true;
}

qcp::fill::Step QCPGraph::ownerStep(const QCPGraphComponent& owner)
{
    switch (owner.lineStyle) {
        case lsStepLeft:   return qcp::fill::Step::Left;
        case lsStepRight:  return qcp::fill::Step::Right;
        case lsStepCenter: return qcp::fill::Step::Center;
        default:           return qcp::fill::Step::None;
    }
}

std::vector<qcp::fill::StripPoint> QCPGraph::fillRows(const Fill& fill,
                                                           const QCPAbstractMultiDataSource& ds,
                                                           int begin, int end) const
{
    const auto gaps = ds.gapsBefore(begin, end, /*cacheGaps=*/!drawsInDataSpace());
    const auto* targetColumn = std::get_if<int>(&fill.target);
    const auto edge = [this](double value)
    {
        return drawsInDataSpace() ? dataSpaceFillValue(value) : fillValue(*valueAxisRaw(), value);
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

// A log value axis has no zero: a value it cannot place goes to the range bound on zero's side
// (upper on a negative range, lower on a positive one), as 2.x's getFillBasePoint closed it. The
// recording then follows that bound, so it is transform-dependent.
double QCPGraph::fillValue(const QCPAxis& valueAxis, double value)
{
    if (valueAxis.scaleType() == QCPAxis::stLinear || std::isnan(value))
        return value;
    const QCPRange range = valueAxis.range();
    if (range.upper < 0)
        return value < 0 ? value : range.upper;
    return value > 0 ? value : range.lower;
}

// Binned when the rows outnumber 4 per pixel, the lines' own L2 density (kLevel2PixelMultiplier).
std::vector<qcp::fill::StripPoint> QCPGraph::shapeFillRows(
    const Fill& fill, std::vector<qcp::fill::StripPoint> rows) const
{
    // The binning is a pixel decision, and the data-space branch has no key pixel: it keeps every
    // row (the simplify note in drawInDataSpace names the ceiling).
    if (!drawsInDataSpace())
        rows = binnedWhenDense(std::move(rows));
    // The system's key axis is the one a straight-segment frame draws in log space.
    const QCPAxis* keyAxis = drawsInDataSpace() ? qobject_cast<const QCPAxis*>(keyAxisBase())
                                                : keyAxisRaw();
    const bool logKeyAxis = keyAxis && keyAxis->scaleType() == QCPAxis::stLogarithmic;
    return steppedOnScreen(std::move(rows), ownerStep(mComponents[fill.owner]), logKeyAxis);
}

std::vector<qcp::fill::StripPoint> QCPGraph::fillToGraphRows(
    const Fill& fill, const QCPAbstractMultiDataSource& ds, int begin, int end) const
{
    const auto* target = std::get_if<FillToGraph>(&fill.target);
    QCPGraph* partner = target ? target->graph.data() : nullptr;
    if (!partner || !canFillTo(*partner))
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
    // Across value axes the partner's value goes through its pixel onto this graph's value axis,
    // before the crop and the interpolation: the band is drawn in this graph's frame.
    const bool across = fillsAcrossValueAxes(*partner);
    const auto onThisAxis = [&](double value)
    { return across ? valueAxisRaw()->pixelToCoord(partner->valueAxisRaw()->coordToPixel(value)) : value; };
    std::vector<qcp::fill::SeriesRow> partnerRows;
    partnerRows.reserve(std::size_t(std::max(0, partnerEnd - partnerBegin)));
    for (int i = partnerBegin; i < partnerEnd; ++i)
        partnerRows.push_back({ partnerSource->keyAt(i),
                                onThisAxis(partnerSource->valueAt(target->component, i)),
                                partnerGaps[i - partnerBegin] != 0, i });

    std::vector<qcp::fill::StripPoint> rows
        = qcp::fill::fillToGraphRows(ownerRows, partnerRows);
    const bool dataSpace = drawsInDataSpace();
    const auto edge = [this, dataSpace](double value)
    { return dataSpace ? dataSpaceFillValue(value) : fillValue(*valueAxisRaw(), value); };
    for (qcp::fill::StripPoint& row : rows)
    {
        row.a = edge(row.a);
        row.b = edge(row.b);
    }
    return rows;
}

std::vector<qcp::fill::StripPoint> QCPGraph::binnedWhenDense(
    std::vector<qcp::fill::StripPoint> rows) const
{
    const QRect rect = axisRectOf(keyAxisRaw())->rect();
    const int pixelLength = keyAxisRaw()->orientation() == Qt::Vertical ? rect.height() : rect.width();
    if (rows.size() <= 4 * std::size_t(std::max(0, pixelLength)))
        return rows;
    const double quarterPixel = 4.0 * std::max(1, pixelLength);
    if (keyAxisRaw()->scaleType() == QCPAxis::stLogarithmic)
    {
        const QCPRange range = keyAxisRaw()->range();
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
    const double width = std::abs(keyAxisRaw()->range().size()) / quarterPixel;
    if (!(width > 0.0))
        return rows;
    return qcp::fill::binToSpans(rows, width);
}

// The selection is simplified (sorted, disjoint): a binary search finds the one range that can
// hold the row, so a selection of many ranges costs log(ranges) per vertex.
bool QCPGraph::rowIsSelected(int column, const QCPAbstractMultiDataSource& ds, int row) const
{
    if (column < 0 || column >= mComponents.size())
        return false;
    const QCPDataSelection& selection = mComponents[column].selection;
    if (selection.isEmpty())
        return false;
    if (mSelectable == QCP::stWhole) // 2.x: any selection draws the whole plottable selected
        return true;
    const auto firstRangeReaching = [&](auto reaches)
    {
        int lo = 0, hi = selection.dataRangeCount();
        while (lo < hi)
        {
            const int mid = lo + (hi - lo) / 2;
            if (reaches(selection.dataRange(mid)))
                hi = mid;
            else
                lo = mid + 1;
        }
        return lo;
    };
    if (mDataSource && &ds == mDataSource.get())
    {
        const int i = firstRangeReaching([row](const QCPDataRange& r) { return r.end() > row; });
        return i < selection.dataRangeCount() && selection.dataRange(i).begin() <= row;
    }

    // A resampled row is not a source row: a selection range becomes the key interval its first
    // and last source rows span.
    if (!mDataSource || row < 0 || row >= ds.size() || mDataSource->empty())
        return false;
    const int size = mDataSource->size();
    const double key = ds.keyAt(row);
    const auto keyAtRow = [&](int r) { return mDataSource->keyAt(std::clamp(r, 0, size - 1)); };
    const int i = firstRangeReaching(
        [&](const QCPDataRange& r) { return r.end() > 0 && keyAtRow(r.end() - 1) >= key; });
    for (int k = i; k < selection.dataRangeCount(); ++k)
    {
        const QCPDataRange range = selection.dataRange(k);
        const int begin = std::clamp(range.begin(), 0, size);
        const int end = std::clamp(range.end(), begin, size);
        if (begin >= end)
            continue;
        return keyAtRow(begin) <= key && key <= keyAtRow(end - 1);
    }
    return false;
}

// One fillBetween command per selected/unselected run of this graph's component selection. The
// split is decided on the raw source rows, before binning or step expansion changes the indices. A
// gap pinches both series together, so neither the polygon nor the ribbon spans it.
void QCPGraph::recordFills(QCPDrawContext& context, const QCPAbstractMultiDataSource& ds,
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
            return rowIsSelected(fill.owner, ds, row);
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

bool QCPGraph::errorBarsDrawable(const ErrorBars& spec, int columnCount) const
{
    const int last = std::min<int>(columnCount, mComponents.size());
    return std::max({spec.value, spec.lower, spec.upper}) < last && mComponents[spec.value].visible;
}

QPen QCPGraph::errorBarPen(const ErrorBars& spec) const
{
    return spec.pen.value_or(mComponents[spec.value].pen);
}

std::vector<qcp::errorbars::Bar> QCPGraph::errorBarRows(const ErrorBars& spec,
                                                             const QCPAbstractMultiDataSource& ds,
                                                             int begin, int end) const
{
    std::vector<qcp::errorbars::Bar> bars;
    bars.reserve(std::max(0, end - begin));
    const bool dataSpace = drawsInDataSpace();
    const auto edge = [this, dataSpace](double value)
    { return dataSpace ? value : fillValue(*valueAxisRaw(), value); };
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
bool QCPGraph::errorBarsFitInView(std::span<const qcp::errorbars::Bar> bars,
                                       double minSpacingPx) const
{
    const QRect rect = axisRectOf(keyAxisRaw())->rect();
    const bool vertical = keyAxisRaw()->orientation() == Qt::Vertical;
    const double first = vertical ? rect.top() : rect.left();
    const double last = vertical ? rect.bottom() : rect.right();
    std::vector<qcp::errorbars::Bar> shown;
    for (qcp::errorbars::Bar bar : bars)
    {
        const double pixel = keyAxisRaw()->coordToPixel(bar.key);
        if (std::isfinite(pixel) && pixel >= first && pixel <= last && qcp::errorbars::resolve(bar))
        {
            bar.key = pixel;
            shown.push_back(bar);
        }
    }
    return qcp::errorbars::barsFit(shown, minSpacingPx);
}

std::vector<qcp::fill::StripPoint> QCPGraph::errorBandRows(
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
void QCPGraph::recordErrorBars(QCPDrawContext& context, const QCPAbstractMultiDataSource& ds,
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
            const QCPGraphComponent& comp = mComponents[spec.value];
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
            const double pixel = keyAxisRaw()->coordToPixel(bar.key);
            const double left = keyAxisRaw()->pixelToCoord(pixel - halfCap);
            const double right = keyAxisRaw()->pixelToCoord(pixel + halfCap);
            centres.emplace_back(bar.key, 0.5 * (bar.lower + bar.upper));
            spans.emplace_back(0.5 * std::abs(right - left), 0.5 * (bar.upper - bar.lower));
        }
        if (centres.empty())
            continue;
        const QCPGraphComponent& comp = mComponents[spec.value];
        const QPen pen = comp.selection.isEmpty() ? errorBarPen(spec) : comp.selectedPen;
        context.errorBars(std::make_shared<const std::vector<QPointF>>(std::move(centres)),
                          std::make_shared<const std::vector<QPointF>>(std::move(spans)), pen);
    }
}

// The icon is a recording of its own: one unit-wide segment per shown component, fitted into the
// rect by the replay, so it never depends on the plot's data or axis ranges.
void QCPGraph::drawLegendIcon(QCPDrawContext& icon) const
{
    if (mComponents.isEmpty())
    {
        icon.rect(QCP::Data { 0.0, -0.5 }, QCP::Data { 1.0, 0.5 }, QPen(Qt::NoPen),
                  QBrush(Qt::NoBrush));
        recordLegendSegment(icon, firstComponentStyle(), emptyGraphFillBrush(), 0.0);
        return;
    }
    const QVector<int> shown = legendComponents();
    if (shown.isEmpty())
        return;

    const double segments = shown.size();
    // The visible extent: it gives the fitted transform the icon's box.
    icon.rect(QCP::Data { 0.0, -0.5 }, QCP::Data { segments, 0.5 }, QPen(Qt::NoPen),
              QBrush(Qt::NoBrush));
    for (int k = 0; k < shown.size(); ++k)
    {
        const QCPGraphComponent& comp = mComponents[shown[k]];
        if (!comp.visible)
            continue;
        const auto fill = componentFill(shown[k]);
        recordLegendSegment(icon, comp,
                            fill ? std::optional<QBrush>(effectiveFillBrush(*fill)) : std::nullopt,
                            k);
        if (const auto bars = componentErrorBars(shown[k]))
        {
            // 5 px half stem, 3 px half cap on a 20 px segment, in segment fractions.
            icon.errorBars(sharedPoints(QVector<QPointF> { QPointF(k + 0.5, 0.0) }),
                           sharedPoints(QVector<QPointF> { QPointF(0.15, 0.25) }),
                           errorBarPen(*bars));
        }
    }
}

void QCPGraph::recordLegendSegment(QCPDrawContext& icon, const QCPGraphComponent& comp,
                                        const std::optional<QBrush>& fill, double x0) const
{
    const double x1 = x0 + 1;
    const double centre = x0 + 0.5;
    if (fill)
        icon.rect(QCP::Data { x0, -0.3 }, QCP::Data { x1, 0.3 }, QPen(Qt::NoPen), *fill);
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
}

// A graph without data has no component: its legend shows the one its first column will be, as
// 2.x drew an empty graph's pen.
QCPGraphComponent QCPGraph::firstComponentStyle() const
{
    QCPGraphComponent style;
    style.pen = mExplicitPen.value_or(QPen(qcp::defaultPlottableColor(0), 0));
    style.lineStyle = mLineStyle;
    if (mExplicitScatterStyle)
        style.scatterStyle = *mExplicitScatterStyle;
    return style;
}

std::optional<QBrush> QCPGraph::emptyGraphFillBrush() const
{
    if (mBrush.style() == Qt::NoBrush)
        return std::nullopt;
    return mBrush;
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

void QCPGraph::drawComponentLegendIcon(QCPPainter* painter, int component, const QLineF& line) const
{
    const auto fill = componentFill(component);
    drawLegendGlyph(painter, mComponents[component],
                    fill ? std::optional<QBrush>(effectiveFillBrush(*fill)) : std::nullopt, line);
    if (const auto bars = componentErrorBars(component))
        drawErrorBarGlyph(painter, line.center(), errorBarPen(*bars));
}

void QCPGraph::drawEmptyLegendIcon(QCPPainter* painter, const QLineF& line) const
{
    drawLegendGlyph(painter, firstComponentStyle(), emptyGraphFillBrush(), line);
}

void QCPGraph::drawLegendGlyph(QCPPainter* painter, const QCPGraphComponent& comp,
                                    const std::optional<QBrush>& fill, const QLineF& line) const
{
    if (fill)
        painter->fillRect(legendSwatchRect(line), *fill);
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
}

// --- Legend ---

bool QCPGraph::addToLegend(QCPLegend* legend)
{
    if (!legendAllowed())
        return false;
    if (!legend) {
        if (mParentPlot)
            legend = mParentPlot->legend;
        else
            return false;
    }
    if (legend->parentPlot() != mParentPlot || legend->hasItemWithPlottable(this))
        return false;
    return legend->addItem(new QCPGroupLegendItem(legend, this));
}

bool QCPGraph::removeFromLegend(QCPLegend* legend) const
{
    if (!legend) {
        if (mParentPlot)
            legend = mParentPlot->legend;
        else
            return false;
    }
    return QCPAbstractPlottable::removeFromLegend(legend);
}

