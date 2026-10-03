#include "test-intervals.h"
#include "qcustomplot.h"
#include "plottables/lane-layout.h"
#include "plottables/intervals-algo.h"
#include "plottables/plottable-intervals.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QMouseEvent>
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

// A lanes plot labels its lanes with y-axis ticks, which follow the axis rect. The bands must
// follow it too, or bars and names drift apart whenever the plot isn't at its natural height.
void TestIntervals::lanesPlacementSplitsTheAxisRect()
{
    QCPLaneLayout layout;
    layout.setLaneHeight(18);
    layout.setPlacement(QCPLaneLayout::plLanes);
    layout.laneIndices({ "A", "B", "C" });
    const QRect rect(0, 10, 100, 90);
    const auto b = layout.laneBand(1, rect);
    QVERIFY(b);
    QCOMPARE(b->top, 40.0);
    QCOMPARE(b->bottom, 70.0);
    QCOMPARE(layout.laneAt(45, rect), 1);
    QCOMPARE(layout.laneAt(95, rect), 2);
    QCOMPARE(layout.lanePixelHeight(rect), 30.0);
    QCOMPARE(layout.lanePixelHeight(QRect(0, 0, 100, 54)), 18.0); // natural height
}

void TestIntervals::busShapeHasAngledEnds()
{
    const QPolygonF hex = qcp::intervals::busShape(QRectF(0, 0, 40, 14), 4);
    QCOMPARE(hex, QPolygonF({ { 0, 7 }, { 4, 0 }, { 36, 0 }, { 40, 7 }, { 36, 14 }, { 4, 14 } }));
    // Too narrow for the full slant: the ends meet in the middle, a diamond-like shape.
    const QPolygonF narrow = qcp::intervals::busShape(QRectF(0, 0, 6, 14), 4);
    QCOMPARE(narrow[1], QPointF(3, 0));
    QCOMPARE(narrow[2], QPointF(3, 0));
}

void TestIntervals::fanTriangulatesAConvexPolygon()
{
    std::vector<float> out;
    qcp::intervals::appendFan(out, qcp::intervals::busShape(QRectF(0, 0, 40, 14), 4), { 1, 0, 0, 1 });
    QCOMPARE(out.size(), std::size_t { 4 * 3 * 6 }); // 4 triangles, 3 vertices, x y r g b a
}

