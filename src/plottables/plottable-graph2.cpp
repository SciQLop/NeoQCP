#include "plottable-graph2.h"
#include "Profiling.hpp"
#include "../datasource/graph-resampler.h"

#include "../axis/axis.h"
#include "../core.h"
#include "../layoutelements/layoutelement-axisrect.h"
#include "../painting/painter.h"
#include "../vector2d.h"

QCPGraph2::QCPGraph2(QCPAxis* keyAxis, QCPAxis* valueAxis)
    : QCPAbstractPlottable(keyAxis, valueAxis)
    , mPipeline(parentPlot() ? parentPlot()->pipelineScheduler() : nullptr, this)
{
    setPen(QPen(Qt::blue, 0));
    setBrush(Qt::NoBrush);

    if (keyAxis)
    {
        connect(keyAxis, QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
                this, &QCPGraph2::onViewportChanged);
    }
    if (valueAxis)
    {
        connect(valueAxis, QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
                this, &QCPGraph2::onViewportChanged);
    }

    connect(&mPipeline, &QCPGraphPipeline::finished,
            this, [this](uint64_t) {
                if (parentPlot())
                    parentPlot()->replot(QCustomPlot::rpQueuedReplot);
            });
}

QCPGraph2::~QCPGraph2() = default;

static void ensureResamplingTransform(QCPGraphPipeline& pipeline, int sourceSize)
{
    if (sourceSize >= qcp::algo::kResampleThreshold && !pipeline.hasTransform())
    {
        pipeline.setTransform(TransformKind::ViewportDependent,
            [](const QCPAbstractDataSource& src,
               const ViewportParams& vp,
               std::any& cache) -> std::shared_ptr<QCPAbstractDataSource> {
                return qcp::algo::hierarchicalResample(src, vp, cache);
            });
    }
}

void QCPGraph2::setDataSource(std::unique_ptr<QCPAbstractDataSource> source)
{
    mDataSource = std::move(source);
    if (mDataSource)
        ensureResamplingTransform(mPipeline, mDataSource->size());
    mPipeline.setSource(mDataSource);
}

void QCPGraph2::setDataSource(std::shared_ptr<QCPAbstractDataSource> source)
{
    mDataSource = std::move(source);
    if (mDataSource)
        ensureResamplingTransform(mPipeline, mDataSource->size());
    mPipeline.setSource(mDataSource);
}

void QCPGraph2::dataChanged()
{
    if (mPipeline.hasTransform())
        mPipeline.onDataChanged();
    else if (mParentPlot)
        mParentPlot->replot();
}

// --- QCPPlottableInterface1D ---

int QCPGraph2::dataCount() const
{
    return mDataSource ? mDataSource->size() : 0;
}

double QCPGraph2::dataMainKey(int index) const
{
    return mDataSource ? mDataSource->keyAt(index) : 0.0;
}

double QCPGraph2::dataSortKey(int index) const
{
    return mDataSource ? mDataSource->keyAt(index) : 0.0;
}

double QCPGraph2::dataMainValue(int index) const
{
    return mDataSource ? mDataSource->valueAt(index) : 0.0;
}

QCPRange QCPGraph2::dataValueRange(int index) const
{
    if (!mDataSource)
        return QCPRange(0, 0);
    double v = mDataSource->valueAt(index);
    return QCPRange(v, v);
}

QPointF QCPGraph2::dataPixelPosition(int index) const
{
    if (!mDataSource || !mKeyAxis || !mValueAxis)
        return {};
    return coordsToPixels(mDataSource->keyAt(index), mDataSource->valueAt(index));
}

int QCPGraph2::findBegin(double sortKey, bool expandedRange) const
{
    return mDataSource ? mDataSource->findBegin(sortKey, expandedRange) : 0;
}

int QCPGraph2::findEnd(double sortKey, bool expandedRange) const
{
    return mDataSource ? mDataSource->findEnd(sortKey, expandedRange) : 0;
}

