#include "test-datasource2d.h"
#include <qcustomplot.h>
#include <datasource/algorithms-2d.h>
#include <datasource/soa-datasource-2d.h>
#include <datasource/resample.h>
#include <QtTest/QtTest>
#include <cmath>
#include <span>

void TestDataSource2D::init()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
}

void TestDataSource2D::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
}

void TestDataSource2D::algo2dFindXBegin()
{
    std::vector<double> x = {1.0, 2.0, 3.0, 4.0, 5.0};
    QCOMPARE(qcp::algo2d::findXBegin(x, 2.5), 1);
    QCOMPARE(qcp::algo2d::findXBegin(x, 1.0), 0);
    QCOMPARE(qcp::algo2d::findXBegin(x, 0.5), 0);
    QCOMPARE(qcp::algo2d::findXBegin(x, 5.5), 4);
}

void TestDataSource2D::algo2dFindXEnd()
{
    std::vector<double> x = {1.0, 2.0, 3.0, 4.0, 5.0};
    QCOMPARE(qcp::algo2d::findXEnd(x, 3.5), 4);
    QCOMPARE(qcp::algo2d::findXEnd(x, 5.0), 5);
    QCOMPARE(qcp::algo2d::findXEnd(x, 0.5), 1);
}

void TestDataSource2D::algo2dXRange()
{
    std::vector<double> x = {1.0, 3.0, 5.0};
    bool found = false;
    auto range = qcp::algo2d::xRange(x, found);
    QVERIFY(found);
    QCOMPARE(range.lower, 1.0);
    QCOMPARE(range.upper, 5.0);
}

void TestDataSource2D::algo2dYRange()
{
    std::vector<double> y = {10.0, 20.0, 30.0};
    bool found = false;
    auto range = qcp::algo2d::yRange(y, found);
    QVERIFY(found);
    QCOMPARE(range.lower, 10.0);
    QCOMPARE(range.upper, 30.0);
}

void TestDataSource2D::algo2dZRange()
{
    // 3x2 grid: x=[0,1,2], ySize=2, z=[1,2, 3,4, 5,6]
    std::vector<double> z = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
    bool found = false;

    // Full range
    auto range = qcp::algo2d::zRange(z, 2, found);
    QVERIFY(found);
    QCOMPARE(range.lower, 1.0);
    QCOMPARE(range.upper, 6.0);

    // Visible x window [1, 3) → rows 1 and 2 → z values {3,4,5,6}
    found = false;
    range = qcp::algo2d::zRange(z, 2, found, 1, 3);
    QVERIFY(found);
    QCOMPARE(range.lower, 3.0);
    QCOMPARE(range.upper, 6.0);
}

void TestDataSource2D::soa2dUniformY()
{
    std::vector<double> x = {1.0, 2.0, 3.0};
    std::vector<double> y = {10.0, 20.0};
    std::vector<double> z = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};

    QCPSoADataSource2D src(std::move(x), std::move(y), std::move(z));

    QCOMPARE(src.xSize(), 3);
    QCOMPARE(src.ySize(), 2);
    QVERIFY(!src.yIs2D());
    QCOMPARE(src.xAt(0), 1.0);
    QCOMPARE(src.xAt(2), 3.0);
    QCOMPARE(src.yAt(0, 0), 10.0);
    QCOMPARE(src.yAt(2, 1), 20.0);
    QCOMPARE(src.zAt(0, 0), 1.0);
    QCOMPARE(src.zAt(2, 1), 6.0);
}

void TestDataSource2D::soa2dVariableY()
{
    std::vector<double> x = {1.0, 2.0};
    std::vector<double> y = {10.0, 20.0, 30.0, 11.0, 21.0, 31.0};
    std::vector<double> z = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};

    QCPSoADataSource2D src(std::move(x), std::move(y), std::move(z));

    QCOMPARE(src.xSize(), 2);
    QCOMPARE(src.ySize(), 3);
    QVERIFY(src.yIs2D());
    QCOMPARE(src.yAt(0, 0), 10.0);
    QCOMPARE(src.yAt(0, 2), 30.0);
    QCOMPARE(src.yAt(1, 0), 11.0);
    QCOMPARE(src.yAt(1, 2), 31.0);
}

