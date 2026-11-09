#include <QtTest/QtTest>
#include "qcustomplot.h"

class TestQCPFinancial : public QObject
{
  Q_OBJECT
private slots:
  void init();
  void cleanup();
  
  void dataManipulation();
  void dataSharing();

  void recordedChartsMatchTheirReferences();
  void recordedChartsMatchTheirReferences_data();
  void aPannedCandlestickIsNotReRecorded();
  void everyStyleSetterReRecords();
  void aLogKeyAxisCentresTheBodyInPixels();
  
private:
  QCustomPlot *mPlot;
  QCPFinancial *mFinancial;
};





