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

#include "layoutelement-axisrect.h"
#include "layoutelement-colorscale.h"

#include "../Profiling.hpp"
#include "../axis/axis.h"
#include "../axis/axisticker.h"
#include "../coordinate-system.h"
#include "../core.h"
#include "../items/item.h"
#include "../misuse.h"
#include "../painting/painter.h"
#include "../plottables/plottable-graph.h"
#include "../plottables/plottable.h"
#include <QSet>
#include <algorithm>

////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPAxisRect
////////////////////////////////////////////////////////////////////////////////////////////////////

/*! \class QCPAxisRect
  \brief Holds multiple axes and arranges them in a rectangular shape.

  This class represents an axis rect, a rectangular area that is bounded on all sides with an
  arbitrary number of axes.

  Initially QCustomPlot has one axis rect, accessible via QCustomPlot::axisRect(). However, the
  layout system allows to have multiple axis rects, e.g. arranged in a grid layout
  (QCustomPlot::plotLayout).

  By default, QCPAxisRect comes with four axes, at bottom, top, left and right. They can be
  accessed via \ref axis by providing the respective axis type (\ref QCPAxis::AxisType) and index.
  If you need all axes in the axis rect, use \ref axes. The top and right axes are set to be
  invisible initially (QCPAxis::setVisible). To add more axes to a side, use \ref addAxis or \ref
  addAxes. To remove an axis, use \ref removeAxis.

  The axis rect layerable itself only draws a background pixmap or color, if specified (\ref
  setBackground). It is placed on the "background" layer initially (see \ref QCPLayer for an
  explanation of the QCustomPlot layer system). The axes that are held by the axis rect can be
  placed on other layers, independently of the axis rect.

  Every axis rect has a child layout of type \ref QCPLayoutInset. It is accessible via \ref
  insetLayout and can be used to have other layout elements (or even other layouts with multiple
  elements) hovering inside the axis rect.

  If an axis rect is clicked and dragged, it processes this by moving certain axis ranges. The
  behaviour can be controlled with \ref setRangeDrag and \ref setRangeDragAxes. If the mouse wheel
  is scrolled while the cursor is on the axis rect, certain axes are scaled. This is controllable
  via \ref setRangeZoom, \ref setRangeZoomAxes and \ref setRangeZoomFactor. These interactions are
  only enabled if \ref QCustomPlot::setInteractions contains \ref QCP::iRangeDrag and \ref
  QCP::iRangeZoom.

  \image html AxisRectSpacingOverview.png
  <center>Overview of the spacings and paddings that define the geometry of an axis. The dashed
  line on the far left indicates the viewport/widget border.</center>
*/

/* start documentation of inline functions */

/*! \fn QCPLayoutInset *QCPAxisRect::insetLayout() const

  Returns the inset layout of this axis rect. It can be used to place other layout elements (or
  even layouts with multiple other elements) inside/on top of an axis rect.

  \see QCPLayoutInset
*/

/*! \fn int QCPAxisRect::left() const

  Returns the pixel position of the left border of this axis rect. Margins are not taken into
  account here, so the returned value is with respect to the inner \ref rect.
*/

/*! \fn int QCPAxisRect::right() const

  Returns the pixel position of the right border of this axis rect. Margins are not taken into
  account here, so the returned value is with respect to the inner \ref rect.
*/

/*! \fn int QCPAxisRect::top() const

  Returns the pixel position of the top border of this axis rect. Margins are not taken into
  account here, so the returned value is with respect to the inner \ref rect.
*/

/*! \fn int QCPAxisRect::bottom() const

  Returns the pixel position of the bottom border of this axis rect. Margins are not taken into
  account here, so the returned value is with respect to the inner \ref rect.
*/

/*! \fn int QCPAxisRect::width() const

  Returns the pixel width of this axis rect. Margins are not taken into account here, so the
  returned value is with respect to the inner \ref rect.
*/

/*! \fn int QCPAxisRect::height() const

  Returns the pixel height of this axis rect. Margins are not taken into account here, so the
  returned value is with respect to the inner \ref rect.
*/

/*! \fn QSize QCPAxisRect::size() const

  Returns the pixel size of this axis rect. Margins are not taken into account here, so the
  returned value is with respect to the inner \ref rect.
*/

/*! \fn QPoint QCPAxisRect::topLeft() const

  Returns the top left corner of this axis rect in pixels. Margins are not taken into account here,
  so the returned value is with respect to the inner \ref rect.
*/

/*! \fn QPoint QCPAxisRect::topRight() const

  Returns the top right corner of this axis rect in pixels. Margins are not taken into account
  here, so the returned value is with respect to the inner \ref rect.
*/

/*! \fn QPoint QCPAxisRect::bottomLeft() const

  Returns the bottom left corner of this axis rect in pixels. Margins are not taken into account
  here, so the returned value is with respect to the inner \ref rect.
*/

/*! \fn QPoint QCPAxisRect::bottomRight() const

  Returns the bottom right corner of this axis rect in pixels. Margins are not taken into account
  here, so the returned value is with respect to the inner \ref rect.
*/

/*! \fn QPoint QCPAxisRect::center() const

  Returns the center of this axis rect in pixels. Margins are not taken into account here, so the
  returned value is with respect to the inner \ref rect.
*/

/* end documentation of inline functions */

namespace
{
bool beingDestroyed(const QCustomPlot* plot)
{
    return plot && plot->isBeingDestroyed();
}
} // namespace

/*!
  Creates a QCPAxisRect instance and sets default values. An axis is added for each of the four
  sides, the top and right axes are set invisible initially.
*/
QCPAxisRect::QCPAxisRect(QCustomPlot* parentPlot, bool setupDefaultAxes)
        : QCPArea(parentPlot)
        , mRangeDrag(Qt::Horizontal | Qt::Vertical)
        , mRangeZoom(Qt::Horizontal | Qt::Vertical)
        , mRangeZoomFactorHorz(0.85)
        , mRangeZoomFactorVert(0.85)
        , mDragging(false)
{
    mWheelZoomRestoreTimer.setSingleShot(true);
    mWheelZoomRestoreTimer.setInterval(300);
    connect(&mWheelZoomRestoreTimer, &QTimer::timeout, this,
            [this]()
            {
                if (mWheelZooming)
                {
                    mWheelZooming = false;
                    mParentPlot->setAntialiasedElements(mAAWheelBackup);
                    mParentPlot->setNotAntialiasedElements(mNotAAWheelBackup);
                    mParentPlot->replot(QCustomPlot::rpQueuedReplot);
                }
            });

    setMinimumSize(50, 50);
    setMinimumMargins(QMargins(15, 15, 15, 15));
    mAxes.insert(QCPAxis::atLeft, QList<QCPAxis*>());
    mAxes.insert(QCPAxis::atRight, QList<QCPAxis*>());
    mAxes.insert(QCPAxis::atTop, QList<QCPAxis*>());
    mAxes.insert(QCPAxis::atBottom, QList<QCPAxis*>());

    if (setupDefaultAxes && beingDestroyed(mParentPlot))
        qcp::reportMisuse(Q_FUNC_INFO, "the plot is being destroyed");
    else if (setupDefaultAxes)
    {
        QCPAxis* xAxis = addAxis(QCPAxis::atBottom);
        QCPAxis* yAxis = addAxis(QCPAxis::atLeft);
        QCPAxis* xAxis2 = addAxis(QCPAxis::atTop);
        QCPAxis* yAxis2 = addAxis(QCPAxis::atRight);
        xAxis2->setVisible(false);
        yAxis2->setVisible(false);
        xAxis->grid()->setVisible(true);
        yAxis->grid()->setVisible(true);
        xAxis2->grid()->setVisible(false);
        yAxis2->grid()->setVisible(false);
        xAxis2->grid()->setZeroLinePen(Qt::NoPen);
        yAxis2->grid()->setZeroLinePen(Qt::NoPen);
        xAxis2->grid()->setVisible(false);
        yAxis2->grid()->setVisible(false);
    }
}

