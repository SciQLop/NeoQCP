#ifndef QCP_ITEM_RSPAN_H
#define QCP_ITEM_RSPAN_H

#include "item-spanbase.h"

class QCPPainter;
class QCustomPlot;

template <typename T, typename Self = void>
class QCPItemRSpanOps;

class QCP_LIB_DECL QCPItemRSpan : public QCPAbstractSpanItem
{
    Q_OBJECT
    Q_PROPERTY(QPen pen READ pen WRITE setPen)
    Q_PROPERTY(QPen selectedPen READ selectedPen WRITE setSelectedPen)
    Q_PROPERTY(QBrush brush READ brush WRITE setBrush)
    Q_PROPERTY(QBrush selectedBrush READ selectedBrush WRITE setSelectedBrush)
    Q_PROPERTY(QPen borderPen READ borderPen WRITE setBorderPen)
    Q_PROPERTY(QPen selectedBorderPen READ selectedBorderPen WRITE setSelectedBorderPen)
    Q_PROPERTY(bool movable READ movable WRITE setMovable)

public:
    template <typename T>
    using OpsFor = QCPItemRSpanOps<T>;

    explicit QCPItemRSpan(QCustomPlot* parentPlot);

    QCPRange keyRange() const;
    QCPRange valueRange() const;
    void setKeyRange(const QCPRange& range);
    void setValueRange(const QCPRange& range);

    virtual double selectTest(const QPointF& pos, bool onlySelectable,
                              QVariant* details = nullptr) const override;

    QCPItemPosition* const leftEdge;
    QCPItemPosition* const rightEdge;
    QCPItemPosition* const topEdge;
    QCPItemPosition* const bottomEdge;

    QCPItemAnchor* const topLeft;
    QCPItemAnchor* const topRight;
    QCPItemAnchor* const bottomRight;
    QCPItemAnchor* const bottomLeft;
    QCPItemAnchor* const center;

    enum HitPart { hpNone = -2, hpFill = -1, hpLeft = 0, hpRight = 1, hpTop = 2, hpBottom = 3 };

signals:
    void keyRangeChanged(const QCPRange& newRange);
    void valueRangeChanged(const QCPRange& newRange);

protected:
    enum AnchorIndex { aiTopLeft, aiTopRight, aiBottomRight, aiBottomLeft, aiCenter };

    HitPart mDragPart = hpNone;
    double mDragStartLeft = 0, mDragStartRight = 0;
    double mDragStartTop = 0, mDragStartBottom = 0;

    virtual void draw(QCPPainter* painter) override;
    virtual QPointF anchorPixelPosition(int anchorId) const override;
    virtual void mousePressEvent(QMouseEvent* event, const QVariant& details) override;
    virtual void mouseMoveEvent(QMouseEvent* event, const QPointF& startPos) override;
    virtual void mouseReleaseEvent(QMouseEvent* event, const QPointF& startPos) override;

private:
    void emitDraggedRanges();
};

// The fluent handle of a QCPItemRSpan; see QCPItemOps for the subclass contract.
template <typename T, typename Self>
class QCPItemRSpanOps : public QCPItemOps<T, qcp::OpsRet<Self, QCPItemRSpanOps<T>>>
{
    using Base = QCPItemOps<T, qcp::OpsRet<Self, QCPItemRSpanOps<T>>>;

public:
    using typename Base::Ret;
    using Base::Base;

    Ret setLeftEdge(double key, double value)
    {
        return this->apply([&](T& i) { i.leftEdge->setCoords(key, value); });
    }

    Ret setRightEdge(double key, double value)
    {
        return this->apply([&](T& i) { i.rightEdge->setCoords(key, value); });
    }

    Ret setTopEdge(double key, double value)
    {
        return this->apply([&](T& i) { i.topEdge->setCoords(key, value); });
    }

    Ret setBottomEdge(double key, double value)
    {
        return this->apply([&](T& i) { i.bottomEdge->setCoords(key, value); });
    }

    Ret setKeyRange(const QCPRange& range)
    {
        return this->apply([&](T& o) { o.setKeyRange(range); });
    }

    Ret setValueRange(const QCPRange& range)
    {
        return this->apply([&](T& o) { o.setValueRange(range); });
    }
};

#endif // QCP_ITEM_RSPAN_H
