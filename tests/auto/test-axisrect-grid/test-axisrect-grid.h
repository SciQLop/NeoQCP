#ifndef TEST_AXISRECT_GRID_H
#define TEST_AXISRECT_GRID_H

#include <QtTest/QtTest>

class QCustomPlot;

class TestAxisRectGrid : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void aGridKeepsTheFirstRect();
    void stretchFactorsApply();
    void aSecondCallGrowsAndShrinks();
    void aSlotDeletingDroppedContentsIsSafe();
    void aDeferredSlotThatDeletesThePlotIsSafe();
    void aDeferredSlotReenteringIsDeferred();
    void aHoleDoesNotMoveKeptRects();
    void columnsAndRowsAlign();
    void misuseIsRefused();
    void aLegendTheUserTookOutStaysOut();
    void aDeletedPlotLayoutIsBuiltAgain();
    void aDestroyedSlotMayDeleteTheDroppedRect();
    void aReentrantCallIsRefused();
    void aDestroyedSlotMayDeleteThePlot();

private:
    void renderOnce(QSize size);
    void settle();

    QCustomPlot* mPlot = nullptr;
};

#endif // TEST_AXISRECT_GRID_H
