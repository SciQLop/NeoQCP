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

QCPGraph* TestOwnership::addRedGraph()
{
    mPlot->legend->setVisible(false); // its icon would be red too
    mPlot->xAxis->setRange(0, 10);
    mPlot->yAxis->setRange(0, 10);
    auto* graph = mPlot->addGraph();
    graph->setPen(QPen(Qt::red, 6));
    graph->setData(QVector<double> { 1, 9 }, QVector<double> { 5, 5 });
    return graph;
}

bool TestOwnership::drawsRed()
{
    const QImage image = mPlot->toPixmap(400, 300).toImage();
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
        {
            const QColor c = image.pixelColor(x, y);
            if (c.red() > 200 && c.green() < 60 && c.blue() < 60)
                return true;
        }
    return false;
}

// Offscreen gives no QRhi, so the GPU layers stay empty there; under a real display they exist.
void TestOwnership::showAndExpose()
{
    mPlot->show();
    QVERIFY(QTest::qWaitForWindowExposed(mPlot));
    QCoreApplication::processEvents();
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

void TestOwnership::deleteAxisLeavesRectAndShortcuts()
{
    auto* graph = addRedGraph();
    QVERIFY(drawsRed());
    QCPAxis* y = mPlot->yAxis;

    delete y;

    QVERIFY(!mPlot->axisRect()->axes().contains(y));
    QCOMPARE(mPlot->axisRect()->axisCount(QCPAxis::atLeft), 0);
    QVERIFY(mPlot->yAxis == nullptr);
    QVERIFY(graph->valueAxis() == nullptr);
    QCOMPARE(gMisuses, 0);
    QVERIFY(!drawsRed());
    QCOMPARE(gMisuses, 1);
    draw();
    QCOMPARE(gMisuses, 1);
}

void TestOwnership::plottablesOfAnotherRectAfterAxisRemoval()
{
    mPlot->xAxis->setRange(0, 10);
    mPlot->yAxis->setRange(0, 10);
    mPlot->setInteractions(QCP::iSelectPlottables | QCP::iSelectItems);
    mPlot->setSelectionRectMode(QCP::srmSelect);
    auto* second = new QCPAxisRect(mPlot);
    mPlot->plotLayout()->addElement(1, 0, second);
    auto* graph = new QCPGraph(second->axis(QCPAxis::atBottom), second->axis(QCPAxis::atLeft));
    graph->setData(QVector<double> { 1, 9 }, QVector<double> { 5, 5 });
    auto* line = new QCPItemLine(mPlot);
    line->start->setAxes(second->axis(QCPAxis::atBottom), second->axis(QCPAxis::atLeft));
    mPlot->replot();

    for (QCPAxis* axis : second->axes())
        QVERIFY(second->removeAxis(axis));

    QVERIFY(graph->keyAxis() == nullptr);
    QVERIFY(mPlot->axisRect(0)->plottables().isEmpty());
    QVERIFY(mPlot->axisRect(0)->graphs().isEmpty());
    QVERIFY(mPlot->axisRect(0)->items().contains(line)); // its end and clip rect are still there
    QVERIFY(second->plottables().isEmpty());
    mPlot->replot();
    const QRect rect = mPlot->axisRect(0)->rect();
    const auto mouse = [this](QEvent::Type type, QPoint pos, Qt::MouseButtons buttons) {
        QMouseEvent e(type, pos, mPlot->mapToGlobal(pos), Qt::LeftButton, buttons, Qt::NoModifier);
        QCoreApplication::sendEvent(mPlot, &e);
    };
    mouse(QEvent::MouseButtonPress, rect.topLeft() + QPoint(2, 2), Qt::LeftButton);
    mouse(QEvent::MouseMove, rect.bottomRight() - QPoint(2, 2), Qt::LeftButton);
    mouse(QEvent::MouseButtonRelease, rect.bottomRight() - QPoint(2, 2), Qt::NoButton);
    QVERIFY(!graph->selected());
    draw();
}

void TestOwnership::deleteGridTwiceSafe()
{
    QPointer<QCPGrid> old = mPlot->xAxis->grid();

    delete old.data();

    QVERIFY(old.isNull());
    QCPGrid* fresh = mPlot->xAxis->grid();
    QVERIFY(fresh != nullptr);
    QCOMPARE(fresh->parent(), static_cast<QObject*>(mPlot->xAxis));
    QCOMPARE(mPlot->xAxis->grid(), fresh);
    draw();
    delete mPlot; // the ASan run checks the axis does not delete the first grid again
    mPlot = nullptr;
}

void TestOwnership::deleteLayerLeavesLists()
{
    showAndExpose();
    QCPLayer* main = mPlot->layer("main");
    QVERIFY(mPlot->addLayer("extra", main, QCustomPlot::limAbove));
    QCPLayer* extra = mPlot->layer("extra");
    auto* graph = addRedGraph();
    QVERIFY(graph->setLayer(extra));
    mPlot->replot();
    QCoreApplication::processEvents();
    const int count = mPlot->layerCount();

    delete extra;

    QCOMPARE(mPlot->layerCount(), count - 1);
    QVERIFY(mPlot->layer("extra") == nullptr);
    QCOMPARE(graph->layer(), main);
    QVERIFY(main->children().contains(graph));
    QCOMPARE(mPlot->currentLayer(), main);
    mPlot->replot();
    QCoreApplication::processEvents();
    QVERIFY(drawsRed());
}

void TestOwnership::deleteCurrentLayer()
{
    QCPLayer* current = mPlot->currentLayer();
    QCPLayer* below = mPlot->layer(current->index() - 1);

    delete current;

    QCOMPARE(mPlot->currentLayer(), below);
    QCOMPARE(mPlot->addGraph()->layer(), below);
    draw();
}

void TestOwnership::deleteRootLayoutAndOverlay()
{
    delete mPlot->plotLayout();

    QVERIFY(mPlot->plotLayout() != nullptr);
    QCOMPARE(mPlot->plotLayout()->elementCount(), 0);
    QVERIFY(mPlot->xAxis == nullptr);
    QVERIFY(mPlot->legend == nullptr);
    draw();

    QPointer<QCPOverlay> overlay = mPlot->overlay();
    delete overlay.data();

    QVERIFY(overlay.isNull());
    QVERIFY(mPlot->overlay() != nullptr);
    mPlot->overlay()->showMessage("back");
    draw();
}

void TestOwnership::deleteOwnedTheme()
{
    delete mPlot->theme();
    QVERIFY(mPlot->theme() == nullptr);

    mPlot->setTheme(nullptr);

    QVERIFY(mPlot->theme() != nullptr);
    mPlot->theme()->setBackground(Qt::darkGray);
    QCoreApplication::processEvents();
    draw();
}

void TestOwnership::removeAxisHidesItsPlottables()
{
    auto* graph = addRedGraph();
    QVERIFY(drawsRed());

    QVERIFY(mPlot->axisRect()->removeAxis(mPlot->axisRect()->axis(QCPAxis::atLeft)));

    QVERIFY(graph->valueAxis() == nullptr);
    QVERIFY(mPlot->yAxis == nullptr);
    QVERIFY(!drawsRed());
    draw();
    QCOMPARE(gMisuses, 1);
}

void TestOwnership::deleteTracedGraph()
{
    auto* graph = addRedGraph();
    graph->setPen(QPen(Qt::blue, 2));
    auto* tracer = new QCPItemTracer(mPlot);
    tracer->setStyle(QCPItemTracer::tsCircle);
    tracer->setPen(QPen(Qt::red, 6));
    tracer->setSize(20);
    tracer->setGraph(graph);
    tracer->setGraphKey(5);
    QVERIFY(drawsRed());

    delete graph;

    QVERIFY(tracer->graph() == nullptr);
    QVERIFY(!drawsRed());
    draw();
    QCOMPARE(gMisuses, 0);
}

void TestOwnership::deleteColorScaleOfMap()
{
    auto* map = new QCPColorMap(mPlot->xAxis, mPlot->yAxis);
    map->data()->setSize(4, 4);
    map->data()->setRange(QCPRange(0, 10), QCPRange(0, 10));
    for (int x = 0; x < 4; ++x)
        for (int y = 0; y < 4; ++y)
            map->data()->setCell(x, y, x + y);
    auto* scale = new QCPColorScale(mPlot);
    mPlot->plotLayout()->addElement(0, 1, scale);
    map->setColorScale(scale);
    map->rescaleDataRange();
    draw();

    delete scale;

    QVERIFY(map->colorScale() == nullptr);
    map->setDataRange(QCPRange(0, 3));
    map->setGradient(QCPColorGradient(QCPColorGradient::gpHot));
    map->setDataScaleType(QCPAxis::stLogarithmic);
    draw();
}

void TestOwnership::deleteBarsInGroup()
{
    const auto barsIn = [this](QCPBarsGroup* group) {
        auto* bars = new QCPBars(mPlot->xAxis, mPlot->yAxis);
        bars->setData(QVector<double> { 1, 2 }, QVector<double> { 3, 4 });
        bars->setBarsGroup(group);
        return bars;
    };
    auto* group = new QCPBarsGroup(mPlot);
    auto* first = barsIn(group);
    auto* second = barsIn(group);
    draw();

    delete first;

    QCOMPARE(group->bars(), QList<QCPBars*> { second });
    draw();
    delete group;
    QVERIFY(second->barsGroup() == nullptr);
    draw();

    auto* other = new QCPBarsGroup(mPlot);
    auto* third = barsIn(other);
    auto* fourth = barsIn(other);
    delete other;
    QVERIFY(third->barsGroup() == nullptr);
    QVERIFY(fourth->barsGroup() == nullptr);
    draw();
    delete third;
    draw();
}

void TestOwnership::polarRemoveGraphUnlistsFirst()
{
    mPlot->plotLayout()->clear();
    auto* angular = new QCPPolarAxisAngular(mPlot);
    mPlot->plotLayout()->addElement(0, 0, angular);
    auto* first = new QCPPolarGraph(angular, angular->radialAxis());
    auto* second = new QCPPolarGraph(angular, angular->radialAxis());
    first->setData(QVector<double> { 0, 90 }, QVector<double> { 1, 2 });
    second->setData(QVector<double> { 0, 90 }, QVector<double> { 2, 1 });
    const QPointer<QCPPolarGraph> firstAlive(first), secondAlive(second);
    // Raw captures on purpose: the second slot asks to remove the graph being deleted.
    connect(first, &QObject::destroyed, angular, [angular, second] { angular->removeGraph(second); });
    connect(second, &QObject::destroyed, angular, [angular, first] { angular->removeGraph(first); });

    QVERIFY(angular->removeGraph(first));

    QVERIFY(firstAlive.isNull());
    QVERIFY(secondAlive.isNull());
    QVERIFY(!angular->removeGraph(second));
    draw();
}

namespace
{
class OpenIntervals : public QCPIntervals
{
public:
    using QCPIntervals::QCPIntervals;
    using QCPIntervals::requestRepaint;
};

qcp::intervals::Columns twoIntervals()
{
    qcp::intervals::Columns c;
    c.start = { 10, 60 };
    c.stop = { 40, 90 };
    c.lane = { 0, 1 };
    c.category = { 0, 1 };
    c.ids = { 100, 101 };
    return c;
}

void sendMouse(QWidget* w, QEvent::Type type, QPoint pos, Qt::MouseButtons buttons)
{
    QMouseEvent e(type, pos, w->mapToGlobal(pos), Qt::LeftButton, buttons, Qt::NoModifier);
    QCoreApplication::sendEvent(w, &e);
}

//! Every public entry point, and the key and mouse paths that edit.
void exerciseIntervals(QCPIntervals* iv, QCustomPlot* plot)
{
    const QPointF pos = plot->axisRect()->rect().center();
    iv->setData(twoIntervals());
    iv->setStackMode(QCPIntervals::skTime);
    iv->setCategoryOrder({ 1, 0 });
    iv->setStackMode(QCPIntervals::skCategory);
    iv->setStyle(QCPIntervals::stWave);
    iv->setForbidOverlap(true);
    iv->setEditable(true);
    iv->setEditModes(QCPIntervals::emMove | QCPIntervals::emResize | QCPIntervals::emChangeLane
                     | QCPIntervals::emCreate | QCPIntervals::emDelete);
    iv->setSnap(QCPIntervals::snEdges);
    iv->setCategoryNames({ "a", "b" });
    iv->setCategoryColors({ Qt::red, Qt::blue });
    iv->setSelectedRows({ 0, 1 });
    (void)iv->legendEntries();
    (void)iv->hitTest(pos);
    (void)iv->pixelOf(20, 0);
    (void)iv->dataPixelPosition(0);
    (void)iv->selectTest(pos, false);
    (void)iv->selectTestRect(plot->axisRect()->rect(), false);
    (void)iv->cursorAt(pos);
    bool found = false;
    (void)iv->getKeyRange(found);
    for (int key : { Qt::Key_Left, Qt::Key_Up, Qt::Key_Delete, Qt::Key_Escape })
    {
        QKeyEvent e(QEvent::KeyPress, key, Qt::NoModifier);
        (void)iv->keyPress(&e);
    }
    sendMouse(plot, QEvent::MouseButtonPress, pos.toPoint(), Qt::LeftButton);
    sendMouse(plot, QEvent::MouseMove, pos.toPoint() + QPoint(30, 20), Qt::LeftButton);
    sendMouse(plot, QEvent::MouseButtonRelease, pos.toPoint() + QPoint(30, 20), Qt::NoButton);
    plot->replot();
    QVERIFY(!plot->toPixmap(400, 300).isNull());
}
} // namespace

void TestOwnership::intervalsWithoutLaneLayout()
{
    mPlot->xAxis->setRange(0, 100);
    mPlot->setInteractions(QCP::iSelectPlottables | QCP::iMultiSelect);

    auto* unlaid = new QCPIntervals(mPlot->xAxis, mPlot->yAxis, nullptr);
    QVERIFY(unlaid->laneLayout() == nullptr);
    QCOMPARE(gMisuses, 1);
    exerciseIntervals(unlaid, mPlot);
    QCOMPARE(gMisuses, 1);
    QVERIFY(mPlot->removePlottable(unlaid));

    gMisuses = 0;
    auto* layout = new QCPLaneLayout;
    layout->laneIndices({ "A", "B" });
    auto* orphaned = new QCPIntervals(mPlot->xAxis, mPlot->yAxis, layout);
    orphaned->setData(twoIntervals());
    orphaned->setEditable(true);
    mPlot->replot();
    const QPoint onBar = orphaned->pixelOf(25, 0).toPoint();
    sendMouse(mPlot, QEvent::MouseButtonPress, onBar, Qt::LeftButton);
    QVERIFY(orphaned->gestureActive());
    QCOMPARE(gMisuses, 0);

    delete layout;

    sendMouse(mPlot, QEvent::MouseMove, onBar + QPoint(30, 20), Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseButtonRelease, onBar + QPoint(30, 20), Qt::NoButton);
    QVERIFY(!orphaned->gestureActive());
    exerciseIntervals(orphaned, mPlot);
    QCOMPARE(gMisuses, 1);

    gMisuses = 0;
    auto* inert = new OpenIntervals(nullptr, nullptr, nullptr);
    QVERIFY(gMisuses >= 1); // the null axis, and the missing layout
    inert->requestRepaint();
    delete inert;
}

void TestOwnership::deletePolarGraphInLegend()
{
    auto* angular = new QCPPolarAxisAngular(mPlot);
    mPlot->plotLayout()->addElement(0, 1, angular);
    const int rows = mPlot->legend->itemCount();
    auto* graph = new QCPPolarGraph(angular, angular->radialAxis());
    graph->setName("polar");
    graph->setData(QVector<double> { 0, 90 }, QVector<double> { 1, 2 });
    QCOMPARE(mPlot->legend->itemCount(), rows + 1);
    draw();

    delete graph;

    QCOMPARE(mPlot->legend->itemCount(), rows);
    draw();
}

void TestOwnership::intervalsWithoutAxes()
{
    QCPLaneLayout layout;
    layout.laneIndices({ "A", "B" });
    auto* inert = new QCPIntervals(nullptr, nullptr, &layout);
    QCOMPARE(gMisuses, 1);
    exerciseIntervals(inert, mPlot);
    QCOMPARE(gMisuses, 1);
    delete inert;

    gMisuses = 0;
    mPlot->xAxis->setRange(0, 100);
    mPlot->setInteractions(QCP::iSelectPlottables | QCP::iMultiSelect);
    auto* bereft = new QCPIntervals(mPlot->xAxis, mPlot->yAxis, &layout);
    bereft->setData(twoIntervals());
    bereft->setEditable(true);
    mPlot->replot();
    const QPoint onBar = bereft->pixelOf(25, 0).toPoint();
    sendMouse(mPlot, QEvent::MouseButtonPress, onBar, Qt::LeftButton);
    QVERIFY(bereft->gestureActive());
    QCOMPARE(gMisuses, 0);

    QCPAxis* keyAxis = mPlot->xAxis;
    delete keyAxis;

    sendMouse(mPlot, QEvent::MouseMove, onBar + QPoint(30, 20), Qt::LeftButton);
    sendMouse(mPlot, QEvent::MouseButtonRelease, onBar + QPoint(30, 20), Qt::NoButton);
    QVERIFY(!bereft->gestureActive());
    exerciseIntervals(bereft, mPlot);
    QCOMPARE(gMisuses, 1);
    QVERIFY(mPlot->removePlottable(bereft)); // before the stack layout goes
}
