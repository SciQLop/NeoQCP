#include <QtTest/QtTest>
#include "qcustomplot.h"

class TestQCustomPlot : public QObject
{
  Q_OBJECT
private slots:
  void init();
  void cleanup();
  
  void interface_plottables();
  void interface_items();
  void rescaleAxes_GraphVisibility();
  void rescaleAxes_FlatGraph();
  void rescaleAxes_MultipleFlatGraphs();
  
private:
  QCustomPlot *mPlot;
};
