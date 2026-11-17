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
/*! \fn void QCPPolarAxisAngular::angleChanged(double angle)

  This signal is emitted when \ref angle changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarAxisAngular::ticksChanged(bool ticks)

  This signal is emitted when \ref ticks changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarAxisAngular::tickLabelsChanged(bool tickLabels)

  This signal is emitted when \ref tickLabels changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarAxisAngular::tickLabelPaddingChanged(int tickLabelPadding)

  This signal is emitted when \ref tickLabelPadding changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarAxisAngular::tickLabelFontChanged(const QFont& tickLabelFont)

  This signal is emitted when \ref tickLabelFont changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarAxisAngular::tickLabelColorChanged(const QColor& tickLabelColor)

  This signal is emitted when \ref tickLabelColor changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarAxisAngular::tickLabelRotationChanged(double tickLabelRotation)

  This signal is emitted when \ref tickLabelRotation changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarAxisAngular::tickLabelModeChanged(QCPPolarAxisAngular::LabelMode tickLabelMode)

  This signal is emitted when \ref tickLabelMode changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarAxisAngular::numberFormatChanged(const QString& numberFormat)

  This signal is emitted when \ref numberFormat changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarAxisAngular::numberPrecisionChanged(int numberPrecision)

  This signal is emitted when \ref numberPrecision changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarAxisAngular::tickLengthInChanged(int tickLengthIn)

  This signal is emitted when \ref tickLengthIn changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarAxisAngular::tickLengthOutChanged(int tickLengthOut)

  This signal is emitted when \ref tickLengthOut changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarAxisAngular::subTicksChanged(bool subTicks)

  This signal is emitted when \ref subTicks changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarAxisAngular::subTickLengthInChanged(int subTickLengthIn)

  This signal is emitted when \ref subTickLengthIn changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarAxisAngular::subTickLengthOutChanged(int subTickLengthOut)

  This signal is emitted when \ref subTickLengthOut changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarAxisAngular::basePenChanged(const QPen& basePen)

  This signal is emitted when \ref basePen changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarAxisAngular::tickPenChanged(const QPen& tickPen)

  This signal is emitted when \ref tickPen changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarAxisAngular::subTickPenChanged(const QPen& subTickPen)

  This signal is emitted when \ref subTickPen changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarAxisAngular::labelFontChanged(const QFont& labelFont)

  This signal is emitted when \ref labelFont changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarAxisAngular::labelColorChanged(const QColor& labelColor)

  This signal is emitted when \ref labelColor changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarAxisAngular::labelChanged(const QString& label)

  This signal is emitted when \ref label changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarAxisAngular::labelPaddingChanged(int labelPadding)

  This signal is emitted when \ref labelPadding changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarAxisAngular::selectedTickLabelFontChanged(const QFont& selectedTickLabelFont)

  This signal is emitted when \ref selectedTickLabelFont changes, after the plot delivers its
  deferred signals.
*/

/*! \fn void QCPPolarAxisAngular::selectedLabelFontChanged(const QFont& selectedLabelFont)

  This signal is emitted when \ref selectedLabelFont changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarAxisAngular::selectedTickLabelColorChanged(const QColor&
  selectedTickLabelColor)

  This signal is emitted when \ref selectedTickLabelColor changes, after the plot delivers its
  deferred signals.
*/

/*! \fn void QCPPolarAxisAngular::selectedLabelColorChanged(const QColor& selectedLabelColor)

  This signal is emitted when \ref selectedLabelColor changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarAxisAngular::selectedBasePenChanged(const QPen& selectedBasePen)

  This signal is emitted when \ref selectedBasePen changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarAxisAngular::selectedTickPenChanged(const QPen& selectedTickPen)

  This signal is emitted when \ref selectedTickPen changes, after the plot delivers its deferred
  signals.
*/

/*! \fn void QCPPolarAxisAngular::selectedSubTickPenChanged(const QPen& selectedSubTickPen)

  This signal is emitted when \ref selectedSubTickPen changes, after the plot delivers its deferred
  signals.
*/

