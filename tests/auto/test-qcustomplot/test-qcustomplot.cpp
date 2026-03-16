#include "test-qcustomplot.h"

void TestQCustomPlot::init()
{
  mPlot = new QCustomPlot(0);
  mPlot->show();
  //QTest::qWait(150);
}

void TestQCustomPlot::cleanup()
{
  delete mPlot;
}

void TestQCustomPlot::interface_plottables()
{
  mPlot->setGeometry(50, 50, 500, 500);
  mPlot->axisRect()->setAutoMargins(QCP::msNone);
  mPlot->axisRect()->setMargins(QMargins(0, 0, 0, 0));
  // set axes so plot coordinates equate pixel coordinates:
  mPlot->xAxis->setRange(mPlot->axisRect()->left(), mPlot->axisRect()->left()+mPlot->axisRect()->width());
  mPlot->yAxis->setRange(mPlot->axisRect()->top(), mPlot->axisRect()->top()+mPlot->axisRect()->height());
  mPlot->yAxis->setRangeReversed(true);
  
  QCPGraph *g1 = new QCPGraph(mPlot->xAxis, mPlot->yAxis);
  QCPGraph *g2 = new QCPGraph(mPlot->xAxis, mPlot->yAxis);
  g1->setData(QVector<double>() << 200 << 300, QVector<double>() << 100 << 120);
  g2->setData(QVector<double>() << 200 << 300, QVector<double>() << 106 << 126);

  QCPBars *b1 = new QCPBars(mPlot->xAxis, mPlot->yAxis);
  b1->setWidth(50);
  b1->setData(QVector<double>() << 250, QVector<double>() << 150);

  // test selectivity of template parameter:
  QCOMPARE(mPlot->plottableAt<QCPGraph>(QPointF(250, 110), false, nullptr), g1);
  QCOMPARE(mPlot->plottableAt<QCPGraph>(QPointF(250, 113), false, nullptr), g2);
  QCOMPARE(mPlot->plottableAt<QCPGraph>(QPointF(250, 130), false, nullptr), static_cast<QCPGraph*>(nullptr));

  QCOMPARE(mPlot->plottableAt<QCPBars>(QPointF(250, 110), false, nullptr), b1);
  QCOMPARE(mPlot->plottableAt<QCPBars>(QPointF(250, 170), false, nullptr), static_cast<QCPBars*>(nullptr));

  // test returned datapoint:
  int index = -1;
  QCOMPARE(mPlot->plottableAt<QCPGraph>(QPointF(180, 100), false, &index), static_cast<QCPGraph*>(nullptr));
  QCOMPARE(index, -1);
  QCOMPARE(mPlot->plottableAt<QCPGraph>(QPointF(200, 100), false, &index), g1);
  QCOMPARE(index, 0);
  QCOMPARE(mPlot->plottableAt<QCPGraph>(QPointF(249, 110), false, &index), g1);
  QCOMPARE(index, 0);
  QCOMPARE(mPlot->plottableAt<QCPGraph>(QPointF(251, 110), false, &index), g1);
  QCOMPARE(index, 1);
  QCOMPARE(mPlot->plottableAt<QCPGraph>(QPointF(300, 120), false, &index), g1);
  QCOMPARE(index, 1);

  /*
  mPlot->replot(QCustomPlot::rpImmediateRefresh);
  QApplication::processEvents();
  QTest::qWait(2000);
  */
}

void TestQCustomPlot::interface_items()
{
  mPlot->setGeometry(50, 50, 500, 500);
  mPlot->axisRect()->setAutoMargins(QCP::msNone);
  mPlot->axisRect()->setMargins(QMargins(0, 0, 0, 0));
  // set axes so plot coordinates equate pixel coordinates:
  mPlot->xAxis->setRange(mPlot->axisRect()->left(), mPlot->axisRect()->left()+mPlot->axisRect()->width());
  mPlot->yAxis->setRange(mPlot->axisRect()->top(), mPlot->axisRect()->top()+mPlot->axisRect()->height());
  mPlot->yAxis->setRangeReversed(true);
  
  QCPItemLine *l1 = new QCPItemLine(mPlot);
  l1->start->setCoords(200, 100);
  l1->end->setCoords(300, 120);
  QCPItemLine *l2 = new QCPItemLine(mPlot);
  l2->start->setCoords(200, 106);
  l2->end->setCoords(300, 126);

  QCPItemTracer *t1 = new QCPItemTracer(mPlot);
  t1->position->setCoords(250, 110);
  t1->setStyle(QCPItemTracer::tsPlus);
  t1->setSize(10);

  // test selectivity of template parameter:
  QCOMPARE(mPlot->itemAt<QCPItemLine>(QPointF(250, 110), false), l1);
  QCOMPARE(mPlot->itemAt<QCPItemLine>(QPointF(250, 113), false), l2);
  QCOMPARE(mPlot->itemAt<QCPItemLine>(QPointF(250, 130), false), static_cast<QCPItemLine*>(nullptr));
  
  QCOMPARE(mPlot->itemAt<QCPItemTracer>(QPointF(250, 110), false), t1);
  QCOMPARE(mPlot->itemAt<QCPItemTracer>(QPointF(250, 130), false), static_cast<QCPItemTracer*>(nullptr));

  /*
  mPlot->replot(QCustomPlot::rpImmediateRefresh);
  QApplication::processEvents();
  QTest::qWait(2000);
  */
}

