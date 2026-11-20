#include "connector.h"

#include <algorithm>
#include <cmath>
#include <vector>

/*
  Each shape's formula is matplotlib 3.11's ConnectionStyle.connect(), ported as it is written:
  https://github.com/matplotlib/matplotlib/blob/v3.11.0/lib/matplotlib/patches.py
  (classes Arc3 = our arc, Angle3 = curve, Angle = elbow, Arc = arms, Bar = bracket). They are
  written in matplotlib's y-up frame, so path() negates the y of both points before the shape is
  built and of every point it produces after.
*/

namespace
{
struct Step
{
    enum class Kind
    {
        Move,
        Line,
        Quad
    };

    Kind kind = Kind::Line;
    QPointF control;
    QPointF point;
};

using Steps = std::vector<Step>;

Step moveTo(QPointF point)
{
    return Step { Step::Kind::Move, point, point };
}

Step lineTo(QPointF point)
{
    return Step { Step::Kind::Line, point, point };
}

Step quadTo(QPointF control, QPointF point)
{
    return Step { Step::Kind::Quad, control, point };
}

Steps straight(QPointF a, QPointF b)
{
    return { moveTo(a), lineTo(b) };
}

QPointF flipY(QPointF point)
{
    return { point.x(), -point.y() };
}

QPointF direction(double degrees)
{
    const double radians = qDegreesToRadians(degrees);
    return { std::cos(radians), std::sin(radians) };
}

QPointF pointTowards(QPointF from, QPointF to, double distance)
{
    const double span = std::hypot(to.x() - from.x(), to.y() - from.y());
    return span > 0 ? from + (to - from) * (distance / span) : from;
}

//! Where the line through \a a along \a alongA meets the line through \a b along \a alongB, or
//! nothing when they are parallel: matplotlib's test, on the cross product.
std::optional<QPointF> intersection(QPointF a, QPointF alongA, QPointF b, QPointF alongB)
{
    const double denominator = alongA.x() * alongB.y() - alongA.y() * alongB.x();
    // Opposite directions are parallel too: sin(180 degrees) is 1.2e-16, not 0.
    if (std::abs(denominator) < 1e-12)
        return { };
    const double along
        = ((b.x() - a.x()) * alongB.y() - (b.y() - a.y()) * alongB.x()) / denominator;
    return a + alongA * along;
}

//! matplotlib Arc3: the control point is \a bend times the start→end vector, turned a quarter.
Steps arcSteps(double bend, QPointF a, QPointF b)
{
    const QPointF delta = b - a;
    return { moveTo(a), quadTo((a + b) / 2 + QPointF { delta.y(), -delta.x() } * bend, b) };
}

//! matplotlib Angle3: the control point is where the two directions cross.
Steps curveSteps(double startAngle, double endAngle, QPointF a, QPointF b)
{
    const std::optional<QPointF> corner
        = intersection(a, direction(startAngle), b, direction(endAngle));
    return corner ? Steps { moveTo(a), quadTo(*corner, b) } : straight(a, b);
}

//! matplotlib Angle: straight to the corner, a quad rounding it, straight to the end.
Steps elbowSteps(double startAngle, double endAngle, double radius, QPointF a, QPointF b)
{
    const std::optional<QPointF> corner
        = intersection(a, direction(startAngle), b, direction(endAngle));
    if (!corner)
        return straight(a, b);
    if (radius == 0)
        return { moveTo(a), lineTo(*corner), lineTo(b) };
    return { moveTo(a), lineTo(pointTowards(*corner, a, radius)),
             quadTo(*corner, pointTowards(*corner, b, radius)), lineTo(b) };
}

struct Corner
{
    QPointF lineTo;
    QPointF control;
    QPointF end;
};

void append(Steps& steps, const Corner& corner)
{
    steps.push_back(lineTo(corner.lineTo));
    steps.push_back(quadTo(corner.control, corner.end));
}

//! matplotlib Arc: an arm at each end, joined through the corner their lines cross at, rounded.
Steps armsSteps(double startAngle, double startArm, double endAngle, double endArm, double radius,
                QPointF a, QPointF b)
{
    Steps steps { moveTo(a) };
    std::optional<Corner> corner;

    if (startArm != 0)
    {
        const QPointF along = direction(startAngle);
        const QPointF armEnd = a + along * startArm;
        corner = Corner { a + along * (startArm - radius), armEnd, armEnd };
    }

    if (endArm != 0)
    {
        const QPointF armEnd = b + direction(endAngle) * endArm;
        const QPointF from = corner ? corner->end : a;
        if (corner)
        {
            corner->end = pointTowards(from, armEnd, radius);
            append(steps, *corner);
        }
        const double span = std::hypot(armEnd.x() - from.x(), armEnd.y() - from.y());
        corner = Corner { pointTowards(from, armEnd, span - radius), armEnd, armEnd };
    }

    if (corner)
    {
        corner->end = pointTowards(corner->end, b, radius);
        append(steps, *corner);
    }

    steps.push_back(lineTo(b));
    return steps;
}

//! matplotlib Bar: the start→end run pushed \a fraction of its length to one side, both ends
//! joined.
Steps bracketSteps(double fraction, std::optional<double> angle, QPointF a, QPointF b)
{
    const QPointF delta = b - a;
    const double span = std::hypot(delta.x(), delta.y());

    QPointF stop = b;
    double minimumArm = 0;
    if (angle)
    {
        const double run = qDegreesToRadians(*angle);
        const double turn = std::atan2(delta.y(), delta.x()) - run;
        const double across = span * std::sin(turn);
        stop = a + QPointF { std::cos(run), std::sin(run) } * (span * std::cos(turn));
        minimumArm = -across;
    }

    const QPointF leg = stop - a;
    const double legSpan = std::hypot(leg.x(), leg.y());
    // simplify: matplotlib divides by this length, so an angle that lands the projected run on the
    // start point is a NaN there; here it draws a straight line.
    if (legSpan == 0)
        return straight(a, b);
    const QPointF perpendicular { leg.y(), -leg.x() };
    const QPointF offset
        = perpendicular * ((fraction * span + std::max(0.0, minimumArm)) / legSpan);
    return { moveTo(a), lineTo(a + offset), lineTo(stop + offset), lineTo(b) };
}

bool isFinite(QPointF point)
{
    return std::isfinite(point.x()) && std::isfinite(point.y());
}

//! The steps as a path, or nothing when one of them holds a coordinate that is not finite (a NaN
//! bend, angle, arm, radius or fraction, say): a path of NaN would poison the painter.
QPainterPath buildPath(const Steps& steps)
{
    const bool finite = std::ranges::all_of(
        steps, [](const Step& step) { return isFinite(step.control) && isFinite(step.point); });
    if (!finite)
        return { };

    QPainterPath path;
    for (const Step& step : steps)
    {
        switch (step.kind)
        {
            case Step::Kind::Move:
                path.moveTo(flipY(step.point));
                break;
            case Step::Kind::Line:
                path.lineTo(flipY(step.point));
                break;
            case Step::Kind::Quad:
                path.quadTo(flipY(step.control), flipY(step.point));
                break;
        }
    }
    return path;
}

template <typename... Functions>
struct Overloaded : Functions...
{
    using Functions::operator()...;
};

template <typename... Functions>
Overloaded(Functions...) -> Overloaded<Functions...>;
} // namespace

