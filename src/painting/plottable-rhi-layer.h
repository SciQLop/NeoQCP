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
        float alpha = 1;    // busy fade, multiplies the premultiplied vertex colour
        QRect scissorRect; // in physical pixels, Y-flipped for Y-up backends
    };

    //! A dashed stroke: 7 floats per vertex (x, y, r, g, b, a, arc length in pixels).
    struct DashedEntry
    {
        int vertexOffset = 0;
        int vertexCount = 0;
        float offsetX = 0;
        float offsetY = 0;
        float alpha = 1;
        float dashOffset = 0;   // pixels
        float patternLength = 0; // pixels
        float pattern[8] = {};
        int patternCount = 0;
        QRect scissorRect;
    };

    explicit QCPPlottableRhiLayer(QRhi* rhi);
    ~QCPPlottableRhiLayer();

    // Geometry accumulation (called during replot)
    void clear();
    void addPlottable(std::span<const float> fillVerts,
                      std::span<const float> strokeVerts,
                      const QRect& clipRect, double dpr,
                      int outputHeight,
                      float offsetX = 0, float offsetY = 0,
                      float alpha = 1);

    // Dashed strokes have their own pipeline and vertex buffer: the solid path keeps its 6-float
    // format and pays nothing for this one.
    void addDashedPlottable(std::span<const float> strokeVerts,
                            std::span<const float> dashPattern, float dashOffset,
                            const QRect& clipRect, double dpr, int outputHeight,
                            float offsetX = 0, float offsetY = 0, float alpha = 1);

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
    bool hasGeometry() const { return !mDrawEntries.isEmpty() || !mDashEntries.isEmpty(); }
    const QVector<DrawEntry>& drawEntries() const { return mDrawEntries; }
    const QVector<DashedEntry>& dashedDrawEntries() const { return mDashEntries; }

private:
    // Per-draw uniform data, aligned to GPU requirements.
    // Matches the ViewportParams UBO in plottable.vert.
    struct alignas(16) PerDrawUniforms
    {
        float width, height, yFlip, dpr;
        float offsetX, offsetY, alpha;
        float _pad; // pad to 32 bytes for std140
    };
    static_assert(sizeof(PerDrawUniforms) == 32);

    //! Matches the ViewportParams UBO in plottable_dashed.vert/.frag.
    struct alignas(16) DashUniforms
    {
        float width, height, yFlip, dpr;
        float offsetX, offsetY, alpha, dashOffset;
        float patternLength, patternCount, _pad0, _pad1;
        float pattern[2][4];
    };
    static_assert(sizeof(DashUniforms) == 80);

    void createDashedPipeline(QRhiRenderPassDescriptor* rpDesc, int sampleCount);
    void uploadSolid(QRhiResourceUpdateBatch* updates, const QSize& outputSize, float dpr,
                     float yFlip);
    void uploadDashed(QRhiResourceUpdateBatch* updates, const QSize& outputSize, float dpr,
                      float yFlip);
    void renderDashed(QRhiCommandBuffer* cb, const QSize& outputSize);

    int ubufStride() const; // aligned slot size for dynamic UBO offsets
    int dashUbufStride() const;
    void dashStagingAppend(const float* src, int count);

    // Raw staging buffer — avoids QVector::resize zero-initialization overhead
    void stagingAppend(const float* src, int count);
    float* mStagingData = nullptr;
    int mStagingSize = 0;     // floats used
    int mStagingCapacity = 0; // floats allocated

    QRhi* mRhi; // non-owned; lifetime managed by QRhiWidget
    QVector<DrawEntry> mDrawEntries;

    float* mDashStagingData = nullptr;
    int mDashStagingSize = 0;
    int mDashStagingCapacity = 0;
    QVector<DashedEntry> mDashEntries;
    QRhiBuffer* mDashVertexBuffer = nullptr;
    QRhiBuffer* mDashUniformBuffer = nullptr;
    QRhiShaderResourceBindings* mDashSrb = nullptr;
    QRhiGraphicsPipeline* mDashPipeline = nullptr;
    int mDashVertexBufferSize = 0;
    int mDashUniformBufferSize = 0;
    int mLastDashSampleCount = 0;

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
