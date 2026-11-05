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
    void themeReaderReRecordsOnThemeChange();
    void transformReaderReRecordsOnEveryPan();
    void contentChangedInsideDrawIsAMisuse();
    void contentChangedInsideLegendIconIsAMisuse();
    void missingAxisHidesWithAMessage();

    void panReusesResolvedGeometry();
    void panBuildsNoFillEither();
    void panMovesAllDataEntriesByOneDelta();
    void zoomRebuildsOnce();
    void tenThousandBarsAreOneDraw();
    void fadeSplitsTheRun();
    void exportDoesNotPoisonTheResolveCache();
    void drawnPolygonStacksAboveAnEarlierGpuGraph();

    void defaultHitTestMatchesGeometry();
    void hitTestBesideTextIsTheGap();
    void pixelAnchoredShapeIsHittable();
    void defaultDataRangeFollowsTheExtentRules();
    void defaultLegendIconIsTheRecordingFitted();
    void flatLineHasALegendIcon();
    void logAxisRescaleKeepsThePositivePart();
    void mixedCornerRectFollowsAPan();
    void anchoredShapeFollowsItsParent();
    void verticalSpanFollowsAnXPan();
    void setPenReRecordsOnce();
    void setCoordsMovesTheItem();
    void selectionChangeReRecords();
    void clickSelectsADrawnPlottable();
    void clickOutsideTheAxisRectMisses();
    void removedAnchorItemDoesNotDangle();
    void pannedTimeAxisKeepsTheShapeOnAxis();
    void rangeBandExampleDraws();
};
