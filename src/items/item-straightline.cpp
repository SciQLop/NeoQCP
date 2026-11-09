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

#include "item-straightline.h"

#include "../core.h"
#include "../painting/draw-context.h"

#include <array>

////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPItemStraightLine
////////////////////////////////////////////////////////////////////////////////////////////////////

/*! \class QCPItemStraightLine
  \brief A straight line that spans infinitely in both directions

  \image html QCPItemStraightLine.png "Straight line example. Blue dotted circles are anchors, solid
  blue discs are positions."

  It has two positions, \a point1 and \a point2, which define the straight line.
*/

/*!
  Creates a straight line item and sets default values.

  The created item is automatically registered with \a parentPlot. This QCustomPlot instance takes
  ownership of the item, so do not delete it manually but use QCustomPlot::removeItem() instead.
*/
QCPItemStraightLine::QCPItemStraightLine(QCustomPlot* parentPlot)
        : QCPAbstractItem(parentPlot)
        , point1(createPosition(QLatin1String("point1")))
        , point2(createPosition(QLatin1String("point2")))
{
    point1->setCoords(0, 0);
    point2->setCoords(1, 1);

    setPen(QPen(Qt::black));
    setSelectedPen(QPen(Qt::blue, 2));
}

QCPItemStraightLine::~QCPItemStraightLine() { }

/*!
  Sets the pen that will be used to draw the line

  \see setSelectedPen
*/
void QCPItemStraightLine::setPen(const QPen& pen)
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
void QCPItemStraightLine::setSelectedPen(const QPen& pen)
{
    if (pen == mSelectedPen)
        return;
    markDirty();
    mSelectedPen = pen;
}

/* inherits documentation from base class */
void QCPItemStraightLine::draw(QCPDrawContext& context) const
{
    // The line fills the view: it is cut to the clip rect, which moves with the view.
    context.recordEveryFrame();
    const QCPVector2D start(point1->pixelPosition());
    const QCPVector2D end(point2->pixelPosition());
    const int clipPad = qCeil(mainPen().widthF());
    const QLineF line = getRectClippedStraightLine(
        start, end - start, clipRect().adjusted(-clipPad, -clipPad, clipPad, clipPad));
    if (line.isNull())
        return;
    context.polyline(std::array<QCP::Position, 2> { QCP::Pixels { line.x1(), line.y1() },
                                                   QCP::Pixels { line.x2(), line.y2() } },
                     mainPen());
}

/*! \internal

  Returns the section of the straight line defined by \a base and direction vector \a
  vec, that is visible in the specified \a rect.

  This is a helper function for \ref draw.
*/
QLineF QCPItemStraightLine::getRectClippedStraightLine(const QCPVector2D& base,
                                                       const QCPVector2D& vec,
                                                       const QRect& rect) const
{
    double bx, by;
    double gamma;
    QLineF result;
    if (vec.x() == 0 && vec.y() == 0)
        return result;
    if (qFuzzyIsNull(vec.x())) // line is vertical
    {
        // check top of rect:
        bx = rect.left();
        by = rect.top();
        gamma = base.x() - bx + (by - base.y()) * vec.x() / vec.y();
        if (gamma >= 0 && gamma <= rect.width())
            result.setLine(
                bx + gamma, rect.top(), bx + gamma,
                rect.bottom()); // no need to check bottom because we know line is vertical
    }
    else if (qFuzzyIsNull(vec.y())) // line is horizontal
    {
        // check left of rect:
        bx = rect.left();
        by = rect.top();
        gamma = base.y() - by + (bx - base.x()) * vec.y() / vec.x();
        if (gamma >= 0 && gamma <= rect.height())
            result.setLine(rect.left(), by + gamma, rect.right(),
                           by + gamma); // no need to check right because we know line is horizontal
    }
    else // line is skewed
    {
        QList<QCPVector2D> pointVectors;
        // check top of rect:
        bx = rect.left();
        by = rect.top();
        gamma = base.x() - bx + (by - base.y()) * vec.x() / vec.y();
        if (gamma >= 0 && gamma <= rect.width())
            pointVectors.append(QCPVector2D(bx + gamma, by));
        // check bottom of rect:
        bx = rect.left();
        by = rect.bottom();
        gamma = base.x() - bx + (by - base.y()) * vec.x() / vec.y();
        if (gamma >= 0 && gamma <= rect.width())
            pointVectors.append(QCPVector2D(bx + gamma, by));
        // check left of rect:
        bx = rect.left();
        by = rect.top();
        gamma = base.y() - by + (bx - base.x()) * vec.y() / vec.x();
        if (gamma >= 0 && gamma <= rect.height())
            pointVectors.append(QCPVector2D(bx, by + gamma));
        // check right of rect:
        bx = rect.right();
        by = rect.top();
        gamma = base.y() - by + (bx - base.x()) * vec.y() / vec.x();
        if (gamma >= 0 && gamma <= rect.height())
            pointVectors.append(QCPVector2D(bx, by + gamma));

        // evaluate points:
        if (pointVectors.size() == 2)
        {
            result.setPoints(pointVectors.at(0).toPointF(), pointVectors.at(1).toPointF());
        }
        else if (pointVectors.size() > 2)
        {
            // line probably goes through corner of rect, and we got two points there. single out
            // the point pair with greatest distance:
            double distSqrMax = 0;
            QCPVector2D pv1, pv2;
            for (int i = 0; i < pointVectors.size() - 1; ++i)
            {
                for (int k = i + 1; k < pointVectors.size(); ++k)
                {
                    double distSqr = (pointVectors.at(i) - pointVectors.at(k)).lengthSquared();
                    if (distSqr > distSqrMax)
                    {
                        pv1 = pointVectors.at(i);
                        pv2 = pointVectors.at(k);
                        distSqrMax = distSqr;
                    }
                }
            }
            result.setPoints(pv1.toPointF(), pv2.toPointF());
        }
    }
    return result;
}

/*! \internal

  Returns the pen that should be used for drawing lines. Returns mPen when the
  item is not selected and mSelectedPen when it is.
*/
QPen QCPItemStraightLine::mainPen() const
{
    return mSelected ? mSelectedPen : mPen;
}
