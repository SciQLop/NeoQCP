#include "plottable-rhi-layer.h"
#include "Profiling.hpp"
#include "embedded_shaders.h"
#include "rhi-utils.h"
#include <algorithm>
#include <cstring>

namespace
{
QRhiVertexInputLayout vertexLayoutOf(int strideFloats,
                                     std::initializer_list<QRhiVertexInputAttribute> attributes)
{
    QRhiVertexInputLayout layout;
    layout.setBindings({ { quint32(strideFloats) * quint32(sizeof(float)) } });
    layout.setAttributes(attributes);
    return layout;
}

QRhiBuffer* createUniformBuffer(QRhi* rhi, int size, const char* failure)
{
    QRhiBuffer* buffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, size);
    if (!buffer->create())
    {
        qDebug() << failure;
        delete buffer;
        return nullptr;
    }
    return buffer;
}

// The SRB names the buffer size, so it has to be recreated after a resize.
bool growUniformBuffer(QRhiBuffer* buffer, int& size, int required, QRhiShaderResourceBindings* srb,
                       const char* failure)
{
    if (required <= size)
        return true;
    size = required;
    buffer->setSize(size);
    if (!buffer->create())
    {
        qDebug() << failure;
        return false;
    }
    if (srb)
        srb->create();
    return true;
}

bool uploadVertices(QRhi* rhi, QRhiResourceUpdateBatch* updates, QRhiBuffer*& buffer,
                    int& bufferSize, const float* floats, int floatCount, const char* failure)
{
    const int requiredSize = floatCount * static_cast<int>(sizeof(float));
    if (!buffer || bufferSize < requiredSize)
    {
        delete buffer;
        buffer = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, requiredSize);
        if (!buffer->create())
        {
            qDebug() << failure;
            delete buffer;
            buffer = nullptr;
            bufferSize = 0;
            return false;
        }
        bufferSize = requiredSize;
    }
    updates->updateDynamicBuffer(buffer, 0, requiredSize, floats);
    return true;
}
} // namespace

QCPPlottableRhiLayer::QCPPlottableRhiLayer(QRhi* rhi) : mRhi(rhi) { }

QCPPlottableRhiLayer::~QCPPlottableRhiLayer()
{
    delete mPipeline;
    delete mSrb;
    delete mUniformBuffer;
    delete mVertexBuffer;
    delete mDashPipeline;
    delete mDashSrb;
    delete mDashUniformBuffer;
    delete mDashVertexBuffer;
    delete mImagePipeline;
    delete mImageUniformBuffer;
    delete mImageVertexBuffer;
    delete mImageSampler;
    for (ImageTexture* texture : mImageTextures)
    {
        delete texture->srb;
        delete texture->texture;
        delete texture;
    }
    std::free(mStagingData);
    std::free(mDashStagingData);
}

void QCPPlottableRhiLayer::invalidatePipeline()
{
    delete mPipeline;
    mPipeline = nullptr;
    delete mSrb;
    mSrb = nullptr;
    delete mUniformBuffer;
    mUniformBuffer = nullptr;
    mUniformBufferSize = 0;
    delete mDashPipeline;
    mDashPipeline = nullptr;
    delete mDashSrb;
    mDashSrb = nullptr;
    delete mDashUniformBuffer;
    mDashUniformBuffer = nullptr;
    mDashUniformBufferSize = 0;
    delete mImagePipeline;
    mImagePipeline = nullptr;
    delete mImageUniformBuffer;
    mImageUniformBuffer = nullptr;
    // The textures outlive a render-pass change; only their SRBs point at the image UBO.
    for (ImageTexture* texture : mImageTextures)
    {
        delete texture->srb;
        texture->srb = nullptr;
    }
}

void QCPPlottableRhiLayer::clear()
{
    mStagingSize = 0; // preserve allocation
    mDrawEntries.resize(0); // preserve capacity
    // The entries that go are gone with the shifts they carried; the ones a replay adds next hold
    // only their own build offset.
    mLastOffsetX = 0;
    mLastOffsetY = 0;
    mDashStagingSize = 0;
    mDashEntries.resize(0);
    mImageEntries.resize(0); // the textures stay: a clear and replay must not re-upload them
    mDrawOrder.resize(0);
    mDirty = true;
}

