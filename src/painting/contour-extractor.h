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

QVector<ContourLine> extract(const QCPColorMapData* data,
                             const QVector<double>& levels);

// Fast path: extract contour lines directly as UV [0,1] float pairs,
// subsampling the grid to at most maxDim×maxDim cells.
// Output: flat array of (u1,v1, u2,v2, ...) for GL_LINES.
//
// UV convention: u = keyIndex/(keySize-1), v = 1 - valueIndex/(valueSize-1).
// Subsampling is point sampling (every sk/sv-th sample, no aggregation):
// an explicit accuracy/speed tradeoff, and the last coarse cell is clamped
// to the final row/column so the tail strip stays covered.
// Saddle cells (cases 5/10) use the cell-center-average heuristic, not the
// bilinear asymptotic decider -- deterministic, documented approximation.
QVector<float> extractUv(const QCPColorMapData* data,
                         const QVector<double>& levels,
                         int maxDim = 300);

} // namespace QCPContourExtractor
