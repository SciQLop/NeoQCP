#pragma once
#include <QtTest/QtTest>

class QCustomPlot;

class TestWaterfall : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void uniformOffset();
    void customOffset();
    void customOffsetFallback();

    void normalizationFactors();
    void normalizationDisabled();
    void invalidateNormalization();

    void adapterValueAt();
    void singleComponentValueRange();

    void getValueRangeNormalized();
    void getValueRangeUnnormalized();
    void getValueRangeSignDomain();

    void drawDoesNotCrash();

    // Snapshot semantics (C4/H6): parameters apply without waiting for a draw,
    // and concurrent data/parameter changes never race in-flight resample jobs.
    void parameterChangesApplyWithoutDraw();
    void concurrentParameterAndDataChangesAreSafe();
    void clearingSourceEmptiesTheGraph();

private:
    QCustomPlot* mPlot = nullptr;
};
