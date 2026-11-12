#pragma once
#include <QtTest/QtTest>
#include <functional>

class QCustomPlot;
class QCPHistogram2D;

class TestPipeline : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    // Scheduler tests
    void schedulerSubmitHeavy();
    void schedulerSubmitFast();
    void schedulerFastPriority();
    void schedulerOnSharedPoolWaitsForItsOwnJob();
    void schedulerOnSharedPoolDoesNotWaitForOthers();

    // Pipeline base tests
    void pipelinePassthrough();
    void pipelineTransformRuns();
    void theFirstJobReportsBusyThenIdleWhenItLands();
    void aBusySlotMayWaitForAnotherThreadUsingThePipeline();
    void theLatestResultIsNotBusyWhenItLands();
    void aDataChangeBumpedJustBeforeAResultLandsIsStillShown();
    void pipelineCoalescing();
    void pipelineViewportIndependentSkipsViewport();
    void pipelineViewportDependentRuns();
    void pipelineCachePreservedOnViewport();
    void pipelineCacheClearedOnDataChange();
    void pipelineInterimResult();
    void pipelineDestructionWhileRunning();

    // QCPMultiGraph pipeline integration
    void multiGraphPipelinePassthrough();

    // QCPColorMap2 pipeline integration
    void colormap2PipelineDefault();
    void colormap2PipelineResample();
    void colormap2FirstDataAfterManyPansWithNoSourceResamplesCorrectly();
    void colormap2DescendingYAxisResamplesNonEmpty();
    void colormap2DescendingYAxisLogScaleResamplesNonEmpty();

    // End-to-end
    void multiGraphDataFromExternalThread();
    void colormap2DataFromExternalThread();
    void colormap2ResamplesAgainAfterResize();
    void colormap2ResamplesForTheLogScaleOfItsKeyAxis();
    void colormap2ResamplesForTheScaleOfANewKeyAxis();
    void colormap2ResamplesForTheScaleOfANewValueAxis();
    void colormap2ResamplesForTheRangeOfANewKeyAxis();
    void colormap2ResamplesForTheRangeOfANewValueAxis();
    void colormap2ResamplesWhenMovedToALogKeyAxis();
    void colormap2ResamplesWhenMovedToALogValueAxis();
    void colormap2OnOneAxisForBothDirectionsListensOnce();

    // Race condition reproducers
    void pipelineSourceReplacedDuringJob();
    void pipelineRapidFireDeliverResult();
    void pipelineNullSourceWhileJobRunningResyncsGeneration();
    void schedulerDtorDropsQueuedJobs();
    void colormap2QueuedJobAfterDeleteDoesNotTouchFreedMemory();
    void colormap2GapThresholdBeforeDataDoesNotStickBusy();
    void colormap2SetNullSourceAfterDataDoesNotStickBusy();

    // Graph resampler
    void graphResamplerBinMinMax();
    void graphResamplerLevel1AndLevel2();
    void graphResamplerL2BinsRawPointsInsideBurst();
    void multiGraphL2FollowsFallingData();
    void orderPairsByTrendMovesOriginsWithValues();
    void graphResamplerCacheReuse();
    void graphResamplerNaNSkipped();
    void graphResamplerEmptyBinsProduceNaN();
    void multiGraphLineCacheRebuiltOnQueuedZoomWithBigData();
    void multiGraphLargeToSmallDataFallback();
    void graphResamplerBinMinMaxKeyPositions();
    void graphResamplerBinMinMaxZeroBins();
    void graphResamplerNonFiniteKeysSkipped();

    // Multi-column resampler
    void multiGraphBinMinMaxMulti();
    void multiGraphBinMinMaxMultiNaN();
    void multiGraphBinMinMaxMultiParallelMatchesSingleThreaded();
    void resampledMultiDataSourceInterface();
    void multiGraphL1AndL2();

    // QCPMultiGraph pipeline integration
    void multiGraphSmallDataNoResampling();
    void multiGraphLargeDataL1L2();
    void multiGraphLineCacheRebuiltOnZoom();
    void multiGraphGapThresholdZeroDrawsIrregularSteps();
    void multiGraphGapThresholdReachesResampledSource();
    void multiGraphUniformDataNeverBreaksAtAnyZoom();
    void multiGraphNaNHoleBreaksTheResampledLine();
    void multiGraphNaNHoleInOneColumnBreaksThatLine();
    void sporadicNaNNeverBreaksTheResampledLine();
    void multiGraphPendingSourceCommitsAfterZoomTriggeredRefetch();
    void multiGraphMultipleRapidLegitimateSupersessionsCommitLast();
    void multiGraphThresholdScalesWithColumnCount();
    void multiGraphRapidSetDataSource();
    void multiGraphExportSynchronousFallback();
    void multiGraphLogScaleFallback();
    void multiGraphDataChangedInvalidatesL1();
    void multiGraphDataChangedRejectsAStaleL1();
    void multiGraphReplacedBySmallSourceRejectsAStaleL1();
    void multiGraphHiddenComponentsStillResampled();

    // L2 lazy rebuild (deferred to draw)
    void multiGraphL2RebuildDeferredToDraw();
    void multiGraphL2CoalescesMultipleViewportChanges();
    void multiGraphL2DirtyAfterL1Ready();

    // Histogram binner
    void bin2dBasicCounts();
    void bin2dNaNSkipped();
    void bin2dEmptyInput();
    void bin2dSingleBin();
    void bin2dUnsortedKeyRange();
    void bin2dNaNAtKeyEndpoint();
    void bin2dAllNaNNoGrid();
    void bin2dLogKeyBinning();
    void bin2dLogDropsNonPositive();
    void bin2dLogAllNonPositiveNoGrid();

    // QCPHistogram2D
    void histogram2dKeyRangeUnsorted();
    void histogram2dValueRangeRestrictedUnsortedKeys();
    void histogram2dSelectTestReflectsNewData();
    void histogram2dPipelineBins();
    void histogram2dRebinsForTheScaleOfANewKeyAxis();
    void histogram2dRebinsForTheScaleOfANewValueAxis();
    void histogram2dRebinsForANewKeyAxisScaleWithoutADraw();
    void histogram2dRebinsForANewValueAxisScaleWithoutADraw();
    void histogram2dIgnoresTheScaleOfTheKeyAxisItLeft();
    void histogram2dIgnoresTheScaleOfTheValueAxisItLeft();
    void histogram2dNewBinsAreRepaintedWhenTheyLand();
    void histogram2dRebinForANewScaleEmitsNothingDuringPaint();
    void histogram2dExportRightAfterAScaleChangeShowsTheNewBins();
    void histogram2dExportRightAfterANewAxisShowsItsScale();
    void histogram2dQueuedRebinIsDroppedAfterAnExportRebinned();
    void histogram2dExportResultIsNotReplacedByAnOlderJob();
    void histogram2dWithNothingFiniteBinsOnce();
    void histogram2dDataWithNothingFiniteClearsTheOldCells();
    void histogram2dColumnNormalizedSumsEachKeyColumnToOne();
    void histogram2dColumnNormalizedDoesNotRebin();
    void histogram2dRenderSmokeTest();
    void histogram2dBinsFollowALogKeyAxis();
    void histogram2dBinCountsDefaultAndSettable();
    void histogram2dSaysReady();
    void bothHistogramsShareTheNormalization();
    void histogram2dAxisScaleTogglesRebinning();
    void histogram2dLogValueAxisFromTheStartIsNotStale();
    void histogram2dStaleScaleIsReboundAgainAfterAQueuedRebin();
    void histogram2dExportDrawsTheNewBins();
    void histogram2dAdoptsTheRangeOfANewColorScale();
    void histogram2dContoursFollowTheCounts();
    void histogram2dContoursFollowTheNormalizedCounts();
    void histogram2dContourLabelsFollowAColourScaleToggle();
    void histogram2dContoursMapALogKeyGeometrically();
    void reversedKeyAxisKeepsContourLabelsWithTheirLines();
    void zoomRebuildsTheContourLabelTile();
    void zoomKeepsTheContourRecordingWithoutLabels();
    void histogram2dChangingTheGradientRequestsARepaint();
    void histogram2dChangingTheDataRangeRequestsARepaint();
    void histogram2dChangingTheDataScaleTypeRequestsARepaint();
    void histogram2dChangingTheNormalizationRequestsARepaint();

    // Layer-level GPU translation
    void stallPixelOffsetMultiGraphBusy();
    void stallPixelOffsetIdleIsZero();
    void visibilityToggleForcesLayerRepaint();
    void layerPixelOffsetFromBusyChild();
    void layerPixelOffsetZeroWhenNoAsyncChildren();
    void layerTranslationClippedToAxisRect();
    void bufferedMainLayerRendersSameAsLogical();
    void existingMultiGraphTranslationUnaffected();
    void hiddenElementDoesNotBlockTranslation();
    void visibleElementStillBlocksTranslation();
    void colormapQuadFollowsPanWhileTranslating();
    void plottableOffsetsRefreshedAtRenderTime();
    void pannedReuseKeepsBarsUnderTheirKeys();

    // GPU translation offset
    void viewportOffsetLinearHorizontal();
    void viewportOffsetLinearVertical();
    void viewportOffsetLogScale();
    void viewportOffsetNoChange();

    // Line caching
    void multiGraphLineCacheRebuiltOnLargePan();
    void multiGraphLineCacheSurvives75PercentPan();
    void multiGraphSmallDataLineCacheRebuiltOnZoom();
    void multiGraphLineCacheRebuiltOnSmallZoom();
    void multiGraphLineCacheInvalidatedOnDataChange();
    void multiGraphLineCacheReusedOnSmallPan();

    // End-to-end fast pan
    void multiGraphFastPanNeverBlank();

    // GPU translation fast path
    void multiGraphTranslationResetsOnFreshData();
    void densePanIsRedrawnFreshBesideASmallGraph();
    void colormapNewDataShowsWhileAnotherLayerIsDirty();
    void multiGraphSetterRedrawsWhileAnotherLayerIsDirty();
    void logValueAxisPanTranslates();
    void logKeyAxisPanStaysCovered();
    void additiveShiftOnLogAxisIsNotATranslation();
    void lineCacheCoversADecadeEachSideOnLogKeys();
    void multiGraphTranslationOffsetWhenBusy();
    void colormap2TranslationOffsetWhenBusy();
    void colormap2ResultSurvivesNullViewport();
    void histogram2dTranslationOffsetWhenBusy();
    void translatedGeometryClippedToAxisRect();
    void multipleMultiGraphsIndependentOffsets();
    void multiGraphDataChangedReplotSlotDeletingTheGraphIsSafe();

private:
    bool repaintFollows(QCPHistogram2D* hist, const std::function<void()>& change);
    QCustomPlot* mPlot = nullptr;
};
