#pragma once
#include "plottable.h"
#include "plottable1d.h"
#include "intervals-algo.h"
#include "lane-layout.h"
#include <QKeyEvent>
#include <QPointer>
#include <optional>

struct QCPIntervalEdit
{
    qint64 id;
    double start, stop;
    int lane;
};
Q_DECLARE_METATYPE(QCPIntervalEdit)

class QCP_LIB_DECL QCPIntervals : public QCPAbstractPlottable, public QCPPlottableInterface1D
{
    Q_OBJECT
public:
    QCPIntervals(QCPAxis* keyAxis, QCPAxis* valueAxis, QCPLaneLayout* layout);
    ~QCPIntervals() override;

    void setData(qcp::intervals::Columns columns);
    [[nodiscard]] const qcp::intervals::Columns& columns() const { return mColumns; }
    [[nodiscard]] int rowCount() const { return static_cast<int>(mColumns.start.size()); }
    [[nodiscard]] QCPLaneLayout* laneLayout() const { return mLayout; }

    void setCategoryColors(const QVector<QColor>& colors);
    //! Names of the category indices, for the legend.
    void setCategoryNames(const QStringList& names);
    //! (name, colour) of each category the data uses, first seen first.
    [[nodiscard]] std::vector<std::pair<QString, QColor>> legendEntries() const;
    using QCPAbstractPlottable::addToLegend;
    using QCPAbstractPlottable::removeFromLegend;
    bool addToLegend(QCPLegend* legend) override;
    bool removeFromLegend(QCPLegend* legend) const override;
    [[nodiscard]] QColor categoryColor(int category) const;
    [[nodiscard]] quint64 buildCount() const { return mBuildCount; }
    [[nodiscard]] QCPLayer* labelLayer() const;
    [[nodiscard]] QList<QCPLayer*> dependentLayers() const override;

    enum HitPart { hpNone, hpEmpty, hpBody, hpLeftEdge, hpRightEdge };

    struct Hit
    {
        int row = -1;
        int lane = -1;
        HitPart part = hpNone;
    };

    enum EditMode { emMove = 0x01, emResize = 0x02, emChangeLane = 0x04, emCreate = 0x08, emDelete = 0x10 };
    Q_DECLARE_FLAGS(EditModes, EditMode)

    enum SnapMode { snNone, snEdges, snStep, snTimes };
    //! stBars: plain coloured bars. stWave: a logic-analyzer look, with angled bus-value ends,
    //! an idle line through each lane and every other lane shaded.
    enum Style { stBars, stWave };
    //! omDraw: overlapping intervals are drawn over each other. omForbid: move, resize, create
    //! and nudges stop at the neighbouring block's edge, so an edit never overlaps one.
    //! omStack: overlapping intervals of a lane go into sub-rows.
    enum OverlapMode { omDraw, omStack, omForbid };
    void setOverlapMode(OverlapMode mode);
    [[nodiscard]] OverlapMode overlapMode() const { return mOverlap; }

    void setStyle(Style style);
    [[nodiscard]] Style style() const { return mStyle; }

    [[nodiscard]] Hit hitTest(const QPointF& pos) const;
    [[nodiscard]] QVector<int> selectedRows() const;
    [[nodiscard]] QVector<qint64> selectedIds() const;
    void setSelectedRows(const QVector<int>& rows);
    [[nodiscard]] QVector<int> rowsWithIds(const QVector<qint64>& ids) const;
    [[nodiscard]] QPointF pixelOf(double key, int lane) const;

    void setEditable(bool editable);
    [[nodiscard]] bool editable() const { return mEditable; }
    void setEditModes(EditModes modes);
    [[nodiscard]] EditModes editModes() const { return mEditModes; }
    void setSnap(SnapMode mode, double step = 0);
    [[nodiscard]] SnapMode snapMode() const { return mSnap; }
    [[nodiscard]] double snapStep() const { return mSnapStep; }
    //! Snap dragged edges to these keys only (e.g. orbit events); sets the snTimes mode.
    void setSnapTimes(std::vector<double> times);
    [[nodiscard]] const std::vector<double>& snapTimes() const { return mSnapTimes; }
    [[nodiscard]] std::optional<Qt::CursorShape> cursorAt(const QPointF& pos) const;
    [[nodiscard]] bool gestureActive() const { return mGesture.has_value(); }

