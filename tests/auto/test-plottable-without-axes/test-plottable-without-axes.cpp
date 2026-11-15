#include "test-plottable-without-axes.h"
#include "../../../src/qcp.h"
#include <array>
#include <cmath>
#include <memory>
#include <numeric>
#include <ranges>
#include <vector>

namespace
{
int gMisuses = 0;

void countMisuse(const char*, const char*)
{
    ++gMisuses;
}

//! A second area under the main one: it has its own axes, and so its own systems.
QCPAxisRect* addSecondRect(QCustomPlot* plot)
{
    auto* rect = new QCPAxisRect(plot);
    plot->plotLayout()->addElement(1, 0, rect);
    plot->replot();
    return rect;
}

qcp::intervals::Columns columns(std::vector<double> start, std::vector<double> stop)
{
    qcp::intervals::Columns c;
    c.start = std::move(start);
    c.stop = std::move(stop);
    c.category.assign(c.start.size(), 0);
    c.lane.assign(c.start.size(), 0);
    c.ids.resize(c.start.size());
    std::iota(c.ids.begin(), c.ids.end(), qint64 { 100 });
    return c;
}

//! A user plottable that inherits the one-dimensional data constructor and adds one of its own.
class SortedGraph : public QCPPlottable1D<QCPGraphData>
{
public:
    using QCPPlottable1D<QCPGraphData>::QCPPlottable1D;

    explicit SortedGraph(int marker) : mMarker(marker) { }

    void draw(QCPDrawContext&) const override { }

    int mMarker = 0;
};

//! Counts its instances, so a refused add can prove the object is gone.
class CountedPlottable : public QCPPlottable
{
public:
    CountedPlottable() : QCPPlottable(QCP::capRectangular) { ++live; }

    ~CountedPlottable() override { --live; }

    void draw(QCPDrawContext&) const override { }

    static int live;
};

int CountedPlottable::live = 0;
} // namespace

void TestPlottableWithoutAxes::init()
{
    gMisuses = 0;
    mMisuseHandler = qcp::setMisuseHandler(countMisuse);
    mPlot = new QCustomPlot(nullptr);
    mPlot->resize(640, 480);
    mPlot->show();
    mPlot->xAxis->setRange(0, 10);
    mPlot->yAxis->setRange(0, 10);
    mPlot->replot();
}

void TestPlottableWithoutAxes::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
    qcp::setMisuseHandler(mMisuseHandler);
}

void TestPlottableWithoutAxes::aGraphTakesItsDataInTheConstructor()
{
    const auto rect = mPlot->axisRect();
    const std::vector<double> keys { 3, 1, 2 };
    const std::vector<double> values { 30, 10, 20 };

    const auto fromStdVector = rect->add<QCPGraph>(keys, values);
    QVERIFY(fromStdVector);
    QCOMPARE(fromStdVector->dataCount(), 3);
    QCOMPARE(fromStdVector->dataMainKey(0), 1.0); // sorted by key
    QCOMPARE(fromStdVector->dataMainValue(0), 10.0);
    QVERIFY(fromStdVector->system() == rect->system(rect->xAxis(), rect->yAxis()));

    const auto fromQVector
        = rect->add<QCPGraph>(QVector<double> { 1, 2 }, QVector<double> { 3, 4 });
    QVERIFY(fromQVector);
    QCOMPARE(fromQVector->dataCount(), 2);

    const auto fromArray
        = rect->add<QCPGraph>(std::array<float, 3> { 0, 1, 2 }, std::array<int, 3> { 5, 6, 7 });
    QVERIFY(fromArray);
    QCOMPARE(fromArray->dataCount(), 3);
    QCOMPARE(fromArray->dataMainValue(2), 7.0);

    const auto fromViews = rect->add<QCPGraph>(std::views::iota(0, 4), std::views::iota(10, 14));
    QVERIFY(fromViews);
    QCOMPARE(fromViews->dataCount(), 4);
    QCOMPARE(fromViews->dataMainKey(3), 3.0);

    QCOMPARE(gMisuses, 0);
}

