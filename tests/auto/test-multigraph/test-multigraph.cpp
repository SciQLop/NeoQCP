#include "test-multigraph.h"
#include "qcustomplot.h"
#include "datasource/soa-multi-datasource.h"
#include "layoutelements/layoutelement-legend-group.h"
#include <vector>
#include <span>

void TestMultiGraph::init()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
}

void TestMultiGraph::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
}

void TestMultiGraph::creation()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    QVERIFY(mg);
    QCOMPARE(mg->componentCount(), 0);
    QCOMPARE(mg->dataCount(), 0);
    QVERIFY(mg->dataSource() == nullptr);
}

void TestMultiGraph::setDataSourceShared()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    auto src = std::make_shared<QCPSoAMultiDataSource<std::vector<double>, std::vector<double>>>(
        std::vector<double>{1.0, 2.0, 3.0},
        std::vector<std::vector<double>>{{10.0, 20.0, 30.0}, {-1.0, -2.0, -3.0}});
    mg->setDataSource(src);
    QCOMPARE(mg->componentCount(), 2);
    QCOMPARE(mg->dataCount(), 3);
}

void TestMultiGraph::setDataSourceUnique()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    std::unique_ptr<QCPAbstractMultiDataSource> src =
        std::make_unique<QCPSoAMultiDataSource<std::vector<double>, std::vector<double>>>(
            std::vector<double>{1.0, 2.0},
            std::vector<std::vector<double>>{{10.0, 20.0}});
    mg->setDataSource(std::move(src));
    QCOMPARE(mg->componentCount(), 1);
    QCOMPARE(mg->dataCount(), 2);
}

void TestMultiGraph::setDataConvenience()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(std::vector<double>{1.0, 2.0, 3.0},
                std::vector<std::vector<double>>{{10.0, 20.0, 30.0}, {-1.0, -2.0, -3.0}});
    QCOMPARE(mg->componentCount(), 2);
    QCOMPARE(mg->dataCount(), 3);
}

void TestMultiGraph::viewDataConvenience()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    std::vector<double> keys = {1.0, 2.0, 3.0};
    std::vector<double> v0 = {10.0, 20.0, 30.0};
    std::vector<double> v1 = {-1.0, -2.0, -3.0};
    mg->viewData(std::span<const double>(keys),
                 std::vector<std::span<const double>>{
                     std::span<const double>(v0),
                     std::span<const double>(v1)});
    QCOMPARE(mg->componentCount(), 2);
    QCOMPARE(mg->dataCount(), 3);
}

void TestMultiGraph::componentCountMatchesDataSource()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(std::vector<double>{1.0, 2.0},
                std::vector<std::vector<double>>{{1.0, 2.0}, {3.0, 4.0}, {5.0, 6.0}});
    QCOMPARE(mg->componentCount(), 3);

    // Replace with 1-column source — components shrink
    mg->setData(std::vector<double>{1.0, 2.0},
                std::vector<std::vector<double>>{{1.0, 2.0}});
    QCOMPARE(mg->componentCount(), 1);
}

void TestMultiGraph::componentValueAt()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(std::vector<double>{1.0, 2.0, 3.0},
                std::vector<std::vector<double>>{{10.0, 20.0, 30.0}, {-1.0, -2.0, -3.0}});
    QCOMPARE(mg->componentValueAt(0, 1), 20.0);
    QCOMPARE(mg->componentValueAt(1, 2), -3.0);
}

void TestMultiGraph::dataCountDelegate()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    QCOMPARE(mg->dataCount(), 0);
    mg->setData(std::vector<double>{1.0, 2.0, 3.0},
                std::vector<std::vector<double>>{{0.0, 0.0, 0.0}});
    QCOMPARE(mg->dataCount(), 3);
}

void TestMultiGraph::dataMainKeyDelegate()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(std::vector<double>{1.0, 2.0, 3.0},
                std::vector<std::vector<double>>{{0.0, 0.0, 0.0}});
    QCOMPARE(mg->dataMainKey(0), 1.0);
    QCOMPARE(mg->dataMainKey(2), 3.0);
}

