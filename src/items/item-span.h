#ifndef QCP_ITEM_SPAN_H
#define QCP_ITEM_SPAN_H

#include "item-spanbase.h"

class QCPPainter;
class QCustomPlot;

template <typename T, typename Self = void>
class QCPItemSpanOps;

/*!
  A band across the whole axis rect, from \a lowerEdge to \a upperEdge.

  Qt::Vertical is a band between two keys, over the full height; Qt::Horizontal is the same band
  between two values, over the full width. The orientation is fixed at construction: the edges'
  position types are the vertical or the horizontal pair, and an orientation setter would have to
  rewrite them.
*/
class QCP_LIB_DECL QCPItemSpan : public QCPAbstractSpanItem
{
    Q_OBJECT
    Q_PROPERTY(QBrush brush READ brush WRITE setBrush)
    Q_PROPERTY(QBrush selectedBrush READ selectedBrush WRITE setSelectedBrush)
    Q_PROPERTY(QPen borderPen READ borderPen WRITE setBorderPen)
    Q_PROPERTY(QPen selectedBorderPen READ selectedBorderPen WRITE setSelectedBorderPen)
    Q_PROPERTY(bool movable READ movable WRITE setMovable)

public:
    template <typename T>
    using OpsFor = QCPItemSpanOps<T>;

    QCPItemSpan(QCustomPlot* parentPlot, Qt::Orientation orientation);

    Qt::Orientation orientation() const { return mOrientation; }

    QCPRange range() const;
    void setRange(const QCPRange& range);

    virtual void draw(QCPDrawContext& context) const override;
    virtual std::optional<double> hitTest(QCP::Pixels at, QVariant* details) const override;

    QCPItemPosition* const lowerEdge;
    QCPItemPosition* const upperEdge;
    QCPItemAnchor* const center;

    enum HitPart
    {
        hpNone = -2,
        hpFill = -1,
        hpLowerEdge = 0,
        hpUpperEdge = 1
    };

signals:
    void rangeChanged(const QCPRange& newRange);

protected:
    enum AnchorIndex
    {
        aiCenter
    };

    const Qt::Orientation mOrientation;
    HitPart mDragPart = hpNone;
    double mDragStartLower = 0.0;
    double mDragStartUpper = 0.0;

    //! The edge's pixel position along the range axis: x for a vertical span, y for a horizontal
    //! one.
    double rangePixel(const QCPItemPosition* edge) const;
    virtual QPointF anchorPixelPosition(int anchorId) const override;
    virtual void mousePressEvent(QMouseEvent* event, const QVariant& details) override;
    virtual void mouseMoveEvent(QMouseEvent* event, const QPointF& startPos) override;
    virtual void mouseReleaseEvent(QMouseEvent* event, const QPointF& startPos) override;
};

// The fluent handle of a QCPItemSpan; see QCPItemOps for the subclass contract.
template <typename T, typename Self>
class QCPItemSpanOps : public QCPItemOps<T, qcp::OpsRet<Self, QCPItemSpanOps<T>>>
{
    using Base = QCPItemOps<T, qcp::OpsRet<Self, QCPItemSpanOps<T>>>;

public:
    using Base::Base;
    using typename Base::Ret;

    Ret setLowerEdge(double key, double value)
    {
        return this->apply([&](T& i) { i.lowerEdge->setCoords(key, value); });
    }

    Ret setUpperEdge(double key, double value)
    {
        return this->apply([&](T& i) { i.upperEdge->setCoords(key, value); });
    }

    Ret setRange(const QCPRange& range)
    {
        return this->apply([&](T& o) { o.setRange(range); });
    }
};

#endif // QCP_ITEM_SPAN_H
