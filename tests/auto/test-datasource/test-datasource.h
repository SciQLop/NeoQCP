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
    void algoGapsIgnoreRepeatedKeys();

    // SoA data source tests
    void soaOwningVector();
    void soaViewSpan();
    void soaFindBeginEnd();
    void soaRangeQueries();
    void soaIntValues();
    void soaMismatchedLengthsDegradeToEmpty();
    void multiSoaGapThresholdZeroDisablesGaps();
    void linesInDataMapToGetLines();
    void thinnedLinesInDataMatchTheOptimizedPixels();
    void indicesFollowTheThinnedPoints();

    // one-column QCPMultiGraph integration tests
    void multiGraphCreation();
    void multiGraphSetDataOwning();
    void multiGraphViewData();
    void multiGraphSharedSource();
    void multiGraphAxisRanges();
    void multiGraphRender();
    void multiGraphLineStyles();
    void multiGraphScatterStyle();
    void multiGraphScatterSkip();
    void multiGraphLineStyleNoneWithScatter();

private:
    QCustomPlot* mPlot = nullptr;
};
