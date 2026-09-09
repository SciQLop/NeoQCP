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
  void QCPColorMap2_selectTestMissReturnsNegativeOne();
  void QCPColorMap2_contourSettersScheduleReplot();
  void QCPColorMapRhiLayer_setImageSkipsRedundantUpload();
  void QCPColorMap2_hidesStaleQuadWhenPannedPastData();
  void QCPContourExtractor_extractUvKeepsSourceRegistration();
  void QCPColorMapData_recalculateDataBoundsSkipsInfinite();
  void QCPColorMap2_contourMirrorsReversedAxis();
  void QCPColorMap2_contourExportDrawsFallback();
  void QCPColorMap2_contourDegenerateDataClearsLines();
  void QCPColorMap2_autoContourLevelsGeometricInLogScale();
  void QCPColorMap2_contourExportAfterRhiDraw();
  void QCPColorMap2_contourExportCachesFallback();
  void QCPColorMapData_fillNonFiniteKeepsBoundsDegenerate();

private:
  QCustomPlot *mPlot;
  QCPColorMap *mColorMap;
};





