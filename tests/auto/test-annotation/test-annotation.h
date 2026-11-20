#ifndef TEST_ANNOTATION_H
#define TEST_ANNOTATION_H

#include <QtTest/QtTest>

//! A text and the connector from it to a target: the box cut, the shrink at both ends, the head.
class TestAnnotation : public QObject
{
    Q_OBJECT
private slots:
    void trimCutsAtTheBoxAndShrinks();
    void theCutIsAtTheFirstExit();
    void aTargetInsideTheBoxDrawsNoConnector();
    void emptyTextStartsAtThePosition();
    void theTipEndsShrinkPixelsFromTheTarget();
    void theHeadPointsAlongThePath();
    void aPanMovesTheTipNotTheText();
    void aFarPixelTargetDrawsInBoundedTime();
    void aSelectedFarTargetDrawsInBoundedTime();
    void aLongArcTrimsInLinearTime();
    void changingTextFlagsRemeasuresTheText();
    void changingTextFlagsRemeasuresTheAnnotation();
    void aDefaultPenTakesTheThemeForeground();
    void aFarDataTargetRecordsNothing();
    void aLongStraightConnectorKeepsItsTwoEnds();
    void theTargetCutIsAtTheFirstApproach();
    void aTargetOutsideTheRectHidesTheAnnotation();
    void aTargetAtTheTopOfTheRangeStillDraws();
    void aTargetThreePixelsOutsideHides();
    void aTargetOnlyPartlyInDataIsNotHidden();
    void aLabelPastTheEdgeDrawsWhole();
    void theTextTakesTheThemeColour();
    void rotatedTextKeepsTheConnectorInScreenSpace();
    void theConnectorDrawsUnderTheText();
    void aClickOnTheConnectorSelects();
    void theShortcutAppliesEveryOption();
    void settersRecordAgain();
};

#endif // TEST_ANNOTATION_H
