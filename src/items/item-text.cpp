/***************************************************************************
**                                                                        **
**  QCustomPlot, an easy to use, modern plotting widget for Qt            **
**  Copyright (C) 2011-2022 Emanuel Eichhammer                            **
**                                                                        **
**  This program is free software: you can redistribute it and/or modify  **
**  it under the terms of the GNU General Public License as published by  **
**  the Free Software Foundation, either version 3 of the License, or     **
**  (at your option) any later version.                                   **
**                                                                        **
**  This program is distributed in the hope that it will be useful,       **
**  but WITHOUT ANY WARRANTY; without even the implied warranty of        **
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         **
**  GNU General Public License for more details.                          **
**                                                                        **
**  You should have received a copy of the GNU General Public License     **
**  along with this program.  If not, see http://www.gnu.org/licenses/.   **
**                                                                        **
****************************************************************************
**           Author: Emanuel Eichhammer                                   **
**  Website/Contact: https://www.qcustomplot.com/                         **
**             Date: 06.11.22                                             **
**          Version: 2.1.1                                                **
****************************************************************************/

#include "item-text.h"

#include "../core.h"
#include "../painting/draw-commands.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPItemText
////////////////////////////////////////////////////////////////////////////////////////////////////

/*! \class QCPItemText
  \brief A text label

  \image html QCPItemText.png "Text example. Blue dotted circles are anchors, solid blue discs are
  positions."

  Its position is defined by the member \a position and the setting of \ref setPositionAlignment.
  The latter controls which part of the text rect shall be aligned with \a position.

  The text alignment itself (i.e. left, center, right) can be controlled with \ref
  setTextAlignment.

  The text may be rotated around the \a position point with \ref setRotation.
*/

/*!
  Creates a text item and sets default values.

  The created item is automatically registered with \a parentPlot. This QCustomPlot instance takes
  ownership of the item, so do not delete it manually but use QCustomPlot::removeItem() instead.
*/
QCPItemText::QCPItemText(QCustomPlot* parentPlot)
        : QCPAbstractItem(parentPlot)
        , position(createPosition(QLatin1String("position")))
        , topLeft(createAnchor(QLatin1String("topLeft"), aiTopLeft))
        , top(createAnchor(QLatin1String("top"), aiTop))
        , topRight(createAnchor(QLatin1String("topRight"), aiTopRight))
        , right(createAnchor(QLatin1String("right"), aiRight))
        , bottomRight(createAnchor(QLatin1String("bottomRight"), aiBottomRight))
        , bottom(createAnchor(QLatin1String("bottom"), aiBottom))
        , bottomLeft(createAnchor(QLatin1String("bottomLeft"), aiBottomLeft))
        , left(createAnchor(QLatin1String("left"), aiLeft))
        , mText(QLatin1String("text"))
        , mPositionAlignment(Qt::AlignCenter)
        , mTextAlignment(Qt::AlignTop | Qt::AlignHCenter)
        , mRotation(0)
{
    position->setCoords(0, 0);

    setPen(Qt::NoPen);
    setSelectedPen(Qt::NoPen);
    setBrush(Qt::NoBrush);
    setSelectedBrush(Qt::NoBrush);
    setColor(Qt::black);
    setSelectedColor(Qt::blue);
}

QCPItemText::~QCPItemText() { }

/*!
  Sets the color of the text.
*/
void QCPItemText::setColor(const QColor& color)
{
    if (color == mColor)
        return;
    markDirty();
    mColor = color;
}

/*!
  Sets the color of the text that will be used when the item is selected.
*/
void QCPItemText::setSelectedColor(const QColor& color)
{
    if (color == mSelectedColor)
        return;
    markDirty();
    mSelectedColor = color;
}

/*!
  Sets the pen that will be used do draw a rectangular border around the text. To disable the
  border, set \a pen to Qt::NoPen.

  \see setSelectedPen, setBrush, setPadding
*/
void QCPItemText::setPen(const QPen& pen)
{
    if (pen == mPen)
        return;
    markDirty();
    mPen = pen;
}

