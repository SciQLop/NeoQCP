#include "test-histogram-plottable.h"
#include "qcustomplot.h"
#include "plottables/plottable-histogram.h"
#include "plottables/histogram-geometry.h"
#include "painting/plottable-rhi-layer.h"
#include "datasource/soa-multi-datasource.h"
#include <QScopeGuard>
#include <QThread>
#include <atomic>
#include <numeric>

namespace {
using Src = QCPSoAMultiDataSource<std::vector<double>, std::vector<double>>;
std::shared_ptr<Src> values(std::vector<double> v)
{
    std::vector<double> keys(v.size());
    for (std::size_t i = 0; i < keys.size(); ++i) keys[i] = double(i);
    return std::make_shared<Src>(std::move(keys), std::vector<std::vector<double>>{std::move(v)});
}
// Leaves the draw as the only thing that can notice the new scale.
void setScaleTypeSilently(QCPAxis* axis, QCPAxis::ScaleType type)
{
    const QSignalBlocker noSignal(axis);
    axis->setScaleType(type);
}
bool waitForResult(QCPHistogram* h)
{
    QSignalSpy ready(h, &QCPHistogram::histogramReady);
    return ready.count() > 0 || ready.wait(5000);
}

struct Gate
{
    std::atomic<bool> open { false };
    std::atomic<bool> entered { false };
    [[nodiscard]] bool waitEntered() const
    {
        return QTest::qWaitFor([this] { return entered.load(); }, 5000);
    }
};

// Blocks every read until the gate opens, so the test controls when a binning job ends.
class GatedSource final : public QCPAbstractMultiDataSource
{
public:
    GatedSource(std::shared_ptr<Src> inner, std::shared_ptr<Gate> gate)
        : mInner(std::move(inner)), mGate(std::move(gate)) {}
    int columnCount() const override { return mInner->columnCount(); }
    int size() const override { return mInner->size(); }
    double keyAt(int i) const override { return mInner->keyAt(i); }
    QCPRange keyRange(bool& found, QCP::SignDomain sd) const override { return mInner->keyRange(found, sd); }
    int findBegin(double k, bool e) const override { return mInner->findBegin(k, e); }
    int findEnd(double k, bool e) const override { return mInner->findEnd(k, e); }
    double valueAt(int column, int i) const override
    {
        mGate->entered.store(true);
        while (!mGate->open.load()) QThread::msleep(1);
        return mInner->valueAt(column, i);
    }
    QCPRange valueRange(int column, bool& found, QCP::SignDomain sd, const QCPRange& r) const override
    {
        return mInner->valueRange(column, found, sd, r);
    }
    QVector<QPointF> getOptimizedLineData(int c, int b, int e, int px, QCPAxis* k, QCPAxis* v) const override
    {
        return mInner->getOptimizedLineData(c, b, e, px, k, v);
    }
    QVector<QPointF> getLines(int c, int b, int e, QCPAxis* k, QCPAxis* v) const override
    {
        return mInner->getLines(c, b, e, k, v);
    }

private:
    std::shared_ptr<Src> mInner;
    std::shared_ptr<Gate> mGate;
};

std::shared_ptr<GatedSource> gated(std::vector<double> v, std::shared_ptr<Gate> gate)
{
    return std::make_shared<GatedSource>(values(std::move(v)), std::move(gate));
}

//! (front, back) of every result shown, in order.
struct EdgeLog
{
    QList<QCPRange> shown;
    explicit EdgeLog(QCPHistogram* h)
    {
        QObject::connect(h, &QCPHistogram::histogramReady, h, [this, h] {
            if (h->histogram())
                shown.append(QCPRange(h->histogram()->edges.front(), h->histogram()->edges.back()));
        });
    }
};

bool settle(QCPHistogram* h)
{
    QTest::qWait(50);
    return QTest::qWaitFor([h] { return !h->pipeline().isBusy(); }, 5000);
}
}

void TestHistogramPlottable::init() { mPlot = new QCustomPlot(); mPlot->resize(400, 300); }
void TestHistogramPlottable::cleanup() { delete mPlot; mPlot = nullptr; }

void TestHistogramPlottable::binsInTheBackground()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBins(4);
    h->setDataSource(values({0, 1, 2, 3, 4}));
    QVERIFY(waitForResult(h));
    QCOMPARE(h->histogram()->heights[0].size(), std::size_t(4));
}

void TestHistogramPlottable::preBinnedNeedsNoPipeline()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBinned({0, 1, 2}, {{3, 5}});
    QVERIFY(h->histogram());
    bool found = false;
    QCOMPARE(h->getValueRange(found).upper, 5.0);
    QVERIFY(found);
}

void TestHistogramPlottable::preBinnedWithBadEdgesIsIgnored()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBinned({0, 1}, {{3, 5}});
    QVERIFY(!h->histogram());
}

void TestHistogramPlottable::autoEdgesStayWhenNewDataFitsInside()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setDataSource(values({0, 10}));
    QVERIFY(waitForResult(h));
    const auto first = h->histogram()->edges;
    QSignalSpy moved(h, &QCPHistogram::edgesChanged);
    h->setDataSource(values({2, 8}));
    QVERIFY(waitForResult(h));
    QCOMPARE(h->histogram()->edges, first);
    QCOMPARE(moved.count(), 0);
}

void TestHistogramPlottable::autoEdgesWidenForDataOutside()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setDataSource(values({0, 10}));
    QVERIFY(waitForResult(h));
    QSignalSpy moved(h, &QCPHistogram::edgesChanged);
    h->setDataSource(values({-5, 8}));
    QVERIFY(waitForResult(h));
    QCOMPARE(h->histogram()->edges.front(), -5.0);
    QCOMPARE(moved.count(), 1);
}

void TestHistogramPlottable::emptyDataAfterDataHasNoRangeAndNoCrash()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setDataSource(values({0, 10}));
    QVERIFY(waitForResult(h));
    QSignalSpy ready(h, &QCPHistogram::histogramReady);
    h->setDataSource(values({std::numeric_limits<double>::quiet_NaN()}));
    QVERIFY(ready.wait(5000));
    QVERIFY(!h->histogram());
    bool found = true;
    h->getValueRange(found);
    QVERIFY(!found);
    mPlot->rescaleAxes();
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    h->setDataSource(values({1, 2}));
    QVERIFY(waitForResult(h));
    QVERIFY(h->histogram());
}

void TestHistogramPlottable::exportBinsSynchronously()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setDataSource(values({0, 1, 2, 3}));
    const QImage img = mPlot->toPixmap(400, 300).toImage();
    QVERIFY(h->histogram()); // drawn without waiting for the pipeline
    Q_UNUSED(img);
}