void TestDataSource2D::soa2dSpanView()
{
    double x[] = {1.0, 2.0, 3.0};
    float y[] = {10.0f, 20.0f};
    double z[] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};

    QCPSoADataSource2D<std::span<const double>, std::span<const float>, std::span<const double>>
        src(std::span{x}, std::span{y}, std::span{z});

    QCOMPARE(src.xSize(), 3);
    QCOMPARE(src.ySize(), 2);
    QVERIFY(!src.yIs2D());
    QCOMPARE(src.yAt(0, 0), 10.0);
    QCOMPARE(src.zAt(1, 0), 3.0);
}

void TestDataSource2D::soa2dMixedTypes()
{
    std::vector<double> x = {1.0, 2.0};
    std::vector<float> y = {10.0f, 20.0f, 30.0f};
    std::vector<int> z = {1, 2, 3, 4, 5, 6};

    QCPSoADataSource2D src(std::move(x), std::move(y), std::move(z));

    QCOMPARE(src.xSize(), 2);
    QCOMPARE(src.ySize(), 3);
    QVERIFY(!src.yIs2D());
    QCOMPARE(src.zAt(0, 0), 1.0);
    QCOMPARE(src.zAt(1, 2), 6.0);
}

void TestDataSource2D::soa2dRangeQueries()
{
    std::vector<double> x = {1.0, 3.0, 5.0};
    std::vector<double> y = {10.0, 20.0};
    std::vector<double> z = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};

    QCPSoADataSource2D src(std::move(x), std::move(y), std::move(z));

    bool found = false;
    auto xr = src.xRange(found);
    QVERIFY(found);
    QCOMPARE(xr.lower, 1.0);
    QCOMPARE(xr.upper, 5.0);

    found = false;
    auto yr = src.yRange(found);
    QVERIFY(found);
    QCOMPARE(yr.lower, 10.0);
    QCOMPARE(yr.upper, 20.0);

    found = false;
    auto zr = src.zRange(found);
    QVERIFY(found);
    QCOMPARE(zr.lower, 1.0);
    QCOMPARE(zr.upper, 6.0);

    QCOMPARE(src.findXBegin(2.0), 0);
    QCOMPARE(src.findXEnd(4.0), 3);
}
void TestDataSource2D::resampleUniformGrid()
{
    std::vector<double> x = {0.0, 1.0, 2.0, 3.0, 4.0};
    std::vector<double> y = {0.0, 1.0, 2.0};
    std::vector<double> z(15);
    for (int i = 0; i < 5; ++i)
        for (int j = 0; j < 3; ++j)
            z[i * 3 + j] = i * 3.0 + j;

    QCPSoADataSource2D src(std::move(x), std::move(y), std::move(z));

    auto* result = qcp::algo2d::resample(src, 0, 5, QCPRange(0, 4), QCPRange(0, 2), 5, 3, false, 1.5);
    QVERIFY(result != nullptr);
    QCOMPARE(result->keySize(), 5);
    QCOMPARE(result->valueSize(), 3);

    QVERIFY(std::abs(result->cell(0, 0) - 0.0) < 1.0);
    QVERIFY(std::abs(result->cell(4, 2) - 14.0) < 2.0);

    delete result;
}

void TestDataSource2D::resampleVariableY()
{
    std::vector<double> x = {0.0, 1.0};
    std::vector<double> y = {10.0, 20.0, 15.0, 25.0};
    std::vector<double> z = {1.0, 2.0, 3.0, 4.0};

    QCPSoADataSource2D src(std::move(x), std::move(y), std::move(z));
    QVERIFY(src.yIs2D());

    auto* result = qcp::algo2d::resample(src, 0, 2, QCPRange(0, 1), QCPRange(10, 25), 2, 2, false, 1.5);
    QVERIFY(result != nullptr);
    QVERIFY(result->keySize() > 0);
    QVERIFY(result->valueSize() > 0);
    delete result;
}

