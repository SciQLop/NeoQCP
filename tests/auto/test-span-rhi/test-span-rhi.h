#pragma once
#include <QtTest/QtTest>

class QCustomPlot;
class QCPItemVSpan;

class TestSpanRhi : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void firstDetectionReportsChange();
    void unchangedFrameReportsNoChange();
    void detectsEdgeMove();
    void detectsBrushChange();
    void detectsAxisRangeChange();
    void detectsAxisRectResize();
    void exportedSpanHasNoOutlineTheScreenLacks();
    void dashedSpanBorderIsDashedOnScreen();
    void gradientSpanBrushLooksTheSameOnScreen();
    void spanOnALowerLayerStaysUnderTheData();

private:
    QCustomPlot* mPlot = nullptr;
    QCPItemVSpan* mSpan = nullptr;
};
