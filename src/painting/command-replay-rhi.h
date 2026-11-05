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

    //! Test hook: how many times a recording was mapped again (the first map counts).
    [[nodiscard]] quint64 remapCount() const { return mRemaps; }

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
    quint64 mRemaps = 0;
};

/*!
  Tells a replay which extruded vertices it may stroke again instead of extruding them.

  One per recording. A command's triangles are kept with the transform they were built for: when
  the next frame's transform differs by a translation only (a pan, not a zoom), the kept vertices go
  to the layer again with the delta as the entry offset, which is today's fast path for GPU lines.
  A re-recorded list, or a new recording, starts from nothing.
*/
class QCPExtrusionCache
{
public:
    //! Starts a replay of recording \a list; the previous replay's unused keys are swept.
    void beginReplay(quint64 list);

    /*!
      The triangles of command \a key when \a transform is a translation of the one they were built
      for, \a delta then holding the pixels to add. An empty span means the caller must extrude.
    */
    [[nodiscard]] std::span<const float> translated(quint64 key, const QCPTransform& transform,
                                                    QPointF* delta);

    //! Keeps \a vertices as command \a key's geometry for \a transform.
    void keep(quint64 key, const QCPTransform& transform, std::span<const float> vertices);

    //! Test hook: how many commands have kept geometry.
    [[nodiscard]] int entryCount() const { return int(mEntries.size()); }

    void clear();

private:
    struct Entry
    {
        quint64 key = 0;
        QCPTransform transform;
        std::vector<float> vertices;
        bool used = false;
    };

    std::deque<Entry> mEntries;
    quint64 mList = 0;
    bool mStarted = false;
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
                      const QSize& outputSize, quint64 generation = 0,
                      QCPExtrusionCache* extrusion = nullptr);

//! Triangles of a fan fill from \a centre over an open \a ring: six floats per vertex.
std::vector<float> qcpFanFillVertices(const QPointF& centre, std::span<const QPointF> ring,
                                      const QColor& color);

//! The pixel polyline of a recording's error bars: the bar and its two caps, NaN gaps between bars.
void qcpErrorBarPixels(const QCPDrawFrame& frame, std::span<const QPointF> centres,
                       std::span<const QPointF> spans, std::vector<QPointF>& out);
