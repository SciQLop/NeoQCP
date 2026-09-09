#pragma once
#include "plottable.h"
#include "plottable1d.h"
#include "datasource/abstract-multi-datasource.h"
#include "datasource/soa-multi-datasource.h"
#include "datasource/row-major-multi-datasource.h"
#include "datasource/async-pipeline.h"
#include "datasource/graph-resampler.h"
#include "plottable-draw-utils.h"
#include <memory>
#include <span>
#include <QTimer>

struct QCP_LIB_DECL QCPGraphComponent {
    QString name;
    QPen pen;
    QPen selectedPen;
    QCPScatterStyle scatterStyle;
    QCPDataSelection selection;
    bool visible = true;
};

class QCP_LIB_DECL QCPMultiGraph : public QCPAbstractPlottable, public QCPPlottableInterface1D {
    Q_OBJECT
public:
    enum LineStyle { lsNone, lsLine, lsStepLeft, lsStepRight, lsStepCenter, lsImpulse };
    Q_ENUM(LineStyle)

    explicit QCPMultiGraph(QCPAxis* keyAxis, QCPAxis* valueAxis);
    ~QCPMultiGraph() override;

    // Data source
    void setDataSource(std::unique_ptr<QCPAbstractMultiDataSource> source);
    virtual void setDataSource(std::shared_ptr<QCPAbstractMultiDataSource> source);
    [[nodiscard]] QCPAbstractMultiDataSource* dataSource() const { return mDataSource.get(); }
    virtual void dataChanged();

    QCPMultiGraphPipeline& pipeline() { return mPipeline; }
    const QCPMultiGraphPipeline& pipeline() const { return mPipeline; }
    [[nodiscard]] bool hasRenderedRange() const { return mHasRenderedRange; }
    QPointF stallPixelOffset() const override;
    bool canProduceContent() const override;
    bool hasPendingData() const override { return mPendingSource != nullptr; }
    bool commitPendingData() override;

    // Convenience: owning
    template <IndexableNumericRange KC, IndexableNumericRange VC>
    void setData(KC&& keys, std::vector<VC>&& valueColumns)
    {
        using KD = std::decay_t<KC>;
        using VD = std::decay_t<VC>;
        setDataSource(std::make_shared<QCPSoAMultiDataSource<KD, VD>>(
            std::forward<KC>(keys), std::forward<std::vector<VC>>(valueColumns)));
    }

    // Convenience: non-owning spans (column-major / Fortran-order)
    template <typename K, typename V>
    void viewData(std::span<const K> keys, std::vector<std::span<const V>> valueColumns)
    {
        setDataSource(std::make_shared<
            QCPSoAMultiDataSource<std::span<const K>, std::span<const V>>>(
            keys, std::move(valueColumns)));
    }

    // Convenience: non-owning row-major (C-order) 2D array.
    // stride is in elements (not bytes).
    template <typename K, typename V>
    void viewRowMajorData(std::span<const K> keys, const V* values,
                           int columns, int stride)
    {
        setDataSource(std::make_shared<QCPRowMajorMultiDataSource<K, V>>(
            keys, values, static_cast<int>(keys.size()), columns, stride));
    }

    // Components
    [[nodiscard]] int componentCount() const { return mComponents.size(); }
    QCPGraphComponent& component(int index) { return mComponents[index]; }
    const QCPGraphComponent& component(int index) const { return mComponents[index]; }
    void setComponentNames(const QStringList& names);
    void setComponentColors(const QList<QColor>& colors);
    void setComponentPens(const QList<QPen>& pens);

    // Per-component value access (for tracers/tooltips)
    [[nodiscard]] double componentValueAt(int column, int index) const;

    // Shared style
    [[nodiscard]] LineStyle lineStyle() const { return mLineStyle; }
    void setLineStyle(LineStyle style) { mLineStyle = style; }
    [[nodiscard]] bool adaptiveSampling() const { return mAdaptiveSampling; }
    void setAdaptiveSampling(bool enabled) { if (mAdaptiveSampling != enabled) { mAdaptiveSampling = enabled; mLineCacheDirty = true; mCachedLines.clear(); } }
    [[nodiscard]] int scatterSkip() const { return mScatterSkip; }
    void setScatterSkip(int skip) { mScatterSkip = qMax(0, skip); }

    // Per-component selection
    [[nodiscard]] QCPDataSelection componentSelection(int index) const;
    void setComponentSelection(int index, const QCPDataSelection& sel);

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

    // Legend
    using QCPAbstractPlottable::addToLegend;
    using QCPAbstractPlottable::removeFromLegend;
    bool addToLegend(QCPLegend* legend) override;
    bool removeFromLegend(QCPLegend* legend) const override;

protected:
    void draw(QCPPainter* painter) override;
    void drawLegendIcon(QCPPainter* painter, const QRectF& rect) const override;

    void selectEvent(QMouseEvent* event, bool additive, const QVariant& details,
                     bool* selectionStateChanged) override;
    void deselectEvent(bool* selectionStateChanged) override;

    friend class TestMultiGraph;
    friend class TestPipeline;

protected:
    std::shared_ptr<QCPAbstractMultiDataSource> mDataSource;
    QVector<QCPGraphComponent> mComponents;
    mutable QVector<QCPDataSelection> mLastRectSelections; // per-component selections from selectTestRect
    LineStyle mLineStyle = lsLine;
    bool mAdaptiveSampling = true;
    int mScatterSkip = 0;
    QCPMultiGraphPipeline mPipeline;
    std::shared_ptr<qcp::algo::MultiGraphResamplerCache> mL1Cache;
    std::shared_ptr<QCPAbstractMultiDataSource> mL2Result;
    bool mL2Dirty = false;
    bool mNeedsResampling = false;
    struct { QCPRange key, value; } mRenderedRange {};
    bool mHasRenderedRange = false;
    // Line cache: per-component cached lines, reused with GPU offset
    QVector<QVector<QPointF>> mCachedLines;
    bool mLineCacheDirty = true;
    QSize mCachedPlotSize;
    // Per-component cached extruded GPU vertices — avoids re-extrusion on pan
    QVector<qcp::ExtrusionCache> mExtrusionCaches;
    // Debounce timer: defers expensive L2 rebuild until panning stops
    QTimer mViewportDebounce;

    // Replacement data staged while the displayed source keeps rendering; the
    // plot commits it through commitPendingData() (see QCustomPlot::requestDataSwap).
    std::shared_ptr<QCPAbstractMultiDataSource> mPendingSource;
    std::shared_ptr<qcp::algo::MultiGraphResamplerCache> mPendingL1;
    bool mPendingReady = false;
    uint64_t mPendingGeneration = 0;

    void applySourceNow(std::shared_ptr<QCPAbstractMultiDataSource> source);
    void stagePendingSource(std::shared_ptr<QCPAbstractMultiDataSource> source);
    void markPendingReady();
    void onL1Ready(uint64_t generation);
    void syncComponentCount(int newCount);
    void rebuildL2(const ViewportParams& vp);
    void onViewportChanged();
    bool pipelineBusy() const override { return mPipeline.isBusy(); }

    void updateBaseSelection();

};
