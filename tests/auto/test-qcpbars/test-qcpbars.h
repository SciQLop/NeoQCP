#include <QtTest/QtTest>
#include "qcustomplot.h"

class TestQCPBars : public QObject
{
  Q_OBJECT
private slots:
  void init();
  void cleanup();
  
  void dataManipulation();
  void dataSharing();

  void setDataTwiceRedrawsTheNewBars();
  void addDataShowsTheNewBar();
  void containerEditRedraws();

  void groupedBarsFollowAPan();
  void stackedBarsFollowAPan();
  void groupedBarsFollowAPanOnGpu();
  void stackedBarsFollowAPanOnGpu();
  void plotWidthBarsNeverReRecord();
  void pixelWidthBarsReRecordOnEveryPan();
  void barsSelectionReRecordsAndUsesTheDecorator();
  void barsPanDoesNotReRecord();
  
private:
  QCustomPlot *mPlot;
  QCPBars *mBars;
};





