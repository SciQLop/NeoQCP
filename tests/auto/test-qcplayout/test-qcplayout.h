#include <QtTest/QtTest>
#include "qcustomplot.h"

class TestQCPLayout : public QObject
{
  Q_OBJECT
private slots:
  void init();
  void cleanup();
  
  void layoutGridElementManagement();
  void layoutGridInsertion();
  void layoutGridLayout();
  void marginGroup();
  void debugExpectationIgnoresSignatureSpelling();
  
  
private:
  QCustomPlot *mPlot;
};
