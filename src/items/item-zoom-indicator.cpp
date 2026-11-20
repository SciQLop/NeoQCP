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

#include "item-zoom-indicator.h"

#include "../core.h"
#include "../painting/draw-context.h"

#include <algorithm>
#include <utility>

////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPItemZoomIndicator
////////////////////////////////////////////////////////////////////////////////////////////////////

/*! \class QCPItemZoomIndicator
  \brief The box an inset's ranges cover in its parent, and the connectors that join it to the inset.

  \image html QCPItemZoomIndicator.png

  The box runs from the inset's bottom axis range to its left axis range, in this rect's bottom and
  left axes. Each of the four corners may carry a connector to the same corner of the inset's data
  rect, chosen by qcp::zoomConnectorVisibility().

  The box and the connectors are remade whenever the inset's ranges change, and whenever the frame
  moves under them, so panning the inset, panning this rect, or resizing the widget all keep the
  picture true. Nothing is drawn while the inset is hidden or after it has been deleted.
*/

/*! \fn void QCPItemZoomIndicator::penChanged(const QPen& pen)

  This signal is emitted when \ref pen changes, after the plot delivers its deferred signals.
*/

/*! \fn void QCPItemZoomIndicator::brushChanged(const QBrush& brush)

  This signal is emitted when \ref brush changes, after the plot delivers its deferred signals.
*/

/*! \fn void QCPItemZoomIndicator::insetChanged(QCPAxisRect* inset)

  This signal is emitted when \ref inset changes, after the plot delivers its deferred signals.
*/

/*!
  Creates a zoom indicator that points at no inset yet: it draws nothing until setInset() names one.
  The box is a plain black line, and the connectors use the same pen.
*/
QCPItemZoomIndicator::QCPItemZoomIndicator() : QCPItem()
{
    setPen(QPen(Qt::black));
    setBrush(Qt::NoBrush);
}

QCPItemZoomIndicator::~QCPItemZoomIndicator() { }

/*!
  Sets the pen of the box and of the connectors.
*/
void QCPItemZoomIndicator::setPen(const QPen& pen)
{
    if (pen == mPen)
        return;
    update();
    mPen = pen;
    qcpEmit(this, mParentPlot, &QCPItemZoomIndicator::penChanged, this->pen());
}

/*!
  Sets the brush that fills the box. The default is Qt::NoBrush, an outline.
*/
void QCPItemZoomIndicator::setBrush(const QBrush& brush)
{
    if (brush == mBrush)
        return;
    update();
    mBrush = brush;
    qcpEmit(this, mParentPlot, &QCPItemZoomIndicator::brushChanged, this->brush());
}

/*!
  Sets the inset whose ranges the box covers, to null for none. An inset of another rect draws no
  box here: the item is meant for one made by QCPAxisRect::addInset() on the rect it belongs to.
*/
void QCPItemZoomIndicator::setInset(QCPAxisRect* inset)
{
    if (inset == mInset.data())
        return;
    for (const QMetaObject::Connection& link : std::exchange(mInsetLinks, { }))
        disconnect(link);
    mInset = inset;
    if (inset)
    {
        for (QCPAxis* axis : { inset->xAxis().get(), inset->yAxis().get() })
            if (axis)
            {
                mInsetLinks << connect(axis, &QCPAxis::rangeChanged, this,
                                       [this] { update(); });
                mInsetLinks << connect(axis, &QCPAxis::scaleChanged, this,
                                       [this] { update(); });
            }
        mInsetLinks << connect(inset, &QCPLayerable::visibleChanged, this, [this] { update(); });
        // A deleted inset leaves the box empty, and the replot that follows draws nothing.
        mInsetLinks << connect(inset, &QObject::destroyed, this, [this] { update(); });
    }
    update();
    qcpEmit(this, mParentPlot, &QCPItemZoomIndicator::insetChanged, inset);
}

/*!
  Records the box in this rect's data, then the connectors, from a box corner in data to the same
  corner of the inset's data rect in pixels.

  The connectors end on the inset's pixel rect, so the transform and the inset's rect are part of
  what the recording holds: this reads the transform to say so, and dependencyRevision() watches the
  rect.
*/
void QCPItemZoomIndicator::draw(QCPDrawContext& context) const
{
    const QCPAxisRect* inset = mInset.data();
    if (!inset || !inset->visible())
        return;
    const QCPAxis* keyAxis = inset->xAxis().get();
    const QCPAxis* valueAxis = inset->yAxis().get();
    if (!keyAxis || !valueAxis)
        return;

    const QCPTransform& transform = context.transform();
    const QCPRange keys = keyAxis->range(), values = valueAxis->range();
    const double keyLow = std::min(keys.lower, keys.upper);
    const double keyHigh = std::max(keys.lower, keys.upper);
    const double valueLow = std::min(values.lower, values.upper);
    const double valueHigh = std::max(values.lower, values.upper);

    const auto pixels = [&transform](double key, double value)
    {
        const QCP::Pixels at = transform.map(QCP::Data { key, value });
        return QPointF(at.x, at.y);
    };
    const QRectF boxPixels = QRectF(pixels(keyLow, valueHigh), pixels(keyHigh, valueLow))
                                 .normalized();
    const QRectF insetPixels(inset->rect());
    const std::array<bool, 4> shown = qcp::zoomConnectorVisibility(boxPixels, insetPixels);

    context.rect(QRectF(keyLow, valueLow, keyHigh - keyLow, valueHigh - valueLow), mPen, mBrush);

    const std::array<QCP::Data, 4> boxCorners { QCP::Data { keyLow, valueLow },
                                                QCP::Data { keyLow, valueHigh },
                                                QCP::Data { keyHigh, valueLow },
                                                QCP::Data { keyHigh, valueHigh } };
    const std::array<QPointF, 4> insetCorners { insetPixels.bottomLeft(), insetPixels.topLeft(),
                                                insetPixels.bottomRight(),
                                                insetPixels.topRight() };
    for (std::size_t i = 0; i < insetCorners.size(); ++i)
        if (shown[i])
            context.line(boxCorners[i], QCP::Pixels { insetCorners[i].x(), insetCorners[i].y() },
                         mPen);
}

quint64 QCPItemZoomIndicator::dependencyRevision() const
{
    const QCPAxisRect* inset = mInset.data();
    if (!inset)
        return QCPItem::dependencyRevision();
    const QCPAxis* keyAxis = inset->xAxis().get();
    const QCPAxis* valueAxis = inset->yAxis().get();
    const QRect rect = inset->rect();
    return qHashMulti(QCPItem::dependencyRevision(), quint64(rect.x()), quint64(rect.y()),
                      quint64(rect.width()), quint64(rect.height()),
                      keyAxis ? qHashMulti(keyAxis->range().lower, keyAxis->range().upper) : 0,
                      valueAxis ? qHashMulti(valueAxis->range().lower, valueAxis->range().upper)
                                : 0);
}
