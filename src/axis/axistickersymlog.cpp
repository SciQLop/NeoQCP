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
double decadeFloor(double base, double value)
{
    const double exponent = logBase(base, value);
    const double nearestInteger = std::round(exponent);
    return std::abs(exponent - nearestInteger) < 1e-9 ? nearestInteger : std::floor(exponent);
}

// matplotlib's upper bound, ceil(log_base(value + 1)), kept un-snapped: the +1 is what keeps the
// decade of the range end in the list, so snapping it would drop that decade when the end is an
// exact power of the base.
double decadeCeil(double base, double value)
{
    return std::ceil(logBase(base, value + 1.0));
}

// No tick a double can hold comes near this exponent, but a base barely above one makes the log
// unbounded, and an exponent is turned into a step count.
constexpr double kExponentLimit = 1e15;

double clampedExponent(double exponent)
{
    return std::max(-kExponentLimit, std::min(exponent, kExponentLimit));
}

bool isMajorTick(const QVector<double>& ticks, double value)
{
    return std::any_of(ticks.cbegin(), ticks.cend(),
                       [value](double tick) { return qFuzzyCompare(tick, value); });
}

// Whether the base's decades are the integer mantissas 2...base-1: ten, or a small integer like
// three. A base barely above one has no room for a mantissa, and one past ten would need more than
// nine of them.
bool hasMantissaDecades(double base)
{
    return base >= 3.0 && base <= 10.0 && base == std::floor(base);
}
} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPAxisTickerSymlog
////////////////////////////////////////////////////////////////////////////////////////////////////
/*! \class QCPAxisTickerSymlog
  \brief Specialized axis ticker suited for symlog axes

  This QCPAxisTicker subclass generates the ticks of a symlog scale (\ref QCPScale::symlog): the
  decades matplotlib's SymmetricalLogLocator places, that is from the decade holding the scale's
  threshold outward on both sides of zero, plus zero when the range straddles the band. A range wide
  enough to hold more decades than the axis has ticks is thinned by the same step formula the log
  ticker uses, so about \ref setTickCount ticks remain. A decade the thinning skips keeps a sub
  tick, so the grid stays complete where the labels are not.

  Outside the band the mapping is logarithmic, and the sub ticks are the decades' mantissas
  (2...9 x base^k) when the base is a small integer; a base barely above one has no integer
  mantissa to show, so each gap between ticks is subdivided linearly there instead. Inside the band
  it is linear, so the sub ticks split the threshold's decade into as many parts as that decade's
  sub tick count asks for (fifths in base 10, thirds in base 3), the sub grid a linear axis draws
  over one decade. Both ends of the band are marked: they are where the mapping bends.

  The ticker reads the threshold and the base of the scale it was built with. QCPAxisBase::setScale
  installs it for a symlog scale, and a later QCPAxisBase::setTicker wins.

  A range that holds fewer than two decades, three to four say, has no symlog tick to show. It gets
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
    return qPow(mScale.base(), clampedExponent(decadeFloor(mScale.base(), mScale.threshold())));
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
    const int visible
        = int(std::count_if(ticks.cbegin(), ticks.cend(), [&range](double tick)
                            { return tick >= range.lower && tick <= range.upper; }));
    if (visible >= 2)
        return ticks;
    mLinearFallback = true;
    return QCPAxisTicker::createTickVector(tickStep, range);
}

/*! \internal

  The decades of \a range: from the decade at or below the threshold outward, on each side the
  range reaches, plus zero when the range straddles the band. The exponent bounds are
  matplotlib's, including its \c +1 on the outer end, which keeps a tick just outside the range
  for the axis to trim.

  The exponents are computed and thinned in double, and only the kept ones are generated: a base
  barely above one spans exponents far past what an int holds, so materialising them all would
  hang the replot.

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

    // matplotlib's exponent bounds, exclusive at the upper end as arange's end is. A base barely
    // above one puts them far past the exponent limit, where the powers saturate, so they are
    // clamped before they are counted or walked.
    const double negativeFirst
        = clampedExponent(decadeFloor(base, std::max(threshold, -range.upper)));
    const double negativeLast = clampedExponent(decadeCeil(base, -range.lower));
    const double positiveFirst = clampedExponent(decadeFloor(base, std::max(threshold, range.lower)));
    const double positiveLast = clampedExponent(decadeCeil(base, range.upper));

    // Only the decades past the threshold are counted for the thinning: the one holding the
    // threshold sits in the band and is always shown.
    const double firstOutOfBand = clampedExponent(decadeFloor(base, threshold) + 1.0);
    double outOfBandCount = 0;
    if (negativeSide)
        outOfBandCount += std::max(0.0, negativeLast - std::max(negativeFirst, firstOutOfBand));
    if (positiveSide)
        outOfBandCount += std::max(0.0, positiveLast - std::max(positiveFirst, firstOutOfBand));
    // A range that shows no out-of-band decade at all, five to five say, still needs a stride:
    // cleanMantissa(0) is NaN and casts to int as undefined behaviour.
    const double counted = std::max(outOfBandCount, 1.0);
    const double stride = std::max(
        1.0,
        std::floor(std::min(cleanMantissa(counted / double(mTickCount)), kExponentLimit)));

    QVector<double> ticks;
    const auto appendSide = [&ticks, base, stride](double first, double last, bool mirrored)
    {
        const double count = std::max(0.0, std::ceil((last - first) / stride));
        std::vector<double> values;
        values.reserve(std::size_t(std::min(count, 64.0)) + 1);
        for (double step = 0; step < count; step += 1.0)
        {
            const double value = qPow(base, clampedExponent(first + step * stride));
            if (std::isfinite(value) && value > 0)
                values.push_back(value);
        }
        // Clamping collapses the exponents past the limit onto one power.
        values.erase(std::unique(values.begin(), values.end()), values.end());
        if (mirrored)
            std::reverse(values.begin(), values.end());
        for (double value : values)
            ticks.append(mirrored ? -value : value);
    };
    if (negativeSide)
        appendSide(negativeFirst, negativeLast, true);
    if (zero)
        ticks.append(0);
    if (positiveSide)
        appendSide(positiveFirst, positiveLast, false);
    return ticks;
}

/*! \internal

  Appends the band's sub grid to \a candidates: the multiples of a \a subTickCount-th of a decade,
  as a linear axis draws one, capped to a readable density, and the two ends of the band, where the
  mapping bends.
*/
void QCPAxisTickerSymlog::addBandSubTicks(QVector<double>& candidates, int subTickCount) const
{
    const double threshold = mScale.threshold();
    const double maxCells = 4.0 * double(subTickCount + 1);
    // A threshold so small that its decade underflows to zero leaves no band grid to divide up:
    // fall back to the coarsest grid the band allows, and draw no cell when even that is zero.
    double bandStep = decadeStep() / double(subTickCount + 1);
    if (!(bandStep > 0.0) || threshold / bandStep > maxCells)
        bandStep = threshold / maxCells;
    candidates.append(threshold);
    candidates.append(-threshold);
    if (!(bandStep > 0.0))
        return;
    const int bandCells = int(std::floor(qMin(threshold / bandStep, maxCells) + 1e-9));
    for (int k = -bandCells; k <= bandCells; ++k)
        candidates.append(double(k) * bandStep);
}

