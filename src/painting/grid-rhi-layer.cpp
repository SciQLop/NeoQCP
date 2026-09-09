#include "grid-rhi-layer.h"
#include "rhi-utils.h"
#include "span-grid-vertex.h"
#include "Profiling.hpp"
#include "embedded_shaders.h"
#include "../axis/axis.h"
#include "../layoutelements/layoutelement-axisrect.h"

#include <algorithm>
#include <array>
#include <cmath>

using namespace qcp::rhi::span_grid;

QCPGridRhiLayer::QCPGridRhiLayer(QRhi* rhi)
    : mRhi(rhi)
{
}

QCPGridRhiLayer::~QCPGridRhiLayer()
{
    delete mPipeline;
    delete mLayoutSrb;
    delete mLayoutUbo;
    delete mVertexBuffer;
    cleanupDrawGroups();
}

void QCPGridRhiLayer::markGeometryDirty() { mGeometryDirty = true; }

void QCPGridRhiLayer::registerAxis(QCPAxis* axis)
{
    if (!mAxes.contains(axis))
    {
        mAxes.append(axis);
        mGeometryDirty = true;
    }
}

void QCPGridRhiLayer::unregisterAxis(QCPAxis* axis)
{
    if (mAxes.removeOne(axis))
    {
        mCachedTicks.remove(axis);
        mGeometryDirty = true;
    }
}

void QCPGridRhiLayer::cleanupDrawGroups()
{
    for (auto& group : mDrawGroups)
    {
        delete group.uniformBuffer;
        delete group.srb;
    }
    mDrawGroups.clear();
}

void QCPGridRhiLayer::invalidatePipeline()
{
    delete mPipeline;
    mPipeline = nullptr;
    delete mLayoutSrb;
    mLayoutSrb = nullptr;
    delete mLayoutUbo;
    mLayoutUbo = nullptr;
    cleanupDrawGroups();
    // Draw groups were just released; force a rebuild on the next uploadResources()
    // even when no signature/bounds change is detectable (e.g. render-target
    // recreation or sample-count change at unchanged size).
    mGeometryDirty = true;
}

bool QCPGridRhiLayer::ensurePipeline(QRhiRenderPassDescriptor* rpDesc, int sampleCount)
{
    PROFILE_HERE_N("QCPGridRhiLayer::ensurePipeline");

    if (mPipeline && mLastSampleCount == sampleCount)
        return true;

    invalidatePipeline();

    QShader vertShader = QShader::fromSerialized(QByteArray::fromRawData(
        reinterpret_cast<const char*>(span_vert_qsb_data), span_vert_qsb_data_len));
    QShader fragShader = QShader::fromSerialized(QByteArray::fromRawData(
        reinterpret_cast<const char*>(plottable_frag_qsb_data), plottable_frag_qsb_data_len));

    if (!vertShader.isValid() || !fragShader.isValid())
        return false;

    mLayoutUbo = mRhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, kUniformBufferSize);
    mLayoutUbo->create();

    mLayoutSrb = mRhi->newShaderResourceBindings();
    mLayoutSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage, mLayoutUbo)
    });
    mLayoutSrb->create();

    mPipeline = mRhi->newGraphicsPipeline();
    mPipeline->setShaderStages({
        {QRhiShaderStage::Vertex, vertShader},
        {QRhiShaderStage::Fragment, fragShader}
    });

    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({{kFloatsPerVertex * sizeof(float)}});
    inputLayout.setAttributes({
        {0, 0, QRhiVertexInputAttribute::Float2, 0},
        {0, 1, QRhiVertexInputAttribute::Float4, 2 * sizeof(float)},
        {0, 2, QRhiVertexInputAttribute::Float2, 6 * sizeof(float)},
        {0, 3, QRhiVertexInputAttribute::Float,  8 * sizeof(float)},
        {0, 4, QRhiVertexInputAttribute::Float2, 9 * sizeof(float)},
    });
    mPipeline->setVertexInputLayout(inputLayout);

    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable = true;
    blend.srcColor = QRhiGraphicsPipeline::One;
    blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    blend.srcAlpha = QRhiGraphicsPipeline::One;
    blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    mPipeline->setTargetBlends({blend});

    mPipeline->setFlags(QRhiGraphicsPipeline::UsesScissor);
    mPipeline->setTopology(QRhiGraphicsPipeline::Triangles);
    mPipeline->setSampleCount(sampleCount);
    mPipeline->setRenderPassDescriptor(rpDesc);
    mPipeline->setShaderResourceBindings(mLayoutSrb);

    if (!mPipeline->create())
    {
        delete mPipeline;
        mPipeline = nullptr;
        return false;
    }

    mLastSampleCount = sampleCount;
    return true;
}

