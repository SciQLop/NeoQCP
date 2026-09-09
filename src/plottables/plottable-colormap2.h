#pragma once
#include "plottable.h"
#include <axis/axis.h>
#include <datasource/async-pipeline.h>
#include <datasource/soa-datasource-2d.h>
#include <painting/colormap-renderer.h>
#include <atomic>
#include <memory>
#include <span>
#include <QPen>

class QCPColorScale;
class QCPColorMapData;
class QCPColormapRhiLayer;

class QCP_LIB_DECL QCPColorMap2 : public QCPAbstractPlottable
{
    Q_OBJECT

public:
    explicit QCPColorMap2(QCPAxis* keyAxis, QCPAxis* valueAxis);
    ~QCPColorMap2() override;

    // Data source management
    void setDataSource(std::unique_ptr<QCPAbstractDataSource2D> source);
    void setDataSource(std::shared_ptr<QCPAbstractDataSource2D> source);
    [[nodiscard]] QCPAbstractDataSource2D* dataSource() const { return mDataSource.get(); }
    void dataChanged();

    // Owning setData
    template <IndexableNumericRange XC, IndexableNumericRange YC, IndexableNumericRange ZC>
    void setData(XC&& x, YC&& y, ZC&& z)
    {
        setDataSource(std::make_shared<QCPSoADataSource2D<
            std::decay_t<XC>, std::decay_t<YC>, std::decay_t<ZC>>>(
            std::forward<XC>(x), std::forward<YC>(y), std::forward<ZC>(z)));
    }

    // Non-owning views
    template <typename X, typename Y, typename Z>
    void viewData(const X* x, int nx, const Y* y, int ny, const Z* z, int nz)
    {
        setDataSource(std::make_shared<QCPSoADataSource2D<
            std::span<const X>, std::span<const Y>, std::span<const Z>>>(
            std::span<const X>{x, static_cast<size_t>(nx)},
            std::span<const Y>{y, static_cast<size_t>(ny)},
            std::span<const Z>{z, static_cast<size_t>(nz)}));
    }

    template <typename X, typename Y, typename Z>
    void viewData(std::span<const X> x, std::span<const Y> y, std::span<const Z> z)
    {
        setDataSource(std::make_shared<QCPSoADataSource2D<
            std::span<const X>, std::span<const Y>, std::span<const Z>>>(x, y, z));
    }

    // Properties
    void setGapThreshold(double threshold);
    [[nodiscard]] double gapThreshold() const { return mGapThreshold; }

    [[nodiscard]] QCPColorGradient gradient() const { return mRenderer.gradient(); }
    [[nodiscard]] QCPColorScale* colorScale() const { return mRenderer.colorScale(); }
    [[nodiscard]] QCPRange dataRange() const { return mRenderer.dataRange(); }
    [[nodiscard]] QCPAxis::ScaleType dataScaleType() const { return mRenderer.dataScaleType(); }
    void setDataScaleType(QCPAxis::ScaleType type);

    void setColorScale(QCPColorScale* colorScale);
    void rescaleDataRange(bool recalc = false);

    QCPColormapPipeline& pipeline() { return mPipeline; }
    const QCPColormapPipeline& pipeline() const { return mPipeline; }

    // Exposed for tests: the GPU quad layer backing this colormap when RHI
    // rendering is active (lazily created; null if RHI was never engaged).
    [[nodiscard]] QCPColormapRhiLayer* rhiLayer() { return mRenderer.ensureRhiLayer(); }

    // Pure-pan pixel offset of the last-rendered image vs the current axes, so
    // the compositor can translate the existing texture during a pan instead of
    // repainting + re-uploading every frame (skip-on-translate fast path).
    [[nodiscard]] QPointF stallPixelOffset() const override;
    [[nodiscard]] bool hasRenderedRange() const { return mHasRenderedRange; }

    // Contour overlay
    void setContourLevels(const QVector<double>& levels);
    [[nodiscard]] QVector<double> contourLevels() const { return mContourLevels; }

    // NOTE: only pen.color() reaches the GPU line renderer (baked into the
    // line geometry, always a fixed hairline regardless of pen width). The
    // full pen -- including width -- applies to the QPainter fallback used
    // for exports (pmNoCaching) and non-RHI compositing.
    void setContourPen(const QPen& pen);
    [[nodiscard]] QPen contourPen() const { return mContourPen; }

