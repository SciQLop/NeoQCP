#pragma once

#include <QColor>
#include <QImage>
#include <QRect>
#include <QRectF>
#include <QVector>
#include <rhi/qrhi.h>

class QCPLayer;
class QCPLayerable;

class QCPColormapRhiLayer
{
public:
    explicit QCPColormapRhiLayer(QRhi* rhi);
    ~QCPColormapRhiLayer();

    void setImage(const QImage& image);
    void setQuadRect(const QRectF& pixelRect);
    void setScissorRect(const QRect& scissor);
    void setLayer(QCPLayer* layer) { mLayer = layer; }
    QCPLayer* layer() const { return mLayer; }

    // The colormap plottable this RHI layer renders for. The compositor skips
    // layers whose owner is not really visible — without this a hidden colormap
    // keeps compositing its last frame (its draw() stops being called, so the
    // quad is never updated and it freezes in place during pans).
    void setOwner(QCPLayerable* owner) { mOwner = owner; }
    QCPLayerable* owner() const { return mOwner; }
    bool ownerVisible() const;

    void setContourLines(QVector<float> uvVertices, const QColor& color);
    void clearContourLines();

    void invalidatePipeline();
    bool ensurePipeline(QRhiRenderPassDescriptor* rpDesc, int sampleCount,
                        QRhiBuffer* compositeUbo);
    void uploadResources(QRhiResourceUpdateBatch* updates,
                         const QSize& outputSize, float dpr, bool isYUpInNDC,
                         QRhiBuffer* compositeUbo);
    void render(QRhiCommandBuffer* cb, const QSize& outputSize);

    bool hasContent() const { return !mStagingImage.isNull(); }
    void clear();

private:
    QRhi* mRhi;
    QCPLayer* mLayer = nullptr;
    QCPLayerable* mOwner = nullptr;

    // CPU staging — colormap
    QImage mStagingImage;
    QRectF mQuadPixelRect;
    QRect mScissorRect;
    bool mTextureDirty = false;
    bool mGeometryDirty = false;

    // CPU staging — contour lines (UV [0,1] space, 2 floats per vertex)
    QVector<float> mContourUvVertices;
    QColor mContourColor{Qt::white};
    bool mContourLinesDirty = false;
    bool mContourUboDirty = false;

    // GPU resources — base colormap
    QRhiTexture* mTexture = nullptr;
    QRhiSampler* mSampler = nullptr;
    QRhiBuffer* mVertexBuffer = nullptr;
    QRhiShaderResourceBindings* mSrb = nullptr;
    QRhiGraphicsPipeline* mPipeline = nullptr;

    // GPU resources — contour lines
    QRhiBuffer* mLineVbo = nullptr;
    QRhiBuffer* mLineUbo = nullptr;
    QRhiShaderResourceBindings* mLineSrb = nullptr;
    QRhiGraphicsPipeline* mLinePipeline = nullptr;
    int mLineVertexCount = 0;
    int mLineVboSize = 0;

    int mLastSampleCount = 0;
    QSize mTextureSize;
    QSize mLastOutputSize;
    float mNdcX0 = 0, mNdcY0 = 0, mNdcX1 = 0, mNdcY1 = 0;

    bool ensureTexture(QRhiBuffer* compositeUbo);
    void updateQuadGeometry(QRhiResourceUpdateBatch* updates,
                            const QSize& outputSize, float dpr, bool isYUpInNDC);
};
