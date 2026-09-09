#include "test-busy-indicator.h"
#include <qcustomplot.h>
#include <painting/plottable-rhi-layer.h>
#include <QTest>

#include <cmath>

namespace {
bool showAndHasRhiBusy(QCustomPlot* plot)
{
    plot->show();
    if (!QTest::qWaitForWindowExposed(plot))
        return false;
    QCoreApplication::processEvents();
    return plot->rhi() != nullptr;
}
} // namespace

void TestBusyIndicator::init()
{
    mPlot = new QCustomPlot(nullptr);
    mPlot->show();
}

void TestBusyIndicator::cleanup()
{
    delete mPlot;
}

void TestBusyIndicator::externalBusyDefault()
{
    auto* g = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    QCOMPARE(g->busy(), false);
    QCOMPARE(g->visuallyBusy(), false);
}

void TestBusyIndicator::setBusyEmitsBusyChanged()
{
    auto* g = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    QSignalSpy spy(g, &QCPAbstractPlottable::busyChanged);
    g->setBusy(true);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), true);
    g->setBusy(false);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(0).toBool(), false);
}

void TestBusyIndicator::debounceShowDelay()
{
    auto* g = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    g->setBusyShowDelayMs(100);
    QSignalSpy spy(g, &QCPAbstractPlottable::visuallyBusyChanged);

    g->setBusy(true);
    QCOMPARE(g->visuallyBusy(), false);

    QTest::qWait(150);
    QCOMPARE(g->visuallyBusy(), true);
    QCOMPARE(spy.count(), 1);
}

void TestBusyIndicator::debounceHideDelay()
{
    auto* g = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    g->setBusyShowDelayMs(10);
    g->setBusyHideDelayMs(100);

    g->setBusy(true);
    QTest::qWait(50);
    QCOMPARE(g->visuallyBusy(), true);

    QSignalSpy spy(g, &QCPAbstractPlottable::visuallyBusyChanged);
    g->setBusy(false);
    QCOMPARE(g->visuallyBusy(), true); // still shown (hide delay)

    QTest::qWait(150);
    QCOMPARE(g->visuallyBusy(), false);
    QCOMPARE(spy.count(), 1);
}

void TestBusyIndicator::fastToggleNoVisualChange()
{
    auto* g = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    g->setBusyShowDelayMs(100);
    QSignalSpy spy(g, &QCPAbstractPlottable::visuallyBusyChanged);

    g->setBusy(true);
    QTest::qWait(30);
    g->setBusy(false);

    QTest::qWait(150);
    QCOMPARE(g->visuallyBusy(), false);
    QCOMPARE(spy.count(), 0);
}

void TestBusyIndicator::perPlottableOverridesTheme()
{
    auto* g = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    QCOMPARE(g->effectiveBusyFadeAlpha(), 0.3);
    g->setBusyFadeAlpha(0.5);
    QCOMPARE(g->effectiveBusyFadeAlpha(), 0.5);
}

void TestBusyIndicator::resetFallsBackToTheme()
{
    auto* g = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    g->setBusyFadeAlpha(0.5);
    QCOMPARE(g->effectiveBusyFadeAlpha(), 0.5);
    g->resetBusyFadeAlpha();
    QCOMPARE(g->effectiveBusyFadeAlpha(), 0.3);
}

void TestBusyIndicator::pipelineBusyContributesToEffective()
{
    auto* g = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    g->setBusyShowDelayMs(10);

    const int N = 10'000'000;
    std::vector<double> keys(N), values(N);
    for (int i = 0; i < N; ++i) {
        keys[i] = i;
        values[i] = i * 0.01;
    }
    g->setData(std::move(keys), std::move(values));

    QCOMPARE(g->busy(), true);
    QTRY_COMPARE_WITH_TIMEOUT(g->busy(), false, 10000);
}

