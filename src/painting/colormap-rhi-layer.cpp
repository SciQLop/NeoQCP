#include "colormap-rhi-layer.h"
#include "embedded_shaders.h"
#include "Profiling.hpp"

QCPColormapRhiLayer::QCPColormapRhiLayer(QRhi* rhi)
    : mRhi(rhi)
{
}

QCPColormapRhiLayer::~QCPColormapRhiLayer()
{
    delete mPipeline;
    delete mSrb;
    delete mSampler;
    delete mVertexBuffer;
    delete mTexture;
}

void QCPColormapRhiLayer::clear()
{
    mStagingImage = QImage();
}

void QCPColormapRhiLayer::invalidatePipeline()
{
    delete mPipeline;
    mPipeline = nullptr;
    delete mSrb;
    mSrb = nullptr;
}

void QCPColormapRhiLayer::setImage(const QImage& image)
{
    mStagingImage = image;
    mTextureDirty = true;
}

void QCPColormapRhiLayer::setQuadRect(const QRectF& pixelRect)
{
    mQuadPixelRect = pixelRect;
    mGeometryDirty = true;
}

void QCPColormapRhiLayer::setScissorRect(const QRect& scissor)
{
    mScissorRect = scissor;
}

bool QCPColormapRhiLayer::ensurePipeline(QRhiRenderPassDescriptor* rpDesc,
                                          int sampleCount)
{
    PROFILE_HERE_N("QCPColormapRhiLayer::ensurePipeline");
    if (mPipeline && mLastSampleCount == sampleCount)
        return true;

    invalidatePipeline();

    // Reuse composite shaders: position(float2) + texcoord(float2),
    // combined image sampler at binding 1, no uniform buffer.
    // We pre-compute NDC positions on the CPU.
    QShader vertShader = QShader::fromSerialized(QByteArray::fromRawData(
        reinterpret_cast<const char*>(composite_vert_qsb_data), composite_vert_qsb_data_len));
    QShader fragShader = QShader::fromSerialized(QByteArray::fromRawData(
        reinterpret_cast<const char*>(composite_frag_qsb_data), composite_frag_qsb_data_len));

    if (!vertShader.isValid() || !fragShader.isValid())
    {
        qDebug() << "Failed to load composite shaders for colormap";
        return false;
    }

    if (!mSampler)
    {
        mSampler = mRhi->newSampler(QRhiSampler::Nearest, QRhiSampler::Nearest,
                                     QRhiSampler::None,
                                     QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
        mSampler->create();
    }

    // Layout-only SRB for pipeline creation (nullptr texture is valid for layout)
    // Actual texture binding created in uploadResources() once texture exists
    auto* layoutSrb = mRhi->newShaderResourceBindings();
    layoutSrb->setBindings({
        QRhiShaderResourceBinding::sampledTexture(
            0, QRhiShaderResourceBinding::FragmentStage,
            nullptr, mSampler)
    });
    layoutSrb->create();

    mPipeline = mRhi->newGraphicsPipeline();
    mPipeline->setShaderStages({
        {QRhiShaderStage::Vertex, vertShader},
        {QRhiShaderStage::Fragment, fragShader}
    });

    // Vertex layout: position(float2) + texcoord(float2) — same as composite
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({{4 * sizeof(float)}});
    inputLayout.setAttributes({
        {0, 0, QRhiVertexInputAttribute::Float2, 0},
        {0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float)}
    });
    mPipeline->setVertexInputLayout(inputLayout);

    // Premultiplied alpha blending
    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable = true;
    blend.srcColor = QRhiGraphicsPipeline::One;
    blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    blend.srcAlpha = QRhiGraphicsPipeline::One;
    blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    mPipeline->setTargetBlends({blend});

    mPipeline->setFlags(QRhiGraphicsPipeline::UsesScissor);
    mPipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
    mPipeline->setSampleCount(sampleCount);
    mPipeline->setRenderPassDescriptor(rpDesc);
    mPipeline->setShaderResourceBindings(layoutSrb);

    if (!mPipeline->create())
    {
        qDebug() << "Failed to create colormap pipeline";
        delete mPipeline;
        mPipeline = nullptr;
        delete layoutSrb;
        return false;
    }

    delete layoutSrb;
    mLastSampleCount = sampleCount;
    return true;
}

