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

#include "../coordinate-system-polar.h"
#include "../core.h"
#include "../items/item.h"
#include "../misuse.h"
#include "../painting/painter.h"
#include "../plottables/plottable.h"

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
    // A grid made on this area besides its own is its child too: it goes while the area is whole.
    qDeleteAll(findChildren<QCPPolarGrid*>(Qt::FindDirectChildrenOnly));

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

QCPCoordinateSystem* QCPPolarArea::createSystem(QCPAxisBase* key, QCPAxisBase* value)
{
    if (key != mAngularAxis || !mRadialAxes.contains(static_cast<QCPPolarAxisRadial*>(value)))
    {
        qcp::reportMisuse(Q_FUNC_INFO,
                          "a polar system is the angular axis and one of its radial axes");
        return nullptr;
    }
    return new QCPPolarSystem(this, static_cast<QCPPolarAxisAngular*>(key),
                              static_cast<QCPPolarAxisRadial*>(value));
}

/*!
  Returns the radial axis with the given \a index.
*/
QCPHandle<QCPPolarAxisAngular> QCPPolarArea::angularAxis() const
{
    return QCPHandle<QCPPolarAxisAngular>(mAngularAxis);
}

QCPHandle<QCPPolarAxisRadial> QCPPolarArea::radialAxis(int index) const
{
    if (index >= 0 && index < mRadialAxes.size())
        return QCPHandle<QCPPolarAxisRadial>(mRadialAxes.at(index));
    qDebug() << Q_FUNC_INFO << "invalid index" << index;
    return { };
}

QList<QCPHandle<QCPPolarAxisRadial>> QCPPolarArea::radialAxes() const
{
    QList<QCPHandle<QCPPolarAxisRadial>> result;
    result.reserve(mRadialAxes.size());
    for (QCPPolarAxisRadial* axis : mRadialAxes)
        result.append(QCPHandle<QCPPolarAxisRadial>(axis));
    return result;
}

QCPHandle<QCPPolarGrid> QCPPolarArea::grid() const
{
    return QCPHandle<QCPPolarGrid>(mGrid);
}

/*!
  Adds the given radial axis to this area. If \a axis is a null pointer, a new radial axis is
  created.

  Returns a null pointer if \a axis does not belong to this area, or already is in it.
*/
QCPHandle<QCPPolarAxisRadial> QCPPolarArea::addRadialAxis(QCPPolarAxisRadial* axis)
{
    if (!axis)
    {
        const auto newAxis = new QCPPolarAxisRadial(this);
        mRadialAxes.append(newAxis);
        return QCPHandle<QCPPolarAxisRadial>(newAxis);
    }
    if (axis->angularAxis() != mAngularAxis)
    {
        qDebug() << Q_FUNC_INFO << "Radial axis isn't associated with this angular axis:"
                 << reinterpret_cast<quintptr>(axis);
        return { };
    }
    if (mRadialAxes.contains(axis))
    {
        qDebug() << Q_FUNC_INFO
                 << "Radial axis already in list:" << reinterpret_cast<quintptr>(axis);
        return { };
    }
    mRadialAxes.append(axis);
    return QCPHandle<QCPPolarAxisRadial>(axis);
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
  Returns the region the area's background pixmap is clipped to: the disc the angular axis draws.
*/
QRegion QCPPolarArea::exactClipRegion() const
{
    return QRegion(mCenter.x() - mRadius, mCenter.y() - mRadius, qRound(2 * mRadius),
                   qRound(2 * mRadius), QRegion::Ellipse);
}

QCPRatioTransform QCPPolarArea::ratioTransform() const
{
    const double start = mAngularAxis ? mAngularAxis->angle() / 180.0 * M_PI : 0.0;
    const bool reversed = mAngularAxis && mAngularAxis->rangeReversed();
    return QCPRatioTransform(mCenter, mRadius, start, 2.0 * M_PI, reversed);
}

QPainterPath QCPPolarArea::clipPath() const
{
    QPainterPath disc;
    disc.addEllipse(mCenter, mRadius, mRadius);
    return disc;
}

/*!
  Returns the polar graphs drawn on this area: the plottables whose coordinate system belongs to it.
  The plot owns them: \ref QCustomPlot::remove and \ref QCustomPlot::take move and delete them.
*/
QList<QCPPolarGraph*> QCPPolarArea::graphs() const
{
    QList<QCPPolarGraph*> result;
    if (!mParentPlot)
        return result;
    for (QCPAbstractPlottable* plottable : mParentPlot->plottablesRaw())
        if (auto* graph = qobject_cast<QCPPolarGraph*>(plottable))
            if (graph->system() && graph->system()->area() == this)
                result.append(graph);
    return result;
}

/*!
  Rescales the radial axis of every graph drawn on this area, so that the graphs' radii are fully
  visible. The angular axis keeps its full turn: see \ref QCPPolarAxisAngular::rescale, the one rule
  for it, whether the fit comes from here or from \ref QCustomPlot::rescaleAxes.
*/
void QCPPolarArea::rescale(bool onlyVisiblePlottables)
{
    for (QCPPolarGraph* graph : graphs())
    {
        if (!graph->realVisibility() && onlyVisiblePlottables)
            continue;
        graph->rescaleValueAxis();
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
QList<QCPHandle<QCPAxisBase>> QCPPolarArea::allAxes() const
{
    QList<QCPHandle<QCPAxisBase>> result;
    const QList<QCPAxisBase*> rawAxes = allAxesRaw();
    result.reserve(rawAxes.size());
    for (QCPAxisBase* axis : rawAxes)
        result << QCPHandle<QCPAxisBase>(axis);
    return result;
}

QList<QCPAxisBase*> QCPPolarArea::allAxesRaw() const
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
        markOnce(mAngularAxis->layerRaw());
    for (QCPPolarAxisRadial* axis : mRadialAxes)
        markOnce(axis->layerRaw());
    if (mGrid)
        markOnce(mGrid->layerRaw());
    if (!mParentPlot)
        return;
    // The plottables of this area's systems draw in the disc too, wherever their layer sits: the
    // same one-pass selection by system the axis rect uses.
    for (QCPAbstractPlottable* plottable : mParentPlot->plottablesRaw())
    {
        if (QCPCoordinateSystem* system = plottable->systemRaw();
            system && system->areaRaw() == this)
        {
            markOnce(plottable->layerRaw());
            for (QCPLayer* dependent : plottable->dependentLayersRaw())
                markOnce(dependent);
        }
    }
    // The items drawn in this disc mark their own layers too, wherever those sit: a rotation is not
    // an axis range change, and nothing else tells an item's layer that its pixels moved. Each layer
    // knows which areas its item positions measure in, so ask it instead of walking every item.
    for (int i = 0; i < mParentPlot->layerCount(); ++i)
        if (QCPLayer* layer = mParentPlot->layer(i); layer && layer->itemsUseArea(this))
            markOnce(layer);
}

void QCPPolarArea::markLayoutAppearanceDirty()
{
    if (!mParentPlot) // the constructor sets sizes before there is a plot to mark
        return QCPLayoutElement::markLayoutAppearanceDirty();
    // Not just the area's own layer: a pending translation would keep the axes, grid and plottables
    // of this disc at their old pixels.
    markAffectedLayersDirty(true);
    QCPLayoutElement::markLayoutAppearanceDirty();
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
