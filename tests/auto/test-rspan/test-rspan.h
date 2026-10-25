#pragma once
#include <QtTest/QtTest>

class QCustomPlot;

class TestRSpan : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void createAndRanges();
    void selectTestEdges();
    void selectTestFill();
    void drawDoesNotCrash();
    void exportFallbackRendersSpan();
    void dirtyTrackingReplots();
    void drawOnLogAxis();
    void edgesHonourTheirPositionType();
    void fillDragSlotDeletingTheSpanIsSafe();
    void pixelModeDragMovesByPixels();
    void dataDragMovesByMouseDelta();
    void logAxisDragFollowsTheMouseInPixels();
    void mixedEdgesEachMoveInTheirOwnUnit();

private:
    QCustomPlot* mPlot = nullptr;
};
