#include "test-pipeline.h"
#include <qcustomplot.h>
#include <datasource/pipeline-scheduler.h>
#include <datasource/async-pipeline.h>
#include <datasource/soa-datasource.h>
#include <datasource/soa-datasource-2d.h>
#include <plottables/plottable-graph2.h>
#include <plottables/plottable-colormap2.h>
#include <plottables/plottable-colormap.h>
#include <datasource/graph-resampler.h>
#include <datasource/histogram-binner.h>
#include <QSignalSpy>
#include <QThread>
#include <cmath>
#include <limits>

void TestPipeline::init()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
}

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

void TestPipeline::pipelinePassthrough()
{
    QCPPipelineScheduler scheduler;
    auto source = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::vector<double>{1, 2, 3}, std::vector<double>{4, 5, 6});

    QCPGraphPipeline pipeline(&scheduler);
    pipeline.setSource(source);

    QVERIFY(!pipeline.hasTransform());
    QCOMPARE(pipeline.result(), source.get());
}

void TestPipeline::pipelineTransformRuns()
{
    QCPPipelineScheduler scheduler;
    auto source = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::vector<double>{1, 2, 3}, std::vector<double>{4, 5, 6});

    QCPGraphPipeline pipeline(&scheduler);
    QSignalSpy spy(&pipeline, &QCPGraphPipeline::finished);

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

void TestPipeline::pipelineCoalescing()
{
    QCPPipelineScheduler scheduler(1);
    std::atomic<int> runCount{0};

    auto source = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::vector<double>{1}, std::vector<double>{1});

    QCPGraphPipeline pipeline(&scheduler);
    QSignalSpy spy(&pipeline, &QCPGraphPipeline::finished);

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

    QCPGraphPipeline pipeline(&scheduler);
    QSignalSpy spy(&pipeline, &QCPGraphPipeline::finished);

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

    QCPGraphPipeline pipeline(&scheduler);
    QSignalSpy spy(&pipeline, &QCPGraphPipeline::finished);

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

    QCPGraphPipeline pipeline(&scheduler);
    QSignalSpy spy(&pipeline, &QCPGraphPipeline::finished);

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

    QCPGraphPipeline pipeline(&scheduler);
    QSignalSpy spy(&pipeline, &QCPGraphPipeline::finished);

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

    QCPGraphPipeline pipeline(&scheduler);
    QSignalSpy spy(&pipeline, &QCPGraphPipeline::finished);

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

        auto pipeline = std::make_unique<QCPGraphPipeline>(&scheduler);
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

void TestPipeline::graph2PipelinePassthrough()
{
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    graph->setData(std::vector<double>{1, 2, 3}, std::vector<double>{4, 5, 6});

    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QCOMPARE(graph->dataSource()->size(), 3);
}

void TestPipeline::graph2PipelineTransform()
{
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);

    graph->pipeline().setTransform(TransformKind::ViewportIndependent,
        [](const QCPAbstractDataSource& src, const ViewportParams&, std::any&)
            -> std::shared_ptr<QCPAbstractDataSource> {
            int n = src.size();
            std::vector<double> keys(n), values(n);
            for (int i = 0; i < n; ++i)
            {
                keys[i] = src.keyAt(i);
                values[i] = src.valueAt(i) * 2;
            }
            return std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
                std::move(keys), std::move(values));
        });

    graph->setData(std::vector<double>{1, 2, 3}, std::vector<double>{4, 5, 6});

    QSignalSpy spy(&graph->pipeline(), &QCPGraphPipeline::finished);
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 1000);

    QCOMPARE(graph->pipeline().result()->valueAt(0), 8.0);
    QCOMPARE(graph->pipeline().result()->valueAt(1), 10.0);
    QCOMPARE(graph->pipeline().result()->valueAt(2), 12.0);

    mPlot->replot(QCustomPlot::rpImmediateRefresh);
}

void TestPipeline::colormap2PipelineDefault()
{
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

void TestPipeline::graph2DataFromExternalThread()
{
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    QSignalSpy spy(&graph->pipeline(), &QCPGraphPipeline::finished);

    graph->pipeline().setTransform(TransformKind::ViewportIndependent,
        [](const QCPAbstractDataSource& src, const ViewportParams&, std::any&)
            -> std::shared_ptr<QCPAbstractDataSource> {
            int n = src.size();
            std::vector<double> k(n), v(n);
            for (int i = 0; i < n; ++i) { k[i] = src.keyAt(i); v[i] = src.valueAt(i); }
            return std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
                std::move(k), std::move(v));
        });

    auto source = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::vector<double>{1, 2, 3}, std::vector<double>{4, 5, 6});

    QThread thread;
    QObject worker;
    worker.moveToThread(&thread);
    thread.start();
    QMetaObject::invokeMethod(&worker, [&]{
        graph->setDataSource(source);
    }, Qt::BlockingQueuedConnection);

    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 1000);
    QVERIFY(graph->pipeline().result() != nullptr);
    QCOMPARE(graph->pipeline().result()->size(), 3);

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

