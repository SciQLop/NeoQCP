#ifndef TEST_ITEM_EVENTS_H
#define TEST_ITEM_EVENTS_H

#include <QtTest/QtTest>

class QCustomPlot;

class TestItemEvents : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void anAcceptedPressGrabsTheDragAndStopsThePan();
    void anIgnoredPressPans();
    void theDragKeepsThePressPart();
    void lastIsThePreviousEvent();
    void dataFollowsTheItemsOwnAxes_data();
    void dataFollowsTheItemsOwnAxes();
    void aDragOutsideThePlotStillEndsOnRelease();
    void aDragStartedByADoubleClickKeepsItsPart();
    void aNewPressReplacesTheGrabber();
    void aPressNothingAcceptsLeavesNoGrabber();
    void anItemDeletedInItsMoveHandler();
    void aPlotDeletedInAPressHandler();
    void spansStillDrag();

private:
    QCustomPlot* mPlot = nullptr;
};

#endif
