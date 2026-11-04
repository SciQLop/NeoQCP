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

#ifndef QCP_ITEM_RECT_H
#define QCP_ITEM_RECT_H

#include "../global.h"
#include "item.h"

class QCPPainter;
class QCustomPlot;

template <typename T, typename Self = void>
class QCPItemRectOps;

class QCP_LIB_DECL QCPItemRect : public QCPAbstractItem
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(QPen pen READ pen WRITE setPen)
    Q_PROPERTY(QPen selectedPen READ selectedPen WRITE setSelectedPen)
    Q_PROPERTY(QBrush brush READ brush WRITE setBrush)
    Q_PROPERTY(QBrush selectedBrush READ selectedBrush WRITE setSelectedBrush)
    /// \endcond

public:
    template <typename T>
    using OpsFor = QCPItemRectOps<T>;

    explicit QCPItemRect(QCustomPlot* parentPlot);
    virtual ~QCPItemRect() override;

    // getters:
    QPen pen() const { return mPen; }

    QPen selectedPen() const { return mSelectedPen; }

    QBrush brush() const { return mBrush; }

    QBrush selectedBrush() const { return mSelectedBrush; }

    // setters;
    void setPen(const QPen& pen);
    void setSelectedPen(const QPen& pen);
    void setBrush(const QBrush& brush);
    void setSelectedBrush(const QBrush& brush);

    // reimplemented virtual methods:
    virtual double selectTest(const QPointF& pos, bool onlySelectable,
                              QVariant* details = nullptr) const override;

    // bounds for culling; see QCPLayerable::pixelBounds()
    [[nodiscard]] std::optional<QRectF> pixelBounds() const override;

    QCPItemPosition* const topLeft;
    QCPItemPosition* const bottomRight;
    QCPItemAnchor* const top;
    QCPItemAnchor* const topRight;
    QCPItemAnchor* const right;
    QCPItemAnchor* const bottom;
    QCPItemAnchor* const bottomLeft;
    QCPItemAnchor* const left;

protected:
    enum AnchorIndex
    {
        aiTop,
        aiTopRight,
        aiRight,
        aiBottom,
        aiBottomLeft,
        aiLeft
    };

    // property members:
    QPen mPen, mSelectedPen;
    QBrush mBrush, mSelectedBrush;

    // reimplemented virtual methods:
    virtual void draw(QCPPainter* painter) override;
    virtual QPointF anchorPixelPosition(int anchorId) const override;

    // non-virtual methods:
    QPen mainPen() const;
    QBrush mainBrush() const;
};

// The fluent handle of a QCPItemRect; see QCPItemOps for the subclass contract.
template <typename T, typename Self>
class QCPItemRectOps : public QCPItemOps<T, qcp::OpsRet<Self, QCPItemRectOps<T>>>
{
    using Base = QCPItemOps<T, qcp::OpsRet<Self, QCPItemRectOps<T>>>;

public:
    using typename Base::Ret;
    using Base::Base;

    Ret setTopLeft(double key, double value)
    {
        return this->apply([&](T& i) { i.topLeft->setCoords(key, value); });
    }

    Ret setBottomRight(double key, double value)
    {
        return this->apply([&](T& i) { i.bottomRight->setCoords(key, value); });
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
};

#endif // QCP_ITEM_RECT_H