void TestPipeline::pipelineSourceReplacedDuringJob()
{
    QCPPipelineScheduler scheduler(1);
    std::atomic<bool> gate{false};
    std::atomic<bool> started{false};

    QCPGraphPipeline pipeline(&scheduler);
    QSignalSpy spy(&pipeline, &QCPGraphPipeline::finished);

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
    QCPGraphPipeline pipeline(&scheduler);
    QSignalSpy spy(&pipeline, &QCPGraphPipeline::finished);

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

// --- Graph resampler tests ---

void TestPipeline::graphResamplerBinMinMax()
{
    std::vector<double> keys = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    std::vector<double> vals = {5, 3, 8, 1, 7, 2, 9, 4, 6, 0};
    QCPRange keyRange(0, 9);

    auto [outKeys, outVals] = qcp::algo::binMinMax(keys, vals, 0, 10, keyRange, 3);

    QCOMPARE(outKeys.size(), 6u);
    QCOMPARE(outVals.size(), 6u);

    // Bin 0: keys [0,3) → vals {5,3,8} → min=3, max=8
    QCOMPARE(outVals[0], 3.0);
    QCOMPARE(outVals[1], 8.0);

    // Bin 1: keys [3,6) → vals {1,7,2} → min=1, max=7
    QCOMPARE(outVals[2], 1.0);
    QCOMPARE(outVals[3], 7.0);

    // Bin 2: keys [6,9] → vals {9,4,6,0} → min=0, max=9
    QCOMPARE(outVals[4], 0.0);
    QCOMPARE(outVals[5], 9.0);
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
    auto l1 = qcp::algo::binMinMax(keys, vals, 0, N, fullRange, l1Bins);

    QCOMPARE(static_cast<int>(l1.keys.size()), l1Bins * 2);

    // Level 2: zoom into [5000, 10000]
    QCPRange viewport(5000, 10000);
    int l2Bins = 400 * 4;
    auto l2 = qcp::algo::binMinMax(l1.keys, l1.values, 0,
        static_cast<int>(l1.keys.size()), viewport, l2Bins);

    QCOMPARE(static_cast<int>(l2.keys.size()), l2Bins * 2);

    for (size_t i = 0; i < l2.keys.size(); ++i)
    {
        QVERIFY2(l2.keys[i] >= viewport.lower - 1 && l2.keys[i] <= viewport.upper + 1,
            qPrintable(QString("L2 key[%1]=%2 outside viewport").arg(i).arg(l2.keys[i])));
    }
}

void TestPipeline::graphResamplerCacheReuse()
{
    const int N = 100000;
    std::vector<double> keys(N), vals(N);
    for (int i = 0; i < N; ++i)
    {
        keys[i] = i;
        vals[i] = std::sin(i * 0.001);
    }

    auto src = std::make_shared<QCPSoADataSource<
        std::vector<double>, std::vector<double>>>(keys, vals);

    std::any cache;
    ViewportParams vp;
    vp.keyRange = QCPRange(0, N - 1);
    vp.plotWidthPx = 800;

    auto result1 = qcp::algo::hierarchicalResample(*src, vp, cache);
    QVERIFY(!result1); // below 10M threshold

    // Exercise binMinMax and GraphResamplerCache directly since
    // hierarchicalResample requires 10M+ points
    qcp::algo::GraphResamplerCache c;
    c.level1 = qcp::algo::binMinMax(keys, vals, 0, N, QCPRange(0, N - 1), 500);
    c.cachedKeyRange = QCPRange(0, N - 1);
    c.sourceSize = N;

    // Level 2 from cache
    auto l2 = qcp::algo::binMinMax(c.level1.keys, c.level1.values,
        0, static_cast<int>(c.level1.keys.size()), QCPRange(10000, 50000), 800);
    QVERIFY(l2.keys.size() > 0u);

    // Pan: different viewport, same cache
    auto l2b = qcp::algo::binMinMax(c.level1.keys, c.level1.values,
        0, static_cast<int>(c.level1.keys.size()), QCPRange(20000, 25000), 800);
    QVERIFY(l2b.keys.size() > 0u);
}

void TestPipeline::graphResamplerNaNSkipped()
{
    std::vector<double> keys = {0, 1, 2, 3, 4};
    double nan = std::numeric_limits<double>::quiet_NaN();
    std::vector<double> vals = {5, nan, 3, nan, 7};
    QCPRange range(0, 4);

    auto [outKeys, outVals] = qcp::algo::binMinMax(keys, vals, 0, 5, range, 1);

    QCOMPARE(outVals[0], 3.0);
    QCOMPARE(outVals[1], 7.0);
}

void TestPipeline::graphResamplerEmptyBinsProduceNaN()
{
    std::vector<double> keys = {0, 0.5, 1};
    std::vector<double> vals = {10, 20, 30};
    QCPRange range(0, 9);

    auto [outKeys, outVals] = qcp::algo::binMinMax(keys, vals, 0, 3, range, 3);

    // Bin 0: has data
    QVERIFY(!std::isnan(outVals[0]));
    QVERIFY(!std::isnan(outVals[1]));

    // Bins 1 and 2: empty → NaN
    QVERIFY(std::isnan(outVals[2]));
    QVERIFY(std::isnan(outVals[3]));
    QVERIFY(std::isnan(outVals[4]));
    QVERIFY(std::isnan(outVals[5]));
}

void TestPipeline::graph2HierarchicalResamplingActivates()
{
    auto* g = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);

    const int N = 10'000'001;
    std::vector<double> keys(N), vals(N);
    for (int i = 0; i < N; ++i)
    {
        keys[i] = i;
        vals[i] = std::sin(i * 0.0001);
    }

    mPlot->xAxis->setRange(0, N - 1);
    mPlot->yAxis->setRange(-1, 1);
    g->setData(std::move(keys), std::move(vals));
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QVERIFY(g->pipeline().hasTransform());

    QSignalSpy spy(&g->pipeline(), &QCPGraphPipeline::finished);
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 30000);

    auto* result = g->pipeline().result();
    QVERIFY(result);
    QVERIFY(result->size() > 0);
    QVERIFY(result->size() < N / 10);
}

