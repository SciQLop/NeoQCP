#pragma once
#include <QtTest/QtTest>

class QCustomPlot;
class QCPItemSpan;

class TestSpanRhi : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void aRecordedSpanDrawsOnTheGpuPath();
    void exportedSpanHasNoOutlineTheScreenLacks();
    void dashedSpanBorderIsDashedOnScreen();
    void gradientSpanBrushLooksTheSameOnScreen();
    void spanOnALowerLayerStaysUnderTheData();

private:
    QCustomPlot* mPlot = nullptr;
    QCPItemSpan* mSpan = nullptr;
};