void QCPPlottableRhiLayer::setAllOffsets(float offsetX, float offsetY)
{
    // An entry keeps the offset it was submitted with: a reused recording is built for the pan it
    // saw, not for this frame. Only the layer's shift since the last call is added, so a pan that
    // repainted the layer and one that translated it add up instead of replacing each other.
    const float shiftX = offsetX - mLastOffsetX;
    const float shiftY = offsetY - mLastOffsetY;
    mLastOffsetX = offsetX;
    mLastOffsetY = offsetY;
    for (auto& entry : mDrawEntries)
    {
        entry.offsetX += shiftX;
        entry.offsetY += shiftY;
    }
    for (auto& entry : mDashEntries)
    {
        entry.offsetX += shiftX;
        entry.offsetY += shiftY;
    }
    for (auto& entry : mImageEntries)
    {
        entry.offsetX += shiftX;
        entry.offsetY += shiftY;
    }
    // UBO needs re-upload but vertex data is unchanged
}

int QCPPlottableRhiLayer::imageUbufStride() const
{
    return mRhi->ubufAligned(sizeof(ImageUniforms));
}

int QCPPlottableRhiLayer::ubufStride() const
{
    return mRhi->ubufAligned(sizeof(PerDrawUniforms));
}

int QCPPlottableRhiLayer::dashUbufStride() const
{
    return mRhi->ubufAligned(sizeof(DashUniforms));
}

void QCPPlottableRhiLayer::addPlottable(std::span<const float> fillVerts,
                                        std::span<const float> strokeVerts, const QRect& clipRect,
                                        double dpr, int outputHeight, float offsetX, float offsetY,
                                        float alpha)
{
    if (fillVerts.empty() && strokeVerts.empty())
        return;
    PROFILE_HERE_N("QCPPlottableRhiLayer::addPlottable");
    DrawEntry entry;
    entry.scissorRect = qcp::rhi::computeScissor(clipRect, dpr, outputHeight);
    entry.offsetX = offsetX;
    entry.offsetY = offsetY;
    entry.alpha = alpha;
    entry.vertexOffset = mStagingSize / 6;
    entry.fillVertexCount = static_cast<int>(fillVerts.size()) / 6;

    qcp::rhi::appendFloats(mStagingData, mStagingSize, mStagingCapacity, fillVerts.data(),
                           int(fillVerts.size()));
    qcp::rhi::appendFloats(mStagingData, mStagingSize, mStagingCapacity, strokeVerts.data(),
                           int(strokeVerts.size()));
    entry.vertexCount = mStagingSize / 6 - entry.vertexOffset;

    mDirty = true;
    if (extendLastSolid(entry))
        return;
    mDrawEntries.append(entry);
    mDrawOrder.append({ DrawKind::Solid, int(mDrawEntries.size()) - 1 });
}

bool QCPPlottableRhiLayer::extendLastSolid(const DrawEntry& entry)
{
    if (mDrawOrder.isEmpty() || mDrawOrder.last().kind != DrawKind::Solid)
        return false;
    DrawEntry& last = mDrawEntries[mDrawOrder.last().index];
    if (last.scissorRect != entry.scissorRect || last.offsetX != entry.offsetX
        || last.offsetY != entry.offsetY || last.alpha != entry.alpha
        || last.vertexOffset + last.vertexCount != entry.vertexOffset)
        return false;
    last.vertexCount += entry.vertexCount;
    last.fillVertexCount += entry.fillVertexCount;
    return true;
}

