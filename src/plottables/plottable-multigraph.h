#pragma once
#include "../drawable.h"
#include "datasource/abstract-multi-datasource.h"
#include "datasource/async-pipeline.h"
#include "datasource/graph-resampler.h"
#include "datasource/row-major-multi-datasource.h"
#include "datasource/soa-multi-datasource.h"
#include "multigraph-errorbars.h"
#include "multigraph-fill.h"
#include "plottable-color-mapper.h"
#include "plottable-draw-utils.h"
#include "plottable-line.h"
#include "plottable.h"
#include "plottable1d.h"
#include <QTimer>
#include <array>
#include <atomic>
#include <memory>
#include <optional>
#include <span>
#include <variant>
#include <vector>

class QCPPlottableRhiLayer;

template <typename T, typename Self = void>
class QCPMultiGraphOps;

class QCP_LIB_DECL QCPMultiGraph : public QCPDrawn<QCPAbstractLine>, public QCPPlottableInterface1D
{
    Q_OBJECT
public:
    template <typename T>
    using OpsFor = QCPMultiGraphOps<T>;

    explicit QCPMultiGraph(QCPAxis* keyAxis, QCPAxis* valueAxis);
    ~QCPMultiGraph() override;

    // Data source
    void setDataSource(std::unique_ptr<QCPAbstractMultiDataSource> source);
    virtual void setDataSource(std::shared_ptr<QCPAbstractMultiDataSource> source);
    [[nodiscard]] QCPAbstractMultiDataSource* dataSource() const { return mDataSource.get(); }
    virtual void dataChanged();

    QCPMultiGraphPipeline& pipeline() { return mPipeline; }
    const QCPMultiGraphPipeline& pipeline() const { return mPipeline; }

    [[nodiscard]] bool hasRenderedRange() const { return mCore.hasRecording(); }

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

    struct Fill {
        int owner = -1;
        std::variant<double, int> target;   // a constant, or another column
        QBrush brush;                       // Qt::NoBrush = owner pen colour at alpha 64
    };
    //! Fills between a column and a constant. False (nothing changed) for an unknown column.
    bool setComponentFill(int owner, double value, const QBrush& brush = Qt::NoBrush);
    //! Fills between two columns. One fill per owner: setting again replaces it.
    bool setComponentFillBetween(int owner, int target, const QBrush& brush = Qt::NoBrush);
    void setComponentFillBrush(int owner, const QBrush& brush);
    void clearComponentFill(int owner);
    [[nodiscard]] std::optional<Fill> componentFill(int owner) const;
    //! Whether a decoration may name \a column: any column >= 0 before the first data, the
    //! data's columns after.
    [[nodiscard]] bool isValidColumn(int column) const;
    [[nodiscard]] QBrush effectiveFillBrush(const Fill& fill) const;

    struct ErrorBars {
        int value = -1, lower = -1, upper = -1;
        std::optional<QPen> pen;      // none = the value component's pen
        double capWidthPx = 6;        // full cap width
    };
    //! Bars from `lower` to `upper` at each row of `value`; a band when rows are too dense.
    //! One per value column: setting again replaces it. Takes the bounds out of the legend;
    //! clearing puts back those no other bars use. False (nothing changed) for an unknown or
    //! repeated column.
    bool setComponentErrorBars(int value, int lower, int upper, double capWidthPx = 6,
                               std::optional<QPen> pen = std::nullopt);
    void clearComponentErrorBars(int value);
    [[nodiscard]] std::optional<ErrorBars> componentErrorBars(int value) const;

    // Per-component value access (for tracers/tooltips)
    [[nodiscard]] double componentValueAt(int column, int index) const;

    [[nodiscard]] bool adaptiveSampling() const { return mAdaptiveSampling; }
    void setAdaptiveSampling(bool enabled);
    // Gap detection on the lines (see QCPAbstractMultiDataSource::setGapThreshold); 0 turns it off.
    [[nodiscard]] double gapThreshold() const { return mGapThreshold; }
    void setGapThreshold(double threshold);

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
    //! Draws one component's legend line: its pen, or, when the graph is coloured, the pen
    //! with a gradient brush along `line` sampled from the colour mapper.
    void drawComponentLegendIcon(QCPPainter* painter, int component, const QLineF& line) const;

protected:
    //! Records the lines and the markers: the pixels come from the replay.
    void draw(QCPDrawContext& context) const override;
    //! The data point under \a at: today's selectTest, without the gates around it.
    std::optional<double> hitTest(QCP::Pixels at, QVariant* details) const override;

