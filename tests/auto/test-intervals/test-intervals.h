#pragma once
#include <QtTest/QtTest>

class QCustomPlot;

class TestIntervals : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void lanesAreAppendedInFirstSeenOrder();
    void bandsStackFromTheTopOfTheAxisRect();
    void hiddenLaneHasNoBand();
    void displayOrderReordersAndHides();
    void renameKeepsTheIndex();
    void laneAtInvertsLaneBand();

private:
    QCustomPlot* mPlot = nullptr;
};
