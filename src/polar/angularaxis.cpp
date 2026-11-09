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

#include "angularaxis.h"
#include "polar-area.h"
#include "polargrid.h"
#include "radialaxis.h"

#include "../axis/axistickerfixed.h"
#include "../axis/labelpainter.h"
#include "../core.h"
#include "../items/item.h"
#include "../painting/painter.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPPolarAxisAngular
////////////////////////////////////////////////////////////////////////////////////////////////////

/*! \class QCPPolarAxisAngular
  \brief The angular axis of a polar plot, drawn as a circle around the viewport centre

  The axis belongs to a \ref QCPPolarArea, which owns its geometry and interactions, and draws it
  as one of its layerables.

  \warning In this QCustomPlot version, polar plots are a tech preview. Expect documentation and
  functionality to be incomplete, as well as changing public interfaces in the future.
*/
/*!
  Creates the angular axis belonging to \a area.
*/
QCPPolarAxisAngular::QCPPolarAxisAngular(QCPPolarArea* area)
        : QCPLayerable(area->parentPlot(), QString(), area)
        , mRangeDrag(false)
        , mRangeZoom(false)
        , mRangeZoomFactor(0.85)
        ,
        // axis base:
        mAngle(-90)
        , mAngleRad(mAngle / 180.0 * M_PI)
        , mSelectableParts(spAxis | spTickLabels | spAxisLabel)
        , mSelectedParts(spNone)
        , mBasePen(QPen(Qt::black, 0, Qt::SolidLine, Qt::SquareCap))
        , mSelectedBasePen(QPen(Qt::blue, 2))
        ,
        // axis label:
        mLabelPadding(0)
        , mLabel()
        , mLabelFont(mParentPlot->font())
        , mSelectedLabelFont(QFont(mLabelFont.family(), mLabelFont.pointSize(), QFont::Bold))
        , mLabelColor(Qt::black)
        , mSelectedLabelColor(Qt::blue)
        ,
        // tick labels:
        // mTickLabelPadding(0), in label painter
        mTickLabels(true)
        ,
        // mTickLabelRotation(0), in label painter
        mTickLabelFont(mParentPlot->font())
        , mSelectedTickLabelFont(
              QFont(mTickLabelFont.family(), mTickLabelFont.pointSize(), QFont::Bold))
        , mTickLabelColor(Qt::black)
        , mSelectedTickLabelColor(Qt::blue)
        , mNumberPrecision(6)
        , mNumberFormatChar('g')
        , mNumberBeautifulPowers(true)
        , mNumberMultiplyCross(false)
        ,
        // ticks and subticks:
        mTicks(true)
        , mSubTicks(true)
        , mTickLengthIn(5)
        , mTickLengthOut(0)
        , mSubTickLengthIn(2)
        , mSubTickLengthOut(0)
        , mTickPen(QPen(Qt::black, 0, Qt::SolidLine, Qt::SquareCap))
        , mSelectedTickPen(QPen(Qt::blue, 2))
        , mSubTickPen(QPen(Qt::black, 0, Qt::SolidLine, Qt::SquareCap))
        , mSelectedSubTickPen(QPen(Qt::blue, 2))
        ,
        // scale and range:
        mRange(0, 360)
        , mRangeReversed(false)
        ,
        // internal members:
        mArea(area)
        , mTicker(new QCPAxisTickerFixed)
        , mLabelPainter(mParentPlot)
{
    if (QCPAxisTickerFixed* fixedTicker = mTicker.dynamicCast<QCPAxisTickerFixed>().data())
    {
        fixedTicker->setTickStep(30);
    }
    setAntialiased(true);

    setTickLabelPadding(5);
    setTickLabelRotation(0);
    setTickLabelMode(lmUpright);
    mLabelPainter.setAnchorReferenceType(QCPLabelPainterPrivate::artNormal);
    mLabelPainter.setAbbreviateDecimalPowers(false);
    mLabelPainter.setCacheSize(24); // so we can cache up to 15-degree intervals, polar angular axis
                                    // uses a bit larger cache than normal axes
}

QCPPolarAxisAngular::LabelMode QCPPolarAxisAngular::tickLabelMode() const
{
    switch (mLabelPainter.anchorMode())
    {
        case QCPLabelPainterPrivate::amSkewedUpright:
            return lmUpright;
        case QCPLabelPainterPrivate::amSkewedRotated:
            return lmRotated;
        default:
            qDebug() << Q_FUNC_INFO << "invalid mode for polar axis";
            break;
    }
    return lmUpright;
}

