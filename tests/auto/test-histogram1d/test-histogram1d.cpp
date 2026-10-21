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
    const auto h = bin1d(src, QCPRange(0, 4), 4, false);
    QCOMPARE(h.edges.size(), std::size_t(5));
    QCOMPARE(h.heights[0], (std::vector<double>{1, 1, 1, 1}));
    QCOMPARE(h.heights[1], (std::vector<double>{3, 0, 0, 1}));
}

void TestHistogram1D::nanAndInfinityAreSkipped()
{
    const auto src = source({{1, kNan, std::numeric_limits<double>::infinity(), 1}});
    const auto h = bin1d(src, QCPRange(0, 2), 2, false);
    QCOMPARE(h.counted[0], 2);
}

void TestHistogram1D::valuesOutsideAFixedRangeAreNotCounted()
{
    const auto src = source({{-10, 0.5, 1.5, 10}});
    const auto h = bin1d(src, QCPRange(0, 2), 2, false);
    QCOMPARE(h.heights[0], (std::vector<double>{1, 1}));
    QCOMPARE(h.counted[0], 2);
}

void TestHistogram1D::logBinsSkipNonPositiveValues()
{
    const auto src = source({{-1, 0, 1, 10, 100}});
    const auto h = bin1d(src, QCPRange(1, 100), 2, true);
    QCOMPARE(h.edges[1], 10.0);
    QCOMPARE(h.counted[0], 3);
}

void TestHistogram1D::flatDataGetsAVisibleBar()
{
    const auto src = source({{5, 5, 5}});
    auto bounds = finiteValueBounds(src, false);
    QVERIFY(bounds.has_value());
    expandIfFlat(*bounds, false);
    const auto h = bin1d(src, *bounds, 4, false);
    double total = 0;
    for (double v : h.heights[0]) total += v;
    QCOMPARE(total, 3.0);
}

void TestHistogram1D::normalizationSums()
{
    const auto src = source({{0.5, 1.5, 1.5, 3.5}});
    auto probability = bin1d(src, QCPRange(0, 4), 4, false);
    normalize(probability, HistogramNormalization::Probability);
    double p = 0;
    for (double v : probability.heights[0]) p += v;
    QCOMPARE(p, 1.0);

    auto density = bin1d(src, QCPRange(0, 8), 4, false); // width 2
    normalize(density, HistogramNormalization::Density);
    double integral = 0;
    for (std::size_t b = 0; b < 4; ++b)
        integral += density.heights[0][b] * (density.edges[b + 1] - density.edges[b]);
    QCOMPARE(integral, 1.0);
}

void TestHistogram1D::densityUsesEachLogBinsOwnWidth()
{
    const auto src = source({{2, 20}});
    auto h = bin1d(src, QCPRange(1, 100), 2, true); // widths 9 and 90
    normalize(h, HistogramNormalization::Density);
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
