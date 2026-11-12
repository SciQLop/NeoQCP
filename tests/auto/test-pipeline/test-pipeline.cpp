#include "test-pipeline.h"
#include <QSignalSpy>
#include <QThread>
#include <QtWidgets/qtestsupport_widgets.h> // QTest::qWaitForWindowExposed
#include <cmath>
#include <datasource/async-pipeline.h>
#include <datasource/graph-resampler.h>
#include <datasource/histogram-binner.h>
#include <datasource/pipeline-scheduler.h>
#include <datasource/resampled-multi-datasource.h>
#include <datasource/soa-datasource-2d.h>
#include <datasource/soa-datasource.h>
#include <datasource/soa-multi-datasource.h>
#include <future>
#include <limits>
#include <optional>
#include <painting/draw-commands.h>
#include <plottables/plottable-colormap.h>
#include <plottables/plottable-colormap2.h>
#include <plottables/plottable-histogram.h>
#include <plottables/plottable-histogram2d.h>
#include <plottables/plottable-multigraph.h>
#include <plottables/plottable-waterfall.h>
#include <qcustomplot.h>
#include <thread>

namespace {
using GenericPipeline = QCPAsyncPipeline<QCPAbstractDataSource, QCPAbstractDataSource>;

std::vector<std::vector<double>> oneColumn(std::vector<double> values)
{
    return std::vector<std::vector<double>> { std::move(values) };
}

std::shared_ptr<QCPSoAMultiDataSource<std::vector<double>, std::vector<double>>>
oneColumnSource(std::vector<double> keys, std::vector<double> values)
{
    return std::make_shared<QCPSoAMultiDataSource<std::vector<double>, std::vector<double>>>(
        std::move(keys), oneColumn(std::move(values)));
}

// Synthetic multi-column source that computes data on-the-fly without allocating N arrays.
class SyntheticLargeMultiSource : public QCPAbstractMultiDataSource
{
public:
    explicit SyntheticLargeMultiSource(int n, int cols) : mN(n), mCols(cols) {}
    int size() const override { return mN; }
    bool empty() const override { return mN == 0; }
    int columnCount() const override { return mCols; }
    double keyAt(int i) const override { return static_cast<double>(i); }
    double valueAt(int col, int i) const override { return std::sin(i * 0.0001 + col); }
    QCPRange keyRange(bool& found, QCP::SignDomain) const override
    {
        found = mN > 0;
        return QCPRange(0, mN - 1);
    }
    QCPRange valueRange(int, bool& found, QCP::SignDomain, const QCPRange&) const override
    {
        found = mN > 0;
        return QCPRange(-1, 1);
    }
    int findBegin(double sortKey, bool) const override
    {
        return std::clamp(static_cast<int>(sortKey), 0, mN);
    }
    int findEnd(double sortKey, bool) const override
    {
        return std::clamp(static_cast<int>(std::ceil(sortKey)) + 1, 0, mN);
    }
    QVector<QPointF> getOptimizedLineData(int col, int begin, int end, int,
                                          QCPAxis* ka, QCPAxis* va) const override
    {
        return getLines(col, begin, end, ka, va);
    }
    QVector<QPointF> getLines(int col, int begin, int end,
                               QCPAxis*, QCPAxis*) const override
    {
        QVector<QPointF> result;
        result.reserve(end - begin);
        for (int i = begin; i < end; ++i)
            result.append(QPointF(keyAt(i), valueAt(col, i)));
        return result;
    }
private:
    int mN;
    int mCols;
};

// Level 1 of one value column: the data bins the multi-column resampler builds.
qcp::algo::MultiColumnBinResult binOneColumn(const std::vector<double>& keys,
                                             const std::vector<double>& values, int begin, int end,
                                             const QCPRange& range, int numBins)
{
    const auto src = oneColumnSource(keys, values);
    return qcp::algo::binMinMaxMulti(*src, begin, end, range, numBins);
}
} // namespace

void TestPipeline::init()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
}

namespace {
// Show the plot, wait for window exposure to drive QRhiWidget::initialize(),
// and report whether a usable QRhi instance was acquired. Tests that exercise
// GPU-backed plottables (MultiGraph/ColorMap2 — whose draw() and
// pipeline are gated on a live RHI context) call this and QSKIP on failure
// in headless / unsupported-platform environments.
bool showAndHasRhi(QCustomPlot* plot)
{
    plot->show();
    if (!QTest::qWaitForWindowExposed(plot))
        return false;
    QCoreApplication::processEvents();
    return plot->rhi() != nullptr;
}
} // namespace

void TestPipeline::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
}

void TestPipeline::schedulerSubmitHeavy()
{
    QCPPipelineScheduler scheduler;
    std::atomic<int> counter{0};
    QEventLoop loop;

    scheduler.submit(QCPPipelineScheduler::Heavy, [&counter]{ ++counter; });

    QTimer::singleShot(100, &loop, &QEventLoop::quit);
    loop.exec();

    QCOMPARE(counter.load(), 1);
}

void TestPipeline::schedulerSubmitFast()
{
    QCPPipelineScheduler scheduler;
    std::atomic<int> counter{0};
    QEventLoop loop;

    scheduler.submit(QCPPipelineScheduler::Fast, [&counter]{ ++counter; });

    QTimer::singleShot(100, &loop, &QEventLoop::quit);
    loop.exec();

    QCOMPARE(counter.load(), 1);
}

void TestPipeline::schedulerFastPriority()
{
    QCPPipelineScheduler scheduler(1); // 1 thread
    std::atomic<bool> gate{false};
    std::vector<int> order;
    QMutex orderMutex;
    QEventLoop loop;

    std::atomic<bool> blockerStarted{false};
    scheduler.submit(QCPPipelineScheduler::Heavy, [&gate, &blockerStarted]{
        blockerStarted.store(true);
        while (!gate.load()) QThread::msleep(5);
    });
    while (!blockerStarted.load()) QThread::msleep(1);

    scheduler.submit(QCPPipelineScheduler::Heavy, [&order, &orderMutex]{
        QMutexLocker lock(&orderMutex);
        order.push_back(1); // heavy
    });
    scheduler.submit(QCPPipelineScheduler::Fast, [&order, &orderMutex]{
        QMutexLocker lock(&orderMutex);
        order.push_back(2); // fast
    });

    gate.store(true);

    QTimer::singleShot(200, &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(order.size() == 2);
    QCOMPARE(order[0], 2); // fast ran first
    QCOMPARE(order[1], 1);
}

void TestPipeline::schedulerOnSharedPoolWaitsForItsOwnJob()
{
    QThreadPool pool;
    std::atomic<bool> started{false}, finished{false};
    auto* scheduler = new QCPPipelineScheduler(&pool);
    scheduler->submit(QCPPipelineScheduler::Heavy, [&]{
        started.store(true);
        QThread::msleep(100);
        finished.store(true);
    });
    while (!started.load()) QThread::msleep(1);
    delete scheduler;
    QVERIFY(finished.load());
}

void TestPipeline::schedulerOnSharedPoolDoesNotWaitForOthers()
{
    // In the browser all plots share one pool: removing a plot must not wait for the others' jobs.
    QThreadPool pool;
    std::atomic<bool> gate{false}, started{false};
    QCPPipelineScheduler busy(&pool);
    busy.submit(QCPPipelineScheduler::Heavy, [&]{
        started.store(true);
        while (!gate.load()) QThread::msleep(5);
    });
    while (!started.load()) QThread::msleep(1);
    auto destroyed = std::async(std::launch::async, [&]{
        QCPPipelineScheduler idle(&pool);
        idle.submit(QCPPipelineScheduler::Fast, []{});
    });
    const bool returned = destroyed.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
    gate.store(true);
    QVERIFY(returned);
}

void TestPipeline::pipelinePassthrough()
{
    QCPPipelineScheduler scheduler;
    auto source = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::vector<double>{1, 2, 3}, std::vector<double>{4, 5, 6});

    GenericPipeline pipeline(&scheduler);
    pipeline.setSource(source);

    QVERIFY(!pipeline.hasTransform());
    QCOMPARE(pipeline.result(), source.get());
}

void TestPipeline::pipelineTransformRuns()
{
    QCPPipelineScheduler scheduler;
    auto source = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::vector<double>{1, 2, 3}, std::vector<double>{4, 5, 6});

    GenericPipeline pipeline(&scheduler);
    QSignalSpy spy(&pipeline, &GenericPipeline::finished);

    pipeline.setTransform(TransformKind::ViewportIndependent,
        [](const QCPAbstractDataSource&, const ViewportParams&, std::any&)
            -> std::shared_ptr<QCPAbstractDataSource> {
            return std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
                std::vector<double>{1, 2, 3}, std::vector<double>{10, 20, 30});
        });

    pipeline.setSource(source);
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 1000);

    QVERIFY(pipeline.result() != nullptr);
    QCOMPARE(pipeline.result()->valueAt(0), 10.0);
}

void TestPipeline::theFirstJobReportsBusyThenIdleWhenItLands()
{
    QCPPipelineScheduler scheduler;
    GenericPipeline pipeline(&scheduler);
    pipeline.setTransform(TransformKind::ViewportIndependent,
        [](const QCPAbstractDataSource& src, const ViewportParams&, std::any&)
            -> std::shared_ptr<QCPAbstractDataSource> {
            return std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
                std::vector<double>{1, 2, 3}, std::vector<double>{4, 5, 6});
        });
    QList<bool> states;
    connect(&pipeline, &GenericPipeline::busyChanged, this, [&](bool busy) { states << busy; });

    pipeline.setSource(std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::vector<double>{1, 2, 3}, std::vector<double>{4, 5, 6}));

    QCOMPARE(states, (QList<bool>{true}));
    QTRY_COMPARE_WITH_TIMEOUT(states, (QList<bool>{true, false}), 1000);
}

// busyChanged is emitted with the pipeline's mutex released: a slot may wait for another
// thread that uses the pipeline.
void TestPipeline::aBusySlotMayWaitForAnotherThreadUsingThePipeline()
{
    QCPPipelineScheduler scheduler;
    GenericPipeline pipeline(&scheduler);
    pipeline.setTransform(TransformKind::ViewportIndependent,
        [](const QCPAbstractDataSource&, const ViewportParams&, std::any&)
            -> std::shared_ptr<QCPAbstractDataSource> {
            return std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
                std::vector<double>{1}, std::vector<double>{1});
        });
    std::promise<void> reached;
    auto reachedFuture = reached.get_future();
    std::thread other;
    bool otherThreadGotThrough = false;
    connect(&pipeline, &GenericPipeline::busyChanged, this, [&](bool busy) {
        if (!busy || other.joinable())
            return;
        other = std::thread([&] {
            pipeline.lastViewport();
            reached.set_value();
        });
        otherThreadGotThrough = reachedFuture.wait_for(std::chrono::seconds(1))
                                == std::future_status::ready;
    });

    pipeline.setSource(std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::vector<double>{1}, std::vector<double>{1}));
    if (other.joinable())
        other.join();
    QVERIFY(otherThreadGotThrough);
}

namespace {
// The pipeline's mutex is the subject of the race test, so the test takes it directly.
struct LockablePipeline : GenericPipeline
{
    using QCPAsyncPipelineBase::mMutex;
    using GenericPipeline::GenericPipeline;
};
}

// Another thread's onDataChanged() while the previous result lands. When the generation was
// bumped before taking the mutex, the landing's idle catch-up counted it as shown before its job
// started, and its result was then dropped as stale. The test parks the other thread on the
// mutex, so the landing races it for the lock.
void TestPipeline::aDataChangeBumpedJustBeforeAResultLandsIsStillShown()
{
    // The other thread wins the lock some of the time: several rounds make the race show.
    for (int round = 0; round < 10; ++round)
    {
        QCPPipelineScheduler scheduler;
        LockablePipeline pipeline(&scheduler);
        pipeline.setTransform(TransformKind::ViewportIndependent,
            [](const QCPAbstractDataSource&, const ViewportParams&, std::any&)
                -> std::shared_ptr<QCPAbstractDataSource> {
                return std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
                    std::vector<double>{1}, std::vector<double>{1});
            });
        std::thread other;
        bool otherStarted = false;
        uint64_t lastShown = 0;
        connect(&pipeline, &GenericPipeline::finished, this, [&](uint64_t generation) {
            lastShown = generation;
            if (std::exchange(otherStarted, true))
                return;
            pipeline.mMutex.lock();
            const uint64_t before = pipeline.currentGeneration();
            other = std::thread([&] { pipeline.onDataChanged(); });
                    // With the fix the bump waits for this lock, so the spin always runs out: the
                    // wait is what lets the unfixed bump-before-lock happen while the other thread
                    // is parked.
            QDeadlineTimer deadline(50);
            while (pipeline.currentGeneration() == before && !deadline.hasExpired())
                QThread::yieldCurrentThread();
            pipeline.mMutex.unlock();
        });

        pipeline.setSource(std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
            std::vector<double>{1}, std::vector<double>{1}));
        QTRY_VERIFY_WITH_TIMEOUT(otherStarted, 1000);
        other.join();
        QTRY_VERIFY_WITH_TIMEOUT(!pipeline.isBusy(), 1000);
        QTest::qWait(20);
        QCOMPARE(lastShown, pipeline.currentGeneration());
    }
}

// Nothing newer is on its way when the latest result lands: a finished() slot sees an idle
// pipeline.
void TestPipeline::theLatestResultIsNotBusyWhenItLands()
{
    QCPPipelineScheduler scheduler;
    GenericPipeline pipeline(&scheduler);
    pipeline.setTransform(TransformKind::ViewportIndependent,
        [](const QCPAbstractDataSource&, const ViewportParams&, std::any&)
            -> std::shared_ptr<QCPAbstractDataSource> {
            return std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
                std::vector<double>{1}, std::vector<double>{1});
        });
    std::optional<bool> busyWhenItLanded;
    connect(&pipeline, &GenericPipeline::finished, this,
            [&](uint64_t) { busyWhenItLanded = pipeline.isBusy(); });

    pipeline.setSource(std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::vector<double>{1}, std::vector<double>{1}));

    QTRY_VERIFY_WITH_TIMEOUT(busyWhenItLanded.has_value(), 1000);
    QCOMPARE(*busyWhenItLanded, false);
}

void TestPipeline::pipelineCoalescing()
{
    QCPPipelineScheduler scheduler(1);
    std::atomic<int> runCount{0};

    auto source = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::vector<double>{1}, std::vector<double>{1});

    GenericPipeline pipeline(&scheduler);
    QSignalSpy spy(&pipeline, &GenericPipeline::finished);

    pipeline.setTransform(TransformKind::ViewportIndependent,
        [&runCount](const QCPAbstractDataSource&, const ViewportParams&, std::any&)
            -> std::shared_ptr<QCPAbstractDataSource> {
            ++runCount;
            QThread::msleep(50);
            return std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
                std::vector<double>{1}, std::vector<double>{1});
        });

    for (int i = 0; i < 5; ++i)
        pipeline.setSource(source);

    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 2000);
    QThread::msleep(200);

    QVERIFY2(runCount.load() <= 3,
        qPrintable(QString("Expected <= 3 runs, got %1").arg(runCount.load())));
}

void TestPipeline::pipelineViewportIndependentSkipsViewport()
{
    QCPPipelineScheduler scheduler;
    std::atomic<int> runCount{0};

    auto source = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::vector<double>{1}, std::vector<double>{1});

    GenericPipeline pipeline(&scheduler);
    QSignalSpy spy(&pipeline, &GenericPipeline::finished);

    pipeline.setTransform(TransformKind::ViewportIndependent,
        [&runCount](const QCPAbstractDataSource&, const ViewportParams&, std::any&)
            -> std::shared_ptr<QCPAbstractDataSource> {
            ++runCount;
            return std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
                std::vector<double>{1}, std::vector<double>{1});
        });

    pipeline.setSource(source);
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 1000);

    int countAfterData = runCount.load();
    pipeline.onViewportChanged(ViewportParams{});
    QThread::msleep(100);

    QCOMPARE(runCount.load(), countAfterData);
}

void TestPipeline::pipelineViewportDependentRuns()
{
    QCPPipelineScheduler scheduler;
    std::atomic<int> runCount{0};

    auto source = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::vector<double>{1}, std::vector<double>{1});

    GenericPipeline pipeline(&scheduler);
    QSignalSpy spy(&pipeline, &GenericPipeline::finished);

    pipeline.setTransform(TransformKind::ViewportDependent,
        [&runCount](const QCPAbstractDataSource&, const ViewportParams&, std::any&)
            -> std::shared_ptr<QCPAbstractDataSource> {
            ++runCount;
            return std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
                std::vector<double>{1}, std::vector<double>{1});
        });

    pipeline.setSource(source);
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 1000);

    pipeline.onViewportChanged(ViewportParams{{0, 10}, {0, 10}, 400, 300, false, false});
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 2, 1000);

    QVERIFY(runCount.load() >= 2);
}

void TestPipeline::pipelineCachePreservedOnViewport()
{
    QCPPipelineScheduler scheduler;

    auto source = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::vector<double>{1}, std::vector<double>{1});

    GenericPipeline pipeline(&scheduler);
    QSignalSpy spy(&pipeline, &GenericPipeline::finished);

    pipeline.setTransform(TransformKind::ViewportDependent,
        [](const QCPAbstractDataSource&, const ViewportParams&, std::any& cache)
            -> std::shared_ptr<QCPAbstractDataSource> {
            int* counter = std::any_cast<int>(&cache);
            if (!counter)
                cache = 1;
            else
            {
                *counter += 1;
                cache = *counter;
            }
            return std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
                std::vector<double>{1}, std::vector<double>{static_cast<double>(std::any_cast<int>(cache))});
        });

    pipeline.setSource(source);
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 1000);

    pipeline.onViewportChanged(ViewportParams{});
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 2, 1000);

    QCOMPARE(pipeline.result()->valueAt(0), 2.0);
}

void TestPipeline::pipelineCacheClearedOnDataChange()
{
    QCPPipelineScheduler scheduler;

    auto source = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::vector<double>{1}, std::vector<double>{1});

    GenericPipeline pipeline(&scheduler);
    QSignalSpy spy(&pipeline, &GenericPipeline::finished);

    pipeline.setTransform(TransformKind::ViewportDependent,
        [](const QCPAbstractDataSource&, const ViewportParams&, std::any& cache)
            -> std::shared_ptr<QCPAbstractDataSource> {
            int val = cache.has_value() ? std::any_cast<int>(cache) + 1 : 1;
            cache = val;
            return std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
                std::vector<double>{1}, std::vector<double>{static_cast<double>(val)});
        });

    pipeline.setSource(source);
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 1000);
    QCOMPARE(pipeline.result()->valueAt(0), 1.0);

    pipeline.setSource(source);
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 2, 1000);
    QCOMPARE(pipeline.result()->valueAt(0), 1.0);
}

void TestPipeline::pipelineInterimResult()
{
    QCPPipelineScheduler scheduler(1);

    auto source = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::vector<double>{1}, std::vector<double>{1});

    GenericPipeline pipeline(&scheduler);
    QSignalSpy spy(&pipeline, &GenericPipeline::finished);

    std::atomic<bool> gate{false};

    pipeline.setTransform(TransformKind::ViewportIndependent,
        [&gate](const QCPAbstractDataSource&, const ViewportParams&, std::any&)
            -> std::shared_ptr<QCPAbstractDataSource> {
            while (!gate.load()) QThread::msleep(5);
            return std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
                std::vector<double>{1}, std::vector<double>{42});
        });

    pipeline.setSource(source);
    QThread::msleep(20);

    pipeline.setSource(source);

    gate.store(true);

    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 2000);
    QCOMPARE(pipeline.result()->valueAt(0), 42.0);
}

