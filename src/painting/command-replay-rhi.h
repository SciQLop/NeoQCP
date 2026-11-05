#pragma once

#include "coords.h"
#include "draw-commands.h"

#include <QColor>
#include <QPointF>
#include <span>
#include <vector>

class QCPPlottableRhiLayer;
class QCPScatterRhiLayer;

/*!
  Resolved pixels of recorded commands, kept across frames.

  A command is identified by \\a key (the address of the data it draws) and is mapped again only
  when \\a generation or the transform changed. This task caches the resolution alone; the recorded
  geometry is still extruded per frame.
*/
class QCPResolveCache
{
public:
    //! The pixels of \\a data for this \\a key; \\a data is mapped only when the entry is stale.
    [[nodiscard]] const std::vector<QPointF>& pixels(const void* key, quint64 generation,
                                                     const QCPTransform& transform,
                                                     std::span<const QPointF> data);
    void clear();

private:
    struct Entry
    {
        const void* key = nullptr;
        quint64 generation = 0;
        QCPTransform transform;
        bool valid = false;
        std::vector<QPointF> pixels;
    };
    std::vector<Entry> mEntries;
};

/*!
  Draws the GPU kinds of \a list on \a plottables and \a scatter.

  Polylines in data or mixed spaces, fills, rects, ellipses, error bars and markers become triangles
  or instanced sprites, dashed or solid depending on the pen. Text, painter paths, images and
  free-form callbacks are skipped: the QPainter replay (QCPReplayKinds::PainterOnly) draws those.
*/
void qcpReplayWithRhi(const QCPDrawList& list, QCPPlottableRhiLayer& plottables,
                      QCPScatterRhiLayer& scatter, const QCPDrawFrame& frame,
                      QCPResolveCache& cache, double dpr, int outputHeight,
                      quint64 generation = 0);

//! Triangles of a fan fill from \a centre over an open \a ring: six floats per vertex.
std::vector<float> qcpFanFillVertices(const QPointF& centre, std::span<const QPointF> ring,
                                      const QColor& color);

//! The pixel polyline of a recording's error bars: the bar and its two caps, NaN gaps between bars.
void qcpErrorBarPixels(const QCPDrawFrame& frame, std::span<const QPointF> centres,
                       std::span<const QPointF> spans, std::vector<QPointF>& out);