void QCPPlottableRhiLayer::addDashedPlottable(std::span<const float> strokeVerts,
                                              std::span<const float> dashPattern, float dashOffset,
                                              const QRect& clipRect, double dpr, int outputHeight,
                                              float offsetX, float offsetY, float alpha)
{
    if (strokeVerts.empty())
        return;
    PROFILE_HERE_N("QCPPlottableRhiLayer::addDashedPlottable");
    DashedEntry entry;
    entry.scissorRect = qcp::rhi::computeScissor(clipRect, dpr, outputHeight);
    entry.offsetX = offsetX;
    entry.offsetY = offsetY;
    entry.alpha = alpha;
    entry.dashOffset = dashOffset;
    entry.patternCount = int(std::min<std::size_t>(dashPattern.size(), 8));
    float total = 0;
    for (int i = 0; i < entry.patternCount; ++i)
    {
        entry.pattern[i] = dashPattern[std::size_t(i)];
        total += entry.pattern[i];
    }
    entry.patternLength = total;
    if (entry.patternCount == 0 || total <= 0)
        return; // an empty pattern is a solid line: the caller takes the solid path

    entry.vertexOffset = mDashStagingSize / 7;
    entry.vertexCount = int(strokeVerts.size()) / 7;
    qcp::rhi::appendFloats(mDashStagingData, mDashStagingSize, mDashStagingCapacity,
                           strokeVerts.data(), int(strokeVerts.size()));
    mDirty = true;
    if (extendLastDashed(entry))
        return;
    mDashEntries.append(entry);
    mDrawOrder.append({ DrawKind::Dashed, int(mDashEntries.size()) - 1 });
}

bool QCPPlottableRhiLayer::extendLastDashed(const DashedEntry& entry)
{
    if (mDrawOrder.isEmpty() || mDrawOrder.last().kind != DrawKind::Dashed)
        return false;
    DashedEntry& last = mDashEntries[mDrawOrder.last().index];
    // A different pattern or phase inks different pixels, so only an identical one can continue
    // the run; the dashes themselves are triangles, so the ranges join without a connecting line.
    if (last.scissorRect != entry.scissorRect || last.offsetX != entry.offsetX
        || last.offsetY != entry.offsetY || last.alpha != entry.alpha
        || last.dashOffset != entry.dashOffset || last.patternLength != entry.patternLength
        || last.patternCount != entry.patternCount
        || !std::equal(std::begin(last.pattern), std::end(last.pattern), std::begin(entry.pattern))
        || last.vertexOffset + last.vertexCount != entry.vertexOffset)
        return false;
    last.vertexCount += entry.vertexCount;
    return true;
}

bool QCPPlottableRhiLayer::addImage(const QImage& image, const QRectF& pixelRect,
                                    const QRect& clipRect, double dpr, int outputHeight,
                                    float offsetX, float offsetY, float alpha)
{
    if (image.isNull() || mImageBroken || mImageEntries.size() >= kMaxImages)
        return false;
    const qint64 key = qint64(image.cacheKey());
    ImageTexture*& texture = mImageTextures[key];
    if (!texture)
    {
        texture = new ImageTexture;
        texture->pending = image; // the only place an upload is scheduled
    }
    texture->used = true;

    ImageEntry entry;
    entry.imageKey = key;
    entry.pixelRect = pixelRect;
    entry.deviceSize = image.size();
    entry.offsetX = offsetX;
    entry.offsetY = offsetY;
    entry.alpha = alpha;
    entry.scissorRect = qcp::rhi::computeScissor(clipRect, dpr, outputHeight);
    mImageEntries.append(entry);
    mDrawOrder.append({ DrawKind::Image, int(mImageEntries.size()) - 1 });
    return true;
}

