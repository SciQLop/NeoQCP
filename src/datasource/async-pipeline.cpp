#include "async-pipeline.h"
#include "pipeline-scheduler.h"
#include "../Profiling.hpp"
#include "../axis/axis.h"
#include <QPointer>
#include <utility>
#include "../layoutelements/layoutelement-axisrect.h"

ViewportParams ViewportParams::fromAxes(const QCPAxis* keyAxis, const QCPAxis* valueAxis)
{
    ViewportParams vp;
    vp.keyRange = keyAxis->range();
    vp.valueRange = valueAxis->range();
    auto* axisRect = keyAxis->axisRect();
    vp.plotWidthPx = axisRect ? axisRect->width() : 800;
    vp.plotHeightPx = axisRect ? axisRect->height() : 600;
    vp.keyLogScale = (keyAxis->scaleType() == QCPAxis::stLogarithmic);
    vp.valueLogScale = (valueAxis->scaleType() == QCPAxis::stLogarithmic);
    return vp;
}

QCPAsyncPipelineBase::QCPAsyncPipelineBase(QCPPipelineScheduler* scheduler,
                                             QObject* parent)
    : QObject(parent)
    , mScheduler(scheduler)
    , mDestroyGuard(std::make_shared<DestroyGuard>())
{
}

QCPAsyncPipelineBase::~QCPAsyncPipelineBase()
{
    // Taken under the guard mutex so no job sits between its destroyed-check
    // and its invokeMethod on this object (see makeJob).
    QMutexLocker lock(&mDestroyGuard->mutex);
    mDestroyGuard->destroyed = true;
}

bool QCPAsyncPipelineBase::isBusy() const
{
    QMutexLocker lock(&mMutex);
    return mGeneration.load() > mDisplayedGeneration;
}

void QCPAsyncPipelineBase::emitBusyIfNeeded(QMutexLocker<QMutex>& lock)
{
    const bool shouldEmitBusy = !mWasBusy;
    if (shouldEmitBusy)
        mWasBusy = true;
    lock.unlock();
    // Last, and callers return at once: a slot may delete the owner and this pipeline.
    if (shouldEmitBusy)
        Q_EMIT busyChanged(true);
}

// A busyChanged slot may delete the plottable that owns this pipeline, so the job is handed to
// the scheduler first and the signal comes last.
void QCPAsyncPipelineBase::startJob(QMutexLocker<QMutex>& lock,
                                    QCPPipelineScheduler::Priority priority,
                                    std::function<void()> job)
{
    mJobRunning = true;
    const bool becameBusy = !std::exchange(mWasBusy, true);
    lock.unlock();
    mScheduler->submit(priority, std::move(job));
    // Last, and callers return at once: a slot may delete the owner and this pipeline.
    if (becameBusy)
        Q_EMIT busyChanged(true);
}

void QCPAsyncPipelineBase::onDataChanged()
{
    PROFILE_HERE_N("Pipeline::onDataChanged");
    // Bumped under the lock: a result landing in between would count this generation as shown.
    QMutexLocker lock(&mMutex);
    uint64_t gen = ++mGeneration;
    mCache = std::any{};

    if (mJobRunning)
    {
        mPending = makeJob(mLastViewport, std::any{}, gen);
        mPendingViewport = false;
        mPendingPriority = QCPPipelineScheduler::Heavy;
    }
    else
    {
        auto job = makeJob(mLastViewport, std::any{}, gen);
        if (!job)
        {
            settleIdle(lock, gen);
            return;
        }
        startJob(lock, QCPPipelineScheduler::Heavy, std::move(job));
        return;
    }

    emitBusyIfNeeded(lock);
}

bool QCPAsyncPipelineBase::claimDisplayedGeneration(uint64_t generation)
{
    QMutexLocker lock(&mMutex);
    if (generation <= mDisplayedGeneration)
        return false;
    mDisplayedGeneration = generation;
    return true;
}

std::optional<bool> QCPAsyncPipelineBase::claimBusyEdge()
{
    QMutexLocker lock(&mMutex);
    const bool busy = mGeneration.load() > mDisplayedGeneration;
    if (busy == mWasBusy)
        return std::nullopt;
    mWasBusy = busy;
    return busy;
}

