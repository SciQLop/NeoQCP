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

#include "polar-area.h"
#include "angularaxis.h"
#include "polargraph.h"
#include "polargrid.h"
#include "radialaxis.h"
#include <QPainterPath>
#include <utility>

#include "../core.h"
#include "../painting/painter.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPPolarArea
////////////////////////////////////////////////////////////////////////////////////////////////////

/*! \class QCPPolarArea
  \brief The container of a polar plot: one angular axis, the radial axes, the grid and the graphs

  The area is a layout element, so it is placed in the plot layout like an axis rect. It computes
  the centre and radius the circular axes are drawn with, and it drags and zooms them.

  \warning In this QCustomPlot version, polar plots are a tech preview. Expect documentation and
  functionality to be incomplete, as well as changing public interfaces in the future.
*/

/*!
  Creates a polar area with one angular axis, one radial axis and a grid, on the plot's current
  layer.
*/
QCPPolarArea::QCPPolarArea(QCustomPlot* parentPlot)
        : QCPArea(parentPlot), mGrid(new QCPPolarGrid(this))
{
    mAngularAxis = new QCPPolarAxisAngular(this);

    setMinimumSize(50, 50);
    // The ring's tick labels sit in these: a minimum, as the angular axis asked for, so a larger
    // plot gives the disc more room instead of the labels more margin.
    setMinimumMargins(QMargins(30, 30, 30, 30));

    addRadialAxis();
    mGrid->setRadialAxis(radialAxis());
}

QCPPolarArea::~QCPPolarArea()
{
    // Deleted mid-drag (by a rangeChanged slot): give the plot its antialiasing back. Without a
    // layer, the plot itself is being torn down.
    if (mParentPlot && layer() && mDragging && mParentPlot->noAntialiasingOnDrag())
    {
        mParentPlot->setAntialiasedElements(mAADragBackup);
        mParentPlot->setNotAntialiasedElements(mNotAADragBackup);
    }
    // Elements leave the inset layout by emitting, so it goes while the plot still delivers.
    destroyInsetLayout();

    delete mGrid; // delete grid here instead of via parent ~QObject for better defined deletion
                  // order
    mGrid = nullptr;

    // The graphs outlive their area: none may reach back into this list once it is gone.
    for (QCPPolarGraph* graph : std::exchange(mGraphs, { }))
        graph->mKeyAxis = nullptr;

    const QList<QCPPolarAxisRadial*> axes = mRadialAxes;
    for (QCPPolarAxisRadial* axis : axes)
        removeRadialAxis(axis);

    // The axes are the plot's children, not the area's: they go with the area whose geometry they
    // draw, or they would reach back into it once it is gone.
    delete mAngularAxis;
    mAngularAxis = nullptr;
}

/*! \internal

  Draws the background inside the disc the axes are drawn in, where the angular axis used to fill
  and clip it: the rect corners stay clear.
*/
void QCPPolarArea::drawBackgroundBrush(QCPPainter* painter)
{
    // The region clip is a 1-bit mask: filling the disc through it would leave a jagged rim, where
    // the ring used to have an antialiased one.
    QPainterPath disc;
    disc.addEllipse(mCenter, mRadius, mRadius);
    painter->save();
    painter->setAntialiasing(true);
    painter->fillPath(disc, mBackgroundBrush);
    painter->restore();
}

void QCPPolarArea::drawBackgroundPixmap(QCPPainter* painter)
{
    painter->save();
    painter->setClipRegion(exactClipRegion());
    QCPArea::drawBackgroundPixmap(painter);
    painter->restore();
}

/*!
  Returns the radial axis with the given \a index.
*/
QCPPolarAxisRadial* QCPPolarArea::radialAxis(int index) const
{
    if (index >= 0 && index < mRadialAxes.size())
        return mRadialAxes.at(index);
    qDebug() << Q_FUNC_INFO << "invalid index" << index;
    return nullptr;
}

/*!
  Adds the given radial axis to this area. If \a axis is a null pointer, a new radial axis is
  created.

  Returns a null pointer if \a axis does not belong to this area, or already is in it.
*/
QCPPolarAxisRadial* QCPPolarArea::addRadialAxis(QCPPolarAxisRadial* axis)
{
    if (!axis)
    {
        const auto newAxis = new QCPPolarAxisRadial(this);
        mRadialAxes.append(newAxis);
        return newAxis;
    }
    if (axis->angularAxis() != mAngularAxis)
    {
        qDebug() << Q_FUNC_INFO << "Radial axis isn't associated with this angular axis:"
                 << reinterpret_cast<quintptr>(axis);
        return nullptr;
    }
    if (mRadialAxes.contains(axis))
    {
        qDebug() << Q_FUNC_INFO
                 << "Radial axis already in list:" << reinterpret_cast<quintptr>(axis);
        return nullptr;
    }
    mRadialAxes.append(axis);
    return axis;
}

