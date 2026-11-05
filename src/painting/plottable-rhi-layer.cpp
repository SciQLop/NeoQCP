#include "plottable-rhi-layer.h"
#include "rhi-utils.h"
#include "Profiling.hpp"
#include "embedded_shaders.h"
#include <cstring>

QCPPlottableRhiLayer::QCPPlottableRhiLayer(QRhi* rhi)
    : mRhi(rhi)
{
}

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
}

void QCPPlottableRhiLayer::clear()
{
    mStagingSize = 0;             // preserve allocation
    mDrawEntries.resize(0);       // preserve capacity
    mDashStagingSize = 0;
    mDashEntries.resize(0);
    mDrawOrder.resize(0);
    mDirty = true;
}

void QCPPlottableRhiLayer::setAllOffsets(float offsetX, float offsetY)
{
    mLastOffsetX = offsetX;
    mLastOffsetY = offsetY;
    for (auto& entry : mDrawEntries)
    {
        entry.offsetX = offsetX;
        entry.offsetY = offsetY;
    }
    for (auto& entry : mDashEntries)
    {
        entry.offsetX = offsetX;
        entry.offsetY = offsetY;
    }
    // UBO needs re-upload but vertex data is unchanged
}

int QCPPlottableRhiLayer::ubufStride() const
{
    return mRhi->ubufAligned(sizeof(PerDrawUniforms));
}

int QCPPlottableRhiLayer::dashUbufStride() const
{
    return mRhi->ubufAligned(sizeof(DashUniforms));
}

void QCPPlottableRhiLayer::dashStagingAppend(const float* src, int count)
{
    if (mDashStagingSize + count > mDashStagingCapacity)
    {
        mDashStagingCapacity = std::max(mDashStagingCapacity * 2, mDashStagingSize + count);
        mDashStagingData = static_cast<float*>(
            std::realloc(mDashStagingData, mDashStagingCapacity * sizeof(float)));
    }
    std::memcpy(mDashStagingData + mDashStagingSize, src, count * sizeof(float));
    mDashStagingSize += count;
}

void QCPPlottableRhiLayer::stagingAppend(const float* src, int count)
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

void
QCPPlottableRhiLayer::addPlottable(std::span<const float> fillVerts,
                                    std::span<const float> strokeVerts,
                                    const QRect& clipRect, double dpr,
                                    int outputHeight,
                                    float offsetX, float offsetY,
                                    float alpha)
{
    PROFILE_HERE_N("QCPPlottableRhiLayer::addPlottable");
    DrawEntry entry;
    entry.scissorRect = qcp::rhi::computeScissor(clipRect, dpr, outputHeight);
    entry.offsetX = offsetX;
    entry.offsetY = offsetY;
    entry.alpha = alpha;

    if (!fillVerts.empty())
    {
        entry.fillOffset = mStagingSize / 6;
        entry.fillVertexCount = static_cast<int>(fillVerts.size()) / 6;
        stagingAppend(fillVerts.data(), static_cast<int>(fillVerts.size()));
    }

    if (!strokeVerts.empty())
    {
        entry.strokeOffset = mStagingSize / 6;
        entry.strokeVertexCount = static_cast<int>(strokeVerts.size()) / 6;
        stagingAppend(strokeVerts.data(), static_cast<int>(strokeVerts.size()));
    }

    mDrawEntries.append(entry);
    mDrawOrder.append({false, int(mDrawEntries.size()) - 1});
    mDirty = true;
}

void QCPPlottableRhiLayer::addDashedPlottable(std::span<const float> strokeVerts,
                                              std::span<const float> dashPattern,
                                              float dashOffset, const QRect& clipRect, double dpr,
                                              int outputHeight, float offsetX, float offsetY,
                                              float alpha)
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
    dashStagingAppend(strokeVerts.data(), int(strokeVerts.size()));
    mDashEntries.append(entry);
    mDrawOrder.append({true, int(mDashEntries.size()) - 1});
    mDirty = true;
}

