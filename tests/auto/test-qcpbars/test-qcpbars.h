#include <QtTest/QtTest>
#include "qcustomplot.h"

class TestQCPBars : public QObject
{
  Q_OBJECT
private slots:
  void init();
  void cleanup();
  
  void dataManipulation();
  void dataSharing();

  void setDataTwiceRedrawsTheNewBars();
  void addDataShowsTheNewBar();
  void containerEditRedraws();

  void groupedBarsFollowAPan();
  void stackedBarsFollowAPan();
  void groupedBarsFollowAPanOnGpu();
  void stackedBarsFollowAPanOnGpu();
  void stackedBarFollowsTheBarBelow();
  void moveAboveRestacks();
  void restackMovesTheGroupSiblingsToTheCentre();
  void groupJoinShiftsTheOthers();
  void groupedBarsOnALogAxis();
  void plotWidthBarsNeverReRecord();
  void pixelWidthBarsReRecordOnEveryPan();
  void barsSelectionReRecordsAndUsesTheDecorator();
  void bracketDecorationOverTheBars();
  void barsPanDoesNotReRecord();

  void barsOnTheKeyBoundEdgeAreHit_data();
  void barsOnTheKeyBoundEdgeAreHit();
  
private:
  QCustomPlot *mPlot;
  QCPBars *mBars;
};





