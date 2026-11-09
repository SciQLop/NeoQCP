#include "qcustomplot.h"
#include <QtTest/QtTest>

// What every axis shares: the one range model, the scale type, the selection, the link rules.
class TestAxisBase : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void everyAxisSharesTheRangeModel_data();
    void everyAxisSharesTheRangeModel();

    void linkingAPolarAxisIsRefused();
    void cartesianAxisApiStillCompiles();
    void axisPropertiesStillNotify();
};
