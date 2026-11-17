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
    // A map moved to another rect's axes follows that rect's scale, unless the user chose one.
    void aMovedMapFollowsItsNewRectsScale();
    void aScaleSlotDeletingTheMovedMapIsSafe();
    // A map moved to another plot keeps its opt-out.
    void anOptOutSurvivesATakeAndAdd();

    // The 2.x path keeps its scale in the layout and the rect's stays hidden.
    void aUserMadeScaleStillWorks();

    // A scale's internal rect is never given a scale of its own.
    void aScalesInternalRectMakesNoScale();

    // The renderer of a map made later reads the values the scale already had.
    void aSecondMapTakesTheRectsGradient();
    // The bar's axis takes the data scale, so its pixel for z = 1 is the lookup of z = 1.
    void theColourBarFollowsTheDataScale();
    void setTypeCarriesTheDataScale();
    // A scale change on the bar installs its ticker once: the colour scale's follow-up adds none.
    void aBarScaleChangeInstallsItsTickerOnce();
    // A constant map on a symlog data scale keeps its width in the scale's own space.
    void aConstantMapRescalesWithTheLinearRule();

    // A rect-owned scale is part of the plot like a layout one: alignment groups and the mouse see it.
    void groupedRectsLeaveRoomForTheScale();
    void theWheelZoomsTheRectsScale();
    // Opposite axes of the scale's inner rect are synchronized: one step must zoom once.
    void theWheelZoomsTheScaleOnce();
    // The bar must not share the data layer, or a pan repaints that layer instead of shifting it.
    void theScaleIsNotOnTheDataLayer();

private:
    QCustomPlot* mPlot = nullptr;
};
