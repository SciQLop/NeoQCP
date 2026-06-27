#include "layoutelement-legend-group.h"
#include "../plottables/plottable-multigraph.h"
#include "../painting/painter.h"

QCPGroupLegendItem::QCPGroupLegendItem(QCPLegend* parent, QCPMultiGraph* multiGraph)
    : QCPAbstractLegendItem(parent)
    , mMultiGraph(multiGraph)
{
    setAntialiased(false);
}

void QCPGroupLegendItem::setExpanded(bool expanded)
{
    if (mExpanded == expanded) return;
    mExpanded = expanded;
    if (mParentPlot)
        mParentPlot->replot();
}

int QCPGroupLegendItem::rowHeight() const
{
    QFont font = mFont.pointSize() > 0 ? mFont : (mParentLegend ? mParentLegend->font() : QFont());
    return QFontMetrics(font).height() + 4;
}

QString QCPGroupLegendItem::headerName() const
{
    if (!mMultiGraph) return QString();
    if (!mMultiGraph->name().isEmpty()) return mMultiGraph->name();
    int n = mMultiGraph->componentCount();
    // No name and no components yet (e.g. async data still loading): show an
    // empty header rather than the C++ class name, which is meaningless to the
    // user. The legend item itself (and any busy indicator) still renders.
    if (n == 0) return QString();
    const QString& first = mMultiGraph->component(0).name;
    if (n == 1) return first;
    return first + QString::fromUtf8(" \u2026 ") + mMultiGraph->component(n - 1).name;
}

double QCPGroupLegendItem::selectTest(const QPointF& pos, bool onlySelectable, QVariant* details) const
{
    if (!mParentPlot || !mSelectable || !mParentLegend->selectableParts().testFlag(QCPLegend::spItems))
        return -1;
    if (onlySelectable && !mSelectable)
        return -1;
    if (!mRect.contains(pos.toPoint()))
        return -1;

    if (details) {
        int rh = rowHeight();
        double relY = pos.y() - mRect.top();
        int hitRow = static_cast<int>(relY / rh);

        QVariantMap detailMap;
        if (!mExpanded || hitRow == 0)
            detailMap[QStringLiteral("componentIndex")] = -1; // header
        else
            detailMap[QStringLiteral("componentIndex")] = qMin(hitRow - 1, mMultiGraph->componentCount() - 1);
        *details = detailMap;
    }

    return mParentPlot->selectionTolerance() * 0.99;
}

void QCPGroupLegendItem::selectEvent([[maybe_unused]] QMouseEvent* event,
                                     [[maybe_unused]] bool additive, const QVariant& details,
                                     bool* selectionStateChanged)
{

    if (!mSelectable || !mParentLegend->selectableParts().testFlag(QCPLegend::spItems))
        return;

    int componentIndex = -1;
    if (details.typeId() == QMetaType::QVariantMap)
        componentIndex = details.toMap().value(QStringLiteral("componentIndex"), -1).toInt();

    if (componentIndex < 0) {
        // Header click: toggle expand/collapse
        setExpanded(!mExpanded);
        mSelectedComponent = -1;
    } else {
        // Component click: select that component
        bool selBefore = mSelected;
        mSelectedComponent = componentIndex;
        setSelected(true);
        if (selectionStateChanged)
            *selectionStateChanged = mSelected != selBefore;
        emit componentClicked(componentIndex);
    }
}

