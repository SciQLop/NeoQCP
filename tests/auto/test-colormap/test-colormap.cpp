#include "test-colormap.h"
#include <QMainWindow>
#include <painting/colormap-rhi-layer.h>
#include <painting/contour-extractor.h>
#include <QtWidgets/qtestsupport_widgets.h> // QTest::qWaitForWindowExposed
#include <limits>

void TestColorMap::init()
{
  mPlot = new QCustomPlot(0);
  mColorMap = new QCPColorMap(mPlot->xAxis, mPlot->yAxis);
}

void TestColorMap::QCPColorScale_rescaleDataRange()
{
  QCPColorScale *scale = new QCPColorScale(mPlot);
  mPlot->plotLayout()->addElement(0, 1, scale);
  
  QCPColorMap *map1 = new QCPColorMap(mPlot->xAxis, mPlot->yAxis);
  QCPColorMap *map2 = new QCPColorMap(mPlot->xAxis, mPlot->yAxis);
  map1->setColorScale(scale);
  map2->setColorScale(scale);
  map1->data()->setSize(2, 2);
  map2->data()->setSize(2, 2);
  
  // normal values:
  map1->data()->setCell(0, 0, 1);
  map1->data()->setCell(1, 0, 2);
  map1->data()->setCell(0, 1, 3);
  map1->data()->setCell(1, 1, 4);
  map1->data()->recalculateDataBounds();
  map2->data()->setCell(0, 0, 6);
  map2->data()->setCell(1, 0, 7);
  map2->data()->setCell(0, 1, 8);
  map2->data()->setCell(1, 1, 9);
  map2->data()->recalculateDataBounds();
  scale->rescaleDataRange(true);
  QCOMPARE(map1->dataRange().lower, 1.0);
  QCOMPARE(map1->dataRange().upper, 9.0);
  QCOMPARE(map2->dataRange().lower, 1.0);
  QCOMPARE(map2->dataRange().upper, 9.0);
  QCOMPARE(scale->dataRange().lower, 1.0);
  QCOMPARE(scale->dataRange().upper, 9.0);
  
  // one singular:
  map1->data()->setCell(0, 0, 2);
  map1->data()->setCell(1, 0, 2);
  map1->data()->setCell(0, 1, 2);
  map1->data()->setCell(1, 1, 2);
  map1->data()->recalculateDataBounds();
  map2->data()->setCell(0, 0, 6);
  map2->data()->setCell(1, 0, 7);
  map2->data()->setCell(0, 1, 8);
  map2->data()->setCell(1, 1, 9);
  map2->data()->recalculateDataBounds();
  scale->rescaleDataRange(true);
  QCOMPARE(map1->dataRange().lower, 2.0);
  QCOMPARE(map1->dataRange().upper, 9.0);
  QCOMPARE(map2->dataRange().lower, 2.0);
  QCOMPARE(map2->dataRange().upper, 9.0);
  QCOMPARE(scale->dataRange().lower, 2.0);
  QCOMPARE(scale->dataRange().upper, 9.0);
  
  // both singular:
  map1->data()->setCell(0, 0, 1);
  map1->data()->setCell(1, 0, 1);
  map1->data()->setCell(0, 1, 1);
  map1->data()->setCell(1, 1, 1);
  map1->data()->recalculateDataBounds();
  map2->data()->setCell(0, 0, 6);
  map2->data()->setCell(1, 0, 6);
  map2->data()->setCell(0, 1, 6);
  map2->data()->setCell(1, 1, 6);
  map2->data()->recalculateDataBounds();
  scale->rescaleDataRange(true);
  QCOMPARE(map1->dataRange().lower, 1.0);
  QCOMPARE(map1->dataRange().upper, 6.0);
  QCOMPARE(map2->dataRange().lower, 1.0);
  QCOMPARE(map2->dataRange().upper, 6.0);
  QCOMPARE(scale->dataRange().lower, 1.0);
  QCOMPARE(scale->dataRange().upper, 6.0);
  
  // both singular at same value (range should center on value):
  scale->setDataRange(QCPRange(0, 1));
  map1->data()->setCell(0, 0, 3);
  map1->data()->setCell(1, 0, 3);
  map1->data()->setCell(0, 1, 3);
  map1->data()->setCell(1, 1, 3);
  map1->data()->recalculateDataBounds();
  map2->data()->setCell(0, 0, 3);
  map2->data()->setCell(1, 0, 3);
  map2->data()->setCell(0, 1, 3);
  map2->data()->setCell(1, 1, 3);
  map2->data()->recalculateDataBounds();
  scale->rescaleDataRange(true);
  QCOMPARE(map1->dataRange().lower, 2.5);
  QCOMPARE(map1->dataRange().upper, 3.5);
  QCOMPARE(map2->dataRange().lower, 2.5);
  QCOMPARE(map2->dataRange().upper, 3.5);
  QCOMPARE(scale->dataRange().lower, 2.5);
  QCOMPARE(scale->dataRange().upper, 3.5);
}

