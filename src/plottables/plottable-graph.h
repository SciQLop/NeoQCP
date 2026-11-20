#pragma once
#include "../drawable.h"
#include "../misuse.h"
#include "datasource/abstract-multi-datasource.h"
#include "datasource/async-pipeline.h"
#include "datasource/graph-resampler.h"
#include "datasource/growable-multi-datasource.h"
#include "datasource/row-major-multi-datasource.h"
#include "datasource/soa-multi-datasource.h"
#include "graph-errorbars.h"
#include "graph-fill.h"
#include "plottable-color-mapper.h"
#include "plottable-draw-utils.h"
#include "plottable-line.h"
#include "plottable.h"
#include "plottable1d.h"
#include <QPointer>
#include <QTimer>
#include <QVector>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <type_traits>
#include <vector>

class QCPPlottableRhiLayer;

template <typename T, typename Self = void>
class QCPGraphOps;

class QCP_LIB_DECL QCPGraph : public QCPLine, public QCPPlottableInterface1D
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(QCPGraph)
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(bool adaptiveSampling READ adaptiveSampling WRITE setAdaptiveSampling NOTIFY
                   adaptiveSamplingChanged)
    Q_PROPERTY(bool deferredDataSwap READ deferredDataSwap WRITE setDeferredDataSwap NOTIFY
                   deferredDataSwapChanged)
    Q_PROPERTY(
        double gapThreshold READ gapThreshold WRITE setGapThreshold NOTIFY gapThresholdChanged)
    /// \endcond
