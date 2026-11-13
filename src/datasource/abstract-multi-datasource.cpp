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

std::vector<QPointF> QCPAbstractMultiDataSource::linesInData(
    int column, int begin, int end, double pixelsPerKey, std::vector<int>* sourceIndices,
    bool logKeys, bool /*cacheGaps*/) const
{
    if (end <= begin) { if (sourceIndices) sourceIndices->clear(); return {}; }
    ColumnWindow w(*this, column, begin, end);
    const auto gaps = qcp::algo::detectKeyGaps(w.keys, 0, end - begin, gapThreshold());
    auto pts = qcp::algo::linesInData(w.keys, w.values, 0, end - begin, pixelsPerKey,
                                      sourceIndices, gapThreshold(), &gaps, logKeys);
    if (sourceIndices) shiftIndices(*sourceIndices, begin);
    return pts;
}