void TestPipeline::pipelineDestructionWhileRunning()
{
    QCPPipelineScheduler scheduler;
    std::atomic<bool> gate{false};
    std::atomic<bool> started{false};

    {
        auto source = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
            std::vector<double>{1}, std::vector<double>{1});

        auto pipeline = std::make_unique<GenericPipeline>(&scheduler);
        pipeline->setTransform(TransformKind::ViewportIndependent,
            [&gate, &started](const QCPAbstractDataSource&, const ViewportParams&, std::any&)
                -> std::shared_ptr<QCPAbstractDataSource> {
                started.store(true);
                while (!gate.load()) QThread::msleep(5);
                return std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
                    std::vector<double>{1}, std::vector<double>{1});
            });

        pipeline->setSource(source);
        while (!started.load()) QThread::msleep(5);

        pipeline.reset();
    }

    gate.store(true);
    QThread::msleep(100);
}

void TestPipeline::multiGraphPipelinePassthrough()
{
    auto* graph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    graph->setData(std::vector<double>{1, 2, 3}, std::vector<double>{4, 5, 6});

    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QCOMPARE(graph->dataSource()->size(), 3);
}

void TestPipeline::colormap2PipelineDefault()
{
    if (!showAndHasRhi(mPlot)) QSKIP("No QRhi available — pipeline needs viewport from draw()");

    auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);

    std::vector<double> x = {0, 1, 2};
    std::vector<double> y = {0, 1};
    std::vector<double> z = {1, 2, 3, 4, 5, 6};
    cm->setData(std::move(x), std::move(y), std::move(z));

    QVERIFY(cm->pipeline().hasTransform());

    QSignalSpy spy(&cm->pipeline(), &QCPColormapPipeline::finished);
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 2000);

    QVERIFY(cm->pipeline().result() != nullptr);
}

void TestPipeline::colormap2PipelineResample()
{
    if (!showAndHasRhi(mPlot)) QSKIP("No QRhi available — pipeline needs viewport from draw()");

    auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
    mPlot->xAxis->setRange(0, 2);
    mPlot->yAxis->setRange(0, 1);

    std::vector<double> x = {0, 1, 2};
    std::vector<double> y = {0, 1};
    std::vector<double> z = {1, 2, 3, 4, 5, 6};
    cm->setData(std::move(x), std::move(y), std::move(z));

    QSignalSpy spy(&cm->pipeline(), &QCPColormapPipeline::finished);
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 2000);

    auto* result = cm->pipeline().result();
    QVERIFY(result != nullptr);
    QVERIFY(result->keySize() > 0);
    QVERIFY(result->valueSize() > 0);

    mPlot->replot(QCustomPlot::rpImmediateRefresh);
}

void TestPipeline::colormap2FirstDataAfterManyPansWithNoSourceResamplesCorrectly()
{
    // Reproducer for a slow out-of-process producer (e.g. a remote channel
    // with multi-second round trips): many axis pans can elapse before the
    // very first data ever arrives. QCPColorMap2::onViewportChanged()
    // early-returns while mDataSource is null, so none of those pans ever
    // update the pipeline's cached viewport (mLastViewport) -- it stays at
    // its zero-initialized default (plotWidthPx = plotHeightPx = 0). When
    // setDataSource() finally installs the first real data, the pipeline's
    // setSource()-triggered resample used to run against that stale
    // zero-sized viewport instead of the axes' real, current state --
    // producing a degenerate ~1px resample (or nullptr, once the zero-width
    // default key/value range no longer intersects the data) that nothing
    // ever corrected once the user stopped panning.
    if (!showAndHasRhi(mPlot)) QSKIP("No QRhi available — pipeline needs viewport from draw()");

    auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);

    // Many pans with NO data source yet -- each is a wasted onViewportChanged().
    for (int i = 0; i < 8; ++i)
    {
        mPlot->xAxis->setRange(i * 10.0, i * 10.0 + 5.0);
        QCoreApplication::processEvents();
    }

    // Settle on the final, real window -- matching the data about to arrive.
    mPlot->xAxis->setRange(70.0, 75.0);
    mPlot->yAxis->setRange(0.0, 1.0);
    QCoreApplication::processEvents();

    // First-ever data, now that the axes are settled far from their
    // construction-time default.
    std::vector<double> x = {70.0, 71.0, 72.0, 73.0, 74.0, 75.0};
    std::vector<double> y = {0.0, 1.0};
    std::vector<double> z(12, 1.0);
    cm->setData(std::move(x), std::move(y), std::move(z));

    QSignalSpy spy(&cm->pipeline(), &QCPColormapPipeline::finished);
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 2000);

    auto* result = cm->pipeline().result();
    QVERIFY2(result != nullptr,
             "first data after many pre-data pans must still resample, not silently stay null");
    QVERIFY2(result->keySize() > 1,
             "resample must use the axes' real current width, not a degenerate ~1px fallback");
    QVERIFY2(result->valueSize() > 1,
             "resample must use the axes' real current height, not a degenerate ~1px fallback");
}

void TestPipeline::colormap2DescendingYAxisResamplesNonEmpty()
{
    // Reproducer for e-Callisto (and any spectrogram whose frequency axis is
    // stored top-of-band-first): the source y is DESCENDING, e.g.
    // {116.75 .. 45}. The resampler's per-cell y-extent used to be computed
    // as halfBelow = (y[j]-y[j-1])*0.5 / halfAbove = (y[j+1]-y[j])*0.5,
    // silently assuming y is ascending. For a descending y both go negative,
    // so yLo > yHi, every target-bin range comes out inverted
    // (ranges.lo > ranges.hi), and the accumulation loop
    // `for (yb = lo; yb <= hi; ++yb)` never runs -- zero cells accumulate and
    // the whole colormap resamples to all-NaN (empty), with the odd single-bin
    // sliver where rounding collapses lo == hi. That is the "thin bars, ~0px,
    // don't always show" symptom.
    if (!showAndHasRhi(mPlot)) QSKIP("No QRhi available — pipeline needs viewport from draw()");

    auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
    mPlot->xAxis->setRange(0.0, 5.0);
    mPlot->yAxis->setRange(1.0, 5.0);
    QCoreApplication::processEvents();

    // 6 time samples x 5 DESCENDING frequency channels, all-finite z.
    std::vector<double> x = {0.0, 1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> y = {5.0, 4.0, 3.0, 2.0, 1.0};
    std::vector<double> z(x.size() * y.size(), 1.0);
    cm->setData(std::move(x), std::move(y), std::move(z));

    QSignalSpy spy(&cm->pipeline(), &QCPColormapPipeline::finished);
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 2000);

    auto* result = cm->pipeline().result();
    QVERIFY2(result != nullptr, "descending-y colormap must still produce a resample result");

    int finite = 0;
    for (int i = 0; i < result->keySize(); ++i)
        for (int j = 0; j < result->valueSize(); ++j)
            if (!std::isnan(result->cell(i, j)))
                ++finite;

    QVERIFY2(finite > 0,
             "descending-y source must accumulate cells: an all-NaN grid means the "
             "y-bin ranges came out inverted and nothing was rasterized (empty colormap)");
    // Every cell is fed z == 1.0, so a correct rasterization fills the whole
    // data extent, not just a stray sliver.
    QVERIFY2(finite >= result->keySize(),
             "a fully-populated descending-y spectrogram must fill more than a single sliver");
}

void TestPipeline::colormap2DescendingYAxisLogScaleResamplesNonEmpty()
{
    // Same descending-y bug as above but through the log-scale branch of
    // computeYBinRanges (a log frequency axis is the common way to view a
    // spectrogram). There the boundaries were yLo = sqrt(prev*yVal),
    // yHi = sqrt(yVal*next) -- for a descending y, prev > yVal > next, so
    // yLo > yHi and the range inverts the same way. Fixed by taking
    // min/max of the two geometric means.
    if (!showAndHasRhi(mPlot)) QSKIP("No QRhi available — pipeline needs viewport from draw()");

    auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
    mPlot->yAxis->setScaleType(QCPAxis::stLogarithmic);
    mPlot->xAxis->setRange(0.0, 5.0);
    mPlot->yAxis->setRange(1.0, 5.0);
    QCoreApplication::processEvents();

    std::vector<double> x = {0.0, 1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> y = {5.0, 4.0, 3.0, 2.0, 1.0};
    std::vector<double> z(x.size() * y.size(), 1.0);
    cm->setData(std::move(x), std::move(y), std::move(z));

    QSignalSpy spy(&cm->pipeline(), &QCPColormapPipeline::finished);
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 2000);

    auto* result = cm->pipeline().result();
    QVERIFY2(result != nullptr, "descending-y log-scale colormap must still produce a result");

    int finite = 0;
    for (int i = 0; i < result->keySize(); ++i)
        for (int j = 0; j < result->valueSize(); ++j)
            if (!std::isnan(result->cell(i, j)))
                ++finite;

    mPlot->yAxis->setScaleType(QCPAxis::stLinear);

    QVERIFY2(finite > 0,
             "descending-y source on a log value axis must still accumulate cells");
}

void TestPipeline::multiGraphDataFromExternalThread()
{
    auto* graph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);

    // Large enough to get the resampling transform, so the pipeline has a job to run.
    constexpr int n = 200'000;
    std::vector<double> keys(n), values(n);
    for (int i = 0; i < n; ++i) { keys[i] = i; values[i] = std::sin(i * 0.01); }
    auto source = oneColumnSource(std::move(keys), std::move(values));

    QSignalSpy spy(&graph->pipeline(), &QCPMultiGraphPipeline::finished);
    QThread thread;
    QObject worker;
    worker.moveToThread(&thread);
    thread.start();
    QMetaObject::invokeMethod(&worker, [&]{
        graph->setDataSource(source);
    }, Qt::BlockingQueuedConnection);

    QVERIFY(graph->pipeline().hasTransform());
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 5000);
    QCOMPARE(graph->dataSource()->size(), n);

    thread.quit();
    thread.wait();
}

void TestPipeline::colormap2DataFromExternalThread()
{
    auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
    mPlot->xAxis->setRange(0, 2);
    mPlot->yAxis->setRange(0, 1);
    cm->pipeline().onViewportChanged(ViewportParams{{0, 2}, {0, 1}, 400, 300, false, false});

    QSignalSpy spy(&cm->pipeline(), &QCPColormapPipeline::finished);

    auto source = std::make_shared<QCPSoADataSource2D<
        std::vector<double>, std::vector<double>, std::vector<double>>>(
        std::vector<double>{0, 1, 2},
        std::vector<double>{0, 1},
        std::vector<double>{1, 2, 3, 4, 5, 6});

    QThread thread;
    QObject worker;
    worker.moveToThread(&thread);
    thread.start();
    QMetaObject::invokeMethod(&worker, [&]{
        cm->setDataSource(source);
    }, Qt::BlockingQueuedConnection);

    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 2000);
    QVERIFY(cm->pipeline().result() != nullptr);

    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    thread.quit();
    thread.wait();
}

// A colormap made before its plot was laid out is resampled for a tiny axis rect. A resize
// changes no axis range, so the coarse image used to stay until the next pan.
void TestPipeline::colormap2ResamplesAgainAfterResize()
{
    const int nx = 400, ny = 64;
    std::vector<double> x(nx), y(ny), z(nx * ny);
    for (int i = 0; i < nx; ++i) x[i] = i;
    for (int j = 0; j < ny; ++j) y[j] = j;
    for (int k = 0; k < nx * ny; ++k) z[k] = k % 7;

    auto colormapOn = [&](QCustomPlot* plot) {
        plot->xAxis->setRange(0, nx - 1);
        plot->yAxis->setRange(0, ny - 1);
        auto* cm = new QCPColorMap2(plot->xAxis, plot->yAxis);
        cm->setDataSource(std::make_shared<QCPSoADataSource2D<
            std::vector<double>, std::vector<double>, std::vector<double>>>(x, y, z));
        return cm;
    };
    auto* cm = colormapOn(mPlot);
    QVERIFY(cm->pipeline().runSynchronously(
        ViewportParams{{0, nx - 1.0}, {0, ny - 1.0}, 2, 2, false, false}));
    QVERIFY(cm->pipeline().result()->keySize() < nx);
    QCustomPlot fresh;
    fresh.resize(mPlot->size());
    colormapOn(&fresh);

    mPlot->replot(QCustomPlot::rpImmediateRefresh); // the coarse image is on screen
    const QImage exported = mPlot->toPixmap(800, 400).toImage();
    QVERIFY(cm->pipeline().result()->keySize() >= nx);
    QCOMPARE(exported, fresh.toPixmap(800, 400).toImage());
}

static QCPColorMap2* gridOnTheSecondAxes(QCustomPlot* plot)
{
    std::vector<double> x(10), y(10), z(100, 1.0);
    for (int i = 0; i < 10; ++i)
        x[i] = y[i] = std::pow(10.0, i / 4.5);
    auto* cm = new QCPColorMap2(plot->xAxis, plot->yAxis);
    cm->setDataSource(std::make_shared<QCPSoADataSource2D<
        std::vector<double>, std::vector<double>, std::vector<double>>>(x, y, z));
    return cm;
}

// None of these draws: the axis signal alone must request the new viewport.
void TestPipeline::colormap2ResamplesForTheLogScaleOfItsKeyAxis()
{
    auto* cm = gridOnTheSecondAxes(mPlot);
    mPlot->xAxis->setRange(1, 100); // a log toggle that leaves the range alone emits no rangeChanged
    mPlot->xAxis->setScaleType(QCPAxis::stLogarithmic);
    QVERIFY(cm->pipeline().lastViewport().keyLogScale);
}

void TestPipeline::colormap2ResamplesForTheScaleOfANewKeyAxis()
{
    auto* cm = gridOnTheSecondAxes(mPlot);
    cm->setKeyAxis(mPlot->xAxis2);
    mPlot->xAxis2->setRange(1, 100);
    mPlot->xAxis2->setScaleType(QCPAxis::stLogarithmic);
    QVERIFY(cm->pipeline().lastViewport().keyLogScale);
}

void TestPipeline::colormap2ResamplesForTheScaleOfANewValueAxis()
{
    auto* cm = gridOnTheSecondAxes(mPlot);
    cm->setValueAxis(mPlot->yAxis2);
    mPlot->yAxis2->setRange(1, 100);
    mPlot->yAxis2->setScaleType(QCPAxis::stLogarithmic);
    QVERIFY(cm->pipeline().lastViewport().valueLogScale);
}

void TestPipeline::colormap2ResamplesForTheRangeOfANewKeyAxis()
{
    auto* cm = gridOnTheSecondAxes(mPlot);
    cm->setKeyAxis(mPlot->xAxis2);
    mPlot->xAxis2->setRange(2, 7);
    QCOMPARE(cm->pipeline().lastViewport().keyRange, QCPRange(2, 7));
}

void TestPipeline::colormap2ResamplesForTheRangeOfANewValueAxis()
{
    auto* cm = gridOnTheSecondAxes(mPlot);
    cm->setValueAxis(mPlot->yAxis2);
    mPlot->yAxis2->setRange(2, 7);
    QCOMPARE(cm->pipeline().lastViewport().valueRange, QCPRange(2, 7));
}

// Ranges and scales are set before the swap: no signal fires at the swap itself.
void TestPipeline::colormap2ResamplesWhenMovedToALogKeyAxis()
{
    auto* cm = gridOnTheSecondAxes(mPlot);
    mPlot->xAxis2->setRange(1, 100);
    mPlot->xAxis2->setScaleType(QCPAxis::stLogarithmic);
    cm->setKeyAxis(mPlot->xAxis2);
    QVERIFY(cm->pipeline().lastViewport().keyLogScale);
}

void TestPipeline::colormap2ResamplesWhenMovedToALogValueAxis()
{
    auto* cm = gridOnTheSecondAxes(mPlot);
    mPlot->yAxis2->setRange(1, 100);
    mPlot->yAxis2->setScaleType(QCPAxis::stLogarithmic);
    cm->setValueAxis(mPlot->yAxis2);
    QVERIFY(cm->pipeline().lastViewport().valueLogScale);
}

// One axis in both roles must not connect twice: a range change would resample twice.
void TestPipeline::colormap2OnOneAxisForBothDirectionsListensOnce()
{
    auto* cm = gridOnTheSecondAxes(mPlot);
    cm->setKeyAxis(mPlot->xAxis2);
    cm->setValueAxis(mPlot->xAxis2);
    QSignalSpy busy(&cm->pipeline(), &QCPColormapPipeline::busyChanged);
    const auto before = cm->pipeline().currentGeneration();
    mPlot->xAxis2->setRange(2, 7);
    QCOMPARE(cm->pipeline().currentGeneration() - before, uint64_t(1));
}

void TestPipeline::pipelineSourceReplacedDuringJob()
{
    QCPPipelineScheduler scheduler(1);
    std::atomic<bool> gate{false};
    std::atomic<bool> started{false};

    GenericPipeline pipeline(&scheduler);
    QSignalSpy spy(&pipeline, &GenericPipeline::finished);

    pipeline.setTransform(TransformKind::ViewportIndependent,
        [&gate, &started](const QCPAbstractDataSource& src, const ViewportParams&, std::any&)
            -> std::shared_ptr<QCPAbstractDataSource> {
            started.store(true);
            while (!gate.load()) QThread::msleep(5);
            int n = src.size();
            std::vector<double> k(n), v(n);
            for (int i = 0; i < n; ++i) { k[i] = src.keyAt(i); v[i] = src.valueAt(i); }
            return std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
                std::move(k), std::move(v));
        });

    auto source1 = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::vector<double>{1, 2, 3}, std::vector<double>{10, 20, 30});
    pipeline.setSource(source1);
    while (!started.load()) QThread::msleep(1);

    // Replace data source while job is running
    auto source2 = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::vector<double>{4, 5}, std::vector<double>{40, 50});
    source1.reset();
    pipeline.setSource(source2);

    gate.store(true);

    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 2000);
    QVERIFY(pipeline.result() != nullptr);
}

void TestPipeline::pipelineRapidFireDeliverResult()
{
    QCPPipelineScheduler scheduler(1);
    GenericPipeline pipeline(&scheduler);
    QSignalSpy spy(&pipeline, &GenericPipeline::finished);

    pipeline.setTransform(TransformKind::ViewportIndependent,
        [](const QCPAbstractDataSource& src, const ViewportParams&, std::any&)
            -> std::shared_ptr<QCPAbstractDataSource> {
            QThread::msleep(10);
            int n = src.size();
            std::vector<double> k(n), v(n);
            for (int i = 0; i < n; ++i) { k[i] = src.keyAt(i); v[i] = src.valueAt(i); }
            return std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
                std::move(k), std::move(v));
        });

    auto source = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::vector<double>{1, 2, 3}, std::vector<double>{4, 5, 6});

    for (int i = 0; i < 20; ++i)
        pipeline.setSource(source);

    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 3000);
    QVERIFY(pipeline.result() != nullptr);
    QCOMPARE(pipeline.result()->size(), 3);
}

