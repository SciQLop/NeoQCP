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

#ifndef QCP_COORDS_CUSTOM_H
#define QCP_COORDS_CUSTOM_H

#include "global.h"

#include <QPointF>

#include <memory>
#include <span>

namespace QCP
{
struct Data;
struct Pixels;
} // namespace QCP

/*! \brief The mapping of a coordinate system that is neither cartesian nor polar.

  A transform holds one of these when its system supplies its own rule (a sky projection, a
  distortion). The entry points are batch calls: a QCPTransform dispatches to map() once per span,
  never once per point, and inverse() maps back. A subclass that overrides only one map() overload
  declares \c using \c QCPCustomMapping::map; to keep the other one visible. equivalentTo() tells a replay whether two mappings
  differ by a pixel offset only; the default says they do not, which costs a repaint and can never
  draw a wrong frame.
*/
class QCP_LIB_DECL QCPCustomMapping
{
public:
    virtual ~QCPCustomMapping() = default;
    //! Maps keys (x) and values (y) into \a out; stops at the shortest of the three spans.
    virtual void map(std::span<const double> keys, std::span<const double> values,
                     std::span<QPointF> out) const = 0;
    //! Maps interleaved data (x = key, y = value) into \a out; stops at the shortest span.
    virtual void map(std::span<const QPointF> data, std::span<QPointF> out) const;
    //! Maps one data point to pixels.
    virtual QCP::Pixels mapPoint(QCP::Data data) const;
    //! Maps one pixel back to data; a pixel the mapping cannot represent yields NaN.
    virtual QCP::Data inverse(QPointF pixel) const = 0;
    //! True when the two mappings differ by a pixel offset only; \a delta gets it. Default: false.
    virtual bool equivalentTo(const QCPCustomMapping& other, QPointF* delta) const;
};

#endif // QCP_COORDS_CUSTOM_H
