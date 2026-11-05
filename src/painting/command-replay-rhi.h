#pragma once

#include "coords.h"
#include "draw-commands.h"

#include <QColor>
#include <QPointF>
#include <QRectF>
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
  Keeps a recording's whole built geometry, so a pan re-emits it instead of building it again.

  One per recording. A replay that starts on the list this cache was built for, at the same device
  pixel ratio, clip and output size, and with a transform the kept one only translates, hands every
  run back to the layer with one delta: no extrusion, no tessellation, no mapping. The runs are
  replayed in the recording's order, so a build frame and a pan frame stack identically. Anything
  else (a zoom, a re-record, a new device pixel ratio) rebuilds everything and keeps the result. A
  command the GPU draws as an image is kept as a run of its own: the image comes from the raster
  cache, which shifts it by the same delta, so it never invalidates the triangles beside it. A
  recording whose geometry moves in more than one way (a ratio, an anchor, a rect with one data and
  one pixel corner) is rebuilt, because a kept run can only be shifted by one delta or not at all.
*/
class QCPRecordingCache
{
public:
    //! What a kept submission is: a solid run, a dashed one, or a rasterised command.
    enum class RunKind
    {
        Solid,
        Dashed,
        Raster
    };

    /*!
      One kept submission, in the recording's order.

      A pan frame re-submits the runs in this order, so a style never moves up or down the layer
      between the frame that built it and one that only shifts it. Neighbouring runs of the same
      kind and state merge (see the keep* calls); a raster run never merges, it is one image.
    */
    struct Run
    {
        RunKind kind = RunKind::Solid;
        int first = 0; //!< first vertex in \ref solidVertices() or \ref dashedVertices()
        int vertexCount = 0;
        int fillVertexCount = 0; //!< solid: the fill prefix; the rest is stroke (and later fills)
        bool movesWithData
            = false; //!< false: a pixel, ratio or anchor position must not be shifted
        float dashOffset = 0; //!< dashed: pixels
        float pattern[8] = { }; //!< dashed: the dash pattern
        int patternCount = 0;
        int index = 0; //!< raster: the command's place in the recording
        QRectF bounds; //!< raster: the clipped pixel rect the image covers
    };

    /*!
      True when the kept geometry of \a list may be re-emitted: same list, same device pixel ratio,
      output height and clip, and \a transform a translation of the one it was built for. \a delta
      then holds the pixels to add to the runs that move with the data.
    */
    bool reuse(const QCPDrawList& list, const QCPTransform& transform, double dpr, int outputHeight,
               const QRect& clip, QPointF* delta);

    //! Drops the runs and starts keeping a new build for \a list.
    void beginBuild(const QCPDrawList& list, const QCPTransform& transform, double dpr,
                    int outputHeight, const QRect& clip);

    /*!
      Keeps \a vertices as the run a submission built; \a fillVertexCount of them are its fill.
      Consecutive runs that move alike are kept as one, so a re-emit is a handful of submissions
      whatever the recording's command count.
    */
    void keepSolid(std::span<const float> vertices, int fillVertexCount, bool movesWithData);

    //! Keeps a dashed stroke, with the \a pattern (pixels) and \a dashOffset it is drawn with.
    void keepDashed(std::span<const float> vertices, std::span<const float> pattern,
                    float dashOffset, bool movesWithData);

    //! Keeps command \a index, rasterised over \a bounds, as a run of its own.
    void keepRaster(int index, const QRectF& bounds, bool movesWithData);

    //! Commits the build, unless a rasterised command spoiled it.
    void commit();

    //! The recording holds something this cache cannot reproduce: it must be rebuilt.
    void invalidate();

    [[nodiscard]] std::span<const float> solidVertices() const { return mSolid; }

    [[nodiscard]] std::span<const float> dashedVertices() const { return mDashed; }

    //! The kept submissions, in the recording's order: a reuse frame replays exactly this.
    [[nodiscard]] std::span<const Run> runs() const { return mRuns; }

    void clear();

private:
    std::vector<float> mSolid; //!< six floats per vertex, in record order
    std::vector<float> mDashed; //!< seven floats per vertex (the seventh is the arc length)
    std::vector<Run> mRuns;
    QCPTransform mTransform;
    QRect mClip;
    quint64 mList = 0;
    double mDpr = 0;
    int mHeight = 0;
    bool mReusable = false; //!< every kept command moves alike: see beginBuild()
    bool mValid = false;
    bool mSpoiled = false;
};

/*!
  Draws the GPU kinds of \a list on \a plottables and \a scatter.

  Polylines in data or mixed spaces, fills, rects, ellipses, error bars and markers become triangles
  or instanced sprites, dashed or solid depending on the pen, at \a alpha. A fill or stroke the GPU
  cannot express becomes an image through \a fallback. Text, painter paths, images and free-form
  callbacks are skipped: the QPainter replay (QCPReplayKinds::PainterOnly) draws those.

  With \a geometry, the replay keeps what it built for this recording and re-emits it on a frame
  that only translates it, instead of building it again.
*/
void qcpReplayWithRhi(const QCPDrawList& list, QCPPlottableRhiLayer& plottables,
                      QCPScatterRhiLayer& scatter, const QCPDrawFrame& frame,
                      QCPResolveCache& cache, qcp::QCPRasterFallback& fallback, double dpr,
                      const QSize& outputSize, quint64 generation = 0,
                      QCPRecordingCache* geometry = nullptr, float alpha = 1.0f);

//! Test hook: how many fill vertex sets the replay has built (a pan must not add one). Counts the
//! fan and ribbon fills together, like QCPLineExtruder::extruderCallCount() counts strokes.
quint64 qcpFillBuildCount();

//! Triangles of a fan fill from \a centre over an open \a ring: six floats per vertex.
std::vector<float> qcpFanFillVertices(const QPointF& centre, std::span<const QPointF> ring,
                                      const QColor& color);
