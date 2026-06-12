#include "test-multi-datasource.h"
#include "qcustomplot.h"
#include "datasource/soa-multi-datasource.h"
#include "datasource/row-major-multi-datasource.h"
#include "datasource/resampled-multi-datasource.h"
#include <vector>
#include <span>

void TestMultiDataSource::init()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
}

void TestMultiDataSource::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
}

void TestMultiDataSource::soaOwningVector()
{
    std::vector<double> keys = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<std::vector<double>> cols = {
        {10.0, 20.0, 30.0, 40.0, 50.0},
        {-1.0, -2.0, -3.0, -4.0, -5.0},
        {0.5, 1.5, 2.5, 3.5, 4.5}
    };
    QCPSoAMultiDataSource src(std::move(keys), std::move(cols));
    QCOMPARE(src.columnCount(), 3);
    QCOMPARE(src.size(), 5);
    QVERIFY(!src.empty());
}

void TestMultiDataSource::soaViewSpan()
{
    std::vector<double> keys = {1.0, 2.0, 3.0};
    std::vector<double> v0 = {10.0, 20.0, 30.0};
    std::vector<double> v1 = {-1.0, -2.0, -3.0};

    QCPSoAMultiDataSource<std::span<const double>, std::span<const double>> src(
        std::span<const double>(keys),
        {std::span<const double>(v0), std::span<const double>(v1)});
    QCOMPARE(src.columnCount(), 2);
    QCOMPARE(src.size(), 3);
}

void TestMultiDataSource::soaColumnCount()
{
    std::vector<double> keys = {1.0};
    std::vector<std::vector<double>> cols;
    QCPSoAMultiDataSource src(std::move(keys), std::move(cols));
    QCOMPARE(src.columnCount(), 0);
}

void TestMultiDataSource::soaKeyAccess()
{
    std::vector<double> keys = {1.0, 2.0, 3.0};
    std::vector<std::vector<double>> cols = {{10.0, 20.0, 30.0}};
    QCPSoAMultiDataSource src(std::move(keys), std::move(cols));
    QCOMPARE(src.keyAt(0), 1.0);
    QCOMPARE(src.keyAt(1), 2.0);
    QCOMPARE(src.keyAt(2), 3.0);
}

void TestMultiDataSource::soaValueAccess()
{
    std::vector<double> keys = {1.0, 2.0, 3.0};
    std::vector<std::vector<double>> cols = {
        {10.0, 20.0, 30.0},
        {-1.0, -2.0, -3.0}
    };
    QCPSoAMultiDataSource src(std::move(keys), std::move(cols));
    QCOMPARE(src.valueAt(0, 0), 10.0);
    QCOMPARE(src.valueAt(0, 2), 30.0);
    QCOMPARE(src.valueAt(1, 0), -1.0);
    QCOMPARE(src.valueAt(1, 2), -3.0);
}

void TestMultiDataSource::soaFindBeginEnd()
{
    std::vector<double> keys = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<std::vector<double>> cols = {{0.0, 0.0, 0.0, 0.0, 0.0}};
    QCPSoAMultiDataSource src(std::move(keys), std::move(cols));
    QCOMPARE(src.findBegin(3.0, false), 2);
    QCOMPARE(src.findBegin(3.0, true), 1);
    QCOMPARE(src.findEnd(3.0, false), 3);
    QCOMPARE(src.findEnd(3.0, true), 4);
}

void TestMultiDataSource::soaKeyRange()
{
    std::vector<double> keys = {-2.0, 1.0, 3.0, 5.0};
    std::vector<std::vector<double>> cols = {{0.0, 0.0, 0.0, 0.0}};
    QCPSoAMultiDataSource src(std::move(keys), std::move(cols));
    bool found = false;
    auto range = src.keyRange(found);
    QVERIFY(found);
    QCOMPARE(range.lower, -2.0);
    QCOMPARE(range.upper, 5.0);
}

void TestMultiDataSource::soaValueRangePerColumn()
{
    std::vector<double> keys = {1.0, 2.0, 3.0};
    std::vector<std::vector<double>> cols = {
        {10.0, 20.0, 30.0},
        {-5.0, -15.0, -25.0}
    };
    QCPSoAMultiDataSource src(std::move(keys), std::move(cols));
    bool found = false;
    auto r0 = src.valueRange(0, found);
    QVERIFY(found);
    QCOMPARE(r0.lower, 10.0);
    QCOMPARE(r0.upper, 30.0);
    auto r1 = src.valueRange(1, found);
    QVERIFY(found);
    QCOMPARE(r1.lower, -25.0);
    QCOMPARE(r1.upper, -5.0);
}

