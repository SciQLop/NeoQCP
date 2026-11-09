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

#ifndef QCP_POLAR_POLAR_AREA_H
#define QCP_POLAR_POLAR_AREA_H

#include "../axis/range.h"
#include "../global.h"
#include "../handle-ops.h"
#include "../layoutelements/area.h"
#include <QPointer>

class QCustomPlot;
class QCPPolarAxisAngular;
class QCPPolarAxisRadial;
class QCPPolarGrid;
class QCPPolarGraph;

template <typename T, typename Self = void>
class QCPPolarAreaOps;

/*!
  A polar plot's area: it holds one angular axis, the radial axes and the grid they share, lists the
  polar graphs drawn on it, and drags and zooms them.
*/
class QCP_LIB_DECL QCPPolarArea : public QCPArea
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES

    /// \endcond

public:
    template <typename T>
    using OpsFor = QCPPolarAreaOps<T>;

    explicit QCPPolarArea(QCustomPlot* parentPlot);
    virtual ~QCPPolarArea() override;

    // getters:
    QCPPolarAxisAngular* angularAxis() const { return mAngularAxis; }

    int radialAxisCount() const { return int(mRadialAxes.size()); }

    QCPPolarAxisRadial* radialAxis(int index = 0) const;

    QList<QCPPolarAxisRadial*> radialAxes() const { return mRadialAxes; }

    QCPPolarGrid* grid() const { return mGrid; }

    QPointF center() const { return mCenter; }

    double radius() const { return mRadius; }

    double rangeZoomFactor() const { return mRangeZoomFactor; }

    QRegion exactClipRegion() const;

    QList<QCPPolarGraph*> graphs() const { return mGraphs; }

    // non-property methods:
    QCPPolarAxisRadial* addRadialAxis(QCPPolarAxisRadial* axis = nullptr);
    bool removeRadialAxis(QCPPolarAxisRadial* axis);
    bool removeGraph(QCPPolarGraph* graph);
    void rescale(bool onlyVisiblePlottables = false);

    /*! Sets the factor a wheel step zooms every radial range by, around the radius under the
     * cursor.
     */
    void setRangeZoomFactor(double factor) { mRangeZoomFactor = factor; }

    // Out of line: QCustomPlot is incomplete here, and clang checks add<T>'s body at its
    // definition.
    [[nodiscard]] bool plotTakesNewParts() const;

    // The handle form of the graph constructor: the graph belongs to this area and draws on its
    // first radial axis. args go to the handle's setData.
    template <std::same_as<QCPPolarGraph> T, typename... Args>
    qcp::HandleFor<T> add(Args&&... args)
    {
        using Handle = qcp::HandleFor<T>;
        static_assert(
            requires(Handle h) { h.setData(std::declval<Args>()...); },
            "add<T>(args...): the graph's handle has no setData taking these arguments");
        QCP_CHECK_OR_RETURN(plotTakesNewParts(), "the plot is being destroyed", Handle { });
        QCP_CHECK_OR_RETURN(mAngularAxis, "the polar area has no angular axis", Handle { });
        QCP_CHECK_OR_RETURN(radialAxis(0), "the polar area has no radial axis", Handle { });
        Handle handle = qcp::makeHandle<Handle>(new T(mAngularAxis, radialAxis(0)));
        handle.get()->markDirty();
        try
        {
            handle.setData(std::forward<Args>(args)...);
        }
        catch (...)
        {
            handle.remove(); // the graph unregisters itself from this area
            throw;
        }
        return handle;
    }

    // reimplemented virtual methods:
    virtual void update(UpdatePhase phase) override;
    virtual QList<QCPAxisBase*> allAxes() const override;

protected:
    void drawBackgroundBrush(QCPPainter* painter) override;
    void drawBackgroundPixmap(QCPPainter* painter) override;

    // Dirties every layer a change of the axes' ranges moves: the axes and the grid, then what the
    // graphs and their items draw. Mirrors QCPAxisRect::markAffectedLayersDirty.
    void markAffectedLayersDirty(bool invalidate = false);

protected:
    // non-property members:
    QCPPolarAxisAngular* mAngularAxis = nullptr;
    QList<QCPPolarAxisRadial*> mRadialAxes;
    QCPPolarGrid* mGrid = nullptr;
    QList<QCPPolarGraph*> mGraphs;
    QPointF mCenter;
    double mRadius = 1;
    double mRangeZoomFactor = 0.85;
    bool mDragging = false;
    QCPRange mDragAngularStart;
    QList<QPair<QPointer<QCPPolarAxisRadial>, QCPRange>> mDragRadialStart;
    QCP::AntialiasedElements mAADragBackup, mNotAADragBackup;

    // reimplemented virtual methods:
    // events:
    virtual void mousePressEvent(QMouseEvent* event, const QVariant& details) override;
    virtual void mouseMoveEvent(QMouseEvent* event, const QPointF& startPos) override;
    virtual void mouseReleaseEvent(QMouseEvent* event, const QPointF& startPos) override;
    virtual void wheelEvent(QWheelEvent* event) override;

    // non-virtual methods:
    bool registerPolarGraph(QCPPolarGraph* graph);

private:
    Q_DISABLE_COPY(QCPPolarArea)

    friend class QCustomPlot;
    friend class QCPPolarAxisAngular;
    friend class QCPPolarAxisRadial;
    friend class QCPPolarGrid;
    friend class QCPPolarGraph;
};

// The handle of a polar area: created in the layout by QCustomPlot::add<QCPPolarArea>(row, column).
template <typename T, typename Self>
class QCPPolarAreaOps : public QCPLayerableOps<T, qcp::OpsRet<Self, QCPPolarAreaOps<T>>>
{
    using Base = QCPLayerableOps<T, qcp::OpsRet<Self, QCPPolarAreaOps<T>>>;

public:
    using Base::Base;
    using typename Base::Ret;

    Ret setBackground(const QBrush& brush)
    {
        return this->apply([&](T& o) { o.setBackground(brush); });
    }

    Ret setBackground(const QPixmap& pixmap)
    {
        return this->apply([&](T& o) { o.setBackground(pixmap); });
    }

    // The handle form of the graph constructor: the graph belongs to this area and draws on its
    // first radial axis. args go to the graph handle's setData.
    template <std::same_as<QCPPolarGraph> Graph, typename... Args>
    qcp::HandleFor<Graph> add(Args&&... args)
    {
        auto* area = this->get();
        QCP_CHECK_OR_RETURN(area, "the handle is dead", qcp::HandleFor<Graph> { });
        return area->template add<Graph>(std::forward<Args>(args)...);
    }
};

// The handle is complete with only this header included: plot.add<QCPPolarArea>() is usable without
// polargraph.h (the graph's own handle only needs it when a graph is added).
static_assert(
    requires(qcp::HandleFor<QCPPolarArea> handle) { handle.setVisible(true); },
    "the polar area handle must be complete in this header");

#endif // QCP_POLAR_POLAR_AREA_H
