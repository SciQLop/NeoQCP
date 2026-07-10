#pragma once

#include <cstdint>
#include <vector>

class QCPAbstractDataSource2D;
class QCPColorMapData;
class QCPRange;

namespace qcp::algo2d {

struct ResampleCache
{
    std::vector<double> yAxis;
    double yLower = 0, yUpper = 0;
    int ny = 0;
    bool yLog = false;

    // Reusable scratch buffers for the accumulation loop. Reused across jobs
    // via assign() (always resets every element, whatever the previous size
    // or content) so back-to-back pan/zoom jobs at the same target size
    // don't reallocate on every call.
    std::vector<double> accum;
    std::vector<uint32_t> counts;
    std::vector<bool> gapBetween;
};

// Core resampling algorithm.
// Output grid is locked to the viewport (xRange/yRange) at pixel resolution
// (targetWidth x targetHeight). Returns a new QCPColorMapData*. Caller owns it.
// Returns nullptr if input is insufficient (srcCount < 2, zero target size, etc.).
// If cache is non-null and the Y parameters match, reuses the cached Y axis
// (avoids expensive pow10 recomputation on X-only pans).
//
// The accumulation loop -- O(visible source cells), independent of target
// grid size -- is split across worker threads by target-bin range once the
// job is large enough to amortize dispatch cost; each thread's writes land
// in disjoint output slices, so no locking is needed. forceSerial is a
// test-only knob to get a single-threaded reference for correctness
// comparisons; production callers should leave it false.
QCPColorMapData* resample(
    const QCPAbstractDataSource2D& src,
    int xBegin, int xEnd,
    const QCPRange& xRange, const QCPRange& yRange,
    int targetWidth, int targetHeight,
    bool yLogScale,
    double gapThreshold,
    ResampleCache* cache = nullptr,
    bool forceSerial = false);

} // namespace qcp::algo2d
