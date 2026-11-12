#include "test-color-by-scalar.h"
#include "datasource/algorithms.h"
#include "datasource/graph-resampler.h"
#include "datasource/resampled-multi-datasource.h"
#include "datasource/row-major-multi-datasource.h"
#include "datasource/soa-multi-datasource.h"
#include "misuse.h"
#include "painting/line-extruder.h"
#include "painting/scatter-rhi-layer.h"
#include "painting/viewport-offset.h"
#include "plottables/plottable-color-mapper.h"
#include "plottables/plottable-color-runs.h"
#include "plottables/plottable-draw-utils.h"
#include "plottables/plottable-linestyle.h"
#include "qcustomplot.h"
#include "scatterstyle.h"
#include <any>
#include <cmath>
#include <numeric>

using SoA = QCPSoAMultiDataSource<std::vector<double>, std::vector<double>>;

namespace
{
int gRefusals = 0;

void countRefusal(const char*, const char*) { ++gRefusals; }

//! Runs \a refused and returns the number of misuse reports it made.
template <typename F>
int refusalsIn(F&& refused)
{
    const auto previous = qcp::setMisuseHandler(countRefusal);
    gRefusals = 0;
    refused();
    qcp::setMisuseHandler(previous);
    return gRefusals;
}
} // namespace

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

namespace
{
QColor pixelAt(QCustomPlot* plot, const QImage& img, double key, double value);
bool isRed(const QColor& c);
} // namespace

std::shared_ptr<QCPAbstractMultiDataSource>
TestColorByScalar::makeSource(std::vector<double> keys, std::vector<std::vector<double>> columns)
{
    return std::make_shared<SoA>(std::move(keys), std::move(columns));
}

void TestColorByScalar::setLineStyleInvalidatesLineCache()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setDataSource(makeSource({ 0, 1, 2, 3 }, { { 0, 1, 0, 1 } }));
    mg->setComponentPens({ QPen(Qt::red, 6) });
    mPlot->xAxis->setRange(0, 3);
    mPlot->yAxis->setRange(-1, 2);
    mPlot->replot();
    QVERIFY(mg->hasRenderedRange());
    const int recordings = mg->mCore.recordingCount();
    const QImage diagonal = mPlot->toPixmap(400, 300).toImage();
    QVERIFY2(isRed(pixelAt(mPlot, diagonal, 0.75, 0.75)), "the diagonal line is missing");

    mg->setLineStyle(QCPMultiGraph::lsStepLeft);
    mPlot->replot();
    QVERIFY2(mg->mCore.recordingCount() > recordings, "the new line style was never recorded");

    const QImage stepped = mPlot->toPixmap(400, 300).toImage();
    QVERIFY2(!isRed(pixelAt(mPlot, stepped, 0.75, 0.75)), "the diagonal line is still drawn");
    QVERIFY2(isRed(pixelAt(mPlot, stepped, 0.75, 0.0)),
             "the step does not hold the previous value");
}

namespace {

// Wraps a source and forwards only the pure virtuals, so the indexed calls
// hit QCPAbstractMultiDataSource's default implementations.
class ForwardingSource final : public QCPAbstractMultiDataSource
{
public:
    explicit ForwardingSource(std::shared_ptr<QCPAbstractMultiDataSource> inner)
        : mInner(std::move(inner)) {}
    // Stands in for a source mutated in place (the graph is told via dataChanged()).
    void setInner(std::shared_ptr<QCPAbstractMultiDataSource> inner) { mInner = std::move(inner); }
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

void TestColorByScalar::defaultIndexedImplementationHandlesAnEmptyWindow()
{
    ForwardingSource generic(makeSource({0, 1, 2, 3}, {{0, 1, 0, 1}}));
    mPlot->xAxis->setRange(0, 3);
    mPlot->yAxis->setRange(-1, 2);
    mPlot->replot();
    for (auto [begin, end] : {std::pair{3, 1}, std::pair{2, 2}})
    {
        QVector<int> a{7}, b{7};
        QVERIFY(generic.getLinesIndexed(0, begin, end, mPlot->xAxis, mPlot->yAxis, a).isEmpty());
        QVERIFY(generic.getOptimizedLineDataIndexed(0, begin, end, 400, mPlot->xAxis, mPlot->yAxis, b).isEmpty());
        QVERIFY(a.isEmpty());
        QVERIFY(b.isEmpty());
    }
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
    const auto l2 = qcp::algo::resampleL2Multi(*l1, vp, *src);
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

void TestColorByScalar::mapperBucketsLinearLogNaNAndGaps()
{
    qcp::ColorScalarMapper m;
    m.setValues(std::make_shared<const std::vector<double>>(
        std::vector<double>{0.0, 0.5, 1.0, 2.0, -1.0, std::nan(""), 10.0, 100.0}));
    m.setRange(QCPRange(0, 1));
    QCOMPARE(m.bucket(0), 0);
    QCOMPARE(m.bucket(1), 128);
    QCOMPARE(m.bucket(2), 255);
    QCOMPARE(m.bucket(3), 255);                        // clamped
    QCOMPARE(m.bucket(4), 0);                          // clamped
    QCOMPARE(m.bucket(5), qcp::ColorScalarMapper::kGap);   // NaN
    QCOMPARE(m.bucket(-1), qcp::ColorScalarMapper::kGap);  // gap marker
    QCOMPARE(m.bucket(99), qcp::ColorScalarMapper::kGap);  // out of range index

    m.setScaleType(QCPAxis::stLogarithmic);
    m.setRange(QCPRange(1, 100));
    QCOMPARE(m.bucket(6), 128);                        // 10 is half way on log [1, 100]
    QCOMPARE(m.bucket(4), qcp::ColorScalarMapper::kGap);   // non-positive on log
    QCOMPARE(m.bucket(0), qcp::ColorScalarMapper::kGap);

    m.setScaleType(QCPAxis::stLinear);
    m.setRange(QCPRange(3, 3));                        // degenerate: every finite value -> 0
    QCOMPARE(m.bucket(1), 0);

    QCPColorGradient redToBlue;
    redToBlue.clearColorStops();
    redToBlue.setColorStopAt(0, Qt::red);
    redToBlue.setColorStopAt(1, Qt::blue);
    m.setGradient(redToBlue);
    QCOMPARE(QColor::fromRgba(m.color(0)), QColor(Qt::red));
    QCOMPARE(QColor::fromRgba(m.color(255)), QColor(Qt::blue));
}

void TestColorByScalar::mapperGenerationBumpsOnEverySetter()
{
    qcp::ColorScalarMapper m;
    auto g = m.generation();
    m.setValues(std::make_shared<const std::vector<double>>(std::vector<double>{1}));
    QVERIFY(m.generation() > g); g = m.generation();
    m.setGradient(QCPColorGradient(QCPColorGradient::gpHot));
    QVERIFY(m.generation() > g); g = m.generation();
    m.setRange(QCPRange(0, 2));
    QVERIFY(m.generation() > g); g = m.generation();
    m.setScaleType(QCPAxis::stLogarithmic);
    QVERIFY(m.generation() > g); g = m.generation();
    m.clearValues();
    QVERIFY(m.generation() > g);
}

void TestColorByScalar::colorValuesOfTheWrongLengthAreRefused()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setColorValues(std::vector<double>{1, 2, 3});      // no data yet
    QVERIFY(!mg->hasColorValues());
    mg->setDataSource(makeSource({0, 1, 2, 3}, {{0, 1, 0, 1}}));
    QCOMPARE(refusalsIn([&] { mg->setColorValues(std::vector<double>{1, 2, 3}); }), 1); // 3 != 4
    QVERIFY(!mg->hasColorValues());
    mg->setColorValues(std::vector<double>{1, 2, 3, 4});
    QVERIFY(mg->hasColorValues());
}

void TestColorByScalar::sameSizeDataRefreshKeepsColorValues()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setDataSource(makeSource({0, 1, 2, 3}, {{0, 1, 0, 1}}));
    mg->setColorValues(std::vector<double>{1, 2, 3, 4});
    mg->setDataSource(makeSource({0, 1, 2, 3}, {{5, 6, 7, 8}}));
    QVERIFY(mg->hasColorValues());
    mg->setDataSource(makeSource({0, 1, 2}, {{5, 6, 7}}));
    QVERIFY(!mg->hasColorValues());
}

void TestColorByScalar::inPlaceResizeAppliesTheColorSizeRule()
{
    auto src = std::make_shared<ForwardingSource>(makeSource({0, 1, 2, 3}, {{0, 1, 0, 1}}));
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setDataSource(src);
    mg->setColorValues(std::vector<double>{1, 2, 3, 4});

    src->setInner(makeSource({0, 1, 2, 3}, {{5, 6, 7, 8}}));
    mg->dataChanged();
    QVERIFY(mg->hasColorValues());
    QVERIFY(mg->mWantOrigin->load());

    src->setInner(makeSource({0, 1, 2, 3, 4}, {{5, 6, 7, 8, 9}}));   // appended in place
    mg->dataChanged();
    QVERIFY(!mg->hasColorValues());
    QVERIFY(!mg->mWantOrigin->load());
}

void TestColorByScalar::firstColoringRebuildsL1OnceWithOrigin()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    const int n = 200'000;   // above kResampleThreshold: the async L1 pipeline runs
    std::vector<double> keys(n), values(n), scalar(n);
    for (int i = 0; i < n; ++i) { keys[i] = i; values[i] = std::sin(i * 0.001); scalar[i] = i; }
    mg->setDataSource(makeSource(keys, {values}));
    mPlot->xAxis->setRange(0, n);
    mPlot->replot();
    QTRY_VERIFY_WITH_TIMEOUT(mg->mL1Cache != nullptr, 5000);
    QVERIFY(mg->mL1Cache->level1.origin.empty());     // uncoloured: no origin built

