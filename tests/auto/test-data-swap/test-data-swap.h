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

private:
    QCustomPlot* mPlot = nullptr;
};
