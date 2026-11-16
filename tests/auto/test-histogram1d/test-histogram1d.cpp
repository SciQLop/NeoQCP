#include "test-histogram1d.h"
#include "datasource/histogram-binner-1d.h"
#include "datasource/soa-multi-datasource.h"
#include <limits>

using namespace qcp::algo;
namespace {
using Src = QCPSoAMultiDataSource<std::vector<double>, std::vector<double>>;
Src source(std::vector<std::vector<double>> columns)
{
    std::vector<double> keys(columns.front().size());
    for (std::size_t i = 0; i < keys.size(); ++i) keys[i] = double(i);
    return Src(std::move(keys), std::move(columns));
}
constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
}

void TestHistogram1D::columnsShareEdges()
{
    const auto src = source({{0, 1, 2, 3}, {0, 0, 0, 3}});
    const auto h = bin1d(src, QCPRange(0, 4), 4, QCPScale());
    QCOMPARE(h.edges.size(), std::size_t(5));
    QCOMPARE(h.heights[0], (std::vector<double>{1, 1, 1, 1}));
    QCOMPARE(h.heights[1], (std::vector<double>{3, 0, 0, 1}));
}

void TestHistogram1D::nanAndInfinityAreSkipped()
{
    const auto src = source({{1, kNan, std::numeric_limits<double>::infinity(), 1}});
    const auto h = bin1d(src, QCPRange(0, 2), 2, QCPScale());
    QCOMPARE(h.counted[0], 2);
}

void TestHistogram1D::valuesOutsideAFixedRangeAreNotCounted()
{
    const auto src = source({{-10, 0.5, 1.5, 10}});
    const auto h = bin1d(src, QCPRange(0, 2), 2, QCPScale());
    QCOMPARE(h.heights[0], (std::vector<double>{1, 1}));
    QCOMPARE(h.counted[0], 2);
}

void TestHistogram1D::logBinsSkipNonPositiveValues()
{
    const auto src = source({{-1, 0, 1, 10, 100}});
    const auto h = bin1d(src, QCPRange(1, 100), 2, QCPScale::log());
    QCOMPARE(h.edges[1], 10.0);
    QCOMPARE(h.counted[0], 3);
}

void TestHistogram1D::flatDataGetsAVisibleBar()
{
    const auto src = source({{5, 5, 5}});
    auto bounds = finiteValueBounds(src, false);
    QVERIFY(bounds.has_value());
    expandIfFlat(*bounds, QCPScale());
    const auto h = bin1d(src, *bounds, 4, QCPScale());
    double total = 0;
    for (double v : h.heights[0]) total += v;
    QCOMPARE(total, 3.0);
}

void TestHistogram1D::normalizationSums()
{
    const auto src = source({{0.5, 1.5, 1.5, 3.5}});
    auto probability = bin1d(src, QCPRange(0, 4), 4, QCPScale());
    normalize(probability, QCP::HistogramNormalization::Probability);
    double p = 0;
    for (double v : probability.heights[0]) p += v;
    QCOMPARE(p, 1.0);

    auto density = bin1d(src, QCPRange(0, 8), 4, QCPScale()); // width 2
    normalize(density, QCP::HistogramNormalization::Density);
    double integral = 0;
    for (std::size_t b = 0; b < 4; ++b)
        integral += density.heights[0][b] * (density.edges[b + 1] - density.edges[b]);
    QCOMPARE(integral, 1.0);
}

void TestHistogram1D::densityUsesEachLogBinsOwnWidth()
{
    const auto src = source({{2, 20}});
    auto h = bin1d(src, QCPRange(1, 100), 2, QCPScale::log()); // widths 9 and 90
    normalize(h, QCP::HistogramNormalization::Density);
    QCOMPARE(h.heights[0][0], 1.0 / (2 * 9));
    QCOMPARE(h.heights[0][1], 1.0 / (2 * 90));
}

void TestHistogram1D::emptyOrAllNanHasNoBounds()
{
    QVERIFY(!finiteValueBounds(source({{kNan, kNan}}), false).has_value());
}

void TestHistogram1D::edgesOnlyWiden()
{
    QCOMPARE(widenedRange(QCPRange(0, 10), QCPRange(2, 8)), QCPRange(0, 10));
    QCOMPARE(widenedRange(QCPRange(0, 10), QCPRange(-5, 8)), QCPRange(-5, 10));
    QCOMPARE(widenedRange(std::nullopt, QCPRange(2, 8)), QCPRange(2, 8));
}

