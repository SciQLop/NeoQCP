#include <QtTest/QtTest>
#include "qcustomplot.h"

class TestQCPGraph : public QObject
{
  Q_OBJECT
private slots:
  void init();
  void cleanup();
  
  void specializedGraphInterface();
  void dataManipulation();
  void dataSharing();
  void channelFill();
  
private:
  QCustomPlot *mPlot;
  QCPGraph *mGraph;
};





