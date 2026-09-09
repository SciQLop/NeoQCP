#include "test-data-swap.h"
#include "qcustomplot.h"

namespace {
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
    mPlot->setDataSwapDebounceMs(50);
    QSignalSpy replots(mPlot, &QCustomPlot::afterReplot);

    mPlot->requestDataSwap();
    QTest::qWait(10);
    mPlot->requestDataSwap();
    QCOMPARE(a->commits, 0);
    QCOMPARE(b->commits, 0);

    QTRY_COMPARE_WITH_TIMEOUT(a->commits, 1, 2000);
    QCOMPARE(b->commits, 1);
    QVERIFY(replots.count() >= 1);
    QTest::qWait(120);
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
