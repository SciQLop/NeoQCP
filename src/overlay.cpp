#include "overlay.h"
#include "core.h"
#include "painting/painter.h"
#include <QApplication>

QCPOverlay::QCPOverlay(QCustomPlot* parentPlot)
    : QCPLayerable(parentPlot)
{
    mFont = QApplication::font();
    setVisible(false);
}

void QCPOverlay::showMessage(const QString& text, Level level,
                              SizeMode sizeMode, Position position)
{
    mText = text;
    mLevel = level;
    mSizeMode = sizeMode;
    mPosition = position;
    setVisible(!text.isEmpty());
    emit messageChanged(mText, mLevel);
    redraw();
}

void QCPOverlay::clearMessage()
{
    mText.clear();
    setVisible(false);
    emit messageChanged(mText, mLevel);
    redraw();
}

void QCPOverlay::setCollapsible(bool enabled)
{
    if (mCollapsible == enabled)
        return;
    mCollapsible = enabled;
    if (!mCollapsible && mCollapsed) {
        mCollapsed = false;
        emit collapsedChanged(false);
    }
    redraw();
}

void QCPOverlay::setCollapsed(bool collapsed)
{
    if (mCollapsed == collapsed)
        return;
    mCollapsed = collapsed;
    emit collapsedChanged(mCollapsed);
    redraw();
}

void QCPOverlay::setOpacity(qreal opacity)
{
    mOpacity = qBound(0.0, opacity, 1.0);
    if (visible())
        redraw();
}

void QCPOverlay::setFont(const QFont& font)
{
    mFont = font;
    if (visible())
        redraw();
}

// The notification layer is buffered: a replot alone repaints the old message.
void QCPOverlay::redraw()
{
    if (mLayer)
        mLayer->invalidatePaintBuffer();
    if (mParentPlot)
        mParentPlot->replot(QCustomPlot::rpQueuedReplot);
}

QColor QCPOverlay::levelColor() const
{
    switch (mLevel) {
        case Info:    return QColor(46, 139, 87);
        case Warning: return QColor(204, 153, 0);
        case Error:   return QColor(178, 34, 34);
        default:      return QColor(46, 139, 87);
    }
}

QRect QCPOverlay::overlayRect() const
{
    return computeRect();
}

QRect QCPOverlay::computeRect() const
{
    if (mText.isEmpty() || !mParentPlot)
        return {};

    const QRect viewport = mParentPlot->rect();
    const QFontMetrics fm(mFont);
    constexpr int pad = 4;
    const bool horizontal = (mPosition == Top || mPosition == Bottom);

    if (mSizeMode == FullWidget && !mCollapsed)
        return viewport;

    int contentSize = 0;
    if (mSizeMode == Compact || mCollapsed) {
        contentSize = fm.height() + 2 * pad;
    } else { // FitContent
        constexpr int handleSize = 20;
        if (horizontal) {
            int availWidth = viewport.width() - 2 * pad;
            if (mCollapsible)
                availWidth -= handleSize;
            QRect textBounds = fm.boundingRect(
                QRect(0, 0, availWidth, 0),
                Qt::AlignLeft | Qt::TextWordWrap, mText);
            contentSize = textBounds.height() + 2 * pad;
        } else {
            int availHeight = viewport.height() - 2 * pad;
            if (mCollapsible)
                availHeight -= handleSize;
            QRect textBounds = fm.boundingRect(
                QRect(0, 0, availHeight, 0),
                Qt::AlignLeft | Qt::TextWordWrap, mText);
            contentSize = qMin(textBounds.width() + 2 * pad, viewport.width());
        }
    }

    switch (mPosition) {
        case Top:
            return QRect(viewport.left(), viewport.top(),
                         viewport.width(), contentSize);
        case Bottom:
            return QRect(viewport.left(), viewport.bottom() - contentSize + 1,
                         viewport.width(), contentSize);
        case Left:
            return QRect(viewport.left(), viewport.top(),
                         contentSize, viewport.height());
        case Right:
            return QRect(viewport.right() - contentSize + 1, viewport.top(),
                         contentSize, viewport.height());
    }
    return {};
}

QRect QCPOverlay::collapseHandleRect() const
{
    if (!mCollapsible)
        return {};
    const QRect rect = computeRect();
    if (rect.isEmpty())
        return {};

    constexpr int handleSize = 20;
    switch (mPosition) {
        case Top:
            return QRect(rect.right() - handleSize + 1, rect.top(),
                         handleSize, handleSize);
        case Bottom:
            return QRect(rect.right() - handleSize + 1, rect.bottom() - handleSize + 1,
                         handleSize, handleSize);
        case Left:
            return QRect(rect.left(), rect.top(),
                         handleSize, handleSize);
        case Right:
            return QRect(rect.right() - handleSize + 1, rect.top(),
                         handleSize, handleSize);
    }
    return {};
}

