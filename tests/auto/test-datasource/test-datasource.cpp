#include "test-datasource.h"
#include "qcustomplot.h"
#include "datasource/algorithms.h"
#include "datasource/resampled-multi-datasource.h"
#include "datasource/row-major-multi-datasource.h"
#include "datasource/soa-datasource.h"
#include "datasource/soa-multi-datasource.h"
#include "plottables/plottable-waterfall.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <vector>

namespace
{

// 1M points, one NaN run per 1000 rows, kept deterministic so the thinning is reproducible.
QCPSoAMultiDataSource<std::vector<double>, std::vector<double>> millionPointSource()
{
    const int n = 1000000;
    std::vector<double> keys(n);
    std::vector<double> values(n);
    quint32 state = 12345;
    double v = 0.0;
    for (int i = 0; i < n; ++i)
    {
        state = state * 1664525u + 1013904223u;
        keys[i] = i * 0.37;
        v += (state >> 8) / 16777216.0 - 0.5;
        values[i] = (i % 1000 == 500) ? std::numeric_limits<double>::quiet_NaN() : v;
    }
    return { std::move(keys), std::vector<std::vector<double>> { std::move(values) } };
}

// Fits the axes to the source and returns its key pixels per key unit.
double fitKeyPixelsPerUnit(QCustomPlot* plot, const QCPAbstractMultiDataSource& src)
{
    plot->xAxis->setRange(src.keyAt(0), src.keyAt(src.size() - 1));
    bool found = false;
    const QCPRange values = src.valueRange(0, found);
    plot->yAxis->setRange(values.lower, values.upper);
    plot->replot(QCustomPlot::rpImmediateRefresh);
    const double pixels = qAbs(plot->xAxis->coordToPixel(src.keyAt(0))
                               - plot->xAxis->coordToPixel(src.keyAt(src.size() - 1)));
    return pixels / (src.keyAt(src.size() - 1) - src.keyAt(0));
}

std::vector<std::vector<double>> oneColumn(std::vector<double> values)
{
    return std::vector<std::vector<double>> { std::move(values) };
}

} // namespace

void TestDataSource::init() {}

void TestDataSource::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
}

void TestDataSource::algoFindBegin()
{
    std::vector<double> keys = {1.0, 2.0, 3.0, 4.0, 5.0};

    // Exact match, no expansion
    QCOMPARE(qcp::algo::findBegin(keys, 3.0, false), 2);
    // Before all data
    QCOMPARE(qcp::algo::findBegin(keys, 0.0, false), 0);
    // After all data
    QCOMPARE(qcp::algo::findBegin(keys, 6.0, false), 5);
    // With expansion: one point before
    QCOMPARE(qcp::algo::findBegin(keys, 3.0, true), 1);
    // Expansion at boundary clamps to 0
    QCOMPARE(qcp::algo::findBegin(keys, 1.0, true), 0);

    // Float keys
    std::vector<float> fkeys = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    QCOMPARE(qcp::algo::findBegin(fkeys, 3.0, false), 2);

    // Int keys
    std::vector<int> ikeys = {10, 20, 30, 40, 50};
    QCOMPARE(qcp::algo::findBegin(ikeys, 25.0, false), 2);
}

void TestDataSource::algoFindEnd()
{
    std::vector<double> keys = {1.0, 2.0, 3.0, 4.0, 5.0};

    // Exact match, no expansion: past-the-end of elements <= sortKey
    QCOMPARE(qcp::algo::findEnd(keys, 3.0, false), 3);
    // Before all data
    QCOMPARE(qcp::algo::findEnd(keys, 0.0, false), 0);
    // After all data
    QCOMPARE(qcp::algo::findEnd(keys, 6.0, false), 5);
    // With expansion: one point after
    QCOMPARE(qcp::algo::findEnd(keys, 3.0, true), 4);
    // Expansion at boundary clamps to size
    QCOMPARE(qcp::algo::findEnd(keys, 5.0, true), 5);
}

