#include <QtTest/QtTest>
#include "qcustomplot.h"

class TestRepaint : public QObject
{
    Q_OBJECT
private slots:
    void markDirtySchedulesOneRepaint();
    void dirtyAfterDrawRepaintsNextTurn();
    void markDuringDrawRepaintsNextTurn();
    void requestDuringReplotRunsNextTurn();
    void afterReplotSlotDirtyingEveryTimeDoesNotStarve();
    void hiddenPlotQueuesNoVisibleWork();
    void markDirtyWithoutPlotIsNoOp();
    void layerKeptWhileBusyIsNotRequeued();
    void syncReplotStillWorks();
    void syncReplotCancelsTheQueuedOne();
    void exportSettlesFirst_data();
    void exportSettlesFirst();
};
