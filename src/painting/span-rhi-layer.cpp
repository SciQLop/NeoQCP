#include "span-rhi-layer.h"
#include "rhi-utils.h"
#include "Profiling.hpp"
#include "embedded_shaders.h"

#include "../items/item-vspan.h"
#include "../items/item-hspan.h"
#include "../items/item-rspan.h"
#include "../layoutelements/layoutelement-axisrect.h"
#include "../axis/axis.h"

#include "span-grid-vertex.h"

#include <array>
#include <cmath>

using namespace qcp::rhi::span_grid;

QCPSpanRhiLayer::QCPSpanRhiLayer(QRhi* rhi)
    : mRhi(rhi)
{
}

QCPSpanRhiLayer::~QCPSpanRhiLayer()
{
    delete mPipeline;
    delete mLayoutSrb;
    delete mLayoutUbo;
    delete mVertexBuffer;
    cleanupDrawGroups();
}

void QCPSpanRhiLayer::registerSpan(QCPAbstractItem* span)
{
    if (!mSpans.contains(span))
    {
        mSpans.append(span);
        mGeometryDirty = true;
    }
}

void QCPSpanRhiLayer::unregisterSpan(QCPAbstractItem* span)
{
    if (mSpans.removeOne(span))
        mGeometryDirty = true;
}

void QCPSpanRhiLayer::markGeometryDirty()
{
    mGeometryDirty = true;
}

void QCPSpanRhiLayer::cleanupDrawGroups()
{
    for (auto& group : mDrawGroups)
    {
        delete group.uniformBuffer;
        delete group.srb;
    }
    mDrawGroups.clear();
}

void QCPSpanRhiLayer::invalidatePipeline()
{
    delete mPipeline;
    mPipeline = nullptr;
    delete mLayoutSrb;
    mLayoutSrb = nullptr;
    delete mLayoutUbo;
    mLayoutUbo = nullptr;
    cleanupDrawGroups();
}

bool QCPSpanRhiLayer::ensurePipeline(QRhiRenderPassDescriptor* rpDesc, int sampleCount)
{
    PROFILE_HERE_N("QCPSpanRhiLayer::ensurePipeline");

    if (mPipeline && mLastSampleCount == sampleCount)
        return true;

    invalidatePipeline();

    auto vertShader = qcp::rhi::loadEmbeddedShader(span_vert_qsb_data, span_vert_qsb_data_len);
    auto fragShader = qcp::rhi::loadEmbeddedShader(plottable_frag_qsb_data, plottable_frag_qsb_data_len);

    if (!vertShader.isValid() || !fragShader.isValid())
    {
        qDebug() << "Failed to load span shaders";
        return false;
    }

    // Layout-only UBO + SRB: the SRB defines the binding layout for the pipeline.
    // Per-group SRBs with real data are used at draw time.
    mLayoutUbo = mRhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, kUniformBufferSize);
    if (!mLayoutUbo->create())
    {
        qDebug() << Q_FUNC_INFO << "Failed to create span layout UBO";
        return false;
    }

    mLayoutSrb = mRhi->newShaderResourceBindings();
    mLayoutSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage, mLayoutUbo)
    });
    if (!mLayoutSrb->create())
    {
        qDebug() << Q_FUNC_INFO << "Failed to create span layout SRB";
        return false;
    }

    mPipeline = mRhi->newGraphicsPipeline();
    mPipeline->setShaderStages({
        {QRhiShaderStage::Vertex, vertShader},
        {QRhiShaderStage::Fragment, fragShader}
    });

    // Vertex layout: stride = 11 * sizeof(float) = 44 bytes
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({{kFloatsPerVertex * sizeof(float)}});
    inputLayout.setAttributes({
        {0, 0, QRhiVertexInputAttribute::Float2, 0},                        // dataCoord
        {0, 1, QRhiVertexInputAttribute::Float4, 2 * sizeof(float)},        // color
        {0, 2, QRhiVertexInputAttribute::Float2, 6 * sizeof(float)},        // extrudeDir
        {0, 3, QRhiVertexInputAttribute::Float,  8 * sizeof(float)},        // extrudeWidth
        {0, 4, QRhiVertexInputAttribute::Float2, 9 * sizeof(float)},        // isPixel
    });
    mPipeline->setVertexInputLayout(inputLayout);

    mPipeline->setTargetBlends({qcp::rhi::premultipliedAlphaBlend()});

    mPipeline->setFlags(QRhiGraphicsPipeline::UsesScissor);
    mPipeline->setTopology(QRhiGraphicsPipeline::Triangles);
    mPipeline->setSampleCount(sampleCount);
    mPipeline->setRenderPassDescriptor(rpDesc);
    mPipeline->setShaderResourceBindings(mLayoutSrb);

    if (!mPipeline->create())
    {
        qDebug() << "Failed to create span pipeline";
        delete mPipeline;
        mPipeline = nullptr;
        return false;
    }

    mLastSampleCount = sampleCount;
    return true;
}

