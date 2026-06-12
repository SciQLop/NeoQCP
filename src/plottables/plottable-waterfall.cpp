#include "plottable-waterfall.h"
#include "datasource/algorithms.h"

// --- QCPWaterfallDataAdapter ---

QCPWaterfallDataAdapter::QCPWaterfallDataAdapter(
    std::shared_ptr<QCPAbstractMultiDataSource> source,
    QVector<double> offsets, QVector<double> normFactors, double gain)
    : mSource(std::move(source))
    , mOffsets(std::move(offsets))
    , mNormFactors(std::move(normFactors))
    , mGain(gain)
{
}

int QCPWaterfallDataAdapter::columnCount() const { return mSource ? mSource->columnCount() : 0; }
int QCPWaterfallDataAdapter::size() const { return mSource ? mSource->size() : 0; }
double QCPWaterfallDataAdapter::keyAt(int i) const { return mSource ? mSource->keyAt(i) : 0.0; }
QCPRange QCPWaterfallDataAdapter::keyRange(bool& found, QCP::SignDomain sd) const
{
    if (!mSource) { found = false; return QCPRange(); }
    return mSource->keyRange(found, sd);
}
int QCPWaterfallDataAdapter::findBegin(double sortKey, bool expandedRange) const { return mSource ? mSource->findBegin(sortKey, expandedRange) : 0; }
int QCPWaterfallDataAdapter::findEnd(double sortKey, bool expandedRange) const { return mSource ? mSource->findEnd(sortKey, expandedRange) : 0; }

double QCPWaterfallDataAdapter::transform(int column, double rawValue) const
{
    double offset = (column < mOffsets.size()) ? mOffsets[column] : 0.0;
    double norm = (column < mNormFactors.size()) ? mNormFactors[column] : 1.0;
    return offset + rawValue * norm * mGain;
}

double QCPWaterfallDataAdapter::valueAt(int column, int i) const
{
    if (!mSource) return 0.0;
    return transform(column, mSource->valueAt(column, i));
}

QCPRange QCPWaterfallDataAdapter::valueRange(int column, bool& found, QCP::SignDomain sd,
                                              const QCPRange& inKeyRange) const
{
    if (!mSource) { found = false; return QCPRange(); }
    QCPRange raw = mSource->valueRange(column, found, QCP::sdBoth, inKeyRange);
    if (!found) return QCPRange();
    double a = transform(column, raw.lower);
    double b = transform(column, raw.upper);
    QCPRange result(qMin(a, b), qMax(a, b));
    if (sd == QCP::sdPositive) result.lower = qMax(result.lower, 0.0);
    if (sd == QCP::sdNegative) result.upper = qMin(result.upper, 0.0);
    found = (result.lower < result.upper);
    return result;
}

QVector<QPointF> QCPWaterfallDataAdapter::getLines(int column, int begin, int end,
                                                     QCPAxis* keyAxis, QCPAxis* valueAxis) const
{
    if (!mSource || begin >= end) return {};
    std::vector<double> keys(end - begin);
    std::vector<double> vals(end - begin);
    for (int i = begin; i < end; ++i) {
        keys[i - begin] = mSource->keyAt(i);
        vals[i - begin] = transform(column, mSource->valueAt(column, i));
    }
    return qcp::algo::linesToPixels(keys, vals, 0, end - begin, keyAxis, valueAxis);
}

QVector<QPointF> QCPWaterfallDataAdapter::getOptimizedLineData(int column, int begin, int end,
                                                                 int pixelWidth,
                                                                 QCPAxis* keyAxis,
                                                                 QCPAxis* valueAxis) const
{
    if (!mSource || begin >= end) return {};
    std::vector<double> keys(end - begin);
    std::vector<double> vals(end - begin);
    for (int i = begin; i < end; ++i) {
        keys[i - begin] = mSource->keyAt(i);
        vals[i - begin] = transform(column, mSource->valueAt(column, i));
    }
    return qcp::algo::optimizedLineData(keys, vals, 0, end - begin, pixelWidth,
                                         keyAxis, valueAxis);
}

// --- QCPWaterfallGraph ---

QCPWaterfallGraph::QCPWaterfallGraph(QCPAxis* keyAxis, QCPAxis* valueAxis)
    : QCPMultiGraph(keyAxis, valueAxis)
{
}

void QCPWaterfallGraph::setDataSource(std::shared_ptr<QCPAbstractMultiDataSource> source)
{
    mOriginalSource = std::move(source);
    mNormDirty = true;
    rebuildAdapter();
}

