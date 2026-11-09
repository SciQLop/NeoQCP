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

#include "item-line.h"

#include "../core.h"
#include "../painting/draw-context.h"

#include <array>

namespace
{
//! The pixel of a recorded position: read through the transform when it is data (so the recording
//! knows it follows the view), from the position itself otherwise.
QPointF pixelOf(const QCPDrawContext& context, const QCP::Position& at, QCPItemPosition* position)
{
    if (const auto* data = std::get_if<QCP::Data>(&at))
    {
        const QCP::Pixels pixels = context.transform().map(*data);
        return QPointF(pixels.x, pixels.y);
    }
    return position->pixelPosition();
}
} // namespace


////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPItemLine
////////////////////////////////////////////////////////////////////////////////////////////////////

/*! \class QCPItemLine
  \brief A line from one point to another

  \image html QCPItemLine.png "Line example. Blue dotted circles are anchors, solid blue discs are
  positions."

  It has two positions, \a start and \a end, which define the end points of the line.

  With \ref setHead and \ref setTail you may set different line ending styles, e.g. to create an
  arrow.
*/

/*!
  Creates a line item and sets default values.

  The created item is automatically registered with \a parentPlot. This QCustomPlot instance takes
  ownership of the item, so do not delete it manually but use QCustomPlot::removeItem() instead.
*/
QCPItemLine::QCPItemLine(QCustomPlot* parentPlot)
        : QCPAbstractItem(parentPlot)
        , start(createPosition(QLatin1String("start")))
        , end(createPosition(QLatin1String("end")))
{
    start->setCoords(0, 0);
    end->setCoords(1, 1);

    setPen(QPen(Qt::black));
    setSelectedPen(QPen(Qt::blue, 2));
}

QCPItemLine::~QCPItemLine() { }

/*!
  Sets the pen that will be used to draw the line

  \see setSelectedPen
*/
void QCPItemLine::setPen(const QPen& pen)
{
    if (pen == mPen)
        return;
    markDirty();
    mPen = pen;
}

/*!
  Sets the pen that will be used to draw the line when selected

  \see setPen, setSelected
*/
void QCPItemLine::setSelectedPen(const QPen& pen)
{
    if (pen == mSelectedPen)
        return;
    markDirty();
    mSelectedPen = pen;
}

/*!
  Sets the line ending style of the head. The head corresponds to the \a end position.

  Note that due to the overloaded QCPLineEnding constructor, you may directly specify
  a QCPLineEnding::EndingStyle here, e.g. \code setHead(QCPLineEnding::esSpikeArrow) \endcode

  \see setTail
*/
void QCPItemLine::setHead(const QCPLineEnding& head)
{
    if (mHead == head)
        return;
    markDirty();
    mHead = head;
}

/*!
  Sets the line ending style of the tail. The tail corresponds to the \a start position.

  Note that due to the overloaded QCPLineEnding constructor, you may directly specify
  a QCPLineEnding::EndingStyle here, e.g. \code setTail(QCPLineEnding::esSpikeArrow) \endcode

  \see setHead
*/
void QCPItemLine::setTail(const QCPLineEnding& tail)
{
    if (mTail == tail)
        return;
    markDirty();
    mTail = tail;
}

/* inherits documentation from base class */
void QCPItemLine::draw(QCPDrawContext& context) const
{
    bool snapshot = false;
    const QCP::Position from = start->asPosition(&snapshot);
    const QCP::Position to = end->asPosition(&snapshot);
    if (snapshot)
        context.recordEveryFrame();
    // A plain line is data on both ends: it is recorded as data and a pan only shifts it.
    if (mHead.style() == QCPLineEnding::esNone && mTail.style() == QCPLineEnding::esNone)
    {
        context.polyline(std::array<QCP::Position, 2> { from, to }, mainPen());
        return;
    }
    // The endings are pixels: their direction comes from the view, so this recording is remade
    // whenever the view changes (a pan included), as P-6 allows. Reading the transform says so even
    // when both ends are anchored or ratios: their pixel is the view's, not the position's.
    (void)context.transform();
    const QPointF startPixel = pixelOf(context, from, start);
    const QPointF endPixel = pixelOf(context, to, end);
    const QCPVector2D startVec(startPixel);
    const QCPVector2D endVec(endPixel);
    if (qFuzzyIsNull((startVec - endVec).lengthSquared()))
        return;
    context.polyline(std::array<QCP::Position, 2> { from, to }, mainPen());
    const QPen pen = mainPen();
    const double penWidth = pen.widthF();
    if (mTail.style() != QCPLineEnding::esNone)
        context.path(QCP::Pixels { startPixel.x(), startPixel.y() },
                     mTail.path(startVec - endVec, penWidth), mTail.penFor(pen),
                     mTail.brushFor(pen));
    if (mHead.style() != QCPLineEnding::esNone)
        context.path(QCP::Pixels { endPixel.x(), endPixel.y() },
                     mHead.path(endVec - startVec, penWidth), mHead.penFor(pen),
                     mHead.brushFor(pen));
}

/*! \internal

  Returns the pen that should be used for drawing lines. Returns mPen when the
  item is not selected and mSelectedPen when it is.
*/
QPen QCPItemLine::mainPen() const
{
    return mSelected ? mSelectedPen : mPen;
}

std::optional<QCPItemExtent> QCPItemLine::dataExtent() const
{
    const double pad =
        qMax(qMax(mHead.boundingDistance(), mTail.boundingDistance()),
             qMax(mPen.widthF(), mSelectedPen.widthF()))
        + 2.0;
    return positionsExtent(pad, positions());
}