    const auto uncolouredL1 = mg->mL1Cache;
    mg->setColorValues(scalar);
    mPlot->replot();
    QTRY_VERIFY_WITH_TIMEOUT(mg->mL1Cache != uncolouredL1, 5000);
    QVERIFY(!mg->mL1Cache->level1.origin.empty());

    const auto colouredL1 = mg->mL1Cache;
    mg->setGradient(QCPColorGradient(QCPColorGradient::gpHot));
    mg->setDataRange(QCPRange(0, 10));
    mg->setDataScaleType(QCPAxis::stLogarithmic);
    std::vector<double> other(n, 1.0);
    mg->setColorValues(other);                        // same length: no rebuild either
    QTest::qWait(200);
    QCOMPARE(mg->mL1Cache, colouredL1);
}

void TestColorByScalar::deferredSetColorValuesUsesThePendingSourceSize()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setDeferredDataSwap(true);
    mg->setDataSource(makeSource({0, 1, 2, 3}, {{0, 1, 0, 1}}));   // n = 4, displayed
    mPlot->xAxis->setRange(0, 3);
    mPlot->yAxis->setRange(-1, 2);
    mPlot->replot();
    QVERIFY(mg->hasRenderedRange());

    mg->setDataSource(makeSource({0, 1, 2, 3, 4}, {{0, 1, 0, 1, 0}}));  // m = 5, deferred
    QVERIFY(mg->hasPendingData());

    // Sized for the pending (5) source, not the still-displayed (4) one: must be accepted.
    mg->setColorValues(std::vector<double>{1, 2, 3, 4, 5});
    QVERIFY(mg->commitPendingData());
    QVERIFY(!mg->hasPendingData());
    QVERIFY(mg->hasColorValues());
    QCOMPARE(mg->mColor.size(), 5);
}

// Turning the deferred swap off applies a source still being prepared: the colours set for it
// must come along, not be dropped with the stash.
void TestColorByScalar::turningTheDeferredSwapOffKeepsThePendingColours()
{
    const auto big = [this](int n)
    {
        std::vector<double> keys(n), values(n);
        for (int i = 0; i < n; ++i)
        {
            keys[i] = i;
            values[i] = i % 2;
        }
        return makeSource(std::move(keys), { std::move(values) });
    };
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setDeferredDataSwap(true);
    mg->setDataSource(big(4));
    mPlot->xAxis->setRange(0, 3);
    mPlot->yAxis->setRange(-1, 2);
    mPlot->replot();
    QVERIFY(mg->hasRenderedRange());

    constexpr int m = 150'000; // resampled: not ready until the worker's result is delivered
    mg->setDataSource(big(m));
    QVERIFY(mg->hasPendingData());
    mg->setColorValues(std::vector<double>(m, 7.0));

    mg->setDeferredDataSwap(false);
    QVERIFY(!mg->hasPendingData());
    QVERIFY(mg->hasColorValues());
    QCOMPARE(mg->mColor.size(), m);
}

void TestColorByScalar::deferredCommitAppliesTheColorSizeRule()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setDeferredDataSwap(true);
    mg->setDataSource(makeSource({0, 1, 2, 3}, {{0, 1, 0, 1}}));
    mg->setColorValues(std::vector<double>{1, 2, 3, 4});
    mPlot->xAxis->setRange(0, 3);
    mPlot->yAxis->setRange(-1, 2);
    mPlot->replot();
    QVERIFY(mg->hasRenderedRange());
    QVERIFY(mg->hasColorValues());

    // Same size, no new colour values supplied while pending: kept through the commit.
    mg->setDataSource(makeSource({0, 1, 2, 3}, {{5, 6, 7, 8}}));
    QVERIFY(mg->hasPendingData());
    QVERIFY(mg->commitPendingData());
    QVERIFY(mg->hasColorValues());
    QVERIFY(mg->mWantOrigin->load());

    mPlot->replot();
    QVERIFY(mg->hasRenderedRange());

    // Different size, no new colour values supplied: dropped on commit, want-origin reset.
    mg->setDataSource(makeSource({0, 1, 2}, {{5, 6, 7}}));
    QVERIFY(mg->hasPendingData());
    QVERIFY(mg->commitPendingData());
    QVERIFY(!mg->hasColorValues());
    QVERIFY(!mg->mWantOrigin->load());
}

