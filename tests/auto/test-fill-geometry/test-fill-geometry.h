#pragma once
#include <QtTest/QtTest>

class TestFillGeometry : public QObject
{
    Q_OBJECT
private slots:
    void twoRowsMakeOneQuad();
    void nanEdgeBreaksTheStrip();
    void breakBeforeBreaksTheStrip();
    void crossingEdgesAreSplitNotDoubleBlended();
    void verticalKeySwapsCoordinates();
    void binToSpansTakesMinAndMaxOfBothEdges();
    void binToSpansKeepsBreaks();
    void stripPolygonsOnePerRun();
    void stepLeftHoldsEachValueUntilTheNextKey();
    void stepRightAndCenterMatchTheLineHelpers();
    void steppedMovesBothEdgesAndKeepsBreaks_data();
    void steppedMovesBothEdgesAndKeepsBreaks();
};
