#include "item-spanbase.h"

#include "../core.h"
#include "../painting/painter.h"
#include "../painting/span-rhi-layer.h"

QCPAbstractSpanItem::~QCPAbstractSpanItem()
{
    if (auto* layer = mParentPlot ? mParentPlot->spanRhiLayerIfExists() : nullptr)
        layer->unregisterSpan(this);
}

void QCPAbstractSpanItem::markRhiDirty()
{
    if (auto* layer = mParentPlot ? mParentPlot->spanRhiLayerIfExists() : nullptr)
        layer->markGeometryDirty();
}

void QCPAbstractSpanItem::setPen(const QPen& pen)
{
    if (pen == mPen)
        return;
    markDirty();
    mPen = pen;
    markRhiDirty();
}

void QCPAbstractSpanItem::setSelectedPen(const QPen& pen)
{
    if (pen == mSelectedPen)
        return;
    markDirty();
    mSelectedPen = pen;
    markRhiDirty();
}

void QCPAbstractSpanItem::setBrush(const QBrush& brush)
{
    if (brush == mBrush)
        return;
    markDirty();
    mBrush = brush;
    markRhiDirty();
}

void QCPAbstractSpanItem::setSelectedBrush(const QBrush& brush)
{
    if (brush == mSelectedBrush)
        return;
    markDirty();
    mSelectedBrush = brush;
    markRhiDirty();
}

void QCPAbstractSpanItem::setBorderPen(const QPen& pen)
{
    if (pen == mBorderPen)
        return;
    markDirty();
    mBorderPen = pen;
    markRhiDirty();
}

void QCPAbstractSpanItem::setSelectedBorderPen(const QPen& pen)
{
    if (pen == mSelectedBorderPen)
        return;
    markDirty();
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

void QCPAbstractSpanItem::setEdgePixel(QCPItemPosition* edge, Qt::Orientation orientation,
                                       double pixel)
{
    // the edge position marks its item, so no extra dirty call here
    const QPointF otherCoords = edge->coords();
    QPointF px = edge->pixelPosition();
    (orientation == Qt::Horizontal ? px.rx() : px.ry()) = pixel;
    edge->setPixelPosition(px);
    edge->setCoords(orientation == Qt::Horizontal ? QPointF(edge->coords().x(), otherCoords.y())
                                                  : QPointF(otherCoords.x(), edge->coords().y()));
}
