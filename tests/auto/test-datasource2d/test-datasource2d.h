#pragma once
#include <QObject>

class QCustomPlot;

class TestDataSource2D : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    // Algorithm tests
    void algo2dFindXBegin();
    void algo2dFindXEnd();
    void algo2dXRange();
    void algo2dYRange();
    void algo2dZRange();

    // SoA 2D data source tests
    void soa2dUniformY();
    void soa2dVariableY();
    void soa2dSpanView();
    void soa2dMixedTypes();
    void soa2dRangeQueries();

    // Resample algorithm tests
    void resampleUniformGrid();
    void resampleVariableY();
    void resampleGapDetection();
    void resampleGapDetectionViewportIndependent();
    void resampleGapBoundaryDataPreserved();
    void resampleLogY();
    void resampleEmptyBins();
    void resampleGapDetectedWhenZoomedIn();
    void resampleGapDetectedWithTwoVisibleColumns();

    // Bug fix regression tests
    void resampleLogYNoBinGaps();
    void dataBoundsSkipsNaN();
    void resampleZoomedOutNotBlack();
    void resampleLogYResolutionNotCoarse();
    void resampleVariableYPerColumn();
    void colormap2NanHandling();
    void colormap2DataScaleTypeSync();

    // QCPColorMap2 integration tests
    void colormap2Creation();
    void colormap2SetDataOwning();
    void colormap2ViewData();
    void colormap2Render();
    void colormap2AxisRescale();
    void colormap2ColorScaleSync();

private:
    QCustomPlot* mPlot = nullptr;
};
