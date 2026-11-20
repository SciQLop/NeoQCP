#ifndef TEST_CONNECTOR_H
#define TEST_CONNECTOR_H

#include <QtTest/QtTest>

class TestConnector : public QObject
{
    Q_OBJECT
private slots:
    void connectorPathsMatchMatplotlib_data();
    void connectorPathsMatchMatplotlib();
    void arcZeroIsStraight();
    void parallelDirectionsGiveAStraightPath();
    void aZeroLengthConnectorIsEmpty();
    void aNonFiniteConnectorIsEmpty();
    void connectorsCompareEqual();
};

#endif // TEST_CONNECTOR_H
