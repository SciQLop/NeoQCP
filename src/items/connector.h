#ifndef QCP_ITEM_CONNECTOR_H
#define QCP_ITEM_CONNECTOR_H

#include "coords.h"

#include <QPainterPath>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>

#include <optional>
#include <variant>

/*!
  \brief The shape of the line an annotation draws from its text to its target.

  A connector is that line's shape, not its ends: \ref path turns a start and an end in pixels into
  the curve matplotlib's `annotate` draws between them. The five shapes are matplotlib's
  `ConnectionStyle` classes, each named as the vocabulary names it:

  - \ref arc, a quadratic curve whose control point sits \a bend times the start→end distance off
    the middle, to the right of the start→end direction for a positive bend (matplotlib `arc3`);
  - \ref curve, a quadratic curve through the corner where the two lines leave the ends at the
    given angles (matplotlib `angle3`);
  - \ref elbow, two straight lines through that corner, rounded with a radius (matplotlib `angle`);
  - \ref arms, one arm of a given length from each end at the given angles, the two arm ends joined
    by one straight segment (matplotlib `arc`);
  - \ref bracket, a straight run offset to one side of the start→end line, joined to both ends
    (matplotlib `bar`).

  Angles are degrees counter-clockwise from +x, in the y-up frame the formulas are written in, so
  `curve(90, 0)` leaves the start upwards whatever the widget's y axis does. A connector with no
  answer is a straight line: parallel directions for \ref curve and \ref elbow (angles 180 degrees
  apart included), a start equal to the end. A connector that would hold a coordinate that is not
  finite draws nothing.
*/
class QCP_LIB_DECL QCPConnector
{
public:
    /*! The quadratic curve of matplotlib's `arc3`: \a bend is how far the control point is off the
      middle of the start→end line, in units of its length, positive bowing to the right. */
    static QCPConnector arc(double bend = 0);

    /*! The quadratic curve of matplotlib's `angle3`: a line of \a startAngle through the start and a
      line of \a endAngle through the end. The corner where those two lines cross is its control
      point, so the curve runs from the one to the other and may well leave the start the other way
      round: the angles name lines, not directions. */
    static QCPConnector curve(double startAngle, double endAngle);

    /*! The two straight lines of matplotlib's `angle`: one runs along a line of \a startAngle through
      the start, the other along a line of \a endAngle through the end, and they meet in a corner
      rounded with \a radius. */
    static QCPConnector elbow(double startAngle, double endAngle, QCP::PixelLength radius = { });

    /*! matplotlib's `arc`: a \a startArm long arm leaves the start along \a startAngle, a \a endArm
      long one leaves the end along \a endAngle, and one straight segment joins their two ends, the
      corners at those ends rounded with \a radius. It does not pass through the corner where the
      two lines cross: that is \ref elbow. A zero-length arm is no arm. */
    static QCPConnector arms(double startAngle, QCP::PixelLength startArm, double endAngle,
                             QCP::PixelLength endArm, QCP::PixelLength radius = { });

    /*! matplotlib's `bar`: a straight bar runs beside the start→end line, pushed sideways from it
      by \a fraction of the start→end distance, negative pushing the other way, and both ends are
      joined to it. \a angle, when given, is the bar's own direction in degrees, absolute rather
      than relative to the start→end line: `bracket(-0.2, 180)` draws a horizontal bar. */
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

namespace qcp
{
/*!
  \brief What is left of a connector after the text's box and the two shrink lengths have cut it.

  \a line is the kept part, in pixels: the path's own flattened vertices between the two cuts, one
  point wherever a cut falls. An empty \a line is nothing to draw, which is what a target inside the
  text box and a path shorter than its two cuts give. \a endAngleDegrees is the direction of the
  last kept segment, in QPainterPath's degrees, so the head at the target can be turned along it.
*/
struct QCP_LIB_DECL TrimmedConnector
{
    QPolygonF line;
    double endAngleDegrees = 0;
};

/*!
  Cuts \a path where it first leaves \a startBox, then \a shrink pixels along from each of its two
  ends, and keeps what is between the cuts. A null \a startBox cuts nothing at the start: a text
  with no box has no edge to leave. A path that never leaves the box leaves nothing, and so does one
  shorter than its two cuts.

  The path is flattened once, by Qt, and the three cuts walk that polyline: a comparison per vertex,
  and a halving of the one segment each cut falls in, the way matplotlib's split_path_inout does.
  simplify: the kept part is the flattened polyline, not the path's own Bézier pieces; upgrade =
  split the quads with de Casteljau if a thick pen on a very curved connector ever shows the facets.
*/
[[nodiscard]] QCP_LIB_DECL TrimmedConnector trimConnector(const QPainterPath& path,
                                                          const QRectF& startBox, double shrink);
} // namespace qcp

#endif
