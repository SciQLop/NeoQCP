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

    void deleteItemLeavesList();
    void deleteLaterItem();
    void removeItemFromDestroyedSlot();
    void clearItemsWhileASlotDeletesAnother();
    void deleteParentAnchorItem();
    void spanDestructorCreatesNoGpuLayer();

    void deleteAxisLeavesRectAndShortcuts();
    void plottablesOfAnotherRectAfterAxisRemoval();
    void deleteGridTwiceSafe();
    void deleteLayerLeavesLists();
    void deleteCurrentLayer();
    void deleteRootLayoutAndOverlay();
    void deleteOwnedTheme();
    void removeAxisHidesItsPlottables();

    void deleteTracedGraph();
    void deleteColorScaleOfMap();
    void deleteBarsInGroup();
    void polarRemoveGraphUnlistsFirst();
    void intervalsWithoutLaneLayout();
    void deletePolarGraphInLegend();
    void intervalsWithoutAxes();

private:
    void draw();
    QCPLegend* addSecondLegend();
    QCPGraph* addRedGraph();
    bool drawsRed();
    void showAndExpose();

    QCustomPlot* mPlot = nullptr;
};
