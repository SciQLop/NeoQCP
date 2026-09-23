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

} // namespace

QVector<QPointF> QCPAbstractMultiDataSource::getLinesIndexed(
    int column, int begin, int end, QCPAxis* keyAxis, QCPAxis* valueAxis,
    QVector<int>& sourceIndices) const
{
    ColumnWindow w(*this, column, begin, end);
    auto pts = qcp::algo::linesToPixelsIndexed(w.keys, w.values, 0, end - begin,
                                               keyAxis, valueAxis, sourceIndices);
    shiftIndices(sourceIndices, begin);
    return pts;
}

QVector<QPointF> QCPAbstractMultiDataSource::getOptimizedLineDataIndexed(
    int column, int begin, int end, int pixelWidth, QCPAxis* keyAxis, QCPAxis* valueAxis,
    QVector<int>& sourceIndices) const
{
    ColumnWindow w(*this, column, begin, end);
    auto pts = qcp::algo::optimizedLineDataIndexed(w.keys, w.values, 0, end - begin, pixelWidth,
                                                   keyAxis, valueAxis, sourceIndices);
    shiftIndices(sourceIndices, begin);
    return pts;
}