/* No documentation as it is a property getter */
QString QCPPolarAxisAngular::numberFormat() const
{
    QString result;
    result.append(mNumberFormatChar);
    if (mNumberBeautifulPowers)
    {
        result.append(QLatin1Char('b'));
        if (mLabelPainter.multiplicationSymbol() == QCPLabelPainterPrivate::SymbolCross)
            result.append(QLatin1Char('c'));
    }
    return result;
}

/*!
  If the scale type (\ref setScaleType) is \ref stLinear, \a diff is added to the lower and upper
  bounds of the range. The range is simply moved by \a diff.

  If the scale type is \ref stLogarithmic, the range bounds are multiplied by \a diff. This
  corresponds to an apparent "linear" move in logarithmic scaling by a distance of log(diff).
*/
void QCPPolarAxisAngular::moveRange(double diff)
{
    QCPRange oldRange = mRange;
    mRange.lower += diff;
    mRange.upper += diff;
    emitRangeChanged(oldRange);
}

/*!
  Scales the range of this axis by \a factor around the center of the current axis range. For
  example, if \a factor is 2.0, then the axis range will double its size, and the point at the axis
  range center won't have changed its position in the QCustomPlot widget (i.e. coordinates around
  the center will have moved symmetrically closer).

  If you wish to scale around a different coordinate than the current axis range center, use the
  overload \ref scaleRange(double factor, double center).
*/
void QCPPolarAxisAngular::scaleRange(double factor)
{
    scaleRange(factor, range().center());
}

/*! \overload

  Scales the range of this axis by \a factor around the coordinate \a center. For example, if \a
  factor is 2.0, \a center is 1.0, then the axis range will double its size, and the point at
  coordinate 1.0 won't have changed its position in the QCustomPlot widget (i.e. coordinates
  around 1.0 will have moved symmetrically closer to 1.0).

  \see scaleRange(double factor)
*/
void QCPPolarAxisAngular::scaleRange(double factor, double center)
{
    QCPRange oldRange = mRange;
    QCPRange newRange;
    newRange.lower = (mRange.lower - center) * factor + center;
    newRange.upper = (mRange.upper - center) * factor + center;
    if (QCPRange::validRange(newRange))
        mRange = newRange.sanitizedForLinScale();
    emitRangeChanged(oldRange);
}

/*!
  Transforms \a value, in pixel coordinates of the QCustomPlot widget, to axis coordinates.
*/
void QCPPolarAxisAngular::pixelToCoord(QPointF pixelPos, double& angleCoord,
                                       double& radiusCoord) const
{
    if (mArea->radialAxisCount() > 0)
        mArea->radialAxis()->pixelToCoord(pixelPos, angleCoord, radiusCoord);
    else
        qDebug() << Q_FUNC_INFO << "no radial axis configured";
}

/*!
  Transforms \a value, in coordinates of the axis, to pixel coordinates of the QCustomPlot widget.
*/
QPointF QCPPolarAxisAngular::coordToPixel(double angleCoord, double radiusCoord) const
{
    if (mArea->radialAxisCount() > 0)
    {
        return mArea->radialAxis()->coordToPixel(angleCoord, radiusCoord);
    }
    else
    {
        qDebug() << Q_FUNC_INFO << "no radial axis configured";
        return QPointF();
    }
}

/*!
  Returns the part of the axis that is hit by \a pos (in pixels). The return value of this function
  is independent of the user-selectable parts defined with \ref setSelectableParts. Further, this
  function does not change the current selection state of the axis.

  If the axis is not visible (\ref setVisible), this function always returns \ref spNone.

  \see setSelectedParts, setSelectableParts, QCustomPlot::setInteractions
*/
QCPPolarAxisAngular::SelectablePart
QCPPolarAxisAngular::getPartAt([[maybe_unused]] const QPointF& pos) const
{
    // TODO remove later

    if (!mVisible)
        return spNone;

    /*
      TODO:
    if (mAxisPainter->axisSelectionBox().contains(pos.toPoint()))
      return spAxis;
    else if (mAxisPainter->tickLabelsSelectionBox().contains(pos.toPoint()))
      return spTickLabels;
    else if (mAxisPainter->labelSelectionBox().contains(pos.toPoint()))
      return spAxisLabel;
    else */
    return spNone;
}

/* inherits documentation from base class */
void QCPPolarAxisAngular::applyDefaultAntialiasingHint(QCPPainter* painter) const
{
    applyAntialiasingHint(painter, mAntialiased, QCP::aeAxes);
}

