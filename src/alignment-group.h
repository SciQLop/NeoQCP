#ifndef QCP_ALIGNMENT_GROUP_H
#define QCP_ALIGNMENT_GROUP_H

#include <QHash>
#include <QList>
#include <QPointer>

#include "global.h"
#include "layout.h"

/*! \brief Aligns layout element edges across plots.

  The members own the group: it deletes itself once the last of them leaves (with deleteLater). A
  member's margin on a side with an automatic margin puts its inner edge on the group's shared edge,
  the innermost edge the members' own requirements ask for. The shared edge wins over a
  QCPMarginGroup on the same side.

  The requirement of a member is its outer rect edge in the window, moved inward by its own
  automatic margin (a rect's colour scale included). A member whose plot is hidden takes no part,
  and neither does a side whose automatic margin is off. Edges are compared in window coordinates
  relative to one member's origin, so moving a whole window does not realign anything, and the
  group never replots: when the shared edge moves it marks its other members' plots dirty.

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

    void add(QCPLayoutElement* element);
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
    explicit QCPAlignmentGroup(QCP::MarginSides sides);
    //! A member takes part while its plot exists and is visible: a hidden plot keeps its geometry.
    static bool eligible(const QCPLayoutElement* element);
    QList<QCPLayoutElement*> liveMembers() const;

    QList<QPointer<QCPLayoutElement>> mMembers;
    QCP::MarginSides mSides;
    QHash<QCP::MarginSide, int> mLastEdge;
    // The outer edge each member's margin was computed from, per side: a pass that sees it move
    // afterwards (a resize tightens the rect only in upLayout) relay out.
    QHash<const QCPLayoutElement*, QHash<QCP::MarginSide, int>> mUsedEdges;
};

#endif // QCP_ALIGNMENT_GROUP_H