void TestHistogramPlottable::logValueAxisRangeSkipsZeroHeights()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBinned({0, 1, 2, 3}, {{0, 4, 16}});
    bool found = false;
    const QCPRange r = h->getValueRange(found, QCP::sdPositive);
    QVERIFY(found);
    QCOMPARE(r.lower, 4.0);
}

void TestHistogramPlottable::allZeroHeightsHaveNoValueRange()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBinned({0, 1, 2}, {{0, 0}});
    bool found = true;
    h->getValueRange(found);
    QVERIFY(!found);
}

void TestHistogramPlottable::nonPositiveFixedRangeIsRejectedOnLogBins()
{
    mPlot->xAxis->setScaleType(QCPAxis::stLogarithmic);
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("log"));
    h->setFixedRange(QCPRange(0, 10));
    h->setDataSource(values({1, 100}));
    QVERIFY(waitForResult(h));
    QCOMPARE(h->histogram()->edges.front(), 1.0);
}

void TestHistogramPlottable::nonPositiveFixedRangeFallsBackToAutoWhenTheAxisTurnsLog()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setFixedRange(QCPRange(-1, 100));
    h->setDataSource(values({1, 100}));
    QVERIFY(waitForResult(h));
    QCOMPARE(h->histogram()->edges.front(), -1.0);
    QSignalSpy ready(h, &QCPHistogram::histogramReady);
    mPlot->xAxis->setScaleType(QCPAxis::stLogarithmic);
    QVERIFY(ready.wait(5000));
    QVERIFY(h->histogram()->log);
    QCOMPARE(h->histogram()->edges.front(), 1.0);
}

// The constructor's scaleTypeChanged link is to the first axis only: draw() notices the rest.
void TestHistogramPlottable::rebinsForTheScaleOfANewKeyAxis()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBins(3);
    h->setDataSource(values({1, 10, 100, 1000}));
    QVERIFY(waitForResult(h));
    QVERIFY(!h->histogram()->log);
    h->setKeyAxis(mPlot->xAxis2);
    mPlot->xAxis2->setScaleType(QCPAxis::stLogarithmic);
    mPlot->replot();
    QTRY_VERIFY_WITH_TIMEOUT(h->histogram() && h->histogram()->log, 5000);
    const auto& edges = h->histogram()->edges;
    QCOMPARE(edges.size(), std::size_t(4));
    QCOMPARE(edges.front(), 1.0);
    QCOMPARE(edges[1], 10.0); // log-spaced, not only flagged log
    QCOMPARE(edges[2], 100.0);
    QCOMPARE(edges.back(), 1000.0);
    QCOMPARE(h->histogram()->heights[0], (std::vector<double>{1, 1, 2}));
}

// Nothing may emit during a paint: the re-bin draw() asks for runs from the event loop.
void TestHistogramPlottable::rebinForANewScaleEmitsNothingDuringPaint()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBins(3);
    h->setDataSource(values({1, 10, 100, 1000}));
    QVERIFY(waitForResult(h));
    QVERIFY(settle(h));
    h->setKeyAxis(mPlot->xAxis2);
    mPlot->xAxis2->setScaleType(QCPAxis::stLogarithmic);
    QSignalSpy busy(&h->pipeline(), &QCPHistogram1DPipeline::busyChanged);
    mPlot->replot();
    QCOMPARE(busy.count(), 0);
    QTRY_VERIFY_WITH_TIMEOUT(h->histogram() && h->histogram()->log, 5000);
}

// No draw is asked for: only the axis signal can notice the new scale.
void TestHistogramPlottable::rebinsForANewKeyAxisScaleWithoutADraw()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBins(3);
    h->setDataSource(values({1, 10, 100, 1000}));
    QVERIFY(waitForResult(h));
    QVERIFY(settle(h));
    h->setKeyAxis(mPlot->xAxis2);
    mPlot->xAxis2->setRange(1, 1000);
    mPlot->xAxis2->setScaleType(QCPAxis::stLogarithmic);
    QTRY_VERIFY_WITH_TIMEOUT(h->histogram() && h->histogram()->log, 5000);
}

void TestHistogramPlottable::theAxisItLeftIsNoLongerListenedTo()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBins(3);
    h->setDataSource(values({1, 10, 100, 1000}));
    QVERIFY(waitForResult(h));
    QVERIFY(settle(h));
    h->setKeyAxis(mPlot->xAxis2);
    QSignalSpy busy(&h->pipeline(), &QCPHistogram1DPipeline::busyChanged);
    mPlot->xAxis->setScaleType(QCPAxis::stLogarithmic);
    QTest::qWait(100);
    QCOMPARE(busy.count(), 0);
}

// The new axis is log before the move: no scale signal re-bins, only the export can notice.
void TestHistogramPlottable::exportRightAfterAScaleChangeBinsForTheNewScale()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBins(3);
    h->setDataSource(values({1, 10, 100, 1000}));
    QVERIFY(waitForResult(h));
    QVERIFY(settle(h));
    mPlot->xAxis2->setScaleType(QCPAxis::stLogarithmic);
    h->setKeyAxis(mPlot->xAxis2);
    QVERIFY(!mPlot->toPixmap(400, 300).isNull());
    QVERIFY(h->histogram()->log);
    QCOMPARE(h->histogram()->edges.front(), 1.0);
    QCOMPARE(h->histogram()->edges.back(), 1000.0);
}

// A reset queued by a stale scale must not land once the scale is fresh again: it would drop
// auto edges that are still right.
void TestHistogramPlottable::queuedResetIsDroppedWhenTheScaleIsFreshAgain()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBins(3);
    h->setDataSource(values({1, 10, 100, 1000}));
    QVERIFY(waitForResult(h));
    QVERIFY(settle(h));
    const auto edges = h->histogram()->edges;
    h->setKeyAxis(mPlot->xAxis2);
    setScaleTypeSilently(mPlot->xAxis2, QCPAxis::stLogarithmic);
    mPlot->replot(); // queues the reset
    setScaleTypeSilently(mPlot->xAxis2, QCPAxis::stLinear);
    QSignalSpy busy(&h->pipeline(), &QCPHistogram1DPipeline::busyChanged);
    QTest::qWait(100);
    QCOMPARE(busy.count(), 0);
    QCOMPARE(h->histogram()->edges, edges);
}

// A job started before the toggle lands after the reset was queued: its result moves the auto
// edges, which reinstalls the transform for the new scale. Its own linear bars must not stay.
void TestHistogramPlottable::resultOfTheOldScaleIsNotShownUnderTheNewOne()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBins(3);
    auto gate = std::make_shared<Gate>();
    h->setDataSource(gated({1, 10, 100, 1000}, gate));
    QVERIFY(gate->waitEntered());
    h->setKeyAxis(mPlot->xAxis2);
    mPlot->xAxis2->setScaleType(QCPAxis::stLogarithmic);
    gate->open = true;
    QThread::msleep(100); // the job ends and posts its result, which nothing handles yet
    mPlot->replot();      // queues the reset behind that result
    QVERIFY(settle(h));
    QTRY_VERIFY_WITH_TIMEOUT(h->histogram() && h->histogram()->log, 2000);
}

