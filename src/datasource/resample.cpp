#include "resample.h"
#include "abstract-datasource-2d.h"
#include "Profiling.hpp"
#include "graph-resampler.h" // qcp::algo::innerPool(), innerThreadCount()
#include <axis/range.h>
#include <plottables/plottable-colormap.h> // for QCPColorMapData
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
#include <QThread>

namespace qcp::algo2d {

namespace {

std::vector<double> generateRange(double start, double end, int n, const QCPScale& scale)
{
    std::vector<double> range(n);
    if (n <= 1)
    {
        if (n == 1)
            range[0] = start;
        return range;
    }
    // Log10 rather than QCPScale::toScale: a log axis keeps the exact arithmetic it has always
    // had, or its reference pictures move.
    if (scale.isLog() && start > 0 && end > 0)
    {
        double logStart = std::log10(start);
        double step = (std::log10(end) - logStart) / (n - 1);
        for (int i = 0; i < n; ++i)
            range[i] = std::pow(10.0, logStart + i * step);
    }
    else if (scale.isSymlog())
    {
        const double scaleStart = scale.toScale(start);
        const double step = (scale.toScale(end) - scaleStart) / (n - 1);
        for (int i = 0; i < n; ++i)
            range[i] = scale.fromScale(scaleStart + i * step);
    }
    else
    {
        double step = (end - start) / (n - 1);
        for (int i = 0; i < n; ++i)
            range[i] = start + i * step;
    }
    return range;
}

// Column boundaries halfway between two columns *on screen*: arithmetic on a linear axis,
// geometric on a log one, the scale-space midpoint on any other. The linear branch is the one it
// has always been, to the operation.
std::vector<double> generateBinEdges(const std::vector<double>& centers, const QCPScale& scale)
{
    int n = static_cast<int>(centers.size());
    std::vector<double> edges(n + 1);
    if (n == 0) return edges;
    const bool log = scale.isLog() && centers.front() > 0 && centers.back() > 0;
    const bool symlog = !log && scale.isSymlog();
    if (n == 1)
    {
        if (log)
        {
            edges[0] = centers[0] / std::sqrt(10.0);
            edges[1] = centers[0] * std::sqrt(10.0);
        }
        else if (symlog)
        {
            const double pos = scale.toScale(centers[0]);
            edges[0] = scale.fromScale(pos - 0.5);
            edges[1] = scale.fromScale(pos + 0.5);
        }
        else
        {
            edges[0] = centers[0] - 0.5;
            edges[1] = centers[0] + 0.5;
        }
        return edges;
    }
    if (log)
    {
        edges[0] = centers[0] / std::sqrt(centers[1] / centers[0]);
        for (int i = 1; i < n; ++i)
            edges[i] = std::sqrt(centers[i - 1] * centers[i]);
        edges[n] = centers[n - 1] * std::sqrt(centers[n - 1] / centers[n - 2]);
        return edges;
    }
    if (symlog)
    {
        const auto mid = [&](double a, double b)
        { return scale.fromScale((scale.toScale(a) + scale.toScale(b)) * 0.5); };
        const double lo = scale.toScale(centers[0]);
        const double loStep = scale.toScale(centers[1]) - lo;
        edges[0] = scale.fromScale(lo - loStep * 0.5);
        for (int i = 1; i < n; ++i)
            edges[i] = mid(centers[i - 1], centers[i]);
        const double hi = scale.toScale(centers[n - 1]);
        const double hiStep = hi - scale.toScale(centers[n - 2]);
        edges[n] = scale.fromScale(hi + hiStep * 0.5);
        return edges;
    }
    edges[0] = centers[0] - (centers[1] - centers[0]) * 0.5;
    for (int i = 1; i < n; ++i)
        edges[i] = (centers[i - 1] + centers[i]) * 0.5;
    edges[n] = centers[n - 1] + (centers[n - 1] - centers[n - 2]) * 0.5;
    return edges;
}

// How a column's samples are accumulated. Each space is a template argument rather than a runtime
// flag: a call the compiler cannot see into (toScale, fromScale) must not sit in the per-sample
// code of the linear and log paths, where it costs ~20 % on a wide resample.
enum class ValueSpace { Linear, Log10, Scale };

template <ValueSpace Space>
double sampleValue(const QCPScale& scale, double v)
{
    if constexpr (Space == ValueSpace::Log10) return std::log10(v);
    else if constexpr (Space == ValueSpace::Scale) return scale.toScale(v);
    else return v;
}

template <ValueSpace Space>
double valueFromMean(const QCPScale& scale, double v)
{
    if constexpr (Space == ValueSpace::Log10) return std::pow(10.0, v);
    else if constexpr (Space == ValueSpace::Scale) return scale.fromScale(v);
    else return v;
}

template <typename F>
void dispatchValueSpace(ValueSpace space, F&& f)
{
    switch (space)
    {
    case ValueSpace::Log10: f(std::integral_constant<ValueSpace, ValueSpace::Log10> {}); break;
    case ValueSpace::Scale: f(std::integral_constant<ValueSpace, ValueSpace::Scale> {}); break;
    default: f(std::integral_constant<ValueSpace, ValueSpace::Linear> {}); break;
    }
}

// The two ends of a log-axis sample's span: geometric midpoints with its neighbours, and at an
// end the known half mirrored (value^2 / other) instead of collapsing onto the sample itself.
// \a above receives the upper end, the return value the lower one.
double geometricHalf(double previous, double value, double next, double* above)
{
    const bool hasBelow = previous > 0.0;
    const bool hasAbove = next > 0.0;
    double lower = hasBelow ? std::sqrt(previous * value) : 0.0;
    double upper = hasAbove ? std::sqrt(value * next) : 0.0;
    if (!hasBelow) lower = hasAbove ? value * value / upper : value;
    if (!hasAbove) upper = hasBelow ? value * value / lower : value;
    *above = std::max(lower, upper);
    return std::min(lower, upper);
}

int lowerBoundRaw(const double* x, int begin, int end, double value)
{
    int lo = begin, hi = end;
    while (lo < hi)
    {
        int mid = lo + (hi - lo) / 2;
        if (x[mid] < value)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

int lowerBoundVirtual(const QCPDataSource2D& src, int begin, int end, double value)
{
    int lo = begin, hi = end;
    while (lo < hi)
    {
        int mid = lo + (hi - lo) / 2;
        if (src.xAt(mid) < value)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
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

// Accessors that use raw arrays when available, virtual calls otherwise.
template <typename YValues, typename ZValues>
struct RawAccessor
{
    const double* x;
    YValues y;
    ZValues z;
    int ys;
    bool yIs2D;

    double xAt(int i) const { return x[i]; }
    double yAt(int i, int j) const
    {
        return yIs2D ? y[static_cast<std::ptrdiff_t>(i) * ys + j] : y[j];
    }
    double zAt(int i, int j) const { return z[static_cast<std::ptrdiff_t>(i) * ys + j]; }

    int lowerBound(int begin, int end, double value) const
    {
        return lowerBoundRaw(x, begin, end, value);
    }
};

struct VirtualAccessor
{
    const QCPDataSource2D& src;
    int ys;
    bool yIs2D;

    double xAt(int i) const { return src.xAt(i); }
    double yAt(int i, int j) const { return src.yAt(i, j); }
    double zAt(int i, int j) const { return src.zAt(i, j); }

    int lowerBound(int begin, int end, double value) const
    {
        return lowerBoundVirtual(src, begin, end, value);
    }
};

struct BinRange { int lo, hi; };

// Processes target bins [xbBegin, xbEnd) only, writing into the shared
// accum/counts buffers (sized nx*ny) at indices xb*ny+yb for xb in that
// range. Disjoint target-bin ranges write disjoint index ranges, so this is
// safe to call from multiple threads concurrently with no locking, as long
// as each thread owns a non-overlapping [xbBegin, xbEnd) partition -- each
// worker seeds its own local srcCursor and yBinRanges rather than sharing
// the sequential-scan state the single-chunk case relies on.
template <ValueSpace Space, typename Accessor>
void resampleRange(
    const Accessor& acc,
    int xbBegin, int xbEnd,
    int xBegin, int xEnd, int ctxBegin, int ctxCount,
    const std::vector<double>& xAxis, const std::vector<double>& xEdges,
    const std::vector<bool>& gapBetween,
    const std::vector<double>& yAxis,
    int ny, int ys,
    const QCPScale& xScale, const QCPScale& yScale, bool yLog, bool ySymlog, bool variableY,
    const QCPScale& zScale, double* accum, uint32_t* counts, uint8_t* nonPositive)
{
    auto computeYBinRanges = [&](int col, std::vector<BinRange>& ranges) {
        for (int yj = 0; yj < ys; ++yj)
        {
            double yVal = acc.yAt(col, yj);
            double yLo, yHi;
            if (ys == 1)
            {
                yLo = yHi = yVal;
            }
            else if (yLog && yVal > 0)
            {
                // Boundaries are the geometric means with each neighbour; assign
                // the smaller to yLo and the larger to yHi so a descending y
                // (e.g. a spectrogram frequency axis stored top-of-band-first)
                // doesn't invert the range. yAxis (the output grid) is always
                // ascending, so an inverted [yLo, yHi] yields lo > hi below and
                // the accumulation loop skips every cell -> empty colormap.
                // The ends mirror the known half (see geometricHalf): a log axis' first and last
                // row cover a full row, as its middle rows do.
                const double prev = (yj > 0) ? acc.yAt(col, yj - 1) : 0.0;
                const double next = (yj < ys - 1) ? acc.yAt(col, yj + 1) : 0.0;
                double gmNext = 0.0;
                yLo = geometricHalf(prev, yVal, next, &gmNext);
                yHi = gmNext;
            }
            else if (ySymlog)
            {
                // Boundaries are the scale-space midpoints with each neighbour, so the rows cover
                // equal spans of the axis however the values are distributed on it.
                const double sp = yScale.toScale(yVal);
                const double below = (yj > 0) ? (yScale.toScale(acc.yAt(col, yj - 1)) + sp) * 0.5
                                             : 0.0;
                const double above = (yj < ys - 1)
                    ? (sp + yScale.toScale(acc.yAt(col, yj + 1))) * 0.5
                    : 0.0;
                const double bBelow = yj > 0 ? below : 2.0 * sp - above;
                const double bAbove = yj < ys - 1 ? above : 2.0 * sp - below;
                yLo = std::min(yScale.fromScale(bBelow), yScale.fromScale(bAbove));
                yHi = std::max(yScale.fromScale(bBelow), yScale.fromScale(bAbove));
            }
            else
            {
                // std::abs so a descending y stays a positive half-spacing:
                // without it both halves go negative for a top-of-band-first
                // frequency axis, yLo > yHi, and the target-bin range inverts
                // (ranges.lo > ranges.hi) so nothing is rasterized.
                double halfBelow = (yj > 0) ? std::abs(yVal - acc.yAt(col, yj - 1)) * 0.5 : 0;
                double halfAbove = (yj < ys - 1) ? std::abs(acc.yAt(col, yj + 1) - yVal) * 0.5 : 0;
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

    // A column covers the key span halfway to its neighbours *on screen*, as a row does.
    const bool xLog = xScale.isLog();
    const bool xSymlog = !xLog && xScale.isSymlog();

    // Seed this chunk's own cursor: for the first chunk this is xBegin (same
    // as the single-threaded scan); for any other chunk, a binary search at
    // the chunk's first bin edge (mirrors qcp::algo::binMinMaxMultiParallel's
    // per-chunk findBegin seeding).
    int srcCursor = (xbBegin == 0) ? xBegin : acc.lowerBound(xBegin, xEnd, xEdges[xbBegin]);

    for (int xb = xbBegin; xb < xbEnd; ++xb)
    {
        double binLo = xEdges[xb];
        double binHi = xEdges[xb + 1];

        const int keyBegin = acc.lowerBound(srcCursor, xEnd, binLo);
        int colBegin = keyBegin;
        if (colBegin > xBegin) --colBegin;

        int colEnd = acc.lowerBound(colBegin, xEnd, binHi);
        if (colEnd < xEnd) ++colEnd;
        // A bin with no sample of its own (a grid coarser than the pixels) is filled by the
        // column whose span covers its centre, so a sparse key axis still paints its own band.
        // keyBegin already is the first key at or past binLo, so this is a comparison, not a search.
        const bool binHasKeys = keyBegin < xEnd && acc.xAt(keyBegin) < binHi;
        const double binCenter = (binLo + binHi) * 0.5;

        for (int xi = colBegin; xi < colEnd; ++xi)
        {
            int ci = xi - ctxBegin;
            double xVal = acc.xAt(xi);

            bool gapLeft = (ci > 0) && gapBetween[ci - 1];
            bool gapRight = (ci < ctxCount - 1) && gapBetween[ci];

            double fillLo, fillHi;
            if (xLog || xSymlog)
            {
                const double previous = (ci > 0 && !gapLeft) ? acc.xAt(ctxBegin + ci - 1) : 0.0;
                const double next = (ci < ctxCount - 1 && !gapRight) ? acc.xAt(ctxBegin + ci + 1)
                                                                     : 0.0;
                if (xSymlog)
                {
                    // Scale-space midpoints, a missing end mirrored in the same space.
                    const double pos = xScale.toScale(xVal);
                    const double lo = std::isfinite(previous) && previous != 0.0
                        ? xScale.toScale(previous) : pos;
                    const double hi = std::isfinite(next) && next != 0.0 ? xScale.toScale(next)
                                                                        : pos;
                    double below = (pos + lo) * 0.5;
                    double above = (pos + hi) * 0.5;
                    if (ci <= 0 || gapLeft) below = 2.0 * pos - above;
                    if (ci >= ctxCount - 1 || gapRight) above = 2.0 * pos - below;
                    fillLo = xScale.fromScale(std::min(below, above));
                    fillHi = xScale.fromScale(std::max(below, above));
                }
                else if (xVal > 0.0)
                    fillLo = geometricHalf(previous, xVal, next, &fillHi);
                else
                    fillLo = fillHi = std::numeric_limits<double>::quiet_NaN();
            }
            else
            {
                double leftDist = (!gapLeft && ci > 0)
                    ? xVal - acc.xAt(ctxBegin + ci - 1) : std::numeric_limits<double>::max();
                double rightDist = (!gapRight && ci < ctxCount - 1)
                    ? acc.xAt(ctxBegin + ci + 1) - xVal : std::numeric_limits<double>::max();

                if (leftDist == std::numeric_limits<double>::max() && rightDist < std::numeric_limits<double>::max())
                    leftDist = rightDist;
                if (rightDist == std::numeric_limits<double>::max() && leftDist < std::numeric_limits<double>::max())
                    rightDist = leftDist;

                double xHalfSpacing = (leftDist < std::numeric_limits<double>::max())
                    ? std::min(leftDist, rightDist) * 0.5 : 0;

                fillLo = xVal - xHalfSpacing;
                fillHi = xVal + xHalfSpacing;
            }

            if (fillHi < binLo || fillLo > binHi)
                continue;

            int xBinLo = findBin(fillLo, xAxis);
            int xBinHi = findBinFloor(fillHi, xAxis);

            // A sample contributes to the bin that holds its key; with no sample in the bin, the
            // one whose span covers the bin's centre does. Where a bin holds samples, exactly
            // those contribute, as they always have.
            const bool keyInBin = xb >= xBinLo && xb <= xBinHi;
            if (!keyInBin && (binHasKeys || !(fillLo <= binCenter && binCenter <= fillHi)))
                continue;

            if (variableY)
                computeYBinRanges(xi, yBinRanges);

            int obBase = xb * ny;

            for (int yj = 0; yj < ys; ++yj)
            {
                double yVal = acc.yAt(xi, yj);
                if (std::isnan(yVal))
                    continue;
                double zVal = acc.zAt(xi, yj);
                if (std::isnan(zVal))
                    continue;

                int yLo = yBinRanges[yj].lo;
                int yHi = yBinRanges[yj].hi;

                if constexpr (Space == ValueSpace::Log10)
                {
                    if (zVal <= 0)
                    {
                        for (int yb = yLo; yb <= yHi; ++yb)
                            nonPositive[obBase + yb] = 1;
                        continue;
                    }
                }
                const double sample = sampleValue<Space>(zScale, zVal);
                for (int yb = yLo; yb <= yHi; ++yb)
                {
                    int idx = obBase + yb;
                    accum[idx] += sample;
                    counts[idx] += 1;
                }
            }
        }

        srcCursor = colBegin;
    }
}

template <typename Accessor>
void resampleImpl(
    const Accessor& acc,
    int xBegin, int xEnd, int ctxBegin, int ctxEnd,
    const std::vector<double>& xAxis, const std::vector<double>& yAxis,
    const std::vector<double>& xEdges,
    int nx, int ny, int ys,
    const QCPScale& yScale, bool variableY, const QCPScale& zScale, const QCPScale& xScale,
    double gapThreshold,
    double* outData,
    ResampleCache* cache,
    bool forceSerial)
{
    int ctxCount = ctxEnd - ctxBegin;

    std::vector<bool> localGapBetween;
    std::vector<bool>& gapBetween = cache ? cache->gapBetween : localGapBetween;
    gapBetween.assign(ctxCount, false);

    // Gap detection
    if (gapThreshold > 0 && ctxCount > 2)
    {
        for (int i = 0; i < ctxCount - 1; ++i)
        {
            double dx = acc.xAt(ctxBegin + i + 1) - acc.xAt(ctxBegin + i);
            double refDx = std::numeric_limits<double>::max();
            if (i > 0)
                refDx = std::min(refDx, acc.xAt(ctxBegin + i) - acc.xAt(ctxBegin + i - 1));
            if (i + 2 < ctxCount)
                refDx = std::min(refDx, acc.xAt(ctxBegin + i + 2) - acc.xAt(ctxBegin + i + 1));
            if (refDx < std::numeric_limits<double>::max() && dx > gapThreshold * refDx)
                gapBetween[i] = true;
        }
    }

    // Accumulation buffers
    int total = nx * ny;
    std::vector<double> localAccum;
    std::vector<uint32_t> localCounts;
    std::vector<double>& accum = cache ? cache->accum : localAccum;
    std::vector<uint32_t>& counts = cache ? cache->counts : localCounts;
    accum.assign(total, 0.0);
    counts.assign(total, 0);
    std::vector<uint8_t> localNonPositive;
    std::vector<uint8_t>& nonPositive = cache ? cache->nonPositive : localNonPositive;
    const bool yLog = yScale.isLog();
    const bool ySymlog = yScale.isSymlog();
    const ValueSpace zSpace = zScale.isLog() ? ValueSpace::Log10
        : (zScale.isSymlog() ? ValueSpace::Scale : ValueSpace::Linear);
    nonPositive.assign(zSpace == ValueSpace::Log10 ? total : 0, 0);

    auto worker = [&](int xbBegin, int xbEnd) {
        dispatchValueSpace(zSpace, [&](auto space)
        {
            resampleRange<decltype(space)::value>(acc, xbBegin, xbEnd, xBegin, xEnd, ctxBegin,
                                                 ctxCount, xAxis, xEdges, gapBetween, yAxis, ny,
                                                 ys, xScale, yScale, yLog, ySymlog, variableY,
                                                 zScale, accum.data(), counts.data(),
                                                 nonPositive.data());
        });
    };

    // Parallelize by target-bin range: each thread's writes land in disjoint
    // xb*ny+yb slices, so no synchronization is needed beyond waiting for all
    // chunks to finish. Only worth it once there's enough work to amortize
    // the thread dispatch cost (cost is ~O(visible source cells) regardless
    // of target size, so gate on that, not on nx).
    long long cellBudget = static_cast<long long>(xEnd - xBegin) * ys;
    int threadCount = forceSerial ? 1 : qcp::algo::innerThreadCount();
    threadCount = std::min(threadCount, nx);
    // Splits [0, count) into `threadCount` contiguous chunks and runs `body`
    // on each -- threadCount-1 chunks on the pool, the last on the calling
    // thread. Only used once the job is large enough to amortize dispatch.
    auto dispatchParallel = [](int count, int threadCount, auto&& body) {
        int perChunk = count / threadCount;
        qcp::algo::QCPPoolJoin join;
        for (int t = 0; t < threadCount; ++t)
        {
            int begin = t * perChunk;
            int end = (t == threadCount - 1) ? count : (t + 1) * perChunk;
            if (t < threadCount - 1)
                join.start([&, begin, end] { body(begin, end); });
            else
                body(begin, end); // current thread does the last chunk
        }
        join.wait();
    };

    if (threadCount <= 1 || cellBudget < 1'000'000)
        worker(0, nx);
    else
        dispatchParallel(nx, threadCount, worker);

    // Write directly to output array (layout: valueIndex * keySize + keyIndex).
    // O(nx*ny) -- independent of source size, so gate on the output grid
    // itself rather than cellBudget; parallelizable the same way since each
    // thread owns a disjoint range of source columns i (and therefore of
    // dstIdx = j*nx+i, since i doesn't repeat across chunks).
    // A log bin holding only z <= 0 keeps 0 so it still draws the lowest colour.
    auto writeOutput = [&](int iBegin, int iEnd) {
        dispatchValueSpace(zSpace, [&](auto space)
        {
            constexpr ValueSpace S = decltype(space)::value;
            for (int i = iBegin; i < iEnd; ++i)
            {
                for (int j = 0; j < ny; ++j)
                {
                    const int srcIdx = i * ny + j;
                    double value = std::nan("");
                    if (counts[srcIdx] != 0)
                        value = valueFromMean<S>(zScale, accum[srcIdx] / counts[srcIdx]);
                    else if constexpr (S == ValueSpace::Log10)
                        if (nonPositive[srcIdx]) value = 0.0;
                    outData[j * nx + i] = value;
                }
            }
        });
    };
    if (threadCount <= 1 || static_cast<long long>(nx) * ny < 1'000'000)
        writeOutput(0, nx);
    else
        dispatchParallel(nx, threadCount, writeOutput);
}

} // anonymous namespace

QCPColorMapData* resample(
    const QCPDataSource2D& src,
    int xBegin, int xEnd,
    const QCPRange& xRange, const QCPRange& yRange,
    int targetWidth, int targetHeight,
    const QCPScale& yScale,
    const QCPScale& zScale,
    double gapThreshold,
    ResampleCache* cache,
    bool forceSerial,
    const QCPScale& xScale)
{
    PROFILE_HERE_N("resample");
    int srcCount = xEnd - xBegin;
    if (srcCount < 2 || targetWidth < 1 || targetHeight < 1)
        return nullptr;
    if (xRange.lower >= xRange.upper || yRange.lower >= yRange.upper)
        return nullptr;

    int nx = targetWidth;
    int ny = targetHeight;

    auto xAxis = generateRange(xRange.lower, xRange.upper, nx, xScale);

    // Reuse cached Y axis when Y parameters haven't changed (avoids pow10 on X-only pans)
    std::vector<double> yAxis;
    if (cache && cache->ny == ny && cache->yScale == yScale
        && cache->yLower == yRange.lower && cache->yUpper == yRange.upper)
    {
        yAxis = cache->yAxis;
    }
    else
    {
        yAxis = generateRange(yRange.lower, yRange.upper, ny, yScale);
        if (cache)
        {
            cache->yAxis = yAxis;
            cache->yLower = yRange.lower;
            cache->yUpper = yRange.upper;
            cache->ny = ny;
            cache->yScale = yScale;
        }
    }

    int ys = src.ySize();
    bool variableY = src.yIs2D();

    auto* data = new QCPColorMapData(nx, ny, {xAxis.front(), xAxis.back()},
                                              {yAxis.front(), yAxis.back()});
    if (!data->rawData())
    {
        delete data;
        return nullptr;
    }

    int ctxBegin = std::max(0, xBegin - 1);
    int ctxEnd = std::min(src.xSize(), xEnd + 1);

    auto xEdges = generateBinEdges(xAxis, xScale);

    auto run = [&](const auto& acc) {
        resampleImpl(acc, xBegin, xEnd, ctxBegin, ctxEnd,
                     xAxis, yAxis, xEdges, nx, ny, ys,
                     yScale, variableY, zScale, xScale, gapThreshold, data->rawData(), cache,
                     forceSerial);
    };
    bool ranRaw = false;
    if (const double* rawX = src.rawX())
        visitRawColumn(src.rawY(), [&](auto y) {
            ranRaw = visitRawColumn(src.rawZ(), [&](auto z) {
                run(RawAccessor<decltype(y), decltype(z)>{rawX, y, z, ys, variableY});
            });
        });
    if (!ranRaw)
        run(VirtualAccessor{src, ys, variableY});

    data->recalculateDataBounds();
    return data;
}

} // namespace qcp::algo2d
