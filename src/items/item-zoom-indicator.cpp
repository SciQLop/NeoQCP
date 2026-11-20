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

namespace
{
//! The box the two ranges cover, in data.
QRectF boxOf(const QCPRange& keys, const QCPRange& values)
{
    const double keyLow = std::min(keys.lower, keys.upper);
    const double keyHigh = std::max(keys.lower, keys.upper);
    const double valueLow = std::min(values.lower, values.upper);
    const double valueHigh = std::max(values.lower, values.upper);
    return QRectF(keyLow, valueLow, keyHigh - keyLow, valueHigh - valueLow);
}

//! \a box in this rect's pixels, its corners in reading order whichever way the axes run.
QRectF pixelsOf(const QCPTransform& transform, const QRectF& box)
{
    const auto at = [&transform](double key, double value)
    {
        const QCP::Pixels pixels = transform.map(QCP::Data { key, value });
        return QPointF(pixels.x, pixels.y);
    };
    return QRectF(at(box.left(), box.bottom()), at(box.right(), box.top())).normalized();
}
} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPItemZoomIndicator
////////////////////////////////////////////////////////////////////////////////////////////////////

/*! \class QCPItemZoomIndicator
  \brief The box an inset's ranges cover in its parent, and the connectors that join it to the inset.

  A line, an inset on a sub-range, and this call: the "An inset" section of the layout guide has the
  example. A layout that moves the inset, or a parent axis that pans, remakes the recording; a move
  costs one queued replot on screen (the geometry signal a rect emits is delivered after the paint),
  and an export is right at once.

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
  Sets the inset whose ranges the box covers, to null for none.

  A rect that is not an inset of this item's own rect, in the same plot, is misuse: one report, and
  the inset stays as it was. The item's own rect is the one it was added to, the area its positions
  and its clip area are measured in.
*/
void QCPItemZoomIndicator::setInset(QCPAxisRect* inset)
{
    const QCPAxisRect* frame = clipAxisRect().get();
    QCP_CHECK_OR_RETURN(!inset
                            || (frame && inset->parentPlot() == mParentPlot && inset->isInset()
                                && inset->layout().get() == frame->insetLayout().get()),
                        "the rect a zoom indicator points at must be an inset of this item's rect");
    if (inset == mInset.data())
        return;
    // First, and alone on its own this: a slot a connection below runs may delete this item, and
    // nothing may touch it afterwards.
    const QPointer<QCPItemZoomIndicator> alive(this);
    update();
    if (!alive)
        return;
    for (const QMetaObject::Connection& link : std::exchange(mInsetLinks, { }))
        disconnect(link);
    mInset = inset;
    if (inset)
    {
        for (QCPAxis* axis : { inset->xAxis().get(), inset->yAxis().get() })
            if (axis)
                mInsetLinks << connect(axis, &QCPAxis::rangeChanged, this, [this] { update(); });
        // The connectors end on the inset's rect: a layout that moves it must remake them.
        mInsetLinks << connect(inset, &QCPLayoutElement::outerRectChanged, this,
                               [this] { update(); });
        mInsetLinks << connect(inset, &QCPLayoutElement::marginsChanged, this, [this] { update(); });
        mInsetLinks << connect(inset, &QCPLayerable::visibleChanged, this, [this] { update(); });
        // A deleted inset leaves the box empty, and the replot that follows draws nothing.
        mInsetLinks << connect(inset, &QObject::destroyed, this, [this] { update(); });
    }
    qcpEmit(this, mParentPlot, &QCPItemZoomIndicator::insetChanged, inset);
}

/*!
  Records the box in this rect's data, then the connectors, from a box corner in pixels to the same
  corner of the inset's data rect.

  Both ends of a connector are pixels: the visibility rule picks a corner of the box as it is drawn,
  which is the box's pixel corner, and a reversed axis turns the data corners round. That is also why
  this reads the transform: the recording holds pixel positions, so a moved frame remakes it.
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
    const QRectF box = boxOf(keyAxis->range(), valueAxis->range());
    const QRectF boxPixels = pixelsOf(transform, box);
    const QRectF insetPixels(inset->rect());
    const std::array<bool, 4> shown = qcp::zoomConnectorVisibility(boxPixels, insetPixels);

    context.rect(box, mPen, mBrush);

    const std::array<QPointF, 4> boxCorners { boxPixels.bottomLeft(), boxPixels.topLeft(),
                                              boxPixels.bottomRight(), boxPixels.topRight() };
    const std::array<QPointF, 4> insetCorners { insetPixels.bottomLeft(), insetPixels.topLeft(),
                                                insetPixels.bottomRight(),
                                                insetPixels.topRight() };
    for (std::size_t i = 0; i < boxCorners.size(); ++i)
        if (shown[i])
            context.line(QCP::Pixels { boxCorners[i].x(), boxCorners[i].y() },
                         QCP::Pixels { insetCorners[i].x(), insetCorners[i].y() }, mPen);
}
