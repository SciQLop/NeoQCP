#include "colormap-rhi-layer.h"
#include "embedded_shaders.h"
#include "rhi-utils.h"
#include "Profiling.hpp"
#include "../layer.h"
#include <cstring>

bool QCPColormapRhiLayer::ownerVisible() const
{
    return !mOwner || mOwner->realVisibility();
}

// ── Contour line UBO (std140, 32 bytes) ────────────────────────────────────
// Must match contour_line.vert/frag ContourLineParams exactly.
struct alignas(16) ContourLineUbo {
    float lineColor[4];   // offset  0: vec4 (premultiplied RGBA)
    float ndcMin[2];      // offset 16: vec2
    float ndcMax[2];      // offset 24: vec2
};                        // total: 32
static_assert(sizeof(ContourLineUbo) == 32);

// ── Lifecycle ───────────────────────────────────────────────────────────────

QCPColormapRhiLayer::QCPColormapRhiLayer(QRhi* rhi)
    : mRhi(rhi)
{
}

QCPColormapRhiLayer::~QCPColormapRhiLayer()
{
    delete mLinePipeline;
    delete mLineSrb;
    delete mLineUbo;
    delete mLineVbo;
    delete mPipeline;
    delete mSampler;
    releaseQuad(mImage);
    releaseQuad(mOverlay);
}

void QCPColormapRhiLayer::releaseQuad(Quad& quad)
{
    delete quad.srb;          quad.srb = nullptr;
    delete quad.vertexBuffer; quad.vertexBuffer = nullptr;
    delete quad.texture;      quad.texture = nullptr;
}

void QCPColormapRhiLayer::clear()
{
    mImage.staging = {};
    mOverlay.staging = {};
    mContourUvVertices.clear();
    mLineVertexCount = 0;
}

void QCPColormapRhiLayer::invalidatePipeline()
{
    delete mLinePipeline; mLinePipeline = nullptr;
    delete mLineSrb;      mLineSrb = nullptr;
    delete mPipeline;     mPipeline = nullptr;
    delete mImage.srb;    mImage.srb = nullptr;
    delete mOverlay.srb;  mOverlay.srb = nullptr;
}

// ── CPU-side setters ────────────────────────────────────────────────────────

void QCPColormapRhiLayer::setImage(const QImage& image)
{
    // draw() calls setImage() every replot regardless of whether the
    // resampled content actually changed (e.g. interactive zoom while the
    // pipeline is still busy re-baking). cacheKey() identifies the QImage's
    // backing data, so an unchanged image (same data, shallow-copied through
    // flippedMapImage's COW) is detected without a pixel comparison.
    if (image.cacheKey() == mImage.staging.cacheKey() && image.size() == mImage.staging.size())
        return;
    mImage.staging = image;
    mImage.textureDirty = true;
}

void QCPColormapRhiLayer::setQuadRect(const QRectF& pixelRect)
{
    mImage.pixelRect = pixelRect;
    mImage.geometryDirty = true;
    mContourUboDirty = true;
}

void QCPColormapRhiLayer::setOverlay(const QImage& image, const QRectF& pixelRect)
{
    mOverlay.staging = image;
    mOverlay.textureDirty = true;
    mOverlay.pixelRect = pixelRect;
    mOverlay.geometryDirty = true;
}

void QCPColormapRhiLayer::clearOverlay()
{
    mOverlay.staging = {};
}

void QCPColormapRhiLayer::setPixelOffset(float offsetX, float offsetY)
{
    if (mOffsetX == offsetX && mOffsetY == offsetY)
        return;
    mOffsetX = offsetX;
    mOffsetY = offsetY;
    mImage.geometryDirty = true;
    mOverlay.geometryDirty = true;
    mContourUboDirty = true;
}

void QCPColormapRhiLayer::setScissorRect(const QRect& scissor)
{
    mScissorRect = scissor;
}

void QCPColormapRhiLayer::setContourLines(QVector<float> uvVertices, const QColor& color)
{
    mContourUvVertices = std::move(uvVertices);
    mContourColor = color;
    mContourLinesDirty = true;
    mContourUboDirty = true;
}

void QCPColormapRhiLayer::clearContourLines()
{
    mContourUvVertices.clear();
    mLineVertexCount = 0;
    mContourLinesDirty = true;
}

// ── Pipeline creation ───────────────────────────────────────────────────────