public:
    template <typename T>
    using OpsFor = QCPGraphOps<T>;

    explicit QCPGraph(QCP::Capabilities required = QCP::capStraightSegments);
    //! One data point per (key, value) pair, sorted by key: add<QCPGraph>(keys, values) needs no
    //! setData() call.
    template <QCPNumberRange K, QCPNumbersOnly V>
    QCPGraph(const K& keys, const V& values, bool alreadySorted = false) : QCPGraph()
    {
        setData(qcp::toDoubles(keys), qcp::toDoubles(values), alreadySorted);
    }
    //! One component per value column, the 2D form: add<QCPGraph>(keys, columns) needs no setData()
    //! call.
    template <QCPNumberRange K, QCPColumns C>
    QCPGraph(const K& keys, const C& columns, bool alreadySorted = false) : QCPGraph()
    {
        setData(keys, columns, alreadySorted);
    }
    //! Value columns of any numeric type: the constructor form of the multi-column setData().
    template <IndexableNumericRange KC, IndexableNumericRange VC>
    QCPGraph(KC&& keys, std::vector<VC>&& valueColumns, bool alreadySorted = false) : QCPGraph()
    {
        setData(std::forward<KC>(keys), std::forward<std::vector<VC>>(valueColumns), alreadySorted);
    }
    ~QCPGraph() override;

    // Data source
    void setDataSource(std::unique_ptr<QCPMultiDataSource> source);
    virtual void setDataSource(std::shared_ptr<QCPMultiDataSource> source);
    [[nodiscard]] QCPMultiDataSource* dataSource() const { return mDataSource.get(); }
    //! The rows this line records from in the frame being drawn: its L2 bins when they are
    //! fresh for it, its own data otherwise. A fill between lines reads what they draw.
    [[nodiscard]] const QCPMultiDataSource* drawnDataSource() const;
    virtual void dataChanged();
    //! A content change: drops a fill whose partner lost its component, then marks the layer.
    void update() override;

    QCPGraphPipeline& pipeline() { return mPipeline; }
    const QCPGraphPipeline& pipeline() const { return mPipeline; }

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
            // I4/C1: the growable source refuses a shape or a non-finite key before it sorts or
            // indexes anything. A refused source is empty: installing it would clear the graph.
            const auto keyCount = std::ranges::size(keys);
            const auto columnCount = valueColumns.size();
            auto source = std::make_shared<QCPGrowableMultiDataSource>(
                std::forward<KC>(keys), std::forward<std::vector<VC>>(valueColumns),
                alreadySorted);
            if (std::size_t(source->size()) != keyCount
                || std::size_t(source->columnCount()) != columnCount)
                return;
            setOwnedSource(std::move(source));
        }
        else
        {
            setDataSource(std::make_shared<QCPSoAMultiDataSource<KD, VD>>(
                std::forward<KC>(keys), std::forward<std::vector<VC>>(valueColumns)));
        }
    }

    //! One component per value column of any QCPColumns, each column read once into the graph's
    //! own storage: the 2D form of the data calls.
    template <QCPNumberRange K, QCPColumns C>
    void setData(const K& keys, const C& columns, bool alreadySorted = false)
    {
        auto adapted = qcp::columns(columns);
        std::vector<std::vector<double>> copied;
        if constexpr (std::ranges::sized_range<decltype(adapted)>)
            copied.reserve(static_cast<std::size_t>(std::ranges::size(adapted)));
        for (auto&& column : adapted)
            copied.push_back(qcp::toDoubles(column));
        setData(qcp::toDoubles(keys), std::move(copied), alreadySorted);
    }

    //! One-column data from any QCPNumberRange, a span or a view say: read once into the graph's
    //! own storage, the container overloads below without the container.
    template <QCPNumberRange K, QCPNumbersOnly V>
    void setData(const K& keys, const V& values, bool alreadySorted = false)
    {
        setData(qcp::toDoubles(keys), qcp::toDoubles(values), alreadySorted);
    }
    //! One-column data: keys and values as QVectors (2.x's spelling).
    void setData(const QVector<double>& keys, const QVector<double>& values,
                 bool alreadySorted = false);
    //! One-column data: keys and values as std::vectors. They are moved into the source.
    void setData(std::vector<double> keys, std::vector<double> values,
                 bool alreadySorted = false);
    //! One-column data from braced lists, `setData({1, 2}, {3, 4})`: 2.x's QVector call, which
    //! the two container overloads above would make ambiguous.
    void setData(std::initializer_list<double> keys, std::initializer_list<double> values,
                 bool alreadySorted = false)
    {
        setData(std::vector<double>(keys), std::vector<double>(values), alreadySorted);
    }

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
    //! Removes the rows whose key is below \a key (2.x's removeDataBefore: a rolling window).
    //! Same rules as addData(): only the source the graph owns; a view, a shared source or a
    //! waterfall is a misuse and nothing changes. A graph without data has nothing to remove.
    void removeDataBefore(double key);
    //! Removes the rows whose key is above \a key.
    void removeDataAfter(double key);
    //! Removes the rows whose key lies in [\a fromKey, \a toKey], both bounds included (2.x).
    void removeData(double fromKey, double toKey);

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
        double target = 0; // the constant the fill reaches
        QBrush brush;      // Qt::NoBrush = owner pen colour at alpha 64
    };
    //! Fills between a column and a constant. False (nothing changed) for an unknown column.
    bool setComponentFill(int owner, double value, const QBrush& brush = Qt::NoBrush);
    void setComponentFillBrush(int owner, const QBrush& brush);
    void clearComponentFill(int owner);
    [[nodiscard]] std::optional<Fill> componentFill(int owner) const;
    //! Whether a decoration may name \a column: any column >= 0 before the first data, the
    //! data's columns after.
    [[nodiscard]] bool isValidColumn(int column) const;
    //! The fill's own brush; for the default brush, the owner's pen colour at alpha 64.
    [[nodiscard]] QBrush effectiveFillBrush(const Fill& fill) const;
    //! The fill brush of a selected or unselected run; a selected run takes the user's decorator
    //! brush, or else the selected pen's colour at the fill's alpha.
    [[nodiscard]] QBrush fillRunBrush(const Fill& fill, bool selected) const;
    //! The step shape of \a owner's line, which shapes both edges of a fill that line owns.
    [[nodiscard]] static qcp::fill::Step ownerStep(const QCPGraphComponent& owner);

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

    //! The name a component carries until it is named: "Component <index>". A one-component
    //! graph's legend row shows the graph's name while its component still has it.
    [[nodiscard]] static QString defaultComponentName(int index);

    //! Colour values on a non-cartesian area that is not an axis rect need an explicit colour
    //! scale: the setter refuses them and setSystem refuses to join such a frame without one (A9).
    void setSystem(QCPCoordinateSystem* system) override;
    void setColorValues(std::shared_ptr<const std::vector<double>> values);
    void setColorValues(std::vector<double> values);
    void setColorScale(QCPColorScale* colorScale) override;

    [[nodiscard]] bool adaptiveSampling() const { return mAdaptiveSampling; }
    void setAdaptiveSampling(bool enabled);
    // Gap detection on the lines (see QCPMultiDataSource::setGapThreshold); 0 turns it off.
    [[nodiscard]] double gapThreshold() const { return mGapThreshold; }
    void setGapThreshold(double threshold);

    // Per-component selection
    [[nodiscard]] QCPDataSelection componentSelection(int index) const;
    void setComponentSelection(int index, QCPDataSelection sel);

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

    // QCPPlottable
    QCPPlottableInterface1D* interface1D() override { return this; }
    double selectTest(const QPointF& pos, bool onlySelectable,
                      QVariant* details = nullptr) const override;
    QCPRange getKeyRange(bool& foundRange,
                         QCP::SignDomain inSignDomain = QCP::sdBoth) const override;
    QCPRange getValueRange(bool& foundRange,
                           QCP::SignDomain inSignDomain = QCP::sdBoth,
                           const QCPRange& inKeyRange = QCPRange()) const override;
    //! An impulse graph's stems stand on its baseline: the fit pushes no margin past it.
    QVector<double> stickyValues(QCPDrawnSide side) const override;

    // Legend
    using QCPPlottable::addToLegend;
    using QCPPlottable::removeFromLegend;
    bool addToLegend(QCPLegend* legend) override;
    bool removeFromLegend(QCPLegend* legend) const override;
    //! Draws one component's legend line: its pen, or, when the graph is coloured, the pen
    //! with a gradient brush along `line` sampled from the colour mapper.
    void drawComponentLegendIcon(QCPPainter* painter, int component, const QLineF& line) const;
    //! The row of a graph without data (no component yet): the pen line, marker and brush its
    //! first column will take, as 2.x drew an empty graph.
    void drawEmptyLegendIcon(QCPPainter* painter, const QLineF& line) const;

