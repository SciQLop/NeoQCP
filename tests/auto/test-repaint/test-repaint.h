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
    void axisUnlistedHandsOffsetToSibling();
    void layoutSettersIgnoreEqualValues();
    void graphTracerSettles();
    void remainingEqualValueDoesNotDirty_data();
    void remainingEqualValueDoesNotDirty();
    void remainingSettersScheduleRepaint_data();
    void remainingSettersScheduleRepaint();
    void secondLegendRowFollowsRename();
    void orphanPlottableKeepsName();
    void axisRectMarginsMoveTheContent();
    void polarPlotSettles();
    void exportSettlesPendingTheme();
    void plottableSettersScheduleRepaint_data();
    void plottableSettersScheduleRepaint();
    void plottableEqualValueDoesNotDirty_data();
    void plottableEqualValueDoesNotDirty();
    void barsGroupSpacingRepaints();
    void plottableGroupVisibilityRepaints();
    void axisMarginChangeDuringPanRepaints();
    void multiGraphLineStyleResetsComponents();
    void equalValueDoesNotDirty_data();
    void equalValueDoesNotDirty();

private:
    // What the plot shows: its paint buffers composed, or the framebuffer under RHI.
    static QImage screenOf(QCustomPlot* plot);
};
