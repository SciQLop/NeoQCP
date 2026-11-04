#pragma once

#include <QtTest/QtTest>

class TestDrawContext : public QObject
{
    Q_OBJECT

private slots:
    void polylineMatchesHandPainting();
    void polylinePositionsMatchesHandPainting();
    void polygonMatchesHandPainting();
    void fillBetweenMatchesHandPainting();
    void rectMatchesHandPainting();
    void ellipseMatchesHandPainting();
    void markersMatchesHandPainting();
    void errorBarsMatchesHandPainting();
    void textMatchesHandPainting();
    void pathMatchesHandPainting();
    void pathDoesNotShiftLaterCommands();
    void imageMatchesHandPainting();
    void painterMatchesHandPainting();
    void painterCallbackStateDoesNotLeak();
    void boundsCoverEveryCommand();
    void sharedDataIsNotCopied();
    void drawListIsCompact();
};
