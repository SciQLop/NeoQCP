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

#include "plottable-financial.h"

#include "../axis/axis.h"
#include "../core.h"
#include "../layoutelements/layoutelement-axisrect.h"
#include "../painting/draw-context.h"
#include "../painting/painter.h"
#include <array>
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
//////////////////// QCPFinancialData
////////////////////////////////////////////////////////////////////////////////////////////////////

/*! \class QCPFinancialData
  \brief Holds the data of one single data point for QCPFinancial.

  The stored data is:
  \li \a key: coordinate on the key axis of this data point (this is the \a mainKey and the \a
  sortKey)
  \li \a open: The opening value at the data point (this is the \a mainValue)
  \li \a high: The high/maximum value at the data point
  \li \a low: The low/minimum value at the data point
  \li \a close: The closing value at the data point

  The container for storing multiple data points is \ref QCPFinancialDataContainer. It is a typedef
  for \ref QCPDataContainer with \ref QCPFinancialData as the DataType template parameter. See the
  documentation there for an explanation regarding the data type's generic methods.

  \see QCPFinancialDataContainer
*/

/* start documentation of inline functions */

/*! \fn double QCPFinancialData::sortKey() const

  Returns the \a key member of this data point.

  For a general explanation of what this method is good for in the context of the data container,
  see the documentation of \ref QCPDataContainer.
*/

/*! \fn static QCPFinancialData QCPFinancialData::fromSortKey(double sortKey)

  Returns a data point with the specified \a sortKey. All other members are set to zero.

  For a general explanation of what this method is good for in the context of the data container,
  see the documentation of \ref QCPDataContainer.
*/

/*! \fn static static bool QCPFinancialData::sortKeyIsMainKey()

  Since the member \a key is both the data point key coordinate and the data ordering parameter,
  this method returns true.

  For a general explanation of what this method is good for in the context of the data container,
  see the documentation of \ref QCPDataContainer.
*/

/*! \fn double QCPFinancialData::mainKey() const

  Returns the \a key member of this data point.

  For a general explanation of what this method is good for in the context of the data container,
  see the documentation of \ref QCPDataContainer.
*/

/*! \fn double QCPFinancialData::mainValue() const

  Returns the \a open member of this data point.

  For a general explanation of what this method is good for in the context of the data container,
  see the documentation of \ref QCPDataContainer.
*/

/*! \fn QCPRange QCPFinancialData::valueRange() const

  Returns a QCPRange spanning from the \a low to the \a high value of this data point.

  For a general explanation of what this method is good for in the context of the data container,
  see the documentation of \ref QCPDataContainer.
*/

/* end documentation of inline functions */

/*!
  Constructs a data point with key and all values set to zero.
*/
QCPFinancialData::QCPFinancialData() : key(0), open(0), high(0), low(0), close(0) { }

/*!
  Constructs a data point with the specified \a key and OHLC values.
*/
QCPFinancialData::QCPFinancialData(double key, double open, double high, double low, double close)
        : key(key), open(open), high(high), low(low), close(close)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPFinancial
////////////////////////////////////////////////////////////////////////////////////////////////////

/*! \class QCPFinancial
  \brief A plottable representing a financial stock chart

  \image html QCPFinancial.png

  This plottable represents time series data binned to certain intervals, mainly used for stock
  charts. The two common representations OHLC (Open-High-Low-Close) bars and Candlesticks can be
  set via \ref setChartStyle.

  The data is passed via \ref setData as a set of open/high/low/close values at certain keys
  (typically times). This means the data must be already binned appropriately. If data is only
  available as a series of values (e.g. \a price against \a time), you can use the static
  convenience function \ref timeSeriesToOhlc to generate binned OHLC-data which can then be passed
  to \ref setData.

  The width of the OHLC bars/candlesticks can be controlled with \ref setWidth and \ref
  setWidthType. A typical choice is to set the width type to \ref wtPlotCoords (the default) and
  the width to (or slightly less than) one time bin interval width.

  \section qcpfinancial-appearance Changing the appearance

  Charts can be either single- or two-colored (\ref setTwoColored). If set to be single-colored,
  lines are drawn with the plottable's pen (\ref setPen) and fills with the brush (\ref setBrush).

  If set to two-colored, positive changes of the value during an interval (\a close >= \a open) are
  represented with a different pen and brush than negative changes (\a close < \a open). These can
  be configured with \ref setPenPositive, \ref setPenNegative, \ref setBrushPositive, and \ref
  setBrushNegative. In two-colored mode, the normal plottable pen/brush is ignored. Upon selection
  however, the normal selected pen/brush (provided by the \ref selectionDecorator) is used,
  irrespective of whether the chart is single- or two-colored.

  \section qcpfinancial-usage Usage

  Like all data representing objects in QCustomPlot, the QCPFinancial is a plottable
  (QCPAbstractPlottable). So the plottable-interface of QCustomPlot applies
  (QCustomPlot::plottable, QCustomPlot::removePlottable, etc.)

  Usually, you first create an instance:

  \snippet documentation/doc-code-snippets/mainwindow.cpp qcpfinancial-creation-1
  which registers it with the QCustomPlot instance of the passed axes. Note that this QCustomPlot
  instance takes ownership of the plottable, so do not delete it manually but use
  QCustomPlot::removePlottable() instead. The newly created plottable can be modified, e.g.:

  \snippet documentation/doc-code-snippets/mainwindow.cpp qcpfinancial-creation-2
  Here we have used the static helper method \ref timeSeriesToOhlc, to turn a time-price data
  series into a 24-hour binned open-high-low-close data series as QCPFinancial uses.
*/

