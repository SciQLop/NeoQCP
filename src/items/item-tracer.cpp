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

#include "item-tracer.h"

#include "../core.h"
#include "../painting/draw-context.h"
#include "../plottables/plottable-graph.h"
#include <array>

////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPItemTracer
////////////////////////////////////////////////////////////////////////////////////////////////////

/*! \class QCPItemTracer
  \brief Item that sticks to QCPGraph data points

  \image html QCPItemTracer.png "Tracer example. Blue dotted circles are anchors, solid blue discs
  are positions."

  The tracer can be connected with a QCPGraph via \ref setGraph. Then it will automatically adopt
  the coordinate axes of the graph and update its \a position to be on the graph's data. This means
  the key stays controllable via \ref setGraphKey, but the value will follow the graph data. If a
  QCPGraph is connected, note that setting the coordinates of the tracer item directly via \a
  position will have no effect because they will be overriden in the next redraw (this is when the
  coordinate update happens).

  If the specified key in \ref setGraphKey is outside the key bounds of the graph, the tracer will
  stay at the corresponding end of the graph.

  With \ref setInterpolating you may specify whether the tracer may only stay exactly on data
  points or whether it interpolates data points linearly, if given a key that lies between two data
  points of the graph.

  The tracer has different visual styles, see \ref setStyle. It is also possible to make the tracer
  have no own visual appearance (set the style to \ref tsNone), and just connect other item
  positions to the tracer \a position (used as an anchor) via \ref
  QCPItemPosition::setParentAnchor.

  \note The tracer position is only automatically updated upon redraws. So when the data of the
  graph changes and immediately afterwards (without a redraw) the position coordinates of the
  tracer are retrieved, they will not reflect the updated data of the graph. In this case \ref
  updatePosition must be called manually, prior to reading the tracer coordinates.
*/

/*!
  Creates a tracer item and sets default values.

  The created item is automatically registered with \a parentPlot. This QCustomPlot instance takes
  ownership of the item, so do not delete it manually but use QCustomPlot::removeItem() instead.
*/
QCPItemTracer::QCPItemTracer(QCustomPlot* parentPlot)
        : QCPAbstractItem(parentPlot)
        , position(createPosition(QLatin1String("position")))
        , mSize(6)
        , mStyle(tsCrosshair)
        , mGraphKey(0)
        , mInterpolating(false)
{
    position->assignCoords(0, 0);

    setBrush(Qt::NoBrush);
    setSelectedBrush(Qt::NoBrush);
    setPen(QPen(Qt::black));
    setSelectedPen(QPen(Qt::blue, 2));
}

QCPItemTracer::~QCPItemTracer() { }

/*!
  Sets the pen that will be used to draw the line of the tracer

  \see setSelectedPen, setBrush
*/
void QCPItemTracer::setPen(const QPen& pen)
{
    if (pen == mPen)
        return;
    markDirty();
    mPen = pen;
}

/*!
  Sets the pen that will be used to draw the line of the tracer when selected

  \see setPen, setSelected
*/
void QCPItemTracer::setSelectedPen(const QPen& pen)
{
    if (pen == mSelectedPen)
        return;
    markDirty();
    mSelectedPen = pen;
}

/*!
  Sets the brush that will be used to draw any fills of the tracer

  \see setSelectedBrush, setPen
*/
void QCPItemTracer::setBrush(const QBrush& brush)
{
    if (brush == mBrush)
        return;
    markDirty();
    mBrush = brush;
}

/*!
  Sets the brush that will be used to draw any fills of the tracer, when selected.

  \see setBrush, setSelected
*/
void QCPItemTracer::setSelectedBrush(const QBrush& brush)
{
    if (brush == mSelectedBrush)
        return;
    markDirty();
    mSelectedBrush = brush;
}

/*!
  Sets the size of the tracer in pixels, if the style supports setting a size (e.g. \ref tsSquare
  does, \ref tsCrosshair does not).
*/
void QCPItemTracer::setSize(double size)
{
    if (size == mSize)
        return;
    markDirty();
    mSize = size;
}

/*!
  Sets the style/visual appearance of the tracer.

  If you only want to use the tracer \a position as an anchor for other items, set \a style to
  \ref tsNone.
*/
void QCPItemTracer::setStyle(QCPItemTracer::TracerStyle style)
{
    if (mStyle == style)
        return;
    markDirty();
    mStyle = style;
}

/*!
  Sets the QCPGraph this tracer sticks to. The tracer \a position will be set to type
  QCPItemPosition::ptPlotCoords and the axes will be set to the axes of \a graph.

  To free the tracer from any graph, set \a graph to \c nullptr. The tracer \a position can then be
  placed freely like any other item position. A tracer whose graph gets deleted while still
  attached to it is neither drawn nor selectable, and \ref graph returns \c nullptr, until the
  next call of this function.

  \see setGraphKey
*/
void QCPItemTracer::setGraph(QCPGraph* graph)
{
    if (graph == mGraph && !graph)
        return;
    markDirty();
    if (graph)
    {
        if (graph->parentPlot() == mParentPlot)
        {
            position->setType(QCPItemPosition::ptPlotCoords);
            position->setAxes(graph->keyAxis(), graph->valueAxis());
            mGraph = graph;
            mFollowsGraph = true;
            updatePosition();
        }
        else
            qDebug() << Q_FUNC_INFO << "graph isn't in same QCustomPlot instance as this item";
    }
    else
    {
        mGraph = nullptr;
        mFollowsGraph = false;
    }
}