void TestPipeline::pipelineNullSourceWhileJobRunningResyncsGeneration()
{
    // A data change that clears the source while a job for the previous data
    // is still in flight rebuilds an empty pending job (no source to run the
    // transform against). onDataChanged() already bumped mGeneration for that
    // change; nothing resynced mDisplayedGeneration once the in-flight job
    // settled, so isBusy() reported true forever (D-04).
    QCPPipelineScheduler scheduler;
    std::atomic<bool> gate{false};
    std::atomic<bool> started{false};

    auto source = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::vector<double>{1}, std::vector<double>{1});

    GenericPipeline pipeline(&scheduler);

    pipeline.setTransform(TransformKind::ViewportIndependent,
        [&gate, &started](const QCPAbstractDataSource&, const ViewportParams&, std::any&)
            -> std::shared_ptr<QCPAbstractDataSource> {
            started.store(true);
            while (!gate.load()) QThread::msleep(5);
            return std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
                std::vector<double>{1}, std::vector<double>{1});
        });

    pipeline.setSource(source);
    while (!started.load()) QThread::msleep(5);
    QVERIFY(pipeline.isBusy());

    pipeline.setSource(nullptr); // clears source while the first job is still running

    gate.store(true);
    QTRY_VERIFY_WITH_TIMEOUT(!pipeline.isBusy(), 2000);
}

void TestPipeline::schedulerDtorDropsQueuedJobs()
{
    // Jobs still queued when the scheduler dies belong to plottables that may
    // already be destroyed — the destructor must drop them, not run them
    // (waitForDone lets a finishing task's epilogue start queued work).
    std::atomic<bool> gate{false};
    std::atomic<bool> started{false};
    std::atomic<bool> queuedRan{false};
    std::thread releaser;
    {
        QCPPipelineScheduler scheduler(1);
        scheduler.submit(QCPPipelineScheduler::Heavy, [&] {
            started.store(true);
            while (!gate.load()) QThread::msleep(2);
        });
        while (!started.load()) QThread::msleep(2);
        scheduler.submit(QCPPipelineScheduler::Heavy, [&] { queuedRan.store(true); });
        releaser = std::thread([&] {
            QThread::msleep(100);
            gate.store(true);
        });
        // ~QCPPipelineScheduler runs here: drop the queued job, join the blocker
    }
    releaser.join();
    QVERIFY(!queuedRan.load());
}

void TestPipeline::colormap2QueuedJobAfterDeleteDoesNotTouchFreedMemory()
{
    // The colormap resample transform used to capture the member mGapThreshold
    // by reference; a job dequeued after removePlottable() then read freed
    // memory. Fails under ASan before the fix (silent in a plain build).
    QCPPipelineScheduler* scheduler = mPlot->pipelineScheduler();
    const int threads = scheduler->maxThreads();
    std::atomic<bool> gate{false};
    std::atomic<int> started{0};
    for (int i = 0; i < threads; ++i)
        scheduler->submit(QCPPipelineScheduler::Heavy, [&] {
            started.fetch_add(1);
            while (!gate.load()) QThread::msleep(2);
        });
    while (started.load() < threads) QThread::msleep(2);

    auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
    std::vector<double> x = {0, 1, 2};
    std::vector<double> y = {0, 1};
    std::vector<double> z = {1, 2, 3, 4, 5, 6};
    cm->setData(std::move(x), std::move(y), std::move(z)); // job queued behind blockers
    QVERIFY(mPlot->removePlottable(cm)); // deletes the colormap; its job is still queued

    gate.store(true);
    QThread::msleep(200); // let the orphaned job run (or be dropped, post-fix)
}

void TestPipeline::colormap2GapThresholdBeforeDataDoesNotStickBusy()
{
    // setGapThreshold() triggers a re-resample, but before any data source is
    // set there is no job to run: onDataChanged() must not bump the pipeline
    // generation, or isBusy() reports true until some future job delivers.
    auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
    cm->setGapThreshold(5.0);
    QVERIFY(!cm->pipeline().isBusy());
}

void TestPipeline::colormap2SetNullSourceAfterDataDoesNotStickBusy()
{
    // SciQLopColorMap::set_data's empty-input path clears an existing source
    // via setDataSource({}) (Speasy sends a "well-formed empty" spectrogram
    // for windows with no samples). onDataChanged() bumps mGeneration before
    // makeJob() can tell there is no data to build a job for; nothing resynced
    // mDisplayedGeneration for that case, so isBusy() reported true forever.
    auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
    std::vector<double> x = {0, 1, 2};
    std::vector<double> y = {0, 1};
    std::vector<double> z = {1, 2, 3, 4, 5, 6};
    cm->setData(x, y, z);
    QTRY_VERIFY_WITH_TIMEOUT(!cm->pipeline().isBusy(), 2000);

    cm->setDataSource(std::shared_ptr<QCPAbstractDataSource2D>{});

    QVERIFY(!cm->pipeline().isBusy());
}

// --- Graph resampler tests ---

void TestPipeline::graphResamplerBinMinMax()
{
    std::vector<double> keys = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    std::vector<double> vals = {5, 3, 8, 1, 7, 2, 9, 4, 6, 0};
    QCPRange keyRange(0, 9);

    const auto out = binOneColumn(keys, vals, 0, 10, keyRange, 3);

    QCOMPARE(out.keys.size(), 6u);
    QCOMPARE(out.values.size(), 6u);

    // Bin 0: keys [0,3) → vals {5,3,8} → min=3, max=8
    QCOMPARE(out.values[0], 3.0);
    QCOMPARE(out.values[1], 8.0);

    // Bin 1: keys [3,6) → vals {1,7,2} → min=1, max=7
    QCOMPARE(out.values[2], 1.0);
    QCOMPARE(out.values[3], 7.0);

    // Bin 2: keys [6,9] → vals {9,4,6,0} → min=0, max=9
    QCOMPARE(out.values[4], 0.0);
    QCOMPARE(out.values[5], 9.0);
}

void TestPipeline::graphResamplerLevel1AndLevel2()
{
    const int N = 20000;
    std::vector<double> keys(N), vals(N);
    for (int i = 0; i < N; ++i)
    {
        keys[i] = i;
        vals[i] = std::sin(i * 0.01);
    }

    QCPRange fullRange(0, N - 1);
    int l1Bins = std::max(N / 2, 1);
    auto l1 = binOneColumn(keys, vals, 0, N, fullRange, l1Bins);

    QCOMPARE(static_cast<int>(l1.keys.size()), l1Bins * 2);

    // Level 2: zoom into [5000, 10000]
    QCPRange viewport(5000, 10000);
    int l2Bins = 400 * 4;
    const std::vector<double> l1Values(l1.values.begin(), l1.values.begin() + l1.stride());
    auto l2 = binOneColumn(l1.keys, l1Values, 0, static_cast<int>(l1.keys.size()), viewport, l2Bins);

    QCOMPARE(static_cast<int>(l2.keys.size()), l2Bins * 2);

    for (size_t i = 0; i < l2.keys.size(); ++i)
    {
        QVERIFY2(l2.keys[i] >= viewport.lower - 1 && l2.keys[i] <= viewport.upper + 1,
            qPrintable(QString("L2 key[%1]=%2 outside viewport").arg(i).arg(l2.keys[i])));
    }
}

void TestPipeline::graphResamplerL2BinsRawPointsInsideBurst()
{
    // A sample every 10 s over 48 h plus a 20 s burst of 25 samples per ms: the burst holds
    // 500k raw points but only ~6 L1 bins, which used to make the graph draw them all raw.
    std::vector<double> keys;
    for (double t = 0; t < 48 * 3600.0; t += 10.0)
        keys.push_back(t);
    const double burst = 10 * 3600.0;
    for (int ms = 0; ms < 20'000; ++ms)
        keys.insert(keys.end(), 25, burst + ms * 1e-3);
    std::sort(keys.begin(), keys.end());
    std::vector<double> vals(keys.size());
    for (std::size_t i = 0; i < keys.size(); ++i)
        vals[i] = static_cast<double>(i % 1000);
    const auto src = oneColumnSource(std::move(keys), std::move(vals));

    ViewportParams vp;
    vp.keyRange = QCPRange(burst, burst + 20.0);
    vp.plotWidthPx = 1000;
    std::any cache;
    qcp::algo::buildL1CacheMulti(*src, vp, cache);
    const auto* l1 = std::any_cast<qcp::algo::MultiGraphResamplerCache>(&cache);
    QVERIFY(l1 != nullptr);
    const auto l2 = qcp::algo::resampleL2Multi(*l1, vp, *src);
    QVERIFY(l2 != nullptr);
    QVERIFY2(l2->size() > 1000, "L2 resolution must come from the raw points, not ~6 L1 bins");
    bool found = false;
    const auto range = l2->valueRange(0, found);
    QVERIFY(found);
    QCOMPARE(range.lower, 0.0);
    QCOMPARE(range.upper, 999.0);
}

// A min/max bin used to emit (min, max) whatever order its samples came in. A falling line then
// went up and down inside every bin and looked thicker than a rising one.
template <typename Source, typename ValueAt>
static int rises(const Source& s, ValueAt valueAt)
{
    int count = 0;
    for (int i = 1; i < s.size(); ++i)
        if (valueAt(s, i) > valueAt(s, i - 1))
            ++count;
    return count;
}

void TestPipeline::multiGraphL2FollowsFallingData()
{
    const int N = 1'000'000;
    std::vector<double> keys(N);
    std::vector<std::vector<double>> cols = {std::vector<double>(N), std::vector<double>(N)};
    for (int i = 0; i < N; ++i)
        keys[i] = i, cols[0][i] = -i, cols[1][i] = i;
    QCPSoAMultiDataSource<std::vector<double>, std::vector<double>> src(keys, cols);
    const auto falling = [](const QCPAbstractMultiDataSource& s, int i) { return s.valueAt(0, i); };
    const auto rising = [](const QCPAbstractMultiDataSource& s, int i) { return -s.valueAt(1, i); };

    for (const bool withOrigin : {false, true})
    {
        std::any cache;
        qcp::algo::buildL1CacheMulti(src, ViewportParams{}, cache, withOrigin);
        auto* c = std::any_cast<qcp::algo::MultiGraphResamplerCache>(&cache);
        QVERIFY(c != nullptr);
        ViewportParams vp;
        vp.plotWidthPx = 200;
        for (const auto range : {QCPRange(0, N - 1), QCPRange(1000, 1500)})
        {
            vp.keyRange = range;
            const auto l2 = qcp::algo::resampleL2Multi(*c, vp, src);
            QVERIFY(l2 != nullptr);
            QCOMPARE(rises(*l2, falling), 0);
            QCOMPARE(rises(*l2, rising), 0);
        }
    }
}

void TestPipeline::orderPairsByTrendMovesOriginsWithValues()
{
    std::vector<double> values = {5, 6, 3, 4, 1, 2};
    std::vector<int> origin = {11, 10, 21, 20, 31, 30};
    qcp::algo::orderPairsByTrend(values, origin);
    QCOMPARE(values, (std::vector<double>{6, 5, 4, 3, 2, 1}));
    QCOMPARE(origin, (std::vector<int>{10, 11, 20, 21, 30, 31}));
}

void TestPipeline::graphResamplerCacheReuse()
{
    const int N = 10000;
    std::vector<double> keys(N), vals(N);
    for (int i = 0; i < N; ++i)
    {
        keys[i] = i;
        vals[i] = std::sin(i * 0.001);
    }
    const auto src = oneColumnSource(keys, vals);

    std::any cache;
    ViewportParams vp;
    vp.keyRange = QCPRange(0, N - 1);
    vp.plotWidthPx = 100;

    qcp::algo::buildL1CacheMulti(*src, vp, cache);
    const auto* l1 = std::any_cast<qcp::algo::MultiGraphResamplerCache>(&cache);
    QVERIFY(l1 != nullptr);
    QCOMPARE(l1->sourceSize, N);
    const double* l1Keys = l1->level1.keys.data();

    // Level 2 from the cache, then a pan: another viewport, same cache.
    vp.keyRange = QCPRange(1000, 5000);
    QVERIFY(qcp::algo::resampleL2Multi(*l1, vp, *src) != nullptr);
    vp.keyRange = QCPRange(2000, 3000);
    QVERIFY(qcp::algo::resampleL2Multi(*l1, vp, *src) != nullptr);

    // The same data again keeps the cache it has.
    qcp::algo::buildL1CacheMulti(*src, vp, cache);
    const auto* again = std::any_cast<qcp::algo::MultiGraphResamplerCache>(&cache);
    QVERIFY(again != nullptr);
    QCOMPARE(again->level1.keys.data(), l1Keys);
}

void TestPipeline::graphResamplerNaNSkipped()
{
    std::vector<double> keys = {0, 1, 2, 3, 4};
    double nan = std::numeric_limits<double>::quiet_NaN();
    std::vector<double> vals = {5, nan, 3, nan, 7};

    const auto out = binOneColumn(keys, vals, 0, 5, QCPRange(0, 4), 1);

    QCOMPARE(out.values[0], 3.0);
    QCOMPARE(out.values[1], 7.0);
}

void TestPipeline::graphResamplerEmptyBinsProduceNaN()
{
    std::vector<double> keys = {0, 0.5, 1};
    std::vector<double> vals = {10, 20, 30};

    const auto out = binOneColumn(keys, vals, 0, 3, QCPRange(0, 9), 3);

    // Bin 0: has data
    QVERIFY(!std::isnan(out.values[0]));
    QVERIFY(!std::isnan(out.values[1]));

    // Bins 1 and 2: empty → NaN
    QVERIFY(std::isnan(out.values[2]));
    QVERIFY(std::isnan(out.values[3]));
    QVERIFY(std::isnan(out.values[4]));
    QVERIFY(std::isnan(out.values[5]));
}

void TestPipeline::multiGraphLineCacheRebuiltOnQueuedZoomWithBigData()
{
    // Driven the way real interaction drives it: several range changes fired in quick
    // succession, each via rpQueuedReplot (what QCPAxisRect::wheelEvent/mouseMoveEvent use),
    // with only one event-loop pump at the end -- not one synchronous replot per step.
    auto* g = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);

    const int N = 10'000'001;
    auto source = std::make_shared<SyntheticLargeMultiSource>(N, 1);

    QSignalSpy spy(&g->pipeline(), &QCPMultiGraphPipeline::finished);

    mPlot->xAxis->setRange(0, N - 1);
    mPlot->yAxis->setRange(-1, 1);
    g->setDataSource(source);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 30000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(g->mL2Result);

    const int fullRangeRecordings = g->mCore.recordingCount();
    QVERIFY(fullRangeRecordings > 0);

    double lo = 0, hi = N - 1;
    for (int i = 0; i < 6; ++i) {
        double width = (hi - lo) * 0.5;
        double center = (hi + lo) / 2.0;
        lo = center - width / 2.0;
        hi = center + width / 2.0;
        mPlot->xAxis->setRange(lo, hi);
        mPlot->replot(QCustomPlot::rpQueuedReplot);
    }
    QCoreApplication::processEvents();
    QTest::qWait(50);
    QCoreApplication::processEvents();

    const int zoomedInRecordings = g->mCore.recordingCount();
    QVERIFY2(zoomedInRecordings > fullRangeRecordings,
             "queued zoom-in did not record fresh lines");

    mPlot->xAxis->setRange(0, N - 1);
    mPlot->replot(QCustomPlot::rpQueuedReplot);
    QCoreApplication::processEvents();
    QTest::qWait(50);
    QCoreApplication::processEvents();

    QVERIFY2(g->mCore.recordingCount() > zoomedInRecordings,
             "queued zoom-out did not record fresh lines");
}

void TestPipeline::multiGraphLargeToSmallDataFallback()
{
    // When a graph that previously had a large dataset (with resampling transform)
    // receives a smaller dataset, the transform should be cleared so the raw
    // data source is used directly — no nullptr result, no blank rendering.
    auto* g = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);

    // Install the resampling transform (simulates prior large-data assignment)
    g->pipeline().setTransform(TransformKind::ViewportDependent,
        [](const QCPAbstractMultiDataSource& src, const ViewportParams& vp, std::any& cache)
            -> std::shared_ptr<QCPAbstractMultiDataSource> {
            return qcp::algo::buildL1CacheMulti(src, vp, cache);
        });
    QVERIFY(g->pipeline().hasTransform());

    // Assign small data — transform should be cleared
    g->setData(std::vector<double>{1, 2, 3}, std::vector<double>{4, 5, 6});

    // Transform is removed; pipeline passes through raw data source
    QVERIFY(!g->pipeline().hasTransform());

    // result() returns the raw source directly (no transform = passthrough)
    auto* result = g->pipeline().result();
    QVERIFY(result);
    QCOMPARE(result->size(), 3);

    // draw() should render normally and not crash
    mPlot->xAxis->setRange(0, 4);
    mPlot->yAxis->setRange(0, 7);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QVERIFY(g->dataSource());
    QCOMPARE(g->dataSource()->size(), 3);
}

void TestPipeline::graphResamplerBinMinMaxKeyPositions()
{
    // Bug: comment says output is (binCenter, min), (binCenter+halfWidth, max)
    // but code writes binCenter + halfWidth * 0.5 (quarter-width offset)
    std::vector<double> keys = {0, 1, 2, 3};
    std::vector<double> vals = {10, 20, 30, 40};

    const auto out = binOneColumn(keys, vals, 0, 4, QCPRange(0, 4), 1);

    // Single bin: binWidth=4, halfWidth=2, binCenter=2
    // Expected: (binCenter, min) = (2, 10), (binCenter+halfWidth, max) = (4, 40)
    QCOMPARE(out.keys[0], 2.0);
    QCOMPARE(out.keys[1], 4.0);
}

void TestPipeline::graphResamplerBinMinMaxZeroBins()
{
    // Bug: numBins=0 causes division by zero in binWidth = keyRange.size() / numBins
    std::vector<double> keys = {1, 2, 3};
    std::vector<double> vals = {4, 5, 6};

    const auto result = binOneColumn(keys, vals, 0, 3, QCPRange(1, 3), 0);
    QCOMPARE(result.keys.size(), 0u);
    QCOMPARE(result.values.size(), 0u);
}

void TestPipeline::graphResamplerNonFiniteKeysSkipped()
{
    // Bug: non-finite keys (NaN/Inf) produce UB in static_cast<int>((k - keyLo) / binWidth)
    double nan = std::numeric_limits<double>::quiet_NaN();
    double inf = std::numeric_limits<double>::infinity();
    std::vector<double> keys = {0, nan, 2, inf, 4};
    std::vector<double> vals = {10, 20, 30, 40, 50};

    const auto out = binOneColumn(keys, vals, 0, 5, QCPRange(0, 4), 1);

    // Only finite keys (0, 2, 4) should contribute → min=10, max=50
    QCOMPARE(out.values[0], 10.0);
    QCOMPARE(out.values[1], 50.0);
}

// --- Multi-column resampler tests ---

