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

#ifndef QCP_ITEM_TRACER_H
#define QCP_ITEM_TRACER_H

#include "../global.h"
#include "../items/item.h"
#include "../plottables/graph-data.h"
#include "../plottables/plottable-multigraph.h"

class QCPPainter;
class QCustomPlot;

template <typename T, typename Self = void>
class QCPItemTracerOps;

class QCP_LIB_DECL QCPItemTracer : public QCPAbstractItem
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(QPen pen READ pen WRITE setPen)
    Q_PROPERTY(QPen selectedPen READ selectedPen WRITE setSelectedPen)
    Q_PROPERTY(QBrush brush READ brush WRITE setBrush)
    Q_PROPERTY(QBrush selectedBrush READ selectedBrush WRITE setSelectedBrush)
    Q_PROPERTY(double size READ size WRITE setSize)
    Q_PROPERTY(TracerStyle style READ style WRITE setStyle)
    Q_PROPERTY(QCPMultiGraph* graph READ graph WRITE setGraph)
    Q_PROPERTY(double graphKey READ graphKey WRITE setGraphKey)
    Q_PROPERTY(bool interpolating READ interpolating WRITE setInterpolating)
    /// \endcond

public:
    template <typename T>
    using OpsFor = QCPItemTracerOps<T>;

    /*!
      The different visual appearances a tracer item can have. Some styles size may be controlled
      with \ref setSize.

      \see setStyle
    */
    enum TracerStyle
    {
        tsNone ///< The tracer is not visible
        ,
        tsPlus ///< A plus shaped crosshair with limited size
        ,
        tsCrosshair ///< A plus shaped crosshair which spans the complete axis rect
        ,
        tsCircle ///< A circle
        ,
        tsSquare ///< A square
    };
    Q_ENUM(TracerStyle)

    explicit QCPItemTracer(QCustomPlot* parentPlot);
    virtual ~QCPItemTracer() override;

    // getters:
    QPen pen() const { return mPen; }

    QPen selectedPen() const { return mSelectedPen; }

    QBrush brush() const { return mBrush; }

    QBrush selectedBrush() const { return mSelectedBrush; }

    double size() const { return mSize; }

    TracerStyle style() const { return mStyle; }

    //! The graph this tracer follows, or an empty handle.
    [[nodiscard]] QCPHandle<QCPMultiGraph> graph() const
    {
        return QCPHandle<QCPMultiGraph>(mGraph.data());
    }

    //! The followed component index (0 when none).
    int component() const { return mComponent; }

    double graphKey() const { return mGraphKey; }

    bool interpolating() const { return mInterpolating; }

    // setters;
    void setPen(const QPen& pen);
    void setSelectedPen(const QPen& pen);
    void setBrush(const QBrush& brush);
    void setSelectedBrush(const QBrush& brush);
    void setSize(double size);
    void setStyle(TracerStyle style);
    //! Follows component \a component of \a graph; null frees the tracer. A component outside the
    //! graph's columns is a misuse.
    void setGraph(QCPMultiGraph* graph, int component = 0);
    //! Changes the followed component; out of range is a misuse. Does nothing without a graph
    //! (there is no target to select a column of).
    void setComponent(int component);
    void setGraphKey(double key);
    void setInterpolating(bool enabled);

    // reimplemented virtual methods:
    void draw(QCPDrawContext& context) const override;

    //! The graph the recorded position comes from: another one makes the recording stale.
    [[nodiscard]] const void* dataSourceIdentity() const override { return mGraph.data(); }

    //! The graph's revision: a new sample moves the tracer without a call on it.
    [[nodiscard]] quint64 dependencyRevision() const override;

    // non-virtual methods:
    void updatePosition();

    QCPItemPosition* const position;

protected:
    // property members:
    QPen mPen, mSelectedPen;
    QBrush mBrush, mSelectedBrush;
    double mSize;
    TracerStyle mStyle;
    QPointer<QCPMultiGraph> mGraph;
    int mComponent = 0;
    bool mFollowsGraph = false; // set by setGraph, so a graph's death is told from no graph
    double mGraphKey;
    bool mInterpolating;

    bool lostGraph() const
    {
        if (!mFollowsGraph)
            return false;
        if (!mGraph)
            return true;
        // M3: the graph lost the column the tracer followed: the last sample is not the target.
        return mComponent < 0 || mComponent >= mGraph->componentCount();
    }

    // non-virtual methods:
    QPen mainPen() const;
    QBrush mainBrush() const;
};
Q_DECLARE_METATYPE(QCPItemTracer::TracerStyle)

// The fluent handle of a QCPItemTracer; see QCPItemOps for the subclass contract.
template <typename T, typename Self>
class QCPItemTracerOps : public QCPItemOps<T, qcp::OpsRet<Self, QCPItemTracerOps<T>>>
{
    using Base = QCPItemOps<T, qcp::OpsRet<Self, QCPItemTracerOps<T>>>;

public:
    using typename Base::Ret;
    using Base::Base;

    Ret setPosition(double key, double value)
    {
        return this->apply([&](T& i) { i.position->setCoords(key, value); });
    }

    Ret setPen(const QPen& pen)
    {
        return this->apply([&](T& o) { o.setPen(pen); });
    }

    Ret setSelectedPen(const QPen& pen)
    {
        return this->apply([&](T& o) { o.setSelectedPen(pen); });
    }

    Ret setBrush(const QBrush& brush)
    {
        return this->apply([&](T& o) { o.setBrush(brush); });
    }

    Ret setSelectedBrush(const QBrush& brush)
    {
        return this->apply([&](T& o) { o.setSelectedBrush(brush); });
    }

    // The graph component to follow; a dead handle is refused with one misuse (use with() and
    // nullptr to detach).
    Ret setGraph(const QCPHandle<QCPMultiGraph>& graph, int component = 0)
    {
        return this->apply(
            [&](T& o)
            {
                QCP_CHECK_OR_RETURN(graph, "the graph handle is dead");
                o.setGraph(graph.get(), component);
            });
    }

    Ret setComponent(int component)
    {
        return this->apply([&](T& o) { o.setComponent(component); });
    }

    Ret setSize(double size)
    {
        return this->apply([&](T& o) { o.setSize(size); });
    }

    Ret setStyle(QCPItemTracer::TracerStyle style)
    {
        return this->apply([&](T& o) { o.setStyle(style); });
    }

    Ret setGraphKey(double key)
    {
        return this->apply([&](T& o) { o.setGraphKey(key); });
    }

    Ret setInterpolating(bool enabled)
    {
        return this->apply([&](T& o) { o.setInterpolating(enabled); });
    }
};

#endif // QCP_ITEM_TRACER_H
