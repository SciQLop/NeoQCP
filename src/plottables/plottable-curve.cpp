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

#include "plottable-curve.h"

#include "../axis/axis.h"
#include "../core.h"
#include "../layoutelements/layoutelement-axisrect.h"
#include "../misuse.h"
#include "../painting/draw-context.h"
#include "../painting/painter.h"
#include "plottable-color-runs.h"

#include <cmath>
#include <limits>
#include <memory>
#include <span>
#include <typeinfo>
#include <vector>

namespace
{
//! The colour of the data point \a dataIndex, transparent where it has none (a gap).
QRgb pointColour(const qcp::ColorScalarMapper& mapper, int dataIndex)
{
    const int bucket = mapper.bucket(dataIndex);
    return bucket == qcp::ColorScalarMapper::kGap ? qRgba(0, 0, 0, 0)
                                                  : qcp::runColor(mapper, bucket).rgba();
}

//! One colour per vertex of the curve, matching the shared line's convention: a segment takes the
//! colour of the point it ends on.
std::shared_ptr<const std::vector<QRgb>> vertexColours(const qcp::ColorScalarMapper& mapper,
                                                       const QVector<int>& vertexIndices)
{
    const int count = static_cast<int>(vertexIndices.size());
    auto colours
        = std::make_shared<std::vector<QRgb>>(static_cast<std::size_t>(count), qRgba(0, 0, 0, 0));
    for (int k = 0; k < count; ++k)
        (*colours)[static_cast<std::size_t>(k)]
            = pointColour(mapper, vertexIndices[std::min(k + 1, count - 1)]);
    return colours;
}

std::shared_ptr<const std::vector<QPointF>> sharedPoints(std::vector<QPointF> points)
{
    return std::make_shared<const std::vector<QPointF>>(std::move(points));
}
} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPCurveData
////////////////////////////////////////////////////////////////////////////////////////////////////

/*! \class QCPCurveData
  \brief Holds the data of one single data point for QCPCurve.

  The stored data is:
  \li \a t: the free ordering parameter of this curve point, like in the mathematical vector
  <em>(x(t), y(t))</em>. (This is the \a sortKey)
  \li \a key: coordinate on the key axis of this curve point (this is the \a mainKey)
  \li \a value: coordinate on the value axis of this curve point (this is the \a mainValue)

  The container for storing multiple data points is \ref QCPCurveDataContainer. It is a typedef for
  \ref QCPDataContainer with \ref QCPCurveData as the DataType template parameter. See the
  documentation there for an explanation regarding the data type's generic methods.

  \see QCPCurveDataContainer
*/

/* start documentation of inline functions */

/*! \fn double QCPCurveData::sortKey() const

  Returns the \a t member of this data point.

  For a general explanation of what this method is good for in the context of the data container,
  see the documentation of \ref QCPDataContainer.
*/

/*! \fn static QCPCurveData QCPCurveData::fromSortKey(double sortKey)

  Returns a data point with the specified \a sortKey (assigned to the data point's \a t member).
  All other members are set to zero.

  For a general explanation of what this method is good for in the context of the data container,
  see the documentation of \ref QCPDataContainer.
*/

/*! \fn static static bool QCPCurveData::sortKeyIsMainKey()

  Since the member \a key is the data point key coordinate and the member \a t is the data ordering
  parameter, this method returns false.

  For a general explanation of what this method is good for in the context of the data container,
  see the documentation of \ref QCPDataContainer.
*/

/*! \fn double QCPCurveData::mainKey() const

  Returns the \a key member of this data point.

  For a general explanation of what this method is good for in the context of the data container,
  see the documentation of \ref QCPDataContainer.
*/

/*! \fn double QCPCurveData::mainValue() const

  Returns the \a value member of this data point.

  For a general explanation of what this method is good for in the context of the data container,
  see the documentation of \ref QCPDataContainer.
*/

/*! \fn QCPRange QCPCurveData::valueRange() const

  Returns a QCPRange with both lower and upper boundary set to \a value of this data point.

  For a general explanation of what this method is good for in the context of the data container,
  see the documentation of \ref QCPDataContainer.
*/

/* end documentation of inline functions */

/*!
  Constructs a curve data point with t, key and value set to zero.
*/
QCPCurveData::QCPCurveData() : t(0), key(0), value(0) { }

