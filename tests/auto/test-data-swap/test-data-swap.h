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

    void multiGraphKeepsTranslationWhileDataPending();
    void multiGraphCommitsAfterWindow();
    void multiGraphFirstDataCommitsImmediately();
    void multiGraphLatestPendingWins();
    void multiGraphDataChangedWhilePendingKeepsDisplayedGeometry();
    void multiGraphPendingWiderSourceDrawsSafely();

    void graph2KeepsTranslationWhileDataPending();
    void graph2CommitsAfterWindow();
    void graph2DataChangedWhilePendingKeepsDisplayedGeometry();

private:
    QCustomPlot* mPlot = nullptr;
};