void TestDataSource::algoKeyRange()
{
    std::vector<double> keys = {-2.0, 1.0, 3.0, 5.0};
    bool found = false;

    auto range = qcp::algo::keyRange(keys, found);
    QVERIFY(found);
    QCOMPARE(range.lower, -2.0);
    QCOMPARE(range.upper, 5.0);

    // Positive only
    range = qcp::algo::keyRange(keys, found, QCP::sdPositive);
    QVERIFY(found);
    QCOMPARE(range.lower, 1.0);
    QCOMPARE(range.upper, 5.0);

    // Negative only
    range = qcp::algo::keyRange(keys, found, QCP::sdNegative);
    QVERIFY(found);
    QCOMPARE(range.lower, -2.0);
    QCOMPARE(range.upper, -2.0);

    // Empty
    std::vector<double> empty;
    range = qcp::algo::keyRange(empty, found);
    QVERIFY(!found);

    // Float
    std::vector<float> fkeys = {1.0f, 5.0f};
    range = qcp::algo::keyRange(fkeys, found);
    QVERIFY(found);
    QCOMPARE(range.lower, 1.0);
    QCOMPARE(range.upper, 5.0);
}

void TestDataSource::algoValueRange()
{
    std::vector<double> keys = {1.0, 2.0, 3.0, 4.0};
    std::vector<double> values = {-1.0, 5.0, 2.0, 8.0};
    bool found = false;

    auto range = qcp::algo::valueRange(keys, values, found);
    QVERIFY(found);
    QCOMPARE(range.lower, -1.0);
    QCOMPARE(range.upper, 8.0);

    // With key range restriction
    range = qcp::algo::valueRange(keys, values, found, QCP::sdBoth, QCPRange(2.0, 3.0));
    QVERIFY(found);
    QCOMPARE(range.lower, 2.0);
    QCOMPARE(range.upper, 5.0);

    // Positive only
    range = qcp::algo::valueRange(keys, values, found, QCP::sdPositive);
    QVERIFY(found);
    QCOMPARE(range.lower, 2.0);
    QCOMPARE(range.upper, 8.0);
}

void TestDataSource::algoValueRangeRestrictionMatchesBruteForce()
{
    // Equivalence guard for the windowed (binary-search) key restriction:
    // results must match a brute-force linear scan for every window placement,
    // including boundary-exact keys and windows outside the data.
    std::vector<double> keys, values;
    for (int i = 0; i < 1000; ++i)
    {
        keys.push_back(i * 0.5);
        values.push_back(std::sin(i * 0.37) * 50.0);
    }

    auto brute = [&](QCP::SignDomain sd, const QCPRange& kr, bool& found) {
        found = false;
        double lo = std::numeric_limits<double>::max();
        double hi = std::numeric_limits<double>::lowest();
        for (size_t i = 0; i < keys.size(); ++i)
        {
            if (keys[i] < kr.lower || keys[i] > kr.upper)
                continue;
            double v = values[i];
            if (sd == QCP::sdPositive && v <= 0) continue;
            if (sd == QCP::sdNegative && v >= 0) continue;
            if (v < lo) lo = v;
            if (v > hi) hi = v;
            found = true;
        }
        return found ? QCPRange(lo, hi) : QCPRange();
    };

    const QCPRange windows[] = {
        {100.0, 200.0},   // boundary-exact keys on both ends
        {100.25, 200.75}, // boundaries between keys
        {0.0, 499.5},     // full range
        {-50.0, -10.0},   // entirely before the data
        {600.0, 700.0},   // entirely after the data
        {499.5, 499.5},   // degenerate window on the last key
    };
    const QCP::SignDomain domains[] = {QCP::sdBoth, QCP::sdPositive, QCP::sdNegative};

    for (const auto& w : windows)
    {
        for (auto sd : domains)
        {
            bool foundRef = false, foundGot = false;
            QCPRange ref = brute(sd, w, foundRef);
            QCPRange got = qcp::algo::valueRange(keys, values, foundGot, sd, w);
            QCOMPARE(foundGot, foundRef);
            if (foundRef)
            {
                QCOMPARE(got.lower, ref.lower);
                QCOMPARE(got.upper, ref.upper);
            }
        }
    }
}

namespace
{
std::vector<int> gapIndices(const qcp::algo::GapVector& gaps)
{
    std::vector<int> out;
    for (int i = 0; i < gaps.size(); ++i)
        if (gaps[i])
            out.push_back(i);
    return out;
}

int nanBreaks(const std::vector<QPointF>& lines)
{
    return static_cast<int>(std::count_if(lines.begin(), lines.end(), [](const QPointF& p) {
        return std::isnan(p.x()) || std::isnan(p.y());
    }));
}
}

