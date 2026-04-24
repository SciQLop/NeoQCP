#include "scatter-rhi-layer.h"
#include "rhi-utils.h"
#include "Profiling.hpp"
#include "embedded_shaders.h"
#include "../scatterstyle.h"
#include <cstring>

QCPScatterRhiLayer::QCPScatterRhiLayer(QRhi* rhi)
    : mRhi(rhi)
{
}

QCPScatterRhiLayer::~QCPScatterRhiLayer()
{
    delete mPipeline;
    delete mSrb;
    delete mUniformBuffer;
    delete mInstanceBuffer;
    delete mQuadIndexBuffer;
    delete mQuadVertexBuffer;
    delete mSpriteTexture;
    delete mColormapTexture;
    delete mSampler;
    std::free(mStagingData);
}

void QCPScatterRhiLayer::invalidatePipeline()
{
    delete mPipeline;
    mPipeline = nullptr;
    delete mSrb;
    mSrb = nullptr;
    delete mUniformBuffer;
    mUniformBuffer = nullptr;
    mUniformBufferSize = 0;
    delete mQuadVertexBuffer;
    mQuadVertexBuffer = nullptr;
    delete mQuadIndexBuffer;
    mQuadIndexBuffer = nullptr;
    mQuadUploaded = false;
    delete mSpriteTexture;
    mSpriteTexture = nullptr;
    delete mColormapTexture;
    mColormapTexture = nullptr;
    delete mSampler;
    mSampler = nullptr;
    mSpriteTextureDirty = true;
    mColormapTextureDirty = true;
}

void QCPScatterRhiLayer::clear()
{
    mStagingSize = 0;
    mDrawEntries.resize(0);
    mDirty = true;
}

void QCPScatterRhiLayer::setAllOffsets(float offsetX, float offsetY)
{
    for (auto& entry : mDrawEntries)
    {
        entry.offsetX = offsetX;
        entry.offsetY = offsetY;
    }
}

int QCPScatterRhiLayer::ubufStride() const
{
    return mRhi->ubufAligned(sizeof(PerDrawUniforms));
}

void QCPScatterRhiLayer::stagingAppend(const float* src, int count)
{
    if (mStagingSize + count > mStagingCapacity)
    {
        mStagingCapacity = std::max(mStagingCapacity * 2, mStagingSize + count);
        mStagingData = static_cast<float*>(
            std::realloc(mStagingData, mStagingCapacity * sizeof(float)));
    }
    std::memcpy(mStagingData + mStagingSize, src, count * sizeof(float));
    mStagingSize += count;
}

void QCPScatterRhiLayer::addScatter(std::span<const float> points,
                                     const QCPScatterStyle& style,
                                     const QRect& clipRect, double dpr,
                                     int outputHeight,
                                     float offsetX, float offsetY,
                                     const QImage& colormapImage)
{
    PROFILE_HERE_N("QCPScatterRhiLayer::addScatter");

    if (points.empty() || points.size() % 3 != 0)
        return;

    // Check if sprite needs re-rendering
    const int newShape = static_cast<int>(style.shape());
    const double newSize = style.size();
    if (newShape != mCachedShape || !qFuzzyCompare(newSize, mCachedSize)
        || style.pen() != mCachedPen || style.brush() != mCachedBrush)
    {
        mSpriteImage = style.renderToImage(64);
        mSpriteTextureDirty = true;
        mCachedShape = newShape;
        mCachedSize = newSize;
        mCachedPen = style.pen();
        mCachedBrush = style.brush();
    }

    mHalfSize = static_cast<float>(newSize * 0.5);

    if (!colormapImage.isNull())
    {
        mColormapImage = colormapImage;
        mColormapTextureDirty = true;
        mUseColorAxis = true;
    }
    else
    {
        mUseColorAxis = false;
    }

    DrawEntry entry;
    entry.scissorRect = qcp::rhi::computeScissor(clipRect, dpr, outputHeight);
    entry.offsetX = offsetX;
    entry.offsetY = offsetY;
    entry.instanceOffset = mStagingSize / 3;
    entry.instanceCount = static_cast<int>(points.size()) / 3;

    stagingAppend(points.data(), static_cast<int>(points.size()));
    mDrawEntries.append(entry);
    mDirty = true;
}

