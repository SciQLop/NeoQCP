#pragma once

#include "../layoutelements/layoutelement-legend.h"
#include <QPointer>

class QCPAbstractPlottable;
class QCPPlottableGroupLegendItem;

/*! \class QCPPlottableGroup
  \brief A set of plottables shown as one legend row.

  Create it with \ref QCustomPlot::addPlottableGroup and delete it with \ref
  QCustomPlot::removePlottableGroup. A member loses its own legend row while it is grouped and
  gets it back when it leaves (if it \ref QCPAbstractPlottable::wantsLegend). The group holds its
  members, it doesn't own them: \ref QCustomPlot::removePlottable takes a plottable out of its group.

  An empty group stays alive, without a legend row.

  A double-click on the row emits \ref doubleClicked; the group doesn't change visibility by itself,
  so its owner decides what a double-click means.
*/
class QCP_LIB_DECL QCPPlottableGroup : public QObject
{
    Q_OBJECT

public:
    ~QCPPlottableGroup() override;

    QCustomPlot* parentPlot() const { return mParentPlot; }
    QString name() const { return mName; }
    void setName(const QString& name);

    QList<QCPAbstractPlottable*> members() const;
    //! False for a null plottable or one of another QCustomPlot. A member of another group moves.
    bool addMember(QCPAbstractPlottable* plottable);
    void removeMember(QCPAbstractPlottable* plottable);

    //! True when any member is shown (for a QCPMultiGraph: visible with a visible component).
    bool visible() const;
    void setVisible(bool visible);

signals:
    void membersChanged();
    void doubleClicked();

private:
    QCPPlottableGroup(QCustomPlot* parentPlot, const QString& name);
    void detach(QCPAbstractPlottable* plottable, bool restoreRow);
    void updateLegendRow();
    void markRowDirty();

    QCustomPlot* mParentPlot;
    QString mName;
    QList<QPointer<QCPAbstractPlottable>> mMembers;
    QPointer<QCPPlottableGroupLegendItem> mLegendRow;

    friend class QCustomPlot;
};

/*! \class QCPPlottableGroupLegendItem
  \brief The legend row of a \ref QCPPlottableGroup: the first shown member's icon and the group
  name. Dimmed while the group is hidden.
*/
class QCP_LIB_DECL QCPPlottableGroupLegendItem : public QCPAbstractLegendItem
{
    Q_OBJECT

public:
    QCPPlottableGroupLegendItem(QCPLegend* parent, QCPPlottableGroup* group);

    QCPPlottableGroup* group() const { return mGroup; }
    //! The group name, after the busy symbol while a member is visually busy.
    QString rowText(bool withBusySymbol = true) const;
    //! Read at draw time, so a legend-wide text colour change can't undo the dimming.
    QColor effectiveTextColor() const;

protected:
    void draw(QCPPainter* painter) override;
    QSize minimumOuterSizeHint() const override;

private:
    QCPAbstractPlottable* iconMember() const;
    QCPAbstractPlottable* busyMember() const;

    QPointer<QCPPlottableGroup> mGroup;
};