/*!
  Sets the key of the graph's data point the tracer will be positioned at. This is the only free
  coordinate of a tracer when attached to a graph.

  Depending on \ref setInterpolating, the tracer will be either positioned on the data point
  closest to \a key, or will stay exactly at \a key and interpolate the value linearly.

  \see setGraph, setInterpolating
*/
void QCPItemTracer::setGraphKey(double key)
{
    if (key == mGraphKey)
        return;
    markDirty();
    mGraphKey = key;
}

/*!
  Sets whether the value of the graph's data points shall be interpolated, when positioning the
  tracer.

  If \a enabled is set to false and a key is given with \ref setGraphKey, the tracer is placed on
  the data point of the graph which is closest to the key, but which is not necessarily exactly
  there. If \a enabled is true, the tracer will be positioned exactly at the specified key, and
  the appropriate value will be interpolated from the graph's data points linearly.

  \see setGraph, setGraphKey
*/
void QCPItemTracer::setInterpolating(bool enabled)
{
    if (enabled == mInterpolating)
        return;
    markDirty();
    mInterpolating = enabled;
}

namespace
{
//! The scatter shape a tracer style draws as one marker on plot coordinates.
QCPScatterStyle::ScatterShape markerShapeOf(QCPItemTracer::TracerStyle style)
{
    switch (style)
    {
        case QCPItemTracer::tsCircle:
            return QCPScatterStyle::ssCircle;
        case QCPItemTracer::tsSquare:
            return QCPScatterStyle::ssSquare;
        case QCPItemTracer::tsPlus:
            return QCPScatterStyle::ssPlus;
        default:
            return QCPScatterStyle::ssNone;
    }
}
} // namespace

/* inherits documentation from base class */
void QCPItemTracer::draw(QCPDrawContext& context) const
{
    // The position store deliberately does not mark the item (see QCPItemPosition): the graph's
    // data decides where the marker is, and reading it here must not dirty the tracer.
    const_cast<QCPItemTracer*>(this)->updatePosition();
    if (mStyle == tsNone || lostGraph())
        return;

    bool snapshot = false;
    const QCP::Position at = position->asPosition(&snapshot);
    // A tracer that follows a graph sits on plot coordinates: what it draws is data, so a pan only
    // shifts it and the recording is kept. The graph's revision makes it stale when the data moves.
    if (!snapshot && std::holds_alternative<QCP::Data>(at))
    {
        const QCP::Data point = std::get<QCP::Data>(at);
        const QPen pen = mainPen();
        if (mStyle == tsCrosshair)
        {
            // The crosshair spans the axis rect, which is the clip rect of an item.
            context.polyline(std::array<QCP::Position, 2> { QCP::RatioXDataY { 0.0, point.y },
                                                            QCP::RatioXDataY { 1.0, point.y } },
                             pen);
            context.polyline(std::array<QCP::Position, 2> { QCP::DataXRatioY { point.x, 0.0 },
                                                            QCP::DataXRatioY { point.x, 1.0 } },
                             pen);
            return;
        }
        // The marker is one scatter symbol of the tracer's size, as the old painter drew it.
        const auto points = std::make_shared<const std::vector<QPointF>>(1, QPointF(point.x, point.y));
        context.markers(points, QCPScatterStyle(markerShapeOf(mStyle), pen, mainBrush(), mSize));
        return;
    }

    // A position in pixels or anchored is a snapshot of this frame: the recording is remade every
    // frame (P-5).
    context.recordEveryFrame();
    const QPointF center(position->pixelPosition());
    const double w = mSize / 2.0;
    const QRect clip = clipRect();
    const QPen pen = mainPen();
    const auto pixels = [](const QPointF& at)
    { return QCP::Pixels { at.x(), at.y() }; };
    switch (mStyle)
    {
        case tsNone:
            return;
        case tsPlus:
        {
            if (clip.intersects(QRectF(center - QPointF(w, w), center + QPointF(w, w)).toRect()))
            {
                context.polyline(std::array<QCP::Position, 2> { pixels(center + QPointF(-w, 0)),
                                                               pixels(center + QPointF(w, 0)) },
                                 pen);
                context.polyline(std::array<QCP::Position, 2> { pixels(center + QPointF(0, -w)),
                                                               pixels(center + QPointF(0, w)) },
                                 pen);
            }
            break;
        }
        case tsCrosshair:
        {
            if (center.y() > clip.top() && center.y() < clip.bottom())
                context.polyline(
                    std::array<QCP::Position, 2> {
                        QCP::Pixels { double(clip.left()), center.y() },
                        QCP::Pixels { double(clip.right()), center.y() } },
                    pen);
            if (center.x() > clip.left() && center.x() < clip.right())
                context.polyline(
                    std::array<QCP::Position, 2> {
                        QCP::Pixels { center.x(), double(clip.top()) },
                        QCP::Pixels { center.x(), double(clip.bottom()) } },
                    pen);
            break;
        }
        case tsCircle:
        {
            if (clip.intersects(QRectF(center - QPointF(w, w), center + QPointF(w, w)).toRect()))
                context.ellipse(pixels(center - QPointF(w, w)), pixels(center + QPointF(w, w)), pen,
                                mainBrush());
            break;
        }
        case tsSquare:
        {
            if (clip.intersects(QRectF(center - QPointF(w, w), center + QPointF(w, w)).toRect()))
                context.rect(pixels(center - QPointF(w, w)), pixels(center + QPointF(w, w)), pen,
                             mainBrush());
            break;
        }
    }
}

