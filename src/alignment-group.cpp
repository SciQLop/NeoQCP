#include "alignment-group.h"

#include <algorithm>
#include <climits>

#include "core.h"
#include "misuse.h"

namespace
{
//! The outer rect edge of \a element on \a side, in its window's pixels.
int windowEdge(const QCPLayoutElement* element, QCP::MarginSide side)
{
    const QCustomPlot* plot = element->parentPlot();
    const QPoint origin = plot->mapTo(plot->window(), QPoint(0, 0));
    const QRect outer = element->outerRect();
    switch (side)
    {
    case QCP::msLeft:
        return origin.x() + outer.left();
    case QCP::msRight:
        return origin.x() + outer.right();
    case QCP::msTop:
        return origin.y() + outer.top();
    default:
        return origin.y() + outer.bottom();
    }
}

bool inward(QCP::MarginSide side)
{
    return side == QCP::msLeft || side == QCP::msTop;
}
} // namespace

QCPAlignmentGroup::QCPAlignmentGroup(QCP::MarginSides sides) : mSides(sides) { }

QCPAlignmentGroup::~QCPAlignmentGroup() = default;

bool QCPAlignmentGroup::eligible(const QCPLayoutElement* element)
{
    if (!element || !element->layout())
        return false;
    const QCustomPlot* plot = element->parentPlot();
    // Hiding a widget keeps its geometry, so the plot itself must still be shown; a member whose
    // outer rect is empty has not been laid out yet and would drag the shared edge elsewhere.
    return plot && plot->isVisible() && !element->outerRect().isEmpty();
}

QList<QCPLayoutElement*> QCPAlignmentGroup::liveMembers() const
{
    QList<QCPLayoutElement*> live;
    for (const auto& member : mMembers)
        if (member)
            live.append(member);
    return live;
}

QPointer<QCPAlignmentGroup> QCPAlignmentGroup::create(const QList<QCPLayoutElement*>& members,
                                                      QCP::MarginSides sides)
{
    QCP_CHECK_OR_RETURN(!members.isEmpty(), "an alignment group needs at least one member", {});
    for (const QCPLayoutElement* member : members)
        QCP_CHECK_OR_RETURN(member && member->parentPlot(),
                            "an alignment group's members need a plot", {});
    QPointer<QCPAlignmentGroup> group(new QCPAlignmentGroup(sides));
    for (QCPLayoutElement* member : members)
        group->add(member);
    return group;
}

void QCPAlignmentGroup::add(QCPLayoutElement* element)
{
    QCP_CHECK_OR_RETURN(element, "an alignment group cannot take a null element", );
    QCP_CHECK_OR_RETURN(element->parentPlot(), "an alignment group's members need a plot", );
    if (mMembers.contains(element))
        return;
    if (QCPAlignmentGroup* previous = element->mAlignmentGroup)
        previous->remove(element);
    mMembers.append(element);
    element->mAlignmentGroup = this;
    invalidate();
    qcpEmit(this, nullptr, &QCPAlignmentGroup::membersChanged);
}

void QCPAlignmentGroup::remove(QCPLayoutElement* element)
{
    if (!element || !mMembers.removeOne(element))
        return;
    QCustomPlot* plot = element->parentPlot();
    element->mAlignmentGroup = nullptr;
    mUsedEdges.remove(element);
    invalidate();
    qcpEmit(this, nullptr, &QCPAlignmentGroup::membersChanged);
    if (mMembers.isEmpty())
    {
        // A plot being torn down may never run the event loop again: waiting for a deferred
        // delete would leak the group for the life of the process.
        if (plot && plot->isBeingDestroyed())
            delete this;
        else
            deleteLater();
    }
}

QList<QCPHandle<QCPLayoutElement>> QCPAlignmentGroup::elements() const
{
    QList<QCPHandle<QCPLayoutElement>> result;
    const QList<QCPLayoutElement*> members = liveMembers();
    result.reserve(members.size());
    for (QCPLayoutElement* member : members)
        result.append(QCPHandle<QCPLayoutElement>(member));
    return result;
}

// simplify: every member asks for the shared edge and this recomputes each member's requirement,
// so a pass costs O(n^2) calculateAutoMargin calls. Compute the shared edge once per pass when a
// profile shows it.
int QCPAlignmentGroup::marginFor(QCPLayoutElement* element, QCP::MarginSide side)
{
    if (!element)
        return 0;
    if (!mSides.testFlag(side) || !eligible(element)
        || !element->autoMargins().testFlag(side))
        return element->calculateAutoMargin(side);

    QList<QCPLayoutElement*> peers;
    for (QCPLayoutElement* member : liveMembers())
        if (eligible(member) && member->autoMargins().testFlag(side))
            peers.append(member);
    if (peers.isEmpty())
        return element->calculateAutoMargin(side);

    const bool growsInward = inward(side);
    int shared = growsInward ? INT_MIN : INT_MAX;
    int own = 0;
    for (QCPLayoutElement* member : peers)
    {
        const int edge = windowEdge(member, side);
        const int need = qMax(member->calculateAutoMargin(side),
                              QCP::getMarginValue(member->minimumMargins(), side));
        const int requirement = growsInward ? edge + need : edge - need;
        shared = growsInward ? qMax(shared, requirement) : qMin(shared, requirement);
        if (member == element)
            own = edge;
    }
    mUsedEdges[element][side] = windowEdge(element, side);
    if (mLastEdge.value(side, INT_MIN) != shared)
    {
        mLastEdge[side] = shared;
        for (QCPLayoutElement* member : peers)
            if (member != element)
                member->markLayoutAppearanceDirty();
    }
    return growsInward ? shared - own : own - shared;
}

bool QCPAlignmentGroup::edgeMoved(const QCPLayoutElement* element) const
{
    const auto used = mUsedEdges.constFind(element);
    if (used == mUsedEdges.constEnd())
        return false;
    for (auto it = used->constBegin(); it != used->constEnd(); ++it)
        if (windowEdge(element, it.key()) != it.value())
            return true;
    return false;
}

void QCPAlignmentGroup::invalidate()
{
    mLastEdge.clear();
    mUsedEdges.clear();
    for (QCPLayoutElement* member : liveMembers())
        if (eligible(member))
            member->markLayoutAppearanceDirty();
}

/*! \internal

  Marks every member's layout dirty without forgetting the shared edges: a member that was hidden
  or shown changes the requirements, and the next pass tells whether the shared edge moved.
*/
void QCPAlignmentGroup::relayoutMembers()
{
    for (QCPLayoutElement* member : liveMembers())
        member->markLayoutAppearanceDirty();
}
