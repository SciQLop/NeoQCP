#include "plottable-colormap2.h"
#include "plottable-colormap.h" // for QCPColorMapData
#include <core.h>
#include <painting/painter.h>
#include <painting/contour-extractor.h> // kept for QPainter export fallback
#include <layoutelements/layoutelement-colorscale.h>
#include <layoutelements/layoutelement-axisrect.h>
#include <axis/axis.h>
#include <layer.h>
#include <datasource/resample.h>
#include <Profiling.hpp>

QCPColorMap2::QCPColorMap2(QCPAxis* keyAxis, QCPAxis* valueAxis)
    : QCPAbstractPlottable(keyAxis, valueAxis)
    , mPipeline(parentPlot() ? parentPlot()->pipelineScheduler() : nullptr, this)
    , mRenderer(this)
{
    mPipeline.setTransform(TransformKind::ViewportDependent,
        [&gapThreshold = mGapThreshold](
            const QCPAbstractDataSource2D& src,
            const ViewportParams& vp,
            std::any& cache) -> std::shared_ptr<QCPColorMapData> {
            if (src.xSize() < 2) return nullptr;

            bool found = false;
            auto xRange = src.xRange(found);
            if (!found) return nullptr;
            auto yRange = src.yRange(found);
            if (!found) return nullptr;

            int xBegin = src.findXBegin(vp.keyRange.lower);
            int xEnd = src.findXEnd(vp.keyRange.upper);
            if (xEnd <= xBegin) return nullptr;

            // Clamp output grid to intersection of viewport and data extent.
            // Without this, zooming out creates a grid spanning the full viewport
            // with bins wider than source spacing, leaving most bins empty (black).
            QCPRange xOut(std::max(vp.keyRange.lower, xRange.lower),
                          std::min(vp.keyRange.upper, xRange.upper));
            QCPRange yOut(std::max(vp.valueRange.lower, yRange.lower),
                          std::min(vp.valueRange.upper, yRange.upper));
            if (xOut.lower >= xOut.upper || yOut.lower >= yOut.upper)
                return nullptr;

            auto logFrac = [](const QCPRange& data, const QCPRange& vp) {
                if (data.lower <= 0 || vp.lower <= 0)
                {
                    double vpSz = vp.size();
                    return vpSz > 0 ? data.size() / vpSz : 1.0;
                }
                double denom = std::log10(vp.upper) - std::log10(vp.lower);
                return denom > 0 ? (std::log10(data.upper) - std::log10(data.lower)) / denom : 1.0;
            };
            double vpKeySz = vp.keyRange.size();
            double xFrac = vpKeySz > 0 ? xOut.size() / vpKeySz : 1.0;
            double vpValSz = vp.valueRange.size();
            double yFrac = vp.valueLogScale ? logFrac(yOut, vp.valueRange)
                                            : (vpValSz > 0 ? yOut.size() / vpValSz : 1.0);
            int pixW = std::clamp(static_cast<int>(vp.plotWidthPx * xFrac), 1, 32768);
            int pixH = std::clamp(static_cast<int>(vp.plotHeightPx * yFrac), 1, 32768);

            int visibleSrcCols = xEnd - xBegin;
            int w = std::clamp(visibleSrcCols, pixW, pixW * 4);
            int h = std::clamp(src.ySize(), pixH, pixH * 4);
            if (w <= 0 || h <= 0) return nullptr;

            if (!cache.has_value())
                cache = qcp::algo2d::ResampleCache{};
            auto& rc = std::any_cast<qcp::algo2d::ResampleCache&>(cache);
            auto* raw = qcp::algo2d::resample(src, xBegin, xEnd,
                xOut, yOut, w, h, vp.valueLogScale, gapThreshold.load(std::memory_order_relaxed), &rc);
            return std::shared_ptr<QCPColorMapData>(raw);
        });

    if (keyAxis)
    {
        connect(keyAxis, QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
                this, &QCPColorMap2::onViewportChanged);
    }
    if (valueAxis)
    {
        connect(valueAxis, QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),
                this, &QCPColorMap2::onViewportChanged);
        connect(valueAxis, &QCPAxis::scaleTypeChanged,
                this, [this](QCPAxis::ScaleType) { onViewportChanged(); });
    }

    connect(&mPipeline, &QCPColormapPipeline::finished,
            this, [this](uint64_t) {
                ++mContourDataGen;
                mRenderer.invalidateMapImage();
                if (parentPlot())
                    parentPlot()->replot(QCustomPlot::rpQueuedReplot);
            });
    connect(&mPipeline, &QCPColormapPipeline::busyChanged,
            this, [this](bool) { updateEffectiveBusy(); });
}

