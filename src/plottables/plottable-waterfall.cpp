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

QCPWaterfallDataAdapter::TransformedWindow
QCPWaterfallDataAdapter::transformedWindow(int column, int begin, int end) const
{
    TransformedWindow window;
    window.keys.resize(end - begin);
    window.values.resize(end - begin);
    for (int i = begin; i < end; ++i) {
        window.keys[i - begin] = mSource->keyAt(i);
        window.values[i - begin] = transform(column, mSource->valueAt(column, i));
    }
    return window;
}

QVector<QPointF> QCPWaterfallDataAdapter::getLines(int column, int begin, int end,
                                                     QCPAxis* keyAxis, QCPAxis* valueAxis) const
{
    if (!mSource || begin >= end) return {};
    const auto w = transformedWindow(column, begin, end);
    return qcp::algo::linesToPixels(w.keys, w.values, 0, end - begin, keyAxis, valueAxis,
                                    gapThreshold());
}

QVector<QPointF> QCPWaterfallDataAdapter::getOptimizedLineData(int column, int begin, int end,
                                                                 int pixelWidth,
                                                                 QCPAxis* keyAxis,
                                                                 QCPAxis* valueAxis) const
{
    if (!mSource || begin >= end) return {};
    const auto w = transformedWindow(column, begin, end);
    const auto gaps = qcp::algo::detectKeyGaps(w.keys, 0, end - begin, gapThreshold());
    return qcp::algo::optimizedLineData(w.keys, w.values, 0, end - begin, pixelWidth,
                                         keyAxis, valueAxis, &gaps);
}

std::vector<QPointF> QCPWaterfallDataAdapter::linesInData(int column, int begin, int end,
                                                           double pixelsPerKey,
                                                           std::vector<int>* sourceIndices) const
{
    if (!mSource || begin >= end) {
        if (sourceIndices) sourceIndices->clear();
        return {};
    }
    const auto w = transformedWindow(column, begin, end);
    const auto gaps = qcp::algo::detectKeyGaps(w.keys, 0, end - begin, gapThreshold());
    auto pts = qcp::algo::linesInData(w.keys, w.values, 0, end - begin, pixelsPerKey,
                                      sourceIndices, gapThreshold(), &gaps);
    if (sourceIndices)
        for (int& i : *sourceIndices)
            if (i >= 0) i += begin;
    return pts;
}

// --- QCPWaterfallGraph ---

QCPWaterfallGraph::QCPWaterfallGraph(QCPAxis* keyAxis, QCPAxis* valueAxis)
    : QCPMultiGraph(keyAxis, valueAxis)
{
}

void QCPWaterfallGraph::setDataSource(std::shared_ptr<QCPAbstractMultiDataSource> source)
{
    markDirty();
    mOriginalSource = std::move(source);
    mNormDirty = true;
    rebuildAdapter();
}

void QCPWaterfallGraph::setOffsetMode(OffsetMode mode)
{
    if (mode == mOffsetMode)
        return;
    markDirty();
    mOffsetMode = mode;
    rebuildAdapter();
}
void QCPWaterfallGraph::setUniformSpacing(double spacing)
{
    if (spacing == mUniformSpacing)
        return;
    markDirty();
    mUniformSpacing = spacing;
    rebuildAdapter();
}
void QCPWaterfallGraph::setOffsets(const QVector<double>& offsets)
{
    if (offsets == mUserOffsets)
        return;
    markDirty();
    mUserOffsets = offsets;
    rebuildAdapter();
}
void QCPWaterfallGraph::setNormalize(bool enabled)
{
    if (enabled == mNormalize)
        return;
    markDirty();
    mNormalize = enabled;
    mNormDirty = true;
    rebuildAdapter();
}
void QCPWaterfallGraph::setGain(double gain)
{
    if (gain == mGain)
        return;
    markDirty();
    mGain = gain;
    rebuildAdapter();
}
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
    // a fresh adapter (which also invalidates line/L1/L2 caches). rebuildAdapter()
    // already routes through QCPMultiGraph::setDataSource(), which does the full
    // staged/immediate invalidation. Do NOT also call QCPMultiGraph::dataChanged()
    // here: once geometry is rendered, that sees the fresh adapter we just staged
    // in mPendingSource and — treating it as "a mutation superseded the pending
    // replacement" — re-stages the OLD displayed adapter over it, discarding the
    // rebuild (stale normalization on the eventual commit).
    mNormDirty = true;
    rebuildAdapter();
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