void TestColorByScalar::deferredCommitJoinsTheRectScale()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setDeferredDataSwap(true);
    mg->setDataSource(makeSource({ 0, 1, 2, 3 }, { { 0, 1, 0, 1 } }));
    mPlot->xAxis->setRange(0, 3);
    mPlot->yAxis->setRange(-1, 2);
    mPlot->replot();
    QVERIFY(mg->hasRenderedRange());
    QVERIFY(!mg->colorScale());

    // New data staged, then colour values for it: the first colouring happens on commit.
    mg->setDataSource(makeSource({ 0, 1, 2, 3 }, { { 1, 0, 1, 0 } }));
    QVERIFY(mg->hasPendingData());
    mg->setColorValues(std::vector<double> { 0, 1, 0, 1 });
    QVERIFY(mg->commitPendingData());

    QCPColorScale* scale = mg->colorScale();
    QVERIFY2(scale, "the deferred commit did not join the rect's scale");
    QCOMPARE(scale, mPlot->axisRect()->colorScale().get());

    scale->setGradient(QCPColorGradient(QCPColorGradient::gpHot));
    QCOMPARE(mg->gradient(), QCPColorGradient(QCPColorGradient::gpHot));
    QCOMPARE(mg->mColor.gradient(), QCPColorGradient(QCPColorGradient::gpHot));
}

void TestColorByScalar::toBucketClampsHugeAndInfinitePositions()
{
    qcp::ColorScalarMapper huge;
    huge.setRange(QCPRange(0, 1));
    huge.setValues(std::make_shared<const std::vector<double>>(std::vector<double>{1e20}));
    QCOMPARE(huge.bucket(0), 255);

    // A finite value over a vanishingly small span overflows the division to +inf;
    // toBucket must clamp the position before lround(), not clamp the already
    // out-of-range int that a raw lround(+inf * 255) would produce.
    qcp::ColorScalarMapper overflow;
    overflow.setRange(QCPRange(0, 1e-300));
    overflow.setValues(std::make_shared<const std::vector<double>>(std::vector<double>{1e300}));
    QCOMPARE(overflow.bucket(0), 255);
}

void TestColorByScalar::wrongLengthOnColouredGraphLeavesItUncoloured()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setDataSource(makeSource({0, 1, 2, 3}, {{0, 1, 0, 1}}));
    mg->setColorValues(std::vector<double>{1, 2, 3, 4});
    QVERIFY(mg->hasColorValues());

    QCOMPARE(refusalsIn([&] { mg->setColorValues(std::vector<double>{1, 2, 3}); }), 1);
    QVERIFY(!mg->hasColorValues());
}

void TestColorByScalar::uncolourRoutesDiscardAPendingStash()
{
    // Wrong length while pending: the stash from the earlier, valid call must not
    // survive to re-colour the graph once the pending source commits.
    {
        auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
        mg->setDeferredDataSwap(true);
        mg->setDataSource(makeSource({0, 1, 2, 3}, {{0, 1, 0, 1}}));   // n = 4, displayed
        mPlot->xAxis->setRange(0, 3);
        mPlot->yAxis->setRange(-1, 2);
        mPlot->replot();
        QVERIFY(mg->hasRenderedRange());

        mg->setDataSource(makeSource({0, 1, 2, 3, 4}, {{0, 1, 0, 1, 0}}));  // m = 5, deferred
        QVERIFY(mg->hasPendingData());

        mg->setColorValues(std::vector<double>{1, 2, 3, 4, 5});   // sized for the pending: stashed
        QCOMPARE(refusalsIn([&] {
                     mg->setColorValues(std::vector<double>{1, 2, 3}); // wrong length: discards the stash
                 }),
                 1);

        QVERIFY(mg->commitPendingData());
        QVERIFY(!mg->hasColorValues());
    }

    // Empty vector while pending: same "uncolour" route, through clearColorValues()'s
    // early-return path (nothing in mColor yet, only a pending stash).
    {
        auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
        mg->setDeferredDataSwap(true);
        mg->setDataSource(makeSource({0, 1, 2, 3}, {{0, 1, 0, 1}}));
        mPlot->xAxis->setRange(0, 3);
        mPlot->yAxis->setRange(-1, 2);
        mPlot->replot();
        QVERIFY(mg->hasRenderedRange());

        mg->setDataSource(makeSource({0, 1, 2, 3, 4}, {{0, 1, 0, 1, 0}}));
        QVERIFY(mg->hasPendingData());

        mg->setColorValues(std::vector<double>{1, 2, 3, 4, 5});   // stashed
        mg->setColorValues(std::vector<double>{});                // empty: clearColorValues()

        QVERIFY(mg->commitPendingData());
        QVERIFY(!mg->hasColorValues());
    }
}

namespace {

QCPColorGradient redToBlue()
{
    QCPColorGradient g;
    g.clearColorStops();
    g.setColorStopAt(0, Qt::red);
    g.setColorStopAt(1, Qt::blue);
    return g;
}

qcp::ColorScalarMapper mapperFor(std::vector<double> values, QCPRange range)
{
    qcp::ColorScalarMapper m;
    m.setGradient(redToBlue());
    m.setRange(range);
    m.setValues(std::make_shared<const std::vector<double>>(std::move(values)));
    return m;
}

} // namespace

void TestColorByScalar::coloredLineRendersTheGradient()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    std::vector<double> keys(200), values(200, 0.0), scalar(200);
    for (int i = 0; i < 200; ++i) { keys[i] = i; scalar[i] = i / 199.0; }
    mg->setDataSource(makeSource(keys, {values}));
    mg->setComponentPens({QPen(Qt::black, 6)});
    mg->setGradient(redToBlue());
    mg->setDataRange(QCPRange(0, 1));
    mg->setColorValues(scalar);
    mPlot->xAxis->setRange(0, 199);
    mPlot->yAxis->setRange(-1, 1);
    const QImage img = mPlot->toPixmap(400, 300).toImage();

    const int y = qRound(mPlot->yAxis->coordToPixel(0));
    const QColor left = img.pixelColor(qRound(mPlot->xAxis->coordToPixel(10)), y);
    const QColor right = img.pixelColor(qRound(mPlot->xAxis->coordToPixel(189)), y);
    QVERIFY2(left.red() > 150 && left.blue() < 100, qPrintable(left.name()));
    QVERIFY2(right.blue() > 150 && right.red() < 100, qPrintable(right.name()));
}