signals:
    void adaptiveSamplingChanged(bool adaptiveSampling);
    void deferredDataSwapChanged(bool deferredDataSwap);
    void gapThresholdChanged(double gapThreshold);

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

    using QCPLine::drawLegendIcon;
    void drawLegendIcon(QCPDrawContext& icon) const override;
    void systemDidChange() override;

    //! B5: the graph draws through its system's transform when that system is not cartesian.
    [[nodiscard]] bool drawsInDataSpace() const;
    //! The key scale the frame draws in: the system's own key axis, whatever its kind.
    [[nodiscard]] QCPScale shownKeyScale() const;
    void drawInDataSpace(QCPDrawContext& context) const;
    [[nodiscard]] std::optional<double> nearestMappedPoint(const QPointF& pos,
                                                           QVariant* details) const;
    //! Value 0, or on a log value axis the range bound nearer zero (the 4j baseline).
    [[nodiscard]] double dataSpaceBaseline() const;
    //! The value the stems of an impulse graph stand on, when that is a fixed value: nullopt when no
    //! visible component draws impulses, and on a log value axis, where the baseline is the range
    //! bound the stem reaches rather than a data value.
    [[nodiscard]] std::optional<double> impulseBaseline() const;
    //! A fill edge: the value itself, or on a log axis the bound on its side of zero.
    [[nodiscard]] double dataSpaceFillValue(double value) const;
    //! Whether the system's area is an axis rect, whose colour scale colour values can join.
    [[nodiscard]] bool areaIsAxisRect() const;
    //! Whether colour values are set on a frame that has no explicit colour scale to map them.
    [[nodiscard]] bool colourNeedsAScale() const;

    void selectEvent(QMouseEvent* event, bool additive, const QVariant& details,
                     bool* selectionStateChanged) override;
    //! Each component's hit rows from a click's or a rect's \a details.
    [[nodiscard]] QVector<QCPDataSelection> componentHits(const QVariant& details) const;
    //! stDataRange / stMultipleDataRanges: each component takes its hit, toggled when \a additive.
    void selectComponentRows(const QVector<QCPDataSelection>& hits, bool additive,
                             bool* selectionStateChanged);
    void deselectEvent(bool* selectionStateChanged) override;
    //! 2.x's rows are shared by the columns: the graph's selection selects them on each component.
    bool assignSelection(const QCPDataSelection& selection) override;

    friend class TestMultiGraph;
    friend class TestPipeline;
    friend class TestDataSwap;
    friend class TestColorByScalar;
    friend class TestRepaint;

