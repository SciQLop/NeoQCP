#pragma once
#include <QtTest/QtTest>

class QCustomPlot;

class TestMultiGraph : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    // Data source wiring
    void creation();
    void setDataSourceShared();
    void setDataSourceUnique();
    void setDataConvenience();
    void viewDataConvenience();
    void componentCountMatchesDataSource();
    void componentValueAt();
    void dataCountDelegate();
    void dataMainKeyDelegate();
    void dataMainValueColumn0();
    void findBeginEndDelegate();
    void keyRangeDelegate();
    void valueRangeUnion();

    // Selection
    void selectTestFindsClosestComponent();
    void selectTestReturnsComponentInDetails();
    void selectTestRectDoesNotMutateState();
    void selectTestRectPerComponent();
    void selectEventSingleComponent();
    void selectEventRectSelection();
    void selectEventAdditive();
    void deselectEventClearsAll();
    void componentSelectionUpdatesBase();

    // Legend
    void addToLegendCreatesGroupItem();
    void removeFromLegendWorks();
    void legendExpandCollapse();
    void legendGroupSelectsAll();
    void legendHeaderDoesNotLeakClassNameWhenEmpty();
    void legendHeaderShowsNameWhileEmpty();

    // Line cache
    void cacheExtendsBeyondVisibleRange();

    // Rendering
    void renderVerticalKeyAxisUsesHeight();
    void renderBasicDoesNotCrash();
    void renderWithSelection();
    void renderHiddenComponent();
    void renderEmptySource();
    void renderAllLineStyles();

private:
    QCustomPlot* mPlot = nullptr;
};