// Every frame showing stale bars asks for a reset; all but the first would supersede the job
// before it, so under a steady replot stream the new bars would never land.
void TestHistogramPlottable::staleScaleIsResetOnceUnderAReplotStream()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBins(3);
    h->setDataSource(values({1, 10, 100, 1000}));
    QVERIFY(waitForResult(h));
    QVERIFY(settle(h));
    auto gate = std::make_shared<Gate>();
    const auto release = qScopeGuard([gate] { gate->open = true; }); // a failure must not hang
    h->setDataSource(gated({1, 10, 100, 1000}, gate));
    QVERIFY(gate->waitEntered());
    h->setKeyAxis(mPlot->xAxis2);
    mPlot->xAxis2->setScaleType(QCPAxis::stLogarithmic);
    const auto before = h->pipeline().currentGeneration();
    for (int i = 0; i < 20; ++i)
    {
        mPlot->replot();
        QCoreApplication::processEvents();
    }
    QCOMPARE(h->pipeline().currentGeneration() - before, uint64_t(1));
    gate->open = true;
    QVERIFY(settle(h));
    QTRY_VERIFY_WITH_TIMEOUT(h->histogram() && h->histogram()->log, 2000);
}

void TestHistogramPlottable::autoEdgesNeverShrinkWhileJobsOverlap()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setDataSource(values({0, 10}));
    QVERIFY(waitForResult(h));
    EdgeLog log(h);
    auto gate = std::make_shared<Gate>();
    h->setDataSource(gated({-5, 8}, gate));
    QVERIFY(gate->waitEntered());
    h->setDataSource(values({2, 8})); // queued behind the blocked job, with the old edges
    gate->open = true;
    QVERIFY(settle(h));
    QVERIFY(!log.shown.isEmpty());
    for (int i = 1; i < log.shown.size(); ++i)
    {
        QVERIFY(log.shown[i].lower <= log.shown[i - 1].lower);
        QVERIFY(log.shown[i].upper >= log.shown[i - 1].upper);
    }
    QCOMPARE(h->histogram()->edges.front(), -5.0);
    QCOMPARE(h->histogram()->edges.back(), 10.0);
}

// The last result is narrower than the edges and re-bins from finished(): that job must land.
void TestHistogramPlottable::rebinningAfterOverlappingJobsShowsTheLastSource()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    auto gate = std::make_shared<Gate>();
    h->setDataSource(gated({0, 10}, gate));
    QVERIFY(gate->waitEntered());
    h->setDataSource(values({2, 5, 8})); // queued, binned without the edges of the first
    gate->open = true;
    QVERIFY(settle(h));
    QVERIFY(h->histogram());
    const auto& heights = h->histogram()->heights.front();
    QCOMPARE(std::accumulate(heights.begin(), heights.end(), 0.0), 3.0);
}

void TestHistogramPlottable::preBinnedBarsDropAnOlderSourcesResultAndRange()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setDataSource(values({-100, -90}));
    QVERIFY(waitForResult(h));
    auto gate = std::make_shared<Gate>();
    h->setDataSource(gated({0, 10}, gate));
    QVERIFY(gate->waitEntered());
    h->setBinned({100, 101, 102}, {{1, 2}});
    h->setDataSource(values({50, 60}));
    EdgeLog log(h);
    gate->open = true;
    QVERIFY(settle(h));
    for (const QCPRange& shown : log.shown)
        QCOMPARE(shown.lower, 50.0); // neither the stale source nor its range
    QCOMPARE(h->histogram()->edges.front(), 50.0);
    QCOMPARE(h->histogram()->edges.back(), 60.0);
}

void TestHistogramPlottable::resetEdgesDropsAResultBinnedBeforeTheReset()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setDataSource(values({-100, -90}));
    QVERIFY(waitForResult(h));
    auto gate = std::make_shared<Gate>();
    h->setDataSource(gated({0, 10}, gate));
    QVERIFY(gate->waitEntered());
    EdgeLog log(h);
    h->resetEdges();
    gate->open = true;
    QVERIFY(settle(h));
    for (const QCPRange& shown : log.shown)
        QCOMPARE(shown.lower, 0.0);
    QCOMPARE(h->histogram()->edges.front(), 0.0);
    QCOMPARE(h->histogram()->edges.back(), 10.0);
}

void TestHistogramPlottable::clearingTheSourceClearsTheBars()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setDataSource(values({0, 10}));
    QVERIFY(waitForResult(h));
    QSignalSpy ready(h, &QCPHistogram::histogramReady);
    h->setDataSource(nullptr);
    QVERIFY(!h->histogram());
    QCOMPARE(ready.count(), 1);
}

void TestHistogramPlottable::clearingTheSourceDropsAnInFlightResult()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    auto gate = std::make_shared<Gate>();
    h->setDataSource(gated({0, 10}, gate));
    QVERIFY(gate->waitEntered());
    h->setDataSource(nullptr);
    gate->open = true;
    QVERIFY(settle(h));
    QVERIFY(!h->histogram());
}

void TestHistogramPlottable::exportBinningEmitsNothing()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    QSignalSpy ready(h, &QCPHistogram::histogramReady);
    QSignalSpy moved(h, &QCPHistogram::edgesChanged);
    h->setDataSource(values({0, 1, 2, 3}));
    QVERIFY(!mPlot->toPixmap(400, 300).isNull());
    QVERIFY(h->histogram());
    QCOMPARE(ready.count(), 0);
    QCOMPARE(moved.count(), 0);
}

void TestHistogramPlottable::exportWhileBinningShowsTheNewData()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBins(1);
    h->setFixedRange(QCPRange(0, 10));
    h->setDataSource(values({1, 2}));
    QVERIFY(waitForResult(h));
    QVERIFY(settle(h));
    h->setDataSource(values({1, 2, 3, 4, 5}));
    QVERIFY(h->pipeline().isBusy());
    QVERIFY(!mPlot->toPixmap(400, 300).isNull());
    QCOMPARE(h->histogram()->heights[0][0], 5.0);
}

static bool isRedish(QColor c) { return c.red() > 150 && c.green() < 120 && c.blue() < 120; }

