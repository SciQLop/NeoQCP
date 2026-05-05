#include "test-pipeline.h"
#include <qcustomplot.h>
#include <datasource/pipeline-scheduler.h>
#include <datasource/async-pipeline.h>
#include <datasource/soa-datasource.h>
#include <datasource/soa-datasource-2d.h>
#include <datasource/soa-multi-datasource.h>
#include <plottables/plottable-graph2.h>
#include <plottables/plottable-colormap2.h>
#include <plottables/plottable-colormap.h>
#include <datasource/graph-resampler.h>
#include <datasource/resampled-multi-datasource.h>
#include <datasource/histogram-binner.h>
#include <plottables/plottable-histogram2d.h>
#include <plottables/plottable-multigraph.h>
#include <plottables/plottable-waterfall.h>
#include <QSignalSpy>
#include <QThread>
#include <QtWidgets/qtestsupport_widgets.h> // QTest::qWaitForWindowExposed
#include <cmath>
#include <limits>

namespace {
// Combined L1+L2 helper — used only by tests below.
std::shared_ptr<QCPAbstractDataSource> hierarchicalResample(
    const QCPAbstractDataSource& src,
    const ViewportParams& vp,
    std::any& cache)
{
    const int srcSize = src.size();
    if (srcSize == 0 || srcSize < qcp::algo::kResampleThreshold || vp.keyLogScale)
        return nullptr;
    qcp::algo::buildL1Cache(src, vp, cache);
    auto* c = std::any_cast<qcp::algo::GraphResamplerCache>(&cache);
    if (!c) return nullptr;
    return qcp::algo::resampleL2(*c, vp);
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
// GPU-backed plottables (Graph2/MultiGraph/ColorMap2 — whose draw() and
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

    // Set data first, then install transform (setData triggers
    // ensureResamplingTransform which would clear a manually-installed transform
    // for small datasets).
    graph->setData(std::vector<double>{1, 2, 3}, std::vector<double>{4, 5, 6});

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
    graph->dataChanged();

    QSignalSpy spy(&graph->pipeline(), &QCPGraphPipeline::finished);
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 1000);

    QCOMPARE(graph->pipeline().result()->valueAt(0), 8.0);
    QCOMPARE(graph->pipeline().result()->valueAt(1), 10.0);
    QCOMPARE(graph->pipeline().result()->valueAt(2), 12.0);

    mPlot->replot(QCustomPlot::rpImmediateRefresh);
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

void TestPipeline::graph2DataFromExternalThread()
{
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);

    auto source = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::vector<double>{1, 2, 3}, std::vector<double>{4, 5, 6});

    // Install transform after setDataSource (setDataSource clears transforms
    // for small datasets via ensureResamplingTransform).
    QThread thread;
    QObject worker;
    worker.moveToThread(&thread);
    thread.start();
    QMetaObject::invokeMethod(&worker, [&]{
        graph->setDataSource(source);
    }, Qt::BlockingQueuedConnection);

    graph->pipeline().setTransform(TransformKind::ViewportIndependent,
        [](const QCPAbstractDataSource& src, const ViewportParams&, std::any&)
            -> std::shared_ptr<QCPAbstractDataSource> {
            int n = src.size();
            std::vector<double> k(n), v(n);
            for (int i = 0; i < n; ++i) { k[i] = src.keyAt(i); v[i] = src.valueAt(i); }
            return std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
                std::move(k), std::move(v));
        });
    graph->dataChanged();

    QSignalSpy spy(&graph->pipeline(), &QCPGraphPipeline::finished);
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
    const int N = 10000;
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

    auto result1 = hierarchicalResample(*src, vp, cache);
    QVERIFY(!result1); // below threshold, no cache built

    // Exercise binMinMax and GraphResamplerCache directly
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

