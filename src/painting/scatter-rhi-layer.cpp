#include "scatter-rhi-layer.h"
#include "rhi-utils.h"
#include "Profiling.hpp"
#include "embedded_shaders.h"
#include "../scatterstyle.h"
#include <algorithm>
#include <cstring>

namespace {
bool sameMarker(const QCPScatterStyle& a, const QCPScatterStyle& b)
{
    return a.shape() == b.shape() && qFuzzyCompare(a.size(), b.size()) && a.pen() == b.pen()
        && a.brush() == b.brush() && a.customPath() == b.customPath()
        && a.pixmap().cacheKey() == b.pixmap().cacheKey();
}
} // namespace

QCPScatterRhiLayer::QCPScatterRhiLayer(QRhi* rhi)
    : mRhi(rhi)
{
}

QCPScatterRhiLayer::~QCPScatterRhiLayer()
{
    delete mColoredPipeline;
    delete mPipeline;
    delete mSrb;
    delete mUniformBuffer;
    delete mInstanceBuffer;
    delete mColorBuffer;
    delete mQuadIndexBuffer;
    delete mQuadVertexBuffer;
    for (auto& look : mLooks)
        releaseLookResources(look);
    delete mPlaceholderTexture;
    delete mSampler;
    std::free(mStagingData);
}

void QCPScatterRhiLayer::invalidatePipeline()
{
    delete mColoredPipeline;
    mColoredPipeline = nullptr;
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
    for (auto& look : mLooks)
        releaseLookResources(look);
    delete mPlaceholderTexture;
    mPlaceholderTexture = nullptr;
    delete mSampler;
    mSampler = nullptr;
}

void QCPScatterRhiLayer::releaseLookResources(Look& look)
{
    delete look.srb;
    look.srb = nullptr;
    delete look.spriteTexture;
    look.spriteTexture = nullptr;
    delete look.colormapTexture;
    look.colormapTexture = nullptr;
}

int QCPScatterRhiLayer::lookFor(const QCPScatterStyle& style, const QImage& colormapImage)
{
    for (int i = 0; i < static_cast<int>(mLooks.size()); ++i)
    {
        if (sameMarker(mLooks[i].style, style) && mLooks[i].colormapImage == colormapImage)
        {
            mLooks[i].used = true;
            return i;
        }
    }
    mLooks.push_back(Look { style, colormapImage, style.renderToImage(64) });
    return static_cast<int>(mLooks.size()) - 1;
}

// Called from clear(), when no draw entry refers to a look any more, so indices may shift.
void QCPScatterRhiLayer::dropUnusedLooks()
{
    const auto unused = std::stable_partition(mLooks.begin(), mLooks.end(),
                                              [](const Look& look) { return look.used; });
    std::for_each(unused, mLooks.end(), releaseLookResources);
    mLooks.erase(unused, mLooks.end());
    for (auto& look : mLooks)
        look.used = false;
}

void QCPScatterRhiLayer::clear()
{
    dropUnusedLooks();
    mStagingSize = 0;
    mDrawEntries.resize(0);
    mColorStaging.clear();
    mDirty = true;
}