/* inherits documentation from base class */
void QCPPolarAxisAngular::draw(QCPPainter* painter)
{
    const QPointF center = mArea->center();
    const double radius = mArea->radius();

    // draw baseline circle:
    painter->setPen(getBasePen());
    painter->drawEllipse(center, radius, radius);

    // draw subticks:
    if (!mSubTickVector.isEmpty())
    {
        painter->setPen(getSubTickPen());
        for (int i = 0; i < mSubTickVector.size(); ++i)
        {
            painter->drawLine(center + mSubTickVectorCosSin.at(i) * (radius - mSubTickLengthIn),
                              center + mSubTickVectorCosSin.at(i) * (radius + mSubTickLengthOut));
        }
    }

    // draw ticks and labels:
    if (!mTickVector.isEmpty())
    {
        mLabelPainter.setAnchorReference(center);
        mLabelPainter.setFont(getTickLabelFont());
        mLabelPainter.setColor(getTickLabelColor());
        const QPen ticksPen = getTickPen();
        painter->setPen(ticksPen);
        for (int i = 0; i < mTickVector.size(); ++i)
        {
            const QPointF outerTick
                = center + mTickVectorCosSin.at(i) * (radius + mTickLengthOut);
            painter->drawLine(center + mTickVectorCosSin.at(i) * (radius - mTickLengthIn),
                              outerTick);
            // draw tick labels:
            if (!mTickVectorLabels.isEmpty())
            {
                if (i < mTickVectorLabels.count() - 1
                    || (mTickVectorCosSin.at(i) - mTickVectorCosSin.first()).manhattanLength() > 5
                            / 180.0
                            * M_PI) // skip last label if it's closer than approx 5 degrees to first
                    mLabelPainter.drawTickLabel(painter, outerTick, mTickVectorLabels.at(i));
            }
        }
    }
}

void QCPPolarAxisAngular::setRangeDrag(bool enabled)
{
    mRangeDrag = enabled;
}

void QCPPolarAxisAngular::setRangeZoom(bool enabled)
{
    mRangeZoom = enabled;
}

void QCPPolarAxisAngular::setRangeZoomFactor(double factor)
{
    mRangeZoomFactor = factor;
}

/*!
  Sets the range of the axis.

  This slot may be connected with the \ref rangeChanged signal of another axis so this axis
  is always synchronized with the other axis range, when it changes.

  To invert the direction of an axis, use \ref setRangeReversed.
*/
void QCPPolarAxisAngular::setRange(const QCPRange& range)
{
    if (range.lower == mRange.lower && range.upper == mRange.upper)
        return;

    if (!QCPRange::validRange(range))
        return;
    markDirty();
    QCPRange oldRange = mRange;
    mRange = range.sanitizedForLinScale();
    emitRangeChanged(oldRange);
}

/*!
  Sets whether the user can (de-)select the parts in \a selectable by clicking on the QCustomPlot
  surface. (When \ref QCustomPlot::setInteractions contains iSelectAxes.)

  However, even when \a selectable is set to a value not allowing the selection of a specific part,
  it is still possible to set the selection of this part manually, by calling \ref setSelectedParts
  directly.

  \see SelectablePart, setSelectedParts
*/
void QCPPolarAxisAngular::setSelectableParts(const SelectableParts& selectable)
{
    if (mSelectableParts != selectable)
    {
        mSelectableParts = selectable;
        qcpEmit(this, mParentPlot, &QCPPolarAxisAngular::selectableChanged);
    }
}

/*!
  Sets the selected state of the respective axis parts described by \ref SelectablePart. When a part
  is selected, it uses a different pen/font.

  The entire selection mechanism for axes is handled automatically when \ref
  QCustomPlot::setInteractions contains iSelectAxes. You only need to call this function when you
  wish to change the selection state manually.

  This function can change the selection state of a part, independent of the \ref setSelectableParts
  setting.

  emits the \ref selectionChanged signal when \a selected is different from the previous selection
  state.

  \see SelectablePart, setSelectableParts, selectTest, setSelectedBasePen, setSelectedTickPen,
  setSelectedSubTickPen, setSelectedTickLabelFont, setSelectedLabelFont, setSelectedTickLabelColor,
  setSelectedLabelColor
*/
void QCPPolarAxisAngular::setSelectedParts(const SelectableParts& selected)
{
    if (mSelectedParts != selected)
    {
        markDirty();
        mSelectedParts = selected;
        qcpEmit(this, mParentPlot, &QCPPolarAxisAngular::selectionChanged);
    }
}

/*!
  \overload

  Sets the lower and upper bound of the axis range.

  To invert the direction of an axis, use \ref setRangeReversed.

  There is also a slot to set a range, see \ref setRange(const QCPRange &range).
*/
void QCPPolarAxisAngular::setRange(double lower, double upper)
{
    if (lower == mRange.lower && upper == mRange.upper)
        return;

    if (!QCPRange::validRange(lower, upper))
        return;
    markDirty();
    QCPRange oldRange = mRange;
    mRange.lower = lower;
    mRange.upper = upper;
    mRange = mRange.sanitizedForLinScale();
    emitRangeChanged(oldRange);
}

