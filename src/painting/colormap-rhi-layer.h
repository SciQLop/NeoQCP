#pragma once

#include <QColor>
#include <QImage>
#include <QRect>
#include <QRectF>
#include <QVector>
#include <rhi/qrhi.h>

class QCPLayer;
class QCPLayerable;

class QCPColormapRhiLayer
{
public:
    explicit QCPColormapRhiLayer(QRhi* rhi);
    ~QCPColormapRhiLayer();

    void setImage(const QImage& image);
    void setQuadRect(const QRectF& pixelRect);
    [[nodiscard]] QRectF quadRect() const { return mImage.pixelRect; }

    // Pan compensation. On frames where this layer skips its repaint (the
    // translate fast path), QCPColorMap2::draw() never runs, so setQuadRect() is
    // never called and the quad rect still holds the PREVIOUS range's pixel
    // rect. The compositor shifts the quad by this offset instead — the colormap
    // counterpart of QCPPlottableRhiLayer/QCPScatterRhiLayer::setAllOffsets().
    // Without it the image stays put while the axes move.
    void setPixelOffset(float offsetX, float offsetY);
    [[nodiscard]] QPointF pixelOffset() const { return { mOffsetX, mOffsetY }; }
    [[nodiscard]] QRectF effectiveQuadRect() const
    {
        return mImage.pixelRect.translated(mOffsetX, mOffsetY);
    }
    void setScissorRect(const QRect& scissor);
    void setLayer(QCPLayer* layer) { mLayer = layer; }
    QCPLayer* layer() const { return mLayer; }

    // The colormap plottable this RHI layer renders for. The compositor skips
    // layers whose owner is not really visible — without this a hidden colormap
    // keeps compositing its last frame (its draw() stops being called, so the
    // quad is never updated and it freezes in place during pans).
    void setOwner(QCPLayerable* owner) { mOwner = owner; }
    QCPLayerable* owner() const { return mOwner; }
    bool ownerVisible() const;

    void setContourLines(QVector<float> uvVertices, const QColor& color);
    void clearContourLines();

    //! A transparent image drawn over the colour map and its contour lines, at \a pixelRect
    //! (logical pixels), following the same pan offset and scissor: the contour labels.
    void setOverlay(const QImage& image, const QRectF& pixelRect);
    void clearOverlay();
    [[nodiscard]] bool hasOverlay() const { return !mOverlay.staging.isNull(); }

    void invalidatePipeline();
    bool ensurePipeline(QRhiRenderPassDescriptor* rpDesc, int sampleCount,
                        QRhiBuffer* compositeUbo);
    void uploadResources(QRhiResourceUpdateBatch* updates,
                         const QSize& outputSize, float dpr, bool isYUpInNDC,
                         QRhiBuffer* compositeUbo);
    void render(QRhiCommandBuffer* cb, const QSize& outputSize);

    bool hasContent() const { return !mImage.staging.isNull(); }
    // Exposed for tests: true when the next uploadResources() call will
    // actually re-upload the staged image to the GPU texture.
    bool textureUploadPending() const { return mImage.textureDirty; }
    // Exposed for tests: the contour line vertices held, and a pending label overlay upload.
    [[nodiscard]] qsizetype contourVertexCount() const { return mContourUvVertices.size() / 2; }
    [[nodiscard]] bool overlayUploadPending() const { return mOverlay.textureDirty; }
    void clear();

    // A resample can produce an image wider or taller than the RHI backend allows in
    // a texture (Metal aborts the whole process on an oversized descriptor rather than
    // failing create() gracefully, see ensureTexture()). Callers deciding whether to
    // even attempt the GPU path should check this first.
    bool fitsInTexture(const QSize& imgSize) const;

private:
    QRhi* mRhi;
    QCPLayer* mLayer = nullptr;
    QCPLayerable* mOwner = nullptr;

    // One textured rectangle: the colour map image, or the label overlay.
    struct Quad
    {
        QImage staging;
        QRectF pixelRect;
        bool textureDirty = false;
        bool geometryDirty = false;
        QRhiTexture* texture = nullptr;
        QRhiShaderResourceBindings* srb = nullptr;
        QRhiBuffer* vertexBuffer = nullptr;
        QSize textureSize;
        QSize lastOutputSize;
        float ndcX0 = 0, ndcY0 = 0, ndcX1 = 0, ndcY1 = 0;
    };
    Quad mImage;
    Quad mOverlay;
    float mOffsetX = 0;
    float mOffsetY = 0;
    QRect mScissorRect;

    // CPU staging — contour lines (UV [0,1] space, 2 floats per vertex)
    QVector<float> mContourUvVertices;
    QColor mContourColor{Qt::white};
    bool mContourLinesDirty = false;
    bool mContourUboDirty = false;

    // GPU resources shared by both quads
    QRhiSampler* mSampler = nullptr;
    QRhiGraphicsPipeline* mPipeline = nullptr;

    // GPU resources — contour lines
    QRhiBuffer* mLineVbo = nullptr;
    QRhiBuffer* mLineUbo = nullptr;
    QRhiShaderResourceBindings* mLineSrb = nullptr;
    QRhiGraphicsPipeline* mLinePipeline = nullptr;
    int mLineVertexCount = 0;
    int mLineVboSize = 0;

    int mLastSampleCount = 0;

    bool ensureTexture(Quad& quad, QRhiBuffer* compositeUbo);
    void uploadQuad(Quad& quad, QRhiResourceUpdateBatch* updates, const QSize& outputSize,
                    float dpr, bool isYUpInNDC, QRhiBuffer* compositeUbo);
    void updateQuadGeometry(Quad& quad, QRhiResourceUpdateBatch* updates,
                            const QSize& outputSize, float dpr, bool isYUpInNDC);
    void drawQuad(const Quad& quad, QRhiCommandBuffer* cb, const QSize& outputSize);
    static void releaseQuad(Quad& quad);
};
