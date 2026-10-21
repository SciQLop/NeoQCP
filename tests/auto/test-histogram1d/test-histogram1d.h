#pragma once
#include <QtTest/QtTest>

class TestHistogram1D : public QObject
{
    Q_OBJECT
private slots:
    void columnsShareEdges();
    void nanAndInfinityAreSkipped();
    void valuesOutsideAFixedRangeAreNotCounted();
    void logBinsSkipNonPositiveValues();
    void flatDataGetsAVisibleBar();
    void normalizationSums();
    void densityUsesEachLogBinsOwnWidth();
    void emptyOrAllNanHasNoBounds();
    void edgesOnlyWiden();
    void noBinsGiveNothing();
    void valueAtTheUpperEdgeLandsInTheLastBin();
};
