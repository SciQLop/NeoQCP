#pragma once
#include <QtTest/QtTest>

class QCustomPlot;

class TestItemPosition : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void axisRectAbsoluteIsOffsetByAxisRectOrigin();
    void axisRectAbsoluteFollowsAxisRectOnResize();
    void axisRectAbsoluteSetPixelPositionRoundTrips();
    void axisRectAbsoluteRespectsParentAnchor();
    void absoluteRemainsWidgetRelative();
    void farOffscreenItemsDoNotOverflow_data();
    void farOffscreenItemsDoNotOverflow();

private:
    QCustomPlot* mPlot = nullptr;
};