void QCPSpanRhiLayer::rebuildGeometry(float dpr, int outputHeight, bool isYUpInNDC)
{
    PROFILE_HERE_N("QCPSpanRhiLayer::rebuildGeometry");

    mStagingVertices.clear();
    cleanupDrawGroups();

    // Group spans by axis rect
    QMap<QCPAxisRect*, QVector<QCPAbstractItem*>> groupedSpans;
    for (auto* span : mSpans)
    {
        QCPAxisRect* ar = span->clipAxisRect();
        if (!ar)
            continue;
        groupedSpans[ar].append(span);
    }

    for (auto it = groupedSpans.constBegin(); it != groupedSpans.constEnd(); ++it)
    {
        QCPAxisRect* ar = it.key();
        const auto& spans = it.value();

        int groupVertexStart = mStagingVertices.size() / kFloatsPerVertex;

        for (auto* span : spans)
        {
            if (auto* vspan = qobject_cast<QCPItemVSpan*>(span))
                appendVSpanGeometry(vspan, ar);
            else if (auto* hspan = qobject_cast<QCPItemHSpan*>(span))
                appendHSpanGeometry(hspan, ar);
            else if (auto* rspan = qobject_cast<QCPItemRSpan*>(span))
                appendRSpanGeometry(rspan, ar);
        }

        int groupVertexCount = mStagingVertices.size() / kFloatsPerVertex - groupVertexStart;
        if (groupVertexCount == 0)
            continue;

        // Compute scissor rect in physical pixels
        QRect scissor = qcp::rhi::computeScissor(
            QRect(ar->left(), ar->top(), ar->width(), ar->height()),
            dpr, outputHeight, isYUpInNDC);

        // Create per-group uniform buffer and SRB
        auto* ubo = mRhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, kUniformBufferSize);
        if (!ubo->create())
        {
            qDebug() << Q_FUNC_INFO << "Failed to create per-group UBO";
            delete ubo;
            continue;
        }

        auto* srb = mRhi->newShaderResourceBindings();
        srb->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(
                0, QRhiShaderResourceBinding::VertexStage, ubo)
        });
        if (!srb->create())
        {
            qDebug() << Q_FUNC_INFO << "Failed to create per-group SRB";
            delete srb;
            delete ubo;
            continue;
        }

        DrawGroup group;
        group.axisRect = ar;
        group.vertexOffset = groupVertexStart;
        group.vertexCount = groupVertexCount;
        group.scissorRect = scissor;
        group.uniformBuffer = ubo;
        group.srb = srb;
        mDrawGroups.append(group);
    }

    // Cache axis rect bounds for layout-change detection
    mLastAxisRectBounds.clear();
    for (const auto& group : mDrawGroups)
        mLastAxisRectBounds[group.axisRect] = QRect(group.axisRect->left(), group.axisRect->top(),
                                                     group.axisRect->width(), group.axisRect->height());
}

void QCPSpanRhiLayer::appendVSpanGeometry(QCPItemVSpan* vspan, QCPAxisRect* ar)
{
    // Use the axis rect's own key axis for coordinate conversion (not the span's
    // position axis, which may belong to a different axis rect in multi-rect layouts).
    auto* keyAxis = ar->axis(QCPAxis::atBottom);
    if (!keyAxis)
        keyAxis = ar->axis(QCPAxis::atTop);
    if (!keyAxis)
        return;

    // Convert data coords to pixels on the CPU in double precision to avoid
    // float32 catastrophic cancellation with large values (e.g. Unix timestamps).
    float pixX0 = float(keyAxis->coordToPixel(vspan->lowerEdge->coords().x()));
    float pixX1 = float(keyAxis->coordToPixel(vspan->upperEdge->coords().x()));
    float pixTop = float(ar->top());
    float pixBot = float(ar->top() + ar->height());

    const QBrush& fillBrush = vspan->selected() ? vspan->selectedBrush() : vspan->brush();
    auto fillColor = qcp::rhi::premultipliedColor(fillBrush.color());
    if (fillBrush.style() != Qt::NoBrush && fillColor[3] > 0.0f)
    {
        appendQuad(mStagingVertices,
                   pixX0, pixTop, pixX1, pixTop,
                   pixX0, pixBot, pixX1, pixBot,
                   fillColor, 0, 0, 0, 1, 1);
    }

    const QPen& borderPen = vspan->selected() ? vspan->selectedBorderPen() : vspan->borderPen();
    auto borderColor = qcp::rhi::premultipliedColor(borderPen.color());
    float penW = borderPen.widthF();
    float halfW = (penW == 0.0 || borderPen.isCosmetic()) ? 0.5f : float(penW) / 2.0f;
    if (borderPen.style() != Qt::NoPen && halfW > 0.0f)
    {
        appendBorder(mStagingVertices,
                     pixX0, pixTop, pixX0, pixBot,
                     borderColor, 1, 0, halfW, 1, 1);
        appendBorder(mStagingVertices,
                     pixX1, pixTop, pixX1, pixBot,
                     borderColor, 1, 0, halfW, 1, 1);
    }
}