void TestColorMap::QCPColorMapData_fillSetsExactValue()
{
  QCPColorMapData data(2, 2, QCPRange(0, 1), QCPRange(0, 1));
  data.fill(3.5);
  QCOMPARE(data.cell(0, 0), 3.5);
  QCOMPARE(data.cell(1, 0), 3.5);
  QCOMPARE(data.cell(0, 1), 3.5);
  QCOMPARE(data.cell(1, 1), 3.5);
}

void TestColorMap::QCPColorMapData_constructorZeroInitializes()
{
  QCPColorMapData data(3, 2, QCPRange(0, 1), QCPRange(0, 1));
  for (int key = 0; key < 3; ++key)
    for (int value = 0; value < 2; ++value)
      QCOMPARE(data.cell(key, value), 0.0);
}

void TestColorMap::QCPColorMapData_fillIsSafeOnEmptyMap()
{
  // clear()/setSize(0,0) is the one reachable path where mData is null;
  // this can't force the bad_alloc branch (unsafe/nondeterministic to
  // simulate a real allocation failure), but it exercises the same
  // null-guard fill() now needs.
  QCPColorMapData data(2, 2, QCPRange(0, 1), QCPRange(0, 1));
  data.clear();
  QVERIFY(data.isEmpty());
  data.fill(3.5);
  QVERIFY(data.isEmpty());
}

void TestColorMap::QCPColorMapData_cellToCoordHandlesSingleCellDimension()
{
  // A 1-cell key dimension is reachable (e.g. SciQLopHistogram2D with
  // x_bins=1): keyIndex/double(mKeySize-1) divides by zero. The value
  // dimension here has the normal size>1 so this also proves the fix
  // doesn't disturb the ordinary calculation.
  QCPColorMapData data(1, 3, QCPRange(0, 10), QCPRange(0, 1));
  double key = -1, value = -1;
  data.cellToCoord(0, 1, &key, &value);
  QVERIFY2(std::isfinite(key), "cellToCoord must not return NaN for a size-1 key dimension");
  QCOMPARE(key, 5.0);
  QCOMPARE(value, 0.5);
}

void TestColorMap::QCPColorMap2_selectTestHitSetsDetails()
{
  mPlot->resize(400, 300);
  mPlot->xAxis->setRange(0, 4);
  mPlot->yAxis->setRange(0, 4);

  auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  std::vector<double> x = {0, 1, 2, 3, 4};
  std::vector<double> y = {0, 1, 2, 3, 4};
  std::vector<double> z(25, 1.0);
  cm->setData(x, y, z);
  mPlot->replot();

  const double px = mPlot->xAxis->coordToPixel(2.0);
  const double py = mPlot->yAxis->coordToPixel(2.0);

  QVariant details;
  const double result = cm->selectTest(QPointF(px, py), true, &details);

  QVERIFY(result > 0);
  QCOMPARE(result, mPlot->selectionTolerance() * 0.99);
  QVERIFY(details.canConvert<QCPDataSelection>());
  QCOMPARE(details.value<QCPDataSelection>(), QCPDataSelection(QCPDataRange(0, 1)));
}

void TestColorMap::QCPColorMap2_selectTestMissReturnsNegativeOne()
{
  mPlot->resize(400, 300);
  mPlot->xAxis->setRange(0, 4);
  mPlot->yAxis->setRange(0, 4);

  auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  std::vector<double> x = {0, 1, 2, 3, 4};
  std::vector<double> y = {0, 1, 2, 3, 4};
  std::vector<double> z(25, 1.0);
  cm->setData(x, y, z);
  mPlot->replot();

  const double px = mPlot->xAxis->coordToPixel(20.0);
  const double py = mPlot->yAxis->coordToPixel(20.0);

  QVariant details;
  const double result = cm->selectTest(QPointF(px, py), true, &details);

  QCOMPARE(result, -1.0);
}

