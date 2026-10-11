#include "item-spanbase.h"

#include "../core.h"
#include "../painting/painter.h"
#include "../painting/span-rhi-layer.h"

QCPAbstractSpanItem::~QCPAbstractSpanItem()
{
    if (mParentPlot && mParentPlot->spanRhiLayer())
        mParentPlot->spanRhiLayer()->unregisterSpan(this);
}

void QCPAbstractSpanItem::markRhiDirty()
{
    if (mParentPlot && mParentPlot->spanRhiLayer())
        mParentPlot->spanRhiLayer()->markGeometryDirty();
}

void QCPAbstractSpanItem::setPen(const QPen& pen)
{
    mPen = pen;
    markRhiDirty();
}

void QCPAbstractSpanItem::setSelectedPen(const QPen& pen)
{
    mSelectedPen = pen;
    markRhiDirty();
}

void QCPAbstractSpanItem::setBrush(const QBrush& brush)
{
    mBrush = brush;
    markRhiDirty();
}

void QCPAbstractSpanItem::setSelectedBrush(const QBrush& brush)
{
    mSelectedBrush = brush;
    markRhiDirty();
}

void QCPAbstractSpanItem::setBorderPen(const QPen& pen)
{
    mBorderPen = pen;
    markRhiDirty();
}

void QCPAbstractSpanItem::setSelectedBorderPen(const QPen& pen)
{
    mSelectedBorderPen = pen;
    markRhiDirty();
}

bool QCPAbstractSpanItem::tryRhiDraw(QCPPainter* painter)
{
    if (auto* layer = mParentPlot ? mParentPlot->spanRhiLayer() : nullptr)
    {
        // The GPU fill is one flat colour: a gradient or pattern brush is painted with QPainter.
        const Qt::BrushStyle fill = mainBrush().style();
        if (fill != Qt::NoBrush && fill != Qt::SolidPattern)
        {
            layer->unregisterSpan(this);
            return false;
        }
        layer->registerSpan(this);
        if (!painter->isExporting())
            return true;
    }
    return false;
}
