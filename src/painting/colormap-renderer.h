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
    //! Colourises \a data key-major, or transposed (key down the rows) for a vertical key axis.
    void updateMapImage(const QCPColorMapData* data, bool keyIsVertical);
    //! A horizontal ramp of the gradient over the data range: a legend icon's swatch.
    QImage gradientImage(int width, int height);

    // Image cache
    bool mapImageInvalidated() const { return mMapImageInvalidated; }

    void invalidateMapImage();

    const QImage& mapImage() const { return mMapImage; }

    //! The orientation the cached image was built for.
    [[nodiscard]] bool keyIsVertical() const { return mKeyIsVertical; }

    //! How many times a grid was colourised (test hook).
    [[nodiscard]] int buildCount() const { return mBuildCount; }

private:
    QCPAbstractPlottable* mOwner;
    QCPColorGradient mGradient;
    QCPRange mDataRange;
    QCPAxis::ScaleType mDataScaleType = QCPAxis::stLinear;
    QImage mMapImage;
    bool mMapImageInvalidated = true;
    bool mKeyIsVertical = false;
    int mBuildCount = 0;
};
