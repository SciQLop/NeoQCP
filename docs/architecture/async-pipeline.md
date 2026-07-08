# Async Pipeline

NeoQCP's plottables can run expensive data transforms (resampling, filtering) off the GUI thread. The async pipeline infrastructure provides request coalescing, generation tracking, and thread-safe result delivery.

## Architecture

```mermaid
classDiagram
    class QCPPipelineScheduler {
        -QThreadPool mPool
        -deque~fn~ mFastQueue
        -deque~fn~ mHeavyQueue
        +submit(Priority, work)
    }

    class QCPAsyncPipelineBase {
        #QCPPipelineScheduler* mScheduler
        #TransformKind mKind
        #atomic~uint64_t~ mGeneration
        #uint64_t mDisplayedGeneration
        +onDataChanged()
        +onViewportChanged(ViewportParams)
        +isBusy() bool
        #makeJob(vp, cache, generation)* fn
        #applyResult(generation, result)*
        #deliverResult(generation, cache, result)
    }

    class QCPAsyncPipeline~In_Out~ {
        -shared_ptr~In~ mSource
        -TransformFn mTransform
        -shared_ptr~Out~ mResult
        +setTransform(kind, fn)
        +setSource(source)
        +result() Out*
    }

    QCPAsyncPipelineBase <|-- QCPAsyncPipeline
    QCPAsyncPipelineBase --> QCPPipelineScheduler : submits to
```

Two concrete type aliases are used:

```cpp
using QCPGraphPipeline   = QCPAsyncPipeline<QCPAbstractDataSource, QCPAbstractDataSource>;
using QCPColormapPipeline = QCPAsyncPipeline<QCPAbstractDataSource2D, QCPColorMapData>;
```

## Pipeline Scheduler

`QCPPipelineScheduler` wraps a `QThreadPool` with two priority queues:

- **Fast** — viewport changes (pan/zoom) — low latency, displaces pending heavy jobs
- **Heavy** — data changes (new data loaded) — can be deferred

Each `QCustomPlot` widget owns one scheduler, shared by all its plottables. This prevents oversubscription when many plottables replot simultaneously.

## Request Lifecycle

```mermaid
sequenceDiagram
    participant User as User Action<br/>(pan/zoom/new data)
    participant Plottable as QCPColorMap2
    participant Pipeline as QCPAsyncPipeline
    participant Scheduler as QCPPipelineScheduler
    participant Worker as Worker Thread
    participant Main as Main Thread

    User->>Plottable: axis rangeChanged
    Plottable->>Pipeline: onViewportChanged(vp)
    Pipeline->>Pipeline: generation++
    Pipeline->>Scheduler: submit(Fast, job_lambda)

    Note over Pipeline: If a job is already running,<br/>the new one is queued.<br/>Only the latest pending request survives.

    Scheduler->>Worker: Execute job
    Worker->>Worker: transform(source, viewport, cache)
    Worker->>Main: QMetaObject::invokeMethod<br/>(Qt::QueuedConnection)
    Main->>Pipeline: deliverResult(gen, cache, result)

    alt generation matches
        Pipeline->>Pipeline: applyResult → store mResult
        Pipeline-->>Plottable: finished(generation)
        Plottable->>Plottable: replot
    else stale generation
        Pipeline->>Pipeline: Discard result
    end
```

## Generation Tracking

Every call to `onDataChanged()` or `onViewportChanged()` increments an atomic generation counter. When a result arrives:

- If `generation > mDisplayedGeneration` → accept and store
- If `generation <= mDisplayedGeneration` → discard (same or newer result already arrived)

This handles the common case where the user pans rapidly, producing many viewport changes. Only the latest result is displayed.

## Request Coalescing

When a job is already running and a new request arrives, the pipeline stores it as `mPending`. When the running job completes, only the latest pending request is executed. Earlier intermediate requests are silently dropped.

```
Time ──────────────────────────────────────────────►
       Request A     Request B    Request C
       submitted     submitted    submitted
         │              │            │
    ┌────┴────┐    (replaced)   (replaced)
    │ Job A   │                     │
    │ running │                ┌────┴────┐
    └────┬────┘                │ Job C   │
         │                     │ running │
    Result A                   └────┬────┘
    displayed                       │
                               Result C
                               displayed

    Request B was never executed — coalesced away.
```

## Transform Kinds

```cpp
enum class TransformKind { ViewportIndependent, ViewportDependent };
```

- **ViewportIndependent** — transform result doesn't change with pan/zoom (e.g., data filtering). Re-run only on `onDataChanged()`.
- **ViewportDependent** — result depends on the visible range and pixel size (e.g., colormap resampling). Re-run on both `onDataChanged()` and `onViewportChanged()`.

## Cache

Each pipeline carries a `std::any mCache` that persists across job invocations. The transform function receives and can modify the cache:

```cpp
using TransformFn = std::function<
    shared_ptr<Out>(const In& source,
                    const ViewportParams& viewport,
                    std::any& cache)>;
```

This enables multi-level resampling strategies where an expensive first pass is cached and a cheap second pass runs on each viewport change (see [Hierarchical Graph Resampling](../specs/2026-03-14-async-pipeline-design.md) for the planned graph resampler).

## Destruction Safety

The pipeline uses a `shared_ptr<DestroyGuard>` (`DestroyGuard` = `{ QMutex mutex; bool destroyed; }`), shared between the pipeline and its in-flight jobs. The destructor takes the mutex and sets `destroyed = true`; a job holds the same mutex across its destroyed-check and its `QMetaObject::invokeMethod` call on `this`, so the pipeline cannot be deleted in the window between the check and the metacall being posted — a plain atomic flag can't close that race, since the destructor could still run (and free `this`) between an atomic load and the `invokeMethod` call that follows it.

## Key Files

| File | Role |
|---|---|
| `src/datasource/pipeline-scheduler.h` | QCPPipelineScheduler: thread pool with priority queues |
| `src/datasource/async-pipeline.h` | QCPAsyncPipelineBase + QCPAsyncPipeline<In,Out> template |
