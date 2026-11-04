#ifndef QCP_ITEM_HSPAN_H
#define QCP_ITEM_HSPAN_H

#include "item-spanbase.h"

class QCPPainter;
class QCustomPlot;

template <typename T, typename Self = void>
class QCPItemHSpanOps;

class QCP_LIB_DECL QCPItemHSpan : public QCPAbstractSpanItem
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
    using OpsFor = QCPItemHSpanOps<T>;

    explicit QCPItemHSpan(QCustomPlot* parentPlot);

    QCPRange range() const;
    void setRange(const QCPRange& range);

    virtual double selectTest(const QPointF& pos, bool onlySelectable,
                              QVariant* details = nullptr) const override;

    QCPItemPosition* const lowerEdge;
    QCPItemPosition* const upperEdge;
    QCPItemAnchor* const center;

    enum HitPart { hpNone = -2, hpFill = -1, hpLowerEdge = 0, hpUpperEdge = 1 };

signals:
    void rangeChanged(const QCPRange& newRange);

protected:
    enum AnchorIndex { aiCenter };

    HitPart mDragPart = hpNone;
    double mDragStartLower = 0.0;
    double mDragStartUpper = 0.0;

    virtual void draw(QCPPainter* painter) override;
    virtual QPointF anchorPixelPosition(int anchorId) const override;
    virtual void mousePressEvent(QMouseEvent* event, const QVariant& details) override;
    virtual void mouseMoveEvent(QMouseEvent* event, const QPointF& startPos) override;
    virtual void mouseReleaseEvent(QMouseEvent* event, const QPointF& startPos) override;
};

// The fluent handle of a QCPItemHSpan; see QCPItemOps for the subclass contract.
template <typename T, typename Self>
class QCPItemHSpanOps : public QCPItemOps<T, qcp::OpsRet<Self, QCPItemHSpanOps<T>>>
{
    using Base = QCPItemOps<T, qcp::OpsRet<Self, QCPItemHSpanOps<T>>>;

public:
    using typename Base::Ret;
    using Base::Base;

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

#endif // QCP_ITEM_HSPAN_H