    //! The lines, markers, fills or error bars changed: a counting bump, not a rebuild.
    quint64 dependencyRevision() const override { return mRevision; }

    //! Without a finished resample an export resamples on the spot: the two recordings differ.
    bool resolvesForTheExport() const override { return true; }

    void drawLegendIcon(QCPPainter* painter, const QRectF& rect) const override;
    void axesChanged() override { followAxes(); }

    void selectEvent(QMouseEvent* event, bool additive, const QVariant& details,
                     bool* selectionStateChanged) override;
    void deselectEvent(bool* selectionStateChanged) override;

    friend class TestMultiGraph;
    friend class TestPipeline;
    friend class TestDataSwap;
    friend class TestColorByScalar;

protected:
    std::shared_ptr<QCPAbstractMultiDataSource> mDataSource;
    struct FillEntry {
        Fill spec;
        mutable std::vector<float> verts; // triangle cache, rebuilt when lines change
    };
    QVector<FillEntry> mFills;
    struct ErrorBarsEntry {
        ErrorBars spec;
        mutable qcp::ExtrusionCache bars;
        mutable QVector<QPointF>
            runs; // pixel runs at the rendered range, re-extruded on a pen change
        mutable std::vector<float> band; // triangle cache of the dense fallback
        mutable bool asBand = false; // mode picked on the last fresh frame, kept on pan frames
    };
    QVector<ErrorBarsEntry> mErrorBars;
    mutable QVector<QCPDataSelection> mLastRectSelections; // per-component selections from selectTestRect
    bool mAdaptiveSampling = true;
    double mGapThreshold = qcp::algo::kDefaultGapThreshold;
    // draw() is const: what it resolves for this frame is mutable state, not cached pixels.
    mutable QCPMultiGraphPipeline mPipeline;
    mutable std::shared_ptr<qcp::algo::MultiGraphResamplerCache> mL1Cache;
    mutable std::shared_ptr<QCPAbstractMultiDataSource> mL2Result;
    mutable bool mL2Dirty = false;
    bool mNeedsResampling = false;
    //! The data the recording was made from changed: the adapter records it again.
    mutable quint64 mRevision = 0;
    // Debounce timer: defers expensive L2 rebuild until panning stops
    QTimer mViewportDebounce;

    // Read by the L1 transform on the pipeline thread: build origin tables only for coloured
    // graphs.
    std::shared_ptr<std::atomic<bool>> mWantOrigin = std::make_shared<std::atomic<bool>>(false);
    mutable bool mL2HasOrigin = false;
    // The lines this object draws from changed: bump the revision, paint the layer again.
    void linesChanged();
    // The shared line base's hooks: every drawing change also drops the cached lines.
    void lineChanged() override;
    void componentVisibilityToggled() override;
    void pensChanged(int componentCount) override;
    void applyBrushToComponent(int index, const QBrush& brush) override;
    [[nodiscard]] const QCPAbstractMultiDataSource* colorValueSource() const override;
    [[nodiscard]] bool hasPendingDataSource() const override;
    void colorValuesStarted() override;
    // Source indices aligned with the line-style-transformed points of a component.
    static QVector<int> lineIndices(LineStyle style, const QVector<int>& dataIdx);
    // The nearest data point of any visible component: what selectTest and hitTest both answer.
    std::optional<double> nearestDataPoint(const QPointF& pos, QVariant* details) const;
    void requestOrigin();
    // Same-size-keeps/other-size-clears colour rule, shared by applySourceNow(),
    // commitPendingData() and dataChanged(): each installs or mutates mDataSource and
    // must apply it identically. Resets mWantOrigin when the graph ends up uncoloured.
    void applyColorSizeRule(int newSize);

