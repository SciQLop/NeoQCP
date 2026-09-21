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

// Width the collapsed row needs left of its text, mirroring the layout in
// QCPGroupLegendItem::draw(): the component-icon column and the gap after it.
static int collapsedChromeWidth(const QCPAbstractLegendItem* item)
{
    constexpr int iconWidth = 20;
    constexpr int iconTextGap = 6;
    return item->margins().left() + item->margins().right() + iconWidth + iconTextGap;
}

void TestMultiGraph::legendCollapsedFitsExpanderMarker()
{
    // The collapsed row is drawn as "<marker> <name>", so the size hint must
    // reserve room for the marker too. It didn't, and QCPGroupLegendItem::draw
    // clips (it does not elide): a group named "H_hi" rendered as "▸ H_".
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(std::vector<double> { 1.0, 2.0 },
                std::vector<std::vector<double>> { { 10.0, 20.0 } });
    mg->setName("H_hi");
    mg->addToLegend();
    auto* item = qobject_cast<QCPGroupLegendItem*>(mPlot->legend->item(0));
    QVERIFY(item);
    QVERIFY(!item->expanded());

    const QFontMetrics fm(mPlot->legend->font());
    const int needed = collapsedChromeWidth(item)
        + fm.horizontalAdvance(QString::fromUtf8("▸ H_hi"));
    QVERIFY2(item->minimumOuterSizeHint().width() >= needed,
             qPrintable(QStringLiteral("collapsed hint %1 < needed %2")
                            .arg(item->minimumOuterSizeHint().width())
                            .arg(needed)));
}

void TestMultiGraph::legendCollapsedFitsBusyIndicator()
{
    // Same row also carries the busy symbol while fetching, which is the only
    // activity cue a graph has before its first data arrives — it must not be
    // pushed out of the drawn text rect by the marker.
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setName("b_gse");
    mg->setBusyShowDelayMs(10);
    mg->addToLegend();
    mg->setBusy(true);
    QTest::qWait(50);
    QVERIFY(mg->visuallyBusy());

    auto* item = qobject_cast<QCPGroupLegendItem*>(mPlot->legend->item(0));
    QVERIFY(item);

    const QFontMetrics fm(mPlot->legend->font());
    const QString drawn = QString::fromUtf8("▸ ") + mg->effectiveBusyIndicatorSymbol()
        + QStringLiteral(" b_gse");
    const int needed = collapsedChromeWidth(item) + fm.horizontalAdvance(drawn);
    QVERIFY2(item->minimumOuterSizeHint().width() >= needed,
             qPrintable(QStringLiteral("collapsed busy hint %1 < needed %2")
                            .arg(item->minimumOuterSizeHint().width())
                            .arg(needed)));
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

// Reproducer for SciQLop's "zoom moves everything except the graph" bug.
// SciQLopPlot's wheel/pinch handlers (and set_range / axis synchronizers) only
// change the axis range and queue a replot. replot() repaints dirty buffers
// only, and ensureAtLeastOneBufferDirty() dirties the range-dependent layers
// only when NO buffer is dirty yet. So an unrelated dirty buffer (an item
// toggled on the overlay layer, a busy legend entry, a catalog span) makes the
// replot skip the graph layer entirely: axes, grid and spans follow the new
// range, the graph stays at the old one until something else dirties it.
// The drag path never showed it because QCPAxisRect::mouseMoveEvent calls
// markAffectedLayersDirty() explicitly.
static QCPMultiGraph* renderedMultiGraph(QCustomPlot* plot)
{
    auto* mg = new QCPMultiGraph(plot->xAxis, plot->yAxis);
    std::vector<double> keys(21), vals(21);
    for (int i = 0; i <= 20; ++i) { keys[i] = i; vals[i] = i * 10.0; }
    mg->setData(std::move(keys), std::vector<std::vector<double>>{std::move(vals)});
    plot->xAxis->setRange(0, 20);
    plot->yAxis->setRange(0, 200);
    plot->replot(QCustomPlot::rpImmediateRefresh);
    return mg;
}

static void dirtyOverlayBuffer(QCustomPlot* plot)
{
    auto* text = new QCPItemText(plot);
    text->setLayer(QLatin1String("overlay"));
    plot->replot(QCustomPlot::rpImmediateRefresh); // settle the add
    text->setVisible(false);                        // dirties only the overlay buffer
}

void TestMultiGraph::renderZoomRepaintsWhenAnotherBufferIsDirty()
{
    auto* mg = renderedMultiGraph(mPlot);
    QVERIFY(mg->mHasRenderedRange);
    dirtyOverlayBuffer(mPlot);

    mPlot->xAxis->scaleRange(0.5, 10.0); // what SciQLopPlot::wheelEvent/_pinch_zoom do
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QCOMPARE(mg->mRenderedRange.key.lower, mPlot->xAxis->range().lower);
    QCOMPARE(mg->mRenderedRange.key.upper, mPlot->xAxis->range().upper);
}

void TestMultiGraph::renderSetRangeRepaintsWhenAnotherBufferIsDirty()
{
    auto* mg = renderedMultiGraph(mPlot);
    QVERIFY(mg->mHasRenderedRange);
    dirtyOverlayBuffer(mPlot);

    mPlot->xAxis->setRange(5, 15); // SciQLopPlotAxis::set_range / axis synchronizers
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QCOMPARE(mg->mRenderedRange.key.lower, 5.0);
    QCOMPARE(mg->mRenderedRange.key.upper, 15.0);
}

void TestMultiGraph::componentVisibilityToggleForcesLayerRepaint()
{
    // A component is shown or hidden through setComponentVisible(): the
    // layer must repaint (not translate its old GPU entries after a pan), or a
    // shown component stays missing until something else forces a repaint.
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    std::vector<double> keys(1000);
    std::vector<std::vector<double>> columns(3, std::vector<double>(1000));
    for (int i = 0; i < 1000; ++i)
    {
        keys[i] = i;
        for (int c = 0; c < 3; ++c)
            columns[c][i] = std::sin(i * 0.01 + c);
    }
    mg->setData(std::move(keys), std::move(columns));
    mPlot->xAxis->setRange(0, 1000);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTRY_VERIFY_WITH_TIMEOUT(!mg->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QCPLayer* mainLayer = mPlot->layer("main");
    QVERIFY(mainLayer);

    mPlot->xAxis->setRange(50, 1050);
    QVERIFY(mainLayer->canSkipRepaintForTranslation());
    mg->setComponentVisible(1, false);
    QVERIFY2(!mainLayer->canSkipRepaintForTranslation(), "hiding a component must repaint");

    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    mPlot->xAxis->setRange(100, 1100);
    mg->setComponentVisible(1, true);
    QVERIFY2(!mainLayer->canSkipRepaintForTranslation(), "showing a component must repaint");
}
