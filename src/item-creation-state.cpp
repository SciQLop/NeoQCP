#include "item-creation-state.h"
#include "core.h"
#include "painting/painter.h"
#include "layoutelements/layoutelement-axisrect.h"
#include "axis/axis.h"
#include "items/item.h"

QCPItemCreationState::QCPItemCreationState(QCustomPlot* plot)
    : QCPLayerable(plot, QLatin1String("overlay"))
    , mPlot(plot)
{
}

void QCPItemCreationState::applyDefaultAntialiasingHint(QCPPainter* painter) const
{
    painter->setAntialiasing(false);
}

void QCPItemCreationState::draw(QCPPainter* painter)
{
    if (!mPlot->creationModeEnabled())
        return;
    for (auto* axisRect : mPlot->axisRects())
        drawBadge(painter, axisRect->rect());
}

bool QCPItemCreationState::handleMousePress(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return false;

    if (mState == Idle)
    {
        const auto& creator = mPlot->itemCreator();
        if (!creator)
            return false;

        auto* axisRect = axisRectAt(event->pos());
        // axisRectAt matches the outer rect (including axis-label margins);
        // creation must only trigger inside the inner data area.
        if (!axisRect || !axisRect->rect().contains(event->pos()))
            return false;

        mKeyAxis = axisRect->axis(QCPAxis::atBottom);
        mValueAxis = axisRect->axis(QCPAxis::atLeft);
        if (!mKeyAxis) mKeyAxis = axisRect->axis(QCPAxis::atTop);
        if (!mValueAxis) mValueAxis = axisRect->axis(QCPAxis::atRight);
        if (!mKeyAxis || !mValueAxis)
            return false;

        mCurrentItem = creator(mPlot, mKeyAxis, mValueAxis);
        if (!mCurrentItem)
            return false;

        rebindPositions(mCurrentItem, mKeyAxis, mValueAxis, axisRect);

        double key = mKeyAxis->pixelToCoord(event->pos().x());
        double value = mValueAxis->pixelToCoord(event->pos().y());
        mAnchorKey = key;
        mAnchorValue = value;
        applyPositioner(key, value);

        mState = Drawing;
        mPlot->setCursor(Qt::CrossCursor);
        mPlot->replot(QCustomPlot::rpQueuedReplot);
        return true;
    }
    else if (mState == Drawing)
    {
        updateItemPosition(event->pos());
        commitItem();
        return true;
    }

    return false;
}

bool QCPItemCreationState::handleMouseMove(QMouseEvent* event)
{
    if (mState != Drawing || !mCurrentItem)
        return false;

    updateItemPosition(event->pos());
    mPlot->replot(QCustomPlot::rpQueuedReplot);
    return true;
}

bool QCPItemCreationState::handleKeyPress(QKeyEvent* event)
{
    if (mState != Drawing)
        return false;

    if (event->key() == Qt::Key_Escape) {
        cancelItem();
        return true;
    }
    return false;
}

void QCPItemCreationState::cancel()
{
    if (mState == Drawing)
        cancelItem();
}

void QCPItemCreationState::applyPositioner(double key, double value)
{
    if (!mCurrentItem) return;

    const auto& positioner = mPlot->itemPositioner();
    if (positioner) {
        positioner(mCurrentItem, mAnchorKey, mAnchorValue, key, value);
        return;
    }
    // Default fallback: 2-position items (e.g. QCPItemLine)
    auto positions = mCurrentItem->positions();
    if (positions.size() == 2) {
        positions[0]->setType(QCPItemPosition::ptPlotCoords);
        positions[0]->setCoords(mAnchorKey, mAnchorValue);
        positions[1]->setType(QCPItemPosition::ptPlotCoords);
        positions[1]->setCoords(key, value);
    }
}

void QCPItemCreationState::commitItem()
{
    auto* item = mCurrentItem.data();
    mCurrentItem = nullptr;
    mKeyAxis = nullptr;
    mValueAxis = nullptr;
    mState = Idle;
    if (!mPlot->creationModeEnabled())
        mPlot->setCursor(Qt::ArrowCursor);
    mPlot->replot(QCustomPlot::rpQueuedReplot);
    if (item)
        emit itemCreated(item);
}

void QCPItemCreationState::cancelItem()
{
    if (mCurrentItem) {
        mPlot->removeItem(mCurrentItem);
        mCurrentItem = nullptr;
    }
    mKeyAxis = nullptr;
    mValueAxis = nullptr;
    mState = Idle;
    if (!mPlot->creationModeEnabled())
        mPlot->setCursor(Qt::ArrowCursor);
    mPlot->replot(QCustomPlot::rpQueuedReplot);
    emit itemCanceled();
}

void QCPItemCreationState::updateItemPosition(const QPointF& pixelPos)
{
    if (!mCurrentItem || !mKeyAxis || !mValueAxis) return;

    double key = mKeyAxis->pixelToCoord(pixelPos.x());
    double value = mValueAxis->pixelToCoord(pixelPos.y());
    applyPositioner(key, value);
}

void QCPItemCreationState::rebindPositions(QCPAbstractItem* item, QCPAxis* keyAxis,
                                            QCPAxis* valueAxis, QCPAxisRect* axisRect)
{
    const auto positions = item->positions();
    for (QCPItemPosition* pos : positions)
    {
        if (pos)
        {
            pos->setAxes(keyAxis, valueAxis);
            pos->setAxisRect(axisRect);
        }
    }
}

QCPAxisRect* QCPItemCreationState::axisRectAt(const QPointF& pos) const
{
    return mPlot->axisRectAt(pos);
}

void QCPItemCreationState::drawBadge(QCPPainter* painter, const QRect& axisRectArea)
{
    painter->save();
    QFont badgeFont = painter->font();
    badgeFont.setPointSize(8);
    painter->setFont(badgeFont);
    QString label = QStringLiteral("Create");
    QFontMetrics fm(badgeFont);
    QRect textRect = fm.boundingRect(label);
    int padding = 4;
    int margin = 6;
    QRect badge(axisRectArea.right() - textRect.width() - 2 * padding - margin,
                axisRectArea.bottom() - textRect.height() - 2 * padding - margin,
                textRect.width() + 2 * padding,
                textRect.height() + 2 * padding);
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(255, 159, 67, 40));
    painter->drawRoundedRect(badge, 4, 4);
    painter->setPen(QColor(255, 159, 67));
    painter->drawText(badge, Qt::AlignCenter, label);
    painter->restore();
}
