#pragma once
#include "abstract-datasource.h" // for IndexableNumericRange concept
#include "algorithms.h"          // for qcp::algo::findBegin/findEnd, keyRange
#include <cmath>
#include <limits>

namespace qcp::algo2d {

template <IndexableNumericRange XC>
int findXBegin(const XC& x, double sortKey)
{
    return qcp::algo::findBegin(x, sortKey, true);
}

template <IndexableNumericRange XC>
int findXEnd(const XC& x, double sortKey)
{
    return qcp::algo::findEnd(x, sortKey, true);
}

template <IndexableNumericRange XC>
QCPRange xRange(const XC& x, bool& found, QCP::SignDomain sd = QCP::sdBoth)
{
    // x columns are sorted (2D source contract) — O(1)/O(log n) range
    return qcp::algo::keyRangeSorted(x, found, sd);
}

template <IndexableNumericRange YC>
QCPRange yRange(const YC& y, bool& found, QCP::SignDomain sd = QCP::sdBoth)
{
    return qcp::algo::keyRange(y, found, sd);
}

template <IndexableNumericRange ZC>
QCPRange zRange(const ZC& z, int ySize, bool& found, int xBegin = 0, int xEnd = -1)
{
    found = false;
    if (std::ranges::empty(z) || ySize <= 0)
        return {};

    int totalRows = static_cast<int>(std::ranges::size(z)) / ySize;
    if (xEnd < 0)
        xEnd = totalRows;
    xBegin = std::max(0, xBegin);
    xEnd = std::min(xEnd, totalRows);

    double minVal = std::numeric_limits<double>::max();
    double maxVal = std::numeric_limits<double>::lowest();

    for (int i = xBegin; i < xEnd; ++i)
    {
        for (int j = 0; j < ySize; ++j)
        {
            double v = static_cast<double>(z[i * ySize + j]);
            if (std::isnan(v))
                continue;
            if (v < minVal) minVal = v;
            if (v > maxVal) maxVal = v;
            found = true;
        }
    }

    return found ? QCPRange(minVal, maxVal) : QCPRange();
}

} // namespace qcp::algo2d
