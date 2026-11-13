#ifndef QCP_ALIGNMENT_GROUP_H
#define QCP_ALIGNMENT_GROUP_H

#include <QHash>
#include <QList>
#include <QPointer>

#include "global.h"
#include "layout.h"

/*! \brief Aligns layout element edges, inside one plot and across plots.

  The members own the group: it deletes itself once the last of them leaves (with deleteLater). A
  member's margin on a side with an automatic margin puts its inner edge on the group's shared edge,
  the innermost edge the members' own requirements ask for.

  \image html QCPAlignmentGroup.png "The left column's left sides in one group"

  An element joins one group per side, so it can align its left and right edges with one column
  and its top and bottom edges with one row. Joining a group for some sides leaves, for those sides
  only, the groups the element had there; it keeps its other sides' groups.

  The requirement of a member is its outer rect edge in the window, moved inward by its own
  automatic margin (a rect's colour scale included). A side whose automatic margin is off takes no
  part. A member in another plot takes part while both plots are shown; a member in the plot being
  laid out always does, so a plot that was never shown (an export) aligns its own elements. Edges
  are compared in window coordinates relative to one member's origin, so moving a whole window does
  not realign anything, and the group never replots: when the shared edge moves it marks its other
  members' plots dirty.

  \see QCPLayoutElement::alignmentGroup
*/
class QCP_LIB_DECL QCPAlignmentGroup : public QObject
{
    Q_OBJECT

public:
    ~QCPAlignmentGroup() override;

    //! Makes a group of \a members, aligning the \a sides; returns null (one misuse) on a bad list.
    static QPointer<QCPAlignmentGroup> create(const QList<QCPLayoutElement*>& members,
                                              QCP::MarginSides sides = QCP::msLeft | QCP::msRight);

    //! Adds \a element for the group's sides; it leaves its previous groups on those sides.
    void add(QCPLayoutElement* element);
    //! Removes \a element from the group on every side.
    void remove(QCPLayoutElement* element);
    QList<QCPHandle<QCPLayoutElement>> elements() const;
    QCP::MarginSides sides() const { return mSides; }

    //! The margin \a element must take on \a side to reach the group's shared edge.
    int marginFor(QCPLayoutElement* element, QCP::MarginSide side);
    //! Forgets the shared edges and dirties the members, so the next layout pass recomputes them.
    void invalidate();
    //! Relays out every member: for a plot whose visibility changed.
    void relayoutMembers();
    //! True when a member's outer edge moved since marginFor last used it (checked after layout).
    bool edgeMoved(const QCPLayoutElement* element) const;

signals:
    void membersChanged();

private:
    struct Member
    {
        QPointer<QCPLayoutElement> element;
        QCP::MarginSides sides; // the group's sides this member still holds
    };

    explicit QCPAlignmentGroup(QCP::MarginSides sides);
    //! A member is relaid out on a change while its plot exists and is shown.
    static bool eligible(const QCPLayoutElement* element);
    //! \a member's edge counts for \a asker: laid out, and in its plot or in a shown one.
    static bool comparable(const QCPLayoutElement* asker, const QCPLayoutElement* member);
    QList<QCPLayoutElement*> liveMembers() const;
    QList<Member>::iterator findMember(const QCPLayoutElement* element);
    //! The sides \a element holds in this group with an automatic margin: the ones it aligns.
    QCP::MarginSides alignedSides(const QCPLayoutElement* element) const;
    //! Drops \a element from the group on \a sides; it goes from the list once it holds none.
    void leave(QCPLayoutElement* element, QCP::MarginSides sides);

    QList<Member> mMembers;
    QCP::MarginSides mSides;
    QHash<QCP::MarginSide, int> mLastEdge;
    // The outer edge each member's margin was computed from, per side: a pass that sees it move
    // afterwards (a resize tightens the rect only in upLayout) relay out.
    QHash<const QCPLayoutElement*, QHash<QCP::MarginSide, int>> mUsedEdges;
};

#endif // QCP_ALIGNMENT_GROUP_H
