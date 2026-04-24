#pragma once

#include <QImage>
#include <QRect>
#include <QVector>
#include <rhi/qrhi.h>
#include <span>
#include <cstdlib>

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
        QRect scissorRect;
    };

    explicit QCPScatterRhiLayer(QRhi* rhi);
    ~QCPScatterRhiLayer();

    void clear();

    void addScatter(std::span<const float> points,
                    const QCPScatterStyle& style,
                    const QRect& clipRect, double dpr, int outputHeight,
                    float offsetX = 0, float offsetY = 0,
                    const QImage& colormapImage = {});

    void setAllOffsets(float offsetX, float offsetY);

    void invalidatePipeline();
    bool ensurePipeline(QRhiRenderPassDescriptor* rpDesc, int sampleCount);
    void uploadResources(QRhiResourceUpdateBatch* updates,
                          const QSize& outputSize, float dpr, bool isYUpInNDC);
    void render(QRhiCommandBuffer* cb, const QSize& outputSize);

    bool isDirty() const { return mDirty; }
    bool hasGeometry() const { return !mDrawEntries.isEmpty(); }

private:
    struct alignas(16) PerDrawUniforms
    {
        float width, height, yFlip, dpr;
        float offsetX, offsetY;
        float halfSize;
        float useColorAxis;
    };
    static_assert(sizeof(PerDrawUniforms) == 32);

    int ubufStride() const;

    void stagingAppend(const float* src, int count);
    float* mStagingData = nullptr;
    int mStagingSize = 0;
    int mStagingCapacity = 0;

    QRhi* mRhi;
    QVector<DrawEntry> mDrawEntries;

    QRhiBuffer* mQuadVertexBuffer = nullptr;
    QRhiBuffer* mQuadIndexBuffer = nullptr;
    QRhiBuffer* mInstanceBuffer = nullptr;
    int mInstanceBufferSize = 0;

    QRhiBuffer* mUniformBuffer = nullptr;
    int mUniformBufferSize = 0;

    QRhiTexture* mSpriteTexture = nullptr;
    QRhiTexture* mColormapTexture = nullptr;
    QRhiSampler* mSampler = nullptr;

    QRhiShaderResourceBindings* mSrb = nullptr;
    QRhiGraphicsPipeline* mPipeline = nullptr;
    int mLastSampleCount = 0;

    bool mDirty = false;
    bool mQuadUploaded = false;
    bool mSpriteTextureDirty = false;
    bool mColormapTextureDirty = false;
    QImage mSpriteImage;
    QImage mColormapImage;
    float mHalfSize = 0;
    bool mUseColorAxis = false;

    int mCachedShape = -1;
    double mCachedSize = -1;
    QPen mCachedPen;
    QBrush mCachedBrush;
};