/*!
  Creates the angular axis belonging to \a area.
*/
QCPPolarAxisAngular::QCPPolarAxisAngular(QCPPolarArea* area)
        : QCPAxisBase(area)
        ,
        // axis base:
        mAngle(-90)
        , mAngleRad(mAngle / 180.0 * M_PI)
        , mBasePen(QPen(Qt::black, 0, Qt::SolidLine, Qt::SquareCap))
        , mSelectedBasePen(QPen(Qt::blue, 2))
        ,
        // axis label:
        mLabelPadding(0)
        , mLabel()
        , mLabelFont(mParentPlot->font())
        , mSelectedLabelFont(QCP::boldFont(mLabelFont))
        , mLabelColor(Qt::black)
        , mSelectedLabelColor(Qt::blue)
        ,
        // tick labels:
        // mTickLabelPadding(0), in label painter
        mTickLabels(true)
        ,
        // mTickLabelRotation(0), in label painter
        mTickLabelFont(mParentPlot->font())
        , mSelectedTickLabelFont(QCP::boldFont(mTickLabelFont))
        , mTickLabelColor(Qt::black)
        , mSelectedTickLabelColor(Qt::blue)
        , mNumberPrecision(6)
        , mNumberFormatChar('g')
        , mNumberBeautifulPowers(true)
        , mNumberMultiplyCross(true) // what the label painter draws by default
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
        // internal members:
        mLabelPainter(mParentPlot)
{
    // A polar area zooms and drags through its own handlers, not by default.
    mRange = QCPRange(0, 360);
    mRangeDraggable = false;
    mRangeZoomable = false;
    mTicker = defaultTicker(QCPScale::linear());
    setAntialiased(true);


    setTickLabelPadding(5);
    setTickLabelRotation(0);
    setTickLabelMode(lmUpright);
    mLabelPainter.setAnchorReferenceType(QCPLabelPainterPrivate::artNormal);
    mLabelPainter.setAbbreviateDecimalPowers(false);
    mLabelPainter.setCacheSize(24); // so we can cache up to 15-degree intervals, polar angular axis
                                    // uses a bit larger cache than normal axes
}

QCPPolarArea* QCPPolarAxisAngular::area() const
{
    return polarArea();
}

/*! \internal

  The angular axis always shows a full turn: rescaling it to the data would cut the circle the polar
  mapping is built on.
*/
void QCPPolarAxisAngular::rescale([[maybe_unused]] bool onlyVisiblePlottables)
{
}

/*! \internal

  The area holds the angular axis by raw pointer: leaving it behind would leave a dangling
  handle in every path that draws or maps the polar plot.
*/
QCPPolarAxisAngular::~QCPPolarAxisAngular()
{
    if (polarArea())
    {
        // The systems that map through it go while it still exists.
        polarArea()->axisRemoved(this);
        polarArea()->mAngularAxis = nullptr;
    }
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
        if (mNumberMultiplyCross)
            result.append(QLatin1Char('c'));
    }
    return result;
}

/*!
  Transforms \a value, in pixel coordinates of the QCustomPlot widget, to axis coordinates.
*/
void QCPPolarAxisAngular::pixelToCoord(QPointF pixelPos, double& angleCoord,
                                       double& radiusCoord) const
{
    if (!polarArea())
    {
        angleCoord = radiusCoord = qQNaN();
        return;
    }
    if (polarArea()->radialAxisCount() > 0)
        polarArea()->radialAxis()->pixelToCoord(pixelPos, angleCoord, radiusCoord);
    else
        qDebug() << Q_FUNC_INFO << "no radial axis configured";
}

