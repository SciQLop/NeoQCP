#include "test-data-swap.h"
#include "qcustomplot.h"
#include "plottables/plottable-multigraph.h"
#include <cmath>
#include <vector>

namespace {
std::vector<double> ramp(int n, double x0 = 0.0)
{
    std::vector<double> v(n);
    for (int i = 0; i < n; ++i)
        v[i] = x0 + i;
    return v;
}
std::vector<std::vector<double>> sines(int n, int cols)
{
    std::vector<std::vector<double>> out(cols, std::vector<double>(n));
    for (int c = 0; c < cols; ++c)
        for (int i = 0; i < n; ++i)
            out[c][i] = std::sin(i * 0.01 * (c + 1));
    return out;
}

// Minimal plottable that always has pending data and counts commits.
class PendingStub : public QCPAbstractPlottable
{
public:
    PendingStub(QCPAxis* k, QCPAxis* v) : QCPAbstractPlottable(k, v) {}
    int commits = 0;
    bool pending = true;

    bool hasPendingData() const override { return pending; }
    void commitPendingData() override { ++commits; }
    void notifyBusy() { updateEffectiveBusy(); }

    double selectTest(const QPointF&, bool, QVariant* = nullptr) const override { return -1; }
    QCPRange getKeyRange(bool& found, QCP::SignDomain = QCP::sdBoth) const override
    { found = false; return {}; }
    QCPRange getValueRange(bool& found, QCP::SignDomain = QCP::sdBoth,
                           const QCPRange& = QCPRange()) const override
    { found = false; return {}; }
    void draw(QCPPainter*) override {}
    void drawLegendIcon(QCPPainter*, const QRectF&) const override {}
};
} // namespace

void TestDataSwap::init()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
}

void TestDataSwap::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
}

void TestDataSwap::pendingCountsAsBusy()
{
    auto* p = new PendingStub(mPlot->xAxis, mPlot->yAxis);
    p->notifyBusy();
    QVERIFY(p->busy());
    p->pending = false;
    p->notifyBusy();
    QVERIFY(!p->busy());
}

void TestDataSwap::requestsWithinWindowCommitOnce()
{
    auto* a = new PendingStub(mPlot->xAxis, mPlot->yAxis);
    auto* b = new PendingStub(mPlot->xAxis, mPlot->yAxis);
    mPlot->setDataSwapDebounceMs(200);
    QSignalSpy replots(mPlot, &QCustomPlot::afterReplot);

    QElapsedTimer clock;
    clock.start();
    mPlot->requestDataSwap();
    QTest::qWait(120);
    mPlot->requestDataSwap();          // rides along, must NOT restart the window
    QCOMPARE(a->commits, 0);
    QCOMPARE(b->commits, 0);

    // Leading edge: the commit lands ~200 ms after the FIRST request. A
    // restarting timer would land at ~320 ms and miss this deadline.
    QTRY_COMPARE_WITH_TIMEOUT(a->commits, 1, 2000);
    QVERIFY2(clock.elapsed() < 280, qPrintable(QString::number(clock.elapsed())));
    QCOMPARE(b->commits, 1);
    QVERIFY(replots.count() >= 1);

    QTest::qWait(250);                  // no second firing from the second request
    QCOMPARE(a->commits, 1);
    QCOMPARE(b->commits, 1);
}

void TestDataSwap::requestAfterWindowCommitsAgain()
{
    auto* a = new PendingStub(mPlot->xAxis, mPlot->yAxis);
    mPlot->setDataSwapDebounceMs(20);
    mPlot->requestDataSwap();
    QTRY_COMPARE_WITH_TIMEOUT(a->commits, 1, 2000);
    mPlot->requestDataSwap();
    QTRY_COMPARE_WITH_TIMEOUT(a->commits, 2, 2000);
}

void TestDataSwap::multiGraphKeepsTranslationWhileDataPending()
{
    // Large data so the L1 cache is built asynchronously.
    constexpr int n = 120'000;
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(ramp(n), sines(n, 2));
    mPlot->xAxis->setRange(0, n);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTRY_VERIFY_WITH_TIMEOUT(!mg->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(mg->hasRenderedRange());

    mPlot->setDataSwapDebounceMs(200);
    mg->setData(ramp(n, 1000.0), sines(n, 2));
    QVERIFY(mg->hasPendingData());
    QVERIFY(mg->busy());
    QCOMPARE(mg->componentCount(), 2);

    // Displayed geometry untouched: a small pan still translates.
    mPlot->xAxis->setRange(50, n + 50);
    QVERIFY(!mg->stallPixelOffset().isNull());
    QVERIFY(mPlot->layer("main")->canSkipRepaintForTranslation());
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(mg->hasPendingData());
    QCOMPARE(mg->dataSource()->keyAt(0), 0.0);
}

void TestDataSwap::multiGraphCommitsAfterWindow()
{
    constexpr int n = 120'000;
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(ramp(n), sines(n, 2));
    mPlot->xAxis->setRange(0, n);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTRY_VERIFY_WITH_TIMEOUT(!mg->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    mPlot->setDataSwapDebounceMs(30);
    QSignalSpy replots(mPlot, &QCustomPlot::afterReplot);
    mg->setData(ramp(n, 1000.0), sines(n, 2));
    QVERIFY(mg->hasPendingData());

    QTRY_VERIFY_WITH_TIMEOUT(!mg->hasPendingData(), 5000);
    QCOMPARE(mg->dataSource()->keyAt(0), 1000.0);
    QVERIFY(replots.count() >= 1);
    QTRY_VERIFY_WITH_TIMEOUT(!mg->busy(), 5000);
    // The committed source draws from its own resampled cache: the next replot
    // must not fall back to the "pipeline active, no L1" early return.
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(mg->hasRenderedRange());
}

void TestDataSwap::multiGraphFirstDataCommitsImmediately()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(ramp(10), sines(10, 1));
    QVERIFY(!mg->hasPendingData());
    QVERIFY(mg->dataSource());
    QCOMPARE(mg->componentCount(), 1);
}

void TestDataSwap::multiGraphLatestPendingWins()
{
    constexpr int n = 120'000;
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(ramp(n), sines(n, 2));
    mPlot->xAxis->setRange(0, n);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTRY_VERIFY_WITH_TIMEOUT(!mg->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    mPlot->setDataSwapDebounceMs(30);
    mg->setData(ramp(n, 1000.0), sines(n, 2));
    mg->setData(ramp(n, 2000.0), sines(n, 3));
    QCOMPARE(mg->componentCount(), 3);

    QTRY_VERIFY_WITH_TIMEOUT(!mg->hasPendingData(), 5000);
    QCOMPARE(mg->dataSource()->keyAt(0), 2000.0);
    QCOMPARE(mg->dataSource()->columnCount(), 3);
}
