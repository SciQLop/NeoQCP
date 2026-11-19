#include "test-qcpgraph.h"
#include <QMainWindow>
#include <QPicture>
#include <cmath>

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
  
  QCPGraph *graph1 = mPlot->addGraph();
  QCOMPARE(mPlot->plottableCount(), 1);
  QCOMPARE(mPlot->graphCount(), 1);
  
  QCPGraph *graph2 = mPlot->addGraph();
  QCOMPARE(mPlot->plottableCount(), 2);
  QCOMPARE(mPlot->graphCount(), 2);
  
  QCPGraph *graph3 = mPlot->add<QCPGraph>().get();
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
  auto *multi = mPlot->add<QCPGraph>().get();
  multi->setData(std::vector<double>{0, 1}, std::vector<std::vector<double>>{{0, 1}, {1, 2}});
  auto *waterfall = mPlot->add<QCPWaterfallGraph>().get();
  QCOMPARE(mPlot->graphCount(), 2);
  QCOMPARE(mPlot->graph(0).get(), multi);
  QCOMPARE(mPlot->graph(1).get(), static_cast<QCPGraph*>(waterfall));
  QCOMPARE(mPlot->clearGraphs(), 2);
  QCOMPARE(mPlot->plottableCount(), 0);
}

void TestQCPGraph::selectedGraphsAndAxisGraphsListTheMultigraph()
{
  QCPGraph *other = mPlot->addGraph();
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

namespace
{
int gDebugMessages = 0;
void countDebug(QtMsgType type, const QMessageLogContext&, const QString&)
{
  if (type == QtDebugMsg)
    ++gDebugMessages;
}
} // namespace

// A bracket that follows the data's tangent printed the number of points it averaged, once per
// bracket per draw: a stray debug line kept from 2.x.
void TestQCPGraph::aTangentBracketPrintsNothing()
{
  mGraph->setData({ 0, 1, 2, 3, 4 }, { 0, 1, 0, 1, 0 });
  auto* bracket = new QCPSelectionDecoratorBracket;
  bracket->setTangentToData(true);
  mGraph->setSelectionDecorator(bracket);
  mGraph->setSelectable(QCP::stDataRange);
  mGraph->setSelection(QCPDataSelection(QCPDataRange(1, 4)));
  mPlot->xAxis->setRange(0, 4);
  mPlot->yAxis->setRange(-1, 2);
  gDebugMessages = 0;
  const QtMessageHandler previous = qInstallMessageHandler(countDebug);
  QPicture picture;
  {
    QCPPainter painter(&picture);
    bracket->drawDecoration(&painter, mGraph->selection());
  }
  qInstallMessageHandler(previous);
  QVERIFY(!picture.boundingRect().isEmpty());
  QCOMPARE(gDebugMessages, 0);
}

void TestQCPGraph::anImpulseGraphFitsItsBaseline()
{
  // A stem is drawn from value zero, so the fit has to include zero: the stems start on the axis,
  // as a bar chart does, and the margin goes above the tallest stem.
  mGraph->setLineStyle(QCPGraph::lsImpulse);
  mGraph->setData({1.0, 2.0, 3.0}, {2.0, 6.0, 10.0});

  mPlot->yAxis->rescale();
  QCOMPARE(mPlot->yAxis->range().lower, 0.0);
  QCOMPARE(mPlot->yAxis->range().upper, 10.5);

  mPlot->yAxis->setRange(0, 20);
  mGraph->rescaleValueAxis();
  QCOMPARE(mPlot->yAxis->range().lower, 0.0);
  QCOMPARE(mPlot->yAxis->range().upper, 10.5);
}

void TestQCPGraph::anImpulseGraphOnALogAxisFitsItsData()
{
  // On a log value axis the stem is drawn down to the axis' bound, whatever it is: that bound is
  // not a data value, so the fit neither holds it nor leaves the margin below the data.
  mPlot->yAxis->setScale(QCPScale::log());
  mPlot->yAxis->setRange(0.001, 1000);
  mGraph->setLineStyle(QCPGraph::lsImpulse);
  mGraph->setData({1.0, 2.0, 3.0}, {2.0, 6.0, 10.0});

  mPlot->yAxis->rescale();

  const double margin = 0.05 * (1.0 - std::log10(2.0));
  QVERIFY(std::abs(mPlot->yAxis->range().lower - std::pow(10.0, std::log10(2.0) - margin)) < 1e-9);
  QVERIFY(std::abs(mPlot->yAxis->range().upper - std::pow(10.0, 1.0 + margin)) < 1e-9);
}

void TestQCPGraph::aComponentDrawingImpulsesFitsItsBaseline()
{
  // The drawing decides per component: a component set to impulses draws stems from zero although
  // the graph-wide style is a line, so the fit holds zero.
  mGraph->setData({1.0, 2.0, 3.0}, {2.0, 6.0, 10.0});
  mGraph->setComponentLineStyle(0, QCPGraph::lsImpulse);

  mPlot->yAxis->rescale();

  QCOMPARE(mPlot->yAxis->range().lower, 0.0);
  QCOMPARE(mPlot->yAxis->range().upper, 10.5);
}

void TestQCPGraph::aGraphWhoseVisibleComponentsDrawLinesTakesNoBaseline()
{
  // The reverse: a graph-wide impulse style whose components all draw lines draws no stem, so zero
  // stays out of the fit and the margin goes below the data.
  mGraph->setData({1.0, 2.0, 3.0}, {2.0, 6.0, 10.0});
  mGraph->setLineStyle(QCPGraph::lsImpulse);
  mGraph->setComponentLineStyle(0, QCPGraph::lsLine);

  mPlot->yAxis->rescale();

  QCOMPARE(mPlot->yAxis->range().lower, 1.6);
  QCOMPARE(mPlot->yAxis->range().upper, 10.4);
}
