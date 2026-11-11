/***************************************************************************
**                                                                        **
**  NeoQCP                                                                **
**                                                                        **
**  This program is free software: you can redistribute it and/or modify  **
**  it under the terms of the GNU General Public License as published by  **
**  the Free Software Foundation, either version 3 of the License, or     **
**  (at your option) any later version.                                   **
**                                                                        **
****************************************************************************/

#ifndef QCP_COORDINATE_SYSTEM_H
#define QCP_COORDINATE_SYSTEM_H

#include "coords.h"
#include "global.h"

#include <QList>
#include <QObject>
#include <QPainterPath>
#include <QRect>

class QCPAbstractPlottable;
class QCPArea;
class QCPAxis;
class QCPAxisBase;
class QCPAxisRect;

struct QCPKeyWindow;

namespace qcp
{
/*! The revision a system is born with, or moved to by a change: one process-wide counter, so a
  system that reuses the address of a deleted one can never answer with its revision. */
[[nodiscard]] QCP_LIB_DECL quint64 nextSystemRevision();
} // namespace qcp

/*! \file coordinate-system.h

  A coordinate system is one frame of an area: the pair of axes that maps data to pixels there, and
  everything a drawing path asks about that mapping. The area owns its systems, one per axis pair,
  and hands the same one back for the same pair.
*/

/*! \brief The mapping of one axis pair: the frame plottables of that pair are drawn through.

  A system snapshots nothing: it answers with the current frame, and its \ref revision tells a
  recording whether the answer changed since it recorded. Every mapping change of either axis
  (range, scale, direction, link) and every geometry change of the area bumps the revision and emits
  \ref changed through the plot's delivery.

  The area creates systems (QCPArea::system) and owns them; a system is never created any other way.
*/
class QCP_LIB_DECL QCPCoordinateSystem : public QObject
{
    Q_OBJECT

public:
    ~QCPCoordinateSystem() override;

    //! The area whose geometry this system maps in.
    QCPArea* area() const { return mArea; }

    //! The axis the system's first data coordinate grows along.
    QCPAxisBase* keyAxis() const { return mKeyAxis; }

    //! The axis the system's second data coordinate grows along.
    QCPAxisBase* valueAxis() const { return mValueAxis; }

    //! Whether \a axis is one of the two this system maps through.
    bool usesAxis(const QCPAxisBase* axis) const;

    //! The frame to map data with and to map pixels back; taken once per paint, not per point.
    virtual QCPTransform transform() const = 0;
    //! The pixels this system draws in: the buffers, the damage and the GPU scissor use it.
    virtual QRect clipRect() const = 0;
    //! The painter's clip: the rect by default, a shape where the system has one.
    virtual QPainterPath clipPath() const;
    /*!
      Whether \ref clipPath() says more than \ref clipRect(). The painter clips through the path
      where it does; the GPU keeps the bounding rect as its scissor, so only the painter content
      follows the shape.
    */
    [[nodiscard]] virtual bool clipsToPath() const;
    //! Whether \a pixel can hit what this system draws; true while the frame is unusable, so a
    //! stale recording never swallows a click.
    virtual bool contains(QPointF pixel) const;
    //! The frame a legend icon replays a recording through.
    virtual QCPTransform iconTransform(const QCPDataBox& box, const QRectF& rect) const;
    //! The key range a recording stays good for, and the resolution it was recorded at.
    virtual QCPKeyWindow keyWindow() const;
    //! What can be drawn through this system; see QCP::Capability.
    virtual QCP::Capabilities capabilities() const = 0;

    //! Bumped by every mapping change; a recording compares it before it replays.
    quint64 revision() const { return mRevision; }

    /*! Whether anything is connected to \ref changed(). The revision always moves; the signal is
        only queued when it has a listener, because every axis setter of a drag or a pan passes
        through \ref bumpRevision. */
    [[nodiscard]] bool listens() const { return mChangedListeners > 0; }

    //! The plottables drawn through this system, in plot order.
    QList<QCPAbstractPlottable*> plottables() const;

signals:
    //! The mapping moved: a recording of it is stale.
    void changed();

protected:
    QCPCoordinateSystem(QCPArea* area, QCPAxisBase* keyAxis, QCPAxisBase* valueAxis);

    // The one place the revision moves: subclasses call it when their parameters changed.
    void bumpRevision();
    void connectNotify(const QMetaMethod& signal) override;
    void disconnectNotify(const QMetaMethod& signal) override;

    QCPArea* mArea;
    QCPAxisBase* mKeyAxis;
    QCPAxisBase* mValueAxis;
    quint64 mRevision = qcp::nextSystemRevision();
    int mChangedListeners = 0;

private:
    friend class QCPArea;
    Q_DISABLE_COPY(QCPCoordinateSystem)
};

/*! \brief The cartesian system: today's two-axis frame of an axis rect.

  Everything the rect's axes map is drawn through it, exactly as QCPAxis::coordToPixel does; the
  clip is the rect, and the hit area the rect's pixels as the plottable hit test has always read
  them.
*/
class QCP_LIB_DECL QCPCartesianSystem : public QCPCoordinateSystem
{
    Q_OBJECT

public:
    QCPTransform transform() const override;
    QRect clipRect() const override;
    bool contains(QPointF pixel) const override;
    QCPKeyWindow keyWindow() const override;
    QCP::Capabilities capabilities() const override;

private:
    friend class QCPAxisRect;
    QCPCartesianSystem(QCPAxisRect* rect, QCPAxis* key, QCPAxis* value);
};

#endif // QCP_COORDINATE_SYSTEM_H
