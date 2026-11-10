/***************************************************************************
**                                                                        **
**  QCustomPlot, an easy to use, modern plotting widget for Qt            **
**  Copyright (C) 2011-2022 Emanuel Eichhammer                            **
**                                                                        **
**  This program is free software: you can redistribute it and/or modify  **
**  it under the terms of the GNU General Public License as published by  **
**  the Free Software Foundation, either version 3 of the License, or     **
**  (at your option) any later version.                                   **
**                                                                        **
**  This program is distributed in the hope that it will be useful,       **
**  but WITHOUT ANY WARRANTY; without even the implied warranty of        **
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         **
**  GNU General Public License for more details.                          **
**                                                                        **
**  You should have received a copy of the GNU General Public License     **
**  along with this program.  If not, see http://www.gnu.org/licenses/.   **
**                                                                        **
****************************************************************************
**           Author: Emanuel Eichhammer                                   **
**  Website/Contact: https://www.qcustomplot.com/                         **
**             Date: 06.11.22                                             **
**          Version: 2.1.1                                                **
****************************************************************************/

#include "polargraph.h"

#include "../coordinate-system-polar.h"
#include "../coordinate-system.h"
#include "../coords.h"
#include "../painting/draw-commands.h"
#include "../painting/draw-context.h"
#include "polar-area.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <typeinfo>
#include <vector>

namespace
{
// The radial fraction the frame clamps a value beyond the range to (see QCPTransform::Polar).
//! A marker style that carries no pen of its own takes the graph's, as 2.x drew it.
QCPScatterStyle withPen(QCPScatterStyle style, const QPen& pen)
{
    if (!style.isPenDefined())
        style.setPen(pen);
    return style;
}

constexpr double kClampFraction = 1.05;

// The angular step of the points inserted beyond the rim, in degrees of arc: the chord between two
// consecutive ones stays outside the clamp circle, whose half angle is acos(1/1.05), about 17.75
// degrees.
constexpr double kBeyondStep = 15.0;

QCPCoordinateSystem* systemOfAxes(QCPPolarAxisAngular* angular, QCPPolarAxisRadial* radial)
{
    if (!angular || !radial || !angular->area())
        return nullptr;
    return angular->area()->system(angular, radial);
}
} // namespace

QCPPolarGraph::QCPPolarGraph(QCPPolarAxisAngular* keyAxis, QCPPolarAxisRadial* valueAxis)
        : QCPPolarGraph(systemOfAxes(keyAxis, valueAxis))
{
}

QCPPolarGraph::QCPPolarGraph(QCPCoordinateSystem* system)
        : QCPAbstractPlottable1D<QCPGraphData>(system)
        , mLineStyle(lsLine)
        , mScatterStyle()
        , mPeriodic(true)
{
    setPen(QPen(Qt::blue, 0));
    setBrush(Qt::NoBrush);
}

QCPPolarAxisAngular* QCPPolarGraph::angularAxis() const
{
    return qobject_cast<QCPPolarAxisAngular*>(system() ? system()->keyAxis() : nullptr);
}

QCPPolarAxisRadial* QCPPolarGraph::radialAxis() const
{
    return qobject_cast<QCPPolarAxisRadial*>(system() ? system()->valueAxis() : nullptr);
}

void QCPPolarGraph::setPeriodic(bool enabled)
{
    if (mPeriodic == enabled)
        return;
    mPeriodic = enabled;
    markDirty();
}

void QCPPolarGraph::setLineStyle(LineStyle ls)
{
    if (mLineStyle == ls)
        return;
    mLineStyle = ls;
    markDirty();
}

void QCPPolarGraph::setScatterStyle(const QCPScatterStyle& style)
{
    mScatterStyle = style;
    markDirty();
}

void QCPPolarGraph::setData(QSharedPointer<QCPGraphDataContainer> data)
{
    mDataContainer = data;
    markDirty();
}