    bool keyPress(QKeyEvent* event) override;

    double selectTest(const QPointF& pos, bool onlySelectable,
                      QVariant* details = nullptr) const override;
    QCPRange getKeyRange(bool& foundRange,
                         QCP::SignDomain inSignDomain = QCP::sdBoth) const override;
    QCPRange getValueRange(bool& foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth,
                           const QCPRange& inKeyRange = QCPRange()) const override;

    QCPPlottableInterface1D* interface1D() override { return this; }

    // QCPPlottableInterface1D
    [[nodiscard]] int dataCount() const override { return rowCount(); }
    [[nodiscard]] double dataMainKey(int index) const override { return mColumns.start[index]; }
    [[nodiscard]] double dataSortKey(int index) const override { return mColumns.start[index]; }
    [[nodiscard]] double dataMainValue(int index) const override { return mColumns.lane[index]; }
    [[nodiscard]] QCPRange dataValueRange(int index) const override
    {
        return QCPRange(mColumns.lane[index], mColumns.lane[index]);
    }
    [[nodiscard]] QPointF dataPixelPosition(int index) const override
    {
        return pixelOf(mColumns.start[index], mColumns.lane[index]);
    }
    [[nodiscard]] bool sortKeyIsMainKey() const override { return false; }
    QCPDataSelection selectTestRect(const QRectF& rect, bool onlySelectable) const override;
    [[nodiscard]] int findBegin(double sortKey, bool expandedRange = true) const override;
    [[nodiscard]] int findEnd(double sortKey, bool expandedRange = true) const override;

Q_SIGNALS:
    void intervalsEdited(const QVector<QCPIntervalEdit>& edits);
    void intervalCreated(double start, double stop, int lane);
    void deleteRequested(const QVector<qint64>& ids);

protected:
    struct BuildKey
    {
        double lower, upper;
        QRect rect;
        quint64 data, layout, colors;
        Style style;
        bool operator==(const BuildKey&) const = default;
    };

    void draw(QCPPainter* painter) override;
    void drawLegendIcon(QCPPainter* painter, const QRectF& rect) const override;

    void regroupIfLanesWereAdded();
    void updateSubRows();
    [[nodiscard]] int subRowOf(int row) const { return mSubRow.empty() ? 0 : mSubRow[row]; }
    //! The vertical band of sub-row \a subRow of \a lane (the whole lane when not stacking).
    [[nodiscard]] std::optional<QCPLaneBand> rowBand(int lane, int subRow) const;
    [[nodiscard]] BuildKey currentBuildKey() const;
    void rebuildBarsIfNeeded();
    void rebuildBars();
    void appendLaneBars(int lane, const QCPLaneBand& band);
    [[nodiscard]] bool sameLabelAsLastBar(int row) const;
    void appendLabelRect(const qcp::intervals::PixelBar& bar);
    void appendLaneBackdrops(const QRect& axisRect);
    [[nodiscard]] QPolygonF barShape(const qcp::intervals::PixelBar& bar) const;
    [[nodiscard]] QColor fillColor(int category) const;
    void drawLabels(QCPPainter* painter) const;
    void drawLaneNames(QCPPainter* painter) const;
    [[nodiscard]] bool drawsLaneNames() const;
    [[nodiscard]] double fillOpacity() const;
    bool drawBarsOnGpu(QCPPainter* painter);
    void drawBarsWithPainter(QCPPainter* painter) const;

    void selectEvent(QMouseEvent* event, bool additive, const QVariant& details,
                     bool* selectionStateChanged) override;
    void deselectEvent(bool* selectionStateChanged) override;
    void mousePressEvent(QMouseEvent* event, const QVariant& details) override;
    void mouseMoveEvent(QMouseEvent* event, const QPointF& startPos) override;
    void mouseReleaseEvent(QMouseEvent* event, const QPointF& startPos) override;