QCPAxisRect::~QCPAxisRect()
{
    // Out of its layout and the plot's children: a slot that the scope below runs may delete the
    // plot, which would delete this rect a second time.
    if (qobject_cast<QCustomPlot*>(mParentPlot))
        detachFromOwners();
    const QCPEmissionScope scope(mParentPlot);
    // Deleted mid-gesture (by a rangeChanged slot): give the plot its antialiasing back. Without a
    // layer, the plot itself is being torn down.
    if (mParentPlot && layer())
    {
        if (mWheelZooming)
        {
            mParentPlot->setAntialiasedElements(mAAWheelBackup);
            mParentPlot->setNotAntialiasedElements(mNotAAWheelBackup);
        }
        if (mDragging && mParentPlot->noAntialiasingOnDrag())
        {
            mParentPlot->setAntialiasedElements(mAADragBackup);
            mParentPlot->setNotAntialiasedElements(mNotAADragBackup);
        }
    }
    // Elements leave the inset layout by emitting, so it goes while the plot still delivers.
    destroyInsetLayout();
    // The rect owns its scale: it goes with the rect, and its users detach (they emit inside the
    // scope above).
    if (mColorScale)
    {
        QCPColorScale* scale = mColorScale.data();
        scale->mOwnerRect = nullptr;
        delete scale;
        mColorScale = nullptr;
    }

    for (const auto& axis : qcp::guarded(axes()))
        if (axis)
            removeAxis(axis);
}

void QCPAxisRect::detachFromOwners()
{
    if (mParentLayout)
        mParentLayout->take(this);
    setParent(nullptr);
}

/*!
  Returns the number of axes on the axis rect side specified with \a type.

  \see axis
*/
int QCPAxisRect::axisCount(QCPAxis::AxisType type) const
{
    return mAxes.value(type).size();
}

/*!
  Returns the axis with the given \a index on the axis rect side specified with \a type.

  \see axisCount, axes
*/
QCPAxis* QCPAxisRect::axis(QCPAxis::AxisType type, int index) const
{
    QList<QCPAxis*> ax(mAxes.value(type));
    if (index >= 0 && index < ax.size())
    {
        return ax.at(index);
    }
    else
    {
        qDebug() << Q_FUNC_INFO << "Axis index out of bounds:" << index;
        return nullptr;
    }
}

/*!
  Returns all axes on the axis rect sides specified with \a types.

  \a types may be a single \ref QCPAxis::AxisType or an <tt>or</tt>-combination, to get the axes of
  multiple sides.

  \see axis
*/
QList<QCPAxis*> QCPAxisRect::axes(QCPAxis::AxisTypes types) const
{
    QList<QCPAxis*> result;
    if (types.testFlag(QCPAxis::atLeft))
        result << mAxes.value(QCPAxis::atLeft);
    if (types.testFlag(QCPAxis::atRight))
        result << mAxes.value(QCPAxis::atRight);
    if (types.testFlag(QCPAxis::atTop))
        result << mAxes.value(QCPAxis::atTop);
    if (types.testFlag(QCPAxis::atBottom))
        result << mAxes.value(QCPAxis::atBottom);
    return result;
}

/*! \overload

  Returns all axes of this axis rect.
*/
QList<QCPAxis*> QCPAxisRect::axes() const
{
    QList<QCPAxis*> result;
    QHashIterator<QCPAxis::AxisType, QList<QCPAxis*>> it(mAxes);
    while (it.hasNext())
    {
        it.next();
        result << it.value();
    }
    return result;
}

/*! \overload

  Returns all axes of this axis rect as base pointers, the type the range model lives in.
*/
QList<QCPAxisBase*> QCPAxisRect::allAxes() const
{
    QList<QCPAxisBase*> result;
    const QList<QCPAxis*> cartesianAxes = axes();
    result.reserve(cartesianAxes.size());
    for (QCPAxis* axis : cartesianAxes)
        result << axis;
    return result;
}

/*!
  Adds a new axis to the axis rect side specified with \a type, and returns it. If \a axis is 0, a
  new QCPAxis instance is created internally. QCustomPlot owns the returned axis, so if you want to
  remove an axis, use \ref removeAxis instead of deleting it manually.

  You may inject QCPAxis instances (or subclasses of QCPAxis) by setting \a axis to an axis that was
  previously created outside QCustomPlot. It is important to note that QCustomPlot takes ownership
  of the axis, so you may not delete it afterwards. Further, the \a axis must have been created
  with this axis rect as parent and with the same axis type as specified in \a type. If this is not
  the case, a debug output is generated, the axis is not added, and the method returns \c nullptr.

  This method can not be used to move \a axis between axis rects. The same \a axis instance must
  not be added multiple times to the same or different axis rects.

  If an axis rect side already contains one or more axes, the lower and upper endings of the new
  axis (\ref QCPAxis::setLowerEnding, \ref QCPAxis::setUpperEnding) are set to \ref
  QCPLineEnding::esHalfBar.

  \see addAxes, setupFullAxesBox
*/
QCPAxis* QCPAxisRect::addAxis(QCPAxis::AxisType type, QCPAxis* axis)
{
    QCP_CHECK_OR_RETURN(!beingDestroyed(mParentPlot), "the plot is being destroyed", nullptr);
    QCPAxis* newAxis = axis;
    if (!newAxis)
    {
        newAxis = new QCPAxis(this, type);
    }
    else // user provided existing axis instance, do some sanity checks
    {
        if (newAxis->axisType() != type)
        {
            qDebug() << Q_FUNC_INFO
                     << "passed axis has different axis type than specified in type parameter";
            return nullptr;
        }
        if (newAxis->axisRect() != this)
        {
            qDebug() << Q_FUNC_INFO
                     << "passed axis doesn't have this axis rect as parent axis rect";
            return nullptr;
        }
        if (axes().contains(newAxis))
        {
            qDebug() << Q_FUNC_INFO << "passed axis is already owned by this axis rect";
            return nullptr;
        }
    }
    if (!mAxes[type].isEmpty()) // multiple axes on one side, add half-bar axis ending to additional
                                // axes with offset
    {
        bool invert = (type == QCPAxis::atRight) || (type == QCPAxis::atBottom);
        newAxis->setLowerEnding(QCPLineEnding(QCPLineEnding::esHalfBar, 6, 10, !invert));
        newAxis->setUpperEnding(QCPLineEnding(QCPLineEnding::esHalfBar, 6, 10, invert));
    }
    mAxes[type].append(newAxis);

    // reset convenience axis pointers on parent QCustomPlot if they are unset:
    if (mParentPlot && mParentPlot->axisRectCount() > 0 && mParentPlot->axisRect(0) == this)
    {
        switch (type)
        {
            case QCPAxis::atBottom:
            {
                if (!mParentPlot->xAxis)
                    mParentPlot->xAxis = newAxis;
                break;
            }
            case QCPAxis::atLeft:
            {
                if (!mParentPlot->yAxis)
                    mParentPlot->yAxis = newAxis;
                break;
            }
            case QCPAxis::atTop:
            {
                if (!mParentPlot->xAxis2)
                    mParentPlot->xAxis2 = newAxis;
                break;
            }
            case QCPAxis::atRight:
            {
                if (!mParentPlot->yAxis2)
                    mParentPlot->yAxis2 = newAxis;
                break;
            }
        }
    }

    return newAxis;
}

