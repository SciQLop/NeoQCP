#include "test-signal-delivery.h"
#include <memory>

namespace
{
QCustomPlot* plotWithGraph()
{
    auto* plot = new QCustomPlot;
    plot->resize(400, 300);
    plot->addGraph()->setData(QVector<double>{0, 10}, QVector<double>{0, 10});
    plot->xAxis->setRange(0, 10);
    plot->yAxis->setRange(0, 10);
    return plot;
}

using PlotSignal = void (QCustomPlot::*)();

PlotSignal replotSignal(const QString& name)
{
    if (name == QLatin1String("beforeReplot"))
        return &QCustomPlot::beforeReplot;
    return name == QLatin1String("afterLayout") ? &QCustomPlot::afterLayout
                                                : &QCustomPlot::afterReplot;
}

QList<QCPLayerable*> layerablesOnLayers(const QCustomPlot& plot)
{
    QList<QCPLayerable*> result;
    for (int i = 0; i < plot.layerCount(); ++i)
        result += plot.layer(i)->children();
    return result;
}
} // namespace

void TestSignalDelivery::nestedScopeDeliversAtOutermost()
{
    QCustomPlot plot;
    bool closing = false;
    int runs = 0;
    bool ranWhileClosing = false;
    {
        QCPEmissionScope outer(&plot);
        {
            QCPEmissionScope inner(&plot);
            plot.deliver(&plot, [&] {
                ++runs;
                ranWhileClosing = closing;
            });
        }
        QCOMPARE(runs, 0);
        closing = true;
    }
    QCOMPARE(runs, 1);
    QVERIFY(ranWhileClosing);
}

void TestSignalDelivery::deadSenderSkipped()
{
    QCustomPlot plot;
    auto* sender = new QObject;
    bool ran = false;
    {
        QCPEmissionScope scope(&plot);
        plot.deliver(sender, [&] { ran = true; });
        delete sender;
    }
    QVERIFY(!ran);
}

void TestSignalDelivery::inputScopeDeliversNextTurn()
{
    QCustomPlot plot;
    bool ran = false;
    {
        QCPInputScope scope(&plot);
        plot.deliver(&plot, [&] { ran = true; });
    }
    QVERIFY(!ran);
    QCoreApplication::processEvents();
    QVERIFY(ran);
}

void TestSignalDelivery::pendingInputSignalsDieWithThePlot()
{
    auto* plot = plotWithGraph();
    QObject outlivesThePlot;
    bool ran = false;
    {
        QCPInputScope scope(plot);
        plot->deliver(&outlivesThePlot, [&ran] { ran = true; });
    }
    delete plot;
    QVERIFY2(!ran, "a queued emission ran during the plot's teardown");
    QCoreApplication::processEvents();
    QVERIFY(!ran);
}

void TestSignalDelivery::slotDeletesPlotDuringReplot_data()
{
    QTest::addColumn<QString>("signal");
    for (const char* name : {"beforeReplot", "afterLayout", "afterReplot"})
        QTest::newRow(name) << QString::fromLatin1(name);
}

void TestSignalDelivery::slotDeletesPlotDuringReplot()
{
    QFETCH(QString, signal);
    auto* plot = plotWithGraph();
    const QPointer<QCustomPlot> alive(plot);
    connect(plot, replotSignal(signal), plot, [plot] { delete plot; });
    plot->replot();
    QVERIFY(!alive);
}

void TestSignalDelivery::replotSignalsWaitForReplotToFinish()
{
    std::unique_ptr<QCustomPlot> plot(plotWithGraph());
    QList<bool> replotting;
    const auto record = [&] { replotting << plot->isReplotting(); };
    connect(plot.get(), &QCustomPlot::beforeReplot, plot.get(), record);
    connect(plot.get(), &QCustomPlot::afterLayout, plot.get(), record);
    connect(plot.get(), &QCustomPlot::afterReplot, plot.get(), record);
    plot->replot();
    QCOMPARE(replotting, QList<bool>({false, false, false}));
}

void TestSignalDelivery::slotDeletesAxisDuringRescale()
{
    std::unique_ptr<QCustomPlot> plot(plotWithGraph());
    QCPAxisRect* rect = plot->axisRect();
    QCPAxis* x = plot->xAxis;
    QCPAxis* y = plot->yAxis;
    bool removed = false;
    const auto removeOther = [&](QCPAxis* other) {
        if (!std::exchange(removed, true))
            rect->removeAxis(other);
    };
    connect(x, qOverload<const QCPRange&>(&QCPAxis::rangeChanged), plot.get(),
            [&, y] { removeOther(y); });
    connect(y, qOverload<const QCPRange&>(&QCPAxis::rangeChanged), plot.get(),
            [&, x] { removeOther(x); });
    plot->graph(0)->setData(QVector<double>{-5, 20}, QVector<double>{-5, 20});
    plot->rescaleAxes();
    QVERIFY(removed);
    QCOMPARE(rect->axes().size(), 3);
}