/*!
  Constructs a curve data point with the specified \a t, \a key and \a value.
*/
QCPCurveData::QCPCurveData(double t, double key, double value) : t(t), key(key), value(value) { }

////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPCurve
////////////////////////////////////////////////////////////////////////////////////////////////////

/*! \class QCPCurve
  \brief A plottable representing a parametric curve in a plot.

  \image html QCPCurve.png

  Unlike QCPGraph, plottables of this type may have multiple points with the same key coordinate,
  so their visual representation can have \a loops. This is realized by introducing a third
  coordinate \a t, which defines the order of the points described by the other two coordinates \a
  x and \a y.

  To plot data, assign it with the \ref setData or \ref addData functions. Alternatively, you can
  also access and modify the curve's data via the \ref data method, which returns a pointer to the
  internal \ref QCPCurveDataContainer.

  Gaps in the curve can be created by adding data points with NaN as key and value
  (<tt>qQNaN()</tt> or <tt>std::numeric_limits<double>::quiet_NaN()</tt>) in between the two data
  points that shall be separated.

  \section qcpcurve-appearance Changing the appearance

  The appearance of the curve is determined by the pen and the brush (\ref setPen, \ref setBrush).

  \section qcpcurve-usage Usage

  Like all data representing objects in QCustomPlot, the QCPCurve is a plottable
  (QCPAbstractPlottable). So the plottable-interface of QCustomPlot applies
  (QCustomPlot::plottable, QCustomPlot::removePlottable, etc.)

  Usually, you first create an instance:
  \snippet documentation/doc-code-snippets/mainwindow.cpp qcpcurve-creation-1
  which registers it with the QCustomPlot instance of the passed axes. Note that this QCustomPlot
  instance takes ownership of the plottable, so do not delete it manually but use
  QCustomPlot::removePlottable() instead. The newly created plottable can be modified, e.g.:
  \snippet documentation/doc-code-snippets/mainwindow.cpp qcpcurve-creation-2
*/

/* start of documentation of inline functions */

/*! \fn QSharedPointer<QCPCurveDataContainer> QCPCurve::data() const

  Returns a shared pointer to the internal data storage of type \ref QCPCurveDataContainer. You may
  use it to directly manipulate the data, which may be more convenient and faster than using the
  regular \ref setData or \ref addData methods.
*/

/* end of documentation of inline functions */

/*!
  Constructs a curve which uses \a keyAxis as its key axis ("x") and \a valueAxis as its value
  axis ("y"). \a keyAxis and \a valueAxis must reside in the same QCustomPlot instance and not have
  the same orientation. If either of these restrictions is violated, a corresponding message is
  printed to the debug output (qDebug), the construction is not aborted, though.

  The created QCPCurve is automatically registered with the QCustomPlot instance inferred from \a
  keyAxis. This QCustomPlot instance takes ownership of the QCPCurve, so do not delete it manually
  but use QCustomPlot::removePlottable() instead.
*/
QCPCurve::QCPCurve(QCPAxis* keyAxis, QCPAxis* valueAxis)
        // A curve needs a straight segment and nothing else: no rectangle, no straight key axis.
        : QCPAbstractPlottable1D<QCPCurveData, QCPAbstractLine>(keyAxis, valueAxis,
                                                                QCP::capStraightSegments)
{
    // One component: the whole curve is one line, with one pen and one marker style.
    mComponents.resize(1);
    mComponents[0].name = QStringLiteral("curve");

    // modify inherited properties from abstract plottable:
    QCPAbstractLine::setPen(QPen(Qt::blue, 0));
    QCPAbstractLine::setBrush(Qt::NoBrush);

    QCPAbstractLine::setScatterStyle(QCPScatterStyle());
    QCPAbstractLine::setLineStyle(lsLine);
    QCPAbstractLine::setScatterSkip(0);
}

QCPCurve::~QCPCurve() { }

