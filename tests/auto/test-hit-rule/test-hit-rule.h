#pragma once
#include <QtTest/QtTest>

class QCustomPlot;

class TestHitRule : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void wheelNeedsDistanceToo();
    void pressAndSelectionAgreeWhenSelectable();
    void nonSelectableLabelTakesPressGraphTakesSelection();
    void axisRectWheelZoomStillWins();
    void topmostWithinToleranceWins();
    void spanDragStillBeatsRangeDrag();

private:
    QPointF pixelOf(double key, double value) const;
    void sendMouse(QEvent::Type type, const QPoint& pos, Qt::MouseButton button);
    void wheelAt(QPointF pos);

    QCustomPlot* mPlot = nullptr;
};
