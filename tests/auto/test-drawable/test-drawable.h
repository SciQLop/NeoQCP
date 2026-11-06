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
    void everyFrameRecordingRebuildsOncePerFrame();
    void itemTextMeasuresOnceForItsQueries();
    void contentChangedInsideDrawIsAMisuse();
    void contentChangedInsideLegendIconIsAMisuse();
    void recordingFromInsideDrawIsAMisuse();
    void missingAxisHidesWithAMessage();

    void panReusesResolvedGeometry();
    void panBuildsNoFillEither();
    void panMovesAllDataEntriesByOneDelta();
    void dataOnlyDrawingLetsItsLayerTranslate();
    void translatedLayerRevealsAnEnteringLabel();
    void translatedLayerRevealsAClippedRaster();
    void translatedBarsRevealTheNextBar();
    void rasterPanKeepsImage();
    void recordedErrorBarsAreDrawn();
    void brokenDashedPipelineStillDraws();
    void brokenDashedPipelineKeepsTheOutline();
    void rasterisedDashesStopTheLayerFromTranslating();
    void zoomRebuildsOnce();
    void tenThousandBarsAreOneDraw();
    void gradientBarsStackAsOne();
    void fadeSplitsTheRun();
    void exportDoesNotPoisonTheResolveCache();
    void drawnPolygonStacksAboveAnEarlierGpuGraph();

    void defaultHitTestMatchesGeometry();
    void hitTestWithAVerticalKeyAxis();
    void hitTestWithAVerticalKeyAxisChecksEachSideOnItsOwnAxis();
    void hitTestBesideTextIsTheGap();
    void pixelAnchoredShapeIsHittable();
    void batchHitTestNamesTheRect();
    void defaultDataRangeFollowsTheExtentRules();
    void defaultLegendIconIsTheRecordingFitted();
    void flatLineHasALegendIcon();
    void legendIconFollowsTheTheme();
    void logAxisRescaleKeepsThePositivePart();
    void mixedCornerRectFollowsAPan();
    void anchoredShapeFollowsItsParent();
    void verticalSpanFollowsAnXPan();
    void dashedUnderOpaqueRectKeepsTheOrderOnAPan();
    void rasterUnderOpaqueRectKeepsTheOrderOnAPan();
    void setPenReRecordsOnce();
    void setCoordsMovesTheItem();
    void selectionChangeReRecords();
    void clickSelectsADrawnPlottable();
    void clickOutsideTheAxisRectMisses();
    void removedAnchorItemDoesNotDangle();
    void pannedTimeAxisKeepsTheShapeOnAxis();
    void rangeBandExampleDraws();
    void adapterExposesThePublicSelectAndRanges();
    void rangeBandRescalesToItsData();
    void rangeBandLegendIconIsInked();
    void exportStacksLikeTheScreen();
};
