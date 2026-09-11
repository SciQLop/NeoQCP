#pragma once

#include <QColor>
#include <QPointF>
#include <QRect>
#include <QVector>
#include <QMap>
#include <rhi/qrhi.h>

#include "../axis/range.h"

class QCPAxis;
class QCPAxisRect;

class QCPGridRhiLayer
{
public:
    struct DrawGroup
    {
        QCPAxisRect* axisRect = nullptr;
        int vertexOffset = 0;
        int vertexCount = 0;
        QRect scissorRect;             // empty = no scissor (tick marks)
        QRhiBuffer* uniformBuffer = nullptr;
        QRhiShaderResourceBindings* srb = nullptr;
        bool isGridLines = true;       // false = tick marks
    };

    // Mirrors the SpanParams UBO layout in span.vert (std140, 16 floats).
    struct UboParams
    {
        float width = 0, height = 0, yFlip = 0, dpr = 0;
        float keyRangeLower = 0, keyRangeUpper = 0, keyAxisOffset = 0, keyAxisLength = 0, keyLogScale = 0;
        float valRangeLower = 0, valRangeUpper = 0, valAxisOffset = 0, valAxisLength = 0, valLogScale = 0;
        float _pad0 = 0, _pad1 = 0;
    };

    explicit QCPGridRhiLayer(QRhi* rhi);
    ~QCPGridRhiLayer();

    void markGeometryDirty();
    bool hasContent() const { return !mDrawGroups.isEmpty(); }

    void invalidatePipeline();
    bool ensurePipeline(QRhiRenderPassDescriptor* rpDesc, int sampleCount);
    void uploadResources(QRhiResourceUpdateBatch* updates,
                         const QSize& outputSize, float dpr,
                         bool isYUpInNDC);
    void renderGridLines(QRhiCommandBuffer* cb, const QSize& outputSize);
    void renderTickMarks(QRhiCommandBuffer* cb, const QSize& outputSize);

    void registerAxis(QCPAxis* axis);
    void unregisterAxis(QCPAxis* axis);

    // Test read-back accessors.
    const QVector<DrawGroup>& drawGroups() const { return mDrawGroups; }
    const QVector<float>& stagingVertices() const { return mStagingVertices; }
    const UboParams* lastUboParams(QCPAxisRect* ar) const
    {
        auto it = mLastUboParams.constFind(ar);
        return it == mLastUboParams.constEnd() ? nullptr : &it.value();
    }

private:
    void rebuildGeometry(float dpr, int outputHeight);
    void appendTickVertices(QCPAxis* axis, QCPAxisRect* ar, QVector<float>& out) const;
    QVector<QCPAxis*> orderedAxesForRect(QCPAxisRect* ar) const;
    void renderGroups(QRhiCommandBuffer* cb, const QSize& outputSize, bool gridLines);
    void cleanupDrawGroups();
    // Reference value subtracted from this axis's tick/range coordinates
    // before they are narrowed to float32, so the GPU never has to represent
    // a huge absolute coordinate (e.g. a Unix timestamp) at full precision --
    // only the small offset from it. Fixed at 0 for log axes, where the
    // subtraction would have to happen before the log(), not after.
    double axisOrigin(QCPAxis* axis) const;

    QRhi* mRhi;

    QVector<float> mStagingVertices;
    QVector<DrawGroup> mDrawGroups;
    bool mGeometryDirty = true;

    QRhiBuffer* mVertexBuffer = nullptr;
    QRhiGraphicsPipeline* mPipeline = nullptr;
    QRhiShaderResourceBindings* mLayoutSrb = nullptr;
    QRhiBuffer* mLayoutUbo = nullptr;
    int mVertexBufferSize = 0;
    int mLastSampleCount = 0;
    QMap<QCPAxisRect*, QRect> mLastAxisRectBounds;

    QVector<QCPAxis*> mAxes;

    struct CachedAxisTicks {
        QVector<double> majorTicks;
        QVector<double> subTicks;
        bool subGridVisible = false;
        QRgb gridColor = 0;
        QRgb subGridColor = 0;
        QRgb zeroLineColor = 0;
        float gridPenWidth = 0;
        float subGridPenWidth = 0;
        float zeroLinePenWidth = 0;
        Qt::PenStyle zeroLinePenStyle = Qt::NoPen;
        QRgb tickColor = 0;
        QRgb subTickColor = 0;
        float tickPenWidth = 0;
        float subTickPenWidth = 0;
        float tickLengthOut = 0;
        float tickLengthIn = 0;
        float subTickLengthOut = 0;
        float subTickLengthIn = 0;
        bool subTicksVisible = false;
        // Neither setVisible() nor setTicks() marks the geometry dirty, and both
        // change the tick vertex count, so they must be part of the signature.
        bool axisVisible = false;
        bool ticksVisible = false;
        // Same for setScaleType()/setRangeReversed(): the range value is
        // unchanged but every baked tick pixel moves.
        int scaleType = 0;
        bool rangeReversed = false;
        // Tick marks are baked to pixels at rebuild time; this is the range that
        // baking used, so uploadResources() can detect a pan (same ticks, moved
        // range) and re-bake in place without a full geometry rebuild.
        QCPRange lastRange;
        // axisOrigin(axis) at rebuild time -- the grid-line vertices baked in
        // this cycle are relative to it, so uploadResources() must keep using
        // it (not a freshly recomputed origin) until the next full rebuild.
        double originValue = 0.0;
    };
    QMap<QCPAxis*, CachedAxisTicks> mCachedTicks;
    QMap<QCPAxisRect*, UboParams> mLastUboParams;
};
