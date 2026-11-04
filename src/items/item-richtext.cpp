#include "item-richtext.h"

#include "../core.h"
#include "../painting/painter.h"

QCPItemRichText::QCPItemRichText(QCustomPlot* parentPlot)
        : QCPItemText(parentPlot)
{
    mDoc.setDocumentMargin(0);
    mDoc.setUseDesignMetrics(true);
}

QCPItemRichText::~QCPItemRichText() { }

void QCPItemRichText::setHtml(const QString& html)
{
    markDirty();
    mHtml = html;
    mDoc.setDefaultFont(mainFont());
    mDoc.setHtml(html);
    mBoundingRect = QRectF(QPointF(0, 0), mDoc.size());
    mUseHtml = true;
}

void QCPItemRichText::clearHtml()
{
    mHtml.clear();
    mUseHtml = false;
}

double QCPItemRichText::selectTest(const QPointF& pos, bool onlySelectable,
                                    [[maybe_unused]] QVariant* details) const
{
    if (!mUseHtml)
        return QCPItemText::selectTest(pos, onlySelectable, details);

    if (onlySelectable && !mSelectable)
        return -1;

    QPointF positionPixels(position->pixelPosition());
    QTransform inputTransform;
    inputTransform.translate(positionPixels.x(), positionPixels.y());
    inputTransform.rotate(-mRotation);
    inputTransform.translate(-positionPixels.x(), -positionPixels.y());
    QPointF rotatedPos = inputTransform.map(pos);

    QRectF drawRect = computeDrawRect(positionPixels);
    return rectDistance(drawRect, rotatedPos, true);
}

void QCPItemRichText::draw(QCPPainter* painter)
{
    if (!mUseHtml)
    {
        QCPItemText::draw(painter);
        return;
    }

    QFont currentFont = mainFont();
    if (mDoc.defaultFont() != currentFont)
    {
        mDoc.setDefaultFont(currentFont);
        mBoundingRect = QRectF(QPointF(0, 0), mDoc.size());
    }

    QPointF pos(position->pixelPosition());
    QTransform transform = painter->transform();
    transform.translate(pos.x(), pos.y());
    if (!qFuzzyIsNull(mRotation))
        transform.rotate(mRotation);

    QRectF textBoxRect = mBoundingRect.adjusted(-mPadding.left(), -mPadding.top(),
                                                 mPadding.right(), mPadding.bottom());
    QPointF drawPos = getTextDrawPoint(QPointF(0, 0), textBoxRect, mPositionAlignment);
    textBoxRect.moveTopLeft(drawPos);

    int clipPad = qCeil(mainPen().widthF());
    QRectF boundingRect = textBoxRect.adjusted(-clipPad, -clipPad, clipPad, clipPad);
    if (transform.mapRect(boundingRect).intersects(painter->transform().mapRect(clipRect())))
    {
        painter->setTransform(transform);
        if ((mainBrush().style() != Qt::NoBrush && mainBrush().color().alpha() != 0)
            || (mainPen().style() != Qt::NoPen && mainPen().color().alpha() != 0))
        {
            painter->setPen(mainPen());
            painter->setBrush(mainBrush());
            if (mCornerRadius > 0)
                painter->drawRoundedRect(textBoxRect, mCornerRadius, mCornerRadius);
            else
                painter->drawRect(textBoxRect);
        }

        painter->translate(drawPos + QPointF(mPadding.left(), mPadding.top()));
        mDoc.drawContents(painter);
    }
}

QPointF QCPItemRichText::anchorPixelPosition(int anchorId) const
{
    if (!mUseHtml)
        return QCPItemText::anchorPixelPosition(anchorId);

    QPointF pos(position->pixelPosition());
    QTransform transform;
    transform.translate(pos.x(), pos.y());
    if (!qFuzzyIsNull(mRotation))
        transform.rotate(mRotation);

    QRectF textBoxRect = mBoundingRect.adjusted(-mPadding.left(), -mPadding.top(),
                                                 mPadding.right(), mPadding.bottom());
    QPointF drawPos = getTextDrawPoint(QPointF(0, 0), textBoxRect, mPositionAlignment);
    textBoxRect.moveTopLeft(drawPos);
    QPolygonF rectPoly = transform.map(QPolygonF(textBoxRect));

    switch (anchorId)
    {
        case aiTopLeft:
            return rectPoly.at(0);
        case aiTop:
            return (rectPoly.at(0) + rectPoly.at(1)) * 0.5;
        case aiTopRight:
            return rectPoly.at(1);
        case aiRight:
            return (rectPoly.at(1) + rectPoly.at(2)) * 0.5;
        case aiBottomRight:
            return rectPoly.at(2);
        case aiBottom:
            return (rectPoly.at(2) + rectPoly.at(3)) * 0.5;
        case aiBottomLeft:
            return rectPoly.at(3);
        case aiLeft:
            return (rectPoly.at(3) + rectPoly.at(0)) * 0.5;
    }

    qDebug() << Q_FUNC_INFO << "invalid anchorId" << anchorId;
    return {};
}

QRectF QCPItemRichText::computeDrawRect(const QPointF& pos) const
{
    QRectF textBoxRect = mBoundingRect.adjusted(-mPadding.left(), -mPadding.top(),
                                                 mPadding.right(), mPadding.bottom());
    QPointF drawPos = getTextDrawPoint(pos, textBoxRect, mPositionAlignment);
    textBoxRect.moveTopLeft(drawPos);
    return textBoxRect;
}