void TestMultiDataSource::soaValueRangeWithKeyRestriction()
{
    std::vector<double> keys = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<std::vector<double>> cols = {{10.0, 50.0, 30.0, 20.0, 40.0}};
    QCPSoAMultiDataSource src(std::move(keys), std::move(cols));
    bool found = false;
    auto range = src.valueRange(0, found, QCP::sdBoth, QCPRange(2.0, 4.0));
    QVERIFY(found);
    QCOMPARE(range.lower, 20.0);
    QCOMPARE(range.upper, 50.0);
}

void TestMultiDataSource::soaGetLines()
{
    std::vector<double> keys = {1.0, 2.0, 3.0};
    std::vector<std::vector<double>> cols = {
        {10.0, 20.0, 30.0},
        {-1.0, -2.0, -3.0}
    };
    QCPSoAMultiDataSource src(std::move(keys), std::move(cols));
    auto* keyAxis = mPlot->xAxis;
    auto* valueAxis = mPlot->yAxis;
    keyAxis->setRange(0, 4);
    valueAxis->setRange(-5, 35);
    mPlot->replot();
    auto lines0 = src.getLines(0, 0, 3, keyAxis, valueAxis);
    auto lines1 = src.getLines(1, 0, 3, keyAxis, valueAxis);
    QCOMPARE(lines0.size(), 3);
    QCOMPARE(lines1.size(), 3);
    QVERIFY(lines0[0].y() != lines1[0].y());
}

void TestMultiDataSource::soaGetOptimizedLineData()
{
    std::vector<double> keys = {1.0, 2.0, 3.0};
    std::vector<std::vector<double>> cols = {{10.0, 20.0, 30.0}};
    QCPSoAMultiDataSource src(std::move(keys), std::move(cols));
    auto* keyAxis = mPlot->xAxis;
    auto* valueAxis = mPlot->yAxis;
    keyAxis->setRange(0, 4);
    valueAxis->setRange(0, 35);
    mPlot->replot();
    auto lines = src.getLines(0, 0, 3, keyAxis, valueAxis);
    auto optimized = src.getOptimizedLineData(0, 0, 3, 400, keyAxis, valueAxis);
    QCOMPARE(optimized.size(), lines.size());
}

void TestMultiDataSource::soaEmpty()
{
    std::vector<double> keys;
    std::vector<std::vector<double>> cols;
    QCPSoAMultiDataSource src(std::move(keys), std::move(cols));
    QCOMPARE(src.size(), 0);
    QVERIFY(src.empty());
    QCOMPARE(src.columnCount(), 0);
    bool found = false;
    src.keyRange(found);
    QVERIFY(!found);
}

void TestMultiDataSource::soaMixedTypes()
{
    std::vector<float> keys = {1.0f, 2.0f, 3.0f};
    std::vector<std::vector<int>> cols = {{10, 20, 30}, {-1, -2, -3}};
    QCPSoAMultiDataSource src(std::move(keys), std::move(cols));
    QCOMPARE(src.size(), 3);
    QCOMPARE(src.columnCount(), 2);
    QCOMPARE(src.keyAt(0), 1.0);
    QCOMPARE(src.valueAt(1, 2), -3.0);
}

void TestMultiDataSource::linesToPixelsNanProducesNanNotZero()
{
    // Reproducer: NaN values in data were mapped to pixel (0,0) instead of NaN,
    // causing the line extruder to draw diagonal fan lines to the top-left corner.
    constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
    std::vector<double> keys = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<std::vector<double>> cols = {{10.0, NaN, 30.0, NaN, 50.0}};
    QCPSoAMultiDataSource src(std::move(keys), std::move(cols));

    mPlot->xAxis->setRange(0, 6);
    mPlot->yAxis->setRange(0, 60);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    auto lines = src.getLines(0, 0, 5, mPlot->xAxis, mPlot->yAxis);
    QCOMPARE(lines.size(), 5);

    // Valid points must not be at (0,0)
    QVERIFY(lines[0].x() != 0 || lines[0].y() != 0);
    QVERIFY(lines[2].x() != 0 || lines[2].y() != 0);
    QVERIFY(lines[4].x() != 0 || lines[4].y() != 0);

    // NaN points must be NaN (so the extruder breaks the polyline), not (0,0)
    QVERIFY(qIsNaN(lines[1].x()) || qIsNaN(lines[1].y()));
    QVERIFY(qIsNaN(lines[3].x()) || qIsNaN(lines[3].y()));
}