void QCPScatterRhiLayer::setAllOffsets(float offsetX, float offsetY)
{
    mLastOffsetX = offsetX;
    mLastOffsetY = offsetY;
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

void QCPScatterRhiLayer::addDraw(std::span<const float> xyz, const QCPScatterStyle& style,
                                 const QRect& clipRect, double dpr, int outputHeight,
                                 float offsetX, float offsetY, float alpha,
                                 const QImage& colormapImage, int colorOffset)
{
    DrawEntry entry;
    entry.scissorRect = qcp::rhi::computeScissor(clipRect, dpr, outputHeight);
    entry.offsetX = offsetX;
    entry.offsetY = offsetY;
    entry.alpha = alpha;
    entry.instanceOffset = mStagingSize / 3;
    entry.instanceCount = static_cast<int>(xyz.size()) / 3;
    entry.halfSize = static_cast<float>(style.size() * 0.5);
    entry.useColorAxis = !colormapImage.isNull();
    entry.colorOffset = colorOffset;
    entry.look = lookFor(style, colormapImage);

    stagingAppend(xyz.data(), static_cast<int>(xyz.size()));
    mDrawEntries.append(entry);
    mDirty = true;
}

void QCPScatterRhiLayer::addScatter(std::span<const float> points,
                                     const QCPScatterStyle& style,
                                     const QRect& clipRect, double dpr,
                                     int outputHeight,
                                     float offsetX, float offsetY,
                                     const QImage& colormapImage,
                                     float alpha)
{
    PROFILE_HERE_N("QCPScatterRhiLayer::addScatter");
    if (points.empty() || points.size() % 3 != 0)
        return;
    addDraw(points, style, clipRect, dpr, outputHeight, offsetX, offsetY, alpha, colormapImage,
            -1);
}

void QCPScatterRhiLayer::addScatterColored(std::span<const float> xy, std::span<const float> rgba,
                                           const QCPScatterStyle& style, const QRect& clipRect,
                                           double dpr, int outputHeight, float offsetX,
                                           float offsetY, float alpha)
{
    PROFILE_HERE_N("QCPScatterRhiLayer::addScatterColored");
    const size_t n = xy.size() / 2;
    if (n == 0 || xy.size() % 2 != 0 || rgba.size() != n * 4)
        return;
    std::vector<float> xyz(n * 3);
    for (size_t i = 0; i < n; ++i)
    {
        xyz[i * 3 + 0] = xy[i * 2 + 0];
        xyz[i * 3 + 1] = xy[i * 2 + 1];
        xyz[i * 3 + 2] = 0.0f;
    }
    const int colorOffset = static_cast<int>(mColorStaging.size() / 4);
    mColorStaging.insert(mColorStaging.end(), rgba.begin(), rgba.end());
    addDraw(xyz, style, clipRect, dpr, outputHeight, offsetX, offsetY, alpha, {}, colorOffset);
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

    // Stands in for the sprite and colormap in the template bindings, and for the
    // colormap of looks that have none (the shader then never samples it).
    mPlaceholderTexture = mRhi->newTexture(qcp::rhi::preferredTextureFormat(mRhi), QSize(1, 1));
    if (!mPlaceholderTexture->create())
        return false;

    // UBO
    const int stride = ubufStride();
    mUniformBufferSize = stride;
    mUniformBuffer = mRhi->newBuffer(QRhiBuffer::Dynamic,
                                      QRhiBuffer::UniformBuffer, mUniformBufferSize);
    if (!mUniformBuffer->create())
        return false;

    mSrb = mRhi->newShaderResourceBindings();
    setBindings(mSrb, mPlaceholderTexture, mPlaceholderTexture);
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
    // A missing coloured pipeline only drops coloured markers (render() skips them);
    // failing here would rebuild every scatter pipeline on every frame.
    if (!createColoredPipeline(rpDesc, sampleCount))
        qWarning() << "Coloured scatter pipeline unavailable: markers coloured by a scalar are not drawn";
    return true;
}

// UBO (binding 0, vert+frag), sprite (binding 1, frag), colormap (binding 2, frag)
void QCPScatterRhiLayer::setBindings(QRhiShaderResourceBindings* srb, QRhiTexture* sprite,
                                     QRhiTexture* colormap) const
{
    srb->setBindings({
        QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            mUniformBuffer, sizeof(PerDrawUniforms)),
        QRhiShaderResourceBinding::sampledTexture(
            1, QRhiShaderResourceBinding::FragmentStage, sprite, mSampler),
        QRhiShaderResourceBinding::sampledTexture(
            2, QRhiShaderResourceBinding::FragmentStage, colormap, mSampler)
    });
}

QRhiTexture* QCPScatterRhiLayer::uploadedTexture(const QImage& image,
                                                 QRhiResourceUpdateBatch* updates)
{
    const auto texFmt = qcp::rhi::preferredTextureFormat(mRhi);
    const QImage::Format imgFmt = (texFmt == QRhiTexture::BGRA8)
        ? QImage::Format_ARGB32_Premultiplied
        : QImage::Format_RGBA8888_Premultiplied;
    const QImage converted = image.convertedTo(imgFmt);
    auto* texture = mRhi->newTexture(texFmt, converted.size());
    if (!texture->create())
    {
        delete texture;
        return nullptr;
    }
    updates->uploadTexture(texture, QRhiTextureUploadDescription(
        QRhiTextureUploadEntry(0, 0, QRhiTextureSubresourceUploadDescription(converted))));
    return texture;
}

bool QCPScatterRhiLayer::createLookResources(Look& look, QRhiResourceUpdateBatch* updates)
{
    look.spriteTexture = uploadedTexture(look.spriteImage, updates);
    if (!look.colormapImage.isNull())
        look.colormapTexture = uploadedTexture(look.colormapImage.scaledToWidth(256), updates);
    if (!look.spriteTexture || (!look.colormapImage.isNull() && !look.colormapTexture))
    {
        releaseLookResources(look);
        return false;
    }
    look.srb = mRhi->newShaderResourceBindings();
    setBindings(look.srb, look.spriteTexture,
                look.colormapTexture ? look.colormapTexture : mPlaceholderTexture);
    if (!look.srb->create())
    {
        releaseLookResources(look);
        return false;
    }
    return true;
}