/*! \overload

  Replaces the current data container with the provided \a data container.

  Since a QSharedPointer is used, multiple QCPCurves may share the same data container safely.
  Modifying the data in the container will then affect all curves that share the container. Sharing
  can be achieved by simply exchanging the data containers wrapped in shared pointers:
  \snippet documentation/doc-code-snippets/mainwindow.cpp qcpcurve-datasharing-1

  If you do not wish to share containers, but create a copy from an existing container, rather use
  the \ref QCPDataContainer<DataType>::set method on the curve's data container directly:
  \snippet documentation/doc-code-snippets/mainwindow.cpp qcpcurve-datasharing-2

  \see addData
*/
void QCPCurve::setData(QSharedPointer<QCPCurveDataContainer> data)
{
    if (mDataContainer == data)
        return;
    markDirty();
    mDataContainer = data;
}

/*! \overload

  Replaces the current data with the provided points in \a t, \a keys and \a values. The provided
  vectors should have equal length. Else, the number of added points will be the size of the
  smallest vector.

  If you can guarantee that the passed data points are sorted by \a t in ascending order, you can
  set \a alreadySorted to true, to improve performance by saving a sorting run.

  \see addData
*/
void QCPCurve::setData(const QVector<double>& t, const QVector<double>& keys,
                       const QVector<double>& values, bool alreadySorted)
{
    mDataContainer->clear();
    addData(t, keys, values, alreadySorted);
}

/*! \overload

  Replaces the current data with the provided points in \a keys and \a values. The provided vectors
  should have equal length. Else, the number of added points will be the size of the smallest
  vector.

  The t parameter of each data point will be set to the integer index of the respective key/value
  pair.

  \see addData
*/
void QCPCurve::setData(const QVector<double>& keys, const QVector<double>& values)
{
    mDataContainer->clear();
    addData(keys, values);
}

/*! \overload

  The curve has one component, so the pen it remembers is the pen it draws with: handing it the
  same pen again must not repaint.
*/
void QCPCurve::setPen(const QPen& pen)
{
    if (mPen == pen && component(0).pen == pen
        && component(0).selectedPen == QCPAbstractLine::defaultSelectedPen(pen))
        return;
    QCPAbstractLine::setPen(pen);
}

/*! \overload

  Adds the provided points in \a t, \a keys and \a values to the current data. The provided vectors
  should have equal length. Else, the number of added points will be the size of the smallest
  vector.

  If you can guarantee that the passed data points are sorted by \a keys in ascending order, you
  can set \a alreadySorted to true, to improve performance by saving a sorting run.

  Alternatively, you can also access and modify the data directly via the \ref data method, which
  returns a pointer to the internal data container.
*/
void QCPCurve::addData(const QVector<double>& t, const QVector<double>& keys,
                       const QVector<double>& values, bool alreadySorted)
{
    if (t.size() != keys.size() || t.size() != values.size())
        qDebug() << Q_FUNC_INFO << "ts, keys and values have different sizes:" << t.size()
                 << keys.size() << values.size();
    const int n = qMin(qMin(t.size(), keys.size()), values.size());
    QVector<QCPCurveData> tempData(n);
    QVector<QCPCurveData>::iterator it = tempData.begin();
    const QVector<QCPCurveData>::iterator itEnd = tempData.end();
    int i = 0;
    while (it != itEnd)
    {
        it->t = t[i];
        it->key = keys[i];
        it->value = values[i];
        ++it;
        ++i;
    }
    mDataContainer->add(
        tempData, alreadySorted); // don't modify tempData beyond this to prevent copy on write
}

/*! \overload

  Adds the provided points in \a keys and \a values to the current data. The provided vectors
  should have equal length. Else, the number of added points will be the size of the smallest
  vector.

  The t parameter of each data point will be set to the integer index of the respective key/value
  pair.

  Alternatively, you can also access and modify the data directly via the \ref data method, which
  returns a pointer to the internal data container.
*/
void QCPCurve::addData(const QVector<double>& keys, const QVector<double>& values)
{
    if (keys.size() != values.size())
        qDebug() << Q_FUNC_INFO << "keys and values have different sizes:" << keys.size()
                 << values.size();
    const int n = qMin(keys.size(), values.size());
    double tStart;
    if (!mDataContainer->isEmpty())
        tStart = (mDataContainer->constEnd() - 1)->t + 1.0;
    else
        tStart = 0;
    QVector<QCPCurveData> tempData(n);
    QVector<QCPCurveData>::iterator it = tempData.begin();
    const QVector<QCPCurveData>::iterator itEnd = tempData.end();
    int i = 0;
    while (it != itEnd)
    {
        it->t = tStart + i;
        it->key = keys[i];
        it->value = values[i];
        ++it;
        ++i;
    }
    mDataContainer->add(tempData,
                        true); // don't modify tempData beyond this to prevent copy on write
}