void TestPipeline::multiGraphBinMinMaxMulti()
{
    // 3 columns, 6 data points, 3 bins
    // Keys: 0,1,2,3,4,5  →  bin0=[0,1], bin1=[2,3], bin2=[4,5]
    std::vector<double> keys = {0, 1, 2, 3, 4, 5};
    std::vector<std::vector<double>> cols = {
        {10, 20, 30, 40, 50, 60},  // col 0
        {60, 50, 40, 30, 20, 10},  // col 1
        {1, 1, 1, 1, 1, 1}         // col 2 (constant)
    };

    auto src = std::make_shared<QCPSoAMultiDataSource<
        std::vector<double>, std::vector<double>>>(keys, cols);

    QCPRange keyRange(0, 6);
    auto result = qcp::algo::binMinMaxMulti(*src, 0, 6, keyRange, 3);

    QCOMPARE(result.numColumns, 3);
    QCOMPARE(static_cast<int>(result.keys.size()), 6); // 3 bins * 2

    int s = result.stride();
    QCOMPARE(s, 6);

    // Col 0, bin 0: min=10, max=20
    QCOMPARE(result.values[0 * s + 0], 10.0);
    QCOMPARE(result.values[0 * s + 1], 20.0);
    // Col 0, bin 1: min=30, max=40
    QCOMPARE(result.values[0 * s + 2], 30.0);
    QCOMPARE(result.values[0 * s + 3], 40.0);

    // Col 1, bin 0: min=50, max=60
    QCOMPARE(result.values[1 * s + 0], 50.0);
    QCOMPARE(result.values[1 * s + 1], 60.0);
    // Col 1, bin 2: min=10, max=20
    QCOMPARE(result.values[1 * s + 4], 10.0);
    QCOMPARE(result.values[1 * s + 5], 20.0);

    // Col 2: all bins should be (1,1)
    for (int b = 0; b < 3; ++b) {
        QCOMPARE(result.values[2 * s + b * 2], 1.0);
        QCOMPARE(result.values[2 * s + b * 2 + 1], 1.0);
    }
}

void TestPipeline::multiGraphBinMinMaxMultiNaN()
{
    std::vector<double> keys = {0, 1, 2, 3};
    std::vector<std::vector<double>> cols = {
        {10, std::numeric_limits<double>::quiet_NaN(), 30, 40},
        {1, 2, 3, 4}
    };
    auto src = std::make_shared<QCPSoAMultiDataSource<
        std::vector<double>, std::vector<double>>>(keys, cols);

    auto result = qcp::algo::binMinMaxMulti(*src, 0, 4, QCPRange(0, 4), 2);
    int s = result.stride();

    // Col 0, bin 0: NaN at index 1 should be skipped → min=max=10
    QCOMPARE(result.values[0 * s + 0], 10.0);
    QCOMPARE(result.values[0 * s + 1], 10.0);
}

void TestPipeline::multiGraphBinMinMaxMultiParallelMatchesSingleThreaded()
{
    const int N = 1'100'000;
    const int cols = 3;
    std::vector<double> keys(N);
    std::vector<std::vector<double>> valueCols(cols, std::vector<double>(N));
    for (int i = 0; i < N; ++i) {
        keys[i] = static_cast<double>(i);
        for (int c = 0; c < cols; ++c)
            valueCols[c][i] = std::sin(i * 0.01 + c);
    }
    auto src = std::make_shared<QCPSoAMultiDataSource<
        std::vector<double>, std::vector<double>>>(keys, valueCols);

    QCPRange range(0, N);
    int numBins = 1000;

    auto single = qcp::algo::binMinMaxMulti(*src, 0, N, range, numBins);
    auto parallel = qcp::algo::binMinMaxMultiParallel(*src, 0, N, range, numBins);

    QCOMPARE(parallel.numColumns, single.numColumns);
    QCOMPARE(parallel.keys.size(), single.keys.size());
    QCOMPARE(parallel.values.size(), single.values.size());

    for (size_t i = 0; i < single.values.size(); ++i)
    {
        if (std::isnan(single.values[i]))
            QVERIFY(std::isnan(parallel.values[i]));
        else
            QCOMPARE(parallel.values[i], single.values[i]);
    }
}

// --- L2 lazy rebuild tests ---

void TestPipeline::multiGraphL2RebuildDeferredToDraw()
{
    // Verify that changing the viewport does NOT immediately rebuild L2,
    // but that L2 IS rebuilt when draw() runs (via replot).
    auto* g = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    const int N = 10'000'001;
    auto source = std::make_shared<SyntheticLargeMultiSource>(N, 1);

    QSignalSpy spy(&g->pipeline(), &QCPMultiGraphPipeline::finished);
    mPlot->xAxis->setRange(0, N - 1);
    mPlot->yAxis->setRange(-1, 1);
    g->setDataSource(source);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    // Wait for L1 build
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 30000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(g->mL1Cache);
    QVERIFY(g->mL2Result);

    auto oldL2 = g->mL2Result;

    // Change viewport — L2 should NOT be rebuilt yet (just flagged dirty)
    mPlot->xAxis->setRange(0, N / 2);
    QVERIFY(g->mL2Dirty);
    QCOMPARE(g->mL2Result, oldL2); // still the old L2

    // Now replot — draw() should rebuild L2
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(!g->mL2Dirty);
    QVERIFY(g->mL2Result);
    QVERIFY(g->mL2Result != oldL2); // rebuilt with new viewport
}

void TestPipeline::multiGraphL2CoalescesMultipleViewportChanges()
{
    // Simulate rapid pan: many range changes, one replot.
    // L2 should only be built once (at draw time), not per range change.
    auto* g = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    const int N = 10'000'001;
    auto source = std::make_shared<SyntheticLargeMultiSource>(N, 1);

    QSignalSpy spy(&g->pipeline(), &QCPMultiGraphPipeline::finished);
    mPlot->xAxis->setRange(0, N - 1);
    mPlot->yAxis->setRange(-1, 1);
    g->setDataSource(source);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 30000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(g->mL2Result);

    auto oldL2 = g->mL2Result;

    // Simulate 10 rapid viewport changes (like 10 mouse move events during pan)
    for (int i = 0; i < 10; ++i)
    {
        double offset = i * 1000.0;
        mPlot->xAxis->setRange(offset, offset + N / 2);
    }

    // L2 should still be the old one — dirty flag set but not yet rebuilt
    QVERIFY(g->mL2Dirty);
    QCOMPARE(g->mL2Result, oldL2);

    // Single replot rebuilds L2 once with the final viewport
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(!g->mL2Dirty);
    QVERIFY(g->mL2Result != oldL2);
}

void TestPipeline::multiGraphL2DirtyAfterL1Ready()
{
    // When L1 finishes, L2 should be marked dirty (not built inline).
    auto* g = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    const int N = 10'000'001;
    auto source = std::make_shared<SyntheticLargeMultiSource>(N, 1);

    mPlot->xAxis->setRange(0, N - 1);
    mPlot->yAxis->setRange(-1, 1);

    QSignalSpy spy(&g->pipeline(), &QCPMultiGraphPipeline::finished);
    g->setDataSource(source);

    // Before L1 finishes: no L1 cache, no L2, not dirty
    QVERIFY(!g->mL1Cache);
    QVERIFY(!g->mL2Result);
    QVERIFY(!g->mL2Dirty);

    // Wait for L1
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 30000);
    // Process the queued replot from onL1Ready
    QCoreApplication::processEvents();

    // After L1 delivery: L1 cache exists, L2 dirty flag was set,
    // and the queued replot should have rebuilt L2
    QVERIFY(g->mL1Cache);
    QVERIFY(g->mL2Result);
    QVERIFY(!g->mL2Dirty); // cleared by draw()
}

// --- Histogram binner tests ---

void TestPipeline::bin2dBasicCounts()
{
    std::vector<double> keys = {0.25, 0.25, 0.75, 0.75};
    std::vector<double> vals = {0.25, 0.75, 0.25, 0.75};
    auto src = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::move(keys), std::move(vals));
    auto* result = qcp::algo::bin2d(*src, 2, 2);
    QVERIFY(result);
    QCOMPARE(result->keySize(), 2);
    QCOMPARE(result->valueSize(), 2);
    QCOMPARE(result->cell(0, 0), 1.0);
    QCOMPARE(result->cell(1, 0), 1.0);
    QCOMPARE(result->cell(0, 1), 1.0);
    QCOMPARE(result->cell(1, 1), 1.0);
    delete result;
}

void TestPipeline::bin2dNaNSkipped()
{
    std::vector<double> keys = {0.5, std::nan(""), 0.5};
    std::vector<double> vals = {0.5, 0.5, std::nan("")};
    auto src = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::move(keys), std::move(vals));
    auto* result = qcp::algo::bin2d(*src, 1, 1);
    QVERIFY(result);
    QCOMPARE(result->cell(0, 0), 1.0);
    delete result;
}

void TestPipeline::bin2dEmptyInput()
{
    std::vector<double> keys, vals;
    auto src = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::move(keys), std::move(vals));
    auto* result = qcp::algo::bin2d(*src, 10, 10);
    QVERIFY(!result);
}

void TestPipeline::bin2dSingleBin()
{
    std::vector<double> keys = {1.0, 1.0, 1.0};
    std::vector<double> vals = {2.0, 2.0, 2.0};
    auto src = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::move(keys), std::move(vals));
    auto* result = qcp::algo::bin2d(*src, 1, 1);
    QVERIFY(result);
    QCOMPARE(result->cell(0, 0), 3.0);
    delete result;
}

// Histogram input is scattered, NOT sorted by key. The bin range must be the
// true min/max of the keys, not the first/last sample. Here the min (1.0) and
// max (9.0) are interior; the endpoints are both 2.0.
void TestPipeline::bin2dUnsortedKeyRange()
{
    std::vector<double> keys = {2.0, 1.0, 9.0, 2.0};
    std::vector<double> vals = {0.0, 0.0, 0.0, 0.0};
    auto src = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::move(keys), std::move(vals));
    auto* result = qcp::algo::bin2d(*src, 4, 1);
    QVERIFY(result);

    // The grid must span the real key extent [1, 9], not the endpoints [2, 2].
    QCOMPARE(result->keyRange().lower, 1.0);
    QCOMPARE(result->keyRange().upper, 9.0);

    // With binWidth = 8/4 = 2: keys 2,1,2 fall in column 0, key 9 clamps to 3.
    QCOMPARE(result->cell(0, 0), 3.0);
    QCOMPARE(result->cell(1, 0), 0.0);
    QCOMPARE(result->cell(2, 0), 0.0);
    QCOMPARE(result->cell(3, 0), 1.0);
    delete result;
}

// A non-finite sample at the first/last key position must not poison the range.
// The finite points must still bin against the finite min/max.
void TestPipeline::bin2dNaNAtKeyEndpoint()
{
    const double nan = std::nan("");
    std::vector<double> keys = {nan, 2.0, 6.0, nan};
    std::vector<double> vals = {1.0, 0.0, 0.0, 1.0};
    auto src = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::move(keys), std::move(vals));
    auto* result = qcp::algo::bin2d(*src, 2, 1);
    QVERIFY(result);

    // Range derives only from finite keys: [2, 6].
    QCOMPARE(result->keyRange().lower, 2.0);
    QCOMPARE(result->keyRange().upper, 6.0);

    // binWidth = 4/2 = 2: key 2 -> col 0, key 6 -> clamps to col 1. NaN dropped.
    QCOMPARE(result->cell(0, 0), 1.0);
    QCOMPARE(result->cell(1, 0), 1.0);
    delete result;
}

// All keys non-finite: no finite sample to bin, so no grid.
void TestPipeline::bin2dAllNaNNoGrid()
{
    const double nan = std::nan("");
    std::vector<double> keys = {nan, nan, nan};
    std::vector<double> vals = {1.0, 2.0, 3.0};
    auto src = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::move(keys), std::move(vals));
    auto* result = qcp::algo::bin2d(*src, 4, 4);
    QVERIFY(!result);
}

// Log key binning: bin edges are evenly spaced in log space, so a decade of
// keys lands one-per-bin instead of all piling into the first linear bin.
void TestPipeline::bin2dLogKeyBinning()
{
    std::vector<double> keys = {1.0, 10.0, 100.0, 1000.0};
    std::vector<double> vals = {5.0, 5.0, 5.0, 5.0};
    auto src = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::move(keys), std::move(vals));
    // keyLog = true, valueLog = false
    auto* result = qcp::algo::bin2d(*src, 3, 1, true, false);
    QVERIFY(result);

    // Grid keeps the linear coordinate extent [1, 1000]; the renderer's
    // uniform-pixel stretch turns these log-spaced cells into log positions.
    QCOMPARE(result->keyRange().lower, 1.0);
    QCOMPARE(result->keyRange().upper, 1000.0);

    // log10 range [0, 3], width/3 per bin: 1->bin0, 10->bin1, 100->bin2,
    // 1000 (log 3) clamps to bin2.
    QCOMPARE(result->cell(0, 0), 1.0);
    QCOMPARE(result->cell(1, 0), 1.0);
    QCOMPARE(result->cell(2, 0), 2.0);
    delete result;
}

// On a log axis, non-positive keys can't be placed and are dropped like NaN;
// the range derives from the positive keys only.
void TestPipeline::bin2dLogDropsNonPositive()
{
    std::vector<double> keys = {-5.0, 1.0, 10.0, 100.0};
    std::vector<double> vals = {5.0, 5.0, 5.0, 5.0};
    auto src = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::move(keys), std::move(vals));
    auto* result = qcp::algo::bin2d(*src, 2, 1, true, false);
    QVERIFY(result);

    // -5 dropped: range is the positive extent [1, 100].
    QCOMPARE(result->keyRange().lower, 1.0);
    QCOMPARE(result->keyRange().upper, 100.0);

    // log10 range [0, 2], width/2 per bin: 1->bin0, 10->bin1, 100->clamp bin1.
    QCOMPARE(result->cell(0, 0), 1.0);
    QCOMPARE(result->cell(1, 0), 2.0);
    delete result;
}

// All keys non-positive on a log axis: nothing to bin, so no grid.
void TestPipeline::bin2dLogAllNonPositiveNoGrid()
{
    std::vector<double> keys = {-1.0, -2.0, 0.0};
    std::vector<double> vals = {1.0, 2.0, 3.0};
    auto src = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::move(keys), std::move(vals));
    auto* result = qcp::algo::bin2d(*src, 4, 4, true, false);
    QVERIFY(!result);
}

// --- QCPHistogram2D tests ---

// Axis auto-scaling asks the plottable for its key range. Histogram keys are
// scattered, not sorted, so it must report the true min/max, not first/last.
void TestPipeline::histogram2dKeyRangeUnsorted()
{
    auto* hist = new QCPHistogram2D(mPlot->xAxis, mPlot->yAxis);
    std::vector<double> keys = {5.0, 1.0, 9.0, 2.0}; // front=5, back=2; min=1, max=9
    std::vector<double> vals = {0.0, 0.0, 0.0, 0.0};
    hist->setData(std::move(keys), std::move(vals));

    bool found = false;
    QCPRange kr = hist->getKeyRange(found);
    QVERIFY(found);
    QCOMPARE(kr.lower, 1.0);
    QCOMPARE(kr.upper, 9.0);
}

void TestPipeline::histogram2dValueRangeRestrictedUnsortedKeys()
{
    // Histogram keys are scattered: a key-restricted value range must NOT use
    // the sorted-keys binary-search window (that's only valid for graphs).
    auto* hist = new QCPHistogram2D(mPlot->xAxis, mPlot->yAxis);
    std::vector<double> keys = {5.0, 1.0, 9.0, 2.0, 7.0};
    std::vector<double> vals = {50.0, 10.0, 90.0, 20.0, 70.0};
    hist->setData(std::move(keys), std::move(vals));

    bool found = false;
    QCPRange vr = hist->getValueRange(found, QCP::sdBoth, QCPRange(1.5, 6.0));
    QVERIFY(found);
    QCOMPARE(vr.lower, 20.0); // keys 5.0 and 2.0 are inside [1.5, 6.0]
    QCOMPARE(vr.upper, 50.0);
}

void TestPipeline::histogram2dSelectTestReflectsNewData()
{
    // Bounds used by selectTest are memoized per data generation: a data
    // change must invalidate the cache, not keep answering for the old data.
    mPlot->xAxis->setRange(0, 100);
    mPlot->yAxis->setRange(0, 100);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    auto* hist = new QCPHistogram2D(mPlot->xAxis, mPlot->yAxis);
    hist->setData(std::vector<double>{10.0, 20.0}, std::vector<double>{10.0, 20.0});

    const QPointF inside_old(mPlot->xAxis->coordToPixel(15.0),
                             mPlot->yAxis->coordToPixel(15.0));
    QVERIFY(hist->selectTest(inside_old, false) >= 0);
    QVERIFY(hist->selectTest(inside_old, false) >= 0); // cached round

    hist->setData(std::vector<double>{60.0, 80.0}, std::vector<double>{60.0, 80.0});
    const QPointF inside_new(mPlot->xAxis->coordToPixel(70.0),
                             mPlot->yAxis->coordToPixel(70.0));
    QVERIFY(hist->selectTest(inside_old, false) < 0);  // stale cache would say yes
    QVERIFY(hist->selectTest(inside_new, false) >= 0);
}

void TestPipeline::histogram2dPipelineBins()
{
    auto* hist = new QCPHistogram2D(mPlot->xAxis, mPlot->yAxis);
    std::vector<double> keys, vals;
    for (int i = 0; i < 1000; ++i) {
        keys.push_back(i * 0.01);
        vals.push_back(std::sin(i * 0.01));
    }
    hist->setBins(10, 20);
    hist->setData(std::move(keys), std::move(vals));

    QSignalSpy spy(&hist->pipeline(), &QCPHistogramPipeline::finished);
    QVERIFY(spy.wait(2000));

    auto* result = hist->pipeline().result();
    QVERIFY(result);
    QCOMPARE(result->keySize(), 10);
    QCOMPARE(result->valueSize(), 20);
}

void TestPipeline::histogram2dBinCountsDefaultAndSettable()
{
    auto* hist = new QCPHistogram2D(mPlot->xAxis, mPlot->yAxis);
    QCOMPARE(hist->keyBins(), 100);
    QCOMPARE(hist->valueBins(), 100);
    QCOMPARE(hist->normalization(), QCP::HistogramNormalization::Count);
    hist->setBins(3, 4);
    QCOMPARE(hist->keyBins(), 3);
    QCOMPARE(hist->valueBins(), 4);
}

void TestPipeline::histogram2dSaysReady()
{
    auto* hist = new QCPHistogram2D(mPlot->xAxis, mPlot->yAxis);
    hist->setBins(4, 4);
    QSignalSpy ready(hist, &QCPHistogram2D::histogramReady);
    std::vector<double> keys = {0.5, 1.5, 2.5, 3.5};
    std::vector<double> vals = {0.5, 1.5, 2.5, 3.5};
    hist->setData(std::move(keys), std::move(vals));
    QVERIFY(ready.wait(2000));
    QCOMPARE(ready.count(), 1); // one binning, one signal
}

void TestPipeline::histogram2dBinsFollowALogKeyAxis()
{
    auto* hist = new QCPHistogram2D(mPlot->xAxis, mPlot->yAxis);
    hist->setBins(3, 1);
    mPlot->xAxis->setScaleType(QCPAxis::stLogarithmic);

    std::vector<double> keys = {1.0, 10.0, 100.0, 1000.0};
    std::vector<double> vals = {5.0, 5.0, 5.0, 5.0};
    hist->setData(std::move(keys), std::move(vals));

    QSignalSpy spy(&hist->pipeline(), &QCPHistogramPipeline::finished);
    QVERIFY(spy.wait(2000));

    auto* data = hist->pipeline().result();
    QVERIFY(data);
    // Log key bins spread the decade one-per-bin (vs all in bin 0 if linear).
    QCOMPARE(data->cell(0, 0), 1.0);
    QCOMPARE(data->cell(1, 0), 1.0);
    QCOMPARE(data->cell(2, 0), 2.0);
}

