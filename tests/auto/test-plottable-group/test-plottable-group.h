#include <QtTest/QtTest>
#include "qcustomplot.h"

class TestPlottableGroup : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void groupHasOneLegendRow();
    void groupedPlottableCannotRejoinTheLegend();
    void doubleClickOnTheRowIsReportedOnce();
    void visibleWhenAnyMemberIsVisible();
    void removedPlottableLeavesTheGroup();
    void emptyGroupStaysWithoutARow();
    void removingTheGroupGivesRowsBack();
    void unwantedLegendStaysOutAfterTheGroup();
    void multiGraphUnwantedLegendStaysOut();
    void memberMovesToItsNewGroup();
    void memberFromAnotherPlotIsRefused();
    void hiddenGroupStaysDimmedAfterTextColorChange();
    void rowRepaintsWhenMembersHide();
    void busyMemberShowsTheBusySymbol();
    void deletingThePlotWithGroupsIsSafe();
    void membersChangedSlotMayRemoveAgain();

private:
    QCPPlottableGroupLegendItem* groupRow() const;
    int plottableRows() const;
    QCustomPlot* mPlot;
};