void TestDataSource2D::resampleGapDetection()
{
    std::vector<double> x = {0.0, 1.0, 2.0, 10.0, 11.0, 12.0};
    std::vector<double> y = {0.0, 1.0};
    std::vector<double> z = {1.0, 1.0, 2.0, 2.0, 3.0, 3.0, 4.0, 4.0, 5.0, 5.0, 6.0, 6.0};

    QCPSoADataSource2D src(std::move(x), std::move(y), std::move(z));

    auto* result = qcp::algo2d::resample(src, 0, 6, QCPRange(0, 12), QCPRange(0, 1), 12, 2, false, 1.5);
    QVERIFY(result != nullptr);

    int midKey = result->keySize() / 2;
    QVERIFY(std::isnan(result->cell(midKey, 0)));

    delete result;
}

void TestDataSource2D::resampleGapDetectionViewportIndependent()
{
    // Three segments: [0..5 step 1], gap, [20..25 step 1], gap, [40..45 step 1]
    // Gap detection must produce the same result regardless of which xBegin we start from.
    std::vector<double> x, y = {0.0, 1.0}, z;
    auto addSeg = [&](double x0, double x1) {
        for (double v = x0; v <= x1; v += 1.0) {
            x.push_back(v);
            z.push_back(v);    // key-row 0
            z.push_back(v);    // key-row 1
        }
    };
    addSeg(0, 5);
    addSeg(20, 25);
    addSeg(40, 45);

    QCPSoADataSource2D src(std::move(x), std::move(y), std::move(z));
    int total = src.xSize(); // 18

    // Wide viewport covering all data — start from index 0
    auto* r1 = qcp::algo2d::resample(src, 0, total,
        QCPRange(-5, 50), QCPRange(0, 1), 55, 2, false, 2.0);
    QVERIFY(r1);

    // Same viewport but start from index 1 (simulating panned xBegin)
    auto* r2 = qcp::algo2d::resample(src, 1, total,
        QCPRange(-5, 50), QCPRange(0, 1), 55, 2, false, 2.0);
    QVERIFY(r2);

    // Count NaN columns in each — should be similar (both detect the same gaps)
    auto countNanCols = [](QCPColorMapData* d) {
        int n = 0;
        for (int i = 0; i < d->keySize(); ++i)
            if (std::isnan(d->cell(i, 0)))
                ++n;
        return n;
    };
    int nan1 = countNanCols(r1);
    int nan2 = countNanCols(r2);
    // Both should have gap bins; the counts should be close (within 1 bin tolerance)
    QVERIFY2(std::abs(nan1 - nan2) <= 1,
        qPrintable(QString("NaN columns differ: xBegin=0 has %1, xBegin=1 has %2").arg(nan1).arg(nan2)));

    // The gap region (x=6..19) must be NaN in both
    auto binForX = [](QCPColorMapData* d, double xVal) {
        double frac = (xVal - d->keyRange().lower) / d->keyRange().size();
        return std::clamp(static_cast<int>(frac * d->keySize()), 0, d->keySize() - 1);
    };
    int gapBin1 = binForX(r1, 12.0);  // middle of first gap
    int gapBin2 = binForX(r2, 12.0);
    QVERIFY2(std::isnan(r1->cell(gapBin1, 0)),
        qPrintable(QString("Gap at x=12 not NaN with xBegin=0 (bin %1)").arg(gapBin1)));
    QVERIFY2(std::isnan(r2->cell(gapBin2, 0)),
        qPrintable(QString("Gap at x=12 not NaN with xBegin=1 (bin %1)").arg(gapBin2)));

    delete r1;
    delete r2;
}

void TestDataSource2D::resampleGapBoundaryDataPreserved()
{
    // Two segments: [0, 1, 2] gap [10, 11, 12] with z = x-value
    // The first point of each segment (x=0, x=10) must contribute data, not be dropped.
    std::vector<double> x = {0.0, 1.0, 2.0, 10.0, 11.0, 12.0};
    std::vector<double> y = {0.0};
    std::vector<double> z = {100.0, 101.0, 102.0, 200.0, 201.0, 202.0};

    QCPSoADataSource2D src(std::move(x), std::move(y), std::move(z));

    // Use enough bins to resolve each source column
    auto* r = qcp::algo2d::resample(src, 0, 6,
        QCPRange(0, 12), QCPRange(0, 0.5), 12, 1, false, 2.0);
    QVERIFY(r);

    // Bin for x=0 (first point of first segment) must have data
    QVERIFY2(!std::isnan(r->cell(0, 0)),
        "First point of segment 1 (x=0) was dropped");

    // Bin for x=10 (first point of second segment) must have data
    int bin10 = static_cast<int>(10.0 / 12.0 * r->keySize());
    bin10 = std::clamp(bin10, 0, r->keySize() - 1);
    QVERIFY2(!std::isnan(r->cell(bin10, 0)),
        qPrintable(QString("First point of segment 2 (x=10) was dropped (bin %1)").arg(bin10)));

    // Gap region (x=3..9) must be NaN
    int gapBin = static_cast<int>(6.0 / 12.0 * r->keySize());
    gapBin = std::clamp(gapBin, 0, r->keySize() - 1);
    QVERIFY2(std::isnan(r->cell(gapBin, 0)),
        qPrintable(QString("Gap region at x=6 not NaN (bin %1, value %2)")
            .arg(gapBin).arg(r->cell(gapBin, 0))));

    delete r;
}

