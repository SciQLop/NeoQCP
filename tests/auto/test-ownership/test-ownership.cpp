#include "test-ownership.h"

#include <QtWidgets/qtestsupport_widgets.h> // QTest::qWaitForWindowExposed

namespace
{
int gMisuses = 0;

void countMisuse(const char*, const char*) { ++gMisuses; }
} // namespace

void TestOwnership::init()
{
    gMisuses = 0;
    qcp::setMisuseHandler(countMisuse);
    mPlot = new QCustomPlot(nullptr);
    mPlot->resize(400, 300);
    mPlot->legend->setVisible(true);
}

void TestOwnership::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
    qcp::setMisuseHandler(nullptr);
}

void TestOwnership::draw()
{
    mPlot->replot();
    QVERIFY(!mPlot->toPixmap(400, 300).isNull());
}

QCPLegend* TestOwnership::addSecondLegend()
{
    auto* second = new QCPLegend;
    mPlot->axisRect()->insetLayout()->addElement(second, Qt::AlignLeft | Qt::AlignBottom);
    return second;
}

void TestOwnership::deletePlottableLeavesLists()
{
    auto* graph = mPlot->addGraph();
    new QCPCurve(mPlot->xAxis, mPlot->yAxis);
    QCOMPARE(mPlot->plottableCount(), 2);
    QCOMPARE(mPlot->legend->itemCount(), 2);

    delete graph;

    QCOMPARE(mPlot->plottableCount(), 1);
    QCOMPARE(mPlot->graphCount(), 0);
    QCOMPARE(mPlot->legend->itemCount(), 1);
    draw();
}

void TestOwnership::deleteLaterPlottable()
{
    auto* graph = mPlot->addGraph();
    new QCPCurve(mPlot->xAxis, mPlot->yAxis);

    graph->deleteLater();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    QCOMPARE(mPlot->plottableCount(), 1);
    QCOMPARE(mPlot->graphCount(), 0);
    QCOMPARE(mPlot->legend->itemCount(), 1);
    draw();
}

void TestOwnership::deleteMultiGraphLeavesItsRow()
{
    auto* multiGraph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    QVERIFY(qobject_cast<QCPGroupLegendItem*>(mPlot->legend->item(0)));

    delete multiGraph;

    QCOMPARE(mPlot->plottableCount(), 0);
    QCOMPARE(mPlot->legend->itemCount(), 0);
    draw();
}

void TestOwnership::deletePlottableInGroup()
{
    auto* first = mPlot->addGraph();
    auto* second = mPlot->addGraph();
    auto* group = mPlot->addPlottableGroup("pair");
    QVERIFY(group->addMember(first));
    QVERIFY(group->addMember(second));
    QSignalSpy changed(group, &QCPPlottableGroup::membersChanged);

    delete first;

    QCOMPARE(group->members(), QList<QCPAbstractPlottable*> { second });
    QCOMPARE(changed.count(), 1);
    QCOMPARE(mPlot->legend->itemCount(), 1); // the group row
    draw();

    delete second;

    QVERIFY(group->members().isEmpty());
    QCOMPARE(mPlot->legend->itemCount(), 0); // an empty group has no row
    QCOMPARE(mPlot->plottableCount(), 0);
    draw();
}

void TestOwnership::removePlottableStillDeletes()
{
    QPointer<QCPGraph> graph = mPlot->addGraph();

    QVERIFY(mPlot->removePlottable(graph));

    QVERIFY(graph.isNull());
    QCOMPARE(mPlot->plottableCount(), 0);
    QCOMPARE(mPlot->graphCount(), 0);
    QCOMPARE(mPlot->legend->itemCount(), 0);
    QVERIFY(!mPlot->removePlottable(graph.data()));
}

