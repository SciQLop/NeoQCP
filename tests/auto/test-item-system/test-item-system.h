#pragma once
#include <QtTest/QtTest>

class QCustomPlot;

class TestItemSystem : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void aPositionHasTheSystemOfItsAxes();
    void aOneAxisPositionKeepsTwoXPixels();
    void aSystemMisuseKeepsTheOldSystem();
    void aDeletedAxisLeavesThePositionInert();
    void takeClearsAndAdoptRebindsItemSystems();
    void theTracerFollowsItsGraphSystem();
    void itemClipAreaIsTheCartesianClip();
    void aCreatedItemBindsTheNewRectsSystem();

    void aTextFollowsAPolarDataPoint();
    void aPolarRatioIsAngleAndRadiusFractions();
    void aPolarItemClipsToTheDisc();
    void anUnclippedPolarItemIsNotClipped();
    void aPolarItemIsHittable();
    void deletingThePolarAreaLeavesItsItemsInert();
    void takeAndAdoptAPolarItem();

    void aPolarSetTypeKeepsThePixel();
    void aPolarAreaAbsolutePositionIsFromTheAreaCorner();
    void aCartesianSystemWithAPolarAreaPlacesRatiosByPixel();
    void aSetAxesAfterAPolarSystemTakesTheRectsArea();
    void aBindingAPositionToASystemMovesTheClipArea();
    void setAxesNullUnbindsAPolarSystem();
    void setSystemTwiceRetakesTheArea();
    void aTakeClearsAOneAxisPosition();
    void theProjectionsFollowEveryFrame();
private:
    QCustomPlot* mPlot = nullptr;
};
