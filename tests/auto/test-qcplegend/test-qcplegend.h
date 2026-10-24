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
  void itemTextColorRepaintsTheLegend();
  void groupRowFollowsComponentVisibility();
  void deselectSlotRemovingAnItemStillDeselectsTheOthers();
  void deselectSlotDeletingTheLegendIsSafe();
  
private:
  QCustomPlot *mPlot;
};