void QCPGridRhiLayer::appendTickVertices(QCPAxis* axis, QCPAxisRect* ar, QVector<float>& out) const
{
    if (!axis->visible() || !axis->ticks())
        return;

    const bool isHorizontal = (axis->orientation() == Qt::Horizontal);
    const auto axisType = axis->axisType();

    int tickDir = (axisType == QCPAxis::atBottom || axisType == QCPAxis::atRight) ? -1 : 1;

    float baseline = 0.0f;
    switch (axisType)
    {
        case QCPAxis::atBottom: baseline = float(ar->bottom()); break;
        case QCPAxis::atTop:    baseline = float(ar->top());    break;
        case QCPAxis::atLeft:   baseline = float(ar->left());   break;
        case QCPAxis::atRight:  baseline = float(ar->right());  break;
    }

    auto emitTickLine = [&](double tickValue, float lengthOut, float lengthIn, const QPen& pen) {
        auto color = qcp::rhi::premultipliedColor(pen.color());
        float penW = pen.widthF();
        float halfW = (penW == 0.0 || pen.isCosmetic()) ? 0.5f : float(penW) / 2.0f;
        if (pen.style() == Qt::NoPen || halfW <= 0.0f || color[3] <= 0.0f)
            return;
        // Convert tick position to pixel space so each axis uses its own transform
        float tv = float(axis->coordToPixel(tickValue));
        float pxStart = baseline - lengthOut * tickDir;
        float pxEnd   = baseline + lengthIn  * tickDir;
        if (isHorizontal)
        {
            appendBorder(out,
                         tv, pxStart, tv, pxEnd,
                         color, 1, 0, halfW, 1, 1);
        }
        else
        {
            appendBorder(out,
                         pxStart, tv, pxEnd, tv,
                         color, 0, 1, halfW, 1, 1);
        }
    };

    float tickLenOut = float(axis->tickLengthOut());
    float tickLenIn  = float(axis->tickLengthIn());
    for (double tickVal : axis->tickVector())
        emitTickLine(tickVal, tickLenOut, tickLenIn, axis->tickPen());

    if (axis->subTicks())
    {
        float subTickLenOut = float(axis->subTickLengthOut());
        float subTickLenIn  = float(axis->subTickLengthIn());
        for (double subTickVal : axis->subTickVector())
            emitTickLine(subTickVal, subTickLenOut, subTickLenIn, axis->subTickPen());
    }
}

QVector<QCPAxis*> QCPGridRhiLayer::orderedAxesForRect(QCPAxisRect* ar) const
{
    QVector<QCPAxis*> axes;
    for (auto* axis : mAxes)
    {
        if (axis->axisRect() == ar)
            axes.append(axis);
    }
    return axes;
}

