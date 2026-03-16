#pragma once
#include <QObject>
#include <QMutex>
#include <QMutexLocker>
#include <QMetaObject>
#include <QThread>
#include <axis/range.h>
#include <functional>
#include <memory>
#include <any>
#include <atomic>
#include <type_traits>
#include "pipeline-scheduler.h"

struct ViewportParams {
    QCPRange keyRange;
    QCPRange valueRange;
    int plotWidthPx = 0;
    int plotHeightPx = 0;
    bool keyLogScale = false;
    bool valueLogScale = false;
};

enum class TransformKind { ViewportIndependent, ViewportDependent };

class QCPAsyncPipelineBase : public QObject
{
    Q_OBJECT

public:
    explicit QCPAsyncPipelineBase(QCPPipelineScheduler* scheduler,
                                  QObject* parent = nullptr);
    ~QCPAsyncPipelineBase() override;

    bool isBusy() const;

    void onDataChanged();
    void onViewportChanged(const ViewportParams& vp);

Q_SIGNALS:
    void finished(uint64_t generation);
    void busyChanged(bool busy);

protected:
    virtual std::function<void()> makeJob(
        const ViewportParams& vp, std::any cache, uint64_t generation) = 0;
    virtual void applyResult(uint64_t generation, std::any result) = 0;
    void deliverResult(uint64_t generation, std::any cache, std::any result);

    QCPPipelineScheduler* mScheduler;
    TransformKind mKind = TransformKind::ViewportIndependent;
    std::atomic<uint64_t> mGeneration{0};
    uint64_t mDisplayedGeneration = 0;

    mutable QMutex mMutex;
    std::any mCache;
    std::function<void()> mPending;
    QCPPipelineScheduler::Priority mPendingPriority = QCPPipelineScheduler::Heavy;
    uint64_t mRunningGeneration = 0;
    bool mJobRunning = false;

    ViewportParams mLastViewport;
    bool mWasBusy = false;

    std::shared_ptr<std::atomic<bool>> mDestroyed;
};

class QCPAbstractDataSource;
class QCPAbstractDataSource2D;
class QCPColorMapData;

template<typename In, typename Out>
class QCPAsyncPipeline : public QCPAsyncPipelineBase
{
public:
    using TransformFn = std::function<
        std::shared_ptr<Out>(const In& source,
                             const ViewportParams& viewport,
                             std::any& cache)>;

    explicit QCPAsyncPipeline(QCPPipelineScheduler* scheduler,
                               QObject* parent = nullptr)
        : QCPAsyncPipelineBase(scheduler, parent) {}

    // Must be called before setSource() — not thread-safe with concurrent jobs.
    void setTransform(TransformKind kind, TransformFn fn)
    {
        mKind = kind;
        mTransform = std::move(fn);
    }

    void setSource(std::shared_ptr<const In> source)
    {
        {
            QMutexLocker lock(&mMutex);
            mSource = std::move(source);
        }
        if (mTransform)
            onDataChanged();
    }

    const Out* result() const
    {
        if (!mTransform)
        {
            if constexpr (std::is_same_v<In, Out>)
            {
                QMutexLocker lock(&mMutex);
                return mSource.get();
            }
            else
                return nullptr;
        }
        return mResult.get();
    }

    bool hasTransform() const { return !!mTransform; }

protected:
    std::function<void()> makeJob(
        const ViewportParams& vp, std::any cache, uint64_t generation) override
    {
        if (!mSource || !mTransform) return {};

        auto source = mSource; // shared_ptr copy — keeps source alive for the job
        auto transform = mTransform;
        auto destroyed = mDestroyed;
        auto* self = this;

        return [source, transform, vp, cache = std::move(cache),
                generation, destroyed, self]() mutable {
            auto result = transform(*source, vp, cache);
            if (destroyed->load()) return;

            QMetaObject::invokeMethod(self, [self, result = std::move(result),
                                              cache = std::move(cache), generation]() mutable {
                self->deliverResult(generation, std::move(cache),
                                    std::any(std::move(result)));
            }, Qt::QueuedConnection);
        };
    }

    void applyResult(uint64_t, std::any result) override
    {
        if (auto* ptr = std::any_cast<std::shared_ptr<Out>>(&result))
            mResult = std::move(*ptr);
    }

private:
    std::shared_ptr<const In> mSource;
    TransformFn mTransform;
    std::shared_ptr<Out> mResult;
};

using QCPGraphPipeline = QCPAsyncPipeline<QCPAbstractDataSource, QCPAbstractDataSource>;
using QCPColormapPipeline = QCPAsyncPipeline<QCPAbstractDataSource2D, QCPColorMapData>;
using QCPHistogramPipeline = QCPAsyncPipeline<QCPAbstractDataSource, QCPColorMapData>;
