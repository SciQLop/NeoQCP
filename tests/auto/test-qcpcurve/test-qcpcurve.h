#include <QtTest/QtTest>
#include <qcp.h>

class TestQCPCurve : public QObject
{
  Q_OBJECT
private slots:
  void init();
  void cleanup();
  
  void dataManipulation();
  void dataSharing();
  
private:
  QCustomPlot *mPlot;
  QCPCurve *mCurve;
};





