#pragma once
#include "../drawable.h"
#include "datasource/abstract-multi-datasource.h"
#include "datasource/async-pipeline.h"
#include "datasource/graph-resampler.h"
#include "datasource/growable-multi-datasource.h"
#include "datasource/row-major-multi-datasource.h"
#include "datasource/soa-multi-datasource.h"
#include "multigraph-errorbars.h"
#include "multigraph-fill.h"
#include "plottable-color-mapper.h"
#include "plottable-draw-utils.h"
#include "plottable-line.h"
#include "plottable.h"
#include "plottable1d.h"
#include <QPointer>
#include <QTimer>
#include <QVector>
#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>
#include <variant>
#include <vector>

class QCPPlottableRhiLayer;

template <typename T, typename Self = void>
class QCPMultiGraphOps;

class QCP_LIB_DECL QCPMultiGraph : public QCPAbstractLine, public QCPPlottableInterface1D
{
    Q_OBJECT
public:
    template <typename T>
    using OpsFor = QCPMultiGraphOps<T>;

    explicit QCPMultiGraph(QCPAxis* keyAxis, QCPAxis* valueAxis,
                           QCP::Capabilities required = QCP::capStraightSegments);
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
    //! Whether a new source waits for the plot's debounced swap instead of replacing the
    //! displayed one at once. Off (default): setDataSource() applies before it returns.
    [[nodiscard]] bool deferredDataSwap() const { return mDeferredDataSwap; }
    void setDeferredDataSwap(bool enabled);

    // Convenience: owning. Double inputs build the growable source addData() appends to; other
    // value types keep the SoA source, whose data the graph does not own.
    template <IndexableNumericRange KC, IndexableNumericRange VC>
    void setData(KC&& keys, std::vector<VC>&& valueColumns, bool alreadySorted = false)
    {
        using KD = std::decay_t<KC>;
        using VD = std::decay_t<VC>;
        if constexpr (std::is_same_v<std::ranges::range_value_t<KD>, double>
                      && std::is_same_v<std::ranges::range_value_t<VD>, double>)
        {
            setOwnedSource(std::make_shared<QCPGrowableMultiDataSource>(
                std::forward<KC>(keys), std::forward<std::vector<VC>>(valueColumns),
                alreadySorted));
        }
        else
        {
            setDataSource(std::make_shared<QCPSoAMultiDataSource<KD, VD>>(
                std::forward<KC>(keys), std::forward<std::vector<VC>>(valueColumns)));
        }
    }

    //! One-column data: keys and values as QVectors (2.x's spelling).
    void setData(const QVector<double>& keys, const QVector<double>& values,
                 bool alreadySorted = false);
    //! One-column data: keys and values as std::vectors.
    void setData(const std::vector<double>& keys, const std::vector<double>& values,
                 bool alreadySorted = false);

    //! Appends rows to the source this graph owns. A view, a shared source, a waterfall adapter
    //! or a source the graph does not own is a misuse and nothing changes.
    void addData(const QVector<double>& keys, const QVector<double>& values,
                 bool alreadySorted = false);
    //! Appends one point to a one-column graph; a multi-column graph refuses it.
    void addData(double key, double value);
    //! Appends one row per component. A column-count mismatch is a misuse.
    void addData(const QVector<double>& keys, std::vector<QVector<double>> columns,
                 bool alreadySorted = false);
    //! Appends one row of \a values, one per component.
    void addData(double key, std::span<const double> values);

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

    struct FillToGraph
    {
        QPointer<QCPMultiGraph> graph;
        int component = 0;
    };
    struct Fill {
        int owner = -1;
        std::variant<double, int, FillToGraph> target; // a constant, another column, another graph
        QBrush brush;                                  // Qt::NoBrush = owner pen colour at alpha 64
    };
    //! Fills between a column and a constant. False (nothing changed) for an unknown column.
    bool setComponentFill(int owner, double value, const QBrush& brush = Qt::NoBrush);
    //! Fills between two columns. One fill per owner: setting again replaces it.
    bool setComponentFillBetween(int owner, int target, const QBrush& brush = Qt::NoBrush);
    //! Fills between this column and a component of \a other. Both graphs must share the system;
    //! a different system is a misuse. The partner is held by QPointer: the band drops when it or
    //! its component goes.
    bool setComponentFillToGraph(int owner, QCPMultiGraph* other, int otherComponent,
                                 const QBrush& brush = Qt::NoBrush);
    void setComponentFillBrush(int owner, const QBrush& brush);
    void clearComponentFill(int owner);
    [[nodiscard]] std::optional<Fill> componentFill(int owner) const;
    //! Whether a decoration may name \a column: any column >= 0 before the first data, the
    //! data's columns after.
    [[nodiscard]] bool isValidColumn(int column) const;
    [[nodiscard]] QBrush effectiveFillBrush(const Fill& fill) const;
    //! The fill brush of a selected or unselected run; a selected run takes the selected pen's
    //! colour at the fill's alpha.
    [[nodiscard]] QBrush fillRunBrush(const Fill& fill, bool selected) const;

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

