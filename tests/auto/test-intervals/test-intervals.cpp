#include "test-intervals.h"
#include "qcustomplot.h"
#include "plottables/lane-layout.h"
#include "plottables/intervals-algo.h"
#include "plottables/plottable-intervals.h"
#include <QElapsedTimer>
#include <QtWidgets/qtestsupport_widgets.h>
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

namespace {
QCPIntervals* twoBars(QCustomPlot* plot, QCPLaneLayout* layout)
{
    auto* iv = new QCPIntervals(plot->xAxis, plot->yAxis, layout);
    auto c = columns({10, 60}, {40, 90}, { layout->laneIndex("A"), layout->laneIndex("B") });
    c.category = { 0, 1 };
    iv->setData(std::move(c));
    iv->setCategoryColors({ Qt::red, Qt::blue });
    plot->xAxis->setRange(0, 100);
    return iv;
}

QColor pixelAt(const QImage& image, QCustomPlot* plot, double key, double laneCentreFromTop)
{
    const double dpr = image.devicePixelRatio();
    const QRect rect = plot->axisRect()->rect();
    return image.pixelColor(qRound(plot->xAxis->coordToPixel(key) * dpr),
                            qRound((rect.top() + laneCentreFromTop) * dpr));
}

bool isRedish(const QColor& c) { return c.red() > 150 && c.green() < 120 && c.blue() < 120; }
bool isBlueish(const QColor& c) { return c.blue() > 150 && c.red() < 120 && c.green() < 120; }
} // namespace

void TestIntervals::setDataRejectsInvalidColumns()
{
    QCPLaneLayout layout;
    auto* iv = new QCPIntervals(mPlot->xAxis, mPlot->yAxis, &layout);
    QVERIFY_THROWS_EXCEPTION(std::invalid_argument, iv->setData(columns({2}, {1}, {0})));
    QCOMPARE(iv->rowCount(), 0);
}

void TestIntervals::emptyDataDrawsNothing()
{
    QCPLaneLayout layout;
    new QCPIntervals(mPlot->xAxis, mPlot->yAxis, &layout);
    mPlot->toPixmap(400, 300); // must not crash
}

void TestIntervals::barsAreDrawnOnTheirLanes()
{
    QCPLaneLayout layout;
    layout.setPlacement(QCPLaneLayout::plLanes);
    twoBars(mPlot, &layout);
    const QImage image = mPlot->toPixmap(400, 300).toImage();
    QVERIFY(isRedish(pixelAt(image, mPlot, 25, 7)));    // lane A, inside bar 0
    QVERIFY(isBlueish(pixelAt(image, mPlot, 75, 21)));  // lane B, inside bar 1
    QVERIFY(!isRedish(pixelAt(image, mPlot, 75, 7)));   // lane A, no bar
}

void TestIntervals::keyRangeSpansAllIntervals()
{
    QCPLaneLayout layout;
    auto* iv = twoBars(mPlot, &layout);
    bool found = false;
    const QCPRange r = iv->getKeyRange(found);
    QVERIFY(found);
    QCOMPARE(r.lower, 10.0);
    QCOMPARE(r.upper, 90.0);
    iv->getValueRange(found);
    QVERIFY(!found); // never drives the value axis
}

void TestIntervals::categoryColorFallsBackBeyondTheTable()
{
    QCPLaneLayout layout;
    auto* iv = new QCPIntervals(mPlot->xAxis, mPlot->yAxis, &layout);
    iv->setCategoryColors({ Qt::red });
    QCOMPARE(iv->categoryColor(0), QColor(Qt::red));
    QVERIFY(iv->categoryColor(1).isValid());
    QVERIFY(iv->categoryColor(1) != iv->categoryColor(2));
}