void QCPGroupLegendItem::draw(QCPPainter* painter)
{
    if (!mMultiGraph) return;

    const bool showBusy = mMultiGraph->visuallyBusy()
        && !painter->modes().testFlag(QCPPainter::pmVectorized);

    QFont font = mSelected ? mSelectedFont : mFont;
    if (font.pointSize() <= 0 && mParentLegend)
        font = mParentLegend->font();
    QColor textColor = mSelected ? mSelectedTextColor : mTextColor;
    QFontMetrics fm(font);
    painter->setFont(font);

    QRectF inRect = mRect;
    int padding = mMargins.left();
    int iconWidth = 20;
    int rh = fm.height() + 4;
    int indent = 16;

    if (!mExpanded) {
        int n = mMultiGraph->componentCount();
        double segWidth = (n > 0) ? static_cast<double>(iconWidth) / n : iconWidth;
        double y = inRect.top() + rh / 2.0;

        if (showBusy)
        {
            painter->save();
            painter->setOpacity(mMultiGraph->effectiveBusyFadeAlpha());
        }
        for (int i = 0; i < n; ++i) {
            if (!mMultiGraph->component(i).visible) continue;
            painter->setPen(mMultiGraph->component(i).pen);
            double x0 = inRect.left() + padding + i * segWidth;
            double x1 = x0 + segWidth;
            painter->drawLine(QLineF(x0, y, x1, y));
        }
        if (showBusy)
            painter->restore();

        painter->setPen(QPen(textColor));
        QRectF textRect(inRect.left() + padding + iconWidth + 6, inRect.top(),
                        inRect.width() - padding - iconWidth - 6, rh);
        QString collapsedText = QString::fromUtf8("\u25B8 ");
        if (showBusy)
        {
            const QString prefix = mMultiGraph->effectiveBusyIndicatorSymbol();
            if (!prefix.isEmpty())
                collapsedText += prefix + QStringLiteral(" ");
        }
        collapsedText += headerName();
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, collapsedText);
    } else {
        painter->setPen(QPen(textColor));
        QRectF headerRect(inRect.left() + padding, inRect.top(),
                          inRect.width() - padding, rh);
        QString headerText = QString::fromUtf8("\u25BE ");
        if (showBusy)
        {
            const QString prefix = mMultiGraph->effectiveBusyIndicatorSymbol();
            if (!prefix.isEmpty())
                headerText += prefix + QStringLiteral(" ");
        }
        headerText += headerName();
        painter->drawText(headerRect, Qt::AlignLeft | Qt::AlignVCenter, headerText);

        for (int i = 0; i < mMultiGraph->componentCount(); ++i) {
            const auto& comp = mMultiGraph->component(i);
            double rowY = inRect.top() + (i + 1) * rh;

            if (mSelected && mSelectedComponent == i && mParentLegend) {
                painter->setPen(mParentLegend->selectedIconBorderPen());
                painter->setBrush(Qt::NoBrush);
                painter->drawRect(QRectF(inRect.left(), rowY, inRect.width(), rh));
            }

            if (showBusy)
            {
                painter->save();
                painter->setOpacity(mMultiGraph->effectiveBusyFadeAlpha());
            }
            painter->setPen(comp.pen);
            double lineY = rowY + rh / 2.0;
            painter->drawLine(QLineF(inRect.left() + padding + indent, lineY,
                                     inRect.left() + padding + indent + iconWidth, lineY));
            if (showBusy)
                painter->restore();

            painter->setPen(QPen(textColor));
            QRectF textRect(inRect.left() + padding + indent + iconWidth + 6, rowY,
                            inRect.width() - padding - indent - iconWidth - 6, rh);
            painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, comp.name);
        }
    }

    if (mSelected && mSelectedComponent < 0 && mParentLegend) {
        painter->setPen(mParentLegend->selectedIconBorderPen());
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(mRect);
    }
}

QSize QCPGroupLegendItem::minimumOuterSizeHint() const
{
    if (!mMultiGraph) return QSize(0, 0);

    QFont font = mFont.pointSize() > 0 ? mFont : (mParentLegend ? mParentLegend->font() : QFont());
    QFontMetrics fm(font);
    int rh = fm.height() + 4;
    int padding = mMargins.left() + mMargins.right();
    int iconWidth = 20;
    int indent = 16;

    auto busyPrefix = [&]() -> QString {
        if (!mMultiGraph->visuallyBusy()) return {};
        const QString sym = mMultiGraph->effectiveBusyIndicatorSymbol();
        return sym.isEmpty() ? QString() : sym + QStringLiteral(" ");
    };

    if (!mExpanded) {
        QString displayHeader = busyPrefix() + headerName();
        int textWidth = fm.horizontalAdvance(displayHeader);
        return QSize(padding + iconWidth + 6 + textWidth, rh + mMargins.top() + mMargins.bottom());
    } else {
        int maxTextWidth = fm.horizontalAdvance(QString::fromUtf8("\u25BE ") + busyPrefix() + headerName());
        for (int i = 0; i < mMultiGraph->componentCount(); ++i) {
            int w = indent + iconWidth + 6 + fm.horizontalAdvance(mMultiGraph->component(i).name);
            if (w > maxTextWidth) maxTextWidth = w;
        }
        int totalHeight = rh * (1 + mMultiGraph->componentCount());
        return QSize(padding + maxTextWidth, totalHeight + mMargins.top() + mMargins.bottom());
    }
}