/*!
  Adds a new axis with \ref addAxis to each axis rect side specified in \a types. This may be an
  <tt>or</tt>-combination of QCPAxis::AxisType, so axes can be added to multiple sides at once.

  Returns a list of the added axes.

  \see addAxis, setupFullAxesBox
*/
QList<QCPAxis*> QCPAxisRect::addAxes(QCPAxis::AxisTypes types)
{
    QList<QCPAxis*> result;
    if (types.testFlag(QCPAxis::atLeft))
        result << addAxis(QCPAxis::atLeft);
    if (types.testFlag(QCPAxis::atRight))
        result << addAxis(QCPAxis::atRight);
    if (types.testFlag(QCPAxis::atTop))
        result << addAxis(QCPAxis::atTop);
    if (types.testFlag(QCPAxis::atBottom))
        result << addAxis(QCPAxis::atBottom);
    return result;
}

/*!
  Removes the specified \a axis from the axis rect and deletes it.

  Returns true on success, i.e. if \a axis was a valid axis in this axis rect.

  \see addAxis
*/
bool QCPAxisRect::removeAxis(QCPAxis* axis)
{
    // Unlisted first, so the state is the one a plain delete leaves, and a slot on destroyed()
    // finds it gone.
    if (!unlistAxis(axis))
    {
        qDebug() << Q_FUNC_INFO << "Axis isn't in axis rect:" << reinterpret_cast<quintptr>(axis);
        return false;
    }
    delete axis;
    return true;
}

QCPCoordinateSystem* QCPAxisRect::createSystem(QCPAxisBase* key, QCPAxisBase* value)
{
    // Every axis of a rect is a cartesian one, and a frame needs one of each direction.
    auto* keyAxis = static_cast<QCPAxis*>(key);
    auto* valueAxis = static_cast<QCPAxis*>(value);
    if (keyAxis->orientation() == valueAxis->orientation())
    {
        qcp::reportMisuse(Q_FUNC_INFO,
                          "a cartesian system needs a horizontal and a vertical axis");
        return nullptr;
    }
    return new QCPCartesianSystem(this, keyAxis, valueAxis);
}

bool QCPAxisRect::unlistAxis(QCPAxis* axis)
{
    // Not from ~QObject of a plot already destroyed (a rect in no layout is its QObject child).
    // Nothing may be dirtied or re-laid out once the plot is going away.
    QCustomPlot* plot = qobject_cast<QCustomPlot*>(parentPlot());
    const bool alive = plot && !plot->isBeingDestroyed();
    // don't access axis->axisType(): axis may be an invalid pointer, so go through all the lists
    for (auto it = mAxes.begin(); it != mAxes.end(); ++it)
    {
        QList<QCPAxis*>& sideAxes = it.value();
        if (!sideAxes.contains(axis))
            continue;
        if (alive && sideAxes.first() == axis
            && sideAxes.size() > 1) // the next axis inherits the offset
            sideAxes[1]->setOffset(axis->offset());
        sideAxes.removeOne(axis);
        if (plot)
        {
            if (alive)
                markLayersUsingAxisDirty(axis);
            plot->axisRemoved(axis);
        }
        // The systems that map through it go while it still exists.
        axisRemoved(axis);
        return true;
    }
    return false;
}

void QCPAxisRect::markLayersUsingAxisDirty(const QCPAxis* axis)
{
    for (QCPAbstractPlottable* p : mParentPlot->mPlottables)
    {
        if (p->keyAxis() != axis && p->valueAxis() != axis)
            continue;
        if (p->layer())
            p->layer()->markDirty();
        for (QCPLayer* dependent : p->dependentLayers())
            dependent->markDirty();
    }
    markItemsUsingAxisDirty(axis);
}

void QCPAxisRect::markItemsUsingAxisDirty(const QCPAxis* axis)
{
    if (!mParentPlot || !axis)
        return;
    // Each layer knows which axes its items follow (and the axes of the items their anchors
    // follow): a range change marks those layers, not every item of the plot.
    for (QCPLayer* layer : std::as_const(mParentPlot->mLayers))
        if (layer->itemsUseAxis(axis))
            layer->markDirty();
}

/*!
  Zooms in (or out) to the passed rectangular region \a pixelRect, given in pixel coordinates.

  All axes of this axis rect will have their range zoomed accordingly. If you only wish to zoom
  specific axes, use the overloaded version of this method.

  \see QCustomPlot::setSelectionRectMode
*/
namespace
{
// A rangeChanged slot may delete an axis or the axis rect, or edit the rect's axis lists: the
// loops below walk snapshots, and stop once the rect is gone.
template <typename Step>
void forEachAxis(const QPointer<QCPAxisRect>& rect, const QList<QPointer<QCPAxis>> axes, Step step)
{
    for (const QPointer<QCPAxis>& axis : axes)
    {
        if (!rect)
            return;
        if (axis)
            step(axis.data());
    }
}

// Moves the axis from its range at the drag start by the pointer's travel.
void dragAxis(QCPAxis* axis, const QCPRange& start, double fromPixel, double toPixel)
{
    if (axis->scaleType() == QCPAxis::stLinear)
    {
        const double diff = axis->pixelToCoord(fromPixel) - axis->pixelToCoord(toPixel);
        axis->setRange(start.lower + diff, start.upper + diff);
    }
    else
    {
        const double ratio = axis->pixelToCoord(fromPixel) / axis->pixelToCoord(toPixel);
        axis->setRange(start.lower * ratio, start.upper * ratio);
    }
}

void dragAxes(const QPointer<QCPAxisRect>& rect, const QList<QPointer<QCPAxis>> axes,
              const QList<QCPRange> starts, double fromPixel, double toPixel)
{
    for (qsizetype i = 0; i < std::min(axes.size(), starts.size()) && rect; ++i)
        if (QCPAxis* axis = axes.at(i))
        {
            // A linked axis drags its source, through the source's pixels: the pointer's travel is
            // mapped onto the source's rect, which may be longer or shorter than this one.
            QCPAxis* target = axis->moveTarget();
            dragAxis(target, starts.at(i), axis->pixelOnMoveTarget(fromPixel),
                     axis->pixelOnMoveTarget(toPixel));
        }
}
}

void QCPAxisRect::zoom(const QRectF& pixelRect)
{
    zoom(pixelRect, axes());
}

/*! \overload

  Zooms in (or out) to the passed rectangular region \a pixelRect, given in pixel coordinates.

  Only the axes passed in \a affectedAxes will have their ranges zoomed accordingly.

  \see QCustomPlot::setSelectionRectMode
*/
void QCPAxisRect::zoom(const QRectF& pixelRect, const QList<QCPAxis*>& affectedAxes)
{
    if (affectedAxes.contains(nullptr))
        qDebug() << Q_FUNC_INFO << "a passed axis was zero";
    QList<QPointer<QCPAxis>> axes;
    for (QCPAxis* axis : moveRepresentatives(affectedAxes))
        axes.append(axis);
    forEachAxis(this, axes,
                [&pixelRect](QCPAxis* axis)
                {
                    const QCPRange pixelRange = axis->orientation() == Qt::Horizontal
                        ? QCPRange(pixelRect.left(), pixelRect.right())
                        : QCPRange(pixelRect.top(), pixelRect.bottom());
                    QCPAxis* target = axis->moveTarget(); // a linked axis zooms its source
                    target->setRange(
                        target->pixelToCoord(axis->pixelOnMoveTarget(pixelRange.lower)),
                        target->pixelToCoord(axis->pixelOnMoveTarget(pixelRange.upper)));
                });
}