QCPDataSelection QCPGraph2::selectTestRect(const QRectF& rect, bool onlySelectable) const
{
    QCPDataSelection result;
    if ((onlySelectable && mSelectable == QCP::stNone) || !mDataSource || mDataSource->empty())
        return result;
    if (!mKeyAxis || !mValueAxis)
        return result;

    double key1, value1, key2, value2;
    pixelsToCoords(rect.topLeft(), key1, value1);
    pixelsToCoords(rect.bottomRight(), key2, value2);
    if (key1 > key2) qSwap(key1, key2);
    if (value1 > value2) qSwap(value1, value2);
    QCPRange keyRange(key1, key2);
    QCPRange valueRange(value1, value2);

    int begin = mDataSource->findBegin(keyRange.lower, false);
    int end = mDataSource->findEnd(keyRange.upper, false);

    int currentSegmentBegin = -1;
    for (int i = begin; i < end; ++i)
    {
        double k = mDataSource->keyAt(i);
        double v = mDataSource->valueAt(i);
        if (currentSegmentBegin == -1)
        {
            if (keyRange.contains(k) && valueRange.contains(v))
                currentSegmentBegin = i;
        }
        else if (!keyRange.contains(k) || !valueRange.contains(v))
        {
            result.addDataRange(QCPDataRange(currentSegmentBegin, i), false);
            currentSegmentBegin = -1;
        }
    }
    if (currentSegmentBegin != -1)
        result.addDataRange(QCPDataRange(currentSegmentBegin, end), false);

    result.simplify();
    return result;
}

// --- QCPAbstractPlottable ---

double QCPGraph2::selectTest(const QPointF& pos, bool onlySelectable, QVariant* details) const
{
    if ((onlySelectable && mSelectable == QCP::stNone) || !mDataSource || mDataSource->empty())
        return -1;
    if (!mKeyAxis || !mValueAxis)
        return -1;

    double posKeyMin, posKeyMax, dummy;
    pixelsToCoords(
        pos - QPointF(mParentPlot->selectionTolerance(), mParentPlot->selectionTolerance()),
        posKeyMin, dummy);
    pixelsToCoords(
        pos + QPointF(mParentPlot->selectionTolerance(), mParentPlot->selectionTolerance()),
        posKeyMax, dummy);
    if (posKeyMin > posKeyMax)
        qSwap(posKeyMin, posKeyMax);

    int begin = mDataSource->findBegin(posKeyMin, true);
    int end = mDataSource->findEnd(posKeyMax, true);
    if (begin == end)
        return -1;

    double minDistSqr = (std::numeric_limits<double>::max)();
    int minDistIndex = -1;
    QCPRange keyRange(mKeyAxis->range());
    QCPRange valueRange(mValueAxis->range());

    for (int i = begin; i < end; ++i)
    {
        double k = mDataSource->keyAt(i);
        double v = mDataSource->valueAt(i);
        if (keyRange.contains(k) && valueRange.contains(v))
        {
            double distSqr = QCPVector2D(coordsToPixels(k, v) - pos).lengthSquared();
            if (distSqr < minDistSqr)
            {
                minDistSqr = distSqr;
                minDistIndex = i;
            }
        }
    }

    QCPDataSelection selectionResult;
    if (minDistIndex >= 0)
        selectionResult.addDataRange(QCPDataRange(minDistIndex, minDistIndex + 1), false);
    selectionResult.simplify();
    if (details)
        details->setValue(selectionResult);
    return minDistIndex >= 0 ? qSqrt(minDistSqr) : -1;
}

QCPRange QCPGraph2::getKeyRange(bool& foundRange, QCP::SignDomain inSignDomain) const
{
    if (!mDataSource || mDataSource->empty())
    {
        foundRange = false;
        return {};
    }
    return mDataSource->keyRange(foundRange, inSignDomain);
}

QCPRange QCPGraph2::getValueRange(bool& foundRange, QCP::SignDomain inSignDomain,
                                   const QCPRange& inKeyRange) const
{
    if (!mDataSource || mDataSource->empty())
    {
        foundRange = false;
        return {};
    }
    return mDataSource->valueRange(foundRange, inSignDomain, inKeyRange);
}

// --- Line style transforms ---
// These operate on pixel-coordinate points returned by the data source.
// For horizontal key axis: x = key pixel, y = value pixel
// For vertical key axis:   x = value pixel, y = key pixel