bool QCPPlottableRhiLayer::ensurePipeline(QRhiRenderPassDescriptor* rpDesc,
                                           int sampleCount)
{
    PROFILE_HERE_N("QCPPlottableRhiLayer::ensurePipeline");
    // A dashed pipeline that failed for this sample count is remembered: rebuilding it every frame
    // would rebuild the solid one too, and the dashed lines would never appear again.
    if (mPipeline && mLastSampleCount == sampleCount
        && (mDashPipeline || (mDashBroken && mLastDashSampleCount == sampleCount)))
        return mPipeline != nullptr;

    // Rebuild pipeline if sample count changed or first creation
    invalidatePipeline();

    // Load shaders from embedded data
    auto vertShader = qcp::rhi::loadEmbeddedShader(plottable_vert_qsb_data, plottable_vert_qsb_data_len);
    auto fragShader = qcp::rhi::loadEmbeddedShader(plottable_frag_qsb_data, plottable_frag_qsb_data_len);

    if (!vertShader.isValid() || !fragShader.isValid())
    {
        qDebug() << "Failed to load plottable shaders";
        return false;
    }

    // Create a small initial UBO — will be grown in uploadResources as needed.
    // Uses dynamic offsets: one PerDrawUniforms slot per draw entry.
    const int stride = ubufStride();
    mUniformBufferSize = stride; // start with 1 slot
    delete mUniformBuffer;
    mUniformBuffer = mRhi->newBuffer(QRhiBuffer::Dynamic,
                                      QRhiBuffer::UniformBuffer, mUniformBufferSize);
    if (!mUniformBuffer->create())
    {
        qDebug() << "Failed to create plottable uniform buffer";
        return false;
    }

    // SRB with dynamic offset: bind the full buffer, one slot at a time
    delete mSrb;
    mSrb = mRhi->newShaderResourceBindings();
    mSrb->setBindings({
        QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
            0, QRhiShaderResourceBinding::VertexStage, mUniformBuffer, sizeof(PerDrawUniforms))
    });
    if (!mSrb->create())
    {
        qDebug() << "Failed to create plottable SRB";
        return false;
    }

    mPipeline = mRhi->newGraphicsPipeline();
    mPipeline->setShaderStages({
        {QRhiShaderStage::Vertex, vertShader},
        {QRhiShaderStage::Fragment, fragShader}
    });

    // Vertex layout: (x, y) float2 + (r, g, b, a) float4
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({{6 * sizeof(float)}});
    inputLayout.setAttributes({
        {0, 0, QRhiVertexInputAttribute::Float2, 0},
        {0, 1, QRhiVertexInputAttribute::Float4, 2 * sizeof(float)}
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
        qDebug() << "Failed to create plottable pipeline";
        delete mPipeline;
        mPipeline = nullptr;
        return false;
    }

    mLastSampleCount = sampleCount;
    createDashedPipeline(rpDesc, sampleCount);
    return true;
}

void QCPPlottableRhiLayer::createDashedPipeline(QRhiRenderPassDescriptor* rpDesc,
                                                 int sampleCount)
{
    // Remembered even when this fails, so ensurePipeline does not rebuild both pipelines every
    // frame; dashed strokes then fall back to QPainter through dashedAvailable().
    mLastDashSampleCount = sampleCount;
    mDashBroken = true;
    auto vertShader = qcp::rhi::loadEmbeddedShader(plottable_dashed_vert_qsb_data,
                                                   plottable_dashed_vert_qsb_data_len);
    auto fragShader = qcp::rhi::loadEmbeddedShader(plottable_dashed_frag_qsb_data,
                                                   plottable_dashed_frag_qsb_data_len);
    if (!vertShader.isValid() || !fragShader.isValid())
    {
        qDebug() << "Failed to load dashed plottable shaders";
        return;
    }

    const int stride = dashUbufStride();
    mDashUniformBufferSize = stride;
    mDashUniformBuffer = mRhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                         mDashUniformBufferSize);
    if (!mDashUniformBuffer->create())
    {
        qDebug() << "Failed to create dashed plottable uniform buffer";
        return;
    }

    mDashSrb = mRhi->newShaderResourceBindings();
    mDashSrb->setBindings({
        QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            mDashUniformBuffer, sizeof(DashUniforms))
    });
    if (!mDashSrb->create())
    {
        qDebug() << "Failed to create dashed plottable SRB";
        return;
    }

    mDashPipeline = mRhi->newGraphicsPipeline();
    mDashPipeline->setShaderStages({
        {QRhiShaderStage::Vertex, vertShader},
        {QRhiShaderStage::Fragment, fragShader}
    });

    // Vertex layout: (x, y) float2 + (r, g, b, a) float4 + distance float
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({{7 * sizeof(float)}});
    inputLayout.setAttributes({
        {0, 0, QRhiVertexInputAttribute::Float2, 0},
        {0, 1, QRhiVertexInputAttribute::Float4, 2 * sizeof(float)},
        {0, 2, QRhiVertexInputAttribute::Float, 6 * sizeof(float)}
    });
    mDashPipeline->setVertexInputLayout(inputLayout);
    mDashPipeline->setTargetBlends({qcp::rhi::premultipliedAlphaBlend()});
    mDashPipeline->setFlags(QRhiGraphicsPipeline::UsesScissor);
    mDashPipeline->setTopology(QRhiGraphicsPipeline::Triangles);
    mDashPipeline->setSampleCount(sampleCount);
    mDashPipeline->setRenderPassDescriptor(rpDesc);
    mDashPipeline->setShaderResourceBindings(mDashSrb);

    if (!mDashPipeline->create())
    {
        qDebug() << "Failed to create dashed plottable pipeline";
        delete mDashPipeline;
        mDashPipeline = nullptr;
        return;
    }
    mDashBroken = false;
}

