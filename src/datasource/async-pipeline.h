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
#include <optional>
#include <type_traits>
#include "pipeline-scheduler.h"

class QCPAxis;
class QCPAxisRect;

struct ViewportParams {
    QCPRange keyRange;
    QCPRange valueRange;
    int plotWidthPx = 0;
    int plotHeightPx = 0;
    bool keyLogScale = false;
    bool valueLogScale = false;

    static ViewportParams fromAxes(const QCPAxis* keyAxis, const QCPAxis* valueAxis);
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

    //! The scheduler the jobs run on. With none (a plottable outside a plot) a job runs on the
    //! calling thread, and its result is delivered by the event loop as usual.
    void setScheduler(QCPPipelineScheduler* scheduler) { mScheduler = scheduler; }
    [[nodiscard]] QCPPipelineScheduler* scheduler() const { return mScheduler; }

    // Generation bumped by every data/viewport change; finished(gen) reports
    // the generation a delivered result belongs to.
    uint64_t currentGeneration() const { return mGeneration.load(); }

    void onDataChanged();
    void onViewportChanged(const ViewportParams& vp);
    //! After a synchronous run (export): the results of the jobs already started are dropped
    //! when they land, so they cannot replace the newer synchronous result. Emits nothing, so
    //! it is safe inside a paint.
    //! - Viewport-independent transforms only. A pending viewport replay takes the latest
    //!   generation, so it would be dropped too, and nothing asks for that viewport again.
    //! - A dropped job still runs to the end. It still installs its cache, which is harmless
    //!   only for cache-less transforms such as bin2d.
    //! - isBusy() turns false at once. busyChanged(false) comes only when the dropped job lands.
    void supersedeEarlierJobs();

    // The viewport of the latest job requested, async or synchronous.
    ViewportParams lastViewport() const
    {
        QMutexLocker lock(&mMutex);
        return mLastViewport;
    }

Q_SIGNALS:
    void finished(uint64_t generation);
    //! With data changed off the owner thread, a queued busyChanged(true) can land after
    //! busyChanged(false): slots re-read isBusy() instead of trusting the argument.
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
    std::function<void()> mPending;  // pre-baked job for data changes
    bool mPendingViewport = false;   // deferred viewport job: created at dispatch with restored cache
    QCPPipelineScheduler::Priority mPendingPriority = QCPPipelineScheduler::Heavy;
    uint64_t mRunningGeneration = 0;
    bool mJobRunning = false;

    void submit(QCPPipelineScheduler::Priority priority, std::function<void()> job);
    bool claimDisplayedGeneration(uint64_t generation);
    //! The new busy state if it differs from the last one emitted; call without the lock held.
    std::optional<bool> claimBusyEdge();
    void emitBusyIfNeeded(QMutexLocker<QMutex>& lock);
    void startJob(QMutexLocker<QMutex>& lock, QCPPipelineScheduler::Priority priority,
                  std::function<void()> job);
    void settleIdle(QMutexLocker<QMutex>& lock, uint64_t gen);

    ViewportParams mLastViewport;
    bool mWasBusy = false;

    // Shared between the pipeline and its in-flight jobs. The destructor flips
    // `destroyed` under the mutex and jobs hold the mutex across their
    // check-and-invoke, so the pipeline cannot be deleted between a job's
    // destroyed-check and its QMetaObject::invokeMethod on `this`.
    struct DestroyGuard {
        QMutex mutex;
        bool destroyed = false;
    };
    std::shared_ptr<DestroyGuard> mDestroyGuard;

public:
    // Only safe to call from the pipeline's thread when no job is running
    // (e.g. inside a finished() signal handler after deliverResult completes).
    std::any& cache() { return mCache; }
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

    // GUI-thread-only (not safe to call concurrently from another thread).
    // Calling it again after setSource() is fine — e.g. to re-bake a captured
    // parameter (QCPColorMap2::setGapThreshold does this) — since makeJob()
    // reads mTransform to build each job's closure, and that closure keeps
    // its own copy; already-dispatched jobs are unaffected by a later call.
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

    // The pointer stays valid only until the next delivery: use it within the call, on the GUI thread.
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
        QMutexLocker lock(&mMutex);
        return mResult.get();
    }

    bool hasTransform() const { return !!mTransform; }

    // Run the transform synchronously (blocking) and store the result.
    // Use this during export (savePdf/toPixmap) where the event loop is not running.
    bool runSynchronously(const ViewportParams& vp)
    {
        QMutexLocker lock(&mMutex);
        if (!mSource || !mTransform) return false;
        auto source = mSource;
        auto transform = mTransform;
        auto cache = std::move(mCache);
        mLastViewport = vp;
        lock.unlock();

        auto out = transform(*source, vp, cache);

        lock.relock();
        mCache = std::move(cache);
        lock.unlock();

        if (!out) return false;
        lock.relock();
        mResult = std::move(out);
        return true;
    }

    void clearTransform()
    {
        mTransform = {};
        QMutexLocker lock(&mMutex);
        mResult.reset();
        mCache = {};
    }

protected:
    std::function<void()> makeJob(
        const ViewportParams& vp, std::any cache, uint64_t generation) override
    {
        if (!mSource || !mTransform) return {};

        auto source = mSource; // shared_ptr copy — keeps source alive for the job
        auto transform = mTransform;
        auto guard = mDestroyGuard;
        auto* self = this;

        return [source, transform, vp, cache = std::move(cache),
                generation, guard, self]() mutable {
            auto result = transform(*source, vp, cache);

            // Held across check + invoke: the destructor takes the same mutex
            // before flipping `destroyed`, so `self` cannot die in between.
            // Once invokeMethod has posted, Qt purges the pending metacall if
            // the receiver is deleted before delivery.
            QMutexLocker lock(&guard->mutex);
            if (guard->destroyed) return;

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
            if (*ptr)
            {
                QMutexLocker lock(&mMutex);
                mResult = std::move(*ptr);
            }
    }

private:
    std::shared_ptr<const In> mSource;
    TransformFn mTransform;
    std::shared_ptr<Out> mResult;
};

using QCPColormapPipeline = QCPAsyncPipeline<QCPAbstractDataSource2D, QCPColorMapData>;
using QCPHistogramPipeline = QCPAsyncPipeline<QCPAbstractDataSource, QCPColorMapData>;

class QCPAbstractMultiDataSource;
using QCPMultiGraphPipeline = QCPAsyncPipeline<QCPAbstractMultiDataSource, QCPAbstractMultiDataSource>;

namespace qcp::algo { struct Histogram1D; }
using QCPHistogram1DPipeline = QCPAsyncPipeline<QCPAbstractMultiDataSource, qcp::algo::Histogram1D>;