QCPConnector QCPConnector::arc(double bend)
{
    QCPConnector connector;
    connector.mShape = Arc { bend };
    return connector;
}

QCPConnector QCPConnector::curve(double startAngle, double endAngle)
{
    QCPConnector connector;
    connector.mShape = Curve { startAngle, endAngle };
    return connector;
}

QCPConnector QCPConnector::elbow(double startAngle, double endAngle, QCP::PixelLength radius)
{
    QCPConnector connector;
    connector.mShape = Elbow { startAngle, endAngle, radius.px };
    return connector;
}

QCPConnector QCPConnector::arms(double startAngle, QCP::PixelLength startArm, double endAngle,
                                QCP::PixelLength endArm, QCP::PixelLength radius)
{
    QCPConnector connector;
    connector.mShape = Arms { startAngle, startArm.px, endAngle, endArm.px, radius.px };
    return connector;
}

QCPConnector QCPConnector::bracket(double fraction, std::optional<double> angle)
{
    QCPConnector connector;
    connector.mShape = Bracket { fraction, angle };
    return connector;
}

QPainterPath QCPConnector::path(QPointF start, QPointF end) const
{
    if (start == end)
        return { };

    const QPointF a = flipY(start);
    const QPointF b = flipY(end);
    const Steps steps = std::visit(
        Overloaded {
            [&](const Arc& shape) { return arcSteps(shape.bend, a, b); },
            [&](const Curve& shape) { return curveSteps(shape.startAngle, shape.endAngle, a, b); },
            [&](const Elbow& shape)
            { return elbowSteps(shape.startAngle, shape.endAngle, shape.radius, a, b); },
            [&](const Arms& shape)
            {
                return armsSteps(shape.startAngle, shape.startArm, shape.endAngle, shape.endArm,
                                 shape.radius, a, b);
            },
            [&](const Bracket& shape) { return bracketSteps(shape.fraction, shape.angle, a, b); },
        },
        mShape);
    return buildPath(steps);
}