void QCPAsyncPipelineBase::supersedeEarlierJobs()
{
    QMutexLocker lock(&mMutex);
    mDisplayedGeneration = ++mGeneration;
}

void QCPAsyncPipelineBase::settleIdle(QMutexLocker<QMutex>& lock, uint64_t gen)
{
    // No job is running or pending for `gen` (there was nothing to build one
    // for, e.g. the source was cleared) — resync mDisplayedGeneration so
    // isBusy() doesn't stay stuck true forever waiting for a result that will
    // never arrive.
    mDisplayedGeneration = gen;
    const bool shouldEmitIdle = mWasBusy;
    mWasBusy = false;
    lock.unlock();
    // Last, and callers return at once: a slot may delete the owner and this pipeline.
    if (shouldEmitIdle)
        Q_EMIT busyChanged(false);
}

void QCPAsyncPipelineBase::onViewportChanged(const ViewportParams& vp)
{
    PROFILE_HERE_N("Pipeline::onViewportChanged");
    if (mKind == TransformKind::ViewportIndependent)
    {
        QMutexLocker lock(&mMutex);
        mLastViewport = vp;
        return;
    }

    QMutexLocker lock(&mMutex);
    uint64_t gen = ++mGeneration;
    mLastViewport = vp;

    if (mJobRunning)
    {
        mPendingViewport = true;
        mPendingPriority = QCPPipelineScheduler::Fast;
    }
    else
    {
        auto cache = std::move(mCache);
        auto job = makeJob(vp, std::move(cache), gen);
        if (!job)
        {
            settleIdle(lock, gen);
            return;
        }
        startJob(lock, QCPPipelineScheduler::Fast, std::move(job));
        return;
    }

    emitBusyIfNeeded(lock);
}

void QCPAsyncPipelineBase::deliverResult(uint64_t generation, std::any cache, std::any result)
{
    PROFILE_HERE_N("Pipeline::deliverResult");
    // No destroyed-check needed: this only runs as a queued metacall on a live
    // object (Qt purges pending metacalls when the receiver is deleted).
    QMutexLocker lock(&mMutex);
    mCache = std::move(cache);

    bool idle = false;

    if (mPending || mPendingViewport)
    {
        auto priority = mPendingPriority;
        mRunningGeneration = mGeneration.load();

        std::function<void()> job;
        if (mPending)
        {
            // Data change: pre-baked job (cache was cleared)
            job = std::move(mPending);
            mPending = nullptr;
        }
        else
        {
            // Viewport change: create job now with the restored cache
            mPendingViewport = false;
            auto jobCache = std::move(mCache);
            job = makeJob(mLastViewport, std::move(jobCache), mRunningGeneration);
        }

        if (!job)
        {
            mJobRunning = false;
            mPendingViewport = false;
            idle = true;
            lock.unlock();
        }
        else
        {
            lock.unlock();
            mScheduler->submit(priority, std::move(job));
        }
    }
    else
    {
        mJobRunning = false;
        idle = true;
        lock.unlock();
    }

    if (claimDisplayedGeneration(generation))
    {
        applyResult(generation, std::move(result));
        // A finished() slot may delete the plottable, and this pipeline with it.
        const QPointer<QCPAsyncPipelineBase> alive(this);
        Q_EMIT finished(generation);
        if (!alive)
            return;
    }

    if (idle)
    {
        // Nothing is running or pending: catch mDisplayedGeneration up to the
        // latest bumped generation. Without this, a data/viewport change that
        // built an empty pending job (e.g. source cleared while this job was
        // in flight) leaves its newer generation stranded — isBusy() would
        // report true forever since no further result will ever arrive for it.
        // A finished() receiver may have started a job meanwhile (QCPHistogram
        // re-bins a result narrower than its edges): that one is not stranded.
        QMutexLocker relock(&mMutex);
        const uint64_t latest = mGeneration.load();
        if (!mJobRunning && latest > mDisplayedGeneration)
            mDisplayedGeneration = latest;
    }

    const auto busyEdge = claimBusyEdge();
    // Last: a slot may delete the owner and this pipeline.
    if (busyEdge)
        Q_EMIT busyChanged(*busyEdge);
}
