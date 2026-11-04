#include <QtTest/QtTest>

#include <qcustomplot.h>
#include <spatial-index.h>
#include <datasource/soa-datasource-2d.h>
#include <datasource/resample.h>
#include <vector>
#include <span>
#include <memory>
#include <random>

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
  
  void QCPGraph2_SetDataDouble();
  void QCPGraph2_SetDataFloat();
  void QCPGraph2_RenderDouble();
  void QCPGraph2_RenderFloat();
  void QCPGraph2_RenderManyLines();

  void QCPGraph2_ScatterDisc1M();
  void QCPGraph2_ScatterSquare1M();
  void QCPGraph2_ScatterWithLine1M();

  void QCPColorMap_Standard();
  void QCPColorMap_ColorizeMap();

  void QCPColorMap2_ResampleDouble();
  void QCPColorMap2_ResampleFloat();
  void QCPColorMapData_ConstructLarge();
  void QCPColorMap_SetCell1M();
  void QCPColorMap2_ResamplePanSequence();
  void QCPColorMap2_ResamplePanSequenceSmall();
  void QCPColorMap2_ResampleWideSerial();
  void QCPColorMap2_ResampleWideParallel();


  void QCPAxis_TickLabels();
  void QCPAxis_TickLabelsCached();
  void QCPAxis_SetRange10k();
  void QCPAxis_SetTickLength10kItems();

  void QCPItems_AddRemove10k();
  void QCPItems_AddRemove20k();
  void QCPItems_Replot10k();
  void QCPItems_Add10k();
  void QCPItems_Clear10k();
  void QCPItems_DeleteEach10k();
  void QCPItems_RemoveEach10k();
  void QObject_DeleteEachChild10k();
  void QCPItems_DeletePlot10k();
  void QCPItems_Zoom10k();
  void QCPItems_Pan10k();
  void QCPItems_Zoom100();
  void QCPItems_ZoomEmpty();
  void QCPItems_Click10k();
  void QCPItems_ClickEmpty10k();
  void QCPItems_Click100();
  void QCPItems_FirstFrame10k();
  void QCPItems_DragOne10k();
  void QCPItems_RestyleAll10k();
  void QCPAxisRects_Stack8_Replot();
  void QCPPlots_Stack8_Replot();
  void QCPInput_MouseMoveStorm();

  void QCPSpatialIndex_Build10k();
  void QCPSpatialIndex_Query10k();

  void QCPGpu_LineSolid100k();
  void QCPGpu_LineDashed100k();
  void QCPGpu_Pan100k();
  void QCPGpu_Graph2Render1M();
  void QCPGpu_Bars10k();
  void QCPGpu_Text1k();
  void QCPGpu_MixedLayer();

  void QCPBars_Zoom10k();
  void QCPBars_Pan10k();
  void QCPItemText_Zoom1k();

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

void Benchmark::QCPGraph2_SetDataDouble()
{
  auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
  Q_UNUSED(graph);
  const int n = 1000000;

  QBENCHMARK {
    std::vector<double> keys(n), values(n);
    for (int i = 0; i < n; ++i) {
      keys[i] = i / static_cast<double>(n);
      values[i] = qSin(keys[i] * 10 * M_PI);
    }
    graph->setData(std::move(keys), std::move(values));
  }
}

void Benchmark::QCPGraph2_SetDataFloat()
{
  auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
  Q_UNUSED(graph);
  const int n = 1000000;

  QBENCHMARK {
    std::vector<double> keys(n);
    std::vector<float> values(n);
    for (int i = 0; i < n; ++i) {
      keys[i] = i / static_cast<double>(n);
      values[i] = static_cast<float>(qSin(keys[i] * 10 * M_PI));
    }
    graph->setData(std::move(keys), std::move(values));
  }
}

void Benchmark::QCPGraph2_RenderDouble()
{
  auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
  const int n = 1000000;
  std::vector<double> keys(n), values(n);
  for (int i = 0; i < n; ++i) {
    keys[i] = i / static_cast<double>(n);
    values[i] = qSin(keys[i] * 10 * M_PI);
  }
  graph->setData(std::move(keys), std::move(values));
  graph->rescaleAxes();
  mPlot->xAxis->scaleRange(0.7, mPlot->xAxis->range().center());

  QBENCHMARK {
    mPlot->replot();
  }
}

void Benchmark::QCPGraph2_RenderFloat()
{
  auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
  const int n = 1000000;
  std::vector<double> keys(n);
  std::vector<float> values(n);
  for (int i = 0; i < n; ++i) {
    keys[i] = i / static_cast<double>(n);
    values[i] = static_cast<float>(qSin(keys[i] * 10 * M_PI));
  }
  graph->setData(std::move(keys), std::move(values));
  graph->rescaleAxes();
  mPlot->xAxis->scaleRange(0.7, mPlot->xAxis->range().center());

  QBENCHMARK {
    mPlot->replot();
  }
}