/* start of documentation of inline functions */

/*! \fn QCPFinancialDataContainer *QCPFinancial::data() const

  Returns a pointer to the internal data storage of type \ref QCPFinancialDataContainer. You may
  use it to directly manipulate the data, which may be more convenient and faster than using the
  regular \ref setData or \ref addData methods, in certain situations.
*/

/* end of documentation of inline functions */

/*!
  Constructs a financial chart which uses \a keyAxis as its key axis ("x") and \a valueAxis as its
  value axis ("y"). \a keyAxis and \a valueAxis must reside in the same QCustomPlot instance and not
  have the same orientation. If either of these restrictions is violated, a corresponding message is
  printed to the debug output (qDebug), the construction is not aborted, though.

  The created QCPFinancial is automatically registered with the QCustomPlot instance inferred from
  \a keyAxis. This QCustomPlot instance takes ownership of the QCPFinancial, so do not delete it
  manually but use QCustomPlot::removePlottable() instead.
*/
QCPFinancial::QCPFinancial(QCPAxis* keyAxis, QCPAxis* valueAxis)
        : QCPAbstractPlottable1D<QCPFinancialData>(keyAxis, valueAxis)
        , mChartStyle(csCandlestick)
        , mWidth(0.5)
        , mWidthType(wtPlotCoords)
        , mTwoColored(true)
        , mBrushPositive(QBrush(QColor(50, 160, 0)))
        , mBrushNegative(QBrush(QColor(180, 0, 15)))
        , mPenPositive(QPen(QColor(40, 150, 0)))
        , mPenNegative(QPen(QColor(170, 5, 5)))
{
    mSelectionDecorator->setBrush(QBrush(QColor(160, 160, 255)));
}

QCPFinancial::~QCPFinancial() { }

/*! \overload

  Replaces the current data container with the provided \a data container.

  Since a QSharedPointer is used, multiple QCPFinancials may share the same data container safely.
  Modifying the data in the container will then affect all financials that share the container.
  Sharing can be achieved by simply exchanging the data containers wrapped in shared pointers:
  \snippet documentation/doc-code-snippets/mainwindow.cpp qcpfinancial-datasharing-1

  If you do not wish to share containers, but create a copy from an existing container, rather use
  the \ref QCPDataContainer<DataType>::set method on the financial's data container directly:
  \snippet documentation/doc-code-snippets/mainwindow.cpp qcpfinancial-datasharing-2

  \see addData, timeSeriesToOhlc
*/
void QCPFinancial::setData(QSharedPointer<QCPFinancialDataContainer> data)
{
    if (mDataContainer == data)
        return;
    markDirty();
    mDataContainer = data;
}

/*! \overload

  Replaces the current data with the provided points in \a keys, \a open, \a high, \a low and \a
  close. The provided vectors should have equal length. Else, the number of added points will be
  the size of the smallest vector.

  If you can guarantee that the passed data points are sorted by \a keys in ascending order, you
  can set \a alreadySorted to true, to improve performance by saving a sorting run.

  \see addData, timeSeriesToOhlc
*/
void QCPFinancial::setData(const QVector<double>& keys, const QVector<double>& open,
                           const QVector<double>& high, const QVector<double>& low,
                           const QVector<double>& close, bool alreadySorted)
{
    mDataContainer->clear();
    addData(keys, open, high, low, close, alreadySorted);
}

/*!
  Sets which representation style shall be used to display the OHLC data.
*/
void QCPFinancial::setChartStyle(QCPFinancial::ChartStyle style)
{
    if (mChartStyle == style)
        return;
    markDirty();
    mChartStyle = style;
}

/*!
  Sets the width of the individual bars/candlesticks to \a width in plot key coordinates.

  A typical choice is to set it to (or slightly less than) one bin interval width.
*/
void QCPFinancial::setWidth(double width)
{
    if (mWidth == width)
        return;
    markDirty();
    mWidth = width;
}

/*!
  Sets how the width of the financial bars is defined. See the documentation of \ref WidthType for
  an explanation of the possible values for \a widthType.

  The default value is \ref wtPlotCoords.

  \see setWidth
*/
void QCPFinancial::setWidthType(QCPFinancial::WidthType widthType)
{
    if (mWidthType == widthType)
        return;
    markDirty();
    mWidthType = widthType;
}

/*!
  Sets whether this chart shall contrast positive from negative trends per data point by using two
  separate colors to draw the respective bars/candlesticks.

  If \a twoColored is false, the normal plottable's pen and brush are used (\ref setPen, \ref
  setBrush).

  \see setPenPositive, setPenNegative, setBrushPositive, setBrushNegative
*/
void QCPFinancial::setTwoColored(bool twoColored)
{
    if (mTwoColored == twoColored)
        return;
    markDirty();
    mTwoColored = twoColored;
}

/*!
  If \ref setTwoColored is set to true, this function controls the brush that is used to draw fills
  of data points with a positive trend (i.e. bars/candlesticks with close >= open).

  If \a twoColored is false, the normal plottable's pen and brush are used (\ref setPen, \ref
  setBrush).

  \see setBrushNegative, setPenPositive, setPenNegative
*/
void QCPFinancial::setBrushPositive(const QBrush& brush)
{
    if (mBrushPositive == brush)
        return;
    markDirty();
    mBrushPositive = brush;
}

