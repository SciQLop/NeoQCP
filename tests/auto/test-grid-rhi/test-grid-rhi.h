#pragma once
#include <QtTest/QtTest>

class QCustomPlot;

class TestGridRhi : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void gridRhiLayerCreatedLazily();
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
    void tickMarksFollowPanWithoutRebuild();
    void gridLinesStayAlignedWithTicksAtEpochScale();

private:
    QCustomPlot* mPlot = nullptr;
};
