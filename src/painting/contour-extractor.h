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
QVector<float> extractUv(const QCPColorMapData* data,
                         const QVector<double>& levels,
                         int maxDim = 300);

} // namespace QCPContourExtractor
