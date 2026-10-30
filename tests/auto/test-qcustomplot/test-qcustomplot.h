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
  void calculateMargin_staleTickVectors();
  void dateTimeTicker_extremeZoomOutNoCrash();
  void escapeCancelsTheSelectionRect();
  void clickSlotDeletingTheGraphIsSafe();
  void deselectSlotDeletingTheClickedGraphIsSafe();
  void rectDeselectSlotDeletingACandidateIsSafe();
  void clickSlotDeletingTheItemIsSafe();
  void deleteKeySlotDeletingEveryItemIsSafe();
  void moveSlotReplacingTheSelectionRectIsSafe();
  void cancelSlotReplacingTheSelectionRectIsSafe();
  void escapeSlotReplacingTheSelectionRectIsSafe();
  void moveSlotDeletingTheSelectionRectIsSafe();
  void escapeSlotDeletingTheSelectionRectIsSafe();
  void replacingADraggedSelectionRectFromItsCancelSlotIsSafe();
  void deletingADraggedSelectionRectDoesNotSignal();
  void cancelSlotInstallingARectWhileTheDraggedOneIsDeletedIsSafe();
  void settingTheCurrentSelectionRectKeepsIt();
  void cancelSlotReinstallingThePreviousRectZoomsOnce();
  void cancelSlotDeletingThePlotWhileSettingARectIsSafe();
  void cancelSlotDeletingThePlotWhileClearingTheRectModeIsSafe();
  void clickSelectionSlotDeletingThePlotIsSafe();
  void rectSelectionSlotDeletingThePlotIsSafe();
  void rectZoomSlotDeletingThePlotIsSafe();
  void releaseSlotDeletingThePlotIsSafe();
  void wheelSlotDeletingThePlotIsSafe();
  void angularRangeSlotDeletingTheAxisIsSafe();
  void radialRangeSlotDeletingTheAxisIsSafe();
  void radialDragSlotRemovingARadialAxisKeepsTheOthersBaselines();
  void radialDragSlotDeletingTheAngularAxisIsSafe();
  void radialWheelSlotRemovingARadialAxisZoomsTheOthers();
  void radialWheelSlotDeletingTheAngularAxisIsSafe();
  void angularDragSlotDeletingTheAngularAxisIsSafe();
  void angularWheelSlotDeletingTheAngularAxisIsSafe();
  void radialDragSlotDeletingTheAngularAxisRestoresAntialiasing();

private:
  QCustomPlot *mPlot;
};
