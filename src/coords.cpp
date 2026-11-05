/***************************************************************************
**                                                                        **
**  NeoQCP                                                                **
**                                                                        **
**  This program is free software: you can redistribute it and/or modify  **
**  it under the terms of the GNU General Public License as published by  **
**  the Free Software Foundation, either version 3 of the License, or     **
**  (at your option) any later version.                                   **
**                                                                        **
****************************************************************************/

#include "coords.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "axis/axis.h"
#include "layoutelements/layoutelement-axisrect.h"

namespace
{
constexpr double kOffRange = 200.0; // QCPAxis draws a log-axis value of the wrong sign this far out
} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPTransform

QCPTransform::QCPTransform(const QCPAxis* keyAxis, const QCPAxis* valueAxis)
        : mKeyAxis(const_cast<QCPAxis*>(keyAxis)), mValueAxis(const_cast<QCPAxis*>(valueAxis))
{
    if (!keyAxis || !valueAxis || keyAxis->orientation() == valueAxis->orientation())
        return; // same orientation: there is no pixel for one of the two, pixelPosition() would
                // report 0
    mKey.snapshot(keyAxis);
    mValue.snapshot(valueAxis);
    mKey.prepare();
    mValue.prepare();
}

namespace
{
//! A flat or single-point recording has no extent on one side: give it a small one, so it can be
//! fitted to an icon instead of producing an invalid transform and an empty legend.
void widenDegenerateSide(double& lower, double& upper, double otherSpan)
{
    if (lower < upper || !std::isfinite(lower) || !std::isfinite(upper))
        return;
    const double half = std::isfinite(otherSpan) && otherSpan > 0 ? 0.01 * otherSpan : 0.5;
    const double centre = lower;
    lower = centre - half;
    upper = centre + half;
}
} // namespace

QCPTransform QCPTransform::fitted(const QCPDataBox& box, const QRectF& rect)
{
    QCPTransform transform;
    if (rect.isEmpty())
        return transform;
    QCPDataBox widened = box;
    widenDegenerateSide(widened.keyLower, widened.keyUpper, box.valueUpper - box.valueLower);
    widenDegenerateSide(widened.valueLower, widened.valueUpper, box.keyUpper - box.keyLower);
    if (!(widened.keyLower < widened.keyUpper) || !(widened.valueLower < widened.valueUpper))
        return transform;
    const auto fit
        = [](QCPTransform::Axis& axis, double lower, double upper, const QRectF& r, bool vertical)
    {
        axis.vertical = vertical;
        axis.reversed = false;
        axis.lower = lower;
        axis.upper = upper;
        axis.left = r.left();
        axis.top = r.top();
        axis.right = r.right();
        axis.bottom = r.bottom();
        axis.origin = vertical ? r.bottom() : r.left();
        axis.length = vertical ? r.height() : r.width();
        axis.prepare();
    };
    fit(transform.mKey, widened.keyLower, widened.keyUpper, rect, false);
    fit(transform.mValue, widened.valueLower, widened.valueUpper, rect, true);
    return transform;
}

QCPRatioTransform QCPRatioTransform::fitted(const QRectF& rect)
{
    QCPRatioTransform transform;
    transform.mLeft = rect.left();
    transform.mTop = rect.top();
    transform.mWidth = rect.width();
    transform.mHeight = rect.height();
    return transform;
}

void QCPTransform::Axis::snapshot(const QCPAxis* axis)
{
    vertical = axis->orientation() == Qt::Vertical;
    logarithmic = axis->scaleType() == QCPAxis::stLogarithmic;
    reversed = axis->rangeReversed();
    lower = axis->range().lower;
    upper = axis->range().upper;
    const QCPAxisRect* rect = axis->axisRect();
    if (!rect)
        return;
    left = rect->left();
    top = rect->top();
    right = rect->right();
    bottom = rect->bottom();
    origin = vertical ? bottom : left;
    length = vertical ? rect->height() : rect->width();
}

void QCPTransform::Axis::prepare()
{
    const double size = upper - lower;
    if (!(length > 0.0) || !(size > 0.0))
        return;
    const double direction = reversed ? -1.0 : 1.0;
    const double axisSign = vertical ? -1.0 : 1.0; // a vertical axis grows upwards on screen
    if (!logarithmic)
    {
        slope = axisSign * direction * length / size;
        intercept = origin - slope * (direction > 0.0 ? lower : upper);
        usable = true;
        return;
    }
    const double ratio = upper / lower;
    if (!(ratio > 0.0) || qFuzzyCompare(ratio, 1.0))
        return;
    positive = lower > 0.0 ? 1.0 : -1.0;
    const double base = direction > 0.0 ? lower : upper;
    logSlope = axisSign * direction * length / std::log(ratio);
    logIntercept = origin - logSlope * std::log(std::abs(base));
    usable = !qIsNaN(logSlope) && !qIsInf(logSlope);
}

