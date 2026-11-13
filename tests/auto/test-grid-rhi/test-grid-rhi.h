#pragma once
#include <QtTest/QtTest>

class QCustomPlot;

class TestGridRhi : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void gridRhiLayerCreatedLazily();
    void rasterExportOfAShownPlotKeepsGpuLinesAndFills();
    void layerReplotDoesNotStackGpuGeometry();
    void gradientGraphFillLooksTheSameOnScreenAndInExports();
    void dashedGridPenIsDashedOnScreen();
    void colorScaleTicksAreDrawnOnScreen();
    void thinLinesAreOneLogicalPixelOnScreen();
    void replotDoesNotCrash();
    void replotWithSubGridDoesNotCrash();
    void replotWithZeroLineDoesNotCrash();
    void replotWithLogAxisDoesNotCrash();
    void replotWithReversedAxisDoesNotCrash();
    void replotMultipleAxisRectsDoesNotCrash();
    void dirtyDetectionSkipsRebuildOnPan();
    void dirtyDetectionRebuildsOnTickChange();
    void dirtyDetectionRebuildsOnPenChange();
    void exportStillUsesQPainter();
    void noGpuLayerIsOfferedWhileExporting();
    void tickMarksFollowPanWithoutRebuild();
    void gridLinesStayAlignedWithTicksAtEpochScale();
    void deletedGridKeepsTickCache();

private:
    QCustomPlot* mPlot = nullptr;
};