QVector<QPointF> QCPGraph2::toStepLeftLines(const QVector<QPointF>& lines, bool keyIsVertical)
{
    if (lines.size() < 2)
        return lines;

    QVector<QPointF> result;
    result.resize(lines.size() * 2);

    if (keyIsVertical)
    {
        double lastValue = lines.first().x();
        for (int i = 0; i < lines.size(); ++i)
        {
            const double key = lines[i].y();
            result[i * 2 + 0] = QPointF(lastValue, key);
            lastValue = lines[i].x();
            result[i * 2 + 1] = QPointF(lastValue, key);
        }
    }
    else
    {
        double lastValue = lines.first().y();
        for (int i = 0; i < lines.size(); ++i)
        {
            const double key = lines[i].x();
            result[i * 2 + 0] = QPointF(key, lastValue);
            lastValue = lines[i].y();
            result[i * 2 + 1] = QPointF(key, lastValue);
        }
    }
    return result;
}

QVector<QPointF> QCPGraph2::toStepRightLines(const QVector<QPointF>& lines, bool keyIsVertical)
{
    if (lines.size() < 2)
        return lines;

    QVector<QPointF> result;
    result.resize(lines.size() * 2);

    if (keyIsVertical)
    {
        double lastKey = lines.first().y();
        for (int i = 0; i < lines.size(); ++i)
        {
            const double value = lines[i].x();
            result[i * 2 + 0] = QPointF(value, lastKey);
            lastKey = lines[i].y();
            result[i * 2 + 1] = QPointF(value, lastKey);
        }
    }
    else
    {
        double lastKey = lines.first().x();
        for (int i = 0; i < lines.size(); ++i)
        {
            const double value = lines[i].y();
            result[i * 2 + 0] = QPointF(lastKey, value);
            lastKey = lines[i].x();
            result[i * 2 + 1] = QPointF(lastKey, value);
        }
    }
    return result;
}

QVector<QPointF> QCPGraph2::toStepCenterLines(const QVector<QPointF>& lines, bool keyIsVertical)
{
    if (lines.size() < 2)
        return lines;

    QVector<QPointF> result;
    result.resize(lines.size() * 2);

    if (keyIsVertical)
    {
        double lastKey = lines.first().y();
        double lastValue = lines.first().x();
        result[0] = QPointF(lastValue, lastKey);
        for (int i = 1; i < lines.size(); ++i)
        {
            const double midKey = (lines[i].y() + lastKey) * 0.5;
            result[i * 2 - 1] = QPointF(lastValue, midKey);
            lastValue = lines[i].x();
            lastKey = lines[i].y();
            result[i * 2 + 0] = QPointF(lastValue, midKey);
        }
        result[lines.size() * 2 - 1] = QPointF(lastValue, lastKey);
    }
    else
    {
        double lastKey = lines.first().x();
        double lastValue = lines.first().y();
        result[0] = QPointF(lastKey, lastValue);
        for (int i = 1; i < lines.size(); ++i)
        {
            const double midKey = (lines[i].x() + lastKey) * 0.5;
            result[i * 2 - 1] = QPointF(midKey, lastValue);
            lastValue = lines[i].y();
            lastKey = lines[i].x();
            result[i * 2 + 0] = QPointF(midKey, lastValue);
        }
        result[lines.size() * 2 - 1] = QPointF(lastKey, lastValue);
    }
    return result;
}

QVector<QPointF> QCPGraph2::toImpulseLines(const QVector<QPointF>& lines, bool keyIsVertical) const
{
    QVector<QPointF> result;
    result.resize(lines.size() * 2);

    const double zeroPixel = mValueAxis->coordToPixel(0);

    if (keyIsVertical)
    {
        for (int i = 0; i < lines.size(); ++i)
        {
            const double key = lines[i].y();
            result[i * 2 + 0] = QPointF(zeroPixel, key);
            result[i * 2 + 1] = QPointF(lines[i].x(), key);
        }
    }
    else
    {
        for (int i = 0; i < lines.size(); ++i)
        {
            const double key = lines[i].x();
            result[i * 2 + 0] = QPointF(key, zeroPixel);
            result[i * 2 + 1] = QPointF(key, lines[i].y());
        }
    }
    return result;
}

// --- Drawing ---

