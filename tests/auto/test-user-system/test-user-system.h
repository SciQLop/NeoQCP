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
    void barsBuiltOnASkewRectAreRefused();
    void anItemPositionOnASkewSystemHasNoProjection();
    void barsAreRefusedOnASkewRect();
    void aCurvePointLandsOnTheSkewMapping();
    void aCurveIsHitThroughTheMapping();
    void aCurveIsRectSelectedThroughTheMapping();
    void aShearChangeRepaints();
    void rescaleAxesFitsASkewCurve();
    void anAxisListsASkewItem();
    void anAxisRangeChangeRepaintsASkewCurve();
    void rescaleAxesFitsASkewGraph();
    void anAxisRangeChangeRepaintsASkewGraph();
    void aGraphLineFollowsTheSkewMapping();
    void stepStylesAreBuiltInDataSpace();
    void anImpulseRunsToValueZero();
    void aLogValueFillStopsAtTheRangeBound();
    void aNaNSplitsLineAndFill();
    void aScatterLandsOnTheMapping();
    void aGraphIsHitAndRectSelectedThroughTheMapping();
    void aShearedPointOutsideTheKeyWindowIsHit();
    void hoveringASkewGraphDoesNotCrash();
    void channelFillNeedsOneSystem();
    void aCartesianChannelFillAcrossValueAxesStillWorks();
};