void QCPColormapRhiLayer::uploadResources(QRhiResourceUpdateBatch* updates,
                                            const QSize& outputSize, float dpr,
                                            bool isYUpInNDC)
{
    PROFILE_HERE_N("QCPColormapRhiLayer::uploadResources");
    if (mStagingImage.isNull())
        return;

    // (Re)create texture if size changed
    QSize imgSize = mStagingImage.size();
    bool textureRecreated = false;
    if (!mTexture || mTextureSize != imgSize)
    {
        delete mTexture;
        const auto fmt = mRhi->isTextureFormatSupported(QRhiTexture::BGRA8)
            ? QRhiTexture::BGRA8
            : QRhiTexture::RGBA8;
        mTexture = mRhi->newTexture(fmt, imgSize);
        if (!mTexture->create())
        {
            qDebug() << "Failed to create colormap RHI texture";
            delete mTexture;
            mTexture = nullptr;
            delete mSrb;
            mSrb = nullptr;
            return;
        }
        mTextureSize = imgSize;
        mTextureDirty = true;
        textureRecreated = true;
    }

    // (Re)create SRB when texture changed or after invalidatePipeline
    if ((!mSrb || textureRecreated) && mTexture)
    {
        delete mSrb;
        mSrb = mRhi->newShaderResourceBindings();
        mSrb->setBindings({
            QRhiShaderResourceBinding::sampledTexture(
                0, QRhiShaderResourceBinding::FragmentStage,
                mTexture, mSampler)
        });
        mSrb->create();
    }

    // Upload texture data
    if (mTextureDirty)
    {
        QRhiTextureSubresourceUploadDescription subDesc(mStagingImage);
        updates->uploadTexture(mTexture, QRhiTextureUploadDescription(
            QRhiTextureUploadEntry(0, 0, subDesc)));
        mTextureDirty = false;
    }

    // Update quad vertex buffer with NDC coordinates
    // Recompute when geometry changes or output size changes (NDC depends on pixel dimensions)
    if (mGeometryDirty || !mVertexBuffer || mLastOutputSize != outputSize)
    {
        mLastOutputSize = outputSize;
        // Convert pixel rect to NDC: x_ndc = (x_px * dpr / width) * 2 - 1
        const float w = float(outputSize.width());
        const float h = float(outputSize.height());
        const float yFlip = isYUpInNDC ? -1.0f : 1.0f;

        auto toNDC = [&](float px, float py) -> std::pair<float, float> {
            float ndcX = (px * dpr / w) * 2.0f - 1.0f;
            float ndcY = yFlip * ((py * dpr / h) * 2.0f - 1.0f);
            return {ndcX, ndcY};
        };

        auto [x0, y0] = toNDC(mQuadPixelRect.left(), mQuadPixelRect.top());
        auto [x1, y1] = toNDC(mQuadPixelRect.right(), mQuadPixelRect.bottom());

        // UV mapping: image top-left = (0,0), bottom-right = (1,1)
        // In Y-up NDC (OpenGL), larger NDC Y = top of screen = top of image (v=0)
        // In Y-down NDC (Metal/D3D), smaller NDC Y = top of screen = top of image (v=0)
        // Since we compute y0 from the top pixel and y1 from the bottom pixel,
        // and yFlip handles the NDC sign, UV is always (0,0) at top-left.
        const float u0 = 0.0f, u1 = 1.0f;
        const float v0 = 0.0f, v1 = 1.0f;

        // Triangle strip: top-left, top-right, bottom-left, bottom-right
        const float verts[] = {
            x0, y0,  u0, v0,
            x1, y0,  u1, v0,
            x0, y1,  u0, v1,
            x1, y1,  u1, v1,
        };

        if (!mVertexBuffer)
        {
            mVertexBuffer = mRhi->newBuffer(QRhiBuffer::Dynamic,
                                             QRhiBuffer::VertexBuffer,
                                             sizeof(verts));
            mVertexBuffer->create();
        }

        updates->updateDynamicBuffer(mVertexBuffer, 0, sizeof(verts), verts);
        mGeometryDirty = false;
    }
}

void QCPColormapRhiLayer::render(QRhiCommandBuffer* cb, const QSize& outputSize)
{
    PROFILE_HERE_N("QCPColormapRhiLayer::render");

    if (!mPipeline || !mVertexBuffer || !mSrb || !mTexture)
        return;

    cb->setGraphicsPipeline(mPipeline);
    cb->setViewport({0, 0, float(outputSize.width()), float(outputSize.height())});
    cb->setShaderResources(mSrb);

    const QRhiCommandBuffer::VertexInput vbufBinding(mVertexBuffer, 0);
    cb->setVertexInput(0, 1, &vbufBinding);

    cb->setScissor({mScissorRect.x(), mScissorRect.y(),
                    mScissorRect.width(), mScissorRect.height()});
    cb->draw(4);
}
