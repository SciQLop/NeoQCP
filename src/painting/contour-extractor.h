#pragma once
#include <QLineF>
#include <QVector>

class QCPColorMapData;

namespace QCPContourExtractor
{

struct ContourLine
{
    double level;
    QVector<QLineF> segments;
};

//! The contour lines of each level, in UV [0,1] space (see below), subsampling the grid to at
//! most maxDim×maxDim cells. Levels without a crossing are left out.
//!
//! UV convention: u = keyIndex/(keySize-1), v = 1 - valueIndex/(valueSize-1), i.e. the image's
//! own space: it maps to pixels through the rectangle the image is drawn into, which is right
//! for any axis scale (a log axis has log-spaced rows, stretched uniformly like the image).
//! Subsampling is point sampling (every sk/sv-th sample, no aggregation):
//! an explicit accuracy/speed tradeoff, and the last coarse cell is clamped
//! to the final row/column so the tail strip stays covered.
//! Saddle cells (cases 5/10) use the cell-center-average heuristic, not the
//! bilinear asymptotic decider -- deterministic, documented approximation.
QVector<ContourLine> extract(const QCPColorMapData* data, const QVector<double>& levels,
                             int maxDim = 300);

//! Flat (u1,v1, u2,v2, ...) pairs for GL_LINES.
QVector<float> flatten(const QVector<ContourLine>& lines);

QVector<float> extractUv(const QCPColorMapData* data,
                         const QVector<double>& levels,
                         int maxDim = 300);

} // namespace QCPContourExtractor