void TestHistogramPlottable::barsAreDrawnUpToTheirHeight()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setColumnColors({Qt::red});
    h->setBinned({0, 1, 2}, {{3, 6}});
    mPlot->xAxis->setRange(0, 2);
    mPlot->yAxis->setRange(0, 8);
    const QImage img = mPlot->toPixmap(400, 300).toImage();
    const auto px = [&](double x, double y) {
        return img.pixelColor(int(mPlot->xAxis->coordToPixel(x)), int(mPlot->yAxis->coordToPixel(y)));
    };
    QVERIFY(isRedish(px(0.5, 2)));
    QVERIFY(!isRedish(px(0.5, 4.5)));
    QVERIFY(isRedish(px(1.5, 4.5)));
}

void TestHistogramPlottable::outlineRunsDoNotJoinBars()
{
    const std::vector<qcp::histogram::BarRect> bars { {0, 10, 100, 50}, {10, 20, 100, 20} };
    const auto runs = qcp::histogram::barOutlines(bars, false);
    QCOMPARE(runs.size(), 5 + 1 + 5);
    QVERIFY(std::isnan(runs[5].x()));
}

void TestHistogramPlottable::stepOutlineIsTheTopProfile()
{
    const std::vector<qcp::histogram::BarRect> bars { {0, 10, 100, 50}, {10, 20, 100, 20} };
    const auto run = qcp::histogram::stepOutline(bars, false);
    QCOMPARE(run.first(), QPointF(0, 100));
    QCOMPARE(run.last(), QPointF(20, 100));
    QVERIFY(run.contains(QPointF(10, 50)) && run.contains(QPointF(10, 20)));
}

namespace {
std::vector<qcp::histogram::BarRect> rectsOf(QCustomPlot* plot, const qcp::algo::Histogram1D& h, int column)
{
    return qcp::histogram::barRects(h, column, plot->xAxis, plot->yAxis);
}
const qcp::algo::Histogram1D twoBins { .edges = {0, 1, 2}, .heights = {{3, 6}}, .counted = {9} };
}

void TestHistogramPlottable::barRectsOfAMissingColumnAreEmpty()
{
    QCOMPARE(rectsOf(mPlot, twoBins, 0).size(), std::size_t(2));
    QVERIFY(rectsOf(mPlot, twoBins, -1).empty());
    QVERIFY(rectsOf(mPlot, twoBins, 1).empty());
}

void TestHistogramPlottable::barRectsOfRaggedEdgesAreEmpty()
{
    const qcp::algo::Histogram1D tooManyHeights { .edges = {0, 1, 2}, .heights = {{3, 6, 5, 4}}, .counted = {9} };
    const qcp::algo::Histogram1D tooManyEdges { .edges = {0, 1, 2, 3, 4}, .heights = {{3, 6}}, .counted = {9} };
    const qcp::algo::Histogram1D oneEdge { .edges = {0}, .heights = {{3}}, .counted = {3} };
    QVERIFY(rectsOf(mPlot, tooManyHeights, 0).empty());
    QVERIFY(rectsOf(mPlot, tooManyEdges, 0).empty());
    QVERIFY(rectsOf(mPlot, oneEdge, 0).empty());
}

void TestHistogramPlottable::logAxisMarksOnlyNonPositiveOrNonFiniteBarsAbsent()
{
    mPlot->yAxis->setScaleType(QCPAxis::stLogarithmic);
    mPlot->yAxis->setRange(1, 10);
    const qcp::algo::Histogram1D h { .edges = {0, 1, 2, 3, 4, 5},
                                     .heights = {{1, 0, -3, qQNaN(), 4}}, .counted = {5}, .log = true };
    const auto bars = rectsOf(mPlot, h, 0);
    QVERIFY(!bars[0].absent);
    QCOMPARE(bars[0].top, mPlot->yAxis->coordToPixel(1));
    QVERIFY(bars[1].absent);
    QVERIFY(bars[2].absent);
    QVERIFY(bars[3].absent);
    QVERIFY(!bars[4].absent);
}

void TestHistogramPlottable::stepOutlineBreaksAtAnAbsentBar()
{
    mPlot->yAxis->setScaleType(QCPAxis::stLogarithmic);
    mPlot->yAxis->setRange(1, 10);
    const qcp::algo::Histogram1D h { .edges = {0, 1, 2, 3}, .heights = {{4, 0, 4}}, .counted = {8}, .log = true };
    const auto bars = rectsOf(mPlot, h, 0);
    const auto run = qcp::histogram::stepOutline(bars, false);
    QCOMPARE(run.size(), 4 + 1 + 4);
    QVERIFY(std::isnan(run[4].x()));
    QCOMPARE(run[3], QPointF(bars[0].right, bars[0].base));
    QCOMPARE(run[5], QPointF(bars[2].left, bars[2].base));
}

void TestHistogramPlottable::gpuQuadsOfBarsWithAGapStayInsideTheirBar()
{
    const std::array<float, 4> rgba { 1, 0, 0, 1 };
    const std::vector<qcp::histogram::BarRect> bars { {0, 10, 100, 50}, {20, 30, 100, 20} };
    std::vector<float> out;
    qcp::histogram::appendBarQuads(bars, false, rgba, out);
    constexpr std::size_t floatsPerTriangle = 3 * 6;
    QCOMPARE(out.size(), 2 * 2 * floatsPerTriangle);
    for (std::size_t t = 0; t < out.size(); t += floatsPerTriangle)
    {
        const float x0 = out[t], x1 = out[t + 6], x2 = out[t + 12];
        const float lo = std::min({x0, x1, x2}), hi = std::max({x0, x1, x2});
        QVERIFY2((lo >= 0 && hi <= 10) || (lo >= 20 && hi <= 30), qPrintable(QString::number(lo) + " " + QString::number(hi)));
    }
}

void TestHistogramPlottable::emptyBinsOnALogAxisStayOutOfTheOutline()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBinned({0, 1, 2, 3}, {{4, 0, 4}});
    mPlot->xAxis->setRange(0, 3);
    mPlot->yAxis->setScaleType(QCPAxis::stLogarithmic);
    mPlot->yAxis->setRange(1, 10);
    mPlot->replot();
    const auto bars = qcp::histogram::barRects(*h->histogram(), 0, mPlot->xAxis, mPlot->yAxis);
    QCOMPARE(bars[1].top, bars[1].base);
    for (const QPointF& p : qcp::histogram::stepOutline(bars, false))
        QVERIFY(std::isnan(p.x()) || std::isfinite(p.y()));
}

// Drawing ignores the selection state, so a click must not select something that looks the same.
void TestHistogramPlottable::aHistogramIsNotSelectable()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBinned({0, 1, 2}, {{3, 6}});
    mPlot->xAxis->setRange(0, 2);
    mPlot->yAxis->setRange(0, 8);
    mPlot->replot();
    const QPointF insideABar(mPlot->xAxis->coordToPixel(0.5), mPlot->yAxis->coordToPixel(2));
    QCOMPARE(h->selectTest(insideABar, false), -1.0);
    QCOMPARE(h->selectTest(insideABar, true), -1.0);
}