    [[nodiscard]] QRectF barRect(int row) const;
    [[nodiscard]] bool siblingHasBarAt(const QPointF& pos) const;
    [[nodiscard]] QVector<int> rowsInRect(const QRectF& rect) const;
    void drawSelection(QCPPainter* painter) const;

    struct Gesture
    {
        enum Kind { Move, ResizeLeft, ResizeRight, Create } kind;
        QPointF pressPos;
        double pressKey = 0;
        int pressLane = -1;
        std::vector<qcp::intervals::DraggedRow> rows; // the grabbed row first
        std::vector<double> snapCandidates;
        std::vector<qcp::intervals::Edit> preview;
        bool moved = false;
    };

    static qcp::intervals::DragKind dragKind(Gesture::Kind kind);
    [[nodiscard]] std::optional<Gesture::Kind> gestureKindFor(const Hit& hit) const;
    bool startGesture(const Hit& hit, const QPointF& pos);
    void updateGesture(const QPointF& pos);
    void finishGesture();
    [[nodiscard]] std::vector<qcp::intervals::DraggedRow> draggedRowsFor(const Hit& hit, Gesture::Kind kind) const;
    [[nodiscard]] std::vector<double> snapCandidatesExcluding(const std::vector<qcp::intervals::DraggedRow>& rows) const;
    [[nodiscard]] double snappedDelta(double raw) const;
    [[nodiscard]] double snappedToStep(double key) const;
    [[nodiscard]] int laneStepsTo(const QPointF& pos) const;
    [[nodiscard]] double keysPerPixels(double px) const;
    void markLayersDirty();
    void requestRepaint();
    [[nodiscard]] bool changesAnyRow(const std::vector<qcp::intervals::Edit>& edits) const;
    void emitEdits(const std::vector<qcp::intervals::Edit>& edits);
    void drawPreview(QCPPainter* painter) const;
    bool nudge(int keySteps, int laneSteps);
    struct Shift
    {
        double dt;
        int laneSteps;
    };
    [[nodiscard]] Shift withoutOverlaps(const std::vector<qcp::intervals::DraggedRow>& rows,
                                        qcp::intervals::DragKind kind, Shift wanted) const;
    [[nodiscard]] double createWithoutOverlaps(double pressKey, int lane, double dt) const;
    [[nodiscard]] std::vector<qcp::intervals::Span>
    obstaclesIn(int lane, const std::vector<qcp::intervals::DraggedRow>& dragged) const;

    bool mEditable = false;
    EditModes mEditModes = EditModes(emMove | emResize);
    SnapMode mSnap = snNone;
    double mSnapStep = 0;
    std::vector<double> mSnapTimes; // sorted
    std::optional<Gesture> mGesture;

    std::optional<QRectF> mRubberBand;

    QPointer<QCPLaneLayout> mLayout;
    qcp::intervals::Columns mColumns;
    std::vector<qcp::intervals::LaneRows> mLanes;
    QVector<QColor> mCategoryColors;
    QStringList mCategoryNames;
    std::vector<qcp::intervals::PixelBar> mBars;
    struct Shape
    {
        QPolygonF polygon;
        QColor color;
    };
    struct LabelRect
    {
        QRectF rect;
        int row;
        QString text;
    };
    std::vector<Shape> mShapes; // what both the GPU and the painter path draw, bottom first
    std::vector<float> mVertices;
    std::vector<LabelRect> mLabelRects;
    Style mStyle = stBars;
    OverlapMode mOverlap = omDraw;
    std::vector<int> mSubRow; // per row, when stacking
    std::optional<BuildKey> mBuiltFor;
    quint64 mDataGeneration = 0;
    quint64 mColorGeneration = 0;
    quint64 mBuildCount = 0;

    // The GPU bar path renders its vertex quads after its own layer's painter
    // output is composited (see QCustomPlot::render), so text drawn by
    // QCPIntervals::draw() on the same layer would end up underneath the bars.
    // This layerable lives one layer above and draws the labels/lane names instead.
    class LabelLayer;
    LabelLayer* mLabelLayer = nullptr;

    friend class TestIntervals;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QCPIntervals::EditModes)