/*! \internal

  Appends the sub ticks outside the band to \a candidates, both signs. When the base has integer
  mantissas and the drawn decades are few, the sub ticks are those mantissas, plus the decades the
  thinning skipped; otherwise the gaps between adjacent \a ticks are subdivided \a subTickCount
  times, as a thinned log axis does, and a skipped decade inside a short gap still gets its own
  sub tick. Both branches generate a bounded number of values.
*/
void QCPAxisTickerSymlog::addOutsideSubTicks(std::vector<double>& candidates, int subTickCount,
                                             const QVector<double>& ticks) const
{
    const double base = mScale.base();
    const double threshold = mScale.threshold();
    const double outerMagnitude = std::max(std::abs(ticks.front()), std::abs(ticks.back()));
    // The span is measured over the decades that are drawn, down to the one below the first
    // tick (its mantissas are visible too), not from the band's decade: a threshold of 1e-15 on a
    // range of 1e5 to 1e7 shows three decades and takes their mantissas, where the band's own
    // decade would push the span past the cap.
    double shownMagnitude = outerMagnitude;
    for (double tick : ticks)
        if (const double magnitude = std::abs(tick); magnitude > threshold && magnitude < shownMagnitude)
            shownMagnitude = magnitude;
    const double firstShownDecade = decadeFloor(base, shownMagnitude);
    const double firstExponent
        = std::max(decadeFloor(base, threshold), firstShownDecade - 1.0);
    const double lastExponent = decadeCeil(base, outerMagnitude);

    if (hasMantissaDecades(base) && lastExponent - firstExponent <= 20.0)
    {
        const int topMantissa = int(std::lround(base)) - 1;
        for (double exponent = firstExponent; exponent < lastExponent; exponent += 1.0)
        {
            const double decade = qPow(base, clampedExponent(exponent));
            if (!std::isfinite(decade) || decade <= 0)
                continue;
            if (isMajorTick(ticks, decade) || isMajorTick(ticks, -decade))
            {
                for (int mantissa = 2; mantissa <= topMantissa; ++mantissa)
                {
                    candidates.push_back(double(mantissa) * decade);
                    candidates.push_back(-double(mantissa) * decade);
                }
            }
            else
            {
                // A decade the thinning skipped is neither a major nor a label, so it becomes a
                // sub tick of its own and the grid keeps its decade marks.
                candidates.push_back(decade);
                candidates.push_back(-decade);
            }
        }
        return;
    }

    for (int i = 1; i < ticks.size(); ++i)
    {
        const double from = ticks.at(i - 1);
        const double to = ticks.at(i);
        if ((from < 0) != (to < 0) || qFuzzyIsNull(from) || qFuzzyIsNull(to))
            continue;
        // A gap the thinning made one, two or a few decades wide keeps its decade marks, the way
        // the mantissa branch does; a wider one is subdivided instead.
        const double skipped = std::round(logBase(base, std::abs(to / from))) - 1.0;
        if (skipped >= 1.0 && skipped <= double(subTickCount))
        {
            const double sign = to < 0 ? -1.0 : 1.0;
            const double first = decadeFloor(base, std::abs(from)) + 1.0;
            for (double step = 0; step < skipped; step += 1.0)
            {
                const double decade = qPow(base, clampedExponent(first + step));
                if (std::isfinite(decade) && decade > 0)
                    candidates.push_back(sign * decade);
            }
            continue;
        }
        const double step = (to - from) / double(subTickCount + 1);
        for (int k = 1; k <= subTickCount; ++k)
            candidates.push_back(from + double(k) * step);
    }
}

