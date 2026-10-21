#pragma once

#include <QPointF>
#include <QVector>
#include <span>

namespace qcp::errorbars {

//! One bar in pixels. value/lower/upper may be NaN. Crossed and log-invalid bounds are clamped
//! once, in data space, by the caller before mapping to pixels; resolve() only fills a missing
//! bound.
struct Bar { double key; double value; double lower; double upper; };

//! A NaN bound falls back to the value (a one-sided bar). Returns false when fewer than two of
//! the three points are finite: no bar.
bool resolve(Bar& bar);

//! Stem and two caps per bar as separate NaN-separated runs (3 runs, 2 points each), for the
//! polyline helpers. Never emits infinities.
QVector<QPointF> barRuns(std::span<const Bar> bars, double capHalfWidthPx, bool keyIsVertical);

//! True when no two consecutive bars with a finite key are closer than minSpacingPx.
bool barsFit(std::span<const Bar> bars, double minSpacingPx);

} // namespace qcp::errorbars