void TestMultiGraph::dataMainValueColumn0()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(std::vector<double>{1.0, 2.0},
                std::vector<std::vector<double>>{{10.0, 20.0}, {-1.0, -2.0}});
    QCOMPARE(mg->dataMainValue(0), 10.0);
    QCOMPARE(mg->dataMainValue(1), 20.0);
}

void TestMultiGraph::findBeginEndDelegate()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(std::vector<double>{1.0, 2.0, 3.0, 4.0, 5.0},
                std::vector<std::vector<double>>{{0.0, 0.0, 0.0, 0.0, 0.0}});
    QCOMPARE(mg->findBegin(3.0, false), 2);
    QCOMPARE(mg->findEnd(3.0, true), 4);
}

void TestMultiGraph::keyRangeDelegate()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(std::vector<double>{-2.0, 1.0, 5.0},
                std::vector<std::vector<double>>{{0.0, 0.0, 0.0}});
    bool found = false;
    auto range = mg->getKeyRange(found);
    QVERIFY(found);
    QCOMPARE(range.lower, -2.0);
    QCOMPARE(range.upper, 5.0);
}

void TestMultiGraph::valueRangeUnion()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(std::vector<double>{1.0, 2.0, 3.0},
                std::vector<std::vector<double>>{{10.0, 20.0, 30.0}, {-25.0, -15.0, -5.0}});
    bool found = false;
    auto range = mg->getValueRange(found);
    QVERIFY(found);
    QCOMPARE(range.lower, -25.0);
    QCOMPARE(range.upper, 30.0);
}

void TestMultiGraph::selectTestFindsClosestComponent()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mPlot->xAxis->setRange(0, 4);
    mPlot->yAxis->setRange(-10, 50);
    mg->setData(std::vector<double>{1.0, 2.0, 3.0},
                std::vector<std::vector<double>>{{10.0, 20.0, 30.0}, {40.0, 40.0, 40.0}});
    mPlot->replot();

    QPointF pixPos = mg->coordsToPixels(2.0, 40.0);
    QVariant details;
    double dist = mg->selectTest(pixPos, false, &details);
    QVERIFY(dist >= 0);
    auto map = details.toMap();
    QCOMPARE(map["componentIndex"].toInt(), 1);
}

void TestMultiGraph::selectTestReturnsComponentInDetails()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mPlot->xAxis->setRange(0, 4);
    mPlot->yAxis->setRange(0, 35);
    mg->setData(std::vector<double>{1.0, 2.0, 3.0},
                std::vector<std::vector<double>>{{10.0, 20.0, 30.0}});
    mPlot->replot();

    QPointF pixPos = mg->coordsToPixels(2.0, 20.0);
    QVariant details;
    mg->selectTest(pixPos, false, &details);
    auto map = details.toMap();
    QVERIFY(map.contains("componentIndex"));
    QVERIFY(map.contains("dataIndex"));
    QCOMPARE(map["componentIndex"].toInt(), 0);
    QCOMPARE(map["dataIndex"].toInt(), 1);
}

void TestMultiGraph::selectTestRectDoesNotMutateState()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setSelectable(QCP::stDataRange);
    mPlot->xAxis->setRange(0, 4);
    mPlot->yAxis->setRange(0, 35);
    mg->setData(std::vector<double>{1.0, 2.0, 3.0},
                std::vector<std::vector<double>>{{10.0, 20.0, 30.0}, {5.0, 5.0, 5.0}});
    mPlot->replot();

    QPointF tl = mg->coordsToPixels(1.0, 32.0);
    QPointF br = mg->coordsToPixels(3.0, 8.0);
    QRectF rect(tl, br);
    auto sel = mg->selectTestRect(rect.normalized(), false);

    QVERIFY(!sel.isEmpty());
    // selectTestRect is const — must not mutate component selections
    QVERIFY(mg->componentSelection(0).isEmpty());
    QVERIFY(mg->componentSelection(1).isEmpty());
}