void QCPPolarGraph::setData(const QVector<double>& keys, const QVector<double>& values,
                            bool alreadySorted)
{
    mDataContainer->clear();
    addData(keys, values, alreadySorted);
}

void QCPPolarGraph::addData(const QVector<double>& keys, const QVector<double>& values,
                            bool alreadySorted)
{
    if (keys.size() != values.size())
        qDebug() << Q_FUNC_INFO << "keys and values have different sizes:" << keys.size()
                 << values.size();
    const int n = qMin(keys.size(), values.size());
    QVector<QCPGraphData> points(n);
    for (int i = 0; i < n; ++i)
        points[i] = QCPGraphData(keys[i], values[i]);
    mDataContainer->add(points, alreadySorted);
    markDirty();
}

void QCPPolarGraph::addData(double key, double value)
{
    mDataContainer->add(QCPGraphData(key, value));
    markDirty();
}

void QCPPolarGraph::coordsToPixels(double key, double value, double& x, double& y) const
{
    const QPointF point = coordsToPixels(key, value);
    x = point.x();
    y = point.y();
}

const QPointF QCPPolarGraph::coordsToPixels(double key, double value) const
{
    QCPPolarAxisRadial* radial = radialAxis();
    if (!radial)
    {
        qDebug() << Q_FUNC_INFO << "invalid radial axis";
        return { };
    }
    return radial->coordToPixel(key, value);
}

void QCPPolarGraph::pixelsToCoords(double x, double y, double& key, double& value) const
{
    pixelsToCoords(QPointF(x, y), key, value);
}

void QCPPolarGraph::pixelsToCoords(const QPointF& pixelPos, double& key, double& value) const
{
    QCPPolarAxisRadial* radial = radialAxis();
    if (!radial)
    {
        qDebug() << Q_FUNC_INFO << "invalid radial axis";
        return;
    }
    radial->pixelToCoord(pixelPos, key, value);
}

void QCPPolarGraph::rescaleAxes(bool onlyEnlarge) const
{
    rescaleKeyAxis(onlyEnlarge);
    rescaleValueAxis(onlyEnlarge);
}

void QCPPolarGraph::rescaleKeyAxis(bool onlyEnlarge) const
{
    QCPPolarAxisAngular* keyAxis = angularAxis();
    if (!keyAxis)
    {
        qDebug() << Q_FUNC_INFO << "invalid angular axis";
        return;
    }

    bool foundRange;
    QCPRange newRange = getKeyRange(foundRange, QCP::sdBoth);
    if (foundRange)
    {
        if (onlyEnlarge)
            newRange.expand(keyAxis->range());
        if (!QCPRange::validRange(newRange))
        {
            const double center = (newRange.lower + newRange.upper) * 0.5;
            newRange.lower = center - keyAxis->range().size() / 2.0;
            newRange.upper = center + keyAxis->range().size() / 2.0;
        }
        keyAxis->setRange(newRange);
    }
}

void QCPPolarGraph::rescaleValueAxis(bool onlyEnlarge, bool inKeyRange) const
{
    QCPPolarAxisAngular* keyAxis = angularAxis();
    QCPPolarAxisRadial* valueAxis = radialAxis();
    if (!keyAxis || !valueAxis)
    {
        qDebug() << Q_FUNC_INFO << "invalid angular or radial axis";
        return;
    }

    QCP::SignDomain signDomain = QCP::sdBoth;
    if (valueAxis->scaleType() == QCPPolarAxisRadial::stLogarithmic)
        signDomain = (valueAxis->range().upper < 0 ? QCP::sdNegative : QCP::sdPositive);

    bool foundRange;
    QCPRange newRange
        = getValueRange(foundRange, signDomain, inKeyRange ? keyAxis->range() : QCPRange());
    if (foundRange)
    {
        if (onlyEnlarge)
            newRange.expand(valueAxis->range());
        if (!QCPRange::validRange(newRange))
        {
            const double center = (newRange.lower + newRange.upper) * 0.5;
            if (valueAxis->scaleType() == QCPPolarAxisRadial::stLinear)
            {
                newRange.lower = center - valueAxis->range().size() / 2.0;
                newRange.upper = center + valueAxis->range().size() / 2.0;
            }
            else
            {
                const double factor = qSqrt(valueAxis->range().upper / valueAxis->range().lower);
                newRange.lower = center / factor;
                newRange.upper = center * factor;
            }
        }
        valueAxis->setRange(newRange);
    }
}

