#pragma once
#include <QtTest/QtTest>

class QCustomPlot;

class TestHSpan : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void createAndRange();
    void setRange();
    void invertedRange();
    void selectTestEdges();
    void selectTestFill();
    void selectTestMiss();
    void movableFlag();
    void drawDoesNotCrash();
    void drawOnLogAxis();
    void exportFallbackRendersSpan();
    void dirtyTrackingReplots();
    void edgesHonourTheirPositionType();
    void setRangeSlotDeletingTheSpanIsSafe();
    void dragSlotDeletingTheSpanIsSafe();
    void pixelModeDragMovesByPixels();

private:
    QCustomPlot* mPlot = nullptr;
};