    // Replacement data staged while the displayed source keeps rendering; the
    // plot commits it through commitPendingData() (see QCustomPlot::requestDataSwap).
    std::shared_ptr<QCPAbstractMultiDataSource> mPendingSource;
    std::shared_ptr<qcp::algo::MultiGraphResamplerCache> mPendingL1;
    bool mPendingReady = false;
    uint64_t mPendingGeneration = 0;
    // Generation floor below which a pipeline result is from a source that's
    // no longer displayed (superseded and already committed/applied) — set
    // whenever a source is installed, so a late result can't repopulate
    // mL1Cache/mL2Result for a source it was never computed for.
    uint64_t mAcceptedGeneration = 0;

    void applySourceNow(std::shared_ptr<QCPAbstractMultiDataSource> source);
    void stagePendingSource(std::shared_ptr<QCPAbstractMultiDataSource> source);
    void markPendingReady();
    void onL1Ready(uint64_t generation);
    void syncComponentCount(const QCPAbstractMultiDataSource* source);
    void dropDecorationsBeyond(int columnCount);
    void hideErrorBound(int column);
    void releaseErrorBounds(const ErrorBars& removed);
    bool setFill(Fill fill);
    [[nodiscard]] bool isErrorBound(int column) const;
    void decorationsChanged();
    [[nodiscard]] bool hasPenDerivedDecoration(int ownerCount) const;
    [[nodiscard]] bool fillDrawable(const Fill& fill, int columnCount) const;
    [[nodiscard]] static qcp::fill::Step ownerStep(const Component& owner);
    [[nodiscard]] std::vector<qcp::fill::StripPoint> fillRows(const Fill& fill,
                                                              const QCPAbstractMultiDataSource& ds,
                                                              int begin, int end) const;
    [[nodiscard]] std::vector<qcp::fill::StripPoint> shapedFillRows(
        const Fill& fill, const QCPAbstractMultiDataSource& ds, int begin, int end) const;
    void drawFills(QCPPainter* painter, const QCPAbstractMultiDataSource& ds, int begin, int end,
                   bool fresh, const QPointF& gpuOffset, bool isExportMode) const;
    void drawFillOnGpu(QCPPainter* painter, QCPPlottableRhiLayer* layer, std::vector<float>& verts,
                       const QColor& color, std::span<const qcp::fill::StripPoint> rows, bool fresh,
                       const QPointF& gpuOffset) const;
    [[nodiscard]] std::vector<qcp::fill::StripPoint>
    binnedWhenDense(std::vector<qcp::fill::StripPoint> rows) const;
    void paintFill(QCPPainter* painter, const QBrush& brush,
                   std::span<const qcp::fill::StripPoint> rows) const;
    [[nodiscard]] bool errorBarsDrawable(const ErrorBars& spec, int columnCount) const;
    [[nodiscard]] QPen errorBarPen(const ErrorBars& spec) const;
    using BoundPixel = double (*)(const QCPAxis& valueAxis, double value);
    [[nodiscard]] static double errorPixel(const QCPAxis& valueAxis, double value);
    [[nodiscard]] std::vector<qcp::errorbars::Bar> errorBarRows(
        const ErrorBars& spec, const QCPAbstractMultiDataSource& ds, int begin, int end,
        BoundPixel boundPixel = &errorPixel) const;
    [[nodiscard]] bool errorBarsFitInView(std::span<const qcp::errorbars::Bar> bars,
                                          double minSpacingPx) const;
    [[nodiscard]] std::vector<qcp::fill::StripPoint> errorBandRows(
        const ErrorBars& spec, const QCPAbstractMultiDataSource& ds, int begin, int end) const;
    void drawErrorBars(QCPPainter* painter, const QCPAbstractMultiDataSource& ds, bool rawRows,
                       int begin, int end, bool fresh, const QPointF& gpuOffset,
                       bool isExportMode) const;
    void drawBarsAsPolylines(QCPPainter* painter, const ErrorBarsEntry& entry,
                             std::span<const qcp::errorbars::Bar> bars, bool fresh,
                             const QPointF& gpuOffset, bool isExportMode) const;
    void drawBarsAsBand(QCPPainter* painter, QCPPlottableRhiLayer* gpuLayer,
                        const ErrorBarsEntry& entry, std::span<const qcp::fill::StripPoint> rows,
                        bool fresh, const QPointF& gpuOffset) const;
    void rebuildL2(const ViewportParams& vp) const;
    void onViewportChanged();
    void followAxes();
    QList<QMetaObject::Connection> mAxisLinks;
    bool pipelineBusy() const override { return mPipeline.isBusy(); }
    void setPipelineScheduler(QCPPipelineScheduler* scheduler) override { mPipeline.setScheduler(scheduler); }