void QCPOverlay::applyDefaultAntialiasingHint(QCPPainter* painter) const
{
    painter->setAntialiasing(true);
}

void QCPOverlay::draw(QCPPainter* painter)
{
    if (mText.isEmpty())
        return;
    // Skip during export
    if (painter->modes().testFlag(QCPPainter::pmNoCaching))
        return;

    const QRect rect = computeRect();
    if (rect.isEmpty())
        return;

    painter->save();

    // Background
    QColor bg = levelColor();
    bg.setAlphaF(mOpacity);
    painter->setPen(Qt::NoPen);
    painter->setBrush(bg);
    painter->drawRoundedRect(rect, 4, 4);

    // Text
    constexpr int pad = 4;
    painter->setPen(Qt::white);
    painter->setFont(mFont);

    const bool horizontal = (mPosition == Top || mPosition == Bottom);
    const bool rotateText = !horizontal && (mSizeMode == Compact || mCollapsed);
    const QString displayText = mCollapsed ? mText.section('\n', 0, 0) : mText;

    if (rotateText) {
        painter->save();
        painter->translate(rect.center());
        if (mPosition == Left)
            painter->rotate(-90); // bottom-to-top
        else
            painter->rotate(90);  // top-to-bottom
        QRect textRect(-rect.height() / 2, -rect.width() / 2,
                       rect.height(), rect.width());
        textRect.adjust(pad, pad, -pad, -pad);
        if (mCollapsible) {
            constexpr int handleSize = 20;
            // In rotated coords, textRect left = top of overlay (Right)
            // or bottom of overlay (Left). Shrink from the handle side.
            if (mPosition == Right)
                textRect.adjust(handleSize, 0, 0, 0);
            else
                textRect.adjust(0, 0, -handleSize, 0);
        }
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, displayText);
        painter->restore();
    } else {
        QRect textRect = rect.adjusted(pad, pad, -pad, -pad);
        if (mCollapsible) {
            constexpr int handleSize = 20;
            if (horizontal)
                textRect.adjust(0, 0, -handleSize, 0);
            else
                textRect.adjust(0, handleSize, 0, 0);
        }
        int flags = Qt::AlignLeft | Qt::AlignVCenter;
        if (mSizeMode != Compact && !mCollapsed)
            flags = Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap;
        painter->drawText(textRect, flags, displayText);
    }

    // Collapse handle
    if (mCollapsible) {
        QRect handleRect = collapseHandleRect();
        painter->setPen(QPen(Qt::white, 1.5));
        painter->setBrush(Qt::NoBrush);
        QPointF center = handleRect.center();
        qreal sz = 4.0;
        // flip: +1 keeps original direction (Top/Left), -1 mirrors (Bottom/Right)
        qreal flip = (mPosition == Bottom || mPosition == Right) ? -1.0 : 1.0;
        if (mPosition == Top || mPosition == Bottom) {
            // ▲ when expanded (collapse toward edge), ▼ when collapsed (expand inward)
            qreal d = mCollapsed ? -flip : flip;
            painter->drawLine(QPointF(center.x() - sz, center.y() + d * sz/2),
                              QPointF(center.x(), center.y() - d * sz/2));
            painter->drawLine(QPointF(center.x(), center.y() - d * sz/2),
                              QPointF(center.x() + sz, center.y() + d * sz/2));
        } else {
            // ◀ when expanded (collapse toward edge), ▶ when collapsed (expand inward)
            qreal d = mCollapsed ? flip : -flip;
            painter->drawLine(QPointF(center.x() - d * sz/2, center.y() - sz),
                              QPointF(center.x() + d * sz/2, center.y()));
            painter->drawLine(QPointF(center.x() + d * sz/2, center.y()),
                              QPointF(center.x() - d * sz/2, center.y() + sz));
        }
    }

    painter->restore();
}

double QCPOverlay::selectTest(const QPointF& pos, bool /*onlySelectable*/,
                               QVariant* /*details*/) const
{
    if (mText.isEmpty() || !visible())
        return -1;

    const QRect rect = computeRect();
    if (!rect.contains(pos.toPoint()))
        return -1;

    if (mSizeMode == FullWidget)
        return 0;

    if (mCollapsible && collapseHandleRect().contains(pos.toPoint()))
        return 0;

    return -1;
}

void QCPOverlay::mousePressEvent(QMouseEvent* event, const QVariant& /*details*/)
{
    if (mCollapsible && collapseHandleRect().contains(event->pos()))
        setCollapsed(!mCollapsed);
}
