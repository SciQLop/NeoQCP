#include "item-spanbase.h"

#include "../core.h"

/*! \fn void QCPAbstractSpanItem::brushChanged(const QBrush& brush)

  This signal is emitted when \ref brush changes, after the plot delivers its deferred signals.
*/

/*! \fn void QCPAbstractSpanItem::selectedBrushChanged(const QBrush& selectedBrush)

  This signal is emitted when \ref selectedBrush changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPAbstractSpanItem::borderPenChanged(const QPen& borderPen)

  This signal is emitted when \ref borderPen changes, after the plot delivers its deferred signals.
*/

/*! \fn void QCPAbstractSpanItem::selectedBorderPenChanged(const QPen& selectedBorderPen)

  This signal is emitted when \ref selectedBorderPen changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPAbstractSpanItem::movableChanged(bool movable)

  This signal is emitted when \ref movable changes, after the plot delivers its deferred signals.
*/

QCPAbstractSpanItem::~QCPAbstractSpanItem() = default;

void QCPAbstractSpanItem::setBrush(const QBrush& brush)
{
    if (brush == mBrush)
        return;
    markDirty();
    mBrush = brush;
    qcpEmit(this, mParentPlot, &QCPAbstractSpanItem::brushChanged, mBrush);
}

void QCPAbstractSpanItem::setSelectedBrush(const QBrush& brush)
{
    if (brush == mSelectedBrush)
        return;
    markDirty();
    mSelectedBrush = brush;
    qcpEmit(this, mParentPlot, &QCPAbstractSpanItem::selectedBrushChanged, mSelectedBrush);
}

void QCPAbstractSpanItem::setBorderPen(const QPen& pen)
{
    if (pen == mBorderPen)
        return;
    markDirty();
    mBorderPen = pen;
    qcpEmit(this, mParentPlot, &QCPAbstractSpanItem::borderPenChanged, mBorderPen);
}

void QCPAbstractSpanItem::setSelectedBorderPen(const QPen& pen)
{
    if (pen == mSelectedBorderPen)
        return;
    markDirty();
    mSelectedBorderPen = pen;
    qcpEmit(this, mParentPlot, &QCPAbstractSpanItem::selectedBorderPenChanged, mSelectedBorderPen);
}

void QCPAbstractSpanItem::setMovable(bool movable)
{
    if (movable == mMovable)
        return;
    mMovable = movable;
    qcpEmit(this, mParentPlot, &QCPAbstractSpanItem::movableChanged, mMovable);
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