void TestIntervals::labelsElideOrHide()
{
    const QFontMetricsF fm(QFont{});
    using qcp::intervals::fittedLabel;
    QCOMPARE(fittedLabel(fm, "burst mode", fm.horizontalAdvance("burst mode") + 1), QString("burst mode"));
    const auto elided = fittedLabel(fm, "burst mode", fm.horizontalAdvance(QStringLiteral("burs…")) + 1);
    QVERIFY(elided && elided->endsWith(QChar(0x2026)) && elided->size() >= 4);
    QVERIFY(!fittedLabel(fm, "burst mode", fm.horizontalAdvance(QStringLiteral("b…"))));
    QCOMPARE(fittedLabel(fm, "LM", fm.horizontalAdvance("LM") + 1), QString("LM")); // short text fits whole
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

// Same text on both sides: the merged bar can still show it.
void TestIntervals::mergedBarsWithTheSameLabelKeepIt()
{
    using namespace qcp::intervals;
    std::vector<PixelBar> bars;
    appendMerged(bars, toPixelBar(0, 10, 0, 14, 1, 0));
    appendMerged(bars, toPixelBar(9, 20, 0, 14, 1, 1), true);
    QCOMPARE(bars.size(), std::size_t { 1 });
    QCOMPARE(bars[0].row, 0);
    appendMerged(bars, toPixelBar(19, 30, 0, 14, 1, 2), false);
    QCOMPARE(bars[0].row, -1);
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
    // Budget sized for virtualized CI runners (~3x slower than a desktop); a quadratic scan
    // of 10^6 rows would still take minutes.
    QVERIFY2(timer.elapsed() < 500, qPrintable(QString::number(timer.elapsed())));
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

bool showWithRhi(QCustomPlot* plot)
{
    plot->show();
    if (!QTest::qWaitForWindowExposed(plot))
        return false;
    QCoreApplication::processEvents();
    return plot->rhi() != nullptr;
}

// Centred in a bar spanning the axis rect, it covers the middle half whatever the platform font.
QString labelSpanningHalfTheAxisRect(QCustomPlot* plot)
{
    const double advance = QFontMetricsF(plot->font()).horizontalAdvance(QLatin1Char('W'));
    return QString(qMax(1, int(plot->axisRect()->width() / 2.0 / advance)), QLatin1Char('W'));
}

bool isRedish(const QColor& c) { return c.red() > 150 && c.green() < 120 && c.blue() < 120; }
bool isBlueish(const QColor& c) { return c.blue() > 150 && c.red() < 120 && c.green() < 120; }

void press(QWidget* w, QPoint pos, Qt::KeyboardModifiers mods = Qt::NoModifier)
{
    QMouseEvent e(QEvent::MouseButtonPress, pos, w->mapToGlobal(pos), Qt::LeftButton,
                  Qt::LeftButton, mods);
    QApplication::sendEvent(w, &e);
}

void moveTo(QWidget* w, QPoint pos, Qt::KeyboardModifiers mods = Qt::NoModifier)
{
    QMouseEvent e(QEvent::MouseMove, pos, w->mapToGlobal(pos), Qt::NoButton, Qt::LeftButton, mods);
    QApplication::sendEvent(w, &e);
}

void release(QWidget* w, QPoint pos, Qt::KeyboardModifiers mods = Qt::NoModifier)
{
    QMouseEvent e(QEvent::MouseButtonRelease, pos, w->mapToGlobal(pos), Qt::LeftButton,
                  Qt::NoButton, mods);
    QApplication::sendEvent(w, &e);
}

void click(QWidget* w, QPoint pos, Qt::KeyboardModifiers mods = Qt::NoModifier)
{
    press(w, pos, mods);
    release(w, pos, mods);
}

//! Bars [10,40] on lane A and [60,90] on lane B, x range [0,100], laid out.
QCPIntervals* laidOutTwoBars(QCustomPlot* plot, QCPLaneLayout* layout)
{
    auto* iv = twoBars(plot, layout);
    plot->setInteractions(QCP::iSelectPlottables | QCP::iMultiSelect);
    plot->replot();
    return iv;
}
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
    const double laneA = layout.lanePixelHeight(mPlot->axisRect()->rect()) / 2;
    const double laneB = laneA * 3;
    QVERIFY(isRedish(pixelAt(image, mPlot, 25, laneA)));    // inside bar 0
    QVERIFY(isBlueish(pixelAt(image, mPlot, 75, laneB)));   // inside bar 1
    QVERIFY(!isRedish(pixelAt(image, mPlot, 75, laneA)));   // lane A, no bar
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
    QCOMPARE(iv->mLabelRects[0].row, 0);
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
    iv->setCategoryColors({ Qt::red });
    mPlot->xAxis->setRange(0, 100);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    auto c = columns({ 0 }, { 100 }, { layout.laneIndex("A") });
    c.labels = QStringList { labelSpanningHalfTheAxisRect(mPlot) };
    iv->setData(std::move(c));
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    const QImage frame = mPlot->grabFramebuffer();
    const double laneCentre = layout.lanePixelHeight(mPlot->axisRect()->rect()) / 2;
    QVERIFY(isRedish(pixelAt(frame, mPlot, 5, laneCentre)));
    int nonRed = 0;
    for (double key = 30; key < 70; key += 0.5)
        nonRed += isRedish(pixelAt(frame, mPlot, key, laneCentre)) ? 0 : 1;
    QVERIFY2(nonRed > 5, "label text is hidden under the GPU bars");
}

namespace {
//! A red bar over the whole [0, 100] range with a label covering its middle half, drawn once.
QCPIntervals* drawnLabelledBar(QCustomPlot* plot, QCPLaneLayout* layout)
{
    layout->setPlacement(QCPLaneLayout::plLanes);
    layout->setLaneHeight(24);
    auto* iv = new QCPIntervals(plot->xAxis, plot->yAxis, layout);
    iv->setCategoryColors({ Qt::red });
    plot->xAxis->setRange(0, 100);
    plot->replot(QCustomPlot::rpImmediateRefresh);
    auto c = columns({ 0 }, { 100 }, { layout->laneIndex("A") });
    c.labels = QStringList { labelSpanningHalfTheAxisRect(plot) };
    iv->setData(std::move(c));
    plot->replot(QCustomPlot::rpImmediateRefresh);
    return iv;
}

int nonRedBetween(const QImage& frame, QCustomPlot* plot, double from, double to, double fromTop)
{
    int n = 0;
    for (double key = from; key < to; key += 0.5)
        n += isRedish(pixelAt(frame, plot, key, fromTop)) ? 0 : 1;
    return n;
}
} // namespace

void TestIntervals::labelsFollowAPanOnTheGpu()
{
    if (!showWithRhi(mPlot))
        QSKIP("no QRhi available in this environment");
    QCPLaneLayout layout;
    drawnLabelledBar(mPlot, &layout);
    const double laneCentre = layout.lanePixelHeight(mPlot->axisRect()->rect()) / 2;
    QVERIFY(nonRedBetween(mPlot->grabFramebuffer(), mPlot, 30, 70, laneCentre) > 5);

    // The label covers keys 25..75. A stale overlay would leave its text where keys 65..115
    // now are; drawn again, it is centred in the bar's visible part (keys 40..100), about
    // keys 45..95, so text shows up in 46..62 only if the labels were redrawn.
    mPlot->xAxis->setRange(40, 140);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(nonRedBetween(mPlot->grabFramebuffer(), mPlot, 46, 62, laneCentre) > 5);
}

void TestIntervals::selectionOutlineShowsOnTheGpu()
{
    if (!showWithRhi(mPlot))
        QSKIP("no QRhi available in this environment");
    QCPLaneLayout layout;
    auto* iv = drawnLabelledBar(mPlot, &layout);
    QCOMPARE(nonRedBetween(mPlot->grabFramebuffer(), mPlot, 5, 20, 1.5), 0);

    iv->setSelectedRows({ 0 });
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY2(nonRedBetween(mPlot->grabFramebuffer(), mPlot, 5, 20, 1.5) > 5,
             "the selection outline was not drawn");
}

void TestIntervals::mergedBarsGetNoLabel()
{
    QCPLaneLayout layout;
    auto* iv = new QCPIntervals(mPlot->xAxis, mPlot->yAxis, &layout);
    auto c = columns({ 0, 20 }, { 30, 50 }, { layout.laneIndex("A"), layout.laneIndex("A") });
    c.labels = QStringList { "a", "b" };
    iv->setData(std::move(c));
    iv->setCategoryColors({ Qt::red });
    mPlot->xAxis->setRange(0, 100);
    mPlot->toPixmap(400, 300);
    QVERIFY(iv->mLabelRects.empty());
}

void TestIntervals::labelLayerFollowsThePlottablesLayer()
{
    QCPLaneLayout layout;
    auto* iv = new QCPIntervals(mPlot->xAxis, mPlot->yAxis, &layout);
    QVERIFY(mPlot->addLayer(QStringLiteral("custom")));
    QVERIFY(iv->setLayer(QStringLiteral("custom")));
    QCOMPARE(iv->labelLayer(), mPlot->layer(QStringLiteral("custom.intervals-labels")));
}

void TestIntervals::hitTestOnEmptyPlottable()
{
    QCPLaneLayout layout;
    auto* iv = new QCPIntervals(mPlot->xAxis, mPlot->yAxis, &layout);
    mPlot->replot();
    QCOMPARE(iv->hitTest(QPointF(100, 100)).part, QCPIntervals::hpNone);
}

void TestIntervals::hitTestFindsBodyAndEdges()
{
    QCPLaneLayout layout;
    auto* iv = laidOutTwoBars(mPlot, &layout);
    const QPointF body = iv->pixelOf(25, 0);
    QCOMPARE(iv->hitTest(body).part, QCPIntervals::hpBody);
    QCOMPARE(iv->hitTest(body).row, 0);
    QCOMPARE(iv->hitTest(iv->pixelOf(10, 0) + QPointF(2, 0)).part, QCPIntervals::hpLeftEdge);
    QCOMPARE(iv->hitTest(iv->pixelOf(40, 0) - QPointF(2, 0)).part, QCPIntervals::hpRightEdge);
    QCOMPARE(iv->hitTest(iv->pixelOf(50, 0)).part, QCPIntervals::hpEmpty);
    QCOMPARE(iv->hitTest(iv->pixelOf(50, 0)).lane, 0);
}

// A bar shrunk to a few pixels has no room for edge zones inside it: they sit just outside its
// ends, so it can still be resized; a press inside moves it.
void TestIntervals::narrowBarResizesFromJustOutside()
{
    QCPLaneLayout layout;
    auto* iv = new QCPIntervals(mPlot->xAxis, mPlot->yAxis, &layout);
    iv->setData(columns({ 50 }, { 50.5 }, { layout.laneIndex("A") }));
    mPlot->xAxis->setRange(0, 100);
    mPlot->replot();
    const QRectF bar = iv->barRect(0);
    const double y = bar.center().y();
    QCOMPARE(iv->hitTest(bar.center()).part, QCPIntervals::hpBody);
    QCOMPARE(iv->hitTest(QPointF(bar.left() - 2, y)).part, QCPIntervals::hpLeftEdge);
    QCOMPARE(iv->hitTest(QPointF(bar.right() + 2, y)).part, QCPIntervals::hpRightEdge);
}

void TestIntervals::instantEventHasOnlyBody()
{
    QCPLaneLayout layout;
    auto* iv = new QCPIntervals(mPlot->xAxis, mPlot->yAxis, &layout);
    iv->setData(columns({ 50 }, { 50 }, { layout.laneIndex("A") }));
    mPlot->xAxis->setRange(0, 100);
    mPlot->replot();
    const QRectF bar = iv->barRect(0);
    QCOMPARE(iv->hitTest(QPointF(bar.left() - 2, bar.center().y())).part, QCPIntervals::hpBody);
    QCOMPARE(iv->hitTest(QPointF(bar.right() + 2, bar.center().y())).part, QCPIntervals::hpBody);
}

void TestIntervals::hiddenLaneIsNotHit()
{
    QCPLaneLayout layout;
    auto* iv = laidOutTwoBars(mPlot, &layout);
    const QPointF onB = iv->pixelOf(75, 1);
    layout.setDisplayOrder({ "A" });
    mPlot->replot();
    QVERIFY(iv->hitTest(onB).row != 1);
}

void TestIntervals::clickSelectsAndCtrlClickToggles()
{
    QCPLaneLayout layout;
    auto* iv = laidOutTwoBars(mPlot, &layout);
    click(mPlot, iv->pixelOf(25, 0).toPoint());
    QCOMPARE(iv->selectedRows(), QVector<int>({ 0 }));
    click(mPlot, iv->pixelOf(75, 1).toPoint(), Qt::ControlModifier);
    QCOMPARE(iv->selectedRows(), QVector<int>({ 0, 1 }));
    click(mPlot, iv->pixelOf(25, 0).toPoint(), Qt::ControlModifier);
    QCOMPARE(iv->selectedRows(), QVector<int>({ 1 }));
    QCOMPARE(iv->selectedIds(), QVector<qint64>({ 101 }));
}

void TestIntervals::clickOnEmptyLaneClearsSelection()
{
    QCPLaneLayout layout;
    auto* iv = laidOutTwoBars(mPlot, &layout);
    iv->setSelectedRows({ 0, 1 });
    click(mPlot, iv->pixelOf(50, 0).toPoint());
    QVERIFY(iv->selectedRows().isEmpty());
}

void TestIntervals::shiftDragSelectsRowsInTheRect()
{
    QCPLaneLayout layout;
    auto* iv = laidOutTwoBars(mPlot, &layout);
    const QPoint from = iv->pixelOf(5, 0).toPoint();
    const QPoint to = iv->pixelOf(95, 1).toPoint();
    press(mPlot, from, Qt::ShiftModifier);
    moveTo(mPlot, to, Qt::ShiftModifier);
    release(mPlot, to, Qt::ShiftModifier);
    QCOMPARE(iv->selectedRows(), QVector<int>({ 0, 1 }));
}

void TestIntervals::selectTestRectFindsRows()
{
    QCPLaneLayout layout;
    auto* iv = laidOutTwoBars(mPlot, &layout);
    const QRectF rect(iv->pixelOf(0, 1) - QPointF(0, 3), iv->pixelOf(100, 1) + QPointF(0, 3));
    const QCPDataSelection sel = iv->selectTestRect(rect, false);
    QCOMPARE(sel, QCPDataSelection(QCPDataRange(1, 2)));
}

void TestIntervals::moveShiftsBothEdges()
{
    using namespace qcp::intervals;
    const auto edits = applyDrag({ { 3, 10, 40, 0 } }, DragKind::Move, 5, 0, { 0 });
    QCOMPARE(edits.size(), std::size_t { 1 });
    QCOMPARE(edits[0].row, 3);
    QCOMPARE(edits[0].start, 15.0);
    QCOMPARE(edits[0].stop, 45.0);
}

void TestIntervals::resizeNeverCrossesTheOtherEdge()
{
    using namespace qcp::intervals;
    QCOMPARE(applyDrag({ { 0, 10, 40, 0 } }, DragKind::ResizeLeft, 50, 0, { 0 })[0].start, 40.0);
    QCOMPARE(applyDrag({ { 0, 10, 40, 0 } }, DragKind::ResizeRight, -50, 0, { 0 })[0].stop, 10.0);
    QCOMPARE(applyDrag({ { 0, 10, 40, 0 } }, DragKind::ResizeRight, 5, 0, { 0 })[0].start, 10.0);
}

void TestIntervals::laneDeltaClampsAtEdges()
{
    using qcp::intervals::shiftLane;
    QCOMPARE(shiftLane(0, -1, { 0, 1, 2 }), 0);
    QCOMPARE(shiftLane(2, 5, { 0, 1, 2 }), 2);
    QCOMPARE(shiftLane(1, 1, { 0, 1, 2 }), 2);
}

void TestIntervals::laneDeltaSkipsHiddenLanes()
{
    using qcp::intervals::shiftLane;
    QCOMPARE(shiftLane(0, 1, { 0, 2 }), 2);
    QCOMPARE(shiftLane(1, 1, { 0, 2 }), 1); // hidden lane: stays
}

void TestIntervals::snapToStepAlignsTheEdge()
{
    QCOMPARE(qcp::intervals::snapToStep(12, 5, 10), 8.0);
    QCOMPARE(qcp::intervals::snapToStep(12, 5, 0), 5.0);
}

void TestIntervals::snapToEdgesLandsOnTheClosestCandidate()
{
    using qcp::intervals::snapToEdges;
    QCOMPARE(snapToEdges({ 10, 40 }, 4.5, { 45.5, 100 }, 2), 5.5);
    QCOMPARE(snapToEdges({ 10, 40 }, 4.5, { 50, 100 }, 2), 4.5);
    QCOMPARE(snapToEdges({ 10 }, 0, {}, 2), 0.0);
}

namespace {
constexpr double keyTolerance = 0.6; // ~2 px at 100 keys over ~350 px

QCPIntervals* editableTwoBars(QCustomPlot* plot, QCPLaneLayout* layout,
                              QCPIntervals::EditModes modes)
{
    auto* iv = laidOutTwoBars(plot, layout);
    iv->setEditable(true);
    iv->setEditModes(modes);
    return iv;
}

void drag(QCustomPlot* plot, QPointF from, QPointF to)
{
    press(plot, from.toPoint());
    moveTo(plot, to.toPoint());
    release(plot, to.toPoint());
}
} // namespace

void TestIntervals::dragBodyEmitsOnceOnReleaseAndLeavesDataAlone()
{
    qRegisterMetaType<QVector<QCPIntervalEdit>>();
    QCPLaneLayout layout;
    auto* iv = editableTwoBars(mPlot, &layout, QCPIntervals::emMove);
    QSignalSpy spy(iv, &QCPIntervals::intervalsEdited);
    press(mPlot, iv->pixelOf(25, 0).toPoint());
    moveTo(mPlot, iv->pixelOf(35, 0).toPoint());
    QCOMPARE(spy.count(), 0);
    release(mPlot, iv->pixelOf(35, 0).toPoint());
    QCOMPARE(spy.count(), 1);
    const auto edits = spy.at(0).at(0).value<QVector<QCPIntervalEdit>>();
    QCOMPARE(edits.size(), 1);
    QCOMPARE(edits[0].id, qint64 { 100 });
    QVERIFY(qAbs(edits[0].start - 20) < keyTolerance);
    QVERIFY(qAbs(edits[0].stop - 50) < keyTolerance);
    QCOMPARE(iv->columns().start[0], 10.0);
}

void TestIntervals::clickWithoutMovingEmitsNothing()
{
    QCPLaneLayout layout;
    auto* iv = editableTwoBars(mPlot, &layout, QCPIntervals::emMove);
    QSignalSpy spy(iv, &QCPIntervals::intervalsEdited);
    click(mPlot, iv->pixelOf(25, 0).toPoint());
    QCOMPARE(spy.count(), 0);
    QCOMPARE(iv->selectedRows(), QVector<int>({ 0 }));
}

void TestIntervals::dragRightEdgeResizes()
{
    QCPLaneLayout layout;
    auto* iv = editableTwoBars(mPlot, &layout, QCPIntervals::emResize);
    QSignalSpy spy(iv, &QCPIntervals::intervalsEdited);
    drag(mPlot, iv->pixelOf(40, 0) - QPointF(2, 0), iv->pixelOf(50, 0) - QPointF(2, 0));
    const auto e = spy.at(0).at(0).value<QVector<QCPIntervalEdit>>()[0];
    QCOMPARE(e.start, 10.0);
    QVERIFY(qAbs(e.stop - 50) < keyTolerance);
}

void TestIntervals::verticalDragChangesLaneOnlyWhenAllowed()
{
    QCPLaneLayout layout;
    auto* iv = editableTwoBars(mPlot, &layout, QCPIntervals::emMove);
    QSignalSpy spy(iv, &QCPIntervals::intervalsEdited);
    drag(mPlot, iv->pixelOf(25, 0), iv->pixelOf(25, 1));
    QCOMPARE(spy.count(), 0); // moved (vertically) but lane change is off: nothing changed
    iv->setEditModes(QCPIntervals::emMove | QCPIntervals::emChangeLane);
    drag(mPlot, iv->pixelOf(25, 0), iv->pixelOf(25, 1));
    QCOMPARE(spy.at(0).at(0).value<QVector<QCPIntervalEdit>>()[0].lane, 1);
}

void TestIntervals::selectedIntervalsMoveTogether()
{
    QCPLaneLayout layout;
    auto* iv = editableTwoBars(mPlot, &layout, QCPIntervals::emMove);
    iv->setSelectedRows({ 0, 1 });
    QSignalSpy spy(iv, &QCPIntervals::intervalsEdited);
    drag(mPlot, iv->pixelOf(25, 0), iv->pixelOf(35, 0));
    const auto edits = spy.at(0).at(0).value<QVector<QCPIntervalEdit>>();
    QCOMPARE(edits.size(), 2);
    QVERIFY(qAbs(edits[1].start - 70) < keyTolerance);
}

void TestIntervals::dragOnEmptyLaneCreates()
{
    QCPLaneLayout layout;
    auto* iv = editableTwoBars(mPlot, &layout, QCPIntervals::emCreate);
    QSignalSpy spy(iv, &QCPIntervals::intervalCreated);
    drag(mPlot, iv->pixelOf(55, 0), iv->pixelOf(45, 0));
    QCOMPARE(spy.count(), 1);
    QVERIFY(qAbs(spy.at(0).at(0).toDouble() - 45) < keyTolerance);
    QVERIFY(qAbs(spy.at(0).at(1).toDouble() - 55) < keyTolerance);
    QCOMPARE(spy.at(0).at(2).toInt(), 0);
}

void TestIntervals::notEditableIgnoresDrags()
{
    QCPLaneLayout layout;
    auto* iv = laidOutTwoBars(mPlot, &layout);
    QSignalSpy spy(iv, &QCPIntervals::intervalsEdited);
    drag(mPlot, iv->pixelOf(25, 0), iv->pixelOf(35, 0));
    QCOMPARE(spy.count(), 0);
    QVERIFY(!iv->gestureActive());
}

// External times (orbit events) are the only candidates: the neighbour's edge at 60 is ignored.
void TestIntervals::snapToTimesLandsExactly()
{
    QCPLaneLayout layout;
    auto* iv = editableTwoBars(mPlot, &layout, QCPIntervals::emMove);
    iv->setSnapTimes({ 62.0, 5.0 });
    QCOMPARE(iv->snapMode(), QCPIntervals::snTimes);
    QSignalSpy spy(iv, &QCPIntervals::intervalsEdited);
    drag(mPlot, iv->pixelOf(25, 0), iv->pixelOf(46, 0)); // stop 40 -> ~61
    QCOMPARE(spy.at(0).at(0).value<QVector<QCPIntervalEdit>>()[0].stop, 62.0);
}

void TestIntervals::snapToEdgesLandsExactly()
{
    QCPLaneLayout layout;
    auto* iv = editableTwoBars(mPlot, &layout, QCPIntervals::emMove);
    iv->setSnap(QCPIntervals::snEdges);
    QSignalSpy spy(iv, &QCPIntervals::intervalsEdited);
    drag(mPlot, iv->pixelOf(25, 0), iv->pixelOf(44, 0)); // stop 40 -> ~59, bar 1 starts at 60
    QCOMPARE(spy.at(0).at(0).value<QVector<QCPIntervalEdit>>()[0].stop, 60.0);
}

void TestIntervals::snapToStepLandsOnMultiples()
{
    QCPLaneLayout layout;
    auto* iv = editableTwoBars(mPlot, &layout, QCPIntervals::emMove);
    iv->setSnap(QCPIntervals::snStep, 5);
    QSignalSpy spy(iv, &QCPIntervals::intervalsEdited);
    drag(mPlot, iv->pixelOf(25, 0), iv->pixelOf(32.3, 0)); // start 10 -> ~17.3 -> 15
    QCOMPARE(spy.at(0).at(0).value<QVector<QCPIntervalEdit>>()[0].start, 15.0);
}

void TestIntervals::setDataDuringDragCancelsGesture()
{
    QCPLaneLayout layout;
    auto* iv = editableTwoBars(mPlot, &layout, QCPIntervals::emMove);
    QSignalSpy spy(iv, &QCPIntervals::intervalsEdited);
    press(mPlot, iv->pixelOf(25, 0).toPoint());
    moveTo(mPlot, iv->pixelOf(35, 0).toPoint());
    iv->setData(columns({ 0 }, { 1 }, { 0 }));
    release(mPlot, iv->pixelOf(35, 0).toPoint());
    QCOMPARE(spy.count(), 0);
    QVERIFY(!iv->gestureActive());
}

void TestIntervals::cursorFollowsThePart()
{
    QCPLaneLayout layout;
    auto* iv = editableTwoBars(mPlot, &layout,
                               QCPIntervals::emMove | QCPIntervals::emResize | QCPIntervals::emCreate);
    QVERIFY(iv->cursorAt(iv->pixelOf(25, 0)) == Qt::SizeAllCursor);
    QVERIFY(iv->cursorAt(iv->pixelOf(10, 0) + QPointF(2, 0)) == Qt::SizeHorCursor);
    QVERIFY(iv->cursorAt(iv->pixelOf(50, 0)) == Qt::CrossCursor);
    iv->setEditable(false);
    QVERIFY(!iv->cursorAt(iv->pixelOf(25, 0)));
}

namespace {
void key(QWidget* w, Qt::Key k)
{
    QKeyEvent press(QEvent::KeyPress, k, Qt::NoModifier);
    QApplication::sendEvent(w, &press);
}
} // namespace

void TestIntervals::deleteKeyRequestsTheSelectedIds()
{
    qRegisterMetaType<QVector<qint64>>();
    QCPLaneLayout layout;
    auto* iv = editableTwoBars(mPlot, &layout, QCPIntervals::emDelete);
    iv->setSelectedRows({ 1 });
    QSignalSpy spy(iv, &QCPIntervals::deleteRequested);
    key(mPlot, Qt::Key_Delete);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<QVector<qint64>>(), QVector<qint64>({ 101 }));
}

