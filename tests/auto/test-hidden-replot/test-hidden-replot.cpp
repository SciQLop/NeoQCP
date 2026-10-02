#include "test-hidden-replot.h"
#include "../../../src/qcp.h"
#include <QtWidgets/qtestsupport_widgets.h> // QTest::qWaitForWindowExposed

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

void TestHiddenReplot::neverShownReplotRunsWhenEnabled()
{
    // Regression guard: never-shown widgets (offscreen rendering, tests) keep
    // classic replot behavior even with the flag enabled.
    QSignalSpy beforeSpy(mPlot, &QCustomPlot::beforeReplot);
    QSignalSpy afterSpy(mPlot, &QCustomPlot::afterReplot);
    mPlot->setSkipReplotsWhenHidden(true);
    mPlot->replot();
    QCOMPARE(beforeSpy.count(), 1);
    QCOMPARE(afterSpy.count(), 1);
}

void TestHiddenReplot::hiddenReplotSkippedWhenEnabled()
{
    mPlot->show(); // was-shown, then hidden again: the guard applies
    mPlot->hide();
    QSignalSpy beforeSpy(mPlot, &QCustomPlot::beforeReplot);
    QSignalSpy afterSpy(mPlot, &QCustomPlot::afterReplot);
    mPlot->setSkipReplotsWhenHidden(true);
    mPlot->replot();
    QCOMPARE(beforeSpy.count(), 0);
    QCOMPARE(afterSpy.count(), 0);
}

void TestHiddenReplot::replotResumesAfterDisable()
{
    mPlot->show();
    mPlot->hide();
    mPlot->setSkipReplotsWhenHidden(true);
    mPlot->replot();
    mPlot->setSkipReplotsWhenHidden(false);
    QSignalSpy spy(mPlot, &QCustomPlot::afterReplot);
    mPlot->replot();
    QCOMPARE(spy.count(), 1);
}

void TestHiddenReplot::skippedReplotRunsWhenShownAgain()
{
    // A tab switch or a sleep hides the plot without resizing it: whatever changed meanwhile
    // must be drawn when it shows again.
    mPlot->setSkipReplotsWhenHidden(true);
    mPlot->show();
    QVERIFY(QTest::qWaitForWindowExposed(mPlot));
    mPlot->replot();
    mPlot->hide();
    mPlot->xAxis->setRange(5, 15);
    QSignalSpy afterSpy(mPlot, &QCustomPlot::afterReplot);
    mPlot->replot();
    QCOMPARE(afterSpy.count(), 0); // skipped while hidden
    mPlot->show();
    QTRY_VERIFY_WITH_TIMEOUT(afterSpy.count() > 0, 2000);
}
