#include "layoutelement-legend-group.h"
#include "../plottables/plottable-multigraph.h"
#include "../painting/painter.h"
#include "../axis/labelrenderer.h"

namespace {

// Through the label renderer, like QCPPlottableLegendItem: a name such as $\frac{a}{b}$ is typeset.
QSize measured(const QFont& font, const QString& text)
{
    return QCPLabelRenderer::measureWith(QCPLabelRenderer::defaultRenderer(), font, text);
}

void drawLabel(QCPPainter* painter, const QRectF& rect, const QString& text)
{
    QCPLabelRenderer::drawWith(QCPLabelRenderer::defaultRenderer(), painter, rect.toRect(),
                               painter->font(), painter->pen().color(), text,
                               Qt::TextDontClip | Qt::AlignLeft | Qt::AlignVCenter);
}

} // namespace

QCPGroupLegendItem::QCPGroupLegendItem(QCPLegend* parent, QCPMultiGraph* multiGraph)
    : QCPAbstractLegendItem(parent)
    , mMultiGraph(multiGraph)
{
    setAntialiased(false);
    repaintWhenBusyChanges(multiGraph);
    if (multiGraph)
        connect(multiGraph, &QCPMultiGraph::componentVisibilityChanged, this,
                [this] { if (auto* legendLayer = layer()) legendLayer->markDirty(); });
}

void QCPGroupLegendItem::setExpanded(bool expanded)
{
    if (mExpanded == expanded) return;
    mExpanded = expanded;
    if (mParentPlot)
        mParentPlot->replot();
}

QFont QCPGroupLegendItem::rowFont() const
{
    return mFont.pointSize() > 0 ? mFont : (mParentLegend ? mParentLegend->font() : QFont());
}

// Rows share one height so selectTest can find a row by division; the tallest typeset name sets it.
int QCPGroupLegendItem::rowHeight() const
{
    const QFont font = rowFont();
    int height = std::max(QFontMetrics(font).height(), measured(font, headerRowText(true)).height());
    if (mExpanded && mMultiGraph)
        for (int i = 0; i < mMultiGraph->componentCount(); ++i)
            height = std::max(height, measured(font, mMultiGraph->component(i).name).height());
    return height + 4;
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

QString QCPGroupLegendItem::headerRowText(bool includeBusySymbol) const
{
    // ▾ when expanded (collapse toward the edge), ▸ when collapsed.
    QString text = QString::fromUtf8(mExpanded ? "▾ " : "▸ ");
    if (includeBusySymbol && mMultiGraph && mMultiGraph->visuallyBusy())
    {
        const QString symbol = mMultiGraph->effectiveBusyIndicatorSymbol();
        if (!symbol.isEmpty())
            text += symbol + QStringLiteral(" ");
    }
    return text + headerName();
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
    painter->setFont(font);

    QRectF inRect = mRect;
    int padding = mMargins.left();
    int iconWidth = 20;
    int rh = rowHeight();
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
            double x0 = inRect.left() + padding + i * segWidth;
            double x1 = x0 + segWidth;
            mMultiGraph->drawComponentLegendLine(painter, i, QLineF(x0, y, x1, y));
        }
        if (showBusy)
            painter->restore();

        painter->setPen(QPen(textColor));
        QRectF textRect(inRect.left() + padding + iconWidth + 6, inRect.top(),
                        inRect.width() - padding - iconWidth - 6, rh);
        drawLabel(painter, textRect, headerRowText(showBusy));
    } else {
        painter->setPen(QPen(textColor));
        QRectF headerRect(inRect.left() + padding, inRect.top(),
                          inRect.width() - padding, rh);
        drawLabel(painter, headerRect, headerRowText(showBusy));

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
            double lineY = rowY + rh / 2.0;
            mMultiGraph->drawComponentLegendLine(painter, i, QLineF(inRect.left() + padding + indent, lineY,
                                     inRect.left() + padding + indent + iconWidth, lineY));
            if (showBusy)
                painter->restore();

            painter->setPen(QPen(textColor));
            QRectF textRect(inRect.left() + padding + indent + iconWidth + 6, rowY,
                            inRect.width() - padding - indent - iconWidth - 6, rh);
            drawLabel(painter, textRect, comp.name);
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

    const QFont font = rowFont();
    int rh = rowHeight();
    int padding = mMargins.left() + mMargins.right();
    int iconWidth = 20;
    int indent = 16;

    const int headerWidth = measured(font, headerRowText(true)).width();

    if (!mExpanded) {
        return QSize(padding + iconWidth + 6 + headerWidth,
                     rh + mMargins.top() + mMargins.bottom());
    } else {
        int maxTextWidth = headerWidth;
        for (int i = 0; i < mMultiGraph->componentCount(); ++i) {
            int w = indent + iconWidth + 6 + measured(font, mMultiGraph->component(i).name).width();
            if (w > maxTextWidth) maxTextWidth = w;
        }
        int totalHeight = rh * (1 + mMultiGraph->componentCount());
        return QSize(padding + maxTextWidth, totalHeight + mMargins.top() + mMargins.bottom());
    }
}