/*!
  Transforms \a value, in coordinates of the axis, to pixel coordinates of the QCustomPlot widget.
*/
QPointF QCPPolarAxisAngular::coordToPixel(double angleCoord, double radiusCoord) const
{
    if (!polarArea())
        return QPointF(qQNaN(), qQNaN());
    if (polarArea()->radialAxisCount() > 0)
    {
        return polarArea()->radialAxis()->coordToPixel(angleCoord, radiusCoord);
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
QCPPolarAxisAngular::SelectablePart QCPPolarAxisAngular::getPartAt(const QPointF& pos) const
{
    if (!mVisible || !polarArea())
        return spNone;
    // A label wins over the circle it sits on: it is what the user aims at.
    for (const QRectF& box : mTickLabelBoxes)
        if (box.contains(pos))
            return spTickLabels;
    const double tolerance = mParentPlot ? mParentPlot->selectionTolerance() : 5.0;
    if (qAbs(QLineF(polarArea()->center(), pos).length() - polarArea()->radius()) <= tolerance)
        return spAxis;
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
    if (!polarArea())
        return;
    const QPointF center = polarArea()->center();
    const double radius = polarArea()->radius();
    mTickLabelBoxes.clear();
    painter->setPen(getBasePen());
    painter->drawEllipse(center, radius, radius);
    if (!mSubTickVector.isEmpty())
    {
        painter->setPen(getSubTickPen());
        for (int i = 0; i < mSubTickVector.size(); ++i)
            painter->drawLine(center + mSubTickVectorCosSin.at(i) * (radius - mSubTickLengthIn),
                              center + mSubTickVectorCosSin.at(i) * (radius + mSubTickLengthOut));
    }
    if (!mTickVector.isEmpty())
    {
        mLabelPainter.setAnchorReference(center);
        mLabelPainter.setFont(getTickLabelFont());
        mLabelPainter.setColor(getTickLabelColor());
        const QPen ticksPen = getTickPen();
        painter->setPen(ticksPen);
        for (int i = 0; i < mTickVector.size(); ++i)
        {
            const QPointF outerTick = center + mTickVectorCosSin.at(i) * (radius + mTickLengthOut);
            painter->drawLine(center + mTickVectorCosSin.at(i) * (radius - mTickLengthIn),
                              outerTick);
            if (!mTickVectorLabels.isEmpty())
            {
                if (i < mTickVectorLabels.count() - 1
                    || (mTickVectorCosSin.at(i) - mTickVectorCosSin.first()).manhattanLength()
                        > 5 / 180.0 * M_PI)
                {
                    // The drawn rect is the click box: a rebuilt one drifts from where the label
                    // painter actually anchors the text.
                    mTickLabelBoxes.append(
                        mLabelPainter.drawTickLabel(painter, outerTick, mTickVectorLabels.at(i)));
                }
            }
        }
    }
}

void QCPPolarAxisAngular::setAngle(double degrees)
{
    if (degrees == mAngle)
        return;
    // The area's systems signal the new frame: a slot there may delete the axis.
    const QCPEmissionScope scope(mParentPlot);
    markAppearanceDirty();
    mAngle = degrees;
    mAngleRad = mAngle / 180.0 * M_PI;
    // The rotation is part of every polar frame: no range moved, so the area is told here.
    if (polarArea())
        polarArea()->axisMappingChanged(this);
    qcpEmit(this, mParentPlot, &QCPPolarAxisAngular::angleChanged, angle());
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
        update();
        mTicks = show;
        // mCachedMarginValid = false;
        qcpEmit(this, mParentPlot, &QCPPolarAxisAngular::ticksChanged, ticks());
    }
}

/*!
  Sets whether tick labels are displayed. Tick labels are the numbers drawn next to tick marks.
*/
void QCPPolarAxisAngular::setTickLabels(bool show)
{
    if (mTickLabels != show)
    {
        update();
        mTickLabels = show;
        // mCachedMarginValid = false;
        if (!mTickLabels)
            mTickVectorLabels.clear();
        qcpEmit(this, mParentPlot, &QCPPolarAxisAngular::tickLabelsChanged, tickLabels());
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
    update();
    mLabelPainter.setPadding(padding);
    qcpEmit(this, mParentPlot, &QCPPolarAxisAngular::tickLabelPaddingChanged, tickLabelPadding());
}

/*!
  Sets the font of the tick labels.

  \see setTickLabels, setTickLabelColor
*/
void QCPPolarAxisAngular::setTickLabelFont(const QFont& font)
{
    if (font == mTickLabelFont)
        return;
    update();
    mTickLabelFont = font;
    qcpEmit(this, mParentPlot, &QCPPolarAxisAngular::tickLabelFontChanged, tickLabelFont());
}

/*!
  Sets the color of the tick labels.

  \see setTickLabels, setTickLabelFont
*/
void QCPPolarAxisAngular::setTickLabelColor(const QColor& color)
{
    if (color == mTickLabelColor)
        return;
    update();
    mTickLabelColor = color;
    qcpEmit(this, mParentPlot, &QCPPolarAxisAngular::tickLabelColorChanged, tickLabelColor());
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
    if (mLabelPainter.rotation() == qBound(-90.0, degrees, 90.0))
        return;
    update();
    mLabelPainter.setRotation(degrees);
    qcpEmit(this, mParentPlot, &QCPPolarAxisAngular::tickLabelRotationChanged, tickLabelRotation());
}

void QCPPolarAxisAngular::setTickLabelMode(LabelMode mode)
{
    const auto anchor = mode == lmUpright ? QCPLabelPainterPrivate::amSkewedUpright
                                          : QCPLabelPainterPrivate::amSkewedRotated;
    if (mLabelPainter.anchorMode() == anchor)
        return;
    update();
    mLabelPainter.setAnchorMode(anchor);
    qcpEmit(this, mParentPlot, &QCPPolarAxisAngular::tickLabelModeChanged, tickLabelMode());
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
    update();
    mLabelPainter.setSubstituteExponent(mNumberBeautifulPowers);
    mLabelPainter.setMultiplicationSymbol(mNumberMultiplyCross ? QCPLabelPainterPrivate::SymbolCross
                                                               : QCPLabelPainterPrivate::SymbolDot);
    qcpEmit(this, mParentPlot, &QCPPolarAxisAngular::numberFormatChanged, numberFormat());
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
    update();
    mNumberPrecision = precision;
    qcpEmit(this, mParentPlot, &QCPPolarAxisAngular::numberPrecisionChanged, numberPrecision());
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
    // Both signals wait for the scope: a slot of the first may delete the axis.
    const QCPEmissionScope scope(mParentPlot);
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
        update();
        mTickLengthIn = inside;
        qcpEmit(this, mParentPlot, &QCPPolarAxisAngular::tickLengthInChanged, tickLengthIn());
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
        update();
        mTickLengthOut = outside;
        // mCachedMarginValid = false; // only outside tick length can change margin
        qcpEmit(this, mParentPlot, &QCPPolarAxisAngular::tickLengthOutChanged, tickLengthOut());
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
        update();
        mSubTicks = show;
        // mCachedMarginValid = false;
        qcpEmit(this, mParentPlot, &QCPPolarAxisAngular::subTicksChanged, subTicks());
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
    // Both signals wait for the scope: a slot of the first may delete the axis.
    const QCPEmissionScope scope(mParentPlot);
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
        update();
        mSubTickLengthIn = inside;
        qcpEmit(this, mParentPlot, &QCPPolarAxisAngular::subTickLengthInChanged, subTickLengthIn());
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
    update();
    mSubTickLengthOut = outside;
    qcpEmit(this, mParentPlot, &QCPPolarAxisAngular::subTickLengthOutChanged, subTickLengthOut());
}

/*!
  Sets the pen, the axis base line is drawn with.

  \see setTickPen, setSubTickPen
*/
void QCPPolarAxisAngular::setBasePen(const QPen& pen)
{
    if (pen == mBasePen)
        return;
    update();
    mBasePen = pen;
    qcpEmit(this, mParentPlot, &QCPPolarAxisAngular::basePenChanged, basePen());
}

/*!
  Sets the pen, tick marks will be drawn with.

  \see setTickLength, setBasePen
*/
void QCPPolarAxisAngular::setTickPen(const QPen& pen)
{
    if (pen == mTickPen)
        return;
    update();
    mTickPen = pen;
    qcpEmit(this, mParentPlot, &QCPPolarAxisAngular::tickPenChanged, tickPen());
}

/*!
  Sets the pen, subtick marks will be drawn with.

  \see setSubTickCount, setSubTickLength, setBasePen
*/
void QCPPolarAxisAngular::setSubTickPen(const QPen& pen)
{
    if (pen == mSubTickPen)
        return;
    update();
    mSubTickPen = pen;
    qcpEmit(this, mParentPlot, &QCPPolarAxisAngular::subTickPenChanged, subTickPen());
}

/*!
  Sets the font of the axis label.

  \see setLabelColor
*/
void QCPPolarAxisAngular::setLabelFont(const QFont& font)
{
    if (mLabelFont != font)
    {
        update();
        mLabelFont = font;
        // mCachedMarginValid = false;
        qcpEmit(this, mParentPlot, &QCPPolarAxisAngular::labelFontChanged, labelFont());
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
    update();
    mLabelColor = color;
    qcpEmit(this, mParentPlot, &QCPPolarAxisAngular::labelColorChanged, labelColor());
}

/*!
  Sets the text of the axis label that will be shown below/above or next to the axis, depending on
  its orientation. To disable axis labels, pass an empty string as \a str.
*/
void QCPPolarAxisAngular::setLabel(const QString& str)
{
    if (mLabel != str)
    {
        update();
        mLabel = str;
        // mCachedMarginValid = false;
        qcpEmit(this, mParentPlot, &QCPPolarAxisAngular::labelChanged, label());
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
        update();
        mLabelPadding = padding;
        // mCachedMarginValid = false;
        qcpEmit(this, mParentPlot, &QCPPolarAxisAngular::labelPaddingChanged, labelPadding());
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
        update();
        mSelectedTickLabelFont = font;
        // don't set mCachedMarginValid to false here because margin calculation is always done with
        // non-selected fonts
        qcpEmit(this, mParentPlot, &QCPPolarAxisAngular::selectedTickLabelFontChanged,
                selectedTickLabelFont());
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
    update();
    mSelectedLabelFont = font;
    qcpEmit(this, mParentPlot, &QCPPolarAxisAngular::selectedLabelFontChanged, selectedLabelFont());
}

/*!
  Sets the color that is used for tick labels when they are selected.

  \see setTickLabelColor, setSelectableParts, setSelectedParts, QCustomPlot::setInteractions
*/
void QCPPolarAxisAngular::setSelectedTickLabelColor(const QColor& color)
{
    if (color != mSelectedTickLabelColor)
    {
        update();
        mSelectedTickLabelColor = color;
        qcpEmit(this, mParentPlot, &QCPPolarAxisAngular::selectedTickLabelColorChanged,
                selectedTickLabelColor());
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
    update();
    mSelectedLabelColor = color;
    qcpEmit(this, mParentPlot, &QCPPolarAxisAngular::selectedLabelColorChanged,
            selectedLabelColor());
}

/*!
  Sets the pen that is used to draw the axis base line when selected.

  \see setBasePen, setSelectableParts, setSelectedParts, QCustomPlot::setInteractions
*/
void QCPPolarAxisAngular::setSelectedBasePen(const QPen& pen)
{
    if (pen == mSelectedBasePen)
        return;
    update();
    mSelectedBasePen = pen;
    qcpEmit(this, mParentPlot, &QCPPolarAxisAngular::selectedBasePenChanged, selectedBasePen());
}

/*!
  Sets the pen that is used to draw the (major) ticks when selected.

  \see setTickPen, setSelectableParts, setSelectedParts, QCustomPlot::setInteractions
*/
void QCPPolarAxisAngular::setSelectedTickPen(const QPen& pen)
{
    if (pen == mSelectedTickPen)
        return;
    update();
    mSelectedTickPen = pen;
    qcpEmit(this, mParentPlot, &QCPPolarAxisAngular::selectedTickPenChanged, selectedTickPen());
}

/*!
  Sets the pen that is used to draw the subticks when selected.

  \see setSubTickPen, setSelectableParts, setSelectedParts, QCustomPlot::setInteractions
*/
void QCPPolarAxisAngular::setSelectedSubTickPen(const QPen& pen)
{
    if (pen == mSelectedSubTickPen)
        return;
    update();
    mSelectedSubTickPen = pen;
    qcpEmit(this, mParentPlot, &QCPPolarAxisAngular::selectedSubTickPenChanged,
            selectedSubTickPen());
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
    if ((!mTicks && !mTickLabels
         && (!polarArea() || !polarArea()->grid() || !polarArea()->grid()->visible()))
        || mRange.size() <= 0)
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

/*! \internal

  A range change moves the ticks on the circle and every graph drawn from the area, so the area's
  layers are dirtied, then the base emits and refreshes any linked axis (a polar axis never is).
*/
void QCPPolarAxisAngular::rangeDidChange(const QCPRange& oldRange)
{
    markAppearanceDirty();
    QCPAxisBase::rangeDidChange(oldRange);
}

/*! \internal

  The ring places every angle by its fraction of the angular range, whatever the scale: a
  non-linear one would change the ticks and nothing else, so it is refused.
*/
bool QCPPolarAxisAngular::scaleWillChange(const QCPScale& scale)
{
    QCP_CHECK_OR_RETURN(scale.isLinear(), "a polar angular axis has no non-linear scale", false);
    return true;
}

/*! \internal

  The ring takes a 30 degree ticker whatever the (always linear) scale brings: the automatic ticks
  of a plain linear ticker would spread a handful of degrees over the circle instead.
*/
QSharedPointer<QCPAxisTicker> QCPPolarAxisAngular::defaultTicker(
    [[maybe_unused]] const QCPScale& scale)
{
    const auto ticker = QSharedPointer<QCPAxisTickerFixed>::create();
    ticker->setTickStep(30);
    return ticker;
}

/*! \internal

  The rotation, the pens and the fonts change the pixels of the ticks and of every graph of the
  area, wherever their layers are, so the area's layers are dirtied, not only the axis's own.
*/
void QCPPolarAxisAngular::markAppearanceDirty()
{
    update();
    if (polarArea())
        polarArea()->markAffectedLayersDirty();
}

/*! \internal

  A polar axis has no cartesian geometry to follow a source with, so it refuses every link.
*/
bool QCPPolarAxisAngular::checkLinkSource([[maybe_unused]] const QCPAxisBase* source) const
{
    qcp::reportMisuse(Q_FUNC_INFO, "a polar axis cannot be linked");
    return false;
}
