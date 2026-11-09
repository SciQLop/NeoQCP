#pragma once
#include <QtTest/QtTest>

//! The line-like items record their drawing, and the recording is what the references show.
class TestItemRecording : public QObject
{
    Q_OBJECT
private slots:
    void aLineWithEndingsRecords();
    void aStraightLineRecords();
    void aCurveRecords();
    void aBracketRecords();
    void aPannedPlainLineIsNotReRecorded();
    void aPannedArrowIsRightAfterThePan();
    void everyStyleSetterReRecords();
    void aRectRecords();
    void anEllipseRecords();
    void aPixmapRecords();
    void aScaledPixmapRecords();
    void aTracerRecords();
    void aTracerFollowsItsGraph();
    void selectionReRecords();
};