void TestColorByScalar::coloredRunsMeetWithButtEndsOnExport()
{
    // Two runs meet at key 1: red (segment 0->1), then blue (segment 1->2).
    // A square cap on the blue run would overpaint pen/2 of the red run.
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setDataSource(makeSource({0, 1, 2}, {{0, 0, 0}}));
    mg->setComponentPens({QPen(Qt::black, 20)});
    mg->setGradient(redToBlue());
    mg->setDataRange(QCPRange(0, 1));
    mg->setColorValues(std::vector<double>{0, 0, 1});
    mPlot->xAxis->setRange(0, 2);
    mPlot->yAxis->setRange(-1, 1);
    const QImage img = mPlot->toPixmap(400, 300).toImage();

    const int y = qRound(mPlot->yAxis->coordToPixel(0));
    const double boundary = mPlot->xAxis->coordToPixel(1);
    for (int dx : {3, 5, 7})
    {
        const QColor before = img.pixelColor(qRound(boundary) - dx, y);
        QVERIFY2(before.red() > 150 && before.blue() < 100,
                 qPrintable(QString("dx %1: %2").arg(dx).arg(before.name())));
    }
}

namespace {

QCPMultiGraph* rampGraph(QCustomPlot* plot, std::shared_ptr<QCPAbstractMultiDataSource> src,
                         std::vector<double> scalar)
{
    auto* mg = new QCPMultiGraph(plot->xAxis, plot->yAxis);
    mg->setDataSource(std::move(src));
    mg->setComponentPens({QPen(Qt::black, 6)});
    mg->setGradient(redToBlue());
    mg->setDataRange(QCPRange(0, 1));
    mg->setColorValues(std::move(scalar));
    plot->xAxis->setRange(0, 199);
    plot->yAxis->setRange(-1, 1);
    return mg;
}

std::vector<double> ramp(int n)
{
    std::vector<double> s(n);
    for (int i = 0; i < n; ++i) s[i] = i / double(n - 1);
    return s;
}

QColor pixelAt(QCustomPlot* plot, const QImage& img, double key, double value)
{
    return img.pixelColor(qRound(plot->xAxis->coordToPixel(key)),
                          qRound(plot->yAxis->coordToPixel(value)));
}

bool isRed(const QColor& c) { return c.red() > 150 && c.blue() < 100; }
bool isBlue(const QColor& c) { return c.blue() > 150 && c.red() < 100; }

} // namespace

void TestColorByScalar::coloredDashedLineRendersTheGradient()
{
    std::vector<double> keys(200), values(200, 0.0);
    std::iota(keys.begin(), keys.end(), 0.0);
    auto* mg = rampGraph(mPlot, makeSource(keys, {values}), ramp(200));
    mg->setComponentPens({QPen(QBrush(Qt::black), 6, Qt::DashLine)});
    const QImage img = mPlot->toPixmap(400, 300).toImage();
    // A dash may fall on the probe: look at a few pixels around it.
    auto anyRed = [&](double key) { for (int d = 0; d < 6; ++d) if (isRed(pixelAt(mPlot, img, key + d, 0))) return true; return false; };
    auto anyBlue = [&](double key) { for (int d = 0; d < 6; ++d) if (isBlue(pixelAt(mPlot, img, key - d, 0))) return true; return false; };
    QVERIFY(anyRed(10));
    QVERIFY(anyBlue(189));
}

void TestColorByScalar::coloredStepLineRendersTheGradient()
{
    std::vector<double> keys(200), values(200, 0.0);
    std::iota(keys.begin(), keys.end(), 0.0);
    auto* mg = rampGraph(mPlot, makeSource(keys, {values}), ramp(200));
    mg->setLineStyle(QCPMultiGraph::lsStepLeft);
    const QImage img = mPlot->toPixmap(400, 300).toImage();
    QVERIFY(isRed(pixelAt(mPlot, img, 10, 0)));
    QVERIFY(isBlue(pixelAt(mPlot, img, 189, 0)));
}

void TestColorByScalar::coloredImpulsesRenderTheGradient()
{
    std::vector<double> keys(20), values(20, 0.8);
    for (int i = 0; i < 20; ++i) keys[i] = i * 10.0;
    auto* mg = rampGraph(mPlot, makeSource(keys, {values}), ramp(20));
    mg->setLineStyle(QCPMultiGraph::lsImpulse);
    // Key 0 is also the x-axis range's lower bound, so it maps to the pixel column the
    // y-axis baseline is drawn on (axes paint over the graph: see the "axes"/"main" layer
    // order in QCustomPlot's constructor). Widen the range so the first impulse renders
    // clear of the axis line; this doesn't change what's asserted, only where it's sampled.
    mPlot->xAxis->setRange(-10, 199);
    const QImage img = mPlot->toPixmap(400, 300).toImage();
    QVERIFY(isRed(pixelAt(mPlot, img, 0, 0.4)));
    QVERIFY(isBlue(pixelAt(mPlot, img, 190, 0.4)));
}

void TestColorByScalar::coloredImpulsesFollowThePanOffset()
{
    // A pure pan is served by translating the recording: the impulses must land on the
    // panned pixels, not stay where the previous view drew them.
    std::vector<double> keys(20), values(20, 0.8);
    for (int i = 0; i < 20; ++i) keys[i] = i * 10.0;
    auto* mg = rampGraph(mPlot, makeSource(keys, {values}), ramp(20));
    mg->setLineStyle(QCPMultiGraph::lsImpulse);
    mPlot->xAxis->setRange(-10, 199);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(mg->hasRenderedRange());
    const int recordings = mg->mCore.recordingCount();
    const double staleRedPixel = mPlot->xAxis->coordToPixel(0); // pre-pan pixel of key 0

    // Pure pan: same-size range shift, comfortably clear of the y-axis spine
    // on both sides (see the coordToPixel comment in coloredImpulsesRenderTheGradient).
    mPlot->xAxis->setRange(-20, 189);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QCOMPARE(mg->mCore.recordingCount(), recordings); // translated, not recorded again

    const QImage img = mPlot->toPixmap(400, 300).toImage(); // an export records afresh
    const int y = qRound(mPlot->yAxis->coordToPixel(0.4));
    QVERIFY2(isRed(img.pixelColor(qRound(mPlot->xAxis->coordToPixel(0)), y)),
             "impulse for key 0 did not follow the pan");
    QVERIFY2(!isRed(img.pixelColor(qRound(staleRedPixel), y)),
             "impulse for key 0 is stuck at its pre-pan pixel position");
}

