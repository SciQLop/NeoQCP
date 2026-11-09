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

#include "plottable-statisticalbox.h"

#include "../axis/axis.h"
#include "../core.h"
#include "../layoutelements/layoutelement-axisrect.h"
#include "../painting/draw-context.h"
#include "../painting/painter.h"
#include <limits>
#include <span>
#include <typeinfo>
#include <vector>

namespace
{
//! A separator between two segments of one polyline.
constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPStatisticalBoxData
////////////////////////////////////////////////////////////////////////////////////////////////////

/*! \class QCPStatisticalBoxData
  \brief Holds the data of one single data point for QCPStatisticalBox.

  The stored data is:

  \li \a key: coordinate on the key axis of this data point (this is the \a mainKey and the \a
  sortKey)

  \li \a minimum: the position of the lower whisker, typically the minimum measurement of the
  sample that's not considered an outlier.

  \li \a lowerQuartile: the lower end of the box. The lower and the upper quartiles are the two
  statistical quartiles around the median of the sample, they should contain 50% of the sample
  data.

  \li \a median: the value of the median mark inside the quartile box. The median separates the
  sample data in half (50% of the sample data is below/above the median). (This is the \a mainValue)

  \li \a upperQuartile: the upper end of the box. The lower and the upper quartiles are the two
  statistical quartiles around the median of the sample, they should contain 50% of the sample
  data.

  \li \a maximum: the position of the upper whisker, typically the maximum measurement of the
  sample that's not considered an outlier.

  \li \a outliers: a QVector of outlier values that will be drawn as scatter points at the \a key
  coordinate of this data point (see \ref QCPStatisticalBox::setOutlierStyle)

  The container for storing multiple data points is \ref QCPStatisticalBoxDataContainer. It is a
  typedef for \ref QCPDataContainer with \ref QCPStatisticalBoxData as the DataType template
  parameter. See the documentation there for an explanation regarding the data type's generic
  methods.

  \see QCPStatisticalBoxDataContainer
*/

/* start documentation of inline functions */

/*! \fn double QCPStatisticalBoxData::sortKey() const

  Returns the \a key member of this data point.

  For a general explanation of what this method is good for in the context of the data container,
  see the documentation of \ref QCPDataContainer.
*/

/*! \fn static QCPStatisticalBoxData QCPStatisticalBoxData::fromSortKey(double sortKey)

  Returns a data point with the specified \a sortKey. All other members are set to zero.

  For a general explanation of what this method is good for in the context of the data container,
  see the documentation of \ref QCPDataContainer.
*/

/*! \fn static static bool QCPStatisticalBoxData::sortKeyIsMainKey()

  Since the member \a key is both the data point key coordinate and the data ordering parameter,
  this method returns true.

  For a general explanation of what this method is good for in the context of the data container,
  see the documentation of \ref QCPDataContainer.
*/

/*! \fn double QCPStatisticalBoxData::mainKey() const

  Returns the \a key member of this data point.

  For a general explanation of what this method is good for in the context of the data container,
  see the documentation of \ref QCPDataContainer.
*/

/*! \fn double QCPStatisticalBoxData::mainValue() const

  Returns the \a median member of this data point.

  For a general explanation of what this method is good for in the context of the data container,
  see the documentation of \ref QCPDataContainer.
*/

/*! \fn QCPRange QCPStatisticalBoxData::valueRange() const

  Returns a QCPRange spanning from the \a minimum to the \a maximum member of this statistical box
  data point, possibly further expanded by outliers.

  For a general explanation of what this method is good for in the context of the data container,
  see the documentation of \ref QCPDataContainer.
*/

/* end documentation of inline functions */

/*!
  Constructs a data point with key and all values set to zero.
*/
QCPStatisticalBoxData::QCPStatisticalBoxData()
        : key(0), minimum(0), lowerQuartile(0), median(0), upperQuartile(0), maximum(0)
{
}

/*!
  Constructs a data point with the specified \a key, \a minimum, \a lowerQuartile, \a median, \a
  upperQuartile, \a maximum and optionally a number of \a outliers.
*/
QCPStatisticalBoxData::QCPStatisticalBoxData(double key, double minimum, double lowerQuartile,
                                             double median, double upperQuartile, double maximum,
                                             const QVector<double>& outliers)
        : key(key)
        , minimum(minimum)
        , lowerQuartile(lowerQuartile)
        , median(median)
        , upperQuartile(upperQuartile)
        , maximum(maximum)
        , outliers(outliers)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPStatisticalBox
////////////////////////////////////////////////////////////////////////////////////////////////////

/*! \class QCPStatisticalBox
  \brief A plottable representing a single statistical box in a plot.

  \image html QCPStatisticalBox.png

  To plot data, assign it with the \ref setData or \ref addData functions. Alternatively, you can
  also access and modify the data via the \ref data method, which returns a pointer to the internal
  \ref QCPStatisticalBoxDataContainer.

  Additionally each data point can itself have a list of outliers, drawn as scatter points at the
  key coordinate of the respective statistical box data point. They can either be set by using the
  respective \ref addData(double,double,double,double,double,double,const QVector<double>&)
  "addData" method or accessing the individual data points through \ref data, and setting the
  <tt>QVector<double> outliers</tt> of the data points directly.

  \section qcpstatisticalbox-appearance Changing the appearance

  The appearance of each data point box, ranging from the lower to the upper quartile, is
  controlled via \ref setPen and \ref setBrush. You may change the width of the boxes with \ref
  setWidth in plot coordinates.

  Each data point's visual representation also consists of two whiskers. Whiskers are the lines
  which reach from the upper quartile to the maximum, and from the lower quartile to the minimum.
  The appearance of the whiskers can be modified with: \ref setWhiskerPen, \ref setWhiskerBarPen,
  \ref setWhiskerWidth. The whisker width is the width of the bar perpendicular to the whisker at
  the top (for maximum) and bottom (for minimum). If the whisker pen is changed, make sure to set
  the \c capStyle to \c Qt::FlatCap. Otherwise the backbone line might exceed the whisker bars by a
  few pixels due to the pen cap being not perfectly flat.

  The median indicator line inside the box has its own pen, \ref setMedianPen.

  The outlier data points are drawn as normal scatter points. Their look can be controlled with
  \ref setOutlierStyle

  \section qcpstatisticalbox-usage Usage

  Like all data representing objects in QCustomPlot, the QCPStatisticalBox is a plottable
  (QCPAbstractPlottable). So the plottable-interface of QCustomPlot applies
  (QCustomPlot::plottable, QCustomPlot::removePlottable, etc.)

  Usually, you first create an instance:
  \snippet documentation/doc-code-snippets/mainwindow.cpp qcpstatisticalbox-creation-1
  which registers it with the QCustomPlot instance of the passed axes. Note that this QCustomPlot
  instance takes ownership of the plottable, so do not delete it manually but use
  QCustomPlot::removePlottable() instead. The newly created plottable can be modified, e.g.:
  \snippet documentation/doc-code-snippets/mainwindow.cpp qcpstatisticalbox-creation-2
*/

/* start documentation of inline functions */

/*! \fn QSharedPointer<QCPStatisticalBoxDataContainer> QCPStatisticalBox::data() const

  Returns a shared pointer to the internal data storage of type \ref
  QCPStatisticalBoxDataContainer. You may use it to directly manipulate the data, which may be more
  convenient and faster than using the regular \ref setData or \ref addData methods.
*/

/* end documentation of inline functions */

/*!
  Constructs a statistical box which uses \a keyAxis as its key axis ("x") and \a valueAxis as its
  value axis ("y"). \a keyAxis and \a valueAxis must reside in the same QCustomPlot instance and
  not have the same orientation. If either of these restrictions is violated, a corresponding
  message is printed to the debug output (qDebug), the construction is not aborted, though.

  The created QCPStatisticalBox is automatically registered with the QCustomPlot instance inferred
  from \a keyAxis. This QCustomPlot instance takes ownership of the QCPStatisticalBox, so do not
  delete it manually but use QCustomPlot::removePlottable() instead.
*/
QCPStatisticalBox::QCPStatisticalBox(QCPAxis* keyAxis, QCPAxis* valueAxis)
        : QCPAbstractPlottable1D<QCPStatisticalBoxData>(keyAxis, valueAxis)
        , mWidth(0.5)
        , mWhiskerWidth(0.2)
        , mWhiskerPen(Qt::black, 0, Qt::DashLine, Qt::FlatCap)
        , mWhiskerBarPen(Qt::black)
        , mWhiskerAntialiased(false)
        , mMedianPen(Qt::black, 3, Qt::SolidLine, Qt::FlatCap)
        , mOutlierStyle(QCPScatterStyle::ssCircle, Qt::blue, 6)
{
    setPen(QPen(Qt::black));
    setBrush(Qt::NoBrush);
}

/*! \overload

  Replaces the current data container with the provided \a data container.

  Since a QSharedPointer is used, multiple QCPStatisticalBoxes may share the same data container
  safely. Modifying the data in the container will then affect all statistical boxes that share the
  container. Sharing can be achieved by simply exchanging the data containers wrapped in shared
  pointers:
  \snippet documentation/doc-code-snippets/mainwindow.cpp qcpstatisticalbox-datasharing-1

  If you do not wish to share containers, but create a copy from an existing container, rather use
  the \ref QCPDataContainer<DataType>::set method on the statistical box data container directly:
  \snippet documentation/doc-code-snippets/mainwindow.cpp qcpstatisticalbox-datasharing-2

  \see addData
*/
void QCPStatisticalBox::setData(QSharedPointer<QCPStatisticalBoxDataContainer> data)
{
    if (mDataContainer == data)
        return;
    markDirty();
    mDataContainer = data;
}

/*! \overload

  Replaces the current data with the provided points in \a keys, \a minimum, \a lowerQuartile, \a
  median, \a upperQuartile and \a maximum. The provided vectors should have equal length. Else, the
  number of added points will be the size of the smallest vector.

  If you can guarantee that the passed data points are sorted by \a keys in ascending order, you
  can set \a alreadySorted to true, to improve performance by saving a sorting run.

  \see addData
*/
void QCPStatisticalBox::setData(const QVector<double>& keys, const QVector<double>& minimum,
                                const QVector<double>& lowerQuartile, const QVector<double>& median,
                                const QVector<double>& upperQuartile,
                                const QVector<double>& maximum, bool alreadySorted)
{
    mDataContainer->clear();
    addData(keys, minimum, lowerQuartile, median, upperQuartile, maximum, alreadySorted);
}

/*!
  Sets the width of the boxes in key coordinates.

  \see setWhiskerWidth
*/
void QCPStatisticalBox::setWidth(double width)
{
    if (mWidth == width)
        return;
    markDirty();
    mWidth = width;
}

/*!
  Sets the width of the whiskers in key coordinates.

  Whiskers are the lines which reach from the upper quartile to the maximum, and from the lower
  quartile to the minimum.

  \see setWidth
*/
void QCPStatisticalBox::setWhiskerWidth(double width)
{
    if (mWhiskerWidth == width)
        return;
    markDirty();
    mWhiskerWidth = width;
}

/*!
  Sets the pen used for drawing the whisker backbone.

  Whiskers are the lines which reach from the upper quartile to the maximum, and from the lower
  quartile to the minimum.

  Make sure to set the \c capStyle of the passed \a pen to \c Qt::FlatCap. Otherwise the backbone
  line might exceed the whisker bars by a few pixels due to the pen cap being not perfectly flat.

  \see setWhiskerBarPen
*/
void QCPStatisticalBox::setWhiskerPen(const QPen& pen)
{
    if (mWhiskerPen == pen)
        return;
    markDirty();
    mWhiskerPen = pen;
}

/*!
  Sets the pen used for drawing the whisker bars. Those are the lines parallel to the key axis at
  each end of the whisker backbone.

  Whiskers are the lines which reach from the upper quartile to the maximum, and from the lower
  quartile to the minimum.

  \see setWhiskerPen
*/
void QCPStatisticalBox::setWhiskerBarPen(const QPen& pen)
{
    if (mWhiskerBarPen == pen)
        return;
    markDirty();
    mWhiskerBarPen = pen;
}

/*!
  Sets whether the statistical boxes whiskers are drawn with antialiasing or not.

  Note that antialiasing settings may be overridden by QCustomPlot::setAntialiasedElements and
  QCustomPlot::setNotAntialiasedElements.
*/
void QCPStatisticalBox::setWhiskerAntialiased(bool enabled)
{
    if (mWhiskerAntialiased == enabled)
        return;
    markDirty();
    mWhiskerAntialiased = enabled;
}

/*!
  Sets the pen used for drawing the median indicator line inside the statistical boxes.
*/
void QCPStatisticalBox::setMedianPen(const QPen& pen)
{
    if (mMedianPen == pen)
        return;
    markDirty();
    mMedianPen = pen;
}

/*!
  Sets the appearance of the outlier data points.

  Outliers can be specified with the method
  \ref addData(double key, double minimum, double lowerQuartile, double median, double
  upperQuartile, double maximum, const QVector<double> &outliers)
*/
void QCPStatisticalBox::setOutlierStyle(const QCPScatterStyle& style)
{
    markDirty();
    mOutlierStyle = style;
}

/*! \overload

  Adds the provided points in \a keys, \a minimum, \a lowerQuartile, \a median, \a upperQuartile and
  \a maximum to the current data. The provided vectors should have equal length. Else, the number
  of added points will be the size of the smallest vector.

  If you can guarantee that the passed data points are sorted by \a keys in ascending order, you
  can set \a alreadySorted to true, to improve performance by saving a sorting run.

  Alternatively, you can also access and modify the data directly via the \ref data method, which
  returns a pointer to the internal data container.
*/
void QCPStatisticalBox::addData(const QVector<double>& keys, const QVector<double>& minimum,
                                const QVector<double>& lowerQuartile, const QVector<double>& median,
                                const QVector<double>& upperQuartile,
                                const QVector<double>& maximum, bool alreadySorted)
{
    if (keys.size() != minimum.size() || minimum.size() != lowerQuartile.size()
        || lowerQuartile.size() != median.size() || median.size() != upperQuartile.size()
        || upperQuartile.size() != maximum.size() || maximum.size() != keys.size())
        qDebug()
            << Q_FUNC_INFO
            << "keys, minimum, lowerQuartile, median, upperQuartile, maximum have different sizes:"
            << keys.size() << minimum.size() << lowerQuartile.size() << median.size()
            << upperQuartile.size() << maximum.size();
    const int n = qMin(keys.size(),
                       qMin(minimum.size(),
                            qMin(lowerQuartile.size(),
                                 qMin(median.size(), qMin(upperQuartile.size(), maximum.size())))));
    QVector<QCPStatisticalBoxData> tempData(n);
    QVector<QCPStatisticalBoxData>::iterator it = tempData.begin();
    const QVector<QCPStatisticalBoxData>::iterator itEnd = tempData.end();
    int i = 0;
    while (it != itEnd)
    {
        it->key = keys[i];
        it->minimum = minimum[i];
        it->lowerQuartile = lowerQuartile[i];
        it->median = median[i];
        it->upperQuartile = upperQuartile[i];
        it->maximum = maximum[i];
        ++it;
        ++i;
    }
    mDataContainer->add(
        tempData, alreadySorted); // don't modify tempData beyond this to prevent copy on write
}

/*! \overload

  Adds the provided data point as \a key, \a minimum, \a lowerQuartile, \a median, \a upperQuartile
  and \a maximum to the current data.

  Alternatively, you can also access and modify the data directly via the \ref data method, which
  returns a pointer to the internal data container.
*/
void QCPStatisticalBox::addData(double key, double minimum, double lowerQuartile, double median,
                                double upperQuartile, double maximum,
                                const QVector<double>& outliers)
{
    mDataContainer->add(QCPStatisticalBoxData(key, minimum, lowerQuartile, median, upperQuartile,
                                              maximum, outliers));
}

/*!
  \copydoc QCPPlottableInterface1D::selectTestRect
*/
QCPDataSelection QCPStatisticalBox::selectTestRect(const QRectF& rect, bool onlySelectable) const
{
    QCPDataSelection result;
    if ((onlySelectable && mSelectable == QCP::stNone) || mDataContainer->isEmpty())
        return result;
    if (!mKeyAxis || !mValueAxis)
        return result;

    QCPStatisticalBoxDataContainer::const_iterator visibleBegin, visibleEnd;
    getVisibleDataBounds(visibleBegin, visibleEnd);

    for (QCPStatisticalBoxDataContainer::const_iterator it = visibleBegin; it != visibleEnd; ++it)
    {
        if (rect.intersects(getQuartileBox(it)))
            result.addDataRange(QCPDataRange(int(it - mDataContainer->constBegin()),
                                             int(it - mDataContainer->constBegin() + 1)),
                                false);
    }
    result.simplify();
    return result;
}

/*!
  Implements a selectTest specific to this plottable's point geometry.

  If \a details is not 0, it will be set to a \ref QCPDataSelection, describing the closest data
  point to \a pos.

  \seebaseclassmethod \ref QCPAbstractPlottable::selectTest
*/
double QCPStatisticalBox::selectTest(const QPointF& pos, bool onlySelectable,
                                     [[maybe_unused]] QVariant* details) const
{
    if ((onlySelectable && mSelectable == QCP::stNone) || mDataContainer->isEmpty())
        return -1;
    if (!mKeyAxis || !mValueAxis)
        return -1;

    if (mKeyAxis->axisRect()->rect().contains(pos.toPoint())
        || mParentPlot->interactions().testFlag(QCP::iSelectPlottablesBeyondAxisRect))
    {
        // get visible data range:
        QCPStatisticalBoxDataContainer::const_iterator visibleBegin, visibleEnd;
        QCPStatisticalBoxDataContainer::const_iterator closestDataPoint
            = mDataContainer->constEnd();
        getVisibleDataBounds(visibleBegin, visibleEnd);
        double minDistSqr = (std::numeric_limits<double>::max)();
        for (QCPStatisticalBoxDataContainer::const_iterator it = visibleBegin; it != visibleEnd;
             ++it)
        {
            if (getQuartileBox(it).contains(pos)) // quartile box
            {
                double currentDistSqr = mParentPlot->selectionTolerance() * 0.99
                    * mParentPlot->selectionTolerance() * 0.99;
                if (currentDistSqr < minDistSqr)
                {
                    minDistSqr = currentDistSqr;
                    closestDataPoint = it;
                }
            }
            else // whiskers
            {
                const QVector<QLineF> whiskerBackbones = getWhiskerBackboneLines(it);
                const QCPVector2D posVec(pos);
                for (const QLineF& backbone : whiskerBackbones)
                {
                    double currentDistSqr = posVec.distanceSquaredToLine(backbone);
                    if (currentDistSqr < minDistSqr)
                    {
                        minDistSqr = currentDistSqr;
                        closestDataPoint = it;
                    }
                }
            }
        }
        if (details)
        {
            int pointIndex = int(closestDataPoint - mDataContainer->constBegin());
            details->setValue(QCPDataSelection(QCPDataRange(pointIndex, pointIndex + 1)));
        }
        return qSqrt(minDistSqr);
    }
    return -1;
}

/* inherits documentation from base class */
QCPRange QCPStatisticalBox::getKeyRange(bool& foundRange, QCP::SignDomain inSignDomain) const
{
    QCPRange range = mDataContainer->keyRange(foundRange, inSignDomain);
    // determine exact range by including width of bars/flags:
    if (foundRange)
    {
        if (inSignDomain != QCP::sdPositive || range.lower - mWidth * 0.5 > 0)
            range.lower -= mWidth * 0.5;
        if (inSignDomain != QCP::sdNegative || range.upper + mWidth * 0.5 < 0)
            range.upper += mWidth * 0.5;
    }
    return range;
}

/* inherits documentation from base class */
QCPRange QCPStatisticalBox::getValueRange(bool& foundRange, QCP::SignDomain inSignDomain,
                                          const QCPRange& inKeyRange) const
{
    return mDataContainer->valueRange(foundRange, inSignDomain, inKeyRange);
}

/* inherits documentation from base class */
void QCPStatisticalBox::draw(QCPDrawContext& context) const
{
    if (mDataContainer->isEmpty())
        return;
    QCPAxis* keyAxis = mKeyAxis.data();
    QCPAxis* valueAxis = mValueAxis.data();
    if (!keyAxis || !valueAxis)
    {
        qDebug() << Q_FUNC_INFO << "invalid key or value axis";
        return;
    }

    // Selected boxes go last, as they always did, with the decorator's look baked in.
    QList<QCPDataRange> selectedSegments, unselectedSegments;
    getDataSegments(selectedSegments, unselectedSegments);
    for (const QCPDataRange& segment : unselectedSegments)
        recordSegment(context, segment, false);
    for (const QCPDataRange& segment : selectedSegments)
        recordSegment(context, segment, true);

    // Only a decorator subclass draws a decoration of its own: the base class's is empty, and
    // recording a painter callback for it would make every selection change a view change.
    if (mSelectionDecorator && !selection().isEmpty()
        && typeid(*mSelectionDecorator) != typeid(QCPSelectionDecorator))
    {
        context.painter(
            [this](QCPPainter& painter, const QCPTransform&)
            {
                if (mSelectionDecorator)
                    mSelectionDecorator->drawDecoration(&painter, selection());
            });
    }
}

/* inherits documentation from base class */
void QCPStatisticalBox::drawLegendIcon(QCPPainter* painter, const QRectF& rect) const
{
    // draw filled rect:
    applyDefaultAntialiasingHint(painter);
    painter->setPen(mPen);
    painter->setBrush(mBrush);
    QRectF r = QRectF(0, 0, rect.width() * 0.67, rect.height() * 0.67);
    r.moveCenter(rect.center());
    painter->drawRect(r);
}

/*! \internal

  Records one segment of the data container, with the pen and brush its boxes draw with: the
  selection decorator's for a selected segment.

  Only the boxes whose bars touch the key window are recorded: the window reaches one view width
  past each side, so a pan inside it only shifts them, and the recording is remade once it leaves.
*/
void QCPStatisticalBox::recordSegment(QCPDrawContext& context, const QCPDataRange& segment,
                                      bool selected) const
{
    // A box straddles its key by half its width, so the window grows by that much in keys.
    const QCPKeyWindow window = context.keyWindow();
    const double margin = qMax(mWidth, mWhiskerWidth) * 0.5;
    auto begin = mDataContainer->findBegin(window.keys.lower - margin);
    auto end = mDataContainer->findEnd(window.keys.upper + margin);
    mDataContainer->limitIteratorsToDataRange(begin, end, segment);
    if (begin == end)
        return;

    const QPen pen = selected && mSelectionDecorator ? mSelectionDecorator->pen() : mPen;
    const QBrush brush = selected && mSelectionDecorator ? mSelectionDecorator->brush() : mBrush;
    QCPScatterStyle outlierStyle = mOutlierStyle;
    if (selected && mSelectionDecorator)
        outlierStyle = mSelectionDecorator->getFinalScatterStyle(mOutlierStyle);

    std::vector<QCP::Position> medians;
    std::vector<QCP::Position> backbones, whiskerBars;
    std::vector<QPointF> outliers;
    for (auto it = begin; it != end; ++it)
    {
        const QCPStatisticalBoxData& entry = *it;
        if (!qIsFinite(entry.key) || !qIsFinite(entry.minimum) || !qIsFinite(entry.lowerQuartile)
            || !qIsFinite(entry.median) || !qIsFinite(entry.upperQuartile)
            || !qIsFinite(entry.maximum))
            continue;
        const double half = mWidth * 0.5;
        // The quartile box, filled with the box's brush and stroked with its pen.
        context.rect(QCP::Data { entry.key - half, entry.upperQuartile },
                     QCP::Data { entry.key + half, entry.lowerQuartile }, pen, brush);
        // The median spans the box, so the box's own edges clip it: it needs no clip of its own.
        medians.push_back(QCP::Data { entry.key - half, entry.median });
        medians.push_back(QCP::Data { entry.key + half, entry.median });
        medians.push_back(QCP::Data { kNan, kNan });
        const double whiskerHalf = mWhiskerWidth * 0.5;
        backbones.push_back(QCP::Data { entry.key, entry.lowerQuartile });
        backbones.push_back(QCP::Data { entry.key, entry.minimum });
        backbones.push_back(QCP::Data { kNan, kNan });
        backbones.push_back(QCP::Data { entry.key, entry.upperQuartile });
        backbones.push_back(QCP::Data { entry.key, entry.maximum });
        backbones.push_back(QCP::Data { kNan, kNan });
        whiskerBars.push_back(QCP::Data { entry.key - whiskerHalf, entry.minimum });
        whiskerBars.push_back(QCP::Data { entry.key + whiskerHalf, entry.minimum });
        whiskerBars.push_back(QCP::Data { kNan, kNan });
        whiskerBars.push_back(QCP::Data { entry.key - whiskerHalf, entry.maximum });
        whiskerBars.push_back(QCP::Data { entry.key + whiskerHalf, entry.maximum });
        whiskerBars.push_back(QCP::Data { kNan, kNan });
        for (double outlier : entry.outliers)
        {
            if (qIsFinite(outlier))
                outliers.emplace_back(entry.key, outlier);
        }
    }
    if (!medians.empty())
        context.polyline(std::span<const QCP::Position>(medians), mMedianPen);
    if (!backbones.empty())
        context.polyline(std::span<const QCP::Position>(backbones), mWhiskerPen);
    if (!whiskerBars.empty())
        context.polyline(std::span<const QCP::Position>(whiskerBars), mWhiskerBarPen);
    if (!outliers.empty())
        context.markers(std::make_shared<const std::vector<QPointF>>(std::move(outliers)),
                        outlierStyle);
}

/*!  \internal

  called by \ref draw to determine which data (key) range is visible at the current key axis range
  setting, so only that needs to be processed. It also takes into account the bar width.

  \a begin returns an iterator to the lowest data point that needs to be taken into account when
  plotting. Note that in order to get a clean plot all the way to the edge of the axis rect, \a
  lower may still be just outside the visible range.

  \a end returns an iterator one higher than the highest visible data point. Same as before, \a end
  may also lie just outside of the visible range.

  if the plottable contains no data, both \a begin and \a end point to constEnd.
*/
void QCPStatisticalBox::getVisibleDataBounds(
    QCPStatisticalBoxDataContainer::const_iterator& begin,
    QCPStatisticalBoxDataContainer::const_iterator& end) const
{
    if (!mKeyAxis)
    {
        qDebug() << Q_FUNC_INFO << "invalid key axis";
        begin = mDataContainer->constEnd();
        end = mDataContainer->constEnd();
        return;
    }
    begin = mDataContainer->findBegin(
        mKeyAxis.data()->range().lower
        - mWidth * 0.5); // subtract half width of box to include partially visible data points
    end = mDataContainer->findEnd(
        mKeyAxis.data()->range().upper
        + mWidth * 0.5); // add half width of box to include partially visible data points
}

/*!  \internal

  Returns the box in plot coordinates (keys in x, values in y of the returned rect) that covers the
  value range from the lower to the upper quartile, of the data given by \a it.

  \see getWhiskerBackboneLines, QCPStatisticalBox::draw
*/
QRectF QCPStatisticalBox::getQuartileBox(QCPStatisticalBoxDataContainer::const_iterator it) const
{
    QRectF result;
    result.setTopLeft(coordsToPixels(it->key - mWidth * 0.5, it->upperQuartile));
    result.setBottomRight(coordsToPixels(it->key + mWidth * 0.5, it->lowerQuartile));
    return result;
}

/*!  \internal

  Returns the whisker backbones (keys in x, values in y of the returned lines) that cover the value
  range from the minimum to the lower quartile, and from the upper quartile to the maximum of the
  data given by \a it.

  \see getQuartileBox, QCPStatisticalBox::draw
*/
QVector<QLineF>
QCPStatisticalBox::getWhiskerBackboneLines(QCPStatisticalBoxDataContainer::const_iterator it) const
{
    QVector<QLineF> result(2);
    result[0].setPoints(coordsToPixels(it->key, it->lowerQuartile),
                        coordsToPixels(it->key, it->minimum)); // min backbone
    result[1].setPoints(coordsToPixels(it->key, it->upperQuartile),
                        coordsToPixels(it->key, it->maximum)); // max backbone
    return result;
}
