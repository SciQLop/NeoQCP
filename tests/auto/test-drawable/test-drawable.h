#pragma once

#include <QtTest/QtTest>

class TestDrawable : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void panDoesNotReRecord();
    void zoomDoesNotReRecord();
    void setterReRecordsOnce();
    void unrelatedMarkDoesNotReRecord();
    void viewEpochReRecordsEscapeHatch();
    void contentChangedInsideDrawIsAMisuse();
    void missingAxisHidesWithAMessage();
};