QCP::Capabilities QCPPolarGraph::requiredCapabilities() const
{
    return { };
}

quint64 QCPPolarGraph::dependencyRevision() const
{
    const QCPPolarAxisAngular* angular = angularAxis();
    const QCPPolarAxisRadial* radial = radialAxis();
    if (!angular || !radial)
        return 0;

    // The recording is clamped against the radial frame, and its window is the angular range of a
    // non-periodic graph: everything else it reads, through the frame, is the frame's business.
    quint64 revision = qHashMulti(mDataContainer ? mDataContainer->revision() : quint64(0),
                                  radial->range().lower, radial->range().upper,
                                  static_cast<int>(radial->scaleType()), radial->rangeReversed(),
                                  angular->range().size());
    if (!mPeriodic)
        revision = qHashMulti(revision, angular->range().lower, angular->range().upper);
    return revision;
}

std::pair<QCPGraphDataContainer::const_iterator, QCPGraphDataContainer::const_iterator>
QCPPolarGraph::window() const
{
    if (mPeriodic)
        return { mDataContainer->constBegin(), mDataContainer->constEnd() };
    const QCPPolarAxisAngular* angular = angularAxis();
    if (!angular)
        return { mDataContainer->constEnd(), mDataContainer->constEnd() };
    return { mDataContainer->findBegin(angular->range().lower),
             mDataContainer->findEnd(angular->range().upper) };
}

/*! \internal

  The recording answers the geometry: its replay is the polyline of the frame, inside the disc, and
  the core measures the distance to it. Only the details must say which point was hit, as 2.1 did:
  the nearest one of the window.
*/
double QCPPolarGraph::selectTest(const QPointF& pos, bool onlySelectable, QVariant* details) const
{
    const double recorded = QCPAbstractPlottable::selectTest(pos, onlySelectable, details);
    if (recorded < 0 || !system() || !parentPlot())
        return recorded;

    // The recording holds the run beyond the rim too, and the replay cuts it: only the geometry it
    // shows is there to click. A click inside a visible fill keeps the base's distance zero.
    const QCPTransform frame = system()->transform();
    const double visible = visibleDistance(pos, frame);
    if (recorded > 0.0 && visible > parentPlot()->selectionTolerance())
        return -1;

    if (details)
    {
        int nearest = -1;
        double nearestDistance = 0;
        const auto [begin, end] = window();
        for (auto it = begin; it != end; ++it)
        {
            const double fraction = frame.polarFraction(it->value);
            if (!std::isfinite(fraction) || fraction < 0.0 || fraction > 1.0)
                continue;
            const QCP::Pixels pixel = frame.map(QCP::Data { it->key, it->value });
            if (!std::isfinite(pixel.x) || !std::isfinite(pixel.y))
                continue;
            const double pointDistance
                = std::hypot(pixel.x - pos.x(), pixel.y - pos.y());
            if (nearest < 0 || pointDistance < nearestDistance)
            {
                nearest = static_cast<int>(it - mDataContainer->constBegin());
                nearestDistance = pointDistance;
            }
        }
        if (nearest >= 0)
            *details = QVariant::fromValue(QCPDataSelection(QCPDataRange(nearest, nearest + 1)));
    }
    return recorded > 0.0 ? visible : recorded;
}

