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

private:
    QCustomPlot* mPlot = nullptr;
};
