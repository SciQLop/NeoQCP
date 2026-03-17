#include "async-pipeline.h"
#include "pipeline-scheduler.h"
#include "../Profiling.hpp"

QCPAsyncPipelineBase::QCPAsyncPipelineBase(QCPPipelineScheduler* scheduler,
                                             QObject* parent)
    : QObject(parent)
    , mScheduler(scheduler)
    , mDestroyed(std::make_shared<std::atomic<bool>>(false))
{
}

QCPAsyncPipelineBase::~QCPAsyncPipelineBase()
{
    mDestroyed->store(true);
}

bool QCPAsyncPipelineBase::isBusy() const
{
    return mGeneration.load() > mDisplayedGeneration;
}

void QCPAsyncPipelineBase::onDataChanged()
{
    PROFILE_HERE_N("Pipeline::onDataChanged");
    uint64_t gen = ++mGeneration;
    QMutexLocker lock(&mMutex);
    mCache = std::any{};

    if (mJobRunning)
    {
        // Data change supersedes any pending viewport change
        mPending = makeJob(mLastViewport, std::any{}, gen);
        mPendingViewport = false;
        mPendingPriority = QCPPipelineScheduler::Heavy;
    }
    else
    {
        auto job = makeJob(mLastViewport, std::any{}, gen);
        if (!job) return;
        mJobRunning = true;
        mRunningGeneration = gen;
        lock.unlock();
        mScheduler->submit(QCPPipelineScheduler::Heavy, std::move(job));
    }
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
        // Defer job creation — cache will be available when current job finishes
        mPendingViewport = true;
        mPendingPriority = QCPPipelineScheduler::Fast;
    }
    else
    {
        auto cache = std::move(mCache);
        auto job = makeJob(vp, std::move(cache), gen);
        if (!job) return;
        mJobRunning = true;
        mRunningGeneration = gen;
        lock.unlock();
        mScheduler->submit(QCPPipelineScheduler::Fast, std::move(job));
    }
}

void QCPAsyncPipelineBase::deliverResult(uint64_t generation, std::any cache, std::any result)
{
    PROFILE_HERE_N("Pipeline::deliverResult");
    if (mDestroyed->load())
        return;

    QMutexLocker lock(&mMutex);
    mCache = std::move(cache);

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

        lock.unlock();
        if (job)
            mScheduler->submit(priority, std::move(job));
        else
            mJobRunning = false;
    }
    else
    {
        mJobRunning = false;
        lock.unlock();
    }

    if (generation > mDisplayedGeneration)
    {
        mDisplayedGeneration = generation;
        applyResult(generation, std::move(result));
        Q_EMIT finished(generation);
    }

    bool busy = isBusy();
    if (busy != mWasBusy)
    {
        mWasBusy = busy;
        Q_EMIT busyChanged(busy);
    }
}