void TestDataSource::algoGapsIgnoreRepeatedKeys()
{
    // Issue SciQLopPlots#118: two samples at the same key (a vertical edge)
    // made the zero step the reference, so every later step was a gap.
    std::vector<double> edges = {0, 0, 1, 1, 5, 5};
    QCOMPARE(gapIndices(qcp::algo::detectKeyGaps(edges, 0, 6)), std::vector<int>{});

    // A real gap next to a repeated key is still found from the other neighbour.
    std::vector<double> gapAfterEdge = {0, 1, 1, 2, 20, 21};
    QCOMPARE(gapIndices(qcp::algo::detectKeyGaps(gapAfterEdge, 0, 6)), std::vector<int>{4});
}

void TestDataSource::soaOwningVector()
{
    std::vector<double> keys = {1.0, 2.0, 3.0};
    std::vector<double> values = {10.0, 20.0, 30.0};
    QCPSoADataSource<std::vector<double>, std::vector<double>> src(
        std::move(keys), std::move(values));

    QCOMPARE(src.size(), 3);
    QVERIFY(!src.empty());
    QCOMPARE(src.keyAt(0), 1.0);
    QCOMPARE(src.keyAt(2), 3.0);
    QCOMPARE(src.valueAt(1), 20.0);
}

void TestDataSource::soaViewSpan()
{
    double keys[] = {1.0, 2.0, 3.0};
    float values[] = {10.0f, 20.0f, 30.0f};
    QCPSoADataSource<std::span<const double>, std::span<const float>> src(
        std::span{keys}, std::span{values});

    QCOMPARE(src.size(), 3);
    QCOMPARE(src.keyAt(1), 2.0);
    QCOMPARE(src.valueAt(2), 30.0);
}

void TestDataSource::soaFindBeginEnd()
{
    std::vector<double> keys = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> values = {10.0, 20.0, 30.0, 40.0, 50.0};
    QCPSoADataSource<std::vector<double>, std::vector<double>> src(
        std::move(keys), std::move(values));

    QCOMPARE(src.findBegin(3.0, false), 2);
    QCOMPARE(src.findEnd(3.0, false), 3);
    QCOMPARE(src.findBegin(3.0, true), 1);
    QCOMPARE(src.findEnd(3.0, true), 4);
}

void TestDataSource::soaRangeQueries()
{
    std::vector<double> keys = {1.0, 2.0, 3.0};
    std::vector<double> values = {-5.0, 10.0, 3.0};
    QCPSoADataSource<std::vector<double>, std::vector<double>> src(
        std::move(keys), std::move(values));

    bool found = false;
    auto kr = src.keyRange(found);
    QVERIFY(found);
    QCOMPARE(kr.lower, 1.0);
    QCOMPARE(kr.upper, 3.0);

    auto vr = src.valueRange(found);
    QVERIFY(found);
    QCOMPARE(vr.lower, -5.0);
    QCOMPARE(vr.upper, 10.0);
}

void TestDataSource::soaIntValues()
{
    std::vector<double> keys = {1.0, 2.0, 3.0};
    std::vector<int> values = {100, 200, 300};
    QCPSoADataSource<std::vector<double>, std::vector<int>> src(
        std::move(keys), std::move(values));

    QCOMPARE(src.size(), 3);
    QCOMPARE(src.valueAt(0), 100.0);
    QCOMPARE(src.valueAt(2), 300.0);
}

void TestDataSource::multiSoaGapThresholdZeroDisablesGaps()
{
    // Issue SciQLopPlots#118: irregular state changes must be drawable as one line.
    QCPSoAMultiDataSource<std::vector<double>, std::vector<double>> src(
        std::vector<double>{0, 1, 5, 6, 12}, oneColumn({1, 0, 1, 0, 0}));

    QVERIFY(nanBreaks(src.linesInData(0, 0, 5, 0)) > 0);

    src.setGapThreshold(0);
    QCOMPARE(src.gapThreshold(), 0.0);
    QCOMPARE(nanBreaks(src.linesInData(0, 0, 5, 0)), 0);
    QCOMPARE(nanBreaks(src.linesInData(0, 0, 5, 400.0 / 12)), 0);
}