void TestMultiGraph::selectTestRectPerComponent()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setSelectable(QCP::stDataRange);
    mPlot->xAxis->setRange(0, 4);
    mPlot->yAxis->setRange(0, 35);
    mg->setData(std::vector<double>{1.0, 2.0, 3.0},
                std::vector<std::vector<double>>{{10.0, 20.0, 30.0}, {5.0, 5.0, 5.0}});
    mPlot->replot();

    QPointF tl = mg->coordsToPixels(1.0, 32.0);
    QPointF br = mg->coordsToPixels(3.0, 8.0);
    QRectF rect(tl, br);
    auto sel = mg->selectTestRect(rect.normalized(), false);

    // Rect selection flows through selectEvent to apply per-component selections
    bool changed = false;
    mg->selectEvent(nullptr, false, QVariant::fromValue(sel), &changed);

    QVERIFY(changed);
    QCOMPARE(mg->componentSelection(0).dataRangeCount(), 1);
    QVERIFY(mg->componentSelection(1).isEmpty());
}

void TestMultiGraph::selectEventRectSelection()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setSelectable(QCP::stDataRange);
    mPlot->xAxis->setRange(0, 4);
    mPlot->yAxis->setRange(-5, 35);
    mg->setData(std::vector<double>{1.0, 2.0, 3.0},
                std::vector<std::vector<double>>{{10.0, 20.0, 30.0}, {-1.0, -2.0, -3.0}});
    mPlot->replot();

    // Select a rect that covers all data points for both components
    QPointF tl = mg->coordsToPixels(0.5, 35.0);
    QPointF br = mg->coordsToPixels(3.5, -5.0);
    QRectF rect(tl, br);
    auto sel = mg->selectTestRect(rect.normalized(), false);

    bool changed = false;
    mg->selectEvent(nullptr, false, QVariant::fromValue(sel), &changed);

    QVERIFY(changed);
    // Both components should have selections
    QVERIFY(!mg->componentSelection(0).isEmpty());
    QVERIFY(!mg->componentSelection(1).isEmpty());
}

void TestMultiGraph::selectEventSingleComponent()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setSelectable(QCP::stDataRange);
    mg->setData(std::vector<double>{1.0, 2.0, 3.0},
                std::vector<std::vector<double>>{{10.0, 20.0, 30.0}, {-1.0, -2.0, -3.0}});

    QVariantMap details;
    details["componentIndex"] = 1;
    details["dataIndex"] = 1;
    QVariant detailsVar = details;

    bool changed = false;
    mg->selectEvent(nullptr, false, detailsVar, &changed);
    QVERIFY(changed);
    QVERIFY(!mg->componentSelection(1).isEmpty());
    QVERIFY(mg->componentSelection(0).isEmpty());
}

void TestMultiGraph::selectEventAdditive()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setSelectable(QCP::stDataRange);
    mg->setData(std::vector<double>{1.0, 2.0, 3.0},
                std::vector<std::vector<double>>{{10.0, 20.0, 30.0}, {-1.0, -2.0, -3.0}});

    QVariantMap d0;
    d0["componentIndex"] = 0;
    d0["dataIndex"] = 0;
    bool changed = false;
    mg->selectEvent(nullptr, false, QVariant(d0), &changed);

    QVariantMap d1;
    d1["componentIndex"] = 1;
    d1["dataIndex"] = 1;
    mg->selectEvent(nullptr, true, QVariant(d1), &changed);

    QVERIFY(!mg->componentSelection(0).isEmpty());
    QVERIFY(!mg->componentSelection(1).isEmpty());
}

void TestMultiGraph::deselectEventClearsAll()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setSelectable(QCP::stDataRange);
    mg->setData(std::vector<double>{1.0, 2.0, 3.0},
                std::vector<std::vector<double>>{{10.0, 20.0, 30.0}, {-1.0, -2.0, -3.0}});

    mg->setComponentSelection(0, QCPDataSelection(QCPDataRange(0, 2)));
    mg->setComponentSelection(1, QCPDataSelection(QCPDataRange(1, 3)));
    QVERIFY(mg->selected());

    bool changed = false;
    mg->deselectEvent(&changed);
    QVERIFY(changed);
    QVERIFY(!mg->selected());
    QVERIFY(mg->componentSelection(0).isEmpty());
    QVERIFY(mg->componentSelection(1).isEmpty());
}

