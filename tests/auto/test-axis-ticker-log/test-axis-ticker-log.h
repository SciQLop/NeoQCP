#pragma once
#include <QtTest/QtTest>

class TestAxisTickerLog : public QObject
{
    Q_OBJECT
private slots:
    void majorTicks_data();
    void majorTicks();
    void shortRangeSubTicksFillTheMissingDigits();
    void tinyRangeFallsBackToEvenSteps();
};
