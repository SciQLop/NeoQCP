#ifndef QCP_ITEM_CONNECTOR_H
#define QCP_ITEM_CONNECTOR_H

#include "coords.h"

#include <QPainterPath>
#include <QPointF>

#include <optional>
#include <variant>

/*!
  \brief The shape of the line an annotation draws from its text to its target.

  A connector is that line's shape, not its ends: \ref path turns a start and an end in pixels into
  the curve matplotlib's `annotate` draws between them. The five shapes are matplotlib's
  `ConnectionStyle` classes, each named as the vocabulary names it:

  - \ref arc, a quadratic curve whose control point sits \a bend times the start→end distance off
    the middle, to the left of the direction for a positive bend (matplotlib `arc3`);
  - \ref curve, a quadratic curve through the corner where the two lines leave the ends at the
    given angles (matplotlib `angle3`);
  - \ref elbow, two straight lines through that corner, rounded with a radius (matplotlib `angle`);
  - \ref arms, one arm of a given length from each end at the given angles, joined through that
    corner (matplotlib `arc`);
  - \ref bracket, a straight run offset to one side of the start→end line, joined to both ends
    (matplotlib `bar`).

  Angles are degrees counter-clockwise from +x, in the y-up frame the formulas are written in, so
  `curve(90, 0)` leaves the start upwards whatever the widget's y axis does. A connector with no
  answer is a straight line: parallel directions for \ref curve and \ref elbow, a start equal to
  the end, a position that is not finite.
*/
class QCP_LIB_DECL QCPConnector
{
public:
    /*! The quadratic curve of matplotlib's `arc3`: \a bend is how far the control point is off the
      middle of the start→end line, in units of its length, positive bowing to the left. */
    static QCPConnector arc(double bend = 0);

    /*! The quadratic curve of matplotlib's `angle3`: it leaves the start along \a startAngle, meets
      the end along `-endAngle`, and the corner where those lines cross is its control point. */
    static QCPConnector curve(double startAngle, double endAngle);

    /*! The two straight lines of matplotlib's `angle`: one leaves the start along \a startAngle,
      the other reaches the end along `-endAngle`, and they meet in a corner rounded with \a radius.
    */
    static QCPConnector elbow(double startAngle, double endAngle, QCP::PixelLength radius = { });

    /*! matplotlib's `arc`: a \a startArm long arm leaves the start along \a startAngle, a \a endArm
      long one leaves the end along `-endAngle`, and the two are joined through the corner where
      their lines cross, the corners rounded with \a radius. A zero-length arm is no arm. */
    static QCPConnector arms(double startAngle, QCP::PixelLength startArm, double endAngle,
                             QCP::PixelLength endArm, QCP::PixelLength radius = { });

    /*! matplotlib's `bar`: the run between the two points is pushed \a fraction of their distance
      to one side, and both ends are joined to it. \a angle turns that run away from the
      start→end direction: it is the angle of the line the two arms reach, in degrees. */
    static QCPConnector bracket(double fraction, std::optional<double> angle = { });

    //! A straight line: \ref arc with a bend of zero.
    QCPConnector() = default;

    //! The connector from \a start (the text's end) to \a end (the target's), in pixels, y down.
    [[nodiscard]] QPainterPath path(QPointF start, QPointF end) const;

    bool operator==(const QCPConnector&) const = default;

private:
    /// \internal matplotlib's `arc3`: one control point off the middle.
    struct Arc
    {
        double bend = 0;
        bool operator==(const Arc&) const = default;
    };

    /// \internal matplotlib's `angle3`: the control point is the two directions' intersection.
    struct Curve
    {
        double startAngle = 0;
        double endAngle = 0;
        bool operator==(const Curve&) const = default;
    };

    /// \internal matplotlib's `angle`: straight to the corner, rounded there, straight to the end.
    struct Elbow
    {
        double startAngle = 0;
        double endAngle = 0;
        double radius = 0;
        bool operator==(const Elbow&) const = default;
    };

    /// \internal matplotlib's `arc`: an arm at each end, the corners rounded.
    struct Arms
    {
        double startAngle = 0;
        double startArm = 0;
        double endAngle = 0;
        double endArm = 0;
        double radius = 0;
        bool operator==(const Arms&) const = default;
    };

    /// \internal matplotlib's `bar`: the run pushed to one side.
    struct Bracket
    {
        double fraction = 0;
        std::optional<double> angle;
        bool operator==(const Bracket&) const = default;
    };

    // The default is an arc with no bend, a straight line. Spelled on the member because
    // std::variant's default constructor cannot name a private alternative.
    std::variant<Arc, Curve, Elbow, Arms, Bracket> mShape = Arc { };
};

#endif