    void updateBaseSelection();

};

using QCPGraphComponent = QCPMultiGraph::Component;

// The fluent handle of a QCPMultiGraph; see QCPPlottableOps for the subclass contract.
template <typename T, typename Self>
class QCPMultiGraphOps : public QCPPlottableOps<T, qcp::OpsRet<Self, QCPMultiGraphOps<T>>>
{
    using Base = QCPPlottableOps<T, qcp::OpsRet<Self, QCPMultiGraphOps<T>>>;

public:
    using Base::Base;
    using typename Base::Ret;

    // Same arguments as QCPMultiGraph::setData: the keys and one value column per component.
    template <IndexableNumericRange KC, IndexableNumericRange VC>
    Ret setData(KC&& keys, std::vector<VC>&& valueColumns)
    {
        return this->apply([&](T& o)
                           { o.setData(std::forward<KC>(keys), std::forward<std::vector<VC>>(valueColumns)); });
    }

    Ret setDataSource(std::shared_ptr<QCPAbstractMultiDataSource> source)
    {
        return this->apply([&](T& o) { o.setDataSource(std::move(source)); });
    }

    Ret setDataSource(std::unique_ptr<QCPAbstractMultiDataSource> source)
    {
        return this->apply([&](T& o) { o.setDataSource(std::move(source)); });
    }

    Ret setComponentNames(const QStringList& names)
    {
        return this->apply([&](T& o) { o.setComponentNames(names); });
    }

    Ret setComponentColors(const QList<QColor>& colors)
    {
        return this->apply([&](T& o) { o.setComponentColors(colors); });
    }

    Ret setComponentPens(const QList<QPen>& pens)
    {
        return this->apply([&](T& o) { o.setComponentPens(pens); });
    }

    Ret setComponentInLegend(int index, bool inLegend)
    {
        return this->apply([&](T& o) { o.setComponentInLegend(index, inLegend); });
    }

    Ret setColorValues(std::vector<double> values)
    {
        return this->apply([&](T& o) { o.setColorValues(std::move(values)); });
    }

    Ret setColorValues(std::shared_ptr<const std::vector<double>> values)
    {
        return this->apply([&](T& o) { o.setColorValues(std::move(values)); });
    }

    Ret setColorGradient(const QCPColorGradient& gradient)
    {
        return this->apply([&](T& o) { o.setColorGradient(gradient); });
    }

    Ret setColorScaleType(QCPAxis::ScaleType type)
    {
        return this->apply([&](T& o) { o.setColorScaleType(type); });
    }

    Ret setScatterSkip(int skip)
    {
        return this->apply([&](T& o) { o.setScatterSkip(skip); });
    }

    Ret setComponentVisible(int index, bool visible)
    {
        return this->apply([&](T& o) { o.setComponentVisible(index, visible); });
    }

    Ret setComponentFillBrush(int owner, const QBrush& brush)
    {
        return this->apply([&](T& o) { o.setComponentFillBrush(owner, brush); });
    }

    Ret setLineStyle(QCPMultiGraph::LineStyle style)
    {
        return this->apply([&](T& o) { o.setLineStyle(style); });
    }

    Ret setComponentLineStyle(int index, QCPMultiGraph::LineStyle style)
    {
        return this->apply([&](T& o) { o.setComponentLineStyle(index, style); });
    }

    Ret setAdaptiveSampling(bool enabled)
    {
        return this->apply([&](T& o) { o.setAdaptiveSampling(enabled); });
    }

    Ret setGapThreshold(double threshold)
    {
        return this->apply([&](T& o) { o.setGapThreshold(threshold); });
    }

    Ret setColorRange(const QCPRange& range)
    {
        return this->apply([&](T& o) { o.setColorRange(range); });
    }
};


