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

#ifndef QCP_ITEM_H
#define QCP_ITEM_H

#include "axis/axis.h"
#include "global.h"
#include "layer.h"
#include "vector2d.h"

class QCPPainter;
class QCustomPlot;
class QCPItemPosition;
class QCPAbstractItem;
class QCPAxisRect;
template <typename T, typename Self = void>
class QCPItemOps;

class QCP_LIB_DECL QCPItemAnchor
{
    Q_GADGET

public:
    QCPItemAnchor(QCustomPlot* parentPlot, QCPAbstractItem* parentItem, const QString& name,
                  int anchorId = -1);
    virtual ~QCPItemAnchor();

    // getters:
    [[nodiscard]] QString name() const { return mName; }

    [[nodiscard]] virtual QPointF pixelPosition() const;

protected:
    // property members:
    QString mName;

    // non-property members:
    QCustomPlot* mParentPlot;
    QCPAbstractItem* mParentItem;
    int mAnchorId;
    QSet<QCPItemPosition*> mChildrenX, mChildrenY;

    // introduced virtual methods:
    virtual QCPItemPosition* toQCPItemPosition() { return nullptr; }

    // non-virtual methods:
    void addChildX(QCPItemPosition* pos); // called from pos when this anchor is set as parent
    void removeChildX(
        QCPItemPosition* pos); // called from pos when its parent anchor is reset or pos deleted
    void addChildY(QCPItemPosition* pos); // called from pos when this anchor is set as parent
    void removeChildY(
        QCPItemPosition* pos); // called from pos when its parent anchor is reset or pos deleted

private:
    Q_DISABLE_COPY(QCPItemAnchor)

    friend class QCPItemPosition;
    friend class QCustomPlot; // take()/add() move an item between plots
};

class QCP_LIB_DECL QCPItemPosition : public QCPItemAnchor
{
    Q_GADGET

public:
    /*!
      Defines the ways an item position can be specified. Thus it defines what the numbers passed to
      \ref setCoords actually mean.

      \see setType
    */
    enum PositionType
    {
        ptAbsolute ///< Static positioning in pixels, starting from the top left corner of the
                   ///< viewport/widget.
        ,
        ptViewportRatio ///< Static positioning given by a fraction of the viewport size. For
                        ///< example, if you call setCoords(0, 0), the position will be at the top
                        ///< left corner of the viewport/widget. setCoords(1, 1) will be at the
                        ///< bottom right corner, setCoords(0.5, 0) will be horizontally centered
                        ///< and vertically at the top of the viewport/widget, etc.
        ,
        ptAxisRectRatio ///< Static positioning given by a fraction of the axis rect size (see \ref
                        ///< setAxisRect). For example, if you call setCoords(0, 0), the position
                        ///< will be at the top left corner of the axis rect. setCoords(1, 1) will
                        ///< be at the bottom right corner, setCoords(0.5, 0) will be horizontally
                        ///< centered and vertically at the top of the axis rect, etc. You can also
                        ///< go beyond the axis rect by providing negative coordinates or
                        ///< coordinates larger than 1.
        ,
        ptPlotCoords ///< Dynamic positioning at a plot coordinate defined by two axes (see \ref
                     ///< setAxes).
        ,
        ptAxisRectAbsolute ///< Static positioning in pixels, starting from the top left corner of
                           ///< the axis rect (see \ref setAxisRect). Unlike \ref ptAbsolute, which
                           ///< is measured from the widget origin and therefore lands inside the
                           ///< axis/label margins for small coordinates, this keeps the offset
                           ///< relative to the plotting area and follows it across resizes.
    };
    Q_ENUM(PositionType)

    QCPItemPosition(QCustomPlot* parentPlot, QCPAbstractItem* parentItem, const QString& name);
    virtual ~QCPItemPosition() override;

    // getters:
    [[nodiscard]] PositionType type() const { return typeX(); }

    [[nodiscard]] PositionType typeX() const { return mPositionTypeX; }

    [[nodiscard]] PositionType typeY() const { return mPositionTypeY; }

    [[nodiscard]] QCPItemAnchor* parentAnchor() const { return parentAnchorX(); }

    [[nodiscard]] QCPItemAnchor* parentAnchorX() const { return mParentAnchorX; }

    [[nodiscard]] QCPItemAnchor* parentAnchorY() const { return mParentAnchorY; }

    [[nodiscard]] double key() const { return mKey; }

    [[nodiscard]] double value() const { return mValue; }

    [[nodiscard]] QPointF coords() const { return QPointF(mKey, mValue); }

    [[nodiscard]] QCPAxis* keyAxis() const { return mKeyAxis.data(); }

    [[nodiscard]] QCPAxis* valueAxis() const { return mValueAxis.data(); }

    [[nodiscard]] QCPAxisRect* axisRect() const;
    //! (0, 0) for an item outside a plot (taken, see QCustomPlot::take).
    [[nodiscard]] virtual QPointF pixelPosition() const override;