/*!
  Convenience function to create an axis on each side that doesn't have any axes yet and set their
  visibility to true. Further, the top/right axes are assigned the following properties of the
  bottom/left axes:

  \li range (\ref QCPAxis::setRange)
  \li range reversed (\ref QCPAxis::setRangeReversed)
  \li scale type (\ref QCPAxis::setScaleType)
  \li tick visibility (\ref QCPAxis::setTicks)
  \li number format (\ref QCPAxis::setNumberFormat)
  \li number precision (\ref QCPAxis::setNumberPrecision)
  \li tick count of ticker (\ref QCPAxisTicker::setTickCount)
  \li tick origin of ticker (\ref QCPAxisTicker::setTickOrigin)

  Tick label visibility (\ref QCPAxis::setTickLabels) of the right and top axes are set to false.

  If \a connectRanges is true, the top and right axes are linked to the bottom and left ones
  (\ref QCPAxis::linkTo): moving either end of a pair moves the other, and a drag or a zoom moves
  the pair once.
*/
void QCPAxisRect::setupFullAxesBox(bool connectRanges)
{
    QCP_CHECK_OR_RETURN(!beingDestroyed(mParentPlot), "the plot is being destroyed");
    QCPAxis *xAxis, *yAxis, *xAxis2, *yAxis2;
    if (axisCount(QCPAxis::atBottom) == 0)
        xAxis = addAxis(QCPAxis::atBottom);
    else
        xAxis = axis(QCPAxis::atBottom);

    if (axisCount(QCPAxis::atLeft) == 0)
        yAxis = addAxis(QCPAxis::atLeft);
    else
        yAxis = axis(QCPAxis::atLeft);

    if (axisCount(QCPAxis::atTop) == 0)
        xAxis2 = addAxis(QCPAxis::atTop);
    else
        xAxis2 = axis(QCPAxis::atTop);

    if (axisCount(QCPAxis::atRight) == 0)
        yAxis2 = addAxis(QCPAxis::atRight);
    else
        yAxis2 = axis(QCPAxis::atRight);

    xAxis->setVisible(true);
    yAxis->setVisible(true);
    xAxis2->setVisible(true);
    yAxis2->setVisible(true);
    xAxis2->setTickLabels(false);
    yAxis2->setTickLabels(false);

    // A second call must re-copy from plain axes, whatever it does with the ranges afterwards.
    xAxis2->unlink();
    yAxis2->unlink();

    xAxis2->setRange(xAxis->range());
    xAxis2->setRangeReversed(xAxis->rangeReversed());
    xAxis2->setScaleType(xAxis->scaleType());
    xAxis2->setTicks(xAxis->ticks());
    xAxis2->setNumberFormat(xAxis->numberFormat());
    xAxis2->setNumberPrecision(xAxis->numberPrecision());
    xAxis2->ticker()->setTickCount(xAxis->ticker()->tickCount());
    xAxis2->ticker()->setTickOrigin(xAxis->ticker()->tickOrigin());

    yAxis2->setRange(yAxis->range());
    yAxis2->setRangeReversed(yAxis->rangeReversed());
    yAxis2->setScaleType(yAxis->scaleType());
    yAxis2->setTicks(yAxis->ticks());
    yAxis2->setNumberFormat(yAxis->numberFormat());
    yAxis2->setNumberPrecision(yAxis->numberPrecision());
    yAxis2->ticker()->setTickCount(yAxis->ticker()->tickCount());
    yAxis2->ticker()->setTickOrigin(yAxis->ticker()->tickOrigin());

    if (connectRanges)
    {
        xAxis2->linkTo(xAxis);
        yAxis2->linkTo(yAxis);
    }
}

namespace
{
// An axis that is gone (deleted, or removed from its rect) is on no rect.
bool isOn(const QCPAxis* axis, const QCPAxisRect* rect)
{
    return axis && axis->axisRect() == rect;
}
} // namespace

/*!
  Returns a list of all the plottables that are associated with this axis rect.

  A plottable is considered associated with an axis rect if it is drawn in a coordinate system of
  this axis rect.

  \see graphs, items
*/
QList<QCPAbstractPlottable*> QCPAxisRect::plottables() const
{
    // Note: don't append all QCPAxis::plottables() into a list, because we might get duplicate
    // entries
    QList<QCPAbstractPlottable*> result;
    for (QCPAbstractPlottable* plottable : mParentPlot->mPlottables)
    {
        // By the system, not by the axes: what a plottable sits in is the frame it is drawn in.
        if (plottable->system() && plottable->system()->area() == this)
            result.append(plottable);
    }
    return result;
}

/*!
  Returns a list of all the graphs that are associated with this axis rect.

  A graph is considered associated with an axis rect if it is drawn in a coordinate system of this
  axis rect.

  \see plottables, items
*/
QList<QCPGraph*> QCPAxisRect::graphs() const
{
    // Note: don't append all QCPAxis::graphs() into a list, because we might get duplicate entries
    QList<QCPGraph*> result;
    for (QCPGraph* graph : mParentPlot->mGraphs)
    {
        if (graph->system() && graph->system()->area() == this)
            result.append(graph);
    }
    return result;
}

/*!
  Returns a list of all the items that are associated with this axis rect.

  An item is considered associated with an axis rect if any of its positions has key or value axis
  set to an axis that is in this axis rect, or if any of its positions has \ref
  QCPItemPosition::setAxisRect set to the axis rect, or if the clip axis rect (\ref
  QCPAbstractItem::setClipAxisRect) is set to this axis rect.

  \see plottables, graphs
*/
QList<QCPAbstractItem*> QCPAxisRect::items() const
{
    // Note: don't just append all QCPAxis::items() into a list, because we might get duplicate
    // entries
    //       and miss those items that have this axis rect as clipAxisRect.
    QList<QCPAbstractItem*> result;
    for (QCPAbstractItem* item : mParentPlot->mItems)
    {
        if (item->clipAxisRect() == this)
        {
            result.append(item);
            continue;
        }
        for (QCPItemPosition* position : item->positions())
        {
            if (position->axisRect() == this || isOn(position->keyAxis(), this)
                || isOn(position->valueAxis(), this))
            {
                result.append(item);
                break;
            }
        }
    }
    return result;
}

/*!
  Returns the colour scale this rect owns, making it on the first call.

  The scale sits in the margin of the side its \ref QCPColorScale::type names, outside that side's
  axes, and this rect's auto margin on that side grows to hold it. It is visible exactly while a
  plottable uses it: a scale nobody uses takes no room and is never laid out.

  The rect deletes the scale with itself. Removing the scale leaves this rect working; the next
  call makes a new scale.

  \see QCPColorScale
*/
QCPColorScaleOps<QCPColorScale> QCPAxisRect::colorScale()
{
    if (!mayOwnColorScale() || !mParentPlot)
        return { };
    if (!mColorScale)
    {
        mColorScale = new QCPColorScale(mParentPlot);
        mColorScale->mOwnerRect = this;
        // Off the data layer: a visible bar there would make every pan repaint that layer instead
        // of shifting it. The axes layer repaints on a pan anyway, for its ticks.
        if (QCPLayer* axesLayer = mParentPlot->layer(QLatin1String("axes")))
            mColorScale->setLayer(axesLayer);
        // A theme set before this scale existed must reach it like any other axis.
        if (mParentPlot->mTheme)
            mParentPlot->applyThemeToAxis(mColorScale->axis());
        colorScaleUsersChanged();
    }
    return qcp::makeHandle<QCPColorScaleOps<QCPColorScale>>(mColorScale.data());
}

/*! \internal

  Returns the rect's own scale while at least one plottable uses it, otherwise null. A scale with
  no user is hidden and costs nothing per replot.
*/
QCPColorScale* QCPAxisRect::usedColorScale() const
{
    return (mColorScale && !mColorScale->users().isEmpty()) ? mColorScale.data() : nullptr;
}

/*! \internal

  The width a vertical scale takes in a left or right margin, or the height a horizontal scale
  takes in a top or bottom margin, its own margins included.
*/
int QCPAxisRect::colorScaleExtent() const
{
    if (!mColorScale)
        return 0;
    const bool vertical = QCPAxis::orientation(mColorScale->type()) == Qt::Vertical;
    const QSize barSize = mColorScale->minimumSize();
    const QMargins barMargins = mColorScale->margins();
    return vertical ? barSize.width() + barMargins.left() + barMargins.right()
                    : barSize.height() + barMargins.top() + barMargins.bottom();
}