void TestMultiDataSource::adaptiveSamplingNanDoesNotPoisonMinMax()
{
    // Reproducer: if the first value in a pixel interval was NaN, minValue/maxValue
    // started as NaN and all subsequent valid values were lost (NaN comparisons
    // always return false), producing empty or NaN-filled output.
    constexpr double NaN = std::numeric_limits<double>::quiet_NaN();

    // Dense data so adaptive sampling kicks in (needs dataCount >= maxCount).
    // 10000 points across a 400px widget → ~25 points per pixel → adaptive.
    const int N = 10000;
    std::vector<double> keys(N);
    std::vector<double> vals(N);
    for (int i = 0; i < N; ++i)
    {
        keys[i] = static_cast<double>(i);
        // Sprinkle NaN at interval boundaries (every 25th point)
        vals[i] = (i % 25 == 0) ? NaN : static_cast<double>(i * 0.1);
    }
    std::vector<std::vector<double>> cols = {std::move(vals)};
    QCPSoAMultiDataSource src(std::move(keys), std::move(cols));

    mPlot->xAxis->setRange(0, N);
    mPlot->yAxis->setRange(0, N * 0.1);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    auto result = src.getOptimizedLineData(0, 0, N, 400, mPlot->xAxis, mPlot->yAxis);
    QVERIFY(result.size() > 0);

    // Count valid (non-NaN) output points — must be the majority
    int validCount = 0;
    for (const auto& pt : result)
        if (!qIsNaN(pt.x()) && !qIsNaN(pt.y()))
            ++validCount;

    // Before the fix, NaN-poisoned intervals produced zero valid points.
    // With the fix, nearly all output points should be valid.
    QVERIFY2(validCount > result.size() / 2,
             qPrintable(QString("Only %1/%2 valid points").arg(validCount).arg(result.size())));

    // No point should be at pixel (0,0) — that was the old broken mapping
    for (const auto& pt : result)
    {
        if (qIsNaN(pt.x()) || qIsNaN(pt.y())) continue;
        QVERIFY2(pt.x() != 0.0 || pt.y() != 0.0,
                 "NaN data produced pixel (0,0) instead of NaN gap");
    }
}

void TestMultiDataSource::linesToPixelsBreaksAtKeyGaps()
{
    // Reproducer: data with a large time gap (e.g. missing measurement period)
    // drew a straight line across the gap instead of breaking the polyline.
    // Keys: 1,2,3, [gap], 100,101,102
    std::vector<double> keys = {1.0, 2.0, 3.0, 100.0, 101.0, 102.0};
    std::vector<std::vector<double>> cols = {{10.0, 20.0, 30.0, 40.0, 50.0, 60.0}};
    QCPSoAMultiDataSource src(std::move(keys), std::move(cols));

    mPlot->xAxis->setRange(0, 110);
    mPlot->yAxis->setRange(0, 70);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    auto lines = src.getLines(0, 0, 6, mPlot->xAxis, mPlot->yAxis);

    // Must contain at least one NaN gap marker between the two data segments
    bool foundNanGap = false;
    for (const auto& pt : lines)
    {
        if (qIsNaN(pt.x()) || qIsNaN(pt.y()))
        {
            foundNanGap = true;
            break;
        }
    }
    QVERIFY2(foundNanGap, "No gap break found between key=3 and key=100");
}

void TestMultiDataSource::resampledGetLinesBreaksAtKeyGaps()
{
    // Same gap detection test but through the L2 resampled path.
    // Build a MultiColumnBinResult with a gap in the keys.
    qcp::algo::MultiColumnBinResult bins;
    bins.numColumns = 1;
    bins.keys = {1.0, 2.0, 3.0, 100.0, 101.0, 102.0};
    bins.values = {10.0, 20.0, 30.0, 40.0, 50.0, 60.0};

    QCPResampledMultiDataSource src(std::move(bins));

    mPlot->xAxis->setRange(0, 110);
    mPlot->yAxis->setRange(0, 70);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    auto lines = src.getLines(0, 0, 6, mPlot->xAxis, mPlot->yAxis);

    bool foundNanGap = false;
    for (const auto& pt : lines)
    {
        if (qIsNaN(pt.x()) || qIsNaN(pt.y()))
        {
            foundNanGap = true;
            break;
        }
    }
    QVERIFY2(foundNanGap, "Resampled path: no gap break between key=3 and key=100");
}

