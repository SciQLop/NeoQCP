#include "axistickersymlog.h"
#include "../misuse.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
// A value's decade in the scale's base. Base 10 goes through log10, which is exact for the powers
// of ten a range ends on; the other bases take the quotient of the two logs.
double logBase(double base, double value)
{
    return base == 10.0 ? std::log10(value) : std::log(value) / std::log(base);
}

// The exponent of the decade at or below the value: 1000 is exponent 3 in base 10, and a log that
// lands a rounding error under an integer still gives that integer.
int decadeFloor(double base, double value)
{
    const double exponent = logBase(base, value);
    const double nearestInteger = std::round(exponent);
    return int(std::abs(exponent - nearestInteger) < 1e-9 ? nearestInteger
                                                          : std::floor(exponent));
}

int decadeCeil(double base, double value)
{
    const double exponent = logBase(base, value);
    const double nearestInteger = std::round(exponent);
    return int(std::abs(exponent - nearestInteger) < 1e-9 ? nearestInteger
                                                          : std::ceil(exponent));
}

std::vector<int> everyStride(const std::vector<int>& exponents, int stride)
{
    std::vector<int> kept;
    for (std::size_t i = 0; i < exponents.size(); i += std::size_t(stride))
        kept.push_back(exponents[i]);
    return kept;
}

bool isMajorTick(const QVector<double>& ticks, double value)
{
    return std::any_of(ticks.cbegin(), ticks.cend(),
                       [value](double tick) { return qFuzzyCompare(tick, value); });
}
} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPAxisTickerSymlog
////////////////////////////////////////////////////////////////////////////////////////////////////
/*! \class QCPAxisTickerSymlog
  \brief Specialized axis ticker suited for symlog axes

  This QCPAxisTicker subclass generates the ticks of a symlog scale (\ref QCPScale::symlog): the
  decades matplotlib's SymmetricalLogLocator places, that is from the decade holding the scale's
  threshold outward on both sides of zero, plus zero when the range straddles the band. A wide
  range is thinned by the same step formula the log ticker uses, so about \ref setTickCount ticks
  remain.

  Outside the band the mapping is logarithmic, and the sub ticks are the decades' mantissas
  (2...9 x base^k). Inside the band it is linear, so the sub ticks are the multiples of a fifth of
  the threshold's decade, the sub grid a linear axis draws over one decade. Both ends of the band
  are marked: they are where the mapping bends.

  The ticker reads the threshold and the base of the scale it was built with. QCPAxisBase::setScale
  installs it for a symlog scale, and a later QCPAxisBase::setTicker wins.

  A range that holds no decade at all, three to four say, has no symlog tick to show. It gets
  evenly spaced ticks there instead, as a range with no decade would be unreadable without them.
*/

/*!
  Constructs the ticker for \a scale, which must be a symlog scale. A scale of another kind is
  refused through the misuse tier and gives QCPScale::symlog().
*/
QCPAxisTickerSymlog::QCPAxisTickerSymlog(const QCPScale& scale)
{
    if (scale.isSymlog())
        mScale = scale;
    else
        qcp::reportMisuse(Q_FUNC_INFO, "a symlog ticker needs a symlog scale");
}

/*! \internal

  Returns the power of the base that holds the threshold: 1 for a threshold of 2 in base 10, as
  the band [-2, 2] sits in the decade [1, 10). The band's sub grid is that decade's linear sub
  grid.
*/
double QCPAxisTickerSymlog::decadeStep() const
{
    return qPow(mScale.base(), decadeFloor(mScale.base(), mScale.threshold()));
}

/*! \internal

  Returns the decade ticks of \a range, in ascending order. A range that stays inside the band
  takes its own ends and zero, as matplotlib does: any tick would be off the ends of the range
  otherwise.

  \seebaseclassmethod
*/
QVector<double> QCPAxisTickerSymlog::createTickVector(double tickStep, const QCPRange& range)
{
    mLinearFallback = false;
    const QVector<double> ticks = symlogDecades(range);
    const bool someTickIsVisible
        = std::any_of(ticks.cbegin(), ticks.cend(), [&range](double tick)
                      { return tick >= range.lower && tick <= range.upper; });
    if (someTickIsVisible)
        return ticks;
    mLinearFallback = true;
    return QCPAxisTicker::createTickVector(tickStep, range);
}

