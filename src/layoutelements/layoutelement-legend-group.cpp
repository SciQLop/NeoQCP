#include "layoutelement-legend-group.h"
#include "../plottables/plottable-graph.h"
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

QCPGroupLegendItem::QCPGroupLegendItem(QCPLegend* parent, QCPGraph* graph)
    : QCPPlottableLegendItem(parent, graph)
{
    if (graph)
        connect(graph, &QCPGraph::componentVisibilityChanged, this,
                [this] { if (QCPLayer* legendLayer = layer()) legendLayer->markDirty(); });
}

QCPHandle<QCPGraph> QCPGroupLegendItem::graph() const
{
    return QCPHandle<QCPGraph>(graphRaw());
}

QCPGraph* QCPGroupLegendItem::graphRaw() const
{
    return static_cast<QCPGraph*>(mPlottable.data());
}

void QCPGroupLegendItem::setExpanded(bool expanded)
{
    if (mExpanded == expanded)
        return;
    markDirty();
    mExpanded = expanded;
}

// One component has nothing to expand to: its row would repeat the header. It is drawn like a plain
// legend entry, even when selecting the component asked for expansion.
bool QCPGroupLegendItem::shownExpanded() const
{
    return mExpanded && graphRaw() && graphRaw()->legendComponents().size() > 1;
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
    if (shownExpanded())
        for (int i : graphRaw()->legendComponents())
            height = std::max(height, measured(font, graphRaw()->component(i).name).height());
    return height + 4;
}

QString QCPGroupLegendItem::headerName() const
{
    if (!graphRaw()) return QString();
    const QVector<int> shown = graphRaw()->legendComponents();
    // One shown component stands for the graph, as 2.x's row did: the graph's name, unless the
    // component was named.
    // simplify: "named" means the name differs from the default one, so a component named exactly
    // "Component 0" by hand shows the graph's name. A flag set by every name write is the upgrade.
    if (shown.size() == 1)
    {
        const QString& only = graphRaw()->component(shown.first()).name;
        return only == QCPGraph::defaultComponentName(shown.first()) ? graphRaw()->name()
                                                                           : only;
    }
    if (!graphRaw()->name().isEmpty()) return graphRaw()->name();
    // No name and no components yet (e.g. async data still loading): show an
    // empty header rather than the C++ class name, which is meaningless to the
    // user. The legend item itself (and any busy indicator) still renders.
    if (shown.isEmpty()) return QString();
    const QString& first = graphRaw()->component(shown.first()).name;
    return first + QString::fromUtf8(" \u2026 ") + graphRaw()->component(shown.last()).name;
}

