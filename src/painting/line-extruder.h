#pragma once

#include <QColor>
#include <QPointF>
#include <QPolygonF>
#include <QVector>
#include <span>
#include <vector>

namespace QCPLineExtruder
{

// Extrude a polyline into a triangle-list vertex buffer.
// Each vertex: (x, y, r, g, b, a) — 6 floats.
// NaN points in the input create gaps.
// Miter joins with bevel fallback at sharp angles.
QVector<float> extrudePolyline(const QVector<QPointF>& points, float penWidth,
                                const QColor& color);

// Same, but writes into caller-owned buffer (retains capacity across frames → zero alloc on reuse).
// Clears out first — see appendPolyline() to build several extrusions into one buffer.
void extrudePolyline(const QVector<QPointF>& points, float penWidth,
                     const QColor& color, std::vector<float>& out);

// Same as the out-param extrudePolyline(), but appends to out instead of clearing it first —
// for building several colour runs into one shared vertex buffer without a per-run temporary.
void appendPolyline(std::span<const QPointF> points, float penWidth,
                     const QColor& color, std::vector<float>& out);

// Tessellate a baseline fill polygon into a triangle-list vertex buffer.
// The polygon structure is: basePoint0, curvePoint0..N, basePoint1.
// Uses trapezoid decomposition between consecutive curve points.
QVector<float> tessellateFillPolygon(const QPolygonF& polygon, const QColor& color);

} // namespace QCPLineExtruder