// Lightweight data source that computes data on-the-fly without allocating 10M+ arrays.
class SyntheticLargeSource : public QCPAbstractDataSource
{
public:
    explicit SyntheticLargeSource(int n) : mN(n) {}
    int size() const override { return mN; }
    bool empty() const override { return mN == 0; }
    double keyAt(int i) const override { return static_cast<double>(i); }
    double valueAt(int i) const override { return std::sin(i * 0.0001); }
    QCPRange keyRange(bool& found, QCP::SignDomain sd) const override
    {
        Q_UNUSED(sd);
        found = mN > 0;
        return QCPRange(0, mN - 1);
    }
    QCPRange valueRange(bool& found, QCP::SignDomain sd, const QCPRange&) const override
    {
        Q_UNUSED(sd);
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
    QVector<QPointF> getOptimizedLineData(int begin, int end, int, QCPAxis*, QCPAxis*) const override
    {
        return getLines(begin, end, nullptr, nullptr);
    }
    QVector<QPointF> getLines(int begin, int end, QCPAxis*, QCPAxis*) const override
    {
        QVector<QPointF> result;
        result.reserve(end - begin);
        for (int i = begin; i < end; ++i)
            result.append(QPointF(keyAt(i), valueAt(i)));
        return result;
    }
private:
    int mN;
};

void TestPipeline::graph2HierarchicalResamplingActivates()
{
    auto* g = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);

    const int N = 10'000'001;
    auto source = std::make_shared<SyntheticLargeSource>(N);

    QSignalSpy spy(&g->pipeline(), &QCPGraphPipeline::finished);

    mPlot->xAxis->setRange(0, N - 1);
    mPlot->yAxis->setRange(-1, 1);
    g->setDataSource(source);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QVERIFY(g->pipeline().hasTransform());

