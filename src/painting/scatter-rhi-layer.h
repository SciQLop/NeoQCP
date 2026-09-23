#pragma once

#include <QBrush>
#include <QImage>
#include <QPen>
#include <QRect>
#include <QVector>
#include <rhi/qrhi.h>
#include <span>
#include <cstdlib>
#include <vector>

class QCPScatterStyle;

class QCPScatterRhiLayer
{
public:
    struct DrawEntry
    {
        int instanceOffset = 0;
        int instanceCount = 0;
        float offsetX = 0;
        float offsetY = 0;
        float alpha = 1;
        QRect scissorRect;
        float halfSize = 0;
        bool useColorAxis = false;
        int colorOffset = -1; // first colour of this draw in the colour buffer, -1 = plain
    };

    explicit QCPScatterRhiLayer(QRhi* rhi);
    ~QCPScatterRhiLayer();

    void clear();

    void addScatter(std::span<const float> points,
                    const QCPScatterStyle& style,
                    const QRect& clipRect, double dpr, int outputHeight,
                    float offsetX = 0, float offsetY = 0,
                    const QImage& colormapImage = {},
                    float alpha = 1);

    // xy: 2 floats per marker; rgba: 4 premultiplied floats per marker.
    void addScatterColored(std::span<const float> xy, std::span<const float> rgba,
                           const QCPScatterStyle& style, const QRect& clipRect, double dpr,
                           int outputHeight, float offsetX = 0, float offsetY = 0,
                           float alpha = 1);

    void setAllOffsets(float offsetX, float offsetY);
    QPointF lastUniformOffset() const { return QPointF(mLastOffsetX, mLastOffsetY); }

    void invalidatePipeline();
    bool ensurePipeline(QRhiRenderPassDescriptor* rpDesc, int sampleCount);
    void uploadResources(QRhiResourceUpdateBatch* updates,
                          const QSize& outputSize, float dpr, bool isYUpInNDC);
    void render(QRhiCommandBuffer* cb, const QSize& outputSize);

    bool isDirty() const { return mDirty; }
    bool hasGeometry() const { return !mDrawEntries.isEmpty(); }
    const QVector<DrawEntry>& drawEntries() const { return mDrawEntries; }

private:
    friend class TestColorByScalar;

    struct alignas(16) PerDrawUniforms
    {
        float width, height, yFlip, dpr;
        float offsetX, offsetY;
        float halfSize;
        float useColorAxis;
        float alpha;
        float _pad[3]; // pad to 48 bytes for std140
    };
    static_assert(sizeof(PerDrawUniforms) == 48);

    int ubufStride() const;
    bool createColoredPipeline(QRhiRenderPassDescriptor* rpDesc, int sampleCount);
    bool uploadColors(QRhiResourceUpdateBatch* updates);
    void addDraw(std::span<const float> xyz, const QCPScatterStyle& style, const QRect& clipRect,
                 double dpr, int outputHeight, float offsetX, float offsetY, float alpha,
                 bool useColorAxis, int colorOffset);

    void stagingAppend(const float* src, int count);
    float* mStagingData = nullptr;
    int mStagingSize = 0;
    int mStagingCapacity = 0;
    std::vector<float> mColorStaging; // 4 floats per coloured instance

    QRhi* mRhi;
    QVector<DrawEntry> mDrawEntries;

    QRhiBuffer* mQuadVertexBuffer = nullptr;
    QRhiBuffer* mQuadIndexBuffer = nullptr;
    QRhiBuffer* mInstanceBuffer = nullptr;
    int mInstanceBufferSize = 0;
    QRhiBuffer* mColorBuffer = nullptr;
    int mColorBufferSize = 0;

    QRhiBuffer* mUniformBuffer = nullptr;
    int mUniformBufferSize = 0;

    QRhiTexture* mSpriteTexture = nullptr;
    QRhiTexture* mColormapTexture = nullptr;
    QRhiSampler* mSampler = nullptr;

    QRhiShaderResourceBindings* mSrb = nullptr;
    QRhiGraphicsPipeline* mPipeline = nullptr;
    QRhiGraphicsPipeline* mColoredPipeline = nullptr;
    int mLastSampleCount = 0;

    bool mDirty = false;
    bool mQuadUploaded = false;
    bool mSpriteTextureDirty = false;
    bool mColormapTextureDirty = false;
    QImage mSpriteImage;
    QImage mColormapImage;

    int mCachedShape = -1;
    double mCachedSize = -1;
    QPen mCachedPen;
    QBrush mCachedBrush;
    float mLastOffsetX = 0;
    float mLastOffsetY = 0;
};
