#include "item-span.h"

#include <array>

#include "../core.h"
#include "../painting/draw-context.h"

namespace
{

//! The edge's position with the covered side pinned to the axis rect ratio \a ratio.
QCP::Position pinnedToRatio(const QCPItemPosition* edge, Qt::Orientation orientation, double ratio)
{
    [[maybe_unused]] bool snapshot = false;
    const QCP::Position at = edge->asPosition(&snapshot);
    if (orientation == Qt::Vertical)
    {
        if (const auto* pinned = std::get_if<QCP::DataXRatioY>(&at))
            return QCP::DataXRatioY { pinned->x, ratio };
        if (const auto* data = std::get_if<QCP::Data>(&at))
            return QCP::DataXRatioY { data->x, ratio };
    }
    else
    {
        if (const auto* pinned = std::get_if<QCP::RatioXDataY>(&at))
            return QCP::RatioXDataY { ratio, pinned->y };
        if (const auto* data = std::get_if<QCP::Data>(&at))
            return QCP::RatioXDataY { ratio, data->y };
    }
    return at;
}

//! Whether the position is resolved against the axis rect ratio, so a pan re-resolves it.
bool pinnedToTheRatio(const QCP::Position& at)
{
    return std::holds_alternative<QCP::DataXRatioY>(at)
        || std::holds_alternative<QCP::RatioXDataY>(at);
}

} // namespace

QCPItemSpan::QCPItemSpan(QCustomPlot* parentPlot, Qt::Orientation orientation)
        : QCPAbstractSpanItem(parentPlot)
        , mOrientation(orientation)
        , lowerEdge(createPosition(QLatin1String("lowerEdge")))
        , upperEdge(createPosition(QLatin1String("upperEdge")))
        , center(createAnchor(QLatin1String("center"), aiCenter))
{
    const bool vertical = mOrientation == Qt::Vertical;
    for (QCPItemPosition* edge : { lowerEdge, upperEdge })
    {
        edge->setTypeX(vertical ? QCPItemPosition::ptPlotCoords : QCPItemPosition::ptAxisRectRatio);
        edge->setTypeY(vertical ? QCPItemPosition::ptAxisRectRatio : QCPItemPosition::ptPlotCoords);
    }
    lowerEdge->setCoords(0, 0);
    upperEdge->setCoords(1, 1);

    setBrush(QBrush(QColor(0, 0, 255, 50)));
    setSelectedBrush(QBrush(QColor(0, 0, 255, 80)));
    setBorderPen(QPen(Qt::black, 2));
    setSelectedBorderPen(QPen(Qt::blue, 2));
}

double QCPItemSpan::rangePixel(const QCPItemPosition* edge) const
{
    const QPointF pixels = edge->pixelPosition();
    return mOrientation == Qt::Vertical ? pixels.x() : pixels.y();
}

QCPRange QCPItemSpan::range() const
{
    const QPointF lower = lowerEdge->coords();
    const QPointF upper = upperEdge->coords();
    const bool vertical = mOrientation == Qt::Vertical;
    return QCPRange(vertical ? lower.x() : lower.y(), vertical ? upper.x() : upper.y());
}

void QCPItemSpan::setRange(const QCPRange& range)
{
    if (range == this->range())
        return;
    markDirty();
    const bool vertical = mOrientation == Qt::Vertical;
    lowerEdge->setCoords(vertical ? range.lower : 0, vertical ? 0 : range.lower);
    upperEdge->setCoords(vertical ? range.upper : 1, vertical ? 1 : range.upper);
    // Emit last: a slot may delete this span.
    qcpEmit(this, mParentPlot, &QCPItemSpan::rangeChanged, range);
}

void QCPItemSpan::draw(QCPDrawContext& context) const
{
    const QCP::Position lowStart = pinnedToRatio(lowerEdge, mOrientation, 0.0);
    const QCP::Position lowEnd = pinnedToRatio(lowerEdge, mOrientation, 1.0);
    const QCP::Position highStart = pinnedToRatio(upperEdge, mOrientation, 0.0);
    const QCP::Position highEnd = pinnedToRatio(upperEdge, mOrientation, 1.0);
    // An edge in pixels or on an anchor is placed against the axis rect, which moves: the recording
    // cannot outlive the frame.
    if (!pinnedToTheRatio(lowStart) || !pinnedToTheRatio(highStart))
        context.recordEveryFrame();

    context.rect(lowStart, highEnd, QPen(Qt::NoPen), mainBrush());

    // The band is filled and its edges are the border lines, as on screen (the GPU path): an
    // outline in the item pen framed every exported span along the axis rect edges.
    const QPen border = mainBorderPen();
    context.polyline(std::array<QCP::Position, 2> { lowStart, lowEnd }, border);
    context.polyline(std::array<QCP::Position, 2> { highStart, highEnd }, border);
}

