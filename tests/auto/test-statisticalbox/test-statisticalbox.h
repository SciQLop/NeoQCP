#include "qcustomplot.h"
#include <QtTest/QtTest>

class TestStatisticalBox : public QObject
{
    Q_OBJECT
private slots:
    void recordedBoxesMatchTheirReferences();
    void recordedBoxesMatchTheirReferences_data();
    void everyStyleSetterReRecords();
};