void TestColorByScalar::plainImpulsesFollowThePanOffset()
{
    // Uncoloured counterpart of coloredImpulsesFollowThePanOffset: the plain impulse
    // path must also follow the pan, and the pan must not record fresh lines.
    std::vector<double> keys(20), values(20, 0.8);
    for (int i = 0; i < 20; ++i) keys[i] = i * 10.0;
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setDataSource(makeSource(keys, {values}));
    mg->setComponentPens({QPen(Qt::black, 6)});
    mg->setLineStyle(QCPMultiGraph::lsImpulse);
    mPlot->xAxis->setRange(-10, 199);
    mPlot->yAxis->setRange(-1, 1);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(mg->hasRenderedRange());
    const int recordings = mg->mCore.recordingCount();
    const double stalePixel = mPlot->xAxis->coordToPixel(0); // pre-pan pixel of key 0

    mPlot->xAxis->setRange(-20, 189); // pure pan, same range size
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QCOMPARE(mg->mCore.recordingCount(), recordings); // translated, not recorded again

    const QImage img = mPlot->toPixmap(400, 300).toImage(); // an export records afresh
    const int y
        = qRound(mPlot->yAxis->coordToPixel(0.4)); // mid-line, clear of the tip's antialiasing
    QVERIFY2(qGray(img.pixel(qRound(mPlot->xAxis->coordToPixel(0)), y)) < 128,
             "impulse for key 0 did not follow the pan");
    QVERIFY2(qGray(img.pixel(qRound(stalePixel), y)) > 150,
             "impulse for key 0 is stuck at its pre-pan pixel position");
}

void TestColorByScalar::exportAfterOnscreenPanUsesCurrentPixelsNotStaleOffset()
{
    // An export made right after an on-screen pan, before any fresh redraw, must put the
    // line at the pixel of its current value, not at the pre-pan one.
    std::vector<double> keys(200), values(200, 0.5);
    std::iota(keys.begin(), keys.end(), 0.0);
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setDataSource(makeSource(keys, {values}));
    mg->setComponentPens({QPen(Qt::black, 6)});
    mPlot->xAxis->setRange(0, 199);
    mPlot->yAxis->setRange(-1, 1);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(mg->hasRenderedRange());
    const int recordings = mg->mCore.recordingCount();
    const int staleY = qRound(mPlot->yAxis->coordToPixel(0.5)); // pre-pan pixel of its value

    // Pure pan: same-size range shift.
    mPlot->yAxis->setRange(-1.2, 0.8);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QCOMPARE(mg->mCore.recordingCount(), recordings); // translated, not recorded again

    const QImage img = mPlot->toPixmap(400, 300).toImage();

    const int x = qRound(mPlot->xAxis->coordToPixel(100));
    const int correctY = qRound(mPlot->yAxis->coordToPixel(0.5));
    QVERIFY2(qGray(img.pixel(x, correctY)) < 128,
             "line is not drawn at the pixel of its current value after export");
    QVERIFY2(qGray(img.pixel(x, staleY)) > 150,
             "line is drawn at the stale pre-export pan offset instead");
}

void TestColorByScalar::nanScalarLeavesAGap()
{
    std::vector<double> keys(200), values(200, 0.0);
    std::iota(keys.begin(), keys.end(), 0.0);
    auto scalar = ramp(200);
    for (int i = 90; i < 110; ++i) scalar[i] = std::nan("");
    rampGraph(mPlot, makeSource(keys, {values}), scalar);
    // Key 100 and value 0 sit on grid lines, whose dots land on the probe on some platforms.
    mPlot->xAxis->grid()->setVisible(false);
    mPlot->yAxis->grid()->setVisible(false);
    const QImage img = mPlot->toPixmap(400, 300).toImage();
    const QColor background = pixelAt(mPlot, img, 100, 0.8);   // nothing is drawn up there
    QCOMPARE(pixelAt(mPlot, img, 100, 0), background);
    QVERIFY(isRed(pixelAt(mPlot, img, 10, 0)));
}

void TestColorByScalar::scatterEntriesKeepTheirOwnSizeAndMode()
{
    QCPScatterRhiLayer layer(nullptr);
    const std::vector<float> pts {10, 10, 0, 20, 20, 0};
    QImage cmap(256, 1, QImage::Format_ARGB32_Premultiplied);
    cmap.fill(Qt::red);
    layer.addScatter(pts, QCPScatterStyle(QCPScatterStyle::ssDisc, 10), QRect(0, 0, 400, 300), 1.0, 300);
    layer.addScatter(pts, QCPScatterStyle(QCPScatterStyle::ssDisc, 20), QRect(0, 0, 400, 300), 1.0, 300,
                     0, 0, cmap);
    layer.addScatter(pts, QCPScatterStyle(QCPScatterStyle::ssDisc, 6), QRect(0, 0, 400, 300), 1.0, 300);
    QCOMPARE(layer.mDrawEntries.size(), 3);
    QCOMPARE(layer.mDrawEntries[0].halfSize, 5.0f);
    QCOMPARE(layer.mDrawEntries[1].halfSize, 10.0f);
    QCOMPARE(layer.mDrawEntries[2].halfSize, 3.0f);
    QVERIFY(!layer.mDrawEntries[0].useColorAxis);
    QVERIFY(layer.mDrawEntries[1].useColorAxis);   // a later plain draw no longer switches it off
    QVERIFY(!layer.mDrawEntries[2].useColorAxis);
    QCOMPARE(layer.mDrawEntries[0].colorOffset, -1);
    QVERIFY(layer.mColorStaging.empty());           // no coloured draw: no colour data at all
}

void TestColorByScalar::coloredScatterStagesColoursPerMarker()
{
    QCPScatterRhiLayer layer(nullptr);
    const std::vector<float> plain {1, 1, 0};
    const std::vector<float> xy {10, 10, 20, 20};
    const std::vector<float> rgba {1, 0, 0, 1,  0, 0, 1, 1};
    layer.addScatter(plain, QCPScatterStyle(QCPScatterStyle::ssDisc, 8), QRect(0, 0, 400, 300), 1.0, 300);
    layer.addScatterColored(xy, rgba, QCPScatterStyle(QCPScatterStyle::ssDisc, 8),
                            QRect(0, 0, 400, 300), 1.0, 300);
    QCOMPARE(layer.mDrawEntries.size(), 2);
    const auto& e = layer.mDrawEntries[1];
    QCOMPARE(e.instanceOffset, 1);
    QCOMPARE(e.instanceCount, 2);
    QCOMPARE(e.colorOffset, 0);
    QCOMPARE(layer.mStagingSize, 9);                          // 3 floats per instance, both draws
    QCOMPARE(layer.mColorStaging, (std::vector<float>{1, 0, 0, 1, 0, 0, 1, 1}));
    QCOMPARE(layer.mStagingData[3], 10.0f);                   // x of the first coloured marker
    QCOMPARE(layer.mStagingData[5], 0.0f);                    // its padding colorValue
}