bool QCPPlottableRhiLayer::ensurePipeline(QRhiRenderPassDescriptor* rpDesc, int sampleCount)
{
    PROFILE_HERE_N("QCPPlottableRhiLayer::ensurePipeline");
    // A dashed pipeline that failed for this sample count is remembered: rebuilding it every frame
    // would rebuild the solid one too, and the dashed lines would never appear again.
    if (mPipeline && mLastSampleCount == sampleCount
        && (mDashPipeline || (mDashBroken && mLastDashSampleCount == sampleCount))
        && (mImagePipeline || (mImageBroken && mLastImageSampleCount == sampleCount)))
        return mPipeline != nullptr;

    // Rebuild pipeline if sample count changed or first creation
    invalidatePipeline();

    // Create a small initial UBO — will be grown in uploadResources as needed.
    // Uses dynamic offsets: one PerDrawUniforms slot per draw entry.
    const int stride = ubufStride();
    mUniformBufferSize = stride; // start with 1 slot
    delete mUniformBuffer;
    mUniformBuffer = createUniformBuffer(mRhi, mUniformBufferSize,
                                         "Failed to create plottable uniform buffer");
    if (!mUniformBuffer)
        return false;

    // SRB with dynamic offset: bind the full buffer, one slot at a time
    delete mSrb;
    mSrb = qcp::rhi::createSrb(mRhi, mUniformBuffer, 0, QRhiShaderResourceBinding::VertexStage,
                               "Failed to create plottable SRB", sizeof(PerDrawUniforms));
    if (!mSrb)
        return false;

    // Vertex layout: (x, y) float2 + (r, g, b, a) float4
    mPipeline = qcp::rhi::createPipeline(
        mRhi, rpDesc, sampleCount, plottable_vert_qsb_data, plottable_vert_qsb_data_len,
        plottable_frag_qsb_data, plottable_frag_qsb_data_len,
        vertexLayoutOf(6,
                       { { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
                         { 0, 1, QRhiVertexInputAttribute::Float4, 2 * sizeof(float) } }),
        mSrb);
    if (!mPipeline)
        return false;

    mLastSampleCount = sampleCount;
    createImagePipeline(rpDesc, sampleCount);
    createDashedPipeline(rpDesc, sampleCount);
    return true;
}

void QCPPlottableRhiLayer::createImagePipeline(QRhiRenderPassDescriptor* rpDesc, int sampleCount)
{
    // Remembered even when this fails, like the dashed pipeline: ensurePipeline must not rebuild
    // all three every frame; fills then fall back to the layer's painter through canAddImage().
    mLastImageSampleCount = sampleCount;
    mImageBroken = true;

    if (!mImageSampler)
    {
        mImageSampler
            = mRhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                               QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
        if (!mImageSampler->create())
        {
            qDebug() << "Failed to create image sampler";
            delete mImageSampler;
            mImageSampler = nullptr;
            return;
        }
    }

    // kMaxImages slots, created once: a resized UBO would invalidate every image's SRB.
    mImageUniformBuffer = createUniformBuffer(mRhi, imageUbufStride() * kMaxImages,
                                              "Failed to create image uniform buffer");
    if (!mImageUniformBuffer)
        return;

    if (!mImageVertexBuffer)
    {
        // Unit quad: [0,1] positions with matching texcoords, stretched onto the entry's pixel
        // rect. Its contents go in with the first upload batch.
        mImageVertexBuffer = mRhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer,
                                             quint32(16 * sizeof(float)));
        if (!mImageVertexBuffer->create())
        {
            qDebug() << "Failed to create image quad buffer";
            delete mImageVertexBuffer;
            mImageVertexBuffer = nullptr;
            return;
        }
        mImageQuadUploaded = false;
    }

    // One layout SRB with no texture (the colormap pipeline's idiom): each image binds its own SRB.
    QRhiShaderResourceBindings* layoutSrb = qcp::rhi::createSrb(
        mRhi, mImageUniformBuffer, 1,
        QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
        "Failed to create image layout SRB", sizeof(ImageUniforms), nullptr, mImageSampler);
    if (!layoutSrb)
        return;

    // Vertex layout: (x, y) float2 position + (u, v) float2 texcoord
    mImagePipeline = qcp::rhi::createPipeline(
        mRhi, rpDesc, sampleCount, plottable_image_vert_qsb_data, plottable_image_vert_qsb_data_len,
        plottable_image_frag_qsb_data, plottable_image_frag_qsb_data_len,
        vertexLayoutOf(4,
                       { { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
                         { 0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float) } }),
        layoutSrb, QRhiGraphicsPipeline::TriangleStrip);
    delete layoutSrb;
    if (!mImagePipeline)
        return;
    mImageBroken = false;
}

