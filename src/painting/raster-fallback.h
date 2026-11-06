#pragma once

// Private to the library: a command whose fill or stroke the GPU cannot express is painted into an
// image, so the RHI replay can draw it at the command's place in the layer instead of the painter
// pass.

#include "draw-commands.h"

#include <QImage>
#include <QPair>
#include <QRectF>
#include <QSet>
#include <algorithm>
#include <deque>

class QCPPainter;

namespace qcp
{
//! Drops the entries the previous replay did not touch -- \a expired also drops those whose key is
//! gone -- and marks the rest as not yet touched by this one.
template <class Entry, class Expired>
void sweepUnused(std::deque<Entry>& entries, Expired&& expired)
{
    std::erase_if(entries, [&](const Entry& entry) { return !entry.used || expired(entry); });
    for (Entry& entry : entries)
        entry.used = false;
}

//! True when \a pen's brush is not a solid colour: the GPU strokes it in one colour, so the stroke
//! has to be part of the command's image.
bool strokeNeedsRaster(const QPen& pen);

//! What \a command needs from the raster: a fill that cannot become a GPU vertex colour (a texture,
//! gradient or hatch brush, a free-form polygon, a series pair that is not a ribbon), a stroke the
//! GPU cannot draw in one colour, and whether that stroke is a dash pattern only a layer with the
//! dashed pipeline can draw. One dispatch answers all three: every caller that asks one asks more.
struct RasterNeeds
{
    bool fill = false;
    bool stroke = false;
    bool dashed = false;

    //! True when either half needs the raster.
    [[nodiscard]] bool any() const { return fill || stroke; }
};

//! One visit over \a command; the kinds without a pen answer stroke and dashed false.
[[nodiscard]] RasterNeeds rasterNeedsOf(const QCPDrawCommand& command);

//! True when \a command is an image (or the painter's) rather than GPU triangles.
inline bool needsRaster(const QCPDrawCommand& command)
{
    return rasterNeedsOf(command).any();
}

//! The pixel bounds the raster has to cover; \a withStroke adds the outline's outer half and its
//! antialiasing.
QRectF fallbackPixelBounds(const QCPDrawCommand& command, const QCPDrawFrame& frame, double dpr,
                           bool withStroke);

//! Paints \a command into \a painter: the fill, plus the outline when \a withStroke. Pixels, no
//! scaling.
void paintFallback(QCPPainter& painter, const QCPDrawCommand& command, const QCPDrawFrame& frame,
                   bool withStroke);

//! Rasterises \a command over \a bounds (logical pixels) at \a dpr: premultiplied ARGB,
//! antialiased.
QImage rasterizeFallback(const QCPDrawCommand& command, const QRectF& bounds,
                         const QCPDrawFrame& frame, double dpr, bool withStroke);

//! How a cached image follows the frame when the transform only translates: a command in data space
//! moves by the translation's delta, a command in pixel space stays where it is, and anything else
//! (a ratio, an anchor) only matches the transform it was rasterised with.
enum class PanBehaviour
{
    Exact,
    Moves,
    Stays
};

//! The images of one replay, kept between replays: a recording that is replayed again (a pan that
//! only moves the layer's offsets) re-uses them, so nothing is rasterised or uploaded twice.
class QCPRasterFallback
{
public:
    //! Starts a replay: forgets which commands the painter took, drops the images it no longer
    //! uses. Images it kept are re-used.
    void beginReplay();

    /*!
      The raster of command \a index of recording \a list, over \a bounds, at \a dpr. The key is the
      command's place in its recording, the generation and the clipped bounds; \a behaviour decides
      how the frame's transform has to match: a zoom repaints, a pan hits and \a delta then holds
      the pixels the image has to be drawn shifted by.
    */
    const QImage& image(quint64 list, quint64 index, const QCPDrawCommand& command,
                        const QCPDrawFrame& frame, const QRectF& bounds, double dpr,
                        quint64 generation, PanBehaviour behaviour, QPointF* delta = nullptr);

    //! The layer is full: the painter pass draws command \a index of \a list instead.
    void painterHandles(quint64 list, quint64 index);
    bool painterHandled(quint64 list, quint64 index) const;

    //! This recording needs more images than the layer has left: the painter pass draws all of it,
    //! so one object never splits between stacking levels.
    void paintWholeRecordingWithPainter(quint64 list);
    bool painterDrawsAll(quint64 list) const;

    int rasterCount() const { return int(mEntries.size()); }

private:
    struct Entry
    {
        quint64 list = 0; //!< QCPDrawList::id of the recording
        quint64 index = 0; //!< the command's place in it
        quint64 generation = 0;
        QCPTransform transform; //!< the frame's mapping the image was painted with
        QRectF bounds; //!< the clipped pixel rect it covers
        qreal dpr = 1.0;
        PanBehaviour behaviour = PanBehaviour::Exact;
        bool used = false; //!< touched by the replay running now; untouched ones are swept
        QImage image;
    };

    std::deque<Entry> mEntries;
    QSet<QPair<quint64, quint64>> mPainterHandled;
    QSet<quint64> mPainterLists;
};
} // namespace qcp
