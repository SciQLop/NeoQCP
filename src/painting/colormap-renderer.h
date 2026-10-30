#pragma once
#include <colorgradient.h>
#include <axis/axis.h>
#include <QImage>
#include <QLineF>
#include <QPen>
#include <QPointer>
#include <QPointF>
#include <QVector>

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
    //! Null once the scale is deleted. Out of line since the QPointer member: rebuild users.
    QCPColorScale* colorScale() const;

    // Rendering
    void updateMapImage(const QCPColorMapData* data);
    void draw(QCPPainter* painter, QCPAxis* keyAxis, QCPAxis* valueAxis,
              const QCPRange& keyRange, const QCPRange& valueRange);
    //! Where the image of a grid over \a keyRange × \a valueRange is drawn, in pixels, ordered
    //! (left < right, top < bottom). A reversed axis flips the image inside it instead.
    static QRectF imageRect(const QCPAxis* keyAxis, const QCPAxis* valueAxis,
                            const QCPRange& keyRange, const QCPRange& valueRange);
    //! True when the last on-screen draw put the image on the GPU quad (painted after the
    //! layer's paint buffer), false when it was painted with the painter.
    [[nodiscard]] bool imageOnGpu() const { return mImageOnGpu; }

    // RHI layer
    QCPColormapRhiLayer* ensureRhiLayer();
    void releaseRhiLayer();

    // Image cache
    bool mapImageInvalidated() const { return mMapImageInvalidated; }
    void invalidateMapImage();
    const QImage& mapImage() const { return mMapImage; }
    QImage& mapImage() { return mMapImage; }

    // Contour lines (UV-space vertices)
    void setContourLines(QVector<float> uvVertices, const QColor& color);
    void clearContour();
    [[nodiscard]] bool hasContourOnGpu() const { return mContourOnGpu; }

    // QPainter fallback for exports (pmNoCaching) and non-RHI compositing:
    // UV segments (already mirrored like the GPU lines), mapped through the image rectangle.
    void setContourFallback(QVector<QLineF> uvSegments, const QPen& pen);

    //! The contour labels on the GPU, over the image and its lines (see QCPColormapRhiLayer).
    void setLabelOverlay(const QImage& image, const QRectF& pixelRect);
    void clearLabelOverlay();
    [[nodiscard]] bool hasLabelOverlay() const;

    // Hides whatever this colormap's GPU quad last showed. For when a caller
    // (QCPColorMap2::draw()) decides the rendered content no longer belongs
    // anywhere near the current axes, so it must not be left on screen.
    // No-op if RHI rendering was never engaged.
    void clearRhiContent();

private:
    const QImage& flippedMapImage(Qt::Orientations flips);
    void drawContourFallback(QCPPainter* painter, const QRectF& imageRect, const QCPAxis* keyAxis);

    QCPAbstractPlottable* mOwner;
    QCPColorGradient mGradient;
    QCPRange mDataRange;
    QCPAxis::ScaleType mDataScaleType = QCPAxis::stLinear;
    QImage mMapImage;
    QImage mFlippedMapImage;
    Qt::Orientations mLastFlips {};
    bool mMapImageInvalidated = true;
    QPointer<QCPColorScale> mColorScale; // the scale may be deleted first
    QCPColormapRhiLayer* mRhiLayer = nullptr;
    bool mContourOnGpu = false;
    bool mImageOnGpu = false;
    QVector<QLineF> mContourFallback;
    QPen mContourFallbackPen;
};