void TestMultiDataSource::adaptiveSamplingBreaksAtKeyGaps()
{
    // Reproducer: QCPGraph2 adaptive sampling path (optimizedLineData) drew
    // lines across key gaps because it had no gap detection — only the
    // linesToPixels fallback path detected gaps.
    // Build data dense enough to trigger adaptive sampling: many points per
    // pixel, with a large gap in the middle.
    // Segment 1: keys [0, 500) step 0.1 => 5000 points
    // Segment 2: keys [10000, 10500) step 0.1 => 5000 points
    const int segSize = 5000;
    std::vector<double> keys(segSize * 2);
    std::vector<double> vals(segSize * 2);
    for (int i = 0; i < segSize; ++i)
    {
        keys[i] = i * 0.1;
        vals[i] = 1.0;
        keys[segSize + i] = 10000.0 + i * 0.1;
        vals[segSize + i] = 2.0;
    }

    // Set up axes so the pixel span is much smaller than the data count
    // (forces adaptive sampling path)
    mPlot->setGeometry(50, 50, 500, 500);
    mPlot->xAxis->setRange(-100, 11000);
    mPlot->yAxis->setRange(0, 3);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    auto lines = qcp::algo::optimizedLineData(
        keys, vals, 0, segSize * 2, 500, mPlot->xAxis, mPlot->yAxis);

    // Must contain at least one NaN gap marker between the two segments
    bool foundNanGap = false;
    for (const auto& pt : lines)
    {
        if (qIsNaN(pt.x()) || qIsNaN(pt.y()))
        {
            foundNanGap = true;
            break;
        }
    }
    QVERIFY2(foundNanGap,
             "Adaptive sampling: no gap break found between key=500 and key=10000");
}

void TestMultiDataSource::rowMajorValueAt()
{
    // 3 rows, 2 columns, packed (stride == columns)
    //   row0: [10, 20]
    //   row1: [30, 40]
    //   row2: [50, 60]
    std::vector<double> keys = {1.0, 2.0, 3.0};
    std::vector<double> values = {10, 20, 30, 40, 50, 60};
    QCPRowMajorMultiDataSource<double, double> src(
        std::span<const double>(keys), values.data(), 3, 2, 2);

    QCOMPARE(src.size(), 3);
    QCOMPARE(src.columnCount(), 2);
    QCOMPARE(src.keyAt(0), 1.0);
    QCOMPARE(src.keyAt(2), 3.0);
    QCOMPARE(src.valueAt(0, 0), 10.0); // col 0, row 0
    QCOMPARE(src.valueAt(1, 0), 20.0); // col 1, row 0
    QCOMPARE(src.valueAt(0, 1), 30.0); // col 0, row 1
    QCOMPARE(src.valueAt(1, 1), 40.0); // col 1, row 1
    QCOMPARE(src.valueAt(0, 2), 50.0); // col 0, row 2
    QCOMPARE(src.valueAt(1, 2), 60.0); // col 1, row 2
}

void TestMultiDataSource::rowMajorWithPadding()
{
    // 3 rows, 2 columns, stride=4 (2 padding elements per row)
    //   row0: [10, 20, __, __]
    //   row1: [30, 40, __, __]
    //   row2: [50, 60, __, __]
    std::vector<double> keys = {1.0, 2.0, 3.0};
    std::vector<double> values = {10, 20, 99, 99,
                                   30, 40, 99, 99,
                                   50, 60, 99, 99};
    QCPRowMajorMultiDataSource<double, double> src(
        std::span<const double>(keys), values.data(), 3, 2, 4);

    QCOMPARE(src.size(), 3);
    QCOMPARE(src.columnCount(), 2);
    QCOMPARE(src.valueAt(0, 0), 10.0);
    QCOMPARE(src.valueAt(1, 0), 20.0);
    QCOMPARE(src.valueAt(0, 1), 30.0);
    QCOMPARE(src.valueAt(1, 1), 40.0);
    QCOMPARE(src.valueAt(0, 2), 50.0);
    QCOMPARE(src.valueAt(1, 2), 60.0);

    // Verify StridedColumnView sees the right values
    qcp::detail::StridedColumnView<double> col0(values.data() + 0, 3, 4);
    QCOMPARE(col0[0], 10.0);
    QCOMPARE(col0[1], 30.0);
    QCOMPARE(col0[2], 50.0);
    QCOMPARE(col0.size(), 3);
}

