#pragma once
#include "plottable.h"
#include "plottable1d.h"
#include "datasource/abstract-datasource.h"
#include "datasource/soa-datasource.h"
#include "datasource/async-pipeline.h"
#include "datasource/graph-resampler.h"
#include "plottable-draw-utils.h"
#include "../colorgradient.h"
#include <memory>
#include <span>
#include <QTimer>

template <typename T, typename Self = void>
class QCPGraph2Ops;

/*! \deprecated Use QCPMultiGraph with one column; this class goes in 3.0. */
class QCP_LIB_DECL QCPGraph2 : public QCPAbstractPlottable, public QCPPlottableInterface1D {
    Q_OBJECT
public:
    template <typename T>
    using OpsFor = QCPGraph2Ops<T>;


    enum LineStyle {
        lsNone,
        lsLine,
        lsStepLeft,
        lsStepRight,
        lsStepCenter,
        lsImpulse
    };
    Q_ENUM(LineStyle)

    explicit QCPGraph2(QCPAxis* keyAxis, QCPAxis* valueAxis);
    virtual ~QCPGraph2() override;

    // Data source
    void setDataSource(std::unique_ptr<QCPAbstractDataSource> source);
    void setDataSource(std::shared_ptr<QCPAbstractDataSource> source);
    [[nodiscard]] QCPAbstractDataSource* dataSource() const { return mDataSource.get(); }

    // Convenience: owning
    template <IndexableNumericRange KC, IndexableNumericRange VC>
    void setData(KC&& keys, VC&& values)
    {
        using KD = std::decay_t<KC>;
        using VD = std::decay_t<VC>;
        setDataSource(std::make_shared<QCPSoADataSource<KD, VD>>(
            std::forward<KC>(keys), std::forward<VC>(values)));
    }

    // Convenience: non-owning view from raw pointers
    template <typename K, typename V>
    void viewData(const K* keys, const V* values, int count)
    {
        setDataSource(std::make_shared<QCPSoADataSource<std::span<const K>, std::span<const V>>>(
            std::span<const K>(keys, count), std::span<const V>(values, count)));
    }

    // Convenience: non-owning view from spans
    template <typename K, typename V>
    void viewData(std::span<const K> keys, std::span<const V> values)
    {
        setDataSource(std::make_shared<QCPSoADataSource<std::span<const K>, std::span<const V>>>(
            keys, values));
    }

    void dataChanged();

    // Pipeline
    QCPGraphPipeline& pipeline() { return mPipeline; }
    const QCPGraphPipeline& pipeline() const { return mPipeline; }
    [[nodiscard]] bool hasRenderedRange() const { return mHasRenderedRange; }
    QPointF stallPixelOffset() const override;
    bool canProduceContent() const override;
    bool hasPendingData() const override { return mPendingSource != nullptr; }
    bool commitPendingData() override;

    // Line style
    [[nodiscard]] LineStyle lineStyle() const { return mLineStyle; }
    void setLineStyle(LineStyle style)
    {
        mLineStyle = style;
        markLinesDirty(); // the cached pixel lines and GPU geometry follow the style
    }

    // Scatter style
    [[nodiscard]] QCPScatterStyle scatterStyle() const { return mScatterStyle; }
    void setScatterStyle(const QCPScatterStyle& style) { mScatterStyle = style; }
    [[nodiscard]] int scatterSkip() const { return mScatterSkip; }
    void setScatterSkip(int skip) { mScatterSkip = qMax(0, skip); }

    // Per-point color axis for scatter rendering
    void setScatterColorValues(std::vector<float> values)
    {
        mScatterColorValues = std::move(values);
        markLinesDirty(); // coloured lines come from the raw data, with each point's index
    }
    void setScatterColorGradient(const QCPColorGradient& gradient);
    void clearScatterColorAxis() { mScatterColorValues.clear(); mScatterColorMapImage = {}; }

