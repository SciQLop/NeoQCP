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
    void aGraphJoinedToASkewSystemIsRefused();
    void anItemPositionOnASkewSystemHasNoProjection();
    void barsAreRefusedOnASkewRect();
    void aCurvePointLandsOnTheSkewMapping();
    void aCurveIsHitThroughTheMapping();
    void aCurveIsRectSelectedThroughTheMapping();
    void aShearChangeRepaints();
    void rescaleAxesFitsASkewCurve();
    void anAxisListsASkewItem();
    void anAxisRangeChangeRepaintsASkewCurve();
};