void TestPlottableWithoutAxes::everyBuiltInPlottableTakesItsData()
{
    const auto rect = mPlot->axisRect();
    const std::vector<double> keys { 1, 2 };
    const std::vector<double> values { 3, 4 };

    const auto bars = rect->add<QCPBars>(keys, values);
    QVERIFY(bars);
    QCOMPARE(bars->dataCount(), 2);

    const auto curve = rect->add<QCPCurve>(keys, keys, values);
    QVERIFY(curve);
    QCOMPARE(curve->dataCount(), 2);

    const auto financial = rect->add<QCPFinancial>(keys, values, values, values, values);
    QVERIFY(financial);
    QCOMPARE(financial->dataCount(), 2);

    const auto statisticalBox
        = rect->add<QCPStatisticalBox>(keys, values, values, values, values, values);
    QVERIFY(statisticalBox);
    QCOMPARE(statisticalBox->dataCount(), 2);

    const auto errorBars = rect->add<QCPErrorBars>(std::vector<double> { 0.1, 0.2 });
    QVERIFY(errorBars);
    QCOMPARE(errorBars->dataCount(), 2);

    const auto asymmetricErrorBars = rect->add<QCPErrorBars>(std::vector<double> { 0.1, 0.2 },
                                                             std::vector<double> { 0.3, 0.4 });
    QVERIFY(asymmetricErrorBars);
    QCOMPARE(asymmetricErrorBars->dataCount(), 2);

    QCPColorMapData mapData(4, 5, QCPRange(0, 1), QCPRange(0, 1));
    const auto colorMap = rect->add<QCPColorMap>(&mapData, true);
    QVERIFY(colorMap);
    QCOMPARE(colorMap->data()->keySize(), 4);
    QCOMPARE(colorMap->data()->valueSize(), 5);

    const auto colorMap2 = rect->add<QCPColorMap2>(
        std::vector<double> { 0, 1 }, std::vector<double> { 0, 1 }, std::vector<double> { 2, 3 });
    QVERIFY(colorMap2);

    const auto histogram2D = rect->add<QCPHistogram2D>(keys, values);
    QVERIFY(histogram2D);

    const auto histogram = rect->add<QCPHistogram>();
    QVERIFY(histogram);

    QCPLaneLayout lanes;
    lanes.laneIndices({ QStringLiteral("A") });
    const auto intervals = rect->add<QCPIntervals>(columns({ 0, 1 }, { 1, 2 }), &lanes);
    QVERIFY(intervals);
    QCOMPARE(intervals->rowCount(), 2);

    const auto waterfall = rect->add<QCPWaterfallGraph>(keys, values);
    QVERIFY(waterfall);
    QCOMPARE(waterfall->dataCount(), 2);

    for (QCPPlottable* plottable :
         { static_cast<QCPPlottable*>(bars.get()), static_cast<QCPPlottable*>(curve.get()),
           static_cast<QCPPlottable*>(financial.get()),
           static_cast<QCPPlottable*>(statisticalBox.get()),
           static_cast<QCPPlottable*>(errorBars.get()), static_cast<QCPPlottable*>(colorMap.get()),
           static_cast<QCPPlottable*>(colorMap2.get()),
           static_cast<QCPPlottable*>(histogram2D.get()),
           static_cast<QCPPlottable*>(histogram.get()), static_cast<QCPPlottable*>(intervals.get()),
           static_cast<QCPPlottable*>(waterfall.get()) })
    {
        QVERIFY(plottable->isRegistered());
        QVERIFY(plottable->system() == rect->system(rect->xAxis(), rect->yAxis()));
    }
    QCOMPARE(gMisuses, 0);
}

void TestPlottableWithoutAxes::aUserTypeInheritsTheDataConstructor()
{
    const auto rect = mPlot->axisRect();

    const auto sorted = rect->add<SortedGraph>(std::vector<double> { 3, 1, 2 },
                                               std::vector<double> { 30, 10, 20 });
    QVERIFY(sorted);
    QCOMPARE(sorted->dataCount(), 3);
    QCOMPARE(sorted->dataMainKey(0), 1.0);
    QCOMPARE(sorted->dataMainKey(2), 3.0);

    const auto marked = rect->add<SortedGraph>(7);
    QVERIFY(marked);
    QCOMPARE(marked->mMarker, 7);
    QCOMPARE(marked->dataCount(), 0);

    QCOMPARE(gMisuses, 0);
}

