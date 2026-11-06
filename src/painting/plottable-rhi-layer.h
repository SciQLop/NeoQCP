#pragma once

#include <QColor>
#include <QHash>
#include <QImage>
#include <QRect>
#include <QSize>
#include <QVector>
#include <cstdlib>
#include <rhi/qrhi.h>
#include <span>

class QCPPlottableRhiLayer
{
public:
    struct DrawEntry
    {
        int vertexOffset = 0; //!< first vertex of the run, in submission order
        int vertexCount = 0; //!< the whole run: fill vertices first, then the stroke
        int fillVertexCount = 0; //!< how many of \a vertexCount are fill (0 for a stroke only)
        float offsetX = 0; // per-draw pixel offset (applied in vertex shader)
        float offsetY = 0;
        float alpha = 1; // busy fade, multiplies the premultiplied vertex colour
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
        float dashOffset = 0; // pixels
        float patternLength = 0; // pixels
        float pattern[8] = { };
        int patternCount = 0;
        QRect scissorRect;
    };

    explicit QCPPlottableRhiLayer(QRhi* rhi);
    ~QCPPlottableRhiLayer();

    // Geometry accumulation (called during replot)
    void clear();
    void addPlottable(std::span<const float> fillVerts, std::span<const float> strokeVerts,
                      const QRect& clipRect, double dpr, int outputHeight, float offsetX = 0,
                      float offsetY = 0, float alpha = 1);

    // Dashed strokes have their own pipeline and vertex buffer: the solid path keeps its 6-float
    // format and pays nothing for this one.
    void addDashedPlottable(std::span<const float> strokeVerts, std::span<const float> dashPattern,
                            float dashOffset, const QRect& clipRect, double dpr, int outputHeight,
                            float offsetX = 0, float offsetY = 0, float alpha = 1);

    //! Adds \a image at \a pixelRect (device pixels) in the submission order. False when the layer
    //! cannot take another image (kMaxImages, a null image, or no image pipeline): the caller then
    //! leaves the command to the layer's painter pass. Re-adding the same image (same cache key)
    //! re-uses its texture and uploads nothing.
    bool addImage(const QImage& image, const QRectF& pixelRect, const QRect& clipRect, double dpr,
                  int outputHeight, float offsetX = 0, float offsetY = 0, float alpha = 1);

    //! A rasterised fill the GPU cannot express, drawn as a textured quad in submission order.
    struct ImageEntry
    {
        qint64 imageKey = 0; //!< QImage::cacheKey() of the raster; identifies its texture
        QRectF pixelRect; //!< device pixels the quad covers (pan offset applied by the shader)
        QSize deviceSize; //!< texture size, device pixels
        float offsetX = 0;
        float offsetY = 0;
        float alpha = 1;
        QRect scissorRect;
    };

    //! One record per submission: which list it went to and where in that list. Drawing walks this
    //! in submission order, so a pen or brush style never moves an object up or down the layer.
    enum class DrawKind
    {
        Solid,
        Dashed,
        Image
    };

    struct OrderedDraw
    {
        DrawKind kind;
        int index;
    };

    //! At most this many rasterised fills per layer per frame; the rest is painted by QPainter,
    //! under this layer's GPU content.
    static constexpr int kMaxImages = 4;

    // Offset-only update (no geometry change, no vertex re-upload)
    void setAllOffsets(float offsetX, float offsetY);

    QPointF lastUniformOffset() const { return QPointF(mLastOffsetX, mLastOffsetY); }

    // GPU resource management
    void invalidatePipeline(); // call on resize (render pass descriptor change)
    bool ensurePipeline(QRhiRenderPassDescriptor* rpDesc, int sampleCount);
    void uploadResources(QRhiResourceUpdateBatch* updates, const QSize& outputSize, float dpr,
                         bool isYUpInNDC);
    void render(QRhiCommandBuffer* cb, const QSize& outputSize);

    bool isDirty() const { return mDirty; }

    //! False once the dashed pipeline failed to build: a dashed stroke must then be painted
    //! instead.
    bool dashedAvailable() const { return !mDashBroken; }

    //! Test hook: how many times the vertex buffers were uploaded (a pan must not add one).
    int solidVertexUploads() const { return mSolidVertexUploads; }

    bool hasGeometry() const
    {
        return !mDrawEntries.isEmpty() || !mDashEntries.isEmpty() || !mImageEntries.isEmpty();
    }