protected:
    std::shared_ptr<QCPMultiDataSource> mDataSource;
    QVector<Fill> mFills;
    QVector<ErrorBars> mErrorBars;
    mutable QVector<QCPDataSelection> mLastRectSelections; // per-component selections from selectTestRect
    bool mAdaptiveSampling = true;
    double mGapThreshold = qcp::algo::kDefaultGapThreshold;
    // draw() is const: what it resolves for this frame is mutable state, not cached pixels.
    mutable QCPGraphPipeline mPipeline;
    mutable std::shared_ptr<qcp::algo::GraphResamplerCache> mL1Cache;
    mutable std::shared_ptr<QCPMultiDataSource> mL2Result;
    mutable bool mL2Dirty = false;
    bool mNeedsResampling = false;
    //! Whether the cartesian pipeline is bypassed (the data-space branch): a system change clears
    //! the bins only when the branch itself changes, not on an axis change within cartesian.
    bool mPipelineBypassed = false;
    //! The data the recording was made from changed: the adapter records it again.
    mutable quint64 mRevision = 0;
    // Debounce timer: defers expensive L2 rebuild until panning stops
    QTimer mViewportDebounce;

    // Read by the L1 transform on the pipeline thread: build origin tables only for coloured
    // graphs.
    std::shared_ptr<std::atomic<bool>> mWantOrigin = std::make_shared<std::atomic<bool>>(false);
    // The binning space the L1 transform closure was baked with (see qcp::algo::binningSpace): a
    // new axis or a new space needs a new closure, and bins built for another space are meaningless
    // rather than stale.
    QCPScale mL1Scale;
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
    std::shared_ptr<QCPMultiDataSource> mPendingSource;
    //! The source the graph owns: the growable one setData()/addData() built. Null after every
    //! other source setter (a view, a shared/unique source, a waterfall adapter).
    std::shared_ptr<QCPGrowableMultiDataSource> mOwnedSource;
    std::shared_ptr<qcp::algo::GraphResamplerCache> mPendingL1;
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
    void installSource(std::shared_ptr<QCPMultiDataSource> source);
    //! The growable source addData() or a removal may write, or null when the graph has no data
    //! yet (a new source starts) or a misuse has already been reported.
    [[nodiscard]] std::shared_ptr<QCPGrowableMultiDataSource> ownedSourceForAppend() const;
    //! Whether addData() may write to a source this graph owns. A waterfall's source is its
    //! adapter, never owned: it refuses every addData, including the first.
    [[nodiscard]] virtual bool acceptsAppend() const { return true; }
    //! After an append or a removal: re-stage the pending source (a new generation, a pipeline
    //! re-run) or refresh the displayed one.
    void ownedSourceChanged();
    //! Applies an append or a removal to the owned source (the pending one when a swap is staged).
    //! A running pipeline job reads the source it was started for, so the edit clones it first
    //! when busy. False, and nothing changes, when \a edit changed nothing.
    bool editOwnedSource(const std::function<bool(QCPGrowableMultiDataSource&)>& edit);
    //! removeDataBefore/After/removeData: addData()'s ownership rules, then the edit.
    void removeOwnedRows(const std::function<bool(QCPGrowableMultiDataSource&)>& remove);
    void applySourceNow(std::shared_ptr<QCPMultiDataSource> source);
    void stagePendingSource(std::shared_ptr<QCPMultiDataSource> source);
    void markPendingReady();
    void onL1Ready(uint64_t generation);
    void syncComponentCount(const QCPMultiDataSource* source);
    [[nodiscard]] QCPGraphComponent firstComponentStyle() const;
    [[nodiscard]] std::optional<QBrush> emptyGraphFillBrush() const;
    void drawLegendGlyph(QCPPainter* painter, const QCPGraphComponent& comp,
                         const std::optional<QBrush>& fill, const QLineF& line) const;
    void recordLegendSegment(QCPDrawContext& icon, const QCPGraphComponent& comp,
                             const std::optional<QBrush>& fill, double x0) const;
    void dropDecorationsBeyond(int columnCount);
    void hideErrorBound(int column);
    void releaseErrorBounds(const ErrorBars& removed);
    bool setFill(Fill fill);
    [[nodiscard]] bool isErrorBound(int column) const;
    void decorationsChanged();
    [[nodiscard]] bool fillDrawable(const Fill& fill, int columnCount) const;
    [[nodiscard]] std::vector<qcp::fill::StripPoint> fillRows(const Fill& fill,
                                                              const QCPMultiDataSource& ds,
                                                              int begin, int end) const;
    [[nodiscard]] std::vector<qcp::fill::StripPoint>
    shapeFillRows(QCPDrawContext& context, const Fill& fill,
                  std::vector<qcp::fill::StripPoint> rows) const;
    //! Whether \a row of \a ds is in \a column's selection; a resampled row by its key.
    [[nodiscard]] bool rowIsSelected(int column, const QCPMultiDataSource& ds,
                                     int row) const;
    //! One flag per vertex: whether its row is selected. A break (row -1) follows the vertex before.
    [[nodiscard]] std::vector<char> selectedVertices(int column,
                                                     const QCPMultiDataSource& ds,
                                                     const std::vector<int>& rows) const;
    //! Records one component: its line in runs of equally selected rows, then its markers.
    //! \a rows holds each vertex's row in \a ds when asked for, else nothing.
    void recordComponent(QCPDrawContext& context, int column, std::vector<QPointF> line,
                         const std::vector<int>& rows, const QCPMultiDataSource& ds,
                         bool coloured, double impulseBaseline, const QCPScale& keyScale) const;
    //! Records \a line in \a style with \a pen; \a dataIdx, when not empty, colours it.
    void recordLine(QCPDrawContext& context, LineStyle style, std::vector<QPointF> line,
                    const QVector<int>& dataIdx, QPen pen, double impulseBaseline,
                    const QCPScale& keyScale) const;
    //! The markers of \a line, the selected ones (\a selected per vertex, or \a allSelected when
    //! empty) in the selected style.
    void recordMarkers(QCPDrawContext& context, const QCPGraphComponent& comp,
                       const std::vector<QPointF>& line, const std::vector<int>& rows,
                       bool indexed, const std::vector<char>& selected, bool allSelected) const;
    //! Whether the user styled or replaced the selection decorator: then it draws the selection,
    //! not the components' selected pens.
    [[nodiscard]] bool decoratorIsTheUsers() const;
    [[nodiscard]] QPen selectedLinePen(const QCPGraphComponent& comp) const;
    [[nodiscard]] QCPScatterStyle selectedMarkerStyle(const QCPGraphComponent& comp) const;
    //! A decorator subclass's own decoration (the bracket), after the lines.
    void recordSelectionDecoration(QCPDrawContext& context) const;
    void recordFills(QCPDrawContext& context, const QCPMultiDataSource& ds, int begin,
                     int end) const;
    [[nodiscard]] bool errorBarsDrawable(const ErrorBars& spec, int columnCount) const;
    [[nodiscard]] QPen errorBarPen(const ErrorBars& spec) const;
    [[nodiscard]] std::vector<qcp::errorbars::Bar>
    errorBarRows(const ErrorBars& spec, const QCPMultiDataSource& ds, int begin,
                 int end) const;
    [[nodiscard]] bool errorBarsFitInView(std::span<const qcp::errorbars::Bar> bars,
                                          double minSpacingPx) const;
    [[nodiscard]] std::vector<qcp::fill::StripPoint>
    errorBandRows(const ErrorBars& spec, const QCPMultiDataSource& ds, int begin,
                  int end) const;
    void recordErrorBars(QCPDrawContext& context, const QCPMultiDataSource& ds,
                         bool rawRows, int begin, int end) const;
    void rebuildL2(const ViewportParams& vp) const;
    void onViewportChanged(const QCPRange& newRange, const QCPRange& oldRange);
    void onKeyScaleChanged();
    void restartL1Cache();
    void bakeL1Transform(const QCPMultiDataSource& src);
    void followAxes();
    QList<QMetaObject::Connection> mAxisLinks;
    bool pipelineBusy() const override { return mPipeline.isBusy(); }
    void setPipelineScheduler(QCPPipelineScheduler* scheduler) override
    {
        if (scheduler)
            mPipeline.setScheduler(scheduler);
        else
            mPipeline.deferJobs(); // outside a plot the transforms wait, they never block
    }

    void updateBaseSelection();
    [[nodiscard]] bool isDataRow(int index) const;

};

