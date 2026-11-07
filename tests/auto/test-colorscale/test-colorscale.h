#include "qcustomplot.h"
#include <QtTest/QtTest>

class TestColorScale : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // A rect makes one scale, on first use, hidden until something uses it.
    void aRectMakesItsScaleOnFirstUse();
    void scaleIsHiddenUntilUsed();
    void mapsOnOneRectShareItsScale();

    // Where the rect puts its own scale.
    void scaleSitsOutsideTheRightAxis();
    void rectShrinksForItsScale();
    void typeTopPutsTheScaleAbove();
    void optingOutHidesTheScale();

    // The 2.x path keeps its scale in the layout and the rect's stays hidden.
    void aUserMadeScaleStillWorks();

    // A scale's internal rect is never given a scale of its own.
    void aScalesInternalRectMakesNoScale();

    // The renderer of a map made later reads the values the scale already had.
    void aSecondMapTakesTheRectsGradient();

    // A rect-owned scale is part of the plot like a layout one: margin groups and the mouse see it.
    void groupedRectsLeaveRoomForTheScale();
    void theWheelZoomsTheRectsScale();
    // The bar must not share the data layer, or a pan repaints that layer instead of shifting it.
    void theScaleIsNotOnTheDataLayer();

private:
    QCustomPlot* mPlot = nullptr;
};