void TestDataSource2D::resampleLogY()
{
    std::vector<double> x = {0.0, 1.0};
    std::vector<double> y = {1.0, 10.0, 100.0};
    std::vector<double> z = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};

    QCPSoADataSource2D src(std::move(x), std::move(y), std::move(z));

    auto* result = qcp::algo2d::resample(src, 0, 2, QCPRange(0, 1), QCPRange(1, 100), 2, 3, true, 1.5);
    QVERIFY(result != nullptr);
    QVERIFY(result->keySize() > 0);
    QVERIFY(result->valueSize() > 0);
    delete result;
}

void TestDataSource2D::resampleEmptyBins()
{
    // With findBin nearest-neighbor mapping and nx = min(targetWidth, srcCount),
    // all source points map to valid bins. Verify no NaNs in a fully-covered grid.
    std::vector<double> x = {0.0, 1.0, 2.0, 3.0, 4.0};
    std::vector<double> y = {0.0, 1.0};
    std::vector<double> z = {1.0, 1.0, 2.0, 2.0, 3.0, 3.0, 4.0, 4.0, 5.0, 5.0};

    QCPSoADataSource2D src(std::move(x), std::move(y), std::move(z));

    auto* result = qcp::algo2d::resample(src, 0, 5, QCPRange(0, 4), QCPRange(0, 1), 5, 2, false, 1.5);
    QVERIFY(result != nullptr);
    QCOMPARE(result->keySize(), 5);
    QCOMPARE(result->valueSize(), 2);

    // All bins should be filled (no NaNs)
    int nanCount = 0;
    for (int i = 0; i < result->keySize(); ++i)
        for (int j = 0; j < result->valueSize(); ++j)
            if (std::isnan(result->cell(i, j)))
                ++nanCount;
    QCOMPARE(nanCount, 0);

    delete result;
}

void TestDataSource2D::resampleGapDetectedWhenZoomedIn()
{
    // Two segments: [0,1,2,3,4] gap [10,11,12,13,14], uniform y, z = x-value.
    // When the viewport is zoomed in at the gap boundary (e.g. [3.5, 10.5]),
    // findXBegin/findXEnd with expandedRange produce only 2 visible source
    // columns (x=3, x=10). Without context columns, gap detection fails and
    // both columns fill the entire viewport — the gap disappears.
    std::vector<double> x = {0,1,2,3,4, 10,11,12,13,14};
    std::vector<double> y = {0.0};
    std::vector<double> z = {0,1,2,3,4, 10,11,12,13,14};

    QCPSoADataSource2D src(std::move(x), std::move(y), std::move(z));

    // Simulate zoomed-in viewport at the gap boundary.
    // findXBegin(3.5) → index 3 (x=3, expanded), findXEnd(10.5) → index 6 (x=11, expanded)
    // That gives xBegin=3, xEnd=6, srcCount=3: columns x=3, x=10, x=11.
    // But in the worst case we might get only xBegin=3, xEnd=5, srcCount=2.
    // Force 2 columns straddling the gap to test the hardest case.
    int xBegin = 4; // x=4
    int xEnd = 6;   // x=11 (exclusive), so columns: x=4, x=10

    auto* r = qcp::algo2d::resample(src, xBegin, xEnd,
        QCPRange(3.5, 10.5), QCPRange(-0.5, 0.5), 100, 1, false, 1.5);
    QVERIFY(r);

    // The gap region (roughly bins 5-95, the middle ~90% of the viewport) must be NaN.
    int midBin = r->keySize() / 2;
    QVERIFY2(std::isnan(r->cell(midBin, 0)),
        qPrintable(QString("Gap not detected when zoomed in: bin %1 has value %2 instead of NaN")
            .arg(midBin).arg(r->cell(midBin, 0))));

    // Data bins near the edges should have values (from x=4 and x=10)
    QVERIFY2(!std::isnan(r->cell(0, 0)),
        "Left edge (x=4 region) should have data");
    QVERIFY2(!std::isnan(r->cell(r->keySize() - 1, 0)),
        "Right edge (x=10 region) should have data");

    delete r;
}

