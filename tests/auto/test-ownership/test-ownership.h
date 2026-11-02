#include <QtTest/QtTest>
#include "qcustomplot.h"

class TestOwnership : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void deletePlottableLeavesLists();
    void deleteLaterPlottable();
    void deleteMultiGraphLeavesItsRow();
    void deletePlottableInGroup();
    void removePlottableStillDeletes();
    void nullAxisPlottableIsRefused();
    void legendItemMovedToSecondLegend();
    void plottableDeathLeavesSecondLegend();

private:
    void draw();
    QCPLegend* addSecondLegend();

    QCustomPlot* mPlot = nullptr;
};