    // Scatter resampling: 0 = no limit (draw all points), >0 = random subsample to this count
    [[nodiscard]] int scatterMaxPoints() const { return mScatterMaxPoints; }
    void setScatterMaxPoints(int maxPoints) { mScatterMaxPoints = qMax(0, maxPoints); }

    // Adaptive sampling control
    [[nodiscard]] bool adaptiveSampling() const { return mAdaptiveSampling; }
    void setAdaptiveSampling(bool enabled)
    {
        if (mAdaptiveSampling == enabled)
            return;
        mAdaptiveSampling = enabled;
        markLinesDirty();
        mCachedLines.clear();
    }

    // Gap detection on the line (see QCPAbstractDataSource::setGapThreshold); 0 turns it off.
    [[nodiscard]] double gapThreshold() const { return mGapThreshold; }
    void setGapThreshold(double threshold)
    {
        if (mGapThreshold == threshold)
            return;
        mGapThreshold = threshold;
        markLinesDirty();
        mCachedLines.clear();
    }

    // QCPPlottableInterface1D
    [[nodiscard]] int dataCount() const override;
    [[nodiscard]] double dataMainKey(int index) const override;
    [[nodiscard]] double dataSortKey(int index) const override;
    [[nodiscard]] double dataMainValue(int index) const override;
    [[nodiscard]] QCPRange dataValueRange(int index) const override;
    [[nodiscard]] QPointF dataPixelPosition(int index) const override;
    [[nodiscard]] bool sortKeyIsMainKey() const override { return true; }
    QCPDataSelection selectTestRect(const QRectF& rect, bool onlySelectable) const override;
    [[nodiscard]] int findBegin(double sortKey, bool expandedRange = true) const override;
    [[nodiscard]] int findEnd(double sortKey, bool expandedRange = true) const override;

    // QCPAbstractPlottable
    QCPPlottableInterface1D* interface1D() override { return this; }
    double selectTest(const QPointF& pos, bool onlySelectable,
                      QVariant* details = nullptr) const override;
    QCPRange getKeyRange(bool& foundRange,
                         QCP::SignDomain inSignDomain = QCP::sdBoth) const override;
    QCPRange getValueRange(bool& foundRange,
                           QCP::SignDomain inSignDomain = QCP::sdBoth,
                           const QCPRange& inKeyRange = QCPRange()) const override;

protected:
    void draw(QCPPainter* painter) override;
    void drawLegendIcon(QCPPainter* painter, const QRectF& rect) const override;
    bool pipelineBusy() const override { return mPipeline.isBusy(); }
    void setPipelineScheduler(QCPPipelineScheduler* scheduler) override { mPipeline.setScheduler(scheduler); }
    void systemChanged() override { followAxes(); }

    void onViewportChanged();

private:
    std::shared_ptr<QCPAbstractDataSource> mDataSource;
    QCPGraphPipeline mPipeline;
    void followAxes();
    QList<QMetaObject::Connection> mAxisLinks;

    // Two-phase resampling: L1 built async, L2 computed lazily at draw time
    std::shared_ptr<qcp::algo::GraphResamplerCache> mL1Cache;
    std::shared_ptr<QCPAbstractDataSource> mL2Result;
    bool mNeedsResampling = false;
    bool mL2Dirty = false;

    // GPU translation fast path: axis ranges when data was last drawn fresh
    struct { QCPRange key, value; } mRenderedRange {};
    bool mHasRenderedRange = false;

    // Line cache: reuse across replots when viewport shift is small
    QVector<QPointF> mCachedLines;
    QVector<int> mCachedLineIndices; // data index of each cached point, with colour values
    void markLinesDirty();
    bool mLineCacheDirty = true;
    QSize mCachedPlotSize;
    // Cached extruded GPU vertices — avoids re-extrusion on pan
    qcp::ExtrusionCache mExtrusionCache;
    // Debounce timer: defers expensive L2 rebuild until panning stops
    QTimer mViewportDebounce;

