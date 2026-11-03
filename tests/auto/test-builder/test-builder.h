#include <QtTest/QtTest>
#include "qcustomplot.h"

class TestBuilder : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void addGraphWithData();
    void addWithoutData();
    void handleIsTheHandle();
    void oneChainOneRepaint();
    void deadHandleSettersAreNoOps();
    void slotDeletesMidChain();
    void setAxesByRole();
    void setAxisRectAndLayerAreChecked();
    void addUserSubclass();
    void addRefusedDuringTeardown();
};