void TestDataSource2D::resampleGapDetectedWithTwoVisibleColumns()
{
    // Edge case: viewport is entirely inside the gap.
    // With context extension, the resample function should still detect the gap
    // and produce mostly NaN output.
    std::vector<double> x = {0,1,2,3, 20,21,22,23};
    std::vector<double> y = {0.0};
    std::vector<double> z = {0,1,2,3, 20,21,22,23};

    QCPSoADataSource2D src(std::move(x), std::move(y), std::move(z));

    // Viewport [5, 15] is entirely in the gap.
    // findXBegin(5) with expanded range → index 3 (x=3)
    // findXEnd(15) with expanded range → index 5 (x=21)
    // srcCount=2, columns: x=3 and x=20
    int xBegin = 3; // x=3
    int xEnd = 5;   // x=21 (exclusive), columns: x=3, x=20

    auto* r = qcp::algo2d::resample(src, xBegin, xEnd,
        QCPRange(5, 15), QCPRange(-0.5, 0.5), 100, 1, false, 1.5);
    QVERIFY(r);

    // Most bins should be NaN since the viewport is inside the gap.
    int nanCount = 0;
    for (int i = 0; i < r->keySize(); ++i)
        if (std::isnan(r->cell(i, 0)))
            ++nanCount;

    // At least 90% of bins should be NaN (gap region)
    QVERIFY2(nanCount > r->keySize() * 9 / 10,
        qPrintable(QString("Viewport inside gap: only %1/%2 bins are NaN, expected >90%%")
            .arg(nanCount).arg(r->keySize())));

    delete r;
}

void TestDataSource2D::resampleLogYNoBinGaps()
{
    // Bug #1: With log-scaled Y and widely-spaced channels (e.g. energy spectrogram),
    // arithmetic half-spacing left gaps between channels that showed as white lines.
    // With the fix, geometric midpoints are used, so all output bins get data.
    std::vector<double> x = {0.0, 1.0, 2.0};
    std::vector<double> y = {1.0, 10.0, 100.0, 1000.0, 10000.0};
    std::vector<double> z(15);
    for (int i = 0; i < 15; ++i)
        z[i] = static_cast<double>(i + 1);

    QCPSoADataSource2D src(std::move(x), std::move(y), std::move(z));

    auto* r = qcp::algo2d::resample(src, 0, 3,
        QCPRange(0, 2), QCPRange(1, 10000), 3, 50, true, 1.5);
    QVERIFY(r);

    // Every output bin should have data — no NaN gaps between channels
    int nanCount = 0;
    for (int i = 0; i < r->keySize(); ++i)
        for (int j = 0; j < r->valueSize(); ++j)
            if (std::isnan(r->cell(i, j)))
                ++nanCount;

    QVERIFY2(nanCount == 0,
        qPrintable(QString("Log-Y resample has %1 NaN bins out of %2 — channels have gaps")
            .arg(nanCount).arg(r->keySize() * r->valueSize())));

    delete r;
}

void TestDataSource2D::dataBoundsSkipsNaN()
{
    // Bug #4: recalculateDataBounds didn't skip NaN, leading to wrong Z range.
    QCPColorMapData data(3, 2, QCPRange(0, 2), QCPRange(0, 1));
    data.setCell(0, 0, 5.0);
    data.setCell(1, 0, std::nan(""));
    data.setCell(2, 0, 10.0);
    data.setCell(0, 1, std::nan(""));
    data.setCell(1, 1, 2.0);
    data.setCell(2, 1, std::nan(""));

    data.recalculateDataBounds();

    QCOMPARE(data.dataBounds().lower, 2.0);
    QCOMPARE(data.dataBounds().upper, 10.0);
}

