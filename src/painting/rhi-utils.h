#pragma once

#include <QColor>
#include <QImage>
#include <array>
#include <rhi/qrhi.h>

namespace qcp::rhi {

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
