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
    void alignmentWinsOverMarginGroup();
};

#endif // TEST_ALIGNMENT_GROUP_H
