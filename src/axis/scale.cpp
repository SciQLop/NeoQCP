#include "scale.h"
#include "../misuse.h"
#include <cmath>

namespace
{
bool aValidBase(double base)
{
    return std::isfinite(base) && base > 1.0;
}

bool aValidThreshold(double threshold)
{
    return std::isfinite(threshold) && threshold > 0;
}

bool aValidLinearScale(double linearScale)
{
    return std::isfinite(linearScale) && linearScale > 0;
}

//! a' of matplotlib's SymmetricalLogTransform.
double symlogSlope(double base, double linearScale)
{
    return linearScale / (1.0 - 1.0 / base);
}

//! The sign of a log range: the log mapping is of the magnitude, the axis owns the sign rule.
double rangeSign(const QCPRange& range)
{
    return range.upper < 0.0 ? -1.0 : 1.0;
}
} // namespace

QCPScale::QCPScale(Kind kind, double base, double threshold, double linearScale)
        : mKind(kind), mBase(base), mThreshold(threshold), mLinearScale(linearScale),
          mInvLogBase(1.0 / std::log(base)), mSymlogSlope(symlogSlope(base, linearScale))
{
}

QCPScale QCPScale::linear()
{
    return QCPScale(Kind::Linear, kDefaultBase, kDefaultThreshold, kDefaultLinearScale);
}

QCPScale QCPScale::log(double base)
{
    if (!aValidBase(base))
    {
        qcp::reportMisuse(Q_FUNC_INFO, "a log scale needs a finite base greater than 1");
        base = kDefaultBase;
    }
    return QCPScale(Kind::Log, base, kDefaultThreshold, kDefaultLinearScale);
}

QCPScale QCPScale::symlog(double threshold, double base, double linearScale)
{
    if (!aValidThreshold(threshold) || !aValidBase(base) || !aValidLinearScale(linearScale))
    {
        qcp::reportMisuse(Q_FUNC_INFO,
                          "a symlog scale needs a finite positive threshold and linear scale, "
                          "and a finite base greater than 1");
        threshold = kDefaultThreshold;
        base = kDefaultBase;
        linearScale = kDefaultLinearScale;
    }
    return QCPScale(Kind::Symlog, base, threshold, linearScale);
}

double QCPScale::toScale(double value) const
{
    switch (mKind)
    {
        case Kind::Linear:
            return value;
        case Kind::Log:
            return std::log(std::abs(value)) * mInvLogBase;
        case Kind::Symlog:
        {
            const double magnitude = std::abs(value);
            if (magnitude <= mThreshold)
                return value * mSymlogSlope;
            const double above = std::log(magnitude / mThreshold) * mInvLogBase;
            return std::copysign(mThreshold * (mSymlogSlope + above), value);
        }
    }
    return value;
}

double QCPScale::fromScale(double s) const
{
    switch (mKind)
    {
        case Kind::Linear:
            return s;
        case Kind::Log:
            return std::pow(mBase, s);
        case Kind::Symlog:
        {
            const double magnitude = std::abs(s);
            if (magnitude <= mThreshold * mSymlogSlope)
                return s / mSymlogSlope;
            const double power = magnitude / mThreshold - mSymlogSlope;
            return std::copysign(mThreshold * std::pow(mBase, power), s);
        }
    }
    return s;
}

QCPRange QCPScale::sanitized(const QCPRange& range) const
{
    return isLog() ? range.sanitizedForLogScale() : range.sanitizedForLinScale();
}

QCPRange QCPScale::shifted(const QCPRange& range, double scaleDelta) const
{
    if (isLinear())
        return QCPRange(range.lower + scaleDelta, range.upper + scaleDelta);
    const double sign = isLog() ? rangeSign(range) : 1.0;
    return QCPRange(sign * fromScale(toScale(range.lower) + scaleDelta),
                    sign * fromScale(toScale(range.upper) + scaleDelta));
}

QCPRange QCPScale::scaled(const QCPRange& range, double factor, double center) const
{
    if (isLinear())
    {
        // The axis' own linear rule, end point by end point.
        return QCPRange((range.lower - center) * factor + center,
                        (range.upper - center) * factor + center);
    }
    const double sign = isLog() ? rangeSign(range) : 1.0;
    const double at = toScale(center);
    return QCPRange(sign * fromScale(at + (toScale(range.lower) - at) * factor),
                    sign * fromScale(at + (toScale(range.upper) - at) * factor));
}

size_t qHash(const QCPScale& scale, size_t seed)
{
    return qHashMulti(seed, static_cast<quint8>(scale.kind()), scale.base(), scale.threshold(),
                      scale.linearScale());
}

QCPRange QCPScale::widened(const QCPRange& range, double fraction) const
{
    // A margin of zero keeps the fit exactly as it was, with no round trip through the scale.
    if (fraction <= 0.0)
        return range;
    const double lower = toScale(range.lower);
    const double upper = toScale(range.upper);
    if (!std::isfinite(lower) || !std::isfinite(upper)) // not a range this scale can widen (0 on a log)
        return range;
    // Log drops the sign, so a negative range arrives with its bounds swapped: order them by their
    // scale values, or the margin would move both ends inward.
    const double from = qMin(lower, upper);
    const double to = qMax(lower, upper);
    const double sign = isLog() ? rangeSign(range) : 1.0;
    const double delta = (to - from) * fraction;
    return QCPRange(sign * fromScale(from - delta), sign * fromScale(to + delta));
}

namespace
{
//! matplotlib's sticky edges: a bound the widening pushed past a sticky value goes back to it.
QCPRange clampedToSticky(const QCPRange& range, const QCPRange& fitted,
                        const QVector<double>& sticky)
{
    QCPRange clamped = range;
    for (const double value : sticky)
    {
        if (clamped.lower < value && value <= fitted.lower)
            clamped.lower = value;
        if (clamped.upper > value && value >= fitted.upper)
            clamped.upper = value;
    }
    return clamped;
}
} // namespace

QCPRange qcp::fittedRange(const QCPScale& scale, const QCPRange& fitted, const QCPRange& current,
                          double margin, bool onlyEnlarge, const QVector<double>& sticky)
{
    QCPRange range = scale.widened(fitted, margin);
    if (!sticky.isEmpty())
        range = clampedToSticky(range, fitted, sticky);
    if (onlyEnlarge)
        range.expand(current);
    if (QCPRange::validRange(range))
        return range;
    // A constant plottable in this dimension: recentre the current range on it, no margin.
    const double center = (fitted.lower + fitted.upper) * 0.5;
    if (!scale.isLog())
        return QCPRange(center - current.size() / 2.0, center + current.size() / 2.0);
    // Log: the same ratio on both sides of the centre, so the range keeps its sign.
    return QCPRange(center / qSqrt(current.upper / current.lower),
                    center * qSqrt(current.upper / current.lower));
}

bool qcp::validRescaleMargin(double fraction)
{
    return std::isfinite(fraction) && fraction >= 0.0;
}
