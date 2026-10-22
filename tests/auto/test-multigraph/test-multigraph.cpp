#include "test-multigraph.h"
#include "qcustomplot.h"
#include "datasource/soa-multi-datasource.h"
#include "datasource/row-major-multi-datasource.h"
#include "layoutelements/layoutelement-legend-group.h"
#include "plottables/plottable-draw-utils.h"
#include <cmath>
#include <limits>
#include <vector>
#include <span>
#include <QtWidgets/qtestsupport_widgets.h> // QTest::qWaitForWindowExposed

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
                std::vector<std::vector<double>> { { 10.0, 20.0 }, { 1.0, 2.0 } });
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
    QTRY_VERIFY_WITH_TIMEOUT(mg->visuallyBusy(), 2000);

    auto* item = qobject_cast<QCPGroupLegendItem*>(mPlot->legend->item(0));
    QVERIFY(item);

    const QFontMetrics fm(mPlot->legend->font());
    // No expander yet: before its first data the graph has no component to expand to.
    const QString drawn = mg->effectiveBusyIndicatorSymbol() + QStringLiteral(" b_gse");
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

// A one-column line is a one-component multigraph: an expander would only open a second row
// with the same name.
void TestMultiGraph::legendOneComponentHasNoExpander()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(std::vector<double>{1.0, 2.0}, std::vector<std::vector<double>>{{10.0, 20.0}});
    mg->setName("B");
    mg->addToLegend();
    auto* item = qobject_cast<QCPGroupLegendItem*>(mPlot->legend->item(0));
    QVERIFY(item);
    QCOMPARE(item->headerRowText(false), QStringLiteral("B"));
    const QSize collapsed = item->minimumOuterSizeHint();
    item->setExpanded(true); // what selecting the component does
    QCOMPARE(item->minimumOuterSizeHint(), collapsed);
}

// The legend icon of a scatter component (no line) used to be a line.
void TestMultiGraph::legendIconDrawsScatterMarkers()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(std::vector<double>{1.0, 2.0}, std::vector<std::vector<double>>{{10.0, 20.0}});
    mg->setComponentLineStyle(0, QCPMultiGraph::lsNone);
    mg->component(0).scatterStyle = QCPScatterStyle(QCPScatterStyle::ssDisc, Qt::black, Qt::black, 6);
    QImage icon(40, 20, QImage::Format_ARGB32);
    icon.fill(Qt::white);
    {
        QCPPainter painter(&icon);
        mg->drawComponentLegendIcon(&painter, 0, QLineF(0, 10, 40, 10));
    }
    QCOMPARE(icon.pixelColor(20, 10), QColor(Qt::black)); // the marker
    QCOMPARE(icon.pixelColor(2, 10), QColor(Qt::white));  // no line
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

void TestMultiGraph::zoomedOutLinesAreThinnedToThePixelWidth_data()
{
    QTest::addColumn<int>("columns");
    QTest::newRow("one column") << 1;
    QTest::newRow("two columns") << 2;   // all visible: the batched all-columns path
}

