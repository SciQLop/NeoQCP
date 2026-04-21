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
// All vertex writes go through this — no per-float append() calls.
struct WriteCursor
{
    float* data;
    int pos = 0;

    void vertex(float x, float y, const std::array<float, 4>& rgba)
    {
        float* p = data + pos;
        p[0] = x;  p[1] = y;
        p[2] = rgba[0];  p[3] = rgba[1];  p[4] = rgba[2];  p[5] = rgba[3];
        pos += 6;
    }

    void quad(QPointF tl, QPointF tr, QPointF br, QPointF bl,
              const std::array<float, 4>& rgba)
    {
        vertex(tl.x(), tl.y(), rgba);
        vertex(tr.x(), tr.y(), rgba);
        vertex(br.x(), br.y(), rgba);
        vertex(tl.x(), tl.y(), rgba);
        vertex(br.x(), br.y(), rgba);
        vertex(bl.x(), bl.y(), rgba);
    }
};

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

QVector<SegmentData> splitByNonFinite(const QVector<QPointF>& points)
{
    QVector<SegmentData> segments;
    int start = 0;
    for (int i = 0; i < points.size(); ++i)
    {
        if (isNonFinite(points[i]))
        {
            if (i - start >= 2)
                segments.append({start, i});
            start = i + 1;
        }
    }
    if (points.size() - start >= 2)
        segments.append({start, int(points.size())});
    return segments;
}

void extrudeSegment(WriteCursor& cur, const QVector<QPointF>& points,
                    int start, int end, float halfWidth, const std::array<float, 4>& rgba)
{
    int count = end - start;
    if (count < 2) return;

    // Sliding window: only previous and current normal/left/right needed.
    QPointF prevNormal = perp(normalized(points[start + 1] - points[start]));
    QPointF prevLeft = points[start] + prevNormal * halfWidth;
    QPointF prevRight = points[start] - prevNormal * halfWidth;

    for (int i = 1; i < count - 1; ++i)
    {
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

            cur.quad(prevLeft, leftPrev, rightPrev, prevRight, rgba);

            double cross = prevNormal.x() * curNormal.y() - prevNormal.y() * curNormal.x();
            if (cross > 0)
            {
                cur.vertex(rightPrev.x(), rightPrev.y(), rgba);
                cur.vertex(rightNext.x(), rightNext.y(), rgba);
                cur.vertex(p.x(), p.y(), rgba);
            }
            else
            {
                cur.vertex(leftPrev.x(), leftPrev.y(), rgba);
                cur.vertex(leftNext.x(), leftNext.y(), rgba);
                cur.vertex(p.x(), p.y(), rgba);
            }

            curLeft = leftNext;
            curRight = rightNext;
        }
        else
        {
            curLeft = points[start + i] + mo;
            curRight = points[start + i] - mo;
            cur.quad(prevLeft, curLeft, curRight, prevRight, rgba);
        }

        prevLeft = curLeft;
        prevRight = curRight;
        prevNormal = curNormal;
    }

    QPointF lastLeft = points[start + count - 1] + prevNormal * halfWidth;
    QPointF lastRight = points[start + count - 1] - prevNormal * halfWidth;
    cur.quad(prevLeft, lastLeft, lastRight, prevRight, rgba);
}

// Upper bound on floats written by extrudePolyline for N input points.
// Each segment of K points produces at most (K-1) quads (6 verts × 6 floats = 36)
// plus (K-2) bevel triangles (3 verts × 6 floats = 18).
// Worst case per point: 36 + 18 = 54 floats. We round up to 6*6*2 = 72.
int maxExtrusionFloats(int pointCount)
{
    return pointCount * 72;
}

} // anonymous namespace

void extrudePolyline(const QVector<QPointF>& points, float penWidth,
                     const QColor& color, std::vector<float>& out)
{
    PROFILE_HERE_N("extrudePolyline");
    out.clear();
    if (points.size() < 2 || penWidth <= 0.0f)
        return;
    PROFILE_PASS_VALUE(points.size());

    auto rgba = qcp::rhi::premultipliedColor(color);
    float halfWidth = penWidth / 2.0f;

    auto segments = splitByNonFinite(points);

    // Pre-allocate worst case — the vector retains capacity across frames
    int maxFloats = maxExtrusionFloats(points.size());
    out.resize(maxFloats);

    WriteCursor cur{out.data()};
    for (const auto& seg : segments)
        extrudeSegment(cur, points, seg.startIdx, seg.endIdx, halfWidth, rgba);

    out.resize(cur.pos); // trim to actual size (no realloc — smaller than capacity)
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

    int maxFloats = maxExtrusionFloats(points.size());
    QVector<float> out(maxFloats);

    WriteCursor cur{out.data()};
    for (const auto& seg : segments)
        extrudeSegment(cur, points, seg.startIdx, seg.endIdx, halfWidth, rgba);

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
    WriteCursor cur{out.data()};

    const QPointF base0 = polygon.first();
    const QPointF base1 = polygon.last();

    if (curveCount < 2)
    {
        cur.vertex(base0.x(), base0.y(), rgba);
        cur.vertex(polygon[1].x(), polygon[1].y(), rgba);
        cur.vertex(base1.x(), base1.y(), rgba);
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
    cur.vertex(base0.x(), base0.y(), rgba);
    cur.vertex(polygon[1].x(), polygon[1].y(), rgba);
    cur.vertex(proj0.x(), proj0.y(), rgba);

    for (int i = 1; i < curveCount; ++i)
    {
        QPointF c0 = polygon[i];
        QPointF c1 = polygon[i + 1];
        QPointF p0 = projectToBaseline(c0);
        QPointF p1 = projectToBaseline(c1);

        cur.quad(c0, c1, p1, p0, rgba);
    }

    QPointF projLast = projectToBaseline(polygon[polygon.size() - 2]);
    cur.vertex(polygon[polygon.size() - 2].x(), polygon[polygon.size() - 2].y(), rgba);
    cur.vertex(base1.x(), base1.y(), rgba);
    cur.vertex(projLast.x(), projLast.y(), rgba);

    out.resize(cur.pos);
    return out;
}

} // namespace QCPLineExtruder