bool QCPScatterRhiLayer::ensurePipeline(QRhiRenderPassDescriptor* rpDesc,
                                         int sampleCount)
{
    PROFILE_HERE_N("QCPScatterRhiLayer::ensurePipeline");
    if (mPipeline && mLastSampleCount == sampleCount)
        return true;

    invalidatePipeline();

    auto vertShader = qcp::rhi::loadEmbeddedShader(scatter_vert_qsb_data, scatter_vert_qsb_data_len);
    auto fragShader = qcp::rhi::loadEmbeddedShader(scatter_frag_qsb_data, scatter_frag_qsb_data_len);

    if (!vertShader.isValid() || !fragShader.isValid())
    {
        qDebug() << "Failed to load scatter shaders";
        return false;
    }

    // Sampler
    mSampler = mRhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
                                 QRhiSampler::None,
                                 QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    if (!mSampler->create())
        return false;

    // Sprite texture (1x1 placeholder until real sprite is uploaded)
    const auto texFmt = qcp::rhi::preferredTextureFormat(mRhi);
    mSpriteTexture = mRhi->newTexture(texFmt, QSize(1, 1));
    if (!mSpriteTexture->create())
        return false;
    mSpriteTextureDirty = true;

    // Colormap texture (256x1 placeholder)
    mColormapTexture = mRhi->newTexture(texFmt, QSize(256, 1));
    if (!mColormapTexture->create())
        return false;
    mColormapTextureDirty = true;

    // UBO
    const int stride = ubufStride();
    mUniformBufferSize = stride;
    mUniformBuffer = mRhi->newBuffer(QRhiBuffer::Dynamic,
                                      QRhiBuffer::UniformBuffer, mUniformBufferSize);
    if (!mUniformBuffer->create())
        return false;

    // SRB: UBO (binding 0, vert+frag), sprite (binding 1, frag), colormap (binding 2, frag)
    mSrb = mRhi->newShaderResourceBindings();
    mSrb->setBindings({
        QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            mUniformBuffer, sizeof(PerDrawUniforms)),
        QRhiShaderResourceBinding::sampledTexture(
            1, QRhiShaderResourceBinding::FragmentStage, mSpriteTexture, mSampler),
        QRhiShaderResourceBinding::sampledTexture(
            2, QRhiShaderResourceBinding::FragmentStage, mColormapTexture, mSampler)
    });
    if (!mSrb->create())
        return false;

    mPipeline = mRhi->newGraphicsPipeline();
    mPipeline->setShaderStages({
        {QRhiShaderStage::Vertex, vertShader},
        {QRhiShaderStage::Fragment, fragShader}
    });

    // Vertex layout: binding 0 = quad corners (PerVertex), binding 1 = instance data (PerInstance)
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        {2 * static_cast<quint32>(sizeof(float)), QRhiVertexInputBinding::PerVertex},
        {3 * static_cast<quint32>(sizeof(float)), QRhiVertexInputBinding::PerInstance}
    });
    inputLayout.setAttributes({
        {0, 0, QRhiVertexInputAttribute::Float2, 0},   // cornerOffset
        {1, 1, QRhiVertexInputAttribute::Float3, 0}    // instanceData
    });
    mPipeline->setVertexInputLayout(inputLayout);

    mPipeline->setTargetBlends({qcp::rhi::premultipliedAlphaBlend()});
    mPipeline->setFlags(QRhiGraphicsPipeline::UsesScissor);
    mPipeline->setTopology(QRhiGraphicsPipeline::Triangles);
    mPipeline->setSampleCount(sampleCount);
    mPipeline->setRenderPassDescriptor(rpDesc);
    mPipeline->setShaderResourceBindings(mSrb);

    if (!mPipeline->create())
    {
        qDebug() << "Failed to create scatter pipeline";
        delete mPipeline;
        mPipeline = nullptr;
        return false;
    }

    mLastSampleCount = sampleCount;
    return true;
}

