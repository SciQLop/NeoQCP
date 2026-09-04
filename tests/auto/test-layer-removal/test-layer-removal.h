#pragma once
#include <QtTest/QtTest>

class QCustomPlot;

class TestLayerRemoval : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void removeLayerThenReplotDoesNotCrash();
    void removeAxisThenReplotDoesNotCrash();

private:
    QCustomPlot* mPlot = nullptr;
};