// One normalization rule for both histograms: the same sample, the same enum, the same meaning.
void TestPipeline::bothHistogramsShareTheNormalization()
{
    // 100 samples: keys alternate 0.5/1.5, values 1.0 for the first 50 and 3.0 for the rest.
    std::vector<double> keys(100), vals(100);
    for (std::size_t i = 0; i < keys.size(); ++i)
    {
        keys[i] = i % 2 ? 1.5 : 0.5;
        vals[i] = i < 50 ? 1.0 : 3.0;
    }

    auto* hist2d = new QCPHistogram2D(mPlot->xAxis, mPlot->yAxis);
    hist2d->setBins(2, 2);
    hist2d->setData(keys, vals);
    QSignalSpy ready2d(hist2d, &QCPHistogram2D::histogramReady);
    QVERIFY(ready2d.wait(2000));
    // Keys 0.5..1.5 in two bins, values 1..3 in two: 25 per cell, cell area 0.5 * 1.
    QVERIFY(hist2d->shown());
    QCOMPARE(hist2d->shown()->cell(0, 0), 25.0);
    hist2d->setNormalization(QCP::HistogramNormalization::Probability);
    QCOMPARE(hist2d->shown()->cell(0, 0), 0.25);
    hist2d->setNormalization(QCP::HistogramNormalization::Density);
    QCOMPARE(hist2d->shown()->cell(0, 0), 0.5);

    auto* hist1d = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    hist1d->setBins(2);
    QSignalSpy ready1d(hist1d, &QCPHistogram::histogramReady);
    // The 1-D histogram bins its value columns: the samples are the keys of the 2-D sample.
    std::vector<double> index(keys.size());
    for (std::size_t i = 0; i < index.size(); ++i)
        index[i] = double(i);
    hist1d->setDataSource(std::make_shared<QCPSoAMultiDataSource<std::vector<double>, std::vector<double>>>(
        index, std::vector<std::vector<double>> { keys }));
    QTRY_COMPARE_WITH_TIMEOUT(hist1d->histogram() ? hist1d->histogram()->heights[0] : std::vector<double>(),
                              (std::vector<double> { 50, 50 }), 5000);
    hist1d->setNormalization(QCP::HistogramNormalization::Probability);
    QTRY_COMPARE_WITH_TIMEOUT(hist1d->histogram()->heights[0], (std::vector<double> { 0.5, 0.5 }), 5000);
    hist1d->setNormalization(QCP::HistogramNormalization::Density);
    QTRY_COMPARE_WITH_TIMEOUT(hist1d->histogram()->heights[0], (std::vector<double> { 1, 1 }), 5000);
}

// The core of "changing the scale changes the look": toggling the key axis
// between linear and log after data is set must re-bin, since the renderer can
// only stretch cells uniformly and cannot redistribute them itself.
void TestPipeline::histogram2dAxisScaleTogglesRebinning()
{
    auto* hist = new QCPHistogram2D(mPlot->xAxis, mPlot->yAxis);
    hist->setBins(3, 1);
    std::vector<double> keys = {1.0, 10.0, 100.0, 1000.0};
    std::vector<double> vals = {5.0, 5.0, 5.0, 5.0};
    hist->setData(std::move(keys), std::move(vals));

    QSignalSpy spy(&hist->pipeline(), &QCPHistogramPipeline::finished);
    QVERIFY(spy.wait(2000));
    auto* lin = hist->pipeline().result();
    QVERIFY(lin);
    // Linear range [1, 1000], binWidth 333: 1/10/100 -> bin 0, 1000 -> bin 2.
    QCOMPARE(lin->cell(0, 0), 3.0);
    QCOMPARE(lin->cell(2, 0), 1.0);

    // Toggle the key axis to log -> the histogram must re-bin in log space.
    QSignalSpy spy2(&hist->pipeline(), &QCPHistogramPipeline::finished);
    mPlot->xAxis->setScaleType(QCPAxis::stLogarithmic);
    QVERIFY(spy2.wait(2000));
    auto* log = hist->pipeline().result();
    QVERIFY(log);
    // log10 range [0, 3]: 1->0, 10->1, 100->2, 1000 clamps to 2.
    QCOMPARE(log->cell(0, 0), 1.0);
    QCOMPARE(log->cell(1, 0), 1.0);
    QCOMPARE(log->cell(2, 0), 2.0);
}

namespace
{
// Four samples a decade apart, in 3 bins: linear puts 3 in bin 0, log spreads them.
QCPHistogram2D* decadeHistogram(QCustomPlot* plot, bool alongKeys)
{
    auto* hist = new QCPHistogram2D(plot->xAxis, plot->yAxis);
    std::vector<double> decades = { 1.0, 10.0, 100.0, 1000.0 };
    std::vector<double> flat(4, 5.0);
    hist->setBins(alongKeys ? 3 : 1, alongKeys ? 1 : 3);
    if (alongKeys)
        hist->setData(std::move(decades), std::move(flat));
    else
        hist->setData(std::move(flat), std::move(decades));
    return hist;
}

bool spreadOverDecades(const QCPColorMapData* data, bool alongKeys)
{
    const auto cell = [&](int i) { return alongKeys ? data->cell(i, 0) : data->cell(0, i); };
    return data && cell(0) == 1.0 && cell(1) == 1.0 && cell(2) == 2.0;
}
} // namespace

// Moving onto an axis that is already log sends no scaleTypeChanged: draw() notices the scale.
void TestPipeline::histogram2dRebinsForTheScaleOfANewKeyAxis()
{
    auto* hist = decadeHistogram(mPlot, true);
    QSignalSpy spy(&hist->pipeline(), &QCPHistogramPipeline::finished);
    QVERIFY(spy.wait(2000));
    QVERIFY(!spreadOverDecades(hist->pipeline().result(), true));
    mPlot->xAxis2->setScaleType(QCPAxis::stLogarithmic);
    hist->setKeyAxis(mPlot->xAxis2);
    mPlot->replot();
    QTRY_VERIFY_WITH_TIMEOUT(spreadOverDecades(hist->pipeline().result(), true), 2000);
}

void TestPipeline::histogram2dRebinsForTheScaleOfANewValueAxis()
{
    auto* hist = decadeHistogram(mPlot, false);
    QSignalSpy spy(&hist->pipeline(), &QCPHistogramPipeline::finished);
    QVERIFY(spy.wait(2000));
    QVERIFY(!spreadOverDecades(hist->pipeline().result(), false));
    mPlot->yAxis2->setScaleType(QCPAxis::stLogarithmic);
    hist->setValueAxis(mPlot->yAxis2);
    mPlot->replot();
    QTRY_VERIFY_WITH_TIMEOUT(spreadOverDecades(hist->pipeline().result(), false), 2000);
}