void TestBusyIndicator::busyPlottableDrawsFaded()
{
    auto* g = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    g->setData(std::vector<double>{1.0, 2.0, 3.0}, std::vector<double>{1.0, 2.0, 3.0});
    g->setBusyShowDelayMs(0);
    g->setBusyHideDelayMs(0);
    g->setBusy(true);
    QTest::qWait(50);
    QCOMPARE(g->visuallyBusy(), true);

    QPixmap busyPixmap = mPlot->toPixmap(200, 200);

    g->setBusy(false);
    QTest::qWait(50);

    QPixmap normalPixmap = mPlot->toPixmap(200, 200);

    QVERIFY(busyPixmap.toImage() != normalPixmap.toImage());
}

void TestBusyIndicator::notBusyPlottableDrawsFullOpacity()
{
    auto* g = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    g->setData(std::vector<double>{1.0, 2.0, 3.0}, std::vector<double>{1.0, 2.0, 3.0});
    QCOMPARE(g->visuallyBusy(), false);

    QPixmap pix = mPlot->toPixmap(200, 200);
    QVERIFY(!pix.isNull());
}

void TestBusyIndicator::legendShowsPrefixWhenBusy()
{
    auto* g = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    g->setName("TestGraph");
    g->setData(std::vector<double>{1.0, 2.0}, std::vector<double>{1.0, 2.0});
    g->addToLegend();
    mPlot->replot();

    QPixmap normalPix = mPlot->toPixmap(400, 300);

    g->setBusyShowDelayMs(0);
    g->setBusy(true);
    QTest::qWait(50);
    QCOMPARE(g->visuallyBusy(), true);
    mPlot->replot();

    QPixmap busyPix = mPlot->toPixmap(400, 300);

    QVERIFY(normalPix.toImage() != busyPix.toImage());
}

void TestBusyIndicator::legendSizeHintAccountsForPrefix()
{
    auto* g = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    g->setName("TestGraph");
    g->addToLegend();

    QCPLayoutElement* legendItem = mPlot->legend->itemWithPlottable(g);
    QVERIFY(legendItem);

    QSize normalSize = legendItem->minimumOuterSizeHint();

    g->setBusyShowDelayMs(0);
    g->setBusy(true);
    QTest::qWait(50);

    QSize busySize = legendItem->minimumOuterSizeHint();

    QVERIFY(busySize.width() > normalSize.width());
}

void TestBusyIndicator::groupLegendShowsBusyPrefix()
{
    mPlot->legend->setVisible(true);
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(std::vector<double>{1.0, 2.0, 3.0},
                std::vector<std::vector<double>>{{10.0, 20.0, 30.0}, {-1.0, -2.0, -3.0}});
    mg->addToLegend();
    mPlot->replot();

    QPixmap normalPix = mPlot->toPixmap(400, 300);

    mg->setBusyShowDelayMs(0);
    mg->setBusy(true);
    QTest::qWait(50);
    QCOMPARE(mg->visuallyBusy(), true);
    mPlot->replot();

    QPixmap busyPix = mPlot->toPixmap(400, 300);
    QVERIFY(normalPix.toImage() != busyPix.toImage());
}

void TestBusyIndicator::fullLifecycleExternalBusy()
{
    // Simulate SciQLopPlots usage: download starts, data arrives, resampling finishes
    auto* g = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    g->setName("Magnetic Field Bx");
    g->addToLegend();
    g->setBusyShowDelayMs(50);
    g->setBusyHideDelayMs(50);

    QSignalSpy busySpy(g, &QCPAbstractPlottable::busyChanged);
    QSignalSpy visualSpy(g, &QCPAbstractPlottable::visuallyBusyChanged);

    // 1. Download starts
    g->setBusy(true);
    QCOMPARE(busySpy.count(), 1);
    QCOMPARE(g->busy(), true);
    QCOMPARE(g->visuallyBusy(), false); // debounce not yet fired

    // 2. Wait for visual busy to show
    QTest::qWait(100);
    QCOMPARE(g->visuallyBusy(), true);
    QCOMPARE(visualSpy.count(), 1);

    // 3. Data arrives — set data and clear external busy
    g->setData(std::vector<double>{1.0, 2.0, 3.0, 4.0, 5.0}, std::vector<double>{10.0, 20.0, 15.0, 25.0, 30.0});
    g->setBusy(false);

    // 4. Visual should stay busy for hide delay
    QCOMPARE(g->visuallyBusy(), true);
    QTest::qWait(100);
    QCOMPARE(g->visuallyBusy(), false);
    QCOMPARE(visualSpy.count(), 2);
}