double QCPTransform::Axis::toPixel(double value) const
{
    if (!logarithmic)
        return slope * value + intercept;
    // QCPAxis::coordToPixel puts a value of the range's opposite sign 200 pixels outside the rect;
    // a replay must draw it where the axis would.
    if (value >= 0.0 && upper < 0.0)
    {
        if (reversed)
            return vertical ? bottom + kOffRange : left - kOffRange;
        return vertical ? top - kOffRange : right + kOffRange;
    }
    if (value <= 0.0 && upper >= 0.0)
    {
        if (reversed)
            return vertical ? top - kOffRange : right + kOffRange;
        return vertical ? bottom + kOffRange : left - kOffRange;
    }
    return logSlope * std::log(std::abs(value)) + logIntercept;
}

double QCPTransform::Axis::toValue(double pixel) const
{
    if (!logarithmic)
        return (pixel - intercept) / slope;
    return positive * std::exp((pixel - logIntercept) / logSlope);
}

QCP::Pixels QCPTransform::map(QCP::Data data) const
{
    if (!mKey.usable || !mValue.usable)
        return QCP::Pixels { std::numeric_limits<double>::quiet_NaN(),
                             std::numeric_limits<double>::quiet_NaN() };
    const double keyPixel = mKey.toPixel(data.x);
    const double valuePixel = mValue.toPixel(data.y);
    return mKey.vertical ? QCP::Pixels { valuePixel, keyPixel }
                         : QCP::Pixels { keyPixel, valuePixel };
}

QCP::Data QCPTransform::inverse(QCP::Pixels pixels) const
{
    if (!mKey.usable || !mValue.usable)
        return QCP::Data { std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::quiet_NaN() };
    const double keyPixel = mKey.vertical ? pixels.y : pixels.x;
    const double valuePixel = mKey.vertical ? pixels.x : pixels.y;
    return QCP::Data { mKey.toValue(keyPixel), mValue.toValue(valuePixel) };
}

template <bool KeyLog, bool ValueLog, typename KeyAt, typename ValueAt>
void QCPTransform::mapRange(std::size_t count, KeyAt keyAt, ValueAt valueAt,
                            std::span<QPointF> out) const
{
    const Axis& keyAxis = mKey;
    const Axis& valueAxis = mValue;
    const double keySlope = keyAxis.slope;
    const double keyIntercept = keyAxis.intercept;
    const double keyLogSlope = keyAxis.logSlope;
    const double keyLogIntercept = keyAxis.logIntercept;
    const double valueSlope = valueAxis.slope;
    const double valueIntercept = valueAxis.intercept;
    const double valueLogSlope = valueAxis.logSlope;
    const double valueLogIntercept = valueAxis.logIntercept;
    const bool keyPositive = keyAxis.positive > 0.0;
    const bool valuePositive = valueAxis.positive > 0.0;
    const bool keyIsX = !keyAxis.vertical;
    QPointF* __restrict o = out.data();
    for (std::size_t i = 0; i < count; ++i)
    {
        const double key = keyAt(i);
        const double value = valueAt(i);
        double keyPixel;
        double valuePixel;
        if constexpr (KeyLog)
            keyPixel = (key > 0.0) == keyPositive
                ? keyLogSlope * std::log(std::abs(key)) + keyLogIntercept
                : keyAxis.toPixel(key); // the sign QCPAxis draws 200 px outside the rect
        else
            keyPixel = keySlope * key + keyIntercept;
        if constexpr (ValueLog)
            valuePixel = (value > 0.0) == valuePositive
                ? valueLogSlope * std::log(std::abs(value)) + valueLogIntercept
                : valueAxis.toPixel(value);
        else
            valuePixel = valueSlope * value + valueIntercept;
        o[i] = keyIsX ? QPointF(keyPixel, valuePixel) : QPointF(valuePixel, keyPixel);
    }
}

