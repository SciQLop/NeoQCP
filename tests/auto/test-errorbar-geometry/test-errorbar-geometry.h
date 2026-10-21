#pragma once
#include <QtTest/QtTest>

class TestErrorBarGeometry : public QObject
{
    Q_OBJECT
private slots:
    void oneBarIsAStemAndTwoCaps();
    void nanBoundGivesAOneSidedBar();
    void twoNanPointsGiveNoBar();
    void missingValueStillDrawsBetweenBounds();
    void verticalKeyGivesHorizontalBars();
    void barsFitNeedsTheSpacing();
    void noInfinitiesEverReachTheRuns();
    void barsAreSeparatedByOneNanNotTrailing();
};