    const QVector<DrawEntry>& drawEntries() const { return mDrawEntries; }

    const QVector<DashedEntry>& dashedDrawEntries() const { return mDashEntries; }

    //! False once the image pipeline failed to build: a rasterised fill must be painted instead.
    //! Every image goes through this one predicate, which also carries the per-frame cap.
    bool canAddImage() const { return !mImageBroken && mImageEntries.size() < kMaxImages; }

    //! True the first time: the cap warning is one per layer, for the plot's lifetime.
    bool warnRasterCapOnce()
    {
        const bool first = !mRasterCapWarned;
        mRasterCapWarned = true;
        return first;
    }

    //! Test hook: how many rasterised fills reached a texture.
    int imageUploads() const { return mImageUploads; }

    //! Test hook: how many textures the layer keeps for the images it drew.
    int imageTextureCount() const { return mImageTextures.size(); }

    //! Test hook: pretend the image pipeline failed to build.
    void breakImagePipelineForTest() { mImageBroken = true; }

    //! Test hook: pretend the dashed pipeline failed to build.
    void breakDashedPipelineForTest() { mDashBroken = true; }

    const QVector<ImageEntry>& imageDrawEntries() const { return mImageEntries; }

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

    //! Matches the ImageParams UBO in plottable_image.vert/.frag. The pan offset is already folded
    //! into \a x0..\a y1 when the slot is written.
    struct alignas(16) ImageUniforms
    {
        float x0, y0, x1, y1;
        float width, height, yFlip, alpha;
    };

    static_assert(sizeof(ImageUniforms) == 32);

    //! One rasterised fill on the GPU. The texture and its SRB outlive a frame; the pending image
    //! is what still has to be uploaded (an unchanged replay leaves it null and uploads nothing).
    struct ImageTexture
    {
        QRhiTexture* texture = nullptr;
        QRhiShaderResourceBindings* srb = nullptr;
        QImage pending;
        bool used = false;
    };

    void createImagePipeline(QRhiRenderPassDescriptor* rpDesc, int sampleCount);
    bool ensureImageTexture(ImageTexture& texture, const QSize& size);
    void uploadImages(QRhiResourceUpdateBatch* updates, const QSize& outputSize, float dpr,
                      float yFlip);

    void createDashedPipeline(QRhiRenderPassDescriptor* rpDesc, int sampleCount);
    void uploadSolid(QRhiResourceUpdateBatch* updates, const QSize& outputSize, float dpr,
                     float yFlip);
    void uploadDashed(QRhiResourceUpdateBatch* updates, const QSize& outputSize, float dpr,
                      float yFlip);

    int ubufStride() const; // aligned slot size for dynamic UBO offsets
    int dashUbufStride() const;
    int imageUbufStride() const;

    //! Extends the last submission when \a entry continues it with the same draw state, so a
    //! hundred thousand consecutive shapes are one draw. False when it has to be its own entry.
    bool extendLastSolid(const DrawEntry& entry);
    bool extendLastDashed(const DashedEntry& entry);

    // Raw staging buffer — avoids QVector::resize zero-initialization overhead
    float* mStagingData = nullptr;
    int mStagingSize = 0; // floats used
    int mStagingCapacity = 0; // floats allocated

    QRhi* mRhi; // non-owned; lifetime managed by QRhiWidget
    QVector<DrawEntry> mDrawEntries;
    QVector<OrderedDraw> mDrawOrder; //!< submission order across both lists

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
    int mLastDashSampleCount = 0; //!< the sample count the dashed pipeline was attempted for
    bool mDashBroken = false; //!< the dashed pipeline failed to build for that sample count
    int mSolidVertexUploads = 0; //!< instrumentation for the pan test

    QVector<ImageEntry> mImageEntries;
    QHash<qint64, ImageTexture*> mImageTextures; //!< keyed by QImage::cacheKey()
    QRhiBuffer* mImageUniformBuffer = nullptr; //!< kMaxImages slots, never resized
    QRhiBuffer* mImageVertexBuffer = nullptr; //!< the shared unit quad
    QRhiGraphicsPipeline* mImagePipeline = nullptr;
    QRhiSampler* mImageSampler = nullptr;
    int mLastImageSampleCount = 0;
    bool mImageBroken = false;
    bool mRasterCapWarned = false; //!< the raster cap warned once for this plot
    bool mImageQuadUploaded = false;
    int mImageUploads = 0;

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
