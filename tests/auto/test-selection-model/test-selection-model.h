#include "qcustomplot.h"
#include <QtTest/QtTest>

class TestSelectionModel : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void everySelectableHasOneShape_data();
    void everySelectableHasOneShape();
    void selectableChangeIsOneSignal_data();
    void selectableChangeIsOneSignal();
    void itemRefusesAPartialSelectionType();
    void userSelectionStillAnnounced();

private:
    QCustomPlot* mPlot = nullptr;
};
