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

#include "item-bracket.h"

#include "../core.h"
#include "../painting/draw-context.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPItemBracket
////////////////////////////////////////////////////////////////////////////////////////////////////

/*! \class QCPItemBracket
  \brief A bracket for referencing/highlighting certain parts in the plot.

  \image html QCPItemBracket.png "Bracket example. Blue dotted circles are anchors, solid blue discs
  are positions."

  It has two positions, \a left and \a right, which define the span of the bracket. If \a left is
  actually farther to the left than \a right, the bracket is opened to the bottom, as shown in the
  example image.

  The bracket supports multiple styles via \ref setStyle. The length, i.e. how far the bracket
  stretches away from the embraced span, can be controlled with \ref setLength.

  \image html QCPItemBracket-length.png
  <center>Demonstrating the effect of different values for \ref setLength, for styles \ref
  bsCalligraphic and \ref bsSquare. Anchors and positions are displayed for reference.</center>

  It provides an anchor \a center, to allow connection of other items, e.g. an arrow (QCPItemLine
  or QCPItemCurve) or a text label (QCPItemText), to the bracket.
*/

/*!
  Creates a bracket item and sets default values.

  The created item is automatically registered with \a parentPlot. This QCustomPlot instance takes
  ownership of the item, so do not delete it manually but use QCustomPlot::removeItem() instead.
*/
QCPItemBracket::QCPItemBracket(QCustomPlot* parentPlot)
        : QCPAbstractItem(parentPlot)
        , left(createPosition(QLatin1String("left")))
        , right(createPosition(QLatin1String("right")))
        , center(createAnchor(QLatin1String("center"), aiCenter))
        , mLength(8)
        , mStyle(bsCalligraphic)
{
    left->setCoords(0, 0);
    right->setCoords(1, 1);

    setPen(QPen(Qt::black));
    setSelectedPen(QPen(Qt::blue, 2));
}

QCPItemBracket::~QCPItemBracket() { }

/*!
  Sets the pen that will be used to draw the bracket.

  Note that when the style is \ref bsCalligraphic, only the color will be taken from the pen, the
  stroke and width are ignored. To change the apparent stroke width of a calligraphic bracket, use
  \ref setLength, which has a similar effect.

  \see setSelectedPen
*/
void QCPItemBracket::setPen(const QPen& pen)
{
    if (pen == mPen)
        return;
    markDirty();
    mPen = pen;
}

/*!
  Sets the pen that will be used to draw the bracket when selected

  \see setPen, setSelected
*/
void QCPItemBracket::setSelectedPen(const QPen& pen)
{
    if (pen == mSelectedPen)
        return;
    markDirty();
    mSelectedPen = pen;
}

/*!
  Sets the \a length in pixels how far the bracket extends in the direction towards the embraced
  span of the bracket (i.e. perpendicular to the <i>left</i>-<i>right</i>-direction)

  \image html QCPItemBracket-length.png
  <center>Demonstrating the effect of different values for \ref setLength, for styles \ref
  bsCalligraphic and \ref bsSquare. Anchors and positions are displayed for reference.</center>
*/
void QCPItemBracket::setLength(double length)
{
    if (length == mLength)
        return;
    markDirty();
    mLength = length;
}

/*!
  Sets the style of the bracket, i.e. the shape/visual appearance.

  \see setPen
*/
void QCPItemBracket::setStyle(QCPItemBracket::BracketStyle style)
{
    if (style == mStyle)
        return;
    markDirty();
    mStyle = style;
}

