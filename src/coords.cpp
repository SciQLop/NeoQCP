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

template <bool KeyLog, bool ValueLog>
void QCPTransform::mapRange(std::span<const double> keys, std::span<const double> values,
                            std::span<QPointF> out) const
{
    const std::size_t count = std::min({ keys.size(), values.size(), out.size() });
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
    for (std::size_t i = 0; i < count; ++i)
    {
        double keyPixel;
        double valuePixel;
        if constexpr (KeyLog)
            keyPixel = (keys[i] > 0.0) == keyPositive
                ? keyLogSlope * std::log(std::abs(keys[i])) + keyLogIntercept
                : keyAxis.toPixel(keys[i]); // the sign QCPAxis draws 200 px outside the rect
        else
            keyPixel = keySlope * keys[i] + keyIntercept;
        if constexpr (ValueLog)
            valuePixel = (values[i] > 0.0) == valuePositive
                ? valueLogSlope * std::log(std::abs(values[i])) + valueLogIntercept
                : valueAxis.toPixel(values[i]);
        else
            valuePixel = valueSlope * values[i] + valueIntercept;
        out[i] = keyIsX ? QPointF(keyPixel, valuePixel) : QPointF(valuePixel, keyPixel);
    }
}

void QCPTransform::map(std::span<const double> keys, std::span<const double> values,
                       std::span<QPointF> out) const
{
    if (!mKey.usable || !mValue.usable)
    {
        const QPointF none(std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::quiet_NaN());
        std::fill_n(out.begin(), std::min({ keys.size(), values.size(), out.size() }), none);
        return;
    }
    if (mKey.logarithmic)
    {
        if (mValue.logarithmic)
            mapRange<true, true>(keys, values, out);
        else
            mapRange<true, false>(keys, values, out);
    }
    else
    {
        if (mValue.logarithmic)
            mapRange<false, true>(keys, values, out);
        else
            mapRange<false, false>(keys, values, out);
    }
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
    if (mKey.slope != other.mKey.slope || mValue.slope != other.mValue.slope)
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
