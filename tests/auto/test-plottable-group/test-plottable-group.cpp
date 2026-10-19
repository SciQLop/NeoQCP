#include "test-plottable-group.h"

void TestPlottableGroup::init()
{
    mPlot = new QCustomPlot(nullptr);
    mPlot->legend->setVisible(true);
}

void TestPlottableGroup::cleanup()
{
    delete mPlot;
}

QCPPlottableGroupLegendItem* TestPlottableGroup::groupRow() const
{
    for (int i = 0; i < mPlot->legend->itemCount(); ++i)
        if (auto* row = qobject_cast<QCPPlottableGroupLegendItem*>(mPlot->legend->item(i)))
            return row;
    return nullptr;
}

int TestPlottableGroup::plottableRows() const
{
    int n = 0;
    for (int i = 0; i < mPlot->legend->itemCount(); ++i)
        if (!qobject_cast<QCPPlottableGroupLegendItem*>(mPlot->legend->item(i)))
            ++n;
    return n;
}

void TestPlottableGroup::groupHasOneLegendRow()
{
    auto* centre = mPlot->addGraph();
    auto* low = mPlot->addGraph();
    auto* high = mPlot->addGraph();
    QCOMPARE(mPlot->legend->itemCount(), 3);

    auto* group = mPlot->addPlottableGroup("Shue");
    QVERIFY(group->addMember(centre));
    QVERIFY(group->addMember(low));
    QVERIFY(group->addMember(high));

    QCOMPARE(mPlot->legend->itemCount(), 1);
    QVERIFY(groupRow());
    QCOMPARE(groupRow()->group(), group);
    QCOMPARE(group->members().size(), 3);
    QCOMPARE(centre->group(), group);
    QCOMPARE(mPlot->plottableGroups(), QList<QCPPlottableGroup*> { group });
}

void TestPlottableGroup::groupedPlottableCannotRejoinTheLegend()
{
    auto* graph = mPlot->addGraph();
    auto* group = mPlot->addPlottableGroup("g");
    group->addMember(graph);

    QVERIFY(!graph->addToLegend());
    QCOMPARE(plottableRows(), 0);
}

void TestPlottableGroup::doubleClickOnTheRowIsReportedOnce()
{
    auto* outside = mPlot->addGraph();
    auto* group = mPlot->addPlottableGroup("g");
    group->addMember(mPlot->addGraph());
    group->addMember(mPlot->addGraph());
    QSignalSpy spy(group, &QCPPlottableGroup::doubleClicked);

    emit mPlot->legendDoubleClick(mPlot->legend, groupRow(), nullptr);
    QCOMPARE(spy.count(), 1);

    emit mPlot->legendDoubleClick(mPlot->legend, mPlot->legend->itemWithPlottable(outside),
                                  nullptr);
    QCOMPARE(spy.count(), 1);
}

void TestPlottableGroup::visibleWhenAnyMemberIsVisible()
{
    auto* a = mPlot->addGraph();
    auto* b = mPlot->addGraph();
    auto* group = mPlot->addPlottableGroup("g");
    group->addMember(a);
    group->addMember(b);
    QVERIFY(group->visible());

    a->setVisible(false);
    QVERIFY(group->visible());

    b->setVisible(false);
    QVERIFY(!group->visible());

    group->setVisible(true);
    QVERIFY(a->visible());
    QVERIFY(b->visible());
}

void TestPlottableGroup::removedPlottableLeavesTheGroup()
{
    auto* a = mPlot->addGraph();
    auto* b = mPlot->addGraph();
    auto* c = mPlot->addGraph();
    auto* group = mPlot->addPlottableGroup("g");
    group->addMember(a);
    group->addMember(b);
    group->addMember(c);

    QVERIFY(mPlot->removePlottable(a));
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QCOMPARE(group->members(), (QList<QCPAbstractPlottable*> { b, c }));
    QCOMPARE(mPlot->legend->itemCount(), 1);
}

void TestPlottableGroup::emptyGroupStaysWithoutARow()
{
    auto* a = mPlot->addGraph();
    QPointer<QCPPlottableGroup> group = mPlot->addPlottableGroup("g");
    group->addMember(a);

    group->removeMember(a);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(group);
    QVERIFY(!groupRow());
    QCOMPARE(plottableRows(), 1); // a got its row back

    group->addMember(a);
    QVERIFY(groupRow());
    QCOMPARE(plottableRows(), 0);
}

void TestPlottableGroup::removingTheGroupGivesRowsBack()
{
    auto* a = mPlot->addGraph();
    auto* b = mPlot->addGraph();
    b->setWantsLegend(false);
    QCOMPARE(plottableRows(), 1);
    auto* group = mPlot->addPlottableGroup("g");
    group->addMember(a);
    group->addMember(b);

    QVERIFY(mPlot->removePlottableGroup(group));
    QVERIFY(!groupRow());
    QCOMPARE(plottableRows(), 1);
    QVERIFY(mPlot->legend->hasItemWithPlottable(a));
    QCOMPARE(a->group(), nullptr);
    QVERIFY(mPlot->plottableGroups().isEmpty());
}

