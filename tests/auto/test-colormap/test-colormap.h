#include <QtTest/QtTest>
#include "qcustomplot.h"

class TestColorMap : public QObject
{
  Q_OBJECT
private slots:
  void init();
  void cleanup();
  
  void QCPColorScale_rescaleDataRange();
  void QCPColorMapData_fillSetsExactValue();
  void QCPColorMapData_constructorZeroInitializes();
  void QCPColorMapData_fillIsSafeOnEmptyMap();
  void QCPColorMapData_cellToCoordHandlesSingleCellDimension();
  void QCPColorMap2_selectTestHitSetsDetails();
  void QCPColorMap2_legendIconShowsTheGradient();
  void QCPHistogram2D_legendIconShowsTheGradient();
  void QCPColorMap2_selectTestMissReturnsNegativeOne();
  void QCPColorMap2_contourSettersScheduleReplot();
  void QCPColorMap2_hidesStaleQuadWhenPannedPastData();
  void QCPContourExtractor_extractUvKeepsSourceRegistration();
  void QCPColorMapData_recalculateDataBoundsSkipsInfinite();
  void QCPColorMap2_contourMirrorsReversedAxis();
  void QCPColorMap2_contourExportDrawsFallback();
  void QCPColorMap2_contourLabelsDrawSpacedInsideThePlot();
  void QCPColorMap2_contourLabelsShowOnTheGpuFrame();
  void QCPColorMap2_contourLinesComeBackAfterAPanAwayAndBack();
  void QCPColorMap2_unchangedLabelsAreNotUploadedAgain();
  void QCPColorMap2_contourExportFollowsALogValueAxis();
  void QCPColorMap2_exportDrawsTheImageOnAReversedAxis();
  void QCPColorMap2_contourLabelsFollowAColourScaleToggle();
  void QCPColorMap2_contourDegenerateDataClearsLines();
  void QCPColorMap2_autoContourLevelsGeometricInLogScale();
  void QCPColorMap2_contourExportAfterRhiDraw();
  void QCPColorMap2_contourExportCachesFallback();
  void QCPColorMapData_fillNonFiniteKeepsBoundsDegenerate();
  void QCPColorScale_rescaleDataRangeIncludesColorMap2();
  void QCPColorMap2_drawEmitsNoBusyChange();
  void QCPColorMap2_colourSlotDeletingTheMapIsSafe();
  void QCPHistogram2D_colourSlotDeletingTheMapIsSafe();
  void QCPColorMap_scaleTypeSlotDeletingTheMapIsSafe();
  void QCPColorScale_rangeSlotDeletingTheScaleIsSafe();
  void QCPColorScale_gradientSlotDeletingTheScaleIsSafe();
  void colourMap2JoinsItsNewPlotsScaleWhenAdded();
  void colourMap2HonoursAVerticalKeyAxis();
  void colourMap2ExportWaitsForNoJobOfItsSize();
  void colourMap2KeepsTheExportsImageOverAnOlderJob();
  void histogram2dHonoursAVerticalKeyAxis();
  void contourLabelsFollowAVerticalKeyAxis();
  void QCPColorMap2_forgetsADeletedColorScale();
  void QCPHistogram2D_forgetsADeletedColorScale();
  void QCPColorMap_forgetsADeletedColorScale();
  void QCPColorMap_colorScaleSlotDeletingTheMapIsSafe();
  void QCPColorScale_rescaleSeesEveryUser();
  void QCPColorScale_aSetterOnOneUserReachesTheOthers();
  void QCPColorScale_attachingTakesTheScaleValues();
  void QCPColorScale_deletingTheScaleIsNoticed();
  void QCPColorScale_deletingAUserLeavesTheScaleList();
  void QCPColorMap2_rescaleKeepsItsViewMeaning();
  void QCPColorMap_rescaleThroughTheBaseRecalculates();
  void colourMapIsOneImageCommand();
  void colourMap2IsOneImageCommand();
  void aPanDoesNotRecolour();
  void contoursAreDataPolylines();
  void contoursDrawOverTheImageOnTheGpu();
  void histogram2DExportBeforeTheFirstBinHasTheImage();
  void noColourMapLayerIsCreated();
  void verticalKeyAxisMapMatchesTheExport();
  void tightBoundaryCropsHalfACell();
  void aPanDoesNotRebuildTheImage();
  void exportMatchesTheOldCode();
  void theHotCellLandsInItsCornerOnEveryAxisLayout();

private:
  QCustomPlot *mPlot;
  QCPColorMap *mColorMap;
};





