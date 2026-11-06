#pragma once

#include "plottable-color-runs.h"
#include <QBrush>
#include <QColor>
#include <QPen>
#include <QPointF>
#include <QRect>
#include <QVector>
#include <cstddef>
#include <span>
#include <vector>

class QCustomPlot;
class QCPPainter;
class QCPLayer;
class QCPPlottableRhiLayer;

namespace qcp
{

/// The colour of the index-th component or column when none is set (cycles after 10).
QColor defaultPlottableColor(int index);

/// The GPU layer a plottable on @a layer draws into, or nullptr: no QRhi, or exporting.
QCPPlottableRhiLayer* gpuLayer(QCustomPlot* parentPlot, QCPLayer* layer, bool exporting);

/// True when the GPU can stroke \a pen: a visible style whose brush is one solid colour. A
/// non-solid style takes the dashed pipeline, so the pen's style never changes where the line
/// stacks; a brush the GPU cannot fill (a gradient, a texture) cannot be a vertex colour, so that
/// stroke goes through the painter instead of vanishing.
bool canStrokeOnGpu(const QPen& pen);

/// True when \a brush's colour alone can stand in for it: a solid pattern with a visible colour.
/// Anything else (a texture, a gradient, a hatch) has to become an image.
bool gpuCanFill(const QBrush& brush);

/// The dash pattern of \a pen in pixels: QPen's on/off lengths multiplied by the pen width (a
/// cosmetic pen uses 1). \a count is 0 for a solid pen; patterns longer than eight entries are cut.
struct DashPattern
{
    std::array<float, 8> lengths = { };
    int count = 0;
    float offset = 0; //!< QPen::dashOffset in pixels
    float total = 0; //!< the length of one period, pixels
};

DashPattern dashPatternOf(const QPen& pen, float penWidth);

/// One definition of "this pen is dashed": the pattern the GPU can draw has at least one entry.
/// A CustomDashLine with an empty or all-zero pattern is solid (QPainter agrees).
bool isDashed(const QPen& pen);

/// Extrudes \a pts in the format \a pen needs: 6 floats, or 7 with the arc length when dashed.
/// The one place that chooses the extruder for a stroke.
void extrudeStroke(std::span<const QPointF> pts, const QPen& pen, float penWidth,
                   std::vector<float>& out, bool closed = false);

/// Strokes \a pts on the GPU, solid or dashed. False means the caller must paint them itself
/// (no QRhi, exporting, no layer, or nothing to stroke).
bool strokePolylineOnGpu(QCPPainter* painter, QCustomPlot* parentPlot, QCPLayer* layer,
                         std::span<const QPointF> pts, const QPen& pen, const QPointF& gpuOffset,
                         const QRect& clipRect);


/// QPainter::drawPolyline does not break at NaN: one polyline per NaN-separated run.
void drawPolylineSplitNaN(QCPPainter* painter, const QVector<QPointF>& pts);

/// Same, for points the caller already owns as a span (no QVector copy).
void drawPolylineSplitNaN(QCPPainter* painter, std::span<const QPointF> pts);

/// Cached extruded vertices for a single polyline.
/// Stores the untranslated GPU vertices so they can be reused across frames
/// with only a cheap translation applied. Uses std::vector to retain capacity
/// across frames (no COW overhead, no detach on non-const data()).
struct ExtrusionCache
{
    std::vector<float> vertices; // untranslated extruded verts (6 floats per vertex)
    float penWidth = 0;
    QRgb penColor = 0;
    quint64 colorGeneration
        = 0; // coloured lines: the mapper generation the vertices were built with
    bool withDistance = false; // 7 floats per vertex (dashed) instead of 6

    void clear() { vertices.clear(); }

