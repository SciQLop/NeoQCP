#include "contour-extractor.h"
#include <plottables/plottable-colormap.h>
#include <cmath>

namespace QCPContourExtractor
{

namespace
{

struct Cell { double v0, v1, v2, v3; };

inline double lerp1d(double a, double va, double b, double vb, double level)
{
    double t = (vb == va) ? 0.5 : (level - va) / (vb - va);
    return a + t * (b - a);
}

inline void marchCell(double kLo, double kHi, double vLo, double vHi,
                      const Cell& c, double level, QVector<QLineF>& out)
{
    int idx = 0;
    if (c.v0 >= level) idx |= 1;
    if (c.v1 >= level) idx |= 2;
    if (c.v2 >= level) idx |= 4;
    if (c.v3 >= level) idx |= 8;

    if (idx == 0 || idx == 15)
        return;

    // Edge interpolation (inlined, no QPointF allocation)
    // Bottom: v0→v1 (kLo,vLo)→(kHi,vLo)
    // Right:  v1→v2 (kHi,vLo)→(kHi,vHi)
    // Top:    v3→v2 (kLo,vHi)→(kHi,vHi)
    // Left:   v0→v3 (kLo,vLo)→(kLo,vHi)
    double bk = lerp1d(kLo, c.v0, kHi, c.v1, level), bv = vLo;
    double rk = kHi, rv = lerp1d(vLo, c.v1, vHi, c.v2, level);
    double tk = lerp1d(kLo, c.v3, kHi, c.v2, level), tv = vHi;
    double lk = kLo, lv = lerp1d(vLo, c.v0, vHi, c.v3, level);

    auto seg = [&](double x1, double y1, double x2, double y2) {
        out.append(QLineF(x1, y1, x2, y2));
    };

    switch (idx)
    {
        case  1: case 14: seg(bk,bv, lk,lv); break;
        case  2: case 13: seg(bk,bv, rk,rv); break;
        case  3: case 12: seg(lk,lv, rk,rv); break;
        case  4: case 11: seg(rk,rv, tk,tv); break;
        case  6: case  9: seg(bk,bv, tk,tv); break;
        case  7: case  8: seg(lk,lv, tk,tv); break;
        case 5:
        {
            double center = (c.v0 + c.v1 + c.v2 + c.v3) * 0.25;
            if (center >= level) { seg(bk,bv,rk,rv); seg(lk,lv,tk,tv); }
            else                 { seg(bk,bv,lk,lv); seg(rk,rv,tk,tv); }
            break;
        }
        case 10:
        {
            double center = (c.v0 + c.v1 + c.v2 + c.v3) * 0.25;
            if (center >= level) { seg(bk,bv,lk,lv); seg(rk,rv,tk,tv); }
            else                 { seg(bk,bv,rk,rv); seg(lk,lv,tk,tv); }
            break;
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

    // Precompute coordinate arrays
    const double kStep = (keyRange.upper - keyRange.lower) / (kSize - 1);
    const double vStep = (valRange.upper - valRange.lower) / (vSize - 1);

    result.reserve(levels.size());

    for (double level : levels)
    {
        ContourLine cl;
        cl.level = level;
        cl.segments.reserve(kSize);

        for (int ki = 0; ki < kSize - 1; ++ki)
        {
            const double kLo = keyRange.lower + ki * kStep;
            const double kHi = kLo + kStep;
            const int rowLo = ki;
            const int rowHi = ki + 1;

            for (int vi = 0; vi < vSize - 1; ++vi)
            {
                // raw layout: mData[vi * kSize + ki]
                double v0 = raw[vi       * kSize + rowLo];
                double v1 = raw[vi       * kSize + rowHi];
                double v2 = raw[(vi + 1) * kSize + rowHi];
                double v3 = raw[(vi + 1) * kSize + rowLo];

                if (!std::isfinite(v0) || !std::isfinite(v1) ||
                    !std::isfinite(v2) || !std::isfinite(v3))
                    continue;

                double vLo = valRange.lower + vi * vStep;
                double vHi = vLo + vStep;

                marchCell(kLo, kHi, vLo, vHi, {v0, v1, v2, v3}, level, cl.segments);
            }
        }

        if (!cl.segments.isEmpty())
            result.append(std::move(cl));
    }

    return result;
}

} // namespace QCPContourExtractor
