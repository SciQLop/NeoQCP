#pragma once
#include "layer.h"
#include <QPointer>

class QCPItem;
class QCPAxis;
class QCPAxisRect;
class QCPPainter;

class QCPItemCreationState : public QCPLayerable {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(QCPItemCreationState)
public:
    enum State { Idle, Drawing };
    Q_ENUM(State)

    explicit QCPItemCreationState(QCustomPlot* plot);

    State state() const { return mState; }

    bool handleMousePress(QMouseEvent* event);
    bool handleMouseMove(QMouseEvent* event);
    bool handleKeyPress(QKeyEvent* event);

    void cancel();

signals:
    void itemCreated(QCPItem* item);
    void itemCanceled();

protected:
    void applyDefaultAntialiasingHint(QCPPainter* painter) const override;
    void draw(QCPPainter* painter) override;

private:
    QCustomPlot* mPlot;
    State mState = Idle;
    QPointer<QCPItem> mCurrentItem;
    QCPAxis* mKeyAxis = nullptr;
    QCPAxis* mValueAxis = nullptr;
    double mAnchorKey = 0;
    double mAnchorValue = 0;

    void applyPositioner(double key, double value);
    void commitItem();
    void cancelItem();
    void updateItemPosition(const QPointF& pixelPos);
    void drawBadge(QCPPainter* painter, const QRect& axisRectArea);
    static void rebindPositions(QCPItem* item, QCPAxis* keyAxis,
                                QCPAxis* valueAxis, QCPAxisRect* axisRect);
    QCPAxisRect* axisRectAt(const QPointF& pos) const;
};
