#include "item-hspan.h"

#include "../core.h"
#include "../painting/painter.h"
#include "../painting/span-rhi-layer.h"

QCPItemHSpan::QCPItemHSpan(QCustomPlot* parentPlot)
    : QCPAbstractSpanItem(parentPlot)
    , lowerEdge(createPosition(QLatin1String("lowerEdge")))
    , upperEdge(createPosition(QLatin1String("upperEdge")))
    , center(createAnchor(QLatin1String("center"), aiCenter))
{
    lowerEdge->setTypeY(QCPItemPosition::ptPlotCoords);
    lowerEdge->setTypeX(QCPItemPosition::ptAxisRectRatio);
    lowerEdge->setCoords(0, 0);

    upperEdge->setTypeY(QCPItemPosition::ptPlotCoords);
    upperEdge->setTypeX(QCPItemPosition::ptAxisRectRatio);
    upperEdge->setCoords(1, 1);

    setPen(QPen(Qt::black));
    setSelectedPen(QPen(Qt::blue, 2));
    setBrush(QBrush(QColor(0, 0, 255, 50)));
    setSelectedBrush(QBrush(QColor(0, 0, 255, 80)));
    setBorderPen(QPen(Qt::black, 2));
    setSelectedBorderPen(QPen(Qt::blue, 2));

    connect(this, &QCPAbstractItem::selectionChanged, this, [this](bool) {
        markRhiDirty();
    });
}

QCPRange QCPItemHSpan::range() const
{
    return QCPRange(lowerEdge->coords().y(), upperEdge->coords().y());
}

void QCPItemHSpan::setRange(const QCPRange& range)
{
    if (range == this->range())
        return;
    markDirty();
    lowerEdge->setCoords(0, range.lower);
    upperEdge->setCoords(1, range.upper);
    // Emit last: a slot may delete this span.
    if (mParentPlot)
        markRhiDirty();
    emit rangeChanged(range);
}

double QCPItemHSpan::selectTest(const QPointF& pos, bool onlySelectable, QVariant* details) const
{
    if (onlySelectable && !mSelectable)
        return -1;

    // Resolve through the positions so their type (plot coords, absolute pixels,
    // axis-rect pixels, ...) is honoured; coordToPixel() would reinterpret
    // already-pixel coords as data values.
    const double lowerPx = lowerEdge->pixelPosition().y();
    const double upperPx = upperEdge->pixelPosition().y();
    const QRectF axisRect = clipRect();
    const double top = qMin(lowerPx, upperPx);
    const double bottom = qMax(lowerPx, upperPx);

    const double tolerance = mParentPlot->selectionTolerance();

    const double distLower = qAbs(pos.y() - lowerPx);
    const double distUpper = qAbs(pos.y() - upperPx);

    if (distLower <= tolerance && axisRect.left() <= pos.x() && pos.x() <= axisRect.right())
    {
        if (details)
            details->setValue(static_cast<int>(hpLowerEdge));
        return distLower;
    }
    if (distUpper <= tolerance && axisRect.left() <= pos.x() && pos.x() <= axisRect.right())
    {
        if (details)
            details->setValue(static_cast<int>(hpUpperEdge));
        return distUpper;
    }

    bool filledRect = mBrush.style() != Qt::NoBrush && mBrush.color().alpha() != 0;
    if (filledRect && pos.y() >= top && pos.y() <= bottom
        && pos.x() >= axisRect.left() && pos.x() <= axisRect.right())
    {
        if (details)
            details->setValue(static_cast<int>(hpFill));
        return 0;
    }

    return -1;
}

void QCPItemHSpan::draw(QCPPainter* painter)
{
    if (tryRhiDraw(painter))
        return;

    const double lowerPx = lowerEdge->pixelPosition().y();
    const double upperPx = upperEdge->pixelPosition().y();
    const QRectF axRect = clipRect();

    const double top = qMin(lowerPx, upperPx);
    const double bottom = qMax(lowerPx, upperPx);
    QRectF spanRect(axRect.left(), top, axRect.width(), bottom - top);

    if (!spanRect.intersects(axRect))
        return;

    // The band is filled and its edges are the border lines, as on screen (the GPU path): an
    // outline in the item pen framed every exported span along the axis rect edges.
    painter->setPen(Qt::NoPen);
    painter->setBrush(mainBrush());
    painter->drawRect(spanRect);

    painter->setPen(mainBorderPen());
    painter->drawLine(QPointF(axRect.left(), lowerPx), QPointF(axRect.right(), lowerPx));
    painter->drawLine(QPointF(axRect.left(), upperPx), QPointF(axRect.right(), upperPx));
}

QPointF QCPItemHSpan::anchorPixelPosition(int anchorId) const
{
    if (anchorId == aiCenter)
    {
        const double midY
            = (lowerEdge->pixelPosition().y() + upperEdge->pixelPosition().y()) * 0.5;
        const QRectF axRect = clipRect();
        return QPointF((axRect.left() + axRect.right()) * 0.5, midY);
    }
    qDebug() << Q_FUNC_INFO << "invalid anchorId" << anchorId;
    return {};
}

void QCPItemHSpan::mousePressEvent(QMouseEvent* event, const QVariant& details)
{
    if (!mMovable)
    {
        event->ignore();
        return;
    }
    mDragPart = static_cast<HitPart>(details.toInt());
    mDragStartLower = lowerEdge->pixelPosition().y();
    mDragStartUpper = upperEdge->pixelPosition().y();
    event->accept();
}

void QCPItemHSpan::mouseMoveEvent(QMouseEvent* event, const QPointF& startPos)
{
    if (mDragPart == hpNone)
        return;

    const double delta = event->pos().y() - startPos.y();

    switch (mDragPart)
    {
        case hpLowerEdge:
            setEdgePixel(lowerEdge, Qt::Vertical, mDragStartLower + delta);
            break;
        case hpUpperEdge:
            setEdgePixel(upperEdge, Qt::Vertical, mDragStartUpper + delta);
            break;
        case hpFill:
            setEdgePixel(lowerEdge, Qt::Vertical, mDragStartLower + delta);
            setEdgePixel(upperEdge, Qt::Vertical, mDragStartUpper + delta);
            break;
        default:
            break;
    }
    // Emit last: a slot may delete this span.
    if (mParentPlot)
    {
        markRhiDirty();
        mParentPlot->replot(QCustomPlot::rpQueuedReplot);
    }
    emit rangeChanged(range());
}

void QCPItemHSpan::mouseReleaseEvent([[maybe_unused]] QMouseEvent* event,
                                     [[maybe_unused]] const QPointF& startPos)
{
    mDragPart = hpNone;
}
