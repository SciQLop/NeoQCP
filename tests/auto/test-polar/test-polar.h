#include "qcustomplot.h"
#include <QtTest/QtTest>

class QCPPolarArea;

class TestPolar : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void areaOwnsItsAxes();
    void areaLaysOutCentreAndRadius();
    void deletingTheAreaMidDragIsSafe();
    void aPolarGraphIsAPlottable();
    void aPolarGraphRecords();
    void aRotationDoesNotReRecord();
    void aNonPeriodicGraphKeepsItsWindow();
    void aRimCrossingIsOnTheChord_data();
    void aRimCrossingIsOnTheChord();
    void aValueTheRadialScaleCannotTakeIsAGap();
    void aGraphFirstRecordedAtZeroSizeAppearsAfterAResize();
    void aClampedRunFollowsTheArc_data();
    void aClampedRunFollowsTheArc();
    void aSparseInRangePairIsAChord();
    void aNaNBreaksTheLine();
    void aScatterOnlyGraphRecordsNoLine();
    void aFilledArcClosesWithAChord();
    void deletingTheRadialAxisLeavesTheGraphInert();
    void deletingTheGraphFromADragSlot();
    void deletingTheAreaDoesNotDeleteTheGraph();
    void insetAndBackgroundAreAreaFeatures();
    void backgroundStaysInsideTheDisc();
    void polarAxisPartsAreClickable();
    void radialDragAndWheelWork();
    void angularWheelZoomRequeuesTheRepaint();
    void theAngularAxisRefusesALogScale();
    void theAreaKeepsTheOldMinimumMargins();
    void hidingTheAngularAxisKeepsTheGraphs();
    void deletingAPolarChildIsSafe();
    void deletingTheAngularAxisMidDragIsSafe();
};