void TestColorByScalar::coloredMarkersTakeTheirPointsColour()
{
    std::vector<double> keys(20), values(20, 0.0);
    for (int i = 0; i < 20; ++i) keys[i] = i * 10.0;
    auto* mg = rampGraph(mPlot, makeSource(keys, {values}), ramp(20));
    mg->setLineStyle(QCPMultiGraph::lsNone);
    mg->component(0).scatterStyle = QCPScatterStyle(QCPScatterStyle::ssDisc, 12);
    // Key 0 is also the x-axis range's lower bound, so it sits under the y-axis
    // spine, which paints over the graph (see coloredImpulsesRenderTheGradient).
    // Widen the range so the first marker renders clear of it.
    mPlot->xAxis->setRange(-10, 199);
    const QImage img = mPlot->toPixmap(400, 300).toImage();
    QVERIFY(isRed(pixelAt(mPlot, img, 0, 0)));
    QVERIFY(isBlue(pixelAt(mPlot, img, 190, 0)));
}

void TestColorByScalar::coloredLegendLineShowsTheGradient()
{
    std::vector<double> keys(20), values(20, 0.0);
    std::iota(keys.begin(), keys.end(), 0.0);
    auto* mg = rampGraph(mPlot, makeSource(keys, {values}), ramp(20));
    mg->setComponentPens({QPen(Qt::black, 8)});
    QImage img(100, 20, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    {
        QCPPainter painter(&img);
        mg->drawComponentLegendIcon(&painter, 0, QLineF(0, 10, 100, 10));
    }
    QVERIFY(isRed(img.pixelColor(3, 10)));
    QVERIFY(isBlue(img.pixelColor(96, 10)));

    mg->clearColorValues();
    img.fill(Qt::white);
    {
        QCPPainter painter(&img);
        mg->drawComponentLegendIcon(&painter, 0, QLineF(0, 10, 100, 10));
    }
    QCOMPARE(img.pixelColor(50, 10), QColor(Qt::black));   // uncoloured: the component pen
}

void TestColorByScalar::aColouredLineShowsItsRectScale()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setDataSource(makeSource({ 0, 1, 2, 3 }, { { 0, 1, 0, 1 } }));
    auto* curve = new QCPCurve(mPlot->xAxis, mPlot->yAxis);
    curve->setData(QVector<double> { 0, 1, 2, 3 }, QVector<double> { 0, 1, 0, 1 });
    mPlot->replot();

    QVERIFY2(mPlot->findChildren<QCPColorScale*>().isEmpty(),
             "a plain line made the rect show a colour bar");
    QCOMPARE(mg->colorScale(), static_cast<QCPColorScale*>(nullptr));
    QCOMPARE(curve->colorScale(), static_cast<QCPColorScale*>(nullptr));

    mg->setColorValues(std::vector<double> { 0, 1, 0, 1 });
    mPlot->replot();
    QCPColorScale* scale = mg->colorScale();
    QVERIFY2(scale, "the coloured line did not join the rect's scale");
    QCOMPARE(scale, mPlot->axisRect()->colorScale().get());
    QVERIFY2(scale->visible(), "the coloured line's scale is hidden");
    // A fresh rect's scale is gpCold over 0..6; a line that made it keeps the mapper's defaults.
    QCOMPARE(mg->gradient(), QCPColorGradient(QCPColorGradient::gpJet));
    QCOMPARE(mg->dataRange(), QCPRange(0, 1));
    QCOMPARE(scale->dataRange(), QCPRange(0, 1));

    mg->clearColorValues();
    mPlot->replot();
    QCOMPARE(mg->colorScale(), static_cast<QCPColorScale*>(nullptr));
    QVERIFY2(!scale->visible(), "the bar stayed after the line lost its colours");
}

void TestColorByScalar::aLineFollowsItsScale()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    std::vector<double> keys(200), values(200, 0.0), scalar(200);
    for (int i = 0; i < 200; ++i) { keys[i] = i; scalar[i] = i / 199.0; }
    mg->setDataSource(makeSource(keys, { values }));
    mg->setComponentPens({ QPen(Qt::black, 6) });
    mg->setGradient(redToBlue());
    mg->setDataRange(QCPRange(0, 1));
    mg->setColorValues(scalar);
    mPlot->xAxis->setRange(0, 199);
    mPlot->yAxis->setRange(-1, 1);

    QCPColorScale* scale = mg->colorScale();
    QVERIFY(scale);
    const QImage before = mPlot->toPixmap(400, 300).toImage();
    QVERIFY2(isRed(pixelAt(mPlot, before, 10, 0)), "the line did not take its own gradient");
    QVERIFY2(isBlue(pixelAt(mPlot, before, 189, 0)), "the line did not take its own gradient");

    scale->setGradient(QCPColorGradient(QCPColorGradient::gpGrayscale));
    mPlot->replot();

    QCOMPARE(mg->gradient(), QCPColorGradient(QCPColorGradient::gpGrayscale));
    const QImage after = mPlot->toPixmap(400, 300).toImage();
    const QColor left = pixelAt(mPlot, after, 10, 0);
    QVERIFY2(qAbs(left.red() - left.green()) <= 2 && qAbs(left.green() - left.blue()) <= 2,
             qPrintable(QString("the line kept its own gradient: pixel is %1").arg(left.name())));
}

void TestColorByScalar::rescaleSeesALine()
{
    auto* map = new QCPColorMap(mPlot->xAxis, mPlot->yAxis);
    map->data()->setSize(2, 2);
    map->data()->setCell(0, 0, 1.0);
    map->data()->setCell(1, 0, 1.0);
    map->data()->setCell(0, 1, 3.0);
    map->data()->setCell(1, 1, 3.0);

    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setDataSource(makeSource({ 0, 1, 2 }, { { 0, 0, 0 } }));
    mg->setColorValues(std::vector<double> { 1, 7, 3 });

    QCPColorScale* scale = mPlot->axisRect()->colorScale().get();
    QVERIFY(scale);
    QCOMPARE(mg->colorScale(), scale);

    bool found = false;
    QVERIFY2(mg->colorDataBounds(found) == QCPRange(1, 7),
             "the line's colour bounds are not the range of its values");
    QVERIFY(found);

    scale->rescaleDataRange(true);
    QCOMPARE(scale->dataRange().upper, 7.0);
    QCOMPARE(mg->dataRange().upper, 7.0);
}

