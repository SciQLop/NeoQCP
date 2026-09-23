#include "test-color-by-scalar.h"
#include "qcustomplot.h"
#include "datasource/soa-multi-datasource.h"
#include "datasource/algorithms.h"
#include "datasource/row-major-multi-datasource.h"
#include "datasource/graph-resampler.h"
#include "datasource/resampled-multi-datasource.h"
#include "plottables/plottable-linestyle.h"
#include <any>
#include <cmath>

using SoA = QCPSoAMultiDataSource<std::vector<double>, std::vector<double>>;

void TestColorByScalar::init()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
}

void TestColorByScalar::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
}

std::shared_ptr<QCPAbstractMultiDataSource> TestColorByScalar::makeSource(
    std::vector<double> keys, std::vector<std::vector<double>> columns)
{
    return std::make_shared<SoA>(std::move(keys), std::move(columns));
}

void TestColorByScalar::setLineStyleInvalidatesLineCache()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setDataSource(makeSource({0, 1, 2, 3}, {{0, 1, 0, 1}}));
    mPlot->xAxis->setRange(0, 3);
    mPlot->yAxis->setRange(-1, 2);
    mPlot->replot();
    QVERIFY(!mg->mLineCacheDirty);

    mg->setLineStyle(QCPMultiGraph::lsStepLeft);
    QVERIFY(mg->mLineCacheDirty);
    QVERIFY(mg->mCachedLines.isEmpty());
}

namespace {

// Wraps a source and forwards only the pure virtuals, so the indexed calls
// hit QCPAbstractMultiDataSource's default implementations.
class ForwardingSource final : public QCPAbstractMultiDataSource
{
public:
    explicit ForwardingSource(std::shared_ptr<QCPAbstractMultiDataSource> inner)
        : mInner(std::move(inner)) {}
    int columnCount() const override { return mInner->columnCount(); }
    int size() const override { return mInner->size(); }
    double keyAt(int i) const override { return mInner->keyAt(i); }
    QCPRange keyRange(bool& f, QCP::SignDomain sd) const override { return mInner->keyRange(f, sd); }
    int findBegin(double k, bool e) const override { return mInner->findBegin(k, e); }
    int findEnd(double k, bool e) const override { return mInner->findEnd(k, e); }
    double valueAt(int c, int i) const override { return mInner->valueAt(c, i); }
    QCPRange valueRange(int c, bool& f, QCP::SignDomain sd, const QCPRange& r) const override
    { return mInner->valueRange(c, f, sd, r); }
    QVector<QPointF> getOptimizedLineData(int c, int b, int e, int w, QCPAxis* k, QCPAxis* v) const override
    { return mInner->getOptimizedLineData(c, b, e, w, k, v); }
    QVector<QPointF> getLines(int c, int b, int e, QCPAxis* k, QCPAxis* v) const override
    { return mInner->getLines(c, b, e, k, v); }
private:
    std::shared_ptr<QCPAbstractMultiDataSource> mInner;
};

// 9000 dense samples on [0, 45], a key gap, then 100 sparse samples on [55, 100];
// NaN values sprinkled in both parts. Exercises every branch of the adaptive path.
void mixedData(std::vector<double>& keys, std::vector<double>& values)
{
    for (int i = 0; i < 9000; ++i)
    {
        keys.push_back(45.0 * i / 8999.0);
        values.push_back(i % 997 == 0 ? std::nan("") : std::sin(i * 0.01) + 0.1 * std::sin(i * 1.3));
    }
    for (int i = 0; i < 100; ++i)
    {
        keys.push_back(55.0 + 45.0 * i / 99.0);
        values.push_back(i % 17 == 0 ? std::nan("") : std::cos(i * 0.2));
    }
}

void checkValuesComeFromIndices(const QVector<QPointF>& pts, const QVector<int>& idx,
                                const std::vector<double>& values, QCPAxis* valueAxis, bool valueIsX)
{
    QCOMPARE(idx.size(), pts.size());
    for (int k = 0; k < pts.size(); ++k)
    {
        if (idx[k] < 0)
        {
            QVERIFY(std::isnan(pts[k].x()) || std::isnan(pts[k].y()));
            continue;
        }
        const double expected = valueAxis->coordToPixel(values[idx[k]]);
        const double got = valueIsX ? pts[k].x() : pts[k].y();
        QVERIFY2(qAbs(got - expected) < 1e-6,
                 qPrintable(QString("point %1: index %2").arg(k).arg(idx[k])));
    }
}

} // namespace

