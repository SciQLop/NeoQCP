#pragma once
#include <QtTest/QtTest>

class QCustomPlot;

class TestDataSource : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    // Algorithm tests
    void algoFindBegin();
    void algoFindEnd();
    void algoKeyRange();
    void algoValueRange();
    void algoValueRangeRestrictionMatchesBruteForce();
    void algoLinesToPixels();
    void algoOptimizedLineData();

    // SoA data source tests
    void soaOwningVector();
    void soaViewSpan();
    void soaFindBeginEnd();
    void soaRangeQueries();
    void soaIntValues();
    void soaMismatchedLengthsDegradeToEmpty();

    // QCPGraph2 integration tests
    void graph2Creation();
    void graph2SetDataOwning();
    void graph2ViewData();
    void graph2SharedSource();
    void graph2AxisRanges();
    void graph2Render();
    void graph2LineStyles();
    void graph2ScatterStyle();
    void graph2ScatterSkip();
    void graph2LineStyleNoneWithScatter();

private:
    QCustomPlot* mPlot = nullptr;
};