void TestColorMap::QCPColorMap2_contourSettersScheduleReplot()
{
  // One shared colormap for all three setters below: each spins up a real
  // background resample job (setData -> onDataChanged submits it regardless
  // of any synchronous bake), so settling once here -- instead of once per
  // setter in three separate fixtures -- keeps this test from dumping extra
  // worker-thread/RHI cold-start load on whatever test class runs next.
  mPlot->resize(400, 300);
  mPlot->xAxis->setRange(0, 4);
  mPlot->yAxis->setRange(0, 4);

  auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  std::vector<double> x = {0, 1, 2, 3, 4};
  std::vector<double> y = {0, 1, 2, 3, 4};
  std::vector<double> z(25, 1.0);
  cm->setData(x, y, z);
  cm->setAutoContourLevels(3);
  mPlot->replot();
  QTRY_VERIFY_WITH_TIMEOUT(!cm->pipeline().isBusy(), 2000);
  QTest::qWait(50); // flush any trailing queued replot from the pipeline settling

  {
    QSignalSpy spy(mPlot, &QCustomPlot::afterReplot);
    cm->setContourPen(QPen(Qt::red, 2));
    QVERIFY2(spy.wait(200), "setContourPen should schedule a queued replot on its own");
  }
  {
    QSignalSpy spy(mPlot, &QCustomPlot::afterReplot);
    cm->setContourLevels({0.2, 0.5, 0.8});
    QVERIFY2(spy.wait(200), "setContourLevels should schedule a queued replot on its own");
  }
  {
    QSignalSpy spy(mPlot, &QCustomPlot::afterReplot);
    cm->setAutoContourLevels(5);
    QVERIFY2(spy.wait(200), "setAutoContourLevels should schedule a queued replot on its own");
  }
}

void TestColorMap::QCPColorMapRhiLayer_setImageSkipsRedundantUpload()
{
  // draw() calls setImage() on every replot, even when the resample result
  // hasn't changed (e.g. interactive zoom while the pipeline is still
  // computing). Re-uploading an unchanged staging image wastes a full
  // texture upload (up to ~4x the axis rect per dimension) every such call.
  mPlot->show();
  if (!QTest::qWaitForWindowExposed(mPlot))
    QSKIP("window not exposed in this environment");
  QCoreApplication::processEvents();
  QRhi* rhi = mPlot->rhi();
  if (!rhi)
    QSKIP("no QRhi available in this environment");

  auto* ubo = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 32);
  QVERIFY(ubo->create());

  QCPColormapRhiLayer layer(rhi);
  QImage img(64, 64, QImage::Format_RGBA8888);
  img.fill(Qt::red);

  layer.setImage(img);
  QVERIFY(layer.textureUploadPending());

  auto* batch = rhi->nextResourceUpdateBatch();
  layer.uploadResources(batch, QSize(400, 300), 1.0f, rhi->isYUpInNDC(), ubo);
  batch->release();
  QVERIFY(!layer.textureUploadPending());

  // Re-submitting the exact same (unchanged) staging image must not
  // schedule another upload.
  layer.setImage(img);
  QVERIFY(!layer.textureUploadPending());

  // A genuinely new image must still be uploaded.
  QImage img2(64, 64, QImage::Format_RGBA8888);
  img2.fill(Qt::blue);
  layer.setImage(img2);
  QVERIFY(layer.textureUploadPending());

  delete ubo;
}

