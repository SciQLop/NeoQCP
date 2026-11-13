#pragma once
#include "abstract-datasource.h"
#include "algorithms.h"
#include <atomic>

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

    // The line of one column in (key, value) units, with a NaN marker wherever a key gap
    // (gapsBefore) or a NaN hole breaks it. pixelsPerKey <= 0 returns every point; otherwise the
    // line is reduced to a min/max envelope, one bucket per key pixel. A source that is already a resampled level never thins again.
    // sourceIndices, when given, receives each point's source row (-1 for a marker). With
    // \a logKeys the buckets are equal spans of log10(key) and pixelsPerKey counts pixels per
    // decade.
    virtual std::vector<QPointF> linesInData(int column, int begin, int end, double pixelsPerKey,
                                            std::vector<int>* sourceIndices = nullptr,
                                            bool logKeys = false, bool cacheGaps = true) const;

    // Element i is set when the step into row begin + i is a key gap under
    // gapThreshold(), exactly what linesInData breaks on. Generic and slow here. cacheGaps = false
    // computes the gaps for this call only: what a reader a worker may run beside uses.
    virtual qcp::algo::GapVector gapsBefore(int begin, int end, bool cacheGaps = true) const
    {
        (void)cacheGaps;
        std::vector<double> keys(std::max(0, end - begin));
        for (int i = begin; i < end; ++i)
            keys[i - begin] = keyAt(i);
        return qcp::algo::detectKeyGaps(keys, 0, static_cast<int>(keys.size()), gapThreshold());
    }

    virtual const double* rawKeyData() const { return nullptr; }
    virtual QCPRawColumn rawColumn(int /*column*/) const { return {}; }

    // A key step larger than gapThreshold() times the smaller neighbouring step
    // breaks the lines. 0 turns gap detection off. The draw sets it on the installed source and
    // its readers may run on the pipeline thread: the field is atomic.
    void setGapThreshold(double threshold) { mGapThreshold.store(threshold, std::memory_order_relaxed); }
    [[nodiscard]] double gapThreshold() const
    {
        return mGapThreshold.load(std::memory_order_relaxed);
    }

private:
    std::atomic<double> mGapThreshold { qcp::algo::kDefaultGapThreshold };
};