bool QCPPlottableRhiLayer::ensureImageTexture(ImageTexture& texture, const QSize& size)
{
    if (size.isEmpty())
        return false;
    const int limit = mRhi->resourceLimit(QRhi::TextureSizeMax);
    if (size.width() > limit || size.height() > limit)
        return false; // Metal aborts on an oversized texture instead of failing the call

    if (texture.texture && texture.texture->pixelSize() != size)
    {
        delete texture.srb;
        texture.srb = nullptr;
        delete texture.texture;
        texture.texture = nullptr;
    }
    if (!texture.texture)
    {
        texture.texture = mRhi->newTexture(qcp::rhi::preferredTextureFormat(mRhi), size);
        if (!texture.texture->create())
        {
            qDebug() << "Failed to create fallback image texture";
            delete texture.texture;
            texture.texture = nullptr;
            return false;
        }
    }
    if (!texture.srb)
    {
        texture.srb = qcp::rhi::createSrb(
            mRhi, mImageUniformBuffer, 1,
            QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            "Failed to create fallback image SRB", sizeof(ImageUniforms), texture.texture,
            mImageSampler);
        if (!texture.srb)
            return false;
    }
    return true;
}

void QCPPlottableRhiLayer::createDashedPipeline(QRhiRenderPassDescriptor* rpDesc, int sampleCount)
{
    // Remembered even when this fails, so ensurePipeline does not rebuild both pipelines every
    // frame; dashed strokes then fall back to QPainter through dashedAvailable().
    mLastDashSampleCount = sampleCount;
    mDashBroken = true;

    const int stride = dashUbufStride();
    mDashUniformBufferSize = stride;
    mDashUniformBuffer = createUniformBuffer(mRhi, mDashUniformBufferSize,
                                             "Failed to create dashed plottable uniform buffer");
    if (!mDashUniformBuffer)
        return;

    mDashSrb = qcp::rhi::createSrb(
        mRhi, mDashUniformBuffer, 0,
        QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
        "Failed to create dashed plottable SRB", sizeof(DashUniforms));
    if (!mDashSrb)
        return;

    // Vertex layout: (x, y) float2 + (r, g, b, a) float4 + distance float
    mDashPipeline = qcp::rhi::createPipeline(
        mRhi, rpDesc, sampleCount, plottable_dashed_vert_qsb_data,
        plottable_dashed_vert_qsb_data_len, plottable_dashed_frag_qsb_data,
        plottable_dashed_frag_qsb_data_len,
        vertexLayoutOf(7,
                       { { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
                         { 0, 1, QRhiVertexInputAttribute::Float4, 2 * sizeof(float) },
                         { 0, 2, QRhiVertexInputAttribute::Float, 6 * sizeof(float) } }),
        mDashSrb);
    if (!mDashPipeline)
        return;
    mDashBroken = false;
}

void QCPPlottableRhiLayer::uploadResources(QRhiResourceUpdateBatch* updates,
                                           const QSize& outputSize, float dpr, bool isYUpInNDC)
{
    PROFILE_HERE_N("QCPPlottableRhiLayer::uploadResources");
    const float yFlip = isYUpInNDC ? -1.0f : 1.0f;

    uploadSolid(updates, outputSize, dpr, yFlip);
    uploadDashed(updates, outputSize, dpr, yFlip);
    uploadImages(updates, outputSize, dpr, yFlip);
    mDirty = false;
}

void QCPPlottableRhiLayer::uploadImages(QRhiResourceUpdateBatch* updates, const QSize& outputSize,
                                        float dpr, float yFlip)
{
    // Not gated on mImageEntries: the sweep below must run on the frame that draws the last image
    // away, or its texture stays on the GPU for the rest of the plot's life.
    if (!mImageUniformBuffer)
        return;

    if (!mImageEntries.isEmpty())
    {
        if (!mImageQuadUploaded && mImageVertexBuffer)
        {
            // Unit quad: [0,1] positions with matching texcoords, stretched onto the entry's pixel
            // rect.
            static const float quad[16] = { 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 1, 1, 1, 1 };
            updates->uploadStaticBuffer(mImageVertexBuffer, quad);
            mImageQuadUploaded = true;
        }

        const int stride = imageUbufStride();
        for (int i = 0; i < mImageEntries.size(); ++i)
        {
            const ImageEntry& entry = mImageEntries[i];
            const float dx = entry.offsetX * dpr;
            const float dy = entry.offsetY * dpr;
            const ImageUniforms params = { float(entry.pixelRect.left()) + dx,
                                           float(entry.pixelRect.top()) + dy,
                                           float(entry.pixelRect.right()) + dx,
                                           float(entry.pixelRect.bottom()) + dy,
                                           float(outputSize.width()),
                                           float(outputSize.height()),
                                           yFlip,
                                           entry.alpha };
            updates->updateDynamicBuffer(mImageUniformBuffer, i * stride, sizeof(params), &params);

            ImageTexture* texture = mImageTextures.value(entry.imageKey, nullptr);
            if (!texture)
                continue;
            texture->used = true;
            if (!ensureImageTexture(*texture, entry.deviceSize))
                continue;
            if (texture->pending.isNull())
                continue; // an unchanged replay after a pan: the texture is already on the GPU
            updates->uploadTexture(
                texture->texture,
                QRhiTextureUploadDescription(QRhiTextureUploadEntry(
                    0, 0,
                    QRhiTextureSubresourceUploadDescription(texture->pending.convertedTo(
                        qcp::rhi::imageFormatFor(texture->texture->format()))))));
            texture->pending = QImage();
            ++mImageUploads;
        }
    }

    // Drop the textures no entry uses any more: the recording that drew them is gone.
    for (auto it = mImageTextures.begin(); it != mImageTextures.end();)
    {
        ImageTexture* texture = it.value();
        if (!texture->used)
        {
            delete texture->srb;
            delete texture->texture;
            delete texture;
            it = mImageTextures.erase(it);
            continue;
        }
        texture->used = false;
        ++it;
    }
}

void QCPPlottableRhiLayer::uploadSolid(QRhiResourceUpdateBatch* updates, const QSize& outputSize,
                                       float dpr, float yFlip)
{
    if (mDrawEntries.isEmpty() || !mUniformBuffer)
        return;

    // Grow UBO if needed (one aligned slot per draw entry)
    const int stride = ubufStride();
    if (!growUniformBuffer(mUniformBuffer, mUniformBufferSize, stride * int(mDrawEntries.size()),
                           mSrb, "Failed to resize plottable uniform buffer"))
        return;

    // Upload per-draw uniforms (always — offsets change on pan frames)
    for (int i = 0; i < mDrawEntries.size(); ++i)
    {
        const auto& entry = mDrawEntries[i];
        PerDrawUniforms params = { float(outputSize.width()),
                                   float(outputSize.height()),
                                   yFlip,
                                   dpr,
                                   entry.offsetX,
                                   entry.offsetY,
                                   entry.alpha,
                                   0.0f };
        updates->updateDynamicBuffer(mUniformBuffer, i * stride, sizeof(params), &params);
    }

    if (mStagingSize == 0 || !mDirty)
        return; // a pan only moves the uniforms; the vertices are already on the GPU

    if (uploadVertices(mRhi, updates, mVertexBuffer, mVertexBufferSize, mStagingData, mStagingSize,
                       "Failed to create plottable vertex buffer"))
        ++mSolidVertexUploads;
}

void QCPPlottableRhiLayer::uploadDashed(QRhiResourceUpdateBatch* updates, const QSize& outputSize,
                                        float dpr, float yFlip)
{
    if (mDashEntries.isEmpty() || !mDashUniformBuffer)
        return;

    const int stride = dashUbufStride();
    if (!growUniformBuffer(mDashUniformBuffer, mDashUniformBufferSize,
                           stride * int(mDashEntries.size()), mDashSrb,
                           "Failed to resize dashed plottable uniform buffer"))
        return;

    for (int i = 0; i < mDashEntries.size(); ++i)
    {
        const auto& entry = mDashEntries[i];
        DashUniforms params = { };
        params.width = float(outputSize.width());
        params.height = float(outputSize.height());
        params.yFlip = yFlip;
        params.dpr = dpr;
        params.offsetX = entry.offsetX;
        params.offsetY = entry.offsetY;
        params.alpha = entry.alpha;
        params.dashOffset = entry.dashOffset;
        params.patternLength = entry.patternLength;
        params.patternCount = float(entry.patternCount);
        for (int p = 0; p < entry.patternCount; ++p)
            params.pattern[p / 4][p % 4] = entry.pattern[p];
        updates->updateDynamicBuffer(mDashUniformBuffer, i * stride, sizeof(params), &params);
    }

    if (mDashStagingSize == 0 || !mDirty)
        return; // as above: nothing moved but the offsets

    uploadVertices(mRhi, updates, mDashVertexBuffer, mDashVertexBufferSize, mDashStagingData,
                   mDashStagingSize, "Failed to create dashed plottable vertex buffer");
}

void QCPPlottableRhiLayer::render(QRhiCommandBuffer* cb, const QSize& outputSize)
{
    PROFILE_HERE_N("QCPPlottableRhiLayer::render");

    if (mDrawOrder.isEmpty())
        return;

    const QRhiViewport viewport(0, 0, float(outputSize.width()), float(outputSize.height()));

    // Everything one submission needs. A null pipeline means the entry cannot be drawn.
    struct DrawCall
    {
        QRhiGraphicsPipeline* pipeline = nullptr;
        QRhiShaderResourceBindings* srb = nullptr;
        QRhiBuffer* vertices = nullptr;
        QRect scissor;
        int firstVertex = 0;
        int vertexCount = 0;
        int uniformIndex = 0;
        int uniformStride = 0;
    };

    const auto callOf = [&](const OrderedDraw& record)
    {
        DrawCall call;
        if (record.kind == DrawKind::Dashed)
        {
            if (!mDashPipeline || !mDashVertexBuffer || !mDashSrb)
                return call;
            const DashedEntry& entry = mDashEntries[record.index];
            return DrawCall { mDashPipeline,     mDashSrb,           mDashVertexBuffer,
                              entry.scissorRect, entry.vertexOffset, entry.vertexCount,
                              record.index,      dashUbufStride() };
        }
        if (record.kind == DrawKind::Image)
        {
            if (!mImagePipeline || !mImageVertexBuffer)
                return call;
            const ImageEntry& entry = mImageEntries[record.index];
            const ImageTexture* texture = mImageTextures.value(entry.imageKey, nullptr);
            if (!texture || !texture->srb)
                return call;
            return DrawCall {
                mImagePipeline, texture->srb,     mImageVertexBuffer, entry.scissorRect, 0, 4,
                record.index,   imageUbufStride()
            };
        }
        if (!mPipeline || !mVertexBuffer || !mSrb)
            return call;
        const DrawEntry& entry = mDrawEntries[record.index];
        if (entry.vertexCount == 0)
            return call;
        return DrawCall { mPipeline,          mSrb,
                          mVertexBuffer,      entry.scissorRect,
                          entry.vertexOffset, entry.vertexCount,
                          record.index,       ubufStride() };
    };

    // Walk the submissions in order and rebind only when the pipeline actually changes: a solid,
    // dashed or image plottable keeps its relative stacking whatever its style looks like.
    const QRhiGraphicsPipeline* bound = nullptr;
    for (const OrderedDraw& record : mDrawOrder)
    {
        const DrawCall call = callOf(record);
        if (!call.pipeline)
            continue;
        if (bound != call.pipeline)
        {
            cb->setGraphicsPipeline(call.pipeline);
            cb->setViewport(viewport); // QRhi: the viewport belongs to the pipeline binding
            bound = call.pipeline;
        }
        // Bind per-draw uniform slot via dynamic offset.
        // setShaderResources must precede setVertexInput — on Metal, QRhi offsets
        // vertex buffer indices by the number of SRB buffer bindings.
        const QPair<int, quint32> dynamicOffset(0, quint32(call.uniformIndex * call.uniformStride));
        cb->setShaderResources(call.srb, 1, &dynamicOffset);
        const QRhiCommandBuffer::VertexInput input(call.vertices, 0);
        cb->setVertexInput(0, 1, &input);
        cb->setScissor(
            { call.scissor.x(), call.scissor.y(), call.scissor.width(), call.scissor.height() });
        // Fill and stroke are one contiguous range: the triangles do not care which is which, so
        // one draw covers both. The image quad is four vertices.
        cb->draw(call.vertexCount, 1, call.firstVertex, 0);
    }
}