void TestColorMap::QCPColorMap2_hidesStaleQuadWhenPannedPastData()
{
  // Regression: once panning moves the axes far enough that the last
  // resampled image no longer overlaps them at all, and no fresher result
  // has arrived yet to replace it (e.g. a slow out-of-process data source
  // still fetching the newly-panned-into range), draw() must not leave the
  // previous GPU quad on screen. It should hide it -- not freeze it in
  // place while the axes keep moving underneath it.
  mPlot->show();
  if (!QTest::qWaitForWindowExposed(mPlot))
    QSKIP("window not exposed in this environment");
  QCoreApplication::processEvents();
  if (!mPlot->rhi())
    QSKIP("no QRhi available in this environment");

  mPlot->resize(400, 300);
  mPlot->xAxis->setRange(0, 4);
  mPlot->yAxis->setRange(0, 4);

  auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  std::vector<double> x = {0, 1, 2, 3, 4};
  std::vector<double> y = {0, 1, 2, 3, 4};
  std::vector<double> z(25, 1.0);
  cm->setData(x, y, z);
  mPlot->replot();
  QTRY_VERIFY_WITH_TIMEOUT(!cm->pipeline().isBusy(), 2000);
  QTest::qWait(50); // flush the trailing queued replot from the pipeline settling
  mPlot->replot();
  QCoreApplication::processEvents();

  QVERIFY2(cm->rhiLayer() && cm->rhiLayer()->hasContent(),
           "sanity: a real resample should have produced GPU quad content");

  // Pan to a same-size range (pure pan, not a zoom) sharing no overlap with
  // the data at all, then replot immediately -- before any fresh (empty,
  // out-of-data) resample has a chance to land. This is the window a slow
  // remote data source leaves stale content sitting in during production.
  mPlot->xAxis->setRange(1000, 1004);
  mPlot->replot();

  QVERIFY2(!cm->rhiLayer()->hasContent(),
           "stale, non-overlapping GPU quad content must be hidden, not left frozen on screen");
}

void TestColorMap::QCPContourExtractor_extractUvKeepsSourceRegistration()
{
  // A grid wider than maxDim is point-sampled with stride sk; the emitted
  // UVs must still reference the SOURCE indices (srcK/(kSize-1)) and the
  // tail strip past the last stride multiple must stay covered.
  constexpr int kSize = 900; // sk=3, last stride multiple 897, tail 897..899
  constexpr int vSize = 4;
  QCPColorMapData data(kSize, vSize, QCPRange(0, kSize - 1), QCPRange(0, vSize - 1));
  for (int ki = 0; ki < kSize; ++ki)
    for (int vi = 0; vi < vSize; ++vi)
      data.setCell(ki, vi, double(ki));
  data.recalculateDataBounds();

  // Level mid-grid: registered to source index 450 -> u = 450/899.
  // The old code emitted reduced-index UVs (150/299), off by ~1.1e-3.
  {
    const QVector<float> uv = QCPContourExtractor::extractUv(&data, {450.0});
    QVERIFY2(!uv.isEmpty(), "mid-grid level must produce segments");
    QVERIFY(uv.size() % 4 == 0);
    constexpr double expected = 450.0 / 899.0;
    for (qsizetype i = 0; i < uv.size(); i += 2)
      QVERIFY2(qAbs(double(uv[i]) - expected) < 3e-4,
               qPrintable(QString("u=%1, expected %2").arg(double(uv[i])).arg(expected)));
  }

  // Level inside the tail strip (past source index 897): the old code never
  // sampled there, so it produced no segments at all.
  {
    const QVector<float> uv = QCPContourExtractor::extractUv(&data, {898.5});
    QVERIFY2(!uv.isEmpty(), "tail-strip level must produce segments");
    double maxU = 0;
    for (qsizetype i = 0; i < uv.size(); i += 2)
    {
      QVERIFY2(uv[i] >= 0.0f && uv[i] <= 1.0f, "u must stay in [0,1]");
      maxU = qMax(maxU, double(uv[i]));
    }
    QVERIFY2(maxU > 0.999, qPrintable(QString("maxU=%1, tail must reach u=1").arg(maxU)));
  }
}

void TestColorMap::QCPColorMapData_recalculateDataBoundsSkipsInfinite()
{
  QCPColorMapData data(2, 2, QCPRange(0, 1), QCPRange(0, 1));
  data.setCell(0, 0, 1.0);
  data.setCell(1, 0, 2.0);
  data.setCell(0, 1, 3.0);
  data.setCell(1, 1, 4.0);
  // Infinities are stored but must not poison the cached bounds (which feed
  // auto contour levels and the color mapping).
  data.setCell(1, 1, std::numeric_limits<double>::infinity());
  QCOMPARE(data.cell(1, 1), std::numeric_limits<double>::infinity());
  data.recalculateDataBounds();
  QCOMPARE(data.dataBounds().lower, 1.0);
  QCOMPARE(data.dataBounds().upper, 3.0);
}