void TestColorByScalar::aCurveFollowsItsScale()
{
    auto* curve = new QCPCurve(mPlot->xAxis, mPlot->yAxis);
    QVector<double> keys(200), values(200, 0.0), scalar(200);
    for (int i = 0; i < 200; ++i) { keys[i] = i; scalar[i] = i / 199.0; }
    curve->setData(keys, values);
    curve->setPen(QPen(Qt::black, 6));
    curve->setGradient(redToBlue());
    curve->setDataRange(QCPRange(0, 1));
    curve->setColorValues(std::vector<double>(scalar.begin(), scalar.end()));
    mPlot->xAxis->setRange(0, 199);
    mPlot->yAxis->setRange(-1, 1);

    QCPColorScale* scale = curve->colorScale();
    QVERIFY(scale);
    const QImage before = mPlot->toPixmap(400, 300).toImage();
    QVERIFY2(isRed(pixelAt(mPlot, before, 10, 0)), "the curve did not take its own gradient");
    QVERIFY2(isBlue(pixelAt(mPlot, before, 189, 0)), "the curve did not take its own gradient");

    scale->setGradient(QCPColorGradient(QCPColorGradient::gpGrayscale));
    mPlot->replot();

    QCOMPARE(curve->gradient(), QCPColorGradient(QCPColorGradient::gpGrayscale));
    const QImage after = mPlot->toPixmap(400, 300).toImage();
    const QColor left = pixelAt(mPlot, after, 10, 0);
    QVERIFY2(qAbs(left.red() - left.green()) <= 2 && qAbs(left.green() - left.blue()) <= 2,
             qPrintable(QString("the curve kept its own gradient: pixel is %1").arg(left.name())));
}

void TestColorByScalar::coloredGraphDrawnFromL2KeepsIndicesConsistent()
{
    // The colour of a vertex is looked up by the data row the L2 point came from. A misaligned
    // origin moves the colour of every segment after it: draw a flat column whose colour changes
    // sharply at the middle row, and the change must stay on that row in every view.
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    const int n = 200'000; // above kResampleThreshold: the async L1 pipeline runs
    std::vector<double> keys(n), values(n), negated(n), flat(n, 0.0), scalar(n);
    for (int i = 0; i < n; ++i)
    {
        keys[i] = i;
        values[i] = std::sin(i * 0.001);
        negated[i] = -values[i];
        scalar[i] = i < n / 2 ? 0.0 : 1.0;
    }
    mg->setDataSource(makeSource(keys, { values, negated, flat }));
    mg->setComponentPens({ QPen(Qt::black, 6), QPen(Qt::black, 6), QPen(Qt::black, 6) });
    mg->setGradient(redToBlue());
    mg->setDataRange(QCPRange(0, 1));
    mg->setColorValues(scalar);
    mPlot->xAxis->setRange(0, n);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot();
    QTRY_VERIFY_WITH_TIMEOUT(mg->mL1Cache && !mg->mL1Cache->level1.origin.empty(), 5000);

    // The flat column is drawn last, so this pixel is its own colour.
    auto colourAtKey = [&](const QImage& img, double key)
    {
        return img.pixelColor(qRound(mPlot->xAxis->coordToPixel(key)),
                              qRound(mPlot->yAxis->coordToPixel(0.0)));
    };

    auto checkColours = [&](double firstHalfKey, double secondHalfKey)
    {
        QVERIFY(mg->mL2Result);
        QVERIFY(mg->mL2HasOrigin);
        mPlot->replot();
        QVERIFY(mg->mL2Result);
        QVERIFY(mg->mL2HasOrigin);
        const QImage img = mPlot->toPixmap(400, 300).toImage();
        QVERIFY2(isRed(colourAtKey(img, firstHalfKey)),
                 qPrintable(QString("key %1 drawn as %2")
                                .arg(firstHalfKey)
                                .arg(colourAtKey(img, firstHalfKey).name())));
        QVERIFY2(isBlue(colourAtKey(img, secondHalfKey)),
                 qPrintable(QString("key %1 drawn as %2")
                                .arg(secondHalfKey)
                                .arg(colourAtKey(img, secondHalfKey).name())));
    };

    checkColours(n * 0.25, n * 0.75);

    mPlot->xAxis->setRange(n * 0.10, n * 0.60);
    checkColours(n * 0.25, n * 0.55);

    mPlot->xAxis->setRange(n * 0.11, n * 0.61); // a small pan
    checkColours(n * 0.25, n * 0.55);
}

// A graph once looked a point's colour up at its position in the pixel lines. A key gap or a NaN
// value inserts a break point there, so every marker after it took its neighbour's colour.
void TestColorByScalar::multiGraphScatterColoursFollowTheirPointAcrossBreaks()
{
    auto* graph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    graph->setLineStyle(QCPMultiGraph::lsNone);
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssSquare, Qt::black, Qt::black, 12));
    graph->setGradient(redToBlue());
    // A key gap after key 3, and a NaN value at key 12.
    graph->setData(std::vector<double>{1, 2, 3, 10, 11, 12, 13},
                   std::vector<double>{1, 1, 1, 1, 1, qQNaN(), 1});
    graph->setColorValues(std::vector<double> {0, 0, 0, 0, 1, 1, 0});
    mPlot->xAxis->setRange(0, 14);
    mPlot->yAxis->setRange(0, 2);

    const QImage image = mPlot->toPixmap(400, 300).toImage();
    const auto colourAt = [&](double key) {
        return image.pixelColor(qRound(mPlot->xAxis->coordToPixel(key)),
                                qRound(mPlot->yAxis->coordToPixel(1)));
    };
    const auto isRed = [](const QColor& c) { return c.red() > 200 && c.blue() < 60; };
    const auto isBlue = [](const QColor& c) { return c.blue() > 200 && c.red() < 60; };
    QVERIFY2(isRed(colourAt(10)), qPrintable("key 10 drawn as " + colourAt(10).name()));
    QVERIFY2(isBlue(colourAt(11)), qPrintable("key 11 drawn as " + colourAt(11).name()));
    QVERIFY2(isRed(colourAt(13)), qPrintable("key 13 drawn as " + colourAt(13).name()));
}

// A scale the user picked on an uncoloured line is not the size rule's to drop; only a resize
// that actually clears colour values leaves it.
void TestColorByScalar::newDataKeepsAnUncolouredLinesChosenScale()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setDataSource(makeSource({ 0, 1, 2, 3 }, { { 0, 1, 0, 1 } }));
    auto* scale = new QCPColorScale(mPlot);
    mg->setColorScale(scale);
    QCOMPARE(mg->colorScale(), scale);

    mg->setDataSource(makeSource({ 0, 1, 2, 3, 4 }, { { 0, 1, 0, 1, 0 } }));
    QCOMPARE(mg->colorScale(), scale);

    // Coloured, then resized: the values are gone, so the scale goes with them.
    mg->setColorValues(std::vector<double> { 0.0, 1.0, 0.0, 1.0, 0.0 });
    mg->setDataSource(makeSource({ 0, 1, 2 }, { { 0, 1, 0 } }));
    QCOMPARE(mg->colorScale(), nullptr);
}