void TestMultiDataSource::rowMajorGetLines()
{
    mPlot->xAxis->setRange(0, 6);
    mPlot->yAxis->setRange(0, 110);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    std::vector<double> keys = {1.0, 2.0, 3.0, 4.0, 5.0};
    std::vector<double> values = {10, 20,
                                   30, 40,
                                   50, 60,
                                   70, 80,
                                   90, 100};
    QCPRowMajorMultiDataSource<double, double> src(
        std::span<const double>(keys), values.data(), 5, 2, 2);

    auto lines0 = src.getLines(0, 0, 5, mPlot->xAxis, mPlot->yAxis);
    auto lines1 = src.getLines(1, 0, 5, mPlot->xAxis, mPlot->yAxis);
    QCOMPARE(lines0.size(), 5);
    QCOMPARE(lines1.size(), 5);

    auto optLines = src.getOptimizedLineData(0, 0, 5, 400,
                                              mPlot->xAxis, mPlot->yAxis);
    QVERIFY(optLines.size() > 0);
    QVERIFY(optLines.size() <= 5);
}

void TestMultiDataSource::soaMismatchedColumnDegradesToEmpty()
{
    // A value column shorter than the keys must not become an OOB read
    // (release) or abort (debug): the source degrades to empty with a warning.
    std::vector<double> keys = {1.0, 2.0, 3.0};
    std::vector<std::vector<double>> columns;
    columns.push_back({10.0, 20.0, 30.0});
    columns.push_back({40.0, 50.0}); // short column, wrong on purpose
    QCPSoAMultiDataSource<std::vector<double>, std::vector<double>> src(
        std::move(keys), std::move(columns));

    QCOMPARE(src.size(), 0);
    QCOMPARE(src.columnCount(), 0);
    bool found = true;
    src.keyRange(found);
    QVERIFY(!found);
}

void TestMultiDataSource::rowMajorInvalidShapeDegradesToEmpty()
{
    std::vector<double> keys = {1.0, 2.0, 3.0};
    std::vector<double> values = {10, 20, 30, 40, 50, 60};

    {
        // keys shorter than rows
        QCPRowMajorMultiDataSource<double, double> src(
            std::span<const double>(keys.data(), 2), values.data(), 3, 2, 2);
        QCOMPARE(src.size(), 0);
        QCOMPARE(src.columnCount(), 0);
    }
    {
        // stride narrower than the column count
        QCPRowMajorMultiDataSource<double, double> src(
            std::span<const double>(keys), values.data(), 3, 2, 1);
        QCOMPARE(src.size(), 0);
    }
    {
        // null values with rows > 0
        QCPRowMajorMultiDataSource<double, double> src(
            std::span<const double>(keys), nullptr, 3, 2, 2);
        QCOMPARE(src.size(), 0);
    }
    {
        // rows == 0 with non-empty keys is a shape lie, not a genuinely
        // empty source: it must warn like the other mismatch paths
        QTest::ignoreMessage(QtWarningMsg,
            QRegularExpression("QCPRowMajorMultiDataSource: invalid shape"));
        QCPRowMajorMultiDataSource<double, double> src(
            std::span<const double>(keys), values.data(), 0, 2, 2);
        QCOMPARE(src.size(), 0);
    }
}

// Helper: build an L1 cache with uniform keys and per-column values
static qcp::algo::MultiGraphResamplerCache makeL1Cache(
    const std::vector<double>& keys,
    const std::vector<std::vector<double>>& columns)
{
    int N = static_cast<int>(columns.size());
    int sz = static_cast<int>(keys.size());
    qcp::algo::MultiGraphResamplerCache cache;
    cache.level1.numColumns = N;
    cache.level1.keys = keys;
    // L1 is column-major: each column has `sz` entries (stride = sz)
    cache.level1.values.resize(N * sz);
    for (int c = 0; c < N; ++c)
        for (int i = 0; i < sz; ++i)
            cache.level1.values[c * sz + i] = columns[c][i];
    cache.sourceSize = sz;
    cache.columnCount = N;
    cache.cachedKeyRange = QCPRange(keys.front(), keys.back());
    return cache;
}

