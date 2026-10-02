#pragma once
#include "plottable.h"
#include "plottable1d.h"
#include "intervals-algo.h"
#include "lane-layout.h"
#include <QPointer>
#include <optional>

class QCP_LIB_DECL QCPIntervals : public QCPAbstractPlottable
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

    double selectTest(const QPointF& pos, bool onlySelectable,
                      QVariant* details = nullptr) const override;
    QCPRange getKeyRange(bool& foundRange,
                         QCP::SignDomain inSignDomain = QCP::sdBoth) const override;
    QCPRange getValueRange(bool& foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth,
                           const QCPRange& inKeyRange = QCPRange()) const override;

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