void TestPlottableGroup::unwantedLegendStaysOutAfterTheGroup()
{
    auto* a = mPlot->addGraph();
    auto* group = mPlot->addPlottableGroup("g");
    group->addMember(a);
    a->setWantsLegend(false);

    group->removeMember(a);
    QCOMPARE(plottableRows(), 0);

    a->setWantsLegend(true);
    QCOMPARE(plottableRows(), 1);
}

void TestPlottableGroup::multiGraphUnwantedLegendStaysOut()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    QCOMPARE(mPlot->legend->itemCount(), 1);

    mg->setWantsLegend(false);
    QCOMPARE(mPlot->legend->itemCount(), 0);
    QVERIFY(!mg->addToLegend());

    auto* group = mPlot->addPlottableGroup("g");
    group->addMember(mg);
    QCOMPARE(mPlot->legend->itemCount(), 1);
    group->removeMember(mg);
    QCOMPARE(mPlot->legend->itemCount(), 0);
}

void TestPlottableGroup::memberMovesToItsNewGroup()
{
    auto* a = mPlot->addGraph();
    auto* first = mPlot->addPlottableGroup("first");
    auto* second = mPlot->addPlottableGroup("second");
    first->addMember(a);

    second->addMember(a);
    QVERIFY(first->members().isEmpty());
    QCOMPARE(second->members(), QList<QCPAbstractPlottable*> { a });
    QCOMPARE(a->group(), second);
    QCOMPARE(mPlot->legend->itemCount(), 1);
}

void TestPlottableGroup::memberFromAnotherPlotIsRefused()
{
    QCustomPlot other;
    auto* foreign = other.addGraph();
    auto* group = mPlot->addPlottableGroup("g");

    QVERIFY(!group->addMember(foreign));
    QVERIFY(!group->addMember(nullptr));
    QVERIFY(group->members().isEmpty());
    QCOMPARE(foreign->group(), nullptr);
}

void TestPlottableGroup::hiddenGroupStaysDimmedAfterTextColorChange()
{
    auto* a = mPlot->addGraph();
    auto* group = mPlot->addPlottableGroup("g");
    group->addMember(a);
    QCOMPARE(groupRow()->effectiveTextColor().alpha(), 255);

    group->setVisible(false);
    mPlot->legend->setTextColor(Qt::white);
    QVERIFY(groupRow()->effectiveTextColor().alpha() < 255);

    group->setVisible(true);
    QCOMPARE(groupRow()->effectiveTextColor(), QColor(Qt::white));
}

void TestPlottableGroup::rowRepaintsWhenMembersHide()
{
    // The row lives on the legend layer, not the members' layers: hiding a member must dirty
    // the legend too, or the row keeps its old (undimmed) look until something else repaints it.
    auto* graph = mPlot->addGraph();
    auto* multiGraph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    multiGraph->setDataSource(
        std::make_shared<QCPSoAMultiDataSource<std::vector<double>, std::vector<double>>>(
            std::vector<double> { 1.0, 2.0 }, std::vector<std::vector<double>> { { 1.0, 2.0 } }));
    QCOMPARE(multiGraph->componentCount(), 1);
    auto* group = mPlot->addPlottableGroup("g");
    group->addMember(graph);
    group->addMember(multiGraph);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    const auto buffer = mPlot->layer("legend")->mPaintBuffer.toStrongRef();
    QVERIFY(buffer);
    QVERIFY(!buffer->contentDirty());
    graph->setVisible(false);
    QVERIFY(buffer->contentDirty());

    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(!buffer->contentDirty());
    multiGraph->setComponentVisible(0, false);
    QVERIFY(buffer->contentDirty());
    QVERIFY(!group->visible());
}

void TestPlottableGroup::busyMemberShowsTheBusySymbol()
{
    auto* a = mPlot->addGraph();
    a->setBusyShowDelayMs(10);
    auto* group = mPlot->addPlottableGroup("Shue");
    group->addMember(a);
    QCOMPARE(groupRow()->rowText(), QStringLiteral("Shue"));

    a->setBusy(true);
    QTRY_VERIFY_WITH_TIMEOUT(a->visuallyBusy(), 2000);
    QVERIFY(groupRow()->rowText().endsWith(QStringLiteral(" Shue")));
    QVERIFY(groupRow()->rowText() != QStringLiteral("Shue"));
}

void TestPlottableGroup::deletingThePlotWithGroupsIsSafe()
{
    auto* plot = new QCustomPlot(nullptr);
    auto* group = plot->addPlottableGroup("g");
    group->addMember(plot->addGraph());
    group->addMember(new QCPMultiGraph(plot->xAxis, plot->yAxis));
    plot->addPlottableGroup("empty");
    plot->replot(QCustomPlot::rpImmediateRefresh);
    delete plot;
}
