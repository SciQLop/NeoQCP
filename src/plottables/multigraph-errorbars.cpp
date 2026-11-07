#include "multigraph-errorbars.h"
#include <cmath>
#include <limits>

namespace qcp::errorbars {

namespace {

constexpr double kNan = std::numeric_limits<double>::quiet_NaN();

double orValue(double bound, double value) { return std::isfinite(bound) ? bound : value; }

} // namespace

bool resolve(Bar& bar)
{
    const int finiteCount = std::isfinite(bar.value) + std::isfinite(bar.lower)
                            + std::isfinite(bar.upper);
    if (finiteCount < 2)
        return false;
    bar.lower = orValue(bar.lower, bar.value);
    bar.upper = orValue(bar.upper, bar.value);
    return true;
}

bool barsFit(std::span<const Bar> bars, double minSpacingPx)
{
    double previous = kNan;
    for (const Bar& bar : bars)
    {
        if (!std::isfinite(bar.key))
            continue;
        if (std::abs(bar.key - previous) < minSpacingPx)
            return false;
        previous = bar.key;
    }
    return true;
}

} // namespace qcp::errorbars