void QCPSpanRhiLayer::appendHSpanGeometry(QCPItemHSpan* hspan, QCPAxisRect* ar)
{
    auto* valAxis = ar->axis(QCPAxis::atLeft);
    if (!valAxis)
        valAxis = ar->axis(QCPAxis::atRight);
    if (!valAxis)
        return;

    float pixY0 = float(valAxis->coordToPixel(hspan->lowerEdge->coords().y()));
    float pixY1 = float(valAxis->coordToPixel(hspan->upperEdge->coords().y()));
    float pixLeft = float(ar->left());
    float pixRight = float(ar->left() + ar->width());

    const QBrush& fillBrush = hspan->selected() ? hspan->selectedBrush() : hspan->brush();
    auto fillColor = qcp::rhi::premultipliedColor(fillBrush.color());
    if (fillBrush.style() != Qt::NoBrush && fillColor[3] > 0.0f)
    {
        appendQuad(mStagingVertices,
                   pixLeft, pixY0, pixRight, pixY0,
                   pixLeft, pixY1, pixRight, pixY1,
                   fillColor, 0, 0, 0, 1, 1);
    }

    const QPen& borderPen = hspan->selected() ? hspan->selectedBorderPen() : hspan->borderPen();
    auto borderColor = qcp::rhi::premultipliedColor(borderPen.color());
    float penW = borderPen.widthF();
    float halfW = (penW == 0.0 || borderPen.isCosmetic()) ? 0.5f : float(penW) / 2.0f;
    if (borderPen.style() != Qt::NoPen && halfW > 0.0f)
    {
        appendBorder(mStagingVertices,
                     pixLeft, pixY0, pixRight, pixY0,
                     borderColor, 0, 1, halfW, 1, 1);
        appendBorder(mStagingVertices,
                     pixLeft, pixY1, pixRight, pixY1,
                     borderColor, 0, 1, halfW, 1, 1);
    }
}

void QCPSpanRhiLayer::appendRSpanGeometry(QCPItemRSpan* rspan, [[maybe_unused]] QCPAxisRect* ar)
{
    auto* keyAxis = ar->axis(QCPAxis::atBottom);
    if (!keyAxis)
        keyAxis = ar->axis(QCPAxis::atTop);
    auto* valAxis = ar->axis(QCPAxis::atLeft);
    if (!valAxis)
        valAxis = ar->axis(QCPAxis::atRight);
    if (!keyAxis || !valAxis)
        return;

    float pixLeft = float(keyAxis->coordToPixel(rspan->leftEdge->coords().x()));
    float pixRight = float(keyAxis->coordToPixel(rspan->rightEdge->coords().x()));
    float pixTop = float(valAxis->coordToPixel(rspan->topEdge->coords().y()));
    float pixBot = float(valAxis->coordToPixel(rspan->bottomEdge->coords().y()));

    const QBrush& fillBrush = rspan->selected() ? rspan->selectedBrush() : rspan->brush();
    auto fillColor = qcp::rhi::premultipliedColor(fillBrush.color());
    if (fillBrush.style() != Qt::NoBrush && fillColor[3] > 0.0f)
    {
        appendQuad(mStagingVertices,
                   pixLeft, pixTop, pixRight, pixTop,
                   pixLeft, pixBot, pixRight, pixBot,
                   fillColor, 0, 0, 0, 1, 1);
    }

    const QPen& borderPen = rspan->selected() ? rspan->selectedBorderPen() : rspan->borderPen();
    auto borderColor = qcp::rhi::premultipliedColor(borderPen.color());
    float penW = borderPen.widthF();
    float halfW = (penW == 0.0 || borderPen.isCosmetic()) ? 0.5f : float(penW) / 2.0f;
    if (borderPen.style() != Qt::NoPen && halfW > 0.0f)
    {
        appendBorder(mStagingVertices,
                     pixLeft, pixTop, pixLeft, pixBot,
                     borderColor, 1, 0, halfW, 1, 1);
        appendBorder(mStagingVertices,
                     pixRight, pixTop, pixRight, pixBot,
                     borderColor, 1, 0, halfW, 1, 1);
        appendBorder(mStagingVertices,
                     pixLeft, pixTop, pixRight, pixTop,
                     borderColor, 0, 1, halfW, 1, 1);
        appendBorder(mStagingVertices,
                     pixLeft, pixBot, pixRight, pixBot,
                     borderColor, 0, 1, halfW, 1, 1);
    }
}

