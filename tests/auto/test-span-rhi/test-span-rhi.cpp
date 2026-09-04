#include "test-span-rhi.h"
#include "../../../src/qcp.h"
#include "../../../src/painting/span-rhi-layer.h"
#include "../../../src/items/item-vspan.h"

void TestSpanRhi::init()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
    mPlot->xAxis->setRange(0, 10);
    mPlot->yAxis->setRange(0, 10);
    mSpan = new QCPItemVSpan(mPlot);
    mSpan->lowerEdge->setCoords(2, 0);
    mSpan->upperEdge->setCoords(4, 0);
    // Shown so that resize() delivers a resizeEvent (hidden widgets defer it),
    // which is what propagates the new viewport into the axis rect layout.
    mPlot->show();
    mPlot->replot();
}

void TestSpanRhi::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
    mSpan = nullptr;
}

void TestSpanRhi::firstDetectionReportsChange()
{
    QCPSpanRhiLayer layer(nullptr);
    layer.registerSpan(mSpan);
    QVERIFY(layer.detectGeometryChanges());
}

void TestSpanRhi::unchangedFrameReportsNoChange()
{
    QCPSpanRhiLayer layer(nullptr);
    layer.registerSpan(mSpan);
    QVERIFY(layer.detectGeometryChanges());
    QVERIFY(!layer.detectGeometryChanges());
}

void TestSpanRhi::detectsEdgeMove()
{
    QCPSpanRhiLayer layer(nullptr);
    layer.registerSpan(mSpan);
    QVERIFY(layer.detectGeometryChanges());
    mSpan->upperEdge->setCoords(6, 0);
    QVERIFY(layer.detectGeometryChanges());
}

void TestSpanRhi::detectsBrushChange()
{
    QCPSpanRhiLayer layer(nullptr);
    layer.registerSpan(mSpan);
    QVERIFY(layer.detectGeometryChanges());
    mSpan->setBrush(QBrush(Qt::red));
    QVERIFY(layer.detectGeometryChanges());
}

void TestSpanRhi::detectsAxisRangeChange()
{
    QCPSpanRhiLayer layer(nullptr);
    layer.registerSpan(mSpan);
    QVERIFY(layer.detectGeometryChanges());
    mPlot->xAxis->setRange(5, 15);
    QVERIFY(layer.detectGeometryChanges());
}

void TestSpanRhi::detectsAxisRectResize()
{
    QCPSpanRhiLayer layer(nullptr);
    layer.registerSpan(mSpan);
    QVERIFY(layer.detectGeometryChanges());
    mPlot->resize(500, 400);
    mPlot->replot();
    QVERIFY(layer.detectGeometryChanges());
}
