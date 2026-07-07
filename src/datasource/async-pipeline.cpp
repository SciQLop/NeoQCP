#include "async-pipeline.h"
#include "pipeline-scheduler.h"
#include "../Profiling.hpp"
#include "../axis/axis.h"
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
    return mGeneration.load() > mDisplayedGeneration;
}

void QCPAsyncPipelineBase::emitBusyIfNeeded(QMutexLocker<QMutex>& lock)
{
    const bool shouldEmitBusy = !mWasBusy;
    if (shouldEmitBusy)
        mWasBusy = true;
    lock.unlock();
    if (shouldEmitBusy)
        Q_EMIT busyChanged(true);
}

void QCPAsyncPipelineBase::onDataChanged()
{
    PROFILE_HERE_N("Pipeline::onDataChanged");
    uint64_t gen = ++mGeneration;
    QMutexLocker lock(&mMutex);
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
        mJobRunning = true;
        mRunningGeneration = gen;
        emitBusyIfNeeded(lock);
        mScheduler->submit(QCPPipelineScheduler::Heavy, std::move(job));
        return;
    }

    emitBusyIfNeeded(lock);
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

    uint64_t gen = ++mGeneration;
    QMutexLocker lock(&mMutex);
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
        mJobRunning = true;
        mRunningGeneration = gen;
        emitBusyIfNeeded(lock);
        mScheduler->submit(QCPPipelineScheduler::Fast, std::move(job));
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

    if (generation > mDisplayedGeneration)
    {
        mDisplayedGeneration = generation;
        applyResult(generation, std::move(result));
        Q_EMIT finished(generation);
    }

    if (idle)
    {
        // Nothing is running or pending: catch mDisplayedGeneration up to the
        // latest bumped generation. Without this, a data/viewport change that
        // built an empty pending job (e.g. source cleared while this job was
        // in flight) leaves its newer generation stranded — isBusy() would
        // report true forever since no further result will ever arrive for it.
        const uint64_t latest = mGeneration.load();
        if (latest > mDisplayedGeneration)
            mDisplayedGeneration = latest;
    }

    bool busy = isBusy();
    if (busy != mWasBusy)
    {
        mWasBusy = busy;
        Q_EMIT busyChanged(busy);
    }
}
