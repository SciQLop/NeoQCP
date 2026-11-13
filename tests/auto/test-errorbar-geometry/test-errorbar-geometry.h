#pragma once
#include <QtTest/QtTest>

class TestErrorBarGeometry : public QObject
{
    Q_OBJECT
private slots:
    void nanBoundGivesAOneSidedBar();
    void twoNanPointsGiveNoBar();
    void missingValueStillDrawsBetweenBounds();
    void barsFitNeedsTheSpacing();
    void recordedErrorBarsMatchTheirReferences_data();
    void recordedErrorBarsMatchTheirReferences();
    void barsFollowTheirPlottablesData();
    void attachDetachReattach();
    void everyStyleSetterReRecords();
    void anInPlaceEditThroughDataIsDrawn();
    void aReplotWithoutADataAccessDoesNotReRecord();
    void swappingThePlottablesAxesReRecords();
    void aSameCountSetDataReRecords();
    void aDecoratorChangeReRecords();
    void clearingTheDecoratorWhileSelectedIsSafe();
    void aDecoratorScatterStyleReachesTheSelectedOutliers();
    void aSwappedPlottableContainerReRecords();
    void aCurveRecordsEveryBar();
    void aBackboneShorterThanTheGapIsNotDrawn();
    void aGroupedBarsErrorBarSitsOnItsOwnBar();
    void aStackedBarsErrorBarSitsOnTheTopOfTheStack();
    void theLegendIconIsRecorded();
};