void QCPScatterRhiLayer::uploadResources(QRhiResourceUpdateBatch* updates,
                                           const QSize& outputSize, float dpr,
                                           bool isYUpInNDC)
{
    PROFILE_HERE_N("QCPScatterRhiLayer::uploadResources");

    if (mDrawEntries.isEmpty() || !mUniformBuffer)
        return;

    // Upload static quad geometry once
    if (!mQuadUploaded)
    {
        // 4 vertices: corner offsets [-1,-1], [1,-1], [1,1], [-1,1]
        static const float quadVerts[] = {
            -1.0f, -1.0f,
             1.0f, -1.0f,
             1.0f,  1.0f,
            -1.0f,  1.0f
        };
        static const quint16 quadIndices[] = { 0, 1, 2, 2, 3, 0 };

        if (!mQuadVertexBuffer)
        {
            mQuadVertexBuffer = mRhi->newBuffer(QRhiBuffer::Immutable,
                                                 QRhiBuffer::VertexBuffer,
                                                 sizeof(quadVerts));
            mQuadVertexBuffer->create();
        }
        if (!mQuadIndexBuffer)
        {
            mQuadIndexBuffer = mRhi->newBuffer(QRhiBuffer::Immutable,
                                                QRhiBuffer::IndexBuffer,
                                                sizeof(quadIndices));
            mQuadIndexBuffer->create();
        }

        updates->uploadStaticBuffer(mQuadVertexBuffer, quadVerts);
        updates->uploadStaticBuffer(mQuadIndexBuffer, quadIndices);
        mQuadUploaded = true;
    }

    // Upload/resize sprite texture
    if (mSpriteTextureDirty && !mSpriteImage.isNull())
    {
        const auto texFmt = qcp::rhi::preferredTextureFormat(mRhi);
        const QImage::Format imgFmt = (texFmt == QRhiTexture::BGRA8)
            ? QImage::Format_ARGB32_Premultiplied
            : QImage::Format_RGBA8888_Premultiplied;
        QImage converted = mSpriteImage.convertedTo(imgFmt);

        if (mSpriteTexture->pixelSize() != converted.size())
        {
            mSpriteTexture->setPixelSize(converted.size());
            mSpriteTexture->create();
            // Recreate SRB since texture was resized
            if (mSrb)
                mSrb->create();
        }

        QRhiTextureSubresourceUploadDescription subDesc(converted);
        updates->uploadTexture(mSpriteTexture, QRhiTextureUploadDescription(
            QRhiTextureUploadEntry(0, 0, subDesc)));
        mSpriteTextureDirty = false;
    }

    // Upload colormap texture
    if (mColormapTextureDirty && !mColormapImage.isNull())
    {
        const auto texFmt = qcp::rhi::preferredTextureFormat(mRhi);
        const QImage::Format imgFmt = (texFmt == QRhiTexture::BGRA8)
            ? QImage::Format_ARGB32_Premultiplied
            : QImage::Format_RGBA8888_Premultiplied;
        QImage converted = mColormapImage.scaledToWidth(256).convertedTo(imgFmt);

        if (mColormapTexture->pixelSize() != converted.size())
        {
            mColormapTexture->setPixelSize(converted.size());
            mColormapTexture->create();
            if (mSrb)
                mSrb->create();
        }

        QRhiTextureSubresourceUploadDescription subDesc(converted);
        updates->uploadTexture(mColormapTexture, QRhiTextureUploadDescription(
            QRhiTextureUploadEntry(0, 0, subDesc)));
        mColormapTextureDirty = false;
    }

    // Grow UBO if needed
    const int stride = ubufStride();
    const int requiredUboSize = stride * mDrawEntries.size();
    if (requiredUboSize > mUniformBufferSize)
    {
        mUniformBufferSize = requiredUboSize;
        mUniformBuffer->setSize(mUniformBufferSize);
        if (!mUniformBuffer->create())
            return;
        if (mSrb)
            mSrb->create();
    }

    // Upload per-draw uniforms
    const float yFlip = isYUpInNDC ? -1.0f : 1.0f;
    for (int i = 0; i < mDrawEntries.size(); ++i)
    {
        const auto& entry = mDrawEntries[i];
        PerDrawUniforms params = {
            float(outputSize.width()),
            float(outputSize.height()),
            yFlip,
            dpr,
            entry.offsetX,
            entry.offsetY,
            mHalfSize,
            mUseColorAxis ? 1.0f : 0.0f
        };
        updates->updateDynamicBuffer(mUniformBuffer, i * stride, sizeof(params), &params);
    }

    // Upload instance data only when geometry changed
    if (!mDirty || mStagingSize == 0)
        return;

    const int requiredSize = mStagingSize * static_cast<int>(sizeof(float));

    if (!mInstanceBuffer || mInstanceBufferSize < requiredSize)
    {
        delete mInstanceBuffer;
        mInstanceBuffer = mRhi->newBuffer(QRhiBuffer::Dynamic,
                                           QRhiBuffer::VertexBuffer,
                                           requiredSize);
        if (!mInstanceBuffer->create())
        {
            delete mInstanceBuffer;
            mInstanceBuffer = nullptr;
            mInstanceBufferSize = 0;
            return;
        }
        mInstanceBufferSize = requiredSize;
    }

    updates->updateDynamicBuffer(mInstanceBuffer, 0, requiredSize, mStagingData);
    mDirty = false;
}

void QCPScatterRhiLayer::render(QRhiCommandBuffer* cb,
                                 const QSize& outputSize)
{
    PROFILE_HERE_N("QCPScatterRhiLayer::render");

    if (!mPipeline || !mQuadVertexBuffer || !mQuadIndexBuffer
        || !mInstanceBuffer || !mSrb || mDrawEntries.isEmpty())
        return;

    cb->setGraphicsPipeline(mPipeline);
    cb->setViewport({0, 0, float(outputSize.width()), float(outputSize.height())});

    const int stride = ubufStride();
    for (int i = 0; i < mDrawEntries.size(); ++i)
    {
        const auto& entry = mDrawEntries[i];
        if (entry.instanceCount <= 0)
            continue;

        const QPair<int, quint32> dynamicOffset(0, quint32(i * stride));
        cb->setShaderResources(mSrb, 1, &dynamicOffset);

        const QRhiCommandBuffer::VertexInput vbufBindings[] = {
            {mQuadVertexBuffer, 0},
            {mInstanceBuffer, quint32(entry.instanceOffset * 3 * sizeof(float))}
        };
        cb->setVertexInput(0, 2, vbufBindings, mQuadIndexBuffer, 0,
                           QRhiCommandBuffer::IndexUInt16);

        cb->setScissor({entry.scissorRect.x(), entry.scissorRect.y(),
                        entry.scissorRect.width(), entry.scissorRect.height()});

        cb->drawIndexed(6, entry.instanceCount, 0, 0, 0);
    }
}
