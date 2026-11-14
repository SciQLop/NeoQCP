#ifndef TEST_ALIGNMENT_GROUP_H
#define TEST_ALIGNMENT_GROUP_H

#include <QtTest/QtTest>

class TestAlignmentGroup : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void leftEdgesAlignAcrossPlots();
    void rightEdgesAlignWithAColourScale();
    void userColourScaleCellAligns();
    void stackedRectsInOnePlotAlign();
    void firstShowSettlesInTwoRepaints();
    void hiddenMemberIsLeftOut();
    void destroyedPlotLeavesItsGroup();
    void lastMemberLeavingDeletesTheGroup();
    void joiningAnotherGroupLeavesTheFirst();
    void refusedCreate();
    void settledStackRepaintsNoPeer();
    void aPeerMoveRepaintsNoOtherPlot();
    void aPeerHeightChangeRepaintsNoOtherPlot();
    void aPeerSidewaysMoveRealigns();
    void aContainerSidewaysMoveRealigns();
    void aWindowMoveRepaintsNoPlot();
    void aReparentedContainerIsHeard();
    void aPlotThatLeftItsGroupIgnoresItsContainer();
    void aPlotDeletedDuringItsContainerMoveIsSafe();
    void aContainerDeletedAfterItsMoveIsSafe();
    void anElementPutBackInItsLayoutIsWatched();
    void anAncestorThatBecameAWindowIsLeftAlone();
    void aWindowPutIntoAWidgetIsWatchedThere();
    void anAncestorsOtherFiltersHearItsReparent();
  void aMemberResizedRealigns();
    void sideBySideRectsAlignAndSettle();

    void anElementJoinsOneGroupPerSide();
    void joiningForSomeSidesKeepsTheOthers();
    void leavingOneGroupKeepsTheOther();
    void aDeletedGroupLeavesTheOtherSides();
    void aDeletedElementLeavesEveryGroup();
    void aBadSideHasNoGroup();
    void aHiddenMemberRepaintsNoVisiblePeer();
    void oneRenderTakesOneExtraPassThenNone();
    void anUnsettledLayoutStopsAndRepaintsLater();
    void aSideSwitchedToManualStopsAligning();
    void anOscillatingLayoutGoesQuiet();
    void aGroupDeletedByTheUserRelaysItsMembersOut();

    // The mixed-side margin-group layouts of 2.x, rebuilt with alignment groups (m5d B2).
    void jointDensityAlignsAfterOneRender();
    void colourScaleLayoutAlignsAfterOneRender();
    void advancedAxesAlignAfterOneRender();
    void stackedPairAlignsSidewaysAfterOneRender();
};

#endif // TEST_ALIGNMENT_GROUP_H
