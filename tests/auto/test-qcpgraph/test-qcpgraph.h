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
  void aTangentBracketPrintsNothing();
  void anImpulseGraphFitsItsBaseline();
  void anImpulseGraphOnALogAxisFitsItsData();
  void aComponentDrawingImpulsesFitsItsBaseline();
  void aGraphWhoseVisibleComponentsDrawLinesTakesNoBaseline();
  
private:
  QCustomPlot *mPlot;
  QCPGraph *mGraph;
};





