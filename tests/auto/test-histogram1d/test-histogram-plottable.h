#pragma once
#include <QtTest/QtTest>

class QCustomPlot;

class TestHistogramPlottable : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void binsInTheBackground();
    void preBinnedNeedsNoPipeline();
    void preBinnedWithBadEdgesIsIgnored();
    void autoEdgesStayWhenNewDataFitsInside();
    void autoEdgesWidenForDataOutside();
    void emptyDataAfterDataHasNoRangeAndNoCrash();
    void exportBinsSynchronously();
    void logValueAxisRangeSkipsZeroHeights();
    void allZeroHeightsHaveNoValueRange();
    void nonPositiveFixedRangeIsRejectedOnLogBins();
    void nonPositiveFixedRangeFallsBackToAutoWhenTheAxisTurnsLog();
    void rebinsForTheScaleOfANewKeyAxis();
    void rebinForANewScaleEmitsNothingDuringPaint();
    void exportRightAfterAScaleChangeBinsForTheNewScale();
    void queuedResetIsDroppedWhenTheScaleIsFreshAgain();
    void resultOfTheOldScaleIsNotShownUnderTheNewOne();
    void staleScaleIsResetOnceUnderAReplotStream();
    void autoEdgesNeverShrinkWhileJobsOverlap();
    void rebinningAfterOverlappingJobsShowsTheLastSource();
    void preBinnedBarsDropAnOlderSourcesResultAndRange();
    void resetEdgesDropsAResultBinnedBeforeTheReset();
    void clearingTheSourceClearsTheBars();
    void clearingTheSourceDropsAnInFlightResult();
    void exportBinningEmitsNothing();
    void exportWhileBinningShowsTheNewData();
    void barsAreDrawnUpToTheirHeight();
    void outlineRunsDoNotJoinBars();
    void stepOutlineIsTheTopProfile();
    void emptyBinsOnALogAxisStayOutOfTheOutline();
    void aHistogramIsNotSelectable();
    void barRectsRejectBadColumnsAndRaggedEdges();
    void logAxisMarksOnlyNonPositiveBarsAbsent();
    void stepOutlineBreaksAtAnAbsentBar();
    void gpuQuadsOfSeveralBarsDoNotJoin();
    void verticalKeyAxisGrowsBarsAlongX();
    void barsAreDrawnOnTheGpu();
    void emptyBinOnALinearAxisKeepsTheStepRun();
    void stepWithOneColumnIsTheOutlineOnly();
    void stepWithSeveralColumnsHasALightFill();
    void barsKeepATranslucentUserColour();
    void readySlotRemovingTheHistogramIsSafe();
    void edgesSlotRemovingTheHistogramIsSafe();
    void preBinnedReadySlotRemovingTheHistogramIsSafe();

private:
    QCustomPlot* mPlot = nullptr;
};