void TestDataSource2D::resampleZoomedOutNotBlack()
{
    // Bug: the pipeline transform passed the full viewport range to resample()
    // even when zoomed out far beyond the data extent. This produced a texture
    // spanning the entire viewport with most bins NaN. Two consequences:
    // (1) texture could exceed GPU max size (>16384 px), silently failing
    // (2) within the data region, bins were wider than source fill radius
    //
    // Fix: pipeline clamps grid to intersection of viewport and data extent,
    // so the resampled result covers only the data and has bounded dimensions.
    mPlot->resize(400, 300);
    auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);

    std::vector<double> x = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    std::vector<double> y = {0.0, 1.0};
    std::vector<double> z(20);
    for (int i = 0; i < 20; ++i)
        z[i] = static_cast<double>(i + 1);
    cm->setData(std::move(x), std::move(y), std::move(z));
    cm->setDataRange(QCPRange(1, 20));

    // Zoom out 10x: viewport [-45, 54], data spans [0, 9]
    mPlot->xAxis->setRange(-45, 54);
    mPlot->yAxis->setRange(-5, 6);

    QSignalSpy spy(&cm->pipeline(), &QCPColormapPipeline::finished);
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 2000);

    auto* result = cm->pipeline().result();
    QVERIFY(result != nullptr);

    // The resampled grid should cover approximately the data extent, not the
    // full viewport. With the bug, keyRange would be [-45, 54] and most bins NaN.
    QCPRange rk = result->keyRange();
    QVERIFY2(rk.lower >= -1 && rk.upper <= 10,
        qPrintable(QString("Resampled key range [%1, %2] should be near data [0, 9], not viewport [-45, 54]")
            .arg(rk.lower).arg(rk.upper)));

    // Grid dimensions should be bounded by screen pixels, not inflated
    QVERIFY2(result->keySize() <= 400 * 4,
        qPrintable(QString("Resampled width %1 should not exceed 4x plot width (1600)")
            .arg(result->keySize())));

    // Most bins within the result should have data (not NaN)
    int filled = 0;
    for (int i = 0; i < result->keySize(); ++i)
        for (int j = 0; j < result->valueSize(); ++j)
            if (!std::isnan(result->cell(i, j)))
                ++filled;
    int total = result->keySize() * result->valueSize();
    QVERIFY2(filled > total * 50 / 100,
        qPrintable(QString("Only %1/%2 bins filled (>50%% expected) — data appears black")
            .arg(filled).arg(total)));
}

void TestDataSource2D::resampleLogYResolutionNotCoarse()
{
    // Bug: the pipeline computed the Y fraction of screen covered by data
    // using linear range sizes. For log-Y, data spanning 5 decades (1 to 1e5)
    // out of a 10-decade viewport (0.01 to 1e8) covers ~50% of screen pixels,
    // but linearly 99999/1e8 ≈ 0.001, giving pixH ≈ 1. The output grid then
    // has only a handful of Y bins, producing giant colored blocks.
    //
    // Fix: use log-space fraction for log-scaled axes.
    mPlot->resize(800, 600);
    auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);

    const int nChannels = 32;
    std::vector<double> x, y(nChannels), z;
    for (int j = 0; j < nChannels; ++j)
        y[j] = std::pow(10.0, 0.0 + j * 5.0 / (nChannels - 1)); // 1 to 1e5

    for (int i = 0; i < 100; ++i)
    {
        double t = i * 0.5;
        x.push_back(t);
        for (int j = 0; j < nChannels; ++j)
            z.push_back(std::sin(t * 0.1) * std::cos(j * 0.3));
    }
    cm->setData(std::move(x), std::move(y), std::move(z));

    // Log-Y axis, zoomed out: data covers 5 decades, viewport covers 10
    mPlot->yAxis->setScaleType(QCPAxis::stLogarithmic);
    mPlot->yAxis->setTicker(QSharedPointer<QCPAxisTickerLog>::create());
    mPlot->xAxis->setRange(0, 50);
    mPlot->yAxis->setRange(0.01, 1e8);

    QSignalSpy spy(&cm->pipeline(), &QCPColormapPipeline::finished);
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 2000);

    auto* result = cm->pipeline().result();
    QVERIFY(result != nullptr);

    // Data covers ~50% of screen (5/10 decades). With 600px height,
    // pixH should be ~300. The output grid should have at least pixH bins.
    // With the bug, pixH was ~1, giving valueSize around 32 (clamped to src.ySize()).
    QVERIFY2(result->valueSize() >= 100,
        qPrintable(QString("Output grid has only %1 Y bins — log-Y fraction bug gives coarse blocks")
            .arg(result->valueSize())));
}

