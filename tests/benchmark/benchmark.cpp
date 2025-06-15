#include <QtTest/QtTest>

#include <qcustomplot.h>

class Benchmark : public QObject
{
  Q_OBJECT

private slots:
  void init();
  void cleanup();
  
  void QCPGraph_Standard();
  void QCPGraph_ManyPoints();
  void QCPGraph_ManyLines();
  void QCPGraph_ManyOffScreenLines();
  void QCPGraph_RemoveDataBetween();
  void QCPGraph_RemoveDataAfter();
  void QCPGraph_RemoveDataBefore();
  void QCPGraph_AddDataAtEndSorted();
  void QCPGraph_AddDataAtBeginSorted();
  void QCPGraph_AddDataMixedSorted();
  void QCPGraph_AddDataAtEndUnsorted();
  void QCPGraph_AddDataAtBeginUnsorted();
  void QCPGraph_AddDataMixedUnsorted();
  void QCPGraph_AddDataSingleAtEnd();
  void QCPGraph_AddDataSingleAtBegin();
  void QCPGraph_AddDataSingleRandom();
  
  void QCPColorMap_Standard();
  void QCPColorMap_ColorizeMap();
  
  
  void QCPAxis_TickLabels();
  void QCPAxis_TickLabelsCached();
  
private:
  QCustomPlot *mPlot;
  QWidget mContainerWidget;
};

QTEST_MAIN(Benchmark)
#include "benchmark.moc"


////////////////////////////////////////////////////////////////////
/////// Benchmark Implementation
////////////////////////////////////////////////////////////////////

void Benchmark::init()
{
  if (!mContainerWidget.isVisible())
  {
    mContainerWidget.setGeometry(0, 0, 640, 360);
    mContainerWidget.show();
  }
  mPlot = new QCustomPlot(&mContainerWidget);
  mPlot->setGeometry(0, 0, 640, 360);
  mPlot->show();
  qApp->processEvents();
}

void Benchmark::cleanup()
{
  delete mPlot;
}

void Benchmark::QCPGraph_Standard()
{
  QCPGraph *graph1 = mPlot->addGraph();
  QCPGraph *graph2 = mPlot->addGraph();
  QCPGraph *graph3 = mPlot->addGraph();
  graph1->setBrush(QBrush(QColor(100, 0, 0, 100)));
  int n = 500;
  QVector<double> x(n), y1(n), y2(n), y3(n);
  for (int i=0; i<n; ++i)
  {
    x[i] = i/(double)n;
    y1[i] = qSin(x[i]*10*M_PI);
    y2[i] = qCos(x[i]*40*M_PI);
    y3[i] = x[i];
  }
  graph1->setData(x, y1);
  graph2->setData(x, y2);
  graph3->setData(x, y3);
  mPlot->rescaleAxes();
  mPlot->xAxis->scaleRange(0.7, mPlot->xAxis->range().center());
  
  QBENCHMARK
  {
    mPlot->replot();
  }
 
}

void Benchmark::QCPGraph_ManyPoints()
{
  QCPGraph *graph1 = mPlot->addGraph();
  QCPGraph *graph2 = mPlot->addGraph();
  QCPGraph *graph3 = mPlot->addGraph();
  graph1->setBrush(QBrush(QColor(100, 0, 0, 100)));
  graph1->setScatterStyle(QCPScatterStyle::ssCross);
  graph2->setScatterStyle(QCPScatterStyle::ssCircle);
  graph3->setScatterStyle(QCPScatterStyle::ssDiamond);
  graph1->setLineStyle(QCPGraph::lsNone);
  graph2->setLineStyle(QCPGraph::lsNone);
  graph3->setLineStyle(QCPGraph::lsNone);
  int n = 50000;
  QVector<double> x(n), y1(n), y2(n), y3(n);
  for (int i=0; i<n; ++i)
  {
    x[i] = i/(double)n;
    y1[i] = qSin(x[i]*10*M_PI);
    y2[i] = qCos(x[i]*40*M_PI);
    y3[i] = x[i];
  }
  graph1->setData(x, y1);
  graph2->setData(x, y2);
  graph3->setData(x, y3);
  mPlot->rescaleAxes();
  mPlot->xAxis->scaleRange(0.7, mPlot->xAxis->range().center());
  
  QBENCHMARK
  {
    mPlot->replot();
  }
}