/*!
  \overload

  Sets the range of the axis.

  The \a position coordinate indicates together with the \a alignment parameter, where the new
  range will be positioned. \a size defines the size of the new axis range. \a alignment may be
  Qt::AlignLeft, Qt::AlignRight or Qt::AlignCenter. This will cause the left border, right border,
  or center of the range to be aligned with \a position. Any other values of \a alignment will
  default to Qt::AlignCenter.
*/
void QCPPolarAxisAngular::setRange(double position, double size, Qt::AlignmentFlag alignment)
{
    // each branch delegates to setRange(double, double), which dirties
    if (alignment == Qt::AlignLeft)
        setRange(position, position + size);
    else if (alignment == Qt::AlignRight)
        setRange(position - size, position);
    else // alignment == Qt::AlignCenter
        setRange(position - size / 2.0, position + size / 2.0);
}

/*!
  Sets the lower bound of the axis range. The upper bound is not changed.
  \see setRange
*/
void QCPPolarAxisAngular::setRangeLower(double lower)
{
    if (mRange.lower == lower)
        return;

    markDirty();
    QCPRange oldRange = mRange;
    mRange.lower = lower;
    mRange = mRange.sanitizedForLinScale();
    emitRangeChanged(oldRange);
}

/*!
  Sets the upper bound of the axis range. The lower bound is not changed.
  \see setRange
*/
void QCPPolarAxisAngular::setRangeUpper(double upper)
{
    if (mRange.upper == upper)
        return;

    markDirty();
    QCPRange oldRange = mRange;
    mRange.upper = upper;
    mRange = mRange.sanitizedForLinScale();
    emitRangeChanged(oldRange);
}

/*!
  Sets whether the axis range (direction) is displayed reversed. Normally, the values on horizontal
  axes increase left to right, on vertical axes bottom to top. When \a reversed is set to true, the
  direction of increasing values is inverted.

  Note that the range and data interface stays the same for reversed axes, e.g. the \a lower part
  of the \ref setRange interface will still reference the mathematically smaller number than the \a
  upper part.
*/
void QCPPolarAxisAngular::setRangeReversed(bool reversed)
{
    if (reversed == mRangeReversed)
        return;
    markDirty();
    mRangeReversed = reversed;
}

void QCPPolarAxisAngular::setAngle(double degrees)
{
    if (degrees == mAngle)
        return;
    markDirty();
    mAngle = degrees;
    mAngleRad = mAngle / 180.0 * M_PI;
}

/*!
  The axis ticker is responsible for generating the tick positions and tick labels. See the
  documentation of QCPAxisTicker for details on how to work with axis tickers.

  You can change the tick positioning/labeling behaviour of this axis by setting a different
  QCPAxisTicker subclass using this method. If you only wish to modify the currently installed axis
  ticker, access it via \ref ticker.

  Since the ticker is stored in the axis as a shared pointer, multiple axes may share the same axis
  ticker simply by passing the same shared pointer to multiple axes.

  \see ticker
*/
void QCPPolarAxisAngular::setTicker(QSharedPointer<QCPAxisTicker> ticker)
{
    if (!ticker)
    {
        qDebug() << Q_FUNC_INFO << "can not set 0 as axis ticker";
        return;
    }
    if (mTicker == ticker)
        return;
    markDirty();
    mTicker = ticker;
    // no need to invalidate margin cache here because produced tick labels are checked for changes
    // in setupTickVector
}

/*!
  Sets whether tick marks are displayed.

  Note that setting \a show to false does not imply that tick labels are invisible, too. To achieve
  that, see \ref setTickLabels.

  \see setSubTicks
*/
void QCPPolarAxisAngular::setTicks(bool show)
{
    if (mTicks != show)
    {
        markDirty();
        mTicks = show;
        // mCachedMarginValid = false;
    }
}

/*!
  Sets whether tick labels are displayed. Tick labels are the numbers drawn next to tick marks.
*/
void QCPPolarAxisAngular::setTickLabels(bool show)
{
    if (mTickLabels != show)
    {
        markDirty();
        mTickLabels = show;
        // mCachedMarginValid = false;
        if (!mTickLabels)
            mTickVectorLabels.clear();
    }
}

/*!
  Sets the distance between the axis base line (including any outward ticks) and the tick labels.
  \see setLabelPadding, setPadding
*/
void QCPPolarAxisAngular::setTickLabelPadding(int padding)
{
    if (mLabelPainter.padding() == padding)
        return;
    markDirty();
    mLabelPainter.setPadding(padding);
}

