#include "test-colormap.h"
#include <QMainWindow>
#include <painting/colormap-rhi-layer.h>
#include <QtWidgets/qtestsupport_widgets.h> // QTest::qWaitForWindowExposed

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

void TestColorMap::cleanup()
{
  delete mPlot;
}
