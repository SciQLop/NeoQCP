#include "test-waterfall.h"
#include "../../../src/qcp.h"
#include <QCoreApplication>
#include <cmath>

void TestWaterfall::init()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
}

void TestWaterfall::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
}

void TestWaterfall::uniformOffset()
{
    auto* wf = new QCPWaterfallGraph(mPlot->xAxis, mPlot->yAxis);
    wf->setOffsetMode(QCPWaterfallGraph::omUniform);
    wf->setUniformSpacing(10.0);

    std::vector<double> keys = {0.0, 1.0, 2.0};
    std::vector<std::vector<double>> vals = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}, {7.0, 8.0, 9.0}};
    wf->setData(std::move(keys), std::move(vals));

    bool found = false;
    QCPRange range = wf->getValueRange(found);
    QVERIFY(found);
    QCOMPARE(range.lower, -1.0);
    QCOMPARE(range.upper, 21.0);
}

void TestWaterfall::customOffset()
{
    auto* wf = new QCPWaterfallGraph(mPlot->xAxis, mPlot->yAxis);
    wf->setOffsetMode(QCPWaterfallGraph::omCustom);
    wf->setOffsets({50.0, 100.0, 150.0});

    std::vector<double> keys = {0.0, 1.0};
    std::vector<std::vector<double>> vals = {{1.0, -1.0}, {2.0, -2.0}, {0.5, -0.5}};
    wf->setData(std::move(keys), std::move(vals));

    bool found = false;
    QCPRange range = wf->getValueRange(found);
    QVERIFY(found);
    QCOMPARE(range.lower, 49.0);
    QCOMPARE(range.upper, 151.0);
}

void TestWaterfall::customOffsetFallback()
{
    auto* wf = new QCPWaterfallGraph(mPlot->xAxis, mPlot->yAxis);
    wf->setOffsetMode(QCPWaterfallGraph::omCustom);
    wf->setOffsets({50.0});
    wf->setUniformSpacing(10.0);

    std::vector<double> keys = {0.0, 1.0};
    std::vector<std::vector<double>> vals = {{1.0, -1.0}, {2.0, -2.0}, {0.5, -0.5}};
    wf->setData(std::move(keys), std::move(vals));

    bool found = false;
    QCPRange range = wf->getValueRange(found);
    QVERIFY(found);
    QCOMPARE(range.lower, 9.0);
    QCOMPARE(range.upper, 51.0);
}

void TestWaterfall::normalizationFactors()
{
    auto* wf = new QCPWaterfallGraph(mPlot->xAxis, mPlot->yAxis);
    wf->setOffsetMode(QCPWaterfallGraph::omUniform);
    wf->setUniformSpacing(100.0);
    wf->setNormalize(true);
    wf->setGain(10.0);

    std::vector<double> keys = {0.0, 1.0, 2.0};
    std::vector<std::vector<double>> vals = {{5.0, -3.0, 1.0}, {1.0, 2.0, -1.0}};
    wf->setData(std::move(keys), std::move(vals));

    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    bool found = false;
    QCPRange range = wf->getValueRange(found);
    QVERIFY(found);
    QCOMPARE(range.lower, -10.0);
    QCOMPARE(range.upper, 110.0);
}

void TestWaterfall::normalizationDisabled()
{
    auto* wf = new QCPWaterfallGraph(mPlot->xAxis, mPlot->yAxis);
    wf->setOffsetMode(QCPWaterfallGraph::omUniform);
    wf->setUniformSpacing(100.0);
    wf->setNormalize(false);
    wf->setGain(1.0);

    std::vector<double> keys = {0.0, 1.0};
    std::vector<std::vector<double>> vals = {{5.0, -3.0}, {1.0, -1.0}};
    wf->setData(std::move(keys), std::move(vals));

    bool found = false;
    QCPRange range = wf->getValueRange(found);
    QVERIFY(found);
    QCOMPARE(range.lower, -5.0);
    QCOMPARE(range.upper, 105.0);
}

