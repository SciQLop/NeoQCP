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

private:
    void rebuildGeometry(float dpr, int outputHeight);
    void appendTickVertices(QCPAxis* axis, QCPAxisRect* ar, QVector<float>& out) const;
    QVector<QCPAxis*> orderedAxesForRect(QCPAxisRect* ar) const;
    void renderGroups(QRhiCommandBuffer* cb, const QSize& outputSize, bool gridLines);
    void cleanupDrawGroups();

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
        // Tick marks are baked to pixels at rebuild time; this is the range that
        // baking used, so uploadResources() can detect a pan (same ticks, moved
        // range) and re-bake in place without a full geometry rebuild.
        QCPRange lastRange;
    };
    QMap<QCPAxis*, CachedAxisTicks> mCachedTicks;
};