/*!
  Sets the font of the tick labels.

  \see setTickLabels, setTickLabelColor
*/
void QCPPolarAxisAngular::setTickLabelFont(const QFont& font)
{
    if (font == mTickLabelFont)
        return;
    markDirty();
    mTickLabelFont = font;
}

/*!
  Sets the color of the tick labels.

  \see setTickLabels, setTickLabelFont
*/
void QCPPolarAxisAngular::setTickLabelColor(const QColor& color)
{
    if (color == mTickLabelColor)
        return;
    markDirty();
    mTickLabelColor = color;
}

/*!
  Sets the rotation of the tick labels. If \a degrees is zero, the labels are drawn normally. Else,
  the tick labels are drawn rotated by \a degrees clockwise. The specified angle is bound to values
  from -90 to 90 degrees.

  If \a degrees is exactly -90, 0 or 90, the tick labels are centered on the tick coordinate. For
  other angles, the label is drawn with an offset such that it seems to point toward or away from
  the tick mark.
*/
void QCPPolarAxisAngular::setTickLabelRotation(double degrees)
{
    if (mLabelPainter.rotation() == degrees)
        return;
    markDirty();
    mLabelPainter.setRotation(degrees);
}

void QCPPolarAxisAngular::setTickLabelMode(LabelMode mode)
{
    const auto anchor = mode == lmUpright ? QCPLabelPainterPrivate::amSkewedUpright
                                          : QCPLabelPainterPrivate::amSkewedRotated;
    if (mLabelPainter.anchorMode() == anchor)
        return;
    markDirty();
    mLabelPainter.setAnchorMode(anchor);
}

/*!
  Sets the number format for the numbers in tick labels. This \a formatCode is an extended version
  of the format code used e.g. by QString::number() and QLocale::toString(). For reference about
  that, see the "Argument Formats" section in the detailed description of the QString class.

  \a formatCode is a string of one, two or three characters. The first character is identical to
  the normal format code used by Qt. In short, this means: 'e'/'E' scientific format, 'f' fixed
  format, 'g'/'G' scientific or fixed, whichever is shorter.

  The second and third characters are optional and specific to QCustomPlot:\n If the first char was
  'e' or 'g', numbers are/might be displayed in the scientific format, e.g. "5.5e9", which might be
  visually unappealing in a plot. So when the second char of \a formatCode is set to 'b' (for
  "beautiful"), those exponential numbers are formatted in a more natural way, i.e. "5.5
  [multiplication sign] 10 [superscript] 9". By default, the multiplication sign is a centered dot.
  If instead a cross should be shown (as is usual in the USA), the third char of \a formatCode can
  be set to 'c'. The inserted multiplication signs are the UTF-8 characters 215 (0xD7) for the
  cross and 183 (0xB7) for the dot.

  Examples for \a formatCode:
  \li \c g normal format code behaviour. If number is small, fixed format is used, if number is
  large, normal scientific format is used
  \li \c gb If number is small, fixed format is used, if number is large, scientific format is used
  with beautifully typeset decimal powers and a dot as multiplication sign
  \li \c ebc All numbers are in scientific format with beautifully typeset decimal power and a cross
  as multiplication sign
  \li \c fb illegal format code, since fixed format doesn't support (or need) beautifully typeset
  decimal powers. Format code will be reduced to 'f'.
  \li \c hello illegal format code, since first char is not 'e', 'E', 'f', 'g' or 'G'. Current
  format code will not be changed.
*/
void QCPPolarAxisAngular::setNumberFormat(const QString& formatCode)
{
    const QChar previousChar = mNumberFormatChar;
    const bool previousBeautiful = mNumberBeautifulPowers;
    const bool previousCross = mNumberMultiplyCross;
    if (formatCode.isEmpty())
    {
        qDebug() << Q_FUNC_INFO << "Passed formatCode is empty";
        return;
    }
    // mCachedMarginValid = false;

    // interpret first char as number format char:
    QString allowedFormatChars(QLatin1String("eEfgG"));
    if (allowedFormatChars.contains(formatCode.at(0)))
    {
        mNumberFormatChar = QLatin1Char(formatCode.at(0).toLatin1());
    }
    else
    {
        qDebug() << Q_FUNC_INFO
                 << "Invalid number format code (first char not in 'eEfgG'):" << formatCode;
        return;
    }

    if (formatCode.length() < 2)
    {
        mNumberBeautifulPowers = false;
        mNumberMultiplyCross = false;
    }
    else
    {
        // interpret second char as indicator for beautiful decimal powers:
        if (formatCode.at(1) == QLatin1Char('b')
            && (mNumberFormatChar == QLatin1Char('e') || mNumberFormatChar == QLatin1Char('g')))
            mNumberBeautifulPowers = true;
        else
            qDebug() << Q_FUNC_INFO
                     << "Invalid number format code (second char not 'b' or first char neither 'e' "
                        "nor 'g'):"
                     << formatCode;

        if (formatCode.length() < 3)
        {
            mNumberMultiplyCross = false;
        }
        else
        {
            // interpret third char as indicator for dot or cross multiplication symbol:
            if (formatCode.at(2) == QLatin1Char('c'))
                mNumberMultiplyCross = true;
            else if (formatCode.at(2) == QLatin1Char('d'))
                mNumberMultiplyCross = false;
            else
                qDebug() << Q_FUNC_INFO
                         << "Invalid number format code (third char neither 'c' nor 'd'):"
                         << formatCode;
        }
    }
    if (mNumberFormatChar == previousChar && mNumberBeautifulPowers == previousBeautiful
        && mNumberMultiplyCross == previousCross)
        return;
    markDirty();
    mLabelPainter.setSubstituteExponent(mNumberBeautifulPowers);
    mLabelPainter.setMultiplicationSymbol(mNumberMultiplyCross ? QCPLabelPainterPrivate::SymbolCross
                                                               : QCPLabelPainterPrivate::SymbolDot);
}

