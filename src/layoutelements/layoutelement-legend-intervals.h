#pragma once
#include "layoutelement-legend.h"
#include <QPointer>

class QCPIntervals;

//! One legend row per category an interval plottable uses: a colour swatch and the name.
class QCP_LIB_DECL QCPIntervalsLegendItem : public QCPLegendItem
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(QCPIntervalsLegendItem)
public:
    QCPIntervalsLegendItem(QCPLegend* parent, QCPIntervals* intervals);

    QCPHandle<QCPIntervals> intervals() const;
    QSize minimumOuterSizeHint() const override;

protected:
    void draw(QCPPainter* painter) override;

private:
    QFont effectiveFont() const;
    QPointer<QCPIntervals> mIntervals;
};