void TestHistogramPlottable::verticalKeyAxisGrowsBarsAlongX()
{
    auto* h = new QCPHistogram(mPlot->yAxis, mPlot->xAxis);
    h->setColumnColors({Qt::red});
    h->setBinned({0, 1, 2}, {{6, 2}});
    mPlot->yAxis->setRange(0, 2);
    mPlot->xAxis->setRange(0, 8);
    const QImage img = mPlot->toPixmap(400, 300).toImage();
    const auto at = [&](double value, double key) {
        return QPointF(mPlot->xAxis->coordToPixel(value), mPlot->yAxis->coordToPixel(key));
    };

    const auto bars = qcp::histogram::barRects(*h->histogram(), 0, mPlot->yAxis, mPlot->xAxis);
    QCOMPARE(bars.size(), std::size_t(2));
    QCOMPARE(bars[0].left, mPlot->yAxis->coordToPixel(0));
    QCOMPARE(bars[0].base, mPlot->xAxis->coordToPixel(0));
    QCOMPARE(bars[0].top, mPlot->xAxis->coordToPixel(6));
    const QRectF r = qcp::histogram::pixelRect(bars[0], true);
    QCOMPARE(r.left(), mPlot->xAxis->coordToPixel(0));
    QCOMPARE(r.right(), mPlot->xAxis->coordToPixel(6));
    QCOMPARE(r.top(), mPlot->yAxis->coordToPixel(1));
    QCOMPARE(r.bottom(), mPlot->yAxis->coordToPixel(0));
    const auto outline = qcp::histogram::barOutlines(bars, true);
    QCOMPARE(outline[0], at(0, 0));
    QCOMPARE(outline[1], at(6, 0));
    QCOMPARE(outline[2], at(6, 1));
    QVERIFY(qcp::histogram::contains(bars, at(4.5, 0.5), true));
    QVERIFY(!qcp::histogram::contains(bars, at(4.5, 1.5), true));
    QVERIFY(qcp::histogram::contains(bars, at(1.5, 1.5), true));

    for (const double key : {0.25, 0.75, 1.25, 1.75})
        for (const double value : {0.5, 1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5})
        {
            const bool inABar = value < (key < 1 ? 6 : 2);
            QVERIFY2(isRedish(img.pixelColor(at(value, key).toPoint())) == inABar,
                     qPrintable(QStringLiteral("value %1, key %2").arg(value).arg(key)));
        }
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
}

void TestHistogramPlottable::barsAreDrawnOnTheGpu()
{
    if (!showWithRhi(mPlot))
        QSKIP("no QRhi available in this environment");
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setColumnColors({Qt::red});
    h->setBinned({0, 1, 2}, {{3, 6}});
    mPlot->xAxis->setRange(0, 2);
    mPlot->yAxis->setRange(0, 8);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(!mPlot->plottableRhiLayer(h->layer())->drawEntries().isEmpty());
    const QImage frame = mPlot->grabFramebuffer();
    const double dpr = frame.devicePixelRatio();
    const auto px = [&](double x, double y) {
        return frame.pixelColor(qRound(mPlot->xAxis->coordToPixel(x) * dpr),
                                qRound(mPlot->yAxis->coordToPixel(y) * dpr));
    };
    QVERIFY(isRedish(px(1.5, 4.5)));
    QVERIFY(!isRedish(px(0.5, 4.5)));
}

void TestHistogramPlottable::emptyBinOnALinearAxisKeepsTheStepRun()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBinned({0, 1, 2, 3}, {{4, 0, 4}});
    mPlot->xAxis->setRange(0, 3);
    mPlot->yAxis->setRange(0, 10);
    mPlot->replot();
    const auto bars = qcp::histogram::barRects(*h->histogram(), 0, mPlot->xAxis, mPlot->yAxis);
    const auto run = qcp::histogram::stepOutline(bars, false);
    for (const QPointF& p : run)
        QVERIFY(!std::isnan(p.x()));
    QVERIFY(run.contains(QPointF(bars[1].left, bars[1].base)));
    QVERIFY(run.contains(QPointF(bars[1].right, bars[1].base)));
}

namespace {
QColor exportedPixel(QCustomPlot* plot, double x, double y)
{
    const QImage img = plot->toPixmap(400, 300).toImage();
    return img.pixelColor(int(plot->xAxis->coordToPixel(x)), int(plot->yAxis->coordToPixel(y)));
}
bool isLightRed(QColor c) { return c.red() > 200 && c.green() > 150 && c.green() < 235; }
}

void TestHistogramPlottable::stepWithOneColumnIsTheOutlineOnly()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setColumnColors({Qt::red});
    h->setStyle(QCPHistogram::sStep);
    h->setBinned({0, 1, 2}, {{3, 6}});
    mPlot->xAxis->setRange(0, 2);
    mPlot->yAxis->setRange(0, 8);
    const QColor inside = exportedPixel(mPlot, 0.5, 2);
    QVERIFY2(inside.green() > 240 && inside.blue() > 240, qPrintable(inside.name()));
}

void TestHistogramPlottable::stepWithSeveralColumnsHasALightFill()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setColumnColors({Qt::red, Qt::blue});
    h->setStyle(QCPHistogram::sStep);
    h->setBinned({0, 1, 2}, {{3, 6}, {0, 0}});
    mPlot->xAxis->setRange(0, 2);
    mPlot->yAxis->setRange(0, 8);
    const QColor inside = exportedPixel(mPlot, 0.5, 2);
    QVERIFY2(isLightRed(inside), qPrintable(inside.name()));
}

void TestHistogramPlottable::barsKeepATranslucentUserColour()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setColumnColors({QColor(255, 0, 0, 64)});
    h->setBinned({0, 1, 2}, {{3, 6}});
    mPlot->xAxis->setRange(0, 2);
    mPlot->yAxis->setRange(0, 8);
    const QColor inside = exportedPixel(mPlot, 0.5, 2);
    QVERIFY2(isLightRed(inside), qPrintable(inside.name()));
}

namespace {
template <typename Signal>
void removeOn(QCustomPlot* plot, QCPHistogram* h, Signal signal)
{
    QObject::connect(
        h, signal, plot,
        [plot, alive = QPointer<QCPHistogram>(h)] {
            if (alive)
                plot->removePlottable(alive);
        },
        Qt::SingleShotConnection);
}
} // namespace

void TestHistogramPlottable::readySlotRemovingTheHistogramIsSafe()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    removeOn(mPlot, h, &QCPHistogram::histogramReady);
    h->setDataSource(values({0, 1, 2, 3, 4}));
    QTRY_COMPARE_WITH_TIMEOUT(mPlot->plottableCount(), 0, 5000);
    QTest::qWait(50);
}