/*! \internal

  The revision of the graph whose data the recorded position came from: a new sample moves the
  tracer, so the recording is stale even though nothing on the tracer itself changed. An item
  anchored to this tracer follows it through the base's answer too, so both are hashed in order.
*/
quint64 QCPItemTracer::dependencyRevision() const
{
    return qHashMulti(qcpDependencyKey(mGraph.data()), QCPAbstractItem::dependencyRevision());
}

/*!
  If the tracer is connected with a graph (\ref setGraph), this function updates the tracer's \a
  position to reside on the graph data, depending on the configured key (\ref setGraphKey).

  It is called automatically on every redraw and normally doesn't need to be called manually. One
  exception is when you want to read the tracer coordinates via \a position and are not sure that
  the graph's data (or the tracer key with \ref setGraphKey) hasn't changed since the last redraw.
  In that situation, call this function before accessing \a position, to make sure you don't get
  out-of-date coordinates.

  If there is no graph set on this tracer, this function does nothing.
*/
void QCPItemTracer::updatePosition()
{
    if (mGraph)
    {
        if (mParentPlot->hasPlottable(mGraph))
        {
            if (mGraph->data()->size() > 1)
            {
                QCPGraphDataContainer::const_iterator first = mGraph->data()->constBegin();
                QCPGraphDataContainer::const_iterator last = mGraph->data()->constEnd() - 1;
                if (mGraphKey <= first->key)
                    position->assignCoords(first->key, first->value);
                else if (mGraphKey >= last->key)
                    position->assignCoords(last->key, last->value);
                else
                {
                    QCPGraphDataContainer::const_iterator it = mGraph->data()->findBegin(mGraphKey);
                    if (it
                        != mGraph->data()->constEnd()) // mGraphKey is not exactly on last iterator,
                                                       // but somewhere between iterators
                    {
                        QCPGraphDataContainer::const_iterator prevIt = it;
                        ++it; // won't advance to constEnd because we handled that case (mGraphKey
                              // >= last->key) before
                        if (mInterpolating)
                        {
                            // interpolate between iterators around mGraphKey:
                            double slope = 0;
                            if (!qFuzzyCompare(double(it->key), double(prevIt->key)))
                                slope = (it->value - prevIt->value) / (it->key - prevIt->key);
                            position->assignCoords(mGraphKey,
                                                (mGraphKey - prevIt->key) * slope + prevIt->value);
                        }
                        else
                        {
                            // find iterator with key closest to mGraphKey:
                            if (mGraphKey < (prevIt->key + it->key) * 0.5)
                                position->assignCoords(prevIt->key, prevIt->value);
                            else
                                position->assignCoords(it->key, it->value);
                        }
                    }
                    else // mGraphKey is exactly on last iterator (should actually be caught when
                         // comparing first/last keys, but this is a failsafe for fp uncertainty)
                        position->assignCoords(it->key, it->value);
                }
            }
            else if (mGraph->data()->size() == 1)
            {
                QCPGraphDataContainer::const_iterator it = mGraph->data()->constBegin();
                position->assignCoords(it->key, it->value);
            }
            else
                qDebug() << Q_FUNC_INFO << "graph has no data";
        }
        else
            qDebug() << Q_FUNC_INFO << "graph not contained in QCustomPlot instance (anymore)";
    }
}

/*! \internal

  Returns the pen that should be used for drawing lines. Returns mPen when the item is not selected
  and mSelectedPen when it is.
*/
QPen QCPItemTracer::mainPen() const
{
    return mSelected ? mSelectedPen : mPen;
}

/*! \internal

  Returns the brush that should be used for drawing fills of the item. Returns mBrush when the item
  is not selected and mSelectedBrush when it is.
*/
QBrush QCPItemTracer::mainBrush() const
{
    return mSelected ? mSelectedBrush : mBrush;
}
