#include "qcustomplot.h"
#include <QtTest/QtTest>

class TestLinkedAxes : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void linkedRangeFollowsTheSource();
    void everyMutatorMovesTheSource();
    void writeBackSettles();
    void linkedPixelsMatchTheSourceOnOneRect();
    void linkOnAnotherRectUsesItsOwnLength();
    void decreasingLinkOrdersTheRange();
    void barsOnADecreasingLink();
    void nonFiniteInverseIsRefused();
    void reversingTheSourceRefreshesTheLink();
    void dataOnALinkedAxisMapsThroughTheLink();
    void severalAxesShareASource();
    void aLinksSourceCanBeLinkedAgain();
    void scaleTypeOnALinkedAxisLeavesTheSourceAlone();
    void refusedLinks();
    void deletedSourceLeavesAPlainAxis();
    void slotDeletingAnAxisDuringDrag();
    void aSlotDeletingTheLinkedAxisDuringAWriteBackIsSafe();
    void aSlotDeletingTheSourceDuringARefreshIsSafe();
    void reversingALinkedAxisToItsCurrentDirectionIsSilent();
    void aFullAxesBoxWithoutRangeSyncLeavesPlainAxes();
    void aSpanOnALinkedAxisLandsAtItsCoordToPixel();
    void aLinkedAxisGridLineLandsAtItsTick();
    void aVerticalLinkFollowsALogReversedSource();
    void rescaleFitsTheUnion();
    void aUnionUnderTheSharedScale();
    void aMappedFollowerDoesNotWiden();
    void anIdentityLinkSharesTheScale();
    void identityLinksFlatten();

    void rescaleLeavesTheDefaultMargin();
    void rescaleMarginOnALogAxisIsInDecades();
    void rescaleMarginOnANegativeLogAxis();
    void rescaleMarginZeroFitsExactly();
    void rescaleAxesMarginArgumentOverridesTheAxes();
    void aFollowerTakesItsMarginToItsSource();
    void linkingTakesTheSourcesMargin();
    void plottableRescaleOnAnIdentityFollowerUsesTheSourceMargin();
    void plottableRescaleOnlyEnlargeDoesNotGrow();
    void aNegativeOrNanMarginIsRefused();
    void constantDataTakesNoMargin();
};