    // setters:
    void setType(PositionType type);
    void setTypeX(PositionType type);
    void setTypeY(PositionType type);
    bool setParentAnchor(QCPItemAnchor* parentAnchor, bool keepPixelPosition = false);
    bool setParentAnchorX(QCPItemAnchor* parentAnchor, bool keepPixelPosition = false);
    bool setParentAnchorY(QCPItemAnchor* parentAnchor, bool keepPixelPosition = false);
    void setCoords(double key, double value);
    void setCoords(const QPointF& pos);
    void setAxes(QCPAxis* keyAxis, QCPAxis* valueAxis);
    void setAxisRect(QCPAxisRect* axisRect);
    void setPixelPosition(const QPointF& pixelPosition);

protected:
    // property members:
    PositionType mPositionTypeX, mPositionTypeY;
    QPointer<QCPAxis> mKeyAxis, mValueAxis;
    QPointer<QCPAxisRect> mAxisRect;
    double mKey, mValue;
    QCPItemAnchor *mParentAnchorX, *mParentAnchorY;

    // reimplemented virtual methods:
    virtual QCPItemPosition* toQCPItemPosition() override { return this; }

private:
    Q_DISABLE_COPY(QCPItemPosition)
    friend class QCPItemTracer;

    // The tracer derives this position from its graph on every draw: storing must not mark, or the
    // draw itself would dirty the layer and the plot would never settle.
    void assignCoords(double key, double value);
};
Q_DECLARE_METATYPE(QCPItemPosition::PositionType)

class QCP_LIB_DECL QCPAbstractItem : public QCPLayerable
{
    Q_OBJECT
    /// \cond INCLUDE_QPROPERTIES
    Q_PROPERTY(bool clipToAxisRect READ clipToAxisRect WRITE setClipToAxisRect)
    Q_PROPERTY(QCPAxisRect* clipAxisRect READ clipAxisRect WRITE setClipAxisRect)
    Q_PROPERTY(bool selectable READ selectable WRITE setSelectable NOTIFY selectableChanged)
    Q_PROPERTY(bool selected READ selected WRITE setSelected NOTIFY selectionChanged)
    /// \endcond

public:
    template <typename T>
    using OpsFor = QCPItemOps<T>;

    explicit QCPAbstractItem(QCustomPlot* parentPlot);
    virtual ~QCPAbstractItem() override;

    // getters:
    [[nodiscard]] bool clipToAxisRect() const { return mClipToAxisRect; }

    [[nodiscard]] QCPAxisRect* clipAxisRect() const;

    [[nodiscard]] bool selectable() const { return mSelectable; }

    [[nodiscard]] bool selected() const { return mSelected; }

    // setters:
    void setClipToAxisRect(bool clip);
    void setClipAxisRect(QCPAxisRect* rect);
    Q_SLOT void setSelectable(bool selectable);
    Q_SLOT void setSelected(bool selected);

    // reimplemented virtual methods:
    virtual double selectTest(const QPointF& pos, bool onlySelectable,
                              QVariant* details = nullptr) const override
        = 0;

    // non-virtual methods:
    [[nodiscard]] QList<QCPItemPosition*> positions() const { return mPositions; }

    [[nodiscard]] QList<QCPItemAnchor*> anchors() const { return mAnchors; }

    [[nodiscard]] QCPItemPosition* position(const QString& name) const;
    [[nodiscard]] QCPItemAnchor* anchor(const QString& name) const;
    [[nodiscard]] bool hasAnchor(const QString& name) const;

signals:
    void selectionChanged(bool selected);
    void selectableChanged(bool selectable);
    void deleteRequested();

protected:
    // property members:
    bool mClipToAxisRect;
    QPointer<QCPAxisRect> mClipAxisRect;
    QList<QCPItemPosition*> mPositions;
    QList<QCPItemAnchor*> mAnchors;
    bool mSelectable, mSelected;

    // reimplemented virtual methods:
    virtual QCP::Interaction selectionCategory() const override;
    virtual QRect clipRect() const override;
    virtual void applyDefaultAntialiasingHint(QCPPainter* painter) const override;
    virtual void draw(QCPPainter* painter) override = 0;
    // events:
    virtual void selectEvent(QMouseEvent* event, bool additive, const QVariant& details,
                             bool* selectionStateChanged) override;
    virtual void deselectEvent(bool* selectionStateChanged) override;

    // introduced virtual methods:
    virtual QPointF anchorPixelPosition(int anchorId) const;

    // non-virtual methods:
    double rectDistance(const QRectF& rect, const QPointF& pos, bool filledRect) const;
    QCPItemPosition* createPosition(const QString& name);
    QCPItemAnchor* createAnchor(const QString& name, int anchorId);

private:
    Q_DISABLE_COPY(QCPAbstractItem)

    QPointer<QCustomPlot> mRegisteredPlot; // the plot whose item list holds this one

    friend class QCustomPlot;
    friend class QCPItemAnchor;
};

/*!
  The fluent handle of an item: what QCustomPlot::add<T>() returns. An item's constructor takes the
  plot only, so add<T>() needs no arguments. Subclass contract as in QCPObjectOps; the position
  setters of a concrete item (setStart/setEnd, ...) take coordinates in the position's current type,
  which is ptPlotCoords on the plot's default axes for a new item (QCPAbstractItem::createPosition).
*/
template <typename T, typename Self>
class QCPItemOps : public QCPLayerableOps<T, qcp::OpsRet<Self, QCPItemOps<T>>>
{
    using Base = QCPLayerableOps<T, qcp::OpsRet<Self, QCPItemOps<T>>>;

public:
    using typename Base::Ret;
    using Base::Base;

    Ret setClipToAxisRect(bool clip)
    {
        return this->apply([&](T& i) { i.setClipToAxisRect(clip); });
    }
    Ret setPen(const QPen& pen)
    {
        return this->apply([&](T& i) { i.setPen(pen); });
    }
};

#endif // QCP_ITEM_H
