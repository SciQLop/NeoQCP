#include "plottable-group.h"
#include "plottable.h"
#include "plottable-graph.h"
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
    auto* graph = qobject_cast<const QCPGraph*>(plottable);
    // A graph with no data yet has no component the user could have hidden: it is shown.
    if (!graph || graph->componentCount() == 0)
        return true;
    for (int i = 0; i < graph->componentCount(); ++i)
        if (graph->component(i).visible)
            return true;
    return false;
}

// A graph is shown when visible with a visible component, or none yet (see isShown), so showing or hiding
// it as a group member sets its components too.
void setShown(const QPointer<QCPAbstractPlottable>& plottable, bool shown)
{
    plottable->setVisible(shown);
    if (auto* graph = qobject_cast<QCPGraph*>(plottable.data()))
        for (int i = 0; graph && i < graph->componentCount(); ++i)
            graph->setComponentVisible(i, shown);
}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPPlottableGroup
////////////////////////////////////////////////////////////////////////////////////////////////////

/*! \fn void QCPPlottableGroup::nameChanged(const QString& name)

  This signal is emitted when \ref name changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPlottableGroup::visibleChanged(bool visible)

  This signal is emitted when \ref visible changes, after the plot delivers its deferred
  signals.
*/

QCPPlottableGroup::QCPPlottableGroup(QCustomPlot* parentPlot, const QString& name)
        : QObject(parentPlot), mParentPlot(parentPlot), mName(name)
{
    connect(mParentPlot, &QCustomPlot::legendDoubleClick, this,
            [this](QCPLegend*, QCPAbstractLegendItem* item, QMouseEvent*)
            {
                if (item && item == mLegendRow)
                    qcpEmit(this, mParentPlot, &QCPPlottableGroup::doubleClicked);
            });
}

QCPPlottableGroup::~QCPPlottableGroup()
{
    mParentPlot->mPlottableGroups.removeOne(this);
    if (mLegendRow && mLegendRow->parentLegend())
        mLegendRow->parentLegend()->removeItem(mLegendRow);
    for (auto* member : membersRaw())
        detach(member, true);
}

void QCPPlottableGroup::setName(const QString& name)
{
    if (name == mName)
        return;
    markRowDirty(); // the only surface a group draws: its legend row
    mName = name;
    markRowDirty();
    qcpEmit(this, mParentPlot, &QCPPlottableGroup::nameChanged, mName);
}

QList<QCPAbstractPlottable*> QCPPlottableGroup::membersRaw() const
{
    QList<QCPAbstractPlottable*> live;
    for (const auto& member : mMembers)
        if (member)
            live.append(member);
    return live;
}

QList<QCPHandle<QCPAbstractPlottable>> QCPPlottableGroup::members() const
{
    QList<QCPHandle<QCPAbstractPlottable>> live;
    for (QCPAbstractPlottable* member : membersRaw())
        live.append(QCPHandle<QCPAbstractPlottable>(member));
    return live;
}

QCPHandle<QCustomPlot> QCPPlottableGroup::parentPlot() const
{
    return QCPHandle<QCustomPlot>(mParentPlot);
}

QCPHandle<QCPPlottableGroup> QCPAbstractPlottable::group() const
{
    return QCPHandle<QCPPlottableGroup>(mGroup);
}