void Benchmark::QCPGraph2_RenderManyLines()
{
  const int n = 50000;
  for (int g = 0; g < 3; ++g) {
    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    std::vector<double> keys(n), values(n);
    for (int i = 0; i < n; ++i) {
      keys[i] = i / static_cast<double>(n);
      values[i] = qSin(keys[i] * (10 + 30 * g) * M_PI);
    }
    graph->setData(std::move(keys), std::move(values));
  }
  mPlot->rescaleAxes();
  mPlot->xAxis->scaleRange(0.7, mPlot->xAxis->range().center());

  QBENCHMARK {
    mPlot->replot();
  }
}

void Benchmark::QCPGraph2_ScatterDisc1M()
{
  auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
  graph->setLineStyle(QCPGraph2::lsNone);
  graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 4));
  const int n = 1000000;
  std::vector<double> keys(n), values(n);
  for (int i = 0; i < n; ++i) {
    keys[i] = i / static_cast<double>(n);
    values[i] = qSin(keys[i] * 10 * M_PI) + 0.1 * (std::rand() / double(RAND_MAX));
  }
  graph->setData(std::move(keys), std::move(values));
  graph->rescaleAxes();

  QBENCHMARK {
    mPlot->replot();
  }
}

void Benchmark::QCPGraph2_ScatterSquare1M()
{
  auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
  graph->setLineStyle(QCPGraph2::lsNone);
  graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssSquare, 4));
  const int n = 1000000;
  std::vector<double> keys(n), values(n);
  for (int i = 0; i < n; ++i) {
    keys[i] = i / static_cast<double>(n);
    values[i] = qSin(keys[i] * 10 * M_PI) + 0.1 * (std::rand() / double(RAND_MAX));
  }
  graph->setData(std::move(keys), std::move(values));
  graph->rescaleAxes();

  QBENCHMARK {
    mPlot->replot();
  }
}

void Benchmark::QCPGraph2_ScatterWithLine1M()
{
  auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
  graph->setLineStyle(QCPGraph2::lsLine);
  graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 4));
  const int n = 1000000;
  std::vector<double> keys(n), values(n);
  for (int i = 0; i < n; ++i) {
    keys[i] = i / static_cast<double>(n);
    values[i] = qSin(keys[i] * 10 * M_PI);
  }
  graph->setData(std::move(keys), std::move(values));
  graph->rescaleAxes();

  QBENCHMARK {
    mPlot->replot();
  }
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

namespace {
// Mirrors a realistic streamed spectrogram: nx columns (time) x ys rows
// (frequency), resampled to a ~1080p viewport. Y is 1D (one frequency axis
// shared by all columns) so only X/Z dtype matters for the accessor choice.
template <typename ZT>
QCPColorMapData* benchmarkResample(int nx, int ys)
{
  std::vector<double> x(nx);
  for (int i = 0; i < nx; ++i)
    x[i] = static_cast<double>(i);

  std::vector<double> y(ys);
  for (int j = 0; j < ys; ++j)
    y[j] = static_cast<double>(j);

  std::vector<ZT> z(static_cast<std::size_t>(nx) * ys);
  for (std::size_t i = 0; i < z.size(); ++i)
    z[i] = static_cast<ZT>(i % 997);

  QCPSoADataSource2D<std::span<const double>, std::span<const double>, std::span<const ZT>>
      src{std::span<const double>(x), std::span<const double>(y), std::span<const ZT>(z)};

  return qcp::algo2d::resample(src, 0, nx, QCPRange(0, nx - 1), QCPRange(0, ys - 1),
                                1500, 800, false, false, 0.0);
}
}

void Benchmark::QCPColorMap2_ResampleDouble()
{
  QBENCHMARK
  {
    delete benchmarkResample<double>(50000, 1024);
  }
}

void Benchmark::QCPColorMap2_ResampleFloat()
{
  QBENCHMARK
  {
    delete benchmarkResample<float>(50000, 1024);
  }
}

namespace {
// LOFAR/e-Callisto-scale dynamic spectrum: a wide, dense pan window (100K
// visible time samples) x 3000 frequency channels -- the scale that measured
// ~1.1-1.5s single-threaded (see docs/backlog, colormap perf investigation).
//
// Data is built once by the caller and passed in by reference -- building the
// 300M-cell (2.4GB) dataset *inside* the QBENCHMARK block instead makes the
// allocation+fill dominate the reported time and masks the actual resample()
// parallel speedup (measured 1.3x instead of the real ~2.3-2.5x the first
// time this mistake was made here).
QCPColorMapData* benchmarkResampleWide(
    const QCPSoADataSource2D<std::span<const double>, std::span<const double>,
                              std::span<const double>>& src,
    int nx, int ys, bool forceSerial)
{
  // Target grid matches the un-reduced-Y case (ys stays under the 4x
  // supersampling cap relative to a ~800px-tall viewport, so h == ys; width
  // clamps to 4x a ~1458px-wide viewport) -- the exact regime measured at
  // ~1.1-1.5s single-threaded during the original investigation.
  return qcp::algo2d::resample(src, 0, nx, QCPRange(0, nx - 1), QCPRange(0, ys - 1),
                                5832, ys, false, false, 0.0, nullptr, forceSerial);
}
}