/*!
  Removes the given radial axis from this area and deletes it. Returns whether it was found.
*/
bool QCPPolarArea::removeRadialAxis(QCPPolarAxisRadial* axis)
{
    if (mRadialAxes.contains(axis))
    {
        mRadialAxes.removeOne(axis);
        delete axis;
        return true;
    }
    qDebug() << Q_FUNC_INFO << "Radial axis not in list:" << reinterpret_cast<quintptr>(axis);
    return false;
}

/*!
  Returns the region the polar graphs are clipped to: the circle the angular axis draws.
*/
QRegion QCPPolarArea::exactClipRegion() const
{
    return QRegion(mCenter.x() - mRadius, mCenter.y() - mRadius, qRound(2 * mRadius),
                   qRound(2 * mRadius), QRegion::Ellipse);
}

/*!
  Removes the given graph from this area and deletes it. The graph is unlisted before the delete,
  so a slot connected to its destruction may remove another one.
*/
bool QCPPolarArea::removeGraph(QCPPolarGraph* graph)
{
    if (!mGraphs.contains(graph))
    {
        qDebug() << Q_FUNC_INFO << "graph not in list:" << reinterpret_cast<quintptr>(graph);
        return false;
    }

    mGraphs.removeOne(graph);
    graph->removeFromLegend();
    delete graph;
    return true;
}

/*!
  Changes the angular axis range such that all graphs drawn on this area are fully visible.
*/
void QCPPolarArea::rescale(bool onlyVisiblePlottables)
{
    QCPRange newRange;
    bool haveRange = false;
    for (QCPPolarGraph* graph : mGraphs)
    {
        if (!graph->realVisibility() && onlyVisiblePlottables)
            continue;
        QCPRange range;
        bool currentFoundRange;
        if (graph->keyAxis() == mAngularAxis)
            range = graph->getKeyRange(currentFoundRange, QCP::sdBoth);
        else
            range = graph->getValueRange(currentFoundRange, QCP::sdBoth);
        if (currentFoundRange)
        {
            if (!haveRange)
                newRange = range;
            else
                newRange.expand(range);
            haveRange = true;
        }
    }
    if (haveRange && mAngularAxis)
    {
        const QCPRange current = mAngularAxis->range();
        if (!QCPRange::validRange(newRange)) // likely due to range being zero (plottable has only
                                             // constant data in this axis dimension), shift current
                                             // range to at least center the plottable
        {
            double center = (newRange.lower + newRange.upper)
                * 0.5; // upper and lower should be equal anyway, but just to make sure, incase
                       // validRange returned false for other reason
            newRange.lower = center - current.size() / 2.0;
            newRange.upper = center + current.size() / 2.0;
        }
        mAngularAxis->setRange(newRange);
    }
}

/* inherits documentation from base class */
bool QCPPolarArea::plotTakesNewParts() const
{
    return mParentPlot && !mParentPlot->isBeingDestroyed();
}

/* inherits documentation from base class */
void QCPPolarArea::update(UpdatePhase phase)
{
    QCPArea::update(phase);

    switch (phase)
    {
        case upPreparation:
        {
            if (mAngularAxis)
                mAngularAxis->setupTickVectors();
            for (QCPPolarAxisRadial* axis : mRadialAxes)
                axis->setupTickVectors();
            break;
        }
        case upLayout:
        {
            mCenter = mRect.center();
            mRadius = 0.5 * qMin(qAbs(mRect.width()), qAbs(mRect.height()));
            if (mRadius < 1) // might be if fully zoomed out
                mRadius = 1;
            for (QCPPolarAxisRadial* axis : mRadialAxes)
                axis->updateGeometry(mCenter, mRadius);
            break;
        }
        default:
            break;
    }
}

/* inherits documentation from base class */
QList<QCPAxisBase*> QCPPolarArea::allAxes() const
{
    QList<QCPAxisBase*> result;
    if (mAngularAxis)
        result << mAngularAxis;
    result.reserve(result.size() + mRadialAxes.size());
    for (QCPPolarAxisRadial* axis : mRadialAxes)
        result << axis;
    return result;
}

/*! \internal

  Dirties every layer a change of an axis' range moves: the angular and radial axes and the grid,
  then what each graph and its items draw. Mirrors QCPAxisRect::markAffectedLayersDirty.
*/
void QCPPolarArea::markAffectedLayersDirty(bool invalidate)
{
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
    if (mAngularAxis)
        markOnce(mAngularAxis->layer());
    for (QCPPolarAxisRadial* axis : mRadialAxes)
        markOnce(axis->layer());
    if (mGrid)
        markOnce(mGrid->layer());
    for (QCPPolarGraph* graph : mGraphs)
        markOnce(graph->layer());
}

void QCPPolarArea::mousePressEvent(QMouseEvent* event, [[maybe_unused]] const QVariant& details)
{
    if (event->buttons() & Qt::LeftButton)
    {
        mDragging = true;
        if (mParentPlot->noAntialiasingOnDrag())
        {
            mAADragBackup = mParentPlot->antialiasedElements();
            mNotAADragBackup = mParentPlot->notAntialiasedElements();
        }

        if (mParentPlot->interactions().testFlag(QCP::iRangeDrag))
        {
            if (mAngularAxis)
                mDragAngularStart = mAngularAxis->range();
            mDragRadialStart.clear();
            for (QCPPolarAxisRadial* axis : mRadialAxes)
                mDragRadialStart.append({ axis, axis->range() });
        }
    }
}

