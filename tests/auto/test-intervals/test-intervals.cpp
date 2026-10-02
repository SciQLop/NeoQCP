#include "test-intervals.h"
#include "qcustomplot.h"
#include "plottables/lane-layout.h"
#include "plottables/intervals-algo.h"
#include <QElapsedTimer>
#include <numeric>

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

namespace {
qcp::intervals::Columns columns(std::vector<double> start, std::vector<double> stop,
                                std::vector<int> lane)
{
    const auto n = start.size();
    qcp::intervals::Columns c;
    c.start = std::move(start);
    c.stop = std::move(stop);
    c.lane = std::move(lane);
    c.category.assign(n, 0);
    c.ids.resize(n);
    std::iota(c.ids.begin(), c.ids.end(), qint64 { 100 });
    return c;
}
} // namespace

void TestIntervals::invalidColumnsAreReported()
{
    using qcp::intervals::invalidColumns;
    QVERIFY(!invalidColumns(columns({0, 1}, {1, 2}, {0, 0})));
    auto shortStop = columns({0, 1}, {1, 2}, {0, 0});
    shortStop.stop.pop_back();
    QVERIFY(invalidColumns(shortStop));
    QVERIFY(invalidColumns(columns({2}, {1}, {0})));
    QVERIFY(invalidColumns(columns({std::nan("")}, {1}, {0})));
    auto badLabels = columns({0}, {1}, {0});
    badLabels.labels = QStringList { "a", "b" };
    QVERIFY(invalidColumns(badLabels));
}

void TestIntervals::groupByLaneSortsEachLane()
{
    const auto lanes = qcp::intervals::groupByLane(columns({5, 1, 3}, {6, 2, 9}, {0, 1, 0}), 2);
    QCOMPARE(lanes.size(), std::size_t { 2 });
    QVERIFY(lanes[0].rows == (std::vector<int> { 2, 0 }));
    QCOMPARE(lanes[0].maxDuration, 6.0);
    QVERIFY(lanes[1].rows == (std::vector<int> { 1 }));
}

void TestIntervals::visibleRowsIncludesLongBarsStartingBeforeRange()
{
    const auto lanes = qcp::intervals::groupByLane(columns({0, 40, 60}, {100, 41, 61}, {0, 0, 0}), 1);
    const auto [b, e] = qcp::intervals::candidateRange(lanes[0], 50, 55);
    QCOMPARE(b, 0);
    QCOMPARE(e, 2); // rows 0 (long) and 1 (ends before 50, filtered by the caller)
}

void TestIntervals::pixelBarsAreAtLeastOnePixelWide()
{
    const auto bar = qcp::intervals::toPixelBar(10.2, 10.2, 0, 14, 3, 7);
    QVERIFY(bar.instant);
    QVERIFY(bar.x1 - bar.x0 >= 1.0);
    const auto reversed = qcp::intervals::toPixelBar(30, 20, 0, 14, 3, 7);
    QCOMPARE(reversed.x0, 20.0);
    QVERIFY(!reversed.instant);
}

void TestIntervals::overlappingSameCategoryBarsMerge()
{
    using namespace qcp::intervals;
    std::vector<PixelBar> bars;
    appendMerged(bars, toPixelBar(0, 10, 0, 14, 1, 0));
    appendMerged(bars, toPixelBar(9, 20, 0, 14, 1, 1));
    appendMerged(bars, toPixelBar(19, 30, 0, 14, 2, 2));
    QCOMPARE(bars.size(), std::size_t { 2 });
    QCOMPARE(bars[0].x1, 20.0);
    QCOMPARE(bars[0].row, -1);
    QCOMPARE(bars[1].category, 2);
}

void TestIntervals::quadIsTwoTriangles()
{
    std::vector<float> out;
    qcp::intervals::appendQuad(out, QRectF(0, 0, 10, 5), { 1, 0, 0, 1 });
    QCOMPARE(out.size(), std::size_t { 36 });
    QCOMPARE(out[0], 0.0f);
    QCOMPARE(out[2], 1.0f); // colour follows position
}

void TestIntervals::millionIntervalsScanQuickly()
{
    const int n = 1'000'000;
    std::vector<double> start(n), stop(n);
    std::vector<int> lane(n);
    for (int i = 0; i < n; ++i)
    {
        start[i] = i;
        stop[i] = i + 0.5;
        lane[i] = i % 20;
    }
    const auto lanes = qcp::intervals::groupByLane(columns(start, stop, lane), 20);
    QElapsedTimer timer;
    timer.start();
    std::vector<qcp::intervals::PixelBar> bars;
    for (const auto& rows : lanes)
    {
        const auto [b, e] = qcp::intervals::candidateRange(rows, 0, n);
        for (int i = b; i < e; ++i)
        {
            const int row = rows.rows[i];
            const double px = start[row] * 1000.0 / n;
            qcp::intervals::appendMerged(
                bars, qcp::intervals::toPixelBar(px, stop[row] * 1000.0 / n, 0, 14, 0, row));
        }
    }
    QVERIFY2(timer.elapsed() < 100, qPrintable(QString::number(timer.elapsed())));
    QVERIFY(bars.size() <= 20 * 1001);
}
