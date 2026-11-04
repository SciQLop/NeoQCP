#include <QtTest/QtTest>
#include "qcustomplot.h"

class TestBuilder : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void addGraphWithData();
    void addWithoutData();
    void handleIsTheHandle();
    void oneChainOneRepaint();
    void deadHandleSettersAreNoOps();
    void slotDeletesMidChain();
    void setAxesByRole();
    void setAxisRectAndLayerAreChecked();
    void addUserSubclass();
    void addRefusedDuringTeardown();
    void addRefusedWithoutAxisRect();
    void addPutsOneLegendRow();
    void addGraphWithContainer();
    void withReachesAnySetter();
    void setterSlotDeletesInsideTheSetter();

    void addEveryPlottable_data();
    void addEveryPlottable();
    void addEveryItem_data();
    void addEveryItem();
    void addAxisToRect();
    void addLayoutParts();
    void addToTakenCellIsRefused();
    void groupsOwnedByPlot();
    void groupHandlesSetAndAddMembers();
    void barsHandleGroupsAndStacks();
    void takeAndReAdd();
    void takeAndReAddItem();
    void takeColorMapLeavesScale();
    void takeBarsLeavesGroupAndStack();
    void takeDeadHandle();
    void takeForeignObject();
    void takeDuringTeardown();
    void addUserBuilt();
    void addRefusesObjectOfAPlot();
    void axesOfTwoPlotsAreRefused();
    void tracerGraphIsClearedOnTake();
    void takenPointerDeletedIsClean();
    void addIntervalsWithBadColumnsLeavesThePlotUnchanged();
    void takeCutsAnchorsBothWays();
    void takeCutsOutboundAnchorLinks();
    void pixelPositionOfATakenItemIsNull();
    void addPlottableDuringTeardownIsRefused();
    void itemsWithoutAxisRectAreRefusedLikePlottables();
    void layoutCellsFarBeyondTheGridAreRefused();
    void takenPipelinePlottablesOutliveTheirPlot();
    void errorBarsTakeTheirPlottableByHandle();
    void tracerTakesItsGraphByHandle();
    void richTextHandleKeepsTheTextSetters();
    void graph2ScatterColoursChangeTheDrawing();
    void addPolar();
    void takePolarGraph();
    void addPolarGraphWithoutRadialAxisIsRefused();
    void deletedPolarGraphLeavesNoLegendRow();
    void takePolarGraphRefusals();
    void takenPolarGraphOutlivesItsPlot();
    void orphanedPolarGraphCanBeTaken();
    void takeGraphClearsTracers();
};
