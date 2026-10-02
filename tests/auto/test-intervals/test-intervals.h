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

private:
    QCustomPlot* mPlot = nullptr;
};
