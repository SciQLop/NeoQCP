#pragma once
#include "layoutelement-legend.h"
#include <QPointer>

class QCPIntervals;

//! One legend row per category an interval plottable uses: a colour swatch and the name.
class QCP_LIB_DECL QCPIntervalsLegendItem : public QCPAbstractLegendItem
{
    Q_OBJECT
public:
    QCPIntervalsLegendItem(QCPLegend* parent, QCPIntervals* intervals);

    QCPIntervals* intervals() const;
    QSize minimumOuterSizeHint() const override;

protected:
    void draw(QCPPainter* painter) override;

private:
    QFont effectiveFont() const;
    QPointer<QCPIntervals> mIntervals;
};