/*!
  Sets the precision of the tick label numbers. See QLocale::toString(double i, char f, int prec)
  for details. The effect of precisions are most notably for number Formats starting with 'e', see
  \ref setNumberFormat
*/
void QCPPolarAxisAngular::setNumberPrecision(int precision)
{
    if (mNumberPrecision == precision)
        return;
    markDirty();
    mNumberPrecision = precision;
}

/*!
  Sets the length of the ticks in pixels. \a inside is the length the ticks will reach inside the
  plot and \a outside is the length they will reach outside the plot. If \a outside is greater than
  zero, the tick labels and axis label will increase their distance to the axis accordingly, so
  they won't collide with the ticks.

  \see setSubTickLength, setTickLengthIn, setTickLengthOut
*/
void QCPPolarAxisAngular::setTickLength(int inside, int outside)
{
    setTickLengthIn(inside);
    setTickLengthOut(outside);
}

/*!
  Sets the length of the inward ticks in pixels. \a inside is the length the ticks will reach
  inside the plot.

  \see setTickLengthOut, setTickLength, setSubTickLength
*/
void QCPPolarAxisAngular::setTickLengthIn(int inside)
{
    if (mTickLengthIn != inside)
    {
        markDirty();
        mTickLengthIn = inside;
    }
}

/*!
  Sets the length of the outward ticks in pixels. \a outside is the length the ticks will reach
  outside the plot. If \a outside is greater than zero, the tick labels and axis label will
  increase their distance to the axis accordingly, so they won't collide with the ticks.

  \see setTickLengthIn, setTickLength, setSubTickLength
*/
void QCPPolarAxisAngular::setTickLengthOut(int outside)
{
    if (mTickLengthOut != outside)
    {
        markDirty();
        mTickLengthOut = outside;
        // mCachedMarginValid = false; // only outside tick length can change margin
    }
}

/*!
  Sets whether sub tick marks are displayed.

  Sub ticks are only potentially visible if (major) ticks are also visible (see \ref setTicks)

  \see setTicks
*/
void QCPPolarAxisAngular::setSubTicks(bool show)
{
    if (mSubTicks != show)
    {
        markDirty();
        mSubTicks = show;
        // mCachedMarginValid = false;
    }
}

/*!
  Sets the length of the subticks in pixels. \a inside is the length the subticks will reach inside
  the plot and \a outside is the length they will reach outside the plot. If \a outside is greater
  than zero, the tick labels and axis label will increase their distance to the axis accordingly,
  so they won't collide with the ticks.

  \see setTickLength, setSubTickLengthIn, setSubTickLengthOut
*/
void QCPPolarAxisAngular::setSubTickLength(int inside, int outside)
{
    setSubTickLengthIn(inside);
    setSubTickLengthOut(outside);
}

/*!
  Sets the length of the inward subticks in pixels. \a inside is the length the subticks will reach
  inside the plot.

  \see setSubTickLengthOut, setSubTickLength, setTickLength
*/
void QCPPolarAxisAngular::setSubTickLengthIn(int inside)
{
    if (mSubTickLengthIn != inside)
    {
        markDirty();
        mSubTickLengthIn = inside;
    }
}

/*!
  Sets the length of the outward subticks in pixels. \a outside is the length the subticks will
  reach outside the plot. If \a outside is greater than zero, the tick labels will increase their
  distance to the axis accordingly, so they won't collide with the ticks.

  \see setSubTickLengthIn, setSubTickLength, setTickLength
*/
void QCPPolarAxisAngular::setSubTickLengthOut(int outside)
{
    if (mSubTickLengthOut == outside)
        return;
    markDirty();
    mSubTickLengthOut = outside;
}