void TestDataSource::everySourceBreaksItsLinesAlike()
{
    // One key gap (3 -> 7) and one NaN sample, so both kinds of break are exercised.
    const std::vector<double> keys = { 0.0, 1.0, 2.0, 3.0, 7.0, 8.0, 9.0 };
    const std::vector<std::vector<double>> columns = {
        { 1.0, 2.0, std::numeric_limits<double>::quiet_NaN(), 3.0, 4.0, 5.0, 6.0 },
        { 2.0, 1.0, 4.0, -1.0, -2.0, 3.0, 5.0 },
    };

    QCPSoAMultiDataSource<std::vector<double>, std::vector<double>> soa(keys, columns);

    // Row-major, with a stride wider than the column count.
    const int rows = static_cast<int>(keys.size());
    const int stride = 3;
    std::vector<double> rowMajor(rows * stride, 0.0);
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < 2; ++c)
            rowMajor[r * stride + c] = columns[c][r];
    QCPRowMajorMultiDataSource<double, double> rowMajorSource(std::span<const double>(keys),
                                                              rowMajor.data(), rows, 2, stride);

    // Already-thinned (L2) bins: their lines must not be thinned again.
    qcp::algo::MultiColumnBinResult bins;
    bins.numColumns = 2;
    bins.keys = keys;
    bins.values = columns[0];
    bins.values.insert(bins.values.end(), columns[1].begin(), columns[1].end());
    bins.gapRatio.assign(keys.size(), 0.0f);
    QCPResampledMultiDataSource resampled(std::move(bins));

    auto shared = std::make_shared<QCPSoAMultiDataSource<std::vector<double>, std::vector<double>>>(
        keys, columns);
    QCPWaterfallDataAdapter waterfall(shared, { 0.0, 5.0 }, { 1.0, -1.0 }, 2.0);

    // The row-major source draws what the SoA one draws; the waterfall moves the points but keeps
    // the breaks; resampled bins break on their gap ratios only.
    for (int column = 0; column < 2; ++column)
    {
        const auto reference = soa.linesInData(column, 0, rows, 0.0);
        const auto isBreak = [](const QPointF& p) { return std::isnan(p.x()); };
        QCOMPARE(std::count_if(reference.begin(), reference.end(), isBreak),
                 std::ptrdiff_t(column == 0 ? 2 : 1)); // the key gap, and column 0's NaN sample
        const auto sameBreaks = [&](const std::vector<QPointF>& lines) {
            if (lines.size() != reference.size())
                return false;
            for (std::size_t i = 0; i < lines.size(); ++i)
                if (isBreak(lines[i]) != isBreak(reference[i]))
                    return false;
            return true;
        };
        const auto rowMajorLines = rowMajorSource.linesInData(column, 0, rows, 0.0);
        QVERIFY(sameBreaks(rowMajorLines));
        for (std::size_t i = 0; i < reference.size(); ++i)
            if (!isBreak(reference[i]))
                QCOMPARE(rowMajorLines[i], reference[i]);
        QVERIFY2(sameBreaks(waterfall.linesInData(column, 0, rows, 0.0)), "the waterfall moved a break");
        // Bins break only where their gap ratios say so (none here), and a NaN bin is joined.
        const auto bins = resampled.linesInData(column, 0, rows, 0.0);
        QCOMPARE(std::count_if(bins.begin(), bins.end(), isBreak), std::ptrdiff_t(0));
    }
}

void TestDataSource::thinnedLinesInDataKeepFourPointsPerPixel()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
    auto src = millionPointSource();

    const int end = src.size();
    const double pixelsPerKey = fitKeyPixelsPerUnit(mPlot, src);

    const auto data = src.linesInData(0, 0, end, pixelsPerKey);
    QVERIFY(!data.empty());
    QVERIFY2(static_cast<int>(data.size()) < end / 100, "linesInData did not thin");
    // The envelope keeps at most four points (first, min, max, last) per key pixel.
    QVERIFY(static_cast<int>(data.size()) <= 4 * (mPlot->axisRect()->width() + 2));
}

