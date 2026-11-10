#include "qcustomplot.h"
#include <QtTest/QtTest>

// The coordinate systems an area keeps: one per axis pair, created on demand and owned by the area.
class TestCoordinateSystem : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void onePairOneSystem();
    void aPairMustBelongToTheArea();
    void cartesianTransformIsTheAxes();
    void polarSystemMapsLikeTheArea();
    void polarSystemRefusesAxisRatios();
    void revisionBumpsOnAnyMappingChange();
    void deletingAnAxisDeletesItsSystems();
    void aPlottableTakesASystem();
    void twoConstructorsShareTheSystem();
    void movingBetweenSystemsReRecords();
    void aPlottableOfADeletedSystemIsInert();
    void crossPlotSystemIsRefused();
    void barsAreRefusedInAPolarSystem_data();
    void barsAreRefusedInAPolarSystem();
    void aRecordingPlottableWithNoNeedsJoinsPolar();
    void relationshipsAcrossAreasAreRefused();
    void aBracketDecorationNeedsAKeyAxis();
    void rescaleAxesVisitsEveryArea();
    void axisPlottablesKeepTheirOrder();
};
