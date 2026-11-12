#pragma once

#include <QtTest/QtTest>

// A user's own coordinate system: what a curve, an item and an axis do on a skew rect. The rect and
// its system live in skew-system.h, which Tasks 2 and 3 reuse.
class TestUserSystem : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void theCapabilityMasks();
    void aGraphOnASkewRectHasNoProjection();
    void addGraphOnASkewRectJoinsIt();
    void addGraphOnABendRectIsRefused();
    void rectAddOfARefusedGraphLeavesNothingDangling();
    void aRefusedPlottableIsOnNoLayer();
    void plotAddOfRefusedBarsReturnsNoHandle();
    void adoptingBarsOntoASkewDefaultRectIsRefused();
    void aRefusedPlottableThatJoinsLaterIsRegistered();
    void aRefusedIntervalsLeavesNoLayerOrLegendRow();
    void aCustomSystemDeclaringARectangleStillRefusesBars();
    void barsBuiltOnASkewRectAreRefused();
    void anItemPositionOnASkewSystemHasNoProjection();
    void aDefaultItemOnASkewRectHasNoProjection();
    void interactiveCreationOnASkewRectLandsOnTheMapping();
    void anAnchoredPositionOnASkewSystemResolvesThroughTheFrame();
    void barsAreRefusedOnASkewRect();
    void errorBarsOnASkewRectAreRefused();
    void aCurvePointLandsOnTheSkewMapping();
    void aCurveIsHitThroughTheMapping();
    void aCurveIsRectSelectedThroughTheMapping();
    void aShearChangeRepaints();
    void rescaleAxesFitsASkewCurve();
    void anAxisListsASkewItem();
    void anAxisRangeChangeRepaintsASkewCurve();
    void rescaleAxesFitsASkewGraph();
    void aSkewGraphRescalesItsAxes();
    void anAxisRangeChangeRepaintsASkewGraph();
    void aGraphLineFollowsTheSkewMapping();
    void stepStylesAreBuiltInDataSpace();
    void anImpulseRunsToValueZero();
    void aLogImpulseIsDropped();
    void aLogValueFillStopsAtTheRangeBound();
    void aLogValueFillOnANegativeRangeClosesTowardZero();
    void aFillWithAnUnrepresentableInverseStaysFinite();
    void aNaNSplitsLineAndFill();
    void aSelectedChannelFillSegmentPaintsOnlyItsBand();
    void aChannelFillAcrossSystemsDrawsNoBand();
    void aCartesianChannelFillPartnerThatLostItsSystemDrawsNoBand();
    void aScatterLandsOnTheMapping();
    void aGraphIsHitAndRectSelectedThroughTheMapping();
    void aCurveHitMapsInOneBatch();
    void aShearedPointOutsideTheKeyWindowIsHit();
    void hoveringASkewGraphDoesNotCrash();
    void channelFillNeedsOneSystem();
    void aCartesianChannelFillAcrossValueAxesStillWorks();
    void aResizeMovesTheRevision();

    void anAreaIsNotAdoptedByAnotherPlot();
    void aNestedLayoutCannotMoveAnElementOfAnotherPlot();
    void itemsFollowTheSkewMapping();
    void theTracerFollowsASkewGraph();
    void theTracerFollowsAMultiGraphOnASkewRect();
    void takeAndAdoptAGraphAndAnItemOnASkewRect();
    void anAxisDeletedMidDragLeavesThemInert();
    void deletingTheSkewRectLeavesThemInert();
    void switchingBetweenCartesianAndSkewReRecords();
    void theLegendIconOfASkewGraphDraws();
    void aSkewItemIsAlwaysAnIndexCandidate();
    void aPolarGraphIsRectSelected();

    void aMultiGraphJoinsASkewRect();
    void aWaterfallIsRefusedOnASkewRect();
    void aMultiGraphLineFollowsTheSkewMapping();
    void aMultiGraphScatterLandsOnTheMapping();
    void aMultiGraphStepAndImpulseAreBuiltInDataSpace();
    void aLargeMultiGraphOnASkewRectDrawsEveryPoint();
    void switchingToASkewRectDropsTheBins();
    void aMultiGraphIsHitAndRectSelectedThroughTheMapping();
    void aMultiGraphDataPixelPositionIsTheMapping();
    void aShearChangeRedrawsAMultiGraph();
    void anAxisDeletedMidDragLeavesAMultiGraphInert();
    void aMultiGraphFillFollowsTheSkewMapping();
    void aMultiGraphFillBetweenColumnsOnASkewRect();
    void aFillToAnotherGraphOnASkewRect();
    void aMultiGraphFillOnALogAxisStopsAtTheBoundNearerZero();
    void aMultiGraphNaNSplitsLineAndFill();
    void multiGraphErrorBarCapsFollowTheMapping();
    void colourByScalarJoinsTheSkewRectsScale();
    void aMultiGraphColourWithoutAScaleIsRefused();
    void aDecoratedMultiGraphJoinsASkewRect();
};
