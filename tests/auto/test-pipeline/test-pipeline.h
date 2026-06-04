#pragma once
#include <QtTest/QtTest>

class QCustomPlot;

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

    // Pipeline base tests
    void pipelinePassthrough();
    void pipelineTransformRuns();
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

    // QCPColorMap2 pipeline integration
    void colormap2PipelineDefault();
    void colormap2PipelineResample();

    // End-to-end
    void graph2DataFromExternalThread();
    void colormap2DataFromExternalThread();

    // Race condition reproducers
    void pipelineSourceReplacedDuringJob();
    void pipelineRapidFireDeliverResult();

    // Graph resampler
    void graphResamplerBinMinMax();
    void graphResamplerLevel1AndLevel2();
    void graphResamplerCacheReuse();
    void graphResamplerNaNSkipped();
    void graphResamplerEmptyBinsProduceNaN();
    void graph2HierarchicalResamplingActivates();
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
    void multiGraphThresholdScalesWithColumnCount();
    void multiGraphRapidSetDataSource();
    void multiGraphExportSynchronousFallback();
    void multiGraphLogScaleFallback();
    void multiGraphDataChangedInvalidatesL1();
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

    // QCPHistogram2D
    void histogram2dPipelineBins();
    void histogram2dNormalizationColumn();
    void histogram2dNormalizationToggleNoRebind();
    void histogram2dRenderSmokeTest();

    // Layer-level GPU translation
    void stallPixelOffsetGraph2Busy();
    void stallPixelOffsetIdleIsZero();
    void layerPixelOffsetFromBusyChild();
    void layerPixelOffsetZeroWhenNoAsyncChildren();
    void layerTranslationClippedToAxisRect();
    void bufferedMainLayerRendersSameAsLogical();
    void existingGraph2TranslationUnaffected();

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
    void multiGraphTranslationOffsetWhenBusy();
    void colormap2TranslationOffsetWhenBusy();
    void colormap2ResultSurvivesNullViewport();
    void histogram2dTranslationOffsetWhenBusy();
    void translatedGeometryClippedToAxisRect();
    void multipleGraph2IndependentOffsets();

private:
    QCustomPlot* mPlot = nullptr;
};
