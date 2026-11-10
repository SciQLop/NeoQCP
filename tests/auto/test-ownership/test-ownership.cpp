#include "test-ownership.h"

#include <QtWidgets/qtestsupport_widgets.h> // QTest::qWaitForWindowExposed

namespace
{
int gMisuses = 0;

void countMisuse(const char*, const char*)
{
    ++gMisuses;
}
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

namespace
{
// A drawn plottable that deletes another one while it records itself: an export defers barA's GPU
// callback to the end of the layer's pass, so this delete lands before that callback runs.
class DeletingBars : public QCPBars
{
public:
    using QCPBars::QCPBars;
    mutable QCPBars* victim = nullptr;

protected:
    void draw(QCPDrawContext& context) const override
    {
        if (QCPBars* doomed = victim)
        {
            victim = nullptr;
            delete doomed;
        }
        QCPBars::draw(context);
    }
};
} // namespace

void TestOwnership::deletingAPlottableFromAnotherDrawKeepsTheDeferredPainterSafe()
{
    mPlot->legend->setVisible(false);
    auto* doomed = new QCPBars(mPlot->xAxis, mPlot->yAxis);
    doomed->setData({ 1.0, 2.0, 3.0 }, { 1.0, 2.0, 1.5 });
    auto* deleter = new DeletingBars(mPlot->xAxis, mPlot->yAxis);
    deleter->victim = doomed;
    deleter->setData({ 4.0, 5.0 }, { 1.0, 2.0 });
    mPlot->rescaleAxes();

    QVERIFY(!mPlot->toPixmap(400, 300).isNull());
    QCOMPARE(mPlot->plottableCount(), 1);
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

namespace
{
template <typename Row>
Row* findRow(QCPLegend* legend)
{
    for (int i = 0; i < legend->itemCount(); ++i)
        if (auto* row = qobject_cast<Row*>(legend->item(i)))
            return row;
    return nullptr;
}

QCPIntervals* intervalsInLegend(QCustomPlot* plot, QCPLaneLayout* layout)
{
    layout->laneIndices({ "A" });
    auto* intervals = new QCPIntervals(plot->xAxis, plot->yAxis, layout);
    (void)intervals->addToLegend(plot->legend);
    return intervals;
}
} // namespace

void TestOwnership::deleteMultiGraphLeavesSecondLegend()
{
    auto* multiGraph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    auto* second = addSecondLegend();
    QVERIFY(second->addItem(findRow<QCPGroupLegendItem>(mPlot->legend)));
    second->setVisible(true);

    delete multiGraph;

    QCOMPARE(second->itemCount(), 0);
    draw();
}

void TestOwnership::deleteIntervalsLeavesItsRows()
{
    QCPLaneLayout layout;
    auto* intervals = intervalsInLegend(mPlot, &layout);
    QVERIFY(findRow<QCPIntervalsLegendItem>(mPlot->legend));
    auto* second = addSecondLegend();
    QVERIFY(intervals->addToLegend(second));
    draw();

    delete intervals;

    QVERIFY(!findRow<QCPIntervalsLegendItem>(mPlot->legend));
    QCOMPARE(second->itemCount(), 0);
    draw();
}

// A row taken out of every legend is in none when its plottable dies: nothing removes it, and it
// must not keep a dangling pointer for when it is added back.
void TestOwnership::detachedRowsOutliveTheirPlottables()
{
    QCPLaneLayout layout;
    auto* multiGraph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    auto* intervals = intervalsInLegend(mPlot, &layout);
    auto* groupRow = findRow<QCPGroupLegendItem>(mPlot->legend);
    auto* intervalsRow = findRow<QCPIntervalsLegendItem>(mPlot->legend);
    QVERIFY(groupRow && intervalsRow);
    QVERIFY(mPlot->legend->take(groupRow));
    QVERIFY(mPlot->legend->take(intervalsRow));

    delete multiGraph;
    delete intervals;

    QVERIFY(groupRow->multiGraph() == nullptr);
    QVERIFY(intervalsRow->intervals() == nullptr);
    QVERIFY(mPlot->legend->addItem(groupRow));
    QVERIFY(mPlot->legend->addItem(intervalsRow));
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

void TestOwnership::nullAxisPlottableIsDetachedHalfAxedIsRefused()
{
    auto* orphan = new QCPCurve(nullptr, nullptr);
    QCOMPARE(gMisuses, 0);
    QVERIFY(orphan->parentPlot() == nullptr);

    auto* halfAxed = new QCPCurve(mPlot->xAxis, nullptr);
    QCOMPARE(gMisuses, 1);
    QVERIFY(halfAxed->parentPlot() == nullptr);

    auto* graph = new QCPGraph(nullptr, nullptr);
    auto* histogram = new QCPHistogram2D(nullptr, nullptr);
    QCOMPARE(gMisuses, 1);

    QCPLaneLayout lanes;
    auto* intervals = new QCPIntervals(nullptr, nullptr, &lanes);
    QCOMPARE(gMisuses, 1);
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
    auto* second = new QCPItemLine(mPlot);
    QPointer<QCPItemLine> first = new QCPItemLine(mPlot);
    new QCPItemText(mPlot); // cleared last: its slot deletes an item of the snapshot
    // clearItems deletes oldest first, so `first` may already be gone by the time this runs: the
    // guarantee is that neither the slot nor the clear crashes, not that a sibling outlives it.
    connect(mPlot->item(), &QObject::destroyed, mPlot, [first] { delete first.data(); });
    Q_UNUSED(second);

    QCOMPARE(mPlot->clearItems(), 3);

    QCOMPARE(mPlot->itemCount(), 0);
    draw();
}

void TestOwnership::clearItemsKeepsOtherLayersOrder()
{
    auto graph = mPlot->add<QCPGraph>();
    QVERIFY(mPlot->addLayer(QStringLiteral("upper")));
    auto* upper = mPlot->layer(QStringLiteral("upper"));
    auto* a = new QCPItemLine(mPlot);
    auto* b = new QCPItemLine(mPlot);
    auto* c = new QCPItemLine(mPlot);
    auto* d = new QCPItemLine(mPlot);
    c->setLayer(upper);
    d->setLayer(upper);
    Q_UNUSED(a);
    Q_UNUSED(b);
    mPlot->replot();

    QCOMPARE(mPlot->clearItems(), 4);

    QCOMPARE(mPlot->itemCount(), 0);
    {
        const auto kids = mPlot->layer("main")->children();
        QVERIFY(kids.contains(graph.get())); // the layer keeps its non-item content
        for (auto* kid : kids)
            QVERIFY2(!qobject_cast<QCPAbstractItem*>(kid), "an item is still in the layer");
    }
    QVERIFY(upper->children().isEmpty());
    QVERIFY(mPlot->isRepaintQueued());
    draw();
}

void TestOwnership::clearItemsSlotDeletesAnother()
{
    auto* victim = new QCPItemLine(mPlot);
    QVERIFY(mPlot->addLayer(QStringLiteral("upper")));
    auto* upper = mPlot->layer(QStringLiteral("upper"));
    auto* other = new QCPItemLine(mPlot);
    other->setLayer(upper);
    connect(victim, &QObject::destroyed, mPlot, [other] { delete other; });

    QCOMPARE(mPlot->clearItems(), 2);

    QCOMPARE(mPlot->itemCount(), 0);
    QVERIFY(upper->children().isEmpty());
    draw();
}

void TestOwnership::itemOrderAfterRemove()
{
    auto* a = new QCPItemLine(mPlot);
    auto* b = new QCPItemLine(mPlot);
    auto* c = new QCPItemLine(mPlot);
    QCOMPARE(mPlot->item(0), a);
    QCOMPARE(mPlot->item(1), b);
    QCOMPARE(mPlot->item(2), c);

    mPlot->removeItem(b);

    QCOMPARE(mPlot->itemCount(), 2);
    QCOMPARE(mPlot->item(0), a);
    QCOMPARE(mPlot->item(1), c);
    QVERIFY(mPlot->item(2) == nullptr);
    draw();
}

void TestOwnership::layerOrderAfterMoves()
{
    QVERIFY(mPlot->addLayer(QStringLiteral("upper")));
    auto* upper = mPlot->layer(QStringLiteral("upper"));
    QVERIFY(mPlot->addLayer(QStringLiteral("third")));
    auto* third = mPlot->layer(QStringLiteral("third"));
    auto first = mPlot->add<QCPGraph>();
    auto second = mPlot->add<QCPGraph>();
    first->setLayer(upper);
    second->setLayer(upper);
    QVERIFY(upper->children() == (QList<QCPLayerable*> { first.get(), second.get() }));

    first->setLayer(third);
    QVERIFY(third->children() == QList<QCPLayerable*> { first.get() });
    QVERIFY(upper->children() == QList<QCPLayerable*> { second.get() });

    first->setLayer(upper); // coming back appends: drawing order is the layer list order
    QVERIFY(upper->children() == (QList<QCPLayerable*> { second.get(), first.get() }));
    QVERIFY(third->children().isEmpty());
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
    const auto mouse = [this](QEvent::Type type, QPoint pos, Qt::MouseButtons buttons)
    {
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

void TestOwnership::deletedGridStaysDeleted()
{
    delete mPlot->xAxis->grid();

    mPlot->xAxis->setRange(1, 7); // dirty-marks the axis rect's layers
    draw();
    mPlot->replot(QCustomPlot::rpQueuedReplot);
    QCoreApplication::processEvents();

    QVERIFY(mPlot->xAxis->gridIfExists() == nullptr);
    QVERIFY(mPlot->xAxis->grid() != nullptr); // an explicit call still makes one
}

// Run under xvfb: offscreen gives no QRhi, so no GPU grid layer.
void TestOwnership::deletedGridStaysDeletedOnGpu()
{
    showAndExpose();
    if (!mPlot->rhi())
        QSKIP("no QRhi on this platform");
    mPlot->xAxis->grid()->setVisible(true);
    mPlot->yAxis->grid()->setVisible(true);
    mPlot->replot();
    QCoreApplication::processEvents();
    QVERIFY(mPlot->gridRhiLayerIfExists() != nullptr);

    delete mPlot->xAxis->grid();
    mPlot->xAxis->setRange(1, 7);
    mPlot->replot();
    mPlot->repaint();
    QCoreApplication::processEvents();

    QVERIFY(mPlot->xAxis->gridIfExists() == nullptr);
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

// A plain delete bypasses removeLayer's "keep one layer" rule.
void TestOwnership::deleteLastLayer()
{
    while (mPlot->layerCount() > 0)
        delete mPlot->layer(0);

    QVERIFY(mPlot->currentLayer() == nullptr);
    auto* graph = mPlot->addGraph();
    QVERIFY(graph);
    graph->setData(QVector<double> { 1, 2 }, QVector<double> { 1, 2 });
    QVERIFY(graph->layer() == nullptr); // drawn nowhere until given a layer
    new QCPAxisRect(mPlot);
    draw();
    QVERIFY(mPlot->overlay() != nullptr);
    QVERIFY(mPlot->addLayer("again"));
    QVERIFY(graph->setLayer("again"));
    draw();
    QCOMPARE(gMisuses, 0);
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
    const auto barsIn = [this](QCPBarsGroup* group)
    {
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
    auto* area = new QCPPolarArea(mPlot);
    mPlot->plotLayout()->addElement(0, 0, area);
    auto* angular = area->angularAxis();
    auto* first = new QCPPolarGraph(angular, area->radialAxis());
    auto* second = new QCPPolarGraph(angular, area->radialAxis());
    first->setData(QVector<double> { 0, 90 }, QVector<double> { 1, 2 });
    second->setData(QVector<double> { 0, 90 }, QVector<double> { 2, 1 });
    const QPointer<QCPPolarGraph> firstAlive(first), secondAlive(second);
    // Raw captures on purpose: the second slot asks to remove the graph being deleted.
    connect(first, &QObject::destroyed, mPlot,
            [plot = mPlot, second] { plot->removePlottable(second); });
    connect(second, &QObject::destroyed, mPlot,
            [plot = mPlot, first] { plot->removePlottable(first); });

    // The plot owns the graphs: removing one unlists it before the delete, so a slot connected to
    // its destruction may remove another.
    QVERIFY(mPlot->removePlottable(first));

    QVERIFY(firstAlive.isNull());
    QVERIFY(secondAlive.isNull());
    QVERIFY(!mPlot->removePlottable(second));
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
    auto* area = new QCPPolarArea(mPlot);
    mPlot->plotLayout()->addElement(0, 1, area);
    const int rows = mPlot->legend->itemCount();
    auto* graph = new QCPPolarGraph(area->angularAxis(), area->radialAxis());
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
    QCOMPARE(gMisuses, 0);
    exerciseIntervals(inert, mPlot);
    QCOMPARE(gMisuses, 0);
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

void TestOwnership::deletePolarAreaWithGraphs()
{
    auto* area = new QCPPolarArea(mPlot);
    auto* other = new QCPPolarArea(mPlot);
    QCPPolarAxisAngular* angular = area->angularAxis();
    mPlot->plotLayout()->addElement(0, 1, area);
    mPlot->plotLayout()->addElement(0, 2, other);
    auto* survivor = new QCPPolarGraph(angular, area->radialAxis());
    survivor->setData(QVector<double> { 0, 90 }, QVector<double> { 1, 2 });
    auto* moved = new QCPPolarGraph(angular, area->radialAxis());
    moved->setSystem(other->system(other->angularAxis(), other->radialAxis()));
    delete moved;
    area->rescale(); // walks its graph list: the moved graph left it
    draw();

    delete area;

    QVERIFY(survivor->system() == nullptr);
    draw();
    delete survivor;
    draw();

    auto* plot = new QCustomPlot;
    auto* held = new QCPPolarArea(plot);
    plot->plotLayout()->addElement(0, 1, held);
    new QCPPolarGraph(held->angularAxis(), held->radialAxis());
    delete plot;
}

void TestOwnership::movePolarGraphKeepsOneLegendRow()
{
    auto* area = new QCPPolarArea(mPlot);
    auto* other = new QCPPolarArea(mPlot);
    QCPPolarAxisAngular* angular = area->angularAxis();
    mPlot->plotLayout()->addElement(0, 1, area);
    mPlot->plotLayout()->addElement(0, 2, other);
    auto* graph = new QCPPolarGraph(angular, area->radialAxis());
    const int rows = mPlot->legend->itemCount();

    graph->setSystem(other->system(other->angularAxis(), other->radialAxis()));
    QCOMPARE(mPlot->legend->itemCount(), rows);

    QVERIFY(!graph->addToLegend());
    QCOMPARE(mPlot->legend->itemCount(), rows);
    QVERIFY(graph->removeFromLegend());
    QVERIFY(graph->addToLegend());
    QVERIFY(!graph->addToLegend());
    QCOMPARE(mPlot->legend->itemCount(), rows);
    draw();
}

void TestOwnership::destroyedSlotAddsDuringTeardown()
{
    auto* plot = new QCustomPlot;
    auto* graph = plot->addGraph();
    QCPGraph* added = graph;
    connect(graph, &QObject::destroyed, [plot, &added] { added = plot->addGraph(); });

    delete plot;

    QVERIFY(added == nullptr);
    QCOMPARE(gMisuses, 1);
}

void TestOwnership::destroyedSlotConstructsDuringTeardown()
{
    auto* plot = new QCustomPlot;
    auto* graph = plot->addGraph();
    connect(graph, &QObject::destroyed,
            [plot]
            {
                new QCPCurve(plot->xAxis, plot->yAxis);
                new QCPItemText(plot);
            });

    delete plot;

    QCOMPARE(gMisuses, 2); // one refused registration each
}

void TestOwnership::destroyedSlotDeletesSiblingDuringTeardown()
{
    auto* plot = new QCustomPlot;
    plot->legend->setVisible(true);
    auto* group = plot->addPlottableGroup("pair");
    QPointer<QCPGraph> first = plot->addGraph();
    auto* second = plot->addGraph();
    group->addMember(first);
    group->addMember(second);
    QPointer<QCPAbstractItem> item = new QCPItemText(plot);
    auto* last = plot->addGraph();
    connect(last, &QObject::destroyed, [first] { delete first.data(); });
    connect(second, &QObject::destroyed, [item] { delete item.data(); });

    delete plot;

    QCOMPARE(gMisuses, 0);
}

void TestOwnership::childrenDeletedWhilePlotWhole()
{
    auto* plot = new QCustomPlot;
    plot->resize(400, 300);
    plot->replot();
    QCPAxisRect* const rectBefore = plot->axisRect();
    const QRect before = rectBefore->rect();
    QVERIFY(before.isValid());
    auto* graph = plot->addGraph();
    auto* item = new QCPItemText(plot);
    QRect seenByGraph, seenByItem;
    QCPAxisRect* rectSeenByGraph = nullptr;
    QCPAxisRect* rectSeenByItem = nullptr;
    auto rectOf = [plot] { return plot->axisRect() ? plot->axisRect()->rect() : QRect(); };
    connect(graph, &QObject::destroyed,
            [&]
            {
                seenByGraph = rectOf();
                rectSeenByGraph = plot->axisRect();
            });
    connect(item, &QObject::destroyed,
            [&]
            {
                seenByItem = rectOf();
                rectSeenByItem = plot->axisRect();
            });

    delete plot;

    QCOMPARE(seenByGraph, before);
    QCOMPARE(seenByItem, before);
    QCOMPARE(rectSeenByGraph, rectBefore);
    QCOMPARE(rectSeenByItem, rectBefore);
}

void TestOwnership::mutatorsRefuseDuringTeardown()
{
    auto* plot = new QCustomPlot;
    QPointer<QCPGraph> sibling = plot->addGraph();
    QPointer<QCPPlottableGroup> group = plot->addPlottableGroup("g");
    QPointer<QCPAbstractItem> item = new QCPItemText(plot);
    QVERIFY(plot->addLayer("extra"));
    auto* watched = plot->addGraph();
    const int plottables = plot->plottableCount();
    const int items = plot->itemCount();
    const int layers = plot->layerCount();

    QPointer<QCPLayer> main = plot->layer("main");
    QPointer<QCPLayer> extra = plot->layer("extra");
    const QList<std::pair<const char*, std::function<void()>>> mutators {
        { "addGraph", [&] { QVERIFY(!plot->addGraph()); } },
        { "addLayer", [&] { QVERIFY(!plot->addLayer("late")); } },
        { "removeLayer", [&] { QVERIFY(!plot->removeLayer(extra)); } },
        { "moveLayer", [&] { QVERIFY(!plot->moveLayer(extra, main)); } },
        { "setCurrentLayer(name)", [&] { QVERIFY(!plot->setCurrentLayer("extra")); } },
        { "setCurrentLayer(layer)", [&] { QVERIFY(!plot->setCurrentLayer(extra)); } },
        { "removePlottable", [&] { QVERIFY(!plot->removePlottable(sibling)); } },
        { "removePlottable(index)", [&] { QVERIFY(!plot->removePlottable(0)); } },
        { "removeGraph", [&] { QVERIFY(!plot->removeGraph(sibling)); } },
        { "removeGraph(index)", [&] { QVERIFY(!plot->removeGraph(0)); } },
        { "clearPlottables", [&] { QCOMPARE(plot->clearPlottables(), 0); } },
        { "clearGraphs", [&] { QCOMPARE(plot->clearGraphs(), 0); } },
        { "addPlottableGroup", [&] { QVERIFY(!plot->addPlottableGroup("late")); } },
        { "removePlottableGroup", [&] { QVERIFY(!plot->removePlottableGroup(group)); } },
        { "removeItem", [&] { QVERIFY(!plot->removeItem(item)); } },
        { "removeItem(index)", [&] { QVERIFY(!plot->removeItem(0)); } },
        { "clearItems", [&] { QCOMPARE(plot->clearItems(), 0); } },
        { "setTheme", [&] { plot->setTheme(nullptr); } },
    };
    QStringList unrefused;
    int seenPlottables = -1, seenItems = -1, seenLayers = -1;
    connect(watched, &QObject::destroyed,
            [&]
            {
                for (const auto& [name, call] : mutators)
                {
                    const int before = gMisuses;
                    call();
                    if (gMisuses != before + 1)
                        unrefused << name;
                }
                seenPlottables = plot->plottableCount();
                seenItems = plot->itemCount();
                seenLayers = plot->layerCount();
            });

    delete plot;

    QCOMPARE(unrefused, QStringList());
    QCOMPARE(seenPlottables, plottables - 1); // only the watched graph went
    QCOMPARE(seenItems, items);
    QCOMPARE(seenLayers, layers);
}

void TestOwnership::recreatingGettersReturnNullDuringTeardown()
{
    auto* plot = new QCustomPlot;
    QCPAxis* axis = plot->xAxis;
    bool layoutMade = true, overlayMade = true, gridMade = true;
    connect(plot->plotLayout(), &QObject::destroyed,
            [&]
            {
                layoutMade = plot->plotLayout() != nullptr;
                overlayMade = plot->overlay() != nullptr;
            });
    connect(axis->grid(), &QObject::destroyed, [&] { gridMade = axis->grid() != nullptr; });

    delete plot;

    QVERIFY(!layoutMade);
    QVERIFY(!overlayMade);
    QVERIFY(!gridMade);
    QCOMPARE(gMisuses, 0); // a getter answers, it does not complain
}

// Run under xvfb with ASan to see the GPU side: offscreen gives no QRhi.
void TestOwnership::shownPlotTeardownFreesGpuResources()
{
    showAndExpose();
    if (!mPlot->rhi())
        QSKIP("no QRhi on this platform");
    auto* span = new QCPItemSpan(mPlot, Qt::Vertical);
    span->setRange(QCPRange(2, 4));
    mPlot->xAxis->grid()->setVisible(true);
    mPlot->replot();
    QCoreApplication::processEvents();
    QVERIFY(mPlot->gridRhiLayerIfExists() != nullptr);
    delete mPlot;
    mPlot = nullptr;
}

void TestOwnership::newAxisRectDuringTeardown()
{
    auto* plot = new QCustomPlot;
    auto* graph = plot->addGraph();
    int axes = -1;
    QCPAxis* added = nullptr;
    connect(graph, &QObject::destroyed,
            [&]
            {
                auto* rect = new QCPAxisRect(plot);
                added = rect->addAxis(QCPAxis::atLeft);
                axes = rect->axes().size();
                rect->setupFullAxesBox(true);
            });

    delete plot;

    QVERIFY(added == nullptr);
    QCOMPARE(axes, 0);
    QCOMPARE(gMisuses, 3); // the default axes, the added one, the axes box
}

void TestOwnership::newColorScaleDuringTeardown()
{
    auto* plot = new QCustomPlot;
    auto* graph = plot->addGraph();
    connect(graph, &QObject::destroyed,
            [plot]
            {
                auto* scale = new QCPColorScale(plot);
                scale->setDataRange(QCPRange(1, 2));
                scale->setDataScaleType(QCPAxis::stLogarithmic);
                scale->setGradient(QCPColorGradient(QCPColorGradient::gpHot));
                scale->setLabel("late");
                scale->setType(QCPAxis::atBottom);
                scale->setRangeDrag(true);
                scale->setRangeZoom(true);
                scale->rescaleDataRange(false);
                (void)scale->axis();
            });

    delete plot;

    QCOMPARE(gMisuses, 1); // its axis rect was refused its axes
}
