#pragma once

#include <QColor>
#include <QRect>
#include <QVector>
#include <rhi/qrhi.h>
#include <span>
#include <cstdlib>

class QCPPlottableRhiLayer
{
public:
    struct DrawEntry
    {
        int fillOffset = 0;
        int fillVertexCount = 0;
        int strokeOffset = 0;
        int strokeVertexCount = 0;
        float offsetX = 0;  // per-draw pixel offset (applied in vertex shader)
        float offsetY = 0;
        QRect scissorRect; // in physical pixels, Y-flipped for Y-up backends
    };

    explicit QCPPlottableRhiLayer(QRhi* rhi);
    ~QCPPlottableRhiLayer();

    // Geometry accumulation (called during replot)
    void clear();
    void addPlottable(std::span<const float> fillVerts,
                      std::span<const float> strokeVerts,
                      const QRect& clipRect, double dpr,
                      int outputHeight,
                      float offsetX = 0, float offsetY = 0);

    // Offset-only update (no geometry change, no vertex re-upload)
    void setAllOffsets(float offsetX, float offsetY);
    QPointF lastUniformOffset() const { return QPointF(mLastOffsetX, mLastOffsetY); }

    // GPU resource management
    void invalidatePipeline(); // call on resize (render pass descriptor change)
    bool ensurePipeline(QRhiRenderPassDescriptor* rpDesc, int sampleCount);
    void uploadResources(QRhiResourceUpdateBatch* updates,
                          const QSize& outputSize, float dpr, bool isYUpInNDC);
    void render(QRhiCommandBuffer* cb, const QSize& outputSize);

    bool isDirty() const { return mDirty; }
    bool hasGeometry() const { return !mDrawEntries.isEmpty(); }

private:
    // Per-draw uniform data, aligned to GPU requirements.
    // Matches the ViewportParams UBO in plottable.vert.
    struct alignas(16) PerDrawUniforms
    {
        float width, height, yFlip, dpr;
        float offsetX, offsetY;
        float _pad[2]; // pad to 32 bytes for std140
    };
    static_assert(sizeof(PerDrawUniforms) == 32);

    int ubufStride() const; // aligned slot size for dynamic UBO offsets

    // Raw staging buffer — avoids QVector::resize zero-initialization overhead
    void stagingAppend(const float* src, int count);
    float* mStagingData = nullptr;
    int mStagingSize = 0;     // floats used
    int mStagingCapacity = 0; // floats allocated

    QRhi* mRhi; // non-owned; lifetime managed by QRhiWidget
    QVector<DrawEntry> mDrawEntries;

    QRhiBuffer* mVertexBuffer = nullptr;
    QRhiBuffer* mUniformBuffer = nullptr;
    QRhiShaderResourceBindings* mSrb = nullptr;
    QRhiGraphicsPipeline* mPipeline = nullptr;
    int mVertexBufferSize = 0;
    int mUniformBufferSize = 0;
    int mLastSampleCount = 0;
    bool mDirty = false;
    float mLastOffsetX = 0;
    float mLastOffsetY = 0;
};