// The fluent handle of a QCPGraph; see QCPPlottableOps for the subclass contract.
template <typename T, typename Self>
class QCPGraphOps : public QCPPlottableOps<T, qcp::OpsRet<Self, QCPGraphOps<T>>>
{
    using Base = QCPPlottableOps<T, qcp::OpsRet<Self, QCPGraphOps<T>>>;

public:
    using Base::Base;
    using typename Base::Ret;

    // Same arguments as QCPGraph::setData: the keys and one value column per component.
    template <IndexableNumericRange KC, IndexableNumericRange VC>
    Ret setData(KC&& keys, std::vector<VC>&& valueColumns, bool alreadySorted = false)
    {
        return this->apply(
            [&](T& o) {
                o.setData(std::forward<KC>(keys), std::forward<std::vector<VC>>(valueColumns),
                          alreadySorted);
            });
    }

    template <QCPNumberRange K, QCPColumns C>
    Ret setData(const K& keys, const C& columns, bool alreadySorted = false)
    {
        return this->apply([&](T& o) { o.setData(keys, columns, alreadySorted); });
    }

    Ret setData(const QVector<double>& keys, const QVector<double>& values,
                bool alreadySorted = false)
    {
        return this->apply([&](T& o) { o.setData(keys, values, alreadySorted); });
    }

