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
    void crossingEdgesSplitAtTheCrossing();
    void edgesTouchingAtARowAreOneQuad();
    void thinStripsAreOneQuad();
    void binToSpansTakesMinAndMaxOfBothEdges();
    void binToSpansKeepsBreaks();
    void stripPolygonsOnePerRun();
    void aBrokenSecondRowDoesNotJoinTheFirst();
    void aSingleRowRunMakesNoPolygon();
    void binToSpansJoinsRowsOfALaterBin();
    void binToSpansPutsEachSpanAtItsBinCentre();
    void stepLeftHoldsEachValueUntilTheNextKey();
    void stepRightAndCenterMatchTheLineHelpers();
    void steppedMovesBothEdgesAndKeepsBreaks_data();
    void steppedMovesBothEdgesAndKeepsBreaks();
    void baselinePixelPastTheEdgeOnALogAxis();
    void baselinePixelOnALogAxisBelowOne();
};