/*! \internal

  Puts the rect's scale in the margin of the side its type names, outside that side's axes.
*/
void QCPAxisRect::placeColorScale()
{
    const int extent = colorScaleExtent();
    QRect scaleOuterRect;
    switch (mColorScale->type())
    {
        case QCPAxis::atLeft:
            scaleOuterRect = QRect(outerRect().left(), rect().top(), extent, rect().height());
            break;
        case QCPAxis::atBottom:
            scaleOuterRect
                = QRect(rect().left(), outerRect().bottom() - extent + 1, rect().width(), extent);
            break;
        case QCPAxis::atTop:
            scaleOuterRect = QRect(rect().left(), outerRect().top(), rect().width(), extent);
            break;
        default:
            scaleOuterRect
                = QRect(outerRect().right() - extent + 1, rect().top(), extent, rect().height());
            break;
    }
    mColorScale->setOuterRect(scaleOuterRect);
}

/*! \internal

  Called when the rect's scale gains or loses a user or changes the room it needs: the margin
  holding it moves.
*/
void QCPAxisRect::colorScaleUsersChanged()
{
    markLayoutAppearanceDirty();
}

/*!
  This method is called automatically upon replot and doesn't need to be called by users of
  QCPAxisRect.

  Calls the base class implementation to update the margins (see \ref QCPArea::update), which also
  lays out the inset layout inside the rect, then prepares the axes' ticks.

  \seebaseclassmethod
*/
void QCPAxisRect::update(UpdatePhase phase)
{
    QCPColorScale* scale = usedColorScale();
    if (mColorScale)
        mColorScale->setVisible(scale != nullptr);
    // A hidden scale is not touched at all: no layout pass, no draw, no cost per replot.
    if (scale && phase == upMargins)
        scale->update(phase); // its minimum size is the margin this rect has to add

    QCPArea::update(phase);

    if (phase == upPreparation)
        for (QCPAxis* axis : axes())
            axis->setupTickVectors();

    if (scale)
    {
        if (phase == upLayout)
            placeColorScale();
        if (phase != upMargins) // already done before the margins were computed
            scale->update(phase);
    }
}

/*! \internal

  The axes of \a orientation a drag (\a drag true) or a zoom moves. A list the user set wins
  whole; otherwise it is every axis of that orientation in the rect, bottom then top (left then
  right), that allows the interaction: a 2.x plot dragged its bottom and left axes only, this one
  moves all four unless an axis opted out.
*/
QList<QCPAxis*> QCPAxisRect::effectiveRangeAxes(bool drag, Qt::Orientation orientation) const
{
    const bool explicitSet = drag ? mRangeDragAxesSet : mRangeZoomAxesSet;
    const QList<QPointer<QCPAxis>>& stored = drag
        ? (orientation == Qt::Horizontal ? mRangeDragHorzAxis : mRangeDragVertAxis)
        : (orientation == Qt::Horizontal ? mRangeZoomHorzAxis : mRangeZoomVertAxis);
    QList<QCPAxis*> result;
    if (explicitSet)
    {
        for (const QPointer<QCPAxis>& axis : stored)
            if (axis)
                result.append(axis.data());
        return result;
    }
    const QList<QCPAxis::AxisType> sides = orientation == Qt::Horizontal
        ? QList<QCPAxis::AxisType> { QCPAxis::atBottom, QCPAxis::atTop }
        : QList<QCPAxis::AxisType> { QCPAxis::atLeft, QCPAxis::atRight };
    for (QCPAxis::AxisType side : sides)
    {
        for (QCPAxis* axis : axes(side))
        {
            if (!axis || (drag ? !axis->rangeDraggable() : !axis->rangeZoomable()))
                continue;
            // A linked axis only stands in for its source while the source allows it too: opting
            // the source out must stop the link as well.
            QCPAxis* source = axis->linkSource();
            if (source && (drag ? !source->rangeDraggable() : !source->rangeZoomable()))
                continue;
            result.append(axis);
        }
    }
    return result;
}

/*! \internal

  Turns \a axes into the axes that actually move: one per source. A source listed itself wins and
  moves once; otherwise the first listed axis linked to it moves it, through its own pixel motion
  and the link's write-back, so a linked axis and its source never move twice.
*/
QList<QCPAxis*> QCPAxisRect::moveRepresentatives(const QList<QCPAxis*>& axes) const
{
    QList<QCPAxis*> moved;
    QSet<QCPAxis*> covered;
    for (QCPAxis* axis : axes)
    {
        if (!axis || axis->linkSource() || covered.contains(axis))
            continue;
        covered.insert(axis);
        moved.append(axis);
    }
    for (QCPAxis* axis : axes)
    {
        QCPAxis* source = axis ? axis->linkSource() : nullptr;
        if (!source || covered.contains(source))
            continue;
        covered.insert(source);
        moved.append(axis);
    }
    return moved;
}

/*!
  Returns the range drag axis of the \a orientation provided. If multiple axes were set, returns
  the first one (use \ref rangeDragAxes to retrieve a list with all set axes).

  \see setRangeDragAxes
*/
QCPAxis* QCPAxisRect::rangeDragAxis(Qt::Orientation orientation)
{
    const QList<QCPAxis*> axes = effectiveRangeAxes(true, orientation);
    return axes.isEmpty() ? nullptr : axes.first();
}

/*!
  Returns the range zoom axis of the \a orientation provided. If multiple axes were set, returns
  the first one (use \ref rangeZoomAxes to retrieve a list with all set axes).

  \see setRangeZoomAxes
*/
QCPAxis* QCPAxisRect::rangeZoomAxis(Qt::Orientation orientation)
{
    const QList<QCPAxis*> axes = effectiveRangeAxes(false, orientation);
    return axes.isEmpty() ? nullptr : axes.first();
}

/*!
  Returns all range drag axes of the \a orientation provided.

  \see rangeZoomAxis, setRangeZoomAxes
*/
QList<QCPAxis*> QCPAxisRect::rangeDragAxes(Qt::Orientation orientation)
{
    return effectiveRangeAxes(true, orientation);
}

/*!
  Returns all range zoom axes of the \a orientation provided.

  \see rangeDragAxis, setRangeDragAxes
*/
QList<QCPAxis*> QCPAxisRect::rangeZoomAxes(Qt::Orientation orientation)
{
    return effectiveRangeAxes(false, orientation);
}

/*!
  Returns the range zoom factor of the \a orientation provided.

  \see setRangeZoomFactor
*/
double QCPAxisRect::rangeZoomFactor(Qt::Orientation orientation)
{
    return (orientation == Qt::Horizontal ? mRangeZoomFactorHorz : mRangeZoomFactorVert);
}

/*!
  Sets which axis orientation may be range dragged by the user with mouse interaction.
  What orientation corresponds to which specific axis can be set with
  \ref setRangeDragAxes(QCPAxis *horizontal, QCPAxis *vertical). By default every axis of the rect
  of that orientation moves, unless it opted out with \ref QCPAxis::setRangeDraggable.

  To disable range dragging entirely, pass \c nullptr as \a orientations or remove \ref
  QCP::iRangeDrag from \ref QCustomPlot::setInteractions. To enable range dragging for both
  directions, pass <tt>Qt::Horizontal | Qt::Vertical</tt> as \a orientations.

  In addition to setting \a orientations to a non-zero value, make sure \ref
  QCustomPlot::setInteractions contains \ref QCP::iRangeDrag to enable the range dragging
  interaction.

  \see setRangeZoom, setRangeDragAxes, QCustomPlot::setNoAntialiasingOnDrag
*/
void QCPAxisRect::setRangeDrag(Qt::Orientations orientations)
{
    mRangeDrag = orientations;
}