void TestHistogramPlottable::edgesSlotRemovingTheHistogramIsSafe()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    removeOn(mPlot, h, &QCPHistogram::edgesChanged);
    h->setDataSource(values({0, 1, 2, 3, 4}));
    QTRY_COMPARE_WITH_TIMEOUT(mPlot->plottableCount(), 0, 5000);
    QTest::qWait(50);
}

void TestHistogramPlottable::preBinnedReadySlotRemovingTheHistogramIsSafe()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    removeOn(mPlot, h, &QCPHistogram::histogramReady);
    h->setBinned({0, 1, 2}, {{3, 5}});
    QCOMPARE(mPlot->plottableCount(), 0);
}

namespace {
struct Probe : QCPHistogram
{
    using QCPHistogram::QCPHistogram;
    using QCPHistogram::canProduceContent;
    using QCPHistogram::draw;
};

QImage exportWithoutAntialiasing(QCustomPlot* plot)
{
    plot->setNotAntialiasedElements(QCP::aeAll);
    return plot->toPixmap(400, 300).toImage();
}

int pixelX(QCustomPlot* plot, double x) { return int(plot->xAxis->coordToPixel(x)); }
int pixelY(QCustomPlot* plot, double y) { return int(plot->yAxis->coordToPixel(y)); }

int rowsOfColor(const QImage& img, int x, int firstRow, int lastRow, QColor color)
{
    int count = 0;
    for (int y = firstRow; y <= lastRow; ++y)
        count += img.pixelColor(x, y).rgb() == color.rgb();
    return count;
}

bool isDark(QColor c) { return c.red() < 100 && c.green() < 100 && c.blue() < 100; }
}

void TestHistogramPlottable::logBinsHonourAFixedRangeStartingAtOne()
{
    mPlot->xAxis->setScaleType(QCPAxis::stLogarithmic);
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setFixedRange(QCPRange(1, 100));
    h->setDataSource(values({5, 50}));
    QVERIFY(waitForResult(h));
    QCOMPARE(h->histogram()->edges.front(), 1.0);
    QCOMPARE(h->histogram()->edges.back(), 100.0);
}

void TestHistogramPlottable::fixedRangeStartingAtZeroFallsBackWhenTheAxisTurnsLog()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setFixedRange(QCPRange(0, 100));
    h->setDataSource(values({1, 100}));
    QVERIFY(waitForResult(h));
    QCOMPARE(h->histogram()->edges.front(), 0.0);
    QSignalSpy ready(h, &QCPHistogram::histogramReady);
    mPlot->xAxis->setScaleType(QCPAxis::stLogarithmic);
    QVERIFY(ready.wait(5000));
    QVERIFY(h->histogram()->log);
    QCOMPARE(h->histogram()->edges.front(), 1.0);
}

void TestHistogramPlottable::flatDataGetsAWidenedRange()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setDataSource(values({5, 5}));
    QVERIFY(waitForResult(h));
    QVERIFY(h->histogram()->edges.front() < 5.0);
    QVERIFY(h->histogram()->edges.back() > 5.0);
}

void TestHistogramPlottable::aSingleBinCanBePreBinned()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBinned({0, 1}, {{3}});
    QVERIFY(h->histogram());
}

void TestHistogramPlottable::preBinnedCountsNothing()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBinned({0, 1, 2}, {{3, 5}, {1, 2}});
    QCOMPARE(h->histogram()->counted, (std::vector<int>{0, 0}));
}

void TestHistogramPlottable::positiveValueRangeStartsAtTheSmallestHeight()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBinned({0, 1, 2, 3}, {{0.5, 4, 16}});
    bool found = false;
    QCOMPARE(h->getValueRange(found, QCP::sdPositive).lower, 0.5);
    QVERIFY(found);
}

// Hidden, so no draw notices the scale: only the axis signal can trigger the re-bin.
void TestHistogramPlottable::keyAxisScaleChangeRebinsWithoutADraw()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setVisible(false);
    h->setBins(3);
    h->setDataSource(values({1, 10, 100, 1000}));
    QVERIFY(waitForResult(h));
    QVERIFY(settle(h));
    QSignalSpy ready(h, &QCPHistogram::histogramReady);
    mPlot->xAxis->setScaleType(QCPAxis::stLogarithmic);
    QVERIFY(ready.wait(5000));
    QVERIFY(h->histogram()->log);
}

// The transform was rebuilt for the new scale (setBins), so only the bars on show tell they are
// from the old one.
void TestHistogramPlottable::shownResultOfTheOtherScaleIsStaleEvenWhenTheTransformIsFresh()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBins(3);
    h->setDataSource(values({1, 10, 100, 1000}));
    QVERIFY(waitForResult(h));
    QVERIFY(settle(h));
    auto gate = std::make_shared<Gate>();
    const auto release = qScopeGuard([gate] { gate->open = true; });
    h->setDataSource(gated({1, 10, 100, 1000}, gate));
    QVERIFY(gate->waitEntered());
    h->setKeyAxis(mPlot->xAxis2);
    mPlot->xAxis2->setScaleType(QCPAxis::stLogarithmic);
    h->setBins(7);
    const auto before = h->pipeline().currentGeneration();
    for (int i = 0; i < 5; ++i)
    {
        mPlot->replot();
        QCoreApplication::processEvents();
    }
    QCOMPARE(h->pipeline().currentGeneration() - before, uint64_t(1));
}

void TestHistogramPlottable::logAxisFromTheStartIsNotStale()
{
    mPlot->xAxis->setScaleType(QCPAxis::stLogarithmic);
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBins(3);
    h->setDataSource(values({1, 10, 100, 1000}));
    QVERIFY(waitForResult(h));
    QVERIFY(settle(h));
    const auto before = h->pipeline().currentGeneration();
    QSignalSpy ready(h, &QCPHistogram::histogramReady);
    for (int i = 0; i < 3; ++i)
    {
        mPlot->replot();
        QCoreApplication::processEvents();
    }
    QTest::qWait(50);
    QCOMPARE(h->pipeline().currentGeneration(), before);
    QCOMPARE(ready.count(), 0);
}

void TestHistogramPlottable::normalizationIsAppliedToTheBars()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBins(2);
    h->setNormalization(qcp::algo::HistogramNormalization::Probability);
    h->setDataSource(values({0, 1, 2, 3}));
    QVERIFY(waitForResult(h));
    QCOMPARE(h->histogram()->heights[0], (std::vector<double>{0.5, 0.5}));
}