void TestPlottableWithoutAxes::anAddWithoutBothAxesRefusesThePlottable()
{
    QCPAxisRect* rect = addSecondRect(mPlot);
    QCPAxis* bottom = rect->xAxis();
    QVERIFY(rect->removeAxis(bottom));
    gMisuses = 0;
    const int plottables = mPlot->plottableCount();

    const auto refused = rect->add<QCPGraph>();
    QVERIFY(!refused);
    QCOMPARE(gMisuses, 1);
    QCOMPARE(mPlot->plottableCount(), plottables);

    const auto stillRefused
        = rect->add<QCPGraph>(std::vector<double> { 1 }, std::vector<double> { 2 });
    QVERIFY(!stillRefused);
    QCOMPARE(gMisuses, 2);
    QCOMPARE(mPlot->plottableCount(), plottables);
}

void TestPlottableWithoutAxes::aRefusedPlottableIsReportedOnceAndDeleted()
{
    auto* area = new QCPPolarArea(mPlot);
    mPlot->plotLayout()->addElement(0, 1, area);
    const int plottables = mPlot->plottableCount();
    CountedPlottable::live = 0;

    const auto refused = area->add<CountedPlottable>();
    QVERIFY(!refused);
    QCOMPARE(gMisuses, 1);
    QCOMPARE(CountedPlottable::live, 0);
    QCOMPARE(mPlot->plottableCount(), plottables);

    const auto graph = area->add<QCPGraph>();
    QVERIFY(!graph);
    QCOMPARE(gMisuses, 2);
    QCOMPARE(mPlot->plottableCount(), plottables);
}

void TestPlottableWithoutAxes::aRectAddBindsTheGivenAxisPair()
{
    QCPAxisRect* rect = addSecondRect(mPlot);
    QCPAxis* top = rect->axis(QCPAxis::atTop);
    QCPAxis* right = rect->axis(QCPAxis::atRight);
    QVERIFY(top);
    QVERIFY(right);

    const auto graph = rect->add<QCPGraph>(top, right, std::vector<double> { 1, 2 },
                                           std::vector<double> { 3, 4 });
    QVERIFY(graph);
    QCOMPARE(graph->dataCount(), 2);
    QVERIFY(graph->system() == rect->system(top, right));
    QCOMPARE(gMisuses, 0);

    // A pair the rect does not own is refused, once, and places nothing.
    const auto elsewhere = rect->add<QCPGraph>(mPlot->xAxis, mPlot->yAxis);
    QVERIFY(!elsewhere);
    QCOMPARE(gMisuses, 1);
}

void TestPlottableWithoutAxes::plotAddBindsTheFirstRectsDefaultPair()
{
    QCPAxisRect* rect = addSecondRect(mPlot);

    const auto graph
        = mPlot->add<QCPGraph>(std::vector<double> { 1, 2 }, std::vector<double> { 3, 4 });
    QVERIFY(graph);
    QCOMPARE(graph->dataCount(), 2);
    QVERIFY(graph->system() == mPlot->axisRect()->system(mPlot->xAxis, mPlot->yAxis));
    QVERIFY(graph->system() != rect->system(rect->xAxis(), rect->yAxis()));
    QCOMPARE(gMisuses, 0);
}

