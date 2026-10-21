#pragma once

#include <QPointF>
#include <limits>

namespace qcp::geometry {

//! The pixel point at a key and a value position, whichever way the key axis runs.
inline QPointF point(double key, double value, bool keyIsVertical)
{
    return keyIsVertical ? QPointF(value, key) : QPointF(key, value);
}

//! The point that ends a run in a NaN-separated polyline (drawPolylineSplitNaN & co).
inline QPointF breakPoint()
{
    constexpr double nan = std::numeric_limits<double>::quiet_NaN();
    return { nan, nan };
}

} // namespace qcp::geometry