/*! \internal

  Returns the sub ticks of \a ticks: the band's linear grid, plus the mantissas of the decades
  outside it, on both sides. A range that fell back to evenly spaced ticks keeps the base class's
  sub ticks.

  \seebaseclassmethod
*/
QVector<double> QCPAxisTickerSymlog::createSubTickVector(int subTickCount,
                                                         const QVector<double>& ticks)
{
    if (mLinearFallback || ticks.size() < 2 || subTickCount < 1)
        return QCPAxisTicker::createSubTickVector(subTickCount, ticks);

    // The band is one decade wide, so it takes a decade's sub grid, not the grid of the (wider)
    // tick step the axis is on.
    QVector<double> candidates;
    const double decade = decadeStep();
    // A base near one can underflow that decade to zero, where the base class's mantissa
    // arithmetic is undefined: the band then takes the count a decade of ten asks for.
    const int bandSubTicks
        = decade > 0 && std::isfinite(decade) ? QCPAxisTicker::getSubTickCount(decade) : 4;
    addBandSubTicks(candidates, bandSubTicks);

    std::vector<double> outside;
    addOutsideSubTicks(outside, subTickCount, ticks);
    for (double value : outside)
    {
        if (!(std::abs(value) > mScale.threshold()))
            continue;
        if (isMajorTick(ticks, value))
            continue;
        candidates.append(value);
    }
    std::sort(candidates.begin(), candidates.end());

    QVector<double> subTicks;
    subTicks.reserve(candidates.size());
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