void QCPPlottableRhiLayer::uploadResources(QRhiResourceUpdateBatch* updates,
                                            const QSize& outputSize, float dpr,
                                            bool isYUpInNDC)
{
    PROFILE_HERE_N("QCPPlottableRhiLayer::uploadResources");
    const float yFlip = isYUpInNDC ? -1.0f : 1.0f;

    uploadSolid(updates, outputSize, dpr, yFlip);
    uploadDashed(updates, outputSize, dpr, yFlip);
    mDirty = false;
}

void QCPPlottableRhiLayer::uploadSolid(QRhiResourceUpdateBatch* updates, const QSize& outputSize,
                                       float dpr, float yFlip)
{
    if (mDrawEntries.isEmpty() || !mUniformBuffer)
        return;

    // Grow UBO if needed (one aligned slot per draw entry)
    const int stride = ubufStride();
    const int requiredUboSize = stride * mDrawEntries.size();
    if (requiredUboSize > mUniformBufferSize)
    {
        mUniformBufferSize = requiredUboSize;
        mUniformBuffer->setSize(mUniformBufferSize);
        if (!mUniformBuffer->create())
        {
            qDebug() << "Failed to resize plottable uniform buffer";
            return;
        }
        // SRB must be rebuilt after UBO resize
        if (mSrb)
            mSrb->create();
    }

    // Upload per-draw uniforms (always — offsets change on pan frames)
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
            entry.alpha,
            0.0f
        };
        updates->updateDynamicBuffer(mUniformBuffer, i * stride, sizeof(params), &params);
    }

    if (mStagingSize == 0 || !mDirty)
        return; // a pan only moves the uniforms; the vertices are already on the GPU

    const int requiredSize = mStagingSize * static_cast<int>(sizeof(float));
    if (!mVertexBuffer || mVertexBufferSize < requiredSize)
    {
        delete mVertexBuffer;
        mVertexBuffer = mRhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                                         requiredSize);
        if (!mVertexBuffer->create())
        {
            qDebug() << "Failed to create plottable vertex buffer";
            delete mVertexBuffer;
            mVertexBuffer = nullptr;
            mVertexBufferSize = 0;
            return;
        }
        mVertexBufferSize = requiredSize;
    }
    updates->updateDynamicBuffer(mVertexBuffer, 0, requiredSize, mStagingData);
    ++mSolidVertexUploads;
}

