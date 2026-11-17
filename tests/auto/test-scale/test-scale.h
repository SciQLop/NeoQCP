#pragma once
#include <QtTest/QtTest>

/*!
  QCPScale: linear, log and symlog as one value, symlog being matplotlib's SymmetricalLogTransform.
*/
class TestScale : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void cleanupTestCase();

    void linearIsIdentity();
    void logIsLogBase();
    void symlogMatchesMatplotlib();
    void roundTrips();
    void badParametersAreRefused();
    void hashMatchesEquality();

    void anAxisTakesAScale();
    void setScaleBringsItsTicker();
    void aLogAxisMapsAsBefore();
    void symlogMapsBothSides();
    void dragAndZoomWorkInScaleSpace();
    void aRadialAxisTakesSymlog();
    void aLogColourScaleMapsAsBefore();
    void aDataScaleSetBeforeTheAddWins();
    void colorizeOverloadsAgreeOnALogScale();
    void aSymlogColourScaleIsSymmetric();
};