    //! Colour values on a non-cartesian area that is not an axis rect need an explicit colour
    //! scale: the setter refuses them and setSystem refuses to join such a frame without one (A9).
    void setSystem(QCPCoordinateSystem* system) override;
    void setColorValues(std::shared_ptr<const std::vector<double>> values);
    void setColorValues(std::vector<double> values);
    void setColorScale(QCPColorScale* colorScale);

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

    //! The lines, markers, fills or error bars changed: a counting bump, not a rebuild. On the
    //! data-space branch the system's revision is part of it (B4): the frame reads its key window
    //! and its baseline.
    quint64 dependencyRevision() const override;

    //! Without a finished resample an export resamples on the spot: the two recordings differ.
    bool resolvesForTheExport() const override { return true; }

    using QCPAbstractLine::drawLegendIcon;
    void drawLegendIcon(QCPDrawContext& icon) const override;
    void systemChanged() override;

    //! B5: the graph draws through its system's transform when that system is not cartesian.
    [[nodiscard]] bool drawsInDataSpace() const;
    void drawInDataSpace(QCPDrawContext& context) const;
    [[nodiscard]] std::optional<double> nearestMappedPoint(const QPointF& pos,
                                                           QVariant* details) const;
    //! Value 0, or on a log value axis the range bound nearer zero (the 4j baseline).
    [[nodiscard]] double dataSpaceBaseline() const;
    //! A fill edge: the value itself, or on a log axis the bound on its side of zero.
    [[nodiscard]] double dataSpaceFillValue(double value) const;
    //! Whether the system's area is an axis rect, whose colour scale colour values can join.
    [[nodiscard]] bool areaIsAxisRect() const;
    //! Whether the scale attached came from setColorScale(), not from an earlier rect.
    [[nodiscard]] bool hasExplicitColorScale() const;
    //! Whether colour values are set on a frame that has no explicit colour scale to map them.
    [[nodiscard]] bool colourNeedsAScale() const;

    void selectEvent(QMouseEvent* event, bool additive, const QVariant& details,
                     bool* selectionStateChanged) override;
    void deselectEvent(bool* selectionStateChanged) override;

    friend class TestMultiGraph;
    friend class TestPipeline;
    friend class TestDataSwap;
    friend class TestColorByScalar;

protected:
    std::shared_ptr<QCPAbstractMultiDataSource> mDataSource;
    QVector<Fill> mFills;
    QVector<ErrorBars> mErrorBars;
    mutable QVector<QCPDataSelection> mLastRectSelections; // per-component selections from selectTestRect
    bool mAdaptiveSampling = true;
    double mGapThreshold = qcp::algo::kDefaultGapThreshold;
    // draw() is const: what it resolves for this frame is mutable state, not cached pixels.
    mutable QCPMultiGraphPipeline mPipeline;
    mutable std::shared_ptr<qcp::algo::MultiGraphResamplerCache> mL1Cache;
    mutable std::shared_ptr<QCPAbstractMultiDataSource> mL2Result;
    mutable bool mL2Dirty = false;
    bool mNeedsResampling = false;
    //! Whether the cartesian pipeline is bypassed (the data-space branch): a system change clears
    //! the bins only when the branch itself changes, not on an axis change within cartesian.
    bool mPipelineBypassed = false;
    //! Whether the current colour scale came from setColorScale(): a scale a rect handed the graph
    //! is not the explicit one a non-rect area needs.
    bool mExplicitColorScale = false;
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
    void applyBrushToComponent(int index, const QBrush& brush) override;
    [[nodiscard]] std::optional<int> colorValueCount() const override;
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
    bool mDeferredDataSwap = false;
    std::shared_ptr<QCPAbstractMultiDataSource> mPendingSource;
    //! The source the graph owns: the growable one setData()/addData() built. Null after every
    //! other source setter (a view, a shared/unique source, a waterfall adapter).
    std::shared_ptr<QCPGrowableMultiDataSource> mOwnedSource;
    std::shared_ptr<qcp::algo::MultiGraphResamplerCache> mPendingL1;
    bool mPendingReady = false;
    uint64_t mPendingGeneration = 0;
    // Generation floor below which a pipeline result is from a source that's
    // no longer displayed (superseded and already committed/applied) — set
    // whenever a source is installed, so a late result can't repopulate
    // mL1Cache/mL2Result for a source it was never computed for.
    uint64_t mAcceptedGeneration = 0;

