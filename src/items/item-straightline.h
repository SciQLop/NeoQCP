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

#ifndef QCP_ITEM_STRAIGHTLINE_H
#define QCP_ITEM_STRAIGHTLINE_H

#include "../global.h"
#include "item.h"

class QCPPainter;
class QCustomPlot;

template <typename T, typename Self = void>
class QCPItemStraightLineOps;

class QCP_LIB_DECL QCPItemStraightLine : public QCPAbstractItem
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(QPen pen READ pen WRITE setPen)
    Q_PROPERTY(QPen selectedPen READ selectedPen WRITE setSelectedPen)
    /// \endcond

public:
    template <typename T>
    using OpsFor = QCPItemStraightLineOps<T>;

    explicit QCPItemStraightLine(QCustomPlot* parentPlot);
    virtual ~QCPItemStraightLine() override;

    // getters:
    QPen pen() const { return mPen; }

    QPen selectedPen() const { return mSelectedPen; }

    // setters;
    void setPen(const QPen& pen);
    void setSelectedPen(const QPen& pen);

    // reimplemented virtual methods:
    void draw(QCPDrawContext& context) const override;

    QCPItemPosition* const point1;
    QCPItemPosition* const point2;

protected:
    // property members:
    QPen mPen, mSelectedPen;

    // non-virtual methods:
    QLineF getRectClippedStraightLine(const QCPVector2D& base, const QCPVector2D& vec,
                                      const QRect& rect) const;
    QPen mainPen() const;
};

// The fluent handle of a QCPItemStraightLine; see QCPItemOps for the subclass contract.
template <typename T, typename Self>
class QCPItemStraightLineOps : public QCPItemOps<T, qcp::OpsRet<Self, QCPItemStraightLineOps<T>>>
{
    using Base = QCPItemOps<T, qcp::OpsRet<Self, QCPItemStraightLineOps<T>>>;

public:
    using typename Base::Ret;
    using Base::Base;

    Ret setPoint1(double key, double value)
    {
        return this->apply([&](T& i) { i.point1->setCoords(key, value); });
    }

    Ret setPoint2(double key, double value)
    {
        return this->apply([&](T& i) { i.point2->setCoords(key, value); });
    }

    Ret setPen(const QPen& pen)
    {
        return this->apply([&](T& o) { o.setPen(pen); });
    }

    Ret setSelectedPen(const QPen& pen)
    {
        return this->apply([&](T& o) { o.setSelectedPen(pen); });
    }
};

#endif // QCP_ITEM_STRAIGHTLINE_H
