#include "item-spanbase.h"

#include "../core.h"

QCPAbstractSpanItem::~QCPAbstractSpanItem() = default;

void QCPAbstractSpanItem::setPen(const QPen& pen)
{
    if (pen == mPen)
        return;
    markDirty();
    mPen = pen;
}

void QCPAbstractSpanItem::setSelectedPen(const QPen& pen)
{
    if (pen == mSelectedPen)
        return;
    markDirty();
    mSelectedPen = pen;
}

void QCPAbstractSpanItem::setBrush(const QBrush& brush)
{
    if (brush == mBrush)
        return;
    markDirty();
    mBrush = brush;
}

void QCPAbstractSpanItem::setSelectedBrush(const QBrush& brush)
{
    if (brush == mSelectedBrush)
        return;
    markDirty();
    mSelectedBrush = brush;
}

void QCPAbstractSpanItem::setBorderPen(const QPen& pen)
{
    if (pen == mBorderPen)
        return;
    markDirty();
    mBorderPen = pen;
}

void QCPAbstractSpanItem::setSelectedBorderPen(const QPen& pen)
{
    if (pen == mSelectedBorderPen)
        return;
    markDirty();
    mSelectedBorderPen = pen;
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