/*!
  If \ref setTwoColored is set to true, this function controls the brush that is used to draw fills
  of data points with a negative trend (i.e. bars/candlesticks with close < open).

  If \a twoColored is false, the normal plottable's pen and brush are used (\ref setPen, \ref
  setBrush).

  \see setBrushPositive, setPenNegative, setPenPositive
*/
void QCPFinancial::setBrushNegative(const QBrush& brush)
{
    if (mBrushNegative == brush)
        return;
    markDirty();
    mBrushNegative = brush;
}

/*!
  If \ref setTwoColored is set to true, this function controls the pen that is used to draw
  outlines of data points with a positive trend (i.e. bars/candlesticks with close >= open).

  If \a twoColored is false, the normal plottable's pen and brush are used (\ref setPen, \ref
  setBrush).

  \see setPenNegative, setBrushPositive, setBrushNegative
*/
void QCPFinancial::setPenPositive(const QPen& pen)
{
    if (mPenPositive == pen)
        return;
    markDirty();
    mPenPositive = pen;
}

/*!
  If \ref setTwoColored is set to true, this function controls the pen that is used to draw
  outlines of data points with a negative trend (i.e. bars/candlesticks with close < open).

  If \a twoColored is false, the normal plottable's pen and brush are used (\ref setPen, \ref
  setBrush).

  \see setPenPositive, setBrushNegative, setBrushPositive
*/
void QCPFinancial::setPenNegative(const QPen& pen)
{
    if (mPenNegative == pen)
        return;
    markDirty();
    mPenNegative = pen;
}

/*! \overload

  Adds the provided points in \a keys, \a open, \a high, \a low and \a close to the current data.
  The provided vectors should have equal length. Else, the number of added points will be the size
  of the smallest vector.

  If you can guarantee that the passed data points are sorted by \a keys in ascending order, you
  can set \a alreadySorted to true, to improve performance by saving a sorting run.

  Alternatively, you can also access and modify the data directly via the \ref data method, which
  returns a pointer to the internal data container.

  \see timeSeriesToOhlc
*/
void QCPFinancial::addData(const QVector<double>& keys, const QVector<double>& open,
                           const QVector<double>& high, const QVector<double>& low,
                           const QVector<double>& close, bool alreadySorted)
{
    if (keys.size() != open.size() || open.size() != high.size() || high.size() != low.size()
        || low.size() != close.size() || close.size() != keys.size())
        qDebug() << Q_FUNC_INFO
                 << "keys, open, high, low, close have different sizes:" << keys.size()
                 << open.size() << high.size() << low.size() << close.size();
    const int n
        = qMin(keys.size(), qMin(open.size(), qMin(high.size(), qMin(low.size(), close.size()))));
    QVector<QCPFinancialData> tempData(n);
    QVector<QCPFinancialData>::iterator it = tempData.begin();
    const QVector<QCPFinancialData>::iterator itEnd = tempData.end();
    int i = 0;
    while (it != itEnd)
    {
        it->key = keys[i];
        it->open = open[i];
        it->high = high[i];
        it->low = low[i];
        it->close = close[i];
        ++it;
        ++i;
    }
    mDataContainer->add(
        tempData, alreadySorted); // don't modify tempData beyond this to prevent copy on write
}

/*! \overload

  Adds the provided data point as \a key, \a open, \a high, \a low and \a close to the current
  data.

  Alternatively, you can also access and modify the data directly via the \ref data method, which
  returns a pointer to the internal data container.

  \see timeSeriesToOhlc
*/
void QCPFinancial::addData(double key, double open, double high, double low, double close)
{
    mDataContainer->add(QCPFinancialData(key, open, high, low, close));
}

