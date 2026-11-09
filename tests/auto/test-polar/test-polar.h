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
    void aPolarGraphAddsToTheArea();
    void insetAndBackgroundAreAreaFeatures();
    void backgroundStaysInsideTheDisc();
};
