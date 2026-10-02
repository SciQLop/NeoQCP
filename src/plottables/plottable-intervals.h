#pragma once
#include "plottable.h"
#include "plottable1d.h"
#include "intervals-algo.h"
#include "lane-layout.h"
#include <QPointer>
#include <optional>

class QCP_LIB_DECL QCPIntervals : public QCPAbstractPlottable, public QCPPlottableInterface1D
{
    Q_OBJECT
public:
    QCPIntervals(QCPAxis* keyAxis, QCPAxis* valueAxis, QCPLaneLayout* layout);

    void setData(qcp::intervals::Columns columns);
    [[nodiscard]] const qcp::intervals::Columns& columns() const { return mColumns; }
    [[nodiscard]] int rowCount() const { return static_cast<int>(mColumns.start.size()); }
    [[nodiscard]] QCPLaneLayout* laneLayout() const { return mLayout; }

    void setCategoryColors(const QVector<QColor>& colors);
    [[nodiscard]] QColor categoryColor(int category) const;
    [[nodiscard]] quint64 buildCount() const { return mBuildCount; }
    [[nodiscard]] QCPLayer* labelLayer() const;

    enum HitPart { hpNone, hpEmpty, hpBody, hpLeftEdge, hpRightEdge };

    struct Hit
    {
        int row = -1;
        int lane = -1;
        HitPart part = hpNone;
    };

    [[nodiscard]] Hit hitTest(const QPointF& pos) const;
    [[nodiscard]] QVector<int> selectedRows() const;
    [[nodiscard]] QVector<qint64> selectedIds() const;
    void setSelectedRows(const QVector<int>& rows);
    [[nodiscard]] QPointF pixelOf(double key, int lane) const;

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

protected:
    struct BuildKey
    {
        double lower, upper;
        QRect rect;
        quint64 data, layout, colors;
        bool operator==(const BuildKey&) const = default;
    };

    void draw(QCPPainter* painter) override;
    void drawLegendIcon(QCPPainter* painter, const QRectF& rect) const override;

    void regroupIfLanesWereAdded();
    [[nodiscard]] BuildKey currentBuildKey() const;
    void rebuildBarsIfNeeded();
    void rebuildBars();
    void appendLaneBars(int lane, const QCPLaneBand& band);
    void appendLabelRect(const qcp::intervals::PixelBar& bar);
    void drawLabels(QCPPainter* painter) const;
    void drawLaneNames(QCPPainter* painter) const;
    [[nodiscard]] bool drawsLaneNames() const;
    [[nodiscard]] std::array<float, 4> rgba(int category) const;
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
    [[nodiscard]] QVector<int> rowsInRect(const QRectF& rect) const;
    void drawSelection(QCPPainter* painter) const;

    std::optional<QRectF> mRubberBand;

    QPointer<QCPLaneLayout> mLayout;
    qcp::intervals::Columns mColumns;
    std::vector<qcp::intervals::LaneRows> mLanes;
    QVector<QColor> mCategoryColors;
    std::vector<qcp::intervals::PixelBar> mBars;
    std::vector<float> mVertices;
    std::vector<std::pair<QRectF, int>> mLabelRects;
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
