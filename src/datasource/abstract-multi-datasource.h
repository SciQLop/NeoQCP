#pragma once
#include "abstract-datasource.h"
#include "algorithms.h"

class QCPAbstractMultiDataSource {
public:
    virtual ~QCPAbstractMultiDataSource() = default;

    virtual int columnCount() const = 0;
    virtual int size() const = 0;
    virtual bool empty() const { return size() == 0; }

    virtual double keyAt(int i) const = 0;
    virtual QCPRange keyRange(bool& found, QCP::SignDomain sd = QCP::sdBoth) const = 0;
    virtual int findBegin(double sortKey, bool expandedRange = true) const = 0;
    virtual int findEnd(double sortKey, bool expandedRange = true) const = 0;

    virtual double valueAt(int column, int i) const = 0;
    virtual QCPRange valueRange(int column, bool& found,
                                QCP::SignDomain sd = QCP::sdBoth,
                                const QCPRange& inKeyRange = QCPRange()) const = 0;

    virtual QVector<QPointF> getOptimizedLineData(
        int column, int begin, int end, int pixelWidth,
        QCPAxis* keyAxis, QCPAxis* valueAxis) const = 0;

    virtual QVector<QPointF> getLines(
        int column, int begin, int end,
        QCPAxis* keyAxis, QCPAxis* valueAxis) const = 0;

    // Data-space counterpart of getLines: the same points in (key, value) units, with a
    // NaN marker wherever getLines breaks the line. pixelsPerKey <= 0 returns every point;
    // otherwise the line is reduced to the same min/max envelope getOptimizedLineData draws,
    // one bucket per key pixel. A source that is already a resampled level never thins again.
    // sourceIndices, when given, receives each point's source row (-1 for a marker). With
    // \a logKeys the buckets are equal spans of log10(key) and pixelsPerKey counts pixels per
    // decade.
    virtual std::vector<QPointF> linesInData(int column, int begin, int end, double pixelsPerKey,
                                            std::vector<int>* sourceIndices = nullptr,
                                            bool logKeys = false) const;

    virtual void getOptimizedLineDataAll(
        int begin, int end, int pixelWidth,
        QCPAxis* keyAxis, QCPAxis* valueAxis,
        QVector<QPointF>* results, int numColumns) const
    {
        for (int c = 0; c < numColumns; ++c)
            results[c] = getOptimizedLineData(c, begin, end, pixelWidth, keyAxis, valueAxis);
    }

    virtual void getLinesAll(
        int begin, int end,
        QCPAxis* keyAxis, QCPAxis* valueAxis,
        QVector<QPointF>* results, int numColumns) const
    {
        for (int c = 0; c < numColumns; ++c)
            results[c] = getLines(c, begin, end, keyAxis, valueAxis);
    }

    // Same points as getLines / getOptimizedLineData, plus the data index behind each
    // point in sourceIndices (-1 for a NaN or gap marker). Generic and slow here;
    // the built-in sources override them.
    virtual QVector<QPointF> getLinesIndexed(int column, int begin, int end,
                                             QCPAxis* keyAxis, QCPAxis* valueAxis,
                                             QVector<int>& sourceIndices) const;
    virtual QVector<QPointF> getOptimizedLineDataIndexed(int column, int begin, int end,
                                                         int pixelWidth,
                                                         QCPAxis* keyAxis, QCPAxis* valueAxis,
                                                         QVector<int>& sourceIndices) const;

    // Element i is set when the step into row begin + i is a key gap under
    // gapThreshold(), exactly what getLines breaks on. Generic and slow here.
    virtual qcp::algo::GapVector gapsBefore(int begin, int end) const
    {
        std::vector<double> keys(std::max(0, end - begin));
        for (int i = begin; i < end; ++i)
            keys[i - begin] = keyAt(i);
        return qcp::algo::detectKeyGaps(keys, 0, static_cast<int>(keys.size()), gapThreshold());
    }

    virtual const double* rawKeyData() const { return nullptr; }
    virtual QCPRawColumn rawColumn(int /*column*/) const { return {}; }

    // A key step larger than gapThreshold() times the smaller neighbouring step
    // breaks the lines. 0 turns gap detection off.
    void setGapThreshold(double threshold) { mGapThreshold = threshold; }
    [[nodiscard]] double gapThreshold() const { return mGapThreshold; }

private:
    double mGapThreshold = qcp::algo::kDefaultGapThreshold;
};
