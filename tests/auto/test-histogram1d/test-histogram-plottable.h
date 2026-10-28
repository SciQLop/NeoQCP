#pragma once
#include <QtTest/QtTest>
#include <functional>

class QCustomPlot;
class QCPHistogram;

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
    void rebinsForANewKeyAxisScaleWithoutADraw();
    void theAxisItLeftIsNoLongerListenedTo();
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
    void barRectsOfAMissingColumnAreEmpty();
    void barRectsOfRaggedEdgesAreEmpty();
    void logAxisMarksOnlyNonPositiveOrNonFiniteBarsAbsent();
    void stepOutlineBreaksAtAnAbsentBar();
    void gpuQuadsOfBarsWithAGapStayInsideTheirBar();
    void emptyBinsOnALogAxisStayOutOfTheOutline();
    void aHistogramIsNotSelectable();
    void verticalKeyAxisGrowsBarsAlongX();
    void barsAreDrawnOnTheGpu();
    void emptyBinOnALinearAxisKeepsTheStepRun();
    void stepWithOneColumnIsTheOutlineOnly();
    void stepWithSeveralColumnsHasALightFill();
    void barsKeepATranslucentUserColour();
    void readySlotRemovingTheHistogramIsSafe();
    void edgesSlotRemovingTheHistogramIsSafe();
    void preBinnedReadySlotRemovingTheHistogramIsSafe();
    void logBinsHonourAFixedRangeStartingAtOne();
    void fixedRangeStartingAtZeroFallsBackWhenTheAxisTurnsLog();
    void flatDataGetsAWidenedRange();
    void aSingleBinCanBePreBinned();
    void preBinnedCountsNothing();
    void positiveValueRangeStartsAtTheSmallestHeight();
    void keyAxisScaleChangeRebinsWithoutADraw();
    void shownResultOfTheOtherScaleIsStaleEvenWhenTheTransformIsFresh();
    void logAxisFromTheStartIsNotStale();
    void normalizationIsAppliedToTheBars();
    void staleScaleIsResetAgainAfterAJobDelivered();
    void resultOfTheOldScaleIsResetWithoutADraw();
    void nonPositiveBinCountsAreIgnored();
    void theSameBinCountDoesNotRebin();
    void changingTheStyleRequestsARepaint();
    void changingTheColoursRequestsARepaint();
    void newBarsRequestARepaint();
    void exportOfPreBinnedBarsDoesNotRebin();
    void exportOfPreBinnedBarsOnAStaleScaleQueuesOneReset();
    void exportOfAStaleScaleForgetsTheAutoRange();
    void emptyHistogramHasNoContent();
    void withoutBarsThereIsNoKeyRange();
    void noPositiveEdgeMeansNoPositiveKeyRange();
    void keyRangeSpansTheEdges();
    void valueRangeOfSmallHeightsStartsAtZero();
    void barsHaveADarkerOutlineAroundTheBase();
    void stepOutlineIsOnePixelThick();
    void stepOutlineHasNoBase();
    void fillHasNoBorderWhateverPenThePainterHad();
    void anAlmostTransparentFillStillPaints();
    void gpuBarsHaveFillAndOutline();
    void gpuBarsAreNotShifted();
    void gpuStepOfOneColumnHasOnlyTheOutline();
    void gpuAnAlmostTransparentFillStillDraws();

private:
    [[nodiscard]] bool repaintFollows(QCPHistogram* h, const std::function<void()>& change);
    QCustomPlot* mPlot = nullptr;
};