namespace
{
// Settles the colormap pipeline like the existing tests do, then forces one
// more draw so updateContourGpu has run against the delivered result.
void settleColorMap(QCustomPlot* plot, QCPColorMap2* cm)
{
  plot->replot();
  QTRY_VERIFY_WITH_TIMEOUT(!cm->pipeline().isBusy(), 2000);
  QTest::qWait(50); // flush the trailing queued replot from the pipeline settling
  plot->replot();
}

int countRedPixels(const QImage& img)
{
  int red = 0;
  for (int py = 0; py < img.height(); ++py)
  {
    const QRgb* row = reinterpret_cast<const QRgb*>(img.constScanLine(py));
    for (int px = 0; px < img.width(); ++px)
      if (qRed(row[px]) > 200 && qGreen(row[px]) < 100 && qBlue(row[px]) < 100)
        ++red;
  }
  return red;
}
} // namespace

void TestColorMap::QCPColorMap2_contourMirrorsReversedAxis()
{
  // z = x on a 5x5 grid, level at 1.0 -> vertical line at u = 0.25.
  // Reversing the key axis flips the image; the contour UVs must mirror
  // (u -> 1-u) instead of staying reflected against it.
  mPlot->resize(400, 300);
  mPlot->xAxis->setRange(0, 4);
  mPlot->yAxis->setRange(0, 4);
  std::vector<double> x = {0, 1, 2, 3, 4}, y = {0, 1, 2, 3, 4}, z(25, 0.0);
  // NOTE: source Z layout is x-outer: z[xIndex * ySize + yIndex].
  for (int ki = 0; ki < 5; ++ki)
    for (int vi = 0; vi < 5; ++vi)
      z[ki * 5 + vi] = double(ki); // z = x -> vertical contours

  auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  cm->setData(x, y, z);
  settleColorMap(mPlot, cm);
  cm->setContourLevels({1.0});
  mPlot->replot();
  QTRY_VERIFY_WITH_TIMEOUT(!cm->pipeline().isBusy(), 2000);
  QTest::qWait(50);
  mPlot->replot();

  const QVector<float> plain = cm->lastContourUv();
  QVERIFY2(!plain.isEmpty(), "sanity: level must produce contour vertices");

  // setRangeReversed() emits no signal: this replot reaches draw() with a
  // valid cache, so only the reversal polling can trigger the rebuild.
  // The resampled field from sparse points is a staircase (exact contour
  // positions are resampler-dependent), so assert the mirror relationship
  // itself: every u maps to 1-u, every v is unchanged.
  mPlot->xAxis->setRangeReversed(true);
  mPlot->replot();

  const QVector<float> mirrored = cm->lastContourUv();
  QCOMPARE(mirrored.size(), plain.size());
  for (qsizetype i = 0; i < mirrored.size(); i += 2)
  {
    QVERIFY2(qAbs(double(mirrored[i]) - (1.0 - double(plain[i]))) < 1e-6,
             qPrintable(QString("u=%1, expected %2 after reversal")
                            .arg(double(mirrored[i]))
                            .arg(1.0 - double(plain[i]))));
    QVERIFY2(qAbs(double(mirrored[i + 1]) - double(plain[i + 1])) < 1e-6,
             "v must be unchanged by a key-axis reversal");
  }
}

void TestColorMap::QCPColorMap2_contourExportDrawsFallback()
{
  // toPixmap() paints with pmNoCaching, where the RHI contour layer never
  // receives the lines. The QPainter fallback must draw them instead --
  // previously the export silently dropped every contour.
  mPlot->resize(400, 300);
  mPlot->xAxis->setRange(0, 4);
  mPlot->yAxis->setRange(0, 4);
  std::vector<double> x = {0, 1, 2, 3, 4}, y = {0, 1, 2, 3, 4}, z(25, 0.0);
  // NOTE: source Z layout is x-outer: z[xIndex * ySize + yIndex].
  for (int ki = 0; ki < 5; ++ki)
    for (int vi = 0; vi < 5; ++vi)
      z[ki * 5 + vi] = double(ki); // z = x -> vertical contours

  auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  cm->setData(x, y, z);
  settleColorMap(mPlot, cm);
  cm->setContourLevels({1.0, 2.0, 3.0});
  cm->setContourPen(QPen(Qt::red, 2));
  mPlot->replot();
  QTRY_VERIFY_WITH_TIMEOUT(!cm->pipeline().isBusy(), 2000);
  QTest::qWait(50);

  const QPixmap pm = mPlot->toPixmap();
  QVERIFY(!pm.isNull());
  const int red = countRedPixels(pm.toImage());
  QVERIFY2(red > 50, qPrintable(QString("export must draw contour lines, red pixels=%1").arg(red)));
}