void TestSignalDelivery::slotDeletesPlotOnClick()
{
    auto* plot = plotWithGraph();
    const QPointer<QCustomPlot> alive(plot);
    plot->replot();
    bool clicked = false;
    connect(plot, &QCustomPlot::plottableClick, plot, [&clicked, plot] {
        clicked = true;
        delete plot;
    });
    const QPoint onGraph(qRound(plot->xAxis->coordToPixel(5)),
                         qRound(plot->yAxis->coordToPixel(5)));
    QTest::mouseClick(plot, Qt::LeftButton, Qt::NoModifier, onGraph);
    QVERIFY2(!clicked, "the click slot ran inside the mouse event");
    QCoreApplication::processEvents();
    QVERIFY(clicked);
    QVERIFY(!alive);
}

void TestSignalDelivery::removeLayerWhileIntervalsAddsLayer()
{
    QCustomPlot plot;
    QCPLaneLayout lanes;
    QVERIFY(plot.addLayer(QStringLiteral("custom")));
    plot.setCurrentLayer(QStringLiteral("custom"));
    auto* intervals = new QCPIntervals(plot.xAxis, plot.yAxis, &lanes);
    plot.setCurrentLayer(QStringLiteral("main"));
    const int layerables = layerablesOnLayers(plot).size();
    QVERIFY(plot.removeLayer(plot.layer(QStringLiteral("custom"))));
    QVERIFY(intervals->layer());
    const QCPLayer* labels = plot.layer(intervals->layer()->name() + ".intervals-labels");
    QVERIFY(labels);
    QCOMPARE(labels->index(), intervals->layer()->index() + 1);
    QCOMPARE(labels->children().size(), 1);
    QCOMPARE(layerablesOnLayers(plot).size(), layerables);
    for (QCPLayerable* layerable : layerablesOnLayers(plot))
        QCOMPARE(layerable->layer()->parentPlot(), &plot);
    plot.replot();
}

void TestSignalDelivery::deletePlotFromBeforeReplotWithHandle_data()
{
    slotDeletesPlotDuringReplot_data();
}

void TestSignalDelivery::deletePlotFromBeforeReplotWithHandle()
{
    QFETCH(QString, signal);
    auto* plot = plotWithGraph();
    const QCPHandle<QCPGraph> graph(plot->graph(0));
    connect(plot, replotSignal(signal), plot, [plot] { delete plot; });
    plot->replot();
    QVERIFY(!graph);
    QVERIFY(graph.get() == nullptr);
}

void TestSignalDelivery::deletedChildSkipsQueuedEmission()
{
    QCustomPlot plot;
    QCPGraph* queued = plot.addGraph();
    QCPGraph* other = plot.addGraph();
    connect(other, &QObject::destroyed, &plot, [queued] { delete queued; });
    bool ran = false;
    {
        QCPEmissionScope scope(&plot);
        plot.deliver(queued, [&ran] { ran = true; });
        delete other;
    }
    QVERIFY(!ran);
    QCOMPARE(plot.graphCount(), 0);
}

void TestSignalDelivery::removeInsideInputScope()
{
    std::unique_ptr<QCustomPlot> plot(plotWithGraph());
    plot->replot();
    QCPHandle<QCPGraph> graph(plot->graph(0));
    QSignalSpy clicks(plot.get(), &QCustomPlot::plottableClick);
    QSignalSpy releases(plot.get(), &QCustomPlot::mouseRelease);
    const QPoint onGraph(qRound(plot->xAxis->coordToPixel(5)),
                         qRound(plot->yAxis->coordToPixel(5)));
    {
        QCPInputScope scope(plot.get());
        QTest::mouseClick(plot.get(), Qt::LeftButton, Qt::NoModifier, onGraph);
        graph.remove();
    }
    QCoreApplication::processEvents();
    QVERIFY(!graph);
    QCOMPARE(clicks.count(), 0);
    QCOMPARE(releases.count(), 1);
    QCOMPARE(plot->plottableCount(), 0);
    plot->replot();
}