void TestMultiGraph::zoomedOutLinesAreThinnedToThePixelWidth()
{
    // Zoomed out, L2 holds ~4 min/max bins per pixel. Adaptive sampling must thin them to
    // ~2 points per pixel before extrusion, as QCPGraph2 does; drawing every L2 point made
    // a full replot about twice as slow.
    QFETCH(int, columns);
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    const int n = 500'000;
    std::vector<double> keys(n);
    std::vector<std::vector<double>> values(columns, std::vector<double>(n));
    for (int i = 0; i < n; ++i)
    {
        keys[i] = i;
        for (int c = 0; c < columns; ++c)
            values[c][i] = std::sin(i * 0.37 + c) + std::sin(i * 1e-4);
    }
    mg->setData(std::move(keys), std::move(values));
    mPlot->xAxis->setRange(0, n);
    mPlot->yAxis->setRange(-2.5, 2.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTRY_VERIFY_WITH_TIMEOUT(!mg->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY2(mg->mL2Result, "the test needs the L2 path");

    // ~2 points per pixel plus a few interval endpoints; unthinned L2 is ~8 per pixel.
    const int budget = 3 * mPlot->xAxis->axisRect()->width();
    for (int c = 0; c < columns; ++c)
        QVERIFY2(mg->mCachedLines[c].size() <= budget,
                 qPrintable(QString("column %1 drew %2 points for a budget of %3")
                                .arg(c).arg(mg->mCachedLines[c].size()).arg(budget)));
}

void TestMultiGraph::componentShownAfterARebuildIsDrawnAgain()
{
    // A rebuild of the line cache while a component is hidden drops its lines. Showing it
    // again must rebuild them: the cache is otherwise "clean" and the component stays empty.
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
    QVERIFY(!mg->mCachedLines[1].isEmpty());

    mg->setComponentVisible(1, false);
    mPlot->xAxis->setRange(0, 200);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(mg->mCachedLines[1].isEmpty());

    mg->setComponentVisible(1, true);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY2(!mg->mCachedLines[1].isEmpty(), "the shown component was not rebuilt");
}

namespace {
QCPMultiGraph* redAndBlueHorizontalLines(QCustomPlot* plot)
{
    auto* mg = new QCPMultiGraph(plot->xAxis, plot->yAxis);
    mg->setData(std::vector<double>{0, 1, 2, 3, 4},
                std::vector<std::vector<double>>{{1, 1, 1, 1, 1}, {2, 2, 2, 2, 2}});
    mg->setComponentPens({QPen(Qt::red, 5), QPen(Qt::blue, 5)});
    plot->xAxis->setRange(0, 4);
    plot->yAxis->setRange(0, 3);
    return mg;
}

QColor pixelOnLine(const QImage& image, QCustomPlot* plot, double value)
{
    const double dpr = image.devicePixelRatio();
    return image.pixelColor(qRound(plot->xAxis->coordToPixel(2) * dpr),
                            qRound(plot->yAxis->coordToPixel(value) * dpr));
}

bool isRed(const QColor& c) { return c.red() > 200 && c.green() < 60 && c.blue() < 60; }
bool isBlue(const QColor& c) { return c.blue() > 200 && c.red() < 60 && c.green() < 60; }
} // namespace

void TestMultiGraph::componentLineStyleOnlyChangesThatComponent()
{
    auto* mg = redAndBlueHorizontalLines(mPlot);
    mPlot->replot();
    mg->setComponentLineStyle(0, QCPMultiGraph::lsNone);

    QCOMPARE(mg->component(0).lineStyle, QCPMultiGraph::lsNone);
    QCOMPARE(mg->component(1).lineStyle, QCPMultiGraph::lsLine);
    const QImage image = mPlot->toPixmap(400, 300).toImage();
    const QColor hidden = pixelOnLine(image, mPlot, 1);
    const QColor shown = pixelOnLine(image, mPlot, 2);
    QVERIFY2(!isRed(hidden), qPrintable(QString("component 0 still drawn: %1").arg(hidden.name())));
    QVERIFY2(isBlue(shown), qPrintable(QString("component 1 not drawn: %1").arg(shown.name())));
}

void TestMultiGraph::componentLineStyleChangeIsDrawnOnTheGpu()
{
    mPlot->show();
    if (!QTest::qWaitForWindowExposed(mPlot))
        QSKIP("window not exposed in this environment");
    QCoreApplication::processEvents();
    if (!mPlot->rhi())
        QSKIP("no QRhi available in this environment");
    auto* mg = redAndBlueHorizontalLines(mPlot);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(isRed(pixelOnLine(mPlot->grabFramebuffer(), mPlot, 1)));

    mg->setComponentLineStyle(0, QCPMultiGraph::lsNone);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    const QImage frame = mPlot->grabFramebuffer();
    const QColor hidden = pixelOnLine(frame, mPlot, 1);
    const QColor shown = pixelOnLine(frame, mPlot, 2);
    QVERIFY2(!isRed(hidden), qPrintable(QString("component 0 still drawn: %1").arg(hidden.name())));
    QVERIFY2(isBlue(shown), qPrintable(QString("component 1 not drawn: %1").arg(shown.name())));
}

void TestMultiGraph::setLineStyleAppliesToEveryComponent()
{
    auto* mg = redAndBlueHorizontalLines(mPlot);
    mg->setComponentLineStyle(0, QCPMultiGraph::lsNone);
    mg->setLineStyle(QCPMultiGraph::lsStepLeft);
    QCOMPARE(mg->lineStyle(), QCPMultiGraph::lsStepLeft);
    QCOMPARE(mg->component(0).lineStyle, QCPMultiGraph::lsStepLeft);
    QCOMPARE(mg->component(1).lineStyle, QCPMultiGraph::lsStepLeft);
}

void TestMultiGraph::newComponentsTakeTheGraphLineStyle()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setLineStyle(QCPMultiGraph::lsNone);
    mg->setData(std::vector<double>{0, 1}, std::vector<std::vector<double>>{{1, 1}, {2, 2}});
    QCOMPARE(mg->component(0).lineStyle, QCPMultiGraph::lsNone);
    QCOMPARE(mg->component(1).lineStyle, QCPMultiGraph::lsNone);
}

namespace {
bool showWithRhi(QCustomPlot* plot)
{
    plot->show();
    if (!QTest::qWaitForWindowExposed(plot))
        return false;
    QCoreApplication::processEvents();
    return plot->rhi() != nullptr;
}

// A step-left line from (0,0) to (4,2) stays flat at y=0 through x=2;
// a straight line would cross (2,1). Selected and plain pens are both red here.
bool crossesTheDiagonal(QCustomPlot* plot)
{
    const QImage frame = plot->grabFramebuffer();
    const double dpr = frame.devicePixelRatio();
    const QColor c = frame.pixelColor(qRound(plot->xAxis->coordToPixel(2) * dpr),
                                      qRound(plot->yAxis->coordToPixel(1) * dpr));
    return c.red() > 150 && c.green() < 100 && c.blue() < 100;
}
} // namespace

void TestMultiGraph::plainReextrusionOnPenChangeNotPan()
{
    const QPen pen(Qt::red, 2);
    qcp::ExtrusionCache cache;
    cache.vertices = {1, 2, 3, 4, 5, 6};
    cache.penWidth = qcp::extrusionPenWidth(pen, 1.0);
    cache.penColor = pen.color().rgba();
    QVERIFY(!qcp::needsReextrusion(cache, false, pen, 1.0));                  // pan frame
    QVERIFY(qcp::needsReextrusion(cache, true, pen, 1.0));                    // fresh lines
    QVERIFY(qcp::needsReextrusion(cache, false, QPen(Qt::red, 3.5), 1.0));    // selected: wider
    QVERIFY(qcp::needsReextrusion(cache, false, QPen(Qt::blue, 2), 1.0));     // selected: colour
    cache.clear();
    QVERIFY(qcp::needsReextrusion(cache, false, pen, 1.0));                   // empty
}

void TestMultiGraph::selectedStepLineKeepsItsSteps()
{
    if (!showWithRhi(mPlot))
        QSKIP("no QRhi available in this environment");
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(std::vector<double>{0, 4}, std::vector<std::vector<double>>{{0, 2}});
    mg->setComponentPens({QPen(Qt::red, 5)});
    mg->setLineStyle(QCPMultiGraph::lsStepLeft);
    mPlot->xAxis->setRange(-1, 5);
    mPlot->yAxis->setRange(-1, 3);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(!crossesTheDiagonal(mPlot));

    mg->setComponentSelection(0, QCPDataSelection(QCPDataRange(0, 2)));
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY2(!crossesTheDiagonal(mPlot), "selected step line drawn as a straight line");
}

static bool onScreenIsRed(QCustomPlot* plot, double key, double value)
{
    const QImage frame = plot->grabFramebuffer();
    const double dpr = frame.devicePixelRatio();
    const QColor c = frame.pixelColor(qRound(plot->xAxis->coordToPixel(key) * dpr),
                                      qRound(plot->yAxis->coordToPixel(value) * dpr));
    return c.red() > 150 && c.green() < 100 && c.blue() < 100;
}

void TestMultiGraph::committedDataIsDrawnOnScreen()
{
    if (!showWithRhi(mPlot))
        QSKIP("no QRhi available in this environment");
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(std::vector<double>{0, 4}, std::vector<std::vector<double>>{{1, 1}});
    mg->setComponentPens({QPen(Qt::red, 5)});
    mPlot->xAxis->setRange(-1, 5);
    mPlot->yAxis->setRange(0, 4);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(onScreenIsRed(mPlot, 2, 1));
    mg->setData(std::vector<double>{0, 4}, std::vector<std::vector<double>>{{3, 3}});
    QVERIFY(mg->hasPendingData());
    QTRY_VERIFY_WITH_TIMEOUT(!mg->hasPendingData(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY2(onScreenIsRed(mPlot, 2, 3), "the committed data is not on screen");
    QVERIFY2(!onScreenIsRed(mPlot, 2, 1), "the replaced data is still on screen");
}

void TestMultiGraph::selectedStepGraph2KeepsItsSteps()
{
    if (!showWithRhi(mPlot))
        QSKIP("no QRhi available in this environment");
    auto* g = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    g->setData(std::vector<double>{0, 4}, std::vector<double>{0, 2});
    g->setPen(QPen(Qt::red, 5));
    g->selectionDecorator()->setPen(QPen(QColor(220, 0, 0), 7));
    g->setLineStyle(QCPGraph2::lsStepLeft);
    mPlot->xAxis->setRange(-1, 5);
    mPlot->yAxis->setRange(-1, 3);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(!crossesTheDiagonal(mPlot));

    g->setSelection(QCPDataSelection(QCPDataRange(0, 2)));
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY2(!crossesTheDiagonal(mPlot), "selected step line drawn as a straight line");
}

namespace {
//! Dense enough that the view is drawn from a resampled (L2) source covering only the view.
QCPMultiGraph* denseGraphDrawn(QCustomPlot* plot)
{
    auto* mg = new QCPMultiGraph(plot->xAxis, plot->yAxis);
    const int n = 400'000;
    std::vector<double> keys(n), values(n);
    for (int i = 0; i < n; ++i)
    {
        keys[i] = i;
        values[i] = std::sin(i * 0.001);
    }
    mg->setData(std::move(keys), std::vector<std::vector<double>> { std::move(values) });
    plot->xAxis->setRange(0, 100'000);
    plot->yAxis->setRange(-1.5, 1.5);
    plot->replot(QCustomPlot::rpImmediateRefresh);
    if (!QTest::qWaitFor([mg] { return !mg->pipeline().isBusy(); }, 5000))
        qWarning("denseGraphDrawn: the pipeline is still busy after 5 s");
    plot->replot(QCustomPlot::rpImmediateRefresh);
    return mg;
}

} // namespace

void TestMultiGraph::panIsRedrawnFreshAfterTheDebounce()
{
    if (!showWithRhi(mPlot))
        QSKIP("no QRhi available in this environment");
    auto* mg = denseGraphDrawn(mPlot);
    QVERIFY(mg->mL2Result); // the cached lines cover only the view
    mPlot->xAxis->setRange(25'000, 125'000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh); // pan frame: translated, edge uncovered
    const auto renderedAtView = [&] {
        return qFuzzyCompare(mg->mRenderedRange.key.lower, mPlot->xAxis->range().lower)
            && qFuzzyCompare(mg->mRenderedRange.key.upper, mPlot->xAxis->range().upper);
    };
    QTRY_VERIFY2_WITH_TIMEOUT(renderedAtView(), "the panned view was never drawn fresh", 2000);
}

void TestMultiGraph::panIsRedrawnFreshWhileAnotherLayerIsDirty()
{
    if (!showWithRhi(mPlot))
        QSKIP("no QRhi available in this environment");
    auto* mg = denseGraphDrawn(mPlot);
    // Like a crosshair following the mouse: some other layer is dirty on every replot.
    connect(mPlot, &QCustomPlot::beforeReplot, mPlot, [this] { mPlot->layer("overlay")->markDirty(); });
    mPlot->xAxis->setRange(25'000, 125'000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    const auto renderedAtView = [&] {
        return qFuzzyCompare(mg->mRenderedRange.key.lower, mPlot->xAxis->range().lower)
            && qFuzzyCompare(mg->mRenderedRange.key.upper, mPlot->xAxis->range().upper);
    };
    QTRY_VERIFY2_WITH_TIMEOUT(renderedAtView(), "the panned view was never drawn fresh", 2000);
}

static QCPMultiGraph* threeColumnGraph(QCustomPlot* plot)
{
    auto* mg = new QCPMultiGraph(plot->xAxis, plot->yAxis);
    mg->setData(std::vector<double>{1.0, 2.0, 3.0},
                std::vector<std::vector<double>>{{1, 2, 3}, {0, 1, 2}, {2, 3, 4}});
    mg->setComponentNames({"y", "low", "high"});
    return mg;
}

static QCPGroupLegendItem* groupItemOf(QCustomPlot* plot, QCPMultiGraph* mg)
{
    for (int i = 0; i < plot->legend->itemCount(); ++i)
        if (auto* gi = qobject_cast<QCPGroupLegendItem*>(plot->legend->item(i)))
            if (gi->multiGraph() == mg)
                return gi;
    return nullptr;
}

void TestMultiGraph::legendSkipsComponentsOutOfTheLegend()
{
    auto* mg = threeColumnGraph(mPlot);
    mg->setComponentInLegend(2, false);
    QCOMPARE(mg->legendComponents(), (QVector<int>{0, 1}));
    mg->addToLegend(mPlot->legend);
    auto* gi = groupItemOf(mPlot, mg);
    QVERIFY(gi);
    gi->setExpanded(true);
    const int rh = gi->rowHeight();
    QCOMPARE(gi->minimumOuterSizeHint().height(),
             rh * 3 + gi->margins().top() + gi->margins().bottom());
}

void TestMultiGraph::legendClickMapsRowsToInLegendComponents()
{
    auto* mg = threeColumnGraph(mPlot);
    mg->setComponentInLegend(1, false);
    mg->addToLegend(mPlot->legend);
    auto* gi = groupItemOf(mPlot, mg);
    gi->setExpanded(true);
    mPlot->replot();
    QVariant details;
    const QRect r = gi->rect();
    const int rh = gi->rowHeight();
    QVERIFY(gi->selectTest(QPointF(r.left() + 5, r.top() + 2 * rh + rh / 2), false, &details) >= 0);
    QCOMPARE(details.toMap().value("componentIndex").toInt(), 2);
}

void TestMultiGraph::legendOneInLegendComponentHasNoExpander()
{
    auto* mg = threeColumnGraph(mPlot);
    mg->setComponentInLegend(1, false);
    mg->setComponentInLegend(2, false);
    mg->addToLegend(mPlot->legend);
    auto* gi = groupItemOf(mPlot, mg);
    gi->setExpanded(true);
    QVERIFY(!gi->shownExpanded());
    QCOMPARE(gi->headerRowText(false), QStringLiteral("y"));
}

void TestMultiGraph::legendHeaderClickSelectsTheOnlyInLegendComponent()
{
    auto* mg = threeColumnGraph(mPlot);
    mg->setComponentInLegend(0, false);
    mg->setComponentInLegend(2, false);
    mg->addToLegend(mPlot->legend);
    auto* gi = groupItemOf(mPlot, mg);
    QSignalSpy clicked(gi, &QCPGroupLegendItem::componentClicked);
    QVariantMap details;
    details[QStringLiteral("componentIndex")] = -1;
    gi->selectEvent(nullptr, false, details, nullptr);
    QCOMPARE(clicked.count(), 1);
    QCOMPARE(clicked.first().first().toInt(), 1);
    QCOMPARE(gi->selectedComponent(), 1);
}

void TestMultiGraph::legendSelectTestSurvivesNullMultiGraph()
{
    QCPGroupLegendItem item(mPlot->legend, nullptr);
    item.setOuterRect(QRect(0, 0, 100, 40));
    QVariant details;
    QVERIFY(item.selectTest(QPointF(5, 5), false, &details) >= 0);
    QCOMPARE(details.toMap().value("componentIndex").toInt(), -1);
}

void TestMultiGraph::fillRejectsBadColumns()
{
    auto* mg = threeColumnGraph(mPlot);
    QVERIFY(!mg->setComponentFillBetween(1, 1));
    QVERIFY(!mg->setComponentFillBetween(0, 3));
    QVERIFY(!mg->setComponentFillBetween(-1, 0));
    QVERIFY(mg->setComponentFillBetween(1, 2));
    QCOMPARE(std::get<int>(mg->componentFill(1)->target), 2);
}

void TestMultiGraph::fillBeforeDataIsKept()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    QVERIFY(mg->setComponentFillBetween(1, 2));
    mg->setData(std::vector<double>{1, 2, 3},
                std::vector<std::vector<double>>{{1, 2, 3}, {0, 1, 2}, {2, 3, 4}});
    QVERIFY(mg->componentFill(1).has_value());
}

void TestMultiGraph::fillIsDroppedWhenItsColumnDisappears()
{
    auto* mg = threeColumnGraph(mPlot);
    QVERIFY(mg->setComponentFillBetween(1, 2));
    mg->setData(std::vector<double>{1, 2}, std::vector<std::vector<double>>{{1, 2}, {0, 1}});
    QVERIFY(!mg->componentFill(1).has_value());
    mg->setData(std::vector<double>{1, 2},
                std::vector<std::vector<double>>{{1, 2}, {0, 1}, {3, 4}});
    QVERIFY(!mg->componentFill(1).has_value()); // shrinking is lossy
}

void TestMultiGraph::fillDefaultBrushIsTheOwnerColourAtQuarterAlpha()
{
    auto* mg = threeColumnGraph(mPlot);
    mg->setComponentColors({Qt::red, Qt::green, Qt::blue});
    mg->setComponentFill(0, 0.0);
    const QBrush b = mg->effectiveFillBrush(*mg->componentFill(0));
    QCOMPARE(b.color().rgb(), QColor(Qt::red).rgb());
    QCOMPARE(b.color().alpha(), 64);
}

void TestMultiGraph::fillReplacesAndClears()
{
    auto* mg = threeColumnGraph(mPlot);
    mg->setComponentFill(0, 1.0);
    mg->setComponentFillBetween(0, 2);
    QCOMPARE(std::get<int>(mg->componentFill(0)->target), 2);
    mg->clearComponentFill(0);
    QVERIFY(!mg->componentFill(0).has_value());
}

void TestMultiGraph::fillValidatesAgainstTheStagedSource()
{
    using Source = QCPSoAMultiDataSource<std::vector<double>, std::vector<double>>;
    const auto stage = [](QCPMultiGraph* g, int columns) {
        g->stagePendingSource(std::make_shared<Source>(
            std::vector<double>{1, 2}, std::vector<std::vector<double>>(columns, {1, 2})));
    };
    auto* mg = threeColumnGraph(mPlot);
    stage(mg, 2);
    QVERIFY(!mg->setComponentFillBetween(0, 2));
    stage(mg, 4);
    QVERIFY(mg->setComponentFillBetween(0, 3));
}

static QImage renderOf(QCustomPlot* plot)
{
    return plot->toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
}

static bool isRedish(QColor c) { return c.red() > 150 && c.green() < 120 && c.blue() < 120; }

// Export renders (toPixmap) never stage a source; a live replot does, and the plot swaps the
// staged source in on its debounce timer, never on replot().
static void commitEmptyWindow(QCustomPlot* plot, QCPMultiGraph* mg)
{
    plot->replot(QCustomPlot::rpImmediateRefresh);
    const std::vector<double> none;
    mg->setDataSource(std::make_shared<QCPRowMajorMultiDataSource<double, double>>(
        std::span<const double>(none), none.data(), 0, 3, 3));
    QVERIFY(mg->hasPendingData());
    QTRY_VERIFY_WITH_TIMEOUT(!mg->hasPendingData(), 5000);
    QCOMPARE(mg->dataSource()->size(), 0);
}

void TestMultiGraph::fillToZeroColoursBelowTheCurveOnly()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    std::vector<double> x(200), y(200);
    for (int i = 0; i < 200; ++i) { x[i] = i; y[i] = 5.0; }
    mg->setData(std::move(x), std::vector<std::vector<double>>{std::move(y)});
    mg->setComponentColors({Qt::red});
    mg->setComponentFill(0, 0.0, QBrush(Qt::red));
    mPlot->xAxis->setRange(0, 199);
    mPlot->yAxis->setRange(-5, 10);
    const QImage img = renderOf(mPlot);
    const QRect r = mPlot->axisRect()->rect();
    const int xm = r.center().x();
    QVERIFY(isRedish(img.pixelColor(xm, int(mPlot->yAxis->coordToPixel(2.5)))));
    QVERIFY(!isRedish(img.pixelColor(xm, int(mPlot->yAxis->coordToPixel(7.5)))));
    QVERIFY(!isRedish(img.pixelColor(xm, int(mPlot->yAxis->coordToPixel(-2.5)))));
}

void TestMultiGraph::hiddenEdgeLinesKeepTheirFill()
{
    auto* mg = threeColumnGraph(mPlot);
    mg->setComponentLineStyle(1, QCPMultiGraph::lsNone);
    mg->setComponentLineStyle(2, QCPMultiGraph::lsNone);
    mg->setComponentFillBetween(1, 2, QBrush(Qt::red));
    mPlot->xAxis->setRange(1, 3);
    mPlot->yAxis->setRange(-1, 5);
    const QImage img = renderOf(mPlot);
    // y = 1.5, not 2: the probe must miss column 0's line, which goes through (2, 2).
    QVERIFY(isRedish(img.pixelColor(int(mPlot->xAxis->coordToPixel(2)),
                                    int(mPlot->yAxis->coordToPixel(1.5)))));
}

void TestMultiGraph::hidingAnEdgeRemovesTheFillAndShowingBringsItBack()
{
    auto* mg = threeColumnGraph(mPlot);
    mg->setComponentFillBetween(1, 2, QBrush(Qt::red));
    mPlot->xAxis->setRange(1, 3);
    mPlot->yAxis->setRange(-1, 5);
    renderOf(mPlot); // lays the axis rect out before the probe is placed
    const QPoint inside(int(mPlot->xAxis->coordToPixel(2)), int(mPlot->yAxis->coordToPixel(1.5)));
    mg->setComponentVisible(2, false);
    QVERIFY(!isRedish(renderOf(mPlot).pixelColor(inside)));
    mg->setComponentVisible(2, true);
    QVERIFY(isRedish(renderOf(mPlot).pixelColor(inside)));
}

void TestMultiGraph::fillBreaksAtAKeyGap()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(std::vector<double>{0, 1, 2, 3, 20, 21, 22, 23},
                std::vector<std::vector<double>>{{5, 5, 5, 5, 5, 5, 5, 5}});
    mg->setComponentFill(0, 0.0, QBrush(Qt::red));
    mPlot->xAxis->setRange(0, 23);
    mPlot->yAxis->setRange(-1, 6);
    const QImage img = renderOf(mPlot);
    QVERIFY(isRedish(img.pixelColor(int(mPlot->xAxis->coordToPixel(1)),
                                    int(mPlot->yAxis->coordToPixel(2)))));
    QVERIFY(!isRedish(img.pixelColor(int(mPlot->xAxis->coordToPixel(11)),
                                     int(mPlot->yAxis->coordToPixel(2)))));
}

void TestMultiGraph::fillOnALogAxisClampsAZeroBaseline()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(std::vector<double>{1, 2, 3}, std::vector<std::vector<double>>{{10, 10, 10}});
    mg->setComponentFill(0, 0.0, QBrush(Qt::red));
    mPlot->yAxis->setScaleType(QCPAxis::stLogarithmic);
    mPlot->xAxis->setRange(1, 3);
    mPlot->yAxis->setRange(0.1, 100);
    const QImage img = renderOf(mPlot);
    const QRect r = mPlot->axisRect()->rect();
    QVERIFY(isRedish(img.pixelColor(r.center().x(), r.bottom() - 8))); // above the inward ticks
    QVERIFY(!isRedish(img.pixelColor(r.center().x(), int(mPlot->yAxis->coordToPixel(50)))));
}

static QCPMultiGraph* bandDownToZeroOnALogAxis(QCustomPlot* plot)
{
    auto* mg = new QCPMultiGraph(plot->xAxis, plot->yAxis);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    mg->setData(std::vector<double>{1, 2, 3, 4},
                std::vector<std::vector<double>>{{0, 0, nan, 0}, {10, 10, 10, 10}});
    mg->setComponentFillBetween(1, 0, QBrush(Qt::red));
    plot->yAxis->setScaleType(QCPAxis::stLogarithmic);
    plot->xAxis->setRange(1, 4);
    plot->yAxis->setRange(0.1, 100);
    return mg;
}

void TestMultiGraph::bandEdgeAtZeroOnALogAxisReachesTheEdge()
{
    bandDownToZeroOnALogAxis(mPlot);
    const QImage img = renderOf(mPlot);
    const QRect r = mPlot->axisRect()->rect();
    const int x = int(mPlot->xAxis->coordToPixel(1.5));
    QVERIFY(isRedish(img.pixelColor(x, r.bottom() - 8))); // above the inward ticks
    QVERIFY(!isRedish(img.pixelColor(x, int(mPlot->yAxis->coordToPixel(50)))));
}

// Clamped like a constant baseline at 0, two axis lengths out: a GPU pan translates the cached
// strip, and an edge only 200 px out (coordToPixel's own guess) can come into view.
void TestMultiGraph::bandEdgeAtZeroOnALogAxisIsClampedLikeABaseline()
{
    auto* mg = bandDownToZeroOnALogAxis(mPlot);
    renderOf(mPlot); // lays the axis rect out
    const auto rows = mg->fillRows(*mg->componentFill(1), *mg->dataSource(), 0, 4);
    const double zeroBaseline = qcp::fill::baselinePixel(*mPlot->yAxis, 0.0);
    QCOMPARE(rows[0].b, zeroBaseline);
    QCOMPARE(rows[1].b, zeroBaseline);
    QVERIFY(std::isnan(rows[2].b)); // a NaN row still breaks the strip
    QCOMPARE(rows[3].b, zeroBaseline);
}

void TestMultiGraph::fillCacheIsRebuiltOnZoom()
{
    if (!showWithRhi(mPlot))
        QSKIP("no exposed window with a QRhi in this environment");
    auto* mg = threeColumnGraph(mPlot);
    mg->setComponentFillBetween(1, 2, QBrush(Qt::red));
    mPlot->xAxis->setRange(1, 3);
    mPlot->yAxis->setRange(-1, 5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(mPlot->plottableRhiLayer(mg->layer())); // the GPU branch ran, not only the export one
    const auto before = mg->mFills.first().verts;
    QVERIFY(!before.empty());

    mPlot->xAxis->setRange(1.5, 2.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    const auto zoomed = mg->mFills.first().verts;
    QVERIFY(zoomed != before);

    mPlot->xAxis->setRange(1.55, 2.55);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY2(mg->mFills.first().verts == zoomed, "a pan within the cache reuses the vertices");
    const QImage frame = mPlot->grabFramebuffer();
    QVERIFY(isRedish(frame.pixelColor(int(mPlot->xAxis->coordToPixel(2)),
                                      int(mPlot->yAxis->coordToPixel(1.5)))));
}

// The GPU pans the cached strip: an edge stopped at the axis bottom would come into view.
void TestMultiGraph::bandEdgeAtZeroOnALogAxisStaysPastTheEdgeUnderAGpuPan()
{
    if (!showWithRhi(mPlot))
        QSKIP("no exposed window with a QRhi in this environment");
    auto* mg = bandDownToZeroOnALogAxis(mPlot);
    mPlot->axisRect()->setAutoMargins(QCP::msNone); // new tick labels must not resize the rect
    mPlot->axisRect()->setMargins(QMargins(50, 20, 20, 30));
    mPlot->xAxis->setTicks(false);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(mPlot->plottableRhiLayer(mg->layer()));
    const auto before = mg->mFills.first().verts;
    QVERIFY(!before.empty());

    mPlot->yAxis->setRange(0.05, 50); // the content moves up a tenth of the axis
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY2(mg->mFills.first().verts == before, "the pan must reuse the cached strip");
    const QImage frame = mPlot->grabFramebuffer();
    const double dpr = frame.devicePixelRatio();
    const QRect r = mPlot->axisRect()->rect();
    const QColor c = frame.pixelColor(qRound(mPlot->xAxis->coordToPixel(1.5) * dpr),
                                      qRound((r.bottom() - 3) * dpr));
    QVERIFY2(isRedish(c), qPrintable(c.name()));
}

void TestMultiGraph::defaultFillFollowsTheOwnerColourOnTheGpu()
{
    if (!showWithRhi(mPlot))
        QSKIP("no exposed window with a QRhi in this environment");
    auto* mg = threeColumnGraph(mPlot);
    mg->setComponentColors({Qt::black, Qt::red, Qt::red});
    mg->setComponentFillBetween(1, 2);
    mPlot->xAxis->setRange(1, 3);
    mPlot->yAxis->setRange(-1, 5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    const QPoint inside(int(mPlot->xAxis->coordToPixel(2)), int(mPlot->yAxis->coordToPixel(1.5)));
    const QColor red = mPlot->grabFramebuffer().pixelColor(inside);
    QVERIFY2(red.red() > red.blue(), qPrintable(red.name()));

    mg->setComponentColors({Qt::black, Qt::blue, Qt::blue});
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    const QColor blue = mPlot->grabFramebuffer().pixelColor(inside);
    QVERIFY2(blue.blue() > blue.red(), qPrintable(blue.name()));

    mg->setComponentPens({QPen(Qt::black), QPen(Qt::red), QPen(Qt::red)});
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    const QColor redAgain = mPlot->grabFramebuffer().pixelColor(inside);
    QVERIFY2(redAgain.red() > redAgain.blue(), qPrintable(redAgain.name()));
}

void TestMultiGraph::autoScaleIncludesHiddenLineBandEdges()
{
    auto* mg = threeColumnGraph(mPlot);
    mg->setComponentLineStyle(2, QCPMultiGraph::lsNone);
    bool found = false;
    QCOMPARE(mg->getValueRange(found).upper, 4.0);
}

void TestMultiGraph::fillFollowsADownsampledLine()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    const int n = 500'000;
    const double period = n / 5.0;
    std::vector<double> keys(n), values(n);
    for (int i = 0; i < n; ++i)
    {
        keys[i] = i;
        values[i] = std::sin(2 * M_PI * i / period);
    }
    mg->setData(std::move(keys), std::vector<std::vector<double>>{std::move(values)});
    mg->setComponentFill(0, 0.0, QBrush(Qt::red));
    mPlot->xAxis->setRange(0, n);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTRY_VERIFY_WITH_TIMEOUT(!mg->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY2(mg->mL2Result, "the test needs the L2 path");
    const QImage img = renderOf(mPlot);
    const int crest = int(mPlot->xAxis->coordToPixel(period / 4));
    QVERIFY(isRedish(img.pixelColor(crest, int(mPlot->yAxis->coordToPixel(0.5)))));
    QVERIFY(!isRedish(img.pixelColor(crest, int(mPlot->yAxis->coordToPixel(1.3)))));
}

void TestMultiGraph::legendIconOfABandIsAFilledSwatch()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(std::vector<double>{1, 2}, std::vector<std::vector<double>>{{0, 0}, {1, 1}});
    mg->setComponentLineStyle(0, QCPMultiGraph::lsNone);
    mg->setComponentLineStyle(1, QCPMultiGraph::lsNone);
    mg->setComponentInLegend(1, false);
    mg->setComponentFillBetween(0, 1, QBrush(Qt::red));
    QImage img(40, 20, QImage::Format_ARGB32);
    img.fill(Qt::white);
    QCPPainter p(&img);
    mg->drawComponentLegendIcon(&p, 0, QLineF(0, 10, 40, 10));
    p.end();
    QVERIFY(isRedish(img.pixelColor(20, 6)));
    QVERIFY(isRedish(img.pixelColor(20, 14)));
}

void TestMultiGraph::legendIconDrawsTheLineOverTheSwatch()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(std::vector<double>{1, 2}, std::vector<std::vector<double>>{{1, 1}});
    mg->setComponentPens({QPen(Qt::blue, 3)});
    mg->setComponentFill(0, 0.0, QBrush(Qt::red));
    QImage img(40, 20, QImage::Format_ARGB32);
    img.fill(Qt::white);
    QCPPainter p(&img);
    mg->drawComponentLegendIcon(&p, 0, QLineF(0, 10, 40, 10));
    p.end();
    const QColor onTheLine = img.pixelColor(20, 10);
    QVERIFY2(onTheLine.blue() > 150 && onTheLine.red() < 100, qPrintable(onTheLine.name()));
    QVERIFY(isRedish(img.pixelColor(20, 6)));
}

void TestMultiGraph::collapsedLegendRowShowsTheBandSwatch()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(std::vector<double>{1, 2}, std::vector<std::vector<double>>{{0, 0}, {1, 1}});
    mg->setComponentLineStyle(0, QCPMultiGraph::lsNone);
    mg->setComponentLineStyle(1, QCPMultiGraph::lsNone);
    mg->setComponentInLegend(1, false);
    mg->setComponentFillBetween(0, 1, QBrush(Qt::red));
    mg->addToLegend(mPlot->legend);
    mPlot->legend->setVisible(true);
    mPlot->xAxis->setRange(10, 20); // the band itself stays out of view
    auto* gi = groupItemOf(mPlot, mg);
    QVERIFY(gi);
    QVERIFY(!gi->shownExpanded());
    const QImage img = renderOf(mPlot);
    const QRect r = gi->rect();
    const QColor swatch = img.pixelColor(r.left() + gi->margins().left() + 10,
                                         r.top() + gi->rowHeight() / 2 - 3);
    QVERIFY2(isRedish(swatch), qPrintable(swatch.name()));
}

static QCPMultiGraph* errorBarGraph(QCustomPlot* plot, int n, double spacing)
{
    std::vector<double> x(n), y(n), lo(n), hi(n);
    for (int i = 0; i < n; ++i) { x[i] = i * spacing; y[i] = 5; lo[i] = 3; hi[i] = 7; }
    auto* mg = new QCPMultiGraph(plot->xAxis, plot->yAxis);
    mg->setData(std::move(x), std::vector<std::vector<double>>{std::move(y), std::move(lo), std::move(hi)});
    mg->setComponentColors({Qt::red, Qt::red, Qt::red});
    mg->setComponentLineStyle(0, QCPMultiGraph::lsNone);
    mg->setComponentLineStyle(1, QCPMultiGraph::lsNone);
    mg->setComponentLineStyle(2, QCPMultiGraph::lsNone);
    mg->setComponentErrorBars(0, 1, 2);
    return mg;
}

void TestMultiGraph::sparseErrorBarsAreDrawnAsBars()
{
    errorBarGraph(mPlot, 5, 1.0);
    mPlot->xAxis->setRange(-0.5, 4.5);
    mPlot->yAxis->setRange(0, 10);
    const QImage img = renderOf(mPlot);
    const int xBar = int(mPlot->xAxis->coordToPixel(2));
    const int xBetween = int(mPlot->xAxis->coordToPixel(2.5));
    const int yInside = int(mPlot->yAxis->coordToPixel(4));
    QVERIFY(isRedish(img.pixelColor(xBar, yInside)));
    QVERIFY(!isRedish(img.pixelColor(xBetween, yInside)));
}

void TestMultiGraph::denseErrorBarsBecomeABand()
{
    errorBarGraph(mPlot, 2000, 0.01);
    mPlot->xAxis->setRange(0, 20);
    mPlot->yAxis->setRange(0, 10);
    const QImage img = renderOf(mPlot);
    const QColor between = img.pixelColor(int(mPlot->xAxis->coordToPixel(10.003)),
                                          int(mPlot->yAxis->coordToPixel(4)));
    QVERIFY(between.red() > between.green() + 30); // translucent red band, everywhere
}

void TestMultiGraph::errorBoundsBelowZeroOnALogAxisReachTheEdge()
{
    auto* mg = errorBarGraph(mPlot, 3, 1.0);
    std::vector<double> x{0, 1, 2}, y{1, 1, 1}, lo{-1, -1, -1}, hi{2, 2, 2};
    mg->setData(std::move(x), std::vector<std::vector<double>>{std::move(y), std::move(lo), std::move(hi)});
    mPlot->yAxis->setScaleType(QCPAxis::stLogarithmic);
    mPlot->xAxis->setRange(-0.5, 2.5);
    mPlot->yAxis->setRange(0.1, 10);
    mPlot->xAxis->setTicks(false); // the tick at x = 1 would cover the bottom rows
    const QImage img = renderOf(mPlot);
    const QRect r = mPlot->axisRect()->rect();
    const int xStem = int(mPlot->xAxis->coordToPixel(1));
    bool lit = false;
    for (int dy = 1; dy <= 3; ++dy)
        for (int dx = 0; dx <= 1; ++dx) // antialiased: a 1 px stem straddles two columns
        {
            const QColor c = img.pixelColor(xStem + dx, r.bottom() - dy);
            lit = lit || c.red() > c.green() + 60;
        }
    QVERIFY(lit); // the stem reaches the bottom edge
    // ... and stops there: the clipped pixels cannot tell the edge from 200 px past it.
    const auto bars = mg->errorBarRows(*mg->componentErrorBars(0), *mg->dataSource(), 0, 3);
    QCOMPARE(bars[1].lower, mPlot->yAxis->coordToPixel(0.1));
}

// A band's bound <= 0 goes two axis lengths out, like a fill edge: the GPU pans the cached band,
// and one stopping at the edge would show its end. Bars keep stopping at the edge.
void TestMultiGraph::errorBandBoundsBelowZeroOnALogAxisAreClampedLikeABaseline()
{
    auto* mg = errorBarGraph(mPlot, 3, 1.0);
    std::vector<double> x{0, 1, 2}, y{1, 1, 1}, lo{-1, 0, 0.5}, hi{2, 2, 2};
    mg->setData(std::move(x), std::vector<std::vector<double>>{std::move(y), std::move(lo), std::move(hi)});
    mPlot->yAxis->setScaleType(QCPAxis::stLogarithmic);
    mPlot->xAxis->setRange(-0.5, 2.5);
    mPlot->yAxis->setRange(0.1, 10);
    renderOf(mPlot); // lays the axis rect out
    const auto rows = mg->errorBandRows(*mg->componentErrorBars(0), *mg->dataSource(), 0, 3);
    const double zeroBaseline = qcp::fill::baselinePixel(*mPlot->yAxis, 0.0);
    QCOMPARE(rows[0].a, zeroBaseline);
    QCOMPARE(rows[1].a, zeroBaseline);
    QCOMPARE(rows[2].a, mPlot->yAxis->coordToPixel(0.5));
}

void TestMultiGraph::selectedErrorBarsUseTheSelectedPen()
{
    auto* mg = errorBarGraph(mPlot, 5, 1.0);
    auto& comp = mg->component(0);
    comp.selectedPen = QPen(Qt::blue, 2);
    mg->setComponentSelection(0, QCPDataSelection(QCPDataRange(0, 5)));
    mPlot->xAxis->setRange(-0.5, 4.5);
    mPlot->yAxis->setRange(0, 10);
    QImage img = renderOf(mPlot);
    const QPoint bar(int(mPlot->xAxis->coordToPixel(2)), int(mPlot->yAxis->coordToPixel(4)));
    const QColor selected = img.pixelColor(bar);
    QVERIFY2(selected.blue() > 150 && selected.red() < 100 && selected.green() < 100,
             qPrintable(selected.name()));
    mg->setComponentSelection(0, QCPDataSelection());
    img = renderOf(mPlot);
    QVERIFY(isRedish(img.pixelColor(bar)));
}

void TestMultiGraph::errorBarsAreDroppedWhenABoundColumnDisappears()
{
    auto* mg = errorBarGraph(mPlot, 3, 1.0);
    mg->setData(std::vector<double>{0, 1}, std::vector<std::vector<double>>{{1, 1}, {0, 0}});
    QVERIFY(!mg->componentErrorBars(0).has_value());
}

void TestMultiGraph::anEmptySourceKeepsFillsAndErrorBars()
{
    auto* mg = threeColumnGraph(mPlot);
    QVERIFY(mg->setComponentFill(2, 0.0));
    QVERIFY(mg->setComponentErrorBars(0, 1, 2));
    mg->setData(std::vector<double>{}, std::vector<std::vector<double>>{{}});
    QCOMPARE(mg->componentCount(), 3);
    mg->setDataSource(std::shared_ptr<QCPAbstractMultiDataSource>());
    mg->setData(std::vector<double>{1, 2, 3},
                std::vector<std::vector<double>>{{1, 2, 3}, {0, 1, 2}, {2, 3, 4}});
    QVERIFY(mg->componentFill(2).has_value());
    QVERIFY(mg->componentErrorBars(0).has_value());
    mg->setData(std::vector<double>{1, 2}, std::vector<std::vector<double>>{{1, 2}, {0, 1}});
    QVERIFY(!mg->componentFill(2).has_value()); // a non-empty narrower source is still lossy
}

// A (0, 3) row-major window degrades to a 0-column source: it says nothing about the columns.
void TestMultiGraph::anEmptySourceKeepsTheComponents()
{
    auto* mg = threeColumnGraph(mPlot);
    mg->setComponentColors({Qt::red, Qt::green, Qt::blue});
    mg->setComponentLineStyle(1, QCPMultiGraph::lsNone);
    mg->setComponentFillBetween(1, 2, QBrush(Qt::red));
    mPlot->xAxis->setRange(1, 3);
    mPlot->yAxis->setRange(-1, 5);
    QImage img = renderOf(mPlot);
    const QPoint inBand(int(mPlot->xAxis->coordToPixel(2)), int(mPlot->yAxis->coordToPixel(1.5)));
    QVERIFY(isRedish(img.pixelColor(inBand)));
    commitEmptyWindow(mPlot, mg);
    QCOMPARE(mg->componentCount(), 3);
    QCOMPARE(mg->component(0).name, QStringLiteral("y"));
    QCOMPARE(mg->component(2).pen.color(), QColor(Qt::blue));
    QCOMPARE(mg->component(1).lineStyle, QCPMultiGraph::lsNone);
    QVERIFY(!isRedish(renderOf(mPlot).pixelColor(inBand)));
    mg->setData(std::vector<double>{1, 2, 3},
                std::vector<std::vector<double>>{{1, 2, 3}, {0, 1, 2}, {2, 3, 4}});
    QTRY_VERIFY_WITH_TIMEOUT(!mg->hasPendingData(), 5000);
    QVERIFY(isRedish(renderOf(mPlot).pixelColor(inBand)));
}

void TestMultiGraph::anEmptyWiderSourceDoesNotGrowTheComponents()
{
    auto* mg = threeColumnGraph(mPlot);
    mg->setData(std::vector<double>{}, std::vector<std::vector<double>>(5));
    QCOMPARE(mg->dataSource()->columnCount(), 5);
    QCOMPARE(mg->componentCount(), 3);
}

void TestMultiGraph::keptComponentsStayEditableDuringAnEmptyWindow()
{
    auto* mg = threeColumnGraph(mPlot);
    commitEmptyWindow(mPlot, mg);
    QVERIFY(mg->setComponentFillBetween(1, 2));
    QVERIFY(mg->setComponentErrorBars(0, 1, 2));
    QVERIFY(!mg->setComponentFill(3, 0.0));
}

void TestMultiGraph::errorBarsRejectBadColumns()
{
    auto* mg = threeColumnGraph(mPlot);
    QVERIFY(!mg->setComponentErrorBars(0, 1, 1));
    QVERIFY(!mg->setComponentErrorBars(0, 0, 2));
    QVERIFY(!mg->setComponentErrorBars(0, 1, 5));
}

void TestMultiGraph::capWidthChangeShowsOnTheNextFrame()
{
    auto* mg = errorBarGraph(mPlot, 5, 1.0);
    mPlot->xAxis->setRange(-0.5, 4.5);
    mPlot->yAxis->setRange(0, 10);
    renderOf(mPlot); // lays the axis rect out: pixel coordinates are meaningless before
    const QPoint capEdge(int(mPlot->xAxis->coordToPixel(2)) + 8, int(mPlot->yAxis->coordToPixel(7)));
    QVERIFY(!isRedish(renderOf(mPlot).pixelColor(capEdge)));
    mg->setComponentErrorBars(0, 1, 2, 20);
    QVERIFY(isRedish(renderOf(mPlot).pixelColor(capEdge)));
}

void TestMultiGraph::errorBarsAreCachedOnTheGpu()
{
    if (!showWithRhi(mPlot))
        QSKIP("no QRhi available in this environment");
    auto* sparse = errorBarGraph(mPlot, 5, 1.0);
    mPlot->xAxis->setRange(-0.5, 4.5);
    mPlot->yAxis->setRange(0, 10);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(!sparse->mErrorBars.first().bars.isEmpty());
    QVERIFY(!sparse->mErrorBars.first().asBand);

    mPlot->clearPlottables();
    auto* dense = errorBarGraph(mPlot, 2000, 0.01);
    mPlot->xAxis->setRange(0, 20);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTRY_VERIFY_WITH_TIMEOUT(!dense->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(dense->mErrorBars.first().asBand);
    QVERIFY(!dense->mErrorBars.first().band.empty());
}

void TestMultiGraph::errorBarsHideTheirBoundsFromTheLegend()
{
    auto* mg = threeColumnGraph(mPlot);
    QVERIFY(mg->setComponentErrorBars(0, 1, 2));
    QCOMPARE(mg->legendComponents(), (QVector<int>{0}));
}

void TestMultiGraph::legendIconOfErrorBarsHasABarGlyph()
{
    auto* mg = threeColumnGraph(mPlot);
    mg->setComponentColors({Qt::red, Qt::red, Qt::red});
    mg->setComponentLineStyle(0, QCPMultiGraph::lsNone);
    mg->setComponentErrorBars(0, 1, 2);
    QImage img(40, 20, QImage::Format_ARGB32);
    img.fill(Qt::white);
    QCPPainter p(&img);
    mg->drawComponentLegendIcon(&p, 0, QLineF(0, 10, 40, 10));
    p.end();
    QVERIFY(isRedish(img.pixelColor(20, 7)));   // the stem
    QVERIFY(!isRedish(img.pixelColor(5, 7)));   // no line
}

void TestMultiGraph::errorBoundsSetBeforeDataStayOutOfTheLegend()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    QVERIFY(mg->setComponentErrorBars(0, 1, 2));
    mg->setData(std::vector<double>{1, 2}, std::vector<std::vector<double>>{{1, 1}, {0, 0}, {2, 2}});
    QCOMPARE(mg->legendComponents(), (QVector<int>{0}));
}