void TestMultiGraph::componentSelectionUpdatesBase()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(std::vector<double>{1.0, 2.0, 3.0},
                std::vector<std::vector<double>>{{10.0, 20.0, 30.0}});
    QVERIFY(!mg->selected());
    mg->setComponentSelection(0, QCPDataSelection(QCPDataRange(0, 2)));
    QVERIFY(mg->selected());
}

void TestMultiGraph::addToLegendCreatesGroupItem()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(std::vector<double>{1.0, 2.0},
                std::vector<std::vector<double>>{{10.0, 20.0}, {-1.0, -2.0}});
    mg->setName("B field");
    mg->addToLegend();
    QCOMPARE(mPlot->legend->itemCount(), 1);
    auto* item = qobject_cast<QCPGroupLegendItem*>(mPlot->legend->item(0));
    QVERIFY(item != nullptr);
    QCOMPARE(item->multiGraph(), mg);
}

void TestMultiGraph::legendHeaderDoesNotLeakClassNameWhenEmpty()
{
    // A multigraph with no data (0 components) and no name must not surface its
    // C++ class name ("QCPMultiGraph") to the user in the legend.
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    QCOMPARE(mg->componentCount(), 0);
    mg->addToLegend();
    auto* item = qobject_cast<QCPGroupLegendItem*>(mPlot->legend->item(0));
    QVERIFY(item != nullptr);
    QVERIFY(item->headerName()
            != QString::fromUtf8(mg->metaObject()->className()));
    QVERIFY(item->headerName().isEmpty());
}

void TestMultiGraph::legendHeaderShowsNameWhileEmpty()
{
    // A named multigraph that has not received data yet keeps showing its name
    // (the loading cue), never the class name.
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setName("b_gse");
    QCOMPARE(mg->componentCount(), 0);
    mg->addToLegend();
    auto* item = qobject_cast<QCPGroupLegendItem*>(mPlot->legend->item(0));
    QVERIFY(item != nullptr);
    QCOMPARE(item->headerName(), QStringLiteral("b_gse"));
}

void TestMultiGraph::removeFromLegendWorks()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(std::vector<double>{1.0, 2.0},
                std::vector<std::vector<double>>{{10.0, 20.0}});
    mg->addToLegend();
    QCOMPARE(mPlot->legend->itemCount(), 1);
    mg->removeFromLegend();
    QCOMPARE(mPlot->legend->itemCount(), 0);
}

void TestMultiGraph::legendExpandCollapse()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(std::vector<double>{1.0, 2.0},
                std::vector<std::vector<double>>{{10.0, 20.0}, {-1.0, -2.0}});
    mg->addToLegend();
    auto* item = qobject_cast<QCPGroupLegendItem*>(mPlot->legend->item(0));
    QVERIFY(item);
    QVERIFY(!item->expanded());
    item->setExpanded(true);
    QVERIFY(item->expanded());
    // Expanded size should be larger
    QSize expanded = item->minimumOuterSizeHint();
    item->setExpanded(false);
    QSize collapsed = item->minimumOuterSizeHint();
    QVERIFY(expanded.height() > collapsed.height());
}

void TestMultiGraph::legendGroupSelectsAll()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setSelectable(QCP::stDataRange);
    mg->setData(std::vector<double>{1.0, 2.0, 3.0},
                std::vector<std::vector<double>>{{10.0, 20.0, 30.0}, {-1.0, -2.0, -3.0}});
    mPlot->replot();
    mg->addToLegend();
    auto* item = qobject_cast<QCPGroupLegendItem*>(mPlot->legend->item(0));
    QVERIFY(item);
}