static QRhiGraphicsPipeline* buildPipeline(
    QRhi* rhi, const QRhiShaderStage& vert, const QRhiShaderStage& frag,
    QRhiShaderResourceBindings* layoutSrb,
    QRhiRenderPassDescriptor* rpDesc, int sampleCount,
    QRhiGraphicsPipeline::Topology topology,
    const QRhiVertexInputLayout& inputLayout)
{
    auto* pl = rhi->newGraphicsPipeline();

    pl->setShaderStages({vert, frag});
    pl->setVertexInputLayout(inputLayout);
    pl->setTargetBlends({qcp::rhi::premultipliedAlphaBlend()});
    pl->setFlags(QRhiGraphicsPipeline::UsesScissor);
    pl->setTopology(topology);
    pl->setSampleCount(sampleCount);
    pl->setRenderPassDescriptor(rpDesc);
    pl->setShaderResourceBindings(layoutSrb);

    if (!pl->create())
    {
        delete pl;
        return nullptr;
    }
    return pl;
}

bool QCPColormapRhiLayer::ensurePipeline(QRhiRenderPassDescriptor* rpDesc,
                                          int sampleCount,
                                          QRhiBuffer* compositeUbo)
{
    PROFILE_HERE_N("QCPColormapRhiLayer::ensurePipeline");
    if (mPipeline && mLastSampleCount == sampleCount)
        return true;

    invalidatePipeline();
    mLastSampleCount = sampleCount;

    if (!mSampler)
    {
        mSampler = mRhi->newSampler(QRhiSampler::Nearest, QRhiSampler::Nearest,
                                     QRhiSampler::None,
                                     QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
        if (!mSampler->create()) { delete mSampler; mSampler = nullptr; return false; }
    }

    auto compositeVert = qcp::rhi::loadEmbeddedShader(composite_vert_qsb_data, composite_vert_qsb_data_len);
    auto compositeFrag = qcp::rhi::loadEmbeddedShader(composite_frag_qsb_data, composite_frag_qsb_data_len);
    if (!compositeVert.isValid() || !compositeFrag.isValid())
        return false;

    // Base colormap pipeline
    {
        QRhiVertexInputLayout inputLayout;
        inputLayout.setBindings({{4 * sizeof(float)}});
        inputLayout.setAttributes({
            {0, 0, QRhiVertexInputAttribute::Float2, 0},
            {0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float)}
        });

        auto* layoutSrb = mRhi->newShaderResourceBindings();
        layoutSrb->setBindings({
            QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage, nullptr, mSampler),
            QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::VertexStage, compositeUbo)
        });
        if (!layoutSrb->create()) { delete layoutSrb; return false; }
        mPipeline = buildPipeline(mRhi, {QRhiShaderStage::Vertex, compositeVert},
                                  {QRhiShaderStage::Fragment, compositeFrag},
                                  layoutSrb, rpDesc, sampleCount,
                                  QRhiGraphicsPipeline::TriangleStrip, inputLayout);
        delete layoutSrb;
        if (!mPipeline) return false;
    }

    // Contour line pipeline
    {
        auto lineVert = qcp::rhi::loadEmbeddedShader(contour_line_vert_qsb_data, contour_line_vert_qsb_data_len);
        auto lineFrag = qcp::rhi::loadEmbeddedShader(contour_line_frag_qsb_data, contour_line_frag_qsb_data_len);
        if (!lineVert.isValid() || !lineFrag.isValid())
            return true;

        if (!mLineUbo)
        {
            mLineUbo = mRhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                        sizeof(ContourLineUbo));
            if (!mLineUbo->create()) { delete mLineUbo; mLineUbo = nullptr; return true; }
            mContourUboDirty = true;
        }

        QRhiVertexInputLayout inputLayout;
        inputLayout.setBindings({{2 * sizeof(float)}});
        inputLayout.setAttributes({
            {0, 0, QRhiVertexInputAttribute::Float2, 0}
        });

        auto* layoutSrb = mRhi->newShaderResourceBindings();
        layoutSrb->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, mLineUbo)
        });
        if (!layoutSrb->create()) { delete layoutSrb; return true; }

        mLinePipeline = buildPipeline(mRhi, {QRhiShaderStage::Vertex, lineVert},
                                      {QRhiShaderStage::Fragment, lineFrag},
                                      layoutSrb, rpDesc, sampleCount,
                                      QRhiGraphicsPipeline::Lines, inputLayout);

        // Create real SRB for draw calls
        mLineSrb = mRhi->newShaderResourceBindings();
        mLineSrb->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, mLineUbo)
        });
        mLineSrb->create();

        delete layoutSrb;
    }

    return true;
}

