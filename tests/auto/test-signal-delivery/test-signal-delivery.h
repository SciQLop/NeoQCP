#include <QtTest/QtTest>
#include "qcustomplot.h"

class TestSignalDelivery : public QObject
{
    Q_OBJECT
private slots:
    void nestedScopeDeliversAtOutermost();
    void deadSenderSkipped();
    void inputScopeDeliversNextTurn();
    void pendingInputSignalsDieWithThePlot();
    void slotDeletesPlotDuringReplot_data();
    void slotDeletesPlotDuringReplot();
    void replotSignalsSeeConsistentPlot();
    void slotDeletesAxisDuringRescale();
    void slotDeletesPlotOnClick();
    void removeLayerWhileIntervalsAddsLayer();

    void deletePlotFromBeforeReplotWithHandle_data();
    void deletePlotFromBeforeReplotWithHandle();
    void deletedChildSkipsQueuedEmission();
    void removeInsideInputScope();

    void beforeReplotChangesShowInSameReplot();
    void afterLayoutSeesExportViewport();
    void addGraphBeforeInputFlush();
    void deleteLayerWhoseChildSlotDeletesPlot();
    void deleteAxisRectWhoseSlotDeletesPlot();
    void replotBeforeInputFlushKeepsClickQueued_data();
    void replotBeforeInputFlushKeepsClickQueued();
    void slotDeletesPlotDuringExport_data();
    void slotDeletesPlotDuringExport();
};