/*!
  Sets which axis orientation may be zoomed by the user with the mouse wheel. What orientation
  corresponds to which specific axis can be set with \ref setRangeZoomAxes(QCPAxis *horizontal,
  QCPAxis *vertical). By default every axis of the rect of that orientation zooms, unless it opted
  out with \ref QCPAxis::setRangeZoomable.

  To disable range zooming entirely, pass \c nullptr as \a orientations or remove \ref
  QCP::iRangeZoom from \ref QCustomPlot::setInteractions. To enable range zooming for both
  directions, pass <tt>Qt::Horizontal | Qt::Vertical</tt> as \a orientations.

  In addition to setting \a orientations to a non-zero value, make sure \ref
  QCustomPlot::setInteractions contains \ref QCP::iRangeZoom to enable the range zooming
  interaction.

  \see setRangeZoomFactor, setRangeZoomAxes, setRangeDrag
*/
void QCPAxisRect::setRangeZoom(Qt::Orientations orientations)
{
    mRangeZoom = orientations;
}

/*! \overload

  Sets the axes whose range will be dragged when \ref setRangeDrag enables mouse range dragging on
  the QCustomPlot widget. Pass \c nullptr if no axis shall be dragged in the respective
  orientation.

  Use the overload taking a list of axes, if multiple axes (more than one per orientation) shall
  react to dragging interactions.

  \see setRangeZoomAxes
*/
void QCPAxisRect::setRangeDragAxes(QCPAxis* horizontal, QCPAxis* vertical)
{
    QList<QCPAxis*> horz, vert;
    if (horizontal)
        horz.append(horizontal);
    if (vertical)
        vert.append(vertical);
    setRangeDragAxes(horz, vert);
}

/*! \overload

  This method allows to set up multiple axes to react to horizontal and vertical dragging. The drag
  orientation that the respective axis will react to is deduced from its orientation (\ref
  QCPAxis::orientation).

  In the unusual case that you wish to e.g. drag a vertically oriented axis with a horizontal drag
  motion, use the overload taking two separate lists for horizontal and vertical dragging.
*/
void QCPAxisRect::setRangeDragAxes(QList<QCPAxis*> axes)
{
    QList<QCPAxis*> horz, vert;
    bool refused = false;
    for (QCPAxis* ax : axes)
    {
        if (!ax)
        {
            if (!refused)
                qcp::reportMisuse(Q_FUNC_INFO, "a passed axis was null");
            refused = true;
            continue;
        }
        if (ax->orientation() == Qt::Horizontal)
            horz.append(ax);
        else
            vert.append(ax);
    }
    setRangeDragAxes(horz, vert);
}

/*! \overload

  This method allows to set multiple axes up to react to horizontal and vertical dragging, and
  define specifically which axis reacts to which drag orientation (irrespective of the axis
  orientation).
*/
void QCPAxisRect::setRangeDragAxes(QList<QCPAxis*> horizontal, QList<QCPAxis*> vertical)
{
    mRangeDragAxesSet = true;
    mRangeDragHorzAxis.clear();
    for (QCPAxis* ax : horizontal)
    {
        QPointer<QCPAxis> axPointer(ax);
        if (!axPointer.isNull())
            mRangeDragHorzAxis.append(axPointer);
        else
            qDebug() << Q_FUNC_INFO
                     << "invalid axis passed in horizontal list:" << reinterpret_cast<quintptr>(ax);
    }
    mRangeDragVertAxis.clear();
    for (QCPAxis* ax : vertical)
    {
        QPointer<QCPAxis> axPointer(ax);
        if (!axPointer.isNull())
            mRangeDragVertAxis.append(axPointer);
        else
            qDebug() << Q_FUNC_INFO
                     << "invalid axis passed in vertical list:" << reinterpret_cast<quintptr>(ax);
    }
}

/*!
  Sets the axes whose range will be zoomed when \ref setRangeZoom enables mouse wheel zooming on
  the QCustomPlot widget. Pass \c nullptr if no axis shall be zoomed in the respective orientation.

  The two axes can be zoomed with different strengths, when different factors are passed to \ref
  setRangeZoomFactor(double horizontalFactor, double verticalFactor).

  Use the overload taking a list of axes, if multiple axes (more than one per orientation) shall
  react to zooming interactions.

  \see setRangeDragAxes
*/
void QCPAxisRect::setRangeZoomAxes(QCPAxis* horizontal, QCPAxis* vertical)
{
    QList<QCPAxis*> horz, vert;
    if (horizontal)
        horz.append(horizontal);
    if (vertical)
        vert.append(vertical);
    setRangeZoomAxes(horz, vert);
}

/*! \overload

  This method allows to set up multiple axes to react to horizontal and vertical range zooming. The
  zoom orientation that the respective axis will react to is deduced from its orientation (\ref
  QCPAxis::orientation).

  In the unusual case that you wish to e.g. zoom a vertically oriented axis with a horizontal zoom
  interaction, use the overload taking two separate lists for horizontal and vertical zooming.
*/
void QCPAxisRect::setRangeZoomAxes(QList<QCPAxis*> axes)
{
    QList<QCPAxis*> horz, vert;
    bool refused = false;
    for (QCPAxis* ax : axes)
    {
        if (!ax)
        {
            if (!refused)
                qcp::reportMisuse(Q_FUNC_INFO, "a passed axis was null");
            refused = true;
            continue;
        }
        if (ax->orientation() == Qt::Horizontal)
            horz.append(ax);
        else
            vert.append(ax);
    }
    setRangeZoomAxes(horz, vert);
}

/*! \overload

  This method allows to set multiple axes up to react to horizontal and vertical zooming, and
  define specifically which axis reacts to which zoom orientation (irrespective of the axis
  orientation).
*/
void QCPAxisRect::setRangeZoomAxes(QList<QCPAxis*> horizontal, QList<QCPAxis*> vertical)
{
    mRangeZoomAxesSet = true;
    mRangeZoomHorzAxis.clear();
    for (QCPAxis* ax : horizontal)
    {
        QPointer<QCPAxis> axPointer(ax);
        if (!axPointer.isNull())
            mRangeZoomHorzAxis.append(axPointer);
        else
            qDebug() << Q_FUNC_INFO
                     << "invalid axis passed in horizontal list:" << reinterpret_cast<quintptr>(ax);
    }
    mRangeZoomVertAxis.clear();
    for (QCPAxis* ax : vertical)
    {
        QPointer<QCPAxis> axPointer(ax);
        if (!axPointer.isNull())
            mRangeZoomVertAxis.append(axPointer);
        else
            qDebug() << Q_FUNC_INFO
                     << "invalid axis passed in vertical list:" << reinterpret_cast<quintptr>(ax);
    }
}

/*!
  Forgets the explicit drag axis lists and returns to dragging every axis of the rect that allows
  it (see \ref QCPAxis::setRangeDraggable).

  \see setRangeDragAxes
*/
void QCPAxisRect::resetRangeDragAxes()
{
    mRangeDragAxesSet = false;
    mRangeDragHorzAxis.clear();
    mRangeDragVertAxis.clear();
}

/*!
  Forgets the explicit zoom axis lists and returns to zooming every axis of the rect that allows
  it (see \ref QCPAxis::setRangeZoomable).

  \see setRangeZoomAxes
*/
void QCPAxisRect::resetRangeZoomAxes()
{
    mRangeZoomAxesSet = false;
    mRangeZoomHorzAxis.clear();
    mRangeZoomVertAxis.clear();
}

/*!
  Sets how strong one rotation step of the mouse wheel zooms, when range zoom was activated with
  \ref setRangeZoom. The two parameters \a horizontalFactor and \a verticalFactor provide a way to
  let the horizontal axis zoom at different rates than the vertical axis. Which axis is horizontal
  and which is vertical, can be set with \ref setRangeZoomAxes.

  When the zoom factor is greater than one, scrolling the mouse wheel backwards (towards the user)
  will zoom in (make the currently visible range smaller). For zoom factors smaller than one, the
  same scrolling direction will zoom out.
*/
void QCPAxisRect::setRangeZoomFactor(double horizontalFactor, double verticalFactor)
{
    mRangeZoomFactorHorz = horizontalFactor;
    mRangeZoomFactorVert = verticalFactor;
}