    // NOTE: stores the flag only -- no renderer currently draws contour labels,
    // so toggling this has no visible effect (known limitation, not yet implemented).
    void setContourLabelEnabled(bool enabled);
    [[nodiscard]] bool contourLabelEnabled() const { return mContourLabelEnabled; }

    void setAutoContourLevels(int count);
    [[nodiscard]] int autoContourLevelCount() const { return mAutoContourCount; }

    // Exposed for tests: the UV-space vertices built by the last contour
    // rebuild (after axis-reversal mirroring). Empty when contours are
    // inactive or the last rebuild was degenerate.
    [[nodiscard]] const QVector<float>& lastContourUv() const { return mLastContourUv; }
    // Exposed for tests: the levels the last rebuild marched (explicit
    // levels filtered to finite values, or resolved auto levels).
    [[nodiscard]] QVector<double> lastContourLevels() const { return mLastContourLevels; }
    // Exposed for tests: how many times the QPainter fallback segments were
    // (re)built. Repeated exports of one frame must not re-march the grid.
    [[nodiscard]] uint64_t fallbackBuildCount() const { return mFallbackBuildCount; }

public Q_SLOTS:
    void setGradient(const QCPColorGradient& gradient);
    void setDataRange(const QCPRange& range);

    double selectTest(const QPointF& pos, bool onlySelectable, QVariant* details = nullptr) const override;

    QCPRange getKeyRange(bool& foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth) const override;
    QCPRange getValueRange(bool& foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth,
                           const QCPRange& inKeyRange = QCPRange()) const override;

Q_SIGNALS:
    void dataRangeChanged(const QCPRange& newRange);
    void gradientChanged(const QCPColorGradient& newGradient);
    void dataScaleTypeChanged(QCPAxis::ScaleType newType);

protected:
    void draw(QCPPainter* painter) override;
    void drawLegendIcon(QCPPainter* painter, const QRectF& rect) const override;
    bool pipelineBusy() const override { return mPipeline.isBusy(); }
    bool canProduceContent() const override;
    // Releasing the RHI layer drops the uploaded contour lines with it; force
    // a rebuild on the next draw so the replacement layer gets them, instead
    // of trusting a cache stamp that predates the loss.
    void releaseGpuResources() override
    {
        mRenderer.releaseRhiLayer();
        mContourCacheGen = 0;
    }

private:
    void installResampleTransform();

    std::shared_ptr<QCPAbstractDataSource2D> mDataSource;
    // Captured by value in the pipeline transform (re-baked by setGapThreshold)
    // so background jobs never reference this object's memory.
    double mGapThreshold = 1.5;
    QCPColormapPipeline mPipeline;
    QCPColormapRenderer mRenderer;

    // Axis ranges at the last full draw — baseline for the pan translation offset.
    bool mHasRenderedRange = false;
    QCPRange mRenderedKeyRange, mRenderedValueRange;

    // Contour overlay
    QVector<double> mContourLevels;
    QPen mContourPen{Qt::white, 1.0};
    bool mContourLabelEnabled = false;
    int mAutoContourCount = 0;

    uint64_t mContourCacheGen = 0;
    uint64_t mContourDataGen = 0;
    // Generation the QPainter fallback segments were built for (0 = none).
    // Reset alongside the contour cache; matched against mContourDataGen so
    // repeated exports of one frame reuse the cached segments.
    uint64_t mFallbackGen = 0;
    uint64_t mFallbackBuildCount = 0;
    // Reversal state the cached UVs were built against (setRangeReversed
    // emits no signal, so draw() polls for changes to trigger a rebuild).
    bool mContourMirrorX = false;
    bool mContourMirrorY = false;
    QVector<float> mLastContourUv;
    QVector<double> mLastContourLevels;

    void invalidateContourCache();

    void onViewportChanged();
    void updateContourGpu(const QCPColorMapData* data);
    void updateContours(const QCPColorMapData* data, QCPPainter* painter,
                        bool imageWasInvalidated);
    void updateContourFallback(const QCPColorMapData* data);
    void clearContourState();
    [[nodiscard]] QVector<double> resolveContourLevels(const QCPRange& bounds) const;
};
