#include <QtTest/QtTest>
#include "qcustomplot.h"

class TestSignalDelivery : public QObject
{
    Q_OBJECT
private slots:
    void nestedScopeDeliversAtOutermost();
    void deadSenderSkipped();
    void slotDeletesPlotDuringReplot_data();
    void slotDeletesPlotDuringReplot();
    void replotSignalsSeeConsistentPlot();
    void slotDeletesAxisDuringRescale();
    void removeLayerWhileIntervalsAddsLayer();

    void deletePlotFromBeforeReplotWithHandle_data();
    void deletePlotFromBeforeReplotWithHandle();
    void deletedChildSkipsQueuedEmission();

    void beforeReplotChangesShowInSameReplot();
    void afterLayoutSeesExportViewport();
    void deleteLayerWhoseChildSlotDeletesPlot();
    void deleteAxisRectWhoseSlotDeletesPlot();
    void slotDeletesPlotDuringExport_data();
    void slotDeletesPlotDuringExport();
    void deleteInsideOwnEventReportsMisuse();
    void replotFromReplotSignalIsIgnored_data();
    void replotFromReplotSignalIsIgnored();
    void pendingSignalsDieWithThePlot();
    void slotDeletesPlotOnClick();
    void slotsRunBeforeEventReturns();
    void wheelIgnoredInSlotReachesParent();
    void nestedEventDeliversWhenItReturns();
    void replotSignalOfAWidgetEventRunsInsideIt_data();
    void replotSignalOfAWidgetEventRunsInsideIt();
    void wheelSlotIgnoringAndDeletingThePlotIsSafe();
    void releaseSlotIgnoringAndDeletingThePlotIsSafe();
    void nestedEventKeepsOuterEntriesQueued();
    void deleteLaterDeliversTeardownSignalsLikeDelete();

    void qcpEmitHoldsUntilTheScopeCloses();
    void noPlotEmitsAtOnce();
    void deadSenderIsNotDelivered();
};