void TestWaterfall::invalidateNormalization()
{
    auto* wf = new QCPWaterfallGraph(mPlot->xAxis, mPlot->yAxis);
    wf->setOffsetMode(QCPWaterfallGraph::omCustom);
    wf->setOffsets({0.0});
    wf->setNormalize(true);
    wf->setGain(1.0);

    auto keys = std::make_shared<std::vector<double>>(std::vector<double>{0.0, 1.0});
    auto vals0 = std::make_shared<std::vector<double>>(std::vector<double>{2.0, -2.0});

    wf->viewData(std::span<const double>(*keys),
                 std::vector<std::span<const double>>{std::span<const double>(*vals0)});

    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QCOMPARE(wf->dataMainValue(0), 1.0);

    (*vals0)[0] = 10.0;
    (*vals0)[1] = -10.0;

    wf->invalidateNormalization();
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QCOMPARE(wf->dataMainValue(0), 1.0);

    bool found = false;
    QCPRange range = wf->getValueRange(found);
    QVERIFY(found);
    QCOMPARE(range.lower, -1.0);
    QCOMPARE(range.upper, 1.0);
}

void TestWaterfall::adapterValueAt()
{
    auto* wf = new QCPWaterfallGraph(mPlot->xAxis, mPlot->yAxis);
    wf->setOffsetMode(QCPWaterfallGraph::omCustom);
    wf->setOffsets({50.0, 100.0});
    wf->setNormalize(false);
    wf->setGain(2.0);

    std::vector<double> keys = {0.0, 1.0};
    std::vector<std::vector<double>> vals = {{3.0, -1.0}, {5.0, 2.0}};
    wf->setData(std::move(keys), std::move(vals));

    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    double v = wf->dataMainValue(0);
    QCOMPARE(v, 56.0);
}

void TestWaterfall::singleComponentValueRange()
{
    auto* wf = new QCPWaterfallGraph(mPlot->xAxis, mPlot->yAxis);
    wf->setOffsetMode(QCPWaterfallGraph::omCustom);
    wf->setOffsets({0.0});
    wf->setNormalize(false);
    wf->setGain(1.0);

    std::vector<double> keys = {0.0, 1.0, 2.0};
    std::vector<std::vector<double>> vals = {{-3.0, 0.0, 5.0}};
    wf->setData(std::move(keys), std::move(vals));

    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    bool found = false;
    QCPRange range = wf->getValueRange(found);
    QVERIFY(found);
    QCOMPARE(range.lower, -5.0);
    QCOMPARE(range.upper, 5.0);
}

void TestWaterfall::getValueRangeNormalized()
{
    auto* wf = new QCPWaterfallGraph(mPlot->xAxis, mPlot->yAxis);
    wf->setOffsetMode(QCPWaterfallGraph::omCustom);
    wf->setOffsets({10.0, 20.0});
    wf->setNormalize(true);
    wf->setGain(3.0);

    std::vector<double> keys = {0.0, 1.0};
    std::vector<std::vector<double>> vals = {{1.0, -1.0}, {2.0, -2.0}};
    wf->setData(std::move(keys), std::move(vals));

    bool found = false;
    QCPRange range = wf->getValueRange(found);
    QVERIFY(found);
    QCOMPARE(range.lower, 7.0);
    QCOMPARE(range.upper, 23.0);
}

void TestWaterfall::getValueRangeUnnormalized()
{
    auto* wf = new QCPWaterfallGraph(mPlot->xAxis, mPlot->yAxis);
    wf->setOffsetMode(QCPWaterfallGraph::omCustom);
    wf->setOffsets({10.0, 20.0});
    wf->setNormalize(false);
    wf->setGain(2.0);

    std::vector<double> keys = {0.0, 1.0};
    std::vector<std::vector<double>> vals = {{3.0, -1.0}, {5.0, -5.0}};
    wf->setData(std::move(keys), std::move(vals));

    bool found = false;
    QCPRange range = wf->getValueRange(found);
    QVERIFY(found);
    QCOMPARE(range.lower, 0.0);
    QCOMPARE(range.upper, 30.0);
}

