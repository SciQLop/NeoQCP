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
    void soaLinesInData();
    void soaThinnedLinesKeepAFewPoints();
    void soaEmpty();
    void soaMixedTypes();

    // NaN gap handling
    void linesInDataNanProducesNanNotZero();
    void adaptiveSamplingNanDoesNotPoisonMinMax();
    void adaptiveSamplingInteriorNaNNoSpuriousBreaks();

    // Key-space gap detection
    void linesInDataBreaksAtKeyGaps();
    void resampledLinesInDataBreaksAtKeyGaps();
    void adaptiveSamplingBreaksAtKeyGaps();
    void adaptiveSamplingFollowsFallingData();
    void adaptiveSamplingBreaksAtNaNRuns();
    void gapsBeforeMatchesTheLineBreaks();
    void gapsBeforeOffByZeroThreshold();
    void gapsBeforeRowMajor();
    void resampledGapsBeforeBreaksAtNaNBinsAndThresholdGaps();
    void gapsBeforeEqualsTheLineBreaksOfEverySource();

    // Row-major data source
    void rowMajorValueAt();
    void rowMajorWithPadding();
    void rowMajorLinesInData();
    void rowMajorExposesTypedRawColumns();
    void soaFloatExposesTypedRawColumn();
    void integerColumnHasNoRawColumn();
    void rowMajorFloatBinsLikeSoaDouble();

    // Shape validation (degrade to empty, never UB/abort)
    void soaMismatchedColumnDegradesToEmpty();
    void rowMajorInvalidShapeDegradesToEmpty();

    // resampleL2Multi correctness
    void l2MultiBasicMinMax();
    void l2MultiNanSkipped();
    void l2MultiMultiColumnConsistency();
    void l2MultiSparseReturnNull();
    void l2MultiEmptyInput();
    void genericLinesInDataHonoursGapThreshold();
    void growableCloneKeepsTheGapThreshold();

    // L2 decides from raw points, not L1 bins (bursty data)
    void l2MultiBinsRawPointsInsideBurst();
    void l2MultiCountsBurstInLineCacheMargin();
    void l2MultiFewRawPointsReturnNull();

private:
    QCustomPlot* mPlot = nullptr;
};
