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

#include "axistickerlog.h"
#include "axisticker-utils.h"

#include <array>
#include <vector>

namespace
{
// Mantissa sets tried in order when the range is too short for whole powers, as matplotlib's
// LogLocator and d3 do (https://github.com/d3/d3-scale/blob/main/src/log.js).
const std::array<std::vector<double>, 3> kMantissaLadders { {
    { 1, 2, 5 },
    { 1, 2, 3, 5, 7 },
    { 1, 2, 3, 4, 5, 6, 7, 8, 9 },
} };

// Every mantissa x 10^p from the decade at or below lower to the decade at or above upper, so one
// tick lies outside the range on each side (lower > 0).
QVector<double> mantissaTicks(const std::vector<double>& mantissas, double lower, double upper)
{
    QVector<double> ticks;
    for (int p = qFloor(std::log10(lower)); p <= qCeil(std::log10(upper)); ++p)
        for (double m : mantissas)
            ticks.append(m * qPow(10.0, p));
    return ticks;
}

int countInRange(const QVector<double>& ticks, double lower, double upper)
{
    return int(std::count_if(ticks.cbegin(), ticks.cend(),
                             [=](double t) { return t >= lower && t <= upper; }));
}

QVector<double> mirrored(QVector<double> ticks)
{
    std::reverse(ticks.begin(), ticks.end());
    for (double& t : ticks)
        t = -t;
    return ticks;
}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPAxisTickerLog
////////////////////////////////////////////////////////////////////////////////////////////////////
/*! \class QCPAxisTickerLog
  \brief Specialized axis ticker suited for logarithmic axes

  \image html axisticker-log.png

  This QCPAxisTicker subclass generates ticks with unequal tick intervals suited for logarithmic
  axis scales. The ticks are placed at powers of the specified log base (\ref setLogBase).

  Especially in the case of a log base equal to 10 (the default), it might be desirable to have
  tick labels in the form of powers of ten without mantissa display. To achieve this, set the
  number precision (\ref QCPAxis::setNumberPrecision) to zero and the number format (\ref
  QCPAxis::setNumberFormat) to scientific (exponential) display with beautifully typeset decimal
  powers, so a format string of <tt>"eb"</tt>. This will result in the following axis tick labels:

  \image html axisticker-log-powers.png

  The ticker can be created and assigned to an axis like this:
  \snippet documentation/doc-image-generator/mainwindow.cpp axistickerlog-creation

  Note that the nature of logarithmic ticks imply that there exists a smallest possible tick step,
  corresponding to one multiplication by the log base. If the user zooms in further than that, no
  new ticks would appear, leading to very sparse or even no axis ticks on the axis. To prevent this
  situation, with log base 10 this ticker first places ticks at round mantissas (1, 2, 5 x 10^n,
  then denser digit sets), and falls back to regular tick generation only if even every digit
  would give too few ticks.
*/

/*!
  Constructs the ticker and sets reasonable default values. Axis tickers are commonly created
  managed by a QSharedPointer, which then can be passed to QCPAxis::setTicker.
*/
QCPAxisTickerLog::QCPAxisTickerLog()
        : mLogBase(10.0)
        , mSubTickCount(8)
        , // generates 10 intervals
        mLogBaseLnInv(1.0 / qLn(mLogBase))
{
}

/*!
  Sets the logarithm base used for tick coordinate generation. The ticks will be placed at integer
  powers of \a base.
*/
void QCPAxisTickerLog::setLogBase(double base)
{
    if (base > 0)
    {
        mLogBase = base;
        mLogBaseLnInv = 1.0 / qLn(mLogBase);
    }
    else
        qDebug() << Q_FUNC_INFO << "log base has to be greater than zero:" << base;
}

/*!
  Sets the number of sub ticks in a tick interval. Within each interval, the sub ticks are spaced
  linearly to provide a better visual guide, so the sub tick density increases toward the higher
  tick.

  Note that \a subTicks is the number of sub ticks (not sub intervals) in one tick interval. So in
  the case of logarithm base 10 an intuitive sub tick spacing would be achieved with eight sub
  ticks (the default). This means e.g. between the ticks 10 and 100 there will be eight ticks,
  namely at 20, 30, 40, 50, 60, 70, 80 and 90.
*/
void QCPAxisTickerLog::setSubTickCount(int subTicks)
{
    if (subTicks >= 0)
        mSubTickCount = subTicks;
    else
        qDebug() << Q_FUNC_INFO << "sub tick count can't be negative:" << subTicks;
}

/*! \internal

  Returns the sub tick count specified in \ref setSubTickCount. For QCPAxisTickerLog, there is no
  automatic sub tick count calculation necessary.

  \seebaseclassmethod
*/
int QCPAxisTickerLog::getSubTickCount([[maybe_unused]] double tickStep)
{
    return mSubTickCount;
}

/*! \internal

  Creates ticks with a spacing given by the logarithm base and an increasing integer power in the
  provided \a range. The step in which the power increases tick by tick is chosen in order to keep
  the total number of ticks as close as possible to the tick count (\ref setTickCount).

  The parameter \a tickStep is ignored for the normal logarithmic ticker generation. Only when
  zoomed in very far such that not enough logarithmically placed ticks would be visible, this
  function falls back to the regular QCPAxisTicker::createTickVector, which then uses \a tickStep.

  \seebaseclassmethod
*/
QVector<double> QCPAxisTickerLog::createTickVector(double tickStep, const QCPRange& range)
{
    const bool positive = range.lower > 0 && range.upper > 0;
    const bool negative = range.lower < 0 && range.upper < 0;
    if (!positive && !negative)
    {
        qDebug() << Q_FUNC_INFO << "Invalid range for logarithmic plot: " << range.lower << ".."
                 << range.upper;
        return {};
    }

    const double ratio = positive ? range.upper / range.lower : range.lower / range.upper;
    const double baseTickCount = qLn(ratio) * mLogBaseLnInv;
    mMantissaTicks = false;
    if (baseTickCount < 1.6)
    {
        if (auto ticks = createMantissaTickVector(range); !ticks.isEmpty())
        {
            mMantissaTicks = true;
            return ticks;
        }
        return QCPAxisTicker::createTickVector(tickStep, range);
    }

    const double exactPowerStep = baseTickCount / double(mTickCount + qcp::kTickCountEpsilon);
    const double newLogBase = qPow(mLogBase, qMax(int(cleanMantissa(exactPowerStep)), 1));
    const double lnNewLogBase = qLn(newLogBase);

    double currentTick = positive
        ? qPow(newLogBase, qFloor(qLn(range.lower) / lnNewLogBase))
        : -qPow(newLogBase, qCeil(qLn(-range.lower) / lnNewLogBase));

    QVector<double> result;
    result.append(currentTick);
    // currentTick might reach zero for ranges ~1e-300, guard against infinite loop
    while (currentTick < range.upper && (positive ? currentTick > 0 : currentTick < 0))
    {
        currentTick = positive ? currentTick * newLogBase : currentTick / newLogBase;
        result.append(currentTick);
    }
    return result;
}

/*! \internal

  Ticks at round mantissas (1, 2, 5 x 10^n, then denser sets) for ranges too short for whole
  powers. Returns an empty vector when even every digit would give too few ticks, or when the log
  base is not 10.
*/
QVector<double> QCPAxisTickerLog::createMantissaTickVector(const QCPRange& range) const
{
    if (mLogBase != 10.0)
        return {};
    const bool negative = range.upper < 0;
    const double lower = negative ? -range.upper : range.lower;
    const double upper = negative ? -range.lower : range.upper;
    const int wanted = qMax(2, (mTickCount + 1) / 2);
    for (const auto& mantissas : kMantissaLadders)
    {
        auto ticks = mantissaTicks(mantissas, lower, upper);
        if (countInRange(ticks, lower, upper) >= wanted)
            return negative ? mirrored(ticks) : ticks;
    }
    return {};
}

/*! \internal

  With mantissa ticks, the sub ticks are the remaining digits (2, 3, 4 ... x 10^n). Otherwise the
  base class spaces them linearly.

  \seebaseclassmethod
*/
QVector<double> QCPAxisTickerLog::createSubTickVector(int subTickCount,
                                                       const QVector<double>& ticks)
{
    if (!mMantissaTicks)
        return QCPAxisTicker::createSubTickVector(subTickCount, ticks);
    const bool negative = ticks.front() < 0;
    const double lower = negative ? -ticks.back() : ticks.front();
    const double upper = negative ? -ticks.front() : ticks.back();
    auto isMajor = [&](double v)
    {
        return std::any_of(ticks.cbegin(), ticks.cend(),
                           [&](double t) { return qFuzzyCompare(qAbs(t), v); });
    };
    QVector<double> subTicks;
    for (double v : mantissaTicks(kMantissaLadders.back(), lower, upper))
        if (v > lower && v < upper && !isMajor(v))
            subTicks.append(v);
    return negative ? mirrored(subTicks) : subTicks;
}