void TestMultiDataSource::l2MultiBasicMinMax()
{
    // 10000 uniform points in [0, 100), 1 column with values = key * 2.
    // With plotWidth=10 and kLevel2PixelMultiplier=4, l2Bins=40.
    // 10000 points >> 40 bins, so L2 kicks in.
    // Each bin covers 100/40 = 2.5 key-units = 250 L1 points.
    // For bin 0 (keys [0, 2.5)): min = 0*2 = 0, max = 249*0.01*2 ≈ 4.98
    const int N = 10000;
    std::vector<double> keys(N);
    std::vector<double> vals(N);
    for (int i = 0; i < N; ++i)
    {
        keys[i] = i * 0.01; // 0.00 .. 99.99
        vals[i] = keys[i] * 2.0;
    }
    auto cache = makeL1Cache(keys, {vals});

    ViewportParams vp;
    vp.keyRange = QCPRange(0, 100);
    vp.plotWidthPx = 10; // l2Bins = 40
    auto result = qcp::algo::resampleL2Multi(cache, vp);
    QVERIFY(result != nullptr);
    QCOMPARE(result->columnCount(), 1);
    QVERIFY(result->size() > 0);

    // Every output point should have a valid (non-NaN) value
    for (int i = 0; i < result->size(); ++i)
        QVERIFY2(!std::isnan(result->valueAt(0, i)),
                 qPrintable(QString("NaN at index %1").arg(i)));

    // The overall min/max across the L2 output should match the input range
    bool found = false;
    auto vr = result->valueRange(0, found);
    QVERIFY(found);
    QVERIFY(vr.lower < 1.0);   // close to 0
    QVERIFY(vr.upper > 198.0); // close to 200
}

void TestMultiDataSource::l2MultiNanSkipped()
{
    // Verify NaN values in L1 don't poison L2 bins.
    const int N = 10000;
    constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
    std::vector<double> keys(N);
    std::vector<double> vals(N);
    for (int i = 0; i < N; ++i)
    {
        keys[i] = i * 0.01;
        vals[i] = (i % 10 == 0) ? NaN : 5.0; // every 10th is NaN, rest = 5
    }
    auto cache = makeL1Cache(keys, {vals});

    ViewportParams vp;
    vp.keyRange = QCPRange(0, 100);
    vp.plotWidthPx = 10;
    auto result = qcp::algo::resampleL2Multi(cache, vp);
    QVERIFY(result != nullptr);

    // All output values should be exactly 5.0 (min and max of non-NaN data)
    for (int i = 0; i < result->size(); ++i)
    {
        double v = result->valueAt(0, i);
        if (std::isnan(v)) continue; // empty-bin sentinels converted to NaN
        QCOMPARE(v, 5.0);
    }
}

void TestMultiDataSource::l2MultiMultiColumnConsistency()
{
    // Two columns with different value ranges — verify they don't cross-contaminate.
    const int N = 10000;
    std::vector<double> keys(N);
    std::vector<double> col0(N), col1(N);
    for (int i = 0; i < N; ++i)
    {
        keys[i] = i * 0.01;
        col0[i] = 1.0;    // constant 1
        col1[i] = 1000.0; // constant 1000
    }
    auto cache = makeL1Cache(keys, {col0, col1});

    ViewportParams vp;
    vp.keyRange = QCPRange(0, 100);
    vp.plotWidthPx = 10;
    auto result = qcp::algo::resampleL2Multi(cache, vp);
    QVERIFY(result != nullptr);
    QCOMPARE(result->columnCount(), 2);

    for (int i = 0; i < result->size(); ++i)
    {
        double v0 = result->valueAt(0, i);
        double v1 = result->valueAt(1, i);
        if (!std::isnan(v0)) QCOMPARE(v0, 1.0);
        if (!std::isnan(v1)) QCOMPARE(v1, 1000.0);
    }
}

void TestMultiDataSource::l2MultiSparseReturnNull()
{
    // When visible points <= l2Bins, resampleL2Multi should return nullptr
    // (the caller uses raw data instead).
    std::vector<double> keys = {1.0, 2.0, 3.0};
    std::vector<double> vals = {10.0, 20.0, 30.0};
    auto cache = makeL1Cache(keys, {vals});

    ViewportParams vp;
    vp.keyRange = QCPRange(0, 4);
    vp.plotWidthPx = 100; // l2Bins = 400 >> 3 points
    auto result = qcp::algo::resampleL2Multi(cache, vp);
    QVERIFY(result == nullptr);
}

void TestMultiDataSource::l2MultiEmptyInput()
{
    qcp::algo::MultiGraphResamplerCache cache;
    cache.level1.numColumns = 0;
    ViewportParams vp;
    vp.keyRange = QCPRange(0, 100);
    vp.plotWidthPx = 100;
    auto result = qcp::algo::resampleL2Multi(cache, vp);
    QVERIFY(result == nullptr);
}
