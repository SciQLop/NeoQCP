#include "test-signal-delivery.h"
#include <QTemporaryDir>
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

int gMisuses = 0;
void countMisuse(const char*, const char*) { ++gMisuses; }

class MetaCallCounter : public QObject
{
public:
    int count = 0;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == QCoreApplication::instance() && event->type() == QEvent::MetaCall)
            ++count;
        return false;
    }
};
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

void TestSignalDelivery::replotSignalsSeeConsistentPlot()
{
    std::unique_ptr<QCustomPlot> plot(plotWithGraph());
    QList<bool> replotting;
    const auto record = [&] { replotting << plot->isReplotting(); };
    connect(plot.get(), &QCustomPlot::beforeReplot, plot.get(), record);
    connect(plot.get(), &QCustomPlot::afterReplot, plot.get(), record);
    plot->replot();
    QCOMPARE(replotting, QList<bool>({false, false}));
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

void TestSignalDelivery::beforeReplotChangesShowInSameReplot()
{
    std::unique_ptr<QCustomPlot> plot(plotWithGraph());
    plot->replot();
    connect(plot.get(), &QCustomPlot::beforeReplot, plot.get(),
            [&plot] { plot->xAxis->setRange(100, 200); }, Qt::SingleShotConnection);
    plot->replot();
    const QVector<double> ticks = plot->xAxis->tickVector();
    QVERIFY(!ticks.isEmpty());
    QVERIFY2(ticks.first() >= 100, "the replot drew the range from before its beforeReplot slot");
}

void TestSignalDelivery::afterLayoutSeesExportViewport()
{
    std::unique_ptr<QCustomPlot> plot(plotWithGraph());
    bool exportViewport = false;
    connect(plot.get(), &QCustomPlot::afterLayout, plot.get(),
            [&] { exportViewport = plot->viewport().size() == QSize(800, 600); });
    const QPixmap pixmap = plot->toPixmap(800, 600);
    QVERIFY(!pixmap.isNull());
    QVERIFY(exportViewport);
}

void TestSignalDelivery::addGraphBeforeInputFlush()
{
    auto* plot = plotWithGraph();
    const QPointer<QCustomPlot> alive(plot);
    plot->replot();
    connect(plot, &QCustomPlot::plottableClick, plot, [plot] { delete plot; });
    const QPoint onGraph(qRound(plot->xAxis->coordToPixel(5)),
                         qRound(plot->yAxis->coordToPixel(5)));
    QTest::mouseClick(plot, Qt::LeftButton, Qt::NoModifier, onGraph);
    QVERIFY(plot->addGraph());
    QVERIFY(alive);
    QCoreApplication::processEvents();
    QVERIFY(!alive);
}

void TestSignalDelivery::deleteLayerWhoseChildSlotDeletesPlot()
{
    auto* plot = plotWithGraph();
    const QPointer<QCustomPlot> alive(plot);
    QVERIFY(plot->addLayer(QStringLiteral("custom")));
    QCPGraph* graph = plot->graph(0);
    QVERIFY(graph->setLayer(QStringLiteral("custom")));
    connect(graph, &QCPLayerable::layerChanged, plot, [plot] { delete plot; });
    delete plot->layer(QStringLiteral("custom"));
    QVERIFY(!alive);
}

void TestSignalDelivery::deleteAxisRectWhoseSlotDeletesPlot()
{
    auto* plot = plotWithGraph();
    const QPointer<QCustomPlot> alive(plot);
    auto* rect = new QCPAxisRect(plot);
    plot->plotLayout()->addElement(1, 0, rect);
    QCPGraph* graph = plot->graph(0);
    connect(rect->axis(QCPAxis::atBottom), &QObject::destroyed, plot,
            [plot, graph] { graph->setLayer(plot->layer(QStringLiteral("axes"))); });
    connect(graph, &QCPLayerable::layerChanged, plot, [plot] { delete plot; });
    delete rect;
    QVERIFY(!alive);
}

void TestSignalDelivery::replotBeforeInputFlushKeepsClickQueued_data()
{
    QTest::addColumn<bool>("byResize");
    QTest::newRow("replot") << false;
    QTest::newRow("resizeEvent") << true;
}

void TestSignalDelivery::replotBeforeInputFlushKeepsClickQueued()
{
    QFETCH(bool, byResize);
    auto* plot = plotWithGraph();
    const QPointer<QCustomPlot> alive(plot);
    plot->show();
    QVERIFY(QTest::qWaitForWindowExposed(plot) || !byResize);
    plot->replot();
    bool clicked = false;
    connect(plot, &QCustomPlot::plottableClick, plot, [&clicked, plot] {
        clicked = true;
        delete plot;
    });
    const QPoint onGraph(qRound(plot->xAxis->coordToPixel(5)),
                         qRound(plot->yAxis->coordToPixel(5)));
    QTest::mouseClick(plot, Qt::LeftButton, Qt::NoModifier, onGraph);
    if (byResize)
        plot->resize(500, 400);
    else
        plot->replot();
    QVERIFY(alive);
    QVERIFY2(!clicked, "the click slot ran inside a later NeoQCP call");
    QCoreApplication::processEvents();
    QVERIFY(clicked);
    QVERIFY(!alive);
}

void TestSignalDelivery::slotDeletesPlotDuringExport_data()
{
    QTest::addColumn<QString>("format");
    for (const char* name : {"toPixmap", "toPainter", "savePdf"})
        QTest::newRow(name) << QString::fromLatin1(name);
}

void TestSignalDelivery::slotDeletesPlotDuringExport()
{
    QFETCH(QString, format);
    auto* plot = plotWithGraph();
    const QPointer<QCustomPlot> alive(plot);
    connect(plot, &QCustomPlot::afterLayout, plot, [plot] { delete plot; });
    if (format == QLatin1String("toPixmap"))
        (void)plot->toPixmap(400, 300);
    else if (format == QLatin1String("toPainter"))
    {
        QPixmap target(400, 300);
        QCPPainter painter(&target);
        plot->toPainter(&painter, 400, 300);
    }
    else
    {
        QTemporaryDir dir;
        QVERIFY(!plot->savePdf(dir.filePath(QStringLiteral("plot.pdf")), 400, 300));
    }
    QVERIFY(!alive);
}

void TestSignalDelivery::inputSignalSurvivesNestedLoopInMethodScope()
{
    std::unique_ptr<QCustomPlot> plot(plotWithGraph());
    plot->replot();
    int clicks = 0;
    connect(plot.get(), &QCustomPlot::plottableClick, plot.get(), [&clicks] { ++clicks; });
    const QPoint onGraph(qRound(plot->xAxis->coordToPixel(5)),
                         qRound(plot->yAxis->coordToPixel(5)));
    QTest::mouseClick(plot.get(), Qt::LeftButton, Qt::NoModifier, onGraph);
    plot->graph(0)->setData(QVector<double>{-5, 20}, QVector<double>{-5, 20});
    connect(plot->xAxis, qOverload<const QCPRange&>(&QCPAxis::rangeChanged), plot.get(),
            [] { QCoreApplication::processEvents(); }, Qt::SingleShotConnection);
    plot->rescaleAxes();
    QCOMPARE(clicks, 0);
    QCoreApplication::processEvents();
    QCOMPARE(clicks, 1);
}

void TestSignalDelivery::slotDeletesPlotOnResize_data()
{
    QTest::addColumn<bool>("byShow");
    QTest::newRow("resize") << false;
    QTest::newRow("show") << true;
}

void TestSignalDelivery::slotDeletesPlotOnResize()
{
    QFETCH(bool, byShow);
    auto* plot = plotWithGraph();
    const QPointer<QCustomPlot> alive(plot);
    if (!byShow)
    {
        plot->show();
        QVERIFY(QTest::qWaitForWindowExposed(plot));
        QCoreApplication::processEvents();
    }
    bool ran = false;
    connect(plot, &QCustomPlot::afterReplot, plot, [&ran, plot] {
        ran = true;
        delete plot;
    });
    if (byShow)
        plot->show();
    else
        plot->resize(500, 400);
    QVERIFY2(!ran, "an afterReplot slot ran inside the widget's own event");
    QTRY_VERIFY(ran);
    QVERIFY(!alive);
}

void TestSignalDelivery::deleteInsideOwnEventReportsMisuse()
{
    gMisuses = 0;
    const auto previous = qcp::setMisuseHandler(countMisuse);
    {
        auto* plot = new QCustomPlot;
        const QCPWidgetEventScope scope(plot);
        delete plot;
    }
    const int insideEvent = gMisuses;
    {
        // A widget event nested under a method scope: processEvents() from a rangeChanged slot.
        auto* plot = new QCustomPlot;
        const QCPEmissionScope methodScope(plot);
        const QCPWidgetEventScope nested(plot);
        delete plot;
    }
    const int nestedEvent = gMisuses - insideEvent;
    {
        // Once the widget event closed, the open method scope alone does not report.
        auto* plot = new QCustomPlot;
        const QCPEmissionScope methodScope(plot);
        {
            const QCPWidgetEventScope closed(plot);
        }
        delete plot;
    }
    {
        auto* plot = new QCustomPlot;
        const QCPWidgetEventScope event(plot);
        const QCPInputScope nestedInput(plot);
        delete plot;
    }
    {
        // Closing a panel from a click slot stays allowed: Qt does not touch the widget after a
        // mouse handler returns.
        auto* plot = new QCustomPlot;
        const QCPInputScope mouseScope(plot);
        delete plot;
    }
    delete new QCustomPlot;
    qcp::setMisuseHandler(previous);
    QCOMPARE(insideEvent, 1);
    QCOMPARE(nestedEvent, 1);
    QCOMPARE(gMisuses, 3);
}

void TestSignalDelivery::inputFlushPostedOnce()
{
    std::unique_ptr<QCustomPlot> plot(plotWithGraph());
    plot->replot();
    QCoreApplication::processEvents();
    int clicks = 0;
    connect(plot.get(), &QCustomPlot::plottableClick, plot.get(), [&clicks] { ++clicks; });
    const QPoint onGraph(qRound(plot->xAxis->coordToPixel(5)),
                         qRound(plot->yAxis->coordToPixel(5)));
    MetaCallCounter counter;
    QCoreApplication::instance()->installEventFilter(&counter);
    QTest::mouseClick(plot.get(), Qt::LeftButton, Qt::NoModifier, onGraph);
    for (int i = 0; i < 10; ++i)
        plot->replot();
    QCOMPARE(clicks, 0);
    QCoreApplication::processEvents();
    QCoreApplication::instance()->removeEventFilter(&counter);
    QCOMPARE(counter.count, 1);
    QCOMPARE(clicks, 1);
}

void TestSignalDelivery::replotFromReplotSignalIsIgnored_data()
{
    slotDeletesPlotDuringReplot_data();
}

void TestSignalDelivery::replotFromReplotSignalIsIgnored()
{
    QFETCH(QString, signal);
    std::unique_ptr<QCustomPlot> plot(plotWithGraph());
    int depth = 0;
    int deepest = 0;
    int replots = 0;
    connect(plot.get(), &QCustomPlot::beforeReplot, plot.get(), [&] {
        ++replots;
        deepest = std::max(deepest, ++depth);
    });
    connect(plot.get(), &QCustomPlot::afterReplot, plot.get(), [&] { --depth; });
    connect(plot.get(), replotSignal(signal), plot.get(), [&] { plot->replot(); });
    plot->replot();
    QCOMPARE(replots, 1);
    QCOMPARE(deepest, 1);
    QCOMPARE(depth, 0);
}