// One dispatch for every batch flavour: the four log/linear instantiations are written once.
template <typename KeyAt, typename ValueAt>
void QCPTransform::mapAny(std::size_t count, KeyAt keyAt, ValueAt valueAt,
                          std::span<QPointF> out) const
{
    if (!mKey.usable || !mValue.usable)
    {
        const QPointF none(std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::quiet_NaN());
        std::fill_n(out.begin(), count, none);
        return;
    }
    if (mKey.logarithmic)
    {
        if (mValue.logarithmic)
            mapRange<true, true>(count, keyAt, valueAt, out);
        else
            mapRange<true, false>(count, keyAt, valueAt, out);
    }
    else
    {
        if (mValue.logarithmic)
            mapRange<false, true>(count, keyAt, valueAt, out);
        else
            mapRange<false, false>(count, keyAt, valueAt, out);
    }
}

void QCPTransform::map(std::span<const double> keys, std::span<const double> values,
                       std::span<QPointF> out) const
{
    // Raw restrict pointers captured by value: the stores to out cannot alias the inputs, so the
    // compiler keeps the loop free of reloads (capturing the spans by reference cost 13 %).
    const double* __restrict k = keys.data();
    const double* __restrict v = values.data();
    mapAny(
        std::min({ keys.size(), values.size(), out.size() }), [k](std::size_t i) { return k[i]; },
        [v](std::size_t i) { return v[i]; }, out);
}

void QCPTransform::map(std::span<const QPointF> data, std::span<QPointF> out) const
{
    // data[i].x() is the key, data[i].y() the value. out must not overlap data (restrict).
    const QPointF* __restrict d = data.data();
    mapAny(
        std::min(data.size(), out.size()), [d](std::size_t i) { return d[i].x(); },
        [d](std::size_t i) { return d[i].y(); }, out);
}

bool QCPTransform::isTranslationOf(const QCPTransform& other, QPointF* delta) const
{
    if (!valid() || !other.valid() || mKey.vertical != other.mKey.vertical)
        return false;
    // Only a linear mapping turns a range shift into a pixel shift: a log axis's shift is
    // multiplicative and is reported as "not a translation", which costs a repaint and can never
    // draw a wrong frame.
    if (mKey.logarithmic || mValue.logarithmic || other.mKey.logarithmic
        || other.mValue.logarithmic)
        return false;
    // The two mappings are translations only when they have the same scale. A drag adds the same
    // diff to both range ends, but the addition rounds, so the two spans can differ by an ulp:
    // allow the scale to move by less than a thousandth of a pixel across the visible span
    // (|delta slope| times the span is exactly that drift).
    const auto sameScale = [](const Axis& a, const Axis& b)
    { return std::abs(a.slope - b.slope) * std::abs(a.upper - a.lower) < 1e-3; };
    if (!sameScale(mKey, other.mKey) || !sameScale(mValue, other.mValue))
        return false; // a zoom: the data-to-pixel scale changed
    const QCP::Pixels here = map(QCP::Data { 0.0, 0.0 });
    const QCP::Pixels there = other.map(QCP::Data { 0.0, 0.0 });
    if (delta)
        *delta = QPointF(here.x - there.x, here.y - there.y);
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPRatioTransform

QCPRatioTransform::QCPRatioTransform(const QCPAxisRect* axisRect)
        : mAxisRect(const_cast<QCPAxisRect*>(axisRect))
{
    if (!axisRect)
        return;
    mLeft = axisRect->left();
    mTop = axisRect->top();
    mWidth = axisRect->width();
    mHeight = axisRect->height();
}

QCP::Pixels QCPRatioTransform::map(QCP::AxisRatio ratio) const
{
    if (!valid())
        return QCP::Pixels { std::numeric_limits<double>::quiet_NaN(),
                             std::numeric_limits<double>::quiet_NaN() };
    // The same arithmetic as QCPItemPosition's ptAxisRectRatio branch.
    return QCP::Pixels { mLeft + ratio.x * mWidth, mTop + ratio.y * mHeight };
}

QCP::AxisRatio QCPRatioTransform::inverse(QCP::Pixels pixels) const
{
    if (!valid())
        return QCP::AxisRatio { std::numeric_limits<double>::quiet_NaN(),
                                std::numeric_limits<double>::quiet_NaN() };
    return QCP::AxisRatio { (pixels.x - mLeft) / mWidth, (pixels.y - mTop) / mHeight };
}

bool QCPRatioTransform::isTranslationOf(const QCPRatioTransform& other, QPointF* delta) const
{
    if (!valid() || !other.valid() || mWidth != other.mWidth || mHeight != other.mHeight)
        return false;
    if (delta)
        *delta = QPointF(mLeft - other.mLeft, mTop - other.mTop);
    return true;
}
