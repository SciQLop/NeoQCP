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
#include <QAtomicInt>
#include <QSemaphore>
#include <QThread>

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

std::vector<double> generateBinEdges(const std::vector<double>& centers)
{
    int n = static_cast<int>(centers.size());
    std::vector<double> edges(n + 1);
    if (n == 0) return edges;
    if (n == 1)
    {
        edges[0] = centers[0] - 0.5;
        edges[1] = centers[0] + 0.5;
        return edges;
    }
    edges[0] = centers[0] - (centers[1] - centers[0]) * 0.5;
    for (int i = 1; i < n; ++i)
        edges[i] = (centers[i - 1] + centers[i]) * 0.5;
    edges[n] = centers[n - 1] + (centers[n - 1] - centers[n - 2]) * 0.5;
    return edges;
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

int lowerBoundVirtual(const QCPAbstractDataSource2D& src, int begin, int end, double value)
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

// Accessors that use raw pointers when available, virtual calls otherwise.
struct RawAccessor
{
    const double* x;
    const double* y;
    const double* z;
    int ys;
    bool yIs2D;

    double xAt(int i) const { return x[i]; }
    double yAt(int i, int j) const { return yIs2D ? y[i * ys + j] : y[j]; }
    double zAt(int i, int j) const { return z[i * ys + j]; }

    int lowerBound(int begin, int end, double value) const
    {
        return lowerBoundRaw(x, begin, end, value);
    }
};

struct VirtualAccessor
{
    const QCPAbstractDataSource2D& src;
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
template <typename Accessor>
void resampleRange(
    const Accessor& acc,
    int xbBegin, int xbEnd,
    int xBegin, int xEnd, int ctxBegin, int ctxCount,
    const std::vector<double>& xAxis, const std::vector<double>& xEdges,
    const std::vector<bool>& gapBetween,
    const std::vector<double>& yAxis,
    int ny, int ys,
    bool yLogScale, bool variableY,
    double* accum, uint32_t* counts)
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
            else if (yLogScale && yVal > 0)
            {
                double prev = (yj > 0) ? acc.yAt(col, yj - 1) : 0;
                double next = (yj < ys - 1) ? acc.yAt(col, yj + 1) : 0;
                yLo = (yj > 0 && prev > 0) ? std::sqrt(prev * yVal) : yVal;
                yHi = (yj < ys - 1 && next > 0) ? std::sqrt(yVal * next) : yVal;
            }
            else
            {
                double halfBelow = (yj > 0) ? (yVal - acc.yAt(col, yj - 1)) * 0.5 : 0;
                double halfAbove = (yj < ys - 1) ? (acc.yAt(col, yj + 1) - yVal) * 0.5 : 0;
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

    // Seed this chunk's own cursor: for the first chunk this is xBegin (same
    // as the single-threaded scan); for any other chunk, a binary search at
    // the chunk's first bin edge (mirrors qcp::algo::binMinMaxParallel's
    // per-chunk findBegin seeding).
    int srcCursor = (xbBegin == 0) ? xBegin : acc.lowerBound(xBegin, xEnd, xEdges[xbBegin]);

    for (int xb = xbBegin; xb < xbEnd; ++xb)
    {
        double binLo = xEdges[xb];
        double binHi = xEdges[xb + 1];

        int colBegin = acc.lowerBound(srcCursor, xEnd, binLo);
        if (colBegin > xBegin) --colBegin;

        int colEnd = acc.lowerBound(colBegin, xEnd, binHi);
        if (colEnd < xEnd) ++colEnd;

        for (int xi = colBegin; xi < colEnd; ++xi)
        {
            int ci = xi - ctxBegin;
            double xVal = acc.xAt(xi);

            bool gapLeft = (ci > 0) && gapBetween[ci - 1];
            bool gapRight = (ci < ctxCount - 1) && gapBetween[ci];

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

            double fillLo = xVal - xHalfSpacing;
            double fillHi = xVal + xHalfSpacing;

            if (fillHi < binLo || fillLo > binHi)
                continue;

            int xBinLo = findBin(fillLo, xAxis);
            int xBinHi = findBinFloor(fillHi, xAxis);

            if (variableY)
                computeYBinRanges(xi, yBinRanges);

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

                for (int ob = std::max(xBinLo, xb); ob <= std::min(xBinHi, xb); ++ob)
                    for (int yb = yLo; yb <= yHi; ++yb)
                    {
                        int idx = ob * ny + yb;
                        accum[idx] += zVal;
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
    bool yLogScale, bool variableY,
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

    auto worker = [&](int xbBegin, int xbEnd) {
        resampleRange(acc, xbBegin, xbEnd, xBegin, xEnd, ctxBegin, ctxCount,
                      xAxis, xEdges, gapBetween, yAxis, ny, ys, yLogScale, variableY,
                      accum.data(), counts.data());
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
        QAtomicInt remaining(threadCount - 1);
        QSemaphore done;
        for (int t = 0; t < threadCount; ++t)
        {
            int begin = t * perChunk;
            int end = (t == threadCount - 1) ? count : (t + 1) * perChunk;
            if (t < threadCount - 1)
                qcp::algo::innerPool().start([&, begin, end] {
                    body(begin, end);
                    if (remaining.fetchAndSubRelaxed(1) == 1)
                        done.release();
                });
            else
                body(begin, end); // current thread does the last chunk
        }
        if (remaining.loadRelaxed() > 0)
            done.acquire();
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
    auto writeOutput = [&](int iBegin, int iEnd) {
        for (int i = iBegin; i < iEnd; ++i)
        {
            for (int j = 0; j < ny; ++j)
            {
                int srcIdx = i * ny + j;
                int dstIdx = j * nx + i;
                outData[dstIdx] = counts[srcIdx] > 0 ? accum[srcIdx] / counts[srcIdx]
                                                      : std::nan("");
            }
        }
    };
    if (threadCount <= 1 || static_cast<long long>(nx) * ny < 1'000'000)
        writeOutput(0, nx);
    else
        dispatchParallel(nx, threadCount, writeOutput);
}

} // anonymous namespace

QCPColorMapData* resample(
    const QCPAbstractDataSource2D& src,
    int xBegin, int xEnd,
    const QCPRange& xRange, const QCPRange& yRange,
    int targetWidth, int targetHeight,
    bool yLogScale,
    double gapThreshold,
    ResampleCache* cache,
    bool forceSerial)
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

    // Reuse cached Y axis when Y parameters haven't changed (avoids pow10 on X-only pans)
    std::vector<double> yAxis;
    if (cache && cache->ny == ny && cache->yLog == yLogScale
        && cache->yLower == yRange.lower && cache->yUpper == yRange.upper)
    {
        yAxis = cache->yAxis;
    }
    else
    {
        yAxis = generateRange(yRange.lower, yRange.upper, ny, yLogScale);
        if (cache)
        {
            cache->yAxis = yAxis;
            cache->yLower = yRange.lower;
            cache->yUpper = yRange.upper;
            cache->ny = ny;
            cache->yLog = yLogScale;
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

    auto xEdges = generateBinEdges(xAxis);

    const double* rawX = src.rawX();
    const double* rawY = src.rawY();
    const double* rawZ = src.rawZ();

    if (rawX && rawY && rawZ)
    {
        RawAccessor acc{rawX, rawY, rawZ, ys, variableY};
        resampleImpl(acc, xBegin, xEnd, ctxBegin, ctxEnd,
                     xAxis, yAxis, xEdges, nx, ny, ys,
                     yLogScale, variableY, gapThreshold, data->rawData(), cache, forceSerial);
    }
    else
    {
        VirtualAccessor acc{src, ys, variableY};
        resampleImpl(acc, xBegin, xEnd, ctxBegin, ctxEnd,
                     xAxis, yAxis, xEdges, nx, ny, ys,
                     yLogScale, variableY, gapThreshold, data->rawData(), cache, forceSerial);
    }

    data->recalculateDataBounds();
    return data;
}

} // namespace qcp::algo2d