/*!
  Sets the pen that will be used do draw a rectangular border around the text, when the item is
  selected. To disable the border, set \a pen to Qt::NoPen.

  \see setPen
*/
void QCPItemText::setSelectedPen(const QPen& pen)
{
    if (pen == mSelectedPen)
        return;
    markDirty();
    mSelectedPen = pen;
}

/*!
  Sets the brush that will be used do fill the background of the text. To disable the
  background, set \a brush to Qt::NoBrush.

  \see setSelectedBrush, setPen, setPadding
*/
void QCPItemText::setBrush(const QBrush& brush)
{
    if (brush == mBrush)
        return;
    markDirty();
    mBrush = brush;
}

/*!
  Sets the brush that will be used do fill the background of the text, when the item is selected. To
  disable the background, set \a brush to Qt::NoBrush.

  \see setBrush
*/
void QCPItemText::setSelectedBrush(const QBrush& brush)
{
    if (brush == mSelectedBrush)
        return;
    markDirty();
    mSelectedBrush = brush;
}

/*!
  Sets the font of the text.

  \see setSelectedFont, setColor
*/
void QCPItemText::setFont(const QFont& font)
{
    if (font == mFont)
        return;
    markDirty();
    mFont = font;
}

/*!
  Sets the font of the text that will be used when the item is selected.

  \see setFont
*/
void QCPItemText::setSelectedFont(const QFont& font)
{
    if (font == mSelectedFont)
        return;
    markDirty();
    mSelectedFont = font;
}

/*!
  Sets the text that will be displayed. Multi-line texts are supported by inserting a line break
  character, e.g. '\n'.

  \see setFont, setColor, setTextAlignment
*/
void QCPItemText::setText(const QString& text)
{
    if (text == mText)
        return;
    markDirty();
    mText = text;
}

/*!
  Sets which point of the text rect shall be aligned with \a position.

  Examples:
  \li If \a alignment is <tt>Qt::AlignHCenter | Qt::AlignTop</tt>, the text will be positioned such
  that the top of the text rect will be horizontally centered on \a position.
  \li If \a alignment is <tt>Qt::AlignLeft | Qt::AlignBottom</tt>, \a position will indicate the
  bottom left corner of the text rect.

  If you want to control the alignment of (multi-lined) text within the text rect, use \ref
  setTextAlignment.
*/
void QCPItemText::setPositionAlignment(Qt::Alignment alignment)
{
    if (alignment == mPositionAlignment)
        return;
    markDirty();
    mPositionAlignment = alignment;
}

/*!
  Controls how (multi-lined) text is aligned inside the text rect (typically Qt::AlignLeft,
  Qt::AlignCenter or Qt::AlignRight).
*/
void QCPItemText::setTextAlignment(Qt::Alignment alignment)
{
    if (alignment == mTextAlignment)
        return;
    markDirty();
    mTextAlignment = alignment;
}

/*!
  Sets the angle in degrees by which the text (and the text rectangle, if visible) will be rotated
  around \a position.
*/
void QCPItemText::setRotation(double degrees)
{
    if (degrees == mRotation)
        return;
    markDirty();
    mRotation = degrees;
}

/*!
  Sets the distance between the border of the text rectangle and the text. The appearance (and
  visibility) of the text rectangle can be controlled with \ref setPen and \ref setBrush.
*/
void QCPItemText::setPadding(const QMargins& padding)
{
    if (padding == mPadding)
        return;
    markDirty();
    mPadding = padding;
}

/* inherits documentation from base class */
/* inherits documentation from base class */
void QCPItemText::setPosition(const QCP::Position& position)
{
    this->position->set(position);
}

/*! \internal

  What the item draws, what its hit test measures and where its anchors sit: one style, so the
  three cannot disagree. The selected variants win while the item is selected.
*/
QCPTextStyle QCPItemText::textStyle() const
{
    QCPTextStyle style;
    style.font = mSelected ? mSelectedFont : mFont;
    style.color = mSelected ? mSelectedColor : mColor;
    style.pen = mSelected ? mSelectedPen : mPen;
    style.brush = mSelected ? mSelectedBrush : mBrush;
    style.alignment = mPositionAlignment;
    style.textFlags = int(Qt::TextDontClip | mTextAlignment);
    style.rotationDegrees = mRotation;
    style.padding = QMarginsF(mPadding);
    style.mathText = true; // measured and drawn through the label renderer, as it always was
    return style;
}

