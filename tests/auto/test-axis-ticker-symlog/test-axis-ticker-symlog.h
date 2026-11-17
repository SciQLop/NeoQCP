#pragma once
#include <QtTest/QtTest>

//! QCPAxisTickerSymlog: matplotlib's SymmetricalLogLocator, a linear sub grid in the band.
class TestAxisTickerSymlog : public QObject
{
    Q_OBJECT
private slots:
    void symlogTicksMatchMatplotlib_data();
    void symlogTicksMatchMatplotlib();
    void subTicksFillTheBand();
    void subTicksOutsideTheBandKeepTheMantissas();
    void aRangeInsideTheBandTakesItsEnds();
    void noDecadeInRangeFallsBackToEvenSteps();
    void aBaseOtherThanTenKeepsItsMantissas();
    void thinnedDecadesBecomeSubTicks();
    void subTicksInTheFallback();
    void extremeScalesStayBounded_data();
    void extremeScalesStayBounded();
};
