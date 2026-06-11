#include "test-datasource.h"
#include "qcustomplot.h"
#include "datasource/algorithms.h"
#include "datasource/soa-datasource.h"
#include <vector>

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

void TestDataSource::algoLinesToPixels()
{
    // Tested in integration with QCPGraph2 (needs axis objects)
}

void TestDataSource::algoOptimizedLineData()
{
    // Tested in integration with QCPGraph2 (needs axis objects)
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

// QCPGraph2 integration tests
void TestDataSource::graph2Creation()
{
    mPlot = new QCustomPlot();
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    Q_UNUSED(graph);
    QVERIFY(graph->dataSource() == nullptr);
}
void TestDataSource::graph2SetDataOwning()
{
    mPlot = new QCustomPlot();
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);

    std::vector<double> keys = {1.0, 2.0, 3.0};
    std::vector<float> values = {10.0f, 20.0f, 30.0f};
    graph->setData(std::move(keys), std::move(values));

    QVERIFY(graph->dataSource() != nullptr);
    QCOMPARE(graph->dataSource()->size(), 3);
    QCOMPARE(graph->dataSource()->keyAt(0), 1.0);
    QCOMPARE(graph->dataSource()->valueAt(2), 30.0);
}

void TestDataSource::graph2ViewData()
{
    mPlot = new QCustomPlot();
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);

    double keys[] = {1.0, 2.0, 3.0};
    int values[] = {100, 200, 300};
    graph->viewData(keys, values, 3);

    QVERIFY(graph->dataSource() != nullptr);
    QCOMPARE(graph->dataSource()->size(), 3);
    QCOMPARE(graph->dataSource()->valueAt(1), 200.0);
}

void TestDataSource::graph2SharedSource()
{
    mPlot = new QCustomPlot();
    auto* g1 = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    auto* g2 = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);

    auto source = std::make_shared<QCPSoADataSource<std::vector<double>, std::vector<double>>>(
        std::vector<double>{1.0, 2.0}, std::vector<double>{10.0, 20.0});
    g1->setDataSource(source);
    g2->setDataSource(source);

    QCOMPARE(g1->dataSource(), g2->dataSource());
    QCOMPARE(g1->dataSource()->size(), 2);
}

void TestDataSource::graph2AxisRanges()
{
    mPlot = new QCustomPlot();
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);

    std::vector<double> keys = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> values = {-1.0, 5.0, 2.0, 8.0, 3.0};
    graph->setData(std::move(keys), std::move(values));

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

void TestDataSource::graph2Render()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);

    std::vector<double> keys = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> values = {1.0, 4.0, 2.0, 5.0, 3.0};
    graph->setData(std::move(keys), std::move(values));

    graph->rescaleAxes();
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(true);

    // Also test with float values
    std::vector<double> keys2 = {0.0, 1.0, 2.0};
    std::vector<float> vals2 = {0.0f, 1.0f, 0.5f};
    graph->setData(std::move(keys2), std::move(vals2));
    graph->rescaleAxes();
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(true);

    // Test with int values
    double keys3[] = {0.0, 1.0, 2.0};
    int vals3[] = {0, 100, 50};
    graph->viewData(keys3, vals3, 3);
    graph->rescaleAxes();
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(true);
}

void TestDataSource::graph2LineStyles()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);

    std::vector<double> keys = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> values = {1.0, 4.0, 2.0, 5.0, 3.0};
    graph->setData(std::move(keys), std::move(values));
    graph->rescaleAxes();

    QCOMPARE(graph->lineStyle(), QCPGraph2::lsLine);

    // Each line style should render without crashing
    const QCPGraph2::LineStyle styles[] = {
        QCPGraph2::lsNone, QCPGraph2::lsLine,
        QCPGraph2::lsStepLeft, QCPGraph2::lsStepRight,
        QCPGraph2::lsStepCenter, QCPGraph2::lsImpulse
    };
    for (auto style : styles)
    {
        graph->setLineStyle(style);
        QCOMPARE(graph->lineStyle(), style);
        mPlot->replot(QCustomPlot::rpImmediateRefresh);
    }
}

void TestDataSource::graph2ScatterStyle()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);

    std::vector<double> keys = {1.0, 2.0, 3.0};
    std::vector<double> values = {1.0, 2.0, 3.0};
    graph->setData(std::move(keys), std::move(values));
    graph->rescaleAxes();

    QVERIFY(graph->scatterStyle().isNone());

    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, 8));
    QCOMPARE(graph->scatterStyle().shape(), QCPScatterStyle::ssCircle);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    // Scatter-only (no lines)
    graph->setLineStyle(QCPGraph2::lsNone);
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCross, 6));
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
}

void TestDataSource::graph2ScatterSkip()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);

    std::vector<double> keys = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> values = {1.0, 2.0, 3.0, 4.0, 5.0};
    graph->setData(std::move(keys), std::move(values));
    graph->rescaleAxes();
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 6));

    graph->setScatterSkip(2);
    QCOMPARE(graph->scatterSkip(), 2);

    // Negative skip clamped to 0
    graph->setScatterSkip(-5);
    QCOMPARE(graph->scatterSkip(), 0);

    mPlot->replot(QCustomPlot::rpImmediateRefresh);
}

void TestDataSource::graph2LineStyleNoneWithScatter()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);

    std::vector<double> keys = {1.0, 2.0, 3.0};
    std::vector<double> values = {1.0, 2.0, 3.0};
    graph->setData(std::move(keys), std::move(values));
    graph->rescaleAxes();

    // lsNone + no scatter = nothing drawn, no crash
    graph->setLineStyle(QCPGraph2::lsNone);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    // lsNone + scatter = scatter only
    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssPlus, 8));
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
}
