#pragma once
#include "plottable.h"
#include "plottable1d.h"
#include "datasource/abstract-multi-datasource.h"
#include "datasource/soa-multi-datasource.h"
#include "datasource/row-major-multi-datasource.h"
#include "datasource/async-pipeline.h"
#include "datasource/graph-resampler.h"
#include "plottable-draw-utils.h"
#include "plottable-color-mapper.h"
#include "multigraph-fill.h"
#include "multigraph-errorbars.h"
#include <array>
#include <atomic>
#include <memory>
#include <optional>
#include <span>
#include <variant>
#include <vector>
#include <QTimer>

class QCPPlottableRhiLayer;

class QCP_LIB_DECL QCPMultiGraph : public QCPAbstractPlottable, public QCPPlottableInterface1D {
    Q_OBJECT
public:
    enum LineStyle { lsNone, lsLine, lsStepLeft, lsStepRight, lsStepCenter, lsImpulse };
    Q_ENUM(LineStyle)

    struct Component {
        QString name;
        QPen pen;
        QPen selectedPen;
        QCPScatterStyle scatterStyle;
        QCPDataSelection selection;
        bool visible = true;
        bool inLegend = true;
        //! Set through setComponentLineStyle(): a direct write leaves the cached lines stale.
        LineStyle lineStyle = lsLine;
    };

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
    Component& component(int index) { return mComponents[index]; }
    //! Shows or hides one component and repaints the layer. Setting `component(i).visible`
    //! directly is not observed: a layer that translates after a pan would keep the old
    //! GPU entries.
    void setComponentVisible(int index, bool visible);
    //! Keeps a component out of the legend (a band edge, an error bound) while it still draws
    //! and counts for auto-scale.
    void setComponentInLegend(int index, bool inLegend);
    [[nodiscard]] QVector<int> legendComponents() const;
    const Component& component(int index) const { return mComponents[index]; }
    void setComponentNames(const QStringList& names);
    void setComponentColors(const QList<QColor>& colors);
    void setComponentPens(const QList<QPen>& pens);

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
    [[nodiscard]] QBrush effectiveFillBrush(const Fill& fill) const;

    struct ErrorBars {
        int value = -1, lower = -1, upper = -1;
        std::optional<QPen> pen;      // none = the value component's pen
        double capWidthPx = 6;        // full cap width
    };
    //! Bars from `lower` to `upper` at each row of `value`; a band when rows are too dense.
    //! One per value column: setting again replaces it. Takes the bounds out of the legend.
    //! False (nothing changed) for an unknown or repeated column.
    bool setComponentErrorBars(int value, int lower, int upper, double capWidthPx = 6,
                               std::optional<QPen> pen = std::nullopt);
    void clearComponentErrorBars(int value);
    [[nodiscard]] std::optional<ErrorBars> componentErrorBars(int value) const;

    // Per-component value access (for tracers/tooltips)
    [[nodiscard]] double componentValueAt(int column, int index) const;

    //! The style given to new components; setLineStyle() also applies it to every component.
    [[nodiscard]] LineStyle lineStyle() const { return mLineStyle; }
    void setLineStyle(LineStyle style);
    void setComponentLineStyle(int index, LineStyle style);
    [[nodiscard]] bool adaptiveSampling() const { return mAdaptiveSampling; }
    void setAdaptiveSampling(bool enabled);
    // Gap detection on the lines (see QCPAbstractMultiDataSource::setGapThreshold); 0 turns it off.
    [[nodiscard]] double gapThreshold() const { return mGapThreshold; }
    void setGapThreshold(double threshold);
    [[nodiscard]] int scatterSkip() const { return mScatterSkip; }
    void setScatterSkip(int skip) { mScatterSkip = qMax(0, skip); }

    // Colour by scalar: one value per key, shared by all components.
    // QCustomPlot convention: these setters do not replot -- the caller must
    // call replot() (the first colouring may repaint on its own once the
    // one-time origin L1 rebuild lands).
    void setColorValues(std::shared_ptr<const std::vector<double>> values);
    void setColorValues(std::vector<double> values);
    void clearColorValues();
    [[nodiscard]] bool hasColorValues() const;
    void setColorGradient(const QCPColorGradient& gradient);
    [[nodiscard]] QCPColorGradient colorGradient() const;
    void setColorRange(const QCPRange& range);
    [[nodiscard]] QCPRange colorRange() const;
    void setColorScaleType(QCPAxis::ScaleType type);
    [[nodiscard]] QCPAxis::ScaleType colorScaleType() const;

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

Q_SIGNALS:
    //! A component was shown or hidden: the legend group row draws one segment per visible one.
    void componentVisibilityChanged();

protected:
    void draw(QCPPainter* painter) override;
    void drawLegendIcon(QCPPainter* painter, const QRectF& rect) const override;

    void selectEvent(QMouseEvent* event, bool additive, const QVariant& details,
                     bool* selectionStateChanged) override;
    void deselectEvent(bool* selectionStateChanged) override;

