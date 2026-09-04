#include "test-layer-removal.h"
#include "../../../src/qcp.h"

void TestLayerRemoval::init()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
    mPlot->xAxis->setRange(0, 10);
    mPlot->yAxis->setRange(0, 10);
    mPlot->replot();
}

void TestLayerRemoval::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
}

void TestLayerRemoval::removeLayerThenReplotDoesNotCrash()
{
    mPlot->addLayer(QLatin1String("extra"));
    auto* graph = mPlot->addGraph();
    graph->setLayer(QLatin1String("extra"));
    graph->setData({0, 1, 2}, {0, 1, 0});
    mPlot->replot();
    QVERIFY(mPlot->removeLayer(mPlot->layer(QLatin1String("extra"))));
    mPlot->replot();
    mPlot->replot();
    QVERIFY(true);
}

void TestLayerRemoval::removeAxisThenReplotDoesNotCrash()
{
    auto* axis = mPlot->axisRect()->addAxis(QCPAxis::atLeft);
    mPlot->replot();
    QVERIFY(mPlot->axisRect()->removeAxis(axis));
    mPlot->replot();
    mPlot->replot();
    QVERIFY(true);
}