void TestColorMap::QCPColorMap2_contourDegenerateDataClearsLines()
{
  mPlot->resize(400, 300);
  mPlot->xAxis->setRange(0, 4);
  mPlot->yAxis->setRange(0, 4);
  std::vector<double> x = {0, 1, 2, 3, 4}, y = {0, 1, 2, 3, 4}, z(25, 0.0);
  // NOTE: source Z layout is x-outer: z[xIndex * ySize + yIndex].
  for (int ki = 0; ki < 5; ++ki)
    for (int vi = 0; vi < 5; ++vi)
      z[ki * 5 + vi] = double(ki); // z = x -> vertical contours

  auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  cm->setData(x, y, z);
  settleColorMap(mPlot, cm);
  cm->setContourLevels({1.0, 2.0, 3.0});
  mPlot->replot();
  QTRY_VERIFY_WITH_TIMEOUT(!cm->pipeline().isBusy(), 2000);
  QTest::qWait(50);
  mPlot->replot();
  QVERIFY2(!cm->lastContourUv().isEmpty(), "sanity: levels must produce contour vertices");

  // Constant data has degenerate bounds (lower == upper): the rebuild must
  // clear the previously built lines instead of leaving them stale.
  cm->setData(x, y, std::vector<double>(25, 1.0));
  mPlot->replot();
  QTRY_VERIFY_WITH_TIMEOUT(!cm->pipeline().isBusy(), 2000);
  QTest::qWait(50);
  mPlot->replot();
  QVERIFY2(cm->lastContourUv().isEmpty(), "degenerate data must clear stale contour lines");
  QVERIFY2(cm->lastContourLevels().isEmpty(), "degenerate data must clear resolved levels");
}

void TestColorMap::QCPColorMap2_autoContourLevelsGeometricInLogScale()
{
  // z spans three decades (1..1000). Linear auto levels would bunch every
  // line at the top decade; under a log Z scale they must space geometrically.
  mPlot->resize(400, 300);
  mPlot->xAxis->setRange(0, 4);
  mPlot->yAxis->setRange(0, 4);
  std::vector<double> x = {0, 1, 2, 3, 4}, y = {0, 1, 2, 3, 4}, z(25, 0.0);
  for (int vi = 0; vi < 5; ++vi)
    for (int ki = 0; ki < 5; ++ki)
      z[ki * 5 + vi] = std::pow(10.0, 3.0 * (ki * 5 + vi) / 24.0);

  auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  cm->setData(x, y, z);
  settleColorMap(mPlot, cm);
  cm->setDataScaleType(QCPAxis::stLogarithmic);
  cm->setAutoContourLevels(3);
  mPlot->replot();
  QTRY_VERIFY_WITH_TIMEOUT(!cm->pipeline().isBusy(), 2000);
  QTest::qWait(50);
  mPlot->replot();

  const QVector<double> levels = cm->lastContourLevels();
  QVERIFY2(levels.size() == 3,
           qPrintable(QString("expected 3 auto levels, got %1").arg(levels.size())));
  // Equal ratios, not equal differences: bounds are ~[1,1000].
  const double r0 = levels[1] / levels[0], r1 = levels[2] / levels[1];
  QVERIFY2(qAbs(r0 - r1) / r0 < 0.02,
           qPrintable(QString("levels must be geometric: %1 %2 %3")
                          .arg(levels[0])
                          .arg(levels[1])
                          .arg(levels[2])));
  QVERIFY2(levels[0] > 2.0 && levels[0] < 12.0,
           qPrintable(QString("first level=%1, expected ~5.6").arg(levels[0])));
}

