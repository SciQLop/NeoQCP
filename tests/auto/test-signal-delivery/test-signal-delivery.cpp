#include "test-signal-delivery.h"
#include <QTemporaryDir>
#include <QtTest/qtestspontaneevent.h>
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
        result += qcp::pointers(plot.layer(i)->children());
    return result;
}

QPoint pixelOf(const QCustomPlot& plot, double key, double value)
{
    return {qRound(plot.xAxis->coordToPixel(key)), qRound(plot.yAxis->coordToPixel(value))};
}

void sendMouse(QCustomPlot* plot, QEvent::Type type, QPoint pos, Qt::MouseButtons buttons)
{
    QMouseEvent event(type, pos, plot->mapToGlobal(pos), Qt::LeftButton, buttons, Qt::NoModifier);
    QCoreApplication::sendEvent(plot, &event);
}

void dragFromTo(QCustomPlot* plot, QPoint from, QPoint to)
{
    sendMouse(plot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(plot, QEvent::MouseMove, to, Qt::LeftButton);
    sendMouse(plot, QEvent::MouseButtonRelease, to, Qt::NoButton);
}

class WheelRecorder : public QWidget
{
public:
    int wheels = 0;

protected:
    void wheelEvent(QWheelEvent* event) override
    {
        ++wheels;
        event->accept();
    }
};

int gMisuses = 0;
void countMisuse(const char*, const char*) { ++gMisuses; }

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
    connect(x, &QCPAxis::rangeChanged, plot.get(),
            [&, y] { removeOther(y); });
    connect(y, &QCPAxis::rangeChanged, plot.get(),
            [&, x] { removeOther(x); });
    plot->graph(0)->setData(QVector<double>{-5, 20}, QVector<double>{-5, 20});
    plot->rescaleAxes();
    QVERIFY(removed);
    QCOMPARE(rect->axes().size(), 3);
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
    // The export lays out at its size, then once more for the screen when it returns.
    const QSize screen = plot->size();
    QList<QSize> seen;
    connect(plot.get(), &QCustomPlot::afterLayout, plot.get(),
            [&] { seen.append(plot->viewport().size()); });
    const QPixmap pixmap = plot->toPixmap(800, 600);
    QVERIFY(!pixmap.isNull());
    QVERIFY(seen.contains(QSize(800, 600)));
    QCOMPARE(seen.last(), screen);
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

void TestSignalDelivery::deleteInsideOwnEventReportsMisuse()
{
    using Kind = QCPEmissionScope::Kind;
    gMisuses = 0;
    const auto previous = qcp::setMisuseHandler(countMisuse);
    {
        auto* plot = new QCustomPlot;
        const QCPEmissionScope scope(plot, Kind::WidgetEvent);
        delete plot;
    }
    const int insideEvent = gMisuses;
    {
        // A widget event nested under a method scope: processEvents() from a rangeChanged slot.
        auto* plot = new QCustomPlot;
        const QCPEmissionScope methodScope(plot);
        const QCPEmissionScope nested(plot, Kind::WidgetEvent);
        delete plot;
    }
    const int nestedEvent = gMisuses - insideEvent;
    {
        // Once the widget event closed, the open method scope alone does not report.
        auto* plot = new QCustomPlot;
        const QCPEmissionScope methodScope(plot);
        {
            const QCPEmissionScope closed(plot, Kind::WidgetEvent);
        }
        delete plot;
    }
    {
        auto* plot = new QCustomPlot;
        const QCPEmissionScope event(plot, Kind::WidgetEvent);
        const QCPEmissionScope nestedInput(plot, Kind::Event);
        delete plot;
    }
    {
        // Closing a panel from a click slot stays allowed: Qt does not touch the widget after a
        // mouse handler returns.
        auto* plot = new QCustomPlot;
        const QCPEmissionScope mouseScope(plot, Kind::Event);
        delete plot;
    }
    delete new QCustomPlot;
    qcp::setMisuseHandler(previous);
    QCOMPARE(insideEvent, 1);
    QCOMPARE(nestedEvent, 1);
    QCOMPARE(gMisuses, 3);
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

void TestSignalDelivery::pendingSignalsDieWithThePlot()
{
    auto* plot = plotWithGraph();
    QObject outlivesThePlot;
    bool ran = false;
    {
        QCPEmissionScope scope(plot);
        plot->deliver(&outlivesThePlot, [&ran] { ran = true; });
        delete plot;
    }
    QVERIFY(!ran);
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
    QTest::mouseClick(plot, Qt::LeftButton, Qt::NoModifier, pixelOf(*plot, 5, 5));
    QVERIFY(clicked);
    QVERIFY(!alive);
}

void TestSignalDelivery::slotsRunBeforeEventReturns()
{
    std::unique_ptr<QCustomPlot> plot(plotWithGraph());
    plot->setInteraction(QCP::iRangeDrag);
    plot->replot();
    bool ran = false;
    QCPLayerable* handledBy = nullptr;
    connect(plot.get(), &QCustomPlot::mousePress, plot.get(), [&] {
        ran = true;
        handledBy = plot->mMouseEventLayerable;
    });
    const QPoint offTheGraph = pixelOf(*plot, 8, 2);
    QTest::mousePress(plot.get(), Qt::LeftButton, Qt::NoModifier, offTheGraph);
    QVERIFY(ran);
    QCOMPARE(handledBy, plot->axisRect());
    QTest::mouseRelease(plot.get(), Qt::LeftButton, Qt::NoModifier, offTheGraph);
}

void TestSignalDelivery::wheelIgnoredInSlotReachesParent()
{
    WheelRecorder parent;
    parent.resize(400, 300);
    auto* plot = plotWithGraph();
    plot->setParent(&parent);
    // QCustomPlot sets it, as 2.x did; a plot in a scroll area clears it to pass wheels on.
    plot->setAttribute(Qt::WA_NoMousePropagation, false);
    plot->show();
    parent.show();
    QVERIFY(QTest::qWaitForWindowExposed(&parent));
    connect(plot, &QCustomPlot::mouseWheel, plot, [](QWheelEvent* event) { event->ignore(); });
    const QPointF pos = plot->rect().center();
    QWheelEvent wheel(pos, plot->mapToGlobal(pos), QPoint(), QPoint(0, 120), Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
    QSpontaneKeyEvent::setSpontaneous(&wheel); // QApplication propagates spontaneous wheels only
    QCoreApplication::instance()->notify(plot, &wheel);
    QCOMPARE(parent.wheels, 1);
}

void TestSignalDelivery::nestedEventDeliversWhenItReturns()
{
    std::unique_ptr<QCustomPlot> plot(plotWithGraph());
    plot->replot();
    int clicks = 0;
    QPoint clickedAt;
    connect(plot.get(), &QCustomPlot::plottableClick, plot.get(),
            [&](QCPAbstractPlottable*, int, QMouseEvent* event) {
                ++clicks;
                clickedAt = event->pos();
            });
    plot->graph(0)->setData(QVector<double>{-5, 20}, QVector<double>{-5, 20});
    int clicksWhenTheEventReturned = -1;
    QPoint onGraph;
    connect(plot->xAxis, &QCPAxis::rangeChanged, plot.get(),
            [&] {
                onGraph = pixelOf(*plot, 5, 5);
                QTest::mouseClick(plot.get(), Qt::LeftButton, Qt::NoModifier, onGraph);
                clicksWhenTheEventReturned = clicks;
            },
            Qt::SingleShotConnection);
    plot->rescaleAxes();
    QCOMPARE(clicksWhenTheEventReturned, 1);
    QCOMPARE(clickedAt, onGraph);
}

void TestSignalDelivery::replotSignalOfAWidgetEventRunsInsideIt_data()
{
    QTest::addColumn<bool>("byShow");
    QTest::newRow("resize") << false;
    QTest::newRow("show") << true;
}

void TestSignalDelivery::replotSignalOfAWidgetEventRunsInsideIt()
{
    QFETCH(bool, byShow);
    std::unique_ptr<QCustomPlot> plot(plotWithGraph());
    int runs = 0;
    int runsOutsideWidgetEvent = 0;
    connect(plot.get(), &QCustomPlot::afterReplot, plot.get(), [&] {
        ++runs;
        runsOutsideWidgetEvent += !plot->mEmissions.insideWidgetEvent();
    });
    if (byShow)
        plot->show();
    else
    {
        QResizeEvent resize(QSize(500, 400), plot->size());
        QCoreApplication::sendEvent(plot.get(), &resize);
    }
    QVERIFY(runs > 0);
    QVERIFY2(runsOutsideWidgetEvent == 0, "deleting the plot from this slot would not be reported");
}

void TestSignalDelivery::wheelSlotIgnoringAndDeletingThePlotIsSafe()
{
    WheelRecorder parent;
    parent.resize(400, 300);
    QPointer<QCustomPlot> plot = plotWithGraph();
    plot->setParent(&parent);
    plot->show();
    parent.show();
    QVERIFY(QTest::qWaitForWindowExposed(&parent));
    connect(plot, &QCustomPlot::mouseWheel, plot, [&plot](QWheelEvent* event) {
        event->ignore();
        delete plot;
    });
    const QPointF pos = plot->rect().center();
    QWheelEvent wheel(pos, plot->mapToGlobal(pos), QPoint(), QPoint(0, 120), Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
    QSpontaneKeyEvent::setSpontaneous(&wheel);
    QCoreApplication::instance()->notify(plot, &wheel);
    QVERIFY(!plot);
}

void TestSignalDelivery::releaseSlotIgnoringAndDeletingThePlotIsSafe()
{
    QPointer<QCustomPlot> plot = plotWithGraph();
    plot->show();
    QVERIFY(QTest::qWaitForWindowExposed(plot));
    connect(plot, &QCustomPlot::mouseRelease, plot, [&plot](QMouseEvent* event) {
        event->ignore();
        delete plot;
    });
    QTest::mouseClick(plot, Qt::LeftButton, Qt::NoModifier, plot->rect().center());
    QVERIFY(!plot);
}

void TestSignalDelivery::nestedEventKeepsOuterEntriesQueued()
{
    QCustomPlot plot;
    bool ranA = false;
    bool ranB = false;
    {
        const QCPEmissionScope outer(&plot);
        plot.deliver(&plot, [&ranA] { ranA = true; });
        {
            const QCPEmissionScope nested(&plot, QCPEmissionScope::Kind::Event);
            plot.deliver(&plot, [&ranB] { ranB = true; });
        }
        QVERIFY(ranB);
        QVERIFY(!ranA);
    }
    QVERIFY(ranA);
}

void TestSignalDelivery::deleteLaterDeliversTeardownSignalsLikeDelete()
{
    const auto layerChangesDuringTeardown = [](bool later) {
        auto* plot = new QCustomPlot;
        int changes = 0;
        connect(plot->selectionRect(), &QCPLayerable::layerChanged, [&changes] { ++changes; });
        if (later)
        {
            plot->deleteLater();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        }
        else
            delete plot;
        return changes;
    };
    const int byDelete = layerChangesDuringTeardown(false);
    QVERIFY(byDelete > 0);
    QCOMPARE(layerChangesDuringTeardown(true), byDelete);
}

void TestSignalDelivery::qcpEmitHoldsUntilTheScopeCloses()
{
    QCustomPlot plot;
    QCPAxis* axis = plot.xAxis;
    int received = 0;
    QCPRange receivedRange;
    connect(axis, &QCPAxis::rangeChanged, &plot,
            [&](const QCPRange& range) {
                ++received;
                receivedRange = range;
            });
    const int before = received;
    {
        const QCPEmissionScope scope(&plot);
        QCPRange range(3, 7);
        qcpEmit(static_cast<QCPAxisBase*>(axis), &plot, &QCPAxisBase::rangeChanged, range,
                QCPRange(2, 6));
        range = QCPRange(1, 2); // the emission took a copy at the call
        QCOMPARE(received, before);
    }
    QCOMPARE(received, before + 1);
    QCOMPARE(receivedRange, QCPRange(3, 7));
}

void TestSignalDelivery::noPlotEmitsAtOnce()
{
    QCPTheme theme;
    int received = 0;
    connect(&theme, &QCPTheme::changed, &theme, [&received] { ++received; });
    qcpEmit(&theme, nullptr, &QCPTheme::changed);
    QCOMPARE(received, 1);
}

void TestSignalDelivery::deadSenderIsNotDelivered()
{
    QCustomPlot plot;
    auto* sender = new QCPTheme;
    int received = 0;
    connect(sender, &QCPTheme::changed, &plot, [&received] { ++received; });
    {
        const QCPEmissionScope scope(&plot);
        qcpEmit(sender, &plot, &QCPTheme::changed);
        delete sender;
    }
    QCOMPARE(received, 0);
}

void TestSignalDelivery::rangeSlotSeesTheNewScaleType()
{
    QCustomPlot plot;
    QCPAxis* axis = plot.xAxis;
    axis->setRange(-5, 10);
    QStringList order;
    QCPAxis::ScaleType seenType = QCPAxis::stLinear;
    QCPRange seenRange;
    connect(axis, &QCPAxis::rangeChanged, &plot,
            [&](const QCPRange&, const QCPRange&)
            {
                order << QStringLiteral("rangeChanged");
                seenType = axis->scaleType();
                seenRange = axis->range();
            });
    connect(axis, &QCPAxis::scaleTypeChanged, &plot,
            [&order](QCPAxis::ScaleType) { order << QStringLiteral("scaleTypeChanged"); });
    axis->setScaleType(QCPAxis::stLogarithmic);
    QCOMPARE(order,
             QStringList({ QStringLiteral("rangeChanged"), QStringLiteral("scaleTypeChanged") }));
    QCOMPARE(seenType, QCPAxis::stLogarithmic);
    QCOMPARE(seenRange, axis->range());
    QVERIFY2(seenRange.lower > 0, "the slot saw the range from before the log sanitisation");
}

void TestSignalDelivery::slotReenteringTheSetterDeliversInOrder()
{
    QCustomPlot plot;
    QCPAxis* axis = plot.xAxis;
    axis->setRange(0, 10);
    QList<QCPRange> received;
    connect(axis, &QCPAxis::rangeChanged, &plot,
            [&](const QCPRange& range, const QCPRange&)
            {
                received << range;
                if (received.size() == 1)
                    axis->setRange(2, 3);
            });
    axis->setRange(1, 2);
    QCOMPARE(received.size(), 2);
    QCOMPARE(received.at(0), QCPRange(1, 2));
    QCOMPARE(received.at(1), QCPRange(2, 3));
}

void TestSignalDelivery::slotDeletingThePlotDropsTheRestOfTheChain()
{
    auto* plot = new QCustomPlot;
    const QPointer<QCustomPlot> alive(plot);
    plot->xAxis->setRange(0, 10);
    bool sawRangeChanged = false;
    int scaleTypes = 0;
    connect(plot->xAxis, &QCPAxis::rangeChanged, plot,
            [&, plot](const QCPRange&, const QCPRange&)
            {
                sawRangeChanged = true;
                delete plot;
            });
    connect(plot->xAxis, &QCPAxis::scaleTypeChanged, plot, [&scaleTypes] { ++scaleTypes; });
    plot->xAxis->setScaleType(QCPAxis::stLogarithmic);
    QVERIFY(sawRangeChanged);
    QVERIFY(!alive);
    QCOMPARE(scaleTypes, 0);
}

void TestSignalDelivery::teardownOpensNoDelivery()
{
    auto* plot = new QCustomPlot;
    plot->resize(400, 300);
    int received = 0;
    const QList<QCPAxis*> axes = { plot->xAxis, plot->yAxis, plot->xAxis2, plot->yAxis2 };
    for (QCPAxis* axis : axes)
    {
        connect(axis, &QCPAxis::rangeChanged, plot,
                [&received](const QCPRange&, const QCPRange&) { ++received; });
        connect(axis, &QCPAxis::selectionChanged, plot, [&received] { ++received; });
        connect(axis, &QCPAxis::selectableChanged, plot, [&received] { ++received; });
    }
    delete plot;
    QCOMPARE(received, 0);
}

void TestSignalDelivery::oneArgumentRangeSlotConnects()
{
    QCustomPlot plot;
    QCPRange received;
    connect(plot.xAxis, &QCPAxis::rangeChanged, &plot,
            [&received](const QCPRange& range) { received = range; });
    plot.xAxis->setRange(1, 2);
    QCOMPARE(received, QCPRange(1, 2));
}

void TestSignalDelivery::slotReenteringAScopedSetterLeavesTheLastSignalCurrent()
{
    QCustomPlot plot;
    QCPAxis* axis = plot.xAxis;
    axis->setRange(-5, 10);
    QList<QCPAxis::ScaleType> received;
    connect(axis, &QCPAxis::rangeChanged, &plot,
            [axis](const QCPRange&, const QCPRange&) { axis->setScaleType(QCPAxis::stLinear); });
    connect(axis, &QCPAxis::scaleTypeChanged, &plot,
            [&received](QCPAxis::ScaleType type) { received << type; });
    axis->setScaleType(QCPAxis::stLogarithmic);
    QCOMPARE(axis->scaleType(), QCPAxis::stLinear);
    QVERIFY(!received.isEmpty());
    QCOMPARE(received.last(), axis->scaleType());
}

void TestSignalDelivery::colorMapGradientSlotDeletingThePlot()
{
    auto* plot = new QCustomPlot;
    plot->resize(400, 300);
    plot->xAxis->setRange(0, 10);
    plot->yAxis->setRange(0, 10);
    auto* map = new QCPColorMap(plot->xAxis, plot->yAxis);
    auto* scale = new QCPColorScale(plot);
    plot->plotLayout()->addElement(0, 1, scale);

    const QPointer<QCustomPlot> alive(plot);
    int scaleGradients = 0;
    int mapGradients = 0;
    // The deleting slot is the map's first one, and the colour scale is wired after it: the echo
    // on the scale, and every later slot of the map, must be dropped with the plot.
    connect(map, &QCPColorMap::gradientChanged, plot,
            [plot](const QCPColorGradient&) { delete plot; });
    map->setColorScale(scale);
    connect(scale, &QCPColorScale::gradientChanged, plot,
            [&scaleGradients](const QCPColorGradient&) { ++scaleGradients; });
    connect(map, &QCPColorMap::gradientChanged, plot,
            [&mapGradients](const QCPColorGradient&) { ++mapGradients; });

    map->setGradient(QCPColorGradient(QCPColorGradient::gpJet));

    QVERIFY(!alive);
    QCOMPARE(scaleGradients, 0);
    QCOMPARE(mapGradients, 0);
}

void TestSignalDelivery::spanDragSlotDeletingThePlot()
{
    auto* plot = new QCustomPlot;
    plot->resize(400, 300);
    plot->xAxis->setRange(0, 10);
    plot->yAxis->setRange(0, 10);
    auto* span = new QCPItemRSpan(plot);
    span->setKeyRange(QCPRange(3, 7));
    span->setValueRange(QCPRange(3, 7));
    plot->replot();

    const QPointer<QCustomPlot> alive(plot);
    int keyMoves = 0;
    int valueMoves = 0;
    // A fill drag emits the two ranges one after the other.
    connect(span, &QCPItemRSpan::keyRangeChanged, plot,
            [&keyMoves, plot](const QCPRange&)
            {
                ++keyMoves;
                delete plot;
            });
    connect(span, &QCPItemRSpan::valueRangeChanged, plot,
            [&valueMoves](const QCPRange&) { ++valueMoves; });

    const QPoint from = pixelOf(*plot, 5, 5);
    sendMouse(plot, QEvent::MouseButtonPress, from, Qt::LeftButton);
    sendMouse(plot, QEvent::MouseMove, from + QPoint(20, 20), Qt::LeftButton);

    QVERIFY(!alive);
    QCOMPARE(keyMoves, 1);
    QCOMPARE(valueMoves, 0);
}

void TestSignalDelivery::selectionRectSlotDeletingThePlot()
{
    auto* plot = new QCustomPlot;
    plot->resize(400, 300);
    auto* graph = plot->addGraph();
    graph->setData(QVector<double>{0, 5, 10}, QVector<double>{8, 8, 8});
    graph->setSelectable(QCP::stWhole);
    plot->xAxis->setRange(0, 10);
    plot->yAxis->setRange(0, 10);
    plot->setInteractions(QCP::iSelectPlottables);
    plot->setSelectionRectMode(QCP::srmSelect);
    plot->replot();

    const QPointer<QCustomPlot> alive(plot);
    int plottableSelections = 0;
    connect(graph, &QCPAbstractPlottable::selectionChanged, plot,
            [&plottableSelections] { ++plottableSelections; });
    connect(plot->selectionRect(), &QCPSelectionRect::accepted, plot,
            [plot](const QRect&, QMouseEvent*) { delete plot; });

    dragFromTo(plot, pixelOf(*plot, 1, 9), pixelOf(*plot, 9, 7));

    // Guard: the rect's action ran before accepted, and deleting the plot from it is harmless. The
    // accepted delivery moves out of the release handler, so nothing may touch the plot after it.
    QVERIFY(!alive);
    QVERIFY2(plottableSelections > 0, "the rect must reach the plottable before accepted returns");
}

void TestSignalDelivery::overlayCollapseSlotDeletingThePlot()
{
    auto* plot = new QCustomPlot;
    plot->resize(400, 300);
    QCPOverlay* overlay = plot->overlay();
    const QPointer<QCustomPlot> alive(plot);
    int laterCollapses = 0;
    connect(overlay, &QCPOverlay::collapsedChanged, plot, [plot](bool) { delete plot; });
    connect(overlay, &QCPOverlay::collapsedChanged, plot,
            [&laterCollapses](bool) { ++laterCollapses; });

    overlay->setCollapsed(true);

    QVERIFY(!alive);
    QCOMPARE(laterCollapses, 0);
}

void TestSignalDelivery::groupMembersSlotDeletingThePlot()
{
    auto* plot = new QCustomPlot;
    plot->resize(400, 300);
    auto* member = plot->addGraph();
    member->setData(QVector<double>{0, 10}, QVector<double>{0, 10});
    QCPPlottableGroup* first = plot->addPlottableGroup(QStringLiteral("first"));
    first->addMember(member);
    QCPPlottableGroup* second = plot->addPlottableGroup(QStringLiteral("second"));

    const QPointer<QCustomPlot> alive(plot);
    int secondChanges = 0;
    // Taking the member away from the first group tells it first, then the new group.
    connect(first, &QCPPlottableGroup::membersChanged, plot, [plot] { delete plot; });
    connect(second, &QCPPlottableGroup::membersChanged, plot,
            [&secondChanges] { ++secondChanges; });

    second->addMember(member);

    QVERIFY(!alive);
    QCOMPARE(secondChanges, 0);
}

void TestSignalDelivery::overlayMessageSlotDeletingThePlot()
{
    auto* plot = new QCustomPlot;
    plot->resize(400, 300);
    QCPOverlay* overlay = plot->overlay();
    const QPointer<QCustomPlot> alive(plot);
    int laterMessages = 0;
    connect(overlay, &QCPOverlay::messageChanged, plot,
            [plot](const QString&, QCPOverlay::Level) { delete plot; });
    connect(overlay, &QCPOverlay::messageChanged, plot,
            [&laterMessages](const QString&, QCPOverlay::Level) { ++laterMessages; });

    overlay->showMessage(QStringLiteral("boom"), QCPOverlay::Error);

    QVERIFY(!alive);
    QCOMPARE(laterMessages, 0);
}

void TestSignalDelivery::plottableSelectionSlotDeletingThePlot()
{
    auto* plot = new QCustomPlot;
    plot->resize(400, 300);
    auto* graph = plot->addGraph();
    graph->setData(QVector<double>{0, 5, 10}, QVector<double>{0, 5, 10});
    graph->setSelectable(QCP::stWhole);
    plot->xAxis->setRange(0, 10);
    plot->yAxis->setRange(0, 10);

    const QPointer<QCustomPlot> alive(plot);
    int selectionChanges = 0;
    // The first slot deletes the plot: the second slot of the same signal must not run.
    connect(graph, &QCPAbstractPlottable::selectionChanged, plot, [plot] { delete plot; });
    connect(graph, &QCPAbstractPlottable::selectionChanged, plot,
            [&selectionChanges] { ++selectionChanges; });

    graph->setSelection(QCPDataSelection(QCPDataRange(0, 1)));

    QVERIFY(!alive);
    QCOMPARE(selectionChanges, 0);
}

// setupFullAxesBox links the second axis to the first in both directions. When both axes changed
// inside one call, the second axis's own queued signal must not land last with a range the axis no
// longer holds.
void TestSignalDelivery::linkedAxisLastRangeSignalIsItsRange()
{
    QCustomPlot plot;
    plot.axisRect()->setupFullAxesBox(true);
    QCPRange lastHeard;
    connect(plot.xAxis2, &QCPAxis::rangeChanged, &plot,
            [&lastHeard](const QCPRange& range, const QCPRange&) { lastHeard = range; });
    {
        const QCPEmissionScope scope(&plot);
        plot.xAxis->setRange(1, 2);
        plot.xAxis2->setRange(5, 6);
    }
    QCOMPARE(plot.xAxis->range(), QCPRange(5, 6));
    QCOMPARE(plot.xAxis2->range(), QCPRange(5, 6));
    QCOMPARE(lastHeard, plot.xAxis2->range());
}