bool QCPScatterRhiLayer::createColoredPipeline(QRhiRenderPassDescriptor* rpDesc, int sampleCount)
{
    auto colVert = qcp::rhi::loadEmbeddedShader(scatter_colored_vert_qsb_data,
                                                scatter_colored_vert_qsb_data_len);
    auto colFrag = qcp::rhi::loadEmbeddedShader(scatter_colored_frag_qsb_data,
                                                scatter_colored_frag_qsb_data_len);
    if (!colVert.isValid() || !colFrag.isValid())
    {
        qDebug() << "Failed to load coloured scatter shaders";
        return false;
    }
    mColoredPipeline = mRhi->newGraphicsPipeline();
    mColoredPipeline->setShaderStages({{QRhiShaderStage::Vertex, colVert},
                                       {QRhiShaderStage::Fragment, colFrag}});
    QRhiVertexInputLayout colLayout;
    colLayout.setBindings({
        {2 * static_cast<quint32>(sizeof(float)), QRhiVertexInputBinding::PerVertex},
        {3 * static_cast<quint32>(sizeof(float)), QRhiVertexInputBinding::PerInstance},
        {4 * static_cast<quint32>(sizeof(float)), QRhiVertexInputBinding::PerInstance}
    });
    colLayout.setAttributes({
        {0, 0, QRhiVertexInputAttribute::Float2, 0},   // cornerOffset
        {1, 1, QRhiVertexInputAttribute::Float3, 0},   // instanceData
        {2, 2, QRhiVertexInputAttribute::Float4, 0}    // instanceColor
    });
    mColoredPipeline->setVertexInputLayout(colLayout);
    mColoredPipeline->setTargetBlends({qcp::rhi::premultipliedAlphaBlend()});
    mColoredPipeline->setFlags(QRhiGraphicsPipeline::UsesScissor);
    mColoredPipeline->setTopology(QRhiGraphicsPipeline::Triangles);
    mColoredPipeline->setSampleCount(sampleCount);
    mColoredPipeline->setRenderPassDescriptor(rpDesc);
    // Shares the plain pipeline's SRB: the coloured shaders use bindings 0 and 1 of it.
    mColoredPipeline->setShaderResourceBindings(mSrb);
    if (!mColoredPipeline->create())
    {
        qDebug() << "Failed to create coloured scatter pipeline";
        delete mColoredPipeline;
        mColoredPipeline = nullptr;
        return false;
    }
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

    for (auto& look : mLooks)
    {
        if (!look.srb)
            createLookResources(look, updates);
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
        for (auto& look : mLooks)
        {
            if (look.srb)
                look.srb->create();
        }
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
            entry.halfSize,
            entry.useColorAxis ? 1.0f : 0.0f,
            entry.alpha, {0, 0, 0}
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
    if (!uploadColors(updates))
        return;
    mDirty = false;
}

bool QCPScatterRhiLayer::uploadColors(QRhiResourceUpdateBatch* updates)
{
    if (mColorStaging.empty())
        return true;
    const int colorBytes = static_cast<int>(mColorStaging.size() * sizeof(float));
    if (!mColorBuffer || mColorBufferSize < colorBytes)
    {
        delete mColorBuffer;
        mColorBuffer = mRhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, colorBytes);
        if (!mColorBuffer->create())
        {
            delete mColorBuffer;
            mColorBuffer = nullptr;
            mColorBufferSize = 0;
            return false;
        }
        mColorBufferSize = colorBytes;
    }
    updates->updateDynamicBuffer(mColorBuffer, 0, colorBytes, mColorStaging.data());
    return true;
}

void QCPScatterRhiLayer::render(QRhiCommandBuffer* cb,
                                 const QSize& outputSize)
{
    PROFILE_HERE_N("QCPScatterRhiLayer::render");

    if (!mPipeline || !mQuadVertexBuffer || !mQuadIndexBuffer
        || !mInstanceBuffer || !mSrb || mDrawEntries.isEmpty())
        return;

    QRhiGraphicsPipeline* current = nullptr;
    const int stride = ubufStride();
    for (int i = 0; i < mDrawEntries.size(); ++i)
    {
        const auto& entry = mDrawEntries[i];
        if (entry.instanceCount <= 0)
            continue;
        const bool coloured = entry.colorOffset >= 0;
        if (coloured && (!mColoredPipeline || !mColorBuffer))
            continue;
        auto* srb = mLooks[entry.look].srb;
        if (!srb)
            continue;
        QRhiGraphicsPipeline* wanted = coloured ? mColoredPipeline : mPipeline;
        if (wanted != current)
        {
            cb->setGraphicsPipeline(wanted);
            cb->setViewport({0, 0, float(outputSize.width()), float(outputSize.height())});
            current = wanted;
        }

        const QPair<int, quint32> dynamicOffset(0, quint32(i * stride));
        cb->setShaderResources(srb, 1, &dynamicOffset);

        const QRhiCommandBuffer::VertexInput plainInputs[] = {
            {mQuadVertexBuffer, 0},
            {mInstanceBuffer, quint32(entry.instanceOffset * 3 * sizeof(float))}
        };
        const QRhiCommandBuffer::VertexInput colouredInputs[] = {
            {mQuadVertexBuffer, 0},
            {mInstanceBuffer, quint32(entry.instanceOffset * 3 * sizeof(float))},
            {mColorBuffer, quint32(entry.colorOffset * 4 * sizeof(float))}
        };
        if (coloured)
            cb->setVertexInput(0, 3, colouredInputs, mQuadIndexBuffer, 0,
                               QRhiCommandBuffer::IndexUInt16);
        else
            cb->setVertexInput(0, 2, plainInputs, mQuadIndexBuffer, 0,
                               QRhiCommandBuffer::IndexUInt16);

        cb->setScissor({entry.scissorRect.x(), entry.scissorRect.y(),
                        entry.scissorRect.width(), entry.scissorRect.height()});
        cb->drawIndexed(6, entry.instanceCount, 0, 0, 0);
    }
}