void TestOwnership::nullAxisPlottableIsRefused()
{
    auto* orphan = new QCPCurve(nullptr, nullptr);
    QCOMPARE(gMisuses, 1);
    QVERIFY(orphan->parentPlot() == nullptr);

    auto* halfAxed = new QCPCurve(mPlot->xAxis, nullptr);
    QCOMPARE(gMisuses, 2);
    QVERIFY(halfAxed->parentPlot() == nullptr);

    auto* graph = new QCPGraph(nullptr, nullptr);
    QCOMPARE(gMisuses, 3);

    auto* histogram = new QCPHistogram2D(nullptr, nullptr);
    QCOMPARE(gMisuses, 4);

    QCPLaneLayout lanes;
    auto* intervals = new QCPIntervals(nullptr, nullptr, &lanes);
    QCOMPARE(gMisuses, 5);
    QVERIFY(intervals->labelLayer() == nullptr);

    QCOMPARE(mPlot->plottableCount(), 0);
    QCOMPARE(mPlot->graphCount(), 0);
    QCOMPARE(mPlot->legend->itemCount(), 0);
    draw();
    delete orphan;
    delete halfAxed;
    delete graph;
    delete histogram;
    delete intervals;
}

void TestOwnership::legendItemMovedToSecondLegend()
{
    auto* graph = mPlot->addGraph();
    auto* second = addSecondLegend();
    QCPPlottableLegendItem* row = mPlot->legend->itemWithPlottable(graph);

    QVERIFY(second->addItem(row));

    QCOMPARE(row->parentLegend(), second);
    delete mPlot->legend;
    QVERIFY(mPlot->legend == nullptr);
    draw();
}

void TestOwnership::plottableDeathLeavesSecondLegend()
{
    auto* graph = mPlot->addGraph();
    auto* second = addSecondLegend();
    QVERIFY(second->addItem(mPlot->legend->itemWithPlottable(graph)));
    QCOMPARE(second->itemCount(), 1);

    delete graph;

    QCOMPARE(second->itemCount(), 0);
    draw();
}

void TestOwnership::deleteItemLeavesList()
{
    auto* line = new QCPItemLine(mPlot);
    new QCPItemText(mPlot);
    QCOMPARE(mPlot->itemCount(), 2);

    delete line;

    QCOMPARE(mPlot->itemCount(), 1);
    QVERIFY(!mPlot->hasItem(line));
    draw();
}

void TestOwnership::deleteLaterItem()
{
    auto* line = new QCPItemLine(mPlot);
    new QCPItemText(mPlot);

    line->deleteLater();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    QCOMPARE(mPlot->itemCount(), 1);
    draw();
}

void TestOwnership::removeItemFromDestroyedSlot()
{
    auto* line = new QCPItemLine(mPlot);
    new QCPItemText(mPlot);
    connect(line, &QObject::destroyed, mPlot, [this] { mPlot->clearItems(); });

    QVERIFY(mPlot->removeItem(line));

    QCOMPARE(mPlot->itemCount(), 0);
    draw();
}

void TestOwnership::clearItemsWhileASlotDeletesAnother()
{
    auto* first = new QCPItemLine(mPlot);
    auto* second = new QCPItemLine(mPlot);
    new QCPItemText(mPlot); // cleared first: its slot deletes an item still in the snapshot
    connect(mPlot->item(), &QObject::destroyed, mPlot, [first] { delete first; });
    Q_UNUSED(second);

    QCOMPARE(mPlot->clearItems(), 3);

    QCOMPARE(mPlot->itemCount(), 0);
    draw();
}

void TestOwnership::deleteParentAnchorItem()
{
    auto* tracer = new QCPItemTracer(mPlot);
    tracer->position->setCoords(1, 2);
    auto* text = new QCPItemText(mPlot);
    text->position->setParentAnchor(tracer->position);
    text->position->setCoords(10, 20); // pixels away from the tracer
    mPlot->replot();
    const QPointF before = text->position->pixelPosition();

    delete tracer;

    QVERIFY(text->position->parentAnchor() == nullptr);
    QCOMPARE(text->position->pixelPosition(), before);
    draw();
}

void TestOwnership::spanDestructorCreatesNoGpuLayer()
{
    mPlot->show();
    if (!QTest::qWaitForWindowExposed(mPlot)
        || (QCoreApplication::processEvents(), !mPlot->rhi()))
        QSKIP("no QRhi on this platform");
    mPlot->replot();
    QVERIFY(mPlot->spanRhiLayerIfExists() == nullptr);

    delete new QCPItemVSpan(mPlot);

    QVERIFY(mPlot->spanRhiLayerIfExists() == nullptr);
}
