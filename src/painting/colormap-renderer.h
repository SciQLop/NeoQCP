#pragma once
#include <colorgradient.h>
#include <axis/axis.h>
#include <QImage>
#include <QLineF>
#include <QPen>
#include <QPointF>
#include <QVector>
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
    QImage& mapImage() { return mMapImage; }

    // Contour lines (UV-space vertices)
    void setContourLines(QVector<float> uvVertices, const QColor& color);
    void clearContour();
    [[nodiscard]] bool hasContourOnGpu() const { return mContourOnGpu; }

    // QPainter fallback for exports (pmNoCaching) and non-RHI compositing:
    // data-coordinate segments drawn with coordToPixel, so axis reversal
    // and log scales map correctly without any UV bookkeeping.
    void setContourFallback(QVector<QLineF> segments, const QPen& pen);

    // Hides whatever this colormap's GPU quad last showed. For when a caller
    // (QCPColorMap2::draw()) decides the rendered content no longer belongs
    // anywhere near the current axes, so it must not be left on screen.
    // No-op if RHI rendering was never engaged.
    void clearRhiContent();

private:
    const QImage& flippedMapImage(Qt::Orientations flips);
    void drawContourFallback(QCPPainter* painter, QCPAxis* keyAxis, QCPAxis* valueAxis);

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
    bool mContourOnGpu = false;
    QVector<QLineF> mContourFallback;
    QPen mContourFallbackPen;
};