void TestWaterfall::getValueRangeSignDomain()
{
    auto* wf = new QCPWaterfallGraph(mPlot->xAxis, mPlot->yAxis);
    wf->setOffsetMode(QCPWaterfallGraph::omCustom);
    wf->setOffsets({10.0, 20.0});
    wf->setNormalize(true);
    wf->setGain(3.0);

    std::vector<double> keys = {0.0, 1.0};
    std::vector<std::vector<double>> vals = {{1.0, -1.0}, {2.0, -2.0}};
    wf->setData(std::move(keys), std::move(vals));

    bool found = false;

    QCPRange posRange = wf->getValueRange(found, QCP::sdPositive);
    QVERIFY(found);
    QCOMPARE(posRange.lower, 7.0);
    QCOMPARE(posRange.upper, 23.0);

    wf->getValueRange(found, QCP::sdNegative);
    QVERIFY(!found);
}

void TestWaterfall::drawDoesNotCrash()
{
    auto* wf = new QCPWaterfallGraph(mPlot->xAxis, mPlot->yAxis);
    wf->setOffsetMode(QCPWaterfallGraph::omCustom);
    wf->setOffsets({50.0, 100.0, 150.0});
    wf->setNormalize(true);
    wf->setGain(15.0);

    std::vector<double> keys = {0.0, 1.0, 2.0, 3.0, 4.0};
    std::vector<std::vector<double>> vals = {
        {1.0, 0.5, -0.3, 0.8, -0.2},
        {0.3, -0.7, 1.0, -0.5, 0.1},
        {-0.5, 0.9, 0.2, -1.0, 0.4}
    };
    wf->setData(std::move(keys), std::move(vals));

    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    new QCPWaterfallGraph(mPlot->xAxis, mPlot->yAxis);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
}

void TestWaterfall::parameterChangesApplyWithoutDraw()
{
    // Gain/offset/normalize changes must be visible in the data source
    // immediately — not only after the next draw() happened to refresh the
    // shared adapter (stale-draw bug: the line cache redrew old amplitudes
    // while autoscale already used the new ones).
    auto* wf = new QCPWaterfallGraph(mPlot->xAxis, mPlot->yAxis);
    wf->setOffsetMode(QCPWaterfallGraph::omUniform);
    wf->setUniformSpacing(0.0);
    wf->setNormalize(false);
    wf->setGain(1.0);

    std::vector<double> keys = {0.0, 1.0};
    std::vector<std::vector<double>> vals = {{1.0, 2.0}};
    wf->setData(std::move(keys), std::move(vals));

    QCOMPARE(wf->dataSource()->valueAt(0, 0), 1.0);

    wf->setGain(3.0);
    QCOMPARE(wf->dataSource()->valueAt(0, 0), 3.0);

    wf->setOffsetMode(QCPWaterfallGraph::omCustom);
    wf->setOffsets({10.0});
    QCOMPARE(wf->dataSource()->valueAt(0, 0), 13.0);
}

void TestWaterfall::concurrentParameterAndDataChangesAreSafe()
{
    // Data and parameter changes while L1 resample jobs are in flight must
    // never mutate state those jobs read (shared adapter members, inner
    // source swap) — pre-fix this is a data race / UAF caught by ASan.
    constexpr int n = 80'000; // x2 columns > kResampleThreshold
    auto* wf = new QCPWaterfallGraph(mPlot->xAxis, mPlot->yAxis);
    wf->setNormalize(true);

    for (int iter = 0; iter < 10; ++iter)
    {
        std::vector<double> keys(n);
        std::vector<std::vector<double>> vals(2, std::vector<double>(n));
        for (int i = 0; i < n; ++i)
        {
            keys[i] = i;
            vals[0][i] = std::sin(i * 0.001) * (iter + 1);
            vals[1][i] = std::cos(i * 0.001) * (iter + 1);
        }
        wf->setData(std::move(keys), std::move(vals)); // kicks an L1 job
        wf->setGain(1.0 + iter);                        // raced the job pre-fix
        wf->setUniformSpacing(2.0 + iter);
        mPlot->replot(QCustomPlot::rpImmediateRefresh); // draw raced it too
        QCoreApplication::processEvents();
    }
    // drain in-flight jobs before the plot dies
    QTRY_VERIFY_WITH_TIMEOUT(!wf->pipeline().isBusy(), 10000);
}
