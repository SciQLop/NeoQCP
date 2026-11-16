#pragma once
#include "qcustomplot.h"
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QtTest/QtTest>

// Whether a shown plot got its QRhi. A real window can take a few event-loop turns to get one, so
// this waits up to 5 s. The offscreen platform never gives one: waiting there cost 5 s per GPU
// test and pushed the CI's offscreen run past meson's 300 s timeout.
inline bool waitForRhi(QCustomPlot& plot)
{
    if (QGuiApplication::platformName() == QLatin1String("offscreen"))
        return plot.rhi() != nullptr;
    QElapsedTimer waited;
    waited.start();
    while (!plot.rhi() && waited.elapsed() < 5000)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QTest::qWait(10);
    }
    return plot.rhi() != nullptr;
}