void QCPPlottableRhiLayer::uploadDashed(QRhiResourceUpdateBatch* updates, const QSize& outputSize,
                                        float dpr, float yFlip)
{
    if (mDashEntries.isEmpty() || !mDashUniformBuffer)
        return;

    const int stride = dashUbufStride();
    const int requiredUboSize = stride * mDashEntries.size();
    if (requiredUboSize > mDashUniformBufferSize)
    {
        mDashUniformBufferSize = requiredUboSize;
        mDashUniformBuffer->setSize(mDashUniformBufferSize);
        if (!mDashUniformBuffer->create())
        {
            qDebug() << "Failed to resize dashed plottable uniform buffer";
            return;
        }
        if (mDashSrb)
            mDashSrb->create();
    }

    for (int i = 0; i < mDashEntries.size(); ++i)
    {
        const auto& entry = mDashEntries[i];
        DashUniforms params = {};
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

    const int requiredSize = mDashStagingSize * static_cast<int>(sizeof(float));
    if (!mDashVertexBuffer || mDashVertexBufferSize < requiredSize)
    {
        delete mDashVertexBuffer;
        mDashVertexBuffer = mRhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                                            requiredSize);
        if (!mDashVertexBuffer->create())
        {
            qDebug() << "Failed to create dashed plottable vertex buffer";
            delete mDashVertexBuffer;
            mDashVertexBuffer = nullptr;
            mDashVertexBufferSize = 0;
            return;
        }
        mDashVertexBufferSize = requiredSize;
    }
    updates->updateDynamicBuffer(mDashVertexBuffer, 0, requiredSize, mDashStagingData);
    ++mDashVertexUploads;
}

void QCPPlottableRhiLayer::render(QRhiCommandBuffer* cb,
                                   const QSize& outputSize)
{
    PROFILE_HERE_N("QCPPlottableRhiLayer::render");

    if (mDrawOrder.isEmpty())
        return;

    const QRhiViewport viewport(0, 0, float(outputSize.width()), float(outputSize.height()));

    // Walk the submissions in order and rebind only when the pipeline actually changes: a solid
    // and a dashed plottable keep their relative stacking whatever their pens look like.
    enum class Bound { None, Solid, Dashed } bound = Bound::None;
    const QRhiCommandBuffer::VertexInput solidBinding(mVertexBuffer, 0);
    const QRhiCommandBuffer::VertexInput dashBinding(mDashVertexBuffer, 0);
    for (const OrderedDraw& record : mDrawOrder)
    {
        if (record.dashed)
        {
            if (!mDashPipeline || !mDashVertexBuffer || !mDashSrb)
                continue;
            if (bound != Bound::Dashed)
            {
                cb->setGraphicsPipeline(mDashPipeline);
                cb->setViewport(viewport); // QRhi: the viewport belongs to the pipeline binding
                bound = Bound::Dashed;
            }
            const auto& entry = mDashEntries[record.index];
            const QPair<int, quint32> dynamicOffset(0, quint32(record.index * dashUbufStride()));
            cb->setShaderResources(mDashSrb, 1, &dynamicOffset);
            cb->setVertexInput(0, 1, &dashBinding);
            cb->setScissor({entry.scissorRect.x(), entry.scissorRect.y(),
                            entry.scissorRect.width(), entry.scissorRect.height()});
            cb->draw(entry.vertexCount, 1, entry.vertexOffset, 0);
            continue;
        }

        if (!mPipeline || !mVertexBuffer || !mSrb)
            continue;
        if (bound != Bound::Solid)
        {
            cb->setGraphicsPipeline(mPipeline);
            cb->setViewport(viewport);
            bound = Bound::Solid;
        }
        const auto& entry = mDrawEntries[record.index];
        // Bind per-draw uniform slot via dynamic offset.
        // setShaderResources must precede setVertexInput — on Metal, QRhi offsets
        // vertex buffer indices by the number of SRB buffer bindings.
        const QPair<int, quint32> dynamicOffset(0, quint32(record.index * ubufStride()));
        cb->setShaderResources(mSrb, 1, &dynamicOffset);
        cb->setVertexInput(0, 1, &solidBinding);
        cb->setScissor({entry.scissorRect.x(), entry.scissorRect.y(),
                        entry.scissorRect.width(), entry.scissorRect.height()});
        if (entry.fillVertexCount > 0)
            cb->draw(entry.fillVertexCount, 1, entry.fillOffset, 0);
        if (entry.strokeVertexCount > 0)
            cb->draw(entry.strokeVertexCount, 1, entry.strokeOffset, 0);
    }
}

