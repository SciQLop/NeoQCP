#pragma once

#include <QtTest/QtTest>

class TestDrawable : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void panDoesNotReRecord();
    void zoomDoesNotReRecord();
    void setterReRecordsOnce();
    void unrelatedMarkDoesNotReRecord();
    void viewEpochReRecordsEscapeHatch();
    void contentChangedInsideDrawIsAMisuse();
    void contentChangedInsideLegendIconIsAMisuse();
    void missingAxisHidesWithAMessage();

    void panReusesResolvedGeometry();
    void exportDoesNotPoisonTheResolveCache();
    void drawnPolygonStacksAboveAnEarlierGpuGraph();

    void defaultHitTestMatchesGeometry();
    void hitTestBesideTextIsTheGap();
    void pixelAnchoredShapeIsHittable();
    void defaultDataRangeFollowsTheExtentRules();
    void defaultLegendIconIsTheRecordingFitted();
    void panKeepsPixelAndRatioCornersInPlace();
    void setPenReRecordsOnce();
    void setCoordsMovesTheItem();
    void selectionChangeReRecords();
    void clickSelectsADrawnPlottable();
};
