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
#include <QSignalSpy>
#include <QThread>
#include <cmath>

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

    // First call builds Level 1 cache + Level 2 output
    auto result1 = qcp::algo::hierarchicalResample(*src, vp, cache);
    // Below 10M threshold — returns nullptr (no resampling needed)
    QVERIFY(!result1);

    // Test with a source that pretends to be large by calling binMinMax directly
    // (hierarchicalResample has a 10M threshold we can't hit in unit tests easily)
    // Instead, verify the cache struct works
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