/*!
  \copydoc QCPPlottableInterface1D::selectTestRect
*/
QCPDataSelection QCPFinancial::selectTestRect(const QRectF& rect, bool onlySelectable) const
{
    QCPDataSelection result;
    if ((onlySelectable && mSelectable == QCP::stNone) || mDataContainer->isEmpty())
        return result;
    if (!keyAxisRaw() || !valueAxisRaw())
        return result;

    QCPFinancialDataContainer::const_iterator visibleBegin, visibleEnd;
    getVisibleDataBounds(visibleBegin, visibleEnd);

    for (QCPFinancialDataContainer::const_iterator it = visibleBegin; it != visibleEnd; ++it)
    {
        if (rect.intersects(selectionHitBox(it)))
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
double QCPFinancial::selectTest(const QPointF& pos, bool onlySelectable,
                                [[maybe_unused]] QVariant* details) const
{
    if ((onlySelectable && mSelectable == QCP::stNone) || mDataContainer->isEmpty())
        return -1;
    if (!keyAxisRaw() || !valueAxisRaw())
        return -1;

    if (axisRectOf(keyAxisRaw())->rect().contains(pos.toPoint())
        || mParentPlot->interactions().testFlag(QCP::iSelectPlottablesBeyondAxisRect))
    {
        // get visible data range:
        QCPFinancialDataContainer::const_iterator visibleBegin, visibleEnd;
        QCPFinancialDataContainer::const_iterator closestDataPoint = mDataContainer->constEnd();
        getVisibleDataBounds(visibleBegin, visibleEnd);
        // perform select test according to configured style:
        double result = -1;
        switch (mChartStyle)
        {
            case QCPFinancial::csOhlc:
                result = ohlcSelectTest(pos, visibleBegin, visibleEnd, closestDataPoint);
                break;
            case QCPFinancial::csCandlestick:
                result = candlestickSelectTest(pos, visibleBegin, visibleEnd, closestDataPoint);
                break;
        }
        if (details)
        {
            int pointIndex = int(closestDataPoint - mDataContainer->constBegin());
            details->setValue(QCPDataSelection(QCPDataRange(pointIndex, pointIndex + 1)));
        }
        return result;
    }

    return -1;
}

/* inherits documentation from base class */
QCPRange QCPFinancial::getKeyRange(bool& foundRange, QCP::SignDomain inSignDomain) const
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
QCPRange QCPFinancial::getValueRange(bool& foundRange, QCP::SignDomain inSignDomain,
                                     const QCPRange& inKeyRange) const
{
    return mDataContainer->valueRange(foundRange, inSignDomain, inKeyRange);
}

/*!
  A convenience function that converts time series data (\a value against \a time) to OHLC binned
  data points. The return value can then be passed on to \ref QCPFinancialDataContainer::set(const
  QCPFinancialDataContainer&).

  The size of the bins can be controlled with \a timeBinSize in the same units as \a time is given.
  For example, if the unit of \a time is seconds and single OHLC/Candlesticks should span an hour
  each, set \a timeBinSize to 3600.

  \a timeBinOffset allows to control precisely at what \a time coordinate a bin should start. The
  value passed as \a timeBinOffset doesn't need to be in the range encompassed by the \a time keys.
  It merely defines the mathematical offset/phase of the bins that will be used to process the
  data.
*/
QCPFinancialDataContainer QCPFinancial::timeSeriesToOhlc(const QVector<double>& time,
                                                         const QVector<double>& value,
                                                         double timeBinSize, double timeBinOffset)
{
    QCPFinancialDataContainer data;
    int count = qMin(time.size(), value.size());
    if (count == 0)
        return QCPFinancialDataContainer();

    QCPFinancialData currentBinData(0, value.first(), value.first(), value.first(), value.first());
    int currentBinIndex = qFloor((time.first() - timeBinOffset) / timeBinSize + 0.5);
    for (int i = 0; i < count; ++i)
    {
        int index = qFloor((time.at(i) - timeBinOffset) / timeBinSize + 0.5);
        if (currentBinIndex == index) // data point still in current bin, extend high/low:
        {
            if (value.at(i) < currentBinData.low)
                currentBinData.low = value.at(i);
            if (value.at(i) > currentBinData.high)
                currentBinData.high = value.at(i);
            if (i == count - 1) // last data point is in current bin, finalize bin:
            {
                currentBinData.close = value.at(i);
                currentBinData.key = timeBinOffset + (index)*timeBinSize;
                data.add(currentBinData);
            }
        }
        else // data point not anymore in current bin, set close of old and open of new bin, and add
             // old to map:
        {
            // finalize current bin:
            currentBinData.close = value.at(i - 1);
            currentBinData.key = timeBinOffset + (index - 1) * timeBinSize;
            data.add(currentBinData);
            // start next bin:
            currentBinIndex = index;
            currentBinData.open = value.at(i);
            currentBinData.high = value.at(i);
            currentBinData.low = value.at(i);
        }
    }

    return data;
}

/* inherits documentation from base class */
void QCPFinancial::draw(QCPDrawContext& context) const
{
    if (!keyAxisRaw() || !valueAxisRaw())
        return;
    if (mDataContainer->isEmpty())
        return;

    // Selected entries go last, as they always did, with the decorator's look baked in.
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

namespace
{
//! The icon rect a unit-box recording is replayed into: the frame maps the box's corners onto it.
QRectF unitBoxPixels(const QCPTransform& transform)
{
    const QCP::Pixels topLeft = transform.map(QCP::Data { 0.0, 1.0 });
    const QCP::Pixels bottomRight = transform.map(QCP::Data { 1.0, 0.0 });
    return QRectF(QPointF(topLeft.x, topLeft.y), QPointF(bottomRight.x, bottomRight.y));
}
} // namespace

/* inherits documentation from base class */
void QCPFinancial::drawLegendIcon(QCPDrawContext& icon) const
{
    // The icon clips to two diagonal halves and turns antialiasing off, which no native command
    // expresses: the unit rect gives the replay its box, and the painter draws inside the rect the
    // box was fitted to.
    icon.rect(QCP::Data { 0.0, 0.0 }, QCP::Data { 1.0, 1.0 }, QPen(Qt::NoPen), QBrush(Qt::NoBrush));
    icon.painter([this](QCPPainter& painter, const QCPTransform& transform)
                 { paintLegendIcon(&painter, unitBoxPixels(transform)); });
}

//! The icon's painter code, in \a rect.
void QCPFinancial::paintLegendIcon(QCPPainter* painter, const QRectF& rect) const
{
    painter->setAntialiasing(
        false); // legend icon especially of csCandlestick looks better without antialiasing
    if (mChartStyle == csOhlc)
    {
        if (mTwoColored)
        {
            // draw upper left half icon with positive color:
            painter->setBrush(mBrushPositive);
            painter->setPen(mPenPositive);
            painter->setClipRegion(QRegion(QPolygon() << rect.bottomLeft().toPoint()
                                                      << rect.topRight().toPoint()
                                                      << rect.topLeft().toPoint()));
            painter->drawLine(QLineF(0, rect.height() * 0.5, rect.width(), rect.height() * 0.5)
                                  .translated(rect.topLeft()));
            painter->drawLine(QLineF(rect.width() * 0.2, rect.height() * 0.3, rect.width() * 0.2,
                                     rect.height() * 0.5)
                                  .translated(rect.topLeft()));
            painter->drawLine(QLineF(rect.width() * 0.8, rect.height() * 0.5, rect.width() * 0.8,
                                     rect.height() * 0.7)
                                  .translated(rect.topLeft()));
            // draw bottom right half icon with negative color:
            painter->setBrush(mBrushNegative);
            painter->setPen(mPenNegative);
            painter->setClipRegion(QRegion(QPolygon() << rect.bottomLeft().toPoint()
                                                      << rect.topRight().toPoint()
                                                      << rect.bottomRight().toPoint()));
            painter->drawLine(QLineF(0, rect.height() * 0.5, rect.width(), rect.height() * 0.5)
                                  .translated(rect.topLeft()));
            painter->drawLine(QLineF(rect.width() * 0.2, rect.height() * 0.3, rect.width() * 0.2,
                                     rect.height() * 0.5)
                                  .translated(rect.topLeft()));
            painter->drawLine(QLineF(rect.width() * 0.8, rect.height() * 0.5, rect.width() * 0.8,
                                     rect.height() * 0.7)
                                  .translated(rect.topLeft()));
        }
        else
        {
            painter->setBrush(mBrush);
            painter->setPen(mPen);
            painter->drawLine(QLineF(0, rect.height() * 0.5, rect.width(), rect.height() * 0.5)
                                  .translated(rect.topLeft()));
            painter->drawLine(QLineF(rect.width() * 0.2, rect.height() * 0.3, rect.width() * 0.2,
                                     rect.height() * 0.5)
                                  .translated(rect.topLeft()));
            painter->drawLine(QLineF(rect.width() * 0.8, rect.height() * 0.5, rect.width() * 0.8,
                                     rect.height() * 0.7)
                                  .translated(rect.topLeft()));
        }
    }
    else if (mChartStyle == csCandlestick)
    {
        if (mTwoColored)
        {
            // draw upper left half icon with positive color:
            painter->setBrush(mBrushPositive);
            painter->setPen(mPenPositive);
            painter->setClipRegion(QRegion(QPolygon() << rect.bottomLeft().toPoint()
                                                      << rect.topRight().toPoint()
                                                      << rect.topLeft().toPoint()));
            painter->drawLine(
                QLineF(0, rect.height() * 0.5, rect.width() * 0.25, rect.height() * 0.5)
                    .translated(rect.topLeft()));
            painter->drawLine(
                QLineF(rect.width() * 0.75, rect.height() * 0.5, rect.width(), rect.height() * 0.5)
                    .translated(rect.topLeft()));
            painter->drawRect(QRectF(rect.width() * 0.25, rect.height() * 0.25, rect.width() * 0.5,
                                     rect.height() * 0.5)
                                  .translated(rect.topLeft()));
            // draw bottom right half icon with negative color:
            painter->setBrush(mBrushNegative);
            painter->setPen(mPenNegative);
            painter->setClipRegion(QRegion(QPolygon() << rect.bottomLeft().toPoint()
                                                      << rect.topRight().toPoint()
                                                      << rect.bottomRight().toPoint()));
            painter->drawLine(
                QLineF(0, rect.height() * 0.5, rect.width() * 0.25, rect.height() * 0.5)
                    .translated(rect.topLeft()));
            painter->drawLine(
                QLineF(rect.width() * 0.75, rect.height() * 0.5, rect.width(), rect.height() * 0.5)
                    .translated(rect.topLeft()));
            painter->drawRect(QRectF(rect.width() * 0.25, rect.height() * 0.25, rect.width() * 0.5,
                                     rect.height() * 0.5)
                                  .translated(rect.topLeft()));
        }
        else
        {
            painter->setBrush(mBrush);
            painter->setPen(mPen);
            painter->drawLine(
                QLineF(0, rect.height() * 0.5, rect.width() * 0.25, rect.height() * 0.5)
                    .translated(rect.topLeft()));
            painter->drawLine(
                QLineF(rect.width() * 0.75, rect.height() * 0.5, rect.width(), rect.height() * 0.5)
                    .translated(rect.topLeft()));
            painter->drawRect(QRectF(rect.width() * 0.25, rect.height() * 0.25, rect.width() * 0.5,
                                     rect.height() * 0.5)
                                  .translated(rect.topLeft()));
        }
    }
}

/*! \internal

  Records one segment of the data container, with the pen and brush its entries draw with: the
  selection decorator's for a selected segment.

  Only the entries whose bars touch the key window are recorded: the window reaches one view width
  past each side, so a pan inside it only shifts them, and the recording is remade once it leaves.

  A width in plot coordinates is geometry in plot coordinates, so a pan only shifts the recording. A
  width in pixels is only right for the view it was recorded in, so that recording reads the
  transform and is remade whenever the view changes.
*/
void QCPFinancial::recordSegment(QCPDrawContext& context, const QCPDataRange& segment,
                                 bool selected) const
{
    QCPAxis* keyAxis = keyAxisRaw();
    QCPAxis* valueAxis = valueAxisRaw();
    if (!keyAxis || !valueAxis)
        return;
    // 2.x centred a plot-coordinate body symmetrically in pixels. On a log key axis the two data
    // halves are not equal on screen, so such a body is recorded in pixels, as a pixel width is.
    const bool logKey = keyAxis->scaleType() == QCPAxis::stLogarithmic;
    const bool dataSpace = mWidthType == wtPlotCoords && !logKey;
    if (!dataSpace)
        (void)context.transform(); // a pixel width (or a log key axis): this geometry is this view's
    // A bar straddles its key by half its width, so the window grows by that much in keys. A window
    // is only recorded on a linear key axis, so a log one culls nothing.
    const QCPKeyWindow window = logKey ? QCPKeyWindow { } : context.keyWindow();
    const double halfPixel = logKey ? 0.0 : qAbs(getPixelWidth(window.keys.lower, 0.0));
    const double margin = dataSpace
        ? mWidth * 0.5
        : (window.pixelsPerKey > 0 ? halfPixel / window.pixelsPerKey : 0.0);
    auto begin = mDataContainer->constBegin();
    auto end = mDataContainer->constEnd();
    if (!logKey)
    {
        begin = mDataContainer->findBegin(window.keys.lower - margin);
        end = mDataContainer->findEnd(window.keys.upper + margin);
    }
    mDataContainer->limitIteratorsToDataRange(begin, end, segment);
    if (begin == end)
        return;
    // A point of the chart, in the space the width type allows.
    const auto at = [&](double key, double value) -> QCP::Position
    {
        if (dataSpace)
            return QCP::Data { key, value };
        const double keyPixel = keyAxis->coordToPixel(key);
        const double valuePixel = valueAxis->coordToPixel(value);
        return keyAxis->orientation() == Qt::Horizontal
            ? QCP::Position(QCP::Pixels { keyPixel, valuePixel })
            : QCP::Position(QCP::Pixels { valuePixel, keyPixel });
    };
    // A point of the chart, \a offset to the high-key side of \a key (in keys, or in pixels).
    const auto across = [&](double key, double value, double offset) -> QCP::Position
    {
        if (dataSpace)
            return QCP::Data { key + offset, value };
        const double keyPixel = keyAxis->coordToPixel(key) + offset;
        const double valuePixel = valueAxis->coordToPixel(value);
        return keyAxis->orientation() == Qt::Horizontal
            ? QCP::Position(QCP::Pixels { keyPixel, valuePixel })
            : QCP::Position(QCP::Pixels { valuePixel, keyPixel });
    };
    // How far the bar of \a key reaches to the high-key side: its half width, signed as the axis
    // runs, so the open and close flags sit on the sides they always did.
    const auto halfOf = [&](double key)
    { return dataSpace ? mWidth * 0.5 : getPixelWidth(key, keyAxis->coordToPixel(key)); };
    const QCP::Position gap = dataSpace ? QCP::Position(QCP::Data { kNan, kNan })
                                        : QCP::Position(QCP::Pixels { kNan, kNan });
    const QPen pen = selected && mSelectionDecorator ? mSelectionDecorator->pen() : mPen;
    const QBrush brush = selected && mSelectionDecorator ? mSelectionDecorator->brush() : mBrush;
    const auto penOf = [&](const QCPFinancialData& entry)
    {
        return selected || !mTwoColored ? pen
                                        : (entry.close >= entry.open ? mPenPositive : mPenNegative);
    };
    const auto brushOf = [&](const QCPFinancialData& entry)
    {
        return selected || !mTwoColored
            ? brush
            : (entry.close >= entry.open ? mBrushPositive : mBrushNegative);
    };
    const auto isDrawn = [](const QCPFinancialData& entry)
    {
        return qIsFinite(entry.key) && qIsFinite(entry.open) && qIsFinite(entry.close)
            && qIsFinite(entry.high) && qIsFinite(entry.low);
    };

    if (mChartStyle == csCandlestick)
    {
        for (auto it = begin; it != end; ++it)
        {
            const QCPFinancialData& entry = *it;
            if (!isDrawn(entry))
                continue;
            // The wick is two segments, ending where the body starts: it does not run behind it.
            const std::array<QCP::Position, 5> wick {
                at(entry.key, entry.high), at(entry.key, qMax(entry.open, entry.close)), gap,
                at(entry.key, entry.low), at(entry.key, qMin(entry.open, entry.close))
            };
            context.polyline(std::span<const QCP::Position>(wick), penOf(entry));
            const double half = halfOf(entry.key);
            context.rect(across(entry.key, entry.open, half), across(entry.key, entry.close, -half),
                         penOf(entry), brushOf(entry));
        }
        return;
    }

    // OHLC: one polyline per colour, so a segment of ten thousand entries is one or two commands. A
    // stroke drawn over another stroke of the same pen looks the same whichever came first.
    std::vector<QCP::Position> lines[2];
    for (auto it = begin; it != end; ++it)
    {
        const QCPFinancialData& entry = *it;
        if (!isDrawn(entry))
            continue;
        const double half = halfOf(entry.key);
        std::vector<QCP::Position>& line = lines[entry.close >= entry.open ? 0 : 1];
        line.push_back(at(entry.key, entry.high)); // backbone
        line.push_back(at(entry.key, entry.low));
        line.push_back(gap);
        line.push_back(across(entry.key, entry.open, -half)); // open flag
        line.push_back(at(entry.key, entry.open));
        line.push_back(gap);
        line.push_back(at(entry.key, entry.close)); // close flag
        line.push_back(across(entry.key, entry.close, half));
        line.push_back(gap);
    }
    const bool coloured = mTwoColored && !selected;
    const QPen groupPen[2] = { coloured ? mPenPositive : pen, coloured ? mPenNegative : pen };
    for (int group = 0; group < 2; ++group)
    {
        if (lines[group].empty())
            continue;
        context.polyline(std::span<const QCP::Position>(lines[group]), groupPen[group]);
    }
}

/*! \internal

  This function is used to determine the width of the bar at coordinate \a key, according to the
  specified width (\ref setWidth) and width type (\ref setWidthType). Provide the pixel position of
  \a key in \a keyPixel (because usually this was already calculated via \ref QCPAxis::coordToPixel
  when this function is called).

  It returns the number of pixels the bar extends to higher keys, relative to the \a key
  coordinate. So with a non-reversed horizontal axis, the return value is positive. With a reversed
  horizontal axis, the return value is negative. This is important so the open/close flags on the
  \ref csOhlc bar are drawn to the correct side.
*/
double QCPFinancial::getPixelWidth(double key, double keyPixel) const
{
    double result = 0;
    switch (mWidthType)
    {
        case wtAbsolute:
        {
            if (keyAxisRaw())
                result = mWidth * 0.5 * keyAxisRaw()->pixelOrientation();
            break;
        }
        case wtAxisRectRatio:
        {
            if (keyAxisRaw() && axisRectOf(keyAxisRaw()))
            {
                if (keyAxisRaw()->orientation() == Qt::Horizontal)
                    result = axisRectOf(keyAxisRaw())->width() * mWidth * 0.5
                        * keyAxisRaw()->pixelOrientation();
                else
                    result = axisRectOf(keyAxisRaw())->height() * mWidth * 0.5
                        * keyAxisRaw()->pixelOrientation();
            }
            break;
        }
        case wtPlotCoords:
        {
            if (keyAxisRaw())
                result = keyAxisRaw()->coordToPixel(key + mWidth * 0.5) - keyPixel;
            break;
        }
    }
    return result;
}

/*! \internal

  This method is a helper function for \ref selectTest. It is used to test for selection when the
  chart style is \ref csOhlc. It only tests against the data points between \a begin and \a end.

  Like \ref selectTest, this method returns the shortest distance of \a pos to the graphical
  representation of the plottable, and \a closestDataPoint will point to the respective data point.
*/
double
QCPFinancial::ohlcSelectTest(const QPointF& pos,
                             const QCPFinancialDataContainer::const_iterator& begin,
                             const QCPFinancialDataContainer::const_iterator& end,
                             QCPFinancialDataContainer::const_iterator& closestDataPoint) const
{
    closestDataPoint = mDataContainer->constEnd();
    QCPAxis* keyAxis = keyAxisRaw();
    QCPAxis* valueAxis = valueAxisRaw();
    if (!keyAxis || !valueAxis)
    {
        return -1;
    }

    double minDistSqr = (std::numeric_limits<double>::max)();
    if (keyAxis->orientation() == Qt::Horizontal)
    {
        for (QCPFinancialDataContainer::const_iterator it = begin; it != end; ++it)
        {
            double keyPixel = keyAxis->coordToPixel(it->key);
            // calculate distance to backbone:
            double currentDistSqr = QCPVector2D(pos).distanceSquaredToLine(
                QCPVector2D(keyPixel, valueAxis->coordToPixel(it->high)),
                QCPVector2D(keyPixel, valueAxis->coordToPixel(it->low)));
            if (currentDistSqr < minDistSqr)
            {
                minDistSqr = currentDistSqr;
                closestDataPoint = it;
            }
        }
    }
    else // keyAxis->orientation() == Qt::Vertical
    {
        for (QCPFinancialDataContainer::const_iterator it = begin; it != end; ++it)
        {
            double keyPixel = keyAxis->coordToPixel(it->key);
            // calculate distance to backbone:
            double currentDistSqr = QCPVector2D(pos).distanceSquaredToLine(
                QCPVector2D(valueAxis->coordToPixel(it->high), keyPixel),
                QCPVector2D(valueAxis->coordToPixel(it->low), keyPixel));
            if (currentDistSqr < minDistSqr)
            {
                minDistSqr = currentDistSqr;
                closestDataPoint = it;
            }
        }
    }
    return qSqrt(minDistSqr);
}

/*! \internal

  This method is a helper function for \ref selectTest. It is used to test for selection when the
  chart style is \ref csCandlestick. It only tests against the data points between \a begin and \a
  end.

  Like \ref selectTest, this method returns the shortest distance of \a pos to the graphical
  representation of the plottable, and \a closestDataPoint will point to the respective data point.
*/
double QCPFinancial::candlestickSelectTest(
    const QPointF& pos, const QCPFinancialDataContainer::const_iterator& begin,
    const QCPFinancialDataContainer::const_iterator& end,
    QCPFinancialDataContainer::const_iterator& closestDataPoint) const
{
    closestDataPoint = mDataContainer->constEnd();
    QCPAxis* keyAxis = keyAxisRaw();
    QCPAxis* valueAxis = valueAxisRaw();
    if (!keyAxis || !valueAxis)
    {
        return -1;
    }

    double minDistSqr = (std::numeric_limits<double>::max)();
    if (keyAxis->orientation() == Qt::Horizontal)
    {
        for (QCPFinancialDataContainer::const_iterator it = begin; it != end; ++it)
        {
            double currentDistSqr;
            // determine whether pos is in open-close-box:
            QCPRange boxKeyRange(it->key - mWidth * 0.5, it->key + mWidth * 0.5);
            QCPRange boxValueRange(it->close, it->open);
            double posKey, posValue;
            pixelsToCoords(pos, posKey, posValue);
            if (boxKeyRange.contains(posKey)
                && boxValueRange.contains(posValue)) // is in open-close-box
            {
                currentDistSqr = mParentPlot->selectionTolerance() * 0.99
                    * mParentPlot->selectionTolerance() * 0.99;
            }
            else
            {
                // calculate distance to high/low lines:
                double keyPixel = keyAxis->coordToPixel(it->key);
                double highLineDistSqr = QCPVector2D(pos).distanceSquaredToLine(
                    QCPVector2D(keyPixel, valueAxis->coordToPixel(it->high)),
                    QCPVector2D(keyPixel, valueAxis->coordToPixel(qMax(it->open, it->close))));
                double lowLineDistSqr = QCPVector2D(pos).distanceSquaredToLine(
                    QCPVector2D(keyPixel, valueAxis->coordToPixel(it->low)),
                    QCPVector2D(keyPixel, valueAxis->coordToPixel(qMin(it->open, it->close))));
                currentDistSqr = qMin(highLineDistSqr, lowLineDistSqr);
            }
            if (currentDistSqr < minDistSqr)
            {
                minDistSqr = currentDistSqr;
                closestDataPoint = it;
            }
        }
    }
    else // keyAxis->orientation() == Qt::Vertical
    {
        for (QCPFinancialDataContainer::const_iterator it = begin; it != end; ++it)
        {
            double currentDistSqr;
            // determine whether pos is in open-close-box:
            QCPRange boxKeyRange(it->key - mWidth * 0.5, it->key + mWidth * 0.5);
            QCPRange boxValueRange(it->close, it->open);
            double posKey, posValue;
            pixelsToCoords(pos, posKey, posValue);
            if (boxKeyRange.contains(posKey)
                && boxValueRange.contains(posValue)) // is in open-close-box
            {
                currentDistSqr = mParentPlot->selectionTolerance() * 0.99
                    * mParentPlot->selectionTolerance() * 0.99;
            }
            else
            {
                // calculate distance to high/low lines:
                double keyPixel = keyAxis->coordToPixel(it->key);
                double highLineDistSqr = QCPVector2D(pos).distanceSquaredToLine(
                    QCPVector2D(valueAxis->coordToPixel(it->high), keyPixel),
                    QCPVector2D(valueAxis->coordToPixel(qMax(it->open, it->close)), keyPixel));
                double lowLineDistSqr = QCPVector2D(pos).distanceSquaredToLine(
                    QCPVector2D(valueAxis->coordToPixel(it->low), keyPixel),
                    QCPVector2D(valueAxis->coordToPixel(qMin(it->open, it->close)), keyPixel));
                currentDistSqr = qMin(highLineDistSqr, lowLineDistSqr);
            }
            if (currentDistSqr < minDistSqr)
            {
                minDistSqr = currentDistSqr;
                closestDataPoint = it;
            }
        }
    }
    return qSqrt(minDistSqr);
}

/*! \internal

  called by the drawing methods to determine which data (key) range is visible at the current key
  axis range setting, so only that needs to be processed.

  \a begin returns an iterator to the lowest data point that needs to be taken into account when
  plotting. Note that in order to get a clean plot all the way to the edge of the axis rect, \a
  begin may still be just outside the visible range.

  \a end returns the iterator just above the highest data point that needs to be taken into
  account. Same as before, \a end may also lie just outside of the visible range

  if the plottable contains no data, both \a begin and \a end point to \c constEnd.
*/
void QCPFinancial::getVisibleDataBounds(QCPFinancialDataContainer::const_iterator& begin,
                                        QCPFinancialDataContainer::const_iterator& end) const
{
    if (!keyAxisRaw())
    {
        begin = mDataContainer->constEnd();
        end = mDataContainer->constEnd();
        return;
    }
    begin = mDataContainer->findBegin(keyAxisRaw()->range().lower
                                      - mWidth * 0.5); // subtract half width of ohlc/candlestick to
                                                       // include partially visible data points
    end = mDataContainer->findEnd(
        keyAxisRaw()->range().upper
        + mWidth
            * 0.5); // add half width of ohlc/candlestick to include partially visible data points
}

/*!  \internal

  Returns the hit box in pixel coordinates that will be used for data selection with the selection
  rect (\ref selectTestRect), of the data point given by \a it.
*/
QRectF QCPFinancial::selectionHitBox(QCPFinancialDataContainer::const_iterator it) const
{
    QCPAxis* keyAxis = keyAxisRaw();
    QCPAxis* valueAxis = valueAxisRaw();
    if (!keyAxis || !valueAxis)
    {
        return {};
    }

    double keyPixel = keyAxis->coordToPixel(it->key);
    double highPixel = valueAxis->coordToPixel(it->high);
    double lowPixel = valueAxis->coordToPixel(it->low);
    double keyWidthPixels = keyPixel - keyAxis->coordToPixel(it->key - mWidth * 0.5);
    if (keyAxis->orientation() == Qt::Horizontal)
        return QRectF(keyPixel - keyWidthPixels, highPixel, keyWidthPixels * 2,
                      lowPixel - highPixel)
            .normalized();
    else
        return QRectF(highPixel, keyPixel - keyWidthPixels, lowPixel - highPixel,
                      keyWidthPixels * 2)
            .normalized();
}