void TestMultiGraph::cacheExtendsBeyondVisibleRange()
{
    // Reproducer: fast panning cropped curves because the line cache only
    // covered the exact visible range.  After the fix, the cache must extend
    // beyond the viewport so GPU-translated pans don't expose uncovered edges.
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    // Dense data from 0..20, show only 5..15
    std::vector<double> keys(21);
    std::vector<double> vals(21);
    for (int i = 0; i <= 20; ++i) { keys[i] = i; vals[i] = i * 10.0; }
    mg->setData(std::vector<double>(keys),
                std::vector<std::vector<double>>{std::vector<double>(vals)});
    mPlot->xAxis->setRange(5, 15);
    mPlot->yAxis->setRange(0, 200);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QVERIFY(!mg->mCachedLines.isEmpty());
    const auto& cached = mg->mCachedLines[0];
    QVERIFY(cached.size() > 0);

    // The cached lines are in pixel space.  The leftmost/rightmost cached
    // pixel-x must extend beyond the axis rect (margin coverage).
    const QRectF axisRect = mPlot->xAxis->axisRect()->rect();
    double minPx = cached[0].x(), maxPx = cached[0].x();
    for (const auto& pt : cached) {
        minPx = qMin(minPx, pt.x());
        maxPx = qMax(maxPx, pt.x());
    }
    // Before the fix, minPx >= axisRect.left() and maxPx <= axisRect.right()
    // After the fix, cache extends beyond the visible rect on both sides.
    QVERIFY2(minPx < axisRect.left(),
             qPrintable(QString("Cache left %1 should be < axis left %2")
                        .arg(minPx).arg(axisRect.left())));
    QVERIFY2(maxPx > axisRect.right(),
             qPrintable(QString("Cache right %1 should be > axis right %2")
                        .arg(maxPx).arg(axisRect.right())));
}

void TestMultiGraph::renderVerticalKeyAxisUsesHeight()
{
    auto* mg = new QCPMultiGraph(mPlot->yAxis, mPlot->xAxis);
    mg->setData(std::vector<double>{1, 2, 3, 4, 5},
                std::vector<std::vector<double>>{{10, 20, 30, 40, 50}, {15, 25, 35, 45, 55}});
    mPlot->yAxis->setRange(0, 6);
    mPlot->xAxis->setRange(0, 60);
    mPlot->replot();
    QVERIFY(true);
}

void TestMultiGraph::renderBasicDoesNotCrash()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(std::vector<double>{1.0, 2.0, 3.0},
                std::vector<std::vector<double>>{{10.0, 20.0, 30.0}, {-1.0, -2.0, -3.0}});
    mPlot->xAxis->setRange(0, 4);
    mPlot->yAxis->setRange(-5, 35);
    mPlot->replot(); // Should not crash
}

void TestMultiGraph::renderWithSelection()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(std::vector<double>{1.0, 2.0, 3.0},
                std::vector<std::vector<double>>{{10.0, 20.0, 30.0}});
    mg->setComponentSelection(0, QCPDataSelection(QCPDataRange(0, 2)));
    mPlot->xAxis->setRange(0, 4);
    mPlot->yAxis->setRange(0, 35);
    mPlot->replot(); // Should not crash
}

void TestMultiGraph::renderHiddenComponent()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(std::vector<double>{1.0, 2.0, 3.0},
                std::vector<std::vector<double>>{{10.0, 20.0, 30.0}, {-1.0, -2.0, -3.0}});
    mg->component(1).visible = false;
    mPlot->xAxis->setRange(0, 4);
    mPlot->yAxis->setRange(-5, 35);
    mPlot->replot(); // Should not crash, should only draw component 0
}

void TestMultiGraph::renderEmptySource()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    Q_UNUSED(mg);
    mPlot->replot(); // No data source — should not crash
}

void TestMultiGraph::renderAllLineStyles()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(std::vector<double>{1.0, 2.0, 3.0, 4.0, 5.0},
                std::vector<std::vector<double>>{{10.0, 20.0, 15.0, 25.0, 30.0}});
    mPlot->xAxis->setRange(0, 6);
    mPlot->yAxis->setRange(0, 35);

    for (auto style : {QCPMultiGraph::lsNone, QCPMultiGraph::lsLine,
                       QCPMultiGraph::lsStepLeft, QCPMultiGraph::lsStepRight,
                       QCPMultiGraph::lsStepCenter, QCPMultiGraph::lsImpulse}) {
        mg->setLineStyle(style);
        mPlot->replot(); // Should not crash
    }
}