    friend class TestMultiGraph;
    friend class TestPipeline;
    friend class TestDataSwap;
    friend class TestColorByScalar;

protected:
    std::shared_ptr<QCPAbstractMultiDataSource> mDataSource;
    QVector<Component> mComponents;
    struct FillEntry {
        Fill spec;
        std::vector<float> verts;   // triangle cache, rebuilt when lines change
    };
    QVector<FillEntry> mFills;
    struct ErrorBarsEntry {
        ErrorBars spec;
        qcp::ExtrusionCache bars;
        QVector<QPointF> runs;      // pixel runs at the rendered range, re-extruded on a pen change
        std::vector<float> band;    // triangle cache of the dense fallback
        bool asBand = false;        // mode picked on the last fresh frame, kept on pan frames
    };
    QVector<ErrorBarsEntry> mErrorBars;
    mutable QVector<QCPDataSelection> mLastRectSelections; // per-component selections from selectTestRect
    LineStyle mLineStyle = lsLine;
    bool mAdaptiveSampling = true;
    double mGapThreshold = qcp::algo::kDefaultGapThreshold;
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
    void markLinesDirty();
    bool mLineCacheDirty = true;
    QSize mCachedPlotSize;
    // Per-component cached extruded GPU vertices — avoids re-extrusion on pan
    QVector<qcp::ExtrusionCache> mExtrusionCaches;
    // Debounce timer: defers expensive L2 rebuild until panning stops
    QTimer mViewportDebounce;

    qcp::ColorScalarMapper mColor;
    // Read by the L1 transform on the pipeline thread: build origin tables only for coloured graphs.
    std::shared_ptr<std::atomic<bool>> mWantOrigin = std::make_shared<std::atomic<bool>>(false);
    bool mL2HasOrigin = false;
    QVector<QVector<int>> mCachedIndices;   // per component, aligned with mCachedLines when coloured
    void invalidateLines();
    // Source indices aligned with the line-style-transformed points of a component.
    static QVector<int> lineIndices(LineStyle style, const QVector<int>& dataIdx);
    // Draws impulse line pairs coloured per point, batched by colour bucket.
    void drawColoredImpulses(QCPPainter* painter, const QVector<QPointF>& pairs,
                             const QVector<int>& indices, QPen pen,
                             const QPointF& gpuOffset) const;
    // Draws markers coloured per point: GPU via addScatterColored, or QPainter
    // fallback batched by colour bucket.
    void drawColoredScatters(QCPPainter* painter, const QVector<QPointF>& points,
                             const QVector<int>& indices, const Component& comp,
                             const QPointF& gpuOffset, bool isExportMode) const;
    // Draws markers in the component's own style: GPU via addScatter, or QPainter fallback.
    void drawPlainScatters(QCPPainter* painter, const QVector<QPointF>& dataLines,
                           const Component& comp, const QPointF& gpuOffset,
                           bool isExportMode) const;
    void requestOrigin();
    // Same-size-keeps/other-size-clears colour rule, shared by applySourceNow(),
    // commitPendingData() and dataChanged(): each installs or mutates mDataSource and
    // must apply it identically. Resets mWantOrigin when the graph ends up uncoloured.
    void applyColorSizeRule(int newSize);

    // Replacement data staged while the displayed source keeps rendering; the
    // plot commits it through commitPendingData() (see QCustomPlot::requestDataSwap).
    std::shared_ptr<QCPAbstractMultiDataSource> mPendingSource;
    std::shared_ptr<qcp::algo::MultiGraphResamplerCache> mPendingL1;
    // Colour values set while a source is staged: validated against mPendingSource's
    // size (not the still-displayed mDataSource's), applied by commitPendingData().
    std::shared_ptr<const std::vector<double>> mPendingColorValues;
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
    bool setFill(Fill fill);
    bool isValidColumn(int column) const;
    [[nodiscard]] bool isErrorBound(int column) const;
    void decorationsChanged();
    [[nodiscard]] bool hasPenDerivedDecoration(int ownerCount) const;
    void resizePerColumn(int nc, QVector<QVector<QPointF>>& lines, QVector<QVector<int>>& indices,
                         bool coloured);
    [[nodiscard]] std::pair<int, int> lineCacheWindow(const QCPAbstractMultiDataSource& ds) const;
    [[nodiscard]] bool fillDrawable(const Fill& fill, int columnCount) const;
    [[nodiscard]] static qcp::fill::Step ownerStep(const Component& owner);
    [[nodiscard]] std::vector<qcp::fill::StripPoint> fillRows(const Fill& fill,
                                                              const QCPAbstractMultiDataSource& ds,
                                                              int begin, int end) const;
    [[nodiscard]] std::vector<qcp::fill::StripPoint> shapedFillRows(
        const Fill& fill, const QCPAbstractMultiDataSource& ds, int begin, int end) const;
    void drawFills(QCPPainter* painter, const QCPAbstractMultiDataSource& ds, int begin, int end,
                   bool fresh, const QPointF& gpuOffset, bool isExportMode);
    void drawFillOnGpu(QCPPainter* painter, QCPPlottableRhiLayer* layer, std::vector<float>& verts,
                       const QColor& color, std::span<const qcp::fill::StripPoint> rows,
                       bool fresh, const QPointF& gpuOffset);
    [[nodiscard]] std::vector<qcp::fill::StripPoint> binnedWhenDense(
        std::vector<qcp::fill::StripPoint> rows) const;
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
                       int begin, int end, bool fresh, const QPointF& gpuOffset, bool isExportMode);
    void drawBarsAsPolylines(QCPPainter* painter, ErrorBarsEntry& entry,
                             std::span<const qcp::errorbars::Bar> bars, bool fresh,
                             const QPointF& gpuOffset, bool isExportMode);
    void drawBarsAsBand(QCPPainter* painter, QCPPlottableRhiLayer* gpuLayer, ErrorBarsEntry& entry,
                        std::span<const qcp::fill::StripPoint> rows, bool fresh,
                        const QPointF& gpuOffset);
    void rebuildL2(const ViewportParams& vp);
    void onViewportChanged();
    bool pipelineBusy() const override { return mPipeline.isBusy(); }

    void updateBaseSelection();

};

using QCPGraphComponent = QCPMultiGraph::Component;