namespace qcp
{
namespace
{
double distance(QPointF a, QPointF b)
{
    return std::hypot(a.x() - b.x(), a.y() - b.y());
}

//! How many steps to walk or sample a path of \a span pixels in: one per pixel, and never more than
//! the cap, so a path of any length costs a bounded amount of work. Clamped in double before the
//! conversion: a span past a few billion pixels is outside what an int holds, and that conversion
//! would be undefined.
//! simplify: a path longer than the cap is walked and sampled coarser than a pixel; upgrade = walk
//! the path's own elements, which are a handful however long the path.
int stepsFor(double span)
{
    constexpr double cap = 4096;
    if (!std::isfinite(span))
        return int(cap);
    return int(std::clamp(span, 2.0, cap));
}

//! The first place at or after \a from where \a beyond holds, nothing when it never does. The path
//! is walked a pixel at a time to find where the answer turns, then that last step is halved: a path
//! that leaves the box and comes back (an elbow whose corner is past the text) crosses more than
//! once, and the connector leaves at the first crossing, as matplotlib's split_path_inout does.
template <typename Beyond>
std::optional<double> firstWhere(const QPainterPath& path, double from, Beyond beyond)
{
    if (beyond(path.pointAtPercent(from)))
        return from;
    const int steps = stepsFor(std::ceil(path.length()));
    double previous = from;
    for (int i = 1; i <= steps; ++i)
    {
        const double t = from + (1 - from) * i / steps;
        if (!beyond(path.pointAtPercent(t)))
        {
            previous = t;
            continue;
        }
        double inside = previous;
        double outside = t;
        for (int j = 0; j < 32; ++j)
        {
            const double middle = (inside + outside) / 2;
            (beyond(path.pointAtPercent(middle)) ? outside : inside) = middle;
        }
        return outside;
    }
    return std::nullopt;
}

//! The last place at or before the path's end where \a since holds, nothing when it never does:
//! the same walk as firstWhere, from the end backwards. The target's cut is measured this way
//! because matplotlib measures it from the end: a path that passes within the cut of its own end
//! on the way and then leaves again is cut at its last approach, not at its first.
template <typename Since>
std::optional<double> lastWhere(const QPainterPath& path, double from, Since since)
{
    if (since(path.pointAtPercent(1)))
        return 1.0; // the end itself is past the cut: nothing to take off
    const int steps = stepsFor(std::ceil(path.length()));
    double failed = 1.0;
    for (int i = 1; i <= steps; ++i)
    {
        const double t = 1 - (1 - from) * i / steps;
        if (!since(path.pointAtPercent(t)))
        {
            failed = t;
            continue;
        }
        double held = t;
        for (int j = 0; j < 32; ++j)
        {
            const double middle = (held + failed) / 2;
            (since(path.pointAtPercent(middle)) ? held : failed) = middle;
        }
        return held;
    }
    return std::nullopt;
}

//! The path between \a from and \a to, as a polyline of points at most one pixel apart.
QPolygonF sampled(const QPainterPath& path, double from, double to)
{
    const double span = (to - from) * path.length();
    const int steps = stepsFor(std::ceil(span) + 1);
    QPolygonF line;
    line.reserve(steps);
    for (int i = 0; i < steps; ++i)
        line << path.pointAtPercent(from + (to - from) * i / (steps - 1));
    return line;
}
} // namespace

TrimmedConnector trimConnector(const QPainterPath& path, const QRectF& startBox, double shrink)
{
    TrimmedConnector trimmed;
    // A path of one point has no direction and no length: nothing to sample or to turn a head along.
    if (path.isEmpty() || !(path.length() > 0))
        return trimmed;
    const double cut = std::max(0.0, shrink);

    double from = 0;
    if (!startBox.isNull())
    {
        const std::optional<double> exit = firstWhere(
            path, 0, [&startBox](QPointF point) { return !startBox.contains(point); });
        if (!exit)
            return trimmed; // the whole path stays in the box: the target is inside the text
        from = *exit;
    }
    if (cut > 0)
    {
        const QPointF start = path.pointAtPercent(from);
        const std::optional<double> away = firstWhere(path, from, [&](QPointF point)
                                                      { return distance(point, start) >= cut; });
        if (!away)
            return trimmed; // shorter than the start's cut
        from = *away;
    }

    double to = 1;
    if (cut > 0)
    {
        const QPointF end = path.pointAtPercent(1);
        const std::optional<double> away = lastWhere(
            path, from, [&](QPointF point) { return distance(point, end) >= cut; });
        if (!away)
            return trimmed; // shorter than the target's cut
        to = *away;
    }
    if (!(to > from))
        return trimmed;

    trimmed.line = sampled(path, from, to);
    trimmed.endAngleDegrees = path.angleAtPercent(to);
    return trimmed;
}
} // namespace qcp