void QCPSpanRhiLayer::uploadResources(QRhiResourceUpdateBatch* updates,
                                        const QSize& outputSize, float dpr,
                                        bool isYUpInNDC)
{
    PROFILE_HERE_N("QCPSpanRhiLayer::uploadResources");

    // Always rebuild geometry: spans use pre-computed pixel coordinates (CPU-side
    // double→float conversion to avoid float32 precision loss with large values
    // like Unix timestamps), so geometry must track axis range and layout changes.
    // Span vertex count is tiny (6-12 per span), so unconditional rebuild is cheap.
    mGeometryDirty = true;

    if (mGeometryDirty)
    {
        rebuildGeometry(dpr, outputSize.height(), isYUpInNDC);
        mGeometryDirty = false;

        if (!mStagingVertices.isEmpty())
        {
            int requiredSize = mStagingVertices.size() * sizeof(float);
            if (!mVertexBuffer || mVertexBufferSize < requiredSize)
            {
                delete mVertexBuffer;
                mVertexBuffer = mRhi->newBuffer(QRhiBuffer::Dynamic,
                                                 QRhiBuffer::VertexBuffer,
                                                 requiredSize);
                if (!mVertexBuffer->create())
                {
                    qDebug() << Q_FUNC_INFO << "Failed to create span vertex buffer";
                    delete mVertexBuffer;
                    mVertexBuffer = nullptr;
                    mVertexBufferSize = 0;
                    return;
                }
                mVertexBufferSize = requiredSize;
            }
            updates->updateDynamicBuffer(mVertexBuffer, 0, requiredSize,
                                          mStagingVertices.constData());
        }
    }

    // Always update uniform buffers with current axis ranges
    for (auto& group : mDrawGroups)
    {
        QCPAxisRect* ar = group.axisRect;
        QCPAxis* keyAxis = ar->axis(QCPAxis::atBottom);
        QCPAxis* valAxis = ar->axis(QCPAxis::atLeft);
        if (!keyAxis || !valAxis)
            continue;

        float keyOffset, keyLength;
        if (!keyAxis->rangeReversed())
        {
            keyOffset = float(ar->left());
            keyLength = float(ar->width());
        }
        else
        {
            keyOffset = float(ar->right());
            keyLength = float(-ar->width());
        }

        float valOffset, valLength;
        if (!valAxis->rangeReversed())
        {
            valOffset = float(ar->bottom());
            valLength = float(-ar->height());
        }
        else
        {
            valOffset = float(ar->top());
            valLength = float(ar->height());
        }

        struct {
            float width, height, yFlip, dpr;
            float keyRangeLower, keyRangeUpper, keyAxisOffset, keyAxisLength, keyLogScale;
            float valRangeLower, valRangeUpper, valAxisOffset, valAxisLength, valLogScale;
            float _pad0, _pad1;
        } params = {
            float(outputSize.width()),
            float(outputSize.height()),
            isYUpInNDC ? -1.0f : 1.0f,
            dpr,
            float(keyAxis->range().lower),
            float(keyAxis->range().upper),
            keyOffset,
            keyLength,
            (keyAxis->scaleType() == QCPAxis::stLogarithmic) ? 1.0f : 0.0f,
            float(valAxis->range().lower),
            float(valAxis->range().upper),
            valOffset,
            valLength,
            (valAxis->scaleType() == QCPAxis::stLogarithmic) ? 1.0f : 0.0f,
            0.0f, 0.0f
        };
        static_assert(sizeof(params) == kUniformBufferSize);
        updates->updateDynamicBuffer(group.uniformBuffer, 0, sizeof(params), &params);
    }
}

void QCPSpanRhiLayer::render(QRhiCommandBuffer* cb, const QSize& outputSize)
{
    PROFILE_HERE_N("QCPSpanRhiLayer::render");

    if (!mPipeline || !mVertexBuffer || mDrawGroups.isEmpty())
        return;

    cb->setGraphicsPipeline(mPipeline);
    cb->setViewport({0, 0, float(outputSize.width()), float(outputSize.height())});

    const QRhiCommandBuffer::VertexInput vbufBinding(mVertexBuffer, 0);
    cb->setVertexInput(0, 1, &vbufBinding);

    for (const auto& group : mDrawGroups)
    {
        cb->setScissor({group.scissorRect.x(), group.scissorRect.y(),
                        group.scissorRect.width(), group.scissorRect.height()});
        cb->setShaderResources(group.srb);
        cb->draw(group.vertexCount, 1, group.vertexOffset, 0);
    }
}
