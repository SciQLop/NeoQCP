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
};
