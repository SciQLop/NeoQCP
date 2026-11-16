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
    return QCPRange(fromScale(toScale(range.lower) + scaleDelta),
                    fromScale(toScale(range.upper) + scaleDelta));
}

QCPRange QCPScale::scaled(const QCPRange& range, double factor, double center) const
{
    if (isLinear())
    {
        const double newCenter = center + (range.center() - center) * factor;
        const double half = 0.5 * range.size() * std::abs(factor);
        return QCPRange(newCenter - half, newCenter + half);
    }
    const double at = toScale(center);
    return QCPRange(fromScale(at + (toScale(range.lower) - at) * factor),
                    fromScale(at + (toScale(range.upper) - at) * factor));
}

size_t qHash(const QCPScale& scale, size_t seed)
{
    return qHashMulti(seed, static_cast<quint8>(scale.kind()), scale.base(), scale.threshold(),
                      scale.linearScale());
}