// No draw is asked for: a scale change that leaves the range alone does not replot.
void TestPipeline::histogram2dRebinsForANewKeyAxisScaleWithoutADraw()
{
    auto* hist = decadeHistogram(mPlot, true);
    QSignalSpy spy(&hist->pipeline(), &QCPHistogramPipeline::finished);
    QVERIFY(spy.wait(2000));
    hist->setKeyAxis(mPlot->xAxis2);
    mPlot->xAxis2->setRange(1, 1000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTest::qWait(50);
    mPlot->xAxis2->setScaleType(QCPAxis::stLogarithmic);
    QTRY_VERIFY_WITH_TIMEOUT(spreadOverDecades(hist->pipeline().result(), true), 2000);
}

void TestPipeline::histogram2dRebinsForANewValueAxisScaleWithoutADraw()
{
    auto* hist = decadeHistogram(mPlot, false);
    QSignalSpy spy(&hist->pipeline(), &QCPHistogramPipeline::finished);
    QVERIFY(spy.wait(2000));
    hist->setValueAxis(mPlot->yAxis2);
    mPlot->yAxis2->setRange(1, 1000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTest::qWait(50);
    mPlot->yAxis2->setScaleType(QCPAxis::stLogarithmic);
    QTRY_VERIFY_WITH_TIMEOUT(spreadOverDecades(hist->pipeline().result(), false), 2000);
}

// The axis the histogram left is someone else's now: toggling its scale must not re-bin.
void TestPipeline::histogram2dIgnoresTheScaleOfTheKeyAxisItLeft()
{
    auto* hist = decadeHistogram(mPlot, true);
    QSignalSpy finished(&hist->pipeline(), &QCPHistogramPipeline::finished);
    QVERIFY(finished.wait(2000));
    hist->setKeyAxis(mPlot->xAxis2);
    QSignalSpy busy(&hist->pipeline(), &QCPHistogramPipeline::busyChanged);
    mPlot->xAxis->setScaleType(QCPAxis::stLogarithmic);
    QCOMPARE(busy.count(), 0);
}

void TestPipeline::histogram2dIgnoresTheScaleOfTheValueAxisItLeft()
{
    auto* hist = decadeHistogram(mPlot, false);
    QSignalSpy finished(&hist->pipeline(), &QCPHistogramPipeline::finished);
    QVERIFY(finished.wait(2000));
    hist->setValueAxis(mPlot->yAxis2);
    QSignalSpy busy(&hist->pipeline(), &QCPHistogramPipeline::busyChanged);
    mPlot->yAxis->setScaleType(QCPAxis::stLogarithmic);
    QCOMPARE(busy.count(), 0);
}

// Bins land in the background, after the replot that asked for them: they repaint on their own.
void TestPipeline::histogram2dNewBinsAreRepaintedWhenTheyLand()
{
    auto* hist = decadeHistogram(mPlot, true);
    QSignalSpy finished(&hist->pipeline(), &QCPHistogramPipeline::finished);
    QVERIFY(finished.wait(2000));
    QTest::qWait(20);
    QSignalSpy replotted(mPlot, &QCustomPlot::afterReplot);
    hist->setData(std::vector<double>{1.0, 2.0}, std::vector<double>{5.0, 5.0});
    QVERIFY(finished.wait(2000));
    QTRY_VERIFY_WITH_TIMEOUT(replotted.count() > 0, 2000);
}

// Nothing may emit during a paint: the re-bin draw() asks for runs from the event loop.
void TestPipeline::histogram2dRebinForANewScaleEmitsNothingDuringPaint()
{
    auto* hist = decadeHistogram(mPlot, true);
    QSignalSpy finished(&hist->pipeline(), &QCPHistogramPipeline::finished);
    QVERIFY(finished.wait(2000));
    hist->setKeyAxis(mPlot->xAxis2);
    mPlot->xAxis2->setScaleType(QCPAxis::stLogarithmic);
    QSignalSpy busy(&hist->pipeline(), &QCPHistogramPipeline::busyChanged);
    mPlot->replot();
    QCOMPARE(busy.count(), 0);
    QTRY_VERIFY_WITH_TIMEOUT(spreadOverDecades(hist->pipeline().result(), true), 2000);
}

// Export does not pump the event loop: the re-bin started by the scale change cannot land.
void TestPipeline::histogram2dExportRightAfterAScaleChangeShowsTheNewBins()
{
    auto* hist = decadeHistogram(mPlot, true);
    QSignalSpy finished(&hist->pipeline(), &QCPHistogramPipeline::finished);
    QVERIFY(finished.wait(2000));
    mPlot->xAxis->setScaleType(QCPAxis::stLogarithmic);
    QVERIFY(!mPlot->toPixmap(400, 300).isNull());
    QVERIFY(spreadOverDecades(hist->pipeline().result(), true));
}

void TestPipeline::histogram2dExportRightAfterANewAxisShowsItsScale()
{
    auto* hist = decadeHistogram(mPlot, true);
    QSignalSpy finished(&hist->pipeline(), &QCPHistogramPipeline::finished);
    QVERIFY(finished.wait(2000));
    hist->setKeyAxis(mPlot->xAxis2);
    mPlot->xAxis2->setScaleType(QCPAxis::stLogarithmic);
    QVERIFY(!mPlot->toPixmap(400, 300).isNull());
    QVERIFY(spreadOverDecades(hist->pipeline().result(), true));
}

// The export re-binned for the new scale: the re-bin queued by the paint before it is moot.
void TestPipeline::histogram2dQueuedRebinIsDroppedAfterAnExportRebinned()
{
    auto* hist = decadeHistogram(mPlot, true);
    QSignalSpy finished(&hist->pipeline(), &QCPHistogramPipeline::finished);
    QVERIFY(finished.wait(2000));
    hist->setKeyAxis(mPlot->xAxis2);
    {
        const QSignalBlocker noSignal(mPlot->xAxis2); // only the paint notices the new scale
        mPlot->xAxis2->setScaleType(QCPAxis::stLogarithmic);
    }
    mPlot->replot(); // queues the re-bin
    QVERIFY(!mPlot->toPixmap(400, 300).isNull());
    QSignalSpy busy(&hist->pipeline(), &QCPHistogramPipeline::busyChanged);
    QTest::qWait(100);
    QCOMPARE(busy.count(), 0);
    QVERIFY(spreadOverDecades(hist->pipeline().result(), true));
}

namespace
{
// Four keys a decade apart, all at value 5. Reads off the GUI thread wait for the gate, so a
// background job can be held while an export bins synchronously.
class GatedDecadeSource final : public QCPAbstractDataSource
{
public:
    explicit GatedDecadeSource(std::shared_ptr<std::atomic<bool>> open) : mOpen(std::move(open)) {}
    int size() const override { return 4; }
    double keyAt(int i) const override
    {
        if (QThread::currentThread() != qApp->thread())
            while (!mOpen->load()) QThread::msleep(1);
        return std::pow(10.0, i);
    }
    double valueAt(int) const override { return 5.0; }
    QCPRange keyRange(bool& found, QCP::SignDomain) const override { found = true; return {1, 1000}; }
    QCPRange valueRange(bool& found, QCP::SignDomain, const QCPRange&) const override
    {
        found = true;
        return {5, 5};
    }
    int findBegin(double, bool) const override { return 0; }
    int findEnd(double, bool) const override { return 4; }

private:
    std::shared_ptr<std::atomic<bool>> mOpen;
};
} // namespace

// An export bins for the new scale at once; a job started before it, in the old scale, must not
// replace that result when it lands.
void TestPipeline::histogram2dExportResultIsNotReplacedByAnOlderJob()
{
    auto* hist = new QCPHistogram2D(mPlot->xAxis, mPlot->yAxis);
    hist->setBins(3, 1);
    auto open = std::make_shared<std::atomic<bool>>(false);
    QSignalSpy finished(&hist->pipeline(), &QCPHistogramPipeline::finished);
    hist->setDataSource(std::make_shared<GatedDecadeSource>(open));
    mPlot->xAxis2->setScaleType(QCPAxis::stLogarithmic); // already log: no re-bin is queued
    hist->setKeyAxis(mPlot->xAxis2);
    QVERIFY(!mPlot->toPixmap(400, 300).isNull());
    QVERIFY(spreadOverDecades(hist->pipeline().result(), true));
    open->store(true);
    QTest::qWait(200);
    QVERIFY(spreadOverDecades(hist->pipeline().result(), true));
}

// Nothing finite bins to no cells: draw() must not take that for "not binned yet" and loop.
void TestPipeline::histogram2dWithNothingFiniteBinsOnce()
{
    auto* hist = new QCPHistogram2D(mPlot->xAxis, mPlot->yAxis);
    // A layer whose plottables all have nothing to show is not drawn: give it one that has.
    mPlot->addGraph()->setData(QVector<double>{0, 1}, QVector<double>{0, 1});
    const double nan = std::numeric_limits<double>::quiet_NaN();
    QSignalSpy finished(&hist->pipeline(), &QCPHistogramPipeline::finished);
    hist->setData(std::vector<double>(10, nan), std::vector<double>(10, nan));
    QVERIFY(finished.wait(2000));
    for (int i = 0; i < 20; ++i)
    {
        mPlot->replot();
        QTest::qWait(10);
    }
    QVERIFY2(finished.count() <= 2, qPrintable(QString::number(finished.count())));
    QVERIFY(hist->pipeline().result()->isEmpty());
}

// The pipeline keeps its previous result when a job returns none: old cells would stay.
void TestPipeline::histogram2dDataWithNothingFiniteClearsTheOldCells()
{
    auto* hist = decadeHistogram(mPlot, true);
    QSignalSpy finished(&hist->pipeline(), &QCPHistogramPipeline::finished);
    QVERIFY(finished.wait(2000));
    QVERIFY(hist->pipeline().result());
    const double nan = std::numeric_limits<double>::quiet_NaN();
    hist->setData(std::vector<double>(4, nan), std::vector<double>(4, nan));
    QVERIFY(finished.wait(2000));
    const auto* result = hist->pipeline().result();
    QVERIFY(result && result->isEmpty());
    QCOMPARE(result->keySize(), 0);
    QCOMPARE(result->valueSize(), 0);
    bool found = true;
    hist->colorDataBounds(found);
    QVERIFY(!found);
}

// The scale the cells were binned for is remembered at install time: a log axis from the start
// is fresh, not stale, so draw() must not ask for a re-bin again and again.
void TestPipeline::histogram2dLogValueAxisFromTheStartIsNotStale()
{
    mPlot->yAxis->setScaleType(QCPAxis::stLogarithmic);
    auto* hist = decadeHistogram(mPlot, false);
    QSignalSpy finished(&hist->pipeline(), &QCPHistogramPipeline::finished);
    QVERIFY(finished.wait(2000));
    QVERIFY(spreadOverDecades(hist->pipeline().result(), false));
    for (int i = 0; i < 20; ++i)
    {
        mPlot->replot();
        QTest::qWait(10);
    }
    QCOMPARE(finished.count(), 1);
}

// A queued re-bin is done once it ran: the next stale scale needs a new one.
// The axes swap scales without a signal, so only draw() sees each change.
void TestPipeline::histogram2dStaleScaleIsReboundAgainAfterAQueuedRebin()
{
    auto* hist = decadeHistogram(mPlot, true);
    QSignalSpy finished(&hist->pipeline(), &QCPHistogramPipeline::finished);
    QVERIFY(finished.wait(2000));
    mPlot->xAxis2->setScaleType(QCPAxis::stLogarithmic);
    hist->setKeyAxis(mPlot->xAxis2);
    mPlot->replot();
    QTRY_VERIFY_WITH_TIMEOUT(spreadOverDecades(hist->pipeline().result(), true), 2000);
    hist->setKeyAxis(mPlot->xAxis);
    mPlot->replot();
    QTRY_VERIFY_WITH_TIMEOUT(!spreadOverDecades(hist->pipeline().result(), true), 2000);
}

namespace
{
QImage exportedLogKeyDecades(QCustomPlot* plot, bool drawnLinearFirst)
{
    auto* hist = new QCPHistogram2D(plot->xAxis, plot->yAxis);
    hist->setBins(3, 1);
    hist->setDataRange(QCPRange(0, 3));
    QSignalSpy finished(&hist->pipeline(), &QCPHistogramPipeline::finished);
    hist->setData(std::vector<double>{1.0, 10.0, 100.0, 1000.0}, std::vector<double>{1.0, 2.0, 3.0, 4.0});
    finished.wait(2000);
    plot->xAxis->setRange(1, 1000);
    plot->yAxis->setRange(0, 5);
    if (drawnLinearFirst)
        plot->replot(QCustomPlot::rpImmediateRefresh);
    plot->xAxis->setScaleType(QCPAxis::stLogarithmic);
    plot->xAxis->setRange(1, 1000);
    return plot->toPixmap(400, 300).toImage();
}
} // namespace

// The export bins for the new scale itself: the picture of the old cells must not be reused.
// Values span a range: a histogram of one value is a zero-height strip, which paints nothing.
void TestPipeline::histogram2dExportDrawsTheNewBins()
{
    QCustomPlot reference;
    reference.resize(400, 300);
    const QImage expected = exportedLogKeyDecades(&reference, false);
    const QImage exported = exportedLogKeyDecades(mPlot, true);
    QVERIFY(!expected.isNull());
    QVERIFY(exported == expected);
}

void TestPipeline::histogram2dAdoptsTheRangeOfANewColorScale()
{
    auto* hist = new QCPHistogram2D(mPlot->xAxis, mPlot->yAxis);
    auto* scale = new QCPColorScale(mPlot);
    scale->setDataRange(QCPRange(5, 6));
    hist->setColorScale(scale);
    QCOMPARE(hist->dataRange(), QCPRange(5, 6));
    delete scale;
}

bool TestPipeline::repaintFollows(QCPHistogram2D* hist, const std::function<void()>& change)
{
    QSignalSpy finished(&hist->pipeline(), &QCPHistogramPipeline::finished);
    if (!finished.wait(2000))
    {
        qWarning("no binned result");
        return false;
    }
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTest::qWait(20);
    const auto buffer = hist->layer()->mPaintBuffer.toStrongRef();
    if (!buffer || buffer->contentDirty())
    {
        qWarning("no clean paint buffer to start from");
        return false;
    }
    QSignalSpy replotted(mPlot, &QCustomPlot::afterReplot);
    change();
    if (!buffer->contentDirty())
    {
        qWarning("no dirty flag");
        return false;
    }
    if (!replotted.wait(2000))
    {
        qWarning("no replot");
        return false;
    }
    return true;
}

// The label tile is drawn in pixels, computed from the grid's image rect: that rect must keep
// the axes' direction, or every label is mirrored away from its line on a reversed axis.
void TestPipeline::reversedKeyAxisKeepsContourLabelsWithTheirLines()
{
    auto* hist = new QCPHistogram2D(mPlot->xAxis, mPlot->yAxis);
    hist->setBins(3, 2);
    const double keyOfColumn[3] = { 1.0, 11.0, 21.0 };
    const int samplesOfColumn[3] = { 1, 3, 3 };
    std::vector<double> keys;
    std::vector<double> values;
    for (int column = 0; column < 3; ++column)
        for (int row = 0; row < 2; ++row)
            for (int sample = 0; sample < samplesOfColumn[column]; ++sample)
            {
                keys.push_back(keyOfColumn[column]);
                values.push_back(row == 0 ? 0.25 : 0.75);
            }
    hist->setData(std::move(keys), std::move(values));

    mPlot->xAxis->setRange(0, 30);
    mPlot->xAxis->setRangeReversed(true);
    mPlot->yAxis->setRange(0, 1);
    hist->setContourLevels({ 2.0 });
    hist->setContourPen(QPen(Qt::red, 2));
    hist->setContourLabelEnabled(true);

    QSignalSpy spy(&hist->pipeline(), &QCPHistogramPipeline::finished);
    QVERIFY(spy.wait(2000));
    mPlot->toPixmap();

    const auto& labels = hist->lastContourLabels();
    QCOMPARE(labels.size(), 1);
    // Level 2 sits between the 1 and 3 sample columns: u = 0.25 of the grid, key 6 here.
    double key = qQNaN();
    for (const auto& command : hist->mCore.recordedList().commands)
        if (const auto* line = std::get_if<qcp::PolylineDataCmd>(&command))
            for (const QPointF& candidate : *line->points)
                if (!std::isnan(candidate.x()))
                {
                    key = candidate.x();
                    break;
                }
    QVERIFY2(std::isfinite(key), "the recording holds a contour point");
    const double linePixel = mPlot->xAxis->coordToPixel(key);
    QVERIFY2(std::abs(labels.first().position.x() - linePixel) < 2.0,
             qPrintable(QString("label at x %1, its line at x %2")
                            .arg(labels.first().position.x())
                            .arg(linePixel)));
}

// A contour sits where the image's cell boundary sits: on a log key axis the extractor's
// key coordinate must be mapped geometrically, like the image corners and the value rows.
void TestPipeline::histogram2dContoursMapALogKeyGeometrically()
{
    auto* hist = new QCPHistogram2D(mPlot->xAxis, mPlot->yAxis);
    hist->setBins(10, 2);
    mPlot->xAxis->setScaleType(QCPAxis::stLogarithmic);

    // Column i holds i + 1 samples in each value row, so level 5.5 separates column 4 from
    // column 5: u = 0.5 of the grid.
    std::vector<double> keys;
    std::vector<double> values;
    for (int column = 0; column < 10; ++column)
    {
        const double centre = std::pow(10.0, 0.3 * (column + 0.5));
        for (int row = 0; row < 2; ++row)
            for (int sample = 0; sample <= column; ++sample)
            {
                const double key = column == 0 ? 1.0 : column == 9 && sample == 0 ? 1000.0 : centre;
                keys.push_back(key);
                values.push_back(row == 0 ? 0.25 : 0.75);
            }
    }
    hist->setData(std::move(keys), std::move(values));

    QSignalSpy spy(&hist->pipeline(), &QCPHistogramPipeline::finished);
    QVERIFY(spy.wait(2000));
    hist->setContourLevels({ 5.5 });
    hist->setContourPen(QPen(Qt::red, 2));
    mPlot->toPixmap();

    QPointF point(qQNaN(), qQNaN());
    for (const auto& command : hist->mCore.recordedList().commands)
        if (const auto* line = std::get_if<qcp::PolylineDataCmd>(&command))
        {
            for (const QPointF& candidate : *line->points)
                if (!std::isnan(candidate.x()))
                {
                    point = candidate;
                    break;
                }
            if (std::isfinite(point.x()))
                break;
        }
    QVERIFY2(std::isfinite(point.x()), "the recording holds a contour point");
    const double midpoint = std::sqrt(1000.0);
    QVERIFY2(std::abs(point.x() - midpoint) < 1.0,
             qPrintable(QString("contour key %1 is not the geometric midpoint %2")
                            .arg(point.x())
                            .arg(midpoint)));
}

void TestPipeline::histogram2dChangingTheGradientRequestsARepaint()
{
    auto* hist = decadeHistogram(mPlot, true);
    QVERIFY(repaintFollows(hist, [&] { hist->setGradient(QCPColorGradient(QCPColorGradient::gpHot)); }));
}

void TestPipeline::histogram2dChangingTheDataRangeRequestsARepaint()
{
    auto* hist = decadeHistogram(mPlot, true);
    QVERIFY(repaintFollows(hist, [&] { hist->setDataRange(QCPRange(1, 2)); }));
}

void TestPipeline::histogram2dChangingTheDataScaleTypeRequestsARepaint()
{
    auto* hist = decadeHistogram(mPlot, true);
    QVERIFY(repaintFollows(hist, [&] { hist->setDataScaleType(QCPAxis::stLogarithmic); }));
}

void TestPipeline::histogram2dChangingTheNormalizationRequestsARepaint()
{
    auto* hist = decadeHistogram(mPlot, true);
    QVERIFY(repaintFollows(hist, [&] { hist->setNormalization(QCP::HistogramNormalization::Density); }));
}

void TestPipeline::histogram2dColumnNormalizedSumsEachKeyColumnToOne()
{
    auto* hist = new QCPHistogram2D(mPlot->xAxis, mPlot->yAxis);
    // col 0: counts [3, 1] (sum=4), col 1: counts [0, 2] (sum=2)
    std::vector<double> keys = {0.25, 0.25, 0.25, 0.25, 0.75, 0.75};
    std::vector<double> vals = {0.25, 0.25, 0.25, 0.75, 0.75, 0.75};
    hist->setBins(2, 2);
    hist->setColumnNormalized(true);
    hist->setData(std::move(keys), std::move(vals));

    QSignalSpy spy(&hist->pipeline(), &QCPHistogramPipeline::finished);
    QVERIFY(spy.wait(2000));

    // The counts are untouched: column normalization is a view of them.
    auto* data = hist->pipeline().result();
    QVERIFY(data);
    QCOMPARE(data->cell(0, 0), 3.0);
    QCOMPARE(data->cell(0, 1), 1.0);
    QCOMPARE(data->cell(1, 0), 0.0);
    QCOMPARE(data->cell(1, 1), 2.0);

    auto* shown = hist->shown();
    QVERIFY(shown);
    QCOMPARE(shown->cell(0, 0), 0.75);
    QCOMPARE(shown->cell(0, 1), 0.25);
    QCOMPARE(shown->cell(1, 0), 0.0);
    QCOMPARE(shown->cell(1, 1), 1.0);

    // Column normalization replaces the normalization; what the user set is still reported.
    QCOMPARE(hist->normalization(), QCP::HistogramNormalization::Count);
    QSignalSpy rebin(&hist->pipeline(), &QCPHistogramPipeline::finished);
    hist->setNormalization(QCP::HistogramNormalization::Density);
    QCOMPARE(hist->normalization(), QCP::HistogramNormalization::Density);
    QCOMPARE(rebin.count(), 0);
    QVERIFY(hist->shown());
    QCOMPARE(hist->shown()->cell(0, 0), 0.75);

    hist->rescaleDataRange();
    mPlot->rescaleAxes();
    mPlot->replot();
    QCoreApplication::processEvents();
}

void TestPipeline::histogram2dColumnNormalizedDoesNotRebin()
{
    auto* hist = new QCPHistogram2D(mPlot->xAxis, mPlot->yAxis);
    hist->setBins(1, 1); // set bins before data to avoid double pipeline trigger
    std::vector<double> keys = {0.5, 0.5, 0.5};
    std::vector<double> vals = {0.5, 0.5, 0.5};
    hist->setData(std::move(keys), std::move(vals));

    QSignalSpy spy(&hist->pipeline(), &QCPHistogramPipeline::finished);
    QVERIFY(spy.wait(2000));
    // Drain any queued events
    QCoreApplication::processEvents();
    int finishedCount = spy.count();

    // Toggling column normalization should NOT trigger a new pipeline run
    hist->setColumnNormalized(true);
    QCoreApplication::processEvents();
    QCOMPARE(spy.count(), finishedCount);
}

void TestPipeline::histogram2dRenderSmokeTest()
{
    auto* hist = new QCPHistogram2D(mPlot->xAxis, mPlot->yAxis);
    std::vector<double> keys = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> vals = {1.0, 4.0, 2.0, 5.0, 3.0};
    hist->setData(std::move(keys), std::move(vals));

    QSignalSpy spy(&hist->pipeline(), &QCPHistogramPipeline::finished);
    QVERIFY(spy.wait(2000));

    hist->rescaleDataRange();
    mPlot->rescaleAxes();
    mPlot->replot();
    QCoreApplication::processEvents();
}

void TestPipeline::resampledMultiDataSourceInterface()
{
    qcp::algo::MultiColumnBinResult bins;
    bins.numColumns = 2;
    bins.keys = {1.0, 1.5, 3.0, 3.5, 5.0, 5.5};
    int s = 6;
    bins.values.resize(2 * s);
    bins.values[0*s+0]=10; bins.values[0*s+1]=20;
    bins.values[0*s+2]=30; bins.values[0*s+3]=40;
    bins.values[0*s+4]=50; bins.values[0*s+5]=60;
    bins.values[1*s+0]=5; bins.values[1*s+1]=15;
    bins.values[1*s+2]=25; bins.values[1*s+3]=35;
    bins.values[1*s+4]=std::numeric_limits<double>::quiet_NaN();
    bins.values[1*s+5]=std::numeric_limits<double>::quiet_NaN();

    QCPResampledMultiDataSource ds(std::move(bins));

    QCOMPARE(ds.columnCount(), 2);
    QCOMPARE(ds.size(), 6);
    QCOMPARE(ds.keyAt(0), 1.0);
    QCOMPARE(ds.keyAt(3), 3.5);
    QCOMPARE(ds.valueAt(0, 0), 10.0);
    QCOMPARE(ds.valueAt(0, 5), 60.0);
    QCOMPARE(ds.valueAt(1, 1), 15.0);
    QVERIFY(std::isnan(ds.valueAt(1, 4)));
    QVERIFY(ds.findBegin(2.0) <= 2);
    QVERIFY(ds.findEnd(4.0) >= 4);
    QVERIFY(!ds.empty());
}

void TestPipeline::multiGraphL1AndL2()
{
    // resampleL2Multi() returns nullptr (sparse path — render directly from L1)
    // when visible-L1-points <= l2Bins, where l2Bins = plotWidthPx * 4.
    // Use enough points to force the L2 binning path.
    const int N = 5000;
    std::vector<double> keys(N);
    std::vector<std::vector<double>> cols = {
        std::vector<double>(N), std::vector<double>(N)
    };
    for (int i = 0; i < N; ++i) {
        keys[i] = static_cast<double>(i);
        cols[0][i] = std::sin(i * 0.01);
        cols[1][i] = std::cos(i * 0.01);
    }
    auto src = std::make_shared<QCPSoAMultiDataSource<
        std::vector<double>, std::vector<double>>>(keys, cols);

    std::any cache;
    qcp::algo::buildL1CacheMulti(*src, ViewportParams{}, cache);
    auto* c = std::any_cast<qcp::algo::MultiGraphResamplerCache>(&cache);
    QVERIFY(c != nullptr);
    QCOMPARE(c->columnCount, 2);
    QVERIFY(c->level1.keys.size() > 0);

    ViewportParams vp;
    vp.keyRange = QCPRange(0, N - 1); // ~5000 visible points
    vp.plotWidthPx = 200;              // l2Bins = 800; 5000 > 800 ⇒ L2 path
    auto l2 = qcp::algo::resampleL2Multi(*c, vp, *src);
    QVERIFY(l2 != nullptr);
    QCOMPARE(l2->columnCount(), 2);
    QVERIFY(l2->size() > 0);
    QVERIFY(l2->keyAt(0) >= 0.0);
}

// --- QCPMultiGraph pipeline integration tests ---

void TestPipeline::multiGraphSmallDataNoResampling()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    std::vector<double> keys = {0, 1, 2, 3, 4};
    std::vector<std::vector<double>> cols = {{10,20,30,40,50}, {5,15,25,35,45}};
    mg->setData(std::move(keys), std::move(cols));

    QVERIFY(!mg->pipeline().hasTransform());
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
}

void TestPipeline::multiGraphLargeDataL1L2()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);

    // 10M+1 points * 3 columns — above threshold on both conditions
    const int N = 10'000'001;
    auto source = std::make_shared<SyntheticLargeMultiSource>(N, 3);
    mPlot->xAxis->setRange(0, N - 1);
    mPlot->yAxis->setRange(-1, 1);
    mg->setDataSource(source);

    QVERIFY(mg->pipeline().hasTransform());

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.start(30000);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(&mg->pipeline(), &QCPMultiGraphPipeline::finished,
            &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(mg->mL1Cache);

    // Trigger replot to build L2
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(mg->mL2Result);
    QVERIFY(mg->mL2Result->size() > 0);
    QVERIFY(mg->mL2Result->size() < N / 10);
}

void TestPipeline::multiGraphLineCacheRebuiltOnZoom()
{
    // Zooming a big resampled dataset must rebuild the lines, for
    // QCPMultiGraph, which is what SciQLopMultiGraphBase (SciQLop's real
    // multi-component line graphs) actually uses under the hood.
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);

    const int N = 10'000'001;
    auto source = std::make_shared<SyntheticLargeMultiSource>(N, 3);
    mPlot->xAxis->setRange(0, N - 1);
    mPlot->yAxis->setRange(-1, 1);
    mg->setDataSource(source);

    QVERIFY(mg->pipeline().hasTransform());

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.start(30000);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(&mg->pipeline(), &QCPMultiGraphPipeline::finished,
            &loop, &QEventLoop::quit);
    loop.exec();
    QVERIFY(mg->mL1Cache);

    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(mg->mL2Result);

    QVERIFY(mg->hasRenderedRange());
    const int fullRangeRecordings = mg->mCore.recordingCount();

    // Zoom IN on the big (resampled) dataset.
    mPlot->xAxis->setRange(N * 0.25, N * 0.75);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    const int zoomedInRecordings = mg->mCore.recordingCount();
    QVERIFY2(zoomedInRecordings > fullRangeRecordings, "zoom-in did not record fresh lines");

    // Zoom back OUT to the full range.
    mPlot->xAxis->setRange(0, N - 1);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY2(mg->mCore.recordingCount() > zoomedInRecordings,
             "zoom-out did not record fresh lines");
}

void TestPipeline::multiGraphPendingSourceCommitsAfterZoomTriggeredRefetch()
{
    // Simulates real usage: a zoom triggers a fresh data fetch for the new
    // visible range (SciQLopMultiGraphBase::set_data -> setDataSource on an
    // already-rendered graph). Checks the newly fetched/resampled data
    // actually reaches the display, not just that a job runs.
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setDeferredDataSwap(true); // keep the old data until the fetched one commits

    const int N1 = 10'000'001;
    auto source1 = std::make_shared<SyntheticLargeMultiSource>(N1, 3);
    mPlot->xAxis->setRange(0, N1 - 1);
    mPlot->yAxis->setRange(-1, 1);
    mg->setDataSource(source1);

    {
        QEventLoop loop;
        QTimer timeout; timeout.setSingleShot(true); timeout.start(30000);
        connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        connect(&mg->pipeline(), &QCPMultiGraphPipeline::finished, &loop, &QEventLoop::quit);
        loop.exec();
    }
    QVERIFY(mg->mL1Cache);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(mg->hasRenderedRange());

    // Zoom in and, as a real zoom-triggered refetch would, supply fresh
    // (still large) data for the new range.
    mPlot->xAxis->setRange(N1 * 0.25, N1 * 0.75);
    const int N2 = 5'000'001;
    auto source2 = std::make_shared<SyntheticLargeMultiSource>(N2, 3);
    mg->setDataSource(source2);

    // Wait for the pending source's L1 build to finish.
    {
        QEventLoop loop;
        QTimer timeout; timeout.setSingleShot(true); timeout.start(30000);
        connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        connect(&mg->pipeline(), &QCPMultiGraphPipeline::finished, &loop, &QEventLoop::quit);
        loop.exec();
    }

    // Give the debounced swap (default 200ms debounce, 1000ms cap) every
    // chance to fire, pumping the event loop the whole time.
    QTest::qWait(1500);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QVERIFY2(mg->dataSource() == source2.get(),
             "the newly fetched data source never got committed as displayed");
}