void TestIntervals::arrowsNudgeBySnapStepOrOnePixel()
{
    QCPLaneLayout layout;
    auto* iv = editableTwoBars(mPlot, &layout, QCPIntervals::emMove);
    iv->setSelectedRows({ 0 });
    QSignalSpy spy(iv, &QCPIntervals::intervalsEdited);
    iv->setSnap(QCPIntervals::snStep, 5);
    key(mPlot, Qt::Key_Right);
    QCOMPARE(spy.at(0).at(0).value<QVector<QCPIntervalEdit>>()[0].start, 15.0);
    iv->setSnap(QCPIntervals::snNone);
    key(mPlot, Qt::Key_Left);
    const double start = spy.at(1).at(0).value<QVector<QCPIntervalEdit>>()[0].start;
    QVERIFY(start < 10.0 && start > 9.0);
}

void TestIntervals::upDownChangeLaneWhenAllowed()
{
    QCPLaneLayout layout;
    auto* iv = editableTwoBars(mPlot, &layout, QCPIntervals::emChangeLane);
    iv->setSelectedRows({ 0 });
    QSignalSpy spy(iv, &QCPIntervals::intervalsEdited);
    key(mPlot, Qt::Key_Down);
    QCOMPARE(spy.at(0).at(0).value<QVector<QCPIntervalEdit>>()[0].lane, 1);
    key(mPlot, Qt::Key_Right); // emMove is off
    QCOMPARE(spy.count(), 1);
}

