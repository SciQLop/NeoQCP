#include "layoutelement-legend-intervals.h"
#include "../painting/painter.h"
#include "../plottables/plottable-intervals.h"

namespace
{
constexpr int kSwatchWidth = 16, kSwatchGap = 6;
}

QCPIntervalsLegendItem::QCPIntervalsLegendItem(QCPLegend* parent, QCPIntervals* intervals)
        : QCPAbstractLegendItem(parent), mIntervals(intervals)
{
}

QFont QCPIntervalsLegendItem::effectiveFont() const
{
    const QFont font = mSelected ? mSelectedFont : mFont;
    return font.pointSize() > 0 || !mParentLegend ? font : mParentLegend->font();
}

QSize QCPIntervalsLegendItem::minimumOuterSizeHint() const
{
    const QFontMetrics fm(effectiveFont());
    const auto entries = mIntervals->legendEntries();
    int textWidth = 0;
    for (const auto& [name, color] : entries)
        textWidth = std::max(textWidth, fm.horizontalAdvance(name));
    const int rows = std::max<int>(1, static_cast<int>(entries.size()));
    return { mMargins.left() + kSwatchWidth + kSwatchGap + textWidth + mMargins.right(),
             mMargins.top() + rows * (fm.height() + 4) + mMargins.bottom() };
}

void QCPIntervalsLegendItem::draw(QCPPainter* painter)
{
    const QFont font = effectiveFont();
    const QFontMetrics fm(font);
    const int rowHeight = fm.height() + 4;
    painter->setFont(font);
    double top = mRect.top();
    for (const auto& [name, color] : mIntervals->legendEntries())
    {
        const QRectF swatch(mRect.left() + mMargins.left(), top + rowHeight / 4.0, kSwatchWidth,
                            rowHeight / 2.0);
        painter->fillRect(swatch, color);
        painter->setPen(mSelected ? mSelectedTextColor : mTextColor);
        painter->drawText(QRectF(swatch.right() + kSwatchGap, top, mRect.width(), rowHeight),
                          Qt::AlignLeft | Qt::AlignVCenter, name);
        top += rowHeight;
    }
}