/*! \overload
  Adds the provided data point as \a t, \a key and \a value to the current data.

  Alternatively, you can also access and modify the data directly via the \ref data method, which
  returns a pointer to the internal data container.
*/
void QCPCurve::addData(double t, double key, double value)
{
    mDataContainer->add(QCPCurveData(t, key, value));
}

/*! \overload

  Adds the provided data point as \a key and \a value to the current data.

  The t parameter is generated automatically by increments of 1 for each point, starting at the
  highest t of previously existing data or 0, if the curve data is empty.

  Alternatively, you can also access and modify the data directly via the \ref data method, which
  returns a pointer to the internal data container.
*/
void QCPCurve::addData(double key, double value)
{
    if (!mDataContainer->isEmpty())
        mDataContainer->add(QCPCurveData((mDataContainer->constEnd() - 1)->t + 1.0, key, value));
    else
        mDataContainer->add(QCPCurveData(0.0, key, value));
}

/*!
  Implements a selectTest specific to this plottable's point geometry.

  If \a details is not 0, it will be set to a \ref QCPDataSelection, describing the closest data
  point to \a pos.

  \seebaseclassmethod \ref QCPAbstractPlottable::selectTest
*/
double QCPCurve::selectTest(const QPointF& pos, bool onlySelectable, QVariant* details) const
{
    if ((onlySelectable && mSelectable == QCP::stNone) || mDataContainer->isEmpty())
        return -1;
    if (!mSystem)
        return -1;

    if (mSystem->contains(pos)
        || mParentPlot->interactions().testFlag(QCP::iSelectPlottablesBeyondAxisRect))
    {
        QCPCurveDataContainer::const_iterator closestDataPoint = mDataContainer->constEnd();
        double result = pointDistance(pos, closestDataPoint);
        if (details)
        {
            int pointIndex = int(closestDataPoint - mDataContainer->constBegin());
            details->setValue(QCPDataSelection(QCPDataRange(pointIndex, pointIndex + 1)));
        }
        return result;
    }
    else
        return -1;
}

/* inherits documentation from base class */
QCPRange QCPCurve::getKeyRange(bool& foundRange, QCP::SignDomain inSignDomain) const
{
    return mDataContainer->keyRange(foundRange, inSignDomain);
}

/* inherits documentation from base class */
QCPRange QCPCurve::getValueRange(bool& foundRange, QCP::SignDomain inSignDomain,
                                 const QCPRange& inKeyRange) const
{
    return mDataContainer->valueRange(foundRange, inSignDomain, inKeyRange);
}

std::optional<int> QCPCurve::colorValueCount() const
{
    return mDataContainer->isEmpty() ? std::nullopt
                                     : std::optional<int>(mDataContainer->size());
}

bool QCPCurve::acceptsLineStyle(LineStyle style) const
{
    QCP_CHECK_OR_RETURN(style == lsNone || style == lsLine,
                        "a curve draws a plain line or none: a step or impulse style needs "
                        "vertices the curve does not have",
                        false);
    return true;
}

/* inherits documentation from base class */
void QCPCurve::draw(QCPDrawContext& context) const
{
    if (!mSystem || mDataContainer->isEmpty())
        return;
    const QCPGraphComponent& comp = component(0);
    if (!comp.visible)
        return;

    // The fill is one polygon of the whole curve, in data space: a self-crossing curve needs the
    // rasteriser's rule, not a triangulation.
    if (mBrush.style() != Qt::NoBrush && mBrush.color().alpha() != 0)
    {
        std::vector<QCP::Position> polygon;
        polygon.reserve(static_cast<std::size_t>(mDataContainer->size()));
        for (auto it = mDataContainer->constBegin(); it != mDataContainer->constEnd(); ++it)
            polygon.push_back(QCP::Data { it->key, it->value });
        context.polygon(polygon, QPen(Qt::NoPen), mBrush);
    }

    // Values set for other data, of another size, would colour the wrong points.
    const bool coloured = mColor.hasValues() && mColor.size() == mDataContainer->size();
    QList<QCPDataRange> selectedSegments, unselectedSegments;
    getDataSegments(selectedSegments, unselectedSegments);
    recordSegments(context, unselectedSegments, false, coloured);
    recordSegments(context, selectedSegments, true, coloured);

    // Only a decorator subclass draws a decoration of its own: the base class's is empty, and
    // recording a painter callback for it would make every selection change a view change.
    if (mSelectionDecorator && !selection().isEmpty()
        && typeid(*mSelectionDecorator) != typeid(QCPSelectionDecorator))
        context.painter(
            [this](QCPPainter& painter, const QCPTransform&)
            {
                if (mSelectionDecorator)
                    mSelectionDecorator->drawDecoration(&painter, selection());
            });
}

