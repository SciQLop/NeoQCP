#pragma once

#include "coords.h"
#include "draw-commands.h"

#include <QColor>
#include <QPointF>
#include <QSize>
#include <deque>
#include <memory>
#include <span>
#include <vector>

class QCPPlottableRhiLayer;
class QCPScatterRhiLayer;
namespace qcp
{
class QCPRasterFallback;
}

/*!
  Resolved pixels of recorded commands, kept across frames.

  A command is identified by \\a key (the address of the data it draws) and is mapped again only
  when \\a generation or the transform changed. This task caches the resolution alone; the recorded
  geometry is still extruded per frame.
*/
class QCPResolveCache
{
public:
    //! Marks the entries unused and sweeps those the previous replay did not touch.
    void beginReplay();

    /*!
      The pixels of \a recording through \a transform, remapped only when the generation or the
      transform changed. Keyed on the recording itself (a weak reference): a freed recording can
      never be matched by a new vector that happens to live at the same address.
    */
    [[nodiscard]] std::span<const QPointF>
    pixels(const std::shared_ptr<const std::vector<QPointF>>& recording, quint64 generation,
           const QCPTransform& transform);

    void clear();

    //! Test hook: how many recordings the cache holds.
    [[nodiscard]] int entryCount() const { return int(mEntries.size()); }

private:
    struct Entry
    {
        std::weak_ptr<const std::vector<QPointF>> recording;
        quint64 generation = 0;
        QCPTransform transform;
        std::vector<QPointF> pixels;
        bool used = false;
    };

    // A deque keeps every entry (and the span handed out from it) at a stable address.
    std::deque<Entry> mEntries;
};

/*!
  Draws the GPU kinds of \a list on \a plottables and \a scatter.

  Polylines in data or mixed spaces, fills, rects, ellipses, error bars and markers become triangles
  or instanced sprites, dashed or solid depending on the pen. A fill or stroke the GPU cannot
  express becomes an image through \a fallback. Text, painter paths, images and free-form callbacks
  are skipped: the QPainter replay (QCPReplayKinds::PainterOnly) draws those.
*/
void qcpReplayWithRhi(const QCPDrawList& list, QCPPlottableRhiLayer& plottables,
                      QCPScatterRhiLayer& scatter, const QCPDrawFrame& frame,
                      QCPResolveCache& cache, qcp::QCPRasterFallback& fallback, double dpr,
                      const QSize& outputSize, quint64 generation = 0);

//! Triangles of a fan fill from \a centre over an open \a ring: six floats per vertex.
std::vector<float> qcpFanFillVertices(const QPointF& centre, std::span<const QPointF> ring,
                                      const QColor& color);

//! The pixel polyline of a recording's error bars: the bar and its two caps, NaN gaps between bars.
void qcpErrorBarPixels(const QCPDrawFrame& frame, std::span<const QPointF> centres,
                       std::span<const QPointF> spans, std::vector<QPointF>& out);