    //! The range form of QCPGraph::setData(): a span or a view, read once.
    template <QCPNumberRange K, QCPNumbersOnly V>
    Ret setData(const K& keys, const V& values, bool alreadySorted = false)
    {
        return this->apply([&](T& o) { o.setData(keys, values, alreadySorted); });
    }

    Ret setData(const std::vector<double>& keys, const std::vector<double>& values,
                bool alreadySorted = false)
    {
        return this->apply([&](T& o) { o.setData(keys, values, alreadySorted); });
    }

    Ret setData(std::initializer_list<double> keys, std::initializer_list<double> values,
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

    Ret removeDataBefore(double key)
    {
        return this->apply([&](T& o) { o.removeDataBefore(key); });
    }

    Ret removeDataAfter(double key)
    {
        return this->apply([&](T& o) { o.removeDataAfter(key); });
    }

    Ret removeData(double fromKey, double toKey)
    {
        return this->apply([&](T& o) { o.removeData(fromKey, toKey); });
    }

    Ret setDataSource(std::shared_ptr<QCPMultiDataSource> source)
    {
        return this->apply([&](T& o) { o.setDataSource(std::move(source)); });
    }

    Ret setDataSource(std::unique_ptr<QCPMultiDataSource> source)
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

    Ret setDataScale(const QCPScale& scale)
    {
        return this->apply([&](T& o) { o.setDataScale(scale); });
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

    Ret setLineStyle(QCPGraph::LineStyle style)
    {
        return this->apply([&](T& o) { o.setLineStyle(style); });
    }

    Ret setComponentLineStyle(int index, QCPGraph::LineStyle style)
    {
        return this->apply([&](T& o) { o.setComponentLineStyle(index, style); });
    }

    Ret setScatterStyle(const QCPScatterStyle& style)
    {
        return this->apply([&](T& o) { o.setScatterStyle(style); });
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


