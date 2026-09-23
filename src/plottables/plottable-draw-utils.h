#pragma once

#include "plottable-color-runs.h"
#include <QPointF>
#include <QPen>
#include <QRect>
#include <QVector>
#include <vector>

class QCustomPlot;
class QCPPainter;
class QCPLayer;

namespace qcp {

/// Cached extruded vertices for a single polyline.
/// Stores the untranslated GPU vertices so they can be reused across frames
/// with only a cheap translation applied. Uses std::vector to retain capacity
/// across frames (no COW overhead, no detach on non-const data()).
struct ExtrusionCache {
    std::vector<float> vertices;   // untranslated extruded verts (6 floats per vertex)
    float penWidth = 0;
    QRgb penColor = 0;
    quint64 colorGeneration = 0;   // coloured lines: the mapper generation the vertices were built with

    void clear() { vertices.clear(); }
    [[nodiscard]] bool isEmpty() const { return vertices.empty(); }
};

/// Draw a polyline using the GPU path if available, otherwise QPainter.
/// The GPU path is disabled during export (pmVectorized, pmNoCaching).
/// When gpuOffset is non-null, points are pre-translated so other plottables
/// on the same shared layer are unaffected.
/// @param clipRect the plottable's clip rect (passed explicitly to avoid protected access)
void drawPolylineWithGpuFallback(QCPPainter* painter,
                                  QCustomPlot* parentPlot,
                                  QCPLayer* layer,
                                  const QVector<QPointF>& pts,
                                  const QPen& pen,
                                  const QPointF& gpuOffset,
                                  const QRect& clipRect);

/// Same as above but with cached extrusion.
/// When @a freshLines is true, re-extrudes and stores in @a cache.
/// When false, translates cached vertices by gpuOffset.
void drawPolylineCached(QCPPainter* painter,
                         QCustomPlot* parentPlot,
                         QCPLayer* layer,
                         const QVector<QPointF>& pts,
                         const QPen& pen,
                         const QPointF& gpuOffset,
                         const QRect& clipRect,
                         bool freshLines,
                         ExtrusionCache& cache);

/// Extrudes each run in its own colour into one vertex buffer (6 floats per vertex).
void extrudeColorRuns(const QVector<QPointF>& points, const std::vector<ColorRun>& runs,
                      float penWidth, const ColorScalarMapper& mapper, std::vector<float>& out);

/// A coloured cache survives pans; it is rebuilt on fresh lines, pen width or colour change.
bool needsColoredReextrusion(const ExtrusionCache& cache, bool freshLines,
                             float penWidth, quint64 colorGeneration);

/// Coloured counterpart of drawPolylineCached. Builds the runs itself, only when it
/// re-extrudes (or has no GPU layer), so any reason to re-extrude gets correct runs.
void drawColoredPolylineCached(QCPPainter* painter, QCustomPlot* parentPlot, QCPLayer* layer,
                               const QVector<QPointF>& points, const QVector<int>& indices,
                               const ColorScalarMapper& mapper, const QPen& pen,
                               const QPointF& gpuOffset, const QRect& clipRect,
                               bool freshLines, ExtrusionCache& cache);

/// QPainter path for coloured lines: one polyline per run.
void drawColoredPolylineRuns(QCPPainter* painter, const QVector<QPointF>& points,
                             const std::vector<ColorRun>& runs, const ColorScalarMapper& mapper,
                             const QPen& pen, const QPointF& gpuOffset);

} // namespace qcp