// A reset for a scale stays in flight until its job delivers; the next stale scale needs a new one.
// xAxis2 is log before each move, so both log re-bins go through draw(), not the scale signal.
void TestHistogramPlottable::staleScaleIsResetAgainAfterAJobDelivered()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBins(3);
    h->setDataSource(values({1, 10, 100, 1000}));
    QVERIFY(waitForResult(h));
    QVERIFY(settle(h));
    mPlot->xAxis2->setScaleType(QCPAxis::stLogarithmic);
    h->setKeyAxis(mPlot->xAxis2);
    mPlot->replot();
    QTRY_VERIFY_WITH_TIMEOUT(h->histogram() && h->histogram()->log, 5000);
    QVERIFY(settle(h));
    h->setVisible(false);
    h->setKeyAxis(mPlot->xAxis);
    h->setBins(4);
    QTRY_VERIFY_WITH_TIMEOUT(h->histogram() && !h->histogram()->log, 5000);
    QVERIFY(settle(h));
    h->setKeyAxis(mPlot->xAxis2);
    h->setVisible(true);
    mPlot->replot();
    QTRY_VERIFY_WITH_TIMEOUT(h->histogram() && h->histogram()->log, 5000);
}

// A job of the old scale lands on a hidden plot: nothing but its own check can ask for the new bars.
void TestHistogramPlottable::resultOfTheOldScaleIsResetWithoutADraw()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setVisible(false);
    h->setKeyAxis(mPlot->xAxis2);
    h->setBins(3);
    auto gate = std::make_shared<Gate>();
    const auto release = qScopeGuard([gate] { gate->open = true; });
    h->setDataSource(gated({1, 10, 100, 1000}, gate));
    QVERIFY(gate->waitEntered());
    mPlot->xAxis2->setScaleType(QCPAxis::stLogarithmic);
    gate->open = true;
    QTRY_VERIFY_WITH_TIMEOUT(h->histogram() && h->histogram()->log, 5000);
}

void TestHistogramPlottable::nonPositiveBinCountsAreIgnored()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBins(4);
    h->setBins(0);
    QCOMPARE(h->bins(), 4);
    h->setBins(-3);
    QCOMPARE(h->bins(), 4);
}

void TestHistogramPlottable::theSameBinCountDoesNotRebin()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBins(4);
    h->setDataSource(values({0, 1, 2, 3}));
    QVERIFY(waitForResult(h));
    QVERIFY(settle(h));
    const auto before = h->pipeline().currentGeneration();
    h->setBins(4);
    QCOMPARE(h->pipeline().currentGeneration(), before);
}

bool TestHistogramPlottable::repaintFollows(QCPHistogram* h, const std::function<void()>& change)
{
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTest::qWait(20);
    const auto buffer = h->layer()->mPaintBuffer.toStrongRef();
    if (!buffer || buffer->contentDirty())
        return false;
    QSignalSpy replotted(mPlot, &QCustomPlot::afterReplot);
    change();
    return buffer->contentDirty() && replotted.wait(2000);
}

void TestHistogramPlottable::changingTheStyleRequestsARepaint()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBinned({0, 1, 2}, {{3, 6}});
    QVERIFY(repaintFollows(h, [&] { h->setStyle(QCPHistogram::sStep); }));
}

void TestHistogramPlottable::changingTheColoursRequestsARepaint()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBinned({0, 1, 2}, {{3, 6}});
    QVERIFY(repaintFollows(h, [&] { h->setColumnColors({Qt::red}); }));
}

void TestHistogramPlottable::newBarsRequestARepaint()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBinned({0, 1, 2}, {{3, 6}});
    QVERIFY(repaintFollows(h, [&] { h->setBinned({0, 1, 2}, {{4, 5}}); }));
}

// No source, so there is nothing to bin for the other scale: the export itself must not re-bin.
void TestHistogramPlottable::exportOfPreBinnedBarsDoesNotRebin()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBinned({0, 1, 2}, {{3, 6}});
    h->setKeyAxis(mPlot->xAxis2);
    mPlot->xAxis2->setScaleType(QCPAxis::stLogarithmic);
    const auto before = h->pipeline().currentGeneration();
    QSignalSpy ready(h, &QCPHistogram::histogramReady);
    QVERIFY(!mPlot->toPixmap(400, 300).isNull());
    QCOMPARE(h->pipeline().currentGeneration(), before);
    QCOMPARE(ready.count(), 0);
}

// The export paints a stale scale, which queues the one reset; a fresh transform would drop it.
void TestHistogramPlottable::exportOfPreBinnedBarsOnAStaleScaleQueuesOneReset()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBinned({0, 1, 2}, {{3, 6}});
    h->setKeyAxis(mPlot->xAxis2);
    setScaleTypeSilently(mPlot->xAxis2, QCPAxis::stLogarithmic);
    const auto before = h->pipeline().currentGeneration();
    QVERIFY(!mPlot->toPixmap(400, 300).isNull());
    QTest::qWait(50);
    QCOMPARE(h->pipeline().currentGeneration() - before, uint64_t(1));
}

// The automatic edges of the old scale must not outlive it: they may be wider than the data.
// The new axis is log before the move, so only the export can notice the scale.
void TestHistogramPlottable::exportOfAStaleScaleForgetsTheAutoRange()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBins(3);
    h->setDataSource(values({1, 1000}));
    QVERIFY(waitForResult(h));
    QVERIFY(settle(h));
    h->setDataSource(values({10, 100}));
    QVERIFY(waitForResult(h));
    QVERIFY(settle(h));
    QCOMPARE(h->histogram()->edges.front(), 1.0);
    mPlot->xAxis2->setScaleType(QCPAxis::stLogarithmic);
    h->setKeyAxis(mPlot->xAxis2);
    QVERIFY(!mPlot->toPixmap(400, 300).isNull());
    QVERIFY(h->histogram()->log);
    QCOMPARE(h->histogram()->edges.front(), 10.0);
    QCOMPARE(h->histogram()->edges.back(), 100.0);
}

void TestHistogramPlottable::emptyHistogramHasNoContent()
{
    auto* h = new Probe(mPlot->xAxis, mPlot->yAxis);
    QVERIFY(!h->canProduceContent());
    h->setBinned({0, 1, 2}, {{3, 5}});
    QVERIFY(h->canProduceContent());
}

void TestHistogramPlottable::withoutBarsThereIsNoKeyRange()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    bool found = true;
    h->getKeyRange(found);
    QVERIFY(!found);
}

void TestHistogramPlottable::noPositiveEdgeMeansNoPositiveKeyRange()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBinned({-2, -1, 0}, {{1, 1}});
    bool found = true;
    h->getKeyRange(found, QCP::sdPositive);
    QVERIFY(!found);
}

void TestHistogramPlottable::positiveKeyRangeStartsAtTheFirstPositiveEdge()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBinned({-1, 0, 0.5, 2}, {{1, 1, 1}});
    bool found = false;
    const QCPRange r = h->getKeyRange(found, QCP::sdPositive);
    QVERIFY(found);
    QCOMPARE(r.lower, 0.5);
    QCOMPARE(r.upper, 2.0);
}

