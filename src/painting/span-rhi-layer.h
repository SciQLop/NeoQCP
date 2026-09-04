#pragma once

#include <QColor>
#include <QPointF>
#include <QRect>
#include <QVector>
#include <QMap>
#include <rhi/qrhi.h>

class QCPAbstractItem;
class QCPAxisRect;
class QCPItemVSpan;
class QCPItemHSpan;
class QCPItemRSpan;
class QBrush;
class QPen;

class QCPSpanRhiLayer
{
public:
    struct DrawGroup
    {
        QCPAxisRect* axisRect = nullptr;
        int vertexOffset = 0;
        int vertexCount = 0;
        QRect scissorRect;
        QRhiBuffer* uniformBuffer = nullptr;
        QRhiShaderResourceBindings* srb = nullptr;
    };

    explicit QCPSpanRhiLayer(QRhi* rhi);
    ~QCPSpanRhiLayer();

    void registerSpan(QCPAbstractItem* span);
    void unregisterSpan(QCPAbstractItem* span);
    void markGeometryDirty();

    // Recomputes the cheap per-span/per-axis-rect change signature, updates the
    // internal cache and returns true when anything affecting geometry changed since
    // the last call. Called by uploadResources() every frame; also used by tests.
    // Never touches mRhi.
    bool detectGeometryChanges();

    bool hasSpans() const { return !mSpans.isEmpty(); }

    void invalidatePipeline();
    bool ensurePipeline(QRhiRenderPassDescriptor* rpDesc, int sampleCount);
    void uploadResources(QRhiResourceUpdateBatch* updates,
                          const QSize& outputSize, float dpr,
                          bool isYUpInNDC);
    void render(QRhiCommandBuffer* cb, const QSize& outputSize);

private:
    struct SpanSignature
    {
        float e0 = 0, e1 = 0, e2 = 0, e3 = 0; // edge pixels; meaning depends on span type
        quint32 fillRgba = 0;
        quint32 borderRgba = 0;
        float borderWidth = 0;
        int borderStyle = 0;
        bool borderCosmetic = false;
        bool selected = false;
        bool operator==(const SpanSignature& other) const = default;
    };

    SpanSignature computeSignature(QCPAbstractItem* span) const;

    void rebuildGeometry(float dpr, int outputHeight);
    void appendVSpanGeometry(QCPItemVSpan* vspan, QCPAxisRect* ar);
    void appendHSpanGeometry(QCPItemHSpan* hspan, QCPAxisRect* ar);
    void appendRSpanGeometry(QCPItemRSpan* rspan, QCPAxisRect* ar);
    void cleanupDrawGroups();

    QRhi* mRhi; // non-owned; lifetime managed by QRhiWidget
    QVector<QCPAbstractItem*> mSpans;

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
    QVector<SpanSignature> mSignatureCache;
};
