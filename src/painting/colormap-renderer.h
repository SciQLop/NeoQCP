#pragma once
#include <QImage>
#include <axis/axis.h>
#include <colorgradient.h>

class QCPAbstractPlottable;
class QCPColorMapData;

//! Colourises a grid and caches the image. Where and when it is drawn belongs to the plottable.
class QCPColormapRenderer
{
public:
    explicit QCPColormapRenderer(QCPAbstractPlottable* owner);

    // State
    void setGradient(const QCPColorGradient& gradient);

    const QCPColorGradient& gradient() const { return mGradient; }

    void setDataRange(const QCPRange& range);

    QCPRange dataRange() const { return mDataRange; }

    void setDataScaleType(QCPAxis::ScaleType type);

    QCPAxis::ScaleType dataScaleType() const { return mDataScaleType; }

    // Rendering
    void updateMapImage(const QCPColorMapData* data);
    //! A horizontal ramp of the gradient over the data range: a legend icon's swatch.
    QImage gradientImage(int width, int height);

    // Image cache
    bool mapImageInvalidated() const { return mMapImageInvalidated; }

    void invalidateMapImage();

    const QImage& mapImage() const { return mMapImage; }

    //! How many times a grid was colourised (test hook).
    [[nodiscard]] int buildCount() const { return mBuildCount; }

private:
    QCPAbstractPlottable* mOwner;
    QCPColorGradient mGradient;
    QCPRange mDataRange;
    QCPAxis::ScaleType mDataScaleType = QCPAxis::stLinear;
    QImage mMapImage;
    bool mMapImageInvalidated = true;
    int mBuildCount = 0;
};
