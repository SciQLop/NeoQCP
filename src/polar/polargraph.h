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

#ifndef QCP_POLARGRAPH_H
#define QCP_POLARGRAPH_H

#include "../axis/range.h"
#include "../global.h"
#include "../handle-ops.h"
#include "../plottables/plottable-graph.h"
#include "../plottables/plottable1d.h"
#include "../scatterstyle.h"
#include "../selection.h"
#include "../vector2d.h"
#include "angularaxis.h"
#include "radialaxis.h"
#include <memory>
#include <utility>

class QCPCoordinateSystem;
class QCPDrawContext;

template <typename T, typename Self = void>
class QCPPolarGraphOps;

/*!
  A graph in a polar area, drawn in the polar system its two axes build: a data point is an angular
  coordinate and a radial value.

  The drawing is recorded once, in data space, and replayed through the area's frame: a rotation of
  the angular axis moves the recording, it does not record it again. What the recording does depend
  on is the radial range - a radial zoom re-records - and, for a non-periodic graph (\ref
  setPeriodic), the angular range, whose keys window the recorded data.

  Beyond the radial range the recording is clamped outside the rim (\ref QCPPolarArea::radius) and
  densified there, so a run that leaves the disc is hidden along the rim instead of being drawn as
  a chord through it. Below the radial range a point lands on the centre.
*/
class QCP_LIB_DECL QCPPolarGraph : public QCPAbstractPlottable1D<QCPGraphData>
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES

    /// \endcond

public:
    /*!
      Defines how the graph's line is represented visually in the plot. The line is drawn with the
      current pen of the graph (\ref setPen).
      \see setLineStyle
    */
    enum LineStyle
    {
        lsNone ///< data points are not connected with any lines (e.g. data only represented
               ///< with symbols according to the scatter style, see \ref setScatterStyle)
        ,
        lsLine ///< data points are connected by a straight line
    };
    Q_ENUM(LineStyle)

    template <typename T>
    using OpsFor = QCPPolarGraphOps<T>;

    QCPPolarGraph(QCPPolarAxisAngular* keyAxis, QCPPolarAxisRadial* valueAxis,
                  QCP::Capabilities required = QCP::Capabilities());
    explicit QCPPolarGraph(QCPCoordinateSystem* system,
                           QCP::Capabilities required = QCP::Capabilities());

    // getters:
    bool periodic() const { return mPeriodic; }

    QCPPolarAxisAngular* angularAxis() const;

    QCPPolarAxisRadial* radialAxis() const;

    QSharedPointer<QCPGraphDataContainer> data() const { return mDataContainer; }

    LineStyle lineStyle() const { return mLineStyle; }

    QCPScatterStyle scatterStyle() const { return mScatterStyle; }

    // setters:
    void setPeriodic(bool enabled);
    void setLineStyle(LineStyle ls);
    void setScatterStyle(const QCPScatterStyle& style);
    void setData(QSharedPointer<QCPGraphDataContainer> data);
    void setData(const QVector<double>& keys, const QVector<double>& values,
                 bool alreadySorted = false);

    // non-property methods:
    void addData(const QVector<double>& keys, const QVector<double>& values,
                 bool alreadySorted = false);
    void addData(double key, double value);
    void coordsToPixels(double key, double value, double& x, double& y) const;
    const QPointF coordsToPixels(double key, double value) const;
    void pixelsToCoords(double x, double y, double& key, double& value) const;
    void pixelsToCoords(const QPointF& pixelPos, double& key, double& value) const;
    void rescaleAxes(bool onlyEnlarge = false) const;
    void rescaleKeyAxis(bool onlyEnlarge = false) const;
    void rescaleValueAxis(bool onlyEnlarge = false, bool inKeyRange = false) const;

    double selectTest(const QPointF& pos, bool onlySelectable,
                      QVariant* details = nullptr) const override;
    QCPDataSelection selectTestRect(const QRectF& rect, bool onlySelectable) const override;
    QCPRange getKeyRange(bool& foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth) const override;
    QCPRange getValueRange(bool& foundRange, QCP::SignDomain inSignDomain = QCP::sdBoth,
                           const QCPRange& inKeyRange = QCPRange()) const override;
    void drawLegendIcon(QCPDrawContext& icon) const override;
    void setSystem(QCPCoordinateSystem* system) override;

    // introduced virtual methods:
    void draw(QCPDrawContext& context) const override;
    quint64 dependencyRevision() const override;

protected:
    // The keys the recording holds: 2.1's window (the whole container for a periodic graph).
    [[nodiscard]] std::pair<QCPGraphDataContainer::const_iterator,
                            QCPGraphDataContainer::const_iterator>
    window() const;
    [[nodiscard]] double visibleDistance(const QPointF& pos, const QCPTransform& frame) const;

    // property members:
    LineStyle mLineStyle;
    QCPScatterStyle mScatterStyle;
    bool mPeriodic;

    friend class TestPolar;
};

// The handle of QCPPolarGraph: what add<T>() returns for it.
template <typename T, typename Self>
class QCPPolarGraphOps : public QCPPlottableOps<T, qcp::OpsRet<Self, QCPPolarGraphOps<T>>>
{
    using Base = QCPPlottableOps<T, qcp::OpsRet<Self, QCPPolarGraphOps<T>>>;

public:
    using Base::Base;
    using typename Base::Ret;

    Ret setPeriodic(bool enabled)
    {
        return this->apply([&](T& o) { o.setPeriodic(enabled); });
    }

    Ret setLineStyle(QCPPolarGraph::LineStyle style)
    {
        return this->apply([&](T& o) { o.setLineStyle(style); });
    }

    Ret setScatterStyle(const QCPScatterStyle& style)
    {
        return this->apply([&](T& o) { o.setScatterStyle(style); });
    }

    Ret setAntialiasedFill(bool enabled)
    {
        return this->apply([&](T& o) { o.setAntialiasedFill(enabled); });
    }

    Ret setAntialiasedScatters(bool enabled)
    {
        return this->apply([&](T& o) { o.setAntialiasedScatters(enabled); });
    }

    Ret setData(const QVector<double>& keys, const QVector<double>& values,
                bool alreadySorted = false)
    {
        return this->apply([&](T& o) { o.setData(keys, values, alreadySorted); });
    }
};

#endif // QCP_POLARGRAPH_H
