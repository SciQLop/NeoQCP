#include "item-vspan.h"

#include "../core.h"
#include "../painting/painter.h"
#include "../painting/span-rhi-layer.h"

QCPItemVSpan::QCPItemVSpan(QCustomPlot* parentPlot)
    : QCPAbstractSpanItem(parentPlot)
    , lowerEdge(createPosition(QLatin1String("lowerEdge")))
    , upperEdge(createPosition(QLatin1String("upperEdge")))
    , center(createAnchor(QLatin1String("center"), aiCenter))
{
    lowerEdge->setTypeX(QCPItemPosition::ptPlotCoords);
    lowerEdge->setTypeY(QCPItemPosition::ptAxisRectRatio);
    lowerEdge->setCoords(0, 0);

    upperEdge->setTypeX(QCPItemPosition::ptPlotCoords);
    upperEdge->setTypeY(QCPItemPosition::ptAxisRectRatio);
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

QCPRange QCPItemVSpan::range() const
{
    return QCPRange(lowerEdge->coords().x(), upperEdge->coords().x());
}

void QCPItemVSpan::setRange(const QCPRange& range)
{
    lowerEdge->setCoords(range.lower, 0);
    upperEdge->setCoords(range.upper, 1);
    emit rangeChanged(range);
    if (mParentPlot)
    {
        markRhiDirty();
        mParentPlot->replot(QCustomPlot::rpQueuedReplot);
    }
}

double QCPItemVSpan::selectTest(const QPointF& pos, bool onlySelectable, QVariant* details) const
{
    if (onlySelectable && !mSelectable)
        return -1;

    // Resolve through the positions so their type (plot coords, absolute pixels,
    // axis-rect pixels, ...) is honoured; coordToPixel() would reinterpret
    // already-pixel coords as data values.
    const double lowerPx = lowerEdge->pixelPosition().x();
    const double upperPx = upperEdge->pixelPosition().x();
    const QRectF axisRect = clipRect();
    const double left = qMin(lowerPx, upperPx);
    const double right = qMax(lowerPx, upperPx);

    const double tolerance = mParentPlot->selectionTolerance();

    const double distLower = qAbs(pos.x() - lowerPx);
    const double distUpper = qAbs(pos.x() - upperPx);

    if (distLower <= tolerance && axisRect.top() <= pos.y() && pos.y() <= axisRect.bottom())
    {
        if (details)
            details->setValue(static_cast<int>(hpLowerEdge));
        return distLower;
    }
    if (distUpper <= tolerance && axisRect.top() <= pos.y() && pos.y() <= axisRect.bottom())
    {
        if (details)
            details->setValue(static_cast<int>(hpUpperEdge));
        return distUpper;
    }

    bool filledRect = mBrush.style() != Qt::NoBrush && mBrush.color().alpha() != 0;
    if (filledRect && pos.x() >= left && pos.x() <= right
        && pos.y() >= axisRect.top() && pos.y() <= axisRect.bottom())
    {
        if (details)
            details->setValue(static_cast<int>(hpFill));
        return 0;
    }

    return -1;
}

void QCPItemVSpan::draw(QCPPainter* painter)
{
    if (tryRhiDraw(painter))
        return;

    const double lowerPx = lowerEdge->pixelPosition().x();
    const double upperPx = upperEdge->pixelPosition().x();
    const QRectF axRect = clipRect();

    const double left = qMin(lowerPx, upperPx);
    const double right = qMax(lowerPx, upperPx);
    QRectF spanRect(left, axRect.top(), right - left, axRect.height());

    if (!spanRect.intersects(axRect))
        return;

    // The band is filled and its edges are the border lines, as on screen (the GPU path): an
    // outline in the item pen framed every exported span along the axis rect edges.
    painter->setPen(Qt::NoPen);
    painter->setBrush(mainBrush());
    painter->drawRect(spanRect);

    painter->setPen(mainBorderPen());
    painter->drawLine(QPointF(lowerPx, axRect.top()), QPointF(lowerPx, axRect.bottom()));
    painter->drawLine(QPointF(upperPx, axRect.top()), QPointF(upperPx, axRect.bottom()));
}

QPointF QCPItemVSpan::anchorPixelPosition(int anchorId) const
{
    if (anchorId == aiCenter)
    {
        const double midX
            = (lowerEdge->pixelPosition().x() + upperEdge->pixelPosition().x()) * 0.5;
        const QRectF axRect = clipRect();
        return QPointF(midX, (axRect.top() + axRect.bottom()) * 0.5);
    }
    qDebug() << Q_FUNC_INFO << "invalid anchorId" << anchorId;
    return {};
}

void QCPItemVSpan::mousePressEvent(QMouseEvent* event, const QVariant& details)
{
    if (!mMovable)
    {
        event->ignore();
        return;
    }
    mDragPart = static_cast<HitPart>(details.toInt());
    mDragStartLower = lowerEdge->coords().x();
    mDragStartUpper = upperEdge->coords().x();
    event->accept();
}

void QCPItemVSpan::mouseMoveEvent(QMouseEvent* event, const QPointF& startPos)
{
    auto* keyAxis = lowerEdge->keyAxis();
    if (!keyAxis || mDragPart == hpNone)
        return;

    const double startCoord = keyAxis->pixelToCoord(startPos.x());
    const double currentCoord = keyAxis->pixelToCoord(event->pos().x());
    const double delta = currentCoord - startCoord;

    switch (mDragPart)
    {
        case hpLowerEdge:
            lowerEdge->setCoords(mDragStartLower + delta, 0);
            break;
        case hpUpperEdge:
            upperEdge->setCoords(mDragStartUpper + delta, 1);
            break;
        case hpFill:
            lowerEdge->setCoords(mDragStartLower + delta, 0);
            upperEdge->setCoords(mDragStartUpper + delta, 1);
            break;
        default:
            break;
    }
    emit rangeChanged(range());
    if (mParentPlot)
    {
        markRhiDirty();
        mParentPlot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void QCPItemVSpan::mouseReleaseEvent([[maybe_unused]] QMouseEvent* event,
                                     [[maybe_unused]] const QPointF& startPos)
{
    mDragPart = hpNone;
}
