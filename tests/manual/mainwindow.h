#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QHBoxLayout>
#include <QDir>
#include <QMetaEnum>
#include <numbers>
#if QT_VERSION >= QT_VERSION_CHECK(4, 7, 0)
#  include <QElapsedTimer>
#endif

#include "../../src/qcp.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
  Q_OBJECT
  
public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow();
  
  // tests:
  void setupItemAnchorTest(QCustomPlot *customPlot);
  void setupItemTracerTest(QCustomPlot *customPlot);
  void setupGraphTest(QCustomPlot *customPlot);
  void setupExportTest(QCustomPlot *customPlot);
  void setupExportMapTest(QCustomPlot *customPlot);
  void setupLogErrorsTest(QCustomPlot *customPlot);
  void setupLogCurveTest(QCustomPlot *customPlot);
  void setupSelectTest(QCustomPlot *customPlot);
  void setupDateTest(QCustomPlot *customPlot);
  void setupTickLabelTest(QCustomPlot *customPlot);
  void setupDaqPerformance(QCustomPlot *customPlot);
  void setupLayoutTest(QCustomPlot* customPlot);
  void setupMultiAxisTest(QCustomPlot* customPlot);
  void setupLayoutElementBugTest(QCustomPlot *customPlot);
  void setupAlignmentGroupTest(QCustomPlot* customPlot);
  void setupInsetLayoutTest(QCustomPlot* customPlot);
  void setupLegendTest(QCustomPlot *customPlot);
  void setupMultiAxisRectInteractions(QCustomPlot *customPlot);
  void setupAdaptiveSamplingTest(QCustomPlot *customPlot);
  void setupOneColumnMultiGraphTest(QCustomPlot *customPlot);
  void setupColorMap2Test(QCustomPlot *customPlot);
  void setupColorMapTest(QCustomPlot *customPlot);
  void setupBarsTest(QCustomPlot *customPlot);
  void setupBarsGroupTest(QCustomPlot *customPlot);
  void setupLargeDataSetDelete(QCustomPlot *customPlot);
  void setupMultiValueGraph(QCustomPlot *customPlot);
  void setupErrorBarTest(QCustomPlot *customPlot);
  void setupDataSelectTest(QCustomPlot *customPlot);
  void setupScatterSkipTest(QCustomPlot *customPlot);
  void setupTimeZoneTest(QCustomPlot *customPlot);
  void setupPolarAxisTest(QCustomPlot *customPlot);
  void setupMultiGraphComparisonTest(QCustomPlot *customPlot);
  void setupWaterfallTest(QCustomPlot *customPlot);
  // testbed:
  void setupTestbed(QCustomPlot *customPlot);
  
  // presets:
  void presetInteractive(QCustomPlot *customPlot);
  
  // helpers:
  void labelItemAnchors(QCPAbstractItem *item, int fontSize=8, bool circle=true, bool labelBelow=true);
  void showSelectTestColorMap(QCustomPlot *customPlot);
  
  // special use cases test:
  void setupIntegerTickStepCase(QCustomPlot *customPlot);
  
public slots:
  void selectionRectChooser(QMouseEvent *event);
  void tracerTestMouseMove(QMouseEvent *event);
  void selectTestColorMapRefresh();
  void mouseMoveRotateTickLabels(QMouseEvent *event);
  void mouseMoveRotateAngularTickLabels(QMouseEvent *event);
  void tickLabelTestTimerSlot();
  void setupMultiAxisRectInteractionsMouseMove(QMouseEvent *event);
  void daqPerformanceDataSlot();
  void daqPerformanceReplotSlot();
  void colorMapMouseMove(QMouseEvent *event);
  
  void testbedMouseClick(QMouseEvent *event);
  
private slots:
  void showReplotTime();
  
private:
  Ui::MainWindow *ui;
  QCustomPlot *mCustomPlot;
  QCPItemTracer *tracerTestTracer;
  QTimer mReplotTimer;
  QTimer mDataTimer;
  QLabel *mStatusBarLabel;
};

#endif // MAINWINDOW_H
