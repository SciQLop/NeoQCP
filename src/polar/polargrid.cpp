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

#include "polargrid.h"
#include "angularaxis.h"
#include "polar-area.h"

#include "../core.h"
#include "../painting/painter.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPPolarGrid
////////////////////////////////////////////////////////////////////////////////////////////////////

/*! \class QCPPolarGrid
  \brief The grid in both angular and radial dimensions for polar plots

  \warning In this QCustomPlot version, polar plots are a tech preview. Expect documentation and
  functionality to be incomplete, as well as changing public interfaces in the future.
*/

/*! \fn void QCPPolarGrid::radialAxisChanged(QCPPolarAxisRadial* radialAxis)

  This signal is emitted when \ref radialAxis changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarGrid::typeChanged(QCPPolarGrid::GridTypes type)

  This signal is emitted when \ref type changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarGrid::subGridTypeChanged(QCPPolarGrid::GridTypes subGridType)

  This signal is emitted when \ref subGridType changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarGrid::antialiasedSubGridChanged(bool antialiasedSubGrid)

  This signal is emitted when \ref antialiasedSubGrid changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarGrid::antialiasedZeroLineChanged(bool antialiasedZeroLine)

  This signal is emitted when \ref antialiasedZeroLine changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarGrid::angularPenChanged(const QPen& angularPen)

  This signal is emitted when \ref angularPen changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarGrid::angularSubGridPenChanged(const QPen& angularSubGridPen)

  This signal is emitted when \ref angularSubGridPen changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarGrid::radialPenChanged(const QPen& radialPen)

  This signal is emitted when \ref radialPen changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarGrid::radialSubGridPenChanged(const QPen& radialSubGridPen)

  This signal is emitted when \ref radialSubGridPen changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarGrid::radialZeroLinePenChanged(const QPen& radialZeroLinePen)

  This signal is emitted when \ref radialZeroLinePen changes, after the plot delivers its deferred
  signals.
*/

/*!
  Creates a QCPPolarGrid instance and sets default values.

  You shouldn't instantiate grids on their own, since every polar area brings its own grid.
*/
QCPPolarGrid::QCPPolarGrid(QCPPolarArea* area)
        : QCPLayerable(area->parentPlot(), QString(), area)
        , mType(gtNone)
        , mSubGridType(gtNone)
        , mAntialiasedSubGrid(true)
        , mAntialiasedZeroLine(true)
        , mArea(area)
{
    // warning: this is called in QCPPolarArea constructor, so area members should not be
    // accessed/called
    setParent(area);
    setType(gtAll);
    setSubGridType(gtNone);

    setAngularPen(QPen(QColor(200, 200, 200), 0, Qt::DotLine));
    setAngularSubGridPen(QPen(QColor(220, 220, 220), 0, Qt::DotLine));

    setRadialPen(QPen(QColor(200, 200, 200), 0, Qt::DotLine));
    setRadialSubGridPen(QPen(QColor(220, 220, 220), 0, Qt::DotLine));
    setRadialZeroLinePen(QPen(QColor(200, 200, 200), 0, Qt::SolidLine));

    setAntialiased(true);
}

/*! \internal

  The area keeps the grid by raw pointer: leaving it behind would leave a dangling handle in every
  draw path, and the area's teardown would delete it twice.
*/
QCPPolarGrid::~QCPPolarGrid()
{
    if (mArea && mArea->mGrid == this)
        mArea->mGrid = nullptr;
}

void QCPPolarGrid::setRadialAxis(QCPPolarAxisRadial* axis)
{
    if (axis == mRadialAxis)
        return;
    update();
    mRadialAxis = axis;
    qcpEmit(this, mParentPlot, &QCPPolarGrid::radialAxisChanged, mRadialAxis.data());
}

void QCPPolarGrid::setType(GridTypes type)
{
    if (type == mType)
        return;
    update();
    mType = type;
    qcpEmit(this, mParentPlot, &QCPPolarGrid::typeChanged, mType);
}

void QCPPolarGrid::setSubGridType(GridTypes type)
{
    if (type == mSubGridType)
        return;
    update();
    mSubGridType = type;
    qcpEmit(this, mParentPlot, &QCPPolarGrid::subGridTypeChanged, mSubGridType);
}

/*!
  Sets whether sub grid lines are drawn antialiased.
*/
void QCPPolarGrid::setAntialiasedSubGrid(bool enabled)
{
    if (enabled == mAntialiasedSubGrid)
        return;
    update();
    mAntialiasedSubGrid = enabled;
    qcpEmit(this, mParentPlot, &QCPPolarGrid::antialiasedSubGridChanged, antialiasedSubGrid());
}

/*!
  Sets whether zero lines are drawn antialiased.
*/
void QCPPolarGrid::setAntialiasedZeroLine(bool enabled)
{
    if (enabled == mAntialiasedZeroLine)
        return;
    update();
    mAntialiasedZeroLine = enabled;
    qcpEmit(this, mParentPlot, &QCPPolarGrid::antialiasedZeroLineChanged, antialiasedZeroLine());
}