/*! \internal

  Records \a segments in order, each with the pen and marker style its selection state draws with:
  the selection decorator's for a selected range, the component's otherwise.
*/
void QCPCurve::recordSegments(QCPDrawContext& context, const QList<QCPDataRange>& segments,
                              bool selected, bool coloured) const
{
    const QCPGraphComponent& comp = component(0);
    const QPen pen = selected && mSelectionDecorator ? mSelectionDecorator->pen() : comp.pen;
    for (const QCPDataRange& segment : segments)
    {
        // An unselected range reaches one point into the selected one, so the two lines meet.
        const QCPDataRange lineRange(selected ? segment.begin() : qMax(0, segment.begin() - 1),
                                     selected ? segment.end()
                                              : qMin(dataCount(), segment.end() + 1));
        recordLine(context, lineRange, pen, coloured);

        const QCPScatterStyle style = markerStyle(
            selected && mSelectionDecorator
                ? mSelectionDecorator->getFinalScatterStyle(comp.scatterStyle)
                : comp.scatterStyle,
            comp.pen);
        if (style.isNone())
            continue;
        recordMarkers(context, segment, style, coloured);
    }
}

/*! \internal

  Records the line of \a range: one data-space polyline, with one colour per vertex when the curve
  is coloured. NaNs stay in the polyline, where both replays split the line.
*/
void QCPCurve::recordLine(QCPDrawContext& context, const QCPDataRange& range, const QPen& pen,
                          bool coloured) const
{
    if (component(0).lineStyle == lsNone || range.size() < 2)
        return;

    std::vector<QPointF> points;
    points.reserve(static_cast<std::size_t>(range.size()));
    QVector<int> indices;
    if (coloured)
        indices.reserve(range.size());
    for (int index = range.begin(); index < range.end(); ++index)
    {
        const QCPCurveData& point = *(mDataContainer->constBegin() + index);
        points.emplace_back(point.key, point.value);
        if (coloured)
            indices.append(index);
    }
    context.polyline(sharedPoints(std::move(points)),
                     coloured ? vertexColours(mColor, indices) : nullptr, pen);
}

/*! \internal

  Records the markers of \a range: every mScatterSkip+1-th data point, skipping non-finite ones,
  as the old getScatters() did.
*/
void QCPCurve::recordMarkers(QCPDrawContext& context, const QCPDataRange& range,
                             const QCPScatterStyle& style, bool coloured) const
{
    const int skip = mScatterSkip + 1;
    int first = range.begin();
    const int remainder = first % skip;
    if (remainder != 0)
        first += skip - remainder;

    auto points = std::make_shared<std::vector<QPointF>>();
    auto colours = std::make_shared<std::vector<QRgb>>();
    for (int index = first; index < range.end(); index += skip)
    {
        const QCPCurveData& point = *(mDataContainer->constBegin() + index);
        if (!qIsFinite(point.key) || !qIsFinite(point.value))
            continue;
        points->emplace_back(point.key, point.value);
        if (coloured)
            colours->push_back(pointColour(mColor, index));
    }
    if (!points->empty())
        context.markers(points, coloured ? colours : nullptr, style);
}