void TestIntervals::escapeCancelsTheGesture()
{
    QCPLaneLayout layout;
    auto* iv = editableTwoBars(mPlot, &layout, QCPIntervals::emMove);
    QSignalSpy spy(iv, &QCPIntervals::intervalsEdited);
    press(mPlot, iv->pixelOf(25, 0).toPoint());
    moveTo(mPlot, iv->pixelOf(35, 0).toPoint());
    key(mPlot, Qt::Key_Escape);
    release(mPlot, iv->pixelOf(35, 0).toPoint());
    QCOMPARE(spy.count(), 0);
}

void TestIntervals::keysAreNotConsumedWhenNotEditable()
{
    QCPLaneLayout layout;
    auto* iv = laidOutTwoBars(mPlot, &layout);
    iv->setSelectedRows({ 0 });
    QKeyEvent e(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    QVERIFY(!iv->keyPress(&e));
}

void TestIntervals::setDataDropsSelectionOfVanishedIds()
{
    QCPLaneLayout layout;
    auto* iv = laidOutTwoBars(mPlot, &layout);
    iv->setSelectedRows({ 1 });
    iv->setData(columns({ 10 }, { 40 }, { 0 }));
    QVERIFY(iv->selectedRows().isEmpty());
    QVERIFY(iv->selectedIds().isEmpty());
    mPlot->replot(); // must not read the vanished row
}

void TestIntervals::setDataSelectionFollowsTheId()
{
    QCPLaneLayout layout;
    auto* iv = laidOutTwoBars(mPlot, &layout);
    iv->setSelectedRows({ 1 }); // id 101
    auto c = columns({ 60, 10 }, { 90, 40 }, { 1, 0 });
    c.ids = { 101, 100 };
    iv->setData(std::move(c));
    QCOMPARE(iv->selectedRows(), QVector<int>({ 0 }));
    QCOMPARE(iv->selectedIds(), QVector<qint64>({ 101 }));
}

namespace {
//! A second timeline on the same layout with one bar at [60,90] on lane A.
QCPIntervals* siblingOnLaneA(QCustomPlot* plot, QCPLaneLayout* layout)
{
    auto* iv = new QCPIntervals(plot->xAxis, plot->yAxis, layout);
    iv->setData(columns({ 60 }, { 90 }, { layout->laneIndex("A") }));
    return iv;
}
} // namespace

void TestIntervals::clickReachesABarUnderASiblingsEmptySpace()
{
    QCPLaneLayout layout;
    auto* a = laidOutTwoBars(mPlot, &layout);
    auto* b = siblingOnLaneA(mPlot, &layout);
    mPlot->replot();
    click(mPlot, a->pixelOf(25, 0).toPoint());
    QCOMPARE(a->selectedRows(), QVector<int>({ 0 }));
    QVERIFY(b->selectedRows().isEmpty());
}

void TestIntervals::plottableAtPrefersABarOverASiblingsEmptySpace()
{
    QCPLaneLayout layout;
    auto* b = siblingOnLaneA(mPlot, &layout);
    auto* a = laidOutTwoBars(mPlot, &layout);
    QCOMPARE(mPlot->plottableAt(a->pixelOf(25, 0)), a);
    QCOMPARE(mPlot->plottableAt(a->pixelOf(75, 0)), b);
}

void TestIntervals::pressOnASiblingsBarStartsNoCreate()
{
    QCPLaneLayout layout;
    auto* a = laidOutTwoBars(mPlot, &layout);
    auto* b = siblingOnLaneA(mPlot, &layout);
    b->setEditable(true);
    b->setEditModes(QCPIntervals::emCreate);
    mPlot->replot();
    QSignalSpy spy(b, &QCPIntervals::intervalCreated);
    drag(mPlot, a->pixelOf(25, 0), a->pixelOf(35, 0));
    QCOMPARE(spy.count(), 0);
}

void TestIntervals::verticalOnlyCreateEmitsNothing()
{
    QCPLaneLayout layout;
    auto* iv = editableTwoBars(mPlot, &layout, QCPIntervals::emCreate);
    QSignalSpy spy(iv, &QCPIntervals::intervalCreated);
    drag(mPlot, iv->pixelOf(50, 0), iv->pixelOf(50, 1));
    QCOMPARE(spy.count(), 0);
}

void TestIntervals::dragBackToTheStartEmitsNothing()
{
    QCPLaneLayout layout;
    auto* iv = editableTwoBars(mPlot, &layout, QCPIntervals::emMove);
    QSignalSpy spy(iv, &QCPIntervals::intervalsEdited);
    press(mPlot, iv->pixelOf(25, 0).toPoint());
    moveTo(mPlot, iv->pixelOf(35, 0).toPoint());
    moveTo(mPlot, iv->pixelOf(25, 0).toPoint());
    release(mPlot, iv->pixelOf(25, 0).toPoint());
    QCOMPARE(spy.count(), 0);
}

void TestIntervals::nudgeClampedAtTheTopLaneEmitsNothing()
{
    QCPLaneLayout layout;
    auto* iv = editableTwoBars(mPlot, &layout, QCPIntervals::emChangeLane);
    iv->setSelectedRows({ 0 });
    QSignalSpy spy(iv, &QCPIntervals::intervalsEdited);
    key(mPlot, Qt::Key_Up);
    QCOMPARE(spy.count(), 0);
}

void TestIntervals::deleteReachesEveryEditableTimeline()
{
    qRegisterMetaType<QVector<qint64>>();
    QCPLaneLayout layout;
    auto* a = editableTwoBars(mPlot, &layout, QCPIntervals::emDelete);
    auto* b = siblingOnLaneA(mPlot, &layout);
    b->setEditable(true);
    b->setEditModes(QCPIntervals::emDelete);
    a->setSelectedRows({ 0 });
    b->setSelectedRows({ 0 });
    QSignalSpy spyA(a, &QCPIntervals::deleteRequested);
    QSignalSpy spyB(b, &QCPIntervals::deleteRequested);
    key(mPlot, Qt::Key_Delete);
    QCOMPARE(spyA.count(), 1);
    QCOMPARE(spyB.count(), 1);
}

void TestIntervals::hiddenTimelineDoesNotConsumeKeys()
{
    QCPLaneLayout layout;
    auto* iv = editableTwoBars(mPlot, &layout, QCPIntervals::emDelete);
    iv->setSelectedRows({ 0 });
    iv->setVisible(false);
    QKeyEvent e(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    QVERIFY(!iv->keyPress(&e));
}

void TestIntervals::createWithStepSnapsBothEnds()
{
    QCPLaneLayout layout;
    auto* iv = editableTwoBars(mPlot, &layout, QCPIntervals::emCreate);
    iv->setSnap(QCPIntervals::snStep, 5);
    QSignalSpy spy(iv, &QCPIntervals::intervalCreated);
    drag(mPlot, iv->pixelOf(52.3, 0), iv->pixelOf(43.1, 0));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toDouble(), 45.0);
    QCOMPARE(spy.at(0).at(1).toDouble(), 50.0);
}

void TestIntervals::firstVisibleTimelineDrawsLaneNames()
{
    QCPLaneLayout layout;
    auto* first = twoBars(mPlot, &layout);
    auto* second = new QCPIntervals(mPlot->xAxis, mPlot->yAxis, &layout);
    first->setVisible(false);
    QVERIFY(!first->drawsLaneNames());
    QVERIFY(second->drawsLaneNames());
}

void TestIntervals::laneIndicesNotifiesOnce()
{
    QCPLaneLayout layout;
    layout.laneIndex("B");
    QSignalSpy spy(&layout, &QCPLaneLayout::changed);
    QCOMPARE(layout.laneIndices({ "A", "B", "C", "A" }), QVector<int>({ 1, 0, 2, 1 }));
    QCOMPARE(spy.count(), 1);
    layout.laneIndices({ "C", "B" });
    QCOMPARE(spy.count(), 1); // nothing new
}

void TestIntervals::labelCentresInTheVisiblePart()
{
    QCPLaneLayout layout;
    layout.setPlacement(QCPLaneLayout::plLanes);
    auto* iv = new QCPIntervals(mPlot->xAxis, mPlot->yAxis, &layout);
    auto c = columns({ -400 }, { 60 }, { layout.laneIndex("A") });
    c.labels = QStringList { "long interval" };
    iv->setData(std::move(c));
    mPlot->xAxis->setRange(0, 100);
    mPlot->toPixmap(400, 300);
    QCOMPARE(iv->mLabelRects.size(), std::size_t { 1 });
    const QRectF label = iv->mLabelRects[0].rect;
    const double visibleCentre = (mPlot->axisRect()->rect().left() + mPlot->xAxis->coordToPixel(60)) / 2;
    QVERIFY2(qAbs(label.center().x() - visibleCentre) < 2, "label is not centred in the visible part");
}

namespace {
//! One red bar on lane A (keys 20..80) and an empty lane B, drawn with the painter.
QImage waveImage(QCustomPlot* plot, QCPLaneLayout* layout, QCPIntervals::Style style)
{
    layout->setPlacement(QCPLaneLayout::plLanes);
    auto* iv = new QCPIntervals(plot->xAxis, plot->yAxis, layout);
    iv->setStyle(style);
    iv->setCategoryColors({ Qt::red });
    iv->setData(columns({ 20 }, { 80 }, { layout->laneIndex("A") }));
    layout->laneIndex("B");
    plot->xAxis->setRange(0, 100);
    return plot->toPixmap(400, 300).toImage();
}

QColor background(QCustomPlot* plot) { return plot->backgroundBrush().color(); }
} // namespace

void TestIntervals::waveStyleCutsTheBarCorners()
{
    QCPLaneLayout layout;
    const QImage bars = waveImage(mPlot, &layout, QCPIntervals::stBars);
    const double lane = layout.lanePixelHeight(mPlot->axisRect()->rect());
    QVERIFY(isRedish(pixelAt(bars, mPlot, 20.2, 3)));          // square corner
    mPlot->removePlottable(mPlot->plottable(0)); // not delete: the plot keeps a pointer to it
    const QImage wave = waveImage(mPlot, &layout, QCPIntervals::stWave);
    QVERIFY(!isRedish(pixelAt(wave, mPlot, 20.2, 3)));         // cut corner
    QVERIFY(isRedish(pixelAt(wave, mPlot, 50, lane / 2)));     // body
}

void TestIntervals::waveStyleDrawsAnIdleBaseline()
{
    QCPLaneLayout layout;
    const QImage wave = waveImage(mPlot, &layout, QCPIntervals::stWave);
    const double lane = layout.lanePixelHeight(mPlot->axisRect()->rect());
    QVERIFY(pixelAt(wave, mPlot, 10, lane / 2) != background(mPlot));    // idle line before the bar
    QCOMPARE(pixelAt(wave, mPlot, 10, lane / 2 - 4), pixelAt(wave, mPlot, 10, 3)); // only a thin line
    mPlot->removePlottable(mPlot->plottable(0)); // not delete: the plot keeps a pointer to it
    const QImage bars = waveImage(mPlot, &layout, QCPIntervals::stBars);
    QCOMPARE(pixelAt(bars, mPlot, 10, lane / 2), pixelAt(bars, mPlot, 10, 3));    // no line with bars
}

void TestIntervals::waveStyleShadesEveryOtherLane()
{
    QCPLaneLayout layout;
    const QImage wave = waveImage(mPlot, &layout, QCPIntervals::stWave);
    const double lane = layout.lanePixelHeight(mPlot->axisRect()->rect());
    QVERIFY(pixelAt(wave, mPlot, 10, 3) != pixelAt(wave, mPlot, 10, lane + 3));
}

void TestIntervals::freeShiftRangeStaysInTheGap()
{
    using qcp::intervals::freeShiftRange;
    const std::vector<qcp::intervals::Span> obstacles { { 0, 10 }, { 50, 70 } };
    const auto range = freeShiftRange(20, 30, obstacles);
    QVERIFY(range);
    QCOMPARE(range->first, -10.0);  // start may go back to 10
    QCOMPARE(range->second, 20.0);  // stop may go up to 50
    QVERIFY(!freeShiftRange(5, 20, obstacles));  // already overlapping: no constraint
    const auto open = freeShiftRange(80, 90, obstacles);
    QVERIFY(open && open->first == -10.0 && std::isinf(open->second));
}

namespace {
//! Lane A: [10, 30] and [50, 70]. Lane B: [40, 60]. Every edit mode, overlaps forbidden.
QCPIntervals* forbiddingBars(QCustomPlot* plot, QCPLaneLayout* layout,
                             QCPIntervals::OverlapMode mode = QCPIntervals::omForbid)
{
    layout->setPlacement(QCPLaneLayout::plLanes);
    auto* iv = new QCPIntervals(plot->xAxis, plot->yAxis, layout);
    const int a = layout->laneIndex("A"), b = layout->laneIndex("B");
    iv->setData(columns({ 10, 50, 40 }, { 30, 70, 60 }, { a, a, b }));
    iv->setEditable(true);
    iv->setEditModes(QCPIntervals::EditModes(QCPIntervals::emMove | QCPIntervals::emResize
                                             | QCPIntervals::emChangeLane | QCPIntervals::emCreate));
    iv->setOverlapMode(mode);
    plot->setInteractions(QCP::iSelectPlottables | QCP::iMultiSelect);
    plot->xAxis->setRange(0, 100);
    plot->replot();
    return iv;
}

QCPIntervalEdit firstEdit(const QSignalSpy& spy)
{
    return spy.at(0).at(0).value<QVector<QCPIntervalEdit>>()[0];
}
} // namespace

void TestIntervals::forbidStopsAMoveAtTheNeighbour()
{
    QCPLaneLayout layout;
    auto* iv = forbiddingBars(mPlot, &layout);
    QSignalSpy spy(iv, &QCPIntervals::intervalsEdited);
    drag(mPlot, iv->pixelOf(20, 0), iv->pixelOf(50, 0)); // +30 would reach [40, 60]
    QCOMPARE(firstEdit(spy).start, 30.0);
    QCOMPARE(firstEdit(spy).stop, 50.0);
}

void TestIntervals::forbidStopsAResizeAtTheNeighbour()
{
    QCPLaneLayout layout;
    auto* iv = forbiddingBars(mPlot, &layout);
    QSignalSpy spy(iv, &QCPIntervals::intervalsEdited);
    drag(mPlot, iv->pixelOf(30, 0) - QPointF(2, 0), iv->pixelOf(65, 0));
    QCOMPARE(firstEdit(spy).stop, 50.0);
    drag(mPlot, iv->pixelOf(50, 0) + QPointF(2, 0), iv->pixelOf(15, 0));
    QCOMPARE(spy.at(1).at(0).value<QVector<QCPIntervalEdit>>()[0].start, 30.0);
}

void TestIntervals::forbidStopsACreateAtTheNeighbour()
{
    QCPLaneLayout layout;
    auto* iv = forbiddingBars(mPlot, &layout);
    QSignalSpy spy(iv, &QCPIntervals::intervalCreated);
    drag(mPlot, iv->pixelOf(40, 0), iv->pixelOf(85, 0));
    QCOMPARE(spy.size(), 1);
    QVERIFY(qAbs(spy.at(0).at(0).toDouble() - 40) < keyTolerance);
    QCOMPARE(spy.at(0).at(1).toDouble(), 50.0);
}

void TestIntervals::forbidKeepsTheLaneWhenTheTargetIsTaken()
{
    QCPLaneLayout layout;
    auto* iv = forbiddingBars(mPlot, &layout);
    QSignalSpy spy(iv, &QCPIntervals::intervalsEdited);
    drag(mPlot, iv->pixelOf(20, 0), iv->pixelOf(45, 1)); // [35, 55] in B overlaps [40, 60]
    QCOMPARE(firstEdit(spy).lane, 0);
    QCOMPARE(firstEdit(spy).stop, 50.0);
}

void TestIntervals::forbidClampsASelectionByItsTightestBlock()
{
    QCPLaneLayout layout;
    auto* iv = forbiddingBars(mPlot, &layout);
    iv->setSelectedRows({ 0, 2 }); // [10, 30] in A, [40, 60] in B: B is free to the right
    QSignalSpy spy(iv, &QCPIntervals::intervalsEdited);
    drag(mPlot, iv->pixelOf(20, 0), iv->pixelOf(45, 0)); // +25: A's block hits 50 after +20
    const auto edits = spy.at(0).at(0).value<QVector<QCPIntervalEdit>>();
    QCOMPARE(edits.size(), 2);
    for (const auto& e : edits)
        QCOMPARE(e.stop - (e.id == 100 ? 30.0 : 60.0), 20.0);
}

void TestIntervals::forbidClampsANudge()
{
    QCPLaneLayout layout;
    auto* iv = forbiddingBars(mPlot, &layout);
    iv->setSnap(QCPIntervals::snStep, 50);
    iv->setSelectedRows({ 0 });
    QSignalSpy spy(iv, &QCPIntervals::intervalsEdited);
    QVERIFY(iv->nudge(1, 0)); // +50 would land on [50, 70]
    QCOMPARE(firstEdit(spy).stop, 50.0);
}

void TestIntervals::drawModeAllowsOverlaps()
{
    QCPLaneLayout layout;
    auto* iv = forbiddingBars(mPlot, &layout, QCPIntervals::omDraw);
    QSignalSpy spy(iv, &QCPIntervals::intervalsEdited);
    drag(mPlot, iv->pixelOf(20, 0), iv->pixelOf(50, 0));
    QVERIFY(qAbs(firstEdit(spy).stop - 60) < keyTolerance);
}

