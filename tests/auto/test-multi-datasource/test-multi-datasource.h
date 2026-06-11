#pragma once
#include <QtTest/QtTest>

class QCustomPlot;

class TestMultiDataSource : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void soaOwningVector();
    void soaViewSpan();
    void soaColumnCount();
    void soaKeyAccess();
    void soaValueAccess();
    void soaFindBeginEnd();
    void soaKeyRange();
    void soaValueRangePerColumn();
    void soaValueRangeWithKeyRestriction();
    void soaGetLines();
    void soaGetOptimizedLineData();
    void soaEmpty();
    void soaMixedTypes();

    // NaN gap handling
    void linesToPixelsNanProducesNanNotZero();
    void adaptiveSamplingNanDoesNotPoisonMinMax();
    void adaptiveSamplingInteriorNaNNoSpuriousBreaks();

    // Key-space gap detection
    void linesToPixelsBreaksAtKeyGaps();
    void resampledGetLinesBreaksAtKeyGaps();
    void adaptiveSamplingBreaksAtKeyGaps();

    // Row-major data source
    void rowMajorValueAt();
    void rowMajorWithPadding();
    void rowMajorGetLines();

    // Shape validation (degrade to empty, never UB/abort)
    void soaMismatchedColumnDegradesToEmpty();
    void rowMajorInvalidShapeDegradesToEmpty();

    // resampleL2Multi correctness
    void l2MultiBasicMinMax();
    void l2MultiNanSkipped();
    void l2MultiMultiColumnConsistency();
    void l2MultiSparseReturnNull();
    void l2MultiEmptyInput();

private:
    QCustomPlot* mPlot = nullptr;
};
