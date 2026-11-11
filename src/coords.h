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

#include "spatial-index.h"

#include <QPointF>
#include <QRectF>
#include <memory>
#include <span>
#include <variant>

class QCPAxis;
class QCPAxisRect;
class QCPItemAnchor;
class QCPPolarAxisAngular;
class QCPPolarAxisRadial;
struct QCPAxisLink;

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

// The custom mapping is written in the position types above: it comes after them, not before.
#include "coords-custom.h"

/*! \brief Maps plot coordinates to pixels and back: one frame type, three mappings.

  A transform copies what it needs when it is constructed, so replaying it never touches the axes
  again. The mapping is one of:

  - cartesian: the ranges, scale types, reversal and the axis rect of two axes, exactly
    QCPAxis::coordToPixel;
  - polar: the centre and radius of a polar area, the angular range, start angle and direction, and
    the radial range, scale and reversal, exactly QCPPolarAxisRadial::coordToPixel, with the radial
    fraction clamped to [0, 1.05]. A value a logarithmic radial scale cannot take (zero, or on the
    other side of zero) maps to NaN;
  - custom: a QCPCustomMapping, which maps a whole span at once.

  Every public call dispatches on the kind once and then runs a plain loop: batchMap() runs the same
  arithmetic without the per-point type dispatch, which is what makes it several times faster than
  calling coordToPixel twice per point.

  A transform with the wrong axes, no area or an unusable range maps to NaN and reports valid()
  false.

  The cartesian mapping is non-virtual and keeps the snapshot code it always had: the drawing paths
  depend on it.
*/
class QCP_LIB_DECL QCPTransform
{
public:
    //! Which mapping the transform holds; the variant's index, so kind() never walks it.
    enum class Kind
    {
        Cartesian,
        Polar,
        Custom
    };

    // Out of line: the variant's alternatives are complete only once this class is, and it is
    // constructed from one of them.
    QCPTransform();
    QCPTransform(const QCPAxis* keyAxis, const QCPAxis* valueAxis);

    //! A transform that fits \a box into \a rect: what a legend icon replays a recording through.
    [[nodiscard]] static QCPTransform fitted(const QCPDataBox& box, const QRectF& rect);

    //! A transform over \a angular's area: its centre, radius and angular range decide the frame.
    [[nodiscard]] static QCPTransform polar(const QCPPolarAxisAngular& angular,
                                            const QCPPolarAxisRadial& radial);

    //! A transform that maps through \a mapping, whose \a revision identifies its state.
    [[nodiscard]] static QCPTransform custom(std::shared_ptr<const QCPCustomMapping> mapping,
                                             quint64 revision);
    //! Maps one data point to pixels.
    [[nodiscard]] QCP::Pixels map(QCP::Data data) const;
    //! Maps keys and values in bulk into \a out; stops at the shortest of the three spans.
    void map(std::span<const double> keys, std::span<const double> values,
             std::span<QPointF> out) const;
    //! Maps interleaved data (x = key, y = value) into \a out; stops at the shortest span.
    void map(std::span<const QPointF> data, std::span<QPointF> out) const;
    //! Maps one pixel back to data.
    [[nodiscard]] QCP::Data inverse(QPointF pixel) const;
    //! The polar position of \a value along the radius, unclamped: the replay clamps it to [0, 1.05].
    //! NaN outside a polar frame, or for a value the radial scale cannot take.
    [[nodiscard]] double polarFraction(double value) const;
    //! The value at \a fraction of the radius in the frame's own scale. NaN outside a polar frame.
    [[nodiscard]] double polarValue(double fraction) const;
    //! True when the two transforms differ by a pixel offset only (a pan, not a zoom); \a delta
    //! gets this transform's pixel minus \a other's.
    [[nodiscard]] bool isTranslationOf(const QCPTransform& other, QPointF* delta = nullptr) const;

    //! True when the key axis is vertical: map() then returns the value pixel first.
    [[nodiscard]] bool keyIsVertical() const;
    //! False when an axis or area is unusable or the snapshot cannot map.
    [[nodiscard]] bool valid() const;

    //! Which mapping the transform holds.
    [[nodiscard]] Kind kind() const { return static_cast<Kind>(mFrame.index()); }

    //! Two transforms that map identically compare equal: the snapshots by value, a custom mapping
    //! by its pointer and revision.
    [[nodiscard]] bool operator==(const QCPTransform& other) const
    {
        return mFrame == other.mFrame;
    }

private:
    //! One axis on its own: the mapping QCPAxis would draw, copied so a replay never reads it.
    struct Snapshot
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

        bool operator==(const Snapshot&) const = default;

