#include "contour-extractor.h"
#include <plottables/plottable-colormap.h>
#include <algorithm>
#include <cmath>
#include <utility>

namespace QCPContourExtractor
{

namespace
{

struct Cell { double v0, v1, v2, v3; };
struct Pt { double x, y; };

inline double lerp1d(double a, double va, double b, double vb, double level)
{
    double t = (vb == va) ? 0.5 : (level - va) / (vb - va);
    return a + t * (b - a);
}

inline Pt lerpPt(const Pt& a, double va, const Pt& b, double vb, double level)
{
    return {lerp1d(a.x, va, b.x, vb, level), lerp1d(a.y, va, b.y, vb, level)};
}

// Shared marching-squares core. Corners: pBL=v0, pBR=v1, pTR=v2, pTL=v3
// (bits 1/2/4/8). emit() receives one segment's endpoints per call.
// Saddle cells (cases 5/10) use the cell-center-average heuristic: a
// deterministic, documented approximation, not the bilinear asymptotic
// decider. The average is scaled per corner first so four huge finite
// values can't overflow the sum into infinity and flip the branch.
template<typename EmitFn>
inline void marchCell(const Pt& pBL, const Pt& pBR, const Pt& pTR, const Pt& pTL,
                      const Cell& c, double level, EmitFn&& sink)
{
    int idx = 0;
    if (c.v0 >= level) idx |= 1;
    if (c.v1 >= level) idx |= 2;
    if (c.v2 >= level) idx |= 4;
    if (c.v3 >= level) idx |= 8;

    if (idx == 0 || idx == 15)
        return;

    // Edge crossings: bottom(v0→v1), right(v1→v2), top(v3→v2), left(v0→v3)
    const Pt b = lerpPt(pBL, c.v0, pBR, c.v1, level);
    const Pt r = lerpPt(pBR, c.v1, pTR, c.v2, level);
    const Pt t = lerpPt(pTL, c.v3, pTR, c.v2, level);
    const Pt l = lerpPt(pBL, c.v0, pTL, c.v3, level);

    auto seg = [&](const Pt& a, const Pt& b2) { sink(a, b2); };

    switch (idx)
    {
        case  1: case 14: seg(b, l); break;
        case  2: case 13: seg(b, r); break;
        case  3: case 12: seg(l, r); break;
        case  4: case 11: seg(r, t); break;
        case  6: case  9: seg(b, t); break;
        case  7: case  8: seg(l, t); break;
        case 5:
        {
            double center = c.v0 * 0.25 + c.v1 * 0.25 + c.v2 * 0.25 + c.v3 * 0.25;
            if (center >= level) { seg(b, r); seg(l, t); }
            else                 { seg(b, l); seg(r, t); }
            break;
        }
        case 10:
        {
            double center = c.v0 * 0.25 + c.v1 * 0.25 + c.v2 * 0.25 + c.v3 * 0.25;
            if (center >= level) { seg(b, l); seg(r, t); }
            else                 { seg(b, r); seg(l, t); }
            break;
        }
    }
}

// March one level over the grid, point-sampling every sk/sv-th sample.
// pos(sK, sV) maps a source sample to output coordinates. The coarse cell
// count ck = (kSize-2)/sk+2 with per-cell clamping (min(ci*sk, kSize-1))
// keeps full coverage: the last coarse cell always ends at the final
// row/column, so no tail strip is dropped and UVs stay registered to the
// source indices they were sampled from.
template<typename PosFn, typename EmitFn>
void marchCells(const double* raw, int kSize, int vSize, int sk, int sv,
                double level, PosFn&& pos, EmitFn&& sink)
{
    if (!std::isfinite(level))
        return;
    const int ck = (kSize - 2) / sk + 2;
    const int cv = (vSize - 2) / sv + 2;
    for (int ci = 0; ci < ck - 1; ++ci)
    {
        const int sK0 = std::min(ci * sk, kSize - 1);
        const int sK1 = std::min((ci + 1) * sk, kSize - 1);
        for (int cj = 0; cj < cv - 1; ++cj)
        {
            const int sV0 = std::min(cj * sv, vSize - 1);
            const int sV1 = std::min((cj + 1) * sv, vSize - 1);

            // raw layout: mData[vi * kSize + ki]
            const double z00 = raw[sV0 * kSize + sK0];
            const double z10 = raw[sV0 * kSize + sK1];
            const double z11 = raw[sV1 * kSize + sK1];
            const double z01 = raw[sV1 * kSize + sK0];

            if (!std::isfinite(z00) || !std::isfinite(z10) ||
                !std::isfinite(z11) || !std::isfinite(z01))
                continue;

            marchCell(pos(sK0, sV0), pos(sK1, sV0), pos(sK1, sV1), pos(sK0, sV1),
                      {z00, z10, z11, z01}, level, sink);
        }
    }
}

} // anonymous namespace

QVector<ContourLine> extract(const QCPColorMapData* data,
                             const QVector<double>& levels)
{
    QVector<ContourLine> result;
    if (!data || levels.isEmpty())
        return result;

    const int kSize = data->keySize();
    const int vSize = data->valueSize();
    if (kSize < 2 || vSize < 2)
        return result;

    const double* raw = data->rawData();
    const QCPRange keyRange = data->keyRange();
    const QCPRange valRange = data->valueRange();

    const double kStep = (keyRange.upper - keyRange.lower) / (kSize - 1);
    const double vStep = (valRange.upper - valRange.lower) / (vSize - 1);
    auto pos = [&](int sK, int sV) -> Pt {
        return {keyRange.lower + sK * kStep, valRange.lower + sV * vStep};
    };

    result.reserve(levels.size());

    for (double level : levels)
    {
        ContourLine cl;
        cl.level = level;
        cl.segments.reserve(kSize);
        marchCells(raw, kSize, vSize, 1, 1, level, pos,
                   [&](const Pt& a, const Pt& b) {
                       cl.segments.append(QLineF(a.x, a.y, b.x, b.y));
                   });

        if (!cl.segments.isEmpty())
            result.append(std::move(cl));
    }

    return result;
}

QVector<float> extractUv(const QCPColorMapData* data,
                         const QVector<double>& levels,
                         int maxDim)
{
    QVector<float> out;
    if (!data || levels.isEmpty())
        return out;

    const int kSize = data->keySize();
    const int vSize = data->valueSize();
    if (kSize < 2 || vSize < 2)
        return out;

    const double* raw = data->rawData();
    maxDim = std::max(2, maxDim);
    int sk = 1, sv = 1;
    if (kSize > maxDim) sk = kSize / maxDim;
    if (vSize > maxDim) sv = vSize / maxDim;

    auto pos = [&](int sK, int sV) -> Pt {
        return {double(sK) / (kSize - 1), 1.0 - double(sV) / (vSize - 1)};
    };

    const int ck = (kSize - 2) / sk + 2;
    const int cv = (vSize - 2) / sv + 2;
    out.reserve(ck * cv);

    for (double level : levels)
    {
        marchCells(raw, kSize, vSize, sk, sv, level, pos,
                   [&](const Pt& a, const Pt& b) {
                       out.append(float(a.x));
                       out.append(float(a.y));
                       out.append(float(b.x));
                       out.append(float(b.y));
                   });
    }

    return out;
}

} // namespace QCPContourExtractor