void TestDataSource::indicesFollowTheThinnedPoints()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
    auto src = millionPointSource();

    const int end = src.size();
    const double pixelsPerKey = fitKeyPixelsPerUnit(mPlot, src);

    std::vector<int> indices;
    const auto data = src.linesInData(0, 0, end, pixelsPerKey, &indices);
    QCOMPARE(static_cast<int>(indices.size()), static_cast<int>(data.size()));
    int realPoints = 0;
    for (int i = 0; i < static_cast<int>(data.size()); ++i)
    {
        if (std::isnan(data[i].x()))
        {
            QCOMPARE(indices[i], -1);
            continue;
        }
        QVERIFY(indices[i] >= 0 && indices[i] < end);
        // The envelope nudges a min/max point inside its bucket; the index still names the
        // row the point took its value from.
        QVERIFY2(std::abs(src.keyAt(indices[i]) - data[i].x()) <= 1.0 / pixelsPerKey,
                 "index is outside the point's bucket");
        QCOMPARE(src.valueAt(0, indices[i]), data[i].y());
        ++realPoints;
    }
    QVERIFY(realPoints > 0);

    // Unthinned: the index is the source row, and a NaN sample keeps its own break marker.
    std::vector<int> rawIndices;
    const auto raw = src.linesInData(0, 495, 505, 0.0, &rawIndices);
    QCOMPARE(static_cast<int>(rawIndices.size()), static_cast<int>(raw.size()));
    for (int i = 0; i < static_cast<int>(raw.size()); ++i)
    {
        if (std::isnan(raw[i].x()))
        {
            QCOMPARE(rawIndices[i], -1);
            continue;
        }
        QCOMPARE(rawIndices[i], 495 + i);
    }
    QVERIFY(
        std::any_of(raw.begin(), raw.end(), [](const QPointF& p) { return std::isnan(p.x()); }));
}

void TestDataSource::soaMismatchedLengthsDegradeToEmpty()
{
    // Shape lies from callers must not become OOB reads (release) or aborts
    // (debug): the source degrades to empty with a warning.
    std::vector<double> keys = {1.0, 2.0, 3.0};
    std::vector<double> values = {10.0, 20.0};
    QCPSoADataSource<std::vector<double>, std::vector<double>> src(
        std::move(keys), std::move(values));

    QCOMPARE(src.size(), 0);
    QVERIFY(src.empty());
    bool found = true;
    src.keyRange(found);
    QVERIFY(!found);
    src.valueRange(found);
    QVERIFY(!found);
}

// One-column QCPMultiGraph integration tests
void TestDataSource::multiGraphCreation()
{
    mPlot = new QCustomPlot();
    auto* graph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    QVERIFY(graph->dataSource() == nullptr);
}

void TestDataSource::multiGraphSetDataOwning()
{
    mPlot = new QCustomPlot();
    auto* graph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);

    std::vector<double> keys = {1.0, 2.0, 3.0};
    std::vector<std::vector<float>> values = {{10.0f, 20.0f, 30.0f}};
    graph->setData(std::move(keys), std::move(values));

    QVERIFY(graph->dataSource() != nullptr);
    QCOMPARE(graph->dataSource()->size(), 3);
    QCOMPARE(graph->dataSource()->columnCount(), 1);
    QCOMPARE(graph->dataSource()->keyAt(0), 1.0);
    QCOMPARE(graph->dataSource()->valueAt(0, 2), 30.0);
}

void TestDataSource::multiGraphViewData()
{
    mPlot = new QCustomPlot();
    auto* graph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);

    double keys[] = {1.0, 2.0, 3.0};
    int values[] = {100, 200, 300};
    graph->viewData(std::span<const double>(keys),
                    std::vector<std::span<const int>> { std::span<const int>(values) });

    QVERIFY(graph->dataSource() != nullptr);
    QCOMPARE(graph->dataSource()->size(), 3);
    QCOMPARE(graph->dataSource()->valueAt(0, 1), 200.0);
}

