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
        : mKind(kind), mBase(base), mThreshold(threshold), mLinearScale(linearScale)
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
            return std::log(std::abs(value)) / std::log(mBase);
        case Kind::Symlog:
        {
            const double slope = symlogSlope(mBase, mLinearScale);
            const double magnitude = std::abs(value);
            if (magnitude <= mThreshold)
                return value * slope;
            const double above = std::log(magnitude / mThreshold) / std::log(mBase);
            return std::copysign(mThreshold * (slope + above), value);
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
            const double slope = symlogSlope(mBase, mLinearScale);
            const double magnitude = std::abs(s);
            if (magnitude <= mThreshold * slope)
                return s / slope;
            return std::copysign(mThreshold * std::pow(mBase, magnitude / mThreshold - slope), s);
        }
    }
    return s;
}

QCPRange QCPScale::sanitized(const QCPRange& range) const
{
    return isLog() ? range.sanitizedForLogScale() : range.sanitizedForLinScale();
}

size_t qHash(const QCPScale& scale, size_t seed)
{
    return qHashMulti(seed, static_cast<quint8>(scale.kind()), scale.base(), scale.threshold(),
                      scale.linearScale());
}
