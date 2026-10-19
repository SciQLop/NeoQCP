#include "plottable-group.h"
#include "plottable.h"
#include "plottable-multigraph.h"
#include "../core.h"
#include "../layer.h"
#include "../painting/painter.h"
#include "../axis/labelrenderer.h"
#include <algorithm>

namespace
{
// Same dimming as a hidden plottable's row in SciQLopPlots.
constexpr int kHiddenRowAlpha = 90;

bool isShown(const QCPAbstractPlottable* plottable)
{
    if (!plottable->visible())
        return false;
    auto* multiGraph = qobject_cast<const QCPMultiGraph*>(plottable);
    if (!multiGraph)
        return true;
    for (int i = 0; i < multiGraph->componentCount(); ++i)
        if (multiGraph->component(i).visible)
            return true;
    return false;
}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPPlottableGroup
////////////////////////////////////////////////////////////////////////////////////////////////////

QCPPlottableGroup::QCPPlottableGroup(QCustomPlot* parentPlot, const QString& name)
        : QObject(parentPlot), mParentPlot(parentPlot), mName(name)
{
    connect(mParentPlot, &QCustomPlot::legendDoubleClick, this,
            [this](QCPLegend*, QCPAbstractLegendItem* item, QMouseEvent*)
            {
                if (item && item == mLegendRow)
                    emit doubleClicked();
            });
}

QCPPlottableGroup::~QCPPlottableGroup()
{
    mParentPlot->mPlottableGroups.removeOne(this);
    if (mLegendRow && mLegendRow->parentLegend())
        mLegendRow->parentLegend()->removeItem(mLegendRow);
    for (auto* member : members())
        detach(member, true);
}

void QCPPlottableGroup::setName(const QString& name)
{
    mName = name;
    markRowDirty();
}

QList<QCPAbstractPlottable*> QCPPlottableGroup::members() const
{
    QList<QCPAbstractPlottable*> live;
    for (const auto& member : mMembers)
        if (member)
            live.append(member);
    return live;
}

bool QCPPlottableGroup::addMember(QCPAbstractPlottable* plottable)
{
    if (!plottable || plottable->parentPlot() != mParentPlot)
        return false;
    if (plottable->mGroup == this)
        return true;
    if (plottable->mGroup)
        plottable->mGroup->removeMember(plottable);

    plottable->mGroup = this;
    plottable->removeFromLegend();
    mMembers.append(plottable);
    connect(plottable, &QCPLayerable::visibleChanged, this, &QCPPlottableGroup::markRowDirty);
    connect(plottable, &QCPAbstractPlottable::visuallyBusyChanged, this,
            &QCPPlottableGroup::markRowDirty);
    if (auto* multiGraph = qobject_cast<QCPMultiGraph*>(plottable))
        connect(multiGraph, &QCPMultiGraph::componentVisibilityChanged, this,
                &QCPPlottableGroup::markRowDirty);
    updateLegendRow();
    emit membersChanged();
    return true;
}

void QCPPlottableGroup::removeMember(QCPAbstractPlottable* plottable)
{
    if (!plottable || plottable->mGroup != this)
        return;
    detach(plottable, true);
    updateLegendRow();
    emit membersChanged();
}

/*! \internal
  \a restoreRow is false when the plottable is about to be deleted: no point making it a row.
*/
void QCPPlottableGroup::detach(QCPAbstractPlottable* plottable, bool restoreRow)
{
    disconnect(plottable, nullptr, this, nullptr);
    mMembers.removeAll(plottable);
    plottable->mGroup = nullptr;
    if (restoreRow && plottable->wantsLegend())
        plottable->addToLegend();
}

bool QCPPlottableGroup::visible() const
{
    const auto live = members();
    return std::any_of(live.cbegin(), live.cend(), isShown);
}

void QCPPlottableGroup::setVisible(bool visible)
{
    for (auto* member : members())
        member->setVisible(visible);
    markRowDirty();
}

void QCPPlottableGroup::updateLegendRow()
{
    QCPLegend* legend = mParentPlot->legend;
    const bool wantsRow = legend && !members().isEmpty();
    if (wantsRow && !mLegendRow)
    {
        mLegendRow = new QCPPlottableGroupLegendItem(legend, this);
        legend->addItem(mLegendRow);
    }
    else if (!wantsRow && mLegendRow)
        legend->removeItem(mLegendRow);
    else
        markRowDirty();
}

void QCPPlottableGroup::markRowDirty()
{
    if (mLegendRow && mLegendRow->layer())
        mLegendRow->layer()->markDirty();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPPlottableGroupLegendItem
////////////////////////////////////////////////////////////////////////////////////////////////////

QCPPlottableGroupLegendItem::QCPPlottableGroupLegendItem(QCPLegend* parent,
                                                         QCPPlottableGroup* group)
        : QCPAbstractLegendItem(parent), mGroup(group)
{
    setAntialiased(false);
}

QCPAbstractPlottable* QCPPlottableGroupLegendItem::iconMember() const
{
    if (!mGroup)
        return nullptr;
    const auto live = mGroup->members();
    auto shown = std::find_if(live.cbegin(), live.cend(), isShown);
    if (shown != live.cend())
        return *shown;
    return live.isEmpty() ? nullptr : live.first();
}

QCPAbstractPlottable* QCPPlottableGroupLegendItem::busyMember() const
{
    if (!mGroup)
        return nullptr;
    const auto live = mGroup->members();
    auto busy = std::find_if(live.cbegin(), live.cend(),
                             [](const QCPAbstractPlottable* p) { return p->visuallyBusy(); });
    return busy == live.cend() ? nullptr : *busy;
}

QString QCPPlottableGroupLegendItem::rowText(bool withBusySymbol) const
{
    if (!mGroup)
        return {};
    if (withBusySymbol)
        if (auto* busy = busyMember())
            if (const QString symbol = busy->effectiveBusyIndicatorSymbol(); !symbol.isEmpty())
                return symbol + QStringLiteral(" ") + mGroup->name();
    return mGroup->name();
}

QColor QCPPlottableGroupLegendItem::effectiveTextColor() const
{
    QColor color = mSelected ? mSelectedTextColor : mTextColor;
    if (mGroup && !mGroup->visible())
        color.setAlpha(kHiddenRowAlpha);
    return color;
}

void QCPPlottableGroupLegendItem::draw(QCPPainter* painter)
{
    QCPAbstractPlottable* icon = iconMember();
    if (!icon)
        return;

    const bool showBusy = busyMember() && !painter->modes().testFlag(QCPPainter::pmVectorized);
    const bool hidden = !mGroup->visible();
    const QString text = rowText(showBusy);

    painter->setFont(mSelected ? mSelectedFont : mFont);
    painter->setPen(QPen(effectiveTextColor()));
    const QSize iconSize = mParentLegend->iconSize();

    auto* renderer = QCPLabelRenderer::defaultRenderer();
    const QSize textSize = QCPLabelRenderer::measureWith(renderer, painter->font(), text);
    const QRect iconRect(mRect.topLeft(), iconSize);
    const int textHeight = qMax(textSize.height(), iconSize.height());
    QCPLabelRenderer::drawWith(
        renderer, painter,
        QRect(mRect.x() + iconSize.width() + mParentLegend->iconTextPadding(), mRect.y(),
              textSize.width(), textHeight),
        painter->font(), painter->pen().color(), text,
        Qt::TextDontClip | Qt::AlignLeft | Qt::AlignVCenter);

    painter->save();
    painter->setClipRect(iconRect, Qt::IntersectClip);
    if (hidden)
        painter->setOpacity(kHiddenRowAlpha / 255.0);
    else if (showBusy)
        painter->setOpacity(icon->effectiveBusyFadeAlpha());
    icon->drawLegendIcon(painter, iconRect);
    painter->restore();

    const QPen border = mSelected ? mParentLegend->selectedIconBorderPen()
                                  : mParentLegend->iconBorderPen();
    if (border.style() != Qt::NoPen)
    {
        painter->setPen(border);
        painter->setBrush(Qt::NoBrush);
        const int halfPen = qCeil(painter->pen().widthF() * 0.5) + 1;
        painter->setClipRect(mOuterRect.adjusted(-halfPen, -halfPen, halfPen, halfPen));
        painter->drawRect(iconRect);
    }
}

QSize QCPPlottableGroupLegendItem::minimumOuterSizeHint() const
{
    if (!iconMember())
        return {};
    const QSize iconSize = mParentLegend->iconSize();
    const QSize textSize = QCPLabelRenderer::measureWith(
        QCPLabelRenderer::defaultRenderer(), mSelected ? mSelectedFont : mFont, rowText());
    return { iconSize.width() + mParentLegend->iconTextPadding() + textSize.width()
                 + mMargins.left() + mMargins.right(),
             qMax(textSize.height(), iconSize.height()) + mMargins.top() + mMargins.bottom() };
}