void QCPWaterfallGraph::setOffsetMode(OffsetMode mode) { mOffsetMode = mode; rebuildAdapter(); }
void QCPWaterfallGraph::setUniformSpacing(double spacing) { mUniformSpacing = spacing; rebuildAdapter(); }
void QCPWaterfallGraph::setOffsets(const QVector<double>& offsets) { mUserOffsets = offsets; rebuildAdapter(); }
void QCPWaterfallGraph::setNormalize(bool enabled) { mNormalize = enabled; mNormDirty = true; rebuildAdapter(); }
void QCPWaterfallGraph::setGain(double gain) { mGain = gain; rebuildAdapter(); }
void QCPWaterfallGraph::invalidateNormalization() { mNormDirty = true; rebuildAdapter(); }

double QCPWaterfallGraph::effectiveOffset(int component) const
{
    if (mOffsetMode == omCustom && component < mUserOffsets.size())
        return mUserOffsets[component];
    return component * mUniformSpacing;
}

void QCPWaterfallGraph::recomputeNormFactors()
{
    if (!mOriginalSource) {
        mCachedNormFactors.clear();
        return;
    }
    int cols = mOriginalSource->columnCount();
    int n = mOriginalSource->size();
    mCachedNormFactors.resize(cols);
    for (int c = 0; c < cols; ++c) {
        if (!mNormalize) {
            mCachedNormFactors[c] = 1.0;
            continue;
        }
        double maxAbs = 0.0;
        for (int i = 0; i < n; ++i)
            maxAbs = qMax(maxAbs, qAbs(mOriginalSource->valueAt(c, i)));
        mCachedNormFactors[c] = (maxAbs > 0.0) ? (1.0 / maxAbs) : 1.0;
    }
    mNormDirty = false;
}

void QCPWaterfallGraph::rebuildAdapter()
{
    if (!mOriginalSource)
    {
        // Clearing the source must uninstall the previous adapter too, or the
        // graph keeps rendering the old data (base caches stay populated).
        mAdapter.reset();
        QCPMultiGraph::setDataSource(std::shared_ptr<QCPAbstractMultiDataSource>{});
        return;
    }
    if (mNormDirty)
        recomputeNormFactors();

    int cols = mOriginalSource->columnCount();
    QVector<double> offsets(cols);
    for (int c = 0; c < cols; ++c)
        offsets[c] = effectiveOffset(c);

    mAdapter = std::make_shared<QCPWaterfallDataAdapter>(
        mOriginalSource, std::move(offsets), mCachedNormFactors, mGain);
    QCPMultiGraph::setDataSource(mAdapter);
}

void QCPWaterfallGraph::dataChanged()
{
    // In-place mutation of the inner source: norm factors are stale, snapshot
    // a fresh adapter (which also invalidates line/L1/L2 caches) before the
    // base handles the change.
    mNormDirty = true;
    rebuildAdapter();
    QCPMultiGraph::dataChanged();
}

QCPRange QCPWaterfallGraph::getValueRange(bool& foundRange,
                                           QCP::SignDomain inSignDomain,
                                           const QCPRange& inKeyRange) const
{
    foundRange = false;
    if (!mOriginalSource || mOriginalSource->columnCount() == 0)
        return QCPRange();

    if (inKeyRange != QCPRange()) {
        QCPRange merged;
        bool first = true;
        for (int c = 0; c < mOriginalSource->columnCount(); ++c) {
            bool f = false;
            QCPRange r = mAdapter->valueRange(c, f, inSignDomain, inKeyRange);
            if (f) {
                if (first) { merged = r; first = false; }
                else merged.expand(r);
            }
        }
        foundRange = !first;
        return merged;
    }

    int cols = mOriginalSource->columnCount();
    double lo = effectiveOffset(0), hi = lo;
    for (int c = 1; c < cols; ++c) {
        double o = effectiveOffset(c);
        lo = qMin(lo, o);
        hi = qMax(hi, o);
    }

    double margin;
    if (mNormalize) {
        margin = mGain;
    } else {
        double maxAmp = 0.0;
        for (int c = 0; c < cols; ++c) {
            bool f = false;
            QCPRange r = mOriginalSource->valueRange(c, f, QCP::sdBoth);
            if (f) maxAmp = qMax(maxAmp, qMax(qAbs(r.lower), qAbs(r.upper)));
        }
        margin = maxAmp * mGain;
    }

    lo -= margin;
    hi += margin;

    if (inSignDomain == QCP::sdPositive) lo = qMax(lo, 0.0);
    if (inSignDomain == QCP::sdNegative) hi = qMin(hi, 0.0);
    foundRange = (lo < hi);
    return QCPRange(lo, hi);
}