void QCPGridRhiLayer::rebuildGeometry(float dpr, int outputHeight)
{
    PROFILE_HERE_N("QCPGridRhiLayer::rebuildGeometry");

    mStagingVertices.clear();
    cleanupDrawGroups();

    QVector<QCPAxisRect*> axisRects;
    for (auto* axis : mAxes)
    {
        if (auto* ar = axis->axisRect(); ar && !axisRects.contains(ar))
            axisRects.append(ar);
    }
    std::sort(axisRects.begin(), axisRects.end());

    for (QCPAxisRect* ar : axisRects)
    {
        // Same predicate used by uploadResources() to re-derive this order when
        // re-baking tick pixels in place, so the two paths cannot drift.
        const QVector<QCPAxis*> axes = orderedAxesForRect(ar);

        int groupVertexStart = mStagingVertices.size() / kFloatsPerVertex;

        for (auto* axis : axes)
        {
            QCPGrid* grid = axis->grid();
            if (!grid || !grid->visible())
                continue;

            const bool isHorizontal = (axis->orientation() == Qt::Horizontal);
            const float pixLeft = float(ar->left());
            const float pixRight = float(ar->left() + ar->width());
            const float pixTop = float(ar->top());
            const float pixBot = float(ar->top() + ar->height());

            auto emitGridLine = [&](double tickValue, const QPen& pen) {
                auto color = qcp::rhi::premultipliedColor(pen.color());
                float penW = pen.widthF();
                float halfW = (penW == 0.0 || pen.isCosmetic()) ? 0.5f : float(penW) / 2.0f;
                if (pen.style() == Qt::NoPen || halfW <= 0.0f || color[3] <= 0.0f)
                    return;
                float tv = float(tickValue);
                if (isHorizontal)
                {
                    appendBorder(mStagingVertices,
                                 tv, pixTop, tv, pixBot,
                                 color, 1, 0, halfW, 0, 1);
                }
                else
                {
                    appendBorder(mStagingVertices,
                                 pixLeft, tv, pixRight, tv,
                                 color, 0, 1, halfW, 1, 0);
                }
            };

            const QCPRange& range = axis->range();
            if (grid->zeroLinePen().style() != Qt::NoPen
                && range.lower < 0 && range.upper > 0)
            {
                emitGridLine(0.0, grid->zeroLinePen());
            }

            for (double tickVal : axis->tickVector())
                emitGridLine(tickVal, grid->pen());

            if (grid->subGridVisible())
            {
                for (double subTickVal : axis->subTickVector())
                    emitGridLine(subTickVal, grid->subGridPen());
            }
        }

        int groupVertexCount = mStagingVertices.size() / kFloatsPerVertex - groupVertexStart;
        if (groupVertexCount == 0)
            continue;

        int sx = int(ar->left() * dpr);
        int sy = int(ar->top() * dpr);
        int sw = int(ar->width() * dpr);
        int sh = int(ar->height() * dpr);
        sy = outputHeight - sy - sh;

        auto* ubo = mRhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, kUniformBufferSize);
        ubo->create();

        auto* srb = mRhi->newShaderResourceBindings();
        srb->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(
                0, QRhiShaderResourceBinding::VertexStage, ubo)
        });
        srb->create();

        DrawGroup group;
        group.axisRect = ar;
        group.vertexOffset = groupVertexStart;
        group.vertexCount = groupVertexCount;
        group.scissorRect = QRect(sx, sy, sw, sh);
        group.uniformBuffer = ubo;
        group.srb = srb;
        group.isGridLines = true;
        mDrawGroups.append(group);

        // --- Tick marks (second pass, per axis rect) ---
        int tickGroupVertexStart = mStagingVertices.size() / kFloatsPerVertex;

        for (auto* axis : axes)
            appendTickVertices(axis, ar, mStagingVertices);

        int tickGroupVertexCount = mStagingVertices.size() / kFloatsPerVertex - tickGroupVertexStart;
        if (tickGroupVertexCount > 0)
        {
            auto* tickUbo = mRhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, kUniformBufferSize);
            tickUbo->create();

            auto* tickSrb = mRhi->newShaderResourceBindings();
            tickSrb->setBindings({
                QRhiShaderResourceBinding::uniformBuffer(
                    0, QRhiShaderResourceBinding::VertexStage, tickUbo)
            });
            tickSrb->create();

            DrawGroup tickGroup;
            tickGroup.axisRect     = ar;
            tickGroup.vertexOffset = tickGroupVertexStart;
            tickGroup.vertexCount  = tickGroupVertexCount;
            tickGroup.scissorRect  = QRect();
            tickGroup.uniformBuffer = tickUbo;
            tickGroup.srb          = tickSrb;
            tickGroup.isGridLines  = false;
            mDrawGroups.append(tickGroup);
        }
    }

    mLastAxisRectBounds.clear();
    for (const auto& group : mDrawGroups)
        mLastAxisRectBounds[group.axisRect] = QRect(group.axisRect->left(), group.axisRect->top(),
                                                     group.axisRect->width(), group.axisRect->height());
}

