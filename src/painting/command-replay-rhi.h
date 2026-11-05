#pragma once

#include "coords.h"
#include "draw-commands.h"

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

//! True when every command of \\a list has a GPU path (the replay is all or nothing).
[[nodiscard]] bool qcpCanReplayOnRhi(const QCPDrawList& list);

/*!
  Draws \\a list on \\a plottables and \\a scatter.

  Returns false, having drawn nothing, when a command has no GPU path: text, a painter path, an
  image, a free-form callback, or a filled free-form polygon. Everything else - polylines in data or
  mixed spaces, fills, rects, ellipses, error bars and markers - becomes triangles or instanced
  sprites, dashed or solid depending on the pen.
*/
bool qcpReplayWithRhi(const QCPDrawList& list, QCPPlottableRhiLayer& plottables,
                      QCPScatterRhiLayer& scatter, const QCPDrawFrame& frame,
                      QCPResolveCache& cache, double dpr, int outputHeight,
                      quint64 generation = 0);
