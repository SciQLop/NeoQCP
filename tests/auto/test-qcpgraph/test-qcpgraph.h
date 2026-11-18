#include <QtTest/QtTest>
#include "qcustomplot.h"

class TestQCPGraph : public QObject
{
  Q_OBJECT
private slots:
  void init();
  void cleanup();
  
  void specializedGraphInterface();
  void multiColumnAndWaterfallGraphsAreListed();
  void selectedGraphsAndAxisGraphsListTheMultigraph();
  void dataManipulation();
  void channelFill();
  void aTangentBracketPrintsNothing();
  void anImpulseGraphFitsItsBaseline();
  
private:
  QCustomPlot *mPlot;
  QCPGraph *mGraph;
};