void TestDataSource::multiGraphSharedSource()
{
    mPlot = new QCustomPlot();
    auto* g1 = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    auto* g2 = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);

    auto source = std::make_shared<QCPSoAMultiDataSource<std::vector<double>, std::vector<double>>>(
        std::vector<double>{1.0, 2.0}, oneColumn({10.0, 20.0}));
    g1->setDataSource(source);
    g2->setDataSource(source);

    QCOMPARE(g1->dataSource(), g2->dataSource());
    QCOMPARE(g1->dataSource()->size(), 2);
}

void TestDataSource::multiGraphAxisRanges()
{
    mPlot = new QCustomPlot();
    auto* graph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);

    graph->setData(std::vector<double> {1.0, 2.0, 3.0, 4.0, 5.0},
                   oneColumn({-1.0, 5.0, 2.0, 8.0, 3.0}));

    bool found = false;
    auto kr = graph->getKeyRange(found);
    QVERIFY(found);
    QCOMPARE(kr.lower, 1.0);
    QCOMPARE(kr.upper, 5.0);

    auto vr = graph->getValueRange(found);
    QVERIFY(found);
    QCOMPARE(vr.lower, -1.0);
    QCOMPARE(vr.upper, 8.0);
}

void TestDataSource::multiGraphRender()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
    auto* graph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);

    graph->setData(std::vector<double> {1.0, 2.0, 3.0, 4.0, 5.0},
                   oneColumn({1.0, 4.0, 2.0, 5.0, 3.0}));
    graph->rescaleAxes();
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(true);

    std::vector<std::vector<float>> floats = {{0.0f, 1.0f, 0.5f}};
    graph->setData(std::vector<double> {0.0, 1.0, 2.0}, std::move(floats));
    graph->rescaleAxes();
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(true);

    double keys3[] = {0.0, 1.0, 2.0};
    int vals3[] = {0, 100, 50};
    graph->viewData(std::span<const double>(keys3),
                    std::vector<std::span<const int>> { std::span<const int>(vals3) });
    graph->rescaleAxes();
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(true);
}

void TestDataSource::multiGraphLineStyles()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
    auto* graph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);

    graph->setData(std::vector<double> {1.0, 2.0, 3.0, 4.0, 5.0},
                   oneColumn({1.0, 4.0, 2.0, 5.0, 3.0}));
    graph->rescaleAxes();

    QCOMPARE(graph->lineStyle(), QCPMultiGraph::lsLine);

    const QCPMultiGraph::LineStyle styles[] = {
        QCPMultiGraph::lsNone, QCPMultiGraph::lsLine,
        QCPMultiGraph::lsStepLeft, QCPMultiGraph::lsStepRight,
        QCPMultiGraph::lsStepCenter, QCPMultiGraph::lsImpulse
    };
    for (auto style : styles)
    {
        graph->setLineStyle(style);
        QCOMPARE(graph->lineStyle(), style);
        mPlot->replot(QCustomPlot::rpImmediateRefresh);
    }
}

void TestDataSource::multiGraphScatterStyle()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
    auto* graph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);

    graph->setData(std::vector<double> {1.0, 2.0, 3.0}, oneColumn({1.0, 2.0, 3.0}));
    graph->rescaleAxes();

    QVERIFY(graph->component(0).scatterStyle.isNone());

    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, 8));
    QCOMPARE(graph->component(0).scatterStyle.shape(), QCPScatterStyle::ssCircle);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    graph->setLineStyle(QCPMultiGraph::lsNone);
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCross, 6));
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
}

void TestDataSource::multiGraphScatterSkip()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
    auto* graph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);

    graph->setData(std::vector<double> {1.0, 2.0, 3.0, 4.0, 5.0},
                   oneColumn({1.0, 2.0, 3.0, 4.0, 5.0}));
    graph->rescaleAxes();
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 6));

    graph->setScatterSkip(2);
    QCOMPARE(graph->scatterSkip(), 2);

    graph->setScatterSkip(-5);
    QCOMPARE(graph->scatterSkip(), 0);

    mPlot->replot(QCustomPlot::rpImmediateRefresh);
}

void TestDataSource::multiGraphLineStyleNoneWithScatter()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
    auto* graph = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);

    graph->setData(std::vector<double> {1.0, 2.0, 3.0}, oneColumn({1.0, 2.0, 3.0}));
    graph->rescaleAxes();

    graph->setLineStyle(QCPMultiGraph::lsNone);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssPlus, 8));
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
}