void QCPGridRhiLayer::uploadResources(QRhiResourceUpdateBatch* updates,
                                       const QSize& outputSize, float dpr,
                                       bool isYUpInNDC)
{
    PROFILE_HERE_N("QCPGridRhiLayer::uploadResources");

    if (!mGeometryDirty)
    {
        for (auto* axis : mAxes)
        {
            auto it = mCachedTicks.constFind(axis);
            if (it == mCachedTicks.constEnd())
            {
                mGeometryDirty = true;
                break;
            }
            const auto& cached = it.value();
            QCPGrid* grid = axis->grid();
            if (cached.majorTicks != axis->tickVector()
                || cached.subTicks != axis->subTickVector()
                || cached.subGridVisible != grid->subGridVisible()
                || cached.gridColor != grid->pen().color().rgba()
                || cached.subGridColor != grid->subGridPen().color().rgba()
                || cached.zeroLineColor != grid->zeroLinePen().color().rgba()
                || cached.gridPenWidth != float(grid->pen().widthF())
                || cached.subGridPenWidth != float(grid->subGridPen().widthF())
                || cached.zeroLinePenWidth != float(grid->zeroLinePen().widthF())
                || cached.zeroLinePenStyle != grid->zeroLinePen().style()
                || cached.tickColor != axis->tickPen().color().rgba()
                || cached.subTickColor != axis->subTickPen().color().rgba()
                || cached.tickPenWidth != float(axis->tickPen().widthF())
                || cached.subTickPenWidth != float(axis->subTickPen().widthF())
                || cached.tickLengthOut != float(axis->tickLengthOut())
                || cached.tickLengthIn != float(axis->tickLengthIn())
                || cached.subTickLengthOut != float(axis->subTickLengthOut())
                || cached.subTickLengthIn != float(axis->subTickLengthIn())
                || cached.subTicksVisible != axis->subTicks()
                || cached.axisVisible != axis->visible()
                || cached.ticksVisible != axis->ticks()
                || cached.scaleType != int(axis->scaleType())
                || cached.rangeReversed != axis->rangeReversed())
            {
                mGeometryDirty = true;
                break;
            }
        }
    }

    if (!mGeometryDirty)
    {
        for (const auto& group : mDrawGroups)
        {
            QRect current(group.axisRect->left(), group.axisRect->top(),
                          group.axisRect->width(), group.axisRect->height());
            if (mLastAxisRectBounds.value(group.axisRect) != current)
            {
                mGeometryDirty = true;
                break;
            }
        }
    }

    // Tick marks are baked to pixels, so a pan that keeps the same tick set (no
    // rebuild triggered above) would otherwise leave them at stale positions.
    // Detect it separately and re-bake their pixels in place: rebuilding geometry
    // here would recreate every group's UBO/SRB, which the pan fast path avoids.
    bool tickPixelsStale = false;
    if (!mGeometryDirty)
    {
        for (auto* axis : mAxes)
        {
            const auto& cached = mCachedTicks.value(axis);
            if (cached.lastRange != axis->range())
            {
                tickPixelsStale = true;
                break;
            }
        }
    }

    if (!mGeometryDirty && tickPixelsStale)
    {
        for (auto& group : mDrawGroups)
        {
            if (group.isGridLines)
                continue;

            QVector<float> scratch;
            for (auto* axis : orderedAxesForRect(group.axisRect))
                appendTickVertices(axis, group.axisRect, scratch);

            if (scratch.size() != group.vertexCount * kFloatsPerVertex)
            {
                // Regenerated tick vertex count no longer matches the existing
                // group: fall back to a full rebuild this frame instead of
                // writing a mismatched slice into the vertex buffer.
                mGeometryDirty = true;
                break;
            }

            const int floatOffset = group.vertexOffset * kFloatsPerVertex;
            std::memcpy(mStagingVertices.data() + floatOffset, scratch.constData(),
                        scratch.size() * sizeof(float));
            updates->updateDynamicBuffer(mVertexBuffer,
                                          floatOffset * int(sizeof(float)),
                                          scratch.size() * int(sizeof(float)),
                                          mStagingVertices.constData() + floatOffset);
        }

        if (!mGeometryDirty)
        {
            for (auto* axis : mAxes)
                mCachedTicks[axis].lastRange = axis->range();
        }
    }

    if (mGeometryDirty)
    {
        rebuildGeometry(dpr, outputSize.height());
        mGeometryDirty = false;

        mCachedTicks.clear();
        for (auto* axis : mAxes)
        {
            QCPGrid* grid = axis->grid();
            CachedAxisTicks cached;
            cached.majorTicks = axis->tickVector();
            cached.subTicks = axis->subTickVector();
            cached.subGridVisible = grid->subGridVisible();
            cached.gridColor = grid->pen().color().rgba();
            cached.subGridColor = grid->subGridPen().color().rgba();
            cached.zeroLineColor = grid->zeroLinePen().color().rgba();
            cached.gridPenWidth = float(grid->pen().widthF());
            cached.subGridPenWidth = float(grid->subGridPen().widthF());
            cached.zeroLinePenWidth = float(grid->zeroLinePen().widthF());
            cached.zeroLinePenStyle = grid->zeroLinePen().style();
            cached.tickColor = axis->tickPen().color().rgba();
            cached.subTickColor = axis->subTickPen().color().rgba();
            cached.tickPenWidth = float(axis->tickPen().widthF());
            cached.subTickPenWidth = float(axis->subTickPen().widthF());
            cached.tickLengthOut = float(axis->tickLengthOut());
            cached.tickLengthIn = float(axis->tickLengthIn());
            cached.subTickLengthOut = float(axis->subTickLengthOut());
            cached.subTickLengthIn = float(axis->subTickLengthIn());
            cached.subTicksVisible = axis->subTicks();
            cached.axisVisible = axis->visible();
            cached.ticksVisible = axis->ticks();
            cached.scaleType = int(axis->scaleType());
            cached.rangeReversed = axis->rangeReversed();
            cached.lastRange = axis->range();
            mCachedTicks[axis] = cached;
        }

        if (!mStagingVertices.isEmpty())
        {
            int requiredSize = mStagingVertices.size() * sizeof(float);
            if (!mVertexBuffer || mVertexBufferSize < requiredSize)
            {
                delete mVertexBuffer;
                mVertexBuffer = mRhi->newBuffer(QRhiBuffer::Dynamic,
                                                 QRhiBuffer::VertexBuffer,
                                                 requiredSize);
                mVertexBuffer->create();
                mVertexBufferSize = requiredSize;
            }
            updates->updateDynamicBuffer(mVertexBuffer, 0, requiredSize,
                                          mStagingVertices.constData());
        }
    }

    for (auto& group : mDrawGroups)
    {
        QCPAxisRect* ar = group.axisRect;

        QCPAxis* hAxis = ar->axis(QCPAxis::atBottom);
        if (!hAxis) hAxis = ar->axis(QCPAxis::atTop);
        QCPAxis* vAxis = ar->axis(QCPAxis::atLeft);
        if (!vAxis) vAxis = ar->axis(QCPAxis::atRight);
        if (!hAxis || !vAxis)
            continue;

        float keyOffset, keyLength;
        if (!hAxis->rangeReversed())
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
        if (!vAxis->rangeReversed())
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
            float(hAxis->range().lower),
            float(hAxis->range().upper),
            keyOffset,
            keyLength,
            (hAxis->scaleType() == QCPAxis::stLogarithmic) ? 1.0f : 0.0f,
            float(vAxis->range().lower),
            float(vAxis->range().upper),
            valOffset,
            valLength,
            (vAxis->scaleType() == QCPAxis::stLogarithmic) ? 1.0f : 0.0f,
            0.0f, 0.0f
        };
        static_assert(sizeof(params) == kUniformBufferSize);
        updates->updateDynamicBuffer(group.uniformBuffer, 0, sizeof(params), &params);
    }
}

