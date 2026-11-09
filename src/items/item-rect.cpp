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

#include "item-rect.h"

#include "../core.h"
#include "../painting/draw-context.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPItemRect
////////////////////////////////////////////////////////////////////////////////////////////////////

/*! \class QCPItemRect
  \brief A rectangle

  \image html QCPItemRect.png "Rectangle example. Blue dotted circles are anchors, solid blue discs
  are positions."

  It has two positions, \a topLeft and \a bottomRight, which define the rectangle.
*/

/*!
  Creates a rectangle item and sets default values.

  The created item is automatically registered with \a parentPlot. This QCustomPlot instance takes
  ownership of the item, so do not delete it manually but use QCustomPlot::removeItem() instead.
*/
QCPItemRect::QCPItemRect(QCustomPlot* parentPlot)
        : QCPAbstractItem(parentPlot)
        , topLeft(createPosition(QLatin1String("topLeft")))
        , bottomRight(createPosition(QLatin1String("bottomRight")))
        , top(createAnchor(QLatin1String("top"), aiTop))
        , topRight(createAnchor(QLatin1String("topRight"), aiTopRight))
        , right(createAnchor(QLatin1String("right"), aiRight))
        , bottom(createAnchor(QLatin1String("bottom"), aiBottom))
        , bottomLeft(createAnchor(QLatin1String("bottomLeft"), aiBottomLeft))
        , left(createAnchor(QLatin1String("left"), aiLeft))
{
    topLeft->setCoords(0, 1);
    bottomRight->setCoords(1, 0);

    setPen(QPen(Qt::black));
    setSelectedPen(QPen(Qt::blue, 2));
    setBrush(Qt::NoBrush);
    setSelectedBrush(Qt::NoBrush);
}

QCPItemRect::~QCPItemRect() { }

/*!
  Sets the pen that will be used to draw the line of the rectangle

  \see setSelectedPen, setBrush
*/
void QCPItemRect::setPen(const QPen& pen)
{
    if (pen == mPen)
        return;
    markDirty();
    mPen = pen;
}

/*!
  Sets the pen that will be used to draw the line of the rectangle when selected

  \see setPen, setSelected
*/
void QCPItemRect::setSelectedPen(const QPen& pen)
{
    if (pen == mSelectedPen)
        return;
    markDirty();
    mSelectedPen = pen;
}

/*!
  Sets the brush that will be used to fill the rectangle. To disable filling, set \a brush to
  Qt::NoBrush.

  \see setSelectedBrush, setPen
*/
void QCPItemRect::setBrush(const QBrush& brush)
{
    if (brush == mBrush)
        return;
    markDirty();
    mBrush = brush;
}

/*!
  Sets the brush that will be used to fill the rectangle when selected. To disable filling, set \a
  brush to Qt::NoBrush.

  \see setBrush
*/
void QCPItemRect::setSelectedBrush(const QBrush& brush)
{
    if (brush == mSelectedBrush)
        return;
    markDirty();
    mSelectedBrush = brush;
}


/* inherits documentation from base class */
void QCPItemRect::draw(QCPDrawContext& context) const
{
    bool snapshot = false;
    const QCP::Position from = topLeft->asPosition(&snapshot);
    const QCP::Position to = bottomRight->asPosition(&snapshot);
    if (snapshot)
        context.recordEveryFrame();
    // Both corners in data coordinates: the recording is data, so a pan only shifts it.
    if (std::holds_alternative<QCP::Data>(from) && std::holds_alternative<QCP::Data>(to))
    {
        context.rect(from, to, mainPen(), mainBrush());
        return;
    }
    // A corner is pixels or anchored: where the rectangle is depends on the view.
    const QCPTransform& transform = context.transform();
    const auto pixelOf = [&](const QCP::Position& at, QCPItemPosition* position)
    {
        if (const auto* data = std::get_if<QCP::Data>(&at))
        {
            const QCP::Pixels pixels = transform.map(*data);
            return QCP::Pixels { pixels.x, pixels.y };
        }
        const QPointF pixels = position->pixelPosition();
        return QCP::Pixels { pixels.x(), pixels.y() };
    };
    context.rect(pixelOf(from, topLeft), pixelOf(to, bottomRight), mainPen(), mainBrush());
}

/* inherits documentation from base class */
QPointF QCPItemRect::anchorPixelPosition(int anchorId) const
{
    QRectF rect = QRectF(topLeft->pixelPosition(), bottomRight->pixelPosition());
    switch (anchorId)
    {
        case aiTop:
            return (rect.topLeft() + rect.topRight()) * 0.5;
        case aiTopRight:
            return rect.topRight();
        case aiRight:
            return (rect.topRight() + rect.bottomRight()) * 0.5;
        case aiBottom:
            return (rect.bottomLeft() + rect.bottomRight()) * 0.5;
        case aiBottomLeft:
            return rect.bottomLeft();
        case aiLeft:
            return (rect.topLeft() + rect.bottomLeft()) * 0.5;
    }

    qDebug() << Q_FUNC_INFO << "invalid anchorId" << anchorId;
    return {};
}

/*! \internal

  Returns the pen that should be used for drawing lines. Returns mPen when the item is not selected
  and mSelectedPen when it is.
*/
QPen QCPItemRect::mainPen() const
{
    return mSelected ? mSelectedPen : mPen;
}

/*! \internal

  Returns the brush that should be used for drawing fills of the item. Returns mBrush when the item
  is not selected and mSelectedBrush when it is.
*/
QBrush QCPItemRect::mainBrush() const
{
    return mSelected ? mSelectedBrush : mBrush;
}

/*! \internal

  Returns the item's extent from its positions, grown by the widest pen.

  The recording cannot answer this one: a position may be data on one axis and pixels on the other,
  and a recorded position is either wholly data or wholly pixels, so the recording would lose the
  data side and the layer's index would never keep the item.
*/
std::optional<QCPItemExtent> QCPItemRect::dataExtent() const
{
    const double pad = qMax(mPen.widthF(), mSelectedPen.widthF()) + 2.0;
    return positionsExtent(pad, positions());
}
