#include <QtTest/QtTest>
#include "qcustomplot.h"

class TestDataLocator : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void locateOnGraph();
    void locateOnGraph2();
    void locateOnCurve();
    void locateOnColorMap();
    void locateOnEmptyPlottable();
    void locateWithNullPlottable();
    void locateOnMultiGraphSelectsClosestComponent();
    void locateAtKeyOnAMultiGraphWithoutColumns();

private:
    QCustomPlot* mPlot;
};