void TestHistogram1D::noBinsGiveNothing()
{
    const auto src = source({{1, 2, 3}});
    for (int bins : {0, -3})
    {
        QVERIFY(binEdges(QCPRange(0, 4), bins, QCPScale()).empty());
        const auto h = bin1d(src, QCPRange(0, 4), bins, QCPScale());
        QVERIFY(h.edges.empty());
        QVERIFY(h.heights.empty());
    }
}

void TestHistogram1D::valueAtTheUpperEdgeLandsInTheLastBin()
{
    const auto h = bin1d(source({{4}}), QCPRange(0, 4), 4, QCPScale());
    QCOMPARE(h.heights[0], (std::vector<double>{0, 0, 0, 1}));
}

void TestHistogram1D::outerEdgesAreTheRangeExactly()
{
    const auto lin = binEdges(QCPRange(0.1, 0.7), 3, QCPScale());
    QCOMPARE(lin.front(), 0.1);
    QCOMPARE(lin.back(), 0.7);
    const auto log = binEdges(QCPRange(3, 70), 7, QCPScale::log());
    QVERIFY(log.front() == 3.0); // exact, not fuzzy: auto edges are compared exactly
    QVERIFY(log.back() == 70.0);
    QVERIFY(lin.back() == 0.7);
}

void TestHistogram1D::positiveBoundsSkipZero()
{
    const auto bounds = finiteValueBounds(source({{0, 2, 8}}), true);
    QVERIFY(bounds.has_value());
    QCOMPARE(bounds->lower, 2.0);
    QCOMPARE(bounds->upper, 8.0);
}

void TestHistogram1D::interiorEdgesAreEvenlySpacedFromTheRangeStart()
{
    QCOMPARE(binEdges(QCPRange(10, 20), 4, QCPScale()), (std::vector<double>{10, 12.5, 15, 17.5, 20}));
}

void TestHistogram1D::aSingleValueIsNormalizedToo()
{
    auto h = bin1d(source({{3}}), QCPRange(0, 8), 4, QCPScale()); // width 2
    normalize(h, QCP::HistogramNormalization::Density);
    QCOMPARE(h.heights[0], (std::vector<double>{0, 0.5, 0, 0}));
}

void TestHistogram1D::aColumnWithNothingCountedStaysZero()
{
    auto h = bin1d(source({{kNan, 100}}), QCPRange(0, 8), 4, QCPScale());
    normalize(h, QCP::HistogramNormalization::Probability);
    QCOMPARE(h.heights[0], (std::vector<double>{0, 0, 0, 0}));
}

void TestHistogram1D::histogramBinsInScaleSpace()
{
    // Equal steps of the scale, not of the data: on a symlog axis the linear band and the two
    // log sides each get bins sized in scale space (a linear grid would put the first inner
    // edge at -50 instead).
    const QCPScale symlog = QCPScale::symlog(2);
    const auto scaleEdges = binEdges(QCPRange(-100, 100), 4, symlog);
    QCOMPARE(scaleEdges.size(), std::size_t(5));
    QCOMPARE(scaleEdges.front(), -100.0);
    QCOMPARE(scaleEdges.back(), 100.0);
    const double step = symlog.toScale(scaleEdges[1]) - symlog.toScale(scaleEdges[0]);
    QVERIFY(step > 0.0);
    for (std::size_t i = 1; i + 1 < scaleEdges.size(); ++i)
        QVERIFY(std::abs((symlog.toScale(scaleEdges[i + 1]) - symlog.toScale(scaleEdges[i])) - step)
                < 1e-9);
    QVERIFY2(scaleEdges[1] > -10.0,
             qPrintable(QString("first inner edge %1: a linear grid would put it at -50")
                            .arg(scaleEdges[1])));

    // Six keys spread evenly in scale space land two/one/one/two, one pair per bin; linear
    // binning would pile them at the two ends ({0, 3, 3, 0}).
    const auto at = [&](double scaleValue) { return symlog.fromScale(scaleValue); };
    std::vector<double> keys { 0, 1, 2, 3, 4, 5 };
    Src src(keys, {{ at(-5), at(-3), at(-1), at(1), at(3), at(5) }});
    const auto h = bin1d(src, QCPRange(-100, 100), 4, symlog);
    QCOMPARE(h.heights[0], (std::vector<double>{ 2, 1, 1, 2 }));

    // Log keeps its current results: one decade per bin, exact outer edges.
    QCOMPARE(binEdges(QCPRange(1, 1000), 3, QCPScale::log()),
             (std::vector<double>{ 1, 10, 100, 1000 }));
    const auto logHist = bin1d(source({{ 1, 10, 100, 1000 }}), QCPRange(1, 1000), 3,
                               QCPScale::log());
    QCOMPARE(logHist.heights[0], (std::vector<double>{ 1, 1, 2 }));
}
