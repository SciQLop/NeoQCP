#pragma once

#include <QPen>
#include <QVector>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <span>
#include <utility>
#include <vector>

// Vertex layout: 11 floats (x, y, r, g, b, a, extX, extY, extW, isPixelX, isPixelY)

namespace qcp::rhi::span_grid {

constexpr int kFloatsPerVertex = 11;
// 14 data floats + 2 padding = 16 floats = 64 bytes (std140)
constexpr int kUniformBufferSize = 64;

struct Vertex
{
    float x, y;
    float r, g, b, a;
    float extX, extY, extW;
    float isPixelX, isPixelY;
};
static_assert(sizeof(Vertex) == kFloatsPerVertex * sizeof(float));

inline void append(QVector<float>& buf, std::span<const Vertex> vertices)
{
    const auto offset = buf.size();
    buf.resize(offset + int(vertices.size()) * kFloatsPerVertex);
    std::memcpy(buf.data() + offset, vertices.data(),
                vertices.size() * sizeof(Vertex));
}

inline auto makeVertex(float x, float y,
                       const std::array<float, 4>& rgba,
                       float extX, float extY, float extW,
                       float isPixelX, float isPixelY) -> Vertex
{
    return {x, y, rgba[0], rgba[1], rgba[2], rgba[3],
            extX, extY, extW, isPixelX, isPixelY};
}

//   TL--TR
//   |  / |
//   BL--BR
inline void appendQuad(QVector<float>& buf,
                       float tlX, float tlY, float trX, float trY,
                       float blX, float blY, float brX, float brY,
                       const std::array<float, 4>& rgba,
                       float extX, float extY, float extW,
                       float isPixelX, float isPixelY)
{
    const std::array<Vertex, 6> verts = {{
        {tlX, tlY, rgba[0], rgba[1], rgba[2], rgba[3], extX, extY, extW, isPixelX, isPixelY},
        {blX, blY, rgba[0], rgba[1], rgba[2], rgba[3], extX, extY, extW, isPixelX, isPixelY},
        {trX, trY, rgba[0], rgba[1], rgba[2], rgba[3], extX, extY, extW, isPixelX, isPixelY},
        {trX, trY, rgba[0], rgba[1], rgba[2], rgba[3], extX, extY, extW, isPixelX, isPixelY},
        {blX, blY, rgba[0], rgba[1], rgba[2], rgba[3], extX, extY, extW, isPixelX, isPixelY},
        {brX, brY, rgba[0], rgba[1], rgba[2], rgba[3], extX, extY, extW, isPixelX, isPixelY},
    }};
    append(buf, verts);
}

inline void appendBorder(QVector<float>& buf,
                         float x0, float y0, float x1, float y1,
                         const std::array<float, 4>& rgba,
                         float extDirX, float extDirY, float halfWidth,
                         float isPixelX, float isPixelY)
{
    const std::array<Vertex, 6> verts = {{
        {x0, y0, rgba[0], rgba[1], rgba[2], rgba[3],  extDirX,  extDirY, halfWidth, isPixelX, isPixelY},
        {x0, y0, rgba[0], rgba[1], rgba[2], rgba[3], -extDirX, -extDirY, halfWidth, isPixelX, isPixelY},
        {x1, y1, rgba[0], rgba[1], rgba[2], rgba[3],  extDirX,  extDirY, halfWidth, isPixelX, isPixelY},
        {x1, y1, rgba[0], rgba[1], rgba[2], rgba[3],  extDirX,  extDirY, halfWidth, isPixelX, isPixelY},
        {x0, y0, rgba[0], rgba[1], rgba[2], rgba[3], -extDirX, -extDirY, halfWidth, isPixelX, isPixelY},
        {x1, y1, rgba[0], rgba[1], rgba[2], rgba[3], -extDirX, -extDirY, halfWidth, isPixelX, isPixelY},
    }};
    append(buf, verts);
}

// The visible pieces of a line from `from` to `to` (logical pixels) drawn with `pen`, laid out as
// QPainter does: the dash pattern is in units of the pen width (zero width counts as 1), offset
// by dashOffset(), and square or round caps lengthen each dash by half the width at both ends.
inline std::vector<std::pair<float, float>> dashSegments(float from, float to, const QPen& pen)
{
    const QVector<qreal> pattern
        = pen.style() == Qt::SolidLine ? QVector<qreal> {} : pen.dashPattern();
    const float unit = pen.widthF() > 0 ? float(pen.widthF()) : 1.0f;
    float period = 0;
    for (qreal step : pattern)
        period += float(step) * unit;
    if (pattern.size() < 2 || period <= 0)
        return { { from, to } };
    const float cap = pen.capStyle() == Qt::FlatCap ? 0.0f : unit / 2.0f;
    std::vector<std::pair<float, float>> pieces;
    float pos = from - std::fmod(float(pen.dashOffset()) * unit, period);
    for (int i = 0; pos < to; ++i)
    {
        const float length = float(pattern[i % pattern.size()]) * unit;
        if (i % 2 == 0)
        {
            const float a = std::max(from, pos - cap), b = std::min(to, pos + length + cap);
            if (b > a)
                pieces.emplace_back(a, b);
        }
        pos += length;
    }
    return pieces;
}

// appendBorder() for an axis-aligned line, in the dash pattern of `pen`: the GPU drew grid lines
// and span borders solid whatever the pen style, while exports followed it.
inline void appendDashedBorder(QVector<float>& buf, float x0, float y0, float x1, float y1,
                               const std::array<float, 4>& rgba, float extDirX, float extDirY,
                               float halfWidth, float isPixelX, float isPixelY, const QPen& pen)
{
    const bool alongY = x0 == x1;
    for (const auto& [a, b] : dashSegments(alongY ? y0 : x0, alongY ? y1 : x1, pen))
    {
        if (alongY)
            appendBorder(buf, x0, a, x1, b, rgba, extDirX, extDirY, halfWidth, isPixelX, isPixelY);
        else
            appendBorder(buf, a, y0, b, y1, rgba, extDirX, extDirY, halfWidth, isPixelX, isPixelY);
    }
}

} // namespace qcp::rhi::span_grid