void TestBusyIndicator::visualBusyToggleForcesLayerRepaint()
{
    // A fade toggle must not be swallowed by the translate-instead-of-repaint
    // path: the layer buffer is invalidated so the next replot rebuilds the
    // GPU entries with the new alpha.
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    QVector<double> keys(1000), values(1000);
    for (int i = 0; i < 1000; ++i)
    {
        keys[i] = i;
        values[i] = std::sin(i * 0.01);
    }
    graph->setData(std::move(keys), std::move(values));
    mPlot->xAxis->setRange(0, 1000);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTRY_VERIFY_WITH_TIMEOUT(!graph->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QCPLayer* mainLayer = mPlot->layer("main");
    QVERIFY(mainLayer);

    mPlot->xAxis->setRange(50, 1050);
    QVERIFY(mainLayer->canSkipRepaintForTranslation());

    // Sampled inside the toggle signal, before the queued replot can run:
    // the offset is still valid (it is a pan) yet the layer refuses to translate.
    bool invalidatedAtToggle = false;
    connect(graph, &QCPAbstractPlottable::visuallyBusyChanged, this, [&](bool) {
        invalidatedAtToggle = !mainLayer->pixelOffset().isNull()
            && !mainLayer->canSkipRepaintForTranslation();
    });

    graph->setBusyShowDelayMs(0);
    graph->setBusyHideDelayMs(0);
    graph->setBusy(true);
    QTRY_VERIFY_WITH_TIMEOUT(graph->visuallyBusy(), 2000);
    QVERIFY(invalidatedAtToggle);

    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    mPlot->xAxis->setRange(100, 1100);
    QVERIFY(mainLayer->canSkipRepaintForTranslation());

    invalidatedAtToggle = false;
    graph->setBusy(false);
    QTRY_VERIFY_WITH_TIMEOUT(!graph->visuallyBusy(), 2000);
    QVERIFY(invalidatedAtToggle);
}

void TestBusyIndicator::gpuEntriesCarryFadeAlpha()
{
    if (!showAndHasRhiBusy(mPlot))
        QSKIP("No QRhi available — GPU draw entries need a real backend");

    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    QVector<double> keys(1000), values(1000);
    for (int i = 0; i < 1000; ++i)
    {
        keys[i] = i;
        values[i] = std::sin(i * 0.01);
    }
    graph->setData(std::move(keys), std::move(values));
    graph->setBusyShowDelayMs(0);
    graph->setBusyHideDelayMs(0);
    mPlot->xAxis->setRange(0, 1000);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTRY_VERIFY_WITH_TIMEOUT(!graph->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    auto* prl = mPlot->plottableRhiLayer(mPlot->layer("main"));
    QVERIFY(prl);
    QVERIFY(!prl->drawEntries().isEmpty());
    for (const auto& e : prl->drawEntries())
        QCOMPARE(e.alpha, 1.0f);

    graph->setBusy(true);
    QTRY_VERIFY_WITH_TIMEOUT(graph->visuallyBusy(), 2000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(!prl->drawEntries().isEmpty());
    const float fade = static_cast<float>(graph->effectiveBusyFadeAlpha());
    for (const auto& e : prl->drawEntries())
        QVERIFY(qFuzzyCompare(e.alpha, fade));

    graph->setBusy(false);
    QTRY_VERIFY_WITH_TIMEOUT(!graph->visuallyBusy(), 2000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    for (const auto& e : prl->drawEntries())
        QCOMPARE(e.alpha, 1.0f);
}
