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

#ifndef QCP_ITEM_CURVE_H
#define QCP_ITEM_CURVE_H

#include "../global.h"
#include "item.h"
#include "../lineending.h"

class QCPPainter;
class QCustomPlot;

template <typename T, typename Self = void>
class QCPItemCurveOps;

class QCP_LIB_DECL QCPItemCurve : public QCPAbstractItem
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(QPen pen READ pen WRITE setPen)
    Q_PROPERTY(QPen selectedPen READ selectedPen WRITE setSelectedPen)
    Q_PROPERTY(QCPLineEnding head READ head WRITE setHead)
    Q_PROPERTY(QCPLineEnding tail READ tail WRITE setTail)
    /// \endcond

public:
    template <typename T>
    using OpsFor = QCPItemCurveOps<T>;

    explicit QCPItemCurve(QCustomPlot* parentPlot);
    virtual ~QCPItemCurve() override;

    // getters:
    QPen pen() const { return mPen; }

    QPen selectedPen() const { return mSelectedPen; }

    QCPLineEnding head() const { return mHead; }

    QCPLineEnding tail() const { return mTail; }

    // setters;
    void setPen(const QPen& pen);
    void setSelectedPen(const QPen& pen);
    void setHead(const QCPLineEnding& head);
    void setTail(const QCPLineEnding& tail);

    // reimplemented virtual methods:
    virtual double selectTest(const QPointF& pos, bool onlySelectable,
                              QVariant* details = nullptr) const override;

    // what the layer's index needs; see QCPAbstractItem::dataExtent()
    [[nodiscard]] std::optional<QCPItemExtent> dataExtent() const override;

    QCPItemPosition* const start;
    QCPItemPosition* const startDir;
    QCPItemPosition* const endDir;
    QCPItemPosition* const end;

protected:
    // property members:
    QPen mPen, mSelectedPen;
    QCPLineEnding mHead, mTail;

    // reimplemented virtual methods:
    virtual void draw(QCPPainter* painter) override;

    // non-virtual methods:
    QPen mainPen() const;
};

// The fluent handle of a QCPItemCurve; see QCPItemOps for the subclass contract.
template <typename T, typename Self>
class QCPItemCurveOps : public QCPItemOps<T, qcp::OpsRet<Self, QCPItemCurveOps<T>>>
{
    using Base = QCPItemOps<T, qcp::OpsRet<Self, QCPItemCurveOps<T>>>;

public:
    using typename Base::Ret;
    using Base::Base;

    Ret setStart(double key, double value)
    {
        return this->apply([&](T& i) { i.start->setCoords(key, value); });
    }

    Ret setStartDir(double key, double value)
    {
        return this->apply([&](T& i) { i.startDir->setCoords(key, value); });
    }

    Ret setEndDir(double key, double value)
    {
        return this->apply([&](T& i) { i.endDir->setCoords(key, value); });
    }

    Ret setEnd(double key, double value)
    {
        return this->apply([&](T& i) { i.end->setCoords(key, value); });
    }

    Ret setPen(const QPen& pen)
    {
        return this->apply([&](T& o) { o.setPen(pen); });
    }

    Ret setSelectedPen(const QPen& pen)
    {
        return this->apply([&](T& o) { o.setSelectedPen(pen); });
    }

    Ret setHead(const QCPLineEnding& head)
    {
        return this->apply([&](T& o) { o.setHead(head); });
    }

    Ret setTail(const QCPLineEnding& tail)
    {
        return this->apply([&](T& o) { o.setTail(tail); });
    }
};

#endif // QCP_ITEM_CURVE_H
