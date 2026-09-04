#include "test-hidden-replot.h"
#include "../../../src/qcp.h"

void TestHiddenReplot::init()
{
    mPlot = new QCustomPlot(); // never shown: isVisible() == false
    mPlot->resize(400, 300);
    mPlot->xAxis->setRange(0, 10);
    mPlot->yAxis->setRange(0, 10);
}

void TestHiddenReplot::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
}

void TestHiddenReplot::hiddenReplotRunsByDefault()
{
    QSignalSpy spy(mPlot, &QCustomPlot::afterReplot);
    mPlot->replot();
    QCOMPARE(spy.count(), 1);
}

void TestHiddenReplot::hiddenReplotSkippedWhenEnabled()
{
    QSignalSpy beforeSpy(mPlot, &QCustomPlot::beforeReplot);
    QSignalSpy afterSpy(mPlot, &QCustomPlot::afterReplot);
    mPlot->setSkipReplotsWhenHidden(true);
    mPlot->replot();
    QCOMPARE(beforeSpy.count(), 0);
    QCOMPARE(afterSpy.count(), 0);
}

void TestHiddenReplot::replotResumesAfterDisable()
{
    mPlot->setSkipReplotsWhenHidden(true);
    mPlot->replot();
    mPlot->setSkipReplotsWhenHidden(false);
    QSignalSpy spy(mPlot, &QCustomPlot::afterReplot);
    mPlot->replot();
    QCOMPARE(spy.count(), 1);
}
