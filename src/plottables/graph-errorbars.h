#pragma once

#include <span>

namespace qcp::errorbars {

//! One bar in data space. value/lower/upper may be NaN. Crossed and log-invalid bounds are
//! clamped by the caller; resolve() only fills a missing bound.
struct Bar { double key; double value; double lower; double upper; };

//! A NaN bound falls back to the value (a one-sided bar). Returns false when fewer than two of
//! the three points are finite: no bar.
bool resolve(Bar& bar);

//! True when no two consecutive bars with a finite key are closer than minSpacingPx.
bool barsFit(std::span<const Bar> bars, double minSpacingPx);

} // namespace qcp::errorbars
