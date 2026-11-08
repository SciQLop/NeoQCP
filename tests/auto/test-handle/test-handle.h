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
    void handleConnects();
    void handleIsNotARawPointer();
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
};
