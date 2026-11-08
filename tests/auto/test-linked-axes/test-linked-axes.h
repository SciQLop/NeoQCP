#include <QtTest/QtTest>
#include "qcustomplot.h"

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
};
