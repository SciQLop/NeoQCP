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

private:
    QCustomPlot* mPlot = nullptr;
};
