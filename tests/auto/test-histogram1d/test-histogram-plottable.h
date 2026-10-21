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
    void nonPositiveFixedRangeIsRejectedOnLogBins();
    void nonPositiveFixedRangeFallsBackToAutoWhenTheAxisTurnsLog();
    void autoEdgesNeverShrinkWhileJobsOverlap();
    void rebinningAfterOverlappingJobsShowsTheLastSource();
    void preBinnedBarsDropAnOlderSourcesResultAndRange();
    void resetEdgesDropsAResultBinnedBeforeTheReset();
    void exportBinningEmitsNothing();
    void barsAreDrawnUpToTheirHeight();
    void outlineRunsDoNotJoinBars();
    void stepOutlineIsTheTopProfile();
    void emptyBinsOnALogAxisStayOutOfTheOutline();
    void clickInsideABarHitsAndAboveItMisses();
    void barsAreDrawnOnTheGpu();
    void emptyBinOnALinearAxisKeepsTheStepRun();
    void stepWithOneColumnIsTheOutlineOnly();
    void stepWithSeveralColumnsHasALightFill();
    void barsKeepATranslucentUserColour();

private:
    QCustomPlot* mPlot = nullptr;
};
