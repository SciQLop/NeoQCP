#include "item-spanbase.h"

#include "../core.h"

/*! \fn void QCPSpanItem::brushChanged(const QBrush& brush)

  This signal is emitted when \ref brush changes, after the plot delivers its deferred signals.
*/

/*! \fn void QCPSpanItem::selectedBrushChanged(const QBrush& selectedBrush)

  This signal is emitted when \ref selectedBrush changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPSpanItem::borderPenChanged(const QPen& borderPen)

  This signal is emitted when \ref borderPen changes, after the plot delivers its deferred signals.
*/

/*! \fn void QCPSpanItem::selectedBorderPenChanged(const QPen& selectedBorderPen)

  This signal is emitted when \ref selectedBorderPen changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPSpanItem::movableChanged(bool movable)

  This signal is emitted when \ref movable changes, after the plot delivers its deferred signals.
*/

QCPSpanItem::~QCPSpanItem() = default;

void QCPSpanItem::setBrush(const QBrush& brush)
{
    if (brush == mBrush)
        return;
    markDirty();
    mBrush = brush;
    qcpEmit(this, mParentPlot, &QCPSpanItem::brushChanged, mBrush);
}

void QCPSpanItem::setSelectedBrush(const QBrush& brush)
{
    if (brush == mSelectedBrush)
        return;
    markDirty();
    mSelectedBrush = brush;
    qcpEmit(this, mParentPlot, &QCPSpanItem::selectedBrushChanged, mSelectedBrush);
}

void QCPSpanItem::setBorderPen(const QPen& pen)
{
    if (pen == mBorderPen)
        return;
    markDirty();
    mBorderPen = pen;
    qcpEmit(this, mParentPlot, &QCPSpanItem::borderPenChanged, mBorderPen);
}

void QCPSpanItem::setSelectedBorderPen(const QPen& pen)
{
    if (pen == mSelectedBorderPen)
        return;
    markDirty();
    mSelectedBorderPen = pen;
    qcpEmit(this, mParentPlot, &QCPSpanItem::selectedBorderPenChanged, mSelectedBorderPen);
}

void QCPSpanItem::setMovable(bool movable)
{
    if (movable == mMovable)
        return;
    mMovable = movable;
    qcpEmit(this, mParentPlot, &QCPSpanItem::movableChanged, mMovable);
}

void QCPSpanItem::setEdgePixel(QCPItemPosition* edge, Qt::Orientation orientation,
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
