#include <QtTest/QtTest>
#include "qcustomplot.h"

class TestHandle : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void handleTestsFalseAfterDelete();
    void handleOutlivesPlot();
    void aHandleConnectsLikeTheObject();
    void connectingADeadHandleWarns();
    void aHandleConvertsToItsPointer();
    void aDerivedHandleConvertsToABaseHandle();
    void handlesHashAndCompare();
    void aHandlePrintsToQDebug();
    void ifOnAHandleIsNotAmbiguous();
    void aRawPointerDoesNotMakeAHandleImplicitly();
    void aHandleTakesPartInTheAmbiguityMatrix();
    void pointerListHelper();
    void thePlotHandsOutHandles();
    void aDeletedDefaultAxisTestsFalse();
    void aDeletedLegendTestsFalse();
    void addGraphReturnsTheSameHandleAsAdd();
    void handleAsCast();
    void handleRemove();
    void deadHandleReportsMisuse();

    void removeAxisThroughHandle();
    void removeLayerThroughHandle();
    void removeAxisRectThroughHandle();
    void noRemoveOnWidgets();
    void removeLooseLayoutElement();
    void removeLastAxisRectThenAddIsRefused();

    void axisAddedOnThePlotStacksOnItsSide();
    void axisAddedWithoutARectIsRefused();
    void colorKeepsPenWidthAndStyle();
    void aNullAxisInARangeListIsRefused();
};