void TestDataSource2D::resampleVariableYPerColumn()
{
    // Bug: Y bin ranges were precomputed from the first visible column only.
    // When Y values vary per column (yIs2D), subsequent columns used wrong
    // bin mapping, placing data in incorrect Y bins.
    //
    // Column 0 has Y={1, 10}. Column 2 has Y={1000, 10000} — dramatically
    // different. Without per-column Y bin computation, column 2's data gets
    // mapped using column 0's bin ranges (y=1..10), landing in the bottom
    // of the grid instead of the top.
    std::vector<double> x = {0, 1, 2};
    // Y is 2D: 3 columns × 2 rows = 6 values
    std::vector<double> y = {1, 10,   100, 1000,   1000, 10000};
    std::vector<double> z = {1.0, 2.0,  3.0, 4.0,  5.0, 6.0};

    QCPSoADataSource2D src(std::move(x), std::move(y), std::move(z));
    QVERIFY(src.yIs2D());

    // Resample with log Y. Output grid: 3 x-bins, 40 y-bins spanning [1, 10000].
    auto* r = qcp::algo2d::resample(src, 0, 3,
        QCPRange(0, 2), QCPRange(1, 10000), 3, 40, true, 0);
    QVERIFY(r);

    // Column 2 (x=2) has Y={1000, 10000}. Its high channel (z=6.0) should
    // land in the top quarter of the grid (y=10000 is the grid max).
    // Without the fix, it uses column 0's bin ranges (y=1..10), so data
    // lands near the bottom instead.
    int topBin = -1;
    for (int j = r->valueSize() - 1; j >= 0; --j)
    {
        if (!std::isnan(r->cell(2, j)))
        {
            topBin = j;
            break;
        }
    }

    QVERIFY2(topBin >= 0, "Column 2 should have data in the output grid");

    // y=10000 is at the top of a [1, 10000] log grid. It should be in the
    // top quarter (bin >= 30 out of 40). Without the fix, it lands near
    // bin 2-5 (column 0's y=10 region).
    QVERIFY2(topBin >= r->valueSize() * 3 / 4,
        qPrintable(QString("Column 2's top channel (y=10000) at bin %1 should be in top quarter (>=%2)")
            .arg(topBin).arg(r->valueSize() * 3 / 4)));

    delete r;
}

void TestDataSource2D::colormap2NanHandling()
{
    // Bug #2: Default NaN handling was nhNone, causing UB when colorizing
    // resampled data (which naturally contains NaN for empty bins).
    // After fix, QCPColorMap2 sets nhTransparent by default.
    auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
    QCOMPARE(cm->gradient().nanHandling(), QCPColorGradient::nhTransparent);
}

void TestDataSource2D::colormap2DataScaleTypeSync()
{
    // Bug #3: colorize() was always called with logarithmic=false.
    // Verify that dataScaleType propagates from QCPColorScale.
    auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
    auto* cs = new QCPColorScale(mPlot);
    mPlot->plotLayout()->addElement(0, 1, cs);

    QCOMPARE(cm->dataScaleType(), QCPAxis::stLinear);

    cs->setDataScaleType(QCPAxis::stLogarithmic);
    cm->setColorScale(cs);
    QCOMPARE(cm->dataScaleType(), QCPAxis::stLogarithmic);

    // Changing scale type on color scale propagates to colormap
    cs->setDataScaleType(QCPAxis::stLinear);
    QCOMPARE(cm->dataScaleType(), QCPAxis::stLinear);

    cs->setDataScaleType(QCPAxis::stLogarithmic);
    QCOMPARE(cm->dataScaleType(), QCPAxis::stLogarithmic);

    // Reverse direction: colormap -> color scale
    cm->setDataScaleType(QCPAxis::stLinear);
    QCOMPARE(cs->dataScaleType(), QCPAxis::stLinear);

    cm->setDataScaleType(QCPAxis::stLogarithmic);
    QCOMPARE(cs->dataScaleType(), QCPAxis::stLogarithmic);
}

