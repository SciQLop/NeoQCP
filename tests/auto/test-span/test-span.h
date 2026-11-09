#pragma once
#include <QtTest/QtTest>

class QCustomPlot;

class TestSpan : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void createAndRange();
    void orientationIsKept();
    void setRange();
    void invertedRange();
    void selectTestEdges_data();
    void selectTestEdges();
    void selectTestFill_data();
    void selectTestFill();
    void selectTestMiss_data();
    void selectTestMiss();
    void movableFlag();
    void drawDoesNotCrash();
    void drawOnLogAxis_data();
    void drawOnLogAxis();
    void exportFallbackRendersSpan_data();
    void exportFallbackRendersSpan();
    void dirtyTrackingReplots();
    void multiAxisRectSpans();
    void edgesHonourTheirPositionType_data();
    void edgesHonourTheirPositionType();
    void setRangeSlotDeletingTheSpanIsSafe();
    void dragSlotDeletingTheSpanIsSafe();
    void pixelModeDragMovesByPixels_data();
    void pixelModeDragMovesByPixels();
    void dataDragMovesByMouseDelta_data();
    void dataDragMovesByMouseDelta();
    void logAxisDragFollowsTheMouseInPixels_data();
    void logAxisDragFollowsTheMouseInPixels();
    void mixedEdgesEachMoveInTheirOwnUnit_data();
    void mixedEdgesEachMoveInTheirOwnUnit();
    void dragMovesAnEdge_data();
    void dragMovesAnEdge();
    void aSpanIsRecorded();
    void spanFollowsADataPanAndKeepsItsRatioSide_data();
    void spanFollowsADataPanAndKeepsItsRatioSide();
    void bothOrientationsMatchTheirReferences();

private:
    QCustomPlot* mPlot = nullptr;
};
