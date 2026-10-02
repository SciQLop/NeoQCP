#include "test-intervals.h"
#include "qcustomplot.h"
#include "plottables/lane-layout.h"

void TestIntervals::init()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
}

void TestIntervals::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
}

void TestIntervals::lanesAreAppendedInFirstSeenOrder()
{
    QCPLaneLayout layout;
    QCOMPARE(layout.laneIndex("MSA"), 0);
    QCOMPARE(layout.laneIndex("MGF"), 1);
    QCOMPARE(layout.laneIndex("MSA"), 0);
    QCOMPARE(layout.laneNames(), QStringList({"MSA", "MGF"}));
    QCOMPARE(layout.visibleLaneCount(), 2);
}

void TestIntervals::bandsStackFromTheTopOfTheAxisRect()
{
    QCPLaneLayout layout;
    layout.setLaneHeight(14);
    layout.laneIndex("A");
    layout.laneIndex("B");
    const QRect rect(10, 20, 300, 200);
    const auto a = layout.laneBand(0, rect);
    const auto b = layout.laneBand(1, rect);
    QVERIFY(a && b);
    QCOMPARE(a->top, 20.0);
    QCOMPARE(a->bottom, 34.0);
    QCOMPARE(b->top, 34.0);
    QCOMPARE(layout.totalHeight(), 28);
}

void TestIntervals::hiddenLaneHasNoBand()
{
    QCPLaneLayout layout;
    layout.laneIndex("A");
    layout.laneIndex("B");
    layout.setDisplayOrder({"B"});
    QVERIFY(!layout.laneBand(0, QRect(0, 0, 100, 100)));
    QCOMPARE(layout.laneBand(1, QRect(0, 0, 100, 100))->top, 0.0);
    QVERIFY(!layout.laneVisible(0));
}

void TestIntervals::displayOrderReordersAndHides()
{
    QCPLaneLayout layout;
    layout.laneIndex("A");
    layout.laneIndex("B");
    QSignalSpy spy(&layout, &QCPLaneLayout::changed);
    layout.setDisplayOrder({"B", "A", "C"});
    QCOMPARE(layout.displayOrder(), QStringList({"B", "A", "C"}));
    QCOMPARE(layout.laneNames(), QStringList({"A", "B", "C"}));
    QCOMPARE(spy.count(), 1);
}

void TestIntervals::renameKeepsTheIndex()
{
    QCPLaneLayout layout;
    layout.laneIndex("A");
    QVERIFY(layout.renameLane("A", "Z"));
    QCOMPARE(layout.laneIndex("Z"), 0);
    QVERIFY(!layout.renameLane("missing", "Y"));
}

void TestIntervals::laneAtInvertsLaneBand()
{
    QCPLaneLayout layout;
    layout.setLaneHeight(10);
    layout.laneIndex("A");
    layout.laneIndex("B");
    const QRect rect(0, 50, 100, 100);
    QCOMPARE(layout.laneAt(55, rect), 0);
    QCOMPARE(layout.laneAt(65, rect), 1);
    QCOMPARE(layout.laneAt(75, rect), -1);
    QCOMPARE(layout.laneAt(40, rect), -1);
}