        void take(const QCPAxis* axis);
        void prepare();
        [[nodiscard]] double toPixel(double value) const;
        [[nodiscard]] double toValue(double pixel) const;
        //! Where \a pixel sits along the axis, as a fraction of its length from its origin.
        [[nodiscard]] double fractionOf(double pixel) const;
        //! The pixel at \a fraction of the axis length, from its origin.
        [[nodiscard]] double pixelAtFraction(double fraction) const;
    };

    //! An axis, mapped through its link when it has one. The link pointer is shared, not copied.
    struct Axis
    {
        Snapshot self;
        std::shared_ptr<const QCPAxisLink> link;
        Snapshot source; //!< the source's mapping, filled when the axis is linked

        bool operator==(const Axis&) const = default;

        void snapshot(const QCPAxis* axis);
        void prepare();
        [[nodiscard]] bool usable() const;
        [[nodiscard]] double toPixel(double value) const;
        [[nodiscard]] double toValue(double pixel) const;
    };

    //! The two-axis snapshot: today's mapping, unchanged.
    struct Cartesian
    {
        Axis key;
        Axis value;

        bool operator==(const Cartesian&) const = default;

        void prepare();

        [[nodiscard]] bool usable() const { return key.usable() && value.usable(); }
    };

    //! The polar frame: QCPPolarAxisRadial::coordToPixel with the radial fraction clamped.
    struct Polar
    {
        double centreX = 0;
        double centreY = 0;
        double radius = 0;
        double startAngle = 0; //!< radians, the angular axis' offset
        double angleLower = 0;
        double angleUpper = 0;
        double lower = 0; //!< the radial range
        double upper = 0;
        bool startReversed = false; //!< the angular axis' direction
        bool logarithmic = false;
        bool reversed = false;

        bool operator==(const Polar&) const = default;

        void take(const QCPPolarAxisAngular& angular, const QCPPolarAxisRadial& radial);
        [[nodiscard]] bool usable() const;
        //! The unclamped position of \a value along the radius; NaN when the scale cannot take it.
        [[nodiscard]] double rawFraction(double value) const;
        //! The clamped [0, 1.05] position of \a value along the radius, in the axis' own scale.
        [[nodiscard]] double fraction(double value) const;
        //! The value at \a fraction of the radius, in the axis' own scale.
        [[nodiscard]] double valueAt(double fraction) const;
        [[nodiscard]] QPointF toPixel(double angleCoord, double value) const;
        [[nodiscard]] QCP::Data toData(QPointF pixel) const;
    };

    //! A mapping that is not a two-axis frame, with the revision it was taken at.
    struct Custom
    {
        std::shared_ptr<const QCPCustomMapping> mapping;
        quint64 revision = 0;

        bool operator==(const Custom&) const = default;
    };

    [[nodiscard]] const Cartesian* cartesian() const { return std::get_if<Cartesian>(&mFrame); }

    template <bool KeyLog, bool ValueLog, typename KeyAt, typename ValueAt>
    void mapRange(const Cartesian& frame, std::size_t count, KeyAt keyAt, ValueAt valueAt,
                  std::span<QPointF> out) const;
    template <typename KeyAt, typename ValueAt>
    void mapAny(const Cartesian& frame, std::size_t count, KeyAt keyAt, ValueAt valueAt,
                std::span<QPointF> out) const;
    void mapPolar(const Polar& polar, std::size_t count, const double* keys, const double* values,
                  std::span<QPointF> out) const;
    void mapPoints(const Polar& polar, std::size_t count, const QPointF* data,
                   std::span<QPointF> out) const;

    std::variant<Cartesian, Polar, Custom> mFrame;
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
    //! A polar ratio: \a ratio.x is a fraction of \a angularSpan from \a startAngle, \a ratio.y a
    //! fraction of \a radius. \a reversed turns the angle the other way, as a reversed angular
    //! axis does.
    QCPRatioTransform(QPointF centre, double radius, double startAngle, double angularSpan,
                      bool reversed);

    //! A ratio transform over \a rect: 0,0 is its top left, 1,1 its bottom right (the icon case).
    [[nodiscard]] static QCPRatioTransform fitted(const QRectF& rect);

    //! Maps a fraction of the area to pixels: linear in a rect, on the ray of an angle in a disc.
    [[nodiscard]] QCP::Pixels map(QCP::AxisRatio ratio) const;

    //! The fractions of \a pixel: linear back from a rect, and the wrapped angle and radius back
    //! from a disc (the angle wraps, so a pixel on the -pi seam answers near 0 or near 1).
    [[nodiscard]] QCP::AxisRatio inverse(QPointF pixel) const;

    //! False when the rect is gone or has no area, or the disc has no radius or no span.
    [[nodiscard]] bool valid() const
    {
        if (mPolar)
            return mRadius > 0.0 && mSpan != 0.0;
        return mAxisRect && mWidth > 0.0 && mHeight > 0.0;
    }

    //! Whether the two ratio axes map independently. A rect does; a polar ratio (a fraction of the
    //! angle and of the radius) does not.
    [[nodiscard]] bool separable() const { return !mPolar; }

private:
    QPointer<QCPAxisRect> mAxisRect;
    double mLeft = 0;
    double mTop = 0;
    double mWidth = 0;
    double mHeight = 0;
    bool mPolar = false;
    QPointF mCentre;
    double mRadius = 0;
    double mStart = 0; //!< radians, where the zero fraction points
    double mSpan = 0; //!< radians, signed: a reversed angular axis spans the other way
};

#endif // QCP_COORDS_H