/* inherits documentation from base class */
void QCPItemBracket::draw(QCPDrawContext& context) const
{
    QCPVector2D leftVec(left->pixelPosition());
    QCPVector2D rightVec(right->pixelPosition());
    if (leftVec.toPoint() == rightVec.toPoint())
        return;

    QCPVector2D widthVec = (rightVec - leftVec) * 0.5;
    QCPVector2D lengthVec = widthVec.perpendicular().normalized() * mLength;
    QCPVector2D centerVec = (rightVec + leftVec) * 0.5 - lengthVec;

    // The bracket is recorded in pixels, so the view decides where it is: reading the transform
    // remakes the recording whenever the view changes (P-6).
    const QCPTransform& transform = context.transform();
    Q_UNUSED(transform)

    QPolygon boundingPoly;
    boundingPoly << leftVec.toPoint() << rightVec.toPoint() << (rightVec - lengthVec).toPoint()
                 << (leftVec - lengthVec).toPoint();
    const int clipEnlarge = qCeil(mainPen().widthF());
    const QRect clip = clipRect().adjusted(-clipEnlarge, -clipEnlarge, clipEnlarge, clipEnlarge);
    if (!clip.intersects(boundingPoly.boundingRect()))
        return;

    QPen pen = mainPen();
    QBrush brush = Qt::NoBrush;
    QPainterPath path;
    switch (mStyle)
    {
        case bsSquare:
        {
            // Three separate lines, as the painter drew them: their ends are butt caps, not joins.
            path.moveTo((centerVec + widthVec).toPointF());
            path.lineTo((centerVec - widthVec).toPointF());
            path.moveTo((centerVec + widthVec).toPointF());
            path.lineTo((centerVec + widthVec + lengthVec).toPointF());
            path.moveTo((centerVec - widthVec).toPointF());
            path.lineTo((centerVec - widthVec + lengthVec).toPointF());
            break;
        }
        case bsRound:
        {
            path.moveTo((centerVec + widthVec + lengthVec).toPointF());
            path.cubicTo((centerVec + widthVec).toPointF(), (centerVec + widthVec).toPointF(),
                         centerVec.toPointF());
            path.cubicTo((centerVec - widthVec).toPointF(), (centerVec - widthVec).toPointF(),
                         (centerVec - widthVec + lengthVec).toPointF());
            break;
        }
        case bsCurly:
        {
            path.moveTo((centerVec + widthVec + lengthVec).toPointF());
            path.cubicTo((centerVec + widthVec - lengthVec * 0.8).toPointF(),
                         (centerVec + 0.4 * widthVec + lengthVec).toPointF(), centerVec.toPointF());
            path.cubicTo((centerVec - 0.4 * widthVec + lengthVec).toPointF(),
                         (centerVec - widthVec - lengthVec * 0.8).toPointF(),
                         (centerVec - widthVec + lengthVec).toPointF());
            break;
        }
        case bsCalligraphic:
        {
            pen = Qt::NoPen;
            brush = QBrush(mainPen().color());
            path.moveTo((centerVec + widthVec + lengthVec).toPointF());

            path.cubicTo((centerVec + widthVec - lengthVec * 0.8).toPointF(),
                         (centerVec + 0.4 * widthVec + 0.8 * lengthVec).toPointF(),
                         centerVec.toPointF());
            path.cubicTo((centerVec - 0.4 * widthVec + 0.8 * lengthVec).toPointF(),
                         (centerVec - widthVec - lengthVec * 0.8).toPointF(),
                         (centerVec - widthVec + lengthVec).toPointF());

            path.cubicTo((centerVec - widthVec - lengthVec * 0.5).toPointF(),
                         (centerVec - 0.2 * widthVec + 1.2 * lengthVec).toPointF(),
                         (centerVec + lengthVec * 0.2).toPointF());
            path.cubicTo((centerVec + 0.2 * widthVec + 1.2 * lengthVec).toPointF(),
                         (centerVec + widthVec - lengthVec * 0.5).toPointF(),
                         (centerVec + widthVec + lengthVec).toPointF());
            break;
        }
    }
    // The path is recorded in pixels from the origin, so it is shifted there.
    context.path(QCP::Pixels { leftVec.x(), leftVec.y() },
                 path.translated(-leftVec.toPointF()), pen, brush);
}

/* inherits documentation from base class */
QPointF QCPItemBracket::anchorPixelPosition(int anchorId) const
{
    QCPVector2D leftVec(left->pixelPosition());
    QCPVector2D rightVec(right->pixelPosition());
    if (leftVec.toPoint() == rightVec.toPoint())
        return leftVec.toPointF();

    QCPVector2D widthVec = (rightVec - leftVec) * 0.5;
    QCPVector2D lengthVec = widthVec.perpendicular().normalized() * mLength;
    QCPVector2D centerVec = (rightVec + leftVec) * 0.5 - lengthVec;

    switch (anchorId)
    {
        case aiCenter:
            return centerVec.toPointF();
    }
    qDebug() << Q_FUNC_INFO << "invalid anchorId" << anchorId;
    return {};
}

/*! \internal

  Returns the pen that should be used for drawing lines. Returns mPen when the
  item is not selected and mSelectedPen when it is.
*/
QPen QCPItemBracket::mainPen() const
{
    return mSelected ? mSelectedPen : mPen;
}

std::optional<QCPItemExtent> QCPItemBracket::dataExtent() const
{
    // the curly and calligraphic paths bend away from the corner polygon
    const double pad = qMax(1.5 * mLength, qMax(mPen.widthF(), mSelectedPen.widthF())) + 2.0;
    return positionsExtent(pad, positions());
}
