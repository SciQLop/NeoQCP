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

#ifndef QCP_COORDINATE_SYSTEM_POLAR_H
#define QCP_COORDINATE_SYSTEM_POLAR_H

#include "coordinate-system.h"

class QCPPolarArea;
class QCPPolarAxisAngular;
class QCPPolarAxisRadial;

/*! \brief The polar system: one angular axis and one radial axis of a polar area.

  It lives in its own header so the cartesian system's header stays free of the polar axes; the
  area creates it, and it maps exactly as the polar axes draw, with the radial fraction clamped to
  the outermost ring.
*/
class QCP_LIB_DECL QCPPolarSystem : public QCPCoordinateSystem
{
    Q_OBJECT

public:
    QCPTransform transform() const override;
    QRect clipRect() const override;
    QPainterPath clipPath() const override;
    //! The disc is the painter's clip; the GPU scissor stays its bounding square.
    bool clipsToPath() const override { return true; }
    bool contains(QPointF pixel) const override;
    QCPKeyWindow keyWindow() const override;
    //! There is no axis ratio in a polar area: items and spans stay cartesian in this milestone.
    QCPRatioTransform ratioTransform() const override;
    //! A polar area offers none of the cartesian capabilities: it has no rect, no straight key.
    QCP::Capabilities capabilities() const override;

private:
    friend class QCPPolarArea;
    QCPPolarSystem(QCPPolarArea* area, QCPPolarAxisAngular* angular, QCPPolarAxisRadial* radial);

    QCPPolarArea* polarArea() const;
    QCPPolarAxisAngular* angularAxis() const;
    QCPPolarAxisRadial* radialAxis() const;
};

#endif // QCP_COORDINATE_SYSTEM_POLAR_H