void TestColorByScalar::linesToPixelsIndexedMatchesPlainAndMarksGaps()
{
    std::vector<double> keys {0, 1, 2, 3, 4, 10, 11, 12};
    std::vector<double> values {0, 1, 2, std::nan(""), 4, 5, 6, 7};
    mPlot->xAxis->setRange(0, 12);
    mPlot->yAxis->setRange(0, 8);
    mPlot->replot();

    QVector<int> idx;
    const auto indexed = qcp::algo::linesToPixelsIndexed(keys, values, 0, 8,
                                                         mPlot->xAxis, mPlot->yAxis, idx);
    const auto plain = qcp::algo::linesToPixels(keys, values, 0, 8, mPlot->xAxis, mPlot->yAxis);

    QCOMPARE(indexed.size(), plain.size());
    for (int k = 0; k < plain.size(); ++k)
        QVERIFY(indexed[k] == plain[k] || (std::isnan(indexed[k].x()) && std::isnan(plain[k].x())));
    QCOMPARE(idx, (QVector<int>{0, 1, 2, -1, 4, -1, 5, 6, 7}));
}

void TestColorByScalar::optimizedLineDataIndexedValuesComeFromTheirIndex()
{
    std::vector<double> keys, values;
    mixedData(keys, values);
    mPlot->xAxis->setRange(0, 100);
    mPlot->yAxis->setRange(-2, 2);
    mPlot->replot();
    const int n = static_cast<int>(keys.size());

    QVector<int> idx;
    const auto indexed = qcp::algo::optimizedLineDataIndexed(keys, values, 0, n, 400,
                                                             mPlot->xAxis, mPlot->yAxis, idx);
    const auto plain = qcp::algo::optimizedLineData(keys, values, 0, n, 400,
                                                    mPlot->xAxis, mPlot->yAxis);
    QCOMPARE(indexed.size(), plain.size());
    QVERIFY(indexed.size() < n);  // the adaptive path really ran
    checkValuesComeFromIndices(indexed, idx, values, mPlot->yAxis, false);
}

void TestColorByScalar::indexedVerticalKeyAxis()
{
    std::vector<double> keys, values;
    mixedData(keys, values);
    mPlot->yAxis->setRange(0, 100);   // key axis is vertical here
    mPlot->xAxis->setRange(-2, 2);
    mPlot->replot();
    const int n = static_cast<int>(keys.size());

    QVector<int> idx;
    const auto pts = qcp::algo::optimizedLineDataIndexed(keys, values, 0, n, 300,
                                                         mPlot->yAxis, mPlot->xAxis, idx);
    checkValuesComeFromIndices(pts, idx, values, mPlot->xAxis, true);

    QVector<int> idx2;
    const auto full = qcp::algo::linesToPixelsIndexed(keys, values, 0, n,
                                                      mPlot->yAxis, mPlot->xAxis, idx2);
    checkValuesComeFromIndices(full, idx2, values, mPlot->xAxis, true);
}

void TestColorByScalar::defaultIndexedImplementationMatchesSoA()
{
    std::vector<double> keys, values;
    mixedData(keys, values);
    auto soa = makeSource(keys, {values});
    ForwardingSource generic(soa);
    mPlot->xAxis->setRange(0, 100);
    mPlot->yAxis->setRange(-2, 2);
    mPlot->replot();
    const int n = static_cast<int>(keys.size());

    QVector<int> a, b;
    const auto gl = generic.getLinesIndexed(0, 0, n, mPlot->xAxis, mPlot->yAxis, a);
    const auto sl = soa->getLinesIndexed(0, 0, n, mPlot->xAxis, mPlot->yAxis, b);
    QCOMPARE(gl.size(), sl.size());
    QCOMPARE(a, b);

    QVector<int> c, d;
    const auto g = generic.getOptimizedLineDataIndexed(0, 100, 5000, 400, mPlot->xAxis, mPlot->yAxis, c);
    const auto s = soa->getOptimizedLineDataIndexed(0, 100, 5000, 400, mPlot->xAxis, mPlot->yAxis, d);
    QCOMPARE(g.size(), s.size());
    QCOMPARE(c, d);
}

