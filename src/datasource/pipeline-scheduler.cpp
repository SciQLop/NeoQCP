#include "pipeline-scheduler.h"
#include "thread-naming.h"
#include <QThread>
#include <algorithm>

QCPPipelineScheduler::QCPPipelineScheduler(int maxThreads, QObject* parent)
    : QObject(parent)
{
    int threads = maxThreads > 0 ? maxThreads
                                 : std::max(1, QThread::idealThreadCount() / 2);
    mPool.setMaxThreadCount(threads);
    mPool.setExpiryTimeout(-1);
}

QCPPipelineScheduler::~QCPPipelineScheduler()
{
    // Queued jobs belong to plottables that may already be deleted by the time
    // the scheduler dies (~QCustomPlot destroys plottables first): drop them
    // before joining, otherwise a finishing task's epilogue would start them.
    {
        QMutexLocker lock(&mMutex);
        mShuttingDown = true;
        mFastQueue.clear();
        mHeavyQueue.clear();
    }
    mPool.waitForDone();
}

void QCPPipelineScheduler::submit(Priority priority, std::function<void()> work)
{
    QMutexLocker lock(&mMutex);
    if (mShuttingDown)
        return;
    if (priority == Fast)
        mFastQueue.push_back(std::move(work));
    else
        mHeavyQueue.push_back(std::move(work));

    lock.unlock();
    scheduleNext();
}

void QCPPipelineScheduler::scheduleNext()
{
    QMutexLocker lock(&mMutex);
    if (mShuttingDown)
        return;
    while (mRunning < mPool.maxThreadCount())
    {
        std::function<void()> work;
        if (!mFastQueue.empty())
        {
            work = std::move(mFastQueue.front());
            mFastQueue.pop_front();
        }
        else if (!mHeavyQueue.empty())
        {
            work = std::move(mHeavyQueue.front());
            mHeavyQueue.pop_front();
        }
        else
            break;

        ++mRunning;
        auto* self = this;
        mPool.start([self, work = std::move(work)]() mutable {
            qcp::nameThisPoolThreadOnce("pipeSched");
            try { work(); } catch (...) { }
            {
                QMutexLocker lock(&self->mMutex);
                --self->mRunning;
            }
            self->scheduleNext();
        });
    }
}

void QCPPipelineScheduler::setMaxThreads(int count)
{
    {
        QMutexLocker lock(&mMutex);
        mPool.setMaxThreadCount(std::max(1, count));
    }
    scheduleNext();
}

int QCPPipelineScheduler::maxThreads() const
{
    return mPool.maxThreadCount();
}