void Benchmark::QCPGraph_ManyLines()
{
  QCPGraph *graph1 = mPlot->addGraph();
  QCPGraph *graph2 = mPlot->addGraph();
  QCPGraph *graph3 = mPlot->addGraph();
  graph1->setBrush(QBrush(QColor(100, 0, 0, 100)));
  graph1->setScatterStyle(QCPScatterStyle());
  graph2->setScatterStyle(QCPScatterStyle());
  graph3->setScatterStyle(QCPScatterStyle());
  graph1->setLineStyle(QCPGraph::lsLine);
  graph2->setLineStyle(QCPGraph::lsLine);
  graph3->setLineStyle(QCPGraph::lsLine);
  int n = 50000;
  QVector<double> x(n), y1(n), y2(n), y3(n);
  for (int i=0; i<n; ++i)
  {
    x[i] = i/(double)n;
    y1[i] = qSin(x[i]*10*M_PI);
    y2[i] = qCos(x[i]*40*M_PI);
    y3[i] = x[i];
  }
  graph1->setData(x, y1);
  graph2->setData(x, y2);
  graph3->setData(x, y3);
  mPlot->rescaleAxes();
  mPlot->xAxis->scaleRange(0.7, mPlot->xAxis->range().center());
  
  QBENCHMARK
  {
    mPlot->replot();
  }
}

void Benchmark::QCPGraph_ManyOffScreenLines()
{
  QCPGraph *graph1 = mPlot->addGraph();
  QCPGraph *graph2 = mPlot->addGraph();
  QCPGraph *graph3 = mPlot->addGraph();
  graph1->setBrush(QBrush(QColor(100, 0, 0, 100)));
  graph1->setScatterStyle(QCPScatterStyle::ssNone);
  graph2->setScatterStyle(QCPScatterStyle::ssNone);
  graph3->setScatterStyle(QCPScatterStyle::ssNone);
  graph1->setLineStyle(QCPGraph::lsLine);
  graph2->setLineStyle(QCPGraph::lsLine);
  graph3->setLineStyle(QCPGraph::lsLine);
  int n = 50000;
  QVector<double> x(n), y1(n), y2(n), y3(n);
  for (int i=0; i<n; ++i)
  {
    x[i] = i/(double)n;
    y1[i] = qSin(x[i]*10*M_PI);
    y2[i] = qCos(x[i]*40*M_PI);
    y3[i] = x[i];
  }
  graph1->setData(x, y1);
  graph2->setData(x, y2);
  graph3->setData(x, y3);
  mPlot->rescaleAxes();
  mPlot->xAxis->setRange(1.1, 2.1);
  
  QBENCHMARK
  {
    mPlot->replot();
  }
}

void Benchmark::QCPGraph_RemoveDataBetween()
{
  // we time and report this benchmark ourselves because it must be re-setup
  // before every iteration, and a single iteration is not enough to
  // provide sufficient precision

  QCPGraph *graph = mPlot->addGraph();
  int n = 1000000;
  QVector<double> x1(n), y1(n);
  for (int i=0; i<n; ++i)
  {
    x1[i] = 2.0*i/(double)n;
    y1[i] = qSin(x1[i]*10*M_PI);
  }
  
  QElapsedTimer timer;
  bool done = false;
  qint64 elapsed = 0;
  int iterations = 0;
  while (!done)
  {
    ++iterations;
    graph->setData(x1, y1);
    timer.restart();
    
    graph->data()->remove(0.5, 1.5); // 50% of total data in center
    
    elapsed += timer.nsecsElapsed();
    done = elapsed > 25e3 && iterations > 3; // have 25us and done some iterations
  }
  QTest::setBenchmarkResult(elapsed/1e6/(double)iterations, QTest::WalltimeMilliseconds);
}

void Benchmark::QCPGraph_RemoveDataAfter()
{
  // we time and report this benchmark ourselves because it must be re-setup
  // before every iteration, and a single iteration is not enough to
  // provide sufficient precision

  QCPGraph *graph = mPlot->addGraph();
  int n = 1000000;
  QVector<double> x1(n), y1(n);
  for (int i=0; i<n; ++i)
  {
    x1[i] = 2.0*i/(double)n;
    y1[i] = qSin(x1[i]*10*M_PI);
  }
  
  QElapsedTimer timer;
  bool done = false;
  qint64 elapsed = 0;
  int iterations = 0;
  while (!done)
  {
    ++iterations;
    graph->setData(x1, y1);
    timer.restart();
    
    graph->data()->removeAfter(1.0); // last 50% of total data
    
    elapsed += timer.nsecsElapsed();
    done = elapsed > 25e3 && iterations > 3; // have 25us and done some iterations
  }
  QTest::setBenchmarkResult(elapsed/1e6/(double)iterations, QTest::WalltimeMilliseconds);
}