std::optional<double> QCPItemSpan::hitTest(QCP::Pixels at, QVariant* details) const
{
    // Resolve through the positions so their type (plot coords, absolute pixels,
    // axis-rect pixels, ...) is honoured; coordToPixel() would reinterpret
    // already-pixel coords as data values.
    const bool vertical = mOrientation == Qt::Vertical;
    const double along = vertical ? at.x : at.y;
    const double across = vertical ? at.y : at.x;
    const QRectF axisRect = clipRect();
    const double acrossStart = vertical ? axisRect.top() : axisRect.left();
    const double acrossEnd = vertical ? axisRect.bottom() : axisRect.right();
    const bool insideTheBand = acrossStart <= across && across <= acrossEnd;

    const double lowerPx = rangePixel(lowerEdge);
    const double upperPx = rangePixel(upperEdge);
    const double tolerance = mParentPlot->selectionTolerance();

    const double distLower = qAbs(along - lowerPx);
    if (distLower <= tolerance && insideTheBand)
    {
        if (details)
            details->setValue(static_cast<int>(hpLowerEdge));
        return distLower;
    }
    const double distUpper = qAbs(along - upperPx);
    if (distUpper <= tolerance && insideTheBand)
    {
        if (details)
            details->setValue(static_cast<int>(hpUpperEdge));
        return distUpper;
    }

    const bool filledRect = mBrush.style() != Qt::NoBrush && mBrush.color().alpha() != 0;
    const double low = qMin(lowerPx, upperPx);
    const double high = qMax(lowerPx, upperPx);
    if (filledRect && low <= along && along <= high && insideTheBand)
    {
        if (details)
            details->setValue(static_cast<int>(hpFill));
        return 0.0;
    }

    return std::nullopt;
}

QPointF QCPItemSpan::anchorPixelPosition(int anchorId) const
{
    if (anchorId == aiCenter)
    {
        const double mid = (rangePixel(lowerEdge) + rangePixel(upperEdge)) * 0.5;
        const QRectF axRect = clipRect();
        return mOrientation == Qt::Vertical ? QPointF(mid, (axRect.top() + axRect.bottom()) * 0.5)
                                            : QPointF((axRect.left() + axRect.right()) * 0.5, mid);
    }
    qDebug() << Q_FUNC_INFO << "invalid anchorId" << anchorId;
    return { };
}

void QCPItemSpan::mousePressEvent(QMouseEvent* event, const QVariant& details)
{
    if (!mMovable)
    {
        event->ignore();
        return;
    }
    mDragPart = static_cast<HitPart>(details.toInt());
    mDragStartLower = rangePixel(lowerEdge);
    mDragStartUpper = rangePixel(upperEdge);
    event->accept();
}

void QCPItemSpan::mouseMoveEvent(QMouseEvent* event, const QPointF& startPos)
{
    if (mDragPart == hpNone)
        return;

    const bool vertical = mOrientation == Qt::Vertical;
    const Qt::Orientation edgeAxis = vertical ? Qt::Horizontal : Qt::Vertical;
    const double delta
        = (vertical ? event->pos().x() - startPos.x() : event->pos().y() - startPos.y());

    switch (mDragPart)
    {
        case hpLowerEdge:
            setEdgePixel(lowerEdge, edgeAxis, mDragStartLower + delta);
            break;
        case hpUpperEdge:
            setEdgePixel(upperEdge, edgeAxis, mDragStartUpper + delta);
            break;
        case hpFill:
            setEdgePixel(lowerEdge, edgeAxis, mDragStartLower + delta);
            setEdgePixel(upperEdge, edgeAxis, mDragStartUpper + delta);
            break;
        default:
            break;
    }
    // Emit last: a slot may delete this span.
    if (mParentPlot)
        mParentPlot->replot(QCustomPlot::rpQueuedReplot);
    qcpEmit(this, mParentPlot, &QCPItemSpan::rangeChanged, range());
}

void QCPItemSpan::mouseReleaseEvent([[maybe_unused]] QMouseEvent* event,
                                    [[maybe_unused]] const QPointF& startPos)
{
    mDragPart = hpNone;
}