QCPColorMap2::~QCPColorMap2()
{
    mRenderer.releaseRhiLayer();
}

void QCPColorMap2::setDataSource(std::unique_ptr<QCPAbstractDataSource2D> source)
{
    setDataSource(std::shared_ptr<QCPAbstractDataSource2D>(std::move(source)));
}

void QCPColorMap2::setDataSource(std::shared_ptr<QCPAbstractDataSource2D> source)
{
    mDataSource = std::move(source);
    mPipeline.setSource(mDataSource);
}

void QCPColorMap2::dataChanged()
{
    mPipeline.onDataChanged();
}

void QCPColorMap2::setGradient(const QCPColorGradient& gradient)
{
    if (mRenderer.gradient() != gradient)
    {
        mRenderer.setGradient(gradient);
        Q_EMIT gradientChanged(mRenderer.gradient());
        if (mLayer)
            mLayer->markDirty();
        if (mParentPlot)
            mParentPlot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void QCPColorMap2::setColorScale(QCPColorScale* colorScale)
{
    if (mRenderer.colorScale())
    {
        disconnect(this, &QCPColorMap2::dataRangeChanged, mRenderer.colorScale(), &QCPColorScale::setDataRange);
        disconnect(this, &QCPColorMap2::gradientChanged, mRenderer.colorScale(), &QCPColorScale::setGradient);
        disconnect(this, &QCPColorMap2::dataScaleTypeChanged, mRenderer.colorScale(), &QCPColorScale::setDataScaleType);
        disconnect(mRenderer.colorScale(), &QCPColorScale::dataRangeChanged, this, &QCPColorMap2::setDataRange);
        disconnect(mRenderer.colorScale(), &QCPColorScale::gradientChanged, this, &QCPColorMap2::setGradient);
        disconnect(mRenderer.colorScale(), &QCPColorScale::dataScaleTypeChanged, this, &QCPColorMap2::setDataScaleType);
    }
    mRenderer.setColorScale(colorScale);
    if (colorScale)
    {
        setGradient(colorScale->gradient());
        setDataScaleType(colorScale->dataScaleType());
        setDataRange(colorScale->dataRange());
        connect(this, &QCPColorMap2::dataRangeChanged, colorScale, &QCPColorScale::setDataRange);
        connect(this, &QCPColorMap2::gradientChanged, colorScale, &QCPColorScale::setGradient);
        connect(this, &QCPColorMap2::dataScaleTypeChanged, colorScale, &QCPColorScale::setDataScaleType);
        connect(colorScale, &QCPColorScale::dataRangeChanged, this, &QCPColorMap2::setDataRange);
        connect(colorScale, &QCPColorScale::gradientChanged, this, &QCPColorMap2::setGradient);
        connect(colorScale, &QCPColorScale::dataScaleTypeChanged, this, &QCPColorMap2::setDataScaleType);
    }
}

void QCPColorMap2::setDataRange(const QCPRange& range)
{
    QCPRange prev = mRenderer.dataRange();
    mRenderer.setDataRange(range);
    QCPRange cur = mRenderer.dataRange();
    if (prev.lower != cur.lower || prev.upper != cur.upper)
    {
        Q_EMIT dataRangeChanged(cur);
        if (mLayer)
            mLayer->markDirty();
        if (mParentPlot)
            mParentPlot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void QCPColorMap2::setDataScaleType(QCPAxis::ScaleType type)
{
    if (mRenderer.dataScaleType() != type)
    {
        QCPRange prevRange = mRenderer.dataRange();
        mRenderer.setDataScaleType(type);
        QCPRange curRange = mRenderer.dataRange();
        if (prevRange.lower != curRange.lower || prevRange.upper != curRange.upper)
            Q_EMIT dataRangeChanged(curRange);
        Q_EMIT dataScaleTypeChanged(type);
        if (mLayer)
            mLayer->markDirty();
        if (mParentPlot)
            mParentPlot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void QCPColorMap2::rescaleDataRange(bool recalc)
{
    if (!mDataSource)
        return;
    auto* data = mPipeline.result();
    bool found = false;
    QCPRange range;
    if (data && !recalc)
        range = data->dataBounds();
    else
        range = mDataSource->zRange(found);
    if (range.lower < range.upper)
        setDataRange(range);
}

void QCPColorMap2::onViewportChanged()
{
    if (!mKeyAxis || !mValueAxis || !mDataSource) return;
    auto* axisRect = mKeyAxis->axisRect();
    if (!axisRect) return;

    mPipeline.onViewportChanged(ViewportParams::fromAxes(mKeyAxis.data(), mValueAxis.data()));
}

bool QCPColorMap2::canProduceContent() const
{
    if (!mKeyAxis || !mValueAxis || !mDataSource)
        return false;
    return mPipeline.result() != nullptr;
}

void QCPColorMap2::draw(QCPPainter* painter)
{
    PROFILE_HERE_N("QCPColorMap2::draw");
    if (!mKeyAxis || !mValueAxis)
        return;

    auto* resampledData = mPipeline.result();
    if (!resampledData)
    {
        if (!mDataSource) return;
        if (painter->modes().testFlag(QCPPainter::pmNoCaching))
        {
            // Export path: run transform synchronously since event loop is not pumped
            if (!mPipeline.runSynchronously(ViewportParams::fromAxes(mKeyAxis.data(), mValueAxis.data())))
                return;
            resampledData = mPipeline.result();
            if (!resampledData) return;
        }
        else
        {
            onViewportChanged();
            return;
        }
    }

    bool imageWasInvalidated = mRenderer.mapImageInvalidated();
    if (imageWasInvalidated)
        mRenderer.updateMapImage(resampledData);

    if (mRenderer.mapImage().isNull())
        return;

    QCPRange keyRange = resampledData->keyRange();
    QCPRange valueRange = resampledData->valueRange();
    applyDefaultAntialiasingHint(painter);
    mRenderer.draw(painter, mKeyAxis.data(), mValueAxis.data(), keyRange, valueRange);

    bool contoursActive = !mContourLevels.isEmpty() || mAutoContourCount > 0;
    if (contoursActive && (imageWasInvalidated || mContourCacheGen != mContourDataGen))
        updateContourGpu(resampledData);
    else if (!contoursActive)
        mRenderer.clearContour();
}

void QCPColorMap2::drawLegendIcon(QCPPainter* painter, const QRectF& rect) const
{
    QLinearGradient lg(rect.topLeft(), rect.topRight());
    lg.setColorAt(0, Qt::blue);
    lg.setColorAt(1, Qt::red);
    painter->setBrush(QBrush(lg));
    painter->setPen(Qt::NoPen);
    painter->drawRect(rect);
}

QCPRange QCPColorMap2::getKeyRange(bool& foundRange, QCP::SignDomain inSignDomain) const
{
    if (!mDataSource)
    {
        foundRange = false;
        return {};
    }
    return mDataSource->xRange(foundRange, inSignDomain);
}

QCPRange QCPColorMap2::getValueRange(bool& foundRange, QCP::SignDomain inSignDomain,
                                     const QCPRange&) const
{
    if (!mDataSource)
    {
        foundRange = false;
        return {};
    }
    return mDataSource->yRange(foundRange, inSignDomain);
}

double QCPColorMap2::selectTest(const QPointF& pos, bool onlySelectable, QVariant*) const
{
    if (onlySelectable && !mSelectable)
        return -1;
    if (!mKeyAxis || !mValueAxis || !mDataSource)
        return -1;

    double key = mKeyAxis->pixelToCoord(pos.x());
    double value = mValueAxis->pixelToCoord(pos.y());

    bool foundKey = false, foundValue = false;
    auto kr = mDataSource->xRange(foundKey);
    auto vr = mDataSource->yRange(foundValue);

    if (foundKey && foundValue && kr.contains(key) && vr.contains(value))
        return 0;
    return -1;
}

void QCPColorMap2::setContourLevels(const QVector<double>& levels)
{
    mContourLevels = levels;
    mAutoContourCount = 0;
    invalidateContourCache();
}

void QCPColorMap2::setContourPen(const QPen& pen)
{
    mContourPen = pen;
}

void QCPColorMap2::setContourLabelEnabled(bool enabled)
{
    mContourLabelEnabled = enabled;
}

void QCPColorMap2::setAutoContourLevels(int count)
{
    mAutoContourCount = qMax(0, count);
    mContourLevels.clear();
    invalidateContourCache();
}

void QCPColorMap2::invalidateContourCache()
{
    mContourCacheGen = 0;
    mContourDataGen = std::max(mContourDataGen, uint64_t(1));
}

void QCPColorMap2::updateContourGpu(const QCPColorMapData* data)
{
    PROFILE_HERE_N("QCPColorMap2::updateContourGpu");
    if (!data)
        return;

    mContourCacheGen = mContourDataGen;

    const int kSize = data->keySize();
    const int vSize = data->valueSize();
    if (kSize < 2 || vSize < 2)
        return;

    QCPRange bounds = data->dataBounds();
    if (bounds.lower >= bounds.upper)
        return;

    QVector<double> levels;
    if (!mContourLevels.isEmpty())
    {
        levels = mContourLevels;
    }
    else if (mAutoContourCount > 0)
    {
        double step = (bounds.upper - bounds.lower) / (mAutoContourCount + 1);
        levels.reserve(mAutoContourCount);
        for (int i = 1; i <= mAutoContourCount; ++i)
            levels.append(bounds.lower + i * step);
    }

    QCPRange keyRange = data->keyRange();
    QCPRange valRange = data->valueRange();
    double kSpan = keyRange.upper - keyRange.lower;
    double vSpan = valRange.upper - valRange.lower;
    if (kSpan <= 0 || vSpan <= 0)
        return;

    // Subsample large grids — contour lines don't need more than ~300 cells per axis
    constexpr int kMaxDim = 300;
    const double* raw = data->rawData();
    int sk = 1, sv = 1;
    int ck = kSize, cv = vSize;
    if (kSize > kMaxDim) { sk = kSize / kMaxDim; ck = kSize / sk; }
    if (vSize > kMaxDim) { sv = vSize / kMaxDim; cv = vSize / sv; }

    QVector<float> uvVerts;
    uvVerts.reserve(ck * cv / 4);

    for (double level : levels)
    {
        for (int ki = 0; ki < ck - 1; ++ki)
        {
            int srcK0 = ki * sk;
            int srcK1 = (ki + 1) * sk;
            double u0 = double(ki) / (ck - 1);
            double u1 = double(ki + 1) / (ck - 1);

            for (int vi = 0; vi < cv - 1; ++vi)
            {
                int srcV0 = vi * sv;
                int srcV1 = (vi + 1) * sv;

                double z00 = raw[srcV0 * kSize + srcK0];
                double z10 = raw[srcV0 * kSize + srcK1];
                double z11 = raw[srcV1 * kSize + srcK1];
                double z01 = raw[srcV1 * kSize + srcK0];

                if (!std::isfinite(z00) || !std::isfinite(z10) ||
                    !std::isfinite(z11) || !std::isfinite(z01))
                    continue;

                int idx = 0;
                if (z00 >= level) idx |= 1;
                if (z10 >= level) idx |= 2;
                if (z11 >= level) idx |= 4;
                if (z01 >= level) idx |= 8;
                if (idx == 0 || idx == 15) continue;

                double v0uv = 1.0 - double(vi) / (cv - 1);
                double v1uv = 1.0 - double(vi + 1) / (cv - 1);

                auto lerpu = [](double a, double va, double b, double vb, double lv) {
                    double t = (vb == va) ? 0.5 : (lv - va) / (vb - va);
                    return a + t * (b - a);
                };

                // Edge midpoints in UV: bottom(z00→z10), right(z10→z11), top(z01→z11), left(z00→z01)
                float bu = float(lerpu(u0, z00, u1, z10, level)), bv = float(v0uv);
                float ru = float(u1), rv = float(lerpu(v0uv, z10, v1uv, z11, level));
                float tu = float(lerpu(u0, z01, u1, z11, level)), tv = float(v1uv);
                float lu = float(u0), lv = float(lerpu(v0uv, z00, v1uv, z01, level));

                auto seg = [&](float x1, float y1, float x2, float y2) {
                    uvVerts.append(x1); uvVerts.append(y1);
                    uvVerts.append(x2); uvVerts.append(y2);
                };

                switch (idx) {
                    case  1: case 14: seg(bu,bv,lu,lv); break;
                    case  2: case 13: seg(bu,bv,ru,rv); break;
                    case  3: case 12: seg(lu,lv,ru,rv); break;
                    case  4: case 11: seg(ru,rv,tu,tv); break;
                    case  6: case  9: seg(bu,bv,tu,tv); break;
                    case  7: case  8: seg(lu,lv,tu,tv); break;
                    case 5: {
                        double center = (z00+z10+z11+z01)*0.25;
                        if (center >= level) { seg(bu,bv,ru,rv); seg(lu,lv,tu,tv); }
                        else                 { seg(bu,bv,lu,lv); seg(ru,rv,tu,tv); }
                        break;
                    }
                    case 10: {
                        double center = (z00+z10+z11+z01)*0.25;
                        if (center >= level) { seg(bu,bv,lu,lv); seg(ru,rv,tu,tv); }
                        else                 { seg(bu,bv,ru,rv); seg(lu,lv,tu,tv); }
                        break;
                    }
                }
            }
        }
    }

    mRenderer.setContourLines(std::move(uvVerts), mContourPen.color());
}
