#include "resample.h"
#include "abstract-datasource-2d.h"
#include "Profiling.hpp"
#include <axis/range.h>
#include <plottables/plottable-colormap.h> // for QCPColorMapData
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace qcp::algo2d {

namespace {

std::vector<double> generateRange(double start, double end, int n, bool log)
{
    std::vector<double> range(n);
    if (n <= 1)
    {
        if (n == 1)
            range[0] = start;
        return range;
    }
    if (log && start > 0 && end > 0)
    {
        double logStart = std::log10(start);
        double step = (std::log10(end) - logStart) / (n - 1);
        for (int i = 0; i < n; ++i)
            range[i] = std::pow(10.0, logStart + i * step);
    }
    else
    {
        double step = (end - start) / (n - 1);
        for (int i = 0; i < n; ++i)
            range[i] = start + i * step;
    }
    return range;
}

int findBin(double value, const std::vector<double>& axis)
{
    auto it = std::lower_bound(axis.begin(), axis.end(), value);
    if (it == axis.end())
        return static_cast<int>(axis.size()) - 1;
    int idx = static_cast<int>(std::distance(axis.begin(), it));
    if (idx > 0 && (value - axis[idx - 1]) < (axis[idx] - value))
        --idx;
    return std::clamp(idx, 0, static_cast<int>(axis.size()) - 1);
}

// Like findBin but ties go to the lower bin (for exclusive upper bounds)
int findBinFloor(double value, const std::vector<double>& axis)
{
    auto it = std::lower_bound(axis.begin(), axis.end(), value);
    if (it == axis.end())
        return static_cast<int>(axis.size()) - 1;
    int idx = static_cast<int>(std::distance(axis.begin(), it));
    if (idx > 0 && (value - axis[idx - 1]) <= (axis[idx] - value))
        --idx;
    return std::clamp(idx, 0, static_cast<int>(axis.size()) - 1);
}

} // anonymous namespace

QCPColorMapData* resample(
    const QCPAbstractDataSource2D& src,
    int xBegin, int xEnd,
    const QCPRange& xRange, const QCPRange& yRange,
    int targetWidth, int targetHeight,
    bool yLogScale,
    double gapThreshold)
{
    PROFILE_HERE_N("resample");
    int srcCount = xEnd - xBegin;
    if (srcCount < 2 || targetWidth < 1 || targetHeight < 1)
        return nullptr;
    if (xRange.lower >= xRange.upper || yRange.lower >= yRange.upper)
        return nullptr;

    int nx = targetWidth;
    int ny = targetHeight;

    auto xAxis = generateRange(xRange.lower, xRange.upper, nx, false);
    auto yAxis = generateRange(yRange.lower, yRange.upper, ny, yLogScale);
    int ys = src.ySize();

    auto* data = new QCPColorMapData(nx, ny, {xAxis.front(), xAxis.back()},
                                              {yAxis.front(), yAxis.back()});
    for (int i = 0; i < nx; ++i)
        for (int j = 0; j < ny; ++j)
            data->setCell(i, j, 0.0);

    std::vector<uint32_t> counts(nx * ny, 0);

    // Extend the source range by 1 on each side as context for gap detection
    // and fill spacing. Without this, zooming in at a gap boundary leaves only
    // 2 visible columns with no neighbors for the local-reference comparison,
    // causing the gap to go undetected and data to fill the entire viewport.
    int ctxBegin = std::max(0, xBegin - 1);
    int ctxEnd = std::min(src.xSize(), xEnd + 1);
    int ctxCount = ctxEnd - ctxBegin;
    int dataOffset = xBegin - ctxBegin;

    // Mark gaps BETWEEN consecutive context columns using local reference dx.
    // gapBetween[i] = true means there's a gap between context index i and i+1.
    std::vector<bool> gapBetween(ctxCount, false);
    if (gapThreshold > 0 && ctxCount > 2)
    {
        for (int i = 0; i < ctxCount - 1; ++i)
        {
            double dx = src.xAt(ctxBegin + i + 1) - src.xAt(ctxBegin + i);
            double refDx = std::numeric_limits<double>::max();
            if (i > 0)
                refDx = std::min(refDx, src.xAt(ctxBegin + i) - src.xAt(ctxBegin + i - 1));
            if (i + 2 < ctxCount)
                refDx = std::min(refDx, src.xAt(ctxBegin + i + 2) - src.xAt(ctxBegin + i + 1));
            if (refDx < std::numeric_limits<double>::max() && dx > gapThreshold * refDx)
                gapBetween[i] = true;
        }
    }

    // Y bin ranges: each source Y-row fills bins within half its local spacing
    // (bounded fill, no bleed beyond data extent). For log-scaled Y axes,
    // boundaries are geometric means of neighbors so channels tile correctly.
    // When Y varies per column (yIs2D), ranges are recomputed per column.
    struct BinRange { int lo, hi; };
    bool variableY = src.yIs2D();

    auto computeYBinRanges = [&](int col, std::vector<BinRange>& ranges) {
        for (int yj = 0; yj < ys; ++yj)
        {
            double yVal = src.yAt(col, yj);
            double yLo, yHi;
            if (ys == 1)
            {
                yLo = yHi = yVal;
            }
            else if (yLogScale && yVal > 0)
            {
                double prev = (yj > 0) ? src.yAt(col, yj - 1) : 0;
                double next = (yj < ys - 1) ? src.yAt(col, yj + 1) : 0;
                yLo = (yj > 0 && prev > 0) ? std::sqrt(prev * yVal) : yVal;
                yHi = (yj < ys - 1 && next > 0) ? std::sqrt(yVal * next) : yVal;
            }
            else
            {
                double halfBelow = (yj > 0) ? (yVal - src.yAt(col, yj - 1)) * 0.5 : 0;
                double halfAbove = (yj < ys - 1) ? (src.yAt(col, yj + 1) - yVal) * 0.5 : 0;
                if (yj == 0) halfBelow = halfAbove;
                if (yj == ys - 1) halfAbove = halfBelow;
                double halfSpacing = std::min(halfBelow, halfAbove);
                yLo = yVal - halfSpacing;
                yHi = yVal + halfSpacing;
            }
            ranges[yj].lo = findBin(yLo, yAxis);
            ranges[yj].hi = findBinFloor(yHi, yAxis);
        }
    };

    std::vector<BinRange> yBinRanges(ys);
    if (!variableY)
        computeYBinRanges(xBegin, yBinRanges);

    for (int si = 0; si < srcCount; ++si)
    {
        int ci = si + dataOffset; // index into context/gapBetween arrays
        int xi = xBegin + si;
        double xVal = src.xAt(xi);

        // X fill range: half the non-gap neighbor distance.
        // Gap boundaries clamp the fill so data doesn't bleed across gaps.
        // Neighbor lookups use the context range, so edge columns get correct
        // spacing even when the actual neighbor is outside [xBegin, xEnd).
        bool gapLeft = (ci > 0) && gapBetween[ci - 1];
        bool gapRight = (ci < ctxCount - 1) && gapBetween[ci];

        double leftDist = (!gapLeft && ci > 0)
            ? xVal - src.xAt(ctxBegin + ci - 1) : std::numeric_limits<double>::max();
        double rightDist = (!gapRight && ci < ctxCount - 1)
            ? src.xAt(ctxBegin + ci + 1) - xVal : std::numeric_limits<double>::max();

        // For edge/gap-boundary points, mirror the available spacing
        if (leftDist == std::numeric_limits<double>::max() && rightDist < std::numeric_limits<double>::max())
            leftDist = rightDist;
        if (rightDist == std::numeric_limits<double>::max() && leftDist < std::numeric_limits<double>::max())
            rightDist = leftDist;

        double xHalfSpacing = (leftDist < std::numeric_limits<double>::max())
            ? std::min(leftDist, rightDist) * 0.5 : 0;

        int xBinLo = findBin(xVal - xHalfSpacing, xAxis);
        int xBinHi = findBinFloor(xVal + xHalfSpacing, xAxis);

        if (variableY)
            computeYBinRanges(xi, yBinRanges);

        for (int yj = 0; yj < ys; ++yj)
        {
            double yVal = src.yAt(xi, yj);
            if (std::isnan(yVal))
                continue;
            double zVal = src.zAt(xi, yj);
            if (std::isnan(zVal))
                continue;

            int yLo = yBinRanges[yj].lo;
            int yHi = yBinRanges[yj].hi;

            for (int xb = xBinLo; xb <= xBinHi; ++xb)
                for (int yb = yLo; yb <= yHi; ++yb)
                {
                    data->setCell(xb, yb, data->cell(xb, yb) + zVal);
                    counts[xb * ny + yb] += 1;
                }
        }
    }

    for (int i = 0; i < nx; ++i)
    {
        for (int j = 0; j < ny; ++j)
        {
            uint32_t c = counts[i * ny + j];
            if (c > 0)
                data->setCell(i, j, data->cell(i, j) / c);
            else
                data->setCell(i, j, std::nan(""));
        }
    }

    data->recalculateDataBounds();
    return data;
}

} // namespace qcp::algo2d