// An uncoloured line stops asking the pipeline for the colour origin at its next data.
void TestColorByScalar::newDataAfterUncolouringStopsTheOriginRequest()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setDataSource(makeSource({ 0, 1, 2, 3 }, { { 0, 1, 0, 1 } }));
    mg->setColorValues(std::vector<double> { 0.0, 1.0, 0.0, 1.0 });
    QVERIFY(mg->mWantOrigin->load());
    mg->clearColorValues();

    mg->setDataSource(makeSource({ 0, 1, 2 }, { { 0, 1, 0 } }));
    QVERIFY(!mg->mWantOrigin->load());
}

// Taking a coloured line drops its old plot's scale; adding it elsewhere joins the new rect's.
void TestColorByScalar::aTakenColouredLineJoinsItsNewPlotsScale()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setDataSource(makeSource({ 0, 1, 2, 3 }, { { 0, 1, 0, 1 } }));
    mg->setColorValues(std::vector<double> { 0.0, 1.0, 0.0, 1.0 });
    const auto oldScale = mPlot->axisRect()->colorScale();
    QVERIFY(oldScale);
    QVERIFY(qcp::pointers(oldScale->users()).contains(mg));
    oldScale->setGradient(QCPColorGradient(QCPColorGradient::gpHot));
    QCOMPARE(mg->gradient(), QCPColorGradient(QCPColorGradient::gpHot));

    QCustomPlot other;
    other.resize(400, 300);
    auto taken = mPlot->take(qcp::makeHandle<QCPHandle<QCPMultiGraph>>(mg));
    QVERIFY(taken);
    QVERIFY2(oldScale->users().isEmpty(), "the taken line still uses its old plot's scale");
    QCOMPARE(taken->colorScale(), nullptr);

    auto added = other.add(std::move(taken));
    QVERIFY(added);
    QVERIFY2(other.axisRect()->colorScale(), "the re-added line did not make the new rect's scale");
    QCOMPARE(added->colorScale(), other.axisRect()->colorScale().get());
    QVERIFY(qcp::pointers(other.axisRect()->colorScale()->users()).contains(added.get()));

    // The old scale is no longer wired to it; the new one is.
    oldScale->setGradient(QCPColorGradient(QCPColorGradient::gpCold));
    QCOMPARE(added->gradient(), QCPColorGradient(QCPColorGradient::gpHot));
    other.axisRect()->colorScale()->setGradient(QCPColorGradient(QCPColorGradient::gpJet));
    QCOMPARE(added->gradient(), QCPColorGradient(QCPColorGradient::gpJet));
}

// The line finishes updating itself before colorScaleChanged is delivered: a slot may delete the
// line, so it must see the colouring complete, not mid-way.
void TestColorByScalar::colourValuesAreCompleteBeforeTheColourScaleSignal()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setDataSource(makeSource({ 0, 1, 2, 3 }, { { 0, 1, 0, 1 } }));
    mPlot->xAxis->setRange(0, 3);
    mPlot->yAxis->setRange(-1, 2);

    const int before = mg->mRevision;
    bool completeAtDelivery = false;
    connect(mg, &QCPAbstractColorMapped::colorScaleChanged, mPlot,
            [&completeAtDelivery, mg, before] { completeAtDelivery = mg->mRevision > before; });
    mg->setColorValues(std::vector<double> { 0.0, 1.0, 0.0, 1.0 });
    QVERIFY2(completeAtDelivery, "colorScaleChanged was delivered before the line was updated");
}

void TestColorByScalar::uncolouringIsCompleteBeforeTheColourScaleSignal()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setDataSource(makeSource({ 0, 1, 2, 3 }, { { 0, 1, 0, 1 } }));
    mPlot->xAxis->setRange(0, 3);
    mPlot->yAxis->setRange(-1, 2);
    mg->setColorValues(std::vector<double> { 0.0, 1.0, 0.0, 1.0 });
    QVERIFY(mg->colorScale());

    const int before = mg->mRevision;
    bool completeAtDelivery = false;
    connect(mg, &QCPAbstractColorMapped::colorScaleChanged, mPlot,
            [&completeAtDelivery, mg, before] { completeAtDelivery = mg->mRevision > before; });
    mg->clearColorValues();
    QVERIFY2(completeAtDelivery, "colorScaleChanged was delivered before the line was updated");
}

// With the update finished the slot may delete the line: the setter must not use it afterwards.
void TestColorByScalar::colourSlotDeletingTheLineIsSafe()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setDataSource(makeSource({ 0, 1, 2, 3 }, { { 0, 1, 0, 1 } }));
    mPlot->xAxis->setRange(0, 3);
    mPlot->yAxis->setRange(-1, 2);
    const QPointer<QCPMultiGraph> alive(mg);
    connect(mg, &QCPAbstractColorMapped::colorScaleChanged, mPlot, [mg] { delete mg; },
            Qt::SingleShotConnection);
    mg->setColorValues(std::vector<double> { 0.0, 1.0, 0.0, 1.0 });
    QVERIFY(!alive);
}

// The first colour values commit with a pending source, and turn the origin on: the pipeline
// restarts and goes busy, and a slot on that may delete the graph mid-commit.
void TestColorByScalar::firstColouredCommitSlotDeletingTheGraphIsSafe()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setDeferredDataSwap(true);
    const int n = 200'000; // above kResampleThreshold: the async L1 pipeline runs
    std::vector<double> keys(n), values(n), scalar(n);
    for (int i = 0; i < n; ++i) { keys[i] = i; values[i] = std::sin(i * 0.001); scalar[i] = i; }
    mg->setDataSource(makeSource(keys, {values}));
    mPlot->xAxis->setRange(0, n);
    mPlot->replot();
    QTRY_VERIFY_WITH_TIMEOUT(mg->mL1Cache != nullptr && mg->hasRenderedRange(), 5000);

    mPlot->setDataSwapDebounceMs(60'000); // the test commits by hand
    mg->setDataSource(makeSource(keys, {values}));
    QVERIFY(mg->hasPendingData());
    mg->setColorValues(scalar); // stashed for the pending source
    QTRY_VERIFY_WITH_TIMEOUT(mg->mPendingReady, 5000);
    QVERIFY(!mg->pipeline().isBusy());

    connect(&mg->pipeline(), &QCPMultiGraphPipeline::busyChanged, mPlot,
            [this, mg] { QVERIFY(mPlot->removePlottable(mg)); }, Qt::SingleShotConnection);
    QVERIFY(mg->commitPendingData());
    QCOMPARE(mPlot->plottableCount(), 0);
}