void TestQCustomPlot::rescaleAxes_GraphVisibility()
{
  mPlot->setGeometry(50, 50, 500, 500);
  
  mPlot->addGraph();
  mPlot->addGraph();
  mPlot->graph(0)->setData(QVector<double>()<<4<<5<<6, QVector<double>()<<-2<<-1<<0);
  mPlot->graph(1)->setData(QVector<double>()<<3<<5<<7, QVector<double>()<<-3<<-1<<1);
  mPlot->graph(1)->setVisible(false);
  
  // test rescaling with respect to all graphs, even invisible ones:
  mPlot->rescaleAxes(false);
  QCOMPARE(mPlot->xAxis->range().lower, 3.0);
  QCOMPARE(mPlot->xAxis->range().upper, 7.0);
  QCOMPARE(mPlot->yAxis->range().lower, -3.0);
  QCOMPARE(mPlot->yAxis->range().upper, 1.0);
  
  // test rescaling with respect to all visible graphs:
  mPlot->rescaleAxes(true);
  QCOMPARE(mPlot->xAxis->range().lower, 4.0);
  QCOMPARE(mPlot->xAxis->range().upper, 6.0);
  QCOMPARE(mPlot->yAxis->range().lower, -2.0);
  QCOMPARE(mPlot->yAxis->range().upper, 0.0);
}

void TestQCustomPlot::rescaleAxes_FlatGraph()
{
  // test rescaling if only one graph is in plot and it's flat:
  mPlot->xAxis->setRange(10, 11);
  mPlot->yAxis->setRange(10, 11);
  mPlot->addGraph();
  mPlot->graph(0)->setData(QVector<double>()<<2<<2<<2, QVector<double>()<<0<<0<<0);
  mPlot->rescaleAxes();
  QCOMPARE(mPlot->xAxis->range().lower, 1.5);
  QCOMPARE(mPlot->xAxis->range().upper, 2.5);
  QCOMPARE(mPlot->yAxis->range().lower, -0.5);
  QCOMPARE(mPlot->yAxis->range().upper, 0.5);
  
  // test rescaling when there are multiple graphs with different values but first graph is flat:
  mPlot->xAxis->setRange(10, 11);
  mPlot->yAxis->setRange(10, 11);
  mPlot->addGraph();
  mPlot->graph(1)->setData(QVector<double>()<<-1<<0<<1, QVector<double>()<<-0.1<<0<<0.1);
  mPlot->rescaleAxes();
  QCOMPARE(mPlot->xAxis->range().lower, -1.0);
  QCOMPARE(mPlot->xAxis->range().upper, 2.0);
  QCOMPARE(mPlot->yAxis->range().lower, -0.1);
  QCOMPARE(mPlot->yAxis->range().upper, 0.1);
}

void TestQCustomPlot::rescaleAxes_MultipleFlatGraphs()
{
  // test rescaling when there are multiple flat graphs:
  mPlot->addGraph();
  mPlot->addGraph();
  mPlot->graph(0)->setData(QVector<double>()<<1<<2<<3, QVector<double>()<<0<<0<<0);
  mPlot->graph(1)->setData(QVector<double>()<<-1<<0<<1, QVector<double>()<<2<<2<<2);

  mPlot->rescaleAxes();

  QCOMPARE(mPlot->xAxis->range().lower, -1.0);
  QCOMPARE(mPlot->xAxis->range().upper, 3.0);
  QCOMPARE(mPlot->yAxis->range().lower, 0.0);
  QCOMPARE(mPlot->yAxis->range().upper, 2.0);
}

void TestQCustomPlot::calculateMargin_staleTickVectors()
{
  // Bug: calculateMargin() and draw() iterate mTickVector and access
  // mTickVectorLabels[i] without bounds checking. The vectors can desync
  // when setTickLabels(false) clears labels, a replot regenerates ticks
  // (but skips labels since mTickLabels is false), then setTickLabels(true)
  // re-enables label access without regenerating them.
  // Without the bounds check, this crashes with QVector::at() out of bounds.
  mPlot->setGeometry(50, 50, 500, 500);
  mPlot->yAxis->setRange(1, 100);

  // Replot populates mTickVector and mTickVectorLabels in sync
  mPlot->replot(QCustomPlot::rpImmediateRefresh);

  // Disable tick labels — this clears mTickVectorLabels (axis.cpp line 852)
  mPlot->yAxis->setTickLabels(false);

  // Replot regenerates mTickVector via setupTickVectors(), but does NOT
  // regenerate mTickVectorLabels (nullptr passed to generate() since
  // mTickLabels is false). Now mTickVector has entries, mTickVectorLabels is empty.
  mPlot->replot(QCustomPlot::rpImmediateRefresh);

  // Re-enable tick labels WITHOUT a replot. mTickLabels is now true,
  // but mTickVectorLabels is still empty from the clear above.
  mPlot->yAxis->setTickLabels(true);

  // calculateMargin iterates mTickVector.size() and accesses mTickVectorLabels[i].
  // Without bounds check: crash. With bounds check: safe.
  for (auto side : {QCP::msLeft, QCP::msRight, QCP::msTop, QCP::msBottom})
      mPlot->axisRect()->calculateAutoMargin(side);

  // If we reach here without crashing, the bounds check works
}