/*! \internal

  The text box before rotation, placed as if the position were the origin: the layout puts it around
  that point according to the position alignment, and the caller translates and rotates it.
*/
QSize QCPItemText::glyphSize(const QCPTextStyle& style) const
{
    if (!mGlyphValid || mGlyphText != mText || mGlyphFont != style.font
        || mGlyphMath != style.mathText)
    {
        mGlyphText = mText;
        mGlyphFont = style.font;
        mGlyphMath = style.mathText;
        mGlyphSize = qcpMeasureText(style, mText);
        mGlyphValid = true;
    }
    return mGlyphSize;
}

QRectF QCPItemText::textBoxAtOrigin() const
{
    const QCPTextStyle style = textStyle();
    return QRectF(qcpTextLayout(style, glyphSize(style)).box);
}

/* inherits documentation from base class */
void QCPItemText::draw(QCPDrawContext& context) const
{
    bool snapshot = false;
    const QCP::Position at = position->asPosition(&snapshot);
    if (snapshot) // the position's space had to become pixels: they go stale without a new read
        context.recordEveryFrame();
    const QCPTextStyle style = textStyle();
    context.text(at, mText, style, glyphSize(style));
}

/* inherits documentation from base class */
std::optional<double> QCPItemText::hitTest(QCP::Pixels at, [[maybe_unused]] QVariant* details) const
{
    // The rect may be rotated, so the query point goes into the unrotated frame, where the plain
    // rectDistance applies. An item reports its distance past the selection tolerance too: the
    // plot decides whether it is a hit.
    const QPointF pixels(position->pixelPosition());
    QPointF local(at.x, at.y);
    if (!qFuzzyIsNull(mRotation))
    {
        QTransform inputTransform;
        inputTransform.translate(pixels.x(), pixels.y());
        inputTransform.rotate(-mRotation);
        inputTransform.translate(-pixels.x(), -pixels.y());
        local = inputTransform.map(local);
    }
    QRectF box = textBoxAtOrigin();
    box.moveTopLeft(box.topLeft() + pixels);
    return rectDistance(box, local, true);
}


/* inherits documentation from base class */
QPointF QCPItemText::anchorPixelPosition(int anchorId) const
{
    // The same box the recording carries, turned around the position the same way:
    const QPointF pos(position->pixelPosition());
    QTransform transform;
    transform.translate(pos.x(), pos.y());
    if (!qFuzzyIsNull(mRotation))
        transform.rotate(mRotation);
    const QPolygonF rectPoly = transform.map(QPolygonF(textBoxAtOrigin()));

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

/*! \internal

  Returns the point that must be given to the QPainter::drawText function (which expects the top
  left point of the text rect), according to the position \a pos, the text bounding box \a rect and
  the requested \a positionAlignment.


/*! \internal

  Returns the point the text is drawn at, given the position \a pos, the box \a rect and the
  requested \a positionAlignment: the box's top left corner lands there.
*/
QPointF QCPItemText::getTextDrawPoint(const QPointF& pos, const QRectF& rect,
                                      Qt::Alignment positionAlignment) const
{
    if (positionAlignment == 0 || positionAlignment == (Qt::AlignLeft | Qt::AlignTop))
        return pos;

    QPointF result = pos; // start at top left
    if (positionAlignment.testFlag(Qt::AlignHCenter))
        result.rx() -= rect.width() / 2.0;
    else if (positionAlignment.testFlag(Qt::AlignRight))
        result.rx() -= rect.width();
    if (positionAlignment.testFlag(Qt::AlignVCenter))
        result.ry() -= rect.height() / 2.0;
    else if (positionAlignment.testFlag(Qt::AlignBottom))
        result.ry() -= rect.height();
    return result;
}
