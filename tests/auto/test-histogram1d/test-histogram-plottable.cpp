#include "test-histogram-plottable.h"
#include "qcustomplot.h"
#include "plottables/plottable-histogram.h"
#include "plottables/histogram-geometry.h"
#include "painting/plottable-rhi-layer.h"
#include "datasource/soa-multi-datasource.h"
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
    QCOMPARE(h->histogram()->edges.front(), 1.0);
    QCOMPARE(h->histogram()->edges.back(), 1000.0);
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

void TestHistogramPlottable::exportRightAfterAScaleChangeBinsForTheNewScale()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBins(3);
    h->setDataSource(values({1, 10, 100, 1000}));
    QVERIFY(waitForResult(h));
    QVERIFY(settle(h));
    h->setKeyAxis(mPlot->xAxis2);
    mPlot->xAxis2->setScaleType(QCPAxis::stLogarithmic);
    QVERIFY(!mPlot->toPixmap(400, 300).isNull());
    QVERIFY(h->histogram()->log);
    QCOMPARE(h->histogram()->edges.front(), 1.0);
    QCOMPARE(h->histogram()->edges.back(), 1000.0);
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