/*! \overload

  Sets both the horizontal and vertical zoom \a factor.
*/
void QCPAxisRect::setRangeZoomFactor(double factor)
{
    mRangeZoomFactorHorz = factor;
    mRangeZoomFactorVert = factor;
}

/*! \internal

  This function makes sure multiple axes on the side specified with \a type don't collide, but are
  distributed according to their respective space requirement (QCPAxis::calculateMargin).

  It does this by setting an appropriate offset (\ref QCPAxis::setOffset) on all axes except the
  one with index zero.

  This function is called by \ref calculateAutoMargin.
*/
void QCPAxisRect::updateAxesOffset(QCPAxis::AxisType type)
{
    const QList<QCPAxis*> axesList = mAxes.value(type);
    if (axesList.isEmpty())
        return;

    bool isFirstVisible
        = !axesList.first()
               ->visible(); // if the first axis is visible, the second axis (which is where the
                            // loop starts) isn't the first visible axis, so initialize with false
    for (int i = 1; i < axesList.size(); ++i)
    {
        int offset = axesList.at(i - 1)->offset() + axesList.at(i - 1)->calculateMargin();
        if (axesList.at(i)->visible()) // only add inner tick length to offset if this axis is
                                       // visible and it's not the first visible one (might happen
                                       // if true first axis is invisible)
        {
            if (!isFirstVisible)
                offset += axesList.at(i)->tickLengthIn();
            isFirstVisible = false;
        }
        axesList.at(i)->setOffset(offset);
    }
}

/* inherits documentation from base class */
int QCPAxisRect::calculateAutoMargin(QCP::MarginSide side)
{
    if (!mAutoMargins.testFlag(side))
        qDebug() << Q_FUNC_INFO << "Called with side that isn't specified as auto margin";

    updateAxesOffset(QCPAxis::marginSideToAxisType(side));

    // note: only need to look at the last (outer most) axis to determine the total margin, due to
    // updateAxisOffset call
    const QList<QCPAxis*> axesList = mAxes.value(QCPAxis::marginSideToAxisType(side));
    int margin
        = axesList.isEmpty() ? 0 : axesList.last()->offset() + axesList.last()->calculateMargin();
    if (const QCPColorScale* scale = usedColorScale())
        if (QCPAxis::marginSideToAxisType(side) == scale->type())
            margin += colorScaleExtent();
    return margin;
}

/*! \internal

  Reacts to a change in layout to potentially set the convenience axis pointers \ref
  QCustomPlot::xAxis, \ref QCustomPlot::yAxis, etc. of the parent QCustomPlot to the respective
  axes of this axis rect. This is only done if the respective convenience pointer is currently zero
  and if there is no QCPAxisRect at position (0, 0) of the plot layout.

  This automation makes it simpler to replace the main axis rect with a newly created one, without
  the need to manually reset the convenience pointers.
*/
void QCPAxisRect::layoutChanged()
{
    if (mParentPlot && mParentPlot->axisRectCount() > 0 && mParentPlot->axisRect(0) == this)
    {
        if (axisCount(QCPAxis::atBottom) > 0 && !mParentPlot->xAxis)
            mParentPlot->xAxis = axis(QCPAxis::atBottom);
        if (axisCount(QCPAxis::atLeft) > 0 && !mParentPlot->yAxis)
            mParentPlot->yAxis = axis(QCPAxis::atLeft);
        if (axisCount(QCPAxis::atTop) > 0 && !mParentPlot->xAxis2)
            mParentPlot->xAxis2 = axis(QCPAxis::atTop);
        if (axisCount(QCPAxis::atRight) > 0 && !mParentPlot->yAxis2)
            mParentPlot->yAxis2 = axis(QCPAxis::atRight);
    }
}

/*! \internal

  Event handler for when a mouse button is pressed on the axis rect. If the left mouse button is
  pressed, the range dragging interaction is initialized (the actual range manipulation happens in
  the \ref mouseMoveEvent).

  The mDragging flag is set to true and some anchor points are set that are needed to determine the
  distance the mouse was dragged in the mouse move/release events later.

  \see mouseMoveEvent, mouseReleaseEvent
*/
void QCPAxisRect::mousePressEvent(QMouseEvent* event, [[maybe_unused]] const QVariant& details)
{
    if (event->buttons() & Qt::LeftButton)
    {
        mDragging = true;
        // initialize antialiasing backup in case we start dragging:
        if (mParentPlot->noAntialiasingOnDrag())
        {
            // Inside a wheel zoom's restore window the plot shows the wheel's forced state.
            mAADragBackup = mWheelZooming ? mAAWheelBackup : mParentPlot->antialiasedElements();
            mNotAADragBackup
                = mWheelZooming ? mNotAAWheelBackup : mParentPlot->notAntialiasedElements();
        }
        // Mouse range dragging interaction:
        if (mParentPlot->interactions().testFlag(QCP::iRangeDrag))
        {
            mDragHorzAxes.clear();
            mDragStartHorzRange.clear();
            for (QCPAxis* axis : moveRepresentatives(effectiveRangeAxes(true, Qt::Horizontal)))
            {
                mDragHorzAxes.append(axis);
                mDragStartHorzRange.append(axis->moveTarget()->range());
            }
            mDragVertAxes.clear();
            mDragStartVertRange.clear();
            for (QCPAxis* axis : moveRepresentatives(effectiveRangeAxes(true, Qt::Vertical)))
            {
                mDragVertAxes.append(axis);
                mDragStartVertRange.append(axis->moveTarget()->range());
            }
        }
    }
}

/*! \internal

  Event handler for when the mouse is moved on the axis rect. If range dragging was activated in a
  preceding \ref mousePressEvent, the range is moved accordingly.

  \see mousePressEvent, mouseReleaseEvent
*/
void QCPAxisRect::mouseMoveEvent(QMouseEvent* event, const QPointF& startPos)
{
    // Mouse range dragging interaction:
    if (!mDragging || !mParentPlot->interactions().testFlag(QCP::iRangeDrag))
        return;
    const QPointer<QCPAxisRect> alive(this);
    const QPointF pos = event->pos();
    if (mRangeDrag.testFlag(Qt::Horizontal))
        dragAxes(alive, mDragHorzAxes, mDragStartHorzRange, startPos.x(), pos.x());
    if (alive && mRangeDrag.testFlag(Qt::Vertical))
        dragAxes(alive, mDragVertAxes, mDragStartVertRange, startPos.y(), pos.y());

    if (alive && mRangeDrag != 0) // if either vertical or horizontal drag was enabled, do a replot
    {
        if (mParentPlot->noAntialiasingOnDrag())
            mParentPlot->setNotAntialiasedElements(QCP::aeAll);
        mParentPlot->replot(QCustomPlot::rpQueuedReplot);
    }
}

/* inherits documentation from base class */
void QCPAxisRect::mouseReleaseEvent([[maybe_unused]] QMouseEvent* event,
                                    [[maybe_unused]] const QPointF& startPos)
{
    mDragging = false;
    if (mParentPlot->noAntialiasingOnDrag())
    {
        mParentPlot->setAntialiasedElements(mAADragBackup);
        mParentPlot->setNotAntialiasedElements(mNotAADragBackup);
    }
}

