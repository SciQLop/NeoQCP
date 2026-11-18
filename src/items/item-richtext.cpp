#include "item-richtext.h"

#include "../core.h"
#include "../painting/painter.h"

/*! \fn void QCPItemRichText::cornerRadiusChanged(double cornerRadius)

  This signal is emitted when \ref cornerRadius changes, after the plot delivers its deferred
  signals.
*/

QCPItemRichText::QCPItemRichText()
        : QCPItemText()
{
    mDoc.setDocumentMargin(0);
    mDoc.setUseDesignMetrics(true);
}

QCPItemRichText::~QCPItemRichText() { }

/*! \fn void QCPItemRichText::htmlChanged(const QString& html)

  This signal is emitted when \ref html changes, after the plot delivers its deferred signals.
*/

/*!
  Sets the radius of the rounded corners of the box.
*/
void QCPItemRichText::setCornerRadius(double radius)
{
    if (radius == mCornerRadius)
        return;
    update();
    mCornerRadius = radius;
    qcpEmit(this, mParentPlot, &QCPItemRichText::cornerRadiusChanged, mCornerRadius);
}

void QCPItemRichText::setHtml(const QString& html)
{
    if (html == mHtml)
        return;
    update();
    mHtml = html;
    mDoc.setDefaultFont(textStyle().font);
    mDoc.setHtml(html);
    mBoundingRect = QRectF(QPointF(0, 0), mDoc.size());
    mUseHtml = true;
    qcpEmit(this, mParentPlot, &QCPItemRichText::htmlChanged, this->html());
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

/*! \internal

  The box around the drawn text, in pixels: the html box, placed as the recording places it and
  turned by the rotation. Without html the plain text item answers.
*/
QPolygonF QCPItemRichText::anchorPolygon() const
{
    if (!mUseHtml)
        return QCPItemText::anchorPolygon();

    const QPointF pos(position->pixelPosition());
    QTransform transform;
    transform.translate(pos.x(), pos.y());
    if (!qFuzzyIsNull(mRotation))
        transform.rotate(mRotation);

    QRectF textBoxRect = mBoundingRect.adjusted(-mPadding.left(), -mPadding.top(),
                                                mPadding.right(), mPadding.bottom());
    const QPointF drawPos = getTextDrawPoint(QPointF(0, 0), textBoxRect, mPositionAlignment);
    textBoxRect.moveTopLeft(drawPos);
    return transform.map(QPolygonF(textBoxRect));
}

QRectF QCPItemRichText::computeDrawRect(const QPointF& pos) const
{
    QRectF textBoxRect = mBoundingRect.adjusted(-mPadding.left(), -mPadding.top(),
                                                 mPadding.right(), mPadding.bottom());
    QPointF drawPos = getTextDrawPoint(pos, textBoxRect, mPositionAlignment);
    textBoxRect.moveTopLeft(drawPos);
    return textBoxRect;
}