/* inherits documentation from base class */
void QCPCurve::drawLegendIcon(QCPDrawContext& icon) const
{
    const QCPGraphComponent& comp = component(0);
    // The icon has no data of its own: an invisible box gives the replay the extent it fits the
    // drawing to, as the old icon filled the legend rect.
    icon.rect(QCP::Data { 0.0, -0.5 }, QCP::Data { 1.0, 0.5 }, QPen(Qt::NoPen),
              QBrush(Qt::NoBrush));

    if (mBrush.style() != Qt::NoBrush && mBrush.color().alpha() != 0)
        icon.rect(QCP::Data { 0.0, -0.15 }, QCP::Data { 1.0, 0.15 }, QPen(Qt::NoPen), mBrush);

    if (component(0).lineStyle != lsNone)
    {
        std::vector<QPointF> points;
        std::shared_ptr<const std::vector<QRgb>> colours;
        if (mColor.hasValues())
        {
            auto lineColours = std::make_shared<std::vector<QRgb>>();
            for (int s = 0; s <= 4; ++s)
            {
                points.emplace_back(s / 4.0, 0.0);
                lineColours->push_back(qcp::runColor(mColor, s * 255 / 4).rgba());
            }
            colours = lineColours;
        }
        else
        {
            points.emplace_back(0.0, 0.0);
            points.emplace_back(1.0, 0.0);
        }
        icon.polyline(sharedPoints(std::move(points)), colours, comp.pen);
    }

    if (!comp.scatterStyle.isNone())
    {
        std::vector<QPointF> marker { { 0.5, 0.0 } };
        icon.markers(sharedPoints(std::move(marker)), markerStyle(comp.scatterStyle, comp.pen));
    }
}

/*!
  Returns the distance in pixels from \a pixelPoint to the curve: the nearest data point, or the
  nearest segment between two finite points. Sets \a closestData to the nearest data point, since
  the distance to the curve line is also taken into account.

  If either the curve has no data or if the line style is \ref lsNone and the scatter style's shape
  is \ref QCPScatterStyle::ssNone (i.e. there is no visual representation of the curve), returns
  -1.0.
*/
double QCPCurve::pointDistance(const QPointF& pixelPoint,
                               QCPCurveDataContainer::const_iterator& closestData) const
{
    closestData = mDataContainer->constEnd();
    if (mDataContainer->isEmpty())
        return -1.0;
    if (component(0).lineStyle == lsNone && component(0).scatterStyle.isNone())
        return -1.0;

    double minDistSqr = (std::numeric_limits<double>::max)();
    const int count = mDataContainer->size();
    QVector<QPointF> pixels(count);
    if (mSystem && !keyAxisRaw())
    {
        // One batch map, not one transform per point (C-6): a user system may build its mapping on
        // every ask, and a hit test is O(n) anyway.
        QVector<QPointF> data(count);
        int i = 0;
        for (QCPCurveDataContainer::const_iterator it = mDataContainer->constBegin();
             it != mDataContainer->constEnd(); ++it, ++i)
            data[i] = QPointF(it->key, it->value);
        mSystem->transform().map(std::span<const QPointF>(data), std::span<QPointF>(pixels));
    }
    else
    {
        int i = 0;
        for (QCPCurveDataContainer::const_iterator it = mDataContainer->constBegin();
             it != mDataContainer->constEnd(); ++it, ++i)
            pixels[i] = coordsToPixels(it->key, it->value);
    }
    for (int i = 0; i < pixels.size(); ++i)
    {
        const QPointF& pixel = pixels.at(i);
        if (!qIsFinite(pixel.x()) || !qIsFinite(pixel.y()))
            continue;
        const double currentDistSqr = QCPVector2D(pixel - pixelPoint).lengthSquared();
        if (currentDistSqr < minDistSqr)
        {
            minDistSqr = currentDistSqr;
            closestData = mDataContainer->constBegin() + i;
        }
    }

    // The distance to a segment is usually smaller than the distance to its endpoints, which is
    // what makes a click midway along a line hit the curve.
    if (component(0).lineStyle != lsNone)
    {
        for (int i = 0; i + 1 < pixels.size(); ++i)
        {
            const QPointF& a = pixels.at(i);
            const QPointF& b = pixels.at(i + 1);
            if (!qIsFinite(a.x()) || !qIsFinite(a.y()) || !qIsFinite(b.x()) || !qIsFinite(b.y()))
                continue;
            const double currentDistSqr = QCPVector2D(pixelPoint).distanceSquaredToLine(a, b);
            if (currentDistSqr < minDistSqr)
                minDistSqr = currentDistSqr;
        }
    }

    if (minDistSqr == (std::numeric_limits<double>::max)())
        return -1.0;
    return qSqrt(minDistSqr);
}
