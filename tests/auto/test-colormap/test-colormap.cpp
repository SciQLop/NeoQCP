#include "test-colormap.h"
#include <QMainWindow>

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

void TestColorMap::cleanup()
{
  delete mPlot;
}
