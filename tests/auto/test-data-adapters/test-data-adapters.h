#pragma once
#include <QtTest/QtTest>

/*!
  qcp::numbers, the hook and QCPNumberRange: any range of numbers, a user type through its ADL
  hook, read once and in place when the storage allows it.
*/
class TestDataAdapters : public QObject
{
    Q_OBJECT
private slots:
    void aVectorIsReadInPlace();
    void aHookAdaptsAUserType();
    void aDeletedHookRefusesARange();
    void aHookOnABaseIsFound();
    void aHookReturnsByValue();
    void aNonCommonRangeIsRead();
    void copiesOnce();
    void aHookedTypeReachesEveryPlotType();
    void aFilterHookIsRead();
    void aConstHookResultIsRead();
    void aByValueHookBuildsAGraph();
    void setDataMovesItsStorage();
    void mixedRangeTypesAreRead();
    void aVectorOfVectorsGivesComponents();
    void aColumnsHookAdaptsAMatrix();
    void aNumbersHookDoesNotMakeColumns();
    void aColumnsTypeIsNotFlattened();
    void columnsAreCopiedOnce();
    void columnsAreTakenOverWhenMoved();
    void columnsConceptMatchesTheRead();
};