    //! Installs the source setData() built and records that the graph owns it.
    virtual void setOwnedSource(std::shared_ptr<QCPGrowableMultiDataSource> source);
    //! The one path a new source takes: applied now or staged for the deferred swap.
    void installSource(std::shared_ptr<QCPAbstractMultiDataSource> source);
    //! The growable source addData() may write, or null when the graph has no data yet (a new
    //! source starts) or a misuse has already been reported.
    [[nodiscard]] std::shared_ptr<QCPGrowableMultiDataSource> ownedSourceForAppend() const;
    //! After an append: re-stage the pending source (a new generation, a pipeline re-run) or
    //! refresh the displayed one.
    void appendChanged();
    //! Applies an append to the owned source (the pending one when a swap is staged). A running
    //! pipeline job reads the source it was started for, so the append clones it first when busy.
    bool appendToOwnedSource(
        const std::function<bool(QCPGrowableMultiDataSource&)>& append);
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
    [[nodiscard]] bool fillDrawable(const Fill& fill, int columnCount) const;
    [[nodiscard]] static qcp::fill::Step ownerStep(const Component& owner);
    [[nodiscard]] std::vector<qcp::fill::StripPoint> fillRows(const Fill& fill,
                                                              const QCPAbstractMultiDataSource& ds,
                                                              int begin, int end) const;
    [[nodiscard]] std::vector<qcp::fill::StripPoint>
    fillToGraphRows(const Fill& fill, const QCPAbstractMultiDataSource& ds, int begin,
                    int end) const;
    [[nodiscard]] std::vector<qcp::fill::StripPoint> shapedFillRows(
        const Fill& fill, const QCPAbstractMultiDataSource& ds, int begin, int end) const;
    [[nodiscard]] std::vector<qcp::fill::StripPoint>
    shapeFillRows(const Fill& fill, std::vector<qcp::fill::StripPoint> rows) const;
    [[nodiscard]] bool rowIsSelected(const Fill& fill, const QCPAbstractMultiDataSource& ds,
                                     int row) const;
    void partnerGone();
    [[nodiscard]] static double fillValue(const QCPAxis& valueAxis, double value);
    void recordFills(QCPDrawContext& context, const QCPAbstractMultiDataSource& ds, int begin,
                     int end) const;
    [[nodiscard]] std::vector<qcp::fill::StripPoint>
    binnedWhenDense(std::vector<qcp::fill::StripPoint> rows) const;
    [[nodiscard]] bool errorBarsDrawable(const ErrorBars& spec, int columnCount) const;
    [[nodiscard]] QPen errorBarPen(const ErrorBars& spec) const;
    [[nodiscard]] std::vector<qcp::errorbars::Bar>
    errorBarRows(const ErrorBars& spec, const QCPAbstractMultiDataSource& ds, int begin,
                 int end) const;
    [[nodiscard]] bool errorBarsFitInView(std::span<const qcp::errorbars::Bar> bars,
                                          double minSpacingPx) const;
    [[nodiscard]] std::vector<qcp::fill::StripPoint>
    errorBandRows(const ErrorBars& spec, const QCPAbstractMultiDataSource& ds, int begin,
                  int end) const;
    void recordErrorBars(QCPDrawContext& context, const QCPAbstractMultiDataSource& ds,
                         bool rawRows, int begin, int end) const;
    void rebuildL2(const ViewportParams& vp) const;
    void onViewportChanged(const QCPRange& newRange, const QCPRange& oldRange);
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
    Ret setData(KC&& keys, std::vector<VC>&& valueColumns, bool alreadySorted = false)
    {
        return this->apply(
            [&](T& o) {
                o.setData(std::forward<KC>(keys), std::forward<std::vector<VC>>(valueColumns),
                          alreadySorted);
            });
    }

    Ret setData(const QVector<double>& keys, const QVector<double>& values,
                bool alreadySorted = false)
    {
        return this->apply([&](T& o) { o.setData(keys, values, alreadySorted); });
    }

    Ret setData(const std::vector<double>& keys, const std::vector<double>& values,
                bool alreadySorted = false)
    {
        return this->apply([&](T& o) { o.setData(keys, values, alreadySorted); });
    }

    Ret addData(const QVector<double>& keys, const QVector<double>& values,
                bool alreadySorted = false)
    {
        return this->apply([&](T& o) { o.addData(keys, values, alreadySorted); });
    }

    Ret addData(double key, double value)
    {
        return this->apply([&](T& o) { o.addData(key, value); });
    }

    Ret addData(const QVector<double>& keys, std::vector<QVector<double>> columns,
                bool alreadySorted = false)
    {
        return this->apply([&](T& o)
                           { o.addData(keys, std::move(columns), alreadySorted); });
    }

    Ret addData(double key, std::span<const double> values)
    {
        return this->apply([&](T& o) { o.addData(key, values); });
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

    Ret setGradient(const QCPColorGradient& gradient)
    {
        return this->apply([&](T& o) { o.setGradient(gradient); });
    }

    Ret setDataRange(const QCPRange& range)
    {
        return this->apply([&](T& o) { o.setDataRange(range); });
    }

    Ret setDataScaleType(QCPAxis::ScaleType type)
    {
        return this->apply([&](T& o) { o.setDataScaleType(type); });
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
};