void TestDataSource2D::colormap2Creation()
{
    auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
    QVERIFY(cm->dataSource() == nullptr);
    QCOMPARE(cm->gapThreshold(), 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
}

void TestDataSource2D::colormap2SetDataOwning()
{
    auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);

    std::vector<double> x = {0.0, 1.0, 2.0};
    std::vector<double> y = {0.0, 1.0};
    std::vector<double> z = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
    cm->setData(std::move(x), std::move(y), std::move(z));

    QVERIFY(cm->dataSource() != nullptr);
    QCOMPARE(cm->dataSource()->xSize(), 3);
    QCOMPARE(cm->dataSource()->ySize(), 2);
    QVERIFY(!cm->dataSource()->yIs2D());
}

void TestDataSource2D::colormap2ViewData()
{
    auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);

    double x[] = {0.0, 1.0};
    float y[] = {0.0f, 1.0f, 2.0f};
    double z[] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
    cm->viewData(x, 2, y, 3, z, 6);

    QVERIFY(cm->dataSource() != nullptr);
    QCOMPARE(cm->dataSource()->xSize(), 2);
    QCOMPARE(cm->dataSource()->ySize(), 3);
}

void TestDataSource2D::colormap2Render()
{
    mPlot->resize(400, 300);
    auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);

    std::vector<double> x = {0.0, 1.0, 2.0, 3.0, 4.0};
    std::vector<double> y = {0.0, 1.0, 2.0};
    std::vector<double> z(15);
    for (int i = 0; i < 5; ++i)
        for (int j = 0; j < 3; ++j)
            z[i * 3 + j] = i * 3.0 + j;
    cm->setData(std::move(x), std::move(y), std::move(z));
    cm->setGradient(QCPColorGradient(QCPColorGradient::gpJet));
    cm->setDataRange(QCPRange(0, 14));
    cm->rescaleAxes();

    // Render without waiting for async resample — draw() handles null mResampledData
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    // Now wait for async resample and render again
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.start(2000);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(mPlot, &QCustomPlot::afterReplot, &loop, &QEventLoop::quit);
    loop.exec();

    mPlot->replot(QCustomPlot::rpImmediateRefresh);
}

void TestDataSource2D::colormap2AxisRescale()
{
    auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);

    std::vector<double> x = {1.0, 2.0, 3.0};
    std::vector<double> y = {10.0, 20.0};
    std::vector<double> z = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
    cm->setData(std::move(x), std::move(y), std::move(z));

    bool found = false;
    auto kr = cm->getKeyRange(found, QCP::sdBoth);
    QVERIFY(found);
    QCOMPARE(kr.lower, 1.0);
    QCOMPARE(kr.upper, 3.0);

    found = false;
    auto vr = cm->getValueRange(found, QCP::sdBoth, QCPRange());
    QVERIFY(found);
    QCOMPARE(vr.lower, 10.0);
    QCOMPARE(vr.upper, 20.0);
}

void TestDataSource2D::colormap2ColorScaleSync()
{
    auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
    auto* cs = new QCPColorScale(mPlot);
    mPlot->plotLayout()->addElement(0, 1, cs);

    cm->setGradient(QCPColorGradient(QCPColorGradient::gpJet));
    cm->setDataRange(QCPRange(-1, 1));

    // Connect color scale
    cm->setColorScale(cs);

    // Colormap should adopt color scale's settings
    // (setColorScale calls setGradient/setDataRange from the scale)
    QCOMPARE(cm->dataRange().lower, cs->dataRange().lower);
    QCOMPARE(cm->dataRange().upper, cs->dataRange().upper);

    // Change range on color scale → should propagate to colormap
    cs->setDataRange(QCPRange(-5, 5));
    QCOMPARE(cm->dataRange().lower, -5.0);
    QCOMPARE(cm->dataRange().upper, 5.0);

    // Change range on colormap → should propagate to color scale
    cm->setDataRange(QCPRange(0, 10));
    QCOMPARE(cs->dataRange().lower, 0.0);
    QCOMPARE(cs->dataRange().upper, 10.0);
}
