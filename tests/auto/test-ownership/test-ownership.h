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
    void deleteMultiGraphLeavesSecondLegend();
    void deleteIntervalsLeavesItsRows();
    void detachedRowsOutliveTheirPlottables();
    void deletePlottableInGroup();
    void removePlottableStillDeletes();
    void nullAxisPlottableIsDetachedHalfAxedIsRefused();
    void legendItemMovedToSecondLegend();
    void plottableDeathLeavesSecondLegend();

    void deletingAPlottableFromAnotherDrawKeepsTheDeferredPainterSafe();

    void deleteItemLeavesList();
    void deleteLaterItem();
    void removeItemFromDestroyedSlot();
    void clearItemsWhileASlotDeletesAnother();
    void clearItemsKeepsOtherLayersOrder();
    void clearItemsSlotDeletesAnother();
    void itemOrderAfterRemove();
    void layerOrderAfterMoves();
    void deleteParentAnchorItem();

    void deleteAxisLeavesRectAndShortcuts();
    void plottablesOfAnotherRectAfterAxisRemoval();
    void deleteGridTwiceSafe();
    void deletedGridStaysDeleted();
    void deletedGridStaysDeletedOnGpu();
    void deleteLayerLeavesLists();
    void deleteCurrentLayer();
    void deleteLastLayer();
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
    void deleteAngularAxisWithGraphs();
    void movePolarGraphKeepsOneLegendRow();

    void destroyedSlotAddsDuringTeardown();
    void destroyedSlotConstructsDuringTeardown();
    void destroyedSlotDeletesSiblingDuringTeardown();
    void childrenDeletedWhilePlotWhole();
    void mutatorsRefuseDuringTeardown();
    void recreatingGettersReturnNullDuringTeardown();
    void shownPlotTeardownFreesGpuResources();
    void newAxisRectDuringTeardown();
    void newColorScaleDuringTeardown();

private:
    void draw();
    QCPLegend* addSecondLegend();
    QCPGraph* addRedGraph();
    bool drawsRed();
    void showAndExpose();

    QCustomPlot* mPlot = nullptr;
};
