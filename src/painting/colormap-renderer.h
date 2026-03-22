#pragma once
#include <colorgradient.h>
#include <axis/axis.h>
#include <QImage>
#include <functional>

class QCPAbstractPlottable;
class QCPColorScale;
class QCPColorMapData;
class QCPColormapRhiLayer;
class QCPPainter;
class QCustomPlot;

class QCPColormapRenderer
{
public:
    explicit QCPColormapRenderer(QCPAbstractPlottable* owner);
    ~QCPColormapRenderer();

    // State
    void setGradient(const QCPColorGradient& gradient);
    const QCPColorGradient& gradient() const { return mGradient; }

    void setDataRange(const QCPRange& range);
    QCPRange dataRange() const { return mDataRange; }

    void setDataScaleType(QCPAxis::ScaleType type);
    QCPAxis::ScaleType dataScaleType() const { return mDataScaleType; }

    void setColorScale(QCPColorScale* scale);
    QCPColorScale* colorScale() const { return mColorScale; }

    void rescaleDataRange(const QCPColorMapData* data, bool recalc);

    // Rendering
    using NormalizeFn = std::function<double(double value, int col, int row)>;
    void updateMapImage(const QCPColorMapData* data, NormalizeFn normalize = {});
    void draw(QCPPainter* painter, QCPAxis* keyAxis, QCPAxis* valueAxis,
              const QCPRange& keyRange, const QCPRange& valueRange);

    // RHI layer
    QCPColormapRhiLayer* ensureRhiLayer();
    void releaseRhiLayer();

    // Image cache
    bool mapImageInvalidated() const { return mMapImageInvalidated; }
    void invalidateMapImage() { mMapImageInvalidated = true; }
    const QImage& mapImage() const { return mMapImage; }

private:
    const QImage& flippedMapImage(Qt::Orientations flips);

    QCPAbstractPlottable* mOwner;
    QCPColorGradient mGradient;
    QCPRange mDataRange;
    QCPAxis::ScaleType mDataScaleType = QCPAxis::stLinear;
    QImage mMapImage;
    QImage mFlippedMapImage;
    Qt::Orientations mLastFlips {};
    bool mMapImageInvalidated = true;
    QCPColorScale* mColorScale = nullptr;
    QCPColormapRhiLayer* mRhiLayer = nullptr;
};
