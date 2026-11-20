#pragma once
#include <QtTest/QtTest>

class QCustomPlot;
class QCPGraph;

class TestFillBetween : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void aFillTakesItsBrushBySide();
    void oneBrushFillsBothSides();
    void aNoBrushSideStaysEmpty();
    void aFillFollowsItsLinesData();
    void aFillFollowsALineThatShrinks();
    void aGapBreaksTheFill();
    void aFillBetweenTwoComponents();
    void aFillOnALogAxisClosesAtTheBound();
    void aFillDrawsUnderItsLines();
    void aFillWhoseLineIsDeletedDrawsNothing();
    void aFillDrawsAgainWhenSetLinesNamesLiveLines();
    void linesOfTwoRectsAreRefused();
    void aFillOfLinesOnAnotherKeyAxisIsReported();
    void aFillOfALineWithItselfIsReported();
    void aFillOfAMissingComponentIsReported();
    void aFillIsNotSelectableByDefault();
    void aFillAcrossTwoValueAxesFollowsTheSecondAxis();
    void aDenseFillIsRebinnedAfterAZoom();
    void aSteppedFillSwapsColourWhereTheLinesSwap();
    void aFillsLegendIconShowsItsBrushes();
    void aCartesianFillIsNotReRecordedByAPan();
    void aFillMadeBeforeTheDataDrawsWhenItComes();
    void aFillOfAColumnThatDisappearsDrawsNothing();

private:
    QCustomPlot* mPlot = nullptr;
    QCPGraph* mFirst = nullptr;
    QCPGraph* mSecond = nullptr;
};