/*! \internal

  The decades of \a range: from the decade at or below the threshold outward, on each side the
  range reaches, plus zero when the range straddles the band. The exponent bounds are
  matplotlib's, including its \c +1 on the outer end, which keeps a tick just outside the range
  for the axis to trim.

  The decades of each side are thinned by the log ticker's step formula, so a wide range does not
  fill the axis with labels.
*/
QVector<double> QCPAxisTickerSymlog::symlogDecades(const QCPRange& range) const
{
    const double base = mScale.base();
    const double threshold = mScale.threshold();
    if (range.lower >= -threshold && range.upper <= threshold)
    {
        QVector<double> ends { range.lower, 0, range.upper };
        std::sort(ends.begin(), ends.end());
        ends.erase(std::unique(ends.begin(), ends.end()), ends.end());
        return ends;
    }

    const bool negativeSide = range.lower < -threshold;
    const bool positiveSide = range.upper > threshold;
    const bool zero = (negativeSide && range.upper > -threshold)
        || (positiveSide && range.lower < threshold);

    std::vector<int> negativeExponents;
    if (negativeSide)
        for (int p = decadeFloor(base, std::max(threshold, -range.upper));
             p < decadeCeil(base, -range.lower + 1.0); ++p)
            negativeExponents.push_back(p);
    std::vector<int> positiveExponents;
    if (positiveSide)
        for (int p = decadeFloor(base, std::max(threshold, range.lower));
             p < decadeCeil(base, range.upper + 1.0); ++p)
            positiveExponents.push_back(p);

    int outOfBandCount = 0;
    for (int p : negativeExponents)
        outOfBandCount += qPow(base, p) > threshold ? 1 : 0;
    for (int p : positiveExponents)
        outOfBandCount += qPow(base, p) > threshold ? 1 : 0;
    const int stride
        = qMax(1, int(cleanMantissa(double(outOfBandCount) / double(mTickCount))));

    QVector<double> ticks;
    ticks.reserve(int(negativeExponents.size() + positiveExponents.size()) + 1);
    const std::vector<int> negativeTicks = everyStride(negativeExponents, stride);
    for (auto it = negativeTicks.rbegin(); it != negativeTicks.rend(); ++it)
        ticks.append(-qPow(base, *it));
    if (zero)
        ticks.append(0);
    for (int p : everyStride(positiveExponents, stride))
        ticks.append(qPow(base, p));
    return ticks;
}

/*! \internal

  Inside the band the sub ticks are a linear grid, so they come from the threshold's decade;
  outside it they are the decades' mantissas. A range that fell back to evenly spaced ticks keeps
  the base class's sub ticks.

  \seebaseclassmethod
*/
int QCPAxisTickerSymlog::getSubTickCount(double tickStep)
{
    if (mLinearFallback)
        return QCPAxisTicker::getSubTickCount(tickStep);
    return QCPAxisTicker::getSubTickCount(decadeStep());
}

/*! \internal

  Returns the sub ticks of \a ticks: the band's linear grid, plus the mantissas of the decades
  outside it, on both sides.

  \seebaseclassmethod
*/
QVector<double> QCPAxisTickerSymlog::createSubTickVector(int subTickCount,
                                                         const QVector<double>& ticks)
{
    if (mLinearFallback || ticks.isEmpty())
        return QCPAxisTicker::createSubTickVector(subTickCount, ticks);

    const double base = mScale.base();
    const double threshold = mScale.threshold();
    const double bandStep
        = decadeStep() / double(QCPAxisTicker::getSubTickCount(decadeStep()) + 1);

    std::vector<double> candidates;
    const int bandCells = int(std::floor(threshold / bandStep + 1e-9));
    for (int k = -bandCells; k <= bandCells; ++k)
        candidates.push_back(k * bandStep);

    const int topMantissa = qMax(1, int(std::ceil(base)) - 1);
    const double outerMagnitude = std::max(std::abs(ticks.front()), std::abs(ticks.back()));
    for (int p = decadeFloor(base, threshold); p <= decadeCeil(base, outerMagnitude); ++p)
        for (int m = 2; m <= topMantissa; ++m)
        {
            const double value = m * qPow(base, p);
            if (value > threshold)
            {
                candidates.push_back(value);
                candidates.push_back(-value);
            }
        }

    std::sort(candidates.begin(), candidates.end());
    QVector<double> subTicks;
    subTicks.reserve(int(candidates.size()));
    for (double value : candidates)
    {
        if (isMajorTick(ticks, value))
            continue;
        if (!subTicks.isEmpty() && qFuzzyCompare(subTicks.back(), value))
            continue;
        subTicks.append(value);
    }
    return subTicks;
}