/*!
  Sets the pen, the axis base line is drawn with.

  \see setTickPen, setSubTickPen
*/
void QCPPolarAxisAngular::setBasePen(const QPen& pen)
{
    if (pen == mBasePen)
        return;
    markDirty();
    mBasePen = pen;
}

/*!
  Sets the pen, tick marks will be drawn with.

  \see setTickLength, setBasePen
*/
void QCPPolarAxisAngular::setTickPen(const QPen& pen)
{
    if (pen == mTickPen)
        return;
    markDirty();
    mTickPen = pen;
}

/*!
  Sets the pen, subtick marks will be drawn with.

  \see setSubTickCount, setSubTickLength, setBasePen
*/
void QCPPolarAxisAngular::setSubTickPen(const QPen& pen)
{
    if (pen == mSubTickPen)
        return;
    markDirty();
    mSubTickPen = pen;
}

/*!
  Sets the font of the axis label.

  \see setLabelColor
*/
void QCPPolarAxisAngular::setLabelFont(const QFont& font)
{
    if (mLabelFont != font)
    {
        markDirty();
        mLabelFont = font;
        // mCachedMarginValid = false;
    }
}

/*!
  Sets the color of the axis label.

  \see setLabelFont
*/
void QCPPolarAxisAngular::setLabelColor(const QColor& color)
{
    if (color == mLabelColor)
        return;
    markDirty();
    mLabelColor = color;
}

/*!
  Sets the text of the axis label that will be shown below/above or next to the axis, depending on
  its orientation. To disable axis labels, pass an empty string as \a str.
*/
void QCPPolarAxisAngular::setLabel(const QString& str)
{
    if (mLabel != str)
    {
        markDirty();
        mLabel = str;
        // mCachedMarginValid = false;
    }
}

/*!
  Sets the distance between the tick labels and the axis label.

  \see setTickLabelPadding, setPadding
*/
void QCPPolarAxisAngular::setLabelPadding(int padding)
{
    if (mLabelPadding != padding)
    {
        markDirty();
        mLabelPadding = padding;
        // mCachedMarginValid = false;
    }
}

/*!
  Sets the font that is used for tick labels when they are selected.

  \see setTickLabelFont, setSelectableParts, setSelectedParts, QCustomPlot::setInteractions
*/
void QCPPolarAxisAngular::setSelectedTickLabelFont(const QFont& font)
{
    if (font != mSelectedTickLabelFont)
    {
        markDirty();
        mSelectedTickLabelFont = font;
        // don't set mCachedMarginValid to false here because margin calculation is always done with
        // non-selected fonts
    }
}

/*!
  Sets the font that is used for the axis label when it is selected.

  \see setLabelFont, setSelectableParts, setSelectedParts, QCustomPlot::setInteractions
*/
void QCPPolarAxisAngular::setSelectedLabelFont(const QFont& font)
{
    if (font == mSelectedLabelFont)
        return;
    markDirty();
    mSelectedLabelFont = font;
}

/*!
  Sets the color that is used for tick labels when they are selected.

  \see setTickLabelColor, setSelectableParts, setSelectedParts, QCustomPlot::setInteractions
*/
void QCPPolarAxisAngular::setSelectedTickLabelColor(const QColor& color)
{
    if (color != mSelectedTickLabelColor)
    {
        markDirty();
        mSelectedTickLabelColor = color;
    }
}

/*!
  Sets the color that is used for the axis label when it is selected.

  \see setLabelColor, setSelectableParts, setSelectedParts, QCustomPlot::setInteractions
*/
void QCPPolarAxisAngular::setSelectedLabelColor(const QColor& color)
{
    if (color == mSelectedLabelColor)
        return;
    markDirty();
    mSelectedLabelColor = color;
}

/*!
  Sets the pen that is used to draw the axis base line when selected.

  \see setBasePen, setSelectableParts, setSelectedParts, QCustomPlot::setInteractions
*/
void QCPPolarAxisAngular::setSelectedBasePen(const QPen& pen)
{
    if (pen == mSelectedBasePen)
        return;
    markDirty();
    mSelectedBasePen = pen;
}

/*!
  Sets the pen that is used to draw the (major) ticks when selected.

  \see setTickPen, setSelectableParts, setSelectedParts, QCustomPlot::setInteractions
*/
void QCPPolarAxisAngular::setSelectedTickPen(const QPen& pen)
{
    if (pen == mSelectedTickPen)
        return;
    markDirty();
    mSelectedTickPen = pen;
}