void TestIntervals::barsRebuildOnlyWhenTheViewChanges()
{
    QCPLaneLayout layout;
    auto* iv = twoBars(mPlot, &layout);
    mPlot->toPixmap(400, 300);
    const auto builds = iv->buildCount();
    mPlot->toPixmap(400, 300);
    QCOMPARE(iv->buildCount(), builds);
    mPlot->xAxis->setRange(5, 105);
    mPlot->toPixmap(400, 300);
    QCOMPARE(iv->buildCount(), builds + 1);
    layout.setLaneHeight(20);
    mPlot->toPixmap(400, 300);
    QCOMPARE(iv->buildCount(), builds + 2);
}

void TestIntervals::stripPlacementIsTranslucent()
{
    QCPLaneLayout layout; // plStrip by default
    twoBars(mPlot, &layout);
    const QColor c = pixelAt(mPlot->toPixmap(400, 300).toImage(), mPlot, 25, 7);
    QVERIFY2(c.red() > 150 && c.green() > 40, qPrintable(c.name())); // red blended with white
}

void TestIntervals::labelIsDrawnOnlyWhenItFits()
{
    QCPLaneLayout layout;
    layout.setPlacement(QCPLaneLayout::plLanes);
    layout.setLaneHeight(20);
    auto* iv = new QCPIntervals(mPlot->xAxis, mPlot->yAxis, &layout);
    auto c = columns({ 0, 99.5 }, { 60, 99.6 }, { layout.laneIndex("A"), 0 });
    c.labels = QStringList { "wide label", "narrow label" };
    iv->setData(std::move(c));
    iv->setCategoryColors({ Qt::white });
    mPlot->xAxis->setRange(0, 100);
    mPlot->toPixmap(400, 300);
    QCOMPARE(iv->mLabelRects.size(), std::size_t { 1 });
    QCOMPARE(iv->mLabelRects[0].second, 0);
}

void TestIntervals::stripDrawsLaneNamesOnce()
{
    QCPLaneLayout layout;
    auto* first = twoBars(mPlot, &layout);
    auto* second = new QCPIntervals(mPlot->xAxis, mPlot->yAxis, &layout);
    QVERIFY(first->drawsLaneNames());
    QVERIFY(!second->drawsLaneNames());
    layout.setPlacement(QCPLaneLayout::plLanes);
    QVERIFY(!first->drawsLaneNames());
}

void TestIntervals::barsAndLabelsShowOnTheGpu()
{
    // Widened from the 400px default (used by every other test here): at 400px the bar's
    // axis-rect width (~367px in this environment's font) is narrower than the 34-char
    // label's advance (~379px), so the fits-check in appendLabelRect correctly suppresses
    // it — exactly the rule under test in labelIsDrawnOnlyWhenItFits. This test needs the
    // label to actually fit so it can prove it renders above the GPU bars.
    mPlot->resize(500, 300);
    mPlot->show();
    if (!QTest::qWaitForWindowExposed(mPlot))
        QSKIP("window not exposed in this environment");
    QCoreApplication::processEvents();
    if (!mPlot->rhi())
        QSKIP("no QRhi available in this environment");
    QCPLaneLayout layout;
    layout.setPlacement(QCPLaneLayout::plLanes);
    layout.setLaneHeight(24);
    auto* iv = new QCPIntervals(mPlot->xAxis, mPlot->yAxis, &layout);
    auto c = columns({ 0 }, { 100 }, { layout.laneIndex("A") });
    c.labels = QStringList { "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW" };
    iv->setData(std::move(c));
    iv->setCategoryColors({ Qt::red });
    mPlot->xAxis->setRange(0, 100);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    const QImage frame = mPlot->grabFramebuffer();
    QVERIFY(isRedish(pixelAt(frame, mPlot, 5, 12)));
    int nonRed = 0;
    for (double key = 20; key < 80; key += 0.5)
        nonRed += isRedish(pixelAt(frame, mPlot, key, 12)) ? 0 : 1;
    QVERIFY2(nonRed > 5, "label text is hidden under the GPU bars");
}
