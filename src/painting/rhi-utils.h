#pragma once

#include <QColor>
#include <QDebug>
#include <QImage>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <rhi/qrhi.h>

namespace qcp::rhi {

//! \a count floats of \a src at the end of a layer's staging buffer (\a data holding \a size
//! floats in \a capacity), growing it geometrically: the buffer is raw floats, one per component.
inline void appendFloats(float*& data, int& size, int& capacity, const float* src, int count)
{
    if (count <= 0)
        return;
    if (size + count > capacity)
    {
        capacity = std::max(capacity * 2, size + count);
        data = static_cast<float*>(std::realloc(data, capacity * sizeof(float)));
    }
    std::memcpy(data + size, src, count * sizeof(float));
    size += count;
}

inline std::array<float, 4> premultipliedColor(const QColor& c)
{
    float a = float(c.alphaF());
    return {float(c.redF()) * a, float(c.greenF()) * a, float(c.blueF()) * a, a};
}

inline QRhiTexture::Format preferredTextureFormat(QRhi* rhi)
{
    return rhi->isTextureFormatSupported(QRhiTexture::BGRA8)
        ? QRhiTexture::BGRA8
        : QRhiTexture::RGBA8;
}

// The QImage layout whose bytes a texture of this format reads in the right channel order.
// WebGL has no BGRA8, so there QPainter's native ARGB32 bytes would come out red/blue swapped.
inline QImage::Format imageFormatFor(QRhiTexture::Format textureFormat)
{
    return textureFormat == QRhiTexture::BGRA8 ? QImage::Format_ARGB32_Premultiplied
                                               : QImage::Format_RGBA8888_Premultiplied;
}

inline QShader loadEmbeddedShader(const unsigned char* data, unsigned int length)
{
    return QShader::fromSerialized(QByteArray::fromRawData(
        reinterpret_cast<const char*>(data), length));
}

inline QRhiGraphicsPipeline::TargetBlend premultipliedAlphaBlend()
{
    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable = true;
    blend.srcColor = QRhiGraphicsPipeline::One;
    blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    blend.srcAlpha = QRhiGraphicsPipeline::One;
    blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    return blend;
}

// One graphics pipeline. Every layer builds the same object and only picks its shaders, vertex
// layout, topology and bindings, so the flags and blend live here.
inline QRhiGraphicsPipeline* createPipeline(QRhi* rhi, QRhiRenderPassDescriptor* rpDesc,
                                            int sampleCount, const QShader& vert,
                                            const QShader& frag,
                                            const QRhiVertexInputLayout& layout,
                                            QRhiShaderResourceBindings* srb,
                                            QRhiGraphicsPipeline::Topology topology
                                                = QRhiGraphicsPipeline::Triangles)
{
    if (!vert.isValid() || !frag.isValid())
        return nullptr;

    QRhiGraphicsPipeline* pipeline = rhi->newGraphicsPipeline();
    pipeline->setShaderStages({ { QRhiShaderStage::Vertex, vert },
                                { QRhiShaderStage::Fragment, frag } });
    pipeline->setVertexInputLayout(layout);
    pipeline->setTargetBlends({ premultipliedAlphaBlend() });
    pipeline->setFlags(QRhiGraphicsPipeline::UsesScissor);
    pipeline->setTopology(topology);
    pipeline->setSampleCount(sampleCount);
    pipeline->setRenderPassDescriptor(rpDesc);
    pipeline->setShaderResourceBindings(srb);
    if (!pipeline->create())
    {
        delete pipeline;
        return nullptr;
    }
    return pipeline;
}

inline QRhiGraphicsPipeline* createPipeline(QRhi* rhi, QRhiRenderPassDescriptor* rpDesc,
                                            int sampleCount, const unsigned char* vertData,
                                            unsigned int vertLength, const unsigned char* fragData,
                                            unsigned int fragLength,
                                            const QRhiVertexInputLayout& layout,
                                            QRhiShaderResourceBindings* srb,
                                            QRhiGraphicsPipeline::Topology topology
                                                = QRhiGraphicsPipeline::Triangles)
{
    return createPipeline(rhi, rpDesc, sampleCount, loadEmbeddedShader(vertData, vertLength),
                          loadEmbeddedShader(fragData, fragLength), layout, srb, topology);
}

// The shader-resource-bindings object every layer creates, from explicit bindings (a layer that
// binds more than one texture). Returns nullptr and reports \a failure when creation fails; the
// caller owns it.
inline QRhiShaderResourceBindings*
createSrb(QRhi* rhi, const QList<QRhiShaderResourceBinding>& bindings, const char* failure)
{
    QRhiShaderResourceBindings* srb = rhi->newShaderResourceBindings();
    srb->setBindings(bindings.cbegin(), bindings.cend());
    if (!srb->create())
    {
        qDebug() << failure;
        delete srb;
        return nullptr;
    }
    return srb;
}

// The same object for the shape every layer has: a uniform buffer at \a binding (bound a
// \a size-byte slot at a time when \a size is non-zero) and, when given, a sampled texture at
// binding 0 (a null texture is allowed when the pipeline only needs the layout).
inline QRhiShaderResourceBindings*
createSrb(QRhi* rhi, QRhiBuffer* ubo, int binding, QRhiShaderResourceBinding::StageFlags stages,
          const char* failure, int size = 0, QRhiTexture* texture = nullptr,
          QRhiSampler* sampler = nullptr)
{
    QList<QRhiShaderResourceBinding> bindings;
    if (texture || sampler)
        bindings.append(QRhiShaderResourceBinding::sampledTexture(
            0, QRhiShaderResourceBinding::FragmentStage, texture, sampler));
    if (size != 0)
        bindings.append(
            QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(binding, stages, ubo, size));
    else
        bindings.append(QRhiShaderResourceBinding::uniformBuffer(binding, stages, ubo));
    return createSrb(rhi, bindings, failure);
}

// The vertex format of every extruded line: dataCoord, colour, extrudeDir, extrudeWidth, isPixel.
inline constexpr int kStrokeFloatsPerVertex = 11;

inline QRhiVertexInputLayout strokeVertexLayout()
{
    QRhiVertexInputLayout layout;
    layout.setBindings({ { quint32(kStrokeFloatsPerVertex) * quint32(sizeof(float)) } });
    layout.setAttributes({
        {0, 0, QRhiVertexInputAttribute::Float2, 0},
        {0, 1, QRhiVertexInputAttribute::Float4, 2 * sizeof(float)},
        {0, 2, QRhiVertexInputAttribute::Float2, 6 * sizeof(float)},
        {0, 3, QRhiVertexInputAttribute::Float, 8 * sizeof(float)},
        {0, 4, QRhiVertexInputAttribute::Float2, 9 * sizeof(float)},
    });
    return layout;
}

// QRhiScissor always uses bottom-left origin (OpenGL convention).
// D3D11/Vulkan/Metal backends convert to top-left internally.
inline QRect computeScissor(const QRect& clip, double dpr, int outputHeight)
{
    int sx = static_cast<int>(clip.x() * dpr);
    int sy = static_cast<int>(clip.y() * dpr);
    int sw = static_cast<int>(clip.width() * dpr);
    int sh = static_cast<int>(clip.height() * dpr);
    sy = outputHeight - sy - sh;
    return QRect(sx, sy, sw, sh);
}

} // namespace qcp::rhi