void TestColorByScalar::rowMajorIndexedMatchesSoA()
{
    std::vector<double> keys, values;
    mixedData(keys, values);
    const int n = static_cast<int>(keys.size());
    std::vector<double> interleaved(2 * n);
    for (int i = 0; i < n; ++i)
    {
        interleaved[2 * i] = values[i];
        interleaved[2 * i + 1] = -values[i];
    }
    QCPRowMajorMultiDataSource<double, double> rowMajor(
        std::span<const double>(keys), interleaved.data(), n, 2, 2);
    std::vector<double> negated(values.size());
    std::transform(values.begin(), values.end(), negated.begin(), [](double v) { return -v; });
    auto soa = makeSource(keys, {values, negated});
    mPlot->xAxis->setRange(0, 100);
    mPlot->yAxis->setRange(-2, 2);
    mPlot->replot();

    for (int col = 0; col < 2; ++col)
    {
        QVector<int> a, b;
        rowMajor.getOptimizedLineDataIndexed(col, 0, n, 400, mPlot->xAxis, mPlot->yAxis, a);
        soa->getOptimizedLineDataIndexed(col, 0, n, 400, mPlot->xAxis, mPlot->yAxis, b);
        QCOMPARE(a, b);
        rowMajor.getLinesIndexed(col, 0, n, mPlot->xAxis, mPlot->yAxis, a);
        soa->getLinesIndexed(col, 0, n, mPlot->xAxis, mPlot->yAxis, b);
        QCOMPARE(a, b);
    }
}

namespace {

// n samples, two columns, keys 0..n with a hole in [0.3 n, 0.4 n) so some bins stay empty.
std::shared_ptr<QCPAbstractMultiDataSource> holeySource(int n)
{
    std::vector<double> keys, a, b;
    for (int i = 0; i < n; ++i)
    {
        if (i >= 3 * n / 10 && i < 4 * n / 10) continue;
        keys.push_back(i);
        a.push_back(std::sin(i * 0.001) + 0.3 * std::sin(i * 0.37));
        b.push_back(i % 101 == 0 ? std::nan("") : std::cos(i * 0.002));
    }
    return std::make_shared<SoA>(std::move(keys),
                                 std::vector<std::vector<double>>{std::move(a), std::move(b)});
}

void checkOrigin(const qcp::algo::MultiColumnBinResult& bins, const QCPAbstractMultiDataSource& src)
{
    const int s = bins.stride();
    QCOMPARE(static_cast<int>(bins.origin.size()), static_cast<int>(bins.values.size()));
    for (int c = 0; c < bins.numColumns; ++c)
        for (int r = 0; r < s; ++r)
        {
            const double v = bins.values[c * s + r];
            const int o = bins.origin[c * s + r];
            if (std::isnan(v)) { QCOMPARE(o, -1); continue; }
            QVERIFY(o >= 0);
            QCOMPARE(src.valueAt(c, o), v);
        }
}

} // namespace