// ── Texture management ──────────────────────────────────────────────────────

bool QCPColormapRhiLayer::fitsInTexture(const QSize& imgSize) const
{
    const int maxTexSize = mRhi->resourceLimit(QRhi::TextureSizeMax);
    return imgSize.width() > 0 && imgSize.height() > 0
        && imgSize.width() <= maxTexSize && imgSize.height() <= maxTexSize;
}

bool QCPColormapRhiLayer::ensureTexture(Quad& quad, QRhiBuffer* compositeUbo)
{
    const QSize imgSize = quad.staging.size();
    // QRhiTexture::create() is documented to return false on an invalid descriptor,
    // but the Metal backend does not: an oversized descriptor aborts inside Metal's
    // own validation instead. Refuse before ever calling newTexture(), so the one
    // dimension the resample clamp can push past the backend's limit (a wide/tall
    // colormap panel, see QCPColorMap2::installResampleTransform) degrades to "no
    // GPU quad this frame" rather than crashing the process.
    if (!fitsInTexture(imgSize))
        return false;

    bool texRecreated = false;
    if (!quad.texture || quad.textureSize != imgSize)
    {
        delete quad.texture;
        quad.texture = mRhi->newTexture(qcp::rhi::preferredTextureFormat(mRhi), imgSize);
        if (!quad.texture->create()) { delete quad.texture; quad.texture = nullptr; return false; }
        quad.textureSize = imgSize;
        quad.textureDirty = true;
        texRecreated = true;
    }

    if (!quad.srb || texRecreated)
    {
        delete quad.srb;
        quad.srb = mRhi->newShaderResourceBindings();
        quad.srb->setBindings({
            QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage, quad.texture, mSampler),
            QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::VertexStage, compositeUbo)
        });
        if (!quad.srb->create()) { delete quad.srb; quad.srb = nullptr; return false; }
    }

    return true;
}

// ── Quad geometry ───────────────────────────────────────────────────────────

void QCPColormapRhiLayer::updateQuadGeometry(Quad& quad, QRhiResourceUpdateBatch* updates,
                                              const QSize& outputSize, float dpr,
                                              bool isYUpInNDC)
{
    if (!quad.geometryDirty && quad.vertexBuffer && quad.lastOutputSize == outputSize)
        return;

    quad.lastOutputSize = outputSize;
    const float w = float(outputSize.width());
    const float h = float(outputSize.height());
    const float yFlip = isYUpInNDC ? -1.0f : 1.0f;

    auto toNDC = [&](float px, float py) -> std::pair<float, float> {
        return {(px * dpr / w) * 2.0f - 1.0f,
                yFlip * ((py * dpr / h) * 2.0f - 1.0f)};
    };

    const QRectF rect = quad.pixelRect.translated(mOffsetX, mOffsetY);
    auto [x0, y0] = toNDC(rect.left(), rect.top());
    auto [x1, y1] = toNDC(rect.right(), rect.bottom());
    quad.ndcX0 = x0; quad.ndcY0 = y0;
    quad.ndcX1 = x1; quad.ndcY1 = y1;

    const float verts[] = {
        x0, y0, 0.0f, 0.0f,
        x1, y0, 1.0f, 0.0f,
        x0, y1, 0.0f, 1.0f,
        x1, y1, 1.0f, 1.0f,
    };

    if (!quad.vertexBuffer)
    {
        quad.vertexBuffer = mRhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, sizeof(verts));
        if (!quad.vertexBuffer->create()) { delete quad.vertexBuffer; quad.vertexBuffer = nullptr; return; }
    }

    updates->updateDynamicBuffer(quad.vertexBuffer, 0, sizeof(verts), verts);
    quad.geometryDirty = false;
}

void QCPColormapRhiLayer::uploadQuad(Quad& quad, QRhiResourceUpdateBatch* updates,
                                     const QSize& outputSize, float dpr, bool isYUpInNDC,
                                     QRhiBuffer* compositeUbo)
{
    if (quad.staging.isNull() || !ensureTexture(quad, compositeUbo))
        return;
    if (quad.textureDirty)
    {
        updates->uploadTexture(quad.texture, QRhiTextureUploadDescription(
            QRhiTextureUploadEntry(0, 0, QRhiTextureSubresourceUploadDescription(
                quad.staging.convertedTo(qcp::rhi::imageFormatFor(quad.texture->format()))))));
        quad.textureDirty = false;
    }
    updateQuadGeometry(quad, updates, outputSize, dpr, isYUpInNDC);
}

