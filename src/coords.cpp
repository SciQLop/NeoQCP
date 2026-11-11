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
#include "misuse.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "axis/axis.h"
#include "layoutelements/layoutelement-axisrect.h"
#include "polar/angularaxis.h"
#include "polar/polar-area.h"
#include "polar/radialaxis.h"
#include "vector2d.h"

namespace
{
constexpr double kOffRange = 200.0; // QCPAxis draws a log-axis value of the wrong sign this far out
constexpr double kPolarClamp = 1.05; // the radial fraction QCPTransform::polar clamps to

QCP::Pixels noPixel()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    return QCP::Pixels { nan, nan };
}

QCP::Data noData()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    return QCP::Data { nan, nan };
}
} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPCustomMapping

void QCPCustomMapping::map(std::span<const QPointF> data, std::span<QPointF> out) const
{
    // The default split costs one copy per span; a mapping that can do better overrides this.
    const std::size_t count = std::min(data.size(), out.size());
    std::vector<double> keys(count);
    std::vector<double> values(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        keys[i] = data[i].x();
        values[i] = data[i].y();
    }
    map(std::span<const double>(keys), std::span<const double>(values), out.first(count));
}

QCP::Pixels QCPCustomMapping::mapPoint(QCP::Data data) const
{
    const double keys[1] = { data.x };
    const double values[1] = { data.y };
    QPointF out[1];
    map(std::span<const double>(keys), std::span<const double>(values), std::span<QPointF>(out));
    return QCP::Pixels { out[0].x(), out[0].y() };
}