void Benchmark::QCPColorMap2_ResampleWideSerial()
{
  const int nx = 100000, ys = 3000;
  std::vector<double> x(nx);
  for (int i = 0; i < nx; ++i)
    x[i] = static_cast<double>(i);
  std::vector<double> y(ys);
  for (int j = 0; j < ys; ++j)
    y[j] = static_cast<double>(j);
  std::vector<double> z(static_cast<std::size_t>(nx) * ys);
  for (std::size_t i = 0; i < z.size(); ++i)
    z[i] = static_cast<double>(i % 997);
  QCPSoADataSource2D<std::span<const double>, std::span<const double>, std::span<const double>>
      src{std::span<const double>(x), std::span<const double>(y), std::span<const double>(z)};

  QBENCHMARK
  {
    delete benchmarkResampleWide(src, nx, ys, /*forceSerial=*/true);
  }
}

void Benchmark::QCPColorMap2_ResampleWideParallel()
{
  const int nx = 100000, ys = 3000;
  std::vector<double> x(nx);
  for (int i = 0; i < nx; ++i)
    x[i] = static_cast<double>(i);
  std::vector<double> y(ys);
  for (int j = 0; j < ys; ++j)
    y[j] = static_cast<double>(j);
  std::vector<double> z(static_cast<std::size_t>(nx) * ys);
  for (std::size_t i = 0; i < z.size(); ++i)
    z[i] = static_cast<double>(i % 997);
  QCPSoADataSource2D<std::span<const double>, std::span<const double>, std::span<const double>>
      src{std::span<const double>(x), std::span<const double>(y), std::span<const double>(z)};

  QBENCHMARK
  {
    delete benchmarkResampleWide(src, nx, ys, /*forceSerial=*/false);
  }
}

void Benchmark::QCPColorMapData_ConstructLarge()
{
  // Isolates the ctor's own cost (setSize's fill(0) + the ctor's redundant
  // second fill(0)) from any resample work.
  QBENCHMARK
  {
    delete new QCPColorMapData(2000, 2000, QCPRange(0, 1), QCPRange(0, 1));
  }
}

// Filling a map cell by cell on a live plot: every setCell reaches the map's repaint mark.
void Benchmark::QCPColorMap_SetCell1M()
{
  QCPColorMap *map = new QCPColorMap(mPlot->xAxis, mPlot->yAxis);
  const int n = 1000;
  map->data()->setSize(n, n);
  mPlot->replot();
  QBENCHMARK
  {
    for (int x=0; x<n; ++x)
      for (int y=0; y<n; ++y)
        map->data()->setCell(x, y, x+y);
  }
}

void Benchmark::QCPColorMap2_ResamplePanSequence()
{
  // Mirrors a pan: repeated resample() calls at the same target size, only
  // xBegin/xEnd (viewport) moves. Isolates the per-job scratch allocation
  // (accum/counts/gapBetween) cost from the one-off Y axis generation that
  // ResampleCache already caches.
  const int nx = 50000, ys = 256;
  std::vector<double> x(nx), y(ys);
  for (int i = 0; i < nx; ++i) x[i] = static_cast<double>(i);
  for (int j = 0; j < ys; ++j) y[j] = static_cast<double>(j);
  std::vector<double> z(static_cast<std::size_t>(nx) * ys);
  for (std::size_t i = 0; i < z.size(); ++i) z[i] = static_cast<double>(i % 997);

  QCPSoADataSource2D<std::span<const double>, std::span<const double>, std::span<const double>>
      src{std::span<const double>(x), std::span<const double>(y), std::span<const double>(z)};

  QBENCHMARK
  {
    qcp::algo2d::ResampleCache cache;
    for (int step = 0; step < 20; ++step)
    {
      int xBegin = step * 100;
      int xEnd = nx - 20 * 100 + step * 100;
      delete qcp::algo2d::resample(src, xBegin, xEnd, QCPRange(xBegin, xEnd - 1),
                                    QCPRange(0, ys - 1), 1500, 800, false, false, 0.0, &cache);
    }
  }
}