    // Wait for async L1 build to finish
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 30000);

    // After L1 delivery, onL1Ready runs L2 synchronously.
    // Trigger a replot to exercise the draw path with resampled data.
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    // Pipeline result is nullptr (L1-only transform returns nullptr).
    QVERIFY(g->pipeline().result() == nullptr);
    QVERIFY(g->dataSource()->size() == N);

    // Verify L2 was actually built and is much smaller than raw data.
    QVERIFY(g->mL2Result);
    QVERIFY(g->mL2Result->size() > 0);
    QVERIFY(g->mL2Result->size() < N / 10);
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
    // When a graph that previously had a large dataset (with resampling transform)
    // receives a smaller dataset, the transform should be cleared so the raw
    // data source is used directly — no nullptr result, no blank rendering.
    auto* g = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);

    // Install the resampling transform (simulates prior large-data assignment)
    g->pipeline().setTransform(TransformKind::ViewportDependent,
        [](const QCPAbstractDataSource& src,
           const ViewportParams& vp,
           std::any& cache) -> std::shared_ptr<QCPAbstractDataSource> {
            return hierarchicalResample(src, vp, cache);
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

void TestPipeline::graphResamplerParallelMatchesSingleThreaded()
{
    // Generate 2M points (above the 1M parallel threshold in binMinMaxParallel)
    const int N = 2'000'000;
    std::vector<double> keys(N), vals(N);
    for (int i = 0; i < N; ++i)
    {
        keys[i] = i;
        vals[i] = std::sin(i * 0.0001) * 100.0;
    }

    auto src = std::make_shared<QCPSoADataSource<
        std::vector<double>, std::vector<double>>>(std::move(keys), std::move(vals));

    QCPRange fullRange(0, N - 1);
    int numBins = 1000;

    auto sequential = qcp::algo::binMinMax(*src, 0, N, fullRange, numBins);
    auto parallel = qcp::algo::binMinMaxParallel(*src, 0, N, fullRange, numBins);

    QCOMPARE(parallel.keys.size(), sequential.keys.size());
    QCOMPARE(parallel.values.size(), sequential.values.size());

    for (size_t i = 0; i < sequential.keys.size(); ++i)
        QCOMPARE(parallel.keys[i], sequential.keys[i]);

    for (size_t i = 0; i < sequential.values.size(); ++i)
    {
        if (std::isnan(sequential.values[i]))
            QVERIFY(std::isnan(parallel.values[i]));
        else
            QCOMPARE(parallel.values[i], sequential.values[i]);
    }
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

void TestPipeline::graph2L2RebuildDeferredToDraw()
{
    // Verify that changing the viewport does NOT immediately rebuild L2,
    // but that L2 IS rebuilt when draw() runs (via replot).
    auto* g = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    const int N = 10'000'001;
    auto source = std::make_shared<SyntheticLargeSource>(N);

    QSignalSpy spy(&g->pipeline(), &QCPGraphPipeline::finished);
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

void TestPipeline::graph2L2CoalescesMultipleViewportChanges()
{
    // Simulate rapid pan: many range changes, one replot.
    // L2 should only be built once (at draw time), not per range change.
    auto* g = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    const int N = 10'000'001;
    auto source = std::make_shared<SyntheticLargeSource>(N);

    QSignalSpy spy(&g->pipeline(), &QCPGraphPipeline::finished);
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

void TestPipeline::graph2L2DirtyAfterL1Ready()
{
    // When L1 finishes, L2 should be marked dirty (not built inline).
    auto* g = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    const int N = 10'000'001;
    auto source = std::make_shared<SyntheticLargeSource>(N);

    mPlot->xAxis->setRange(0, N - 1);
    mPlot->yAxis->setRange(-1, 1);

    QSignalSpy spy(&g->pipeline(), &QCPGraphPipeline::finished);
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

// --- QCPHistogram2D tests ---

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

void TestPipeline::histogram2dNormalizationColumn()
{
    auto* hist = new QCPHistogram2D(mPlot->xAxis, mPlot->yAxis);
    // col 0: counts [3, 1] (sum=4), col 1: counts [0, 2] (sum=2)
    std::vector<double> keys = {0.25, 0.25, 0.25, 0.25, 0.75, 0.75};
    std::vector<double> vals = {0.25, 0.25, 0.25, 0.75, 0.75, 0.75};
    hist->setBins(2, 2);
    hist->setNormalization(QCPHistogram2D::nColumn);
    hist->setData(std::move(keys), std::move(vals));

    QSignalSpy spy(&hist->pipeline(), &QCPHistogramPipeline::finished);
    QVERIFY(spy.wait(2000));

    auto* data = hist->pipeline().result();
    QVERIFY(data);
    QCOMPARE(data->cell(0, 0), 3.0);
    QCOMPARE(data->cell(0, 1), 1.0);
    QCOMPARE(data->cell(1, 0), 0.0);
    QCOMPARE(data->cell(1, 1), 2.0);

    // Render with normalization — exercises the draw() normalization lambda
    hist->rescaleDataRange();
    mPlot->rescaleAxes();
    mPlot->replot();
    QCoreApplication::processEvents();
}

void TestPipeline::histogram2dNormalizationToggleNoRebind()
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

    // Toggling normalization should NOT trigger a new pipeline run
    hist->setNormalization(QCPHistogram2D::nColumn);
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
    auto l2 = qcp::algo::resampleL2Multi(*c, vp);
    QVERIFY(l2 != nullptr);
    QCOMPARE(l2->columnCount(), 2);
    QVERIFY(l2->size() > 0);
    QVERIFY(l2->keyAt(0) >= 0.0);
}

// --- QCPMultiGraph pipeline integration tests ---

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

void TestPipeline::graph2LineCacheReusedOnSmallPan()
{
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    const int N = 10000;
    std::vector<double> keys(N), values(N);
    for (int i = 0; i < N; ++i) { keys[i] = i; values[i] = std::sin(i * 0.01); }
    graph->setDataSource(std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::vector<double>(keys), std::vector<double>(values)));
    mPlot->xAxis->setRange(0, N);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    // Cache should be populated
    QVERIFY(!graph->mCachedLines.isEmpty());
    auto cachedBefore = graph->mCachedLines;

    // Small pan: shift by 5% of range — should reuse cache
    double shift = N * 0.05;
    mPlot->xAxis->setRange(shift, N + shift);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    // Cache should NOT have been rebuilt (same data)
    QCOMPARE(graph->mCachedLines, cachedBefore);
}

void TestPipeline::graph2LineCacheRebuiltOnLargePan()
{
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    const int N = 10000;
    std::vector<double> keys(N), values(N);
    for (int i = 0; i < N; ++i) { keys[i] = i; values[i] = std::sin(i * 0.01); }
    graph->setDataSource(std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::vector<double>(keys), std::vector<double>(values)));
    mPlot->xAxis->setRange(0, N);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    auto cachedBefore = graph->mCachedLines;
    QVERIFY(!cachedBefore.isEmpty());

    // Large pan: shift by 110% — should trigger rebuild (threshold is now 100%)
    double shift = N * 1.1;
    mPlot->xAxis->setRange(shift, N + shift);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    // Cache should have been rebuilt (different data)
    QVERIFY(graph->mCachedLines != cachedBefore);
}

void TestPipeline::graph2LineCacheSurvives75PercentPan()
{
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
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

void TestPipeline::graph2LineCacheRebuiltOnZoom()
{
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    const int N = 10000;
    std::vector<double> keys(N), values(N);
    for (int i = 0; i < N; ++i) { keys[i] = i; values[i] = std::sin(i * 0.01); }
    graph->setDataSource(std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::vector<double>(keys), std::vector<double>(values)));
    mPlot->xAxis->setRange(0, N);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    auto cachedBefore = graph->mCachedLines;
    QVERIFY(!cachedBefore.isEmpty());

    // Zoom in 2x (center stays same, range halved) — should trigger rebuild
    mPlot->xAxis->setRange(N * 0.25, N * 0.75);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QVERIFY(graph->mCachedLines != cachedBefore);
}

void TestPipeline::graph2LineCacheRebuiltOnSmallZoom()
{
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    const int N = 10000;
    std::vector<double> keys(N), values(N);
    for (int i = 0; i < N; ++i) { keys[i] = i; values[i] = std::sin(i * 0.01); }
    graph->setDataSource(std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::vector<double>(keys), std::vector<double>(values)));
    mPlot->xAxis->setRange(0, N);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    auto cachedBefore = graph->mCachedLines;
    QVERIFY(!cachedBefore.isEmpty());

    // Zoom by 0.5% — simulates Wayland smooth scrolling fine zoom step.
    // Must still trigger cache rebuild (translation-only offset is wrong for zoom).
    double half = N * 0.5;
    double shrink = N * 0.0025; // 0.5% of range on each side
    mPlot->xAxis->setRange(half - (N / 2.0 - shrink), half + (N / 2.0 - shrink));
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QVERIFY(graph->mCachedLines != cachedBefore);
}

void TestPipeline::graph2LineCacheInvalidatedOnDataChange()
{
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    const int N = 10000;
    std::vector<double> keys(N), values(N);
    for (int i = 0; i < N; ++i) { keys[i] = i; values[i] = std::sin(i * 0.01); }
    graph->setDataSource(std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::vector<double>(keys), std::vector<double>(values)));
    mPlot->xAxis->setRange(0, N);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QVERIFY(!graph->mCachedLines.isEmpty());

    // Replace data — cache must be invalidated
    std::vector<double> keys2(N), values2(N);
    for (int i = 0; i < N; ++i) { keys2[i] = i; values2[i] = std::cos(i * 0.01); }
    graph->setDataSource(std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::move(keys2), std::move(values2)));

    QVERIFY(graph->mCachedLines.isEmpty());
    QVERIFY(graph->mLineCacheDirty);
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

    QVERIFY(!mg->mCachedLines.isEmpty());
    auto cachedBefore = mg->mCachedLines;

    // Small pan
    double shift = N * 0.05;
    mPlot->xAxis->setRange(shift, N + shift);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QCOMPARE(mg->mCachedLines, cachedBefore);
}

void TestPipeline::graph2FastPanNeverBlank()
{
    if (!showAndHasRhi(mPlot)) QSKIP("No QRhi available — hasRenderedRange() requires GPU draw()");

    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);

    int N = 500'000;
    std::vector<double> keys(N), vals(N);
    for (int i = 0; i < N; ++i) { keys[i] = i; vals[i] = std::sin(i * 0.0001); }
    graph->setData(std::move(keys), std::move(vals));

    mPlot->xAxis->setRange(0, 100000);
    mPlot->yAxis->setRange(-1.5, 1.5);

    for (int p = 0; p < 20; ++p)
    {
        double lo = mPlot->xAxis->range().lower + 30000;
        mPlot->xAxis->setRange(lo, lo + 100000);
        mPlot->replot();
        QCoreApplication::processEvents();
        QVERIFY(graph->hasRenderedRange());
    }
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