QString QCPGroupLegendItem::headerRowText(bool includeBusySymbol) const
{
    // ▾ when expanded (collapse toward the edge), ▸ when collapsed, nothing for one component.
    QString text;
    if (graphRaw() && graphRaw()->legendComponents().size() > 1)
        text = QString::fromUtf8(shownExpanded() ? "▾ " : "▸ ");
    if (includeBusySymbol && graphRaw() && graphRaw()->visuallyBusy())
    {
        const QString symbol = graphRaw()->effectiveBusyIndicatorSymbol();
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
        const QVector<int> shown = graphRaw() ? graphRaw()->legendComponents() : QVector<int>();
        if (!shownExpanded() || hitRow == 0 || shown.isEmpty())
            detailMap[QStringLiteral("componentIndex")] = -1; // header
        else
            detailMap[QStringLiteral("componentIndex")] = shown[qMin(hitRow - 1, shown.size() - 1)];
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

    const QVector<int> shown = graphRaw() ? graphRaw()->legendComponents() : QVector<int>();
    // Nothing to expand: a click selects the line, like a plain entry
    if (componentIndex < 0 && shown.size() == 1)
        componentIndex = shown.first();

    if (componentIndex < 0) {
        // Header click: toggle expand/collapse
        mSelectedComponent = -1;
        setExpanded(!mExpanded); // last: it replots, which reaches user slots
    } else {
        // Component click: select that component
        bool selBefore = mSelected;
        mSelectedComponent = componentIndex;
        const QPointer<QCPGroupLegendItem> alive(this);
        setSelected(true);
        if (selectionStateChanged)
            *selectionStateChanged = !alive || mSelected != selBefore;
        if (alive)
            qcpEmit(this, mParentPlot, &QCPGroupLegendItem::componentClicked, componentIndex);
    }
}

void QCPGroupLegendItem::draw(QCPPainter* painter)
{
    if (!graphRaw()) return;

    const bool showBusy = graphRaw()->visuallyBusy()
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

    if (!shownExpanded()) {
        const QVector<int> shown = graphRaw()->legendComponents();
        double segWidth = shown.isEmpty() ? iconWidth : static_cast<double>(iconWidth) / shown.size();
        double y = inRect.top() + rh / 2.0;

        if (showBusy)
        {
            painter->save();
            painter->setOpacity(graphRaw()->effectiveBusyFadeAlpha());
        }
        for (int k = 0; k < shown.size(); ++k) {
            const int i = shown[k];
            if (!graphRaw()->component(i).visible) continue;
            double x0 = inRect.left() + padding + k * segWidth;
            double x1 = x0 + segWidth;
            graphRaw()->drawComponentLegendIcon(painter, i, QLineF(x0, y, x1, y));
        }
        if (graphRaw()->componentCount() == 0)
            graphRaw()->drawEmptyLegendIcon(
                painter, QLineF(inRect.left() + padding, y, inRect.left() + padding + iconWidth, y));
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

        const QVector<int> shown = graphRaw()->legendComponents();
        for (int k = 0; k < shown.size(); ++k) {
            const int i = shown[k];
            const auto& comp = graphRaw()->component(i);
            double rowY = inRect.top() + (k + 1) * rh;

            if (mSelected && mSelectedComponent == i && mParentLegend) {
                painter->setPen(mParentLegend->selectedIconBorderPen());
                painter->setBrush(Qt::NoBrush);
                painter->drawRect(QRectF(inRect.left(), rowY, inRect.width(), rh));
            }

            if (showBusy)
            {
                painter->save();
                painter->setOpacity(graphRaw()->effectiveBusyFadeAlpha());
            }
            double lineY = rowY + rh / 2.0;
            graphRaw()->drawComponentLegendIcon(painter, i, QLineF(inRect.left() + padding + indent, lineY,
                                     inRect.left() + padding + indent + iconWidth, lineY));
            if (showBusy)
                painter->restore();

            painter->setPen(QPen(textColor));
            QRectF textRect(inRect.left() + padding + indent + iconWidth + 6, rowY,
                            inRect.width() - padding - indent - iconWidth - 6, rh);
            drawLabel(painter, textRect, comp.name);
        }
    }

    if (mSelected && (mSelectedComponent < 0 || !shownExpanded()) && mParentLegend) {
        painter->setPen(mParentLegend->selectedIconBorderPen());
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(mRect);
    }
}

QSize QCPGroupLegendItem::minimumOuterSizeHint() const
{
    if (!graphRaw()) return QSize(0, 0);

    const QFont font = rowFont();
    int rh = rowHeight();
    int padding = mMargins.left() + mMargins.right();
    int iconWidth = 20;
    int indent = 16;

    const int headerWidth = measured(font, headerRowText(true)).width();

    if (!shownExpanded()) {
        return QSize(padding + iconWidth + 6 + headerWidth,
                     rh + mMargins.top() + mMargins.bottom());
    } else {
        int maxTextWidth = headerWidth;
        const QVector<int> shown = graphRaw()->legendComponents();
        for (int i : shown) {
            int w = indent + iconWidth + 6 + measured(font, graphRaw()->component(i).name).width();
            if (w > maxTextWidth) maxTextWidth = w;
        }
        int totalHeight = rh * (1 + shown.size());
        return QSize(padding + maxTextWidth, totalHeight + mMargins.top() + mMargins.bottom());
    }
}