bool QCPPlottableGroup::addMember(QCPAbstractPlottable* plottable)
{
    if (!plottable || plottable->parentPlot().get() != mParentPlot)
        return false;
    if (plottable->mGroup == this)
        return true;
    // Taken over before anyone is told: a membersChanged slot may remove the plottable or delete
    // a group, so every signal waits until the state is consistent, and is guarded.
    QPointer<QCPPlottableGroup> previous = plottable->mGroup;
    if (previous)
        previous->detach(plottable, false); // still out of the legend; its row-on-leave stays
    else
        plottable->mRowOnLeave = plottable->removeFromLegend();
    plottable->mGroup = this;
    mMembers.append(plottable);
    const auto shownChanged = [this]
    {
        markRowDirty();
        noteVisibility();
    };
    connect(plottable, &QCPLayerable::visibleChanged, this, shownChanged);
    connect(plottable, &QCPAbstractPlottable::visuallyBusyChanged, this,
            &QCPPlottableGroup::markRowDirty);
    if (auto* graph = qobject_cast<QCPGraph*>(plottable))
        connect(graph, &QCPGraph::componentVisibilityChanged, this, shownChanged);
    updateLegendRow();

    // Two groups are told, and a slot may delete either: deliver at the outermost call.
    const QCPEmissionScope scope(mParentPlot);
    if (previous)
    {
        previous->updateLegendRow();
        qcpEmit(previous.data(), mParentPlot, &QCPPlottableGroup::membersChanged);
        previous->noteVisibility();
    }
    qcpEmit(this, mParentPlot, &QCPPlottableGroup::membersChanged);
    noteVisibility();
    return true;
}

void QCPPlottableGroup::removeMember(QCPAbstractPlottable* plottable)
{
    if (!plottable || plottable->mGroup != this)
        return;
    detach(plottable, true);
    memberLeft();
}

/*! \internal

  What follows a member leaving, by removeMember or by the plot (take, removal, delete): the
  legend row, membersChanged, and visibleChanged when the last shown member left. Two signals: a
  slot of the first may delete this group.
*/
void QCPPlottableGroup::memberLeft()
{
    const QCPEmissionScope scope(mParentPlot);
    updateLegendRow();
    qcpEmit(this, mParentPlot, &QCPPlottableGroup::membersChanged);
    noteVisibility();
}

/*! \internal
  \a restoreRow is false when the plottable is about to be deleted: no point making it a row.
*/
void QCPPlottableGroup::detach(QCPAbstractPlottable* plottable, bool restoreRow)
{
    disconnect(plottable, nullptr, this, nullptr);
    mMembers.removeAll(plottable);
    plottable->mGroup = nullptr;
    if (restoreRow && plottable->mRowOnLeave)
        plottable->addToLegend();
}

bool QCPPlottableGroup::visible() const
{
    const auto live = membersRaw();
    return std::any_of(live.cbegin(), live.cend(), isShown);
}

void QCPPlottableGroup::setVisible(bool visible)
{
    // The members' signals, and this group's relayed from them, wait for the loop: a slot may
    // delete a member, this group or the plot.
    const QCPEmissionScope scope(mParentPlot);
    markRowDirty();
    for (QCPAbstractPlottable* member : membersRaw())
        setShown(member, visible);
    markRowDirty();
    noteVisibility();
}

void QCPPlottableGroup::noteVisibility()
{
    const bool shown = visible();
    if (shown == mShown)
        return;
    mShown = shown;
    qcpEmit(this, mParentPlot, &QCPPlottableGroup::visibleChanged, shown);
}

void QCPPlottableGroup::updateLegendRow()
{
    QCPLegend* legend = mParentPlot->legend;
    const bool wantsRow = legend && !membersRaw().isEmpty();
    if (wantsRow && !mLegendRow)
    {
        mLegendRow = new QCPPlottableGroupLegendItem(legend, this);
        legend->addItem(mLegendRow);
    }
    else if (!wantsRow && mLegendRow && legend)
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
    const auto live = mGroup->membersRaw();
    auto shown = std::find_if(live.cbegin(), live.cend(), isShown);
    if (shown != live.cend())
        return *shown;
    return live.isEmpty() ? nullptr : live.first();
}

QCPAbstractPlottable* QCPPlottableGroupLegendItem::busyMember() const
{
    if (!mGroup)
        return nullptr;
    const auto live = mGroup->membersRaw();
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
        painter->setOpacity(busyMember()->effectiveBusyFadeAlpha());
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
