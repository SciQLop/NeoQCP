#include <QtTest/QtTest>
#include "qcustomplot.h"

class TestRepaint : public QObject
{
    Q_OBJECT
private slots:
    void markDirtySchedulesOneRepaint();
    void dirtyAfterDrawRepaintsNextTurn();
    void markDuringDrawRepaintsNextTurn();
    void markAfterDrawSurvivesAnotherDirtyBuffer();
    void appearanceChangeIsNotHiddenByTranslation();
    void requestDuringReplotRunsNextTurn();
    void afterReplotSlotDirtyingEveryTimeDoesNotStarve();
    void hiddenPlotQueuesNoVisibleWork();
    void markDirtyWithoutPlotIsNoOp();
    void layerKeptWhileBusyIsNotRequeued();
    void syncReplotStillWorks();
    void syncReplotCancelsTheQueuedOne();
    void exportSettlesFirst_data();
    void exportSettlesFirst();
    void exportShowsReadyStagedData();
    void legendShowsAddedGraphAndItsName();
    void setLayerRepaintsTheLayerLeft();
    void axisSettersScheduleRepaint_data();
    void axisSettersScheduleRepaint();

private:
    // What the plot shows: its paint buffers composed, or the framebuffer under RHI.
    static QImage screenOf(QCustomPlot* plot);
};
