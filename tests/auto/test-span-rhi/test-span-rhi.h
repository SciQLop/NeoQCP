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

private:
    QCustomPlot* mPlot = nullptr;
    QCPItemVSpan* mSpan = nullptr;
};
