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

constexpr QCP::MarginSide everySide[] = { QCP::msLeft, QCP::msRight, QCP::msTop, QCP::msBottom };

bool laidOut(const QCPLayoutElement* element)
{
    return element && element->layout() && element->parentPlot()
        && !element->outerRect().isEmpty();
}

// Recorded for a member that had no geometry yet: any edge it gets later counts as a move.
constexpr int notLaidOut = INT_MIN;
} // namespace

QCPAlignmentGroup::QCPAlignmentGroup(QCP::MarginSides sides) : mSides(sides) { }

// Deleted by hand while members remain: they lay their freed sides out on their own.
QCPAlignmentGroup::~QCPAlignmentGroup()
{
    invalidate();
}

bool QCPAlignmentGroup::eligible(const QCPLayoutElement* element)
{
    if (!element || !element->layout())
        return false;
    const QCustomPlot* plot = element->parentPlot();
    // Hiding a widget keeps its geometry, so the plot itself must still be shown; a member whose
    // outer rect is empty has not been laid out yet and would drag the shared edge elsewhere.
    return plot && plot->isVisible() && !element->outerRect().isEmpty();
}

bool QCPAlignmentGroup::comparable(const QCPLayoutElement* asker, const QCPLayoutElement* member)
{
    if (!laidOut(member))
        return false;
    // The plot being laid out has its geometry even before it is shown; another plot's edge means
    // something only while both are on screen, in window coordinates.
    const QCustomPlot* own = asker->parentPlot();
    const QCustomPlot* plot = member->parentPlot();
    return plot == own || (plot->isVisible() && own->isVisible());
}

QList<QCPLayoutElement*> QCPAlignmentGroup::liveMembers() const
{
    QList<QCPLayoutElement*> live;
    for (const Member& member : mMembers)
        if (member.element)
            live.append(member.element);
    return live;
}

QList<QCPAlignmentGroup::Member>::iterator
QCPAlignmentGroup::findMember(const QCPLayoutElement* element)
{
    return std::find_if(mMembers.begin(), mMembers.end(),
                        [element](const Member& member) { return member.element == element; });
}

QPointer<QCPAlignmentGroup> QCPAlignmentGroup::create(const QList<QCPLayoutElement*>& members,
                                                      QCP::MarginSides sides)
{
    QCP_CHECK_OR_RETURN(!members.isEmpty(), "an alignment group needs at least one member", {});
    QCP_CHECK_OR_RETURN(sides & (QCP::msLeft | QCP::msRight | QCP::msTop | QCP::msBottom),
                        "an alignment group aligns at least one side", {});
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
    for (QCP::MarginSide side : everySide)
    {
        if (!mSides.testFlag(side))
            continue;
        QCPAlignmentGroup* previous
            = element->mAlignmentGroups[QCPLayoutElement::alignmentSlot(side)];
        if (previous && previous != this)
            previous->leave(element, side);
    }
    auto member = findMember(element);
    if (member == mMembers.end())
        member = mMembers.insert(mMembers.end(), Member { element, QCP::msNone });
    else if ((member->sides & mSides) == mSides)
        return;
    member->sides = mSides;
    for (QCP::MarginSide side : everySide)
        if (mSides.testFlag(side))
            element->mAlignmentGroups[QCPLayoutElement::alignmentSlot(side)] = this;
    invalidate();
    qcpEmit(this, nullptr, &QCPAlignmentGroup::membersChanged);
}

void QCPAlignmentGroup::remove(QCPLayoutElement* element)
{
    leave(element, QCP::msAll);
}

void QCPAlignmentGroup::leave(QCPLayoutElement* element, QCP::MarginSides sides)
{
    const auto member = findMember(element);
    if (!element || member == mMembers.end() || !(member->sides & sides))
        return;
    const QCP::MarginSides leaving = member->sides & sides;
    member->sides &= ~sides;
    for (QCP::MarginSide side : everySide)
        if (leaving.testFlag(side))
            element->mAlignmentGroups[QCPLayoutElement::alignmentSlot(side)] = nullptr;
    QCustomPlot* plot = element->parentPlot();
    if (member->sides == QCP::msNone)
    {
        mMembers.erase(member);
        mUsedEdges.remove(element);
    }
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
    const auto self = findMember(element);
    if (self == mMembers.end() || !self->sides.testFlag(side)
        || !element->autoMargins().testFlag(side))
        return element->calculateAutoMargin(side);
    if (!laidOut(element))
    {
        // The first pass has no edge to compare; the one that gives it an edge runs again.
        mUsedEdges[element][side] = notLaidOut;
        return element->calculateAutoMargin(side);
    }

    QList<QCPLayoutElement*> peers;
    for (const Member& member : mMembers)
        if (member.sides.testFlag(side) && comparable(element, member.element)
            && member.element->autoMargins().testFlag(side))
            peers.append(member.element);

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
    mUsedEdges[element][side] = own;
    // A hidden plot's member saw its own plot's members only: its shared edge is not the group's,
    // and those members are laid out in this very pass anyway.
    if (element->parentPlot()->isVisible() && mLastEdge.value(side, INT_MIN) != shared)
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
    if (used == mUsedEdges.constEnd() || !laidOut(element))
        return false;
    // The record of a side outlives its part in the group (a margin switched to manual keeps it
    // until the next invalidate): only a side the element still aligns can be unsettled.
    const QCP::MarginSides aligned = alignedSides(element);
    for (auto it = used->constBegin(); it != used->constEnd(); ++it)
        if (aligned.testFlag(it.key()) && windowEdge(element, it.key()) != it.value())
            return true;
    return false;
}

QCP::MarginSides QCPAlignmentGroup::alignedSides(const QCPLayoutElement* element) const
{
    const auto member
        = std::find_if(mMembers.cbegin(), mMembers.cend(),
                       [element](const Member& candidate) { return candidate.element == element; });
    return member == mMembers.cend() ? QCP::msNone : member->sides & element->autoMargins();
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