void Benchmark::QCPColorMap2_ResamplePanSequenceSmall()
{
  // Same pattern as QCPColorMap2_ResamplePanSequence but with a small,
  // typical-widget-sized target and a modest windowed source column count —
  // the regime where per-job scratch-buffer allocation could plausibly be a
  // larger fraction of the total cost than in the multi-million-cell case.
  const int nx = 4000, ys = 64;
  std::vector<double> x(nx), y(ys);
  for (int i = 0; i < nx; ++i) x[i] = static_cast<double>(i);
  for (int j = 0; j < ys; ++j) y[j] = static_cast<double>(j);
  std::vector<double> z(static_cast<std::size_t>(nx) * ys);
  for (std::size_t i = 0; i < z.size(); ++i) z[i] = static_cast<double>(i % 997);

  QCPSoADataSource2D<std::span<const double>, std::span<const double>, std::span<const double>>
      src{std::span<const double>(x), std::span<const double>(y), std::span<const double>(z)};

  QBENCHMARK
  {
    qcp::algo2d::ResampleCache cache;
    for (int step = 0; step < 100; ++step)
    {
      int xBegin = step;
      int xEnd = nx - 100 + step;
      delete qcp::algo2d::resample(src, xBegin, xEnd, QCPRange(xBegin, xEnd - 1),
                                    QCPRange(0, ys - 1), 800, 400, false, false, 0.0, &cache);
    }
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

namespace {

QVector<double> sineKeys(int n)
{
  QVector<double> x(n);
  for (int i = 0; i < n; ++i)
    x[i] = i / static_cast<double>(n);
  return x;
}

QVector<double> sineValues(const QVector<double> &x)
{
  QVector<double> y(x.size());
  for (int i = 0; i < x.size(); ++i)
    y[i] = qSin(x[i] * 10 * M_PI);
  return y;
}

void addLineItems(QCustomPlot *plot, int count, double xMax)
{
  for (int i = 0; i < count; ++i)
  {
    auto *item = new QCPItemLine(plot);
    const double x = xMax * i / count;
    item->start->setCoords(x, 0);
    item->end->setCoords(x + 0.01, 1);
  }
}

}

void Benchmark::QCPAxis_SetRange10k()
{
  const auto x = sineKeys(10000);
  mPlot->addGraph()->setData(x, sineValues(x));
  QBENCHMARK
  {
    for (int i = 0; i < 10000; ++i)
      mPlot->xAxis->setRange(i * 1e-5, 1 + i * 1e-5);
  }
}

void Benchmark::QCPAxis_SetTickLength10kItems()
{
  addLineItems(mPlot, 10000, 100);
  int step = 0;
  QBENCHMARK
  {
    for (int i = 0; i < 100; ++i)
      mPlot->xAxis->setTickLengthIn(step++ % 2 ? 8 : 10);
  }
}

void Benchmark::QCPItems_AddRemove10k()
{
  QBENCHMARK
  {
    addLineItems(mPlot, 10000, 100);
    mPlot->clearItems();
  }
}

void Benchmark::QCPItems_AddRemove20k()
{
  QBENCHMARK
  {
    addLineItems(mPlot, 20000, 100);
    mPlot->clearItems();
  }
}

void Benchmark::QCPItems_Replot10k()
{
  addLineItems(mPlot, 10000, 100);
  mPlot->xAxis->setRange(0, 1);
  mPlot->yAxis->setRange(0, 1);
  int step = 0;
  QBENCHMARK
  {
    mPlot->xAxis->setRange(0.001 * ++step, 1 + 0.001 * step);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
  }
}

namespace
{
// A shuffled order, so neither "delete the newest first" nor "delete the oldest first" can hide in
// an index pattern. Seeded, so two runs measure the same sequence.
QVector<QCPAbstractItem *> itemsInOrder(QCustomPlot *plot)
{
  QVector<QCPAbstractItem *> items;
  items.reserve(plot->itemCount());
  for (int i = 0; i < plot->itemCount(); ++i)
    items.append(plot->item(i));
  return items;
}

QVector<int> shuffledIndices(int count)
{
  QVector<int> indices(count);
  std::iota(indices.begin(), indices.end(), 0);
  std::shuffle(indices.begin(), indices.end(), std::mt19937 { 42 });
  return indices;
}
} // namespace

void Benchmark::QCPItems_Add10k()
{
  // The clear happens in cleanup() with the plot, outside the measurement.
  QBENCHMARK_ONCE { addLineItems(mPlot, 10000, 100); }
}

void Benchmark::QCPItems_Clear10k()
{
  addLineItems(mPlot, 10000, 100);
  QBENCHMARK_ONCE { mPlot->clearItems(); }
}

void Benchmark::QCPItems_DeleteEach10k()
{
  // The user path: `delete item`, or a handle's remove(), in a shuffled order.
  addLineItems(mPlot, 10000, 100);
  QVector<QCPAbstractItem *> items = itemsInOrder(mPlot);
  const QVector<int> order = shuffledIndices(items.size());
  QBENCHMARK_ONCE
  {
    for (int index : order)
      delete items[index];
  }
}

void Benchmark::QCPItems_RemoveEach10k()
{
  // The same shuffled order through the plot API, which looks the item up before deleting it.
  addLineItems(mPlot, 10000, 100);
  QVector<QCPAbstractItem *> items = itemsInOrder(mPlot);
  const QVector<int> order = shuffledIndices(items.size());
  QBENCHMARK_ONCE
  {
    for (int index : order)
      mPlot->removeItem(items[index]);
  }
}

void Benchmark::QObject_DeleteEachChild10k()
{
  // The control row: Qt's own children bookkeeping, without any QCP list.
  auto *root = new QObject;
  QList<QObject *> children;
  for (int i = 0; i < 10000; ++i)
    children.append(new QObject(root));
  const QVector<int> order = shuffledIndices(children.size());
  QBENCHMARK_ONCE
  {
    for (int index : order)
      delete children[index];
  }
  delete root;
}

void Benchmark::QCPItems_DeletePlot10k()
{
  QBENCHMARK_ONCE
  {
    auto *plot = new QCustomPlot;
    addLineItems(plot, 10000, 100);
    delete plot;
  }
}

namespace
{
// A zoom around a fixed centre: the range width changes, so the layer cannot be translated and
// every visible item is drawn again on each replot.
void zoomInPlace(QCustomPlot *plot, double centre, double width, int &step)
{
  const double w = (step++ % 2) ? width : width * 0.98;
  plot->xAxis->setRange(centre - w / 2, centre + w / 2);
  plot->replot(QCustomPlot::rpImmediateRefresh);
}
} // namespace

void Benchmark::QCPItems_Zoom10k()
{
  // Items every 0.01 over 0..100; a width of 1 keeps 100 of them in view, as in Zoom100.
  addLineItems(mPlot, 10000, 100);
  mPlot->xAxis->setRange(50, 51);
  mPlot->yAxis->setRange(0, 1);
  mPlot->replot();
  int step = 0;
  QBENCHMARK { zoomInPlace(mPlot, 50.5, 1.0, step); }
}

void Benchmark::QCPItems_Zoom100()
{
  addLineItems(mPlot, 100, 1);
  mPlot->xAxis->setRange(0, 1);
  mPlot->yAxis->setRange(0, 1);
  mPlot->replot();
  int step = 0;
  QBENCHMARK { zoomInPlace(mPlot, 0.5, 1.0, step); }
}

void Benchmark::QCPItems_ZoomEmpty()
{
  // The floor: the same ranges as Zoom100 with no item at all.
  mPlot->xAxis->setRange(0, 1);
  mPlot->yAxis->setRange(0, 1);
  mPlot->replot();
  int step = 0;
  QBENCHMARK { zoomInPlace(mPlot, 0.5, 1.0, step); }
}

namespace
{
// A click is the only pointer event that hit-tests the items: press selects, release resolves the
// click. Alternating between a point on an item and an empty point keeps both directions measured.
void clickStorm(QCustomPlot *plot, const QPointF &onItem, const QPointF &empty)
{
  plot->setInteractions(QCP::iSelectItems);
  int step = 0;
  QBENCHMARK
  {
    const QPointF pos = (step++ % 2) ? onItem : empty;
    const QPointF global = plot->mapToGlobal(pos);
    QMouseEvent press(QEvent::MouseButtonPress, pos, global, Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(plot, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, pos, global, Qt::LeftButton, Qt::NoButton,
                        Qt::NoModifier);
    QCoreApplication::sendEvent(plot, &release);
  }
}
} // namespace

void Benchmark::QCPItems_ClickEmpty10k()
{
  // Clicks that hit nothing and change no selection: the index answers for the items away from the
  // click, and the deselect pass visits only selected items.
  addLineItems(mPlot, 10000, 100);
  // The items span y 0..1; y = 1.5 is in view and hits none of them.
  mPlot->xAxis->setRange(50, 51);
  mPlot->yAxis->setRange(0, 2);
  mPlot->replot();
  const QPointF empty(mPlot->xAxis->coordToPixel(50.5), mPlot->yAxis->coordToPixel(1.5));
  clickStorm(mPlot, empty, empty);
}

void Benchmark::QCPItems_Click10k()
{
  // The same layout as Zoom10k: 100 of the 10 000 items are in view.
  addLineItems(mPlot, 10000, 100);
  mPlot->xAxis->setRange(50, 51);
  mPlot->yAxis->setRange(0, 1);
  mPlot->replot();
  const QPointF onItem(mPlot->xAxis->coordToPixel(50.5), mPlot->yAxis->coordToPixel(0.5));
  const QPointF empty(mPlot->xAxis->coordToPixel(50.9), mPlot->yAxis->coordToPixel(0.5));
  clickStorm(mPlot, onItem, empty);
}

void Benchmark::QCPItems_Click100()
{
  addLineItems(mPlot, 100, 1);
  mPlot->xAxis->setRange(0, 1);
  mPlot->yAxis->setRange(0, 1);
  mPlot->replot();
  const QPointF onItem(mPlot->xAxis->coordToPixel(0.5), mPlot->yAxis->coordToPixel(0.5));
  const QPointF empty(mPlot->xAxis->coordToPixel(0.9), mPlot->yAxis->coordToPixel(0.5));
  clickStorm(mPlot, onItem, empty);
}

void Benchmark::QCPAxisRects_Stack8_Replot()
{
  const auto x = sineKeys(100000);
  const auto y = sineValues(x);
  QList<QCPAxisRect *> rects;
  mPlot->plotLayout()->clear();
  for (int i = 0; i < 8; ++i)
  {
    auto *rect = new QCPAxisRect(mPlot);
    mPlot->plotLayout()->addElement(i, 0, rect);
    mPlot->addGraph(rect->axis(QCPAxis::atBottom), rect->axis(QCPAxis::atLeft))->setData(x, y);
    rects.append(rect);
  }
  mPlot->replot(QCustomPlot::rpImmediateRefresh);
  int step = 0;
  QBENCHMARK
  {
    rects[3]->axis(QCPAxis::atBottom)->setRange(0.001 * ++step, 0.7 + 0.001 * step);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
  }
}

void Benchmark::QCPPlots_Stack8_Replot()
{
  const auto x = sineKeys(100000);
  const auto y = sineValues(x);
  std::vector<std::unique_ptr<QCustomPlot>> plots;
  for (int i = 0; i < 8; ++i)
  {
    auto plot = std::make_unique<QCustomPlot>(&mContainerWidget);
    plot->setGeometry(0, 0, 640, 45);
    plot->addGraph()->setData(x, y);
    plot->rescaleAxes();
    plot->show();
    plots.push_back(std::move(plot));
  }
  qApp->processEvents();
  QCustomPlot *changing = plots[3].get();
  changing->replot(QCustomPlot::rpImmediateRefresh);
  int step = 0;
  QBENCHMARK
  {
    changing->xAxis->setRange(0.001 * ++step, 0.7 + 0.001 * step);
    changing->replot(QCustomPlot::rpImmediateRefresh);
  }
}

void Benchmark::QCPInput_MouseMoveStorm()
{
  mPlot->addGraph()->setData(sineKeys(1000), sineValues(sineKeys(1000)));
  mPlot->rescaleAxes();
  mPlot->replot(QCustomPlot::rpImmediateRefresh);
  int moves = 0;
  connect(mPlot, &QCustomPlot::mouseMove, mPlot, [&moves] { ++moves; });
  const int kMoves = 10000;
  QBENCHMARK
  {
    moves = 0;
    for (int i = 0; i < kMoves; ++i)
    {
      const QPointF pos(10 + i % 600, 10 + i % 300);
      QMouseEvent event(QEvent::MouseMove, pos, mPlot->mapToGlobal(pos), Qt::NoButton,
                        Qt::NoButton, Qt::NoModifier);
      QCoreApplication::sendEvent(mPlot, &event);
    }
  }
  QCOMPARE(moves, kMoves);
}

namespace
{
std::vector<std::pair<QCPDataBox, int>> spatialIndexEntries10k()
{
  std::mt19937 rng(7);
  std::uniform_real_distribution<double> position(0.0, 1000.0);
  std::vector<std::pair<QCPDataBox, int>> entries;
  entries.reserve(10000);
  for (int i = 0; i < 10000; ++i)
  {
    const double key = position(rng);
    const double value = position(rng);
    entries.push_back({{key, key + 1.0, value, value + 1.0}, i});
  }
  return entries;
}
} // namespace

void Benchmark::QCPSpatialIndex_Build10k()
{
  const std::vector<std::pair<QCPDataBox, int>> entries = spatialIndexEntries10k();
  QCPSpatialIndex index;
  QBENCHMARK { index.build(entries); }
  QCOMPARE(index.size(), 10000);
}

void Benchmark::QCPSpatialIndex_Query10k()
{
  QCPSpatialIndex index;
  index.build(spatialIndexEntries10k());
  // 10 000 items of one unit square over 1000x1000: a 100x100 window hits about a hundred.
  const QCPDataBox query{450.0, 550.0, 450.0, 550.0};
  int hits = 0;
  QBENCHMARK
  {
    hits = 0;
    index.query(query, [&hits](int) { ++hits; });
  }
  QVERIFY(hits > 50 && hits < 200);
}

void Benchmark::QCPItems_FirstFrame10k()
{
  QBENCHMARK
  {
    addLineItems(mPlot, 10000, 100);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    mPlot->clearItems();
  }
}

void Benchmark::QCPItems_DragOne10k()
{
  addLineItems(mPlot, 10000, 100);
  mPlot->replot(QCustomPlot::rpImmediateRefresh);
  auto* dragged = qobject_cast<QCPItemLine*>(mPlot->item(5000));
  QVERIFY(dragged);
  double x = 50.0;
  QBENCHMARK
  {
    x += 0.01;
    dragged->start->setCoords(x, 0.2);
    dragged->end->setCoords(x + 0.01, 0.8);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
  }
  mPlot->clearItems();
}

namespace
{
std::vector<QCPItemLine*> linesOf(const QCustomPlot& plot)
{
    std::vector<QCPItemLine*> lines;
    for (int i = 0; i < plot.itemCount(); ++i)
        if (auto* line = qobject_cast<QCPItemLine*>(plot.item(i)))
            lines.push_back(line);
    return lines;
}
} // namespace

void Benchmark::QCPItems_RestyleAll10k()
{
    addLineItems(mPlot, 10000, 100);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    const std::vector<QCPItemLine*> lines = linesOf(*mPlot);
    QCOMPARE(int(lines.size()), 10000);
    int round = 0;
    QBENCHMARK_ONCE
    {
        const QPen pen(QColor(30 * (round++ % 8), 120, 200), 1 + round % 4);
        for (QCPItemLine* line : lines)
            line->setPen(pen);
        mPlot->replot(QCustomPlot::rpImmediateRefresh);
    }
    mPlot->clearItems();
}

void Benchmark::QCPItems_Pan10k()
{
  // Same layout as Zoom10k: 10 000 items every 0.01 over 0..100, 100 of them in view.
  addLineItems(mPlot, 10000, 100);
  mPlot->xAxis->setRange(50, 51);
  mPlot->yAxis->setRange(0, 1);
  mPlot->replot();
  double left = 50;
  QBENCHMARK
  {
    left += 0.01; // a pan step: the width never changes, only the window slides
    mPlot->xAxis->setRange(left, left + 1);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
  }
}

namespace
{
// A GPU row builds its own shown window: mPlot lives in a container an earlier row may have kept
// unexposed, and only a shown widget creates a QRhi. Returns false when the platform has none
// (qoffscreen), which the row turns into QSKIP.
bool makeGpuPlot(QWidget &host, QCustomPlot *&plot)
{
  host.resize(1000, 600);
  plot = new QCustomPlot(&host);
  plot->setGeometry(0, 0, 1000, 600);
  host.show();
  if (!QTest::qWaitForWindowExposed(&host))
    return false;
  // QRhiWidget creates its QRhi on the first render: pump the loop until that has happened.
  QElapsedTimer waited;
  waited.start();
  while (!plot->rhi() && waited.elapsed() < 5000)
  {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    QTest::qWait(10);
  }
  return plot->rhi() != nullptr;
}
} // namespace
namespace
{
void fillSine(QVector<double> &keys, QVector<double> &values, double span)
{
  const int n = int(keys.size());
  for (int i = 0; i < n; ++i)
  {
    keys[i] = span * i / n;
    values[i] = qSin(6 * M_PI * i / n);
  }
}
} // namespace
namespace
{
// A freshly shown plot has to paint once before the loop so texture/pipeline setup stays out of
// the measurement.
void settleGpuPlot(QCustomPlot *plot)
{
  plot->replot();
  QCoreApplication::processEvents();
}
} // namespace

void Benchmark::QCPGpu_LineSolid100k()
{
  QWidget host;
  QCustomPlot *plot = nullptr;
  if (!makeGpuPlot(host, plot))
    QSKIP("no QRhi on this platform (qoffscreen)");
  auto *graph = plot->addGraph();
  graph->setPen(QPen(Qt::black, 1, Qt::SolidLine));
  QVector<double> keys(100000), values(100000);
  fillSine(keys, values, 1.0);
  graph->setData(keys, values);
  plot->xAxis->setRange(0, 1);
  plot->yAxis->setRange(-1.2, 1.2);
  settleGpuPlot(plot);
  int step = 0;
  QBENCHMARK { zoomInPlace(plot, 0.5, 1.0, step); }
}

void Benchmark::QCPGpu_LineDashed100k()
{
  QWidget host;
  QCustomPlot *plot = nullptr;
  if (!makeGpuPlot(host, plot))
    QSKIP("no QRhi on this platform (qoffscreen)");
  auto *graph = plot->addGraph();
  graph->setPen(QPen(Qt::black, 1, Qt::DashLine)); // the QPainter branch, on a GPU surface
  QVector<double> keys(100000), values(100000);
  fillSine(keys, values, 1.0);
  graph->setData(keys, values);
  plot->xAxis->setRange(0, 1);
  plot->yAxis->setRange(-1.2, 1.2);
  settleGpuPlot(plot);
  int step = 0;
  QBENCHMARK { zoomInPlace(plot, 0.5, 1.0, step); }
}

void Benchmark::QCPGpu_Pan100k()
{
  QWidget host;
  QCustomPlot *plot = nullptr;
  if (!makeGpuPlot(host, plot))
    QSKIP("no QRhi on this platform (qoffscreen)");
  auto *graph = plot->addGraph();
  graph->setPen(QPen(Qt::black, 1, Qt::SolidLine));
  QVector<double> keys(100000), values(100000);
  fillSine(keys, values, 100.0);
  graph->setData(keys, values);
  plot->xAxis->setRange(50, 51);
  plot->yAxis->setRange(-1.2, 1.2);
  settleGpuPlot(plot);
  double left = 50;
  QBENCHMARK
  {
    left += 0.01; // 1 % of the window, the pan-fast-path candidate
    plot->xAxis->setRange(left, left + 1);
    plot->replot(QCustomPlot::rpImmediateRefresh);
  }
}

void Benchmark::QCPGpu_Graph2Render1M()
{
  QWidget host;
  QCustomPlot *plot = nullptr;
  if (!makeGpuPlot(host, plot))
    QSKIP("no QRhi on this platform (qoffscreen)");
  auto *graph = new QCPGraph2(plot->xAxis, plot->yAxis);
  const int n = 1000000;
  std::vector<double> keys(n), values(n);
  for (int i = 0; i < n; ++i)
  {
    keys[i] = i / static_cast<double>(n);
    values[i] = qSin(keys[i] * 10 * M_PI);
  }
  graph->setData(std::move(keys), std::move(values));
  plot->xAxis->setRange(0, 1);
  plot->yAxis->setRange(-1.2, 1.2);
  settleGpuPlot(plot);
  int step = 0;
  QBENCHMARK { zoomInPlace(plot, 0.5, 1.0, step); }
}

void Benchmark::QCPGpu_Bars10k()
{
  QWidget host;
  QCustomPlot *plot = nullptr;
  if (!makeGpuPlot(host, plot))
    QSKIP("no QRhi on this platform (qoffscreen)");
  auto *bars = new QCPBars(plot->xAxis, plot->yAxis);
  QVector<double> keys(10000), values(10000);
  for (int i = 0; i < 10000; ++i)
  {
    keys[i] = 100.0 * i / 10000;
    values[i] = 0.5 + 0.5 * qSin(i * 0.01);
  }
  bars->setData(keys, values);
  plot->xAxis->setRange(50, 51);
  plot->yAxis->setRange(0, 1);
  settleGpuPlot(plot);
  int step = 0;
  QBENCHMARK { zoomInPlace(plot, 50.5, 1.0, step); }
}

void Benchmark::QCPGpu_Text1k()
{
  QWidget host;
  QCustomPlot *plot = nullptr;
  if (!makeGpuPlot(host, plot))
    QSKIP("no QRhi on this platform (qoffscreen)");
  for (int i = 0; i < 1000; ++i)
  {
    auto *text = new QCPItemText(plot);
    text->setText(QString::number(i));
    text->position->setCoords(100.0 * i / 1000, 0.5);
  }
  plot->xAxis->setRange(50, 51);
  plot->yAxis->setRange(0, 1);
  settleGpuPlot(plot);
  int step = 0;
  QBENCHMARK { zoomInPlace(plot, 50.5, 1.0, step); }
}

void Benchmark::QCPGpu_MixedLayer()
{
  QWidget host;
  QCustomPlot *plot = nullptr;
  if (!makeGpuPlot(host, plot))
    QSKIP("no QRhi on this platform (qoffscreen)");
  // The stacking question of the drawing API: GPU-drawn graphs and QPainter text interleaved in one
  // layer, which is what a default plot does today.
  for (int i = 0; i < 100; ++i)
  {
    auto *graph = plot->addGraph();
    graph->setPen(QPen(QColor::fromHsv((i * 7) % 360, 200, 160), 1, Qt::SolidLine));
    QVector<double> keys(1000), values(1000);
    for (int j = 0; j < 1000; ++j)
    {
      keys[j] = j / 999.0;
      values[j] = 0.9 * qSin(6 * M_PI * keys[j] + i * 0.1);
    }
    graph->setData(keys, values);
    auto *text = new QCPItemText(plot);
    text->setText(QStringLiteral("g%1").arg(i));
    text->position->setCoords(0.01 * i, 0.9);
  }
  plot->xAxis->setRange(0, 1);
  plot->yAxis->setRange(-1.1, 1.1);
  settleGpuPlot(plot);
  int step = 0;
  QBENCHMARK { zoomInPlace(plot, 0.5, 1.0, step); }
}

void Benchmark::QCPBars_Zoom10k()
{
  auto *bars = new QCPBars(mPlot->xAxis, mPlot->yAxis);
  QVector<double> keys(10000), values(10000);
  for (int i = 0; i < 10000; ++i)
  {
    keys[i] = 100.0 * i / 10000;
    values[i] = 0.5 + 0.5 * qSin(i * 0.01);
  }
  bars->setData(keys, values);
  mPlot->xAxis->setRange(50, 51);
  mPlot->yAxis->setRange(0, 1);
  mPlot->replot();
  int step = 0;
  QBENCHMARK { zoomInPlace(mPlot, 50.5, 1.0, step); }
}

void Benchmark::QCPBars_Pan10k()
{
  auto *bars = new QCPBars(mPlot->xAxis, mPlot->yAxis);
  QVector<double> keys(10000), values(10000);
  for (int i = 0; i < 10000; ++i)
  {
    keys[i] = 100.0 * i / 10000;
    values[i] = 0.5 + 0.5 * qSin(i * 0.01);
  }
  bars->setData(keys, values);
  mPlot->xAxis->setRange(50, 51);
  mPlot->yAxis->setRange(0, 1);
  mPlot->replot();
  double left = 50;
  QBENCHMARK
  {
    left += 0.01;
    mPlot->xAxis->setRange(left, left + 1);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
  }
}

void Benchmark::QCPItemText_Zoom1k()
{
  for (int i = 0; i < 1000; ++i)
  {
    auto *text = new QCPItemText(mPlot);
    text->setText(QString::number(i));
    text->position->setCoords(100.0 * i / 1000, 0.5);
  }
  mPlot->xAxis->setRange(50, 51);
  mPlot->yAxis->setRange(0, 1);
  mPlot->replot();
  int step = 0;
  QBENCHMARK { zoomInPlace(mPlot, 50.5, 1.0, step); }
}
