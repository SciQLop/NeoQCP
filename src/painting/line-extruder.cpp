#include "line-extruder.h"
#include "rhi-utils.h"
#include "Profiling.hpp"
#include <QtMath>
#include <array>
#include <cstring>

namespace QCPLineExtruder
{

namespace
{

constexpr float MITER_LIMIT = 4.0f;

// Write cursor: tracks position in a pre-allocated float buffer.
// All vertex writes go through this — no per-float append() calls. The vertex format is a
// compile-time choice, so the solid path pays neither the arc length nor a branch per vertex.
template <bool WithDistance>
struct WriteCursor
{
    float* data;
    int pos = 0;

    //! \a arcLength is written only when the format carries it.
    void vertex(float x, float y, const std::array<float, 4>& rgba, float arcLength)
    {
        float* p = data + pos;
        p[0] = x;  p[1] = y;
        p[2] = rgba[0];  p[3] = rgba[1];  p[4] = rgba[2];  p[5] = rgba[3];
        if constexpr (WithDistance)
        {
            p[6] = arcLength;
            pos += 7;
        }
        else
        {
            (void)arcLength;
            pos += 6;
        }
    }

    //! \a dPrev is the arc length of the previous point, \a dCur of the current one.
    void quad(QPointF tl, QPointF tr, QPointF br, QPointF bl,
              const std::array<float, 4>& rgba, float dPrev, float dCur)
    {
        vertex(tl.x(), tl.y(), rgba, dPrev);
        vertex(tr.x(), tr.y(), rgba, dCur);
        vertex(br.x(), br.y(), rgba, dCur);
        vertex(tl.x(), tl.y(), rgba, dPrev);
        vertex(br.x(), br.y(), rgba, dCur);
        vertex(bl.x(), bl.y(), rgba, dPrev);
    }
};

double distance(const QPointF& a, const QPointF& b)
{
    return qSqrt((b.x() - a.x()) * (b.x() - a.x()) + (b.y() - a.y()) * (b.y() - a.y()));
}

bool isNonFinite(const QPointF& p) { return !qIsFinite(p.x()) || !qIsFinite(p.y()); }

QPointF perp(const QPointF& dir)
{
    return {-dir.y(), dir.x()};
}

QPointF normalized(const QPointF& p)
{
    double len = qSqrt(p.x() * p.x() + p.y() * p.y());
    if (!qIsFinite(len) || len < 1e-10) return {0, 1};
    return p / len;
}

QPointF miterOffset(const QPointF& n0, const QPointF& n1, float halfWidth)
{
    QPointF tangent = normalized(n0 + n1);
    double dot = tangent.x() * n0.x() + tangent.y() * n0.y();
    if (qAbs(dot) < 1e-6)
        return {};
    double miterLen = halfWidth / dot;
    if (qAbs(miterLen) > MITER_LIMIT * halfWidth)
        return {};
    return tangent * miterLen;
}

struct SegmentData
{
    int startIdx;
    int endIdx;
};

QVector<SegmentData> splitByNonFinite(std::span<const QPointF> points)
{
    QVector<SegmentData> segments;
    int start = 0;
    for (int i = 0; i < static_cast<int>(points.size()); ++i)
    {
        if (isNonFinite(points[i]))
        {
            if (i - start >= 2)
                segments.append({start, i});
            start = i + 1;
        }
    }
    if (static_cast<int>(points.size()) - start >= 2)
        segments.append({start, static_cast<int>(points.size())});
    return segments;
}

//! \a walked carries the arc length in and out: a caller chaining colour runs continues the dash
//! pattern, and the caller resets it between NaN-separated segments.
template <bool WithDistance>
void extrudeSegment(WriteCursor<WithDistance>& cur, std::span<const QPointF> points, int start,
                    int end, float halfWidth, const std::array<float, 4>& rgba, double& walked)
{
    int count = end - start;
    if (count < 2) return;

    // Sliding window: only previous and current normal/left/right needed.
    QPointF prevNormal = perp(normalized(points[start + 1] - points[start]));
    QPointF prevLeft = points[start] + prevNormal * halfWidth;
    QPointF prevRight = points[start] - prevNormal * halfWidth;

    for (int i = 1; i < count - 1; ++i)
    {
        const double prevWalked = walked;
        if constexpr (WithDistance)
            walked += distance(points[start + i - 1], points[start + i]);
        QPointF curNormal = perp(normalized(points[start + i + 1] - points[start + i]));
        QPointF mo = miterOffset(prevNormal, curNormal, halfWidth);

        QPointF curLeft, curRight;
        if (mo.isNull())
        {
            QPointF p = points[start + i];
            QPointF leftPrev = p + prevNormal * halfWidth;
            QPointF rightPrev = p - prevNormal * halfWidth;
            QPointF leftNext = p + curNormal * halfWidth;
            QPointF rightNext = p - curNormal * halfWidth;

            cur.quad(prevLeft, leftPrev, rightPrev, prevRight, rgba, float(prevWalked),
                     float(walked));

            double cross = prevNormal.x() * curNormal.y() - prevNormal.y() * curNormal.x();
            if (cross > 0)
            {
                cur.vertex(rightPrev.x(), rightPrev.y(), rgba, float(walked));
                cur.vertex(rightNext.x(), rightNext.y(), rgba, float(walked));
                cur.vertex(p.x(), p.y(), rgba, float(walked));
            }
            else
            {
                cur.vertex(leftPrev.x(), leftPrev.y(), rgba, float(walked));
                cur.vertex(leftNext.x(), leftNext.y(), rgba, float(walked));
                cur.vertex(p.x(), p.y(), rgba, float(walked));
            }

            curLeft = leftNext;
            curRight = rightNext;
        }
        else
        {
            curLeft = points[start + i] + mo;
            curRight = points[start + i] - mo;
            cur.quad(prevLeft, curLeft, curRight, prevRight, rgba, float(prevWalked),
                     float(walked));
        }

        prevLeft = curLeft;
        prevRight = curRight;
        prevNormal = curNormal;
    }

    const double lastStep = distance(points[start + count - 2], points[start + count - 1]);
    if constexpr (WithDistance)
        walked += lastStep;
    QPointF lastLeft = points[start + count - 1] + prevNormal * halfWidth;
    QPointF lastRight = points[start + count - 1] - prevNormal * halfWidth;
    cur.quad(prevLeft, lastLeft, lastRight, prevRight, rgba, float(walked - lastStep),
             float(walked));
}

// Upper bound on floats written for N input points.
// Each segment of K points produces at most (K-1) quads (6 verts) plus (K-2) bevel triangles
// (3 verts): 9 vertices per point, times 6 floats (72) or 7 with the arc length (84).
int maxExtrusionFloats(int pointCount, bool withDistance)
{
    return pointCount * (withDistance ? 84 : 72);
}

//! Appends the extrusion of \a points, starting the arc length at \a startDistance and returning
//! the arc length at the end of the last segment. Each NaN-separated segment restarts the pattern.
template <bool WithDistance>
float appendPolylineImpl(std::span<const QPointF> points, float penWidth, const QColor& color,
                         std::vector<float>& out, float startDistance)
{
    PROFILE_HERE_N("extrudePolyline");
    if (points.size() < 2 || penWidth <= 0.0f)
        return startDistance;
    PROFILE_PASS_VALUE(points.size());

    auto rgba = qcp::rhi::premultipliedColor(color);
    float halfWidth = penWidth / 2.0f;

    auto segments = splitByNonFinite(points);

    // Pre-allocate worst case on top of whatever's already in out — the vector
    // retains capacity across frames/runs, so repeated appends amortize to zero alloc.
    const size_t base = out.size();
    int maxFloats = maxExtrusionFloats(static_cast<int>(points.size()), WithDistance);
    out.resize(base + maxFloats);

    WriteCursor<WithDistance> cur{out.data() + base};
    double walked = startDistance;
    bool firstSegment = true;
    for (const auto& seg : segments)
    {
        if (!firstSegment)
            walked = 0.0; // a NaN gap restarts the dash pattern
        extrudeSegment<WithDistance>(cur, points, seg.startIdx, seg.endIdx, halfWidth, rgba,
                                     walked);
        firstSegment = false;
    }

    out.resize(base + cur.pos); // trim to actual size (no realloc — smaller than capacity)
    return static_cast<float>(walked);
}

} // anonymous namespace

void appendPolyline(std::span<const QPointF> points, float penWidth,
                     const QColor& color, std::vector<float>& out)
{
    appendPolylineImpl<false>(points, penWidth, color, out, 0.0f);
}

float appendPolylineWithDistance(std::span<const QPointF> points, float penWidth,
                                 const QColor& color, std::vector<float>& out,
                                 float startDistance)
{
    return appendPolylineImpl<true>(points, penWidth, color, out, startDistance);
}

void extrudePolyline(const QVector<QPointF>& points, float penWidth,
                     const QColor& color, std::vector<float>& out)
{
    out.clear();
    appendPolyline(points, penWidth, color, out);
}

QVector<float> extrudePolyline(const QVector<QPointF>& points, float penWidth,
                                const QColor& color)
{
    PROFILE_HERE_N("extrudePolyline");
    if (points.size() < 2 || penWidth <= 0.0f)
        return {};
    PROFILE_PASS_VALUE(points.size());

    auto rgba = qcp::rhi::premultipliedColor(color);
    float halfWidth = penWidth / 2.0f;

    auto segments = splitByNonFinite(points);

    int maxFloats = maxExtrusionFloats(points.size(), false);
    QVector<float> out(maxFloats);

    WriteCursor<false> cur{out.data()};
    double walked = 0.0;
    for (const auto& seg : segments)
        extrudeSegment<false>(cur, points, seg.startIdx, seg.endIdx, halfWidth, rgba, walked);

    out.resize(cur.pos);
    return out;
}

QVector<float> tessellateFillPolygon(const QPolygonF& polygon, const QColor& color)
{
    PROFILE_HERE_N("tessellateFillPolygon");
    if (polygon.size() < 4)
        return {};
    PROFILE_PASS_VALUE(polygon.size());

    auto rgba = qcp::rhi::premultipliedColor(color);
    int curveCount = polygon.size() - 2;

    int maxFloats = (curveCount < 2) ? 18 : ((curveCount - 1) * 36 + 2 * 18);
    QVector<float> out(maxFloats);
    WriteCursor<false> cur{out.data()};

    const QPointF base0 = polygon.first();
    const QPointF base1 = polygon.last();

    if (curveCount < 2)
    {
        cur.vertex(base0.x(), base0.y(), rgba, 0.0f);
        cur.vertex(polygon[1].x(), polygon[1].y(), rgba, 0.0f);
        cur.vertex(base1.x(), base1.y(), rgba, 0.0f);
        out.resize(cur.pos);
        return out;
    }

    bool horizontalBaseline = qFuzzyCompare(base0.y(), base1.y());

    auto projectToBaseline = [&](const QPointF& pt) -> QPointF {
        if (horizontalBaseline)
            return {pt.x(), base0.y()};
        else
            return {base0.x(), pt.y()};
    };

    QPointF proj0 = projectToBaseline(polygon[1]);
    cur.vertex(base0.x(), base0.y(), rgba, 0.0f);
    cur.vertex(polygon[1].x(), polygon[1].y(), rgba, 0.0f);
    cur.vertex(proj0.x(), proj0.y(), rgba, 0.0f);

    for (int i = 1; i < curveCount; ++i)
    {
        QPointF c0 = polygon[i];
        QPointF c1 = polygon[i + 1];
        QPointF p0 = projectToBaseline(c0);
        QPointF p1 = projectToBaseline(c1);

        cur.quad(c0, c1, p1, p0, rgba, 0.0f, 0.0f);
    }

    QPointF projLast = projectToBaseline(polygon[polygon.size() - 2]);
    cur.vertex(polygon[polygon.size() - 2].x(), polygon[polygon.size() - 2].y(), rgba, 0.0f);
    cur.vertex(base1.x(), base1.y(), rgba, 0.0f);
    cur.vertex(projLast.x(), projLast.y(), rgba, 0.0f);

    out.resize(cur.pos);
    return out;
}

} // namespace QCPLineExtruder
