#include "abstract-datasource.h"
#include "algorithms.h"

#include <vector>

QVector<QPointF> QCPAbstractDataSource::getLinesIndexed(int begin, int end, QCPAxis* keyAxis,
                                                        QCPAxis* valueAxis,
                                                        QVector<int>& sourceIndices) const
{
    sourceIndices.clear();
    if (end <= begin)
        return {};
    std::vector<double> keys, values;
    keys.reserve(end - begin);
    values.reserve(end - begin);
    for (int i = begin; i < end; ++i)
    {
        keys.push_back(keyAt(i));
        values.push_back(valueAt(i));
    }
    auto pts = qcp::algo::linesToPixelsIndexed(keys, values, 0, end - begin, keyAxis, valueAxis,
                                               sourceIndices, gapThreshold());
    for (int& i : sourceIndices)
        if (i >= 0)
            i += begin;
    return pts;
}
