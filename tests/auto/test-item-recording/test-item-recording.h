#pragma once
#include <QtTest/QtTest>

//! The line-like items record their drawing, and the recording is what the references show.
class TestItemRecording : public QObject
{
    Q_OBJECT
private slots:
    void aLineWithEndingsRecords();
    void aStraightLineRecords();
    void aCurveRecords();
    void aBracketRecords();
    void aPannedPlainLineIsNotReRecorded();
    void aPannedArrowIsRightAfterThePan();
    void anAnchoredArrowFollowsThePan();
    void aRatioArrowFollowsTheAxisRect();
    void aFollowerFollowsItsAnchorItem();
    void aTracerOnAnotherLayerFollowsItsGraph();
    void anErrorBarOnAnotherLayerFollowsItsGraph();
    void aPixelOnlyItemDrawsWithoutAxes();
    void aDataItemWithoutAxesDrawsNothing();
    void aHugeDataEllipseExports();
    void everyStyleSetterReRecords();
    void aRectRecords();
    void anEllipseRecords();
    void aPixmapRecords();
    void aScaledPixmapRecords();
    void aRatioPixmapFollowsTheAxisRect();
    void aStaticPixmapIsConvertedOnce();
    void aTracerRecords();
    void aTracerFollowsItsGraph();
    void aPannedTracerIsNotReRecorded();
    void aTracerFollowsSetGraphKey();
    void aTracerFollowsInterpolation();
    void theTracerFollowsAMultiGraphComponent();
    void theTracerOutsideTheDataClampsAs2xDid();
    void theTracerOnAGapIsInert();
    void aTracerWhoseComponentDisappearsIsInert();
    void setComponentWithoutATargetDoesNothing();
    void deletingTheTracedMultiGraphLeavesTheTracerInert();
    void takingTheTracedMultiGraphLeavesTheTracerInert();
    void retargetingToAnotherGraphLeavesTheFirst();
    void theTracerOpsHandleSetsALineGraph();
    void theTracerRefusesAnOutOfRangeComponent();
    void aSwappedGraphContainerReRecords();
    void selectionReRecords();
};