// ── Upload ──────────────────────────────────────────────────────────────────

void QCPColormapRhiLayer::uploadResources(QRhiResourceUpdateBatch* updates,
                                            const QSize& outputSize, float dpr,
                                            bool isYUpInNDC,
                                            QRhiBuffer* compositeUbo)
{
    PROFILE_HERE_N("QCPColormapRhiLayer::uploadResources");
    if (mImage.staging.isNull())
        return;

    const float oldNdc[] = { mImage.ndcX0, mImage.ndcY0, mImage.ndcX1, mImage.ndcY1 };
    uploadQuad(mImage, updates, outputSize, dpr, isYUpInNDC, compositeUbo);
    if (!mImage.texture)
        return;
    if (std::memcmp(oldNdc, &mImage.ndcX0, sizeof(oldNdc)) != 0)
        mContourUboDirty = true;
    uploadQuad(mOverlay, updates, outputSize, dpr, isYUpInNDC, compositeUbo);

    // Contour line vertex buffer
    if (mContourLinesDirty && mLinePipeline)
    {
        mLineVertexCount = mContourUvVertices.size() / 2;
        if (mLineVertexCount > 0)
        {
            int byteSize = mContourUvVertices.size() * int(sizeof(float));
            if (!mLineVbo || mLineVboSize < byteSize)
            {
                delete mLineVbo;
                mLineVboSize = byteSize;
                mLineVbo = mRhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, mLineVboSize);
                if (!mLineVbo->create()) { delete mLineVbo; mLineVbo = nullptr; mLineVertexCount = 0; return; }
            }
            updates->updateDynamicBuffer(mLineVbo, 0, byteSize, mContourUvVertices.constData());
        }
        mContourLinesDirty = false;
    }

    // Contour line UBO (quad NDC bounds + color)
    if (mContourUboDirty && mLineUbo)
    {
        ContourLineUbo ubo{};
        float a = float(mContourColor.alphaF());
        ubo.lineColor[0] = float(mContourColor.redF()) * a;
        ubo.lineColor[1] = float(mContourColor.greenF()) * a;
        ubo.lineColor[2] = float(mContourColor.blueF()) * a;
        ubo.lineColor[3] = a;
        ubo.ndcMin[0] = mImage.ndcX0;
        ubo.ndcMin[1] = mImage.ndcY0;
        ubo.ndcMax[0] = mImage.ndcX1;
        ubo.ndcMax[1] = mImage.ndcY1;
        updates->updateDynamicBuffer(mLineUbo, 0, sizeof(ubo), &ubo);
        mContourUboDirty = false;
    }
}

// ── Render ──────────────────────────────────────────────────────────────────

void QCPColormapRhiLayer::drawQuad(const Quad& quad, QRhiCommandBuffer* cb, const QSize& outputSize)
{
    if (!mPipeline || !quad.vertexBuffer || !quad.srb || !quad.texture || quad.staging.isNull())
        return;
    cb->setGraphicsPipeline(mPipeline);
    cb->setViewport({0, 0, float(outputSize.width()), float(outputSize.height())});
    cb->setShaderResources(quad.srb);
    const QRhiCommandBuffer::VertexInput vbufBinding(quad.vertexBuffer, 0);
    cb->setVertexInput(0, 1, &vbufBinding);
    cb->setScissor({mScissorRect.x(), mScissorRect.y(),
                    mScissorRect.width(), mScissorRect.height()});
    cb->draw(4);
}

void QCPColormapRhiLayer::render(QRhiCommandBuffer* cb, const QSize& outputSize)
{
    PROFILE_HERE_N("QCPColormapRhiLayer::render");

    drawQuad(mImage, cb, outputSize);

    // Contour lines over the image
    if (mLinePipeline && mLineVbo && mLineSrb && mLineVertexCount > 0)
    {
        cb->setGraphicsPipeline(mLinePipeline);
        cb->setViewport({0, 0, float(outputSize.width()), float(outputSize.height())});
        cb->setShaderResources(mLineSrb);

        const QRhiCommandBuffer::VertexInput vbufBinding(mLineVbo, 0);
        cb->setVertexInput(0, 1, &vbufBinding);
        cb->setScissor({mScissorRect.x(), mScissorRect.y(),
                        mScissorRect.width(), mScissorRect.height()});
        cb->draw(mLineVertexCount);
    }

    // Labels over the lines
    drawQuad(mOverlay, cb, outputSize);
}
