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

    // QCPGraph2 pipeline integration
    void graph2PipelinePassthrough();
    void graph2PipelineTransform();
    void graph2LinesFollowTheScaleOfANewKeyAxis();
    void graph2LinesFollowTheScaleOfANewValueAxis();
    void graph2LinesAreRebuiltWhenMovedToALogKeyAxis();
    void graph2LinesAreRebuiltWhenMovedToALogValueAxis();
    void graph2ResampleFollowsANewKeyAxis();

    // QCPColorMap2 pipeline integration
    void colormap2PipelineDefault();
    void colormap2PipelineResample();
    void colormap2FirstDataAfterManyPansWithNoSourceResamplesCorrectly();
    void colormap2DescendingYAxisResamplesNonEmpty();
    void colormap2DescendingYAxisLogScaleResamplesNonEmpty();

    // End-to-end
    void graph2DataFromExternalThread();
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
    void graphResamplerL2FollowsFallingData();
    void multiGraphL2FollowsFallingData();
    void orderPairsByTrendMovesOriginsWithValues();
    void graphResamplerCacheReuse();
    void graphResamplerNaNSkipped();
    void graphResamplerEmptyBinsProduceNaN();
    void graph2HierarchicalResamplingActivates();
    void graph2LineCacheRebuiltOnZoomWithBigData();
    void graph2LineCacheRebuiltOnQueuedZoomWithBigData();
    void graph2SmallDataNoResampling();
    void graph2LargeToSmallDataFallback();
    void graphResamplerBinMinMaxKeyPositions();
    void graphResamplerBinMinMaxZeroBins();
    void graphResamplerNonFiniteKeysSkipped();
    void graphResamplerParallelMatchesSingleThreaded();

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
    void graph2GapThresholdZeroDrawsIrregularSteps();
    void multiGraphGapThresholdZeroDrawsIrregularSteps();
    void graph2GapThresholdReachesResampledSource();
    void multiGraphGapThresholdReachesResampledSource();
    void graph2UniformDataNeverBreaksAtAnyZoom();
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
    void graph2L2RebuildDeferredToDraw();
    void graph2L2CoalescesMultipleViewportChanges();
    void graph2L2DirtyAfterL1Ready();

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
    void histogram2dNormalizationColumn();
    void histogram2dNormalizationToggleNoRebind();
    void histogram2dRenderSmokeTest();
    void histogram2dLogKeyBinScaleRebins();
    void histogram2dBinScaleDefaultsLinear();
    void histogram2dAxisScaleTogglesRebinning();
    void histogram2dLogValueAxisFromTheStartIsNotStale();
    void histogram2dStaleScaleIsReboundAgainAfterAQueuedRebin();
    void histogram2dExportDrawsTheNewBins();
    void histogram2dAdoptsTheRangeOfANewColorScale();
    void histogram2dContoursFollowTheCounts();
    void histogram2dContoursFollowTheNormalizedCounts();
    void histogram2dContourLabelsFollowAColourScaleToggle();
    void histogram2dChangingTheGradientRequestsARepaint();
    void histogram2dChangingTheDataRangeRequestsARepaint();
    void histogram2dChangingTheDataScaleTypeRequestsARepaint();
    void histogram2dChangingTheNormalizationRequestsARepaint();

    // Layer-level GPU translation
    void stallPixelOffsetGraph2Busy();
    void stallPixelOffsetIdleIsZero();
    void visibilityToggleForcesLayerRepaint();
    void layerPixelOffsetFromBusyChild();
    void layerPixelOffsetZeroWhenNoAsyncChildren();
    void layerTranslationClippedToAxisRect();
    void bufferedMainLayerRendersSameAsLogical();
    void existingGraph2TranslationUnaffected();
    void hiddenElementDoesNotBlockTranslation();
    void visibleElementStillBlocksTranslation();
    void colormapQuadOffsetTranslatesRect();
    void colormapQuadFollowsPanWhileTranslating();
    void plottableOffsetsRefreshedAtRenderTime();
    void pannedReuseKeepsBarsUnderTheirKeys();

    // GPU translation offset
    void viewportOffsetLinearHorizontal();
    void viewportOffsetLinearVertical();
    void viewportOffsetLogScale();
    void viewportOffsetNoChange();

    // Line caching
    void graph2LineCacheReusedOnSmallPan();
    void graph2LineCacheRebuiltOnLargePan();
    void graph2LineCacheSurvives75PercentPan();
    void graph2LineCacheRebuiltOnZoom();
    void graph2LineCacheRebuiltOnSmallZoom();
    void graph2LineCacheInvalidatedOnDataChange();
    void multiGraphLineCacheReusedOnSmallPan();

    // End-to-end fast pan
    void graph2FastPanNeverBlank();
    void multiGraphFastPanNeverBlank();

    // GPU translation fast path
    void graph2TranslationOffsetWhenBusy();
    void graph2TranslationResetsOnFreshData();
    void graph2PanIsRedrawnFreshAfterTheDebounce();
    void graph2PanIsRedrawnFreshWhileAnotherLayerIsDirty();
    void densePanIsRedrawnFreshBesideASmallGraph();
    void colormapNewDataShowsWhileAnotherLayerIsDirty();
    void graph2SetterRedrawsWhileAnotherLayerIsDirty();
    void logValueAxisPanTranslates();
    void logKeyAxisPanStaysCovered();
    void additiveShiftOnLogAxisIsNotATranslation();
    void lineCacheCoversADecadeEachSideOnLogKeys();
    void multiGraphTranslationOffsetWhenBusy();
    void colormap2TranslationOffsetWhenBusy();
    void colormap2ResultSurvivesNullViewport();
    void histogram2dTranslationOffsetWhenBusy();
    void translatedGeometryClippedToAxisRect();
    void multipleGraph2IndependentOffsets();
    void graph2DataChangedReplotSlotDeletingTheGraphIsSafe();

private:
    bool repaintFollows(QCPHistogram2D* hist, const std::function<void()>& change);
    QCustomPlot* mPlot = nullptr;
};