void QCPGraph2::draw(QCPPainter* painter)
{
    PROFILE_HERE_N("QCPGraph2::draw");
    if (!mKeyAxis || !mValueAxis || !mDataSource)
        return;

    const QCPAbstractDataSource* ds = mPipeline.hasTransform()
        ? mPipeline.result()
        : mDataSource.get();
    if (!ds || ds->empty())
        return;
    if (mKeyAxis->range().size() <= 0)
        return;
    if (mLineStyle == lsNone && mScatterStyle.isNone())
        return;

    int begin = ds->findBegin(mKeyAxis->range().lower);
    int end = ds->findEnd(mKeyAxis->range().upper);
    if (begin >= end)
        return;

    QVector<QPointF> lines;
    if (mAdaptiveSampling)
    {
        lines = ds->getOptimizedLineData(
            begin, end, static_cast<int>(mKeyAxis->axisRect()->width()),
            mKeyAxis.data(), mValueAxis.data());
    }
    else
    {
        lines = ds->getLines(begin, end, mKeyAxis.data(), mValueAxis.data());
    }

    if (lines.isEmpty())
        return;

    const bool keyIsVertical = mKeyAxis->orientation() == Qt::Vertical;

    // Draw lines
    if (mLineStyle != lsNone && mPen.style() != Qt::NoPen && mPen.color().alpha() != 0)
    {
        applyDefaultAntialiasingHint(painter);
        painter->setPen(mPen);
        painter->setBrush(Qt::NoBrush);

        switch (mLineStyle)
        {
            case lsNone:
                break;
            case lsLine:
                painter->drawPolyline(lines.constData(), lines.size());
                break;
            case lsStepLeft:
            {
                auto stepped = toStepLeftLines(lines, keyIsVertical);
                painter->drawPolyline(stepped.constData(), stepped.size());
                break;
            }
            case lsStepRight:
            {
                auto stepped = toStepRightLines(lines, keyIsVertical);
                painter->drawPolyline(stepped.constData(), stepped.size());
                break;
            }
            case lsStepCenter:
            {
                auto stepped = toStepCenterLines(lines, keyIsVertical);
                painter->drawPolyline(stepped.constData(), stepped.size());
                break;
            }
            case lsImpulse:
            {
                auto impulse = toImpulseLines(lines, keyIsVertical);
                QPen impulsePen = mPen;
                impulsePen.setCapStyle(Qt::FlatCap);
                painter->setPen(impulsePen);
                painter->drawLines(impulse);
                break;
            }
        }
    }

    // Draw scatters
    if (!mScatterStyle.isNone())
    {
        applyScattersAntialiasingHint(painter);
        mScatterStyle.applyTo(painter, mPen);
        const int skip = mScatterSkip + 1;
        for (int i = 0; i < lines.size(); i += skip)
            mScatterStyle.drawShape(painter, lines[i].x(), lines[i].y());
    }
}

void QCPGraph2::drawLegendIcon(QCPPainter* painter, const QRectF& rect) const
{
    applyDefaultAntialiasingHint(painter);

    // Draw line sample
    if (mLineStyle != lsNone)
    {
        painter->setPen(mPen);
        painter->drawLine(QLineF(rect.left(), rect.center().y(),
                                  rect.right() + 5, rect.center().y()));
    }

    // Draw scatter sample
    if (!mScatterStyle.isNone())
    {
        applyScattersAntialiasingHint(painter);
        QCPScatterStyle iconStyle = mScatterStyle;
        iconStyle.setSize(qMin(mScatterStyle.size(), rect.height() * 0.8));
        iconStyle.applyTo(painter, mPen);
        iconStyle.drawShape(painter, rect.center().x(), rect.center().y());
    }
}

void QCPGraph2::onViewportChanged()
{
    if (!mKeyAxis || !mValueAxis) return;
    auto* axisRect = mKeyAxis->axisRect();
    if (!axisRect) return;

    ViewportParams vp;
    vp.keyRange = mKeyAxis->range();
    vp.valueRange = mValueAxis->range();
    vp.plotWidthPx = axisRect->width();
    vp.plotHeightPx = axisRect->height();
    vp.keyLogScale = (mKeyAxis->scaleType() == QCPAxis::stLogarithmic);
    vp.valueLogScale = (mValueAxis->scaleType() == QCPAxis::stLogarithmic);

    mPipeline.onViewportChanged(vp);
}