/*!
  Sets the pen that is used to draw the subticks when selected.

  \see setSubTickPen, setSelectableParts, setSelectedParts, QCustomPlot::setInteractions
*/
void QCPPolarAxisAngular::setSelectedSubTickPen(const QPen& pen)
{
    if (pen == mSelectedSubTickPen)
        return;
    markDirty();
    mSelectedSubTickPen = pen;
}

/*! \internal

  Prepares the internal tick vector, sub tick vector and tick label vector. This is done by calling
  QCPAxisTicker::generate on the currently installed ticker.

  If a change in the label text/count is detected, the cached axis margin is invalidated to make
  sure the next margin calculation recalculates the label sizes and returns an up-to-date value.
*/
void QCPPolarAxisAngular::setupTickVectors()
{
    if (!mParentPlot)
        return;
    if ((!mTicks && !mTickLabels && !mArea->grid()->visible()) || mRange.size() <= 0)
        return;

    mSubTickVector.clear(); // since we might not pass it to mTicker->generate(), and we don't want
                            // old data in there
    mTicker->generate(mRange, mParentPlot->locale(), mNumberFormatChar, mNumberPrecision,
                      mTickVector, mSubTicks ? &mSubTickVector : 0,
                      mTickLabels ? &mTickVectorLabels : 0);

    // fill cos/sin buffers which will be used by draw() and QCPPolarGrid::draw(), so we don't have
    // to calculate it twice:
    mTickVectorCosSin.resize(mTickVector.size());
    for (int i = 0; i < mTickVector.size(); ++i)
    {
        const double theta = coordToAngleRad(mTickVector.at(i));
        mTickVectorCosSin[i] = QPointF(qCos(theta), qSin(theta));
    }
    mSubTickVectorCosSin.resize(mSubTickVector.size());
    for (int i = 0; i < mSubTickVector.size(); ++i)
    {
        const double theta = coordToAngleRad(mSubTickVector.at(i));
        mSubTickVectorCosSin[i] = QPointF(qCos(theta), qSin(theta));
    }
}

/*! \internal

  Returns the pen that is used to draw the axis base line. Depending on the selection state, this
  is either mSelectedBasePen or mBasePen.
*/
QPen QCPPolarAxisAngular::getBasePen() const
{
    return mSelectedParts.testFlag(spAxis) ? mSelectedBasePen : mBasePen;
}

/*! \internal

  Returns the pen that is used to draw the (major) ticks. Depending on the selection state, this
  is either mSelectedTickPen or mTickPen.
*/
QPen QCPPolarAxisAngular::getTickPen() const
{
    return mSelectedParts.testFlag(spAxis) ? mSelectedTickPen : mTickPen;
}

/*! \internal

  Returns the pen that is used to draw the subticks. Depending on the selection state, this
  is either mSelectedSubTickPen or mSubTickPen.
*/
QPen QCPPolarAxisAngular::getSubTickPen() const
{
    return mSelectedParts.testFlag(spAxis) ? mSelectedSubTickPen : mSubTickPen;
}

/*! \internal

  Returns the font that is used to draw the tick labels. Depending on the selection state, this
  is either mSelectedTickLabelFont or mTickLabelFont.
*/
QFont QCPPolarAxisAngular::getTickLabelFont() const
{
    return mSelectedParts.testFlag(spTickLabels) ? mSelectedTickLabelFont : mTickLabelFont;
}

/*! \internal

  Returns the font that is used to draw the axis label. Depending on the selection state, this
  is either mSelectedLabelFont or mLabelFont.
*/
QFont QCPPolarAxisAngular::getLabelFont() const
{
    return mSelectedParts.testFlag(spAxisLabel) ? mSelectedLabelFont : mLabelFont;
}

/*! \internal

  Returns the color that is used to draw the tick labels. Depending on the selection state, this
  is either mSelectedTickLabelColor or mTickLabelColor.
*/
QColor QCPPolarAxisAngular::getTickLabelColor() const
{
    return mSelectedParts.testFlag(spTickLabels) ? mSelectedTickLabelColor : mTickLabelColor;
}

/*! \internal

  Returns the color that is used to draw the axis label. Depending on the selection state, this
  is either mSelectedLabelColor or mLabelColor.
*/
QColor QCPPolarAxisAngular::getLabelColor() const
{
    return mSelectedParts.testFlag(spAxisLabel) ? mSelectedLabelColor : mLabelColor;
}

void QCPPolarAxisAngular::emitRangeChanged(const QCPRange& oldRange)
{
    qcpEmitIf(
        this, mParentPlot, &QCPPolarAxisAngular::rangeChanged,
        [this, range = mRange] { return mRange == range; }, mRange, oldRange);
}
