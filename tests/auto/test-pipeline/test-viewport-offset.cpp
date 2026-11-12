#include "test-pipeline.h"
#include <qcustomplot.h>
#include <painting/viewport-offset.h>

void TestPipeline::viewportOffsetLinearHorizontal()
{
    QCPRange oldKey(0, 100);
    QCPRange newKey(10, 110);
    QCPRange oldVal(0, 50);
    QCPRange newVal(0, 50);

    mPlot->xAxis->setRange(newKey);
    mPlot->yAxis->setRange(newVal);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    auto offset = qcp::computeViewportOffset(
        mPlot->xAxis, mPlot->yAxis, oldKey, oldVal);

    double expectedDx = mPlot->xAxis->coordToPixel(oldKey.lower)
                      - mPlot->xAxis->coordToPixel(newKey.lower);
    QCOMPARE(offset.x(), expectedDx);
    QCOMPARE(offset.y(), 0.0);
}

void TestPipeline::viewportOffsetLinearVertical()
{
    QCPAxisRect* axisRect = mPlot->axisRect();
    auto* keyAxis = axisRect->axis(QCPAxis::atLeft);
    auto* valueAxis = axisRect->axis(QCPAxis::atBottom);

    QCPRange oldKey(0, 100);
    QCPRange newKey(10, 110);
    keyAxis->setRange(newKey);
    valueAxis->setRange(0, 50);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    auto offset = qcp::computeViewportOffset(
        keyAxis, valueAxis, oldKey, QCPRange(0, 50));

    double expectedDy = keyAxis->coordToPixel(oldKey.lower)
                      - keyAxis->coordToPixel(newKey.lower);
    QCOMPARE(offset.y(), expectedDy);
}

void TestPipeline::viewportOffsetLogScale()
{
    mPlot->xAxis->setScaleType(QCPAxis::stLogarithmic);
    QCPRange oldKey(1, 1000);
    QCPRange newKey(10, 10000);
    mPlot->xAxis->setRange(newKey);
    mPlot->yAxis->setRange(0, 50);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    auto offset = qcp::computeViewportOffset(
        mPlot->xAxis, mPlot->yAxis, oldKey, QCPRange(0, 50));

    double expectedDx = mPlot->xAxis->coordToPixel(oldKey.lower)
                      - mPlot->xAxis->coordToPixel(newKey.lower);
    QVERIFY(qAbs(offset.x() - expectedDx) < 0.01);
    QVERIFY(qAbs(offset.x()) > 1.0);
}

void TestPipeline::viewportOffsetNoChange()
{
    QCPRange key(0, 100);
    QCPRange val(0, 50);
    mPlot->xAxis->setRange(key);
    mPlot->yAxis->setRange(val);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    auto offset = qcp::computeViewportOffset(
        mPlot->xAxis, mPlot->yAxis, key, val);
    QCOMPARE(offset.x(), 0.0);
    QCOMPARE(offset.y(), 0.0);
}