/*!
  Sets the pen with which (major) grid lines are drawn.
*/
void QCPPolarGrid::setAngularPen(const QPen& pen)
{
    if (pen == mAngularPen)
        return;
    update();
    mAngularPen = pen;
    qcpEmit(this, mParentPlot, &QCPPolarGrid::angularPenChanged, angularPen());
}

/*!
  Sets the pen with which sub grid lines are drawn.
*/
void QCPPolarGrid::setAngularSubGridPen(const QPen& pen)
{
    if (pen == mAngularSubGridPen)
        return;
    update();
    mAngularSubGridPen = pen;
    qcpEmit(this, mParentPlot, &QCPPolarGrid::angularSubGridPenChanged, angularSubGridPen());
}

void QCPPolarGrid::setRadialPen(const QPen& pen)
{
    if (pen == mRadialPen)
        return;
    update();
    mRadialPen = pen;
    qcpEmit(this, mParentPlot, &QCPPolarGrid::radialPenChanged, radialPen());
}

void QCPPolarGrid::setRadialSubGridPen(const QPen& pen)
{
    if (pen == mRadialSubGridPen)
        return;
    update();
    mRadialSubGridPen = pen;
    qcpEmit(this, mParentPlot, &QCPPolarGrid::radialSubGridPenChanged, radialSubGridPen());
}

void QCPPolarGrid::setRadialZeroLinePen(const QPen& pen)
{
    if (pen == mRadialZeroLinePen)
        return;
    update();
    mRadialZeroLinePen = pen;
    qcpEmit(this, mParentPlot, &QCPPolarGrid::radialZeroLinePenChanged, radialZeroLinePen());
}

/*! \internal

  A convenience function to easily set the QPainter::Antialiased hint on the provided \a painter
  before drawing the major grid lines.

  This is the antialiasing state the painter passed to the \ref draw method is in by default.

  This function takes into account the local setting of the antialiasing flag as well as the
  overrides set with \ref QCustomPlot::setAntialiasedElements and \ref
  QCustomPlot::setNotAntialiasedElements.

  \see setAntialiased
*/
void QCPPolarGrid::applyDefaultAntialiasingHint(QCPPainter* painter) const
{
    applyAntialiasingHint(painter, mAntialiased, QCP::aeGrid);
}

/*! \internal

  Draws grid lines and sub grid lines at the positions of (sub) ticks of the parent axis, spanning
  over the complete axis rect. Also draws the zero line, if appropriate (\ref setZeroLinePen).
*/
void QCPPolarGrid::draw(QCPPainter* painter)
{
    QCPPolarAxisAngular* angularAxis = mArea ? mArea->angularAxis() : nullptr;
    if (!angularAxis)
    {
        qDebug() << Q_FUNC_INFO << "invalid polar area";
        return;
    }

    const QPointF center = mArea->center();
    const double radius = mArea->radius();

    painter->setBrush(Qt::NoBrush);
    // draw main angular grid:
    if (mType.testFlag(gtAngular))
        drawAngularGrid(painter, center, radius, angularAxis->mTickVectorCosSin, mAngularPen);
    // draw main radial grid:
    if (mType.testFlag(gtRadial) && mRadialAxis)
        drawRadialGrid(painter, center, mRadialAxis->tickVector(), mRadialPen, mRadialZeroLinePen);

    applyAntialiasingHint(painter, mAntialiasedSubGrid, QCP::aeGrid);
    // draw sub angular grid:
    if (mSubGridType.testFlag(gtAngular))
        drawAngularGrid(painter, center, radius, angularAxis->mSubTickVectorCosSin,
                        mAngularSubGridPen);
    // draw sub radial grid:
    if (mSubGridType.testFlag(gtRadial) && mRadialAxis)
        drawRadialGrid(painter, center, mRadialAxis->subTickVector(), mRadialSubGridPen);
}

void QCPPolarGrid::drawRadialGrid(QCPPainter* painter, const QPointF& center,
                                  const QVector<double>& coords, const QPen& pen,
                                  const QPen& zeroPen)
{
    if (!mRadialAxis)
        return;
    if (coords.isEmpty())
        return;
    const bool drawZeroLine = zeroPen != Qt::NoPen;
    const double zeroLineEpsilon = qAbs(coords.last() - coords.first()) * 1e-6;

    painter->setPen(pen);
    for (int i = 0; i < coords.size(); ++i)
    {
        const double r = mRadialAxis->coordToRadius(coords.at(i));
        if (drawZeroLine && qAbs(coords.at(i)) < zeroLineEpsilon)
        {
            applyAntialiasingHint(painter, mAntialiasedZeroLine, QCP::aeZeroLine);
            painter->setPen(zeroPen);
            painter->drawEllipse(center, r, r);
            painter->setPen(pen);
            applyDefaultAntialiasingHint(painter);
        }
        else
        {
            painter->drawEllipse(center, r, r);
        }
    }
}

void QCPPolarGrid::drawAngularGrid(QCPPainter* painter, const QPointF& center, double radius,
                                   const QVector<QPointF>& ticksCosSin, const QPen& pen)
{
    if (ticksCosSin.isEmpty())
        return;

    painter->setPen(pen);
    for (int i = 0; i < ticksCosSin.size(); ++i)
        painter->drawLine(center, center + ticksCosSin.at(i) * radius);
}
