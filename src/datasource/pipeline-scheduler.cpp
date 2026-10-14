#include "pipeline-scheduler.h"
#include "thread-naming.h"
#include <QThread>
#include <algorithm>
#include <latch>
#include <memory>

namespace {
int defaultThreadCount(int maxThreads)
{
    return maxThreads > 0 ? maxThreads : std::max(1, QThread::idealThreadCount() / 2);
}
}

QCPPipelineScheduler::QCPPipelineScheduler(int maxThreads, QObject* parent)
    : QCPPipelineScheduler(&mOwnPool, maxThreads, parent)
{
    mOwnPool.setMaxThreadCount(mMaxThreads);
    mOwnPool.setExpiryTimeout(-1);
}

QCPPipelineScheduler::QCPPipelineScheduler(QThreadPool* pool, int maxThreads, QObject* parent)
    : QObject(parent), mPool(pool), mMaxThreads(defaultThreadCount(maxThreads))
{
}

// A browser thread only starts once the main thread gets back to the browser's event loop,
// unless one of the web workers made before main() (-sPTHREAD_POOL_SIZE, 4) is free. A plot
// with a pool of its own makes threads late, and removing it waits for them on the main
// thread: the page freezes. So every plot shares this pool, its threads started at once,
// on the workers made before main(), and never retired.
// simplify: 3 threads for every plot of the page; more needs a larger PTHREAD_POOL_SIZE.
QThreadPool* QCPPipelineScheduler::browserPool()
{
    static constexpr int threads = 3;
    static QThreadPool* pool = [] {
        auto* p = new QThreadPool; // never deleted: its threads live as long as the page
        p->setMaxThreadCount(threads);
        p->setExpiryTimeout(-1);
        // Each task waits for the others, so each one gets a thread of its own.
        auto started = std::make_shared<std::latch>(threads);
        for (int i = 0; i < threads; ++i)
            p->start([started] { started->arrive_and_wait(); });
        return p;
    }();
    return pool;
}

QCPPipelineScheduler::~QCPPipelineScheduler()
{
    // Queued jobs belong to plottables that may already be deleted by the time
    // the scheduler dies (~QCustomPlot destroys plottables first): drop them,
    // then wait for the running ones (only ours: the pool may be shared).
    QMutexLocker lock(&mMutex);
    mShuttingDown = true;
    mFastQueue.clear();
    mHeavyQueue.clear();
    while (mRunning > 0)
        mIdle.wait(&mMutex);
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
    scheduleNextLocked();
}

void QCPPipelineScheduler::scheduleNextLocked()
{
    if (mShuttingDown)
        return;
    while (mRunning < mMaxThreads)
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
        mPool->start([self, work = std::move(work)]() mutable {
            qcp::nameThisPoolThreadOnce("pipeSched");
            try { work(); } catch (...) { }
            // The last touch of self, under its mutex: the destructor waits for it.
            QMutexLocker lock(&self->mMutex);
            --self->mRunning;
            self->scheduleNextLocked();
            self->mIdle.wakeAll();
        });
    }
}

void QCPPipelineScheduler::setMaxThreads(int count)
{
    {
        QMutexLocker lock(&mMutex);
        mMaxThreads = std::max(1, count);
        if (mPool == &mOwnPool)
            mOwnPool.setMaxThreadCount(mMaxThreads);
    }
    scheduleNext();
}

int QCPPipelineScheduler::maxThreads() const
{
    return mMaxThreads;
}