void TestPipeline::graph2SmallDataNoResampling()
{
    auto* g = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);

    std::vector<double> keys = {1, 2, 3, 4, 5};
    std::vector<double> vals = {10, 20, 30, 40, 50};
    g->setData(std::move(keys), std::move(vals));

    QVERIFY(!g->pipeline().hasTransform());

    auto* result = g->pipeline().result();
    QVERIFY(result);
    QCOMPARE(result->size(), 5);
    QCOMPARE(result->keyAt(0), 1.0);
    QCOMPARE(result->valueAt(4), 50.0);
}

void TestPipeline::graph2LargeToSmallDataFallback()
{
    // Bug: when hierarchicalResample returns nullptr (e.g. data below threshold
    // or log axis), draw() got nullptr and rendered nothing instead of falling
    // back to the raw data source.
    auto* g = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);

    // Install the resampling transform (simulates prior large-data assignment)
    g->pipeline().setTransform(TransformKind::ViewportDependent,
        [](const QCPAbstractDataSource& src,
           const ViewportParams& vp,
           std::any& cache) -> std::shared_ptr<QCPAbstractDataSource> {
            return qcp::algo::hierarchicalResample(src, vp, cache);
        });

    // Assign small data — transform stays but returns nullptr
    g->setData(std::vector<double>{1, 2, 3}, std::vector<double>{4, 5, 6});

    // Transform is still installed but result is nullptr (below 10M threshold)
    QVERIFY(g->pipeline().hasTransform());
    QVERIFY(!g->pipeline().result());

    // draw() should fall back to mDataSource and not crash
    mPlot->xAxis->setRange(0, 4);
    mPlot->yAxis->setRange(0, 7);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    // Verify the data source is accessible for rendering
    QVERIFY(g->dataSource());
    QCOMPARE(g->dataSource()->size(), 3);
}

void TestPipeline::graphResamplerBinMinMaxKeyPositions()
{
    // Bug: comment says output is (binCenter, min), (binCenter+halfWidth, max)
    // but code writes binCenter + halfWidth * 0.5 (quarter-width offset)
    std::vector<double> keys = {0, 1, 2, 3};
    std::vector<double> vals = {10, 20, 30, 40};
    QCPRange range(0, 4);

    auto [outKeys, outVals] = qcp::algo::binMinMax(keys, vals, 0, 4, range, 1);

    // Single bin: binWidth=4, halfWidth=2, binCenter=2
    // Expected: (binCenter, min) = (2, 10), (binCenter+halfWidth, max) = (4, 40)
    QCOMPARE(outKeys[0], 2.0);
    QCOMPARE(outKeys[1], 4.0);
}

void TestPipeline::graphResamplerBinMinMaxZeroBins()
{
    // Bug: numBins=0 causes division by zero in binWidth = keyRange.size() / numBins
    std::vector<double> keys = {1, 2, 3};
    std::vector<double> vals = {4, 5, 6};

    auto result = qcp::algo::binMinMax(keys, vals, 0, 3, QCPRange(1, 3), 0);
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
    QCPRange range(0, 4);

    auto [outKeys, outVals] = qcp::algo::binMinMax(keys, vals, 0, 5, range, 1);

    // Only finite keys (0, 2, 4) should contribute → min=10, max=50
    QCOMPARE(outVals[0], 10.0);
    QCOMPARE(outVals[1], 50.0);
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