/*! \internal

  The distance from pos to what the replay shows: the recorded segments whose two ends are in the
  disc (H-2 records a crossing at fraction exactly 1, so a segment is either wholly visible or wholly
  hidden), and the markers, which only in-range points get.
*/
double QCPPolarGraph::visibleDistance(const QPointF& pos, const QCPTransform& frame) const
{
    const auto isVisible = [&frame](const QPointF& point)
    {
        const double fraction = frame.polarFraction(point.y());
        return std::isfinite(fraction) && fraction >= 0.0 && fraction <= 1.0;
    };
    const auto segmentDistance = [&pos](const QPointF& from, const QPointF& to)
    {
        const double dx = to.x() - from.x();
        const double dy = to.y() - from.y();
        const double lengthSquared = dx * dx + dy * dy;
        double t = 0.0;
        if (lengthSquared > 0)
            t = qBound(0.0,
                       ((pos.x() - from.x()) * dx + (pos.y() - from.y()) * dy) / lengthSquared,
                       1.0);
        return std::hypot(from.x() + dx * t - pos.x(), from.y() + dy * t - pos.y());
    };

    double nearest = std::numeric_limits<double>::max();
    for (const auto& command : mCore.recordedList().commands)
    {
        if (const auto* line = std::get_if<qcp::PolylineDataCmd>(&command))
        {
            const std::vector<QPointF>& points = *line->points;
            std::vector<QPointF> pixels(points.size());
            for (std::size_t i = 0; i < points.size(); ++i)
            {
                const QCP::Pixels pixel = frame.map(QCP::Data { points[i].x(), points[i].y() });
                pixels[i] = QPointF(pixel.x, pixel.y);
            }
            for (std::size_t i = 0; i + 1 < points.size(); ++i)
                if (isVisible(points[i]) && isVisible(points[i + 1]))
                    nearest = qMin(nearest, segmentDistance(pixels[i], pixels[i + 1]));
        }
        else if (const auto* markers = std::get_if<qcp::MarkersCmd>(&command))
        {
            for (const QPointF& point : *markers->points)
            {
                if (!isVisible(point))
                    continue;
                const QCP::Pixels pixel = frame.map(QCP::Data { point.x(), point.y() });
                nearest = qMin(nearest, std::hypot(pixel.x - pos.x(), pixel.y - pos.y()));
            }
        }
    }
    return nearest;
}

/*! \internal

  The rubber band, in pixels of the frame: the base class walks coordinates on a key and a value
  axis, and a polar graph has neither.
*/
QCPDataSelection QCPPolarGraph::selectTestRect(const QRectF& rect, bool onlySelectable) const
{
    QCPDataSelection result;
    if ((onlySelectable && mSelectable == QCP::stNone) || mDataContainer->isEmpty() || !system())
        return result;

    const QCPTransform frame = system()->transform();
    const auto [begin, end] = window();
    const auto indexOf = [this](QCPGraphDataContainer::const_iterator it)
    { return static_cast<int>(it - mDataContainer->constBegin()); };
    int segmentBegin = -1;
    for (auto it = begin; it != end; ++it)
    {
        const QCP::Pixels pixel = frame.map(QCP::Data { it->key, it->value });
        const double fraction = frame.polarFraction(it->value);
        const bool inside = std::isfinite(pixel.x) && std::isfinite(pixel.y) && fraction >= 0.0
            && fraction <= 1.0 && rect.contains(QPointF(pixel.x, pixel.y));
        if (inside && segmentBegin < 0)
            segmentBegin = indexOf(it);
        else if (!inside && segmentBegin >= 0)
        {
            result.addDataRange(QCPDataRange(segmentBegin, indexOf(it)), false);
            segmentBegin = -1;
        }
    }
    if (segmentBegin >= 0)
        result.addDataRange(QCPDataRange(segmentBegin, indexOf(end)), false);
    return result;
}