void QCPPolarArea::mouseMoveEvent(QMouseEvent* event, const QPointF& startPos)
{
    bool doReplot = false;
    const QPointer<QCPPolarArea> alive(this);
    if (mDragging && mParentPlot->interactions().testFlag(QCP::iRangeDrag))
    {
        // angular axis:
        if (mAngularAxis && mAngularAxis->rangeDraggable())
        {
            doReplot = true;
            double angleCoordStart, radiusCoordStart;
            double angleCoord, radiusCoord;
            mAngularAxis->pixelToCoord(startPos, angleCoordStart, radiusCoordStart);
            mAngularAxis->pixelToCoord(event->pos(), angleCoord, radiusCoord);
            double diff = angleCoordStart - angleCoord;
            mAngularAxis->setRange(mDragAngularStart.lower + diff, mDragAngularStart.upper + diff);
            if (!alive)
                return;
        }

        // radial axes:
        const auto dragStart = mDragRadialStart;
        for (const auto& [axis, start] : dragStart)
        {
            if (!alive)
                break;
            if (!axis || !axis->rangeDraggable())
                continue;
            doReplot = true;
            double angleCoordStart, radiusCoordStart;
            double angleCoord, radiusCoord;
            axis->pixelToCoord(startPos, angleCoordStart, radiusCoordStart);
            axis->pixelToCoord(event->pos(), angleCoord, radiusCoord);
            if (axis->scaleType() == QCPPolarAxisRadial::stLinear)
            {
                double diff = radiusCoordStart - radiusCoord;
                axis->setRange(start.lower + diff, start.upper + diff);
            }
            else if (axis->scaleType() == QCPPolarAxisRadial::stLogarithmic)
            {
                if (radiusCoord != 0)
                {
                    double diff = radiusCoordStart / radiusCoord;
                    axis->setRange(start.lower * diff, start.upper * diff);
                }
            }
        }
        if (alive && doReplot)
        {
            if (mParentPlot->noAntialiasingOnDrag())
                mParentPlot->setNotAntialiasedElements(QCP::aeAll);
            mParentPlot->replot(QCustomPlot::rpQueuedReplot);
        }
    }
}

void QCPPolarArea::mouseReleaseEvent([[maybe_unused]] QMouseEvent* event,
                                     [[maybe_unused]] const QPointF& startPos)
{
    mDragging = false;
    if (mParentPlot->noAntialiasingOnDrag())
    {
        mParentPlot->setAntialiasedElements(mAADragBackup);
        mParentPlot->setNotAntialiasedElements(mNotAADragBackup);
    }
}

void QCPPolarArea::wheelEvent(QWheelEvent* event)
{
    bool doReplot = false;
    const QPointer<QCPPolarArea> alive(this);
    if (mParentPlot->interactions().testFlag(QCP::iRangeZoom))
    {
        const double delta = event->angleDelta().y();
        const QPointF pos = event->position();
        const double wheelSteps = delta / 120.0; // a single step delta is +/- 120

        if (mAngularAxis && mAngularAxis->rangeZoomable())
        {
            double angleCoord, radiusCoord;
            mAngularAxis->pixelToCoord(pos, angleCoord, radiusCoord);
            mAngularAxis->scaleRange(qPow(mRangeZoomFactor, wheelSteps), angleCoord);
            if (!alive)
                return;
        }
        const QList<QPointer<QCPPolarAxisRadial>> axes(mRadialAxes.begin(), mRadialAxes.end());
        for (const QPointer<QCPPolarAxisRadial>& axis : axes)
        {
            if (!alive)
                break;
            if (!axis || !axis->rangeZoomable())
                continue;
            doReplot = true;
            double angleCoord, radiusCoord;
            axis->pixelToCoord(pos, angleCoord, radiusCoord);
            axis->scaleRange(qPow(mRangeZoomFactor, wheelSteps), radiusCoord);
        }
    }
    if (alive && doReplot)
        mParentPlot->replot(QCustomPlot::rpQueuedReplot);
}

bool QCPPolarArea::registerPolarGraph(QCPPolarGraph* graph)
{
    if (mGraphs.contains(graph))
    {
        qDebug() << Q_FUNC_INFO << "plottable already added:" << reinterpret_cast<quintptr>(graph);
        return false;
    }
    if (graph->keyAxis() != mAngularAxis)
    {
        qDebug() << Q_FUNC_INFO
                 << "plottable not created with this as axis:" << reinterpret_cast<quintptr>(graph);
        return false;
    }

    mGraphs.append(graph);
    // possibly add plottable to legend:
    if (mParentPlot->autoAddPlottableToLegend())
        graph->addToLegend();
    if (!graph->layer()) // usually the layer is already set in the constructor of the plottable
                         // (via QCPLayerable constructor)
        graph->setLayer(mParentPlot->currentLayer());
    return true;
}
