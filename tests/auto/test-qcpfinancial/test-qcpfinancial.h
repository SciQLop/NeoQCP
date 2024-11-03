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
  
private:
  QCustomPlot *mPlot;
  QCPFinancial *mFinancial;
};





