#pragma once
#include <QtTest/QtTest>

class QCustomPlot;

class TestDataSwap : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void pendingCountsAsBusy();
    void requestsWithinWindowCommitOnce();
    void requestAfterWindowCommitsAgain();
    void swapWaitsForPanToSettle();
    void swapCapForcesCommitWhilePanning();

    void newDataIsSeenAtOnceByDefault();
    void rescaleAxesSeesNewDataAtOnce();
    void deferredSwapKeepsTheOldDataUntilCommit();
    void turningTheDeferredSwapOffAppliesThePendingData();

    void multiGraphKeepsTranslationWhileDataPending();
    void multiGraphCommitsAfterWindow();
    void multiGraphFirstDataCommitsImmediately();
    void multiGraphLatestPendingWins();
    void multiGraphDataChangedWhilePendingKeepsDisplayedGeometry();
    void multiGraphPendingWiderSourceDrawsSafely();
    void multiGraphSmallPendingIgnoresLateLargeJob();
    void multiGraphLateLargeJobDoesNotContaminateCommittedSmallSource();

    void noCommitNoReplot();
    void commitRemovingAPlottableStillCommitsTheOthers();


private:
    QCustomPlot* mPlot = nullptr;
};
