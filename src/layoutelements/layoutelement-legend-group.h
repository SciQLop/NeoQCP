#pragma once
#include "layoutelement-legend.h"
#include "../handle.h"
#include <QPointer>

class QCPGraph;

//! A graph's legend row: a plottable's row (itemWithPlottable finds it) that draws one segment per
//! shown component and expands to one row each.
class QCP_LIB_DECL QCPGroupLegendItem : public QCPPlottableLegendItem
{
    Q_OBJECT
public:
    QCPGroupLegendItem(QCPLegend* parent, QCPGraph* graph);

    //! The graph this row shows.
    [[nodiscard]] QCPHandle<QCPGraph> graph() const;
    bool expanded() const { return mExpanded; }
    void setExpanded(bool expanded);
    int selectedComponent() const { return mSelectedComponent; }
    void setSelectedComponent(int index) { mSelectedComponent = index; }

    QSize minimumOuterSizeHint() const override;
    double selectTest(const QPointF& pos, bool onlySelectable, QVariant* details = nullptr) const override;

    // The title shown for the group (collapsed header / expanded header row).
    QString headerName() const;

    // Full text of the header row: expander marker, optional busy symbol, then
    // headerName(). draw() and minimumOuterSizeHint() must build it the same
    // way or the row is laid out too narrow and drawText clips it.
    QString headerRowText(bool includeBusySymbol) const;

signals:
    void componentClicked(int componentIndex);

protected:
    void draw(QCPPainter* painter) override;
    void selectEvent(QMouseEvent* event, bool additive, const QVariant& details, bool* selectionStateChanged) override;

private:
    friend class TestMultiGraph;
    bool shownExpanded() const;
    QFont rowFont() const;
    int rowHeight() const;
    //! The base's plottable: this row is only ever made for a graph.
    [[nodiscard]] QCPGraph* graphRaw() const;
    bool mExpanded = false;
    int mSelectedComponent = -1;
};
