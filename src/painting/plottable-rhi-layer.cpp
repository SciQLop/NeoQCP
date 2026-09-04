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
    std::free(mStagingData);
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
}

void QCPPlottableRhiLayer::clear()
{
    mStagingSize = 0;          // preserve allocation
    mDrawEntries.resize(0);    // preserve capacity
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
    // UBO needs re-upload but vertex data is unchanged
}

int QCPPlottableRhiLayer::ubufStride() const
{
    return mRhi->ubufAligned(sizeof(PerDrawUniforms));
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
                                    float offsetX, float offsetY)
{
    PROFILE_HERE_N("QCPPlottableRhiLayer::addPlottable");
    DrawEntry entry;
    entry.scissorRect = qcp::rhi::computeScissor(clipRect, dpr, outputHeight);
    entry.offsetX = offsetX;
    entry.offsetY = offsetY;

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
    mDirty = true;
}

bool QCPPlottableRhiLayer::ensurePipeline(QRhiRenderPassDescriptor* rpDesc,
                                           int sampleCount)
{
    PROFILE_HERE_N("QCPPlottableRhiLayer::ensurePipeline");
    if (mPipeline && mLastSampleCount == sampleCount)
        return true;

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
    return true;
}

void QCPPlottableRhiLayer::uploadResources(QRhiResourceUpdateBatch* updates,
                                            const QSize& outputSize, float dpr,
                                            bool isYUpInNDC)
{
    PROFILE_HERE_N("QCPPlottableRhiLayer::uploadResources");

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
            {0, 0}
        };
        updates->updateDynamicBuffer(mUniformBuffer, i * stride, sizeof(params), &params);
    }

    // Upload vertex data only when geometry changed
    if (!mDirty || mStagingSize == 0)
        return;

    const int requiredSize = mStagingSize * static_cast<int>(sizeof(float));

    if (!mVertexBuffer || mVertexBufferSize < requiredSize)
    {
        delete mVertexBuffer;
        mVertexBuffer = mRhi->newBuffer(QRhiBuffer::Dynamic,
                                         QRhiBuffer::VertexBuffer,
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
    mDirty = false;
}

void QCPPlottableRhiLayer::render(QRhiCommandBuffer* cb,
                                   const QSize& outputSize)
{
    PROFILE_HERE_N("QCPPlottableRhiLayer::render");

    if (!mPipeline || !mVertexBuffer || !mSrb || mDrawEntries.isEmpty())
        return;

    cb->setGraphicsPipeline(mPipeline);
    cb->setViewport({0, 0, float(outputSize.width()), float(outputSize.height())});

    const QRhiCommandBuffer::VertexInput vbufBinding(mVertexBuffer, 0);
    const int stride = ubufStride();
    for (int i = 0; i < mDrawEntries.size(); ++i)
    {
        const auto& entry = mDrawEntries[i];

        // Bind per-draw uniform slot via dynamic offset.
        // setShaderResources must precede setVertexInput — on Metal, QRhi offsets
        // vertex buffer indices by the number of SRB buffer bindings.
        const QPair<int, quint32> dynamicOffset(0, quint32(i * stride));
        cb->setShaderResources(mSrb, 1, &dynamicOffset);
        cb->setVertexInput(0, 1, &vbufBinding);

        cb->setScissor({entry.scissorRect.x(), entry.scissorRect.y(),
                        entry.scissorRect.width(), entry.scissorRect.height()});

        if (entry.fillVertexCount > 0)
            cb->draw(entry.fillVertexCount, 1, entry.fillOffset, 0);
        if (entry.strokeVertexCount > 0)
            cb->draw(entry.strokeVertexCount, 1, entry.strokeOffset, 0);
    }
}
