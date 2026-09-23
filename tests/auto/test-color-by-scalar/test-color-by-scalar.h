#pragma once
#include <QtTest/QtTest>
#include <memory>
#include <vector>

class QCustomPlot;
class QCPAbstractMultiDataSource;

class TestColorByScalar : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void setLineStyleInvalidatesLineCache();

    void linesToPixelsIndexedMatchesPlainAndMarksGaps();
    void optimizedLineDataIndexedValuesComeFromTheirIndex();
    void indexedVerticalKeyAxis();
    void defaultIndexedImplementationMatchesSoA();
    void defaultIndexedImplementationHandlesAnEmptyWindow();
    void rowMajorIndexedMatchesSoA();

    void l1OriginPointsAtEachBinsExtremes();
    void l1WithoutOriginBuildsNone();
    void l1ParallelOriginEqualsSerial();
    void l2OriginComposesThroughL1AndCompacts();

    void stepIndexMapsFollowTheTransforms();

    void mapperBucketsLinearLogNaNAndGaps();
    void mapperGenerationBumpsOnEverySetter();
    void colorValuesOfTheWrongLengthAreRefused();
    void sameSizeDataRefreshKeepsColorValues();
    void inPlaceResizeAppliesTheColorSizeRule();
    void firstColoringRebuildsL1OnceWithOrigin();

    void deferredSetColorValuesUsesThePendingSourceSize();
    void deferredCommitAppliesTheColorSizeRule();
    void toBucketClampsHugeAndInfinitePositions();
    void wrongLengthOnColouredGraphLeavesItUncoloured();
    void uncolourRoutesDiscardAPendingStash();

    void colorRunsMergeEqualBucketsAndSkipGaps();
    void extrudedRunsCarryTheirColour();
    void coloredReextrusionOnlyOnColorChangeNotPan();
    void coloredLineRendersTheGradient();
    void coloredRunsMeetWithButtEndsOnExport();

    void coloredDashedLineRendersTheGradient();
    void coloredStepLineRendersTheGradient();
    void coloredImpulsesRenderTheGradient();
    void nanScalarLeavesAGap();

    void scatterEntriesKeepTheirOwnSizeAndMode();
    void coloredScatterStagesColoursPerMarker();
    void coloredMarkersTakeTheirPointsColour();

    void coloredLegendLineShowsTheGradient();

private:
    QCustomPlot* mPlot = nullptr;
    static std::shared_ptr<QCPAbstractMultiDataSource> makeSource(
        std::vector<double> keys, std::vector<std::vector<double>> columns);
};