void Benchmark::QCPGraph_RemoveDataBefore()
{
  // we time and report this benchmark ourselves because it must be re-setup
  // before every iteration, and a single iteration is not enough to
  // provide sufficient precision
  
  QCPGraph *graph = mPlot->addGraph();
  int n = 1000000;
  QVector<double> x1(n), y1(n);
  for (int i=0; i<n; ++i)
  {
    x1[i] = 2.0*i/(double)n;
    y1[i] = qSin(x1[i]*10*M_PI);
  }
  
  QElapsedTimer timer;
  bool done = false;
  qint64 elapsed = 0;
  int iterations = 0;
  while (!done)
  {
    ++iterations;
    graph->setData(x1, y1);
    timer.restart();
    
    graph->data()->removeBefore(1.0); // first 50% of total data
    
    elapsed += timer.nsecsElapsed();
    done = elapsed > 25e3 && iterations > 3; // have 25us and done some iterations
  }
  QTest::setBenchmarkResult(elapsed/1e6/(double)iterations, QTest::WalltimeMilliseconds);
}

void Benchmark::QCPGraph_AddDataAtEndSorted()
{
  QCPGraph *graph = mPlot->addGraph();
  int n = 500000;
  QVector<double> x1(n), y1(n), x2(n), y2(n);
  for (int i=0; i<n; ++i)
  {
    x1[i] = i/(double)n;
    y1[i] = qSin(x1[i]*10*M_PI);
    x2[i] = (i+n)/(double)n;
    y2[i] = qSin(x2[i]*10*M_PI);
  }

  graph->setData(x1, y1);
  QBENCHMARK_ONCE
  {
    graph->addData(x2, y2, true);
  }
}

void Benchmark::QCPGraph_AddDataAtBeginSorted()
{
  QCPGraph *graph = mPlot->addGraph();
  int n = 500000;
  QVector<double> x1(n), y1(n), x2(n), y2(n);
  for (int i=0; i<n; ++i)
  {
    x1[i] = (i+n)/(double)n;
    y1[i] = qSin(x1[i]*10*M_PI);
    x2[i] = i/(double)n;
    y2[i] = qSin(x2[i]*10*M_PI);
  }

  graph->setData(x1, y1);
  QBENCHMARK_ONCE
  {
    graph->addData(x2, y2, true);
  }
}

void Benchmark::QCPGraph_AddDataMixedSorted()
{
  QCPGraph *graph = mPlot->addGraph();
  int n = 500000;
  QVector<double> x1(n), y1(n), x2(n), y2(n);
  for (int i=0; i<n; ++i)
  {
    x1[i] = i/(double)n;
    y1[i] = qSin(x1[i]*10*M_PI);
    x2[i] = (i+0.5)/(double)(3*n);
    y2[i] = qSin(x2[i]*10*M_PI);
  }

  graph->setData(x1, y1);
  QBENCHMARK_ONCE
  {
    graph->addData(x2, y2, true);
  }
}

void Benchmark::QCPGraph_AddDataAtEndUnsorted()
{
  QCPGraph *graph = mPlot->addGraph();
  int n = 500000;
  QVector<double> x1(n), y1(n), x2(n), y2(n);
  for (int i=0; i<n; ++i)
  {
    x1[i] = i/(double)n;
    y1[i] = qSin(x1[i]*10*M_PI);
    x2[i] = (i+n+0.1*(std::rand()%500))/(double)n; // not completely randomized, but local disorder, global ascending order (typical use-case for data acquisition)
    y2[i] = qSin(x2[i]*10*M_PI);
  }

  graph->setData(x1, y1);
  QBENCHMARK_ONCE
  {
    graph->addData(x2, y2, false);
  }
}

void Benchmark::QCPGraph_AddDataAtBeginUnsorted()
{
  QCPGraph *graph = mPlot->addGraph();
  int n = 500000;
  QVector<double> x1(n), y1(n), x2(n), y2(n);
  for (int i=0; i<n; ++i)
  {
    x1[i] = (i+n)/(double)n;
    y1[i] = qSin(x1[i]*10*M_PI);
    x2[i] = (i-0.1*(std::rand()%500))/(double)n; // not completely randomized, but local disorder, global ascending order (typical use-case for data acquisition)
    y2[i] = qSin(x2[i]*10*M_PI);
  }

  graph->setData(x1, y1);
  QBENCHMARK_ONCE
  {
    graph->addData(x2, y2, false);
  }
}