/*! \internal

  A polar graph draws in a polar system: another one would be a frame it cannot use, so it keeps the
  system it has and says so.
*/
void QCPPolarGraph::setSystem(QCPCoordinateSystem* system)
{
    QCP_CHECK_OR_RETURN(!system || qobject_cast<QCPPolarSystem*>(system),
                        "a polar graph draws in a polar system, not in this one", );
    QCPAbstractPlottable::setSystem(system);
}

/*! \internal

  The data ranges, not the recording's box: a rescale asks what the graph holds, and the box of a
  clamped recording would answer with the rim instead.
*/
QCPRange QCPPolarGraph::getKeyRange(bool& foundRange, QCP::SignDomain inSignDomain) const
{
    return mDataContainer->keyRange(foundRange, inSignDomain);
}

QCPRange QCPPolarGraph::getValueRange(bool& foundRange, QCP::SignDomain inSignDomain,
                                      const QCPRange& inKeyRange) const
{
    return mDataContainer->valueRange(foundRange, inSignDomain, inKeyRange);
}

/*! \internal

  The legend icon: a band, a line and one marker, in the graph's own styles. It has no data of its
  own, so an invisible box gives the replay the extent to fit the drawing to.
*/
void QCPPolarGraph::drawLegendIcon(QCPDrawContext& icon) const
{
    icon.rect(QCP::Data { 0.0, -0.5 }, QCP::Data { 1.0, 0.5 }, QPen(Qt::NoPen),
              QBrush(Qt::NoBrush));
    if (mBrush.style() != Qt::NoBrush && mBrush.color().alpha() != 0)
        icon.rect(QCP::Data { 0.0, -0.15 }, QCP::Data { 1.0, 0.15 }, QPen(Qt::NoPen), mBrush);
    if (mLineStyle != lsNone)
        icon.polyline(std::make_shared<const std::vector<QPointF>>(
                          std::vector<QPointF> { { 0.0, 0.0 }, { 1.0, 0.0 } }),
                      mPen);
    if (!mScatterStyle.isNone())
        icon.markers(std::make_shared<const std::vector<QPointF>>(
                         std::vector<QPointF> { { 0.5, 0.0 } }),
                     withPen(mScatterStyle, mPen));
}

