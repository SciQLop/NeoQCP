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
    delete mSrb;
    delete mSampler;
    delete mVertexBuffer;
    delete mTexture;
}

void QCPColormapRhiLayer::clear()
{
    mStagingImage = {};
    mContourUvVertices.clear();
    mLineVertexCount = 0;
}

void QCPColormapRhiLayer::invalidatePipeline()
{
    delete mLinePipeline; mLinePipeline = nullptr;
    delete mLineSrb;      mLineSrb = nullptr;
    delete mPipeline;     mPipeline = nullptr;
    delete mSrb;          mSrb = nullptr;
}

// ── CPU-side setters ────────────────────────────────────────────────────────

void QCPColormapRhiLayer::setImage(const QImage& image)
{
    mStagingImage = image;
    mTextureDirty = true;
}

void QCPColormapRhiLayer::setQuadRect(const QRectF& pixelRect)
{
    mQuadPixelRect = pixelRect;
    mGeometryDirty = true;
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

bool QCPColormapRhiLayer::ensureTexture(QRhiBuffer* compositeUbo)
{
    QSize imgSize = mStagingImage.size();
    bool texRecreated = false;
    if (!mTexture || mTextureSize != imgSize)
    {
        delete mTexture;
        mTexture = mRhi->newTexture(qcp::rhi::preferredTextureFormat(mRhi), imgSize);
        if (!mTexture->create()) { delete mTexture; mTexture = nullptr; return false; }
        mTextureSize = imgSize;
        mTextureDirty = true;
        texRecreated = true;
    }

    if ((!mSrb || texRecreated) && mTexture)
    {
        delete mSrb;
        mSrb = mRhi->newShaderResourceBindings();
        mSrb->setBindings({
            QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage, mTexture, mSampler),
            QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::VertexStage, compositeUbo)
        });
        if (!mSrb->create()) { delete mSrb; mSrb = nullptr; return false; }
    }

    return true;
}

// ── Quad geometry ───────────────────────────────────────────────────────────

void QCPColormapRhiLayer::updateQuadGeometry(QRhiResourceUpdateBatch* updates,
                                              const QSize& outputSize, float dpr,
                                              bool isYUpInNDC)
{
    if (!mGeometryDirty && mVertexBuffer && mLastOutputSize == outputSize)
        return;

    mLastOutputSize = outputSize;
    const float w = float(outputSize.width());
    const float h = float(outputSize.height());
    const float yFlip = isYUpInNDC ? -1.0f : 1.0f;

    auto toNDC = [&](float px, float py) -> std::pair<float, float> {
        return {(px * dpr / w) * 2.0f - 1.0f,
                yFlip * ((py * dpr / h) * 2.0f - 1.0f)};
    };

    auto [x0, y0] = toNDC(mQuadPixelRect.left(), mQuadPixelRect.top());
    auto [x1, y1] = toNDC(mQuadPixelRect.right(), mQuadPixelRect.bottom());

    // Store NDC bounds for contour line UBO
    mNdcX0 = x0; mNdcY0 = y0;
    mNdcX1 = x1; mNdcY1 = y1;
    mContourUboDirty = true;

    const float verts[] = {
        x0, y0, 0.0f, 0.0f,
        x1, y0, 1.0f, 0.0f,
        x0, y1, 0.0f, 1.0f,
        x1, y1, 1.0f, 1.0f,
    };

    if (!mVertexBuffer)
    {
        mVertexBuffer = mRhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, sizeof(verts));
        if (!mVertexBuffer->create()) { delete mVertexBuffer; mVertexBuffer = nullptr; return; }
    }

    updates->updateDynamicBuffer(mVertexBuffer, 0, sizeof(verts), verts);
    mGeometryDirty = false;
}

// ── Upload ──────────────────────────────────────────────────────────────────

void QCPColormapRhiLayer::uploadResources(QRhiResourceUpdateBatch* updates,
                                            const QSize& outputSize, float dpr,
                                            bool isYUpInNDC,
                                            QRhiBuffer* compositeUbo)
{
    PROFILE_HERE_N("QCPColormapRhiLayer::uploadResources");
    if (mStagingImage.isNull())
        return;

    if (!ensureTexture(compositeUbo))
        return;

    if (mTextureDirty && mTexture)
    {
        updates->uploadTexture(mTexture, QRhiTextureUploadDescription(
            QRhiTextureUploadEntry(0, 0, QRhiTextureSubresourceUploadDescription(mStagingImage))));
        mTextureDirty = false;
    }

    updateQuadGeometry(updates, outputSize, dpr, isYUpInNDC);

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
        ubo.ndcMin[0] = mNdcX0;
        ubo.ndcMin[1] = mNdcY0;
        ubo.ndcMax[0] = mNdcX1;
        ubo.ndcMax[1] = mNdcY1;
        updates->updateDynamicBuffer(mLineUbo, 0, sizeof(ubo), &ubo);
        mContourUboDirty = false;
    }
}

// ── Render ──────────────────────────────────────────────────────────────────

void QCPColormapRhiLayer::render(QRhiCommandBuffer* cb, const QSize& outputSize)
{
    PROFILE_HERE_N("QCPColormapRhiLayer::render");

    // Draw colormap quad
    if (mPipeline && mVertexBuffer && mSrb && mTexture)
    {
        cb->setGraphicsPipeline(mPipeline);
        cb->setViewport({0, 0, float(outputSize.width()), float(outputSize.height())});
        cb->setShaderResources(mSrb);

        const QRhiCommandBuffer::VertexInput vbufBinding(mVertexBuffer, 0);
        cb->setVertexInput(0, 1, &vbufBinding);
        cb->setScissor({mScissorRect.x(), mScissorRect.y(),
                        mScissorRect.width(), mScissorRect.height()});
        cb->draw(4);
    }

    // Draw contour lines on top
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
}
