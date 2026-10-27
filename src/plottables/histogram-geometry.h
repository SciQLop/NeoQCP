#pragma once

#include <QPointF>
#include <QRectF>
#include <QVector>
#include <array>
#include <span>
#include <vector>

class QCPAxis;
namespace qcp::algo { struct Histogram1D; }

namespace qcp::histogram {

//! One column's bars as pixel rectangles. absent: the height has no position on the value axis
//! (non-finite, or <= 0 on a log axis); top == base and the step outline breaks there.
struct BarRect { double left, right, base, top; bool absent = false; };

[[nodiscard]] inline bool isEmpty(const BarRect& bar) { return bar.top == bar.base; }

//! One bar per bin of the column. Empty for a missing column or when edges.size() != bins + 1,
//! the same input QCPHistogram::setBinned rejects.
std::vector<BarRect> barRects(const qcp::algo::Histogram1D& h, int column,
                              const QCPAxis* keyAxis, const QCPAxis* valueAxis);

//! The bar in screen orientation, normalized.
QRectF pixelRect(const BarRect& bar, bool keyIsVertical);

//! Bars: one closed 5-point run per bar, NaN-separated (no diagonals between bars).
QVector<QPointF> barOutlines(std::span<const BarRect> bars, bool keyIsVertical);

//! Step: the top profile only, one run per gap-free stretch of bars.
QVector<QPointF> stepOutline(std::span<const BarRect> bars, bool keyIsVertical);

//! Triangles (6 floats per vertex) filling each bar.
void appendBarQuads(std::span<const BarRect> bars, bool keyIsVertical,
                    const std::array<float, 4>& rgba, std::vector<float>& out);

//! Whether the pixel position lies inside a non-empty bar.
bool contains(std::span<const BarRect> bars, const QPointF& pos, bool keyIsVertical);

} // namespace qcp::histogram