void Benchmark::QCPGraph_AddDataMixedUnsorted()
{
  QCPGraph *graph = mPlot->addGraph();
  int n = 500000;
  QVector<double> x1(n), y1(n), x2(n), y2(n);
  for (int i=0; i<n; ++i)
  {
    x1[i] = i/(double)n;
    y1[i] = qSin(x1[i]*10*M_PI);
    x2[i] = (i+0.1*(std::rand()%500))/(double)(3*n); // not completely randomized, but local disorder, global ascending order (typical use-case for data acquisition)
    y2[i] = qSin(x2[i]*10*M_PI);
  }

  graph->setData(x1, y1);
  QBENCHMARK_ONCE
  {
    graph->addData(x2, y2, false);
  }
}

void Benchmark::QCPGraph_AddDataSingleAtEnd()
{
  // we time and report this benchmark ourselves because it must be re-setup
  // before every iteration, and a single iteration is not enough to
  // provide sufficient precision

  QCPGraph *graph = mPlot->addGraph();
  int n = 50000;
  QVector<double> x1(n), y1(n);
  for (int i=0; i<n; ++i)
  {
    x1[i] = i/(double)n;
    y1[i] = qSin(x1[i]*10*M_PI);
  }
  // prepare random data that can be used by qbenchmark-loops:
  int n2 = 5000;
  QVector<double> y2(n2);
  for (int i=0; i<n2; ++i)
  {
    y2[i] = qSin(i/(double)n*10*M_PI);
  }
  
  graph->setData(x1, y1);
  double key = x1.last()+0.1;
  QElapsedTimer timer;
  qint64 elapsed = 0;
  int iteration = 0;
  while (iteration < n2)
  {
    timer.restart();
    
    graph->addData(key, y2[iteration]);
    
    elapsed += timer.nsecsElapsed();
    key += 0.1;
    ++iteration;
  }
  QTest::setBenchmarkResult(elapsed/1e3/(double)iteration, QTest::WalltimeMilliseconds); // 1e3 instead of 1e6 is intentional to get time of 1000 iterations (fits better to precision of benchmark script)
}

void Benchmark::QCPGraph_AddDataSingleAtBegin()
{
  // we time and report this benchmark ourselves because it must be re-setup
  // before every iteration, and a single iteration is not enough to
  // provide sufficient precision

  QCPGraph *graph = mPlot->addGraph();
  int n = 50000;
  QVector<double> x1(n), y1(n);
  for (int i=0; i<n; ++i)
  {
    x1[i] = i/(double)n;
    y1[i] = qSin(x1[i]*10*M_PI);
  }
  // prepare random data that can be used by qbenchmark-loops:
  int n2 = 5000;
  QVector<double> y2(n2);
  for (int i=0; i<n2; ++i)
  {
    y2[i] = qSin(i/(double)n*10*M_PI);
  }
  
  graph->setData(x1, y1);
  double key = x1.first()-0.1;
  QElapsedTimer timer;
  qint64 elapsed = 0;
  int iteration = 0;
  while (iteration < n2)
  {
    timer.restart();
    
    graph->addData(key, y2[iteration]);
    
    elapsed += timer.nsecsElapsed();
    key -= 0.1;
    ++iteration;
  }
  QTest::setBenchmarkResult(elapsed/1e3/(double)iteration, QTest::WalltimeMilliseconds); // 1e3 instead of 1e6 is intentional to get time of 1000 iterations (fits better to precision of benchmark script)
}

