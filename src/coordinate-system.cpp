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

#include "coordinate-system.h"

#include "axis/axis.h"
#include "core.h"
#include "datasource/graph-resampler.h"
#include "layoutelements/area.h"
#include "layoutelements/layoutelement-axisrect.h"
#include "misuse.h"
#include "plottables/plottable.h"

/*! \class QCPCoordinateSystem
  \brief The mapping of one axis pair of an area.

  See the class documentation in coordinate-system.h.
*/

QCPCoordinateSystem::QCPCoordinateSystem(QCPArea* area, QCPAxisBase* keyAxis,
                                         QCPAxisBase* valueAxis)
        : QObject(area), mArea(area), mKeyAxis(keyAxis), mValueAxis(valueAxis)
{
}

QCPCoordinateSystem::~QCPCoordinateSystem() = default;

bool QCPCoordinateSystem::usesAxis(const QCPAxisBase* axis) const
{
    return axis && (axis == mKeyAxis || axis == mValueAxis);
}

/*! \internal

  The default clip is the system's rect: a system whose drawing is shaped says so by overriding
  this.
*/
QPainterPath QCPCoordinateSystem::clipPath() const
{
    QPainterPath path;
    path.addRect(clipRect());
    return path;
}

/*! \internal

  The default hit area accepts every pixel: a system says what it draws, not what it does not.
*/
bool QCPCoordinateSystem::contains([[maybe_unused]] QPointF pixel) const
{
    return true;
}

/*! \internal

  A legend icon draws a recording fitted into its own rect, whatever frame it was recorded in.
*/
QCPTransform QCPCoordinateSystem::iconTransform(const QCPDataBox& box, const QRectF& rect) const
{
    return QCPTransform::fitted(box, rect);
}

/*! \internal

  The default keeps no window: every transform change remakes the recording.
*/
QCPKeyWindow QCPCoordinateSystem::keyWindow() const
{
    return { };
}

/*! \internal

  No plottable holds a system yet (they do once setSystem arrives): the cartesian projection of its
  axes is the frame a plottable draws through, so that is the answer here.
*/
QList<QCPAbstractPlottable*> QCPCoordinateSystem::plottables() const
{
    QList<QCPAbstractPlottable*> result;
    QCustomPlot* plot = mArea ? mArea->parentPlot() : nullptr;
    if (!plot)
        return result;
    // simplify: one scan of the plot's plottables per call, as keyAxis()/valueAxis() would need:
    // an index on the system when a plot holds enough of them.
    const int count = plot->plottableCount();
    for (int i = 0; i < count; ++i)
        if (QCPAbstractPlottable* plottable = plot->plottable(i))
            if (plottable->system() == this)
                result << plottable;
    return result;
}

/*! \internal

  One mapping change, one signal: the revision is what a recording compares, the signal what the
  area listens to in order to dirty its layers.
*/
void QCPCoordinateSystem::bumpRevision()
{
    ++mRevision;
    qcpEmit(this, mArea ? mArea->parentPlot() : nullptr, &QCPCoordinateSystem::changed);
}

/*! \class QCPCartesianSystem
  \brief The two-axis frame of an axis rect.

  The transform is the one QCPAxis draws with, and the hit area is the rect's, as the plottable hit
  test has always read it.
*/

QCPCartesianSystem::QCPCartesianSystem(QCPAxisRect* rect, QCPAxis* key, QCPAxis* value)
        : QCPCoordinateSystem(rect, key, value)
{
}

QCPTransform QCPCartesianSystem::transform() const
{
    return QCPTransform(static_cast<QCPAxis*>(keyAxis()), static_cast<QCPAxis*>(valueAxis()));
}

QRect QCPCartesianSystem::clipRect() const
{
    const QCPAxis* key = static_cast<QCPAxis*>(keyAxis());
    const QCPAxis* value = static_cast<QCPAxis*>(valueAxis());
    if (!key || !value || !key->axisRect() || !value->axisRect())
        return { };
    return key->axisRect()->rect() & value->axisRect()->rect();
}

/*! \internal

  The rule the plottable hit test has always used: the rect, or every pixel while the plot lets
  clicks outside the rect select plottables. An unusable frame accepts, so a stale recording never
  swallows a click.
*/
bool QCPCartesianSystem::contains(QPointF pixel) const
{
    const QCPAxis* key = static_cast<QCPAxis*>(keyAxis());
    if (!key || !key->axisRect() || !mArea || !mArea->parentPlot())
        return true;
    if (!transform().valid())
        return true;
    return key->axisRect()->rect().contains(pixel.toPoint())
        || mArea->parentPlot()->interactions().testFlag(QCP::iSelectPlottablesBeyondAxisRect);
}

QCPKeyWindow QCPCartesianSystem::keyWindow() const
{
    const QCPAxis* key = static_cast<QCPAxis*>(keyAxis());
    if (!key)
        return { };
    return { qcp::algo::lineCacheKeyRange(key->range(),
                                          key->scaleType() == QCPAxisBase::stLogarithmic),
             qcpKeyPixelsPerUnit(key) };
}

QCPRatioTransform QCPCartesianSystem::ratioTransform() const
{
    return QCPRatioTransform(static_cast<QCPAxisRect*>(area()));
}

QCP::Capabilities QCPCartesianSystem::capabilities() const
{
    return QCP::capRectangular | QCP::capStraightKey | QCP::capColourScale | QCP::capLanes;
}
