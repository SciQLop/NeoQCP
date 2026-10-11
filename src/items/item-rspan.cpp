#include "item-rspan.h"

#include "../core.h"
#include "../painting/painter.h"
#include "../painting/span-rhi-layer.h"

QCPItemRSpan::QCPItemRSpan(QCustomPlot* parentPlot)
    : QCPAbstractSpanItem(parentPlot)
    , leftEdge(createPosition(QLatin1String("leftEdge")))
    , rightEdge(createPosition(QLatin1String("rightEdge")))
    , topEdge(createPosition(QLatin1String("topEdge")))
    , bottomEdge(createPosition(QLatin1String("bottomEdge")))
    , topLeft(createAnchor(QLatin1String("topLeft"), aiTopLeft))
    , topRight(createAnchor(QLatin1String("topRight"), aiTopRight))
    , bottomRight(createAnchor(QLatin1String("bottomRight"), aiBottomRight))
    , bottomLeft(createAnchor(QLatin1String("bottomLeft"), aiBottomLeft))
    , center(createAnchor(QLatin1String("center"), aiCenter))
{
    leftEdge->setType(QCPItemPosition::ptPlotCoords);
    leftEdge->setCoords(0, 0);

    rightEdge->setType(QCPItemPosition::ptPlotCoords);
    rightEdge->setCoords(1, 0);

    topEdge->setType(QCPItemPosition::ptPlotCoords);
    topEdge->setCoords(0, 1);

    bottomEdge->setType(QCPItemPosition::ptPlotCoords);
    bottomEdge->setCoords(0, 0);

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

QCPRange QCPItemRSpan::keyRange() const
{
    return QCPRange(leftEdge->coords().x(), rightEdge->coords().x());
}

QCPRange QCPItemRSpan::valueRange() const
{
    return QCPRange(bottomEdge->coords().y(), topEdge->coords().y());
}

void QCPItemRSpan::setKeyRange(const QCPRange& range)
{
    leftEdge->setCoords(range.lower, leftEdge->coords().y());
    rightEdge->setCoords(range.upper, rightEdge->coords().y());
    emit keyRangeChanged(range);
    if (mParentPlot)
    {
        markRhiDirty();
        mParentPlot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void QCPItemRSpan::setValueRange(const QCPRange& range)
{
    bottomEdge->setCoords(bottomEdge->coords().x(), range.lower);
    topEdge->setCoords(topEdge->coords().x(), range.upper);
    emit valueRangeChanged(range);
    if (mParentPlot)
    {
        markRhiDirty();
        mParentPlot->replot(QCustomPlot::rpQueuedReplot);
    }
}

double QCPItemRSpan::selectTest(const QPointF& pos, bool onlySelectable, QVariant* details) const
{
    if (onlySelectable && !mSelectable)
        return -1;

    auto* keyAxis = leftEdge->keyAxis();
    auto* valAxis = leftEdge->valueAxis();
    if (!keyAxis || !valAxis)
        return -1;

    // Resolve through the positions so their type (plot coords, absolute pixels,
    // axis-rect pixels, ...) is honoured; coordToPixel() would reinterpret
    // already-pixel coords as data values.
    const double leftPx = leftEdge->pixelPosition().x();
    const double rightPx = rightEdge->pixelPosition().x();
    const double topPx = topEdge->pixelPosition().y();
    const double bottomPx = bottomEdge->pixelPosition().y();

    const double pxLeft = qMin(leftPx, rightPx);
    const double pxRight = qMax(leftPx, rightPx);
    const double pxTop = qMin(topPx, bottomPx);
    const double pxBottom = qMax(topPx, bottomPx);

    const double tolerance = mParentPlot->selectionTolerance();

    const bool inXBand = pos.x() >= pxLeft - tolerance && pos.x() <= pxRight + tolerance;
    const bool inYBand = pos.y() >= pxTop - tolerance && pos.y() <= pxBottom + tolerance;

    const double distLeft = qAbs(pos.x() - leftPx);
    if (distLeft <= tolerance && inYBand)
    {
        if (details)
            details->setValue(static_cast<int>(hpLeft));
        return distLeft;
    }

    const double distRight = qAbs(pos.x() - rightPx);
    if (distRight <= tolerance && inYBand)
    {
        if (details)
            details->setValue(static_cast<int>(hpRight));
        return distRight;
    }

    const double distTop = qAbs(pos.y() - topPx);
    if (distTop <= tolerance && inXBand)
    {
        if (details)
            details->setValue(static_cast<int>(hpTop));
        return distTop;
    }

    const double distBottom = qAbs(pos.y() - bottomPx);
    if (distBottom <= tolerance && inXBand)
    {
        if (details)
            details->setValue(static_cast<int>(hpBottom));
        return distBottom;
    }

    bool filledRect = mBrush.style() != Qt::NoBrush && mBrush.color().alpha() != 0;
    if (filledRect && pos.x() >= pxLeft && pos.x() <= pxRight
        && pos.y() >= pxTop && pos.y() <= pxBottom)
    {
        if (details)
            details->setValue(static_cast<int>(hpFill));
        return 0;
    }

    return -1;
}

void QCPItemRSpan::draw(QCPPainter* painter)
{
    if (tryRhiDraw(painter))
        return;

    auto* keyAxis = leftEdge->keyAxis();
    auto* valAxis = leftEdge->valueAxis();
    if (!keyAxis || !valAxis)
        return;

    const double leftPx = leftEdge->pixelPosition().x();
    const double rightPx = rightEdge->pixelPosition().x();
    const double topPx = topEdge->pixelPosition().y();
    const double bottomPx = bottomEdge->pixelPosition().y();

    const double pxLeft = qMin(leftPx, rightPx);
    const double pxRight = qMax(leftPx, rightPx);
    const double pxTop = qMin(topPx, bottomPx);
    const double pxBottom = qMax(topPx, bottomPx);

    QRectF spanRect(pxLeft, pxTop, pxRight - pxLeft, pxBottom - pxTop);
    const QRectF axRect = clipRect();

    if (!spanRect.intersects(axRect))
        return;

    // The band is filled and its edges are the border lines, as on screen (the GPU path): an
    // outline in the item pen framed every exported span along the axis rect edges.
    painter->setPen(Qt::NoPen);
    painter->setBrush(mainBrush());
    painter->drawRect(spanRect);

    painter->setPen(mainBorderPen());
    painter->drawLine(QPointF(leftPx, pxTop), QPointF(leftPx, pxBottom));
    painter->drawLine(QPointF(rightPx, pxTop), QPointF(rightPx, pxBottom));
    painter->drawLine(QPointF(pxLeft, topPx), QPointF(pxRight, topPx));
    painter->drawLine(QPointF(pxLeft, bottomPx), QPointF(pxRight, bottomPx));
}

QPointF QCPItemRSpan::anchorPixelPosition(int anchorId) const
{
    auto* keyAxis = leftEdge->keyAxis();
    auto* valAxis = leftEdge->valueAxis();
    if (!keyAxis || !valAxis)
        return {};

    const double leftPx = leftEdge->pixelPosition().x();
    const double rightPx = rightEdge->pixelPosition().x();
    const double topPx = topEdge->pixelPosition().y();
    const double bottomPx = bottomEdge->pixelPosition().y();

    switch (anchorId)
    {
        case aiTopLeft:     return QPointF(leftPx, topPx);
        case aiTopRight:    return QPointF(rightPx, topPx);
        case aiBottomRight: return QPointF(rightPx, bottomPx);
        case aiBottomLeft:  return QPointF(leftPx, bottomPx);
        case aiCenter:      return QPointF((leftPx + rightPx) * 0.5, (topPx + bottomPx) * 0.5);
    }
    qDebug() << Q_FUNC_INFO << "invalid anchorId" << anchorId;
    return {};
}

void QCPItemRSpan::mousePressEvent(QMouseEvent* event, const QVariant& details)
{
    if (!mMovable)
    {
        event->ignore();
        return;
    }
    mDragPart = static_cast<HitPart>(details.toInt());
    mDragStartLeft = leftEdge->coords().x();
    mDragStartRight = rightEdge->coords().x();
    mDragStartTop = topEdge->coords().y();
    mDragStartBottom = bottomEdge->coords().y();
    event->accept();
}

void QCPItemRSpan::mouseMoveEvent(QMouseEvent* event, const QPointF& startPos)
{
    auto* keyAxis = leftEdge->keyAxis();
    auto* valAxis = leftEdge->valueAxis();
    if (!keyAxis || !valAxis || mDragPart == hpNone)
        return;

    const double startKeyCoord = keyAxis->pixelToCoord(startPos.x());
    const double currentKeyCoord = keyAxis->pixelToCoord(event->pos().x());
    const double keyDelta = currentKeyCoord - startKeyCoord;

    const double startValCoord = valAxis->pixelToCoord(startPos.y());
    const double currentValCoord = valAxis->pixelToCoord(event->pos().y());
    const double valDelta = currentValCoord - startValCoord;

    switch (mDragPart)
    {
        case hpLeft:
            leftEdge->setCoords(mDragStartLeft + keyDelta, leftEdge->coords().y());
            break;
        case hpRight:
            rightEdge->setCoords(mDragStartRight + keyDelta, rightEdge->coords().y());
            break;
        case hpTop:
            topEdge->setCoords(topEdge->coords().x(), mDragStartTop + valDelta);
            break;
        case hpBottom:
            bottomEdge->setCoords(bottomEdge->coords().x(), mDragStartBottom + valDelta);
            break;
        case hpFill:
            leftEdge->setCoords(mDragStartLeft + keyDelta, leftEdge->coords().y());
            rightEdge->setCoords(mDragStartRight + keyDelta, rightEdge->coords().y());
            topEdge->setCoords(topEdge->coords().x(), mDragStartTop + valDelta);
            bottomEdge->setCoords(bottomEdge->coords().x(), mDragStartBottom + valDelta);
            break;
        default:
            break;
    }

    if (mDragPart == hpLeft || mDragPart == hpRight || mDragPart == hpFill)
        emit keyRangeChanged(keyRange());
    if (mDragPart == hpTop || mDragPart == hpBottom || mDragPart == hpFill)
        emit valueRangeChanged(valueRange());

    if (mParentPlot)
    {
        markRhiDirty();
        mParentPlot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void QCPItemRSpan::mouseReleaseEvent([[maybe_unused]] QMouseEvent* event,
                                     [[maybe_unused]] const QPointF& startPos)
{
    mDragPart = hpNone;
}