/*! \internal

  Event handler for mouse wheel events. If rangeZoom is Qt::Horizontal, Qt::Vertical or both, the
  ranges of the axes defined as rangeZoomHorzAxis and rangeZoomVertAxis are scaled. The center of
  the scaling operation is the current cursor position inside the axis rect. The scaling factor is
  dependent on the mouse wheel delta (which direction the wheel was rotated) to provide a natural
  zooming feel. The Strength of the zoom can be controlled via \ref setRangeZoomFactor.

  Note, that event->angleDelta() is usually +/-120 for single rotation steps. However, if the mouse
  wheel is turned rapidly, many steps may bunch up to one event, so the delta may then be multiples
  of 120. This is taken into account here, by calculating \a wheelSteps and using it as exponent of
  the range zoom factor. This takes care of the wheel direction automatically, by inverting the
  factor, when the wheel step is negative (f^-1 = 1/f).
*/
namespace
{
// Moves the axis so its data follows a pointer moved by pixels along it, as a drag does.
void followPointer(QCPAxis* axis, double pixels)
{
    // A linked axis pans its source, through the source's pixels.
    QCPAxis* target = axis->moveTarget();
    const QCPRange range = target->range();
    const double from = axis->pixelOnMoveTarget(0.0);
    const double to = axis->pixelOnMoveTarget(pixels);
    if (target->scaleType() == QCPAxis::stLogarithmic)
    {
        const double ratio = target->pixelToCoord(from) / target->pixelToCoord(to);
        target->setRange(range.lower * ratio, range.upper * ratio);
    }
    else
    {
        const double diff = target->pixelToCoord(from) - target->pixelToCoord(to);
        target->setRange(range.lower + diff, range.upper + diff);
    }
}

// A trackpad reports the swipe in pixels; a mouse wheel only in angle steps.
QPointF swipePixels(const QWheelEvent* event)
{
    constexpr double pixelsPerWheelStep = 50.0;
    return event->pixelDelta().isNull()
        ? QPointF(event->angleDelta()) * (pixelsPerWheelStep / 120.0)
        : QPointF(event->pixelDelta());
}
}

void QCPAxisRect::panByPixels(const QPointF& delta)
{
    const QPointer<QCPAxisRect> alive(this);
    QList<QPointer<QCPAxis>> moved;
    if (mRangeDrag.testFlag(Qt::Horizontal))
    {
        for (QCPAxis* axis : moveRepresentatives(effectiveRangeAxes(true, Qt::Horizontal)))
            moved.append(axis);
        forEachAxis(alive, moved, [&](QCPAxis* axis) { followPointer(axis, delta.x()); });
    }
    if (alive && mRangeDrag.testFlag(Qt::Vertical))
    {
        moved.clear();
        for (QCPAxis* axis : moveRepresentatives(effectiveRangeAxes(true, Qt::Vertical)))
            moved.append(axis);
        forEachAxis(alive, moved, [&](QCPAxis* axis) { followPointer(axis, delta.y()); });
    }
}

void QCPAxisRect::zoomAt(const QPointF& pos, double factor)
{
    zoomAxesAt(pos, factor, factor);
}

void QCPAxisRect::zoomAxesAt(const QPointF& pos, double horizontalFactor, double verticalFactor)
{
    const QPointer<QCPAxisRect> alive(this);
    QList<QPointer<QCPAxis>> moved;
    if (mRangeZoom.testFlag(Qt::Horizontal))
    {
        for (QCPAxis* axis : moveRepresentatives(effectiveRangeAxes(false, Qt::Horizontal)))
            moved.append(axis);
        forEachAxis(alive, moved,
                    [&](QCPAxis* axis)
                    {
                        QCPAxis* target = axis->moveTarget(); // a linked axis zooms its source
                        target->scaleRange(horizontalFactor,
                                           target->pixelToCoord(axis->pixelOnMoveTarget(pos.x())));
                    });
    }
    if (alive && mRangeZoom.testFlag(Qt::Vertical))
    {
        moved.clear();
        for (QCPAxis* axis : moveRepresentatives(effectiveRangeAxes(false, Qt::Vertical)))
            moved.append(axis);
        forEachAxis(alive, moved,
                    [&](QCPAxis* axis)
                    {
                        QCPAxis* target = axis->moveTarget(); // a linked axis zooms its source
                        target->scaleRange(verticalFactor,
                                           target->pixelToCoord(axis->pixelOnMoveTarget(pos.y())));
                    });
    }
}

void QCPAxisRect::wheelEvent(QWheelEvent* event)
{
    PROFILE_HERE_N("QCPAxisRect::wheelEvent");
    // A browser pinch can come with a pixel delta only.
    const double delta
        = event->angleDelta().y() != 0 ? event->angleDelta().y() : event->pixelDelta().y();
    const QPointF pos = event->position();

    // With gestures, a swipe pans; a pinch arrives as Ctrl+wheel and zooms below.
    if (mParentPlot->interactions().testFlag(QCP::iGestures)
        && !event->modifiers().testFlag(Qt::ControlModifier))
    {
        if (mParentPlot->interactions().testFlag(QCP::iRangeDrag))
        {
            const QPointer<QCPAxisRect> alive(this); // a rangeChanged slot may delete this rect
            panByPixels(swipePixels(event));
            if (alive)
                mParentPlot->replot(QCustomPlot::rpQueuedReplot);
        }
        return;
    }

    // Mouse range zooming interaction:
    if (mParentPlot->interactions().testFlag(QCP::iRangeZoom))
    {
        if (mRangeZoom != 0)
        {
            if (mParentPlot->noAntialiasingOnDrag())
            {
                if (!mWheelZooming)
                {
                    mWheelZooming = true;
                    // During a drag the plot shows the drag's forced state.
                    const bool dragForced = mDragging && mParentPlot->noAntialiasingOnDrag();
                    mAAWheelBackup
                        = dragForced ? mAADragBackup : mParentPlot->antialiasedElements();
                    mNotAAWheelBackup
                        = dragForced ? mNotAADragBackup : mParentPlot->notAntialiasedElements();
                }
                mParentPlot->setNotAntialiasedElements(QCP::aeAll);
                mWheelZoomRestoreTimer.start();
            }

            const double wheelSteps = delta / 120.0;
            const QPointer<QCPAxisRect> alive(this); // a rangeChanged slot may delete this rect
            zoomAxesAt(pos, qPow(mRangeZoomFactorHorz, wheelSteps),
                       qPow(mRangeZoomFactorVert, wheelSteps));
            if (alive)
                mParentPlot->replot(QCustomPlot::rpQueuedReplot);
        }
    }
}

void QCPAxisRect::markLayoutAppearanceDirty()
{
    if (!mParentPlot) // the constructor sets sizes before there is a plot to mark
        return QCPLayoutElement::markLayoutAppearanceDirty();
    // Not just the rect's own layer: a pending pan translation would keep the axes, grids and
    // plottables of this rect at their old pixels.
    markAffectedLayersDirty(true);
    QCPLayoutElement::markLayoutAppearanceDirty();
}

void QCPAxisRect::markAffectedLayersDirty(bool invalidate)
{
    // Collect unique layers from axes, grids, and plottables in this rect.
    // Typically <=4 unique layers, so linear scan is fine.
    QVarLengthArray<QCPLayer*, 8> seen;
    auto markOnce = [&](QCPLayer* l)
    {
        if (l && !seen.contains(l))
        {
            seen.append(l);
            if (invalidate)
                l->invalidatePaintBuffer();
            else
                l->markDirty();
        }
    };

    const bool gpuGrid = mParentPlot->gridRhiLayer() != nullptr;
    for (const auto& axisList : mAxes)
    {
        for (QCPAxis* ax : axisList)
        {
            markOnce(ax->layer());
            // A linked axis' grid is drawn by the CPU while the GPU grid has it skipped.
            if (QCPGrid* grid = ax->gridIfExists(); grid && (!gpuGrid || ax->linkSource()))
                markOnce(grid->layer());
        }
    }
    for (QCPAbstractPlottable* p : mParentPlot->mPlottables)
    {
        // One pass over the plot's plottables, selected by the system they are drawn in: a plottable
        // whose system died (its axis was removed) is skipped as it draws nothing.
        if (p->system() && p->system()->area() == this)
        {
            markOnce(p->layer());
            for (QCPLayer* dependent : p->dependentLayers())
                markOnce(dependent);
        }
    }
}
