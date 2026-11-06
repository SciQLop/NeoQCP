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
    if (html == mHtml)
        return;
    markDirty();
    mHtml = html;
    mDoc.setDefaultFont(textStyle().font);
    mDoc.setHtml(html);
    mBoundingRect = QRectF(QPointF(0, 0), mDoc.size());
    mUseHtml = true;
}

void QCPItemRichText::clearHtml()
{
    mHtml.clear();
    mUseHtml = false;
}

std::optional<double> QCPItemRichText::hitTest(QCP::Pixels at, QVariant* details) const
{
    if (!mUseHtml)
        return QCPItemText::hitTest(at, details);

    const QPointF positionPixels(position->pixelPosition());
    QTransform inputTransform;
    inputTransform.translate(positionPixels.x(), positionPixels.y());
    inputTransform.rotate(-mRotation);
    inputTransform.translate(-positionPixels.x(), -positionPixels.y());
    const QPointF rotatedPos = inputTransform.map(QPointF(at.x, at.y));
    return rectDistance(computeDrawRect(positionPixels), rotatedPos, true);
}

void QCPItemRichText::draw(QCPDrawContext& context) const
{
    if (!mUseHtml)
    {
        QCPItemText::draw(context);
        return;
    }

    const QFont currentFont = textStyle().font;
    if (mDoc.defaultFont() != currentFont)
    {
        mDoc.setDefaultFont(currentFont);
        mBoundingRect = QRectF(QPointF(0, 0), mDoc.size());
    }
    // A text document is not a draw command: it goes through the painter escape hatch, which reads
    // its position when it runs, so a pan needs no new recording.
    context.painter([this](QCPPainter& painter, const QCPTransform&) { drawHtml(painter); });
}

/*! \internal

  Draws the html document: its box, then the document's contents, turned around the position.
*/
void QCPItemRichText::drawHtml(QCPPainter& painter) const
{
    const QPointF pos(position->pixelPosition());
    QTransform transform = painter.transform();
    transform.translate(pos.x(), pos.y());
    if (!qFuzzyIsNull(mRotation))
        transform.rotate(mRotation);

    QRectF textBoxRect = mBoundingRect.adjusted(-mPadding.left(), -mPadding.top(),
                                               mPadding.right(), mPadding.bottom());
    const QPointF drawPos = getTextDrawPoint(QPointF(0, 0), textBoxRect, mPositionAlignment);
    textBoxRect.moveTopLeft(drawPos);

    const QCPTextStyle style = textStyle();
    const int clipPad = qCeil(style.pen.widthF());
    const QRectF boundingRect = textBoxRect.adjusted(-clipPad, -clipPad, clipPad, clipPad);
    if (!transform.mapRect(boundingRect).intersects(painter.transform().mapRect(clipRect())))
        return;

    painter.setTransform(transform);
    if ((style.brush.style() != Qt::NoBrush && style.brush.color().alpha() != 0)
        || (style.pen.style() != Qt::NoPen && style.pen.color().alpha() != 0))
    {
        painter.setPen(style.pen);
        painter.setBrush(style.brush);
        if (mCornerRadius > 0)
            painter.drawRoundedRect(textBoxRect, mCornerRadius, mCornerRadius);
        else
            painter.drawRect(textBoxRect);
    }

    painter.translate(drawPos + QPointF(mPadding.left(), mPadding.top()));
    mDoc.drawContents(&painter);
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