    // Replacement data staged while the displayed source keeps rendering; the
    // plot commits it through commitPendingData() (see QCustomPlot::requestDataSwap).
    std::shared_ptr<QCPAbstractDataSource> mPendingSource;
    std::shared_ptr<qcp::algo::GraphResamplerCache> mPendingL1;
    bool mPendingReady = false;
    uint64_t mPendingGeneration = 0;
    // Generation floor below which a pipeline result is from a source that's
    // no longer displayed (superseded and already committed/applied) — set
    // whenever a source is installed, so a late result can't repopulate
    // mL1Cache/mL2Result for a source it was never computed for.
    uint64_t mAcceptedGeneration = 0;

    void applySourceNow(std::shared_ptr<QCPAbstractDataSource> source);
    void stagePendingSource(std::shared_ptr<QCPAbstractDataSource> source);
    void markPendingReady();
    void onL1Ready(uint64_t generation);
    void rebuildL2(const ViewportParams& vp);

    friend class TestPipeline;
    friend class TestDataSwap;
    friend class TestColorByScalar;

    LineStyle mLineStyle = lsLine;
    QCPScatterStyle mScatterStyle;
    int mScatterSkip = 0;
    int mScatterMaxPoints = 100'000;
    bool mAdaptiveSampling = true;
    double mGapThreshold = qcp::algo::kDefaultGapThreshold;

    // Color axis: normalized [0,1] per-point color values + pre-rendered 1D gradient
    std::vector<float> mScatterColorValues;
    QImage mScatterColorMapImage;

    // Reused across draw() calls to avoid per-frame heap allocations
    std::vector<int> mScatterSubset;
    std::vector<float> mScatterPts;
};

// The fluent handle of a QCPGraph2; see QCPPlottableOps for the subclass contract.
template <typename T, typename Self>
class QCPGraph2Ops : public QCPPlottableOps<T, qcp::OpsRet<Self, QCPGraph2Ops<T>>>
{
    using Base = QCPPlottableOps<T, qcp::OpsRet<Self, QCPGraph2Ops<T>>>;

public:
    using typename Base::Ret;
    using Base::Base;

    // Same arguments as QCPGraph2::setData: two numeric ranges, one point per index.
    template <IndexableNumericRange KC, IndexableNumericRange VC>
    Ret setData(KC&& keys, VC&& values)
    {
        return this->apply([&](T& p) { p.setData(std::forward<KC>(keys), std::forward<VC>(values)); });
    }
    Ret setDataSource(std::shared_ptr<QCPAbstractDataSource> source)
    {
        return this->apply([&](T& p) { p.setDataSource(std::move(source)); });
    }
    Ret setDataSource(std::unique_ptr<QCPAbstractDataSource> source)
    {
        return this->apply([&](T& p) { p.setDataSource(std::move(source)); });
    }
    Ret setLineStyle(QCPGraph2::LineStyle style)
    {
        return this->apply([&](T& p) { p.setLineStyle(style); });
    }
    Ret setScatterStyle(const QCPScatterStyle& style)
    {
        return this->apply([&](T& p) { p.setScatterStyle(style); });
    }
    Ret setScatterMaxPoints(int maxPoints)
    {
        return this->apply([&](T& p) { p.setScatterMaxPoints(maxPoints); });
    }
    Ret setGapThreshold(double threshold)
    {
        return this->apply([&](T& p) { p.setGapThreshold(threshold); });
    }
    Ret setScatterSkip(int skip)
    {
        return this->apply([&](T& p) { p.setScatterSkip(skip); });
    }
    Ret setAdaptiveSampling(bool enabled)
    {
        return this->apply([&](T& p) { p.setAdaptiveSampling(enabled); });
    }
    Ret setScatterColorValues(std::vector<float> values)
    {
        return this->apply([&](T& p) { p.setScatterColorValues(std::move(values)); });
    }
    Ret setScatterColorGradient(const QCPColorGradient& gradient)
    {
        return this->apply([&](T& p) { p.setScatterColorGradient(gradient); });
    }
};