void TestColorByScalar::l1OriginPointsAtEachBinsExtremes()
{
    auto src = holeySource(50'000);
    bool found = false;
    const auto range = src->keyRange(found);
    const auto bins = qcp::algo::binMinMaxMulti(*src, 0, src->size(), range, 1000, true);
    checkOrigin(bins, *src);
}

void TestColorByScalar::l1WithoutOriginBuildsNone()
{
    auto src = holeySource(50'000);
    bool found = false;
    const auto bins = qcp::algo::binMinMaxMulti(*src, 0, src->size(), src->keyRange(found), 1000);
    QVERIFY(bins.origin.empty());
    std::any cache;
    qcp::algo::buildL1CacheMulti(*src, ViewportParams{}, cache);
    QVERIFY(std::any_cast<qcp::algo::MultiGraphResamplerCache>(&cache)->level1.origin.empty());
}

void TestColorByScalar::l1ParallelOriginEqualsSerial()
{
    auto src = holeySource(1'300'000);   // above the 1M parallel threshold
    bool found = false;
    const auto range = src->keyRange(found);
    const auto serial = qcp::algo::binMinMaxMulti(*src, 0, src->size(), range, 20'000, true);
    const auto parallel = qcp::algo::binMinMaxMultiParallel(*src, 0, src->size(), range, 20'000, true);
    QCOMPARE(parallel.origin, serial.origin);
    checkOrigin(parallel, *src);
}

void TestColorByScalar::l2OriginComposesThroughL1AndCompacts()
{
    auto src = holeySource(200'000);
    std::any cache;
    qcp::algo::buildL1CacheMulti(*src, ViewportParams{}, cache, true);
    const auto* l1 = std::any_cast<qcp::algo::MultiGraphResamplerCache>(&cache);
    QVERIFY(l1 && !l1->level1.origin.empty());

    mPlot->xAxis->setRange(0, 200'000);
    mPlot->yAxis->setRange(-2, 2);
    mPlot->replot();
    ViewportParams vp;
    vp.keyRange = mPlot->xAxis->range();
    vp.valueRange = mPlot->yAxis->range();
    vp.plotWidthPx = 100;   // 400 L2 bins, far fewer than the visible L1 rows
    const auto l2 = qcp::algo::resampleL2Multi(*l1, vp);
    QVERIFY(l2);
    // The hole leaves L1 bins whose values are NaN; L2 skips NaN rows, so the ~40 L2 bins
    // covering only the hole receive no data and are compacted away (720 rows, not 800).
    QVERIFY(l2->size() < 800);

    for (int c = 0; c < 2; ++c)
    {
        QVector<int> idx;
        const auto pts = l2->getLinesIndexed(c, 0, l2->size(), mPlot->xAxis, mPlot->yAxis, idx);
        QCOMPARE(idx.size(), pts.size());
        int gapMarkers = 0;
        for (int k = 0; k < pts.size(); ++k)
        {
            if (idx[k] < 0) { ++gapMarkers; QVERIFY(std::isnan(pts[k].y())); continue; }
            QVERIFY(qAbs(pts[k].y() - mPlot->yAxis->coordToPixel(src->valueAt(c, idx[k]))) < 1e-6);
        }
        QVERIFY(gapMarkers >= 1);   // the hole is a key gap in L2
    }
}

void TestColorByScalar::stepIndexMapsFollowTheTransforms()
{
    const QVector<int> d {10, 20, 30};
    QCOMPARE(qcp::stepLeftIndices(d),   (QVector<int>{10, 10, 10, 20, 20, 30}));
    QCOMPARE(qcp::stepRightIndices(d),  (QVector<int>{10, 10, 20, 20, 30, 30}));
    QCOMPARE(qcp::stepCenterIndices(d), (QVector<int>{10, 10, 20, 20, 30, 30}));
    QCOMPARE(qcp::impulseIndices(d),    (QVector<int>{10, 10, 20, 20, 30, 30}));
    QCOMPARE(qcp::stepLeftIndices({7}), (QVector<int>{7}));   // < 2 points: returned as is

    // Sizes match the point transforms, gaps (-1) travel with their positions.
    QVector<QPointF> pts {{0, 0}, {1, 5}, {2, 2}, {3, 7}};
    const QVector<int> di {0, -1, 2, 3};
    QCOMPARE(qcp::stepLeftIndices(di).size(),   qcp::toStepLeftLines(pts, false).size());
    QCOMPARE(qcp::stepRightIndices(di).size(),  qcp::toStepRightLines(pts, false).size());
    QCOMPARE(qcp::stepCenterIndices(di).size(), qcp::toStepCenterLines(pts, false).size());
    QCOMPARE(qcp::impulseIndices(di).size(),    qcp::toImpulseLines(pts, false, 0).size());
}