    [[nodiscard]] bool isEmpty() const { return vertices.empty(); }
};

/// Adds already-extruded stroke vertices to \a layer: the dashed pipeline when \a pen is not
/// solid, the solid one otherwise. Shared by the plottable paths and the command replay.
/// False when a dashed pen has no dashed pipeline (it failed to build): the caller paints it.
bool addGpuStroke(QCPPlottableRhiLayer& layer, std::span<const float> verts, const QPen& pen,
                  float penWidth, const QRect& clipRect, double dpr, int outputHeight,
                  const QPointF& offset = QPointF(), float alpha = 1);

/// Draw a polyline using the GPU path if available, otherwise QPainter.
/// The GPU path is disabled during export (pmVectorized, pmNoCaching).
/// When gpuOffset is non-null, points are pre-translated so other plottables
/// on the same shared layer are unaffected.
/// @param clipRect the plottable's clip rect (passed explicitly to avoid protected access)
void drawPolylineWithGpuFallback(QCPPainter* painter, QCustomPlot* parentPlot, QCPLayer* layer,
                                 const QVector<QPointF>& pts, const QPen& pen,
                                 const QPointF& gpuOffset, const QRect& clipRect);

/// GPU stroke width for @a pen: cosmetic or zero-width pens are one device pixel.
float extrusionPenWidth(const QPen& pen, double devicePixelRatio);

/// A plain cache survives pans; it is rebuilt on fresh lines or any pen change
/// (e.g. selection). Callers that pass pre-transformed points (steps) must pass them
/// whenever this is true, since the cache is then re-extruded from those points.
bool needsReextrusion(const ExtrusionCache& cache, bool freshLines, const QPen& pen,
                      double devicePixelRatio);

/// The vertex storage the uncached stroke's thread-local scratch hands back above, in floats: a
/// megabyte, some 43k vertices of the 6 a solid extrusion writes per segment.
inline constexpr std::size_t STROKE_SCRATCH_LIMIT = std::size_t(1) << 18;

/// Gives @a cache's vertex storage back when it grew past STROKE_SCRATCH_LIMIT and the stroke it
/// holds used less than a quarter of it, so that a stroke extruded once cannot pin its size on the
/// thread for the life of the process (see strokePolylineOnGpu). A stroke that uses what it grew to
/// keeps it, so a pan of the same data still allocates nothing. The cache is left empty: the
/// uncached path rebuilds it on every stroke anyway.
void trimExtrusionScratch(ExtrusionCache& cache);

/// Same as above but with cached extrusion.
/// When @a freshLines is true, re-extrudes and stores in @a cache.
/// When false, translates cached vertices by gpuOffset.
void drawPolylineCached(QCPPainter* painter, QCustomPlot* parentPlot, QCPLayer* layer,
                        const QVector<QPointF>& pts, const QPen& pen, const QPointF& gpuOffset,
                        const QRect& clipRect, bool freshLines, ExtrusionCache& cache);

/// A coloured cache survives pans; it is rebuilt on fresh lines, pen width or colour change.
bool needsColoredReextrusion(const ExtrusionCache& cache, bool freshLines, float penWidth,
                             quint64 colorGeneration, bool dashed);

/// Coloured counterpart of drawPolylineCached. Builds the runs itself, only when it
/// re-extrudes (or has no GPU layer), so any reason to re-extrude gets correct runs.
void drawColoredPolylineCached(QCPPainter* painter, QCustomPlot* parentPlot, QCPLayer* layer,
                               const QVector<QPointF>& points, const QVector<int>& indices,
                               const ColorScalarMapper& mapper, const QPen& pen,
                               const QPointF& gpuOffset, const QRect& clipRect, bool freshLines,
                               ExtrusionCache& cache);

/// Extrudes \a points into \a out one coloured run at a time, the runs being \a spans.
/// Dashed strokes carry one dash phase across the whole polyline: the runs share their end points,
/// so restarting the pattern at every colour change would stripe the line.
void extrudeColoredStroke(std::span<const QPointF> points, std::span<const ColorSpan> spans,
                          const QPen& pen, float penWidth, std::vector<float>& out);

/// QPainter path for coloured lines: one polyline per run, butt-ended so a run never overpaints the
/// next one's half pen width.
void drawColoredPolylineSpans(QCPPainter* painter, std::span<const QPointF> points,
                              std::span<const ColorSpan> spans, const QPen& pen);

/// QPainter path for coloured lines: one polyline per run.
void drawColoredPolylineRuns(QCPPainter* painter, const QVector<QPointF>& points,
                             const std::vector<ColorRun>& runs, const ColorScalarMapper& mapper,
                             const QPen& pen, const QPointF& gpuOffset);

} // namespace qcp