void TestColorMap::QCPColorMap2_contourExportAfterRhiDraw()
{
  // An on-screen RHI draw uploads contour lines to the GPU layer only. A
  // later export (pmNoCaching bypasses RHI) must still draw them via the
  // QPainter fallback -- previously they vanished because the fallback
  // stayed empty while the GPU flag claimed everything was fine.
  mPlot->show();
  if (!QTest::qWaitForWindowExposed(mPlot))
    QSKIP("window not exposed in this environment");
  QCoreApplication::processEvents();
  if (!mPlot->rhi())
    QSKIP("no QRhi available in this environment");

  mPlot->resize(400, 300);
  mPlot->xAxis->setRange(0, 4);
  mPlot->yAxis->setRange(0, 4);
  std::vector<double> x = {0, 1, 2, 3, 4}, y = {0, 1, 2, 3, 4}, z(25, 0.0);
  // NOTE: source Z layout is x-outer: z[xIndex * ySize + yIndex].
  for (int ki = 0; ki < 5; ++ki)
    for (int vi = 0; vi < 5; ++vi)
      z[ki * 5 + vi] = double(ki); // z = x -> vertical contours

  auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  cm->setData(x, y, z);
  cm->setContourLevels({1.0, 2.0, 3.0});
  cm->setContourPen(QPen(Qt::red, 2));
  settleColorMap(mPlot, cm);
  QCoreApplication::processEvents();
  QVERIFY2(cm->rhiLayer() && cm->rhiLayer()->hasContent(),
           "sanity: on-screen RHI draw must have produced GPU content");

  const QPixmap pm = mPlot->toPixmap();
  QVERIFY(!pm.isNull());
  const int red = countRedPixels(pm.toImage());
  QVERIFY2(red > 50,
           qPrintable(QString("export after RHI draw must draw contour lines, red pixels=%1").arg(red)));
}

void TestColorMap::QCPColorMap2_contourExportCachesFallback()
{
  // Repeated exports of one frame must reuse the cached fallback segments,
  // not re-march the full grid on every paint pass.
  mPlot->resize(400, 300);
  mPlot->xAxis->setRange(0, 4);
  mPlot->yAxis->setRange(0, 4);
  std::vector<double> x = {0, 1, 2, 3, 4}, y = {0, 1, 2, 3, 4}, z(25, 0.0);
  // NOTE: source Z layout is x-outer: z[xIndex * ySize + yIndex].
  for (int ki = 0; ki < 5; ++ki)
    for (int vi = 0; vi < 5; ++vi)
      z[ki * 5 + vi] = double(ki); // z = x -> vertical contours

  auto* cm = new QCPColorMap2(mPlot->xAxis, mPlot->yAxis);
  cm->setData(x, y, z);
  cm->setContourLevels({1.0, 2.0, 3.0});
  cm->setContourPen(QPen(Qt::red, 2));
  // Settle to true quiescence: setData/replot can leave several chained
  // pipeline jobs (each finished() bumps the contour data generation), so
  // repeat until a full settle round neither builds nor leaves work behind.
  // Otherwise a trailing job could land mid-export and legitimately rebuild.
  for (int i = 0; i < 20; ++i)
  {
    const uint64_t roundBefore = cm->fallbackBuildCount();
    settleColorMap(mPlot, cm);
    QTRY_VERIFY_WITH_TIMEOUT(!cm->pipeline().isBusy(), 2000);
    if (cm->fallbackBuildCount() == roundBefore)
      break;
  }
  QTRY_VERIFY_WITH_TIMEOUT(!cm->pipeline().isBusy(), 2000);

  const uint64_t buildsBefore = cm->fallbackBuildCount();
  QVERIFY2(buildsBefore >= 1, "sanity: settling must have built the fallback once");
  const QImage img1 = mPlot->toPixmap().toImage();
  const QImage img2 = mPlot->toPixmap().toImage();
  QCOMPARE(cm->fallbackBuildCount(), buildsBefore);
  QVERIFY2(countRedPixels(img1) > 50, "export must draw contour lines");
  QVERIFY(img1 == img2);
}

void TestColorMap::QCPColorMapData_fillNonFiniteKeepsBoundsDegenerate()
{
  QCPColorMapData data(2, 2, QCPRange(0, 1), QCPRange(0, 1));
  data.fill(std::numeric_limits<double>::infinity());
  QCOMPARE(data.cell(0, 0), std::numeric_limits<double>::infinity());
  QCOMPARE(data.dataBounds().lower, 0.0);
  QCOMPARE(data.dataBounds().upper, 0.0);
  data.fill(std::numeric_limits<double>::quiet_NaN());
  QCOMPARE(data.dataBounds().lower, 0.0);
  QCOMPARE(data.dataBounds().upper, 0.0);
  data.fill(2.5);
  QCOMPARE(data.dataBounds().lower, 2.5);
  QCOMPARE(data.dataBounds().upper, 2.5);
}

void TestColorMap::cleanup()
{
  delete mPlot;
}