void TestPipeline::multiGraphMultipleRapidLegitimateSupersessionsCommitLast()
{
    // Per the user: assume the remote side is well-behaved (every request
    // honoured, responses arrive in order) -- the question is whether
    // NeoQCP's OWN pending-source generation-guard ("the fast feedback
    // loop": keep old data shown, swap when the new source commits) holds up
    // when it is superseded MULTIPLE times in a row before any of the
    // earlier pending sources ever finish their async L1 build or commit.
    // multiGraphRapidSetDataSource() (above) only calls setDataSource()
    // before the graph has ever rendered, so canDefer() is false both times
    // and neither call actually stages a pending source -- it doesn't
    // exercise this path at all.
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setDeferredDataSwap(true); // the staging path this test exercises is opt-in

    const int N = 10'000'001;
    auto source0 = std::make_shared<SyntheticLargeMultiSource>(N, 3);
    mPlot->xAxis->setRange(0, N - 1);
    mPlot->yAxis->setRange(-1, 1);
    mg->setDataSource(source0);
    {
        QEventLoop loop;
        QTimer timeout; timeout.setSingleShot(true); timeout.start(30000);
        connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        connect(&mg->pipeline(), &QCPMultiGraphPipeline::finished, &loop, &QEventLoop::quit);
        loop.exec();
    }
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(mg->hasRenderedRange()); // canDefer() now possible

    // Five legitimate, in-order, back-to-back "remote responses" -- each
    // superseding the previous PENDING one before it has any chance to
    // finish its own L1 build or commit.
    std::vector<std::shared_ptr<SyntheticLargeMultiSource>> sources;
    for (int i = 1; i <= 5; ++i)
    {
        auto s = std::make_shared<SyntheticLargeMultiSource>(N, 3);
        sources.push_back(s);
        mg->setDataSource(s);
    }
    auto lastSource = sources.back();

    // Replot until it settles. Waiting for one `finished` and a fixed delay raced on slow CI
    // runners: that `finished` could be an earlier, superseded job's.
    auto lastIsDisplayed = [&] {
        mPlot->replot(QCustomPlot::rpImmediateRefresh);
        return mg->dataSource() == lastSource.get();
    };
    QTRY_VERIFY2_WITH_TIMEOUT(lastIsDisplayed(),
                              "the last of 5 rapid, legitimate, in-order supersessions never "
                              "got committed as displayed",
                              30000);
}

void TestPipeline::multiGraphThresholdScalesWithColumnCount()
{
    // 200 points * 2 columns = 400 < 100K threshold → no resampling
    auto* mg1 = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    const int N = 200;
    auto src1 = std::make_shared<SyntheticLargeMultiSource>(N, 2);
    mg1->setDataSource(src1);
    QVERIFY(!mg1->pipeline().hasTransform());

    // 200K points * 100 columns = 20M > 100K threshold → resampling
    auto* mg2 = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    auto src2 = std::make_shared<SyntheticLargeMultiSource>(200'000, 100);
    mg2->setDataSource(src2);
    QVERIFY(mg2->pipeline().hasTransform());

    // 50 points * 200 columns = 10K but 50 < 100K floor → no resampling
    auto* mg3 = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    auto src3 = std::make_shared<SyntheticLargeMultiSource>(50, 200);
    mg3->setDataSource(src3);
    QVERIFY(!mg3->pipeline().hasTransform());
}

void TestPipeline::multiGraphRapidSetDataSource()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);

    // Set large data source twice rapidly — second should supersede first
    for (int round = 0; round < 2; ++round)
    {
        const int N = 10'000'001;
        auto source = std::make_shared<SyntheticLargeMultiSource>(N, 2);
        mg->setDataSource(source);
    }

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.start(30000);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(&mg->pipeline(), &QCPMultiGraphPipeline::finished,
            &loop, &QEventLoop::quit);
    loop.exec();

    // Should not crash, and L1 should be available
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
}

void TestPipeline::multiGraphExportSynchronousFallback()
{
    mPlot->resize(400, 300);
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);

    // Large data above threshold — pipeline has transform but no async result yet
    const int N = 5'000'000;
    auto source = std::make_shared<SyntheticLargeMultiSource>(N, 3);
    mg->setDataSource(source);
    mPlot->xAxis->setRange(0, N);
    mPlot->yAxis->setRange(-1, 1);

    QVERIFY(mg->pipeline().hasTransform());

    // Export immediately — no event loop, synchronous fallback path
    QPixmap pix = mPlot->toPixmap(400, 300);
    QVERIFY(!pix.isNull());

    // Center should not be white (data was rendered via synchronous fallback)
    QImage img = pix.toImage();
    QColor center = img.pixelColor(img.width() / 2, img.height() / 2);
    QVERIFY(center != Qt::white);
}

void TestPipeline::multiGraphLogScaleFallback()
{
    mPlot->xAxis->setScaleType(QCPAxis::stLogarithmic);
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);

    // 10M+1 points: above threshold, pipeline is installed
    const int N = 10'000'001;
    auto source = std::make_shared<SyntheticLargeMultiSource>(N, 2);
    mPlot->xAxis->setRange(1, N);
    mPlot->yAxis->setRange(-1, 1);
    mg->setDataSource(source);

    QVERIFY(mg->pipeline().hasTransform());

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.start(30000);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(&mg->pipeline(), &QCPMultiGraphPipeline::finished,
            &loop, &QEventLoop::quit);
    loop.exec();

    // Draw should not crash — log scale draw path falls back to raw data for L2
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
}

void TestPipeline::multiGraphDataChangedInvalidatesL1()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);

    const int N = 10'000'001;
    auto source = std::make_shared<SyntheticLargeMultiSource>(N, 2);
    mPlot->xAxis->setRange(0, N - 1);
    mPlot->yAxis->setRange(-1, 1);
    mg->setDataSource(source);

    QVERIFY(mg->pipeline().hasTransform());

    {
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        timeout.start(30000);
        connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        connect(&mg->pipeline(), &QCPMultiGraphPipeline::finished,
                &loop, &QEventLoop::quit);
        loop.exec();
    }
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(mg->mL1Cache);

    // dataChanged should invalidate L1 and trigger rebuild
    mg->dataChanged();
    QVERIFY(!mg->mL1Cache);

    {
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        timeout.start(30000);
        connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        connect(&mg->pipeline(), &QCPMultiGraphPipeline::finished,
                &loop, &QEventLoop::quit);
        loop.exec();
    }

    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(mg->mL1Cache);
}

// Whether the graph kept the first result to land. The `finished` slot added here runs after the
// graph's own, so the L1 cache already holds the result if the graph accepted it.
static bool keepsTheFirstL1ToLand(QCPMultiGraph* mg, const std::function<bool()>& hasL1)
{
    bool landed = false, kept = false;
    const auto connection = QObject::connect(
        &mg->pipeline(), &QCPMultiGraphPipeline::finished, mg, [&](uint64_t) {
            if (!std::exchange(landed, true))
                kept = hasL1();
        });
    QTest::qWaitFor([&] { return landed; }, 30000);
    QObject::disconnect(connection);
    return landed && kept;
}

// The job running for the old data lands first and must not be taken for the new data's.
void TestPipeline::multiGraphDataChangedRejectsAStaleL1()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    const int N = 10'000'001;
    mPlot->xAxis->setRange(0, N - 1);
    mPlot->yAxis->setRange(-1, 1);
    mg->setDataSource(std::make_shared<SyntheticLargeMultiSource>(N, 2));
    mg->dataChanged();
    QVERIFY(!keepsTheFirstL1ToLand(mg, [mg] { return bool(mg->mL1Cache); }));
}

// No job is coming for a source too small to resample, so the running one is the only L1 left.
void TestPipeline::multiGraphReplacedBySmallSourceRejectsAStaleL1()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    const int N = 10'000'001;
    mPlot->xAxis->setRange(0, N - 1);
    mPlot->yAxis->setRange(-1, 1);
    mg->setDataSource(std::make_shared<SyntheticLargeMultiSource>(N, 2));
    mg->setData(std::vector<double>{1, 2, 3}, std::vector<std::vector<double>>{{1, 2, 3}});
    QVERIFY(!mg->pipeline().hasTransform());
    QVERIFY(!keepsTheFirstL1ToLand(mg, [mg] { return bool(mg->mL1Cache); }));
}

void TestPipeline::multiGraphHiddenComponentsStillResampled()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);

    const int N = 10'000'001;
    auto source = std::make_shared<SyntheticLargeMultiSource>(N, 3);
    mPlot->xAxis->setRange(0, N - 1);
    mPlot->yAxis->setRange(-1, 1);
    mg->setDataSource(source);

    // Hide component 1 before L1 finishes
    mg->component(1).visible = false;

    // Pipeline should still resample all columns
    QVERIFY(mg->pipeline().hasTransform());

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.start(30000);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(&mg->pipeline(), &QCPMultiGraphPipeline::finished,
            &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(mg->mL1Cache);
    QCOMPARE(mg->mL1Cache->columnCount, 3);

    // Draw with one hidden component — should not crash
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
}

void TestPipeline::multiGraphLineCacheRebuiltOnLargePan()
{
    auto* graph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    const int N = 10000;
    std::vector<double> keys(N), values(N);
    for (int i = 0; i < N; ++i) { keys[i] = i; values[i] = std::sin(i * 0.01); }
    graph->setDataSource(oneColumnSource(keys, values));
    mPlot->xAxis->setRange(0, N);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    const int recordingsBefore = graph->mCore.recordingCount();
    QVERIFY(recordingsBefore > 0);

    // Large pan: shift by 110% — should trigger rebuild (threshold is now 100%)
    double shift = N * 1.1;
    mPlot->xAxis->setRange(shift, N + shift);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QTRY_VERIFY_WITH_TIMEOUT(graph->mCore.recordingCount() > recordingsBefore, 2000);
}

void TestPipeline::multiGraphLineCacheSurvives75PercentPan()
{
    auto* graph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    std::vector<double> keys(500), vals(500);
    for (int i = 0; i < 500; ++i) { keys[i] = i; vals[i] = i; }
    graph->setData(std::move(keys), std::move(vals));

    mPlot->xAxis->setRange(0, 100);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(graph->hasRenderedRange());

    // Pan 75% of axis range — now survives with 100% threshold
    mPlot->xAxis->setRange(75, 175);
    QPointF offset = graph->stallPixelOffset();
    QVERIFY(!offset.isNull());
}

void TestPipeline::multiGraphSmallDataLineCacheRebuiltOnZoom()
{
    auto* graph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    const int N = 10000;
    std::vector<double> keys(N), values(N);
    for (int i = 0; i < N; ++i) { keys[i] = i; values[i] = std::sin(i * 0.01); }
    graph->setDataSource(oneColumnSource(keys, values));
    mPlot->xAxis->setRange(0, N);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    const int recordingsBefore = graph->mCore.recordingCount();
    QVERIFY(recordingsBefore > 0);

    // Zoom in 2x (center stays same, range halved) — should trigger rebuild
    mPlot->xAxis->setRange(N * 0.25, N * 0.75);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QTRY_VERIFY_WITH_TIMEOUT(graph->mCore.recordingCount() > recordingsBefore, 2000);
}

void TestPipeline::multiGraphLineCacheRebuiltOnSmallZoom()
{
    auto* graph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    const int N = 10000;
    std::vector<double> keys(N), values(N);
    for (int i = 0; i < N; ++i) { keys[i] = i; values[i] = std::sin(i * 0.01); }
    graph->setDataSource(oneColumnSource(keys, values));
    mPlot->xAxis->setRange(0, N);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    const int recordingsBefore = graph->mCore.recordingCount();
    QVERIFY(recordingsBefore > 0);

    // Zoom by 0.5% — simulates Wayland smooth scrolling fine zoom step.
    // Must still trigger cache rebuild (translation-only offset is wrong for zoom).
    double half = N * 0.5;
    double shrink = N * 0.0025; // 0.5% of range on each side
    mPlot->xAxis->setRange(half - (N / 2.0 - shrink), half + (N / 2.0 - shrink));
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QTRY_VERIFY_WITH_TIMEOUT(graph->mCore.recordingCount() > recordingsBefore, 2000);
}

void TestPipeline::multiGraphLineCacheInvalidatedOnDataChange()
{
    auto* graph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    const int N = 10000;
    std::vector<double> keys(N), values(N);
    for (int i = 0; i < N; ++i) { keys[i] = i; values[i] = std::sin(i * 0.01); }
    graph->setDataSource(oneColumnSource(keys, values));
    mPlot->xAxis->setRange(0, N);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    const int recordingsBefore = graph->mCore.recordingCount();
    QVERIFY(recordingsBefore > 0);

    // Replace data — the graph is already rendered, so the replacement is staged;
    // committing it (via the plot's swap window) also triggers the plot's automatic
    // post-commit replot, which immediately rebuilds the lines from the new source.
    // So the meaningful assertion is "the lines now reflect the new data".
    std::vector<double> keys2(N), values2(N);
    for (int i = 0; i < N; ++i) { keys2[i] = i; values2[i] = std::cos(i * 0.01); }
    graph->setDataSource(oneColumnSource(std::move(keys2), std::move(values2)));

    QTRY_VERIFY_WITH_TIMEOUT(!graph->hasPendingData(), 5000);
    QCOMPARE(graph->dataSource()->valueAt(0, 0), std::cos(0.0));
    QTRY_VERIFY_WITH_TIMEOUT(graph->mCore.recordingCount() > recordingsBefore, 2000);
}

void TestPipeline::multiGraphLineCacheReusedOnSmallPan()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    const int N = 10000;
    const int cols = 3;
    std::vector<double> keys(N);
    std::vector<std::vector<double>> values(cols, std::vector<double>(N));
    for (int i = 0; i < N; ++i) {
        keys[i] = i;
        for (int c = 0; c < cols; ++c)
            values[c][i] = std::sin(i * 0.01 + c);
    }
    mg->setDataSource(std::make_shared<QCPSoAMultiDataSource<
        std::vector<double>, std::vector<double>>>(
        std::move(keys), std::move(values)));
    mPlot->xAxis->setRange(0, N);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QVERIFY(mg->hasRenderedRange());
    const int recordings = mg->mCore.recordingCount();

    // Small pan
    double shift = N * 0.05;
    mPlot->xAxis->setRange(shift, N + shift);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QCOMPARE(mg->mCore.recordingCount(), recordings); // a pan inside the window is a translation
}

void TestPipeline::multiGraphFastPanNeverBlank()
{
    if (!showAndHasRhi(mPlot)) QSKIP("No QRhi available — hasRenderedRange() requires GPU draw()");

    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);

    int N = 500'000;
    std::vector<double> keys(N);
    std::vector<std::vector<double>> cols(2, std::vector<double>(N));
    for (int i = 0; i < N; ++i) {
        keys[i] = i;
        cols[0][i] = std::sin(i * 0.0001);
        cols[1][i] = std::cos(i * 0.0001);
    }
    mg->setData(std::move(keys), std::move(cols));

    mPlot->xAxis->setRange(0, 100000);
    // Wait for the first render.
    QTRY_VERIFY_WITH_TIMEOUT((mPlot->replot(), mg->hasRenderedRange()), 10000);

    for (int p = 0; p < 20; ++p)
    {
        double lo = mPlot->xAxis->range().lower + 30000;
        mPlot->xAxis->setRange(lo, lo + 100000);
        mPlot->replot();
        QCoreApplication::processEvents();
        QVERIFY(mg->hasRenderedRange());
    }
}

void TestPipeline::colormap2ResultSurvivesNullViewport()
{
    // Reproducer: when a viewport-dependent transform returns nullptr (no data
    // overlap), applyResult must not overwrite the previous valid result.
    // This caused colormaps to go permanently blank after panning through gaps.
    auto scheduler = std::make_unique<QCPPipelineScheduler>();
    using Pipeline = QCPAsyncPipeline<QCPAbstractDataSource, QCPAbstractDataSource>;
    Pipeline pipeline(scheduler.get());

    // Transform that returns nullptr when viewport key range starts above 50
    pipeline.setTransform(TransformKind::ViewportDependent,
        [](const QCPAbstractDataSource& src, const ViewportParams& vp, std::any&)
            -> std::shared_ptr<QCPAbstractDataSource> {
            if (vp.keyRange.lower > 50)
                return nullptr;
            int n = src.size();
            std::vector<double> k(n), v(n);
            for (int i = 0; i < n; ++i) { k[i] = src.keyAt(i); v[i] = src.valueAt(i); }
            return std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
                std::move(k), std::move(v));
        });

    auto source = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::vector<double>{1, 2, 3}, std::vector<double>{4, 5, 6});
    pipeline.setSource(source);

    // First result with valid viewport
    ViewportParams validVp;
    validVp.keyRange = {0, 10};
    validVp.plotWidthPx = 400;
    validVp.plotHeightPx = 300;
    pipeline.onViewportChanged(validVp);

    QSignalSpy spy(&pipeline, &Pipeline::finished);
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 2000);
    QVERIFY2(pipeline.result() != nullptr, "Valid viewport must produce a result");

    // Viewport with no data overlap — transform returns nullptr
    spy.clear();
    ViewportParams invalidVp;
    invalidVp.keyRange = {100, 200};
    invalidVp.plotWidthPx = 400;
    invalidVp.plotHeightPx = 300;
    pipeline.onViewportChanged(invalidVp);
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 2000);

    QVERIFY2(pipeline.result() != nullptr,
             "Null transform result must not erase previous valid pipeline result");
}

namespace {
// Irregular state changes (SciQLopPlots#118): with the default threshold the
// steps 1 -> 5 and 6 -> 12 are gaps; with 0 the whole trace is one line.
const std::vector<double> kIrregularKeys = {0, 1, 5, 6, 12};
const std::vector<double> kStateValues = {1, 0, 1, 0, 0};

}

void TestPipeline::multiGraphGapThresholdZeroDrawsIrregularSteps()
{
    auto* g = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    g->setLineStyle(QCPMultiGraph::lsStepLeft);
    g->setDataSource(std::make_shared<QCPSoAMultiDataSource<std::vector<double>, std::vector<double>>>(
        kIrregularKeys, std::vector<std::vector<double>>{kStateValues}));
    mPlot->xAxis->setRange(0, 12);
    mPlot->yAxis->setRange(-1, 2);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QCOMPARE(g->mCore.recordedCommandCount(), 1);
    QVERIFY(g->mCore.recordedPolylineNaNs() > 0);

    g->setGapThreshold(0);
    QCOMPARE(g->gapThreshold(), 0.0);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QCOMPARE(g->mCore.recordedPolylineNaNs(), 0);
}

namespace {
// Big enough to be drawn from the resampled (L2) source, with a hole as wide as
// the data in the middle of the keys.
constexpr int kHoleN = 400'000;

std::vector<double> keysWithHole()
{
    std::vector<double> keys(kHoleN);
    for (int i = 0; i < kHoleN; ++i)
        keys[i] = i < kHoleN / 2 ? i : i + kHoleN;
    return keys;
}

std::vector<double> sineValues()
{
    std::vector<double> values(kHoleN);
    for (int i = 0; i < kHoleN; ++i)
        values[i] = std::sin(i * 1e-3);
    return values;
}
}

void TestPipeline::multiGraphGapThresholdReachesResampledSource()
{
    auto* g = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    QSignalSpy spy(&g->pipeline(), &QCPMultiGraphPipeline::finished);
    mPlot->xAxis->setRange(0, 2 * kHoleN);
    mPlot->yAxis->setRange(-1, 1);
    g->setDataSource(std::make_shared<QCPSoAMultiDataSource<std::vector<double>, std::vector<double>>>(
        keysWithHole(), std::vector<std::vector<double>>{sineValues()}));
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 30000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(g->mL2Result);
    QVERIFY(g->mCore.recordedPolylineNaNs() > 0);

    g->setGapThreshold(0);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QCOMPARE(g->mCore.recordedPolylineNaNs(), 0);
}

