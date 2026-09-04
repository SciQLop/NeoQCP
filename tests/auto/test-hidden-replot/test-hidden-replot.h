#pragma once
#include <QtTest/QtTest>

class QCustomPlot;

class TestHiddenReplot : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void hiddenReplotRunsByDefault();
    void hiddenReplotSkippedWhenEnabled();
    void replotResumesAfterDisable();

private:
    QCustomPlot* mPlot = nullptr;
};
