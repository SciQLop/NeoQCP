#include <QtTest/QtTest>
#include "qcustomplot.h"

class TestQCPLegend : public QObject
{
  Q_OBJECT
private slots:
  void init();
  void cleanup();
  
  void autoAddPlottables();
  void addAndRemove();
  
private:
  QCustomPlot *mPlot;
};
