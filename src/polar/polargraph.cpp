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

#include "../coordinate-system.h"
#include "../coords.h"
#include "../painting/draw-commands.h"
#include "../painting/draw-context.h"
#include "polar-area.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace
{
// The radial fraction the frame clamps a value beyond the range to (see QCPTransform::Polar).
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

    // Where a data point sits in the unit disc, unclamped: the rim crossing is a property of the
    // segment between the points themselves, not of the clamped ends the replay draws.
    const auto unitPoint = [&](double angleCoord, double value)
    {
        const double theta = angular->coordToAngleRad(angleCoord);
        const double fraction = radial->coordToRadius(value) / radius;
        return QPointF(std::cos(theta) * fraction, std::sin(theta) * fraction);
    };

    const double rimValue = radial->radiusToCoord(radius);
    const double clampedValue = radial->radiusToCoord(kClampFraction * radius);

    // Where the segment from a to b, drawn straight in the plot, crosses the rim. Only called for a
    // segment that straddles it.
    const auto rimCrossing = [&](double angleA, double valueA, double angleB, double valueB)
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
        // The angular coordinate of that point, on the branch of the segment: the frame repeats
        // every angular range in coordinate units.
        const double angle = angular->angleRadToCoord(std::atan2(crossing.y(), crossing.x()));
        return angle + span * std::round(0.5 * (angleA + angleB - angle) / span);
    };

    std::vector<QPointF> line; // the recorded polyline of the current run
    std::vector<QPointF> points; // the data points of the current run, for the markers
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

    const auto flush = [&]
    {
        if (line.empty())
            return;
        if (drawsLine)
        {
            context.polyline(std::make_shared<const std::vector<QPointF>>(line), mPen);
            if (drawsFill)
            {
                std::vector<QCP::Position> vertices;
                vertices.reserve(line.size());
                for (const QPointF& point : line)
                    vertices.emplace_back(QCP::Data { point.x(), point.y() });
                context.polygon(vertices, QPen(Qt::NoPen), mBrush);
            }
        }
        if (drawsMarkers && !points.empty())
            context.markers(std::make_shared<const std::vector<QPointF>>(points), mScatterStyle);
        line.clear();
        points.clear();
        beyond = false;
    };

    // 2.1's window: the whole container for a periodic graph, the keys inside the angular range
    // otherwise.
    auto begin = mPeriodic ? mDataContainer->constBegin()
                           : mDataContainer->findBegin(angular->range().lower);
    auto end
        = mPeriodic ? mDataContainer->constEnd() : mDataContainer->findEnd(angular->range().upper);

    QCPGraphData previous;
    for (auto it = begin; it != end; ++it)
    {
        const QCPGraphData& data = *it;
        if (!std::isfinite(data.key) || !std::isfinite(data.value))
        {
            flush();
            continue;
        }
        // Exactly at the rim is in range: 2.1 kept it, and it is the rim the frame clamps to.
        const bool isBeyond = radial->coordToRadius(data.value) / radius > 1.0;
        if (line.empty())
        {
            line.emplace_back(data.key, data.value);
            points.emplace_back(data.key, data.value);
            beyond = isBeyond;
            beyondAngle = data.key;
            previous = data;
            continue;
        }
        if (isBeyond)
        {
            if (!beyond)
            {
                beyondAngle = rimCrossing(previous.key, previous.value, data.key, data.value);
                line.emplace_back(beyondAngle, rimValue);
            }
            appendBeyondArc(beyondAngle, data.key);
            line.emplace_back(data.key, data.value);
        }
        else
        {
            if (beyond)
                line.emplace_back(rimCrossing(previous.key, previous.value, data.key, data.value),
                                  rimValue);
            line.emplace_back(data.key, data.value);
        }
        points.emplace_back(data.key, data.value);
        beyond = isBeyond;
        beyondAngle = data.key;
        previous = data;
    }
    flush();
}
