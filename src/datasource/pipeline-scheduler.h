#pragma once
#include <QObject>
#include <QThreadPool>
#include <QMutex>
#include <QRunnable>
#include <functional>
#include <deque>

class QCPPipelineScheduler : public QObject
{
    Q_OBJECT

public:
    enum Priority { Fast, Heavy };

    explicit QCPPipelineScheduler(int maxThreads = 0, QObject* parent = nullptr);
    ~QCPPipelineScheduler() override;

    void submit(Priority priority, std::function<void()> work);
    void setMaxThreads(int count);
    int maxThreads() const;

private:
    void scheduleNext();

    QThreadPool mPool;
    QMutex mMutex;
    std::deque<std::function<void()>> mFastQueue;
    std::deque<std::function<void()>> mHeavyQueue;
    int mRunning = 0;
    bool mShuttingDown = false;
};