void TestHistogramPlottable::keyRangeSpansTheEdges()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBinned({-2, -1, 0}, {{1, 1}});
    bool found = false;
    const QCPRange r = h->getKeyRange(found);
    QVERIFY(found);
    QCOMPARE(r.lower, -2.0);
    QCOMPARE(r.upper, 0.0);
}

void TestHistogramPlottable::valueRangeOfSmallHeightsStartsAtZero()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBinned({0, 1}, {{0.5}});
    bool found = false;
    const QCPRange r = h->getValueRange(found);
    QVERIFY(found);
    QCOMPARE(r.lower, 0.0);
    QCOMPARE(r.upper, 0.5);
}

void TestHistogramPlottable::barsHaveADarkerOutlineAroundTheBase()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setColumnColors({Qt::red});
    h->setBinned({0, 1, 2}, {{3, 6}});
    mPlot->xAxis->setRange(0, 2);
    mPlot->yAxis->setRange(-2, 8);
    const QImage img = exportWithoutAntialiasing(mPlot);
    const int base = pixelY(mPlot, 0);
    QVERIFY(rowsOfColor(img, pixelX(mPlot, 0.5), base - 1, base + 1, QColor(Qt::red).darker(125)) > 0);
}

void TestHistogramPlottable::stepOutlineIsOnePixelThick()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setColumnColors({Qt::red});
    h->setStyle(QCPHistogram::sStep);
    h->setBinned({0, 1, 2}, {{3, 6}});
    mPlot->xAxis->setRange(0, 2);
    mPlot->yAxis->setRange(-2, 8);
    const QImage img = exportWithoutAntialiasing(mPlot);
    const int top = pixelY(mPlot, 3);
    QCOMPARE(rowsOfColor(img, pixelX(mPlot, 0.5), top - 3, top + 3, Qt::red), 1);
}

void TestHistogramPlottable::stepOutlineHasNoBase()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setColumnColors({Qt::red});
    h->setStyle(QCPHistogram::sStep);
    h->setBinned({0, 1, 2}, {{3, 6}});
    mPlot->xAxis->setRange(0, 2);
    mPlot->yAxis->setRange(-2, 8);
    const QImage img = exportWithoutAntialiasing(mPlot);
    const int base = pixelY(mPlot, 0);
    QCOMPARE(rowsOfColor(img, pixelX(mPlot, 0.5), base - 1, base + 1, Qt::red), 0);
}

void TestHistogramPlottable::fillHasNoBorderWhateverPenThePainterHad()
{
    auto* h = new Probe(mPlot->xAxis, mPlot->yAxis);
    h->setColumnColors({Qt::red, Qt::blue});
    h->setStyle(QCPHistogram::sStep);
    h->setBinned({0, 1, 2}, {{3, 6}, {0, 0}});
    mPlot->xAxis->setRange(0, 2);
    mPlot->yAxis->setRange(-2, 8);
    mPlot->setViewport(QRect(0, 0, 400, 300));
    mPlot->replot();
    QImage img(400, 300, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    {
        QCPPainter painter(&img);
        painter.setPen(QPen(Qt::green));
        h->draw(&painter);
    }
    const int y = pixelY(mPlot, 1.5), edge = pixelX(mPlot, 1.0);
    for (int x = edge - 1; x <= edge + 1; ++x)
    {
        const QColor c = img.pixelColor(x, y);
        QVERIFY2(!(c.green() > c.red() + 60 && c.green() > c.blue() + 60), qPrintable(c.name()));
    }
}

void TestHistogramPlottable::anAlmostTransparentFillStillPaints()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setColumnColors({QColor(255, 0, 0, 1)});
    h->setBinned({0, 1, 2}, {{3, 6}});
    mPlot->xAxis->setRange(0, 2);
    mPlot->yAxis->setRange(-2, 8);
    const QImage img = exportWithoutAntialiasing(mPlot);
    const QColor inside = img.pixelColor(pixelX(mPlot, 0.5), pixelY(mPlot, 1.5));
    QCOMPARE(inside.red(), 255);
    QCOMPARE(inside.green(), 254);
    QCOMPARE(inside.blue(), 254);
}

namespace {
QVector<QCPPlottableRhiLayer::DrawEntry> gpuEntries(QCustomPlot* plot, QCPHistogram* h)
{
    plot->replot(QCustomPlot::rpImmediateRefresh);
    return plot->plottableRhiLayer(h->layer())->drawEntries();
}

QCPHistogram* gpuHistogram(QCustomPlot* plot, QCPHistogram::Style style, QColor color)
{
    auto* h = new QCPHistogram(plot->xAxis, plot->yAxis);
    h->setColumnColors({color});
    h->setStyle(style);
    h->setBinned({0, 1, 2}, {{3, 6}});
    plot->xAxis->setRange(0, 2);
    plot->yAxis->setRange(0, 8);
    return h;
}
}

void TestHistogramPlottable::gpuBarsHaveFillAndOutline()
{
    if (!showWithRhi(mPlot))
        QSKIP("no QRhi available in this environment");
    const auto drawn = gpuEntries(mPlot, gpuHistogram(mPlot, QCPHistogram::sBars, Qt::red));
    QCOMPARE(drawn.size(), 1);
    QVERIFY(drawn[0].fillVertexCount > 0);
    QVERIFY(drawn[0].strokeVertexCount > 0);
}

void TestHistogramPlottable::gpuBarsAreNotShifted()
{
    if (!showWithRhi(mPlot))
        QSKIP("no QRhi available in this environment");
    const auto drawn = gpuEntries(mPlot, gpuHistogram(mPlot, QCPHistogram::sBars, Qt::red));
    QCOMPARE(drawn.size(), 1);
    QCOMPARE(drawn[0].offsetX, 0.0f);
    QCOMPARE(drawn[0].offsetY, 0.0f);
}

void TestHistogramPlottable::gpuStepOfOneColumnHasOnlyTheOutline()
{
    if (!showWithRhi(mPlot))
        QSKIP("no QRhi available in this environment");
    const auto drawn = gpuEntries(mPlot, gpuHistogram(mPlot, QCPHistogram::sStep, Qt::red));
    QCOMPARE(drawn.size(), 1);
    QCOMPARE(drawn[0].fillVertexCount, 0);
    QVERIFY(drawn[0].strokeVertexCount > 0);
}

void TestHistogramPlottable::gpuAnAlmostTransparentFillStillDraws()
{
    if (!showWithRhi(mPlot))
        QSKIP("no QRhi available in this environment");
    const auto drawn = gpuEntries(mPlot, gpuHistogram(mPlot, QCPHistogram::sBars, QColor(255, 0, 0, 1)));
    QCOMPARE(drawn.size(), 1);
    QVERIFY(drawn[0].fillVertexCount > 0);
}
