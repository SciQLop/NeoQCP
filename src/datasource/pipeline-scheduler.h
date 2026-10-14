#pragma once
#include <QObject>
#include <QThreadPool>
#include <QMutex>
#include <QWaitCondition>
#include <QRunnable>
#include <functional>
#include <deque>

class QCPPipelineScheduler : public QObject
{
    Q_OBJECT

public:
    enum Priority { Fast, Heavy };

    explicit QCPPipelineScheduler(int maxThreads = 0, QObject* parent = nullptr);
    //! Runs jobs on \a pool, which other schedulers may share; it must outlive this one.
    explicit QCPPipelineScheduler(QThreadPool* pool, int maxThreads = 0, QObject* parent = nullptr);
    ~QCPPipelineScheduler() override;

    //! The pool every plot shares in the browser, its threads started at once (see the .cpp).
    static QThreadPool* browserPool();

    void submit(Priority priority, std::function<void()> work);
    void setMaxThreads(int count);
    int maxThreads() const;

private:
    void scheduleNext();
    void scheduleNextLocked();

    QThreadPool mOwnPool;
    QThreadPool* mPool;
    int mMaxThreads;
    QMutex mMutex;
    QWaitCondition mIdle;
    std::deque<std::function<void()>> mFastQueue;
    std::deque<std::function<void()>> mHeavyQueue;
    int mRunning = 0;
    bool mShuttingDown = false;
};