void QCPGridRhiLayer::renderGroups(QRhiCommandBuffer* cb, const QSize& outputSize, bool gridLines)
{
    if (!mPipeline || !mVertexBuffer || mDrawGroups.isEmpty())
        return;

    cb->setGraphicsPipeline(mPipeline);
    cb->setViewport({0, 0, float(outputSize.width()), float(outputSize.height())});

    const QRhiCommandBuffer::VertexInput vbufBinding(mVertexBuffer, 0);
    for (const auto& group : mDrawGroups)
    {
        if (group.isGridLines != gridLines)
            continue;
        // setShaderResources must precede setVertexInput — on Metal, QRhi offsets
        // vertex buffer indices by the number of SRB buffer bindings.
        cb->setShaderResources(group.srb);
        cb->setVertexInput(0, 1, &vbufBinding);
        if (!group.scissorRect.isNull())
            cb->setScissor({group.scissorRect.x(), group.scissorRect.y(),
                            group.scissorRect.width(), group.scissorRect.height()});
        else
            cb->setScissor({0, 0, outputSize.width(), outputSize.height()});
        cb->draw(group.vertexCount, 1, group.vertexOffset, 0);
    }
}

void QCPGridRhiLayer::renderGridLines(QRhiCommandBuffer* cb, const QSize& outputSize)
{
    PROFILE_HERE_N("QCPGridRhiLayer::renderGridLines");
    renderGroups(cb, outputSize, true);
}

void QCPGridRhiLayer::renderTickMarks(QRhiCommandBuffer* cb, const QSize& outputSize)
{
    PROFILE_HERE_N("QCPGridRhiLayer::renderTickMarks");
    renderGroups(cb, outputSize, false);
}
