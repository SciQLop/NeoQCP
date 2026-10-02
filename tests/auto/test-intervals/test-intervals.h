#pragma once
#include <QtTest/QtTest>

class QCustomPlot;

class TestIntervals : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void lanesAreAppendedInFirstSeenOrder();
    void bandsStackFromTheTopOfTheAxisRect();
    void hiddenLaneHasNoBand();
    void displayOrderReordersAndHides();
    void renameKeepsTheIndex();
    void laneAtInvertsLaneBand();
    void laneIndicesNotifiesOnce();

    void invalidColumnsAreReported();
    void groupByLaneSortsEachLane();
    void visibleRowsIncludesLongBarsStartingBeforeRange();
    void pixelBarsAreAtLeastOnePixelWide();
    void overlappingSameCategoryBarsMerge();
    void quadIsTwoTriangles();
    void millionIntervalsScanQuickly();

    void setDataRejectsInvalidColumns();
    void emptyDataDrawsNothing();
    void barsAreDrawnOnTheirLanes();
    void keyRangeSpansAllIntervals();
    void categoryColorFallsBackBeyondTheTable();
    void barsRebuildOnlyWhenTheViewChanges();
    void stripPlacementIsTranslucent();

    void labelIsDrawnOnlyWhenItFits();
    void stripDrawsLaneNamesOnce();
    void barsAndLabelsShowOnTheGpu();
    void mergedBarsGetNoLabel();
    void labelLayerFollowsThePlottablesLayer();

    void hitTestOnEmptyPlottable();
    void hitTestFindsBodyAndEdges();
    void narrowBarHasOnlyBody();
    void hiddenLaneIsNotHit();
    void clickSelectsAndCtrlClickToggles();
    void clickOnEmptyLaneClearsSelection();
    void shiftDragSelectsRowsInTheRect();
    void selectTestRectFindsRows();

    void moveShiftsBothEdges();
    void resizeNeverCrossesTheOtherEdge();
    void laneDeltaClampsAtEdges();
    void laneDeltaSkipsHiddenLanes();
    void snapToStepAlignsTheEdge();
    void snapToEdgesLandsOnTheClosestCandidate();

    void dragBodyEmitsOnceOnReleaseAndLeavesDataAlone();
    void clickWithoutMovingEmitsNothing();
    void dragRightEdgeResizes();
    void verticalDragChangesLaneOnlyWhenAllowed();
    void selectedIntervalsMoveTogether();
    void dragOnEmptyLaneCreates();
    void notEditableIgnoresDrags();
    void snapToEdgesLandsExactly();
    void snapToStepLandsOnMultiples();
    void setDataDuringDragCancelsGesture();
    void cursorFollowsThePart();

    void deleteKeyRequestsTheSelectedIds();
    void arrowsNudgeBySnapStepOrOnePixel();
    void upDownChangeLaneWhenAllowed();
    void escapeCancelsTheGesture();
    void keysAreNotConsumedWhenNotEditable();

    void setDataDropsSelectionOfVanishedIds();
    void setDataSelectionFollowsTheId();

    void clickReachesABarUnderASiblingsEmptySpace();
    void plottableAtPrefersABarOverASiblingsEmptySpace();
    void pressOnASiblingsBarStartsNoCreate();

    void verticalOnlyCreateEmitsNothing();
    void dragBackToTheStartEmitsNothing();
    void nudgeClampedAtTheTopLaneEmitsNothing();

    void deleteReachesEveryEditableTimeline();
    void hiddenTimelineDoesNotConsumeKeys();

    void createWithStepSnapsBothEnds();

    void firstVisibleTimelineDrawsLaneNames();

private:
    QCustomPlot* mPlot = nullptr;
};