namespace {
// MMS FGM survey: 16 S/s, enough samples to be resampled.
constexpr int kUniformN = 200'000;
constexpr double kUniformStep = 0.0625;

std::vector<double> uniformKeys()
{
    std::vector<double> keys(kUniformN);
    for (int i = 0; i < kUniformN; ++i)
        keys[i] = i * kUniformStep;
    return keys;
}

std::vector<double> uniformValues()
{
    std::vector<double> values(kUniformN);
    for (int i = 0; i < kUniformN; ++i)
        values[i] = std::sin(i * 0.01);
    return values;
}

// From raw rows (< 1.3 samples per pixel) through L2 binned from raw rows, to L2 binned from L1.
const std::vector<double> kSamplesPerPixel = {0.5, 1.0, 1.5, 2.0, 3.4, 5.0, 8.0, 12.0, 20.0, 40.0};

// The zooms at which the graph breaks a line drawn from gap-free data.
template <typename Breaks, typename UsesL2>
QStringList zoomsThatBreakUniformData(QCustomPlot* plot, Breaks breaks, UsesL2 usesL2,
                                     const std::vector<double>& zooms = kSamplesPerPixel)
{
    QStringList broken;
    const double width = plot->xAxis->axisRect()->width();
    for (const double perPixel : zooms)
    {
        const double start = kUniformN * kUniformStep / 2;
        plot->xAxis->setRange(start, start + perPixel * width * kUniformStep);
        plot->replot(QCustomPlot::rpImmediateRefresh);
        if (const int count = breaks(); count > 0)
            broken << QString("%1 samples/px (%2): %3 breaks")
                          .arg(perPixel)
                          .arg(usesL2() ? "L2" : "raw")
                          .arg(count);
    }
    return broken;
}
}

void TestPipeline::multiGraphUniformDataNeverBreaksAtAnyZoom()
{
    auto* g = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    QSignalSpy spy(&g->pipeline(), &QCPMultiGraphPipeline::finished);
    mPlot->yAxis->setRange(-1.5, 1.5);
    g->setDataSource(std::make_shared<QCPSoAMultiDataSource<std::vector<double>, std::vector<double>>>(
        uniformKeys(), std::vector<std::vector<double>>{uniformValues()}));
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 30000);

    const auto broken = zoomsThatBreakUniformData(
        mPlot, [g] { return g->mCore.recordedPolylineNaNs(); }, [g] { return bool(g->mL2Result); });
    QVERIFY2(broken.isEmpty(), qPrintable(broken.join("; ")));
    QVERIFY2(g->mL2Result, "the widest zoom must be drawn from L2, or this test checks nothing");
}

void TestPipeline::multiGraphNaNHoleBreaksTheResampledLine()
{
    // The hole must be narrower than the view at both zooms, so the line has data on both sides of
    // it: at 3.4 samples per pixel a 360-pixel axis rect shows about 1 200 samples.
    constexpr int hole = 300;
    auto values = uniformValues();
    std::fill(values.begin() + kUniformN / 2, values.begin() + kUniformN / 2 + hole, qQNaN());
    auto* g = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    QSignalSpy spy(&g->pipeline(), &QCPMultiGraphPipeline::finished);
    mPlot->yAxis->setRange(-1.5, 1.5);
    g->setDataSource(std::make_shared<QCPSoAMultiDataSource<std::vector<double>, std::vector<double>>>(
        uniformKeys(), std::vector<std::vector<double>>{values}));
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 30000);

    for (const double perPixel : {3.4, 12.0})
    {
        const double centre = (kUniformN / 2 + hole / 2) * kUniformStep;
        const double span = perPixel * mPlot->xAxis->axisRect()->width() * kUniformStep;
        mPlot->xAxis->setRange(centre - span / 2, centre + span / 2);
        g->setGapThreshold(0);
        mPlot->replot(QCustomPlot::rpImmediateRefresh);
        QVERIFY(g->mL2Result);
        QVERIFY2(g->mCore.recordedPolylineNaNs() > 0, qPrintable(QString::number(perPixel)));
    }
}

void TestPipeline::multiGraphNaNHoleInOneColumnBreaksThatLine()
{
    // Column 0 is NaN over 4800 samples (5 min at 16 S/s); column 1 has data everywhere.
    auto hole = uniformValues();
    std::fill(hole.begin() + kUniformN / 2, hole.begin() + kUniformN / 2 + 4'800, qQNaN());
    auto* g = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    QSignalSpy spy(&g->pipeline(), &QCPMultiGraphPipeline::finished);
    mPlot->yAxis->setRange(-1.5, 1.5);
    g->setDataSource(std::make_shared<QCPSoAMultiDataSource<std::vector<double>, std::vector<double>>>(
        uniformKeys(), std::vector<std::vector<double>>{hole, uniformValues()}));
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 30000);

    const double centre = (kUniformN / 2 + 2'400) * kUniformStep;
    const double span = 3 * 3600;  // 3 h view: the hole is ~1/36 of the width
    mPlot->xAxis->setRange(centre - span / 2, centre + span / 2);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(g->mL2Result);
    const int breaks = g->mCore.recordedPolylineNaNs();
    qInfo() << "plot width" << mPlot->xAxis->axisRect()->width() << "px, column 0 breaks" << breaks;
    QVERIFY2(breaks > 0, "the hole was bridged: the recorded line has no break");
}

// A NaN narrower than a pixel cannot be seen: it must not break a resampled line. Narrower means
// its two neighbours are less than a pixel apart, so more than 2 samples per pixel.
void TestPipeline::sporadicNaNNeverBreaksTheResampledLine()
{
    const std::vector<double> subPixelNaN = {3.4, 5.0, 8.0, 12.0, 20.0, 40.0};
    auto values = uniformValues();
    for (int i = 0; i < kUniformN; i += 37)
        values[i] = qQNaN();
    auto* g = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    QSignalSpy spy(&g->pipeline(), &QCPMultiGraphPipeline::finished);
    mPlot->yAxis->setRange(-1.5, 1.5);
    g->setDataSource(std::make_shared<QCPSoAMultiDataSource<std::vector<double>, std::vector<double>>>(
        uniformKeys(), std::vector<std::vector<double>>{values, values}));
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 30000);
    QStringList broken = zoomsThatBreakUniformData(
        mPlot, [g] { return g->mCore.recordedPolylineNaNs(); }, [g] { return bool(g->mL2Result); },
        subPixelNaN);

    QVERIFY2(broken.isEmpty(), qPrintable(broken.join("; ")));
}

void TestPipeline::densePanIsRedrawnFreshBesideASmallGraph()
{
    // Both graphs share the "main" layer. The small one is drawn raw: its lines never go
    // dirty after a pan, so it keeps offering a translation for the whole layer.
    if (!showAndHasRhi(mPlot))
        QSKIP("no QRhi available in this environment");
    auto* small = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    QVector<double> sk(500), sv(500);
    for (int i = 0; i < 500; ++i)
    {
        sk[i] = i * 800.0;
        sv[i] = 0.5;
    }
    small->setData(sk, sv);
    auto* dense = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    QVector<double> keys(400000), values(400000);
    for (int i = 0; i < 400000; ++i)
    {
        keys[i] = i;
        values[i] = std::sin(i * 0.001);
    }
    dense->setData(keys, values);
    mPlot->xAxis->setRange(0, 100000);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTRY_VERIFY_WITH_TIMEOUT(!dense->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(dense->mL2Result);
    QCOMPARE(small->layer(), dense->layer());

    const int recordings = dense->mCore.recordingCount();
    mPlot->xAxis->setRange(25000, 125000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTRY_VERIFY2_WITH_TIMEOUT(dense->mCore.recordingCount() > recordings,
                              "the dense graph was never drawn fresh", 2000);
}

void TestPipeline::colormapNewDataShowsWhileAnotherLayerIsDirty()
{
    if (!showAndHasRhi(mPlot))
        QSKIP("no QRhi available in this environment");
    auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
    cm->setGradient(QCPColorGradient(QCPColorGradient::gpGrayscale));
    cm->setDataRange(QCPRange(0, 1));
    const auto fill = [](double value) {
        std::vector<double> x(50), y(50), z(50 * 50, value);
        for (int i = 0; i < 50; ++i)
            x[i] = y[i] = i;
        return std::tuple { std::move(x), std::move(y), std::move(z) };
    };
    auto [x0, y0, z0] = fill(0.0);
    cm->setData(std::move(x0), std::move(y0), std::move(z0));
    mPlot->xAxis->setRange(0, 49);
    mPlot->yAxis->setRange(0, 49);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTRY_VERIFY_WITH_TIMEOUT(!cm->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    const auto centre = [&] {
        const QImage frame = mPlot->grabFramebuffer();
        const QPoint c = mPlot->axisRect()->rect().center() * frame.devicePixelRatio();
        return frame.pixelColor(c);
    };
    QVERIFY2(centre().lightness() < 60, qPrintable(centre().name()));

    // Like a crosshair following the mouse: some other layer is dirty on every replot.
    connect(mPlot, &QCustomPlot::beforeReplot, mPlot, [this] { mPlot->layer("overlay")->markDirty(); });
    auto [x1, y1, z1] = fill(1.0);
    cm->setData(std::move(x1), std::move(y1), std::move(z1));
    QTRY_VERIFY2_WITH_TIMEOUT(centre().lightness() > 200, "the new colormap data never showed", 3000);
}

void TestPipeline::multiGraphSetterRedrawsWhileAnotherLayerIsDirty()
{
    auto* graph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    graph->setData(QVector<double> { 0, 1, 2, 3 }, QVector<double> { 0, 1, 0, 1 });
    mPlot->xAxis->setRange(0, 3);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(graph->hasRenderedRange());
    const int recordings = graph->mCore.recordingCount();
    // Like a crosshair following the mouse: some other layer is dirty on every replot.
    connect(mPlot, &QCustomPlot::beforeReplot, mPlot, [this] { mPlot->layer("overlay")->markDirty(); });
    graph->setGapThreshold(0);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY2(graph->mCore.recordingCount() > recordings,
             "the graph was not drawn after setGapThreshold");
}

namespace
{
//! Positive values over keys 1..10^4, drawn once on the given axis scales.
QCPMultiGraph* drawnPositiveGraph(QCustomPlot* plot, bool logKeys, bool logValues)
{
    auto* graph = new QCPMultiGraph(plot->xAxis, plot->yAxis);
    QVector<double> keys(2000), values(2000);
    for (int i = 0; i < 2000; ++i)
    {
        keys[i] = std::pow(10.0, 4.0 * i / 1999.0);
        values[i] = 2.0 + std::sin(i * 0.05);
    }
    graph->setData(std::move(keys), std::move(values));
    if (logKeys)
        plot->xAxis->setScaleType(QCPAxis::stLogarithmic);
    if (logValues)
        plot->yAxis->setScaleType(QCPAxis::stLogarithmic);
    plot->xAxis->setRange(10, 100);
    plot->yAxis->setRange(0.5, 5);
    plot->replot(QCustomPlot::rpImmediateRefresh);
    plot->replot(QCustomPlot::rpImmediateRefresh);
    return graph;
}
} // namespace

void TestPipeline::logValueAxisPanTranslates()
{
    if (!showAndHasRhi(mPlot))
        QSKIP("no QRhi available in this environment");
    // A pan on a log axis multiplies both ends (QCPAxisRect drag, QCPAxis::moveRange): every
    // point moves by the same number of pixels, so the cached lines can be shifted.
    auto* graph = drawnPositiveGraph(mPlot, false, true);
    mPlot->yAxis->moveRange(2.0);
    QVERIFY(!graph->stallPixelOffset().isNull());
}

void TestPipeline::logKeyAxisPanStaysCovered()
{
    if (!showAndHasRhi(mPlot))
        QSKIP("no QRhi available in this environment");
    auto* graph = drawnPositiveGraph(mPlot, true, false);
    mPlot->xAxis->moveRange(2.0); // [10, 100] -> [20, 200]
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    // The thinned lines depend on the key view, so a pan records them again (no translation):
    // the recording holds data coordinates and must still span the view. A linear key window would stop at 190 and leave the right edge blank.
    double left = std::numeric_limits<double>::max(), right = std::numeric_limits<double>::lowest();
    for (const auto& command : graph->mCore.recordedList().commands)
    {
        const auto* polyline = std::get_if<qcp::PolylineDataCmd>(&command);
        if (!polyline || !polyline->points)
            continue;
        for (const QPointF& p : *polyline->points)
            if (std::isfinite(p.x()))
            {
                left = std::min(left, p.x());
                right = std::max(right, p.x());
            }
    }
    const QCPRange view = mPlot->xAxis->range();
    QVERIFY2(left <= view.lower && right >= view.upper,
             qPrintable(QString("lines span %1..%2, view %3..%4").arg(left).arg(right).arg(view.lower).arg(view.upper)));
}

void TestPipeline::additiveShiftOnLogAxisIsNotATranslation()
{
    if (!showAndHasRhi(mPlot))
        QSKIP("no QRhi available in this environment");
    // [10, 100] -> [20, 110] squeezes the low end: points move by different amounts.
    auto* graph = drawnPositiveGraph(mPlot, true, false);
    mPlot->xAxis->setRange(20, 110);
    QVERIFY(graph->stallPixelOffset().isNull());
}

void TestPipeline::lineCacheCoversADecadeEachSideOnLogKeys()
{
    const QCPRange log = qcp::algo::lineCacheKeyRange(QCPRange(1, 10), true);
    QCOMPARE(log.lower, 0.1);
    QCOMPARE(log.upper, 100.0);
    const QCPRange lin = qcp::algo::lineCacheKeyRange(QCPRange(1, 10), false);
    QCOMPARE(lin.lower, -8.0);
    QCOMPARE(lin.upper, 19.0);
}

// Below the resampling threshold dataChanged replots at once, so beforeReplot slots run inside
// it; one deleting the graph must end dataChanged.
void TestPipeline::multiGraphDataChangedReplotSlotDeletingTheGraphIsSafe()
{
    auto* g = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    g->setData(std::vector<double>{ 0, 1, 2 }, std::vector<double>{ 0, 1, 2 });
    QVERIFY(!g->pipeline().hasTransform());
    connect(mPlot, &QCustomPlot::beforeReplot, mPlot,
            [this, g] { QVERIFY(mPlot->removePlottable(g)); }, Qt::SingleShotConnection);
    g->dataChanged();
    QCOMPARE(mPlot->plottableCount(), 0);
}

namespace
{
//! One sample in every cell of a 40×40 grid, plus a round bump of up to 10 more in the middle.
QCPHistogram2D* bumpHistogram(QCustomPlot* plot)
{
    auto* hist = new QCPHistogram2D(plot->xAxis, plot->yAxis);
    std::vector<double> keys, values;
    for (int i = 0; i < 40; ++i)
        for (int j = 0; j < 40; ++j)
        {
            const int extra = qRound(10 * std::exp(-((i - 20) * (i - 20) + (j - 20) * (j - 20)) / 60.0));
            for (int n = 0; n <= extra; ++n)
            {
                keys.push_back(i + 0.5);
                values.push_back(j + 0.5);
            }
        }
    hist->setBins(40, 40);
    hist->setData(std::move(keys), std::move(values));
    plot->resize(400, 400);
    plot->xAxis->setRange(0, 40);
    plot->yAxis->setRange(0, 40);
    return hist;
}

//! How many contour lines the last recording holds, one polyline per level.
int recordedContourLines(const QCPDrawnCore& core)
{
    int lines = 0;
    for (const auto& command : core.recordedList().commands)
        lines += std::get_if<qcp::PolylineDataCmd>(&command) != nullptr;
    return lines;
}

int redPixels(const QImage& img)
{
    int red = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
        {
            const QColor c = img.pixelColor(x, y);
            red += c.red() > 200 && c.green() < 60 && c.blue() < 60;
        }
    return red;
}
} // namespace

// The label tile is drawn in pixels, so a zoom must rebuild it: otherwise it is replayed
// stretched, and the area a pan reveals has no labels at all.
void TestPipeline::zoomRebuildsTheContourLabelTile()
{
    auto* hist = bumpHistogram(mPlot);
    hist->setAutoContourLevels(3);
    hist->setContourLabelEnabled(true);
    QSignalSpy spy(&hist->pipeline(), &QCPHistogramPipeline::finished);
    QVERIFY(spy.wait(2000));
    mPlot->replot();
    QVERIFY(!hist->lastContourLabels().isEmpty());
    const int recordings = hist->mCore.recordingCount();

    mPlot->xAxis->setRange(10, 20);
    mPlot->replot();
    QVERIFY2(hist->mCore.recordingCount() > recordings, "the label tile was replayed stretched");
}

// Without labels the recording is all data geometry, so a zoom may replay it as is.
void TestPipeline::zoomKeepsTheContourRecordingWithoutLabels()
{
    auto* hist = bumpHistogram(mPlot);
    hist->setAutoContourLevels(3);
    QSignalSpy spy(&hist->pipeline(), &QCPHistogramPipeline::finished);
    QVERIFY(spy.wait(2000));
    mPlot->replot();
    const int recordings = hist->mCore.recordingCount();

    mPlot->xAxis->setRange(10, 20);
    mPlot->replot();
    QCOMPARE(hist->mCore.recordingCount(), recordings);
}

void TestPipeline::histogram2dContoursFollowTheCounts()
{
    auto* hist = bumpHistogram(mPlot);
    hist->setGradient(QCPColorGradient(QCPColorGradient::gpGrayscale));
    hist->rescaleDataRange(true);
    QSignalSpy spy(&hist->pipeline(), &QCPHistogramPipeline::finished);
    QVERIFY(spy.wait(2000));
    const QImage without = mPlot->toPixmap().toImage();
    hist->setContourLevels({ 3.0, 6.0 });
    hist->setContourPen(QPen(Qt::red, 2));
    const QImage with = mPlot->toPixmap().toImage();
    QCOMPARE(hist->lastContourLevels(), (QVector<double> { 3.0, 6.0 }));
    QVERIFY2(recordedContourLines(hist->mCore) == 2, "one polyline per contour level");
    QVERIFY2(redPixels(without) == 0 && redPixels(with) > 50,
             qPrintable(QString("red pixels: %1 without, %2 with").arg(redPixels(without)).arg(redPixels(with))));
}

// With column normalization the image shows fractions of each column: the lines must too.
void TestPipeline::histogram2dContoursFollowTheNormalizedCounts()
{
    auto* hist = bumpHistogram(mPlot);
    hist->setColumnNormalized(true);
    hist->setAutoContourLevels(3);
    QSignalSpy spy(&hist->pipeline(), &QCPHistogramPipeline::finished);
    QVERIFY(spy.wait(2000));
    mPlot->toPixmap();
    const QVector<double> levels = hist->lastContourLevels();
    QCOMPARE(levels.size(), 3);
    for (double level : levels)
        QVERIFY2(level > 0 && level < 1, qPrintable(QString("level %1 is not a column fraction").arg(level)));
    QVERIFY2(recordedContourLines(hist->mCore) == 3, "one polyline per auto level");
}

// Auto levels change with the colour scale type, which does not re-bin: the labels must follow.
void TestPipeline::histogram2dContourLabelsFollowAColourScaleToggle()
{
    auto* hist = bumpHistogram(mPlot);
    hist->setAutoContourLevels(3);
    hist->setContourLabelEnabled(true);
    QSignalSpy spy(&hist->pipeline(), &QCPHistogramPipeline::finished);
    QVERIFY(spy.wait(2000));
    mPlot->toPixmap();
    const QVector<double> linear = hist->lastContourLevels();
    hist->setDataScaleType(QCPAxis::stLogarithmic);
    mPlot->toPixmap();
    const QVector<double> levels = hist->lastContourLevels();
    QVERIFY2(levels != linear, "sanity: log auto levels differ from linear ones");
    QVERIFY(!hist->lastContourLabels().isEmpty());
    for (const auto& label : hist->lastContourLabels())
        QVERIFY2(levels.contains(label.level),
                 qPrintable(QString("label level %1 is not a current level").arg(label.level)));
}
