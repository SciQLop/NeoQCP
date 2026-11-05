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

  void plotWidthBarsNeverReRecord();
  void pixelWidthBarsReRecordOnZoomOnly();
  void barsSelectionReRecordsAndUsesTheDecorator();
  void barsPanDoesNotReRecord();
  
private:
  QCustomPlot *mPlot;
  QCPBars *mBars;
};





