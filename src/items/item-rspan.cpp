#include "item-rspan.h"

#include "../core.h"
#include <array>

#include "../painting/draw-context.h"

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
    if (keyRange() == range)
        return;
    markDirty();
    leftEdge->setCoords(range.lower, leftEdge->coords().y());
    rightEdge->setCoords(range.upper, rightEdge->coords().y());
    // Emit last: a slot may delete this span.
    qcpEmit(this, mParentPlot, &QCPItemRSpan::keyRangeChanged, range);
}

void QCPItemRSpan::setValueRange(const QCPRange& range)
{
    if (valueRange() == range)
        return;
    markDirty();
    bottomEdge->setCoords(bottomEdge->coords().x(), range.lower);
    topEdge->setCoords(topEdge->coords().x(), range.upper);
    // Emit last: a slot may delete this span.
    qcpEmit(this, mParentPlot, &QCPItemRSpan::valueRangeChanged, range);
}

std::optional<double> QCPItemRSpan::hitTest(QCP::Pixels at, QVariant* details) const
{
    const QPointF pos(at.x, at.y);

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
        return 0.0;
    }

    return std::nullopt;
}

void QCPItemRSpan::draw(QCPDrawContext& context) const
{
    // A corner is data only when both of its edges are: an edge in pixels or on an anchor is
    // placed against the axis rect, which moves, so the recording cannot outlive the frame.
    const auto dataOf = [](const QCPItemPosition* edge) -> std::optional<QCP::Data>
    {
        [[maybe_unused]] bool snapshot = false;
        const QCP::Position at = edge->asPosition(&snapshot);
        if (const auto* data = std::get_if<QCP::Data>(&at))
            return *data;
        return std::nullopt;
    };
    const std::optional<QCP::Data> left = dataOf(leftEdge);
    const std::optional<QCP::Data> right = dataOf(rightEdge);
    const std::optional<QCP::Data> top = dataOf(topEdge);
    const std::optional<QCP::Data> bottom = dataOf(bottomEdge);
    const bool allData = left && right && top && bottom;
    if (!allData)
        context.recordEveryFrame();

    // A corner takes its x from a vertical edge and its y from a horizontal one; without the data
    // form it is the pair of pixels those edges sit at.
    const auto at = [&](const std::optional<QCP::Data>& x, const std::optional<QCP::Data>& y,
                        const QCPItemPosition* xEdge, const QCPItemPosition* yEdge)
        -> QCP::Position
    {
        if (allData)
            return QCP::Data { x->x, y->y };
        return QCP::Pixels { xEdge->pixelPosition().x(), yEdge->pixelPosition().y() };
    };

    const QCP::Position leftTop = at(left, top, leftEdge, topEdge);
    const QCP::Position leftBottom = at(left, bottom, leftEdge, bottomEdge);
    const QCP::Position rightTop = at(right, top, rightEdge, topEdge);
    const QCP::Position rightBottom = at(right, bottom, rightEdge, bottomEdge);
    context.rect(leftTop, rightBottom, QPen(Qt::NoPen), mainBrush());

    // The band is filled and its edges are the border lines, as on screen (the GPU path): an
    // outline in the item pen framed every exported span along the axis rect edges.
    const QPen border = mainBorderPen();
    context.polyline(std::array<QCP::Position, 2> { leftTop, leftBottom }, border);
    context.polyline(std::array<QCP::Position, 2> { rightTop, rightBottom }, border);
    context.polyline(std::array<QCP::Position, 2> { leftTop, rightTop }, border);
    context.polyline(std::array<QCP::Position, 2> { leftBottom, rightBottom }, border);
}

QPointF QCPItemRSpan::anchorPixelPosition(int anchorId) const
{
    QCPAxis* keyAxis = leftEdge->keyAxis();
    QCPAxis* valAxis = leftEdge->valueAxis();
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
    mDragStartLeft = leftEdge->pixelPosition().x();
    mDragStartRight = rightEdge->pixelPosition().x();
    mDragStartTop = topEdge->pixelPosition().y();
    mDragStartBottom = bottomEdge->pixelPosition().y();
    event->accept();
}

void QCPItemRSpan::mouseMoveEvent(QMouseEvent* event, const QPointF& startPos)
{
    if (mDragPart == hpNone)
        return;

    const double dx = event->pos().x() - startPos.x();
    const double dy = event->pos().y() - startPos.y();

    switch (mDragPart)
    {
        case hpLeft:
            setEdgePixel(leftEdge, Qt::Horizontal, mDragStartLeft + dx);
            break;
        case hpRight:
            setEdgePixel(rightEdge, Qt::Horizontal, mDragStartRight + dx);
            break;
        case hpTop:
            setEdgePixel(topEdge, Qt::Vertical, mDragStartTop + dy);
            break;
        case hpBottom:
            setEdgePixel(bottomEdge, Qt::Vertical, mDragStartBottom + dy);
            break;
        case hpFill:
            setEdgePixel(leftEdge, Qt::Horizontal, mDragStartLeft + dx);
            setEdgePixel(rightEdge, Qt::Horizontal, mDragStartRight + dx);
            setEdgePixel(topEdge, Qt::Vertical, mDragStartTop + dy);
            setEdgePixel(bottomEdge, Qt::Vertical, mDragStartBottom + dy);
            break;
        default:
            break;
    }

    if (mParentPlot)
        mParentPlot->replot(QCustomPlot::rpQueuedReplot);
    emitDraggedRanges();
}

// A drag moves both ranges, and a slot may delete this span: deliver at the outermost call, reading
// each value when it is delivered.
void QCPItemRSpan::emitDraggedRanges()
{
    const bool keyMoved = mDragPart == hpLeft || mDragPart == hpRight || mDragPart == hpFill;
    const bool valueMoved = mDragPart == hpTop || mDragPart == hpBottom || mDragPart == hpFill;
    const QCPEmissionScope scope(mParentPlot);
    if (keyMoved)
        qcpEmit(this, mParentPlot, &QCPItemRSpan::keyRangeChanged, keyRange());
    if (valueMoved)
        qcpEmitCurrent(this, mParentPlot, &QCPItemRSpan::valueRangeChanged,
                       [this] { return valueRange(); });
}

void QCPItemRSpan::mouseReleaseEvent([[maybe_unused]] QMouseEvent* event,
                                     [[maybe_unused]] const QPointF& startPos)
{
    mDragPart = hpNone;
}