// The L1 resample of a big graph runs on the plot's pool, never on the thread that builds it.
void TestPlottableWithoutAxes::aDeferredTransformWaitsForItsPlot()
{
    const int n = 120'000; // above the resample threshold: the graph would ask for an L1 pass
    std::vector<double> keys(n), values(n), heights(n);
    for (int i = 0; i < n; ++i)
    {
        keys[i] = 10.0 * i / (n - 1);
        values[i] = std::sin(0.01 * i);
        heights[i] = 0.5 + 0.5 * std::cos(0.02 * i);
    }

    // Built detached, a data constructor only keeps the source: running the transform there would
    // block this thread for a whole pass, and a data constructor must not.
    auto graph = std::make_unique<QCPGraph>(keys, values);
    QVERIFY(graph->pipeline().waitsForScheduler());
    QCOMPARE(graph->pipeline().scheduler(), nullptr);
    QCOMPARE(graph->dataCount(), n);

    const auto drawn = mPlot->add(std::move(graph));
    QVERIFY(drawn);
    QVERIFY(!drawn->pipeline().waitsForScheduler());
    QTRY_VERIFY_WITH_TIMEOUT(!drawn->pipeline().isBusy(), 30000);
    mPlot->replot();
    QVERIFY(drawn->drawnDataRange().has_value());
    QCOMPARE(gMisuses, 0);

    // The two plottables whose whole picture comes out of a pipeline defer the same way.
    auto map = std::make_unique<QCPColorMap2>(keys, values, heights);
    QVERIFY(map->pipeline().waitsForScheduler());
    const auto placedMap = mPlot->add(std::move(map));
    QVERIFY(placedMap);
    QVERIFY(!placedMap->pipeline().waitsForScheduler());
    QTRY_VERIFY_WITH_TIMEOUT(!placedMap->pipeline().isBusy(), 30000);

    auto histogram = std::make_unique<QCPHistogram2D>(keys, values);
    QVERIFY(histogram->pipeline().waitsForScheduler());
    const auto placedHistogram = mPlot->add(std::move(histogram));
    QVERIFY(placedHistogram);
    QVERIFY(!placedHistogram->pipeline().waitsForScheduler());
    QTRY_VERIFY_WITH_TIMEOUT(!placedHistogram->pipeline().isBusy(), 30000);

    mPlot->replot();
    QCOMPARE(gMisuses, 0);
}

// A taken plottable asks for nothing while it is out, and the add that takes it back starts what
// it needs: its data is the data it had.
void TestPlottableWithoutAxes::aTakenPlottableResumesWhenItIsAddedBack()
{
    const std::vector<double> keys { 0, 1, 2 }, values { 3, 2, 1 };
    const auto graph = mPlot->add<QCPGraph>(keys, values);
    QVERIFY(graph);
    mPlot->replot();

    auto taken = mPlot->take(graph);
    QVERIFY(taken);
    QCOMPARE(taken->parentPlot(), nullptr);
    QCOMPARE(mPlot->graphCount(), 0);

    const auto readded = mPlot->add(std::move(taken));
    QVERIFY(readded);
    QCOMPARE(readded->dataCount(), 3);
    QVERIFY(!readded->pipeline().waitsForScheduler());
    QTRY_VERIFY_WITH_TIMEOUT(!readded->pipeline().isBusy(), 10000);
    mPlot->replot();
    QVERIFY(readded->drawnDataRange().has_value());
    QCOMPARE(mPlot->graphCount(), 1);
    QCOMPARE(gMisuses, 0);
}

// An intervals added without a lane layout draws the rows of the lanes its default layout names,
// exactly as the add that built one for it did before.
void TestPlottableWithoutAxes::anIntervalsAddedWithoutALaneLayoutGetsOne()
{
    const auto rect = mPlot->axisRect();

    const auto plain = rect->add<QCPIntervals>();
    QVERIFY(plain);
    QVERIFY(plain->laneLayout());
    QCOMPARE(gMisuses, 0);

    const auto laid = rect->add<QCPIntervals>(columns({ 0, 1 }, { 1, 2 }));
    QVERIFY(laid);
    QVERIFY(laid->laneLayout());
    QCOMPARE(laid->rowCount(), 2);
    QCOMPARE(gMisuses, 0);

    // A layout the caller gave is the one it keeps.
    QCPLaneLayout lanes;
    lanes.laneIndices({ QStringLiteral("A") });
    const auto given = rect->add<QCPIntervals>(&lanes);
    QVERIFY(given);
    QVERIFY(given->laneLayout() == &lanes);
    QCOMPARE(gMisuses, 0);

    mPlot->replot();
    QCOMPARE(gMisuses, 0);
}
