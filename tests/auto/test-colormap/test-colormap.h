#include <QtTest/QtTest>
#include "qcustomplot.h"

class TestColorMap : public QObject
{
  Q_OBJECT
private slots:
  void init();
  void cleanup();
  
  void QCPColorScale_rescaleDataRange();
  void QCPColorMapData_fillSetsExactValue();
  void QCPColorMapData_fillIsSafeOnEmptyMap();

private:
  QCustomPlot *mPlot;
  QCPColorMap *mColorMap;
};





