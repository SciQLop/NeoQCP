#pragma once

#include <QtTest/QtTest>

#include "qcustomplot.h"

//! An inset's zoom indicator: the box its ranges cover in the parent, and the connectors that join
//! the box to the inset's corners.
class TestZoomIndicator : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void connectorsForAnInsetUpRight();
    void connectorsForAnInsetDownRight();
    void theBoxCoversTheInsetRanges();
    void theBoxFollowsThePannedInset();
    void aPlainReplotKeepsTheRecording();
    void connectorsEndAtTheInsetCorners();
    void aHiddenInsetHidesTheIndicator();
    void anIndicatorOutlivesItsInset();
    void aRectThatIsNotAnInsetIsRefused();

private:
    QCustomPlot* mPlot = nullptr;
    QCPAxisRect* mAx = nullptr;
    QCPAxisRect* mInset = nullptr;
    QCPHandle<QCPColorMap2> mInsetMap;
    QCPHandle<QCPItemZoomIndicator> mIndicator;
};