bool QCPCustomMapping::equivalentTo(const QCPCustomMapping& other, QPointF* delta) const
{
    Q_UNUSED(other);
    Q_UNUSED(delta);
    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// QCPTransform

// The variant's default constructor needs the alternatives complete, which they are only here.
QCPTransform::QCPTransform()
        : mFrame(std::in_place_type<Cartesian>)
{
}

QCPTransform::QCPTransform(const QCPAxis* keyAxis, const QCPAxis* valueAxis)
        : QCPTransform()
{
    if (!keyAxis || !valueAxis || keyAxis->orientation() == valueAxis->orientation())
        return; // same orientation: there is no pixel for one of the two, pixelPosition() would
                // report 0
    Cartesian& frame = std::get<Cartesian>(mFrame);
    frame.key.snapshot(keyAxis);
    frame.value.snapshot(valueAxis);
    frame.prepare();
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
    const auto fit =
        [](QCPTransform::Snapshot& axis, double lower, double upper, const QRectF& r, bool vertical)
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
    Cartesian& frame = std::get<Cartesian>(transform.mFrame);
    fit(frame.key.self, widened.keyLower, widened.keyUpper, rect, false);
    fit(frame.value.self, widened.valueLower, widened.valueUpper, rect, true);
    return transform;
}

QCPTransform QCPTransform::polar(const QCPPolarAxisAngular& angular,
                                 const QCPPolarAxisRadial& radial)
{
    QCP_CHECK_OR_RETURN(radial.area() == angular.area(),
                        "the two polar axes belong to two areas: one disc has one centre",
                        QCPTransform());
    QCPTransform transform;
    // A polar frame never reads the axes again: everything it maps is copied here.
    transform.mFrame.emplace<Polar>().take(angular, radial);
    return transform;
}

QCPTransform QCPTransform::custom(std::shared_ptr<const QCPCustomMapping> mapping, quint64 revision)
{
    QCPTransform transform;
    Custom& custom = transform.mFrame.emplace<Custom>();
    custom.mapping = std::move(mapping);
    custom.revision = revision;
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

void QCPTransform::Snapshot::take(const QCPAxis* axis)
{
    vertical = axis->orientation() == Qt::Vertical;
    logarithmic = axis->scaleType() == QCPAxisBase::stLogarithmic;
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

void QCPTransform::Snapshot::prepare()
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

double QCPTransform::Snapshot::toPixel(double value) const
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

double QCPTransform::Snapshot::toValue(double pixel) const
{
    if (!usable)
        return qQNaN();
    if (!logarithmic)
        return (pixel - intercept) / slope;
    return positive * std::exp((pixel - logIntercept) / logSlope);
}

double QCPTransform::Snapshot::fractionOf(double pixel) const
{
    return vertical ? (bottom - pixel) / length : (pixel - left) / length;
}

double QCPTransform::Snapshot::pixelAtFraction(double fraction) const
{
    return vertical ? bottom - fraction * length : left + fraction * length;
}

void QCPTransform::Axis::snapshot(const QCPAxis* axis)
{
    self.take(axis);
    link = axis->link();
    if (!link || !link->source)
        return;
    // Only cartesian axes link, so a link source always is one.
    const auto* linkedSource = static_cast<const QCPAxis*>(link->source.data());
    Q_ASSERT(linkedSource);
    source.take(linkedSource);
}

void QCPTransform::Axis::prepare()
{
    self.prepare();
    source.prepare();
}

bool QCPTransform::Axis::usable() const
{
    if (link)
        return self.length > 0.0 && source.usable;
    return self.usable;
}

double QCPTransform::Axis::toPixel(double value) const
{
    if (!link)
        return self.toPixel(value);
    if (!source.usable || !link->inverse)
        return qQNaN();
    // The source maps inverse(value) on its own rect; the fraction along the source is the
    // fraction along this axis, so the same data fills a rect of any size. A value the source
    // draws 200 px outside its rect lands 200 px * (own length / source length) outside this one.
    return self.pixelAtFraction(source.fractionOf(source.toPixel(link->inverse(value))));
}

double QCPTransform::Axis::toValue(double pixel) const
{
    if (!link)
        return self.toValue(pixel);
    if (!source.usable || !link->forward)
        return qQNaN();
    // The inverse of toPixel: where the pixel is along this axis, mapped on the source, and
    // carried back through the link's forward function.
    return link->forward(source.toValue(source.pixelAtFraction(self.fractionOf(pixel))));
}

void QCPTransform::Cartesian::prepare()
{
    key.prepare();
    value.prepare();
}

void QCPTransform::Polar::take(const QCPPolarAxisAngular& angular, const QCPPolarAxisRadial& radial)
{
    const QCPPolarArea* area = angular.area();
    if (!area)
        return;
    centreX = area->center().x();
    centreY = area->center().y();
    radius = area->radius();
    const QCPRange angularRange = angular.range();
    angleLower = angularRange.lower;
    angleUpper = angularRange.upper;
    // coordToAngleRad(lower) is the axis' start angle, without reading its private members.
    startAngle = angular.coordToAngleRad(angularRange.lower);
    startReversed = angular.rangeReversed();
    const QCPRange radialRange = radial.range();
    lower = radialRange.lower;
    upper = radialRange.upper;
    logarithmic = radial.scaleType() == QCPAxisBase::stLogarithmic;
    reversed = radial.rangeReversed();
}

bool QCPTransform::Polar::usable() const
{
    if (!(radius >= 1.0) || !(angleUpper > angleLower) || !(upper > lower))
        return false;
    return !logarithmic || lower > 0.0 || upper < 0.0;
}

double QCPTransform::Polar::rawFraction(double value) const
{
    if (!logarithmic)
        return (reversed ? upper - value : value - lower) / (upper - lower);
    if (value == 0.0 || (value > 0.0) != (lower > 0.0))
        return qQNaN(); // the range's opposite sign has no log position
    const double ratio = upper / lower;
    return reversed ? qLn(upper / value) / qLn(ratio) : qLn(value / lower) / qLn(ratio);
}

double QCPTransform::Polar::fraction(double value) const
{
    return qBound(0.0, rawFraction(value), kPolarClamp);
}

double QCPTransform::Polar::valueAt(double fraction) const
{
    if (!logarithmic)
        return reversed ? upper - fraction * (upper - lower) : lower + fraction * (upper - lower);
    const double ratio = upper / lower;
    return reversed ? upper / qPow(ratio, fraction) : lower * qPow(ratio, fraction);
}

double QCPTransform::polarFraction(double value) const
{
    const auto* polar = std::get_if<Polar>(&mFrame);
    return polar ? polar->rawFraction(value) : qQNaN();
}

double QCPTransform::polarValue(double fraction) const
{
    const auto* polar = std::get_if<Polar>(&mFrame);
    return polar ? polar->valueAt(fraction) : qQNaN();
}

QPointF QCPTransform::Polar::toPixel(double angleCoord, double value) const
{
    const double radiusPixel = fraction(value) * radius;
    const double theta = startAngle
        + (angleCoord - angleLower) / (angleUpper - angleLower)
            * (startReversed ? -2.0 * M_PI : 2.0 * M_PI);
    return QPointF(centreX + qCos(theta) * radiusPixel, centreY + qSin(theta) * radiusPixel);
}

QCP::Data QCPTransform::Polar::toData(QPointF pixel) const
{
    // The radial axis' pixelToCoord arithmetic, written on the snapshot.
    const QCPVector2D offset(pixel - QPointF(centreX, centreY));
    const double radiusPixel = offset.length();
    const double angleCoord = radiusPixel > 0.0 ? angleLower
            + (offset.angle() - startAngle) / (startReversed ? -2.0 * M_PI : 2.0 * M_PI)
                * (angleUpper - angleLower)
                                                : angleLower;
    double value;
    if (!logarithmic)
        value = reversed ? upper - radiusPixel / radius * (upper - lower)
                         : lower + radiusPixel / radius * (upper - lower);
    else
    {
        const double ratio = upper / lower;
        value = reversed ? qPow(ratio, -radiusPixel / radius) * upper
                         : qPow(ratio, radiusPixel / radius) * lower;
    }
    return QCP::Data { angleCoord, value };
}

QCP::Pixels QCPTransform::map(QCP::Data data) const
{
    if (!valid())
        return noPixel();
    if (const Cartesian* frame = cartesian())
    {
        const double keyPixel = frame->key.toPixel(data.x);
        const double valuePixel = frame->value.toPixel(data.y);
        return frame->key.self.vertical ? QCP::Pixels { valuePixel, keyPixel }
                                        : QCP::Pixels { keyPixel, valuePixel };
    }
    if (const Polar* polar = std::get_if<Polar>(&mFrame))
    {
        const QPointF pixel = polar->toPixel(data.x, data.y);
        return QCP::Pixels { pixel.x(), pixel.y() };
    }
    return std::get<Custom>(mFrame).mapping->mapPoint(data);
}

QCP::Data QCPTransform::inverse(QPointF pixel) const
{
    if (!valid())
        return noData();
    if (const Cartesian* frame = cartesian())
    {
        const bool vertical = frame->key.self.vertical;
        return QCP::Data { frame->key.toValue(vertical ? pixel.y() : pixel.x()),
                           frame->value.toValue(vertical ? pixel.x() : pixel.y()) };
    }
    if (const Polar* polar = std::get_if<Polar>(&mFrame))
        return polar->toData(pixel);
    return std::get<Custom>(mFrame).mapping->inverse(pixel);
}

template <bool KeyLog, bool ValueLog, typename KeyAt, typename ValueAt>
void QCPTransform::mapRange(const Cartesian& frame, std::size_t count, KeyAt keyAt, ValueAt valueAt,
                            std::span<QPointF> out) const
{
    const Snapshot& keyAxis = frame.key.self;
    const Snapshot& valueAxis = frame.value.self;
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
void QCPTransform::mapAny(const Cartesian& frame, std::size_t count, KeyAt keyAt, ValueAt valueAt,
                          std::span<QPointF> out) const
{
    // A linked axis holds a function, not a line: it has no slope to hoist out of the loop.
    if (frame.key.link || frame.value.link)
    {
        const bool keyIsX = !frame.key.self.vertical;
        QPointF* __restrict o = out.data();
        for (std::size_t i = 0; i < count; ++i)
        {
            const double keyPixel = frame.key.toPixel(keyAt(i));
            const double valuePixel = frame.value.toPixel(valueAt(i));
            o[i] = keyIsX ? QPointF(keyPixel, valuePixel) : QPointF(valuePixel, keyPixel);
        }
        return;
    }
    if (frame.key.self.logarithmic)
    {
        if (frame.value.self.logarithmic)
            mapRange<true, true>(frame, count, keyAt, valueAt, out);
        else
            mapRange<true, false>(frame, count, keyAt, valueAt, out);
    }
    else
    {
        if (frame.value.self.logarithmic)
            mapRange<false, true>(frame, count, keyAt, valueAt, out);
        else
            mapRange<false, false>(frame, count, keyAt, valueAt, out);
    }
}

void QCPTransform::mapPolar(const Polar& polar, std::size_t count, const double* keys,
                            const double* values, std::span<QPointF> out) const
{
    QPointF* o = out.data();
    for (std::size_t i = 0; i < count; ++i)
        o[i] = polar.toPixel(keys[i], values[i]);
}

void QCPTransform::mapPoints(const Polar& polar, std::size_t count, const QPointF* data,
                             std::span<QPointF> out) const
{
    QPointF* o = out.data();
    for (std::size_t i = 0; i < count; ++i)
        o[i] = polar.toPixel(data[i].x(), data[i].y());
}

void QCPTransform::map(std::span<const double> keys, std::span<const double> values,
                       std::span<QPointF> out) const
{
    const std::size_t count = std::min({ keys.size(), values.size(), out.size() });
    if (!valid())
    {
        const QPointF none(std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::quiet_NaN());
        std::fill_n(out.begin(), count, none);
        return;
    }
    if (const Cartesian* frame = cartesian())
    {
        // Raw restrict pointers captured by value: the stores to out cannot alias the inputs, so
        // the compiler keeps the loop free of reloads (capturing the spans by reference cost 13 %).
        const double* __restrict k = keys.data();
        const double* __restrict v = values.data();
        mapAny(
            *frame, count, [k](std::size_t i) { return k[i]; }, [v](std::size_t i) { return v[i]; },
            out);
        return;
    }
    if (const Polar* polar = std::get_if<Polar>(&mFrame))
    {
        mapPolar(*polar, count, keys.data(), values.data(), out);
        return;
    }
    std::get<Custom>(mFrame).mapping->map(keys.first(count), values.first(count), out.first(count));
}

void QCPTransform::map(std::span<const QPointF> data, std::span<QPointF> out) const
{
    const std::size_t count = std::min(data.size(), out.size());
    if (!valid())
    {
        const QPointF none(std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::quiet_NaN());
        std::fill_n(out.begin(), count, none);
        return;
    }
    if (const Cartesian* frame = cartesian())
    {
        // data[i].x() is the key, data[i].y() the value. out must not overlap data (restrict).
        const QPointF* __restrict d = data.data();
        mapAny(
            *frame, count, [d](std::size_t i) { return d[i].x(); },
            [d](std::size_t i) { return d[i].y(); }, out);
        return;
    }
    if (const Polar* polar = std::get_if<Polar>(&mFrame))
    {
        mapPoints(*polar, count, data.data(), out);
        return;
    }
    std::get<Custom>(mFrame).mapping->map(data.first(count), out.first(count));
}

bool QCPTransform::isTranslationOf(const QCPTransform& other, QPointF* delta) const
{
    if (kind() != other.kind() || !valid() || !other.valid())
        return false;
    const Cartesian* here = cartesian();
    if (!here && std::get_if<Polar>(&mFrame))
        return false; // a rotation is not a pan
    if (!here)
    {
        // A custom mapping knows what a pan means for it; the default says it does not. Two
        // equal mappings are the same frame: no pixel moved, so no call is needed.
        if (*this == other)
        {
            if (delta)
                *delta = QPointF(0, 0);
            return true;
        }
        const Custom& mine = std::get<Custom>(mFrame);
        return mine.mapping->equivalentTo(*std::get<Custom>(other.mFrame).mapping, delta);
    }
    const Cartesian* there = other.cartesian();
    if (here->key.self.vertical != there->key.self.vertical)
        return false;
    // A link is a function of the source, not a scaling of this axis: a source pan does not move
    // it by a constant. Never replay it as a translation.
    if (here->key.link || here->value.link || there->key.link || there->value.link)
        return false;
    // Only a linear mapping turns a range shift into a pixel shift: a log axis's shift is
    // multiplicative and is reported as "not a translation", which costs a repaint and can never
    // draw a wrong frame.
    if (here->key.self.logarithmic || here->value.self.logarithmic || there->key.self.logarithmic
        || there->value.self.logarithmic)
        return false;
    // The two mappings are translations only when they have the same scale. A drag adds the same
    // diff to both range ends, but the addition rounds, so the two spans can differ by an ulp:
    // allow the scale to move by less than a thousandth of a pixel across the visible span
    // (|delta slope| times the span is exactly that drift).
    const auto sameScale = [](const Snapshot& a, const Snapshot& b)
    { return std::abs(a.slope - b.slope) * std::abs(a.upper - a.lower) < 1e-3; };
    if (!sameScale(here->key.self, there->key.self)
        || !sameScale(here->value.self, there->value.self))
        return false; // a zoom: the data-to-pixel scale changed
    // Measure the delta where the recording is, not at data (0, 0): the tolerance above allows the
    // two slopes to differ by a thousandth of a pixel across the visible span, which at a 1.7e9
    // epoch is tens of pixels of drift at the origin. At the range's lower corner the delta is the
    // one the visible pixels moved by.
    const QCP::Data corner { here->key.self.lower, here->value.self.lower };
    const QCP::Pixels herePixel = map(corner);
    const QCP::Pixels therePixel = other.map(corner);
    if (delta)
        *delta = QPointF(herePixel.x - therePixel.x, herePixel.y - therePixel.y);
    return true;
}

bool QCPTransform::keyIsVertical() const
{
    const Cartesian* frame = cartesian();
    return frame && frame->key.self.vertical;
}

bool QCPTransform::valid() const
{
    if (const Cartesian* frame = cartesian())
        return frame->usable();
    if (const Polar* polar = std::get_if<Polar>(&mFrame))
        return polar->usable();
    return std::get<Custom>(mFrame).mapping != nullptr;
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

QCPRatioTransform::QCPRatioTransform(QPointF centre, double radius, double startAngle,
                                     double angularSpan, bool reversed)
        : mPolar(true)
        , mCentre(centre)
        , mRadius(radius)
        , mStart(startAngle)
        , mSpan(reversed ? -angularSpan : angularSpan)
{
}

QCP::Pixels QCPRatioTransform::map(QCP::AxisRatio ratio) const
{
    if (!valid())
        return noPixel();
    if (mPolar)
    {
        const double theta = mStart + ratio.x * mSpan;
        const double r = ratio.y * mRadius;
        return QCP::Pixels { mCentre.x() + std::cos(theta) * r,
                             mCentre.y() + std::sin(theta) * r };
    }
    // The same arithmetic as QCPItemPosition's ptAxisRectRatio branch.
    return QCP::Pixels { mLeft + ratio.x * mWidth, mTop + ratio.y * mHeight };
}

QCP::AxisRatio QCPRatioTransform::inverse(QPointF pixel) const
{
    if (!valid())
        return { };
    if (!mPolar)
        return { (pixel.x() - mLeft) / mWidth, (pixel.y() - mTop) / mHeight };
    const QPointF offset = pixel - mCentre;
    double x = (std::atan2(offset.y(), offset.x()) - mStart) / mSpan;
    x -= std::floor(x); // the angle is a circle: the -pi seam maps back to the other end
    const double y = mRadius > 0.0 ? std::hypot(offset.x(), offset.y()) / mRadius : 0.0;
    return { x, y };
}