/*! \internal

  Records the graph in data space: the angular coordinate and the radial value of every point of the
  window, split into runs of finite values. Each run gets a polyline, and a fill polygon when it has
  a brush. Beyond the rim the run is extended along the rim, so that the clip of the replay hides it
  there instead of drawing a chord through the disc.
*/
void QCPPolarGraph::draw(QCPDrawContext& context) const
{
    QCPPolarArea* area = qobject_cast<QCPPolarArea*>(this->area());
    QCPPolarAxisAngular* angular = angularAxis();
    QCPPolarAxisRadial* radial = radialAxis();
    if (!area || !angular || !radial || mDataContainer->isEmpty())
        return;

    const double radius = area->radius();
    const double span = angular->range().size();
    if (!(radius > 0) || !(span > 0) || !(radial->range().size() > 0))
        return;

    const bool drawsLine = mLineStyle == lsLine;
    const bool drawsFill = drawsLine && mBrush.style() != Qt::NoBrush;
    const bool drawsMarkers = !mScatterStyle.isNone();
    if (!drawsLine && !drawsMarkers)
        return;

    // The frame of the system the graph draws in: the recording is clamped, and the rim crossings
    // are computed, in that one frame.
    const QCPTransform frame = system()->transform();

    // Where a data point sits in the unit disc, as the replay draws it: the frame's fraction,
    // unclamped above and held at the centre below. The rim crossing is a property of the segment
    // between the points themselves, not of the clamped ends the replay draws.
    const auto drawnFraction = [&](double value) { return qMax(0.0, frame.polarFraction(value)); };
    const auto unitPoint = [&](double angleCoord, double value)
    {
        const double theta = angular->coordToAngleRad(angleCoord);
        const double fraction = drawnFraction(value);
        return QPointF(std::cos(theta) * fraction, std::sin(theta) * fraction);
    };

    const double rimValue = frame.polarValue(1.0);
    const double clampedValue = frame.polarValue(kClampFraction);

    // A value the radial scale cannot take (zero or the other sign on a logarithmic axis) has no
    // place in the disc: it is a gap, exactly like a NaN value. The frame's fraction says so without
    // the trigonometry of a full map, which the replay pays once per point anyway.
    const auto isDrawn = [&](const QCPGraphData& data)
    {
        return std::isfinite(data.key) && std::isfinite(data.value)
            && std::isfinite(frame.polarFraction(data.value));
    };

    // Where the segment from a to b, drawn straight in the plot, crosses the rim. Only called for a
    // segment that straddles it. \a anchor is the key of its beyond end: the crossing belongs to the
    // branch of the angular range that end is on.
    const auto rimCrossing = [&](double angleA, double valueA, double angleB, double valueB,
                                 double anchor)
    {
        const QPointF from = unitPoint(angleA, valueA);
        const QPointF delta = unitPoint(angleB, valueB) - from;
        const double a = QPointF::dotProduct(delta, delta);
        const double b = 2.0 * QPointF::dotProduct(from, delta);
        const double c = QPointF::dotProduct(from, from) - 1.0;
        const double discriminant = b * b - 4.0 * a * c;
        // The first of the two rim crossings the segment meets, on the segment itself.
        double t = 0.0;
        if (a > 0 && discriminant >= 0)
        {
            const double root = std::sqrt(discriminant);
            const double entry = (-b - root) / (2.0 * a);
            const double leave = (-b + root) / (2.0 * a);
            if (entry >= 0.0 && entry <= 1.0)
                t = entry;
            else if (leave >= 0.0 && leave <= 1.0)
                t = leave;
        }
        const QPointF crossing = from + t * delta;
        // The angular coordinate of that point, on the branch of the beyond end: the frame repeats
        // every angular range in coordinate units.
        const double angle = angular->angleRadToCoord(std::atan2(crossing.y(), crossing.x()));
        return angle + span * std::round((anchor - angle) / span);
    };

    std::vector<QPointF> line; // the recorded polyline of the current run
    bool beyond = false; // does the last recorded point lie beyond the rim?
    double beyondAngle = 0; // where the recording left the rim

    const auto appendBeyondArc = [&](double from, double to)
    {
        const double gap = std::abs(to - from);
        const double step = span * kBeyondStep / 360.0;
        if (!(gap > step))
            return;
        const int count = static_cast<int>(std::ceil(gap / step));
        for (int i = 1; i < count; ++i)
            line.emplace_back(from + (to - from) * i / count, clampedValue);
    };

    // The decorator styles a segment as a whole: 2.x takes its pen and brush when both of its ends
    // are selected. A marker is styled by the point it marks.
    const bool hasSelection = mSelectionDecorator && !mSelection.isEmpty();
    bool lineSelected = false;

    const auto scatterStyleFor = [&](bool selected)
    {
        return selected ? mSelectionDecorator->getFinalScatterStyle(mScatterStyle)
                        : withPen(mScatterStyle, mPen);
    };

    const auto flushLine = [&]
    {
        if (line.empty())
            return;
        if (drawsLine)
        {
            const QPen pen = lineSelected ? mSelectionDecorator->pen() : mPen;
            context.polyline(std::make_shared<const std::vector<QPointF>>(line), pen);
            const QBrush brush = lineSelected ? mSelectionDecorator->brush() : mBrush;
            if (drawsFill && brush.style() != Qt::NoBrush)
            {
                std::vector<QCP::Position> vertices;
                vertices.reserve(line.size());
                for (const QPointF& point : line)
                    vertices.emplace_back(QCP::Data { point.x(), point.y() });
                context.polygon(vertices, QPen(Qt::NoPen), brush);
            }
        }
        line.clear();
        beyond = false;
        lineSelected = false;
    };

    // Markers of the points inside the radial range, as 2.1's clip kept them: a marker a little past
    // a range end still belongs to the graph that runs through it.
    const double clipMargin = radial->range().size() * 0.05;
    const double upperClipValue
        = radial->range().upper + (radial->rangeReversed() ? 0.0 : clipMargin);
    const double lowerClipValue
        = radial->range().lower - (radial->rangeReversed() ? clipMargin : 0.0);
    std::vector<QPointF> markers;
    bool markersSelected = false;
    const auto flushMarkers = [&]
    {
        if (drawsMarkers && !markers.empty())
            context.markers(std::make_shared<const std::vector<QPointF>>(markers),
                            scatterStyleFor(markersSelected));
        markers.clear();
    };

    const auto [begin, end] = window();

    QCPGraphData previous;
    bool previousSelected = false;
    for (auto it = begin; it != end; ++it)
    {
        const QCPGraphData& data = *it;
        if (!isDrawn(data))
        {
            flushLine();
            previousSelected = false;
            continue;
        }
        // Only a decorated selection varies the runs: the selection is not asked at all otherwise,
        // which keeps a plain graph's re-record as cheap as it was.
        const int index = static_cast<int>(it - mDataContainer->constBegin());
        const bool selected
            = hasSelection && mSelection.contains(QCPDataSelection(QCPDataRange(index, index + 1)));
        const bool isBeyond = drawnFraction(data.value) > 1.0;

        if (!line.empty())
        {
            const bool segmentSelected = previousSelected && selected;
            if (segmentSelected != lineSelected)
            {
                // The joint belongs to both runs: the next one starts with the point they share.
                flushLine();
                line.emplace_back(previous.key, previous.value);
                beyond = drawnFraction(previous.value) > 1.0;
                beyondAngle = previous.key;
            }
            lineSelected = segmentSelected;
        }

        if (drawsMarkers && data.value > lowerClipValue && data.value < upperClipValue)
        {
            if (!markers.empty() && selected != markersSelected)
                flushMarkers();
            markersSelected = selected;
            markers.emplace_back(data.key, data.value);
        }

        if (line.empty())
        {
            line.emplace_back(data.key, data.value);
            beyond = isBeyond;
            beyondAngle = data.key;
            previous = data;
            previousSelected = selected;
            continue;
        }
        if (isBeyond)
        {
            if (!beyond)
            {
                beyondAngle
                    = rimCrossing(previous.key, previous.value, data.key, data.value, data.key);
                line.emplace_back(beyondAngle, rimValue);
                // The hidden side starts with a radial step: a thick pen takes its width from here,
                // not from the rim, and leaves no sliver along it.
                line.emplace_back(beyondAngle, clampedValue);
            }
            appendBeyondArc(beyondAngle, data.key);
            line.emplace_back(data.key, data.value);
        }
        else
        {
            if (beyond)
            {
                const double crossing = rimCrossing(previous.key, previous.value, data.key,
                                                    data.value, previous.key);
                appendBeyondArc(beyondAngle, crossing);
                line.emplace_back(crossing, clampedValue);
                line.emplace_back(crossing, rimValue);
            }
            line.emplace_back(data.key, data.value);
        }
        beyond = isBeyond;
        beyondAngle = data.key;
        previous = data;
        previousSelected = selected;
    }
    flushLine();
    flushMarkers();

    // A decorator subclass draws a decoration of its own: the base class's is empty, and recording a
    // painter callback for it would make every selection change a view change.
    if (mSelectionDecorator && !mSelection.isEmpty()
        && typeid(*mSelectionDecorator) != typeid(QCPSelectionDecorator))
        context.painter(
            [this](QCPPainter& painter, const QCPTransform&)
            {
                if (mSelectionDecorator)
                    mSelectionDecorator->drawDecoration(&painter, selection());
            });
}
