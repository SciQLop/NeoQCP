#include "abstract-multi-datasource.h"
#include "algorithms.h"
#include <vector>

namespace {

// Copies [begin, end) of one column so the templated algorithms can run on any source.
struct ColumnWindow {
    std::vector<double> keys;
    std::vector<double> values;
    ColumnWindow(const QCPAbstractMultiDataSource& src, int column, int begin, int end)
    {
        keys.reserve(end - begin);
        values.reserve(end - begin);
        for (int i = begin; i < end; ++i)
        {
            keys.push_back(src.keyAt(i));
            values.push_back(src.valueAt(column, i));
        }
    }
};

inline void shiftIndices(QVector<int>& indices, int begin)
{
    for (int& i : indices)
        if (i >= 0) i += begin;
}

inline void shiftIndices(std::vector<int>& indices, int begin)
{
    for (int& i : indices)
        if (i >= 0) i += begin;
}

} // namespace

QVector<QPointF> QCPAbstractMultiDataSource::getLinesIndexed(
    int column, int begin, int end, QCPAxis* keyAxis, QCPAxis* valueAxis,
    QVector<int>& sourceIndices) const
{
    if (end <= begin) { sourceIndices.clear(); return {}; }
    ColumnWindow w(*this, column, begin, end);
    auto pts = qcp::algo::linesToPixelsIndexed(w.keys, w.values, 0, end - begin,
                                               keyAxis, valueAxis, sourceIndices, gapThreshold());
    shiftIndices(sourceIndices, begin);
    return pts;
}

QVector<QPointF> QCPAbstractMultiDataSource::getOptimizedLineDataIndexed(
    int column, int begin, int end, int pixelWidth, QCPAxis* keyAxis, QCPAxis* valueAxis,
    QVector<int>& sourceIndices) const
{
    if (end <= begin) { sourceIndices.clear(); return {}; }
    ColumnWindow w(*this, column, begin, end);
    const auto gaps = qcp::algo::detectKeyGaps(w.keys, 0, end - begin, gapThreshold());
    auto pts = qcp::algo::optimizedLineDataIndexed(w.keys, w.values, 0, end - begin, pixelWidth,
                                                   keyAxis, valueAxis, sourceIndices, &gaps);
    shiftIndices(sourceIndices, begin);
    return pts;
}

std::vector<QPointF> QCPAbstractMultiDataSource::linesInData(
    int column, int begin, int end, double pixelsPerKey, std::vector<int>* sourceIndices,
    bool logKeys) const
{
    if (end <= begin) { if (sourceIndices) sourceIndices->clear(); return {}; }
    ColumnWindow w(*this, column, begin, end);
    const auto gaps = qcp::algo::detectKeyGaps(w.keys, 0, end - begin, gapThreshold());
    auto pts = qcp::algo::linesInData(w.keys, w.values, 0, end - begin, pixelsPerKey,
                                      sourceIndices, gapThreshold(), &gaps, logKeys);
    if (sourceIndices) shiftIndices(*sourceIndices, begin);
    return pts;
}
