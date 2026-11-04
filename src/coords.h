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

#ifndef QCP_COORDS_H
#define QCP_COORDS_H

#include "global.h"

#include <QPointF>
#include <QPointer>
#include <span>
#include <variant>

class QCPAxis;
class QCPAxisRect;
class QCPItemAnchor;

/*! \file coords.h

  Typed positions and the transforms between the spaces a plot mixes.

  A position lives in exactly one space: data coordinates (key, value), widget pixels, or a fraction
  of the axis rect. The types are distinct, so a pixel cannot be passed where a data coordinate is
  expected, and no operator joins two spaces: a value leaves its space only through a transform.
*/

namespace QCP
{
//! A point in plot coordinates: key x, value y.
struct Data
{
    double x = 0;
    double y = 0;
};

//! A point in widget pixels, the space painter calls and item positions of type ptAbsolute use.
struct Pixels
{
    double x = 0;
    double y = 0;
};

//! A point as a fraction of the axis rect: 0,0 is its top left, 1,1 its bottom right.
struct AxisRatio
{
    double x = 0;
    double y = 0;
};

//! A displacement in pixels, the only thing that relates two points of the same space.
struct PixelOffset
{
    double dx = 0;
    double dy = 0;
};

//! A length in pixels.
struct PixelLength
{
    double value = 0;
};

//! Pixels relative to an anchor: the item position type ptAbsolute with a parent anchor.
struct AnchorOffset
{
    QCPItemAnchor* anchor = nullptr;
    PixelOffset offset;
};

//! Data along the key axis, an axis-rect fraction along the value axis: a vertical span.
struct DataXRatioY
{
    double x = 0; // plot coordinate, mapped by the key axis
    double y = 0; // axis-rect ratio
};

//! An axis-rect fraction along the key axis, data along the value axis: a horizontal span.
struct RatioXDataY
{
    double x = 0; // axis-rect ratio
    double y = 0; // plot coordinate, mapped by the value axis
};

//! A position in any of the modelled spaces.
using Position = std::variant<Data, Pixels, AxisRatio, AnchorOffset, DataXRatioY, RatioXDataY>;

//! Pixels relative to an anchor: the 2.x spelling of an anchored position.
[[nodiscard]] inline AnchorOffset operator+(QCPItemAnchor* anchor, PixelOffset offset)
{
    return AnchorOffset { anchor, offset };
}

//! Moves a pixel point.
[[nodiscard]] inline Pixels operator+(Pixels pixels, PixelOffset offset)
{
    return Pixels { pixels.x + offset.dx, pixels.y + offset.dy };
}
} // namespace QCP

/*! \brief Maps plot coordinates to pixels and back, snapshotted from two axes.

  A transform copies the ranges, scale types, reversal and the axis rect of its two axes when it is
  constructed, so replaying it never touches the axes again: valid() is the only member that looks
  at them, to say whether they still exist. A transform with an axis of the wrong orientation, no
  axis rect or an unusable range maps to NaN and reports valid() == false.

  The mapping is exactly QCPAxis::coordToPixel, including log axes, reversed axes and the two sign
  cases QCPAxis draws 200 pixels outside the rect. batchMap() runs the same arithmetic without the
  per-point type dispatch, which is what makes it several times faster than calling coordToPixel
  twice per point.
*/
class QCP_LIB_DECL QCPTransform
{
public:
    QCPTransform() = default;
    QCPTransform(const QCPAxis* keyAxis, const QCPAxis* valueAxis);

    //! Maps one data point to pixels.
    [[nodiscard]] QCP::Pixels map(QCP::Data data) const;
    //! Maps one pixel point back to data coordinates (the inverse of map, not clamped to the
    //! range).
    [[nodiscard]] QCP::Data inverse(QCP::Pixels pixels) const;
    //! Maps keys and values in bulk into \a out; stops at the shortest of the three spans.
    void map(std::span<const double> keys, std::span<const double> values,
             std::span<QPointF> out) const;
    //! Maps interleaved data (x = key, y = value) into \a out; stops at the shortest span.
    void map(std::span<const QPointF> data, std::span<QPointF> out) const;
    //! True when the two transforms differ by a pixel offset only (a pan, not a zoom); \a delta
    //! gets it.
    [[nodiscard]] bool isTranslationOf(const QCPTransform& other, QPointF* delta = nullptr) const;

    //! False when an axis is gone, the orientations are the same or the snapshot cannot map.
    [[nodiscard]] bool valid() const { return mKey.usable && mValue.usable; }

private:
    struct Axis
    {
        bool vertical = false;
        bool logarithmic = false;
        bool reversed = false;
        bool usable = false;
        double origin = 0; //!< the pixel the axis grows from: the rect's left, or its bottom
        double length = 0; //!< the pixels it spans
        double lower = 0;
        double upper = 0;
        double left = 0;
        double top = 0;
        double right = 0;
        double bottom = 0;
        double slope = 0; //!< pixels = slope * value + intercept
        double intercept = 0;
        double logSlope = 0; //!< pixels = logSlope * ln(|value|) + logIntercept
        double logIntercept = 0;
        double positive = 1; //!< the sign of the log range: the values it can map

        void snapshot(const QCPAxis* axis);
        void prepare();
        [[nodiscard]] double toPixel(double value) const;
        [[nodiscard]] double toValue(double pixel) const;
    };

    template <bool KeyLog, bool ValueLog, typename KeyAt, typename ValueAt>
    void mapRange(std::size_t count, KeyAt keyAt, ValueAt valueAt, std::span<QPointF> out) const;
    template <typename KeyAt, typename ValueAt>
    void mapAny(std::size_t count, KeyAt keyAt, ValueAt valueAt, std::span<QPointF> out) const;

    Axis mKey;
    Axis mValue;
    QPointer<QCPAxis> mKeyAxis;
    QPointer<QCPAxis> mValueAxis;
};

/*! \brief Maps an axis-rect fraction to pixels and back.

  Same shape as QCPTransform for the ratio space: snapshotted at construction, valid() reports a
  missing or empty axis rect, isTranslationOf() tells a moved rect from a resized one.
*/
class QCP_LIB_DECL QCPRatioTransform
{
public:
    QCPRatioTransform() = default;
    explicit QCPRatioTransform(const QCPAxisRect* axisRect);

    //! Maps a fraction of the axis rect to pixels.
    [[nodiscard]] QCP::Pixels map(QCP::AxisRatio ratio) const;
    //! Maps pixels back to a fraction of the axis rect (not clamped to 0..1).
    [[nodiscard]] QCP::AxisRatio inverse(QCP::Pixels pixels) const;
    //! True when the two transforms differ by a pixel offset only (the rect moved, same size).
    [[nodiscard]] bool isTranslationOf(const QCPRatioTransform& other,
                                       QPointF* delta = nullptr) const;

    //! False when the axis rect is gone or has no area.
    [[nodiscard]] bool valid() const { return mAxisRect && mWidth > 0.0 && mHeight > 0.0; }

private:
    QPointer<QCPAxisRect> mAxisRect;
    double mLeft = 0;
    double mTop = 0;
    double mWidth = 0;
    double mHeight = 0;
};

#endif // QCP_COORDS_H
