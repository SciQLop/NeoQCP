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

#include "coordinate-system-polar.h"
#include "painting/draw-context.h"

#include "datasource/graph-resampler.h"
#include "misuse.h"
#include "polar/angularaxis.h"
#include "polar/polar-area.h"
#include "polar/radialaxis.h"

#include <QLineF>

/*! \class QCPPolarSystem
  \brief The frame of one angular axis and one radial axis.

  The transform is QCPTransform::polar, the clip is the disc and the hit area the disc's pixels; the
  resolution window is the arc length a key covers at the outer radius, since the angular axis maps
  its whole range onto one turn.
*/

QCPPolarSystem::QCPPolarSystem(QCPPolarArea* area, QCPPolarAxisAngular* angular,
                               QCPPolarAxisRadial* radial)
        : QCPCoordinateSystem(area, angular, radial)
{
}

QCPPolarArea* QCPPolarSystem::polarArea() const
{
    return static_cast<QCPPolarArea*>(mArea);
}

QCPPolarAxisAngular* QCPPolarSystem::angularAxis() const
{
    return static_cast<QCPPolarAxisAngular*>(mKeyAxis);
}

QCPPolarAxisRadial* QCPPolarSystem::radialAxis() const
{
    return static_cast<QCPPolarAxisRadial*>(mValueAxis);
}

QCPTransform QCPPolarSystem::transform() const
{
    const QCPPolarAxisAngular* angular = angularAxis();
    const QCPPolarAxisRadial* radial = radialAxis();
    if (!angular || !radial)
        return QCPTransform();
    return QCPTransform::polar(*angular, *radial);
}

QRect QCPPolarSystem::clipRect() const
{
    const QCPPolarArea* area = polarArea();
    const QPointF centre = area->center();
    const double radius = area->radius();
    return QRectF(centre.x() - radius, centre.y() - radius, 2.0 * radius, 2.0 * radius)
        .toAlignedRect();
}

QPainterPath QCPPolarSystem::clipPath() const
{
    const QCPPolarArea* area = polarArea();
    QPainterPath disc;
    disc.addEllipse(area->center(), area->radius(), area->radius());
    return disc;
}

bool QCPPolarSystem::contains(QPointF pixel) const
{
    const QCPPolarArea* area = polarArea();
    if (!area || !(area->radius() > 0.0) || !transform().valid())
        return true;
    return QLineF(area->center(), pixel).length() <= area->radius();
}

QCPKeyWindow QCPPolarSystem::keyWindow() const
{
    const QCPPolarAxisAngular* angular = angularAxis();
    const QCPPolarArea* area = polarArea();
    if (!angular || !area)
        return { };
    const double span = angular->range().size();
    if (!(span > 0.0))
        return { };
    // The angular range covers one turn, so one unit of angle is 2*pi/span radians: a key covers
    // that much of the circle at the outer radius.
    return { qcp::algo::lineCacheKeyRange(angular->range(), false),
             area->radius() * 2.0 * M_PI / span };
}

QCP::Capabilities QCPPolarSystem::capabilities() const
{
    return { };
}