void Benchmark::QCPGraph_AddDataSingleRandom()
{
  // we time and report this benchmark ourselves because it must be re-setup
  // before every iteration, and a single iteration is not enough to
  // provide sufficient precision
  
  QCPGraph *graph = mPlot->addGraph();
  int n = 50000;
  QVector<double> x1(n), y1(n);
  for (int i=0; i<n; ++i)
  {
    x1[i] = i/(double)n;
    y1[i] = qSin(x1[i]*10*M_PI);
  }
  // prepare random data that can be used by qbenchmark-loops:
  int n2 = 5000;
  QVector<double> x2(n2), y2(n2);
  for (int i=0; i<n2; ++i)
  {
    x2[i] = (std::rand()%n+0.5)/(double)n; // randomly in range of existing data, that's why mod n
    y2[i] = qSin(x2[i]*10*M_PI);
  }

  graph->setData(x1, y1);
  QElapsedTimer timer;
  qint64 elapsed = 0;
  int iteration = 0;
  while (iteration < n2)
  {
    timer.restart();
    
    graph->addData(x2[iteration], y2[iteration]);
    
    elapsed += timer.nsecsElapsed();
    ++iteration;
  }
  QTest::setBenchmarkResult(elapsed/1e3/(double)iteration, QTest::WalltimeMilliseconds); // 1e3 instead of 1e6 is intentional to get time of 1000 iterations (fits better to precision of benchmark script)
}

void Benchmark::QCPColorMap_Standard()
{
  QCPColorMap *map = new QCPColorMap(mPlot->xAxis, mPlot->yAxis);
  const int n = 200;
  map->data()->setSize(n, n);
  map->data()->setRange(QCPRange(0, 5), QCPRange(0, 5));
  std::srand(0);
  for (int x=0; x<map->data()->keySize(); ++x)
  {
    for (int y=0; y<map->data()->valueSize(); ++y)
    {
      map->data()->setCell(x, y, std::rand()/double(RAND_MAX));
    }
  }
  
  const int nanCount = 50;
  for (int i=0; i<nanCount*nanCount; ++i)
    map->data()->setCell(i%nanCount + n/2-nanCount/2, i/nanCount + n/2-nanCount/2, qQNaN()); // insert NaNs in reproducible pseudorandom locations
  
  QCPColorGradient gradient(QCPColorGradient::gpHot);
  map->setGradient(gradient);
  map->setInterpolate(false);
  map->rescaleDataRange();
  mPlot->rescaleAxes();
  
  QBENCHMARK
  {
    mPlot->replot();
  }
}

void Benchmark::QCPColorMap_ColorizeMap()
{
  QCPColorMap *map = new QCPColorMap(mPlot->xAxis, mPlot->yAxis);
  const int n = 200;
  map->data()->setSize(n, n);
  map->data()->setRange(QCPRange(0, 5), QCPRange(0, 5));
  std::srand(0);
  for (int x=0; x<map->data()->keySize(); ++x)
  {
    for (int y=0; y<map->data()->valueSize(); ++y)
    {
      map->data()->setCell(x, y, std::rand()/double(RAND_MAX));
    }
  }
  
  const int nanCount = 50;
  for (int i=0; i<nanCount*nanCount; ++i)
    map->data()->setCell(i%nanCount + n/2-nanCount/2, i/nanCount + n/2-nanCount/2, qQNaN()); // insert NaNs in reproducible pseudorandom locations
  
  QCPColorGradient gradient(QCPColorGradient::gpHot);
  map->setGradient(gradient);
  map->setInterpolate(false);
  map->rescaleDataRange();
  mPlot->rescaleAxes();
  
  QBENCHMARK
  {
    map->data()->setCell(0, 0, 0); // to invalidate the currently cached map image
    mPlot->replot();
  }
}

void Benchmark::QCPAxis_TickLabels()
{
  mPlot->setPlottingHint(QCP::phCacheLabels, false);
  mPlot->axisRect()->setupFullAxesBox();
  mPlot->xAxis2->setTickLabels(true);
  mPlot->yAxis2->setTickLabels(true);
  mPlot->xAxis->setRange(-10, 10);
  mPlot->yAxis->setRange(0.001, 0.002);
  mPlot->xAxis2->setRange(1e6, 1e8);
  mPlot->yAxis2->setRange(-1e100, 1e100);
  QBENCHMARK
  {
    mPlot->replot();
  }
}

void Benchmark::QCPAxis_TickLabelsCached()
{
  mPlot->setPlottingHint(QCP::phCacheLabels, true);
  mPlot->axisRect()->setupFullAxesBox();
  mPlot->xAxis2->setTickLabels(true);
  mPlot->yAxis2->setTickLabels(true);
  mPlot->xAxis->setRange(-10, 10);
  mPlot->yAxis->setRange(0.001, 0.002);
  mPlot->xAxis2->setRange(1e6, 1e8);
  mPlot->yAxis2->setRange(-1e100, 1e100);
  QBENCHMARK
  {
    mPlot->replot();
  }
}
