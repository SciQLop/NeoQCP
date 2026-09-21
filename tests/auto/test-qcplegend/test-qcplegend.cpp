#include "test-qcplegend.h"

void TestQCPLegend::init()
{
  mPlot = new QCustomPlot(0);
  mPlot->legend->setVisible(true);
  mPlot->show();
  //QTest::qWait(150);
}


void TestQCPLegend::cleanup()
{
  delete mPlot;
}

void TestQCPLegend::autoAddPlottables()
{
  // legend items shouldn't be added if auto-add is off:
  QCOMPARE(mPlot->legend->itemCount(), 0);
  mPlot->setAutoAddPlottableToLegend(false);
  mPlot->addGraph();
  QCOMPARE(mPlot->legend->itemCount(), 0);
  new QCPBars(mPlot->xAxis, mPlot->yAxis);
  QCOMPARE(mPlot->legend->itemCount(), 0);
  
  // legend items should be added if auto-add is on:
  mPlot->setAutoAddPlottableToLegend(true);
  mPlot->addGraph();
  QCOMPARE(mPlot->legend->itemCount(), 1);
  mPlot->addGraph();
  QCOMPARE(mPlot->legend->itemCount(), 2);
  new QCPBars(mPlot->xAxis, mPlot->yAxis);
  QCOMPARE(mPlot->legend->itemCount(), 3);
}

void TestQCPLegend::addAndRemove()
{
  mPlot->setAutoAddPlottableToLegend(false);
  
  QCPGraph *g1 = new QCPGraph(mPlot->xAxis, mPlot->yAxis);
  QCPGraph *g2 = new QCPGraph(mPlot->xAxis, mPlot->yAxis);
  QCPBars *b1 = new QCPBars(mPlot->xAxis, mPlot->yAxis);
  QCOMPARE(mPlot->legend->itemCount(), 0);
  
  g1->addToLegend();
  QCOMPARE(mPlot->legend->itemCount(), 1);
  mPlot->legend->addItem(new QCPPlottableLegendItem(mPlot->legend, g2)); // explicit way of adding to legend
  QCOMPARE(mPlot->legend->itemCount(), 2);
  b1->addToLegend();
  QCOMPARE(mPlot->legend->itemCount(), 3);
  
  g1->removeFromLegend();
  QCOMPARE(mPlot->legend->itemCount(), 2);
  
  mPlot->legend->clearItems();
  QCOMPARE(mPlot->legend->itemCount(), 0);
}





void TestQCPLegend::itemTextColorRepaintsTheLegend()
{
  // Dimming an entry (a hidden graph) only changes its text colour. The legend lives on a
  // layer of its own, so the setter must dirty it or the entry keeps its old colour until
  // something else repaints the legend.
  auto *graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
  graph->setName("dimmed");
  graph->addToLegend();
  mPlot->replot(QCustomPlot::rpImmediateRefresh);

  const auto buffer = mPlot->layer("legend")->mPaintBuffer.toStrongRef();
  QVERIFY(buffer);
  QVERIFY(!buffer->contentDirty());

  QCPPlottableLegendItem *item = mPlot->legend->itemWithPlottable(graph);
  QVERIFY(item);
  item->setTextColor(Qt::red);
  QVERIFY2(buffer->contentDirty(), "a new text colour must repaint the legend");

  mPlot->replot(QCustomPlot::rpImmediateRefresh);
  QVERIFY(!buffer->contentDirty());
  item->setTextColor(Qt::red);
  QVERIFY2(!buffer->contentDirty(), "an unchanged colour must not repaint it");
}

void TestQCPLegend::groupRowFollowsComponentVisibility()
{
  auto *mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
  mg->setData(std::vector<double>{1.0, 2.0, 3.0},
              std::vector<std::vector<double>>{{10.0, 20.0, 30.0}, {-1.0, -2.0, -3.0}});
  mg->addToLegend();
  mPlot->replot(QCustomPlot::rpImmediateRefresh);

  const auto buffer = mPlot->layer("legend")->mPaintBuffer.toStrongRef();
  QVERIFY(buffer);
  QVERIFY(!buffer->contentDirty());

  mg->setComponentVisible(0, false);
  QVERIFY2(buffer->contentDirty(), "the group row draws one segment per visible component");
}
