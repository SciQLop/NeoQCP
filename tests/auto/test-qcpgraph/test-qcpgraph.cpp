#include "test-qcpgraph.h"
#include <QMainWindow>

void TestQCPGraph::init()
{
  mPlot = new QCustomPlot(0);
  mGraph = mPlot->addGraph();
}

void TestQCPGraph::cleanup()
{
  delete mPlot;
}

void TestQCPGraph::specializedGraphInterface()
{
  // for this test we don't use the graph created in init().
  mPlot->clearPlottables();
  
  // adding graphs:
  QCOMPARE(mPlot->plottableCount(), 0);
  QCOMPARE(mPlot->graphCount(), 0);
  
  QCPMultiGraph *graph1 = mPlot->addGraph();
  QCOMPARE(mPlot->plottableCount(), 1);
  QCOMPARE(mPlot->graphCount(), 1);
  
  QCPMultiGraph *graph2 = mPlot->addGraph();
  QCOMPARE(mPlot->plottableCount(), 2);
  QCOMPARE(mPlot->graphCount(), 2);
  
  QCPMultiGraph *graph3 = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
  QCOMPARE(mPlot->plottableCount(), 3);
  QCOMPARE(mPlot->graphCount(), 3);
  
  // accessing graphs:
  QCOMPARE(mPlot->graph(0), graph1);
  QCOMPARE(mPlot->graph(1), graph2);
  QCOMPARE(mPlot->graph(2), graph3);
  QCOMPARE(mPlot->plottable(0), graph1);
  QCOMPARE(mPlot->plottable(1), graph2);
  QCOMPARE(mPlot->plottable(2), graph3);

  // removing graphs:
  mPlot->removeGraph(0);
  QCOMPARE(mPlot->graphCount(), 2);
  QCOMPARE(mPlot->graph(0), graph2);
  QCOMPARE(mPlot->graph(1), graph3);
  
  mPlot->removeGraph(graph2);
  QCOMPARE(mPlot->graphCount(), 1);
  QCOMPARE(mPlot->graph(0), graph3);
  
  mPlot->removeGraph(0);
  QCOMPARE(mPlot->graphCount(), 0);
  QCOMPARE(mPlot->plottableCount(), 0);
}

void TestQCPGraph::multiColumnAndWaterfallGraphsAreListed()
{
  mPlot->clearPlottables();
  auto *multi = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
  multi->setData(std::vector<double>{0, 1}, std::vector<std::vector<double>>{{0, 1}, {1, 2}});
  auto *waterfall = new QCPWaterfallGraph(mPlot->xAxis, mPlot->yAxis);
  QCOMPARE(mPlot->graphCount(), 2);
  QCOMPARE(mPlot->graph(0).get(), multi);
  QCOMPARE(mPlot->graph(1).get(), static_cast<QCPMultiGraph*>(waterfall));
  QCOMPARE(mPlot->clearGraphs(), 2);
  QCOMPARE(mPlot->plottableCount(), 0);
}

void TestQCPGraph::selectedGraphsAndAxisGraphsListTheMultigraph()
{
  QCPMultiGraph *other = mPlot->addGraph();
  mGraph->setData(QVector<double>{0, 1}, QVector<double>{0, 1});
  mGraph->setSelection(QCPDataSelection(QCPDataRange(0, 1)));
  QCOMPARE(mPlot->selectedGraphs().size(), 1);
  QCOMPARE(mPlot->selectedGraphs().first().get(), mGraph);
  QCOMPARE(mPlot->xAxis->graphs().size(), 2);
  QCOMPARE(mPlot->axisRect()->graphs().last().get(), other);
  QVERIFY(mPlot->yAxis2->graphs().isEmpty());
}

void TestQCPGraph::dataManipulation()
{
  QCOMPARE(mGraph->dataCount(), 0);

  QVector<double> x, y;
  x << -1 << 1 << -2 << 2;
  y <<  1 << 2 <<  0 << 3;

  mGraph->setData(x, y);
  QCOMPARE(mGraph->dataCount(), 4);
  // data should be sorted by x:
  const QVector<double> sortedKeys {-2, -1, 1, 2};
  for (int i = 0; i < 4; ++i)
  {
    QCOMPARE(mGraph->dataMainKey(i), sortedKeys[i]);
    QCOMPARE(mGraph->dataMainValue(i), double(i));
  }

  // add single data points:
  mGraph->setData(QVector<double>(), QVector<double>());
  mGraph->addData(5, 6);
  QCOMPARE(mGraph->dataCount(), 1);
  mGraph->addData(4, 5);
  QCOMPARE(mGraph->dataCount(), 2);
  QCOMPARE(mGraph->dataMainValue(0), 5.0);
  QCOMPARE(mGraph->dataMainValue(1), 6.0);

  mGraph->setData(x, y);
  mGraph->addData(3, 4);
  QCOMPARE(mGraph->dataCount(), 5);
  for (int i = 0; i < 5; ++i)
    QCOMPARE(mGraph->dataMainValue(i), double(i));

  mGraph->setData(x, y);
  mGraph->addData(QVector<double>() << 3 << 4 << 5, QVector<double>() << 4 << 5 << 6);
  QCOMPARE(mGraph->dataCount(), 7);
  for (int i = 0; i < 7; ++i)
    QCOMPARE(mGraph->dataMainValue(i), double(i));
}

void TestQCPGraph::channelFill()
{
  QCPMultiGraph *otherGraph = mPlot->addGraph();

  mGraph->setBrush(Qt::black);
  QVERIFY(mGraph->setComponentFillToGraph(0, otherGraph, 0));
  mPlot->replot();

  mGraph->setData(QVector<double>()<<1<<2<<3<<4, QVector<double>()<<4<<2<<2.5<<6);
  mPlot->replot();

  otherGraph->setData(QVector<double>()<<-2<<-1.5<<-1<<-0.5, QVector<double>()<<1<<1.2<<1.3<<1.4);
  mPlot->replot();

  mGraph->setData(QVector<double>(), QVector<double>());
  mPlot->replot();

  QVERIFY(mPlot->removeGraph(otherGraph));
  // 2.x reset channelFillGraph to null: the fill has no partner any more.
  const auto fill = mGraph->componentFill(0);
  const auto *toGraph = fill ? std::get_if<QCPMultiGraph::FillToGraph>(&fill->target) : nullptr;
  QVERIFY(!toGraph || !toGraph->graph);
  mPlot->replot();
}
